const std = @import("std");
const formats = @import("formats.zig");
const handles = @import("handles.zig");
pub const TextureError = error{ InvalidDimensions, InvalidMipCount, Overflow, UnsupportedFormat, OutOfBounds };
pub const Texture = struct { width: u32, height: u32, mips: u32, format: formats.PixelFormat, last_use_serial: u64 = 0 };
pub fn mipExtent(value: u32, level: u32) u32 { return @max(@as(u32, 1), value >> @intCast(@min(level, 31))); }
/// Bytes per texel of the uncompressed formats; the block formats have none.
pub fn bytesPerPixel(format: formats.PixelFormat) u32 {
    return switch (format) { .a8, .r8 => 1, .rg8, .depth16 => 2, else => 4 };
}
pub fn rowPitch(format: formats.PixelFormat, width: u32) TextureError!u64 {
    if (width == 0) return TextureError.InvalidDimensions;
    return switch (format) { .dxt1 => ((width + 3) / 4) * 8, .dxt3, .dxt5 => ((width + 3) / 4) * 16, else => @as(u64, width) * bytesPerPixel(format) };
}
/// A region of a texture's base level, in texels.
pub const Region = struct { x: u32, y: u32, w: u32, h: u32 };
/// Checks a region upload against the texture and the caller's bytes. `byte_length` must cover
/// the last row's texels, not a full final pitch, so a tightly packed sub-rectangle of a larger
/// bitmap can be handed over as it is.
pub fn validateRegion(texture_width: u32, texture_height: u32, bytes_per_pixel: u32, region: Region, row_pitch: u32, byte_length: u32) TextureError!void {
    if (region.w == 0 or region.h == 0) return TextureError.OutOfBounds;
    const right = std.math.add(u32, region.x, region.w) catch return TextureError.OutOfBounds;
    const bottom = std.math.add(u32, region.y, region.h) catch return TextureError.OutOfBounds;
    if (right > texture_width or bottom > texture_height) return TextureError.OutOfBounds;
    const row_bytes = std.math.mul(u64, region.w, bytes_per_pixel) catch return TextureError.Overflow;
    if (row_pitch < row_bytes or row_pitch % bytes_per_pixel != 0) return TextureError.OutOfBounds;
    const needed = @as(u64, row_pitch) * (region.h - 1) + row_bytes;
    if (byte_length < needed) return TextureError.OutOfBounds;
}
pub fn validate(width: u32, height: u32, mips: u32, format: formats.PixelFormat) TextureError!Texture { if (width == 0 or height == 0) return TextureError.InvalidDimensions; if (mips == 0 or mips > 16) return TextureError.InvalidMipCount; _ = try rowPitch(format, width); return .{ .width = width, .height = height, .mips = mips, .format = format }; }
test "r8 pitch is one byte per texel and region bounds are checked" {
    try std.testing.expectEqual(@as(u64, 7), try rowPitch(.r8, 7));
    try std.testing.expectEqual(@as(u64, 28), try rowPitch(.rgba8, 7));
    try std.testing.expectEqual(@as(u32, 1), bytesPerPixel(.r8));
    try validateRegion(8, 8, 1, .{ .x = 4, .y = 4, .w = 4, .h = 4 }, 4, 16);
    try validateRegion(8, 8, 1, .{ .x = 0, .y = 0, .w = 3, .h = 2 }, 8, 11);
    try std.testing.expectError(TextureError.OutOfBounds, validateRegion(8, 8, 1, .{ .x = 5, .y = 0, .w = 4, .h = 1 }, 4, 4));
    try std.testing.expectError(TextureError.OutOfBounds, validateRegion(8, 8, 1, .{ .x = 0, .y = 0, .w = 0, .h = 1 }, 1, 1));
    try std.testing.expectError(TextureError.OutOfBounds, validateRegion(8, 8, 1, .{ .x = std.math.maxInt(u32), .y = 0, .w = 2, .h = 1 }, 2, 2));
    try std.testing.expectError(TextureError.OutOfBounds, validateRegion(8, 8, 1, .{ .x = 0, .y = 0, .w = 4, .h = 4 }, 3, 16));
    try std.testing.expectError(TextureError.OutOfBounds, validateRegion(8, 8, 1, .{ .x = 0, .y = 0, .w = 4, .h = 4 }, 4, 15));
}
test "texture dimensions, mip extents, and compressed row pitches" {
    const texture = try validate(7, 5, 3, .dxt1); try std.testing.expectEqual(@as(u32, 1), mipExtent(texture.width, 2)); try std.testing.expectEqual(@as(u64, 16), try rowPitch(.dxt1, 7)); try std.testing.expectError(TextureError.InvalidDimensions, validate(0, 1, 1, .rgba8));
}
