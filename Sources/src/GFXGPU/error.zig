pub const Result = u32;

pub const ok: Result = 0;
pub const invalid_argument: Result = 1;
pub const invalid_state: Result = 2;
pub const invalid_handle: Result = 3;
pub const unsupported: Result = 4;
pub const out_of_memory: Result = 5;
pub const sdl_error: Result = 6;
pub const io_error: Result = 7;
pub const shader_error: Result = 8;
pub const internal_error: Result = 9;

pub fn copyBounded(message: []const u8, destination: ?[*]u8, capacity: u32, written: ?*u32) Result {
    if (written) |out| out.* = 0;
    if (capacity != 0 and destination == null) return invalid_argument;
    const count: usize = @min(message.len, @as(usize, capacity));
    if (destination) |dest| {
        @memcpy(dest[0..count], message[0..count]);
        if (count < capacity) dest[count] = 0;
    }
    if (written) |out| out.* = @intCast(count);
    return ok;
}

test "bounded diagnostic copy never exceeds caller capacity" {
    var bytes = [_]u8{ 0xaa, 0xaa, 0xaa, 0xaa };
    var written: u32 = 0;
    try std.testing.expectEqual(ok, copyBounded("diagnostic", &bytes, 3, &written));
    try std.testing.expectEqual(@as(u32, 3), written);
    try std.testing.expectEqualSlices(u8, "dia", bytes[0..3]);
}

const std = @import("std");
