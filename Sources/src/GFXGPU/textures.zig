const std = @import("std");
const formats = @import("formats.zig");
const handles = @import("handles.zig");
pub const TextureError = error{ InvalidDimensions, InvalidMipCount, Overflow, UnsupportedFormat, OutOfBounds };
pub const Texture = struct { width: u32, height: u32, mips: u32, format: formats.PixelFormat, last_use_serial: u64 = 0 };
pub fn mipExtent(value: u32, level: u32) u32 { return @max(@as(u32, 1), value >> @intCast(@min(level, 31))); }
pub fn rowPitch(format: formats.PixelFormat, width: u32) TextureError!u64 {
    if (width == 0) return TextureError.InvalidDimensions;
    return switch (format) { .dxt1 => ((width + 3) / 4) * 8, .dxt3, .dxt5 => ((width + 3) / 4) * 16, else => @as(u64, width) * 4 };
}
pub fn validate(width: u32, height: u32, mips: u32, format: formats.PixelFormat) TextureError!Texture { if (width == 0 or height == 0) return TextureError.InvalidDimensions; if (mips == 0 or mips > 16) return TextureError.InvalidMipCount; _ = try rowPitch(format, width); return .{ .width = width, .height = height, .mips = mips, .format = format }; }
test "texture dimensions, mip extents, and compressed row pitches" {
    const texture = try validate(7, 5, 3, .dxt1); try std.testing.expectEqual(@as(u32, 3), mipExtent(texture.width, 2)); try std.testing.expectEqual(@as(u64, 16), try rowPitch(.dxt1, 7)); try std.testing.expectError(TextureError.InvalidDimensions, validate(0, 1, 1, .rgba8));
}
