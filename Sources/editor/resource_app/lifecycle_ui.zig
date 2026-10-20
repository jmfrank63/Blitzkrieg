//! The interactive mode's File menu over `lifecycle.Session`: the menu
//! items, the OS file dialogs, the unsaved-changes, lock and recovery
//! modals, the status line, the window title, autosave's tick with its
//! recovery sidecars, and `resourceeditor.cfg` with ImGui's `layout.ini`
//! beside it. Every decision is the session's (tested in
//! test-resource-app-logic); this file only draws and does the I/O the
//! session cannot do through the bridge or the kit's `Files`.
//!
//! A `--hidden` run (automated) never reads or writes the user's settings,
//! layout, recovery folder or recent list, and never autosaves: the same rule
//! the map editor keeps for its automated modes.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const kit = @import("editor_kit");
const core = @import("resource_core");
const c_bridge = @import("c_bridge.zig");
const lifecycle = @import("lifecycle.zig");
const logic = @import("panels_logic.zig");
const app_settings = @import("settings.zig");

const ig = imgui.c;
const l10n = kit.l10n;
const Kind = core.bridge.Kind;
const kind_count = logic.kind_count;

/// The dialogs' filters: every project kind first, then one per kind for
/// Save As (SDL wants `;`-separated extensions, and the list must outlive the
/// dialog, so these are globals).
const all_pattern: [:0]const u8 = blk: {
    var joined: []const u8 = "";
    for (std.enums.values(Kind), 0..) |kind, i| joined = joined ++ (if (i == 0) "" else ";") ++ kind.extension();
    break :blk std.fmt.comptimePrint("{s}", .{joined});
};
const kind_patterns = blk: {
    var patterns: [kind_count][:0]const u8 = undefined;
    for (std.enums.values(Kind), 0..) |kind, i| patterns[i] = std.fmt.comptimePrint("{s}", .{kind.extension()});
    break :blk patterns;
};
/// The filters' names follow the language, so they are filled when a dialog
/// opens; the dialog keeps the pointers, hence the globals.
var open_filters: [1]sdl3.c.SDL_DialogFileFilter = undefined;
var kind_filters: [kind_count]sdl3.c.SDL_DialogFileFilter = undefined;

fn fillFilters() void {
    open_filters[0] = .{ .name = l10n.t("resource_app.file.filter_projects"), .pattern = all_pattern.ptr };
    for (std.enums.values(Kind), 0..) |kind, i| kind_filters[i] = .{ .name = lifecycle.kindLabel(kind), .pattern = kind_patterns[i].ptr };
}

/// Where the dialog's callback writes its answer: SDL may call back on
/// another thread, or after the editor quit, so it is a global.
var dialog_slot: DialogSlot = .{};

const DialogSlot = kit.dialogs.PathSlot(void, logic.path_capacity);

/// ImGui keeps the ini file's name by pointer for the whole session.
var layout_ini_path: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;

const max_offers = 16;

/// A part of the File menu another file draws (`Ui.file_menu_tools`).
pub const FileMenuHook = struct {
    ptr: *anyopaque,
    draw: *const fn (ptr: *anyopaque) void,
};

pub const Ui = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    window: *sdl3.c.SDL_Window,
    real: c_bridge.RealResBridge,
    std_files: kit.files.StdFiles,
    session: lifecycle.Session = .{},
    settings: app_settings.Settings = .{},
    /// False for --hidden: no settings, layout, recovery or autosave.
    persistent: bool,
    settings_path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined,
    settings_path: ?[]const u8 = null,
    paths: c_bridge.c.BkEditorPathSet = std.mem.zeroes(c_bridge.c.BkEditorPathSet),
    owner_buffer: [256]u8 = undefined,
    owner: []const u8 = "unknown",
    offers: [max_offers]lifecycle.RecoveryOffer = undefined,
    offer_count: usize = 0,
    offers_dismissed: bool = false,
    dialog_open: ?lifecycle.Dialog = null,
    title_dirty: ?bool = null,
    title_path_len: usize = std.math.maxInt(usize),
    /// Drawn in the File menu after Autosave and before Exit: MOD Settings,
    /// Export Result and Compress to PAK, which tools_ui.zig owns.
    file_menu_tools: ?FileMenuHook = null,

    /// On the heap: the session's path buffers and the offers are tens of
    /// kilobytes.
    pub fn create(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, session: *anyopaque, window: *sdl3.c.SDL_Window, persistent: bool) !*Ui {
        const self = try gpa.create(Ui);
        self.* = .{
            .gpa = gpa,
            .io = io,
            .window = window,
            .real = c_bridge.RealResBridge.init(gpa, session),
            .std_files = .{ .io = io, .dir = std.Io.Dir.cwd() },
            .persistent = persistent,
        };
        _ = c_bridge.c.BkEditorPaths(@ptrCast(@alignCast(session)), &self.paths);
        self.owner = self.lockUser(environ);
        if (persistent) {
            const override: ?[]u8 = environ.getAlloc(gpa, app_settings.settings_env) catch null;
            defer if (override) |o| gpa.free(o);
            self.settings_path = app_settings.settingsPath(&self.settings_path_buffer, override, self.userRoot());
            if (self.settings_path) |path| {
                if (std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 * 1024))) |bytes| {
                    defer gpa.free(bytes);
                    self.settings = app_settings.parse(bytes);
                } else |_| {}
                enableLayoutPersistence(io, path);
            }
            self.scanRecoveryOffers();
            // BkResSave does not make folders: the recovery copies' folder
            // must exist before autosave's first write into it.
            var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            if (self.recoveryFolder(&folder_buffer)) |folder| std.Io.Dir.cwd().createDirPath(io, folder) catch {};
        } else {
            // No layout.ini in the working directory either.
            ig.igGetIO().*.IniFilename = null;
        }
        return self;
    }

    pub fn destroy(self: *Ui) void {
        self.session.deinit(self.gpa);
        self.gpa.destroy(self);
    }

    fn lockUser(self: *Ui, environ: std.process.Environ) []const u8 {
        var names: [3]?[]u8 = .{ null, null, null };
        for ([_][]const u8{ "BK_RESOURCE_EDITOR_USER", "USER", "USERNAME" }, 0..) |key, i| names[i] = environ.getAlloc(self.gpa, key) catch null;
        defer for (names) |n| if (n) |v| self.gpa.free(v);
        const name = lifecycle.lockUserName(names[0], names[1], names[2]);
        const len = @min(name.len, self.owner_buffer.len);
        @memcpy(self.owner_buffer[0..len], name[0..len]);
        return self.owner_buffer[0..len];
    }

    fn userRoot(self: *const Ui) []const u8 {
        return std.mem.sliceTo(&self.paths.user_root, 0);
    }

    fn baseRoot(self: *const Ui) []const u8 {
        return std.mem.sliceTo(&self.paths.base_root, 0);
    }

    pub fn ctx(self: *Ui) lifecycle.Ctx {
        return .{
            .allocator = self.gpa,
            .bridge = self.real.bridge(),
            .files = self.std_files.files(),
            .base_root = self.baseRoot(),
            // A --hidden run has no user root to write a recovery copy into.
            .user_root = if (self.persistent) self.userRoot() else "",
            .owner = self.owner,
            .settings = &self.settings,
            .take_over = .{ .ptr = &self.real, .call = takeOver },
        };
    }

    fn takeOver(ptr: *anyopaque) core.bridge.Status {
        const real: *c_bridge.RealResBridge = @ptrCast(@alignCast(ptr));
        return real.lockTakeOver();
    }

    /// The first project: the file named on the command line, else a new
    /// project of the kind named there, else of the remembered sub-editor.
    pub fn start(self: *Ui, path: ?[]const u8, kind: ?Kind) void {
        var context = self.ctx();
        if (path) |p| {
            if (logic.PathText.fromSlice(p)) |text| self.session.request(&context, .{ .open_path = text });
            if (self.session.life.is_open) return;
        }
        self.session.request(&context, .{ .new_project = kind orelse logic.restoreEditor(self.settings.last_editor) });
    }

    pub fn wantsQuit(self: *const Ui) bool {
        return self.session.quit;
    }

    /// The window's close button or the OS quit: through the unsaved prompt.
    pub fn requestQuit(self: *Ui) void {
        var context = self.ctx();
        self.session.request(&context, .quit);
    }

    // --- Menu ---------------------------------------------------------------

    const mod_label = if (builtin.os.tag == .macos) "Cmd" else "Ctrl";

    /// File menu items, inside the main menu bar's File menu.
    pub fn drawFileMenuItems(self: *Ui) void {
        var context = self.ctx();
        const state = self.session.life.menuState();
        const new_open = ig.igBeginMenuEx(l10n.id("resource_app.menu.new"), true);
        l10n.noteRect("resource_app.menu.new");
        if (new_open) {
            for (std.enums.values(Kind)) |kind| {
                var label_buffer: [128]u8 = undefined;
                const label = std.mem.printSentinel(&label_buffer, "{s} (.{s})###new_{s}", .{ lifecycle.kindLabel(kind), kind.extension(), @tagName(kind) }, 0) catch continue;
                if (ig.igMenuItemEx(label.ptr, null, false, true)) self.session.request(&context, .{ .new_project = kind });
            }
            ig.igEndMenu();
        }
        if (ig.igMenuItemEx(l10n.id("resource_app.file.open"), mod_label ++ "+O", false, logic.isEnabled(.open_project, state))) self.session.request(&context, .open_dialog);
        l10n.noteRect("resource_app.file.open");
        const recent_open = ig.igBeginMenuEx(l10n.id("resource_app.file.open_recent"), self.settings.recentCount() != 0);
        l10n.noteRect("resource_app.file.open_recent");
        if (recent_open) {
            var i: usize = 0;
            while (i < self.settings.recentCount()) : (i += 1) {
                var label_buffer: [logic.path_capacity + 1]u8 = undefined;
                const label = std.mem.printSentinel(&label_buffer, "{s}", .{self.settings.recentAt(i)}, 0) catch continue;
                if (ig.igMenuItemEx(label.ptr, null, false, true)) {
                    if (logic.PathText.fromSlice(self.settings.recentAt(i))) |text| self.session.request(&context, .{ .open_path = text });
                    break;
                }
            }
            ig.igEndMenu();
        }
        if (ig.igMenuItemEx(l10n.id("resource_app.file.close"), null, false, logic.isEnabled(.close_project, state))) self.session.request(&context, .close);
        l10n.noteRect("resource_app.file.close");
        ig.igSeparator();
        if (ig.igMenuItemEx(l10n.id("resource_app.file.save"), mod_label ++ "+S", false, logic.isEnabled(.save, state))) self.session.save(&context);
        l10n.noteRect("resource_app.file.save");
        if (ig.igMenuItemEx(l10n.id("resource_app.file.save_as"), mod_label ++ "+Shift+S", false, logic.isEnabled(.save_as, state))) self.session.saveAs();
        l10n.noteRect("resource_app.file.save_as");
        ig.igSeparator();
        if (ig.igMenuItemEx(l10n.id("resource_app.file.autosave"), null, self.settings.autosave, true)) {
            self.settings.autosave = !self.settings.autosave;
            self.session.settings_changed = true;
        }
        l10n.noteRect("resource_app.file.autosave");
        if (self.file_menu_tools) |hook| {
            ig.igSeparator();
            hook.draw(hook.ptr);
        }
        ig.igSeparator();
        if (ig.igMenuItemEx(l10n.id("resource_app.file.exit"), null, false, true)) self.session.request(&context, .quit);
        l10n.noteRect("resource_app.file.exit");
    }

    /// Ctrl/Cmd+O, +S and +Shift+S, unless a text field has the keyboard.
    pub fn handleShortcuts(self: *Ui) void {
        const io = ig.igGetIO();
        if (io.*.WantTextInput) return;
        if (!io.*.KeyCtrl and !io.*.KeySuper) return;
        var context = self.ctx();
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_O, false)) {
            self.session.request(&context, .open_dialog);
        } else if (ig.igIsKeyPressedEx(ig.ImGuiKey_S, false)) {
            if (io.*.KeyShift) self.session.saveAs() else self.session.save(&context);
        }
    }

    // --- Modals -------------------------------------------------------------

    pub fn drawModals(self: *Ui) void {
        self.drawUnsavedPrompt();
        self.drawLockPrompt();
        self.drawRecoveryPrompt();
        self.drawStatusLine();
    }

    fn openOnce(id: [*:0]const u8) void {
        if (!ig.igIsPopupOpen(id, 0)) _ = ig.igOpenPopup(id, 0);
    }

    fn drawUnsavedPrompt(self: *Ui) void {
        if (!self.session.prompt.isAsking()) return;
        const id = l10n.id("resource_app.unsaved.title");
        openOnce(id);
        if (!ig.igBeginPopupModal(id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
        var line: [logic.path_capacity + 256]u8 = undefined;
        const name = self.session.life.doc.pathSlice() orelse l10n.str("resource_app.unsaved.new_project");
        textLine(l10n.say(&line, "resource_app.unsaved.text", .{name}));
        var context = self.ctx();
        var choice: ?logic.UnsavedPrompt.Choice = null;
        if (ig.igButton(l10n.id("resource_app.unsaved.save"))) choice = .save;
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.unsaved.dont_save"))) choice = .dont_save;
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.unsaved.cancel"))) choice = .cancel;
        if (choice) |answer| {
            ig.igCloseCurrentPopup();
            self.session.answerUnsaved(&context, answer);
        }
        ig.igEndPopup();
    }

    fn drawLockPrompt(self: *Ui) void {
        if (!self.session.lock_prompt.asking) return;
        const id = l10n.id("resource_app.lock.title");
        openOnce(id);
        if (!ig.igBeginPopupModal(id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
        var line: [512]u8 = undefined;
        textLine(l10n.say(&line, "resource_app.lock.text", .{self.session.lock_prompt.owner()}));
        textLine(l10n.str("resource_app.lock.question"));
        var context = self.ctx();
        var choice: ?lifecycle.LockPrompt.Choice = null;
        if (ig.igButton(l10n.id("resource_app.lock.read_only"))) choice = .read_only;
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.lock.take_over"))) choice = .take_over;
        if (choice) |answer| {
            ig.igCloseCurrentPopup();
            _ = self.session.answerLock(&context, answer);
        }
        ig.igEndPopup();
    }

    /// Recovery copies found at start-up, offered back once: Open, Discard,
    /// or Later (kept for the next start).
    fn drawRecoveryPrompt(self: *Ui) void {
        if (self.offer_count == 0 or self.offers_dismissed or self.session.busy()) return;
        const id = l10n.id("resource_app.recovery.title");
        openOnce(id);
        if (!ig.igBeginPopupModal(id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
        var index: usize = 0;
        while (index < self.offer_count) : (index += 1) {
            const offer = &self.offers[index];
            ig.igPushIDInt(@intCast(index));
            var line: [logic.path_capacity + 256]u8 = undefined;
            const original = offer.original.slice();
            textLine(l10n.say(&line, "resource_app.recovery.line", .{ if (original.len == 0) l10n.str("resource_app.recovery.never_saved") else original, lifecycle.kindLabel(offer.kind), offer.unix_seconds }));
            ig.igSameLine();
            const open = ig.igSmallButton(l10n.id("resource_app.recovery.open"));
            ig.igSameLine();
            const discard = ig.igSmallButton(l10n.id("resource_app.recovery.discard"));
            ig.igPopID();
            if (open or discard) {
                const taken = offer.*;
                self.removeOffer(index);
                if (open) {
                    var context = self.ctx();
                    self.session.openRecovery(&context, &taken);
                } else {
                    self.deleteRecoveryFiles(taken.path.slice());
                }
                if (self.offer_count == 0 or open) ig.igCloseCurrentPopup();
                break;
            }
        }
        if (ig.igButton(l10n.id("resource_app.recovery.later"))) {
            self.offers_dismissed = true;
            ig.igCloseCurrentPopup();
        }
        ig.igEndPopup();
    }

    fn drawStatusLine(self: *Ui) void {
        const io = ig.igGetIO();
        ig.igSetNextWindowPos(.{ .x = 0, .y = io.*.DisplaySize.y - 24 }, ig.ImGuiCond_Always);
        ig.igSetNextWindowSize(.{ .x = io.*.DisplaySize.x, .y = 24 }, ig.ImGuiCond_Always);
        const flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoMove | ig.ImGuiWindowFlags_NoSavedSettings | ig.ImGuiWindowFlags_NoFocusOnAppearing | ig.ImGuiWindowFlags_NoNav;
        if (ig.igBegin("##status", null, flags)) {
            var line: [1024]u8 = undefined;
            const life = &self.session.life;
            const doc = if (!life.is_open) l10n.str("resource_app.status.no_project") else life.doc.pathSlice() orelse l10n.str("resource_app.status.untitled");
            const lock = if (life.read_only) l10n.str("resource_app.status.read_only") else "";
            textLine(std.fmt.bufPrint(&line, "{s}{s}{s}{s}   {s}", .{ doc, if (life.dirty()) " *" else "", if (life.read_only) " " else "", lock, self.session.message() }) catch "");
        }
        ig.igEnd();
    }

    // --- Per frame ----------------------------------------------------------

    /// After the frame: shows a dialog the session asked for, takes a
    /// dialog's answer, ticks autosave and writes the settings file when it
    /// changed. `now_ms` is SDL's tick count.
    pub fn afterFrame(self: *Ui, now_ms: u64) void {
        var context = self.ctx();
        if (self.session.dialog) |which| {
            if (self.dialog_open == null) self.showDialog(which);
        }
        if (dialog_slot.take()) |taken| {
            self.dialog_open = null;
            switch (taken) {
                .path => |p| self.session.dialogAnswered(&context, p.path),
                .cancelled => self.session.dialogAnswered(&context, null),
                .failed => |reason| {
                    self.session.dialogAnswered(&context, null);
                    std.debug.print("resource-editor: the file dialog failed: {s}\n", .{reason});
                },
            }
        }
        if (self.persistent) {
            switch (self.session.tickAutosave(&context, now_ms)) {
                .recovery => |copy| self.writeSidecar(copy.slice()),
                else => {},
            }
            if (self.session.settings_changed) {
                self.session.settings_changed = false;
                if (self.settings_path) |path| writeSettingsFile(self.io, path, &self.settings) catch |err| {
                    std.debug.print("resource-editor: {s} was not written: {s}\n", .{ path, @errorName(err) });
                };
            }
        } else {
            self.session.settings_changed = false;
        }
        self.updateTitle();
    }

    fn showDialog(self: *Ui, which: lifecycle.Dialog) void {
        if (!dialog_slot.request({})) return;
        self.dialog_open = which;
        fillFilters();
        var folder_z: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        var location: ?[*:0]const u8 = null;
        if (self.settings.projectsFolder().len != 0) {
            if (std.mem.printSentinel(&folder_z, "{s}", .{self.settings.projectsFolder()}, 0)) |z| location = z.ptr else |_| {}
        }
        switch (which) {
            .open => sdl3.c.SDL_ShowOpenFileDialog(DialogSlot.callback, &dialog_slot, self.window, &open_filters, open_filters.len, location, false),
            .save_as, .save_as_for_prompt => {
                const index: usize = @intCast(@intFromEnum(self.session.life.doc.kind));
                sdl3.c.SDL_ShowSaveFileDialog(DialogSlot.callback, &dialog_slot, self.window, &kind_filters[index], 1, location);
            },
        }
    }

    fn updateTitle(self: *Ui) void {
        const life = &self.session.life;
        const dirty = life.dirty();
        const path = life.doc.pathSlice() orelse "";
        const path_len = if (life.is_open) path.len else std.math.maxInt(usize) - 1;
        if (self.title_dirty == dirty and self.title_path_len == path_len) return;
        self.title_dirty = dirty;
        self.title_path_len = path_len;
        var buffer: [logic.path_capacity + 64]u8 = undefined;
        const name = if (!life.is_open) "" else if (path.len == 0) l10n.str("resource_app.status.untitled") else std.fs.path.basename(path);
        const title = std.mem.printSentinel(&buffer, "{s}{s}{s}Resource Editor", .{ name, if (dirty) " *" else "", if (name.len != 0) " - " else "" }, 0) catch return;
        _ = sdl3.c.SDL_SetWindowTitle(self.window, title.ptr);
    }

    // --- Recovery files -----------------------------------------------------

    fn recoveryFolder(self: *const Ui, buffer: []u8) ?[]const u8 {
        return lifecycle.recoveryFolder(buffer, self.userRoot());
    }

    /// Start-up only: what the recovery folder holds now, each project file
    /// paired with its `.txt` sidecar. Nothing here fails start-up.
    fn scanRecoveryOffers(self: *Ui) void {
        var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const folder = self.recoveryFolder(&folder_buffer) orelse return;
        var dir = std.Io.Dir.cwd().openDir(self.io, folder, .{ .iterate = true }) catch return;
        defer dir.close(self.io);
        var it = dir.iterate();
        while (self.offer_count < self.offers.len) {
            const entry = (it.next(self.io) catch break) orelse break;
            if (entry.kind != .file) continue;
            var sidecar_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const sidecar_path = std.fmt.bufPrint(&sidecar_buffer, "{s}{c}{s}.txt", .{ folder, std.fs.path.sep, entry.name }) catch continue;
            const sidecar = std.Io.Dir.cwd().readFileAlloc(self.io, sidecar_path, self.gpa, .limited(8192)) catch null;
            defer if (sidecar) |bytes| self.gpa.free(bytes);
            const offer = lifecycle.offerFromEntry(folder, entry.name, sidecar) orelse continue;
            self.offers[self.offer_count] = offer;
            self.offer_count += 1;
        }
    }

    fn removeOffer(self: *Ui, index: usize) void {
        var i = index;
        while (i + 1 < self.offer_count) : (i += 1) self.offers[i] = self.offers[i + 1];
        self.offer_count -= 1;
    }

    fn deleteRecoveryFiles(self: *Ui, path: []const u8) void {
        const files = self.std_files.files();
        files.delete(path);
        var buffer: [logic.path_capacity + 8]u8 = undefined;
        if (std.fmt.bufPrint(&buffer, "{s}.txt", .{path})) |sidecar| files.delete(sidecar) else |_| {}
        if (std.fmt.bufPrint(&buffer, "{s}.bak", .{path})) |bak| files.delete(bak) else |_| {}
    }

    fn writeSidecar(self: *Ui, copy_path: []const u8) void {
        var sidecar_buffer: [logic.path_capacity + 8]u8 = undefined;
        const sidecar_path = std.fmt.bufPrint(&sidecar_buffer, "{s}.txt", .{copy_path}) catch return;
        var text_buffer: [logic.path_capacity + 32]u8 = undefined;
        const unix_seconds = std.Io.Clock.real.now(self.io).toSeconds();
        const body = lifecycle.formatSidecar(&text_buffer, self.session.life.doc.pathSlice() orelse "", unix_seconds) orelse return;
        std.Io.Dir.cwd().writeFile(self.io, .{ .sub_path = sidecar_path, .data = body }) catch {};
    }
};

fn textLine(line: []const u8) void {
    ig.igTextUnformattedEx(line.ptr, line.ptr + line.len);
}

/// `layout.ini` beside the settings file, its folder made first.
fn enableLayoutPersistence(io: std.Io, settings_path: []const u8) void {
    var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = app_settings.layoutPath(&buffer, settings_path) orelse return;
    const folder = std.fs.path.dirname(path) orelse return;
    std.Io.Dir.cwd().createDirPath(io, folder) catch return;
    const z = std.mem.printSentinel(&layout_ini_path, "{s}", .{path}, 0) catch return;
    ig.igGetIO().*.IniFilename = z.ptr;
}

/// Through a temporary file and a rename, like the map editor's own.
fn writeSettingsFile(io: std.Io, path: []const u8, settings: *const app_settings.Settings) !void {
    var text_buffer: [16 * 1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&text_buffer);
    try app_settings.format(settings, &writer);
    if (std.fs.path.dirname(path)) |dir| try std.Io.Dir.cwd().createDirPath(io, dir);
    var temp_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const temp_path = std.fmt.bufPrint(&temp_buffer, "{s}.tmp", .{path}) catch return error.NameTooLong;
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = temp_path, .data = writer.buffered() });
    try std.Io.Dir.rename(std.Io.Dir.cwd(), temp_path, std.Io.Dir.cwd(), path, io);
}
