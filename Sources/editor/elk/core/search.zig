//! Find (`CELKTreeWindow::FindItem` with the main frame's `SSearchParam`,
//! Sources/src/ELK/ELK_TreeWindow.cpp): from where the last find stopped,
//! each text's original, then its description, then its translation is
//! searched for the typed text, and the walk goes on to the next text in
//! the tree's order (or the previous one, searching up); the filter plays no
//! part, as it did not. Without Match case the text and the search are both
//! lower-cased first; with Whole word a hit must have a spelling delimiter
//! (or the text's end) on either side. A hit leaves the cursor one past its
//! start, so Find Next finds the next hit in the same field.
//!
//! Changed from the MFC search: it wraps, going on from the other end of
//! the tree and stopping when it is back where it started, where the MFC
//! one stopped at the end; a hit Whole word rejects no longer skips the rest
//! of that field (the MFC loop moved on to the next field, so "cat" in
//! "concatenate the cat" was never found); and the texts are the editor's
//! (WTF-8, LF), so an offset is a byte offset into what the edit box holds.
const std = @import("std");
const database_mod = @import("database.zig");
const tree_mod = @import("tree.zig");
const stats = @import("stats.zig");
const Progress = @import("progress.zig").Progress;

const Database = database_mod.Database;
const Tree = tree_mod.Tree;

/// The three views a text is searched in, in the order they are searched.
pub const Field = enum(u2) { original, description, translation };

pub const Options = struct {
    text: []const u8,
    down: bool = true,
    match_case: bool = false,
    whole_word: bool = false,
};

/// Where the next search starts: a text (an index into `Tree.texts`), the
/// field in it and the byte offset in that field (`nWindowType`,
/// `nPosition`).
pub const Cursor = struct {
    text: u32,
    field: Field = .original,
    offset: usize = 0,
};

pub const Hit = struct {
    text: u32,
    field: Field,
    offset: usize,
    len: usize,

    /// Where Find Next goes on from: one past the hit's start.
    pub fn next(self: Hit) Cursor {
        return .{ .text = self.text, .field = self.field, .offset = self.offset + 1 };
    }
};

/// The next hit from `from` in the direction the options say, wrapping
/// once round the tree; null when the text is empty or found nowhere.
/// Without a cursor the search starts at the first text (the last, going
/// up). With a `Progress` (D-14) each text visited is a unit, and Cancel
/// stops it with `error.Cancelled`.
pub fn findNext(gpa: std.mem.Allocator, database: *Database, tree: *const Tree, options: Options, from: ?Cursor, progress: ?*Progress) !?Hit {
    if (options.text.len == 0 or tree.texts.len == 0) return null;
    const needle = try gpa.dupe(u8, options.text);
    defer gpa.free(needle);
    if (!options.match_case) foldCase(needle);
    const start = from orelse Cursor{ .text = tree.browse(if (options.down) .first else .last, null, null).? };

    var cursor = start;
    // Every text once, and the first one again from its start, so a hit
    // before the cursor in it is found after the wrap.
    var visits: usize = 0;
    Progress.start(progress, tree.texts.len + 1);
    while (visits <= tree.texts.len) : (visits += 1) {
        try Progress.advance(progress, database.io);
        var field = @intFromEnum(cursor.field);
        while (field <= @intFromEnum(Field.translation)) : (field += 1) {
            const offset = if (field == @intFromEnum(cursor.field)) cursor.offset else 0;
            if (try searchField(gpa, database, tree.texts[cursor.text], @enumFromInt(field), needle, options, offset)) |at| {
                return .{ .text = cursor.text, .field = @enumFromInt(field), .offset = at, .len = needle.len };
            }
        }
        const step: tree_mod.Step = if (options.down) .next else .previous;
        const wrap: tree_mod.Step = if (options.down) .first else .last;
        const following = tree.browse(step, cursor.text, null) orelse tree.browse(wrap, null, null).?;
        cursor = .{ .text = following };
    }
    return null;
}

/// The first acceptable hit of `needle` (already folded unless Match case)
/// in one field of `text` at or after `offset`.
fn searchField(gpa: std.mem.Allocator, database: *Database, text: tree_mod.Text, field: Field, needle: []const u8, options: Options, offset: usize) !?usize {
    const content = switch (field) {
        .original => try database.readOriginal(gpa, text.element, text.key),
        .description => try database.readDescription(gpa, text.element, text.key),
        .translation => try database.readTranslation(gpa, text.element, text.key),
    };
    defer gpa.free(content);
    if (!options.match_case) foldCase(content);
    var at = offset;
    while (at <= content.len) {
        const found = std.mem.indexOfPos(u8, content, at, needle) orelse return null;
        if (!options.whole_word or isWholeWord(content, found, needle.len)) return found;
        at = found + 1;
    }
    return null;
}

fn isWholeWord(content: []const u8, at: usize, len: usize) bool {
    if (at > 0 and !isDelimiter(content[at - 1])) return false;
    if (at + len < content.len and !isDelimiter(content[at + len])) return false;
    return true;
}

/// A byte of a multi-byte character is never a delimiter, as no byte of a
/// code page's own letters was in the MFC set.
fn isDelimiter(byte: u8) bool {
    return std.mem.indexOfScalar(u8, stats.spelling_delimiters, byte) != null;
}

/// Lower case in place for the letters the shipped translations use, as
/// `CString::MakeLower` lowered them in the Western and Cyrillic code pages:
/// ASCII, Latin-1 and the basic Cyrillic capitals. Each keeps its byte
/// length, so an offset into the folded text is one into the text.
pub fn foldCase(bytes: []u8) void {
    var index: usize = 0;
    while (index < bytes.len) {
        const byte = bytes[index];
        if (byte < 0x80) {
            bytes[index] = std.ascii.toLower(byte);
            index += 1;
            continue;
        }
        if (byte & 0xE0 == 0xC0 and index + 1 < bytes.len and bytes[index + 1] & 0xC0 == 0x80) {
            const point: u21 = (@as(u21, byte & 0x1F) << 6) | (bytes[index + 1] & 0x3F);
            const lower: u21 = switch (point) {
                0xC0...0xD6, 0xD8...0xDE => point + 0x20,
                0x400...0x40F => point + 0x50,
                0x410...0x42F => point + 0x20,
                else => point,
            };
            bytes[index] = @intCast(0xC0 | (lower >> 6));
            bytes[index + 1] = @intCast(0x80 | (lower & 0x3F));
            index += 2;
            continue;
        }
        index += 1;
    }
}

const testing = std.testing;
const utf16 = database_mod.utf16;

test "case folding keeps byte offsets for Latin-1 and Cyrillic" {
    var text = "ÄÖÜ Straße ПРИВЕТ Ёж".*;
    foldCase(&text);
    try testing.expectEqualStrings("äöü straße привет ёж", &text);
}

test "find walks original, description and translation, resumes, wraps and keeps to whole words" {
    const io = testing.io;
    const root = try database_mod.freshScratch(io, "search");
    defer testing.allocator.free(root);
    try database_mod.writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>game</Path><Version>1</Version></item></Elements></base>\r\n");
    try database_mod.writeFixture(io, root, "game_data_base/a.elk", utf16("The Tank tank"));
    try database_mod.writeFixture(io, root, "game_data_base/b.elk", utf16("concatenate the cat"));
    try database_mod.writeFixture(io, root, "game_data_base/b.dsc", utf16("a cat note"));
    try database_mod.writeFixture(io, root, "game_data_base/c.elk", utf16("nothing"));
    try database_mod.writeFixture(io, root, "game_data_base/c.txt", utf16("Panzer TANK"));
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    var tree = try Tree.build(testing.allocator, &database);
    defer tree.deinit();
    const a: u32 = 0;
    const b: u32 = 1;
    const c: u32 = 2;

    // Case-insensitive: both of a's, then c's translation, then round to a.
    const tank: Options = .{ .text = "TANK" };
    var hit = (try findNext(testing.allocator, &database, &tree, tank, null, null)).?;
    try testing.expectEqual(Hit{ .text = a, .field = .original, .offset = 4, .len = 4 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, tank, hit.next(), null)).?;
    try testing.expectEqual(Hit{ .text = a, .field = .original, .offset = 9, .len = 4 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, tank, hit.next(), null)).?;
    try testing.expectEqual(Hit{ .text = c, .field = .translation, .offset = 7, .len = 4 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, tank, hit.next(), null)).?;
    try testing.expectEqual(Hit{ .text = a, .field = .original, .offset = 4, .len = 4 }, hit);

    // Match case: only c's capitals.
    const exact: Options = .{ .text = "TANK", .match_case = true };
    hit = (try findNext(testing.allocator, &database, &tree, exact, .{ .text = a }, null)).?;
    try testing.expectEqual(Hit{ .text = c, .field = .translation, .offset = 7, .len = 4 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, exact, hit.next(), null)).?;
    try testing.expectEqual(c, hit.text);

    // Whole word skips "concatenate" and still finds "cat" later in the field;
    // the description comes before the translation.
    const cat: Options = .{ .text = "cat", .whole_word = true };
    hit = (try findNext(testing.allocator, &database, &tree, cat, .{ .text = b }, null)).?;
    try testing.expectEqual(Hit{ .text = b, .field = .original, .offset = 16, .len = 3 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, cat, hit.next(), null)).?;
    try testing.expectEqual(Hit{ .text = b, .field = .description, .offset = 2, .len = 3 }, hit);

    // Up: from c to a, the hits within a text still in reading order.
    const up: Options = .{ .text = "tank", .down = false };
    hit = (try findNext(testing.allocator, &database, &tree, up, .{ .text = b }, null)).?;
    try testing.expectEqual(Hit{ .text = a, .field = .original, .offset = 4, .len = 4 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, up, .{ .text = a, .field = .translation }, null)).?;
    try testing.expectEqual(c, hit.text);

    try testing.expectEqual(@as(?Hit, null), try findNext(testing.allocator, &database, &tree, .{ .text = "zebra" }, null, null));
    try testing.expectEqual(@as(?Hit, null), try findNext(testing.allocator, &database, &tree, .{ .text = "" }, null, null));
}

test "a find over the whole tracked database" {
    const io = testing.io;
    var database = try Database.open(testing.allocator, io, "Data/ELK/elk.xml");
    defer database.deinit();
    var tree = try Tree.build(testing.allocator, &database);
    defer tree.deinit();
    // A miss reads every text's three fields once round.
    try testing.expectEqual(@as(?Hit, null), try findNext(testing.allocator, &database, &tree, .{ .text = "no-such-text-anywhere" }, null, null));
    // Kuban: twice in the desc's translation, then the name's original,
    // then round again.
    const kuban: Options = .{ .text = "kuban" };
    const desc = tree.nodes[tree.findText(0, "medals/german/kuban/desc").?].text.?;
    const name = tree.nodes[tree.findText(0, "medals/german/kuban/name").?].text.?;
    var hit = (try findNext(testing.allocator, &database, &tree, kuban, null, null)).?;
    try testing.expectEqual(Hit{ .text = desc, .field = .translation, .offset = 4, .len = 5 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, kuban, hit.next(), null)).?;
    try testing.expectEqual(Hit{ .text = desc, .field = .translation, .offset = 68, .len = 5 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, kuban, hit.next(), null)).?;
    try testing.expectEqual(Hit{ .text = name, .field = .original, .offset = 0, .len = 5 }, hit);
    hit = (try findNext(testing.allocator, &database, &tree, kuban, hit.next(), null)).?;
    try testing.expectEqual(desc, hit.text);
    try testing.expectEqual(@as(usize, 4), hit.offset);
    // The Russian original, its capital found in lower case; not with Match case.
    hit = (try findNext(testing.allocator, &database, &tree, .{ .text = "кубанском" }, null, null)).?;
    try testing.expectEqual(Hit{ .text = desc, .field = .original, .offset = 80, .len = "кубанском".len }, hit);
    try testing.expectEqual(@as(?Hit, null), try findNext(testing.allocator, &database, &tree, .{ .text = "кубанском", .match_case = true }, null, null));
}
