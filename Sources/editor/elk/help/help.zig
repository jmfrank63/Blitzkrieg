//! ELK's help (D-02, D-18, D079): `elk.md` and its nine translations
//! `elk_<code>.md`, embedded in the binary so ELK needs no help file beside it,
//! and their blocks for the Help window to draw. The
//! text keeps to the Markdown that window draws: `#` and `##` headings,
//! `- ` bullets and paragraphs, each block on one line, no inline marks.
const std = @import("std");

pub const text = @embedFile("elk.md");

const Translation = struct { code: []const u8, text: []const u8 };

/// The nine translations, in the order of the kit's language list. English is `text`.
const translations = [_]Translation{
    .{ .code = "ru", .text = @embedFile("elk_ru.md") },
    .{ .code = "zh-Hans", .text = @embedFile("elk_zh-Hans.md") },
    .{ .code = "hi", .text = @embedFile("elk_hi.md") },
    .{ .code = "es", .text = @embedFile("elk_es.md") },
    .{ .code = "fr", .text = @embedFile("elk_fr.md") },
    .{ .code = "de", .text = @embedFile("elk_de.md") },
    .{ .code = "pt-BR", .text = @embedFile("elk_pt-BR.md") },
    .{ .code = "ja", .text = @embedFile("elk_ja.md") },
    .{ .code = "id", .text = @embedFile("elk_id.md") },
};

/// The help in the language `code` (`en`, `ru`, `zh-Hans`, ...); English for a code without one.
pub fn forLanguage(code: []const u8) []const u8 {
    for (translations) |translation| {
        if (std.mem.eql(u8, translation.code, code)) return translation.text;
    }
    return text;
}

pub const Block = struct {
    kind: Kind,
    /// The line without its marker.
    text: []const u8,

    pub const Kind = enum { title, heading, bullet, paragraph };
};

/// The blocks of `source`, one per non-empty line, CRLF or LF.
pub const Blocks = struct {
    lines: std.mem.SplitIterator(u8, .scalar),

    pub fn init(source: []const u8) Blocks {
        return .{ .lines = std.mem.splitScalar(u8, source, '\n') };
    }

    pub fn next(self: *Blocks) ?Block {
        while (self.lines.next()) |raw| {
            const line = std.mem.trimEnd(u8, raw, "\r ");
            if (line.len == 0) continue;
            return if (std.mem.startsWith(u8, line, "## "))
                .{ .kind = .heading, .text = line[3..] }
            else if (std.mem.startsWith(u8, line, "# "))
                .{ .kind = .title, .text = line[2..] }
            else if (std.mem.startsWith(u8, line, "- "))
                .{ .kind = .bullet, .text = line[2..] }
            else
                .{ .kind = .paragraph, .text = line };
        }
        return null;
    }
};

const testing = std.testing;

test "the blocks of a CRLF text" {
    var blocks = Blocks.init("# ELK\r\n\r\nSome text.\r\n## Keys\r\n- F1 help\n");
    const wanted = [_]Block{
        .{ .kind = .title, .text = "ELK" },
        .{ .kind = .paragraph, .text = "Some text." },
        .{ .kind = .heading, .text = "Keys" },
        .{ .kind = .bullet, .text = "F1 help" },
    };
    for (wanted) |block| {
        const got = blocks.next().?;
        try testing.expectEqual(block.kind, got.kind);
        try testing.expectEqualStrings(block.text, got.text);
    }
    try testing.expectEqual(@as(?Block, null), blocks.next());
}

test "the embedded help covers what ELK does and keeps to the Markdown the window draws" {
    var headings: std.ArrayList([]const u8) = .empty;
    defer headings.deinit(testing.allocator);
    var titles: usize = 0;
    var blocks = Blocks.init(text);
    while (blocks.next()) |block| {
        switch (block.kind) {
            .title => titles += 1,
            .heading => try headings.append(testing.allocator, block.text),
            .bullet, .paragraph => {},
        }
        // A mark the window would show as typed.
        for ([_][]const u8{ "**", "`", "](", "#", "|" }) |mark| {
            if (std.mem.indexOf(u8, block.text, mark) != null) {
                std.debug.print("help: \"{s}\" in \"{s}\"\n", .{ mark, block.text });
                return error.UndrawnMarkdown;
            }
        }
        const first = block.text[0];
        try testing.expect(first != '*' and first != '>' and !std.ascii.isDigit(first) and first != ' ');
    }
    try testing.expectEqual(@as(usize, 1), titles);
    for ([_][]const u8{
        "Starting ELK",  "Opening and saving", "The text tree", "States",       "Filters and browsing",
        "Find",          "Statistics",         "Spell check",   "Spreadsheets", "Putting the translation into the game",
        "Keys",
    }) |wanted| {
        for (headings.items) |heading| {
            if (std.mem.eql(u8, heading, wanted)) break;
        } else {
            std.debug.print("help: no heading \"{s}\"\n", .{wanted});
            return error.MissingTopic;
        }
    }
    try testing.expect(std.mem.indexOf(u8, text, "--short") != null);
    try testing.expect(std.mem.indexOf(u8, text, "Language") != null);
}

/// The kinds of the blocks of `source`, in order.
fn structure(allocator: std.mem.Allocator, source: []const u8) !std.ArrayList(Block.Kind) {
    var kinds: std.ArrayList(Block.Kind) = .empty;
    errdefer kinds.deinit(allocator);
    var blocks = Blocks.init(source);
    while (blocks.next()) |block| try kinds.append(allocator, block.kind);
    return kinds;
}

test "each translation has the block structure of the English help and keeps to the Markdown the window draws" {
    var english = try structure(testing.allocator, text);
    defer english.deinit(testing.allocator);
    for (translations) |translation| {
        var kinds = try structure(testing.allocator, translation.text);
        defer kinds.deinit(testing.allocator);
        if (!std.mem.eql(Block.Kind, english.items, kinds.items)) {
            std.debug.print("help: {s} has {d} blocks, English {d}, or they differ in kind\n", .{ translation.code, kinds.items.len, english.items.len });
            return error.StructureDiffers;
        }
        try testing.expect(std.mem.indexOf(u8, translation.text, "\r\n") != null);
        try testing.expect(std.mem.indexOf(u8, translation.text, "--short") != null);
        try testing.expect(std.mem.indexOf(u8, translation.text, "mods/") != null);
        var blocks = Blocks.init(translation.text);
        while (blocks.next()) |block| {
            for ([_][]const u8{ "**", "`", "](", "#", "|" }) |mark| {
                if (std.mem.indexOf(u8, block.text, mark) != null) {
                    std.debug.print("help: {s}: \"{s}\" in \"{s}\"\n", .{ translation.code, mark, block.text });
                    return error.UndrawnMarkdown;
                }
            }
            const first = block.text[0];
            try testing.expect(first != '*' and first != '>' and !std.ascii.isDigit(first) and first != ' ');
        }
    }
}

test "forLanguage picks the translation and falls back to English" {
    try testing.expectEqualStrings(text, forLanguage("en"));
    try testing.expectEqualStrings(text, forLanguage("xx"));
    for (translations) |translation| try testing.expectEqualStrings(translation.text, forLanguage(translation.code));
    try testing.expect(forLanguage("de").ptr != text.ptr);
}
