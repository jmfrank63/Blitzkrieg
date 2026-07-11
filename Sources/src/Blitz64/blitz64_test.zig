const std = @import("std");
const blitz64 = @import("blitz64");

test "bit conversion preserves every IEEE-754 single-precision bit" {
    const bits: u32 = 0x7fc0_1234;
    try std.testing.expectEqual(bits, blitz64.bk_f32_bits(@bitCast(bits)));
}
