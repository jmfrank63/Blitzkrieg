//! The interactive mode's Tools over tools_logic.zig: File > MOD Settings,
//! Export Result and Compress current MOD to PAK (drawn into lifecycle_ui's
//! File menu), Edit > Set Picture Options, the Tools menu (Set Directories,
//! Export Stats Only, Batch Mode, Run Blitzkrieg), the Editors menu and its
//! combo in the menu bar, their accelerators, and the report window every one
//! of them answers in. Every decision is tools_logic's; this file draws, shows
//! the OS dialogs and does the file I/O (gamma.cfg) and the game launch.
//!
//! Batch Mode runs BkResBatch in the frame that asked for it: the bridge has
//! no progress callback, so MFC's progress dialog becomes the report shown
//! when the batch is done.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const kit = @import("editor_kit");
const core = @import("resource_core");
const lifecycle = @import("lifecycle.zig");
const lifecycle_ui = @import("lifecycle_ui.zig");
const logic = @import("panels_logic.zig");
const tools = @import("tools_logic.zig");
const batch_cli = @import("batch_cli.zig");

const ig = imgui.c;
const Kind = core.bridge.Kind;
const ModSettings = core.bridge.ModSettings;
const testlaunch = kit.testlaunch;

const field_capacity = 1024;
const shortcut_label = if (builtin.os.tag == .macos) "Cmd" else "Ctrl";

/// What an OS dialog of this file was opened for.
const Target = enum { pak, source_folder, game_folder, mod_export, batch_src, batch_dst };

/// SDL may answer a dialog on another thread, so the answer lands in a
/// global, as lifecycle_ui's and panels' own do.
var dialog_slot: Slot = .{};

const Slot = struct {
    state: std.atomic.Value(u8) = .init(@intFromEnum(State.idle)),
    target: Target = .pak,
    buffer: [field_capacity]u8 = undefined,
    len: usize = 0,

    const State = enum(u8) { idle, waiting, arrived, cancelled };

    fn request(self: *Slot, target: Target) bool {
        if (self.state.cmpxchgStrong(@intFromEnum(State.idle), @intFromEnum(State.waiting), .acquire, .monotonic) != null) return false;
        self.target = target;
        return true;
    }

    fn deliver(self: *Slot, text: ?[]const u8) void {
        if (self.state.load(.acquire) != @intFromEnum(State.waiting)) return;
        const chosen = text orelse {
            self.state.store(@intFromEnum(State.cancelled), .release);
            return;
        };
        if (chosen.len > self.buffer.len) {
            self.state.store(@intFromEnum(State.cancelled), .release);
            return;
        }
        self.len = chosen.len;
        @memcpy(self.buffer[0..chosen.len], chosen);
        self.state.store(@intFromEnum(State.arrived), .release);
    }

    fn take(self: *Slot) ?struct { target: Target, path: ?[]const u8 } {
        const state: State = @enumFromInt(self.state.load(.acquire));
        const path: ?[]const u8 = switch (state) {
            .idle, .waiting => return null,
            .arrived => self.buffer[0..self.len],
            .cancelled => null,
        };
        self.state.store(@intFromEnum(State.idle), .release);
        return .{ .target = self.target, .path = path };
    }
};

fn dialogCallback(userdata: ?*anyopaque, filelist: [*c]const [*c]const u8, filter: c_int) callconv(.c) void {
    _ = filter;
    const slot: *Slot = @ptrCast(@alignCast(userdata orelse return));
    if (filelist == null or filelist[0] == null) return slot.deliver(null);
    slot.deliver(std.mem.span(filelist[0]));
}

const pak_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "PAK files", .pattern = "pak" }};

const Modal = enum {
    mod_settings,
    set_directories,
    picture_options,
    batch_mode,
    report,

    fn id(self: Modal) [*:0]const u8 {
        return switch (self) {
            .mod_settings => "MOD Settings",
            .set_directories => "Set Directories",
            .picture_options => "Set Picture Options",
            .batch_mode => "Batch Mode",
            .report => "Report###tools_report",
        };
    }
};

pub const Tools = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    environ: std.process.Environ,
    window: *sdl3.c.SDL_Window,
    ui: *lifecycle_ui.Ui,
    modal: ?Modal = null,
    modal_opened: bool = false,

    mod_export: [260]u8 = @splat(0),
    mod_name: [64]u8 = @splat(0),
    mod_version: [32]u8 = @splat(0),
    mod_desc: [256]u8 = @splat(0),

    source_edit: [field_capacity]u8 = @splat(0),
    game_folder_edit: [field_capacity]u8 = @splat(0),
    arguments_edit: [256]u8 = @splat(0),

    picture: tools.PictureOptions = .{},
    picture_current_only: bool = false,
    picture_source: [field_capacity]u8 = @splat(0),

    /// 0 is every kind; 1 + the kind's integer otherwise.
    batch_kind: usize = 0,
    batch_src: [field_capacity]u8 = @splat(0),
    batch_dst: [field_capacity]u8 = @splat(0),
    batch_force: bool = false,
    batch_open_save: bool = false,

    report_title: [64]u8 = @splat(0),
    report_text: std.ArrayList(u8) = .empty,
    export_outcome: tools.ExportOutcome = .{},

    running: ?testlaunch.Running = null,
    game_log: [field_capacity]u8 = @splat(0),

    /// On the heap like lifecycle_ui.Ui: the edit buffers and the export
    /// report are tens of kilobytes. Hooks itself into `ui`'s File menu.
    pub fn create(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, ui: *lifecycle_ui.Ui, window: *sdl3.c.SDL_Window) !*Tools {
        const self = try gpa.create(Tools);
        self.* = .{ .gpa = gpa, .io = io, .environ = environ, .window = window, .ui = ui };
        ui.file_menu_tools = .{ .ptr = self, .draw = drawFileItemsHook };
        return self;
    }

    pub fn destroy(self: *Tools) void {
        self.ui.file_menu_tools = null;
        self.report_text.deinit(self.gpa);
        self.gpa.destroy(self);
    }

    fn bridge(self: *Tools) core.bridge.ResBridge {
        return self.ui.real.bridge();
    }

    fn baseRoot(self: *const Tools) []const u8 {
        return std.mem.sliceTo(&self.ui.paths.base_root, 0);
    }

    fn userRoot(self: *const Tools) []const u8 {
        return std.mem.sliceTo(&self.ui.paths.user_root, 0);
    }

    // --- Menus ----------------------------------------------------------------

    fn drawFileItemsHook(ptr: *anyopaque) void {
        const self: *Tools = @ptrCast(@alignCast(ptr));
        self.drawFileItems();
    }

    /// File: MOD Settings, Export Result, Compress current MOD to PAK.
    fn drawFileItems(self: *Tools) void {
        const state = self.ui.session.life.menuState();
        if (ig.igMenuItemEx("MOD Settings...", shortcut_label ++ "+M", false, logic.isEnabled(.mod_settings, state))) self.perform(.mod_settings);
        if (ig.igMenuItemEx("Export Result", shortcut_label ++ "+E", false, logic.isEnabled(.export_result, state))) self.perform(.export_result);
        if (ig.igMenuItemEx("Compress current MOD to PAK...", null, false, logic.isEnabled(.pack_mod, state))) self.askPak();
    }

    /// Edit: Set Picture Options, after panels' own Edit items.
    pub fn drawEditMenuItems(self: *Tools) void {
        ig.igSeparator();
        if (ig.igMenuItemEx("Set Picture Options...", null, false, logic.isEnabled(.picture_options, self.ui.session.life.menuState()))) self.openPictureOptions();
    }

    /// The Tools menu. MFC's SaveMapObjects is not here: it had no menu
    /// entry and no message-map entry (06-PARITY.md A-23).
    pub fn drawToolsMenu(self: *Tools) void {
        if (!ig.igBeginMenuEx("Tools", true)) return;
        const state = self.ui.session.life.menuState();
        if (ig.igMenuItemEx("Set Directories...", shortcut_label ++ "+T", false, logic.isEnabled(.set_directories, state))) self.perform(.set_directories);
        if (ig.igMenuItemEx("Export Stats Only", shortcut_label ++ "+R", false, logic.isEnabled(.export_stats_only, state))) self.perform(.export_stats_only);
        if (ig.igMenuItemEx("Batch Mode...", shortcut_label ++ "+B", false, logic.isEnabled(.batch_mode, state))) self.perform(.batch_mode);
        ig.igSeparator();
        const running = self.gameRunning();
        if (ig.igMenuItemEx(if (running) "Run Blitzkrieg (running)" else "Run Blitzkrieg", "F7", false, logic.isEnabled(.run_game, state) and !running)) self.perform(.run_game);
        ig.igEndMenu();
    }

    /// The Editors menu: MFC's twenty entries with their separators, the
    /// active sub-editor checked. A choice goes through the session, which
    /// asks about unsaved changes, parks the open project and remembers it.
    pub fn drawEditorsMenu(self: *Tools) void {
        if (!ig.igBeginMenuEx("Editors", true)) return;
        for (logic.editors_menu) |entry| {
            if (entry.separator_before) ig.igSeparator();
            if (ig.igMenuItemEx(entry.label.ptr, null, entry.kind == self.ui.session.life.active, true)) self.switchEditor(entry.kind);
        }
        ig.igEndMenu();
    }

    /// The Editors combo in the menu bar (MFC's toolbar combo).
    pub fn drawEditorCombo(self: *Tools) void {
        const active = self.ui.session.life.active;
        const preview: [*:0]const u8 = if (logic.menuEntry(active)) |entry| entry.label.ptr else lifecycle.kindLabel(active).ptr;
        ig.igSetNextItemWidth(170);
        if (ig.igBeginCombo("##editors", preview, 0)) {
            for (logic.editors_menu) |entry| {
                if (entry.separator_before) ig.igSeparator();
                if (ig.igSelectableEx(entry.label.ptr, entry.kind == active, 0, .{ .x = 0, .y = 0 })) self.switchEditor(entry.kind);
            }
            ig.igEndCombo();
        }
    }

    fn switchEditor(self: *Tools, kind: Kind) void {
        var context = self.ui.ctx();
        self.ui.session.request(&context, .{ .switch_editor = kind });
    }

    /// Ctrl/Cmd+M, +E, +R, +T, +B and F7, unless a text field or a modal has
    /// the keyboard.
    pub fn handleShortcuts(self: *Tools) void {
        const io = ig.igGetIO();
        if (io.*.WantTextInput or self.modal != null or self.ui.session.busy()) return;
        const ctrl = io.*.KeyCtrl or io.*.KeySuper;
        const keys = [_]struct { key: ig.ImGuiKey, which: tools.ShortcutKey }{
            .{ .key = ig.ImGuiKey_M, .which = .m },
            .{ .key = ig.ImGuiKey_E, .which = .e },
            .{ .key = ig.ImGuiKey_R, .which = .r },
            .{ .key = ig.ImGuiKey_T, .which = .t },
            .{ .key = ig.ImGuiKey_B, .which = .b },
            .{ .key = ig.ImGuiKey_F7, .which = .f7 },
        };
        for (keys) |k| {
            if (!ig.igIsKeyPressedEx(k.key, false)) continue;
            if (tools.shortcutFor(ctrl, io.*.KeyShift, k.which)) |action| {
                const state = self.ui.session.life.menuState();
                const item: logic.MenuItem = switch (action) {
                    .mod_settings => .mod_settings,
                    .export_result => .export_result,
                    .export_stats_only => .export_stats_only,
                    .set_directories => .set_directories,
                    .batch_mode => .batch_mode,
                    .run_game => .run_game,
                };
                if (logic.isEnabled(item, state)) self.perform(action);
            }
            return;
        }
    }

    fn perform(self: *Tools, action: tools.ToolAction) void {
        switch (action) {
            .mod_settings => self.openModSettings(),
            .export_result => self.exportProject(false),
            .export_stats_only => self.exportProject(true),
            .set_directories => self.openDirectories(),
            .batch_mode => self.openBatch(),
            .run_game => self.runGame(),
        }
    }

    // --- Per frame ------------------------------------------------------------

    /// After the frame: an OS dialog's answer, the test game's exit, and the
    /// active sub-editor remembered (MFC's "Active Frame").
    pub fn afterFrame(self: *Tools) void {
        if (dialog_slot.take()) |answer| if (answer.path) |path| self.dialogAnswered(answer.target, path);
        if (self.running) |*running| {
            if (running.poll(self.io)) |exit| {
                self.running = null;
                switch (testlaunch.describe(exit)) {
                    .clean => {},
                    .early_failure, .failure => self.showReport("Run Blitzkrieg", "Game ended with a failure (exit {?d}, signal {?d}); its output is in {s}\n", .{ exit.code, exit.signal, std.mem.sliceTo(&self.game_log, 0) }),
                }
            }
        }
        if (tools.rememberActive(&self.ui.settings, self.ui.session.life.active)) self.ui.session.settings_changed = true;
    }

    pub fn drawModals(self: *Tools) void {
        const modal = self.modal orelse return;
        if (!self.modal_opened) {
            _ = ig.igOpenPopup(modal.id(), 0);
            self.modal_opened = true;
        }
        if (modal == .report) ig.igSetNextWindowSize(.{ .x = 620, .y = 380 }, ig.ImGuiCond_Appearing);
        const flags: ig.ImGuiWindowFlags = if (modal == .report) 0 else ig.ImGuiWindowFlags_AlwaysAutoResize;
        if (!ig.igBeginPopupModal(modal.id(), null, flags)) {
            // Closed by ImGui itself (Escape): forget it.
            self.modal = null;
            self.modal_opened = false;
            return;
        }
        const done = switch (modal) {
            .mod_settings => self.drawModSettings(),
            .set_directories => self.drawDirectories(),
            .picture_options => self.drawPictureOptions(),
            .batch_mode => self.drawBatch(),
            .report => self.drawReport(),
        };
        if (done) {
            ig.igCloseCurrentPopup();
            // A modal that answered with a report shows it next frame.
            if (self.modal == modal) self.modal = null;
            self.modal_opened = false;
        }
        ig.igEndPopup();
    }

    fn openModal(self: *Tools, modal: Modal) void {
        self.modal = modal;
        self.modal_opened = false;
    }

    // --- Report ---------------------------------------------------------------

    fn showReport(self: *Tools, title: []const u8, comptime format: []const u8, args: anytype) void {
        setField(&self.report_title, title);
        self.report_text.clearRetainingCapacity();
        self.report_text.print(self.gpa, format, args) catch {};
        std.debug.print("resource-editor: {s}: {s}", .{ title, self.report_text.items });
        self.openModal(.report);
    }

    fn showWritten(self: *Tools, title: []const u8, text: []const u8) void {
        self.showReport(title, "{s}", .{text});
    }

    fn drawReport(self: *Tools) bool {
        textLine(std.mem.sliceTo(&self.report_title, 0));
        ig.igSeparator();
        if (ig.igBeginChild("##report_text", .{ .x = 0, .y = -32 }, 0, ig.ImGuiWindowFlags_HorizontalScrollbar)) {
            textLine(self.report_text.items);
        }
        ig.igEndChild();
        return ig.igButton("Close");
    }

    // --- MOD Settings ---------------------------------------------------------

    fn openModSettings(self: *Tools) void {
        var mod: ModSettings = .{};
        if (self.bridge().modSettingsGet(&mod) != .ok) return self.showReport("MOD Settings", "the MOD settings could not be read: {s}\n", .{self.bridge().lastMessage()});
        setField(&self.mod_export, mod.exportDirSlice());
        setField(&self.mod_name, mod.nameSlice());
        setField(&self.mod_version, mod.versionSlice());
        setField(&self.mod_desc, mod.descSlice());
        self.openModal(.mod_settings);
    }

    fn drawModSettings(self: *Tools) bool {
        _ = ig.igInputText("Export folder", &self.mod_export, self.mod_export.len, 0);
        ig.igSameLine();
        if (ig.igSmallButton("Browse...##mod")) self.askFolder(.mod_export, std.mem.sliceTo(&self.mod_export, 0));
        _ = ig.igInputText("Name", &self.mod_name, self.mod_name.len, 0);
        _ = ig.igInputText("Version", &self.mod_version, self.mod_version.len, 0);
        _ = ig.igInputText("Description", &self.mod_desc, self.mod_desc.len, 0);
        textLine("OK writes <export folder>/data/mod.xml and seeds modobjects.xml.");
        if (ig.igButton("OK")) {
            var mod: ModSettings = .{};
            _ = mod.setExportDir(std.mem.sliceTo(&self.mod_export, 0));
            _ = mod.setName(std.mem.sliceTo(&self.mod_name, 0));
            _ = mod.setVersion(std.mem.sliceTo(&self.mod_version, 0));
            _ = mod.setDesc(std.mem.sliceTo(&self.mod_desc, 0));
            const status = self.bridge().modSettingsSet(&mod);
            if (status != .ok) {
                self.showReport("MOD Settings", "refused: {s}\n", .{self.bridge().lastMessage()});
            } else {
                self.showReport("MOD Settings", "mod.xml written for {s} into {s}\n", .{ mod.nameSlice(), mod.exportDirSlice() });
            }
            return true;
        }
        ig.igSameLine();
        return ig.igButton("Cancel");
    }

    // --- Export, PAK ----------------------------------------------------------

    fn exportProject(self: *Tools, stats_only: bool) void {
        const life = &self.ui.session.life;
        var gamma_found = true;
        if (life.doc.pathSlice()) |path| {
            var buffer: [logic.path_capacity]u8 = undefined;
            gamma_found = tools.findGammaCfg(self.ui.std_files.files(), std.fs.path.dirname(path) orelse ".", &buffer) != null;
        }
        tools.runExport(self.bridge(), life, self.ui.session.recovery_active != null, stats_only, gamma_found, &self.export_outcome);
        var out: std.Io.Writer.Allocating = .init(self.gpa);
        defer out.deinit();
        tools.formatExportReport(&self.export_outcome, &out.writer) catch {};
        self.showWritten(if (stats_only) "Export Stats Only" else "Export Result", out.written());
    }

    fn askPak(self: *Tools) void {
        if (!dialog_slot.request(.pak)) return;
        var mod: ModSettings = .{};
        _ = self.bridge().modSettingsGet(&mod);
        var location_buffer: [field_capacity + 1]u8 = undefined;
        const location: ?[*:0]const u8 = if (std.fmt.bufPrintZ(&location_buffer, "{s}", .{mod.exportDirSlice()})) |z| (if (z.len != 0) z.ptr else null) else |_| null;
        sdl3.c.SDL_ShowSaveFileDialog(dialogCallback, &dialog_slot, self.window, &pak_filters, pak_filters.len, location);
    }

    fn packMod(self: *Tools, chosen: []const u8) void {
        var buffer: [field_capacity + 8]u8 = undefined;
        const path = tools.withPakExtension(&buffer, chosen) orelse return self.showReport("Compress MOD to PAK", "the name is too long\n", .{});
        if (self.bridge().packMod(path) != .ok) return self.showReport("Compress MOD to PAK", "refused: {s}\n", .{self.bridge().lastMessage()});
        self.showReport("Compress MOD to PAK", "{s} written and read back through the engine's PAK reader\n", .{path});
    }

    // --- Set Directories -------------------------------------------------------

    fn openDirectories(self: *Tools) void {
        setField(&self.source_edit, self.ui.settings.sourceFolder());
        setField(&self.game_folder_edit, self.ui.settings.gameFolder());
        setField(&self.arguments_edit, self.ui.settings.gameParameters());
        self.openModal(.set_directories);
    }

    fn drawDirectories(self: *Tools) bool {
        _ = ig.igInputText("Source folder", &self.source_edit, self.source_edit.len, 0);
        ig.igSameLine();
        if (ig.igSmallButton("Browse...##src")) self.askFolder(.source_folder, std.mem.sliceTo(&self.source_edit, 0));
        _ = ig.igInputTextWithHint("Game folder", "the Game installed beside the editor", &self.game_folder_edit, self.game_folder_edit.len, 0);
        ig.igSameLine();
        if (ig.igSmallButton("Browse...##game")) self.askFolder(.game_folder, std.mem.sliceTo(&self.game_folder_edit, 0));
        _ = ig.igInputText("Game arguments", &self.arguments_edit, self.arguments_edit.len, 0);
        textLine("The export folder is the MOD's: File > MOD Settings.");
        if (ig.igButton("OK")) {
            self.ui.settings.source_folder.set(std.mem.sliceTo(&self.source_edit, 0));
            self.ui.settings.game_folder.set(std.mem.sliceTo(&self.game_folder_edit, 0));
            self.ui.settings.setGameParameters(std.mem.sliceTo(&self.arguments_edit, 0));
            self.ui.session.settings_changed = true;
            return true;
        }
        ig.igSameLine();
        return ig.igButton("Cancel");
    }

    fn askFolder(self: *Tools, target: Target, current: []const u8) void {
        if (!dialog_slot.request(target)) return;
        var location_buffer: [field_capacity + 1]u8 = undefined;
        const location: ?[*:0]const u8 = if (std.fmt.bufPrintZ(&location_buffer, "{s}", .{current})) |z| (if (z.len != 0) z.ptr else null) else |_| null;
        sdl3.c.SDL_ShowOpenFolderDialog(dialogCallback, &dialog_slot, self.window, location, false);
    }

    fn dialogAnswered(self: *Tools, target: Target, path: []const u8) void {
        switch (target) {
            .pak => self.packMod(path),
            .source_folder => setField(&self.source_edit, path),
            .game_folder => setField(&self.game_folder_edit, path),
            .mod_export => setField(&self.mod_export, path),
            .batch_src => setField(&self.batch_src, path),
            .batch_dst => setField(&self.batch_dst, path),
        }
    }

    // --- Picture Options -------------------------------------------------------

    /// Starts from the gamma.cfg the export would use: searched upward from
    /// the project's folder, else from the sub-editor's source folder.
    fn openPictureOptions(self: *Tools) void {
        const life = &self.ui.session.life;
        var start_buffer: [logic.path_capacity]u8 = undefined;
        const start: ?[]const u8 = if (life.is_open and life.doc.pathSlice() != null)
            std.fs.path.dirname(life.doc.pathSlice().?) orelse "."
        else switch (tools.gammaTarget(&start_buffer, false, null, self.ui.settings.sourceFolder(), life.active)) {
            .path => |p| std.fs.path.dirname(p),
            else => null,
        };
        self.picture = .{};
        setField(&self.picture_source, "");
        if (start) |dir| {
            var found_buffer: [logic.path_capacity]u8 = undefined;
            if (tools.findGammaCfg(self.ui.std_files.files(), dir, &found_buffer)) |found| {
                if (std.Io.Dir.cwd().readFileAlloc(self.io, found, self.gpa, .limited(64 * 1024))) |bytes| {
                    defer self.gpa.free(bytes);
                    self.picture = tools.parseGammaCfg(bytes).clamped();
                    setField(&self.picture_source, found);
                } else |_| {}
            }
        }
        self.openModal(.picture_options);
    }

    fn drawPictureOptions(self: *Tools) bool {
        const source = std.mem.sliceTo(&self.picture_source, 0);
        var line: [field_capacity + 32]u8 = undefined;
        textLine(if (source.len != 0) std.fmt.bufPrint(&line, "From {s}", .{source}) catch source else "No gamma.cfg found: starting from 0");
        _ = ig.igSliderFloat("Brightness", &self.picture.brightness, -1, 1);
        _ = ig.igSliderFloat("Contrast", &self.picture.contrast, -1, 1);
        _ = ig.igSliderFloat("Gamma", &self.picture.gamma, -1, 1);
        self.picture = self.picture.clamped();
        textLine("Before / after (the engine's correction):");
        drawRamp(.{});
        drawRamp(self.picture);
        _ = ig.igCheckbox("Current project only", &self.picture_current_only);
        if (ig.igButton("OK")) {
            self.writeGammaCfg();
            return true;
        }
        ig.igSameLine();
        return ig.igButton("Cancel");
    }

    fn writeGammaCfg(self: *Tools) void {
        const life = &self.ui.session.life;
        var path_buffer: [logic.path_capacity]u8 = undefined;
        const path = switch (tools.gammaTarget(&path_buffer, self.picture_current_only, if (life.is_open) life.doc.pathSlice() else null, self.ui.settings.sourceFolder(), life.active)) {
            .path => |p| p,
            .untitled => return self.showReport("Set Picture Options", "save the project first: its gamma.cfg goes beside it\n", .{}),
            .no_source_folder => return self.showReport("Set Picture Options", "set the source folder in Tools > Set Directories first\n", .{}),
            .too_long => return self.showReport("Set Picture Options", "the gamma.cfg path is too long\n", .{}),
        };
        const context = self.ui.ctx();
        if (lifecycle.Session.isShipped(&context, path)) return self.showReport("Set Picture Options", "{s} is shipped data and is not written\n", .{path});
        var text_buffer: [256]u8 = undefined;
        const text = tools.formatGammaCfg(&text_buffer, self.picture).?;
        if (std.fs.path.dirname(path)) |dir| std.Io.Dir.cwd().createDirPath(self.io, dir) catch {};
        std.Io.Dir.cwd().writeFile(self.io, .{ .sub_path = path, .data = text }) catch |err| return self.showReport("Set Picture Options", "{s} was not written: {s}\n", .{ path, @errorName(err) });
        self.showReport("Set Picture Options", "{s} written\n", .{path});
    }

    // --- Batch Mode ------------------------------------------------------------

    /// MFC's defaults: the source folder (plus the sub-editor's folder) and
    /// the MOD's export folder; the current kind first.
    fn openBatch(self: *Tools) void {
        const active = self.ui.session.life.active;
        self.batch_kind = @as(usize, @intCast(@intFromEnum(active))) + 1;
        var src_buffer: [logic.path_capacity]u8 = undefined;
        const source = self.ui.settings.sourceFolder();
        const src = if (source.len == 0) "" else std.fmt.bufPrint(&src_buffer, "{s}{c}{s}", .{ std.mem.trimEnd(u8, source, "/\\"), std.fs.path.sep, tools.kindFolder(active) }) catch source;
        setField(&self.batch_src, src);
        var mod: ModSettings = .{};
        _ = self.bridge().modSettingsGet(&mod);
        setField(&self.batch_dst, mod.exportDirSlice());
        self.openModal(.batch_mode);
    }

    fn drawBatch(self: *Tools) bool {
        const kinds = std.enums.values(Kind);
        const preview: [*:0]const u8 = if (self.batch_kind == 0) "All projects" else lifecycle.kindLabel(kinds[self.batch_kind - 1]).ptr;
        if (ig.igBeginCombo("Projects", preview, 0)) {
            if (ig.igSelectableEx("All projects", self.batch_kind == 0, 0, .{ .x = 0, .y = 0 })) self.batch_kind = 0;
            for (kinds, 1..) |kind, i| {
                var label_buffer: [64]u8 = undefined;
                const label = std.fmt.bufPrintZ(&label_buffer, "{s} (*.{s})", .{ lifecycle.kindLabel(kind), kind.extension() }) catch continue;
                if (ig.igSelectableEx(label.ptr, self.batch_kind == i, 0, .{ .x = 0, .y = 0 })) self.batch_kind = i;
            }
            ig.igEndCombo();
        }
        _ = ig.igInputText("Source folder##batch", &self.batch_src, self.batch_src.len, 0);
        ig.igSameLine();
        if (ig.igSmallButton("Browse...##bsrc")) self.askFolder(.batch_src, std.mem.sliceTo(&self.batch_src, 0));
        _ = ig.igInputText("Destination folder##batch", &self.batch_dst, self.batch_dst.len, 0);
        ig.igSameLine();
        if (ig.igSmallButton("Browse...##bdst")) self.askFolder(.batch_dst, std.mem.sliceTo(&self.batch_dst, 0));
        _ = ig.igCheckbox("Force (export even when up to date)", &self.batch_force);
        _ = ig.igCheckbox("Only open and save the projects", &self.batch_open_save);
        if (ig.igButton("Run")) {
            self.runBatch();
            return true;
        }
        ig.igSameLine();
        return ig.igButton("Cancel");
    }

    fn runBatch(self: *Tools) void {
        const kinds = std.enums.values(Kind);
        const request: tools.BatchRequest = .{
            .mask = if (self.batch_kind == 0) .all else .{ .one = kinds[self.batch_kind - 1] },
            .src = std.mem.sliceTo(&self.batch_src, 0),
            .dst = std.mem.sliceTo(&self.batch_dst, 0),
            .flags = .{ .force = self.batch_force, .open_save = self.batch_open_save },
        };
        var summary = batch_cli.execute(self.gpa, self.io, self.bridge(), request) catch |err| return self.showReport("Batch Mode", "the batch failed: {s}\n", .{@errorName(err)});
        defer summary.deinit(self.gpa);
        var out: std.Io.Writer.Allocating = .init(self.gpa);
        defer out.deinit();
        tools.formatBatchReport(&summary, &out.writer) catch {};
        self.showWritten("Batch Mode", out.written());
    }

    // --- Run Blitzkrieg -------------------------------------------------------------

    fn gameRunning(self: *Tools) bool {
        if (self.running) |*running| return running.poll(self.io) == null;
        return false;
    }

    /// F7: Game with the MOD's export folder as -mod=, this editor's test
    /// profile and Set Directories' arguments; its output goes to
    /// `<UserRoot>resourceeditor/testgame.log`.
    fn runGame(self: *Tools) void {
        if (self.gameRunning()) return;
        var mod: ModSettings = .{};
        if (self.bridge().modSettingsGet(&mod) != .ok) return self.showReport("Run Blitzkrieg", "the MOD settings could not be read: {s}\n", .{self.bridge().lastMessage()});
        const mod_folder = tools.modFolderForGame(mod.exportDirSlice(), self.baseRoot()) orelse
            return self.showReport("Run Blitzkrieg", "the export folder {s} is not a folder of {s}mods, so the game cannot load it as a mod (File > MOD Settings)\n", .{ mod.exportDirSlice(), self.baseRoot() });
        var installed_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const installed = testlaunch.gamePath(self.io, &installed_buffer) catch |err| return self.showReport("Run Blitzkrieg", "the editor's own folder is unknown: {s}\n", .{@errorName(err)});
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = tools.gamePath(&game_buffer, self.ui.settings.gameFolder(), installed, builtin.os.tag == .windows) orelse return self.showReport("Run Blitzkrieg", "the game path is too long\n", .{});
        const user_root = self.userRoot();
        const log = std.fmt.bufPrint(self.game_log[0 .. self.game_log.len - 1], "{s}resourceeditor{c}testgame.log", .{ user_root, std.fs.path.sep }) catch return self.showReport("Run Blitzkrieg", "the log path is too long\n", .{});
        self.game_log[log.len] = 0;
        self.running = testlaunch.start(self.gpa, self.io, self.environ, tools.runGameOptions(game, mod_folder, &self.ui.settings, log)) catch |err|
            return self.showReport("Run Blitzkrieg", "{s} did not start: {s}\n", .{ game, @errorName(err) });
        std.debug.print("resource-editor: Run Blitzkrieg: {s} -mod={s} (log {s})\n", .{ game, mod_folder, log });
    }
};

fn setField(buffer: []u8, text: []const u8) void {
    const n = @min(text.len, buffer.len - 1);
    @memcpy(buffer[0..n], text[0..n]);
    buffer[n] = 0;
}

fn textLine(line: []const u8) void {
    ig.igTextUnformattedEx(line.ptr, line.ptr + line.len);
}

/// A grey ramp through `options`: MFC's before/after SingleIcon pair, drawn
/// as 32 steps so the curve each slider makes is visible.
fn drawRamp(options: tools.PictureOptions) void {
    const steps = 32;
    const step_w: f32 = 10;
    const height: f32 = 16;
    const draw = ig.igGetWindowDrawList();
    const origin = ig.igGetCursorScreenPos();
    var i: usize = 0;
    while (i < steps) : (i += 1) {
        const value: u8 = @intCast(i * 255 / (steps - 1));
        const v: u32 = tools.correctChannel(value, options);
        const colour: u32 = 0xFF000000 | (v << 16) | (v << 8) | v;
        const x = origin.x + @as(f32, @floatFromInt(i)) * step_w;
        ig.ImDrawList_AddRectFilled(draw, .{ .x = x, .y = origin.y }, .{ .x = x + step_w, .y = origin.y + height }, colour);
    }
    ig.igDummy(.{ .x = steps * step_w, .y = height });
}
