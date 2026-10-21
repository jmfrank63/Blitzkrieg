//! Glyph raster: one glyph of one face at one pixel size as an R8 coverage bitmap. The scale is
//! stb's ScaleForMappingEmToPixels (pixels per em over units per em), the same factor HarfBuzz
//! gets from hb_font_set_scale(px * 64), so raster boxes agree with shaped advances and extents.
//! Always at shift 0 and an integer pen origin: the atlas caches one bitmap per glyph and size.
const std = @import("std");
const c = @import("c.zig");
const faces = @import("faces.zig");

pub const Bitmap = struct {
    /// width * height coverage bytes, row-major, top row first. Empty for a glyph without ink.
    pixels: []u8,
    width: u32,
    height: u32,
    /// Pixels from the pen to the bitmap's left edge (right is positive).
    bearing_x: i32,
    /// Pixels from the baseline up to the bitmap's top edge (up is positive).
    bearing_y: i32,

    pub fn deinit(self: Bitmap, allocator: std.mem.Allocator) void {
        allocator.free(self.pixels);
    }
};

/// Rasterises `glyph_id` of `face`. Glyph 0 is the face's .notdef box; reporting a missing glyph
/// is the shaper's job and happens at shaping time, not here.
pub fn rasterise(allocator: std.mem.Allocator, set: *const faces.FaceSet, face: faces.Face, glyph_id: u32, pixel_size: f32) !Bitmap {
    const stb = set.get(face).stb.ptr;
    const glyph: c_int = @intCast(glyph_id);
    const scale = c.bk_stbtt_scale_for_mapping_em_to_pixels(stb, pixel_size);
    var x0: c_int = 0;
    var y0: c_int = 0;
    var x1: c_int = 0;
    var y1: c_int = 0;
    c.bk_stbtt_get_glyph_bitmap_box_subpixel(stb, glyph, scale, scale, 0, 0, &x0, &y0, &x1, &y1);
    if (x1 <= x0 or y1 <= y0) {
        return .{ .pixels = &.{}, .width = 0, .height = 0, .bearing_x = 0, .bearing_y = 0 };
    }
    const w: usize = @intCast(x1 - x0);
    const h: usize = @intCast(y1 - y0);
    const pixels = try allocator.alloc(u8, w * h);
    @memset(pixels, 0);
    c.bk_stbtt_make_glyph_bitmap_subpixel(stb, pixels.ptr, @intCast(w), @intCast(h), @intCast(w), scale, scale, 0, 0, glyph);
    return .{ .pixels = pixels, .width = @intCast(w), .height = @intCast(h), .bearing_x = x0, .bearing_y = -y0 };
}
