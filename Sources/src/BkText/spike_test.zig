//! The two unknowns of M004, retired before the API is built on them:
//! HarfBuzz driven from Zig, and stb_truetype rasterising the CFF faces.
const std = @import("std");
const c = @import("c.zig");
const faces = @import("faces.zig");

const hb = c.hb;

const Shaped = struct {
    buffer: *hb.hb_buffer_t,
    infos: []hb.hb_glyph_info_t,
    positions: []hb.hb_glyph_position_t,

    fn deinit(self: Shaped) void {
        hb.hb_buffer_destroy(self.buffer);
    }

    fn distinctClusters(self: Shaped) usize {
        var n: usize = 0;
        var prev: ?u32 = null;
        for (self.infos) |info| {
            if (prev == null or prev.? != info.cluster) n += 1;
            prev = info.cluster;
        }
        return n;
    }
};

fn shape(set: *const faces.FaceSet, face: faces.Face, text: []const u16, script: []const u8, lang: []const u8) !Shaped {
    const buffer = hb.hb_buffer_create() orelse return error.OutOfMemory;
    errdefer hb.hb_buffer_destroy(buffer);
    hb.hb_buffer_add_utf16(buffer, text.ptr, @intCast(text.len), 0, @intCast(text.len));
    hb.hb_buffer_set_direction(buffer, hb.HB_DIRECTION_LTR);
    hb.hb_buffer_set_script(buffer, hb.hb_script_from_string(script.ptr, @intCast(script.len)));
    hb.hb_buffer_set_language(buffer, hb.hb_language_from_string(lang.ptr, @intCast(lang.len)));
    hb.hb_shape(set.get(face).hb_font, buffer, null, 0);
    var n: c_uint = 0;
    const infos = hb.hb_buffer_get_glyph_infos(buffer, &n);
    const positions = hb.hb_buffer_get_glyph_positions(buffer, &n);
    return .{ .buffer = buffer, .infos = infos[0..n], .positions = positions[0..n] };
}

fn utf16Literal(comptime s: []const u8) [std.unicode.calcUtf16LeLen(s) catch unreachable]u16 {
    var out: [std.unicode.calcUtf16LeLen(s) catch unreachable]u16 = undefined;
    _ = std.unicode.utf8ToUtf16Le(&out, s) catch unreachable;
    return out;
}

test "HarfBuzz shapes Devanagari from Zig into syllable clusters" {
    var set = try faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    const text = utf16Literal("क्षत्रिय हिन्दी");
    const shaped = try shape(&set, .noto_sans_devanagari, &text, "Deva", "hi");
    defer shaped.deinit();
    // Shaper.cpp counts consecutive distinct HarfBuzz cluster values the same
    // way; hb-shape on this face gives cluster starts 0 3 7 8 9 11 13.
    try std.testing.expectEqual(@as(usize, 7), shaped.distinctClusters());
    try std.testing.expect(shaped.infos.len > 6);
}

test "Japanese and Chinese Han glyphs differ for the same text" {
    var set = try faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    const text = utf16Literal("骨直");
    const ja = try shape(&set, .noto_sans_jp, &text, "Hani", "ja");
    defer ja.deinit();
    const zh = try shape(&set, .noto_sans_sc, &text, "Hani", "zh-Hans");
    defer zh.deinit();
    try std.testing.expectEqual(@as(usize, 2), ja.infos.len);
    try std.testing.expectEqual(@as(usize, 2), zh.infos.len);
    var differs = false;
    for (ja.infos, zh.infos) |a, b| differs = differs or a.codepoint != b.codepoint;
    // Glyph ids are per face, so identical ids would be a coincidence; the
    // advances differ too when the faces are not the same design.
    try std.testing.expect(differs);
}

fn rasteriseAll(set: *const faces.FaceSet, face: faces.Face, text: []const u16, script: []const u8, lang: []const u8) !void {
    const gpa = std.testing.allocator;
    const shaped = try shape(set, face, text, script, lang);
    defer shaped.deinit();
    const loaded = set.get(face);
    const px: f32 = 32;
    const scale = c.bk_stbtt_scale_for_mapping_em_to_pixels(loaded.stb.ptr, px);
    const upem: f32 = @floatFromInt(hb.hb_face_get_upem(loaded.hb_face));
    const hb_scale = px / upem;
    for (shaped.infos) |info| {
        const glyph: c_int = @intCast(info.codepoint);
        var x0: c_int = 0;
        var y0: c_int = 0;
        var x1: c_int = 0;
        var y1: c_int = 0;
        c.bk_stbtt_get_glyph_bitmap_box_subpixel(loaded.stb.ptr, glyph, scale, scale, 0, 0, &x0, &y0, &x1, &y1);
        const w: usize = @intCast(x1 - x0);
        const h: usize = @intCast(y1 - y0);
        try std.testing.expect(w > 0 and h > 0);
        const pixels = try gpa.alloc(u8, w * h);
        defer gpa.free(pixels);
        @memset(pixels, 0);
        c.bk_stbtt_make_glyph_bitmap_subpixel(loaded.stb.ptr, pixels.ptr, @intCast(w), @intCast(h), @intCast(w), scale, scale, 0, 0, glyph);
        var coverage: usize = 0;
        for (pixels) |p| coverage += p;
        try std.testing.expect(coverage > 0);

        // HarfBuzz's extents are in font units, y up; stb's box is in pixels, y down.
        var ext: hb.hb_glyph_extents_t = undefined;
        try std.testing.expect(hb.hb_font_get_glyph_extents(loaded.hb_font, info.codepoint, &ext) != 0);
        const ex0: f32 = @as(f32, @floatFromInt(ext.x_bearing)) * hb_scale;
        const ey0: f32 = -@as(f32, @floatFromInt(ext.y_bearing)) * hb_scale;
        const ex1: f32 = ex0 + @as(f32, @floatFromInt(ext.width)) * hb_scale;
        const ey1: f32 = ey0 - @as(f32, @floatFromInt(ext.height)) * hb_scale;
        try std.testing.expect(@abs(ex0 - @as(f32, @floatFromInt(x0))) <= 1.0);
        try std.testing.expect(@abs(ex1 - @as(f32, @floatFromInt(x1))) <= 1.0);
        try std.testing.expect(@abs(ey0 - @as(f32, @floatFromInt(y0))) <= 1.0);
        try std.testing.expect(@abs(ey1 - @as(f32, @floatFromInt(y1))) <= 1.0);
    }
}

test "stb_truetype rasterises the CFF faces and Devanagari glyphs like HarfBuzz measures them" {
    var set = try faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    const han = utf16Literal("骨直中文");
    try rasteriseAll(&set, .noto_sans_jp, &han, "Hani", "ja");
    try rasteriseAll(&set, .noto_sans_sc, &han, "Hani", "zh-Hans");
    // No space: its glyph has no outline to measure.
    const deva = utf16Literal("क्षत्रियहिन्दी");
    try rasteriseAll(&set, .noto_sans_devanagari, &deva, "Deva", "hi");
}
