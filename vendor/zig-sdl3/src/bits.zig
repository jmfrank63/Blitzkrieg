const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// Determine if a unsigned 32-bit value has exactly one bit set.
///
/// ## Function Parameters
/// * `val`: The 32-bit value to examine.
///
/// ## Return Value
/// Returns true if exactly one bit is set in `val`, false otherwise.
///
/// ## Remarks
/// If there are no bits set (x is zero), or more than one bit set, this returns false.
/// If any one bit is exclusively set, this returns true.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasExactlyOneBitSet(val: u32) bool {
    return c.SDL_HasExactlyOneBitSet32(val);
}

/// Get the index of the most significant (set) bit in a 32-bit number.
///
/// ## Function Parameters
/// * `val`: The 32-bit value to examine.
///
/// ## Return Value
/// Returns the index of the most significant bit, or `null` if the value is 0.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn mostSignificantBitIndex(val: u32) ?u5 {
    const ret = c.SDL_MostSignificantBitIndex32(val);
    if (ret == -1)
        return null;
    return @intCast(ret);
}

// Test bit-level functions.
test "Bits" {
    errors.refAllDeclsRecursive(@This());

    try std.testing.expect(hasExactlyOneBitSet(0x00010000));
    try std.testing.expect(!hasExactlyOneBitSet(0x10010000));
    try std.testing.expect(!hasExactlyOneBitSet(0));
    try std.testing.expectEqual(null, mostSignificantBitIndex(0));
    try std.testing.expectEqual(31, mostSignificantBitIndex(0xFEFE0000));
    try std.testing.expectEqual(12, mostSignificantBitIndex(0x1000));
}
