const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// The basic state for the system's power supply.
///
/// ## Remarks
/// This is returned by `PowerState.get()`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const PowerState = enum(c.SDL_PowerState) {
    /// Can not determine power status.
    unknown = c.SDL_POWERSTATE_UNKNOWN,
    /// Not plugged in, running on battery.
    on_battery = c.SDL_POWERSTATE_ON_BATTERY,
    /// Plugged in, no battery available.
    no_battery = c.SDL_POWERSTATE_NO_BATTERY,
    /// Plugged in, battery charging.
    charging = c.SDL_POWERSTATE_CHARGING,
    /// Plugged in, battery charged.
    charged = c.SDL_POWERSTATE_CHARGED,

    /// Get the current power supply details.
    ///
    /// ## Return Value
    /// Returns the power state of the computer.
    /// The seconds left of battery will also be populated if available, as well as the percent.
    /// Percent values range from 0 to 100.
    ///
    /// ## Remarks
    /// You should never take a battery status as absolute truth.
    /// Batteries (especially failing batteries) are delicate hardware, and the values reported here are best estimates based on what that hardware reports.
    /// It's not uncommon for older batteries to lose stored power much faster than it reports, or completely drain when reporting it has 20 percent left, etc.
    ///
    /// Battery status can change at any time; if you are concerned with power state, you should call this function frequently,
    /// and perhaps ignore changes until they seem to be stable for a few seconds.
    ///
    /// It's possible a platform can only report battery percentage or time left but not both.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn get() !struct { state: PowerState, seconds_left: ?u32, percent: ?u7 } {
        var seconds_left: c_int = undefined;
        var percent: c_int = undefined;
        const val = c.SDL_GetPowerInfo(
            &seconds_left,
            &percent,
        );
        const ret = try errors.wrapCall(c_int, val, c.SDL_POWERSTATE_ERROR);
        return .{
            .state = @enumFromInt(ret),
            .seconds_left = if (seconds_left == -1) null else @intCast(seconds_left),
            .percent = if (percent == -1) null else @intCast(percent),
        };
    }
};

// Test power state.
test "Power" {
    errors.refAllDeclsRecursive(@This());

    _ = try PowerState.get();
}
