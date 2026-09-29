//! The panels, drawn with ImGui over the engine's frame: the menu bar, the
//! tool palette, the object palette, the properties of the selected object,
//! the players and the map's own fields, and the status bar. Each is its own
//! ImGui window, placed along the window's edges the first time it is drawn;
//! the map view is whatever they leave uncovered (main.zig routes the mouse
//! and keys there through ImGui's capture flags).
//!
//! Everything that can be tested without a window - the file dialogs'
//! hand-over, the file actions, the palette's filter, the direction
//! conversion, what may be edited, the title - is in panels_logic.zig.
//!
//! Save goes through editor.save's safe-save contract (03-03), redirects to
//! Save As for a new or shipped map (D-18), and Open/Quit/a window close
//! all go through the unsaved-changes prompt (D-23) before they run.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const core = @import("editor_core");
const c_bridge = @import("c_bridge.zig");
const view_mod = @import("view.zig");
const logic = @import("panels_logic.zig");
const testlaunch = @import("testlaunch.zig");
const pictures_mod = @import("pictures.zig");
const marker_logic = @import("marker_logic.zig");
const markers = @import("markers.zig");
const commands = @import("commands.zig");
const panels_m2 = @import("panels_m2.zig");

const ig = imgui.c;
const Editor = core.editor.Editor;
const View = view_mod.View;
const Tool = view_mod.Tool;
const RealBridge = c_bridge.RealBridge;
const CatalogueEntry = c_bridge.c.BkEditorCatalogueEntry;
const c = c_bridge.c;

pub const FileActions = logic.FileActions;
pub const TestLaunchPrompt = logic.TestLaunchPrompt;

/// The dialogs' filter. SDL wants the extensions alone, `;`-separated
/// (SDL_DialogFileFilter), and the list must outlive the dialog, which
/// reports after the call that showed it returned - hence a global.
const map_filters = [_]sdl3.c.SDL_DialogFileFilter{
    .{ .name = "Blitzkrieg maps (*.bzm;*.xml)", .pattern = "bzm;xml" },
};

/// Where a dialog's callback writes its answer. A global, not a field of
/// State: SDL may call back after the State that showed the dialog is gone
/// (the editor quit with the dialog still up), and must find live memory.
var dialog_slot: logic.PathSlot = .{};

/// The panels' layout, in screen pixels, for their first appearance.
const layout = struct {
    const left_width: f32 = 280;
    const right_width: f32 = 320;
    const tools_height: f32 = 150;
    const properties_height: f32 = 250;
    const players_height: f32 = 220;
    const anchors_height: f32 = 190;
};

/// The map types the map file names (CMapInfo::GAME_TYPE,
/// Sources/src/RandomMapGen/MapInfo_Types.h), TYPE_COUNT of them.
const map_type_names = [_][:0]const u8{ "single player", "flag control", "sabotage" };

/// One entry of the recovery-offer modal (D-22, spec Errors -> Crashes): the
/// recovery copy's own OS path, and its sidecar's original document path and
/// unix time.
const RecoveryOffer = struct {
    file_path: [std.Io.Dir.max_path_bytes]u8 = undefined,
    file_path_len: usize = 0,
    original_path: [std.Io.Dir.max_path_bytes]u8 = undefined,
    original_path_len: usize = 0,
    unix_time: i64 = 0,

    fn filePath(self: *const RecoveryOffer) []const u8 {
        return self.file_path[0..self.file_path_len];
    }
    fn originalPath(self: *const RecoveryOffer) []const u8 {
        return self.original_path[0..self.original_path_len];
    }
};

pub const State = struct {
    allocator: std.mem.Allocator,
    editor: *Editor,
    view: *View,
    real: *RealBridge,
    window: *sdl3.c.SDL_Window,
    io: std.Io,
    /// The parent process's environment, for testlaunch.start (Test in game
    /// inherits it, plus whatever extra_env a later mode adds).
    environ: std.process.Environ,
    /// Read once at init (BkEditorPaths): the host roots, for the test
    /// game's own log path.
    paths: c.BkEditorPathSet = std.mem.zeroes(c.BkEditorPathSet),

    /// The running test game, if F5/"Test in game" started one (D-03: the
    /// editor keeps running and drawing beside it). None of this changes the
    /// document - saveCopy, not editor.save (D-01).
    test_game: ?testlaunch.Running = null,
    test_prompt: TestLaunchPrompt = .{},
    /// The active mod's folder (D-09, D-26, D-28): an owned buffer, not a
    /// borrowed slice - State is returned by value from `init` (see
    /// `tile_buffer`'s own comment above, the same reason), and File > Mod
    /// sets this from a menu string with no address of its own to borrow.
    /// Use `modFolder()`/`setModFolder` rather than these fields directly.
    mod_folder_buffer: [64]u8 = undefined,
    mod_folder_len: usize = 0,
    test_game_log_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined,
    test_game_log_len: usize = 0,
    test_restart_popup_shown: bool = false,
    test_report_popup_shown: bool = false,
    /// D-23's "Unsaved changes" modal has been opened for the prompt's
    /// current ask - so it is not re-opened every frame while it waits.
    unsaved_popup_shown: bool = false,

    /// mapeditor.cfg's fields (D-24, D-25, D-27): loaded once by main.zig
    /// (interactive), or by `--check`'s panel smoke under the BK_EDITOR_SETTINGS
    /// test seam; defaults otherwise (the automated modes never load a file).
    settings: core.settings.Settings = .{},
    /// Set by `applySettings` whenever a control in the Settings window (or
    /// the BK_EDITOR_SETTINGS round trip) changes a value: main.zig's `run`
    /// writes `mapeditor.cfg` back once, after the frame this became true,
    /// then clears it - never per keystroke, and never at all when nothing
    /// changed.
    settings_changed: bool = false,
    settings_window_open: bool = false,
    /// The Settings window's "Maps folder" field, loaded from `settings`
    /// whenever the window is (re)opened, edited in place, and only copied
    /// back into `settings` once editing is deactivated (not per keystroke).
    maps_folder_edit: [core.settings.max_path:0]u8 = [_:0]u8{0} ** core.settings.max_path,

    /// File > Open Recent (D-27): whether each entry's file still exists,
    /// checked once (std.Io.Dir access) the frame the submenu newly opens
    /// and reused every frame it stays open - not once per entry per frame.
    recent_menu_open_prev: bool = false,
    recent_exists_cache: [core.settings.recent_capacity]bool = [_]bool{true} ** core.settings.recent_capacity,

    /// File > Mod (D-26): the installed mods, refreshed once the frame the
    /// submenu newly opens - the same shape as Open Recent's own cache above.
    /// 64 comfortably holds any real installation; a longer list is REFUSED
    /// by BkEditorMods and simply capped here rather than shown incomplete.
    mod_menu_open_prev: bool = false,
    mod_list_buffer: [64]c.BkEditorMod = undefined,
    mod_list_count: usize = 0,

    /// D-20..D-22's schedule; `tickAutosave` keeps `enabled`/`interval_ms` in
    /// step with `settings` every frame, so a Settings-window or menu change
    /// takes effect at once.
    autosave: core.autosave.Autosave = .{},
    /// This document's currently-live recovery copy (D-22), if any - its OS
    /// path, so a later real Save/Save As or a clean/Don't-save quit can
    /// delete it and its sidecar. Set when a recovery write succeeds or a
    /// recovery copy is reopened; cleared once deleted.
    recovery_active: ?logic.PathText = null,
    /// `documentIsShipped`'s last answer and the document path it was for:
    /// the rule asks the disk (a realpath, the data-root markers), and the
    /// title, `act` and autosave all ask every frame - the disk is asked
    /// again only when the document's path changes.
    shipped_path: logic.PathText = .{},
    shipped_known: bool = false,
    shipped: bool = false,
    /// What `scanRecoveryOffers` (interactive startup only) found in
    /// `<user_root>mapeditor/recovery/`: each entry's own file, its sidecar's
    /// original path and time. Never populated in an automated mode - nothing
    /// calls `scanRecoveryOffers` there.
    recovery_offers: [8]RecoveryOffer = undefined,
    recovery_offers_count: usize = 0,
    recovery_popup_shown: bool = false,
    /// [Later] on the recovery-offer modal: leaves whatever is still listed
    /// on disk, just stops asking again this session.
    recovery_prompt_dismissed: bool = false,

    /// The object database, and its indices ordered by game type (stable,
    /// so a type keeps the database's order): the palette's groups are runs
    /// of this order.
    catalogue: []CatalogueEntry = &.{},
    order: []u32 = &.{},
    /// Bumped every time the catalogue is read from the bridge (`init`, and
    /// each `reloadCatalogue` a mod switch makes), so the smoke and the host
    /// check can tell a palette the switch re-read from one merely left over
    /// - the fixture mod adds no objects, so the entries alone look the same.
    catalogue_generation: u32 = 0,
    filter: [64:0]u8 = [_:0]u8{0} ** 64,

    /// The catalogue's own sound entries (game type 100), sorted
    /// case-insensitively, for the Sounds panel's combo - slices into
    /// `catalogue`'s own name buffers (built and freed alongside it), never
    /// owned separately.
    sound_names: [][]const u8 = &.{},

    /// D-29: each palette object's own picture, decoded by the engine on
    /// demand and cached per session - cleared on a mod switch
    /// (reloadCatalogue's own pictures.clear() call), released here in
    /// deinit.
    pictures: pictures_mod.Pictures = undefined,

    /// The tiles the open map's tileset has, for the brush's palette.
    /// A count, not a slice: State is returned by value from `init`, and a
    /// slice into its own buffer would point into the copy that was left.
    tile_buffer: [256]u8 = undefined,
    tile_count: usize = 0,
    /// Why the brush has no tiles when a map IS open (Task 5, carried from
    /// plan 5: the palette used to say "no map open" even then, dropping the
    /// bridge's own reason) - BkEditorTilesetTiles's message, copied here
    /// since the bridge's pointer is only valid until its next call and this
    /// must survive to the next frame's draw.
    tile_reason_buffer: [256]u8 = undefined,
    tile_reason_len: usize = 0,
    /// The Brush's tile picker (03-15 gap fix, Johannes's M1 hand try: "tile
    /// 0", "tile 1", ... could only be told apart by painting each): every
    /// offered tile with its terrain type (BkEditorDescribeTile), in the
    /// picker's order (`logic.sortTilesForPicker`) - `tile_count` of them.
    tile_entries: [256]logic.TileEntry = undefined,
    /// Each tile's picture (BkEditorTilePicture), decoded on demand while
    /// the picker is open or for the current tile beside it, and kept for
    /// the tileset `tile_pictures_tileset` names: `mapOpened` drops them only
    /// when a map with another tileset opens; a mod switch always does.
    tile_pictures: pictures_mod.Pictures = undefined,
    tile_pictures_tileset: logic.NameText = .{},
    /// Where the picker's combo and each visible tile cell were drawn last
    /// frame (screen centres), for the smoke to click as a hand would.
    tile_combo_centre: ?ig.ImVec2 = null,
    tile_cell_centres: [256]?ig.ImVec2 = [_]?ig.ImVec2{null} ** 256,
    tile_picker_open: bool = false,

    /// Every distinct object type the open map has that the object database
    /// does not know, most frequent first (`summarizeUnknown`), and the
    /// modal that reports them (spec Errors -> Open). 64 comfortably holds
    /// any real mod map's distinct unknown types - the same "capped rather
    /// than shown incomplete" convention `mod_list_buffer` already uses.
    unknown_types: [64]logic.UnknownType = undefined,
    unknown_types_count: usize = 0,
    unknown_objects_total: usize = 0,
    unknown_popup_shown: bool = false,

    /// The panels' own current column widths (Task 2, carried from plan 5):
    /// `layout.left_width`/`right_width` until a panel's live width is read
    /// back after its own `igBegin` - kept here, not just passed through, so
    /// a viewport-size change can re-place a column at its new edge with
    /// `ImGuiCond_Always` without also resetting a width the user dragged.
    left_width: f32 = layout.left_width,
    right_width: f32 = layout.right_width,
    /// The main viewport's size as of the last frame's `draw` - a change is
    /// how `draw` notices a resize happened at all (ImGuiCond_FirstUseEver
    /// only ever applies once per window, ever, so re-passing the same
    /// condition after the first frame is a no-op: this is what let the
    /// carried "right-hand panels do not follow a resize" bug happen).
    last_viewport_size: ig.ImVec2 = .{ .x = 0, .y = 0 },

    /// The open map's own sound list (CMapInfo::sounds.sounds through
    /// RealBridge.sounds - see bridge.h's own comment on why not
    /// CMapInfo::soundsList), for the Sounds panel. Read again whenever a map
    /// opens (`mapOpened`) or `editor.sounds_generation` moves past
    /// `sounds_generation_seen` (`draw`'s own check) - a sound edit does not
    /// go through `mapOpened`.
    sounds: []core.bridge.SoundRecord = &.{},
    sounds_generation_seen: u32 = 0,

    /// The Sounds panel's selected row, and its fields while they are being
    /// edited - `drawProperties`' own commit-on-deactivate pattern (edit.zig
    /// there, this one here since a sound is not an ObjectRecord).
    selected_sound: ?usize = null,
    sound_edit: struct {
        index: usize = 0,
        name_buffer: [core.bridge.name_capacity:0]u8 = [_:0]u8{0} ** core.bridge.name_capacity,
        x: f32 = 0,
        y: f32 = 0,
        repeat_seconds: f32 = 0,
        repeat_random_seconds: f32 = 0,
        mute_in_combat: bool = false,
        min_radius: c_int = 0,
        max_radius: c_int = 0,
        active: bool = false,
    } = .{},

    /// The open map's camera anchors (D-22), for the Camera anchors panel and
    /// the anchor markers. Read again whenever a map opens (`mapOpened`) or
    /// `editor.record_generations` for the anchors moves past
    /// `anchors_generation_seen` (`refreshAnchors`, called from `draw`): an
    /// anchor edit, undo or redo does not go through `mapOpened`.
    anchors: core.records.CameraAnchors = .{},
    anchors_generation_seen: u32 = 0,
    /// View -> Markers: which M2 marker kinds are drawn (all, until switched
    /// off). The active tool's own kinds are drawn regardless.
    marker_set: marker_logic.MarkerSet = .{},

    /// open_requested, save_requested, save_as_requested, quit_requested,
    /// and the dialog's hand-over: see panels_logic.FileActions.
    actions: FileActions = .{ .dialog = &dialog_slot },
    /// False under main.zig's --smoke, whose script hands the dialog slot
    /// its answer itself: `showDialog` then leaves the slot waiting without
    /// asking SDL for a real file dialog. A real one outlives the step that
    /// asked for it - nothing ever closes it - and on Windows SDL shows it
    /// from its own thread whenever that thread gets there, modal to the
    /// editor's window, so it takes the focus and the pointer from the
    /// window in the middle of a later step.
    os_dialogs: bool = true,
    /// Real file dialogs asked of SDL so far; the smoke checks it stays 0.
    os_dialogs_opened: u32 = 0,
    /// BK_EDITOR_AUTO's own `test` action (smoke.zig's `AutoRunner`): extra
    /// environment for the next `startTestGame` spawn - BK_AUTO_UI from
    /// BK_EDITOR_AUTO_GAME and BK_NO_HELP=1, so the child game runs and
    /// exits unattended. Empty for every other caller (F5, the menu item),
    /// which inherit the parent's environment unchanged, exactly as before.
    test_extra_env: []const [2][]const u8 = &.{},

    /// The properties panel's fields while they are being edited: loaded
    /// from the selected object whenever none of them was active last
    /// frame, so a typed number is not overwritten under the cursor.
    edit: struct {
        link_id: i32 = -1,
        x: f32 = 0,
        y: f32 = 0,
        degrees: f32 = 0,
        degrees_shown: f32 = 0,
        player: c_int = 0,
        active: bool = false,
    } = .{},

    title: [256]u8 = undefined,
    title_len: usize = 0,

    /// The catalogue is read once: it is the database's, not the map's.
    /// A catalogue that will not read leaves the palette empty and says so
    /// on the status bar rather than stopping the editor. `mod_folder` is the
    /// mod chosen on the command line (main.zig's `-mod=`), already applied
    /// to the bridge session before this call - `init` only records it.
    pub fn init(allocator: std.mem.Allocator, editor: *Editor, view: *View, real: *RealBridge, window: *sdl3.c.SDL_Window, io: std.Io, environ: std.process.Environ, mod_folder: ?[]const u8) State {
        var state: State = .{ .allocator = allocator, .editor = editor, .view = view, .real = real, .window = window, .io = io, .environ = environ, .pictures = pictures_mod.Pictures.init(allocator), .tile_pictures = pictures_mod.Pictures.initFor(allocator, .tile) };
        state.setModFolder(mod_folder);
        if (real.paths(&state.paths) != .ok) state.paths = std.mem.zeroes(c.BkEditorPathSet);
        // Editor.save's own read-only refusal (D-18) judges against the
        // same installation the panels do.
        editor.setBaseRoot(std.mem.sliceTo(&state.paths.base_root, 0));
        state.loadCatalogue() catch view.setStatus("failed: ", "the object catalogue did not read");
        state.mapOpened();
        return state;
    }

    pub fn deinit(self: *State) void {
        self.pictures.deinit();
        self.tile_pictures.deinit();
        self.allocator.free(self.catalogue);
        self.allocator.free(self.order);
        self.allocator.free(self.sound_names);
        self.allocator.free(self.sounds);
        self.* = undefined;
    }

    /// The active mod's folder, or null for the base game.
    pub fn modFolder(self: *const State) ?[]const u8 {
        return if (self.mod_folder_len == 0) null else self.mod_folder_buffer[0..self.mod_folder_len];
    }

    /// Copies `folder` into the owned buffer (null or "" clears it) - never a
    /// borrowed slice, per the field's own doc comment.
    fn setModFolder(self: *State, folder: ?[]const u8) void {
        const value = folder orelse "";
        self.mod_folder_len = @min(value.len, self.mod_folder_buffer.len);
        @memcpy(self.mod_folder_buffer[0..self.mod_folder_len], value[0..self.mod_folder_len]);
    }

    /// File > Mod (D-26): frees and re-reads the catalogue, so the palette
    /// follows a mod switch. A failure leaves the palette empty and says so
    /// on the status bar, the same as `init`'s own failure path.
    /// D-29: also drops every cached and queued picture - the new mod's
    /// objects may reuse a name with a different icon.tga, or none at all,
    /// and the old mod's pictures are meaningless once its storage unmounts.
    pub fn reloadCatalogue(self: *State) void {
        self.pictures.clear();
        // The new mod may ship the same tileset name with other pictures.
        self.tile_pictures.clear();
        self.tile_pictures_tileset = .{};
        self.allocator.free(self.catalogue);
        self.allocator.free(self.order);
        self.allocator.free(self.sound_names);
        self.catalogue = &.{};
        self.order = &.{};
        self.sound_names = &.{};
        self.loadCatalogue() catch self.view.setStatus("failed: ", "the object catalogue did not read");
    }

    pub fn tiles(self: *const State) []const u8 {
        return self.tile_buffer[0..self.tile_count];
    }

    /// The picker's entries, in its order (see `tile_entries`).
    pub fn tileEntries(self: *const State) []const logic.TileEntry {
        return self.tile_entries[0..self.tile_count];
    }

    /// Names every offered tile by its terrain type and sorts them into the
    /// picker's sections; drops the cached tile pictures when the tileset is
    /// another one than they were decoded from (03-15 gap fix).
    fn describeTiles(self: *State) void {
        var tileset: logic.NameText = .{};
        for (self.tiles(), 0..) |tile, i| {
            var entry: logic.TileEntry = .{ .tile = tile };
            if (self.real.describeTile(tile)) |info| {
                entry.terrain_index = info.terrain_index;
                entry.terrain = logic.NameText.init(std.mem.sliceTo(&info.terrain, 0));
                if (tileset.len == 0) tileset = logic.NameText.init(std.mem.sliceTo(&info.tileset, 0));
            }
            self.tile_entries[i] = entry;
        }
        logic.sortTilesForPicker(self.tile_entries[0..self.tile_count]);
        if (logic.tilePicturesStale(self.tile_pictures_tileset.slice(), tileset.slice())) {
            self.tile_pictures.clear();
            self.tile_pictures_tileset = tileset;
        }
    }

    /// Frees the previous sound list and reads it again through the core's
    /// own bridge (the vtable Task 2 added) - the open map's, or empty when
    /// none is open or the read fails. Called after every open (`mapOpened`)
    /// and whenever `editor.sounds_generation` moves past
    /// `sounds_generation_seen` (`draw`'s own check): a sound edit does not
    /// go through `mapOpened`. The same two-pass sizing `document.reload`
    /// uses for `bridge.objects`.
    pub fn loadSounds(self: *State) void {
        self.allocator.free(self.sounds);
        self.sounds = &.{};
        if (!mapIsOpen(self.editor)) return;
        var none: [0]core.bridge.SoundRecord = .{};
        var total: usize = 0;
        const sizing = self.editor.bridge.sounds(&none, &total);
        if (sizing != .ok and sizing != .refused) return;
        const buffer = self.allocator.alloc(core.bridge.SoundRecord, total) catch return;
        var got: usize = total;
        if (self.editor.bridge.sounds(buffer, &got) != .ok or got != buffer.len) {
            self.allocator.free(buffer);
            return;
        }
        self.sounds = buffer;
    }

    fn loadCatalogue(self: *State) !void {
        self.catalogue_generation +%= 1;
        const entries = try self.real.catalogue(self.allocator);
        errdefer self.allocator.free(entries);
        // Only what can be placed: a sound or a tank pit picked from the
        // palette could only ever be refused (logic.isPlaceable), and so could
        // a single soldier - the bridge's own `placeable`, 0 for every
        // infantry unit, which goes on a map only inside a squad (the MFC
        // palette never listed units\Humans either).
        var placeable: usize = 0;
        for (entries) |entry| {
            if (logic.isPlaceable(entry.game_type) and entry.placeable != 0) placeable += 1;
        }
        const order = try self.allocator.alloc(u32, placeable);
        var next: usize = 0;
        for (entries, 0..) |entry, i| {
            if (!logic.isPlaceable(entry.game_type) or entry.placeable == 0) continue;
            order[next] = @intCast(i);
            next += 1;
        }
        // ~5.5k entries: std.sort.block (stable - a type keeps the database's
        // own order), not the O(n^2)-worst-case sort this used before (Task 5,
        // carried from plan 5).
        std.sort.block(u32, order, entries, struct {
            fn less(context: []CatalogueEntry, a: u32, b: u32) bool {
                return context[a].game_type < context[b].game_type;
            }
        }.less);
        self.catalogue = entries;
        self.order = order;

        // The Sounds panel's "known sounds" combo (game type 100, SGVOGT_SOUND):
        // slices into `entries`' own name buffers, sorted once here rather than
        // filtered and sorted again every frame the combo is open. Pointer
        // capture (`|*entry|`) matters: `entries` is a heap slice, but `for
        // (entries) |entry|` copies each element into a loop-local value, and
        // a slice of *that* dangles the moment the loop moves on - measured
        // the hard way (every name printed as stack garbage until this was
        // `|*entry|`). An empty name is left out too: BkEditorAddSound/
        // SetSound refuse one anyway, and it would otherwise sort first,
        // ahead of every real sound.
        var sound_names: std.ArrayListUnmanaged([]const u8) = .empty;
        errdefer sound_names.deinit(self.allocator);
        for (entries) |*entry| {
            if (entry.game_type == 100 and entry.name[0] != 0) try sound_names.append(self.allocator, std.mem.sliceTo(&entry.name, 0));
        }
        const owned_sound_names = try sound_names.toOwnedSlice(self.allocator);
        logic.sortNamesIgnoreCase(owned_sound_names);
        self.sound_names = owned_sound_names;
    }

    /// The catalogue's first entry of game type unit, for the placer's
    /// default object (view.zig's `showMap`) - read from `catalogue`, already
    /// loaded here, rather than asking the bridge for the whole thing again
    /// (Task 3, carried from plan 5: `View.pickDefaultPlacerObject` used to).
    fn defaultPlacerObject(self: *const State) ?[]const u8 {
        // `|*entry|`, not `|entry|`: `self.catalogue` is a heap slice, but
        // `for (self.catalogue) |entry|` copies each element into a
        // loop-local value, and a slice of *that* dangles the moment this
        // function returns - the same bug `loadCatalogue`'s own
        // `sound_names` loop documents and avoids, measured there the hard
        // way (every name printed as stack garbage). Found here by
        // map-editor-smoke's placer trying to add an object named from
        // whatever briefly sat in the reused stack space instead.
        for (self.catalogue) |*entry| {
            if (entry.game_type == view_mod.unit_game_type and entry.placeable != 0) return std.mem.sliceTo(&entry.name, 0);
        }
        return null;
    }

    /// The brush combo's own choice (drawToolPalette) and the smoke's
    /// `prepare` both choose a tile through here (Task 7, carried from plan
    /// 5: the smoke used to set `view.brush.tile` directly, bypassing the
    /// palette-to-brush path it is meant to exercise).
    pub fn chooseBrushTile(self: *State, tile: u8) void {
        self.view.brush.tile = tile;
    }

    /// Re-reads the camera anchors into `anchors` when the map's anchor
    /// record moved since the last read (or no map is open: none). A read
    /// that fails leaves them unset, which the panel and the markers show as
    /// "unset" rather than as stale positions.
    pub fn refreshAnchors(self: *State) void {
        const generation = self.editor.record_generations.get(.camera_anchors);
        if (generation == self.anchors_generation_seen and mapIsOpen(self.editor)) return;
        self.anchors_generation_seen = generation;
        self.anchors = commands.readAnchors(self) orelse .{};
    }

    /// Why the brush has no tiles right now, when a map is open (`tiles()` is
    /// empty but `mapIsOpen` is true) - empty otherwise.
    pub fn tileReason(self: *const State) []const u8 {
        return self.tile_reason_buffer[0..self.tile_reason_len];
    }

    fn setTileReason(self: *State, reason: []const u8) void {
        self.tile_reason_len = @min(reason.len, self.tile_reason_buffer.len);
        @memcpy(self.tile_reason_buffer[0..self.tile_reason_len], reason[0..self.tile_reason_len]);
    }

    /// After any open that succeeded, the startup one included: the camera,
    /// the tileset's tiles and the fields follow the new map.
    pub fn mapOpened(self: *State) void {
        self.edit = .{};
        self.tile_count = 0;
        self.tile_reason_len = 0;
        self.selected_sound = null;
        self.sound_edit.active = false;
        self.loadSounds();
        self.sounds_generation_seen = self.editor.sounds_generation;
        self.anchors_generation_seen = self.editor.record_generations.get(.camera_anchors);
        self.anchors = commands.readAnchors(self) orelse .{};
        self.unknown_types_count = 0;
        self.unknown_objects_total = 0;
        self.unknown_popup_shown = false;
        if (!mapIsOpen(self.editor)) return;
        self.view.showMap(self.real, self.editor.document.path.items, self.editor.document.info, self.defaultPlacerObject());
        if (self.real.tilesetTiles(&self.tile_buffer)) |got| {
            self.tile_count = got.len;
            // Task 5, carried from plan 5: the palette used to say "no map
            // open" here too, when the call succeeded but the tileset simply
            // has no tiles to offer.
            if (got.len == 0) self.setTileReason("the tileset has no tiles");
        } else {
            // Same carried item: a failed call dropped the bridge's own
            // reason the same way.
            self.setTileReason(self.editor.bridge.lastMessage());
        }
        // The brush keeps its tile if the new tileset has it; otherwise it
        // takes the first the tileset has, so it never paints a refusal.
        const offered = self.tiles();
        if (offered.len != 0 and std.mem.indexOfScalar(u8, offered, self.view.brush.tile) == null)
            self.view.brush.tile = offered[0];
        self.describeTiles();

        // The unknown-objects warning (spec Errors -> Open): every object the
        // open map lists that the object database does not know.
        var unknown_total: usize = 0;
        for (self.editor.document.objects.items) |object| {
            if (!object.known) unknown_total += 1;
        }
        self.unknown_objects_total = unknown_total;
        self.unknown_types_count = logic.summarizeUnknown(self.editor.document.objects.items, &self.unknown_types);
    }
};

pub fn mapIsOpen(editor: *const Editor) bool {
    return editor.document.path.items.len != 0;
}

/// All the panels, once a frame, between host.beginFrame and host.endFrame.
/// Edits made through them go to the editor at once; the file actions the
/// menu asks for are left in `state.actions` for `act`.
pub fn draw(state: *State) void {
    // Drawn first so the brush outline sits under the panels' own draw
    // calls - it targets the background draw list (behind the panels'
    // window draw lists regardless of call order), so this is about
    // reading this frame's hover/tool state before anything else changes it.
    state.refreshAnchors();
    state.view.drawOverlay(state.real, state.sounds, state.selected_sound);
    markers.drawM2Markers(state, state.real);
    const menu_height = drawMenuBar(state);
    // ImGui's own capture flag (WantTextInput, not WantCaptureKeyboard): a
    // properties field mid-edit must keep F5 as a literal keystroke, but a
    // window merely being focused must not swallow it.
    if (ig.igIsKeyPressedEx(ig.ImGuiKey_F5, false) and !ig.igGetIO().*.WantTextInput) requestTestLaunch(state);
    if (closeShortcutPressed() and mapIsOpen(state.editor)) state.actions.close_requested = true;
    const viewport = ig.igGetMainViewport();
    const size = viewport.*.Size;
    // Task 2, carried from plan 5: ImGuiCond_FirstUseEver only ever applies
    // the first time a window is drawn, so a plain resize's own
    // igSetNextWindowPos/Size calls below were no-ops after that - the
    // right-hand panels stayed wherever they first landed. Always for the one
    // frame the viewport itself changes size re-applies pos and height;
    // size.x is handed back exactly as `beginPanel` last read it
    // (state.left_width/right_width), so a width the user dragged survives.
    const resized = size.x != state.last_viewport_size.x or size.y != state.last_viewport_size.y;
    state.last_viewport_size = size;
    const cond: ig.ImGuiCond = if (resized) ig.ImGuiCond_Always else ig.ImGuiCond_FirstUseEver;
    const status_height = ig.igGetFrameHeightWithSpacing() + 4;
    const body_top = menu_height;
    const body_height = @max(size.y - menu_height - status_height, 100);

    drawToolPalette(state, .{ .x = 0, .y = body_top }, .{ .x = state.left_width, .y = layout.tools_height }, cond);
    drawObjectPalette(state, .{ .x = 0, .y = body_top + layout.tools_height }, .{ .x = state.left_width, .y = @max(body_height - layout.tools_height, 100) }, cond);
    const right_x = @max(size.x - state.right_width, state.left_width);
    drawProperties(state, .{ .x = right_x, .y = body_top }, .{ .x = state.right_width, .y = layout.properties_height }, cond);
    drawPlayers(state, .{ .x = right_x, .y = body_top + layout.properties_height }, .{ .x = state.right_width, .y = layout.players_height }, cond);
    panels_m2.drawCameraAnchors(state, .{ .x = right_x, .y = body_top + layout.properties_height + layout.players_height }, .{ .x = state.right_width, .y = layout.anchors_height }, cond);
    drawSounds(state, .{ .x = right_x, .y = body_top + layout.properties_height + layout.players_height + layout.anchors_height }, .{ .x = state.right_width, .y = @max(body_height - layout.properties_height - layout.players_height - layout.anchors_height, 100) }, cond);
    drawStatusBar(state, .{ .x = 0, .y = size.y - status_height }, .{ .x = size.x, .y = status_height });
    drawTestLaunchModals(state);
    drawUnsavedPrompt(state);
    drawUnknownObjectsPrompt(state);
    drawSettingsWindow(state);
    drawRecoveryPrompt(state);
    updateTitle(state);
}

/// The base root as `BkEditorPaths` gave it at init, sliced to its content.
fn baseRoot(state: *const State) []const u8 {
    return std.mem.sliceTo(&state.paths.base_root, 0);
}

/// The user root the same way.
fn userRoot(state: *const State) []const u8 {
    return std.mem.sliceTo(&state.paths.user_root, 0);
}

/// Whether the open document is a shipped map (D-18, core/shipped.zig):
/// read-only - Save becomes Save As, autosave writes only a recovery copy,
/// the title says so. Cached per document path (`State.shipped_path`).
pub fn documentIsShipped(state: *State) bool {
    const path = state.editor.document.path.items;
    if (state.shipped_known and std.mem.eql(u8, state.shipped_path.slice(), path)) return state.shipped;
    state.shipped = logic.isShippedMap(path, baseRoot(state), state.editor.files);
    state.shipped_path.set(path);
    state.shipped_known = path.len <= state.shipped_path.buffer.len;
    return state.shipped;
}

/// `logic.needsSaveAs` for the open document, through `documentIsShipped`'s
/// cache.
pub fn documentNeedsSaveAs(state: *State) bool {
    const path = state.editor.document.path.items;
    return logic.needsSaveAsKnowing(path, documentIsShipped(state), userRoot(state));
}

/// The file actions the menu asked for, and whatever a dialog delivered,
/// after the frame. True when the editor should quit.
pub fn act(state: *State) bool {
    var quit = false;
    while (true) {
        const needs_save_as = documentNeedsSaveAs(state);
        switch (state.actions.next(state.editor.dirty(), needs_save_as)) {
            .none, .ask_unsaved => return quit,
            // Task 5, carried from plan 5: a second Open or Save As while a
            // dialog is already up used to be dropped silently.
            .dialog_busy => {
                state.view.setStatusFrom(.dialog, "", "a dialog is already open");
                return quit;
            },
            // The dialog that was busy has ended: "a dialog is already
            // open" (or an earlier dialog's failure) is no longer true.
            .dialog_cancelled => state.view.clearStatusFrom(.dialog),
            // A quit that reaches here is either clean (never dirty) or
            // answered Don't save (the prompt's .proceed path) - both are
            // "this document's edits, if any, are abandoned" (D-22).
            .quit => {
                deleteRecoveryIfActive(state);
                quit = true;
            },
            .save => {
                const ok = saveToDocumentPath(state);
                if (ok) {
                    pushRecentFromDocument(state);
                    deleteRecoveryIfActive(state);
                }
                state.actions.noteSaveOutcome(ok);
            },
            .show_dialog => |kind| showDialog(state, kind),
            .act_on_path => |chosen| {
                const result = logic.actOnPath(state.editor, chosen.kind, chosen.path);
                state.view.noteEditResult(state.editor, result);
                if (chosen.kind == .open) {
                    // A failed open may have emptied the document (editor.open
                    // says when); either way the panels follow what is open now.
                    if (result) |_| {
                        state.mapOpened();
                        pushRecentFromDocument(state);
                    } else |_| if (!mapIsOpen(state.editor)) state.mapOpened();
                } else {
                    // Save As: the unsaved-changes prompt, if it asked for
                    // this one, hears whether it landed.
                    const ok = if (result) |_| true else |_| false;
                    if (ok) {
                        pushRecentFromDocument(state);
                        deleteRecoveryIfActive(state);
                    }
                    state.actions.noteSaveOutcome(ok);
                }
            },
            .dialog_failed => |message| state.view.setStatusFrom(.dialog, "the file dialog failed: ", message),
            .switch_mod => |folder| performModSwitch(state, folder),
            .close => performClose(state),
        }
    }
}

/// D-27: after every successful Open, Save or Save As, the document's OS
/// path goes to the front of the recent list, and main.zig's `run` is asked
/// to write it back. The automated modes call this exactly as often as the
/// interactive one (nothing here is gated on mode) - what keeps them from
/// ever touching mapeditor.cfg is that they always pass a null settings_path
/// to `run`, so the write itself never happens; only this in-memory list,
/// discarded when the process exits, ever changes.
fn pushRecentFromDocument(state: *State) void {
    var buffer: [core.files.max_path]u8 = undefined;
    const os_path = core.files.osPathFromEngine(&buffer, state.editor.document.path.items) orelse return;
    state.settings.pushRecent(os_path);
    state.settings_changed = true;
}

/// Saves to the document's own path; true on success. editor.save copies
/// the path before it writes, so handing it its own path is safe.
fn saveToDocumentPath(state: *State) bool {
    if (!mapIsOpen(state.editor)) return false;
    const result = state.editor.save(state.editor.document.path.items);
    state.view.noteEditResult(state.editor, result);
    return if (result) |_| true else |_| false;
}

/// `<user_root>mapeditor/recovery`, an OS path (D-22's own location).
fn recoveryFolder(buffer: []u8, state: *const State) ?[]const u8 {
    return std.fmt.bufPrint(buffer, "{s}mapeditor{c}recovery", .{ userRoot(state), std.fs.path.sep }) catch null;
}

/// Whether a modal or file dialog is already up: autosave must never fight
/// the user for the map (Task 3's own action item) while one of these is
/// showing.
fn anyModalOpen(state: *const State) bool {
    return state.actions.dialog.waiting() or state.actions.prompt.isAsking() or state.settings_window_open or
        state.test_prompt.isAskingRestart() or state.test_prompt.report() != null or
        (state.recovery_offers_count != 0 and !state.recovery_prompt_dismissed);
}

/// Once per frame, interactive only (main.zig's `run` gates this on
/// `is_interactive`), never while a modal or dialog is up (D-20..D-22): due
/// -> the map file itself (D-20, through editor.save's own safe-save and
/// .bak rule) when the document has a real, writable path; a recovery copy
/// plus its sidecar (D-22) otherwise. A failure still calls `wrote` - the
/// next try is a full interval later, not next frame.
pub fn tickAutosave(state: *State, now_ms: u64) void {
    state.autosave.enabled = state.settings.autosave;
    state.autosave.interval_ms = @as(u64, state.settings.autosave_minutes) * std.time.ms_per_min;
    if (!mapIsOpen(state.editor) or anyModalOpen(state)) {
        // Still tracked (not ticked away): a dirty map waiting out a modal
        // must not lose its place in the interval once the modal closes.
        state.autosave.note(now_ms, state.editor.dirty() and mapIsOpen(state.editor));
        return;
    }
    const dirty = state.editor.dirty();
    state.autosave.note(now_ms, dirty);
    if (!state.autosave.due(now_ms, dirty)) return;
    const needs_save_as = documentNeedsSaveAs(state);
    switch (core.autosave.target(needs_save_as)) {
        .map_file => autosaveIntoMapFile(state, now_ms),
        .recovery_copy => writeRecoveryCopy(state, now_ms),
    }
}

fn autosaveIntoMapFile(state: *State, now_ms: u64) void {
    const result = state.editor.save(state.editor.document.path.items);
    state.autosave.wrote(now_ms);
    if (result) |_| {
        var buffer: [64]u8 = undefined;
        const name = logic.baseName(state.editor.document.path.items);
        state.view.setStatus("", std.fmt.bufPrint(&buffer, "autosaved {s}", .{name}) catch "autosaved");
    } else |_| {
        state.view.setStatus("autosave failed: ", state.editor.status());
    }
}

/// D-22: `real.saveCopy` into the recovery folder, plus the sidecar with the
/// document's own OS path and the current unix time. `state.recovery_active`
/// remembers this write's OS path so a later real Save/Save As or a
/// clean/Don't-save quit can delete both files.
fn writeRecoveryCopy(state: *State, now_ms: u64) void {
    state.autosave.wrote(now_ms);
    var name_buffer: [300]u8 = undefined;
    const name = core.autosave.recoveryName(&name_buffer, state.editor.document.path.items) orelse {
        state.view.setStatus("autosave failed: ", "the recovery file name is too long");
        return;
    };
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = recoveryFolder(&folder_buffer, state) orelse {
        state.view.setStatus("autosave failed: ", "the recovery folder's path is too long");
        return;
    };
    std.Io.Dir.cwd().createDirPath(state.io, folder) catch {};
    var os_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const os_path = std.fmt.bufPrint(&os_path_buffer, "{s}{c}{s}", .{ folder, std.fs.path.sep, name }) catch {
        state.view.setStatus("autosave failed: ", "the recovery path is too long");
        return;
    };
    var engine_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const engine_path = logic.enginePath(&engine_buffer, os_path, .open) orelse {
        state.view.setStatus("autosave failed: ", "the recovery path is too long");
        return;
    };
    if (state.real.saveCopy(engine_path) != .ok) {
        state.view.setStatus("autosave failed: ", std.mem.span(c.BkEditorLastMessage(state.real.session)));
        return;
    }
    writeRecoverySidecar(state, os_path);
    state.recovery_active = logic.PathText.init(os_path);
    state.view.setStatus("", "recovery copy written");
}

/// `<recovery copy>.txt`: the original OS path, then the unix time, one per
/// line - `scanRecoveryOffers` reads both back for the modal.
fn writeRecoverySidecar(state: *State, recovery_os_path: []const u8) void {
    var sidecar_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const sidecar_path = std.fmt.bufPrint(&sidecar_buffer, "{s}.txt", .{recovery_os_path}) catch return;
    var doc_os_buffer: [core.files.max_path]u8 = undefined;
    const original_os_path = core.files.osPathFromEngine(&doc_os_buffer, state.editor.document.path.items) orelse return;
    const unix_seconds = std.Io.Clock.real.now(state.io).toSeconds();
    var sidecar_text_buffer: [core.files.max_path + 64]u8 = undefined;
    const sidecar_text = std.fmt.bufPrint(&sidecar_text_buffer, "{s}\n{d}\n", .{ original_os_path, unix_seconds }) catch return;
    std.Io.Dir.cwd().writeFile(state.io, .{ .sub_path = sidecar_path, .data = sidecar_text }) catch {};
}

/// D-22: after a real save lands, or a clean/Don't-save quit, this document's
/// recovery copy (if it had one) is no longer needed.
fn deleteRecoveryIfActive(state: *State) void {
    const active = state.recovery_active orelse return;
    var sidecar_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    if (std.fmt.bufPrint(&sidecar_buffer, "{s}.txt", .{active.slice()})) |sidecar| {
        std.Io.Dir.cwd().deleteFile(state.io, sidecar) catch {};
    } else |_| {}
    std.Io.Dir.cwd().deleteFile(state.io, active.slice()) catch {};
    state.recovery_active = null;
}

/// Interactive startup only: what `<user_root>mapeditor/recovery/` holds
/// right now, each `.bzm` paired with its own `<name>.bzm.txt` sidecar. A
/// recovery file with no readable sidecar (or one whose folder does not
/// exist yet - a fresh install) is simply not offered; nothing here is a
/// reason to fail startup.
pub fn scanRecoveryOffers(state: *State) void {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = recoveryFolder(&folder_buffer, state) orelse return;
    var dir = std.Io.Dir.cwd().openDir(state.io, folder, .{ .iterate = true }) catch return;
    defer dir.close(state.io);
    var it = dir.iterate();
    while (state.recovery_offers_count < state.recovery_offers.len) {
        const entry = (it.next(state.io) catch break) orelse break;
        if (entry.kind != .file) continue;
        if (!std.ascii.endsWithIgnoreCase(entry.name, ".bzm")) continue;
        var offer: RecoveryOffer = .{};
        const file_path = std.fmt.bufPrint(&offer.file_path, "{s}{c}{s}", .{ folder, std.fs.path.sep, entry.name }) catch continue;
        offer.file_path_len = file_path.len;
        var sidecar_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const sidecar_path = std.fmt.bufPrint(&sidecar_buffer, "{s}.txt", .{file_path}) catch continue;
        const sidecar_bytes = std.Io.Dir.cwd().readFileAlloc(state.io, sidecar_path, state.allocator, .limited(4096)) catch continue;
        defer state.allocator.free(sidecar_bytes);
        var lines = std.mem.splitScalar(u8, sidecar_bytes, '\n');
        const original = lines.next() orelse continue;
        const time_text = std.mem.trim(u8, lines.next() orelse "0", " \r\n\t");
        offer.unix_time = std.fmt.parseInt(i64, time_text, 10) catch 0;
        offer.original_path_len = @min(original.len, offer.original_path.len);
        @memcpy(offer.original_path[0..offer.original_path_len], original[0..offer.original_path_len]);
        state.recovery_offers[state.recovery_offers_count] = offer;
        state.recovery_offers_count += 1;
    }
}

/// D-22, spec Errors -> Crashes: at the next start, every recovery copy found
/// is offered back - Open reopens it (through the recovery folder itself, so
/// it keeps autosaving there per D-22 until its own Save As), Discard drops
/// it unopened, and Later leaves the list for next time without asking again
/// this session.
fn drawRecoveryPrompt(state: *State) void {
    if (state.recovery_offers_count == 0 or state.recovery_prompt_dismissed) return;
    const popup_id = "Unsaved work from an earlier session";
    if (!state.recovery_popup_shown) {
        _ = ig.igOpenPopup(popup_id, 0);
        state.recovery_popup_shown = true;
    }
    if (!ig.igBeginPopupModal(popup_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
    var index: usize = 0;
    while (index < state.recovery_offers_count) {
        const offer = state.recovery_offers[index];
        ig.igPushIDInt(@intCast(index));
        var line: [400]u8 = undefined;
        const label = std.fmt.bufPrint(&line, "{s} - {d}", .{ logic.baseName(offer.originalPath()), offer.unix_time }) catch "?";
        text(label);
        ig.igSameLine();
        const opened = ig.igSmallButton("Open");
        ig.igSameLine();
        const discarded = ig.igSmallButton("Discard");
        ig.igPopID();
        if (opened) {
            openRecoveryOffer(state, offer);
            removeRecoveryOffer(state, index);
            ig.igCloseCurrentPopup();
            break;
        } else if (discarded) {
            discardRecoveryOffer(state, offer);
            removeRecoveryOffer(state, index);
        } else {
            index += 1;
        }
    }
    if (state.recovery_offers_count == 0) {
        ig.igCloseCurrentPopup();
    } else if (ig.igButton("Later")) {
        state.recovery_prompt_dismissed = true;
        ig.igCloseCurrentPopup();
    }
    ig.igEndPopup();
}

fn openRecoveryOffer(state: *State, offer: RecoveryOffer) void {
    var engine_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const engine_path = logic.enginePath(&engine_buffer, offer.filePath(), .open) orelse {
        state.view.setStatus("failed: ", "the recovery path is too long");
        return;
    };
    const result = state.editor.open(engine_path);
    if (result) |_| {
        state.mapOpened();
        // needsSaveAs's own recovery-folder check (Task 3) keeps this
        // document on Save As until it leaves the recovery folder for real;
        // recovery_active lets autosave keep writing this same file until then.
        state.recovery_active = logic.PathText.init(offer.filePath());
    } else |_| {
        state.view.setStatus("failed: ", state.editor.status());
    }
}

fn discardRecoveryOffer(state: *State, offer: RecoveryOffer) void {
    var sidecar_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    if (std.fmt.bufPrint(&sidecar_buffer, "{s}.txt", .{offer.filePath()})) |sidecar| {
        std.Io.Dir.cwd().deleteFile(state.io, sidecar) catch {};
    } else |_| {}
    std.Io.Dir.cwd().deleteFile(state.io, offer.filePath()) catch {};
}

fn removeRecoveryOffer(state: *State, index: usize) void {
    var i = index;
    while (i + 1 < state.recovery_offers_count) : (i += 1) state.recovery_offers[i] = state.recovery_offers[i + 1];
    state.recovery_offers_count -= 1;
}

/// D-23: while the prompt asks (an Open, Quit or window close found the map
/// dirty), a modal offers Save, Don't save or Cancel; the buttons only set
/// what `act`'s next call to `next` picks up - no save happens here.
fn drawUnsavedPrompt(state: *State) void {
    const popup_id = "Unsaved changes";
    if (state.actions.prompt.isAsking()) {
        if (!state.unsaved_popup_shown) {
            _ = ig.igOpenPopup(popup_id, 0);
            state.unsaved_popup_shown = true;
        }
    } else {
        state.unsaved_popup_shown = false;
    }
    if (!ig.igBeginPopupModal(popup_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
    var buffer: [300]u8 = undefined;
    const name = logic.baseName(state.editor.document.path.items);
    const shown_name = if (name.len != 0) name else "This map";
    const message = std.fmt.bufPrint(&buffer, "{s} has changes that are not saved.", .{shown_name}) catch
        "This map has changes that are not saved.";
    text(message);
    if (ig.igButton("Save")) state.actions.answer_pending = .save;
    ig.igSameLine();
    if (ig.igButton("Don't save")) state.actions.answer_pending = .dont_save;
    ig.igSameLine();
    if (ig.igButton("Cancel")) state.actions.answer_pending = .cancel;
    if (state.actions.answer_pending != null) ig.igCloseCurrentPopup();
    ig.igEndPopup();
}

/// Spec Errors -> Open: after an open that succeeded (`State.mapOpened`)
/// found objects the object database does not know, a modal names how many
/// of how many types are kept unchanged and not shown, lists each type once
/// with its count (`summarizeUnknown`, most frequent first) in a scrolling
/// child so a mod map with many stays inside one window, and hints at the
/// likely cause. Every name is drawn with `text` (igTextUnformattedEx, never
/// a format string) - T-03-11-01: a map's own object names reach this popup
/// as plain untrusted bytes.
fn drawUnknownObjectsPrompt(state: *State) void {
    if (state.unknown_types_count == 0) return;
    const popup_id = "Objects this game does not know";
    if (!state.unknown_popup_shown) {
        _ = ig.igOpenPopup(popup_id, 0);
        state.unknown_popup_shown = true;
    }
    if (!ig.igBeginPopupModal(popup_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
    var buffer: [200]u8 = undefined;
    const message = std.fmt.bufPrint(&buffer, "{d} objects of {d} types are kept unchanged in the map and are not shown.", .{ state.unknown_objects_total, state.unknown_types_count }) catch
        "Some objects are kept unchanged in the map and are not shown.";
    text(message);
    const rows = @as(f32, @floatFromInt(state.unknown_types_count));
    const child_height = std.math.clamp(rows * ig.igGetTextLineHeightWithSpacing(), ig.igGetTextLineHeightWithSpacing(), 200);
    if (ig.igBeginChild("unknown-types", .{ .x = 320, .y = child_height }, ig.ImGuiChildFlags_Borders, 0)) {
        for (state.unknown_types[0..state.unknown_types_count]) |entry| {
            var row_buffer: [core.bridge.name_capacity + 16]u8 = undefined;
            const row = std.fmt.bufPrint(&row_buffer, "{s} x {d}", .{ entry.nameSlice(), entry.count }) catch entry.nameSlice();
            text(row);
        }
    }
    ig.igEndChild();
    text("Maps made for a mod need that mod: File > Mod.");
    if (ig.igButton("OK")) ig.igCloseCurrentPopup();
    ig.igEndPopup();
}

/// The Settings window's controls (D-25) and `--check`'s round trip (Task 1's
/// own verify) both go through this single path, so the two can never drift:
/// what a setting changes live in the app (today, only the scroll speed) and
/// the flag that tells main.zig's `run` to write `mapeditor.cfg` back.
pub fn applySettings(state: *State) void {
    state.view.wheel_sensitivity = state.settings.scroll_speed;
    state.settings_changed = true;
}

/// Edit > "Settings...": (re)opens the window and loads the maps-folder
/// field's editing buffer fresh from `settings`, so a stale in-progress edit
/// from a previous opening is never shown.
fn openSettingsWindow(state: *State) void {
    state.settings_window_open = true;
    const folder = state.settings.mapsFolder();
    @memset(&state.maps_folder_edit, 0);
    @memcpy(state.maps_folder_edit[0..folder.len], folder);
}

/// D-25's Settings window: scroll/swipe speed, autosave on/off and interval,
/// and the default maps folder. Every control applies at once through
/// `applySettings`; a slider drags live (`state.view.wheel_sensitivity`
/// tracks it every frame) but only marks `settings_changed` - and so only
/// asks main.zig's `run` to write the file - once the drag or the typed text
/// is deactivated, never once per frame of a drag or per keystroke.
fn drawSettingsWindow(state: *State) void {
    if (!state.settings_window_open) return;
    if (!ig.igBegin("Settings", &state.settings_window_open, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        ig.igEnd();
        return;
    }
    defer ig.igEnd();

    var scroll_speed = state.settings.scroll_speed;
    if (ig.igSliderFloatEx("Scroll and swipe speed", &scroll_speed, core.settings.min_scroll_speed, core.settings.max_scroll_speed, "%.2f", ig.ImGuiSliderFlags_Logarithmic)) {
        state.settings.scroll_speed = scroll_speed;
        state.view.wheel_sensitivity = scroll_speed;
    }
    if (ig.igIsItemDeactivatedAfterEdit()) applySettings(state);

    var autosave = state.settings.autosave;
    if (ig.igCheckbox("Autosave", &autosave)) {
        state.settings.autosave = autosave;
        applySettings(state);
    }

    var minutes: c_int = @intCast(state.settings.autosave_minutes);
    if (ig.igSliderInt("Every ... minutes", &minutes, @intCast(core.settings.min_autosave_minutes), @intCast(core.settings.max_autosave_minutes))) {
        state.settings.autosave_minutes = @intCast(minutes);
    }
    if (ig.igIsItemDeactivatedAfterEdit()) applySettings(state);

    var plain_folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    var hint_buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
    const default_folder = logic.defaultMapsFolder(&plain_folder_buffer, userRoot(state), state.modFolder()) orelse "";
    const hint_z = std.fmt.bufPrintZ(&hint_buffer, "{s}", .{default_folder}) catch "";
    _ = ig.igInputTextWithHint("Maps folder", hint_z.ptr, &state.maps_folder_edit, state.maps_folder_edit.len + 1, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        state.settings.setMapsFolder(std.mem.sliceTo(&state.maps_folder_edit, 0));
        applySettings(state);
    }
    if (ig.igButton("Use default")) {
        state.settings.setMapsFolder("");
        @memset(&state.maps_folder_edit, 0);
        applySettings(state);
    }
}

/// D-17: Open and Save As both start in the user maps folder (or the active
/// mod's), created first if it does not exist yet - a fresh install has
/// none of it. D-25: the Settings window's own "Maps folder", when set,
/// overrides that default outright. A folder that cannot be resolved (a bad
/// mod folder name) or created just leaves SDL to its own default_location
/// rather than failing the dialog.
/// Where the Open and Save As dialogs start: the Settings window's maps
/// folder when one is set (D-25), otherwise the user maps folder of the
/// active mod - `<user_root>mods/<Folder>/maps`, or `<user_root>maps` for
/// None (D-28) - so after File > Mod it follows the new mod. A relative
/// maps folder typed into Settings is under the user root, never the working
/// directory (panels_logic.dialogFolderFor). Null when the path does not fit
/// `buffer`.
pub fn dialogFolder(state: *const State, buffer: []u8) ?[]const u8 {
    return logic.dialogFolderFor(buffer, state.settings.mapsFolder(), userRoot(state), state.modFolder());
}

fn showDialog(state: *State, kind: logic.DialogKind) void {
    const slot: *logic.PathSlot = state.actions.dialog;
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    var folder_z_buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
    var default_location: ?[*:0]const u8 = null;
    const folder = dialogFolder(state, &folder_buffer);
    if (folder) |f| {
        std.Io.Dir.cwd().createDirPath(state.io, f) catch {};
        if (std.fmt.bufPrintZ(&folder_z_buffer, "{s}", .{f})) |z| default_location = z.ptr else |_| {}
    }
    if (!state.os_dialogs) return;
    state.os_dialogs_opened += 1;
    switch (kind) {
        .open => sdl3.c.SDL_ShowOpenFileDialog(dialogCallback, slot, state.window, &map_filters, map_filters.len, default_location, false),
        .save_as => sdl3.c.SDL_ShowSaveFileDialog(dialogCallback, slot, state.window, &map_filters, map_filters.len, default_location),
    }
}

/// SDL calls this when the user chose, cancelled or the dialog failed -
/// maybe on another thread, maybe before SDL_Show*FileDialog returned. It
/// only hands the answer to the slot; `act` does something with it in the
/// frame loop.
fn dialogCallback(userdata: ?*anyopaque, filelist: [*c]const [*c]const u8, filter: c_int) callconv(.c) void {
    _ = filter;
    const slot: *logic.PathSlot = @ptrCast(@alignCast(userdata orelse return));
    if (filelist == null) {
        const reason = sdl3.c.SDL_GetError();
        slot.deliverFailure(if (reason != null) std.mem.span(reason) else "no reason given");
        return;
    }
    const first = filelist[0];
    slot.deliver(if (first != null) std.mem.span(first) else null);
}

/// Once per frame, after `act` (main.zig's run loop): polls the running test
/// game without blocking and feeds its exit to the prompt (D-06). Safe to
/// call with no test game running.
pub fn pollTestGame(state: *State) void {
    if (state.test_game) |*running| {
        if (running.poll(state.io)) |exit| {
            state.test_game = null;
            const log_path = state.test_game_log_buffer[0..state.test_game_log_len];
            if (state.test_prompt.gameExited(exit, log_path) == .start) startTestGame(state);
        }
    }
}

/// The menu item or F5: asks first if one is already running (D-06);
/// otherwise starts immediately. Never touches the document (D-01) - no
/// unsaved-changes prompt, even when there is one (spec's own wording).
/// `pub`: smoke.zig's `AutoRunner` (BK_EDITOR_AUTO's `test` action) calls
/// this directly, the same way it already calls `addSoundAtViewCentre`.
pub fn requestTestLaunch(state: *State) void {
    if (!mapIsOpen(state.editor)) return;
    if (state.test_prompt.request(state.test_game != null) == .start) startTestGame(state);
}

/// The window's own display, 1-based, for -monitor (D-05: beside the
/// editor). Null lets the game choose its own default rather than guess.
fn windowMonitor(window: *sdl3.c.SDL_Window) ?u32 {
    const display_id = sdl3.c.SDL_GetDisplayForWindow(window);
    if (display_id == 0) return null;
    var count: c_int = 0;
    const displays = sdl3.c.SDL_GetDisplays(&count) orelse return null;
    defer sdl3.c.SDL_free(displays);
    var i: c_int = 0;
    while (i < count) : (i += 1) {
        if (displays[@intCast(i)] == display_id) return @intCast(i + 1);
    }
    return null;
}

/// `<user_root>mapeditor/test-game.log`, in the buffer's own storage so it
/// outlives the launch that needs it (the log path is read again when the
/// game later exits).
fn testGameLogPath(state: *State) ?[]const u8 {
    const root = std.mem.sliceTo(&state.paths.user_root, 0);
    const text_written = std.fmt.bufPrint(&state.test_game_log_buffer, "{s}mapeditor{c}test-game.log", .{ root, std.fs.path.sep }) catch return null;
    state.test_game_log_len = text_written.len;
    return text_written;
}

/// Writes the test copy and starts the game beside the editor (D-01, D-02,
/// D-04, D-05, D-07, D-08, D-09), then reports how that went through
/// `TestLaunchPrompt.noteLaunch`, which keeps the status line right for
/// every outcome (WINDOWS.md 1): a failed copy shows the bridge's message on
/// the status bar, like any other edit failure, and starts nothing; a spawn
/// that cannot find Game beside MapEditor is the one launch failure the spec
/// calls out for its own modal (Errors -> Test launch); a start clears an
/// earlier attempt's failure from the status bar.
fn startTestGame(state: *State) void {
    var buffer: [512]u8 = undefined;
    const attempt = launchTestGame(state, &buffer);
    state.test_prompt.noteLaunch(attempt, &state.view.status);
}

/// One launch attempt, for `startTestGame`. `buffer` holds a formatted
/// message the returned attempt may point into.
fn launchTestGame(state: *State, buffer: *[512]u8) logic.LaunchAttempt {
    const real = state.real;
    var test_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const test_path = real.testMapPath(testlaunch.profile_name, state.modFolder(), testlaunch.map_file_name, &test_path_buffer) orelse
        return .{ .status_failure = std.mem.span(c.BkEditorLastMessage(real.session)) };
    if (real.saveCopy(test_path) != .ok)
        return .{ .status_failure = std.mem.span(c.BkEditorLastMessage(real.session)) };
    var game_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const game_path = testlaunch.gamePath(state.io, &game_path_buffer) catch |err|
        return .{ .report_failure = std.fmt.bufPrint(buffer, "No game beside the Map Editor: {s}", .{@errorName(err)}) catch "No game beside the Map Editor" };
    const log_path = testGameLogPath(state) orelse
        return .{ .status_failure = "the test log's path is too long" };
    const running = testlaunch.start(state.allocator, state.io, state.environ, .{
        .game_path = game_path,
        .mod_folder = state.modFolder(),
        .monitor = windowMonitor(state.window),
        .log_path = log_path,
        .extra_env = state.test_extra_env,
    }) catch |err| {
        const message = if (err == error.FileNotFound)
            std.fmt.bufPrint(buffer, "No game beside the Map Editor at {s}", .{game_path}) catch "No game beside the Map Editor"
        else
            std.fmt.bufPrint(buffer, "the game would not start: {s}", .{@errorName(err)}) catch "the game would not start";
        return .{ .report_failure = message };
    };
    state.test_game = running;
    return .started;
}

/// D-06's still-running prompt, and the exit report (bad code or an early
/// failure); a clean exit is never shown at all (TestLaunchPrompt.report()).
fn drawTestLaunchModals(state: *State) void {
    const restart_id = "Test in game##restart";
    if (state.test_prompt.isAskingRestart()) {
        if (!state.test_restart_popup_shown) {
            _ = ig.igOpenPopup(restart_id, 0);
            state.test_restart_popup_shown = true;
        }
    } else {
        state.test_restart_popup_shown = false;
    }
    if (ig.igBeginPopupModal(restart_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        text("A test game is still running.");
        if (ig.igButton("Restart with this version")) {
            state.test_prompt.answer(.restart);
            if (state.test_game) |*running| running.terminate(state.io);
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton("Keep it running")) {
            state.test_prompt.answer(.keep);
            ig.igCloseCurrentPopup();
        }
        ig.igEndPopup();
    }

    const report_id = "Test in game##report";
    if (state.test_prompt.report() != null) {
        if (!state.test_report_popup_shown) {
            _ = ig.igOpenPopup(report_id, 0);
            state.test_report_popup_shown = true;
        }
    } else {
        state.test_report_popup_shown = false;
    }
    if (state.test_prompt.report()) |message| {
        if (ig.igBeginPopupModal(report_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            text(message);
            if (ig.igButton("OK")) {
                state.test_prompt.acknowledgeReport();
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
}

/// Returns the bar's height, which the other panels start below.
fn drawMenuBar(state: *State) f32 {
    if (!ig.igBeginMainMenuBar()) return 0;
    const height = ig.igGetFrameHeight();
    const editor = state.editor;
    const map_open = mapIsOpen(editor);
    if (ig.igBeginMenu("File")) {
        if (ig.igMenuItemEx("Open...", null, false, true)) state.actions.open_requested = true;
        if (ig.igBeginMenu("Open Recent")) {
            if (!state.recent_menu_open_prev) refreshRecentExistsCache(state);
            state.recent_menu_open_prev = true;
            drawOpenRecentItems(state);
            ig.igEndMenu();
        } else {
            state.recent_menu_open_prev = false;
        }
        // D-26: None and every installed mod; the active one checked.
        // Switching guards through the same unsaved-changes prompt as Open.
        if (ig.igBeginMenu("Mod")) {
            if (!state.mod_menu_open_prev) refreshModList(state);
            state.mod_menu_open_prev = true;
            drawModItems(state);
            ig.igEndMenu();
        } else {
            state.mod_menu_open_prev = false;
        }
        if (ig.igMenuItemEx("Save", null, false, map_open)) state.actions.save_requested = true;
        if (ig.igMenuItemEx("Save As...", null, false, map_open)) state.actions.save_as_requested = true;
        // 03-15 gap fix: through the unsaved-changes prompt (D-23), like Open.
        if (ig.igMenuItemEx("Close", close_shortcut_label, false, map_open)) state.actions.close_requested = true;
        ig.igSeparator();
        // D-21: switchable from the menu, not only the Settings window.
        var autosave_on = state.settings.autosave;
        if (ig.igMenuItemBoolPtr("Autosave", null, &autosave_on, true)) {
            state.settings.autosave = autosave_on;
            applySettings(state);
        }
        ig.igSeparator();
        if (ig.igMenuItemEx("Quit", null, false, true)) state.actions.quit_requested = true;
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("Edit")) {
        if (ig.igMenuItemEx("Undo", "Ctrl+Z", false, editor.history.canUndo())) state.view.undo(editor);
        if (ig.igMenuItemEx("Redo", "Ctrl+Y", false, editor.history.canRedo())) state.view.redo(editor);
        ig.igSeparator();
        if (ig.igMenuItemEx("Settings...", null, false, true)) openSettingsWindow(state);
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("Map")) {
        drawMapMenu(state, map_open);
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("Tools")) {
        inline for (.{ .{ "Select", "1", Tool.select }, .{ "Brush", "2", Tool.brush }, .{ "Place", "3", Tool.place } }) |item| {
            if (ig.igMenuItemEx(item[0], item[1], state.view.tool == item[2], true)) state.view.selectTool(editor, item[2]);
        }
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("View")) {
        if (ig.igMenuItemEx("Reset view", "Home", false, map_open)) state.view.resetView(state.real);
        if (ig.igBeginMenu("Markers")) {
            inline for (comptime std.enums.values(marker_logic.MarkerKind)) |kind| {
                var on = state.marker_set.has(kind);
                if (ig.igMenuItemBoolPtr(kind.label(), null, &on, true)) state.marker_set.setKind(kind, on);
            }
            ig.igEndMenu();
        }
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("Test")) {
        if (ig.igMenuItemEx("Test in game", "F5", false, map_open)) requestTestLaunch(state);
        ig.igEndMenu();
    }
    ig.igEndMainMenuBar();
    return height;
}

/// Map > Player camera (D-22): the ground point under the screen's centre
/// becomes the anchor of the current player (the placer's player) or the
/// neutral one - the same commands the Camera anchors panel and
/// BK_EDITOR_AUTO's `do=` run.
fn drawMapMenu(state: *State, map_open: bool) void {
    if (ig.igBeginMenu("Player camera")) {
        const player: i32 = @max(state.view.placer.player, 0);
        var label: [48:0]u8 = undefined;
        const label_text = std.fmt.bufPrintZ(&label, "Set camera for player {d}", .{player}) catch "Set camera for player";
        if (ig.igMenuItemEx(label_text, null, false, map_open)) _ = commands.setAnchorAtViewCentre(state, player);
        if (ig.igMenuItemEx("Set neutral camera", null, false, map_open)) _ = commands.setAnchorAtViewCentre(state, commands.neutral_slot);
        ig.igEndMenu();
    }
}

/// File > Open Recent (D-27): every entry's existence, checked once for the
/// frame the submenu newly opened - std.Io.Dir access, so it never runs more
/// than once per entry per opening, not once per entry per frame it stays
/// open.
fn refreshRecentExistsCache(state: *State) void {
    var i: usize = 0;
    while (i < state.settings.recentCount()) : (i += 1) {
        state.recent_exists_cache[i] = pathExists(state.io, state.settings.recentAt(i));
    }
}

fn pathExists(io: std.Io, os_path: []const u8) bool {
    _ = std.Io.Dir.cwd().statFile(io, os_path, .{}) catch return false;
    return true;
}

/// The submenu's own items: each entry shows its file name (the full path as
/// a tooltip), disabled with a "Remove" beside it when the cached check found
/// it missing; choosing an existing one guards through the unsaved-changes
/// prompt like Open itself (D-23, D-27). "Clear list" empties it outright.
fn drawOpenRecentItems(state: *State) void {
    const count = state.settings.recentCount();
    if (count == 0) {
        ig.igTextDisabled("(none)");
        return;
    }
    var index: usize = 0;
    while (index < count) {
        const path = state.settings.recentAt(index);
        const exists = state.recent_exists_cache[index];
        ig.igPushIDInt(@intCast(index));
        var name_buffer: [300:0]u8 = undefined;
        const name_z = std.fmt.bufPrintZ(&name_buffer, "{s}", .{logic.baseName(path)}) catch "?";
        var removed = false;
        if (ig.igMenuItemEx(name_z, null, false, exists)) state.actions.requestOpenPath(path);
        if (ig.igIsItemHovered(0) and ig.igBeginTooltip()) {
            text(path);
            ig.igEndTooltip();
        }
        if (!exists) {
            ig.igSameLine();
            if (ig.igSmallButton("Remove")) {
                state.settings.removeRecent(index);
                state.settings_changed = true;
                removed = true;
            }
        }
        ig.igPopID();
        // A removal shifted every later entry down one index; the cache
        // needs the same shift so it still names the right file next frame -
        // simplest is to fully refresh it now, while `index` stays put to
        // draw whatever moved into this slot.
        if (removed) {
            refreshRecentExistsCache(state);
        } else {
            index += 1;
        }
    }
    ig.igSeparator();
    if (ig.igMenuItemEx("Clear list", null, false, true)) {
        while (state.settings.recentCount() != 0) state.settings.removeRecent(0);
        state.settings_changed = true;
    }
}

/// File > Mod (D-26): every installed mod, once for the frame the submenu
/// newly opened - `BkEditorMods` itself, capped at the cache's own capacity.
fn refreshModList(state: *State) void {
    var count: c_int = 0;
    _ = c.BkEditorMods(state.real.session, &state.mod_list_buffer, @intCast(state.mod_list_buffer.len), &count);
    state.mod_list_count = if (count < 0) 0 else @min(@as(usize, @intCast(count)), state.mod_list_buffer.len);
}

/// The submenu's own items: "None" and each installed mod ("<name> <version>",
/// its folder as a tooltip), the active one checked. Choosing one queues the
/// switch through the unsaved-changes prompt (D-23) - `act` closes the map
/// and makes the actual switch once it is guarded; choosing the active one
/// again does nothing at all (`requestSwitchMod`'s own no-op).
fn drawModItems(state: *State) void {
    const active = state.modFolder();
    if (ig.igMenuItemEx("None", null, active == null, true)) state.actions.requestSwitchMod("", active);
    var i: usize = 0;
    while (i < state.mod_list_count) : (i += 1) {
        const mod = state.mod_list_buffer[i];
        const folder = std.mem.sliceTo(&mod.folder, 0);
        const checked = if (active) |a| std.mem.eql(u8, a, folder) else false;
        var label_buffer: [130:0]u8 = undefined;
        const label = std.fmt.bufPrintZ(&label_buffer, "{s} {s}", .{ std.mem.sliceTo(&mod.name, 0), std.mem.sliceTo(&mod.version, 0) }) catch "?";
        if (ig.igMenuItemEx(label, null, checked, true)) state.actions.requestSwitchMod(folder, active);
        if (ig.igIsItemHovered(0) and ig.igBeginTooltip()) {
            text(folder);
            ig.igEndTooltip();
        }
    }
}

/// File > Mod's own step (D-26, revised 2026-09-29 in the hand try:
/// switching closes the map). The unsaved-changes prompt (D-23) has already
/// been answered by the time `act` gets here - a Save landed, or Don't save
/// abandoned the edits - so the map is closed rather than reopened under the
/// new mod: a map read under one object database and shown under another
/// mixed the two (saved under AchtungPanzer2, switched to None: 1359 unknown
/// objects). The editor is left with no document - title, status, undo
/// history, autosave, the view and the panels all as at a start with no
/// map - and the palette follows the new mod. The Open dialog's default
/// folder follows it too, through `showDialog`'s own `state.modFolder()`.
///
/// A refusal (BkEditorSetMod checks the folder before touching anything)
/// shows the bridge's reason and leaves the mod and the open map exactly as
/// they were. A failure partway has already closed the engine's map, so the
/// document is closed too and the palette re-read from whatever the engine
/// now holds.
///
/// A running test game is its own process on its own copy of the map
/// (D-01/D-03) and keeps running; `pollTestGame` still reports its exit.
fn performModSwitch(state: *State, folder: []const u8) void {
    const had_map = mapIsOpen(state.editor);
    switch (logic.switchModClosingMap(state.editor, state.real, folder)) {
        .refused => {
            state.view.setStatus("the mod would not load: ", std.mem.span(c.BkEditorLastMessage(state.real.session)));
            return;
        },
        .failed => {
            state.view.setStatus("failed: the mod switch did not finish: ", std.mem.span(c.BkEditorLastMessage(state.real.session)));
        },
        .switched => {
            state.setModFolder(if (folder.len == 0) null else folder);
            state.view.clearStatus();
        },
    }
    mapClosed(state, had_map);
    state.reloadCatalogue();
}

/// Everything that followed the document, once `editor.close` has emptied
/// it: the view as with no map, the panels' per-map state (`mapOpened`
/// already resets it all when no map is open), the autosave schedule idle,
/// and this document's recovery copy deleted - the map was either saved
/// (which deleted it already) or its edits abandoned by Don't save, the
/// same rule a quit follows (D-22). `had_map` false (the switch ran with no
/// map open) leaves the recovery bookkeeping alone: there was no document
/// for it to belong to.
fn mapClosed(state: *State, had_map: bool) void {
    state.view.closeMap();
    state.mapOpened();
    state.autosave.note(0, false);
    if (had_map) deleteRecoveryIfActive(state);
    state.shipped_known = false;
}

/// File > Close's own step (03-15 gap fix): the unsaved-changes prompt
/// (D-23) has been answered by the time `act` gets here - Save landed or
/// Don't save abandoned the edits - so the engine's map and the document
/// close (`logic.closeMapAndDocument`) and everything that followed the
/// document goes with them (`mapClosed`: the view as with no map, the
/// per-map panels, autosave idle, the recovery copy deleted). The editor is
/// left as at a start with no map: title "Map Editor [<mod>]", status bar
/// "no map open". The mod and the palette stay as they are. A test game
/// keeps running, as it does across a mod switch.
fn performClose(state: *State) void {
    const had_map = mapIsOpen(state.editor);
    const result = logic.closeMapAndDocument(state.editor, state.real);
    mapClosed(state, had_map);
    if (result == .ok) {
        state.view.clearStatus();
    } else {
        state.view.setStatus("the engine did not close the map cleanly: ", std.mem.span(c.BkEditorLastMessage(state.real.session)));
    }
}

/// File > Close's shortcut, as the menu shows it: Cmd+W on macOS, Ctrl+W
/// elsewhere - `closeShortcutPressed` takes either modifier on every
/// platform, as view.zig's own Cmd/Ctrl+Z does.
const close_shortcut_label: [*:0]const u8 = if (builtin.os.tag == .macos) "Cmd+W" else "Ctrl+W";

/// Cmd+W or Ctrl+W this frame, not while a text field is being typed in
/// (the same WantTextInput rule F5 follows), and not held down (a held W
/// must not close the next map the moment one opens).
fn closeShortcutPressed() bool {
    const io = ig.igGetIO();
    if (io.*.WantTextInput) return false;
    if (!io.*.KeyCtrl and !io.*.KeySuper) return false;
    return ig.igIsKeyPressedEx(ig.ImGuiKey_W, false);
}

/// Every panel's widgets leave room for their labels to the right.
const label_room: f32 = 110;

/// `cond` is `ImGuiCond_FirstUseEver` on an ordinary frame (a no-op once the
/// window has been drawn once - Dear ImGui's own behavior) or
/// `ImGuiCond_Always` for the one frame after the viewport itself resized
/// (Task 2, carried from plan 5: the right-hand panels used to stay wherever
/// `FirstUseEver` first put them, since a plain resize never re-applied).
/// `track_width`, when given, is written back with the window's own live
/// width right after `igBegin` - the caller's column keeps whatever width the
/// user last dragged it to, even on an `Always` frame, because `size.x` was
/// itself read back from here the frame before.
pub fn beginPanel(name: [*:0]const u8, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond, track_width: ?*f32) bool {
    ig.igSetNextWindowPos(pos, cond);
    ig.igSetNextWindowSize(size, cond);
    const open = ig.igBegin(name, null, ig.ImGuiWindowFlags_NoCollapse);
    if (open) {
        ig.igPushItemWidth(-label_room);
        if (track_width) |w| w.* = ig.igGetWindowWidth();
    }
    return open;
}

/// Ends what beginPanel began; igEnd whether or not it was open.
pub fn endPanel(open: bool) void {
    if (open) ig.igPopItemWidth();
    ig.igEnd();
}

pub fn text(slice: []const u8) void {
    ig.igTextUnformattedEx(slice.ptr, slice.ptr + slice.len);
}

fn drawToolPalette(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = beginPanel("Tools", pos, size, cond, &state.left_width);
    defer endPanel(open);
    if (!open) return;
    const view = state.view;
    inline for (.{ .{ "Select", Tool.select }, .{ "Brush", Tool.brush }, .{ "Place", Tool.place } }, 0..) |item, index| {
        if (index != 0) ig.igSameLine();
        const active = view.tool == item[1];
        // The active tool's button wears the pressed colour.
        if (active) ig.igPushStyleColorImVec4(ig.ImGuiCol_Button, ig.igGetStyleColorVec4(ig.ImGuiCol_ButtonActive).*);
        if (ig.igButton(item[0])) view.selectTool(state.editor, item[1]);
        if (active) ig.igPopStyleColor();
    }
    ig.igSeparatorText("Brush");
    if (state.tile_count == 0) {
        if (!mapIsOpen(state.editor)) {
            text("no map open: no tiles to paint");
        } else {
            // Task 5, carried from plan 5: this used to say "no map open"
            // here too, dropping the bridge's own reason (tilesetTiles
            // failed) or saying nothing about why (the tileset is empty).
            var buffer: [300]u8 = undefined;
            const reason = state.tileReason();
            const message = std.fmt.bufPrint(&buffer, "no tiles to paint: {s}", .{reason}) catch "no tiles to paint";
            text(message);
        }
    } else {
        drawTilePicker(state);
    }
    var radius: c_int = view.brush.radius;
    if (ig.igSliderInt("radius", &radius, 0, 4)) view.brush.radius = radius;
    if (state.tile_pictures.queue.pendingCount() != 0) {
        if (state.real.gpuDevice()) |device| state.tile_pictures.pump(state.real, device, tile_picture_pump_budget);
    }
}

/// Tile pictures decoded per frame. One is ~0.14 ms once the tileset
/// texture is decoded (the first, ~6 ms, decodes it) - measured by the
/// engine tier on coldwinter - so a whole tileset (184 tiles there) fills
/// the open picker within a few frames.
const tile_picture_pump_budget: usize = 32;
/// A tile's cell in the picker: the picture (a shipped tile is 64x32) over
/// its number.
const tile_cell_width: f32 = 64;
const tile_picture_height: f32 = 32;
/// The current tile's picture beside the combo.
const tile_preview_width: f32 = 40;
/// Columns the picker's grid aims for; the popup is made that wide.
const tile_picker_columns: f32 = 6;

/// The Brush's tile picker (03-15 gap fix, Johannes's M1 hand try: "It would
/// be great to see the tile as graphics next to the name and number;
/// otherwise hard to choose"): the current tile's picture beside a combo
/// naming it ("tile 12 - Snow"); the combo opens onto every tile of the
/// tileset as a grid of pictures with their numbers, in sections headed by
/// the terrain type, the current tile highlighted and scrolled to. Pictures
/// are decoded by the engine on demand (BkEditorTilePicture) - the grid's
/// own only while it is open - and kept per tileset.
fn drawTilePicker(state: *State) void {
    const view = state.view;
    const entries = state.tileEntries();
    var key_buffer: [3]u8 = undefined;
    drawTilePicture(state, logic.tileKey(&key_buffer, view.brush.tile), tile_preview_width, tile_preview_width / 2, ig.igGetCursorScreenPos());
    ig.igDummy(.{ .x = tile_preview_width, .y = ig.igGetFrameHeight() });
    ig.igSameLine();

    var preview: [96]u8 = undefined;
    const current: logic.TileEntry = if (logic.indexOfTile(entries, view.brush.tile)) |index| entries[index] else .{ .tile = view.brush.tile };
    const preview_text = logic.tileLabel(&preview, current);
    const style = ig.igGetStyle();
    const grid_width = tile_picker_columns * (tile_cell_width + style.*.ItemSpacing.x) + 2 * style.*.WindowPadding.x + style.*.ScrollbarSize;
    const viewport_height = ig.igGetMainViewport().*.Size.y;
    ig.igSetNextWindowSizeConstraints(.{ .x = grid_width, .y = 0 }, .{ .x = grid_width, .y = @max(viewport_height * 0.6, 200) }, null, null);
    // The whole rest of the row, unlabelled: the picture beside it says what
    // it is, and the panel is too narrow for "tile 41 - Dirty Snow" and a
    // label both. The popup opaque: it lies over the map and the palette.
    ig.igSetNextItemWidth(-std.math.floatMin(f32));
    var popup_bg = ig.igGetStyleColorVec4(ig.ImGuiCol_PopupBg).*;
    popup_bg.w = 1;
    ig.igPushStyleColorImVec4(ig.ImGuiCol_PopupBg, popup_bg);
    const combo_open = ig.igBeginCombo("##tile", preview_text.ptr, ig.ImGuiComboFlags_HeightLargest);
    // Popped in the window it was pushed in (ImGui checks each window's
    // style stack is where its Begin left it): here when the popup is shut,
    // after igEndCombo when it is open.
    defer ig.igPopStyleColor();
    // Read only while the popup is shut: once it is open, the last item is
    // the popup window's own, not the combo.
    if (!combo_open) {
        const combo_min = ig.igGetItemRectMin();
        const combo_max = ig.igGetItemRectMax();
        state.tile_combo_centre = .{ .x = (combo_min.x + combo_max.x) / 2, .y = (combo_min.y + combo_max.y) / 2 };
    }
    state.tile_picker_open = combo_open;
    state.tile_cell_centres = [_]?ig.ImVec2{null} ** 256;
    if (!combo_open) return;
    defer ig.igEndCombo();
    drawTileGrid(state);
}

fn drawTileGrid(state: *State) void {
    const entries = state.tileEntries();
    const style = ig.igGetStyle();
    const columns = logic.gridColumns(ig.igGetContentRegionAvail().x, tile_cell_width, style.*.ItemSpacing.x);
    const cell_height = tile_picture_height + ig.igGetTextLineHeight() + 4;
    var start: usize = 0;
    while (start < entries.len) {
        const group = logic.nextTileGroup(entries, start);
        defer start = group.end;
        var header: [300]u8 = undefined;
        const terrain = entries[group.start].terrain.slice();
        const header_text = std.fmt.bufPrintZ(&header, "{s} ({d})", .{ if (terrain.len != 0) terrain else "other tiles", group.end - group.start }) catch "tiles";
        ig.igSeparatorText(header_text.ptr);
        for (entries[group.start..group.end], 0..) |entry, i| {
            if (i % columns != 0) ig.igSameLine();
            drawTileCell(state, entry, cell_height);
        }
    }
}

/// One cell of the grid: a selectable the size of the picture and its
/// number, the picture drawn over it once decoded (an empty frame until
/// then), the current tile's cell selected and outlined. Choosing a cell
/// sets the brush's tile (`State.chooseBrushTile`) and closes the popup.
fn drawTileCell(state: *State, entry: logic.TileEntry, cell_height: f32) void {
    ig.igPushIDInt(entry.tile);
    defer ig.igPopID();
    const selected = entry.tile == state.view.brush.tile;
    const top_left = ig.igGetCursorScreenPos();
    if (ig.igSelectableEx("##tile", selected, 0, .{ .x = tile_cell_width, .y = cell_height })) state.chooseBrushTile(entry.tile);
    if (selected) {
        ig.igSetItemDefaultFocus();
        if (ig.igIsWindowAppearing()) ig.igSetScrollHereY(0.5);
    }
    const visible = ig.igIsItemVisible();
    if (visible) state.tile_cell_centres[entry.tile] = .{ .x = top_left.x + tile_cell_width / 2, .y = top_left.y + cell_height / 2 };
    if (ig.igIsItemHovered(0) and ig.igBeginTooltip()) {
        var label: [96]u8 = undefined;
        text(logic.tileLabel(&label, entry));
        ig.igEndTooltip();
    }
    // Requested only once the cell scrolls into view: a tileset's texture
    // is decoded once, and its tiles cost little, but there is no call for
    // pictures nobody looks at.
    var key_buffer: [3]u8 = undefined;
    const key = logic.tileKey(&key_buffer, entry.tile);
    if (!visible) return;
    drawTilePicture(state, key, tile_cell_width, tile_picture_height, top_left);
    const draw_list = ig.igGetWindowDrawList();
    var number: [4]u8 = undefined;
    const number_text = std.fmt.bufPrint(&number, "{d}", .{entry.tile}) catch "?";
    const text_pos = ig.ImVec2{ .x = top_left.x + 2, .y = top_left.y + tile_picture_height + 2 };
    ig.ImDrawList_AddTextImFontPtrEx(draw_list, ig.igGetFont(), ig.igGetFontSize(), text_pos, ig.igGetColorU32(ig.ImGuiCol_Text), number_text.ptr, number_text.ptr + number_text.len, 0, null);
    if (selected) {
        const bottom_right = ig.ImVec2{ .x = top_left.x + tile_cell_width, .y = top_left.y + cell_height };
        ig.ImDrawList_AddRectEx(draw_list, top_left, bottom_right, ig.igGetColorU32(ig.ImGuiCol_PlotHistogram), 0, 2, 0);
    }
}

/// A tile's picture in a `width` x `height` box at `top_left` (screen), its
/// shape kept and centred - requested from the tile cache if it is not
/// there yet; a thin frame while it is pending, the frame alone if the
/// tile has none. Draw-list only: the caller reserves the space.
fn drawTilePicture(state: *State, key: []const u8, width: f32, height: f32, top_left: ig.ImVec2) void {
    state.tile_pictures.request(key);
    const draw_list = ig.igGetWindowDrawList();
    switch (state.tile_pictures.lookup(key)) {
        .ready => |ready| {
            const w: f32 = @floatFromInt(ready.width);
            const h: f32 = @floatFromInt(ready.height);
            const scale = @min(width / w, height / h);
            const left = top_left.x + (width - w * scale) / 2;
            const top = top_left.y + (height - h * scale) / 2;
            ig.ImDrawList_AddImage(draw_list, pictureTextureRef(ready.texture), .{ .x = left, .y = top }, .{ .x = left + w * scale, .y = top + h * scale });
        },
        .pending, .missing => ig.ImDrawList_AddRect(draw_list, top_left, .{ .x = top_left.x + width, .y = top_left.y + height }, ig.igGetColorU32(ig.ImGuiCol_Border)),
    }
}

fn drawObjectPalette(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = beginPanel("Objects", pos, size, cond, null);
    defer endPanel(open);
    if (!open) return;
    _ = ig.igInputTextWithHint("##filter", "filter", &state.filter, state.filter.len + 1, 0);
    const filter = std.mem.sliceTo(&state.filter, 0);
    if (state.catalogue.len == 0) {
        text("the object catalogue is empty");
        return;
    }
    const placing = state.view.placer.name;
    var start: usize = 0;
    while (start < state.order.len) {
        const game_type = state.catalogue[state.order[start]].game_type;
        var end = start;
        var matches: usize = 0;
        while (end < state.order.len and state.catalogue[state.order[end]].game_type == game_type) : (end += 1) {
            if (logic.matchesFilter(std.mem.sliceTo(&state.catalogue[state.order[end]].name, 0), filter)) matches += 1;
        }
        defer start = end;
        if (matches == 0) continue;
        var header: [96:0]u8 = undefined;
        // "###" keeps the header's ID the type alone, so its open state
        // survives the count changing as the filter does.
        const header_text = std.fmt.bufPrintZ(&header, "{s} ({d})###type{d}", .{ logic.gameTypeName(game_type), matches, game_type }) catch continue;
        if (filter.len != 0) ig.igSetNextItemOpen(true, ig.ImGuiCond_Always);
        if (!ig.igCollapsingHeader(header_text.ptr, 0)) continue;
        for (state.order[start..end]) |index| {
            const entry = &state.catalogue[index];
            const name = std.mem.sliceTo(&entry.name, 0);
            if (!logic.matchesFilter(name, filter)) continue;
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            // D-29: a picture per object, decoded by the engine on demand -
            // requested only for an open group's rows, drawn from whatever
            // pump already has for it (a same-size placeholder while it is
            // still queued or has none).
            state.pictures.request(name);
            drawPaletteRowPicture(state, name);
            ig.igSameLine();
            const selected = state.view.tool == .place and std.mem.eql(u8, name, placing);
            if (ig.igSelectableEx(&entry.name, selected, 0, .{ .x = 0, .y = 0 })) {
                state.view.setPlacerObject(name);
                state.view.selectTool(state.editor, .place);
            }
        }
    }
    // Once per frame regardless of which groups are open, so a budget of
    // decodes/uploads still drains while nothing new is being requested.
    if (state.real.gpuDevice()) |device| state.pictures.pump(state.real, device, picture_pump_budget);
}

/// Names decoded through the bridge per frame (Pictures.pump's budget).
/// Task 1's engine tier measured 2.226 ms/decode on this host - opening a
/// group of 300 objects at once queues all 300, but this budget caps any one
/// frame's actual decode work to 8 * 2.226 ms =~ 17.8 ms, leaving headroom
/// under a 33 ms (30 fps) frame for the GPU upload and the rest of the
/// panels and the engine's own frame; the remaining names simply arrive a
/// few frames later (03-09-SUMMARY.md).
const picture_pump_budget: usize = 8;

/// One palette row's picture cell, always `palette_picture_size` square:
/// a neutral bordered frame drawn first - visible the instant the row is
/// drawn, in every state - with the real picture overlaid on top once
/// `ready`. Drawing the frame unconditionally (not only for `pending` and
/// `missing`) also covers a real GPU timing gap: a texture `pump` uploads on
/// the very frame a group first opens is not always visible to that same
/// frame's own render yet (measured manually building this task - a fresh
/// upload's row rendered blank until a couple of frames later, even though
/// `lookup` already reported it `ready`; see 03-09-SUMMARY.md) - with the
/// frame always drawn, that row shows the border instead of nothing while
/// the GPU catches up, and the picture simply appears inside it a frame or
/// two afterwards. `missing` (no shipped icon.tga, D-29's fallback) wraps
/// the object's own name inside the frame; `pending` leaves it blank since a
/// picture may still land this frame or the next. Never a per-type symbol
/// either way.
const palette_picture_size: f32 = 48;
fn drawPaletteRowPicture(state: *State, name: []const u8) void {
    const top_left = ig.igGetCursorScreenPos();
    const draw_list = ig.igGetWindowDrawList();
    ig.ImDrawList_AddRect(draw_list, .{ .x = top_left.x, .y = top_left.y }, .{ .x = top_left.x + palette_picture_size, .y = top_left.y + palette_picture_size }, ig.igGetColorU32(ig.ImGuiCol_Border));
    switch (state.pictures.lookup(name)) {
        .ready => |ready| {
            const w: f32 = @floatFromInt(ready.width);
            const h: f32 = @floatFromInt(ready.height);
            const scale = @min(palette_picture_size / w, palette_picture_size / h);
            const bottom_right = ig.ImVec2{ .x = top_left.x + w * scale, .y = top_left.y + h * scale };
            ig.ImDrawList_AddImage(draw_list, pictureTextureRef(ready.texture), top_left, bottom_right);
        },
        .pending => {},
        .missing => drawPlaceholderLabel(draw_list, top_left, name),
    }
    // Reserves the row's layout space - the border and (once ready) the
    // image above are draw-list primitives, which never move the cursor.
    ig.igDummy(.{ .x = palette_picture_size, .y = palette_picture_size });
}

fn pictureTextureRef(texture: *sdl3.c.SDL_GPUTexture) ig.ImTextureRef {
    return .{ ._TexData = null, ._TexID = @intCast(@intFromPtr(texture)) };
}

/// A `missing` row's own name, word-wrapped inside its frame - the same
/// frame and the same wrap width for every object type (D-29: never a
/// per-type symbol).
fn drawPlaceholderLabel(draw_list: *ig.ImDrawList, top_left: ig.ImVec2, name: []const u8) void {
    const text_pos = ig.ImVec2{ .x = top_left.x + 2, .y = top_left.y + 2 };
    const wrap_width = palette_picture_size - 4;
    ig.ImDrawList_AddTextImFontPtrEx(draw_list, ig.igGetFont(), ig.igGetFontSize(), text_pos, ig.igGetColorU32(ig.ImGuiCol_Text), name.ptr, name.ptr + name.len, wrap_width, null);
}

fn drawProperties(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = beginPanel("Properties", pos, size, cond, &state.right_width);
    defer endPanel(open);
    if (!open) return;
    const editor = state.editor;
    // A click elsewhere on the map while a field was being typed in changes
    // the selection before this frame's widgets can report the edit: the
    // typed value (InputFloat writes it as it is typed) goes to the object
    // it was typed for, not to the newly selected one, and not nowhere.
    if (state.edit.active and state.edit.link_id != (editor.selection orelse -1)) {
        commitEdit(state, state.edit.link_id);
        state.edit.active = false;
    }
    const link_id = editor.selection orelse {
        state.edit = .{};
        text("nothing selected");
        return;
    };
    const object = editor.document.find(link_id) orelse {
        state.edit = .{};
        text("nothing selected");
        return;
    };
    const record = object.*;
    labelled("name", record.nameSlice());
    var number: [32]u8 = undefined;
    labelled("link ID", std.fmt.bufPrint(&number, "{d}", .{record.link_id}) catch "?");
    if (logic.readOnlyReason(editor.document.objects.items, record)) |reason| {
        text("kept as it is:");
        text(reason);
        state.edit = .{};
        return;
    }

    const edit = &state.edit;
    if (edit.link_id != link_id or !edit.active) {
        edit.* = .{
            .link_id = link_id,
            .x = record.x,
            .y = record.y,
            .degrees = logic.dirToDegrees(record.dir),
            .degrees_shown = logic.dirToDegrees(record.dir),
            .player = record.player,
        };
    }
    var active = false;
    var committed = false;
    _ = ig.igInputFloatEx("x", &edit.x, 0, 0, "%.1f", 0);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    _ = ig.igInputFloatEx("y", &edit.y, 0, 0, "%.1f", 0);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    _ = ig.igInputFloatEx("direction", &edit.degrees, 0, 0, "%.2f deg", 0);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    const players = editor.document.info.player_count;
    const player_max = @max(players - 1, record.player, 0);
    _ = ig.igSliderInt("player", &edit.player, 0, player_max);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    edit.active = active;

    if (committed) {
        commitEdit(state, link_id);
        edit.active = false;
    }
}

/// The fields' pose, as one `editor.place` with gesture 0: one typed
/// number, one undo step. A refused pose leaves the object as it was; the
/// fields reload from it next frame. An object gone or no longer editable
/// takes nothing.
fn commitEdit(state: *State, link_id: i32) void {
    const editor = state.editor;
    const edit = &state.edit;
    const object = editor.document.find(link_id) orelse return;
    if (logic.readOnlyReason(editor.document.objects.items, object.*) != null) return;
    const original: core.editor.Pose = .{ .x = object.x, .y = object.y, .dir = object.dir, .player = object.player };
    const pose = logic.editedPose(original, edit.x, edit.y, edit.degrees, edit.degrees_shown, edit.player);
    // Task 5, carried from plan 5: a selection change while typing (the check
    // atop drawProperties) can commit here for the object that was just left,
    // and Editor.place's own equal-pose check silently no-ops the second call
    // that pattern used to produce - this is the same check one layer up, so
    // the common case (nothing was actually typed) never reaches the bridge
    // at all. Editor.place's check stays as the backstop.
    if (pose.x == original.x and pose.y == original.y and pose.dir == original.dir and pose.player == original.player) return;
    state.view.noteEditResult(editor, editor.place(link_id, pose, 0));
}

fn labelled(label: []const u8, value: []const u8) void {
    text(label);
    ig.igSameLineEx(90, -1);
    text(value);
}

fn drawPlayers(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = beginPanel("Players", pos, size, cond, null);
    defer endPanel(open);
    if (!open) return;
    const editor = state.editor;
    if (!mapIsOpen(editor)) {
        text("no map open");
        return;
    }
    const info = editor.document.info;
    var map_type: c_int = info.map_type;
    var preview: [32:0]u8 = undefined;
    const known_type = map_type >= 0 and map_type < map_type_names.len;
    const preview_text = if (known_type) map_type_names[@intCast(map_type)] else std.fmt.bufPrintZ(&preview, "type {d}", .{map_type}) catch "type ?";
    if (ig.igBeginCombo("map type", preview_text.ptr, 0)) {
        for (map_type_names, 0..) |name, index| {
            const selected = index == map_type;
            if (ig.igSelectableEx(name.ptr, selected, 0, .{ .x = 0, .y = 0 })) map_type = @intCast(index);
        }
        ig.igEndCombo();
    }
    if (map_type != info.map_type) state.view.noteEditResult(editor, editor.setMapType(map_type));

    var attacking: c_int = info.attacking_side;
    if (ig.igCombo("attacking side", &attacking, "side 0\x00side 1\x00") and attacking != info.attacking_side)
        state.view.noteEditResult(editor, editor.setAttackingSide(attacking));

    ig.igSeparatorText("Diplomacy");
    for (editor.document.diplomacy.items, 0..) |side, player| {
        ig.igPushIDInt(@intCast(player));
        defer ig.igPopID();
        var label: [32:0]u8 = undefined;
        const label_text = std.fmt.bufPrintZ(&label, "player {d}", .{player}) catch continue;
        var value: c_int = side;
        if (ig.igCombo(label_text.ptr, &value, "side 0\x00side 1\x00neutral\x00") and value != side)
            state.view.noteEditResult(editor, editor.setDiplomacy(@intCast(player), value));
    }
}

/// The map's own sound list: one row per sound, name and world position and
/// radii; a selected row's fields below the list, committed on deactivation
/// as one `editor.editSound` (`drawProperties`' own pattern) - and "Add at
/// view centre"/"Delete". View markers are `View.drawOverlay`'s (view.zig).
fn drawSounds(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = beginPanel("Sounds", pos, size, cond, null);
    defer endPanel(open);
    if (!open) return;
    const editor = state.editor;
    if (state.sounds_generation_seen != editor.sounds_generation) {
        state.loadSounds();
        state.sounds_generation_seen = editor.sounds_generation;
    }
    if (!mapIsOpen(editor)) {
        text("no map open");
        return;
    }
    // A click on another row, or the selected one going away from under it
    // (undo/redo, a delete elsewhere), while a field was mid-edit commits it
    // first - the same reason `drawProperties` does this for objects.
    if (state.sound_edit.active and (state.selected_sound == null or state.selected_sound.? != state.sound_edit.index))
        commitSoundEdit(state, state.sound_edit.index);

    if (state.sounds.len == 0) {
        text("no sounds");
    } else {
        for (state.sounds, 0..) |sound, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            const name = std.mem.sliceTo(&sound.name, 0);
            var line: [96:0]u8 = undefined;
            const line_text = std.fmt.bufPrintZ(&line, "{s}  ({d:.0}, {d:.0})", .{ name, sound.x, sound.y }) catch continue;
            const selected = state.selected_sound != null and state.selected_sound.? == index;
            if (ig.igSelectableEx(line_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) state.selected_sound = index;
        }
    }

    ig.igSeparator();
    if (ig.igButton("Add at view centre")) addSoundAtViewCentre(state);
    ig.igSameLine();
    if (ig.igButton("Delete")) {
        if (state.selected_sound) |index| if (index < state.sounds.len) {
            state.view.noteEditResult(editor, editor.deleteSound(index));
            state.selected_sound = null;
            state.sound_edit.active = false;
            state.loadSounds();
            state.sounds_generation_seen = editor.sounds_generation;
        };
    }
    // How the game plays these (GameTT/iMissionInternal.cpp hands them to
    // Scene/SoundScene.cpp's map sounds, beside the rivers' own), so a test
    // game that starts its view elsewhere is not mistaken for a silent sound.
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    text(logic.sound_panel_note);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    const index = state.selected_sound orelse {
        state.sound_edit.active = false;
        return;
    };
    if (index >= state.sounds.len) {
        state.selected_sound = null;
        state.sound_edit.active = false;
        return;
    }
    ig.igSeparator();
    const sound = state.sounds[index];
    const edit = &state.sound_edit;
    if (edit.index != index or !edit.active) {
        edit.* = .{
            .index = index,
            .x = sound.x,
            .y = sound.y,
            .repeat_seconds = logic.msToSeconds(sound.repeat_ms),
            .repeat_random_seconds = logic.msToSeconds(sound.repeat_random_ms),
            .mute_in_combat = sound.mute_in_combat,
            .min_radius = sound.min_radius,
            .max_radius = sound.max_radius,
        };
        setSoundName(&edit.name_buffer, std.mem.sliceTo(&sound.name, 0));
    }

    var active = false;
    var committed = false;
    if (ig.igBeginCombo("sound", &edit.name_buffer, 0)) {
        for (state.sound_names) |candidate| {
            ig.igPushIDPtr(candidate.ptr);
            defer ig.igPopID();
            var row: [core.bridge.name_capacity + 1:0]u8 = undefined;
            const row_text = std.fmt.bufPrintZ(&row, "{s}", .{candidate}) catch continue;
            const row_selected = std.mem.eql(u8, candidate, std.mem.sliceTo(&edit.name_buffer, 0));
            if (ig.igSelectableEx(row_text.ptr, row_selected, 0, .{ .x = 0, .y = 0 })) {
                setSoundName(&edit.name_buffer, candidate);
                committed = true;
            }
        }
        ig.igEndCombo();
    }
    _ = ig.igInputFloatEx("x", &edit.x, 0, 0, "%.1f", 0);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    _ = ig.igInputFloatEx("y", &edit.y, 0, 0, "%.1f", 0);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    _ = ig.igInputFloatEx("repeat (s)", &edit.repeat_seconds, 0, 0, "%.1f", 0);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    _ = ig.igInputFloatEx("random repeat (s)", &edit.repeat_random_seconds, 0, 0, "%.1f", 0);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    if (ig.igCheckbox("mute in combat", &edit.mute_in_combat)) committed = true;
    _ = ig.igSliderInt("min radius", &edit.min_radius, 0, 50);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    _ = ig.igSliderInt("max radius", &edit.max_radius, 0, 50);
    active = active or ig.igIsItemActive();
    committed = committed or ig.igIsItemDeactivatedAfterEdit();
    if (logic.soundRadiusError(edit.min_radius, edit.max_radius)) |message| text(message);
    edit.active = active;

    if (committed) commitSoundEdit(state, index);
}

/// `name_buffer` is always null-terminated within its own capacity, the
/// `maps_folder_edit`/`filter` fields' own pattern above.
fn setSoundName(name_buffer: *[core.bridge.name_capacity:0]u8, name: []const u8) void {
    @memset(name_buffer, 0);
    const len = @min(name.len, name_buffer.len);
    @memcpy(name_buffer[0..len], name[0..len]);
}

/// The Sounds panel's selected fields as one `editor.editSound`, one undo
/// step - `commitEdit`'s own pattern for objects. A refused edit leaves the
/// sound as it was; the fields reload from it next frame.
fn commitSoundEdit(state: *State, index: usize) void {
    const editor = state.editor;
    const edit = &state.sound_edit;
    if (index >= state.sounds.len) return;
    var record: core.bridge.SoundRecord = .{
        .x = edit.x,
        .y = edit.y,
        .z = state.sounds[index].z,
        .repeat_ms = logic.secondsToMs(edit.repeat_seconds),
        .repeat_random_ms = logic.secondsToMs(edit.repeat_random_seconds),
        .mute_in_combat = edit.mute_in_combat,
        .min_radius = edit.min_radius,
        .max_radius = edit.max_radius,
    };
    record.setName(std.mem.sliceTo(&edit.name_buffer, 0));
    state.view.noteEditResult(editor, editor.editSound(index, record, 0));
    state.loadSounds();
    state.sounds_generation_seen = editor.sounds_generation;
}

/// "Add at view centre" (Task 3): the world point under the screen's centre
/// - `editor.resolve`, the same conversion the properties/place tools use -
/// with `logic.defaultSoundName` (the rivers' loop, which the game plays
/// without a break near the view) and the default radii. Nothing happens
/// with no known sound in the catalogue.
/// Public: smoke.zig's own `add_sound_at_view_centre` step calls this
/// directly, the same way it sets `state.actions.save_requested` for a menu
/// item with no widget to click in the smoke's hidden window.
pub fn addSoundAtViewCentre(state: *State) void {
    const editor = state.editor;
    if (!mapIsOpen(editor)) return;
    const name = logic.defaultSoundName(state.sound_names) orelse return;
    const screen = state.real.screenSize() orelse return;
    const pointer = editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0) catch return;
    var record: core.bridge.SoundRecord = .{ .x = pointer.world_x, .y = pointer.world_y };
    record.setName(name);
    state.view.noteEditResult(editor, editor.addSound(-1, record));
    state.loadSounds();
    state.sounds_generation_seen = editor.sounds_generation;
    state.selected_sound = if (state.sounds.len != 0) state.sounds.len - 1 else null;
    state.sound_edit.active = false;
}

fn drawStatusBar(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_Always);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_Always);
    defer ig.igEnd();
    const flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoMove | ig.ImGuiWindowFlags_NoSavedSettings | ig.ImGuiWindowFlags_NoFocusOnAppearing | ig.ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!ig.igBegin("status", null, flags)) return;
    var buffer: [1024]u8 = undefined;
    const line = statusLine(state, &buffer);
    text(line);
}

/// The tool, the hovered tile and map position (or "no map open" - at a
/// start with no map, or after File > Mod closed it), then the editor's last
/// refusal or failure and the view's own failures.
fn statusLine(state: *State, buffer: []u8) []const u8 {
    var len: usize = 0;
    append(buffer, &len, "{t}", .{state.view.tool});
    if (!mapIsOpen(state.editor)) append(buffer, &len, " | no map open", .{});
    if (state.view.hover) |hover| {
        if (hover.tile) |tile| append(buffer, &len, " | tile {d},{d}", .{ tile[0], tile[1] });
        append(buffer, &len, " | map {d:.0},{d:.0}", .{ hover.map_x, hover.map_y });
    }
    const editor_status = state.editor.status();
    const view_status = state.view.statusLine();
    if (view_status.len != 0 and std.mem.endsWith(u8, view_status, editor_status)) {
        // "failed: <reason>" where the editor already holds the reason (or
        // holds nothing): the view's line says it all, once.
        append(buffer, &len, " | {s}", .{view_status});
    } else {
        if (editor_status.len != 0) append(buffer, &len, " | {s}", .{editor_status});
        if (view_status.len != 0) append(buffer, &len, " | {s}", .{view_status});
    }
    return buffer[0..len];
}

/// Appends what fits; a status line cut short is still a status line.
fn append(buffer: []u8, len: *usize, comptime format: []const u8, args: anytype) void {
    const written = std.fmt.bufPrint(buffer[len.*..], format, args) catch {
        len.* = buffer.len;
        return;
    };
    len.* += written.len;
}

/// D-26: the active mod's name, appended after `formatTitle`'s own suffixes -
/// a plain post-process rather than a `formatTitle` parameter, so
/// panels_logic.zig's own `formatTitle` tests (base game, no mod) need no
/// change for this app-layer decoration.
fn updateTitle(state: *State) void {
    var buffer: [256]u8 = undefined;
    const read_only = documentIsShipped(state);
    const base_title = logic.formatTitle(&buffer, state.editor.document.path.items, state.editor.dirty(), read_only);
    var full_buffer: [320:0]u8 = undefined;
    const title: [:0]const u8 = if (state.real.activeMod()) |mod|
        std.fmt.bufPrintZ(&full_buffer, "{s} [{s}]", .{ base_title, std.mem.sliceTo(&mod.name, 0) }) catch base_title
    else
        base_title;
    if (std.mem.eql(u8, title, state.title[0..state.title_len])) return;
    _ = sdl3.c.SDL_SetWindowTitle(state.window, title.ptr);
    const len = @min(title.len, state.title.len);
    @memcpy(state.title[0..len], title[0..len]);
    state.title_len = len;
}
