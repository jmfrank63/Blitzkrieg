//! What the translator edits, without a window: the open database, its
//! text tree (the core's, with each text's and folder's state), the selected
//! text's original and translation buffers, its state, and the save points
//! of D-15 (leaving a text, Save, Close, Exit, and every 30 seconds while
//! the edit is dirty). The UI draws this and the scripted runs drive it, so
//! both go through the same commit. Browsing (First, Previous, Next, Last)
//! asks the core's tree for the text a step reaches through the current
//! filter and selects it, which is a save point like any other selection.
//! Find Next asks the core's search for the next hit from where the last
//! one stopped and selects its text the same way; the statistics are the
//! core's, counted over the tree.
const std = @import("std");
const elk = @import("elk_core");

const Database = elk.database.Database;
pub const Tree = elk.tree.Tree;
pub const Text = elk.tree.Text;
pub const State = elk.state.State;
pub const Filter = elk.filter.Filter;
pub const Step = elk.tree.Step;
pub const FindOptions = elk.search.Options;
pub const Hit = elk.search.Hit;
pub const Statistic = elk.stats.Statistic;

/// D-15: a dirty edit is written this long after it became dirty.
pub const autosave_ms: u64 = 30_000;

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
    /// The open database's tree; it borrows the database's keys.
    tree: ?Tree = null,
    /// The selected text, an index into `tree.texts`.
    selected: ?usize = null,
    original: TextBuffer = .{},
    translation: TextBuffer = .{},
    /// The translation as it was when the text was selected or last saved:
    /// the edit is dirty while the buffer differs from it.
    saved_translation: std.ArrayListUnmanaged(u8) = .empty,
    state: State = .not_translated,
    /// A state button set the selected text's state, so an edit no longer
    /// turns it Translated (the MFC input view's `bManualState`).
    manual_state: bool = false,
    /// Where Find Next goes on from, on the text the last hit selected; a
    /// search from any other selected text starts at its beginning.
    find_cursor: ?elk.search.Cursor = null,
    /// The last Find Next's hit; null after a miss.
    last_hit: ?Hit = null,
    /// When the edit became dirty, on the caller's clock; null while clean.
    dirty_since_ms: ?u64 = null,
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

    /// How many texts the open database has; 0 when nothing is open.
    pub fn textCount(self: *const Session) usize {
        return if (self.tree) |tree| tree.texts.len else 0;
    }

    pub fn textAt(self: *const Session, index: usize) Text {
        return self.tree.?.texts[index];
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
        var tree = Tree.build(self.gpa, &database) catch |err| {
            self.fail("could not read the texts of {s}: {s}", .{ elk_path, @errorName(err) });
            return err;
        };
        errdefer tree.deinit();
        self.path_storage.clearRetainingCapacity();
        try self.path_storage.appendSlice(self.gpa, elk_path);
        self.database = database;
        self.tree = tree;
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
        if (index >= self.textCount()) return error.NoSuchText;
        if (self.selected == index) return;
        try self.save();
        const database = &self.database.?;
        const chosen = self.textAt(index);
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
        self.manual_state = false;
        self.dirty_since_ms = null;
        self.selected = index;
    }

    /// The text `step` reaches through `filter`, null when none does (the
    /// Browse commands are then disabled, as `IsNextItemExists` made them).
    pub fn browseTarget(self: *const Session, step: Step, filter: ?Filter) ?usize {
        const tree = &(self.tree orelse return null);
        const from: ?u32 = if (self.selected) |index| @intCast(index) else null;
        const target = tree.browse(step, from, filter) orelse return null;
        return target;
    }

    /// Browse First, Previous, Next or Last: selects the text the step
    /// reaches, saving the one being left. Returns false when none is
    /// reached, the selection then unchanged.
    pub fn browse(self: *Session, step: Step, filter: ?Filter) !bool {
        const target = self.browseTarget(step, filter) orelse return false;
        try self.select(target);
        return true;
    }

    /// The index of `key`, the first element that has it.
    pub fn find(self: *const Session, key: []const u8) ?usize {
        const tree = self.tree orelse return null;
        for (tree.texts, 0..) |text, index| {
            if (std.mem.eql(u8, text.key, key)) return index;
        }
        return null;
    }

    /// Edit > Find...'s Find Next: the edit is saved first, so the search
    /// reads what the box holds, then the hit's text is selected (a save
    /// point like any other). Null when nothing is found; the selection and
    /// the cursor then stay.
    pub fn findNext(self: *Session, options: FindOptions) !?Hit {
        const tree = &(self.tree orelse return null);
        try self.save();
        var from: ?elk.search.Cursor = null;
        if (self.selected) |index| {
            from = .{ .text = @intCast(index) };
            if (self.find_cursor) |cursor| if (cursor.text == index) {
                from = cursor;
            };
        }
        const hit = try elk.search.findNext(self.gpa, &self.database.?, tree, options, from) orelse {
            self.last_hit = null;
            return null;
        };
        try self.select(hit.text);
        self.find_cursor = hit.next();
        self.last_hit = hit;
        return hit;
    }

    /// Closing the Find dialog sends the next search back to the start of
    /// the selected text, as `FR_DIALOGTERM` reset the search position.
    pub fn resetFind(self: *Session) void {
        self.find_cursor = null;
        self.last_hit = null;
    }

    /// View > ELK Statistic: the counts of every text under the state the
    /// tree shows, its slices in `allocator`.
    pub fn statistics(self: *Session, allocator: std.mem.Allocator) !Statistic {
        const tree = &(self.tree orelse return error.NoDatabaseOpen);
        return elk.stats.compute(allocator, self.gpa, &self.database.?, tree) catch |err| {
            self.fail("could not count the texts: {s}", .{@errorName(err)});
            return err;
        };
    }

    /// Every save point: writes the selected text's translation when it was
    /// edited. An edited translation turns the text Translated, as the MFC
    /// input view did when its translation changed, unless a state button
    /// chose the state for this text.
    pub fn save(self: *Session) !void {
        const index = self.selected orelse return;
        if (!self.dirty()) return;
        const database = &self.database.?;
        const chosen = self.textAt(index);
        const edited = self.translation.text();
        _ = database.writeTranslation(chosen.element, chosen.key, edited) catch |err| {
            self.fail("could not save {s}: {s}", .{ chosen.key, @errorName(err) });
            return err;
        };
        if (!self.manual_state) try self.writeState(index, .translated);
        self.saved_translation.clearRetainingCapacity();
        try self.saved_translation.appendSlice(self.gpa, edited);
        self.dirty_since_ms = null;
        self.failure_len = 0;
    }

    /// The edit view's state buttons (`CInputViewDialog::On*RadioButton`):
    /// the selected text's state, written at once and shown up the tree.
    /// Outdated is set only by an update, never by hand.
    pub fn setState(self: *Session, new_state: State) !void {
        const index = self.selected orelse return error.NoTextSelected;
        if (new_state == .outdated) return error.OutdatedIsNotSetByHand;
        try self.writeState(index, new_state);
        self.manual_state = true;
        self.failure_len = 0;
    }

    fn writeState(self: *Session, index: usize, new_state: State) !void {
        const chosen = self.textAt(index);
        _ = self.database.?.setState(chosen.element, chosen.key, new_state) catch |err| {
            self.fail("could not save the state of {s}: {s}", .{ chosen.key, @errorName(err) });
            return err;
        };
        self.tree.?.setTextState(@intCast(index), new_state);
        self.state = new_state;
    }

    /// D-15's timed save point, called every frame with a millisecond clock:
    /// a dirty edit is saved `autosave_ms` after it became dirty, and again
    /// that long after each later change. A failed autosave keeps the
    /// reason in the status line and tries again after the next interval.
    /// Returns whether it saved.
    pub fn tick(self: *Session, now_ms: u64) bool {
        if (!self.dirty()) {
            self.dirty_since_ms = null;
            return false;
        }
        const since = self.dirty_since_ms orelse {
            self.dirty_since_ms = now_ms;
            return false;
        };
        if (now_ms -| since < autosave_ms) return false;
        self.save() catch {
            self.dirty_since_ms = now_ms;
            return false;
        };
        return true;
    }

    fn drop(self: *Session) void {
        if (self.tree) |*tree| tree.deinit();
        self.tree = null;
        if (self.database) |*database| database.deinit();
        self.database = null;
        self.selected = null;
        self.state = .not_translated;
        self.manual_state = false;
        self.dirty_since_ms = null;
        self.resetFind();
    }

    /// Keeps a failure for the status line; ELK's own (a failed file
    /// dialog) go here too.
    pub fn fail(self: *Session, comptime format: []const u8, args: anytype) void {
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
    try testing.expectEqual(@as(usize, 2852), session.textCount());
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
    try testing.expectEqual(@as(usize, 1), session.textCount());
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

test "a state button writes at once, shows up the tree and outlasts an edit" {
    const io = testing.io;
    const root = try fixture(io, "state-buttons");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    try session.select(0);
    const tree = &session.tree.?;
    const folder = tree.findFolder(0, "medals/1").?;
    try testing.expectEqual(State.not_translated, tree.nodes[folder].state);
    try testing.expectError(error.OutdatedIsNotSetByHand, session.setState(.outdated));
    try session.setState(.approved);
    try testing.expectEqual(State.approved, session.state);
    try testing.expectEqual(State.approved, tree.nodes[folder].state);
    try testing.expectEqual(State.approved, tree.nodes[0].state);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    try testing.expectEqual(State.approved, (try database.readState(0, "medals/1/name")).state);

    // The button chose the state: saving an edit keeps it Approved.
    try session.translation.set(testing.allocator, "Orden");
    try session.save();
    try testing.expectEqual(State.approved, (try database.readState(0, "medals/1/name")).state);
    try session.setState(.not_translated);
    try testing.expectEqual(State.not_translated, tree.nodes[0].state);
    const property = try database.readState(0, "medals/1/name");
    try testing.expectEqual(State.not_translated, property.state);
    try testing.expect(property.changed);
}

test "a dirty edit autosaves 30 seconds after it became dirty, and only then" {
    const io = testing.io;
    const root = try fixture(io, "autosave");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const txt = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base/medals/1/name.txt" });
    defer testing.allocator.free(txt);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    try session.select(0);
    // Clean: the clock runs on without a save.
    try testing.expect(!session.tick(0));
    try testing.expect(!session.tick(autosave_ms * 3));
    try testing.expect(!session.tick(autosave_ms * 4));
    try session.translation.set(testing.allocator, "Orden");
    try testing.expect(!session.tick(1_000));
    try testing.expect(!session.tick(1_000 + autosave_ms - 1));
    try testing.expectError(error.FileNotFound, std.Io.Dir.cwd().access(io, txt, .{}));
    try testing.expect(session.tick(1_000 + autosave_ms));
    try testing.expect(!session.dirty());

    // Dropped without a save point, as a crash would: the disk has the
    // autosaved text, not the later edit.
    try session.translation.set(testing.allocator, "Orden 2");
    try testing.expect(!session.tick(40_000));
    session.drop();
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    const written = try database.readTranslation(testing.allocator, 0, "medals/1/name");
    defer testing.allocator.free(written);
    try testing.expectEqualStrings("Orden", written);
    try testing.expectEqual(State.translated, (try database.readState(0, "medals/1/name")).state);
}

test "browsing through a filter saves the text it leaves and skips the rest" {
    const io = testing.io;
    const root = try fixture(io, "browse");
    defer testing.allocator.free(root);
    // A second text beside the fixture's, so there is somewhere to go.
    const cwd = std.Io.Dir.cwd();
    for ([_][]const u8{ "game_data_base/medals/1/desc.elk", "game_data_base/medals/1/desc.xml" }) |relative| {
        const source = try std.fs.path.join(testing.allocator, &.{ "Data/ELK", relative });
        defer testing.allocator.free(source);
        const bytes = try cwd.readFileAlloc(io, source, testing.allocator, .limited(1 << 20));
        defer testing.allocator.free(bytes);
        const out = try std.fs.path.join(testing.allocator, &.{ root, relative });
        defer testing.allocator.free(out);
        try cwd.writeFile(io, .{ .sub_path = out, .data = bytes });
    }
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    const desc = session.find("medals/1/desc").?;
    const name = session.find("medals/1/name").?;
    // Nothing selected: Next and Previous reach nothing, First and Last do.
    try testing.expectEqual(@as(?usize, null), session.browseTarget(.next, null));
    try testing.expect(try session.browse(.first, null));
    try testing.expectEqual(desc, session.selected.?);
    try testing.expect(try session.browse(.next, null));
    try testing.expectEqual(name, session.selected.?);
    try testing.expect(!try session.browse(.next, null));
    try testing.expectEqual(name, session.selected.?);

    // Approved only: name is the one; desc, edited, is left by Last.
    const approved = elk.filter.built_in[4];
    try session.setState(.approved);
    try testing.expect(try session.browse(.previous, null));
    try testing.expectEqual(desc, session.selected.?);
    try session.translation.set(testing.allocator, "Beschreibung");
    try testing.expect(try session.browse(.last, approved));
    try testing.expectEqual(name, session.selected.?);
    try testing.expectEqual(@as(?usize, null), session.browseTarget(.previous, approved));
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    const written = try database.readTranslation(testing.allocator, 0, "medals/1/desc");
    defer testing.allocator.free(written);
    try testing.expectEqualStrings("Beschreibung", written);
}

test "Find Next goes on from the last hit and starts over on another text" {
    const io = testing.io;
    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open("Data/ELK/elk.xml");
    const desc = session.find("medals/german/kuban/desc").?;
    const name = session.find("medals/german/kuban/name").?;
    const kuban: FindOptions = .{ .text = "Kuban" };
    try testing.expectEqual(@as(usize, 4), (try session.findNext(kuban)).?.offset);
    try testing.expectEqual(desc, session.selected.?);
    try testing.expectEqual(@as(usize, 68), (try session.findNext(kuban)).?.offset);
    try testing.expectEqual(elk.search.Field.original, (try session.findNext(kuban)).?.field);
    try testing.expectEqual(name, session.selected.?);
    // Chosen by hand, the desc is searched from its start again.
    try session.select(desc);
    try testing.expectEqual(@as(usize, 4), (try session.findNext(kuban)).?.offset);
    // A miss keeps the selection.
    try testing.expectEqual(@as(?Hit, null), try session.findNext(.{ .text = "kuban", .match_case = true }));
    try testing.expectEqual(desc, session.selected.?);
    try testing.expectEqual(@as(?Hit, null), session.last_hit);

    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const statistic = try session.statistics(arena.allocator());
    try testing.expectEqual(@as(i32, 2852), elk.stats.elementTotal(statistic.original[0]).texts);
    try session.close();
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
