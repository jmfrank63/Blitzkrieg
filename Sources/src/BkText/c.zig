//! The C side of bk_text: HarfBuzz through one translated header module (Zig
//! 0.17 has no @cImport; build.zig makes `hb_c`), and the thin
//! non-static wrappers around the file-local stb_truetype in stbtt.c.
pub const hb = @import("hb_c");

/// std.c.getenv is an `extern "c"` and so makes the module depend on Zig's libc; on MSVC the library
/// takes the engine's CRT only (build.zig, bkTextModuleAt), and this plain extern resolves against it.
pub extern fn getenv(name: [*:0]const u8) ?[*:0]u8;

pub extern fn bk_stbtt_info_size() usize;
pub extern fn bk_stbtt_init_font(info: *anyopaque, data: [*]const u8, offset: c_int) c_int;
pub extern fn bk_stbtt_find_glyph_index(info: *const anyopaque, codepoint: c_int) c_int;
pub extern fn bk_stbtt_get_font_vmetrics(info: *const anyopaque, ascent: *c_int, descent: *c_int, line_gap: *c_int) void;
pub extern fn bk_stbtt_scale_for_mapping_em_to_pixels(info: *const anyopaque, pixels: f32) f32;
pub extern fn bk_stbtt_get_glyph_hmetrics(info: *const anyopaque, glyph: c_int, advance: *c_int, lsb: *c_int) void;
pub extern fn bk_stbtt_get_glyph_bitmap_box_subpixel(info: *const anyopaque, glyph: c_int, sx: f32, sy: f32, shift_x: f32, shift_y: f32, x0: *c_int, y0: *c_int, x1: *c_int, y1: *c_int) void;
pub extern fn bk_stbtt_make_glyph_bitmap_subpixel(info: *const anyopaque, output: [*]u8, out_w: c_int, out_h: c_int, out_stride: c_int, sx: f32, sy: f32, shift_x: f32, shift_y: f32, glyph: c_int) void;
