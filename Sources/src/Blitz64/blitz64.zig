pub fn bk_f32_bits(value: f32) u32 {
    return @bitCast(value);
}

export fn bk_f32_bits_c(value: f32) u32 {
    return bk_f32_bits(value);
}

test "bit conversion preserves every IEEE-754 single-precision bit" {
    const std = @import("std");
    const bits: u32 = 0x7fc0_1234;
    try std.testing.expectEqual(bits, bk_f32_bits(@bitCast(bits)));
}
