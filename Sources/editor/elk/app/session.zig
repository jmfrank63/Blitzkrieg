//! What the translator edits, without a window: the open database, the flat
//! list of its texts, the selected text's original and translation buffers,
//! and the save points of D-15 (leaving a text, Save, Close, Exit). The UI
//! draws this and the scripted runs drive it, so both go through the same
//! commit. The 30-second autosave of D-15 comes with a later slice.
const std = @import("std");
const elk = @import("elk_core");

const Database = elk.database.Database;
pub const State = elk.state.State;

/// One text: its element in `elk.xml` and its key below that element's
/// `<element>_data_base/` folder. The key is the database's own (borrowed).
pub const Entry = struct {
    element: usize,
    key: []const u8,
};

/// A NUL-terminated byte buffer ImGui edits in place and grows through its
/// resize callback. `bytes.items` always ends in the terminator.
pub const TextBuffer = struct {
    bytes: std.ArrayListUnmanaged(u8) = .empty,

    pub fn deinit(self: *TextBuffer, gpa: std.mem.Allocator) void {
        self.bytes.deinit(gpa);
    }

    pub fn set(self: *TextBuffer, gpa: std.mem.Allocator, value: []const u8) !void {
        self.bytes.clearRetainingCapacity();
        try self.bytes.ensureTotalCapacity(gpa, value.len + 1);
        self.bytes.appendSliceAssumeCapacity(value);
        self.bytes.appendAssumeCapacity(0);
    }

    pub fn text(self: *const TextBuffer) []const u8 {
        if (self.bytes.items.len == 0) return "";
        return std.mem.sliceTo(self.bytes.items, 0);
    }

    /// The whole allocation, for ImGui's `buf` and `buf_size`.
    pub fn storage(self: *TextBuffer, gpa: std.mem.Allocator) ![]u8 {
        if (self.bytes.items.len == 0) try self.set(gpa, "");
        return self.bytes.allocatedSlice();
    }

    /// ImGui asks for `size` bytes (terminator included) holding `len` bytes
    /// of text; returns the new storage, or null when it could not grow.
    pub fn grow(self: *TextBuffer, gpa: std.mem.Allocator, size: usize, len: usize) ?[]u8 {
        self.bytes.ensureTotalCapacity(gpa, size) catch return null;
        self.bytes.items.len = @min(len + 1, self.bytes.capacity);
        return self.bytes.allocatedSlice();
    }

    /// After ImGui wrote into `storage`: the text ends at the first NUL.
    pub fn settle(self: *TextBuffer) void {
        const all = self.bytes.allocatedSlice();
        const len = std.mem.indexOfScalar(u8, all, 0) orelse all.len - 1;
        all[len] = 0;
        self.bytes.items.len = len + 1;
    }
};

pub const Session = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    database: ?Database = null,
    /// Owns the copy of the opened path (`path`).
    path_storage: std.ArrayListUnmanaged(u8) = .empty,
    entries: std.ArrayListUnmanaged(Entry) = .empty,
    selected: ?usize = null,
    original: TextBuffer = .{},
    translation: TextBuffer = .{},
    /// The translation as it was when the text was selected or last saved:
    /// the edit is dirty while the buffer differs from it.
    saved_translation: std.ArrayListUnmanaged(u8) = .empty,
    state: State = .not_translated,
    /// The last save point's failure, for the status bar; "" when it worked.
    failure_storage: [256]u8 = undefined,
    failure_len: usize = 0,

    pub fn init(gpa: std.mem.Allocator, io: std.Io) Session {
        return .{ .gpa = gpa, .io = io };
    }

    /// Drops everything without saving; `close` is the save point.
    pub fn deinit(self: *Session) void {
        self.drop();
        self.path_storage.deinit(self.gpa);
        self.entries.deinit(self.gpa);
        self.original.deinit(self.gpa);
        self.translation.deinit(self.gpa);
        self.saved_translation.deinit(self.gpa);
    }

    pub fn isOpen(self: *const Session) bool {
        return self.database != null;
    }

    /// The opened `elk.xml` as given; "" when nothing is open.
    pub fn openPath(self: *const Session) []const u8 {
        return if (self.database != null) self.path_storage.items else "";
    }

    pub fn failureText(self: *const Session) []const u8 {
        return self.failure_storage[0..self.failure_len];
    }

    pub fn dirty(self: *const Session) bool {
        if (self.selected == null) return false;
        return !std.mem.eql(u8, self.translation.text(), self.saved_translation.items);
    }

    pub fn entryAt(self: *const Session, index: usize) Entry {
        return self.entries.items[index];
    }

    /// Opens `elk_path`, closing (and so saving) what was open first. On
    /// failure the previous database stays closed and the reason is kept.
    pub fn open(self: *Session, elk_path: []const u8) !void {
        try self.close();
        var database = Database.open(self.gpa, self.io, elk_path) catch |err| {
            self.fail("could not open {s}: {s}", .{ elk_path, @errorName(err) });
            return err;
        };
        errdefer database.deinit();
        self.entries.clearRetainingCapacity();
        for (database.bases, 0..) |base, element| {
            for (base.keys) |key| try self.entries.append(self.gpa, .{ .element = element, .key = key });
        }
        self.path_storage.clearRetainingCapacity();
        try self.path_storage.appendSlice(self.gpa, elk_path);
        self.database = database;
        self.failure_len = 0;
    }

    /// File > Close: saves the selected text, then closes the database. A
    /// failed save keeps the database open so the edit is not lost.
    pub fn close(self: *Session) !void {
        if (self.database == null) return;
        try self.save();
        self.drop();
    }

    /// Selects `index`, saving the text being left first. A failed save
    /// keeps the current text selected.
    pub fn select(self: *Session, index: usize) !void {
        if (index >= self.entries.items.len) return error.NoSuchText;
        if (self.selected == index) return;
        try self.save();
        const database = &self.database.?;
        const chosen = self.entries.items[index];
        const original = try database.readOriginal(self.gpa, chosen.element, chosen.key);
        defer self.gpa.free(original);
        const translation = try database.readTranslation(self.gpa, chosen.element, chosen.key);
        defer self.gpa.free(translation);
        const property = try database.readState(chosen.element, chosen.key);
        try self.original.set(self.gpa, original);
        try self.translation.set(self.gpa, translation);
        self.saved_translation.clearRetainingCapacity();
        try self.saved_translation.appendSlice(self.gpa, translation);
        self.state = property.state;
        self.selected = index;
    }

    /// The index of `key` in the list, the first element that has it.
    pub fn find(self: *const Session, key: []const u8) ?usize {
        for (self.entries.items, 0..) |item, index| {
            if (std.mem.eql(u8, item.key, key)) return index;
        }
        return null;
    }

    /// Every save point: writes the selected text's translation when it was
    /// edited. An edited translation turns the text Translated, as the MFC
    /// input view did when its translation changed (the state buttons that
    /// override that come with a later slice).
    pub fn save(self: *Session) !void {
        const index = self.selected orelse return;
        if (!self.dirty()) return;
        const database = &self.database.?;
        const chosen = self.entries.items[index];
        const edited = self.translation.text();
        _ = database.writeTranslation(chosen.element, chosen.key, edited) catch |err| {
            self.fail("could not save {s}: {s}", .{ chosen.key, @errorName(err) });
            return err;
        };
        _ = database.setState(chosen.element, chosen.key, .translated) catch |err| {
            self.fail("could not save the state of {s}: {s}", .{ chosen.key, @errorName(err) });
            return err;
        };
        self.state = .translated;
        self.saved_translation.clearRetainingCapacity();
        try self.saved_translation.appendSlice(self.gpa, edited);
        self.failure_len = 0;
    }

    fn drop(self: *Session) void {
        if (self.database) |*database| database.deinit();
        self.database = null;
        self.entries.clearRetainingCapacity();
        self.selected = null;
        self.state = .not_translated;
    }

    fn fail(self: *Session, comptime format: []const u8, args: anytype) void {
        const written = std.fmt.bufPrint(&self.failure_storage, format, args) catch {
            // Cut to the buffer: the start of the message says what failed.
            self.failure_len = self.failure_storage.len;
            return;
        };
        self.failure_len = written.len;
    }
};

const testing = std.testing;
const scratch = "zig-out/local-test/elk/app";

/// A copy of the tracked database's project file and one text, so the test
/// never writes into Data/ELK (D-19).
fn fixture(io: std.Io, name: []const u8) ![]u8 {
    const root = try std.fs.path.join(testing.allocator, &.{ scratch, name });
    errdefer testing.allocator.free(root);
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, root) catch {};
    const files = [_][]const u8{ "elk.xml", "game_data_base/medals/1/name.elk", "game_data_base/medals/1/name.xml" };
    for (files) |relative| {
        const source = try std.fs.path.join(testing.allocator, &.{ "Data/ELK", relative });
        defer testing.allocator.free(source);
        const bytes = try cwd.readFileAlloc(io, source, testing.allocator, .limited(1 << 20));
        defer testing.allocator.free(bytes);
        const out = try std.fs.path.join(testing.allocator, &.{ root, relative });
        defer testing.allocator.free(out);
        try cwd.createDirPath(io, std.fs.path.dirname(out).?);
        try cwd.writeFile(io, .{ .sub_path = out, .data = bytes });
    }
    return root;
}

test "the tracked database lists every text and shows the selected one" {
    const io = testing.io;
    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open("Data/ELK/elk.xml");
    try testing.expectEqual(@as(usize, 2852), session.entries.items.len);
    const index = session.find("medals/1/name").?;
    try session.select(index);
    try testing.expectEqualStrings("Test Medal", session.original.text());
    try testing.expectEqualStrings("", session.translation.text());
    try testing.expect(!session.dirty());
    // Leaving an untouched text writes nothing (Data/ELK is never written).
    try session.select(0);
    try session.close();
    try testing.expect(!session.isOpen());
}

test "each save point writes an edited translation and turns it Translated" {
    const io = testing.io;
    const root = try fixture(io, "save-points");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const txt = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base/medals/1/name.txt" });
    defer testing.allocator.free(txt);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    try testing.expectEqual(@as(usize, 1), session.entries.items.len);
    try session.select(0);
    try session.translation.set(testing.allocator, "Orden");
    try testing.expect(session.dirty());
    try session.save();
    try testing.expect(!session.dirty());
    try testing.expectEqual(State.translated, session.state);

    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    const written = try database.readTranslation(testing.allocator, 0, "medals/1/name");
    defer testing.allocator.free(written);
    try testing.expectEqualStrings("Orden", written);
    try testing.expectEqual(State.translated, (try database.readState(0, "medals/1/name")).state);

    // Close is a save point too.
    try session.translation.set(testing.allocator, "Orden 2");
    try session.close();
    const closed = try database.readTranslation(testing.allocator, 0, "medals/1/name");
    defer testing.allocator.free(closed);
    try testing.expectEqualStrings("Orden 2", closed);
}

test "the text buffer grows for ImGui and settles at the terminator" {
    var buffer: TextBuffer = .{};
    defer buffer.deinit(testing.allocator);
    try buffer.set(testing.allocator, "ab");
    const grown = buffer.grow(testing.allocator, 64, 2).?;
    try testing.expect(grown.len >= 64);
    @memcpy(grown[0..4], "abcd");
    grown[4] = 0;
    buffer.settle();
    try testing.expectEqualStrings("abcd", buffer.text());
}
