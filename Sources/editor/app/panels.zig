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
    /// null or empty is the base game; a later plan (03-08, the Mod menu)
    /// fills this in (D-09: the test loads the editor's own mod).
    mod_folder: ?[]const u8 = null,
    test_game_log_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined,
    test_game_log_len: usize = 0,
    test_restart_popup_shown: bool = false,
    test_report_popup_shown: bool = false,
    /// D-23's "Unsaved changes" modal has been opened for the prompt's
    /// current ask - so it is not re-opened every frame while it waits.
    unsaved_popup_shown: bool = false,

    /// The object database, and its indices ordered by game type (stable,
    /// so a type keeps the database's order): the palette's groups are runs
    /// of this order.
    catalogue: []CatalogueEntry = &.{},
    order: []u32 = &.{},
    filter: [64:0]u8 = [_:0]u8{0} ** 64,

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
    /// on the status bar rather than stopping the editor.
    pub fn init(allocator: std.mem.Allocator, editor: *Editor, view: *View, real: *RealBridge, window: *sdl3.c.SDL_Window, io: std.Io, environ: std.process.Environ) State {
        var state: State = .{ .allocator = allocator, .editor = editor, .view = view, .real = real, .window = window, .io = io, .environ = environ };
        if (real.paths(&state.paths) != .ok) state.paths = std.mem.zeroes(c.BkEditorPathSet);
        state.loadCatalogue() catch view.setStatus("failed: ", "the object catalogue did not read");
        state.mapOpened();
        return state;
    }

    pub fn deinit(self: *State) void {
        self.allocator.free(self.catalogue);
        self.allocator.free(self.order);
        self.* = undefined;
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
        const needs_save_as = logic.needsSaveAs(state.editor.document.path.items, baseRoot(state));
        switch (state.actions.next(state.editor.dirty(), needs_save_as)) {
            .none, .ask_unsaved => return quit,
            .dialog_cancelled => {},
            .quit => quit = true,
            .save => state.actions.noteSaveOutcome(saveToDocumentPath(state)),
            .show_dialog => |kind| showDialog(state, kind),
            .act_on_path => |chosen| {
                const result = logic.actOnPath(state.editor, chosen.kind, chosen.path);
                state.view.noteEditResult(state.editor, result);
                if (chosen.kind == .open) {
                    // A failed open may have emptied the document (editor.open
                    // says when); either way the panels follow what is open now.
                    if (result) |_| state.mapOpened() else |_| if (!mapIsOpen(state.editor)) state.mapOpened();
                } else {
                    // Save As: the unsaved-changes prompt, if it asked for
                    // this one, hears whether it landed.
                    state.actions.noteSaveOutcome(if (result) |_| true else |_| false);
                }
            },
            .dialog_failed => |message| state.view.setStatus("the file dialog failed: ", message),
        }
    }
}

/// Saves to the document's own path; true on success. editor.save copies
/// the path before it writes, so handing it its own path is safe.
fn saveToDocumentPath(state: *State) bool {
    if (!mapIsOpen(state.editor)) return false;
    const result = state.editor.save(state.editor.document.path.items);
    state.view.noteEditResult(state.editor, result);
    return if (result) |_| true else |_| false;
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

/// D-17: Open and Save As both start in the user maps folder (or the active
/// mod's), created first if it does not exist yet - a fresh install has
/// none of it. A folder that cannot be resolved (a bad mod folder name) or
/// created just leaves SDL to its own default_location rather than failing
/// the dialog.
fn showDialog(state: *State, kind: logic.DialogKind) void {
    const slot: *logic.PathSlot = state.actions.dialog;
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    var folder_z_buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
    var default_location: ?[*:0]const u8 = null;
    if (logic.defaultMapsFolder(&folder_buffer, userRoot(state), state.mod_folder)) |folder| {
        std.Io.Dir.cwd().createDirPath(state.io, folder) catch {};
        if (std.fmt.bufPrintZ(&folder_z_buffer, "{s}", .{folder})) |z| default_location = z.ptr else |_| {}
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
    const test_path = real.testMapPath(testlaunch.profile_name, state.mod_folder, testlaunch.map_file_name, &test_path_buffer) orelse {
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
        .mod_folder = state.mod_folder,
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
        if (ig.igMenuItemEx("Save", null, false, map_open)) state.actions.save_requested = true;
        if (ig.igMenuItemEx("Save As...", null, false, map_open)) state.actions.save_as_requested = true;
        ig.igSeparator();
        if (ig.igMenuItemEx("Quit", null, false, true)) state.actions.quit_requested = true;
        ig.igEndMenu();
    }
    if (ig.igBeginMenu("Edit")) {
        if (ig.igMenuItemEx("Undo", "Ctrl+Z", false, editor.history.canUndo())) state.view.undo(editor);
        if (ig.igMenuItemEx("Redo", "Ctrl+Y", false, editor.history.canRedo())) state.view.redo(editor);
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
            const selected = state.view.tool == .place and std.mem.eql(u8, name, placing);
            if (ig.igSelectableEx(&entry.name, selected, 0, .{ .x = 0, .y = 0 })) {
                state.view.setPlacerObject(name);
                state.view.selectTool(state.editor, .place);
            }
        }
    }
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

fn updateTitle(state: *State) void {
    var buffer: [256]u8 = undefined;
    const read_only = logic.isShippedMap(state.editor.document.path.items, baseRoot(state));
    const title = logic.formatTitle(&buffer, state.editor.document.path.items, state.editor.dirty(), read_only);
    if (std.mem.eql(u8, title, state.title[0..state.title_len])) return;
    _ = sdl3.c.SDL_SetWindowTitle(state.window, title.ptr);
    const len = @min(title.len, state.title.len);
    @memcpy(state.title[0..len], title[0..len]);
    state.title_len = len;
}
