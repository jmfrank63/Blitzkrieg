//! `BK_ELK_AUTO` and the smoke: the kit's schedule grammar
//! (`kit.auto_schedule`) replayed into ELK's own frame loop. Keys and typed
//! text become SDL events handed to ImGui's backend exactly as real ones
//! are, so the edit boxes see what a translator's keyboard would send;
//! `do=` and `expect=` resolve against ELK's own commands and state.
//!
//! What ELK drives in this slice: `key=`, `text=`, `do=`, `expect=`, `exit`.
//! The pointer actions, `open=`/`save`/`saveas=`, test-in-game and the
//! screenshot actions fail the run as not driven yet.
//!
//!   do=select:<index>        select the list's row, leaving the current text
//!   do=select_key:<key>      the same, by the text's key
//!   do=focus:translation     the translation box takes the keyboard
//!   do=save  do=close  do=reopen  (File > Save, File > Close, open again)
//!   do=open                  File > Open; the run shows no OS dialog
//!   do=answer:<path>         the waiting File > Open dialog chose <path>,
//!                            relative to the runner's dialog folder if set
//!   expect=open  expect=closed  expect=rows  expect=glyphs
//!   expect=texts:<n>  expect=title:<file>  expect=dirty:<0|1>
//!   expect=original_has:<text>  expect=translation:<text>
//!   expect=disk:<key>=<text>   the translation file as the core reads it
//!   expect=state:<key>=<not_translated|outdated|translated|approved>
//!   expect=untouched:<index>   the row's text files are byte-identical to
//!                              the source the smoke copied (smoke only)
//!   expect=identical           every file of the opened copy is byte-identical
//!                              to the source and none was added (smoke only)
const std = @import("std");
const kit = @import("editor_kit");
const sdl3 = @import("sdl3");
const elk = @import("elk_core");
const ui_mod = @import("ui.zig");
const fonts = @import("fonts.zig");
const window_mod = kit.window;

const schedule = kit.auto_schedule;
const c = sdl3.c;

pub const Runner = struct {
    entries: []const schedule.Scheduled,
    next: usize = 0,
    /// The database the smoke copied, for `expect=untouched`; "" otherwise.
    source_folder: []const u8 = "",
    /// How many files the smoke copied, for `expect=identical`.
    source_files: usize = 0,
    /// Where a scripted dialog answers, standing in for the folder the user
    /// browsed to: the grammar's arguments are short, the paths are not.
    dialog_folder: []const u8 = "",
    /// Text events borrow their bytes until ImGui copies them in.
    text_storage: [schedule.max_arg_len + 1]u8 = undefined,
    failure_storage: [512]u8 = undefined,
    failure: []const u8 = "",
    /// The last database the run had open, for `do=reopen` and the
    /// on-disk expectations (which also hold after File > Close).
    last_path: [std.Io.Dir.max_path_bytes]u8 = undefined,
    last_path_len: usize = 0,

    /// Runs every entry scheduled for `frame`; false when one failed
    /// (`failure` says which and why).
    pub fn step(self: *Runner, frame: u32, window: *window_mod.Window, ui: *ui_mod.Ui) bool {
        // File > Open opens in a frame, not in a command.
        self.remember(ui.session.openPath());
        while (self.next < self.entries.len and self.entries[self.next].frame <= frame) : (self.next += 1) {
            const entry = self.entries[self.next];
            std.debug.print("BK_ELK_AUTO: frame {d} action {s}\n", .{ frame, entry.text });
            self.run(entry.action, window, ui) catch |err| {
                if (self.failure.len == 0) self.fail("{s}: {s}", .{ entry.text, @errorName(err) });
                return false;
            };
            if (self.failure.len != 0) return false;
        }
        return true;
    }

    pub fn done(self: *const Runner) bool {
        return self.next >= self.entries.len;
    }

    fn run(self: *Runner, action: schedule.Action, window: *window_mod.Window, ui: *ui_mod.Ui) !void {
        switch (action) {
            .key => |pressed| try self.press(window, pressed),
            .text => |text| {
                @memcpy(self.text_storage[0..text.len], text);
                self.text_storage[text.len] = 0;
                var event = std.mem.zeroes(c.SDL_Event);
                event.text.type = c.SDL_EVENT_TEXT_INPUT;
                event.text.windowID = c.SDL_GetWindowID(window.window);
                event.text.text = &self.text_storage;
                _ = window.handleEvent(&event);
            },
            .do => |named| try self.command(window, ui, named),
            .expect => |named| try self.expect(window, ui, named),
            .exit => ui.requestExit(),
            else => self.fail("{s} is not driven by ELK yet", .{@tagName(action)}),
        }
    }

    fn press(self: *Runner, window: *window_mod.Window, key_action: schedule.Key) !void {
        var name_buffer: [64:0]u8 = undefined;
        const name = std.mem.printSentinel(&name_buffer, "{s}", .{key_action.name}, 0) catch return error.BadKey;
        const scancode = c.SDL_GetScancodeFromName(name.ptr);
        if (scancode == c.SDL_SCANCODE_UNKNOWN) return self.fail("unknown key {s}", .{key_action.name});
        var mods: c.SDL_Keymod = 0;
        if (key_action.mods.ctrl) mods |= c.SDL_KMOD_LCTRL;
        if (key_action.mods.shift) mods |= c.SDL_KMOD_LSHIFT;
        if (key_action.mods.alt) mods |= c.SDL_KMOD_LALT;
        if (key_action.mods.cmd) mods |= c.SDL_KMOD_LGUI;
        for ([_]bool{ true, false }) |down| {
            var event = std.mem.zeroes(c.SDL_Event);
            event.key.type = if (down) c.SDL_EVENT_KEY_DOWN else c.SDL_EVENT_KEY_UP;
            event.key.windowID = c.SDL_GetWindowID(window.window);
            event.key.scancode = scancode;
            event.key.key = c.SDL_GetKeyFromScancode(scancode, 0, false);
            // The backend takes ImGui's modifiers from each event, so the
            // release carries none: the modifiers go up with the key, as a
            // real keyboard's do, and Ctrl does not stay held over typing.
            event.key.mod = if (down) mods else 0;
            event.key.down = down;
            _ = window.handleEvent(&event);
        }
    }

    fn command(self: *Runner, window: *window_mod.Window, ui: *ui_mod.Ui, named: schedule.Named) !void {
        const session = ui.session;
        if (is(named, "select")) {
            const index = std.fmt.parseInt(usize, named.arg, 10) catch return self.fail("select needs a row number, got {s}", .{named.arg});
            try session.select(index);
        } else if (is(named, "select_key")) {
            const index = session.find(named.arg) orelse return self.fail("no text {s}", .{named.arg});
            try session.select(index);
        } else if (is(named, "focus") and std.mem.eql(u8, named.arg, "translation")) {
            ui.focus_translation = true;
        } else if (is(named, "save")) {
            try session.save();
        } else if (is(named, "close")) {
            try session.close();
        } else if (is(named, "open")) {
            ui.requestOpen(window);
        } else if (is(named, "answer")) {
            var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const path = if (self.dialog_folder.len == 0) named.arg else try std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ self.dialog_folder, std.fs.path.sep, named.arg });
            if (!ui.answerOpen(path)) return self.fail("do=answer: no File > Open dialog is waiting", .{});
        } else if (is(named, "reopen")) {
            var copy: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const last = self.last_path[0..self.last_path_len];
            @memcpy(copy[0..last.len], last);
            try session.open(copy[0..last.len]);
        } else return self.fail("unknown command do={s}", .{named.name});
        self.remember(session.openPath());
    }

    pub fn remember(self: *Runner, opened: []const u8) void {
        if (opened.len == 0 or opened.len > self.last_path.len) return;
        @memcpy(self.last_path[0..opened.len], opened);
        self.last_path_len = opened.len;
    }

    fn expect(self: *Runner, window: *window_mod.Window, ui: *ui_mod.Ui, named: schedule.Named) !void {
        const session = ui.session;
        const gpa = ui.gpa;
        if (is(named, "open")) {
            if (!session.isOpen()) return self.fail("expect=open: no database is open ({s})", .{session.failureText()});
        } else if (is(named, "closed")) {
            if (session.isOpen()) return self.fail("expect=closed: {s} is still open", .{session.openPath()});
        } else if (is(named, "rows")) {
            if (ui.rows_drawn == 0) return self.fail("expect=rows: the list drew no rows", .{});
        } else if (is(named, "glyphs")) {
            for (fonts.samples) |codepoint| {
                if (!fonts.covers(codepoint)) return self.fail("expect=glyphs: the font stack has no glyph for U+{X:0>4}", .{codepoint});
            }
        } else if (is(named, "texts")) {
            const wanted = std.fmt.parseInt(usize, named.arg, 10) catch return self.fail("texts needs a number", .{});
            if (session.entries.items.len != wanted) return self.fail("expect=texts: {d} texts, wanted {d}", .{ session.entries.items.len, wanted });
        } else if (is(named, "title")) {
            const shown = std.mem.span(c.SDL_GetWindowTitle(window.window));
            var buffer: [512]u8 = undefined;
            const wanted = std.fmt.bufPrint(&buffer, "{s} - {s}", .{ named.arg, ui_mod.app_title }) catch return error.TitleTooLong;
            if (!std.mem.eql(u8, shown, wanted)) return self.fail("expect=title: the window says \"{s}\", wanted \"{s}\"", .{ shown, wanted });
        } else if (is(named, "dirty")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (session.dirty() != wanted) return self.fail("expect=dirty: dirty is {}, wanted {}", .{ session.dirty(), wanted });
        } else if (is(named, "original_has")) {
            if (std.mem.indexOf(u8, session.original.text(), named.arg) == null) return self.fail("expect=original_has: the original \"{s}\" lacks \"{s}\"", .{ session.original.text(), named.arg });
        } else if (is(named, "translation")) {
            if (!std.mem.eql(u8, session.translation.text(), named.arg)) return self.fail("expect=translation: the box holds \"{s}\", wanted \"{s}\"", .{ session.translation.text(), named.arg });
        } else if (is(named, "disk") or is(named, "state")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("{s} needs <key>=<value>", .{named.name});
            const text_key = named.arg[0..eq];
            const wanted = named.arg[eq + 1 ..];
            var database = try elk.database.Database.open(gpa, ui.io, self.last_path[0..self.last_path_len]);
            defer database.deinit();
            const element = elementOf(&database, text_key) orelse return self.fail("{s}: no text {s} in the database", .{ named.name, text_key });
            if (is(named, "disk")) {
                const on_disk = try database.readTranslation(gpa, element, text_key);
                defer gpa.free(on_disk);
                if (!std.mem.eql(u8, on_disk, wanted)) return self.fail("expect=disk: {s} holds \"{s}\" on disk, wanted \"{s}\"", .{ text_key, on_disk, wanted });
            } else {
                const property = try database.readState(element, text_key);
                if (!std.mem.eql(u8, @tagName(property.state), wanted)) return self.fail("expect=state: {s} is {s} on disk, wanted {s}", .{ text_key, @tagName(property.state), wanted });
            }
        } else if (is(named, "untouched")) {
            try self.untouched(ui, named.arg);
        } else if (is(named, "identical")) {
            try self.identical(ui);
        } else return self.fail("unknown expectation expect={s}", .{named.name});
    }

    /// The row's translation and state files compare byte for byte with
    /// the source the smoke copied: visiting a text never rewrites it.
    fn untouched(self: *Runner, ui: *ui_mod.Ui, arg: []const u8) !void {
        if (self.source_folder.len == 0) return self.fail("expect=untouched needs the smoke's source", .{});
        const index = std.fmt.parseInt(usize, arg, 10) catch return self.fail("untouched needs a row number", .{});
        const session = ui.session;
        const text = session.entryAt(index);
        const database = &session.database.?;
        const base = database.bases[text.element].path.?;
        const relative_base = base[database.folder.len + 1 ..];
        const cwd = std.Io.Dir.cwd();
        for ([_][]const u8{ ".txt", ".xml", ".elk" }) |extension| {
            var copy_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            var source_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const copy_path = try std.fmt.bufPrint(&copy_buffer, "{s}/{s}{s}", .{ base, text.key, extension });
            const source_path = try std.fmt.bufPrint(&source_buffer, "{s}/{s}/{s}{s}", .{ self.source_folder, relative_base, text.key, extension });
            const copied = cwd.readFileAlloc(ui.io, copy_path, ui.gpa, .limited(16 << 20)) catch null;
            defer if (copied) |bytes| ui.gpa.free(bytes);
            const source = cwd.readFileAlloc(ui.io, source_path, ui.gpa, .limited(16 << 20)) catch null;
            defer if (source) |bytes| ui.gpa.free(bytes);
            const same = if (copied != null and source != null) std.mem.eql(u8, copied.?, source.?) else copied == null and source == null;
            if (!same) return self.fail("expect=untouched: {s}{s} changed though it was only visited", .{ text.key, extension });
        }
    }

    /// Every file under the opened database's folder equals the source's
    /// file of the same name, and there are as many as the smoke copied: a
    /// Save of an untouched database wrote nothing that differs.
    fn identical(self: *Runner, ui: *ui_mod.Ui) !void {
        if (self.source_folder.len == 0) return self.fail("expect=identical needs the smoke's source", .{});
        const folder = std.fs.path.dirname(self.last_path[0..self.last_path_len]) orelse return self.fail("expect=identical: no database was opened", .{});
        const cwd = std.Io.Dir.cwd();
        var copy = try cwd.openDir(ui.io, folder, .{ .iterate = true });
        defer copy.close(ui.io);
        var source = try cwd.openDir(ui.io, self.source_folder, .{});
        defer source.close(ui.io);
        var walker = try copy.walk(ui.gpa);
        defer walker.deinit();
        var count: usize = 0;
        while (try walker.next(ui.io)) |file| {
            if (file.kind != .file) continue;
            count += 1;
            const copied = try copy.readFileAlloc(ui.io, file.path, ui.gpa, .limited(16 << 20));
            defer ui.gpa.free(copied);
            const original = source.readFileAlloc(ui.io, file.path, ui.gpa, .limited(16 << 20)) catch
                return self.fail("expect=identical: {s} is new", .{file.path});
            defer ui.gpa.free(original);
            if (!std.mem.eql(u8, copied, original)) return self.fail("expect=identical: {s} differs from the source", .{file.path});
        }
        if (count != self.source_files) return self.fail("expect=identical: {d} files, the smoke copied {d}", .{ count, self.source_files });
    }

    fn fail(self: *Runner, comptime format: []const u8, args: anytype) void {
        self.failure = std.fmt.bufPrint(&self.failure_storage, format, args) catch self.failure_storage[0..];
    }
};

/// The first element whose texts include `key`.
fn elementOf(database: *const elk.database.Database, key: []const u8) ?usize {
    for (database.bases, 0..) |base, element| {
        for (base.keys) |candidate| if (std.mem.eql(u8, candidate, key)) return element;
    }
    return null;
}

fn is(named: schedule.Named, name: []const u8) bool {
    return std.mem.eql(u8, named.name, name);
}
