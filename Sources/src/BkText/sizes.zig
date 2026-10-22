//! The Game's pixel size per size class (D102, D108), in one place so the Game, l10n-check and
//! language-mods measure at exactly the sizes the Game draws. The values are the shipped bitmap
//! fonts' heights, the GDI cell height FontGen rendered them at (ascent + descent,
//! `SFontFormat::metrics.nHeight`), so layouts built for them keep their proportions. A Shaper's
//! pixel size is an em size (HarfBuzz scale = px * 64, stb_truetype scale_for_mapping_em_to_pixels);
//! `emForCell` turns a cell height into it. Used as an em size directly, 40 drew Times-metric text
//! about 11 % larger than the 2003 font (its ascent + descent is 1.107 em).
const std = @import("std");
const c = @import("c.zig");
const faces = @import("faces.zig");

pub const SizeClass = enum(u32) { tiny = 0, small = 1, medium = 2, large = 3 };

/// The cell height in pixels of `class` at `ui_scale` (1.0 is the Game's own): ascent + descent of
/// the font, not its em size (`emForCell`).
pub fn pixelSize(class: SizeClass, ui_scale: f32) f32 {
    const base: f32 = switch (class) {
        .tiny => 10.0,
        .small => 16.0,
        .medium => 20.0,
        .large => 40.0,
    };
    return base * ui_scale;
}

test "pixelSize is the Game's table" {
    try std.testing.expectEqual(@as(f32, 10), pixelSize(.tiny, 1.0));
    try std.testing.expectEqual(@as(f32, 16), pixelSize(.small, 1.0));
    try std.testing.expectEqual(@as(f32, 20), pixelSize(.medium, 1.0));
    try std.testing.expectEqual(@as(f32, 40), pixelSize(.large, 1.0));
    try std.testing.expectEqual(@as(f32, 30), pixelSize(.medium, 1.5));
}

/// The em size in pixels at which the chains' primary face (Liberation Serif, first in every chain)
/// is `cell` pixels from ascent to descent: FontGen's stbtt_ScaleForPixelHeight, so a size class is
/// as tall as the shipped bitmap font of that class.
pub fn emForCell(set: *const faces.FaceSet, cell: f32) f32 {
    const stb = set.get(.liberation_serif).stb.ptr;
    var ascent: c_int = 0;
    var descent: c_int = 0;
    var line_gap: c_int = 0;
    c.bk_stbtt_get_font_vmetrics(stb, &ascent, &descent, &line_gap);
    const per_unit = c.bk_stbtt_scale_for_mapping_em_to_pixels(stb, 1.0);
    const span: f32 = @floatFromInt(ascent - descent);
    return cell / (span * per_unit);
}

test "emForCell makes Liberation Serif as tall as the cell" {
    var set = try faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    const em = emForCell(&set, 40);
    try std.testing.expect(em > 35.9 and em < 36.3);
    try std.testing.expectApproxEqAbs(@as(f32, 18.07), emForCell(&set, 20), 0.05);
}
