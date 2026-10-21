//! The game's line breaking, ported for the fit rule of l10n-check (M003 D097): `wrapLines` is
//! WrapTextLinesWith of Sources/src/GFXGPU/TextWrap.h and `canBreakTextBetween` is
//! Sources/src/GFX/TextBreak.h, so a text measured here wraps where the game wraps it. A line
//! breaks at the last space or, for text without spaces, at the last break opportunity; text with
//! neither breaks at the overflowing character. CR, LF and CRLF force a line. The width is summed
//! in the order FontGpu::TextWidth adds it: A + kern + B + C per character.
//!
//! A font here is anything with `advance(previous, current) f32` and `runWidth([]const u16) f32`
//! (l10n_font.Font, or the fixed-width font of the tests, as gfx_text_wrap_test.cpp does).
const std = @import("std");

/// A line as the half-open range of the text it holds.
pub const Line = struct { begin: usize, end: usize };

fn isHan(c: u16) bool {
    return (c >= 0x3400 and c <= 0x4DBF) or (c >= 0x4E00 and c <= 0x9FFF) or (c >= 0xF900 and c <= 0xFAFF);
}
fn isKana(c: u16) bool {
    return c >= 0x3040 and c <= 0x30FF;
}
fn isCjkPunct(c: u16) bool {
    return (c >= 0x3000 and c <= 0x303F) or (c >= 0xFF00 and c <= 0xFFEF);
}
fn isCjk(c: u16) bool {
    return isHan(c) or isKana(c) or isCjkPunct(c);
}

/// Characters that must not start a line: closing brackets, stops, the prolonged sound mark,
/// iteration marks, the two-dot and three-dot leaders, the katakana double hyphen, the
/// fullwidth wave dash and the small kana.
pub fn isNoStart(c: u16) bool {
    switch (c) {
        0x3001, 0x3002, 0x3005, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011, 0x3015 => return true,
        0x301C, 0x30FB, 0x30FC, 0x30FD, 0x30FE, 0x309B, 0x309C, 0x309D, 0x309E => return true,
        0x2025, 0x2026, 0x30A0, 0xFF5E => return true,
        0xFF01, 0xFF09, 0xFF0C, 0xFF0E, 0xFF1A, 0xFF1B, 0xFF1F, 0xFF3D, 0xFF5D => return true,
        else => {},
    }
    if (c >= 0x3041 and c <= 0x3049) return c & 1 != 0;
    if (c == 0x3063 or c == 0x3083 or c == 0x3085 or c == 0x3087 or c == 0x308E) return true;
    if (c >= 0x30A1 and c <= 0x30A9) return c & 1 != 0;
    return c == 0x30C3 or c == 0x30E3 or c == 0x30E5 or c == 0x30E7 or c == 0x30EE or c == 0x30F5 or c == 0x30F6;
}

/// Characters that must not end a line: opening brackets.
pub fn isNoEnd(c: u16) bool {
    return switch (c) {
        0x3008, 0x300A, 0x300C, 0x300E, 0x3010, 0x3014, 0xFF08, 0xFF3B, 0xFF5B => true,
        else => false,
    };
}

/// CanBreakTextBetween: whether the line may break between `previous` and `next` with no space
/// there. Private-use code points (the Hindi cluster glyphs) are never break opportunities.
pub fn canBreakTextBetween(previous: u16, next: u16) bool {
    if ((previous >= 0xE000 and previous <= 0xF8FF) or (next >= 0xE000 and next <= 0xF8FF)) return false;
    if (!isCjk(previous) and !isCjk(next)) return false;
    return !isNoStart(next) and !isNoEnd(previous);
}

/// The lines of `text` (UTF-16, a zero ends it) in a box `wrap_width` wide at `scale`. A
/// `wrap_width` of zero or less measures nothing, so only line breaks of the text itself apply.
pub fn wrapLines(arena: std.mem.Allocator, font: anytype, text_in: []const u16, scale: f32, wrap_width: f32, lines: *std.ArrayList(Line)) !void {
    const none = std.math.maxInt(usize);
    lines.clearRetainingCapacity();
    const length = std.mem.indexOfScalar(u16, text_in, 0) orelse text_in.len;
    const text = text_in[0..length];
    const measure = wrap_width > 0.0;
    var line_begin: usize = 0;
    var last_space: usize = none;
    // Where the line could break without a space: the index of the character that would start the
    // next line.
    var last_opportunity: usize = none;
    var run_width: f32 = 0.0;
    var previous: u16 = 0;
    var index: usize = 0;
    while (index <= length) : (index += 1) {
        if (index == length or text[index] == '\n' or text[index] == '\r') {
            try lines.append(arena, .{ .begin = line_begin, .end = index });
            if (index + 1 < length and text[index] == '\r' and text[index + 1] == '\n') index += 1;
            line_begin = index + 1;
            last_space = none;
            last_opportunity = none;
            run_width = 0.0;
            previous = 0;
            continue;
        }
        if (text[index] == ' ') last_space = index;
        if (!measure) continue;
        const current = text[index];
        if (index > line_begin and current != ' ' and text[index - 1] != ' ' and canBreakTextBetween(text[index - 1], current))
            last_opportunity = index;
        run_width += font.advance(previous, current);
        previous = current;
        if (index == line_begin) continue;
        if (run_width * scale <= wrap_width) continue;
        const have_space = last_space != none and last_space > line_begin;
        const have_opportunity = last_opportunity != none and last_opportunity > line_begin;
        const break_on_space = have_space and (!have_opportunity or last_space > last_opportunity);
        const break_at = if (break_on_space) last_space else if (have_opportunity) last_opportunity else index;
        try lines.append(arena, .{ .begin = line_begin, .end = break_at });
        line_begin = if (break_on_space) break_at + 1 else break_at;
        last_space = none;
        last_opportunity = none;
        // The word that moved down is measured again from the start of the new line, since the
        // kerning against 0 differs from the kerning it had mid-line.
        const carried: usize = if (line_begin <= index) index - line_begin + 1 else 0;
        run_width = font.runWidth(text[line_begin..][0..carried]);
        previous = if (carried != 0) text[index] else 0;
    }
}

/// A run between spaces and line breaks, with its width.
pub const Word = struct { begin: usize = 0, end: usize = 0, width: f32 = 0 };

/// The widest run of `text` between spaces, line breaks and break opportunities (CJK text has no
/// spaces, and the game breaks it between characters), which the game splits where it overflows
/// when the run is wider than the box.
pub fn widestWord(font: anytype, text_in: []const u16, scale: f32) Word {
    const length = std.mem.indexOfScalar(u16, text_in, 0) orelse text_in.len;
    const text = text_in[0..length];
    var widest: Word = .{};
    var begin: usize = 0;
    var at: usize = 0;
    while (at <= text.len) : (at += 1) {
        const separator = at != text.len and (text[at] == ' ' or text[at] == '\n' or text[at] == '\r');
        const opportunity = at != text.len and at > begin and canBreakTextBetween(text[at - 1], text[at]);
        if (at != text.len and !separator and !opportunity) continue;
        const width = font.runWidth(text[begin..at]) * scale;
        if (width > widest.width) widest = .{ .begin = begin, .end = at, .width = width };
        begin = if (separator) at + 1 else at;
    }
    return widest;
}

/// Every glyph advances 10, so a box of width 10 * n holds n characters, as in
/// tools/zig/gfx_text_wrap_test.cpp.
const FixedFont = struct {
    pub fn advance(_: FixedFont, _: u16, _: u16) f32 {
        return 10.0;
    }
    pub fn runWidth(_: FixedFont, text: []const u16) f32 {
        return 10.0 * @as(f32, @floatFromInt(text.len));
    }
};

/// The wrapped lines of `text` in a box `chars` glyphs wide, joined with `|`.
fn wrapped(allocator: std.mem.Allocator, text: []const u8, chars: usize) ![]u8 {
    const units = try std.unicode.utf8ToUtf16LeAlloc(allocator, text);
    defer allocator.free(units);
    var arena_state = std.heap.ArenaAllocator.init(allocator);
    defer arena_state.deinit();
    var lines: std.ArrayList(Line) = .empty;
    try wrapLines(arena_state.allocator(), FixedFont{}, units, 1.0, 10.0 * @as(f32, @floatFromInt(chars)), &lines);
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(allocator);
    for (lines.items, 0..) |line, at| {
        if (at != 0) try out.append(allocator, '|');
        const piece = try std.unicode.utf16LeToUtf8Alloc(allocator, units[line.begin..line.end]);
        defer allocator.free(piece);
        try out.appendSlice(allocator, piece);
    }
    return out.toOwnedSlice(allocator);
}

fn expectWrap(text: []const u8, chars: usize, want: []const u8) !void {
    const got = try wrapped(std.testing.allocator, text, chars);
    defer std.testing.allocator.free(got);
    try std.testing.expectEqualStrings(want, got);
}

test "Han and kana break between characters" {
    try expectWrap("一二三四五六", 4, "一二三四|五六");
    try expectWrap("あいうえおか", 3, "あいう|えおか");
}

test "kinsoku: closing marks never start a line, opening marks never end one" {
    try expectWrap("一二三四。五", 4, "一二三|四。五");
    try expectWrap("一二三四、五", 4, "一二三|四、五");
    try expectWrap("一二三四，五", 4, "一二三|四，五");
    try expectWrap("一二三四！五", 4, "一二三|四！五");
    try expectWrap("一二三四？五", 4, "一二三|四？五");
    try expectWrap("一二三四）五", 4, "一二三|四）五");
    try expectWrap("一二三四」五", 4, "一二三|四」五");
    try expectWrap("一二三四』五", 4, "一二三|四』五");
    try expectWrap("あいうーあ", 3, "あい|うーあ");
    try expectWrap("あいうゃあ", 3, "あい|うゃあ");
    try expectWrap("アイウィア", 3, "アイ|ウィア");
    try expectWrap("一二三（四五", 4, "一二三|（四五");
    try expectWrap("一二三「四五", 4, "一二三|「四五");
    try expectWrap("一二三『四五", 4, "一二三|『四五");
    try expectWrap("一二三四\u{2025}五", 4, "一二三|四\u{2025}五");
    try expectWrap("一二三四\u{2026}五", 4, "一二三|四\u{2026}五");
    try expectWrap("一二三四\u{30A0}五", 4, "一二三|四\u{30A0}五");
    try expectWrap("一二三四\u{FF5E}五", 4, "一二三|四\u{FF5E}五");
}

test "mixed text breaks at the space and between Han" {
    try expectWrap("ab \u{4E00}\u{4E8C}\u{4E09}\u{56DB}", 5, "ab \u{4E00}\u{4E8C}|\u{4E09}\u{56DB}");
    try expectWrap("ab \u{4E00}\u{4E8C}\u{4E09}\u{56DB}", 3, "ab|\u{4E00}\u{4E8C}\u{4E09}|\u{56DB}");
    try expectWrap("一二三 ab 四五", 6, "一二三 ab|四五");
    try expectWrap("ab一二三", 3, "ab一|二三");
}

test "Latin and Cyrillic break at the last space or at the overflowing character" {
    try expectWrap("aaa bbb ccc", 7, "aaa bbb|ccc");
    try expectWrap("aaaaaaaaaa", 4, "aaaa|aaaa|aa");
    try expectWrap("привет мир", 7, "привет|мир");
    try expectWrap("приветмир", 4, "прив|етми|р");
    try expectWrap("one\ntwo", 9, "one|two");
    try expectWrap("one\r\ntwo", 9, "one|two");
    try expectWrap("", 4, "");
}

test "private-use code points are never break opportunities" {
    var c: u16 = 0xE000;
    while (c < 0xF8FF) : (c += 0x7F) {
        try std.testing.expect(!canBreakTextBetween(c, c + 1));
        try std.testing.expect(!canBreakTextBetween(0x4E00, c));
        try std.testing.expect(!canBreakTextBetween(c, 0x4E00));
    }
    // A cluster word of six glyphs in a box of five breaks where it overflows.
    try expectWrap("\u{E000}\u{E001}\u{E002}\u{E003}\u{E004}\u{E005}", 5, "\u{E000}\u{E001}\u{E002}\u{E003}\u{E004}|\u{E005}");
}

test "a line past the box width counts the same lines as the game" {
    // Seven glyphs, a space, seven glyphs in a box of eight: one line each, the space ending the first.
    try expectWrap("aaaaaaa bbbbbbb", 8, "aaaaaaa|bbbbbbb");
    // A word wider than the box is split where it overflows, after the line before it.
    try expectWrap("ab cccccc", 4, "ab|cccc|cc");
}

test "widestWord measures the longest run between spaces and line breaks" {
    const units = try std.unicode.utf8ToUtf16LeAlloc(std.testing.allocator, "ab cccccc d\nefgh");
    defer std.testing.allocator.free(units);
    const word = widestWord(FixedFont{}, units, 1.0);
    try std.testing.expectEqual(@as(f32, 60), word.width);
    try std.testing.expectEqual(@as(usize, 3), word.begin);
    try std.testing.expectEqual(@as(usize, 9), word.end);
    try std.testing.expectEqual(@as(f32, 120), widestWord(FixedFont{}, units, 2.0).width);
    try std.testing.expectEqual(@as(f32, 0), widestWord(FixedFont{}, &.{}, 1.0).width);
}

test "widestWord ends a run at a break opportunity, not inside kinsoku" {
    const units = try std.unicode.utf8ToUtf16LeAlloc(std.testing.allocator, "一二三四。五");
    defer std.testing.allocator.free(units);
    // Every Han pair is an opportunity, so the runs are 一 二 三 四。 五: the full stop stays with 四.
    const word = widestWord(FixedFont{}, units, 1.0);
    try std.testing.expectEqual(@as(f32, 20), word.width);
    try std.testing.expectEqual(@as(usize, 3), word.begin);
}
