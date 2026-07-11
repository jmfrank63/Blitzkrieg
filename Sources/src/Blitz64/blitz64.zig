pub fn bk_f32_bits(value: f32) u32 {
    return @bitCast(value);
}

export fn bk_f32_bits_c(value: f32) u32 {
    return bk_f32_bits(value);
}
