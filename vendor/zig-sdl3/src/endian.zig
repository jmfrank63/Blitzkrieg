const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// Byte order of the system.
///
/// ## Version
/// This enum is provided by zig-sdl3.
pub const ByteOrder = enum(c_int) {
    /// A value to represent bigendian byteorder.
    big = c.SDL_BIG_ENDIAN,
    /// A value to represent littleendian byteorder.
    little = c.SDL_LIL_ENDIAN,
};

/// A function that reports the target system's byte order.
///
/// ## Return Value
/// The system's byte order.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn byteOrder() ByteOrder {
    return @enumFromInt(c.SDL_BYTEORDER);
}

/// A function that reports the target system's floating point word order.
///
/// ## Return Value
/// The system's floating point word order.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn floatWordOrder() ByteOrder {
    return @enumFromInt(c.SDL_FLOATWORDORDER);
}

/// Byte-swap an unsigned 16-bit number.
///
/// ## Function Parameters
/// * `val`: The value to byte-swap.
///
/// ## Return Value
/// Returns `val` with its bytes in the opposite endian order.
///
/// ## Remarks
/// This will always byte-swap the value, whether it's currently in the native byteorder of the system or not.
/// You should use `endian.swap16Le()` or `endian.swap16Be()` instead, in most cases.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap16(val: u16) u16 {
    return c.SDL_Swap16(val);
}

/// Swap a 16-bit value from bigendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in bigendian byte order.
///
/// ## Remarks
/// If this is running on a bigendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap16Be(val: u16) u16 {
    return c.SDL_Swap16BE(val);
}

/// Swap a 16-bit value from littleendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in littleendian byte order.
///
/// ## Remarks
/// If this is running on a littleendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap16Le(val: u16) u16 {
    return c.SDL_Swap16LE(val);
}

/// Byte-swap an unsigned 32-bit number.
///
/// ## Function Parameters
/// * `val`: The value to byte-swap.
///
/// ## Return Value
/// Returns `val` with its bytes in the opposite endian order.
///
/// ## Remarks
/// This will always byte-swap the value, whether it's currently in the native byteorder of the system or not.
/// You should use `endian.swap32Le()` or `endian.swap32Be()` instead, in most cases.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap32(val: u32) u32 {
    return c.SDL_Swap32(val);
}

/// Swap a 32-bit value from bigendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in bigendian byte order.
///
/// ## Remarks
/// If this is running on a bigendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap32Be(val: u32) u32 {
    return c.SDL_Swap32BE(val);
}

/// Swap a 32-bit value from littleendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in littleendian byte order.
///
/// ## Remarks
/// If this is running on a littleendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap32Le(val: u32) u32 {
    return c.SDL_Swap32LE(val);
}

/// Byte-swap an unsigned 64-bit number.
///
/// ## Function Parameters
/// * `val`: The value to byte-swap.
///
/// ## Return Value
/// Returns `val` with its bytes in the opposite endian order.
///
/// ## Remarks
/// This will always byte-swap the value, whether it's currently in the native byteorder of the system or not.
/// You should use `endian.swap64Le()` or `endian.swap64Be()` instead, in most cases.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap64(val: u64) u64 {
    return c.SDL_Swap64(val);
}

/// Swap a 64-bit value from bigendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in bigendian byte order.
///
/// ## Remarks
/// If this is running on a bigendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap64Be(val: u64) u64 {
    return c.SDL_Swap64BE(val);
}

/// Swap a 64-bit value from littleendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in littleendian byte order.
///
/// ## Remarks
/// If this is running on a littleendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swap64Le(val: u64) u64 {
    return c.SDL_Swap64LE(val);
}

/// Byte-swap a floating point number.
///
/// ## Function Parameters
/// * `val`: The value to byte-swap.
///
/// ## Return Value
/// Returns `val` with its bytes in the opposite endian order.
///
/// ## Remarks
/// This will always byte-swap the value, whether it's currently in the native byteorder of the system or not.
/// You should use `endian.swapFloatLe()` or `endian.swapFloatBe()` instead, in most cases.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swapFloat(val: f32) f32 {
    return c.SDL_SwapFloat(val);
}

/// Swap a floating point value from bigendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in bigendian byte order.
///
/// ## Return Value
/// Returns `val` in native byte order.
///
/// ## Remarks
/// If this is running on a bigendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swapFloatBe(val: f32) f32 {
    return c.SDL_SwapFloatBE(val);
}

/// Swap a floating point value from littleendian to native byte order.
///
/// ## Function Parameters
/// * `val`: The value to swap, in littleendian byte order.
///
/// ## Return Value
/// Returns `val` in native byte order.
///
/// ## Remarks
/// If this is running on a littleendian system, `val` is returned unchanged.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn swapFloatLe(val: f32) f32 {
    return c.SDL_SwapFloatLE(val);
}

// Endian tests.
test "Endian" {
    errors.refAllDeclsRecursive(@This());

    try std.testing.expectEqual(0x1234, swap16(0x3412));
    try std.testing.expectEqual(0x12345678, swap32(0x78563412));
    try std.testing.expectEqual(0x123456789abcdef0, swap64(0xf0debc9a78563412));

    _ = swapFloat(0);
    _ = byteOrder();
    _ = floatWordOrder();

    try std.testing.expectEqual(swap16Be(0x1234), swap16Le(0x3412));
    try std.testing.expectEqual(swap32Be(0x12345678), swap32Le(0x78563412));
    try std.testing.expectEqual(swap64Be(0x123456789abcdef0), swap64Le(0xf0debc9a78563412));

    _ = swapFloatBe(0);
    _ = swapFloatLe(0);
}
