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
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const core = @import("editor_core");
const c_bridge = @import("c_bridge.zig");
const view_mod = @import("view.zig");
const logic = @import("panels_logic.zig");
const testlaunch = @import("testlaunch.zig");
const pictures_mod = @import("pictures.zig");

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
    filter: [64:0]u8 = [_:0]u8{0} ** 64,

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

    /// open_requested, save_requested, save_as_requested, quit_requested,
    /// and the dialog's hand-over: see panels_logic.FileActions.
    actions: FileActions = .{ .dialog = &dialog_slot },

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
        var state: State = .{ .allocator = allocator, .editor = editor, .view = view, .real = real, .window = window, .io = io, .environ = environ, .pictures = pictures_mod.Pictures.init(allocator) };
        state.setModFolder(mod_folder);
        if (real.paths(&state.paths) != .ok) state.paths = std.mem.zeroes(c.BkEditorPathSet);
        state.loadCatalogue() catch view.setStatus("failed: ", "the object catalogue did not read");
        state.mapOpened();
        return state;
    }

    pub fn deinit(self: *State) void {
        self.pictures.deinit();
        self.allocator.free(self.catalogue);
        self.allocator.free(self.order);
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
        self.allocator.free(self.catalogue);
        self.allocator.free(self.order);
        self.catalogue = &.{};
        self.order = &.{};
        self.loadCatalogue() catch self.view.setStatus("failed: ", "the object catalogue did not read");
    }

    pub fn tiles(self: *const State) []const u8 {
        return self.tile_buffer[0..self.tile_count];
    }

    fn loadCatalogue(self: *State) !void {
        const entries = try self.real.catalogue(self.allocator);
        errdefer self.allocator.free(entries);
        // Only what can be placed: a sound or a tank pit picked from the
        // palette could only ever be refused (logic.isPlaceable).
        var placeable: usize = 0;
        for (entries) |entry| {
            if (logic.isPlaceable(entry.game_type)) placeable += 1;
        }
        const order = try self.allocator.alloc(u32, placeable);
        var next: usize = 0;
        for (entries, 0..) |entry, i| {
            if (!logic.isPlaceable(entry.game_type)) continue;
            order[next] = @intCast(i);
            next += 1;
        }
        std.sort.insertion(u32, order, entries, struct {
            fn less(context: []CatalogueEntry, a: u32, b: u32) bool {
                return context[a].game_type < context[b].game_type;
            }
        }.less);
        self.catalogue = entries;
        self.order = order;
    }

    /// After any open that succeeded, the startup one included: the camera,
    /// the tileset's tiles and the fields follow the new map.
    pub fn mapOpened(self: *State) void {
        self.edit = .{};
        self.tile_count = 0;
        if (!mapIsOpen(self.editor)) return;
        self.view.showMap(self.real, self.editor.document.path.items, self.editor.document.info);
        self.tile_count = if (self.real.tilesetTiles(&self.tile_buffer)) |got| got.len else 0;
        // The brush keeps its tile if the new tileset has it; otherwise it
        // takes the first the tileset has, so it never paints a refusal.
        const offered = self.tiles();
        if (offered.len != 0 and std.mem.indexOfScalar(u8, offered, self.view.brush.tile) == null)
            self.view.brush.tile = offered[0];
    }
};

fn mapIsOpen(editor: *const Editor) bool {
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
    state.view.drawOverlay(state.real);
    const menu_height = drawMenuBar(state);
    // ImGui's own capture flag (WantTextInput, not WantCaptureKeyboard): a
    // properties field mid-edit must keep F5 as a literal keystroke, but a
    // window merely being focused must not swallow it.
    if (ig.igIsKeyPressedEx(ig.ImGuiKey_F5, false) and !ig.igGetIO().*.WantTextInput) requestTestLaunch(state);
    const viewport = ig.igGetMainViewport();
    const size = viewport.*.Size;
    const status_height = ig.igGetFrameHeightWithSpacing() + 4;
    const body_top = menu_height;
    const body_height = @max(size.y - menu_height - status_height, 100);

    drawToolPalette(state, .{ .x = 0, .y = body_top }, .{ .x = layout.left_width, .y = layout.tools_height });
    drawObjectPalette(state, .{ .x = 0, .y = body_top + layout.tools_height }, .{ .x = layout.left_width, .y = @max(body_height - layout.tools_height, 100) });
    const right_x = @max(size.x - layout.right_width, layout.left_width);
    drawProperties(state, .{ .x = right_x, .y = body_top }, .{ .x = layout.right_width, .y = layout.properties_height });
    drawPlayers(state, .{ .x = right_x, .y = body_top + layout.properties_height }, .{ .x = layout.right_width, .y = @max(body_height - layout.properties_height, 100) });
    drawStatusBar(state, .{ .x = 0, .y = size.y - status_height }, .{ .x = size.x, .y = status_height });
    drawTestLaunchModals(state);
    drawUnsavedPrompt(state);
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

/// The file actions the menu asked for, and whatever a dialog delivered,
/// after the frame. True when the editor should quit.
pub fn act(state: *State) bool {
    var quit = false;
    while (true) {
        const needs_save_as = logic.needsSaveAs(state.editor.document.path.items, baseRoot(state), userRoot(state));
        switch (state.actions.next(state.editor.dirty(), needs_save_as)) {
            .none, .ask_unsaved => return quit,
            .dialog_cancelled => {},
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
            .dialog_failed => |message| state.view.setStatus("the file dialog failed: ", message),
            .switch_mod => |folder| performModSwitch(state, folder),
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
    const needs_save_as = logic.needsSaveAs(state.editor.document.path.items, baseRoot(state), userRoot(state));
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
fn showDialog(state: *State, kind: logic.DialogKind) void {
    const slot: *logic.PathSlot = state.actions.dialog;
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    var folder_z_buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
    var default_location: ?[*:0]const u8 = null;
    const custom_folder = state.settings.mapsFolder();
    const folder: ?[]const u8 = if (custom_folder.len != 0) custom_folder else logic.defaultMapsFolder(&folder_buffer, userRoot(state), state.modFolder());
    if (folder) |f| {
        std.Io.Dir.cwd().createDirPath(state.io, f) catch {};
        if (std.fmt.bufPrintZ(&folder_z_buffer, "{s}", .{f})) |z| default_location = z.ptr else |_| {}
    }
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
fn requestTestLaunch(state: *State) void {
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
/// D-04, D-05, D-07, D-08, D-09). A failed copy shows the bridge's message on
/// the status bar, like any other edit failure, and starts nothing; a spawn
/// that cannot find Game beside MapEditor is the one launch failure the spec
/// calls out for its own modal (Errors -> Test launch).
fn startTestGame(state: *State) void {
    const real = state.real;
    var test_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const test_path = real.testMapPath(testlaunch.profile_name, state.modFolder(), testlaunch.map_file_name, &test_path_buffer) orelse {
        state.view.setStatus("test in game: ", std.mem.span(c.BkEditorLastMessage(real.session)));
        return;
    };
    if (real.saveCopy(test_path) != .ok) {
        state.view.setStatus("test in game: ", std.mem.span(c.BkEditorLastMessage(real.session)));
        return;
    }
    var game_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const game_path = testlaunch.gamePath(state.io, &game_path_buffer) catch |err| {
        var buffer: [256]u8 = undefined;
        state.test_prompt.reportFailure(std.fmt.bufPrint(&buffer, "No game beside the Map Editor: {s}", .{@errorName(err)}) catch "No game beside the Map Editor");
        return;
    };
    const log_path = testGameLogPath(state) orelse {
        state.view.setStatus("test in game: ", "the test log's path is too long");
        return;
    };
    const running = testlaunch.start(state.allocator, state.io, state.environ, .{
        .game_path = game_path,
        .mod_folder = state.modFolder(),
        .monitor = windowMonitor(state.window),
        .log_path = log_path,
    }) catch |err| {
        var buffer: [512]u8 = undefined;
        const message = if (err == error.FileNotFound)
            std.fmt.bufPrint(&buffer, "No game beside the Map Editor at {s}", .{game_path}) catch "No game beside the Map Editor"
        else
            std.fmt.bufPrint(&buffer, "the game would not start: {s}", .{@errorName(err)}) catch "the game would not start";
        state.test_prompt.reportFailure(message);
        return;
    };
    state.test_game = running;
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
    if (ig.igBeginMenu("Tools")) {
        inline for (.{ .{ "Select", "1", Tool.select }, .{ "Brush", "2", Tool.brush }, .{ "Place", "3", Tool.place } }) |item| {
            if (ig.igMenuItemEx(item[0], item[1], state.view.tool == item[2], true)) state.view.selectTool(editor, item[2]);
        }
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("View")) {
        if (ig.igMenuItemEx("Reset view", "Home", false, map_open)) state.view.resetView(state.real);
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("Test")) {
        if (ig.igMenuItemEx("Test in game", "F5", false, map_open)) requestTestLaunch(state);
        ig.igEndMenu();
    }
    ig.igEndMainMenuBar();
    return height;
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
/// switch through the unsaved-changes prompt (D-23) - `act` makes the actual
/// switch once it is guarded.
fn drawModItems(state: *State) void {
    const active = state.modFolder();
    if (ig.igMenuItemEx("None", null, active == null, true)) state.actions.requestSwitchMod("");
    var i: usize = 0;
    while (i < state.mod_list_count) : (i += 1) {
        const mod = state.mod_list_buffer[i];
        const folder = std.mem.sliceTo(&mod.folder, 0);
        const checked = if (active) |a| std.mem.eql(u8, a, folder) else false;
        var label_buffer: [130:0]u8 = undefined;
        const label = std.fmt.bufPrintZ(&label_buffer, "{s} {s}", .{ std.mem.sliceTo(&mod.name, 0), std.mem.sliceTo(&mod.version, 0) }) catch "?";
        if (ig.igMenuItemEx(label, null, checked, true)) state.actions.requestSwitchMod(folder);
        if (ig.igIsItemHovered(0) and ig.igBeginTooltip()) {
            text(folder);
            ig.igEndTooltip();
        }
    }
}

/// File > Mod's own step (D-26): switches through the bridge, reloads the
/// palette, and reopens the document's own path if one was open - a Don't
/// save on the unsaved-changes prompt has already said any edits are
/// abandoned, the same as it does for Open (D-22/D-23's own reasoning). A
/// refusal shows the bridge's reason on the status bar and leaves the mod
/// (and the open map) exactly as `BkEditorSetMod`'s own contract promises.
fn performModSwitch(state: *State, folder: []const u8) void {
    const requested: ?[]const u8 = if (folder.len == 0) null else folder;
    if (state.real.setMod(requested) != .ok) {
        state.view.setStatus("the mod would not load: ", std.mem.span(c.BkEditorLastMessage(state.real.session)));
        return;
    }
    state.setModFolder(requested);
    state.reloadCatalogue();
    if (mapIsOpen(state.editor)) {
        // Copied first: editor.open is about to replace document.path
        // itself, the same aliasing hazard editor.save's own doc comment
        // describes for a path argument taken from the document it owns.
        var path_buffer: [logic.PathSlot.max_path]u8 = undefined;
        const len = @min(state.editor.document.path.items.len, path_buffer.len);
        @memcpy(path_buffer[0..len], state.editor.document.path.items[0..len]);
        const result = state.editor.open(path_buffer[0..len]);
        state.view.noteEditResult(state.editor, result);
    }
    state.mapOpened();
}

/// Every panel's widgets leave room for their labels to the right.
const label_room: f32 = 110;

fn beginPanel(name: [*:0]const u8, pos: ig.ImVec2, size: ig.ImVec2) bool {
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin(name, null, ig.ImGuiWindowFlags_NoCollapse);
    if (open) ig.igPushItemWidth(-label_room);
    return open;
}

/// Ends what beginPanel began; igEnd whether or not it was open.
fn endPanel(open: bool) void {
    if (open) ig.igPopItemWidth();
    ig.igEnd();
}

fn text(slice: []const u8) void {
    ig.igTextUnformattedEx(slice.ptr, slice.ptr + slice.len);
}

fn drawToolPalette(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    const open = beginPanel("Tools", pos, size);
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
        text("no map open: no tiles to paint");
    } else {
        var preview: [32:0]u8 = undefined;
        const preview_text = std.fmt.bufPrintZ(&preview, "tile {d}", .{view.brush.tile}) catch "tile";
        if (ig.igBeginCombo("tile", preview_text.ptr, 0)) {
            for (state.tiles()) |tile| {
                var label: [32:0]u8 = undefined;
                const label_text = std.fmt.bufPrintZ(&label, "tile {d}", .{tile}) catch continue;
                const selected = tile == view.brush.tile;
                if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) view.brush.tile = tile;
                if (selected) ig.igSetItemDefaultFocus();
            }
            ig.igEndCombo();
        }
    }
    var radius: c_int = view.brush.radius;
    if (ig.igSliderInt("radius", &radius, 0, 4)) view.brush.radius = radius;
}

fn drawObjectPalette(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    const open = beginPanel("Objects", pos, size);
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

fn drawProperties(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    const open = beginPanel("Properties", pos, size);
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
    state.view.noteEditResult(editor, editor.place(link_id, pose, 0));
}

fn labelled(label: []const u8, value: []const u8) void {
    text(label);
    ig.igSameLineEx(90, -1);
    text(value);
}

fn drawPlayers(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    const open = beginPanel("Players", pos, size);
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

/// The tool, the hovered tile and map position, then the editor's last
/// refusal or failure and the view's own failures.
fn statusLine(state: *State, buffer: []u8) []const u8 {
    var len: usize = 0;
    append(buffer, &len, "{t}", .{state.view.tool});
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
    const read_only = logic.isShippedMap(state.editor.document.path.items, baseRoot(state));
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
