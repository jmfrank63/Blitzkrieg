//! Splits UTF-16 text into runs of one script and one face. The face is chosen
//! per cluster (a base code point with its following marks), so a combining
//! mark never lands in another face than its base. CR and LF are hard-break
//! runs that are never shaped.
const std = @import("std");
const c = @import("c.zig");
const faces = @import("faces.zig");
const utf16 = @import("utf16.zig");

const hb = c.hb;

pub const Run = struct {
    /// UTF-16 index and length of the run in the text.
    start: u32,
    len: u32,
    script: hb.hb_script_t,
    face: faces.Face,
    /// A CR, LF or CRLF: no glyphs, the layout breaks the line here.
    hard_break: bool = false,
};

pub fn isMark(cp: u21) bool {
    if (cp == 0x200C or cp == 0x200D) return true;
    if ((cp >= 0xFE00 and cp <= 0xFE0F) or (cp >= 0xE0100 and cp <= 0xE01EF)) return true;
    return switch (hb.hb_unicode_general_category(hb.hb_unicode_funcs_get_default(), cp)) {
        hb.HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK,
        hb.HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK,
        hb.HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK,
        => true,
        else => false,
    };
}

/// Joiners and variation selectors have no glyph in most faces and HarfBuzz
/// handles them itself, so they never count against a face's coverage.
fn isDefaultIgnorable(cp: u21) bool {
    return cp == 0x200C or cp == 0x200D or (cp >= 0xFE00 and cp <= 0xFE0F) or (cp >= 0xE0100 and cp <= 0xE01EF);
}

fn isCommonOrInherited(script: hb.hb_script_t) bool {
    return script == hb.HB_SCRIPT_COMMON or script == hb.HB_SCRIPT_INHERITED or script == hb.HB_SCRIPT_INVALID;
}

/// The face for one cluster: the first in `chain` covering the base and all
/// marks, else the first covering the base, else the last (glyph 0 = missing).
pub fn faceForCluster(set: *const faces.FaceSet, chain: []const faces.Face, cluster: []const u16) faces.Face {
    var first = utf16.iterate(cluster);
    const base = (first.next() orelse return chain[chain.len - 1]).cp;
    for (chain) |face| {
        var it = utf16.iterate(cluster);
        var all = true;
        while (it.next()) |item| {
            if (item.cp != base and isDefaultIgnorable(item.cp)) continue;
            if (!set.covers(face, item.cp)) {
                all = false;
                break;
            }
        }
        if (all) return face;
    }
    for (chain) |face| if (set.covers(face, base)) return face;
    return chain[chain.len - 1];
}

const Cluster = struct {
    start: u32,
    end: u32,
    script: hb.hb_script_t,
    face: faces.Face,
    hard_break: bool,
};

/// The runs of `text`, owned by the caller. `chain` is the fallback order.
pub fn segment(allocator: std.mem.Allocator, set: *const faces.FaceSet, chain: []const faces.Face, text: []const u16) std.mem.Allocator.Error![]Run {
    const funcs = hb.hb_unicode_funcs_get_default();
    var clusters: std.ArrayList(Cluster) = .empty;
    defer clusters.deinit(allocator);

    var it = utf16.iterate(text);
    while (it.next()) |item| {
        const start: u32 = @intCast(it.index - item.len);
        if (item.cp == '\r' or item.cp == '\n') {
            try clusters.append(allocator, .{ .start = start, .end = @intCast(it.index), .script = hb.HB_SCRIPT_COMMON, .face = chain[0], .hard_break = true });
            continue;
        }
        var end = it.index;
        while (end < text.len) {
            var peek = utf16.iterate(text[end..]);
            const next = peek.next().?;
            if (!isMark(next.cp)) break;
            end += next.len;
        }
        it.index = end;
        try clusters.append(allocator, .{
            .start = start,
            .end = @intCast(end),
            .script = hb.hb_unicode_script(funcs, item.cp),
            .face = faceForCluster(set, chain, text[start..end]),
            .hard_break = false,
        });
    }

    // Common and Inherited join the script before them; leading ones take the
    // script after them; text with neither stays Latin for the shaper.
    var last: ?hb.hb_script_t = null;
    for (clusters.items) |*cl| {
        if (cl.hard_break) continue;
        if (isCommonOrInherited(cl.script)) {
            if (last) |s| cl.script = s;
        } else last = cl.script;
    }
    var next_script: ?hb.hb_script_t = null;
    var i = clusters.items.len;
    while (i > 0) {
        i -= 1;
        const cl = &clusters.items[i];
        if (cl.hard_break) continue;
        if (isCommonOrInherited(cl.script)) {
            if (next_script) |s| cl.script = s;
        } else next_script = cl.script;
    }

    var runs: std.ArrayList(Run) = .empty;
    errdefer runs.deinit(allocator);
    for (clusters.items) |cl| {
        const latin: hb.hb_script_t = hb.HB_SCRIPT_LATIN;
        const common: hb.hb_script_t = hb.HB_SCRIPT_COMMON;
        const script = if (cl.hard_break) common else if (isCommonOrInherited(cl.script)) latin else cl.script;
        if (runs.items.len > 0) {
            const prev = &runs.items[runs.items.len - 1];
            const same = if (cl.hard_break) prev.hard_break else !prev.hard_break and prev.script == script and prev.face == cl.face;
            if (same) {
                prev.len = cl.end - prev.start;
                continue;
            }
        }
        try runs.append(allocator, .{ .start = cl.start, .len = cl.end - cl.start, .script = script, .face = cl.face, .hard_break = cl.hard_break });
    }
    return runs.toOwnedSlice(allocator);
}

fn testSet() !faces.FaceSet {
    return faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
}

test "Latin text is one run in the first face" {
    var set = try testSet();
    defer set.deinit();
    const text = utf16.literal("Äv Évé fiancé AV To");
    const runs = try segment(std.testing.allocator, &set, &faces.chainFor("en"), &text);
    defer std.testing.allocator.free(runs);
    try std.testing.expectEqual(@as(usize, 1), runs.len);
    try std.testing.expectEqual(faces.Face.liberation_serif, runs[0].face);
    try std.testing.expectEqual(@as(u32, text.len), runs[0].len);
}

test "CR LF and CRLF are their own hard-break runs" {
    var set = try testSet();
    defer set.deinit();
    const text = utf16.literal("ab\r\ncd\nef");
    const runs = try segment(std.testing.allocator, &set, &faces.chainFor("en"), &text);
    defer std.testing.allocator.free(runs);
    try std.testing.expectEqual(@as(usize, 5), runs.len);
    try std.testing.expect(!runs[0].hard_break and runs[1].hard_break and !runs[2].hard_break and runs[3].hard_break and !runs[4].hard_break);
    try std.testing.expectEqual(@as(u32, 2), runs[1].len);
    try std.testing.expectEqual(@as(u32, 1), runs[3].len);
}

test "a mark stays with its base even when only a later face has the mark" {
    var set = try testSet();
    defer set.deinit();
    // The Devanagari stress sign is not in Liberation Serif; the base 'a' is.
    const text = utf16.literal("a\u{0951}b");
    const runs = try segment(std.testing.allocator, &set, &faces.chainFor("en"), &text);
    defer std.testing.allocator.free(runs);
    try std.testing.expectEqual(@as(u32, 0), runs[0].start);
    try std.testing.expect(runs[0].len >= 2);
    try std.testing.expect(!set.covers(.liberation_serif, 0x0951) or runs[0].face == .liberation_serif);
    try std.testing.expect(runs[0].face != .noto_sans_devanagari);
}

test "an uncovered code point takes the last face" {
    var set = try testSet();
    defer set.deinit();
    const text = utf16.literal("\u{10FFFD}");
    const runs = try segment(std.testing.allocator, &set, &faces.chainFor("en"), &text);
    defer std.testing.allocator.free(runs);
    try std.testing.expectEqual(@as(usize, 1), runs.len);
    try std.testing.expectEqual(faces.Face.noto_sans_devanagari, runs[0].face);
    try std.testing.expectEqual(@as(u32, 2), runs[0].len);
}
