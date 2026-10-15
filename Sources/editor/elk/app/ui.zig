//! The extended layout's minimum for a translator: File menu, the list of
//! texts (the tree comes later), the selected text's original and its
//! translation, and a status line. Every save point goes through the
//! session (D-15): leaving a text, Save (Ctrl+S), Close and Exit.
const std = @import("std");
const imgui = @import("editor_imgui");
const session_mod = @import("session.zig");
const settings_mod = @import("settings.zig");
const window_mod = @import("window.zig");

const ig = imgui.c;
const Session = session_mod.Session;

pub const app_title = "Blitzkrieg ELK";

pub const Ui = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    session: *Session,
    /// Null for scripted runs, which never read or write the user's settings.
    settings: ?*settings_mod.Settings,
    settings_path: []const u8 = "",
    /// Set by `do=focus:translation`: the translation box takes the keyboard
    /// at the next frame.
    focus_translation: bool = false,
    /// How many list rows the last frame drew (the smoke's proof the list
    /// was laid out).
    rows_drawn: usize = 0,
    quit: bool = false,
    /// The last Exit could not save; the next one quits anyway.
    exit_refused: bool = false,
    title_storage: [512:0]u8 = @splat(0),

    /// Opens `elk_path` and records it as the most recent database.
    pub fn openDatabase(self: *Ui, elk_path: []const u8) void {
        self.session.open(elk_path) catch {
            if (self.settings) |settings| settings.forgetRecent(elk_path);
            self.saveSettings();
            return;
        };
        if (self.settings) |settings| settings.pushRecent(elk_path);
        self.saveSettings();
    }

    /// File > Exit and the window's close button: Exit is a save point. A
    /// failed save keeps ELK open with the reason in the status line, and a
    /// second Exit then quits without saving, so a database that cannot be
    /// written never holds the window open.
    pub fn requestExit(self: *Ui) void {
        self.session.close() catch {
            if (self.exit_refused) self.quit = true;
            self.exit_refused = true;
            return;
        };
        self.quit = true;
    }

    pub fn select(self: *Ui, index: usize) void {
        self.session.select(index) catch {};
    }

    pub fn saveSettings(self: *Ui) void {
        const settings = self.settings orelse return;
        if (self.settings_path.len == 0) return;
        var buffer: [16 * 1024]u8 = undefined;
        var writer: std.Io.Writer = .fixed(&buffer);
        settings_mod.format(settings, &writer) catch return;
        const cwd = std.Io.Dir.cwd();
        if (std.fs.path.dirname(self.settings_path)) |folder| cwd.createDirPath(self.io, folder) catch return;
        cwd.writeFile(self.io, .{ .sub_path = self.settings_path, .data = writer.buffered() }) catch return;
    }

    /// The window title: `<file> - Blitzkrieg ELK`, as the MFC document
    /// frame showed it, or the application's name alone.
    pub fn title(self: *Ui) [:0]const u8 {
        const opened = self.session.openPath();
        const text = if (opened.len == 0)
            std.mem.printSentinel(&self.title_storage, "{s}", .{app_title}, 0)
        else
            std.mem.printSentinel(&self.title_storage, "{s} - {s}", .{ std.fs.path.basename(opened), app_title }, 0);
        return text catch app_title;
    }

    pub fn draw(self: *Ui, window: *window_mod.Window) void {
        window.setTitle(self.title().ptr);
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_S, ig.ImGuiInputFlags_RouteGlobal)) self.session.save() catch {};

        const viewport = ig.igGetMainViewport();
        ig.igSetNextWindowPos(viewport.*.WorkPos, 0);
        ig.igSetNextWindowSize(viewport.*.WorkSize, 0);
        const flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoMove | ig.ImGuiWindowFlags_NoSavedSettings |
            ig.ImGuiWindowFlags_MenuBar | ig.ImGuiWindowFlags_NoBringToFrontOnFocus;
        _ = ig.igBegin("ELK", null, flags);
        defer ig.igEnd();
        self.drawMenu();
        self.rows_drawn = 0;
        if (!self.session.isOpen()) {
            ig.igTextUnformatted("No database is open. Start ELK with the path of an elk.xml, or pick one under File > Recent.");
            self.drawFailure();
            return;
        }
        const avail = ig.igGetContentRegionAvail();
        const body_height = avail.y - ig.igGetFrameHeightWithSpacing();
        if (ig.igBeginChild("texts", .{ .x = avail.x * 0.35, .y = body_height }, ig.ImGuiChildFlags_Borders, 0)) self.drawList();
        ig.igEndChild();
        ig.igSameLine();
        if (ig.igBeginChild("text", .{ .x = 0, .y = body_height }, 0, 0)) self.drawText();
        ig.igEndChild();
        self.drawStatus();
    }

    fn drawMenu(self: *Ui) void {
        if (!ig.igBeginMenuBar()) return;
        defer ig.igEndMenuBar();
        if (!ig.igBeginMenu("File")) return;
        defer ig.igEndMenu();
        const settings_count = if (self.settings) |settings| settings.recentCount() else 0;
        if (ig.igBeginMenuEx("Recent", settings_count != 0)) {
            defer ig.igEndMenu();
            var path_buffer: [settings_mod.max_path:0]u8 = undefined;
            var index: usize = 0;
            while (index < settings_count) : (index += 1) {
                const recent = self.settings.?.recentAt(index);
                const label = std.mem.printSentinel(&path_buffer, "{s}", .{recent}, 0) catch continue;
                if (ig.igMenuItem(label.ptr)) {
                    // The list changes as it opens; open a copy.
                    var copy: [settings_mod.max_path]u8 = undefined;
                    @memcpy(copy[0..recent.len], recent);
                    self.openDatabase(copy[0..recent.len]);
                    break;
                }
            }
        }
        const open = self.session.isOpen();
        if (ig.igMenuItemEx("Save", "Ctrl+S", false, open)) self.session.save() catch {};
        if (ig.igMenuItemEx("Close", null, false, open)) self.session.close() catch {};
        ig.igSeparator();
        if (ig.igMenuItem("Exit")) self.requestExit();
    }

    fn drawList(self: *Ui) void {
        var clipper = std.mem.zeroes(ig.ImGuiListClipper);
        ig.ImGuiListClipper_Begin(&clipper, @intCast(self.session.entries.items.len), -1.0);
        defer ig.ImGuiListClipper_End(&clipper);
        var label: [512:0]u8 = undefined;
        while (ig.ImGuiListClipper_Step(&clipper)) {
            var row: usize = @intCast(clipper.DisplayStart);
            while (row < clipper.DisplayEnd) : (row += 1) {
                const key = self.session.entryAt(row).key;
                const text = std.mem.printSentinel(&label, "{s}", .{key}, 0) catch continue;
                ig.igPushIDInt(@intCast(row));
                defer ig.igPopID();
                if (ig.igSelectableEx(text.ptr, self.session.selected == row, 0, .{ .x = 0, .y = 0 })) self.select(row);
                self.rows_drawn += 1;
            }
        }
    }

    fn drawText(self: *Ui) void {
        const index = self.session.selected orelse {
            ig.igTextUnformatted("Select a text in the list.");
            return;
        };
        const half = (ig.igGetContentRegionAvail().y - 2 * ig.igGetTextLineHeightWithSpacing()) / 2;
        // The text's own ID, so an edit box active on the previous text does
        // not carry its buffer over to this one.
        ig.igPushIDInt(@intCast(index));
        defer ig.igPopID();
        ig.igTextUnformatted("Original");
        if (self.session.original.storage(self.gpa)) |storage| {
            _ = ig.igInputTextMultilineEx("##original", storage.ptr, storage.len, .{ .x = -1, .y = half }, ig.ImGuiInputTextFlags_ReadOnly, null, null);
        } else |_| {}
        ig.igTextUnformatted("Translation");
        if (self.focus_translation) {
            ig.igSetKeyboardFocusHere();
            self.focus_translation = false;
        }
        if (self.session.translation.storage(self.gpa)) |storage| {
            _ = ig.igInputTextMultilineEx("##translation", storage.ptr, storage.len, .{ .x = -1, .y = -1 }, ig.ImGuiInputTextFlags_CallbackResize, resizeTranslation, self);
            self.session.translation.settle();
        } else |_| {}
    }

    fn drawStatus(self: *Ui) void {
        var buffer: [768:0]u8 = undefined;
        const failure = self.session.failureText();
        const text = if (failure.len != 0)
            std.mem.printSentinel(&buffer, "{s}{s}", .{ failure, if (self.exit_refused) " | Exit again to quit without saving" else "" }, 0)
        else if (self.session.selected) |index|
            std.mem.printSentinel(&buffer, "{d} texts | {s} | {s}{s}", .{
                self.session.entries.items.len,
                self.session.entryAt(index).key,
                stateName(self.session.state),
                if (self.session.dirty()) " | unsaved" else "",
            }, 0)
        else
            std.mem.printSentinel(&buffer, "{d} texts", .{self.session.entries.items.len}, 0);
        ig.igTextUnformatted((text catch return).ptr);
    }

    fn drawFailure(self: *Ui) void {
        const failure = self.session.failureText();
        if (failure.len == 0) return;
        var buffer: [512:0]u8 = undefined;
        const text = std.mem.printSentinel(&buffer, "{s}", .{failure}, 0) catch return;
        ig.igTextUnformatted(text.ptr);
    }
};

pub fn stateName(state: session_mod.State) []const u8 {
    return switch (state) {
        .not_translated => "Not translated",
        .outdated => "Outdated",
        .translated => "Translated",
        .approved => "Approved",
    };
}

/// ImGui grows the translation buffer through this when an edit needs more
/// room than it has.
fn resizeTranslation(data: [*c]ig.ImGuiInputTextCallbackData) callconv(.c) c_int {
    const event = &data[0];
    if (event.EventFlag != ig.ImGuiInputTextFlags_CallbackResize) return 0;
    const self: *Ui = @ptrCast(@alignCast(event.UserData));
    if (self.session.translation.grow(self.gpa, @intCast(event.BufSize), @intCast(event.BufTextLen))) |storage| event.Buf = storage.ptr;
    return 0;
}
