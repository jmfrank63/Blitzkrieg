const std = @import("std");
pub const ReadbackError = error{ TooSmall, InvalidPitch, UnsupportedFormat };
pub fn rgba8(destination: []u8, source: []const u8, width: usize, height: usize, source_pitch: usize, bgra: bool) ReadbackError!usize {
    const row_bytes = width * 4;
    if (source_pitch < row_bytes) return ReadbackError.InvalidPitch;
    if (destination.len < row_bytes * height) return ReadbackError.TooSmall;
    for (0..height) |row| {
        for (0..width) |column| {
            const src = row * source_pitch + column * 4;
            const dst = row * row_bytes + column * 4;
            destination[dst + 0] = if (bgra) source[src + 2] else source[src + 0];
            destination[dst + 1] = source[src + 1];
            destination[dst + 2] = if (bgra) source[src + 0] else source[src + 2];
            destination[dst + 3] = source[src + 3];
        }
    }
    return row_bytes * height;
}
test "readback normalizes BGRA rows and checks destination size" { var source = [_]u8{ 1, 2, 3, 4 }; var destination = [_]u8{ 0, 0, 0, 0 }; _ = try rgba8(&destination, &source, 1, 1, 4, true); try std.testing.expectEqualSlices(u8, &[_]u8{ 3, 2, 1, 4 }, &destination); try std.testing.expectError(ReadbackError.TooSmall, rgba8(destination[0..3], &source, 1, 1, 4, false)); }
