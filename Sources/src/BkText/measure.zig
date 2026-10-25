//! Measuring from the shaped advances, the same numbers drawing and wrapping use (S03: measured ==
//! drawn). HarfBuzz positions are whole 26.6 units, so every advance is a multiple of 1/64 and
//! sums of them are exact in f32: the order of summation cannot change a width. The order is
//! still fixed (glyph order of the run) so a future non-integer source cannot drift.
const std = @import("std");
const c = @import("c.zig");
const faces = @import("faces.zig");
const shape = @import("shape.zig");

const hb = c.hb;

/// Pixels. descent is positive below the baseline.
pub const FaceMetrics = struct {
    ascent: f32,
    descent: f32,
    line_gap: f32,
};

/// hhea ascender, descender and line gap of `face` at the shaper's pixel size.
pub fn faceMetrics(shaper: *const shape.Shaper, face: faces.Face) FaceMetrics {
    var ext: hb.hb_font_extents_t = std.mem.zeroes(hb.hb_font_extents_t);
    _ = hb.hb_font_get_h_extents(shaper.fonts[@intFromEnum(face)], &ext);
    return .{
        .ascent = @as(f32, @floatFromInt(ext.ascender)) / 64.0,
        .descent = -@as(f32, @floatFromInt(ext.descender)) / 64.0,
        .line_gap = @as(f32, @floatFromInt(ext.line_gap)) / 64.0,
    };
}

/// Distance between baselines in whole pixels. It comes from the chain's primary face only, so a
/// font has one line space whichever fallback faces a string happens to use (D104).
pub fn lineSpace(shaper: *const shape.Shaper) f32 {
    const m = faceMetrics(shaper, shaper.chain[0]);
    return @ceil(m.ascent + m.descent + m.line_gap);
}

/// The whole-pixel width a summed advance occupies. Measure and wrap both end here and nowhere
/// else; rounding up means a line that fits by this number never overhangs its box.
pub fn pixelWidth(advance_sum: f32) f32 {
    return @ceil(advance_sum);
}

pub const Extents = struct {
    /// Sum of the glyph advances, unrounded.
    advance: f32,
    /// pixelWidth(advance).
    width: f32,
    /// Ink bounds relative to the pen start; x grows right, y grows up (HarfBuzz). All zero for a
    /// run without ink.
    ink_left: f32,
    ink_right: f32,
    ink_top: f32,
    ink_bottom: f32,
};

/// `run` shaped by `shaper`: the advance sum in glyph order plus the ink extents.
pub fn measureRun(shaper: *const shape.Shaper, run: shape.GlyphRun) Extents {
    var sum: f32 = 0;
    var any = false;
    var left: f32 = 0;
    var right: f32 = 0;
    var top: f32 = 0;
    var bottom: f32 = 0;
    for (run.glyphs) |g| {
        var ext: hb.hb_glyph_extents_t = std.mem.zeroes(hb.hb_glyph_extents_t);
        if (hb.hb_font_get_glyph_extents(shaper.fonts[@intFromEnum(g.face)], g.glyph_id, &ext) != 0 and ext.width != 0 and ext.height != 0) {
            const x0 = sum + g.x_offset + @as(f32, @floatFromInt(ext.x_bearing)) / 64.0;
            const x1 = x0 + @as(f32, @floatFromInt(ext.width)) / 64.0;
            const y1 = g.y_offset + @as(f32, @floatFromInt(ext.y_bearing)) / 64.0;
            const y0 = y1 + @as(f32, @floatFromInt(ext.height)) / 64.0;
            if (!any) {
                left = x0;
                right = x1;
                top = y1;
                bottom = y0;
                any = true;
            } else {
                left = @min(left, x0);
                right = @max(right, x1);
                top = @max(top, y1);
                bottom = @min(bottom, y0);
            }
        }
        sum += g.x_advance;
    }
    return .{ .advance = sum, .width = pixelWidth(sum), .ink_left = left, .ink_right = right, .ink_top = top, .ink_bottom = bottom };
}

/// Per UTF-16 unit advances for wrap: a cluster's whole advance sits on its first unit, the
/// other units (and hard-break characters) hold 0. `out.len` is the text length.
pub fn unitAdvances(run: shape.GlyphRun, out: []f32) void {
    @memset(out, 0);
    for (run.clusters) |cl| {
        var sum: f32 = 0;
        for (run.glyphs[cl.glyph_start..][0..cl.glyph_count]) |g| sum += g.x_advance;
        out[cl.start] = sum;
    }
}

/// The measured width of units [start, end) of `advances`, as wrap compares it.
pub fn rangeWidth(advances: []const f32, start: usize, end: usize) f32 {
    var sum: f32 = 0;
    for (advances[start..end]) |a| sum += a;
    return pixelWidth(sum);
}

// ---- tests ----

fn loadSet() !faces.FaceSet {
    return faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
}

test "metrics are sane for all five faces at four sizes" {
    var set = try loadSet();
    defer set.deinit();
    for ([_]f32{ 10, 16, 20, 40 }) |size| {
        var shaper = try shape.Shaper.init(std.testing.allocator, &set, "en", size);
        defer shaper.deinit();
        for (std.enums.values(faces.Face)) |face| {
            const m = faceMetrics(&shaper, face);
            try std.testing.expect(m.ascent > 0);
            try std.testing.expect(m.descent >= 0);
            try std.testing.expect(m.line_gap >= 0);
            const space = @ceil(m.ascent + m.descent + m.line_gap);
            try std.testing.expect(space >= m.ascent + m.descent);
        }
        const space = lineSpace(&shaper);
        try std.testing.expectEqual(@floor(space), space);
        const primary = faceMetrics(&shaper, .liberation_serif);
        try std.testing.expect(space >= primary.ascent + primary.descent);
    }
}

test "line space is the same for every string of a font" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try shape.Shaper.init(std.testing.allocator, &set, "ja", 24);
    defer shaper.deinit();
    const before = lineSpace(&shaper);
    const run = try shaper.shape(&@import("utf16.zig").literal("戦車 Tank टैंक"));
    defer run.deinit(std.testing.allocator);
    try std.testing.expectEqual(before, lineSpace(&shaper));
}

test "measureRun sums the advances and bounds the ink" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try shape.Shaper.init(std.testing.allocator, &set, "en", 40);
    defer shaper.deinit();
    const text = @import("utf16.zig").literal("Hxg");
    const run = try shaper.shape(&text);
    defer run.deinit(std.testing.allocator);
    const e = measureRun(&shaper, run);
    var expected: f32 = 0;
    for (run.glyphs) |g| expected += g.x_advance;
    try std.testing.expectEqual(expected, e.advance);
    try std.testing.expectEqual(@ceil(expected), e.width);
    try std.testing.expect(e.ink_right > e.ink_left);
    try std.testing.expect(e.ink_top > 0 and e.ink_bottom < 0); // H rises, g descends
    try std.testing.expect(e.ink_right <= e.advance + 1);
    // Per-unit advances add up to the same sum.
    var units: [3]f32 = undefined;
    unitAdvances(run, &units);
    try std.testing.expectEqual(e.width, rangeWidth(&units, 0, 3));
}

test "an empty run has no ink and no width" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try shape.Shaper.init(std.testing.allocator, &set, "en", 20);
    defer shaper.deinit();
    const run = try shaper.shape(&.{});
    defer run.deinit(std.testing.allocator);
    const e = measureRun(&shaper, run);
    try std.testing.expectEqual(@as(f32, 0), e.width);
    try std.testing.expectEqual(@as(f32, 0), e.ink_right - e.ink_left);
}
