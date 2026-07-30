const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// A `GUID` is a 128-bit identifier for an input device that identifies that device across runs of SDL programs on the same platform.
///
/// ## Remarks
/// If the device is detached and then re-attached to a different port, or if the base system is rebooted, the device should still report the same GUID.
///
/// GUIDs are as precise as possible but are not guaranteed to distinguish physically distinct but equivalent devices.
/// For example, two game controllers from the same vendor with the same product ID and revision may have the same GUID.
///
/// GUIDs may be platform-dependent (i.e., the same device may report different GUIDs on different operating systems).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Guid = struct {
    value: c.SDL_GUID,

    /// Convert a GUID string into a GUID structure.
    ///
    /// ## Function Parameters
    /// * `str`: String containing an ASCII representation of a GUID.
    ///
    /// ## Return Value
    /// Returns a GUID.
    ///
    /// ## Remarks
    /// Performs no error checking.
    /// If this function is given a string containing an invalid GUID, the function will silently succeed, but the GUID generated will not be useful.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromString(
        str: [:0]const u8,
    ) Guid {
        const ret = c.SDL_StringToGUID(
            str,
        );
        return Guid{ .value = ret };
    }

    /// Get an ASCII string representation for a given `GUID`.
    ///
    /// ## Function Parameters
    /// * `self`: The GUID to convert to a string.
    /// * `str`: Buffer in which to write the string to.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn toString(
        self: Guid,
        str: *[33:0]u8,
    ) void {
        const ret = c.SDL_GUIDToString(
            self.value,
            str,
            @intCast(str.len),
        );
        _ = ret;
    }
};

// GUID testing.
test "GUID" {
    errors.refAllDeclsRecursive(@This());

    const guid = Guid{
        .value = .{
            .data = [_]u8{ 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF },
        },
    };
    var buf: [33:0]u8 = undefined;
    guid.toString(&buf);
    const guid_new = Guid.fromString(&buf);
    try std.testing.expectEqual(guid, guid_new);
}
