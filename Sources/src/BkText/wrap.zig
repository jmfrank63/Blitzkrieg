//! D104 line breaking: WrapTextLinesWith of Sources/src/GFXGPU/TextWrap.h over shaped clusters.
//! A line breaks at the last space (dropped at the line end), else at the last CJK break
//! opportunity (textbreak.canBreakTextBetween), else at the overflowing cluster. CR, LF and CRLF
//! force a line. Widths are the cluster advances of the shaped paragraph, never a reshaped line,
//! so a break never lands inside a cluster and a cluster wider than the box stands alone.
const std = @import("std");
const shape = @import("shape.zig");
const measure = @import("measure.zig");
const textbreak = @import("textbreak.zig");

/// UTF-16 indices; `end` excludes the break space or newline.
pub const Line = struct { start: u32, end: u32 };

/// Wraps `text` given per-unit advances as measure.unitAdvances makes them: a cluster's advance
/// on its first unit, 0 on the rest. `cluster_len[i]` is the length of the cluster starting at
/// unit i (0 on units inside a cluster); a fake provider in a test passes all ones. A `width` of
/// zero or less measures nothing, so only the text's own line breaks apply.
pub fn wrapAdvances(
    allocator: std.mem.Allocator,
    text: []const u16,
    advances: []const f32,
    cluster_len: []const u32,
    width: f32,
    lines: *std.ArrayList(Line),
) !void {
    std.debug.assert(advances.len == text.len and cluster_len.len == text.len);
    const none = std.math.maxInt(usize);
    const length = text.len;
    lines.clearRetainingCapacity();
    const measuring = width > 0.0;
    var line_begin: usize = 0;
    var last_space: usize = none;
    // Index of the cluster that would start the next line if the line broke without a space.
    var last_opportunity: usize = none;
    var run_width: f32 = 0.0;
    var index: usize = 0;
    while (index <= length) {
        if (index == length or text[index] == '\n' or text[index] == '\r') {
            try lines.append(allocator, .{ .start = @intCast(line_begin), .end = @intCast(index) });
            if (index + 1 < length and text[index] == '\r' and text[index + 1] == '\n') index += 1;
            index += 1;
            line_begin = index;
            last_space = none;
            last_opportunity = none;
            run_width = 0.0;
            continue;
        }
        const step: usize = @max(1, cluster_len[index]);
        defer index += step;
        const current = text[index];
        if (current == ' ') last_space = index;
        if (!measuring) continue;
        if (index > line_begin and current != ' ' and text[index - 1] != ' ' and textbreak.canBreakTextBetween(text[index - 1], current))
            last_opportunity = index;
        run_width += advances[index];
        if (index == line_begin) continue;
        if (measure.pixelWidth(run_width) <= width) continue;
        const have_space = last_space != none and last_space > line_begin;
        const have_opportunity = last_opportunity != none and last_opportunity > line_begin;
        const break_on_space = have_space and (!have_opportunity or last_space > last_opportunity);
        const break_at = if (break_on_space) last_space else if (have_opportunity) last_opportunity else index;
        try lines.append(allocator, .{ .start = @intCast(line_begin), .end = @intCast(break_at) });
        line_begin = if (break_on_space) break_at + 1 else break_at;
        last_space = none;
        last_opportunity = none;
        // What moved down is measured again from the start of the new line. Breaking on a space
        // that is itself the overflowing cluster leaves nothing on the new line yet.
        run_width = 0.0;
        if (line_begin <= index) for (advances[line_begin .. index + step]) |a| {
            run_width += a;
        };
    }
}

/// Shapes `text` with `shaper` and wraps it into `width` pixels.
pub fn wrapText(
    allocator: std.mem.Allocator,
    shaper: *shape.Shaper,
    text: []const u16,
    width: f32,
    lines: *std.ArrayList(Line),
) !void {
    const run = try shaper.shape(text);
    defer run.deinit(allocator);
    const advances = try allocator.alloc(f32, text.len);
    defer allocator.free(advances);
    const cluster_len = try allocator.alloc(u32, text.len);
    defer allocator.free(cluster_len);
    measure.unitAdvances(run, advances);
    @memset(cluster_len, 0);
    for (run.clusters) |cl| cluster_len[cl.start] = cl.end - cl.start;
    // Units outside every cluster (hard breaks, nothing else) step one at a time.
    try wrapAdvances(allocator, text, advances, cluster_len, width, lines);
}

// ---- tests ----

const faces = @import("faces.zig");
const utf16 = @import("utf16.zig");
const l10n_wrap = @import("l10n_wrap");

/// Every unit advances 10 and is its own cluster, as in tools/zig/gfx_text_wrap_test.cpp.
fn fixed(allocator: std.mem.Allocator, text: []const u16, width: f32, lines: *std.ArrayList(Line)) !void {
    const adv = try allocator.alloc(f32, text.len);
    defer allocator.free(adv);
    const len = try allocator.alloc(u32, text.len);
    defer allocator.free(len);
    @memset(adv, 10.0);
    @memset(len, 1);
    try wrapAdvances(allocator, text, adv, len, width, lines);
}

fn expectLines(text: []const u16, width: f32, expected: []const [2]u32) !void {
    const gpa = std.testing.allocator;
    var lines: std.ArrayList(Line) = .empty;
    defer lines.deinit(gpa);
    try fixed(gpa, text, width, &lines);
    try std.testing.expectEqual(expected.len, lines.items.len);
    for (expected, lines.items) |e, l| {
        try std.testing.expectEqual(e[0], l.start);
        try std.testing.expectEqual(e[1], l.end);
    }
}

test "plain spaces: the space at the break is dropped" {
    // "aaa bbb ccc": 3 characters per 30 px; "aaa bbb" is 70 wide.
    try expectLines(&utf16.literal("aaa bbb ccc"), 70, &.{ .{ 0, 7 }, .{ 8, 11 } });
    try expectLines(&utf16.literal("aaa bbb ccc"), 40, &.{ .{ 0, 3 }, .{ 4, 7 }, .{ 8, 11 } });
    try expectLines(&utf16.literal("aaa bbb ccc"), 200, &.{.{ 0, 11 }});
}

test "a long word breaks at the overflowing cluster" {
    try expectLines(&utf16.literal("abcdefghij"), 40, &.{ .{ 0, 4 }, .{ 4, 8 }, .{ 8, 10 } });
    // A word that overflows after a space moves down whole, then splits.
    try expectLines(&utf16.literal("ab cdefghij"), 40, &.{ .{ 0, 2 }, .{ 3, 7 }, .{ 7, 11 } });
}

test "CRLF and LF force lines, an empty line stays" {
    try expectLines(&utf16.literal("ab\r\ncd\nef"), 100, &.{ .{ 0, 2 }, .{ 4, 6 }, .{ 7, 9 } });
    try expectLines(&utf16.literal("ab\n\ncd"), 100, &.{ .{ 0, 2 }, .{ 3, 3 }, .{ 4, 6 } });
    try expectLines(&utf16.literal(""), 100, &.{.{ 0, 0 }});
    try expectLines(&utf16.literal("ab\r"), 100, &.{ .{ 0, 2 }, .{ 3, 3 } });
}

test "no width measures nothing" {
    try expectLines(&utf16.literal("abcdef ghij"), 0, &.{.{ 0, 11 }});
}

test "Japanese: no line start on stops or small kana" {
    // Four characters per line. The 。 would start line two, so the break moves up one.
    const t1 = utf16.literal("あいうえ。おか");
    try expectLines(&t1, 40, &.{ .{ 0, 3 }, .{ 3, 7 } });
    // 、 after the fourth character likewise; small ゃ (0x3083) too.
    const t2 = utf16.literal("きしちにゃ");
    try expectLines(&t2, 40, &.{ .{ 0, 3 }, .{ 3, 5 } });
}

test "opening brackets never end a line" {
    // 「 would end the first line of four, so it goes down with its text.
    const t = utf16.literal("あいう「えお」");
    try expectLines(&t, 40, &.{ .{ 0, 3 }, .{ 3, 7 } });
}

test "Chinese breaks between any two characters" {
    const t = utf16.literal("中文字符串测试");
    try expectLines(&t, 30, &.{ .{ 0, 3 }, .{ 3, 6 }, .{ 6, 7 } });
}

test "mixed Latin and CJK: the later opportunity wins over the space" {
    // "ab 中文cd": the overflow is at 'c' (index 5, 60 wide). Han|Latin before it is an
    // opportunity later than the space at 2, so the break goes there and keeps 中文 on line one.
    const t = utf16.literal("ab 中文cd");
    try expectLines(&t, 50, &.{ .{ 0, 5 }, .{ 5, 7 } });
    // Latin|Han is an opportunity too: "abc中" breaks before 中 when the box is 30 wide.
    const t2 = utf16.literal("abc中文");
    try expectLines(&t2, 30, &.{ .{ 0, 3 }, .{ 3, 5 } });
}

test "a cluster wider than the box stands alone" {
    const gpa = std.testing.allocator;
    const text = utf16.literal("a bcd e");
    // 'bcd' is one cluster of 30 px in a box of 20.
    const adv = [_]f32{ 10, 10, 30, 0, 0, 10, 10 };
    const len = [_]u32{ 1, 1, 3, 0, 0, 1, 1 };
    var lines: std.ArrayList(Line) = .empty;
    defer lines.deinit(gpa);
    try wrapAdvances(gpa, &text, &adv, &len, 20, &lines);
    const expected = [_][2]u32{ .{ 0, 1 }, .{ 2, 5 }, .{ 6, 7 } };
    try std.testing.expectEqual(expected.len, lines.items.len);
    for (expected, lines.items) |e, l| {
        try std.testing.expectEqual(e[0], l.start);
        try std.testing.expectEqual(e[1], l.end);
    }
}

const FixedFont = struct {
    pub fn advance(_: FixedFont, _: u16, _: u16) f32 {
        return 10.0;
    }
    pub fn runWidth(_: FixedFont, text: []const u16) f32 {
        return 10.0 * @as(f32, @floatFromInt(text.len));
    }
};

test "agrees with l10n_wrap on the same fixed advances" {
    const gpa = std.testing.allocator;
    const texts = [_][]const u16{
        &utf16.literal("aaa bbb ccc ddd"),
        &utf16.literal("abcdefghijklmno"),
        &utf16.literal("あいうえ。おかきゃく、けこ"),
        &utf16.literal("ab 中文cd「ef」gh\r\nij"),
        &utf16.literal("a  b   c"),
    };
    var arena_state = std.heap.ArenaAllocator.init(gpa);
    defer arena_state.deinit();
    for (texts) |text| for ([_]f32{ 20, 30, 50, 90 }) |w| {
        var ours: std.ArrayList(Line) = .empty;
        defer ours.deinit(gpa);
        try fixed(gpa, text, w, &ours);
        var theirs: std.ArrayList(l10n_wrap.Line) = .empty;
        try l10n_wrap.wrapLines(arena_state.allocator(), FixedFont{}, text, 1.0, w, &theirs);
        try std.testing.expectEqual(theirs.items.len, ours.items.len);
        for (theirs.items, ours.items) |t, o| {
            try std.testing.expectEqual(@as(u32, @intCast(t.begin)), o.start);
            try std.testing.expectEqual(@as(u32, @intCast(t.end)), o.end);
        }
    };
}

fn loadSet() !faces.FaceSet {
    return faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
}

/// Every line fits, unless it is a single cluster; no line edge is inside a cluster; the lines
/// cover the text except the dropped breaks.
fn expectShapedLines(shaper: *shape.Shaper, text: []const u16, width: f32) !usize {
    const gpa = std.testing.allocator;
    var lines: std.ArrayList(Line) = .empty;
    defer lines.deinit(gpa);
    try wrapText(gpa, shaper, text, width, &lines);
    const run = try shaper.shape(text);
    defer run.deinit(gpa);
    const adv = try gpa.alloc(f32, text.len);
    defer gpa.free(adv);
    measure.unitAdvances(run, adv);
    const is_start = try gpa.alloc(bool, text.len + 1);
    defer gpa.free(is_start);
    @memset(is_start, false);
    is_start[text.len] = true;
    for (run.clusters) |cl| is_start[cl.start] = true;
    for (lines.items) |l| {
        try std.testing.expect(is_start[l.start] and is_start[l.end]);
        var clusters_in_line: usize = 0;
        for (l.start..l.end) |u| clusters_in_line += @intFromBool(is_start[u]);
        if (clusters_in_line > 1) try std.testing.expect(measure.rangeWidth(adv, l.start, l.end) <= width);
    }
    return lines.items.len;
}

test "shaped Latin, Cyrillic and Han lines fit" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try shape.Shaper.init(std.testing.allocator, &set, "en", 20);
    defer shaper.deinit();
    const latin = utf16.literal("Äv Évé fiancé AV To the quick brown fox jumps");
    try std.testing.expect(try expectShapedLines(&shaper, &latin, 120) > 2);
    const cyr = utf16.literal("Жёлтый танк едет по полю");
    try std.testing.expect(try expectShapedLines(&shaper, &cyr, 100) > 1);
    const han = utf16.literal("戦車は野原を走る。「前進」と言った。");
    try std.testing.expect(try expectShapedLines(&shaper, &han, 90) > 1);
}

test "a Devanagari line never splits a conjunct" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try shape.Shaper.init(std.testing.allocator, &set, "hi", 20);
    defer shaper.deinit();
    const text = utf16.literal("नमस्ते दुनिया क्षत्रिय श्रीमान् क्षेत्र");
    const widths = [_]f32{ 12, 40, 60, 90 };
    for (widths) |w| _ = try expectShapedLines(&shaper, &text, w);
    // At 12 px every cluster is wider than the box: each line is one cluster.
    const gpa = std.testing.allocator;
    var lines: std.ArrayList(Line) = .empty;
    defer lines.deinit(gpa);
    try wrapText(gpa, &shaper, &text, 12, &lines);
    const run = try shaper.shape(&text);
    defer run.deinit(gpa);
    var non_space: usize = 0;
    for (run.clusters) |cl| non_space += @intFromBool(text[cl.start] != ' ');
    try std.testing.expectEqual(non_space, lines.items.len);
}

test "CRLF in shaped text forces a line and is not drawn" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try shape.Shaper.init(std.testing.allocator, &set, "en", 16);
    defer shaper.deinit();
    const text = utf16.literal("one\r\ntwo\nthree");
    const gpa = std.testing.allocator;
    var lines: std.ArrayList(Line) = .empty;
    defer lines.deinit(gpa);
    try wrapText(gpa, &shaper, &text, 500, &lines);
    try std.testing.expectEqual(@as(usize, 3), lines.items.len);
    try std.testing.expectEqual(Line{ .start = 0, .end = 3 }, lines.items[0]);
    try std.testing.expectEqual(Line{ .start = 5, .end = 8 }, lines.items[1]);
    try std.testing.expectEqual(Line{ .start = 9, .end = 14 }, lines.items[2]);
}
