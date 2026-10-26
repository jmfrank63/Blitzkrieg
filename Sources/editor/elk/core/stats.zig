//! ELK Statistic (`CELK::CreateStatistic` in
//! Sources/src/ELK/ELK_StaticMethods.cpp): per element and per state, how
//! many texts there are and their words, word symbols and symbols, once for
//! the originals and once for the translations, counted as
//! `CSpellChecker::GetTextCounts` counted them. A text is counted under its
//! own state in both halves, so the translation half of "Not translated"
//! counts whatever stale text such a file still holds.
//!
//! The MFC ELK counted the code page's bytes (an MBCS build); this counts
//! UTF-16 code units, which is the same number for every single-byte code
//! page the shipped translations used.
const std = @import("std");
const database_mod = @import("database.zig");
const tree_mod = @import("tree.zig");
const project_file = @import("project_file.zig");
const text_codec = @import("text_codec.zig");
const spell = @import("spell.zig");

const Database = database_mod.Database;
const Tree = tree_mod.Tree;
pub const Counts = project_file.Counts;
pub const Statistic = project_file.Statistic;
pub const ElementStatistic = project_file.ElementStatistic;

/// `CSpellChecker`'s tables, kept with its word rules in spell.zig.
pub const spelling_delimiters = spell.spelling_delimiters;
const word_delimiters = spell.word_delimiters;

pub const TextCounts = struct { words: i32 = 0, word_symbols: i32 = 0, symbols: i32 = 0 };

/// `GetTextCounts`, with its quirk kept so the numbers match the snapshots
/// the MFC ELK stored: each word starts after a run of spelling delimiters
/// (punctuation included) but ends only at a blank, so "end." is a word of
/// four symbols, and a text ending in delimiters counts one more, empty,
/// word. Symbols are the code units that are not CR or LF.
pub fn textCounts(units: []const u16) TextCounts {
    var counts: TextCounts = .{};
    var rest = units;
    while (rest.len != 0) {
        while (rest.len != 0 and isOneOf(rest[0], spelling_delimiters)) rest = rest[1..];
        var end: usize = 0;
        while (end < rest.len and !isOneOf(rest[end], word_delimiters)) end += 1;
        counts.words += 1;
        counts.word_symbols += @intCast(end);
        rest = rest[end..];
    }
    for (units) |unit| {
        if (unit != '\r' and unit != '\n') counts.symbols += 1;
    }
    return counts;
}

fn isOneOf(unit: u16, set: []const u8) bool {
    return unit < 0x80 and std.mem.indexOfScalar(u8, set, @intCast(unit)) != null;
}

/// Counts every text of the tree under the state the tree shows for it
/// (the state files' as opened, with the edits since). The slices are
/// allocated in `allocator`, one entry per element.
pub fn compute(allocator: std.mem.Allocator, gpa: std.mem.Allocator, database: *Database, tree: *const Tree) !Statistic {
    const elements = database.bases.len;
    const original = try allocator.alloc(ElementStatistic, elements);
    const translation = try allocator.alloc(ElementStatistic, elements);
    @memset(original, @splat(.{}));
    @memset(translation, @splat(.{}));
    for (tree.texts, tree.node_of_text) |text, node| {
        const state = @intFromEnum(tree.nodes[node].state);
        add(&original[text.element][state], try fileCounts(gpa, database, text, .original));
        add(&translation[text.element][state], try fileCounts(gpa, database, text, .translation));
    }
    return .{ .valid = true, .original = original, .translation = translation };
}

/// The counts of one text file, read as the MFC ELK read it for the
/// statistic: every CR removed, the trailing CR, LF and NUL run dropped.
/// A missing file counts as an empty text.
fn fileCounts(gpa: std.mem.Allocator, database: *Database, text: tree_mod.Text, extension: database_mod.Extension) !TextCounts {
    const bytes = try database.readBytes(gpa, text.element, text.key, extension) orelse return .{};
    defer gpa.free(bytes);
    const units = try text_codec.decode(gpa, bytes, true);
    defer gpa.free(units);
    return textCounts(units);
}

fn add(counts: *Counts, text: TextCounts) void {
    counts.texts += 1;
    counts.words += text.words;
    counts.word_symbols += text.word_symbols;
    counts.symbols += text.symbols;
}

/// The sum over the elements of one state's counts, the rows the MFC
/// dialog showed first under each half.
pub fn stateTotal(half: []const ElementStatistic, state: usize) Counts {
    var total: Counts = .{};
    for (half) |element| total = sum(total, element[state]);
    return total;
}

/// The sum of an element's four states, its own row in the dialog.
pub fn elementTotal(element: ElementStatistic) Counts {
    var total: Counts = .{};
    for (element) |counts| total = sum(total, counts);
    return total;
}

pub fn sum(a: Counts, b: Counts) Counts {
    return .{ .texts = a.texts + b.texts, .words = a.words + b.words, .word_symbols = a.word_symbols + b.word_symbols, .symbols = a.symbols + b.symbols };
}

/// `SizeToStringWithSpaces`: thousands set apart by spaces ("1 234 567").
pub fn formatCount(buffer: []u8, value: i32) []const u8 {
    var digits_buffer: [16]u8 = undefined;
    const digits = std.fmt.bufPrint(&digits_buffer, "{d}", .{value}) catch return "";
    var len: usize = 0;
    for (digits, 0..) |digit, index| {
        const left = digits.len - index;
        if (index != 0 and left % 3 == 0 and digits[index - 1] != '-') {
            if (len == buffer.len) return buffer[0..len];
            buffer[len] = ' ';
            len += 1;
        }
        if (len == buffer.len) return buffer[0..len];
        buffer[len] = digit;
        len += 1;
    }
    return buffer[0..len];
}

const testing = std.testing;

fn ascii(comptime text: []const u8) []const u16 {
    return comptime blk: {
        var out: [text.len]u16 = undefined;
        for (text, &out) |byte, *unit| unit.* = byte;
        const final = out;
        break :blk &final;
    };
}

test "words start after punctuation but end only at a blank, as GetTextCounts counted" {
    try testing.expectEqual(TextCounts{}, textCounts(ascii("")));
    try testing.expectEqual(TextCounts{ .words = 2, .word_symbols = 11, .symbols = 12 }, textCounts(ascii("Hello world.")));
    // "(a)" is the word "a)": the leading bracket is stripped, the closing one is not.
    try testing.expectEqual(TextCounts{ .words = 2, .word_symbols = 4, .symbols = 6 }, textCounts(ascii("(a) b.")));
    // Trailing delimiters make one more, empty, word; CR and LF are no symbols.
    try testing.expectEqual(TextCounts{ .words = 2, .word_symbols = 1, .symbols = 5 }, textCounts(ascii("x\r\n -- ")));
    try testing.expectEqual(TextCounts{ .words = 1, .word_symbols = 0, .symbols = 3 }, textCounts(ascii("...")));
}

test "the thousands are set apart by spaces" {
    var buffer: [32]u8 = undefined;
    try testing.expectEqualStrings("0", formatCount(&buffer, 0));
    try testing.expectEqualStrings("999", formatCount(&buffer, 999));
    try testing.expectEqualStrings("1 000", formatCount(&buffer, 1000));
    try testing.expectEqualStrings("188 738", formatCount(&buffer, 188738));
    try testing.expectEqualStrings("1 234 567", formatCount(&buffer, 1234567));
}

test "the counts equal the snapshot the MFC ELK stored in Data/AmericanELK's elk.xml" {
    // The snapshot was taken before this project edited six of the
    // database's files (the credits lost FMOD and Bink, four option names
    // were renamed); the fixture holds those six as they were, laid over a
    // scratch copy, so the database is the one the MFC ELK counted.
    const io = testing.io;
    const root = try database_mod.freshScratch(io, "stats");
    defer testing.allocator.free(root);
    const elk_xml = try std.Io.Dir.cwd().readFileAlloc(io, "Data/AmericanELK/elk.xml", testing.allocator, .limited(1 << 20));
    defer testing.allocator.free(elk_xml);
    try database_mod.writeFixture(io, root, "elk.xml", elk_xml);
    const base = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base" });
    defer testing.allocator.free(base);
    try database_mod.copyTree(io, "Data/AmericanELK/game_data_base", base);
    try database_mod.copyTree(io, "tools/zig/fixtures/elk/american_snapshot/game_data_base", base);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);

    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    var tree = try Tree.build(testing.allocator, &database);
    defer tree.deinit();
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const statistic = try compute(arena.allocator(), testing.allocator, &database, &tree);
    const stored = database.project.statistic;
    try testing.expect(stored.valid and statistic.valid);
    try testing.expectEqual(@as(usize, 1), statistic.original.len);
    try testing.expectEqualDeep(stored.original, statistic.original);
    try testing.expectEqualDeep(stored.translation, statistic.translation);
    try testing.expectEqual(Counts{ .texts = 3227, .words = 57506, .word_symbols = 295282, .symbols = 366227 }, elementTotal(statistic.original[0]));
    try testing.expectEqual(Counts{ .texts = 113, .words = 10890, .word_symbols = 54375, .symbols = 67193 }, stateTotal(statistic.translation, 2));
}

test "the tracked Data/ELK counts" {
    // No snapshot to compare with here (the stored one is of an older
    // database of 2,968 texts); these are the numbers the app shows.
    const io = testing.io;
    var database = try Database.open(testing.allocator, io, "Data/ELK/elk.xml");
    defer database.deinit();
    var tree = try Tree.build(testing.allocator, &database);
    defer tree.deinit();
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const statistic = try compute(arena.allocator(), testing.allocator, &database, &tree);
    try testing.expectEqual(Counts{ .texts = 1130, .words = 20350, .word_symbols = 106667, .symbols = 129878 }, statistic.original[0][0]);
    try testing.expectEqual(Counts{ .texts = 4, .words = 55, .word_symbols = 263, .symbols = 316 }, statistic.translation[0][2]);
    try testing.expectEqual(@as(i32, 3081), elementTotal(statistic.original[0]).texts);
}
