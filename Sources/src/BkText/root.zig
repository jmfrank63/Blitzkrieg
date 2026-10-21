//! bk_text: the portable TrueType text engine (M004). Faces, UTF-16,
//! HarfBuzz shaping and stb_truetype raster; the Game does not use it yet.
pub const c = @import("c.zig");
pub const faces = @import("faces.zig");
pub const utf16 = @import("utf16.zig");
pub const segment = @import("segment.zig");
pub const shape = @import("shape.zig");

test {
    _ = faces;
    _ = utf16;
    _ = segment;
    _ = shape;
    _ = @import("spike_test.zig");
}
