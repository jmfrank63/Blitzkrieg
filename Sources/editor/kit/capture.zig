//! The frame capture's file format: an uncompressed 32-bit TGA with a
//! top-left origin, the layout `tools/zig/bk_image.zig`'s `readTga` and the
//! engine's reader accept. The GPU readback itself is `Window.capture`; this
//! half is pure so its bytes are unit-tested without a device.
const std = @import("std");

pub const Error = error{ BadSize, BadLength, OutOfMemory };

const footer = "\x00\x00\x00\x00\x00\x00\x00\x00TRUEVISION-XFILE.\x00";

/// `rgba` holds `w * h` pixels as R, G, B, A bytes, top row first. The file
/// stores B, G, R, A. Caller owns the result. A TGA's size fields are 16 bits,
/// so a side of 0 or over 65535 is `BadSize`.
pub fn encodeTga(alloc: std.mem.Allocator, w: u32, h: u32, rgba: []const u8) Error![]u8 {
    if (w == 0 or h == 0 or w > std.math.maxInt(u16) or h > std.math.maxInt(u16)) return error.BadSize;
    const count = @as(usize, w) * h;
    if (rgba.len != count * 4) return error.BadLength;
    const out = try alloc.alloc(u8, 18 + count * 4 + footer.len);
    @memset(out[0..18], 0);
    out[2] = 2; // true colour, uncompressed
    std.mem.writeInt(u16, out[12..14], @intCast(w), .little);
    std.mem.writeInt(u16, out[14..16], @intCast(h), .little);
    out[16] = 32;
    out[17] = 0x28; // 8 alpha bits, top-left origin
    for (0..count) |i| {
        const src = rgba[i * 4 ..][0..4];
        const dst = out[18 + i * 4 ..][0..4];
        dst.* = .{ src[2], src[1], src[0], src[3] };
    }
    @memcpy(out[18 + count * 4 ..], footer);
    return out;
}

test "a 2x2 image round trips through the TGA bytes" {
    const rgba = [_]u8{
        1,  2,  3,  4,  5,  6,  7,  8,
        9,  10, 11, 12, 13, 14, 15, 16,
    };
    const bytes = try encodeTga(std.testing.allocator, 2, 2, &rgba);
    defer std.testing.allocator.free(bytes);
    try std.testing.expectEqual(@as(u8, 2), bytes[2]);
    try std.testing.expectEqual(@as(u16, 2), std.mem.readInt(u16, bytes[12..14], .little));
    try std.testing.expectEqual(@as(u16, 2), std.mem.readInt(u16, bytes[14..16], .little));
    try std.testing.expectEqual(@as(u8, 32), bytes[16]);
    try std.testing.expect(bytes[17] & 0x20 != 0);
    // Decode as a reader does: B, G, R, A back to R, G, B, A.
    var decoded: [16]u8 = undefined;
    for (0..4) |i| {
        const px = bytes[18 + i * 4 ..][0..4];
        decoded[i * 4 ..][0..4].* = .{ px[2], px[1], px[0], px[3] };
    }
    try std.testing.expectEqualSlices(u8, &rgba, &decoded);
    try std.testing.expectEqualStrings("TRUEVISION-XFILE.\x00", bytes[bytes.len - 18 ..]);
}

test "a zero size and a wrong buffer length are rejected" {
    const rgba: [16]u8 = @splat(0);
    try std.testing.expectError(error.BadSize, encodeTga(std.testing.allocator, 0, 2, &rgba));
    try std.testing.expectError(error.BadSize, encodeTga(std.testing.allocator, 2, 0, &rgba));
    try std.testing.expectError(error.BadSize, encodeTga(std.testing.allocator, 65536, 1, &rgba));
    try std.testing.expectError(error.BadLength, encodeTga(std.testing.allocator, 2, 2, rgba[0..15]));
    try std.testing.expectError(error.BadLength, encodeTga(std.testing.allocator, 2, 1, &rgba));
}
