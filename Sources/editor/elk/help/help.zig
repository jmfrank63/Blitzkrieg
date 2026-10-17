//! ELK's help (D-02, D-18): `elk.md`, embedded in the binary so ELK needs
//! no help file beside it, and its blocks for the Help window to draw. The
//! text keeps to the Markdown that window draws: `#` and `##` headings,
//! `- ` bullets and paragraphs, each block on one line, no inline marks.
const std = @import("std");

pub const text = @embedFile("elk.md");

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
