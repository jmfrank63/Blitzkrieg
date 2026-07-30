const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// Number of microseconds in a second.
///
/// ## Remarks
/// This is always 1000000.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const microseconds_per_second: comptime_int = c.SDL_US_PER_SECOND;

/// Number of milliseconds in a second.
///
/// ## Remarks
/// This is always 1000.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const milliseconds_per_second: comptime_int = c.SDL_MS_PER_SECOND;

/// Number of nanoseconds in a microsecond.
///
/// ## Remarks
/// This is always 1000.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const nanoseconds_per_microsecond: comptime_int = c.SDL_NS_PER_US;

/// Number of nanoseconds in a millisecond.
///
/// ## Remarks
/// This is always 1000000.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const nanoseconds_per_millisecond: comptime_int = c.SDL_NS_PER_MS;

/// Number of nanoseconds in a second.
///
/// ## Remarks
/// This is always 1000000000.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const nanoseconds_per_second: comptime_int = c.SDL_NS_PER_SECOND;

/// Function prototype for the millisecond timer callback function.
///
/// ## Function Parameters
/// * `user_data`: An arbitrary pointer provided by the app through `timer.Timer.initMilliseconds()`, for its own use.
/// * `timer`: The current timer being processed.
/// * `interval_milliseconds`: The current callback time interval.
///
/// ## Return Value
/// Returns the new callback time interval, or `0` to disable further runs of the callback.
///
/// ## Remarks
/// The callback function is passed the current timer interval and returns the next timer interval, in milliseconds.
/// If the returned value is the same as the one passed in, the periodic alarm continues, otherwise a new alarm is scheduled.
/// If the callback returns `0`, the periodic alarm is canceled and will be removed.
///
/// ## Thread Safety
/// SDL may call this callback at any time from a background thread;
/// the application is responsible for locking resources the callback touches that need to be protected.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn MillisecondsTimerCallback(
    comptime UserData: type,
) type {
    return *const fn (
        user_data: ?*UserData,
        timer: Timer,
        interval_milliseconds: u32,
    ) u32;
}

/// Function prototype for the nanosecond timer callback function.
///
/// ## Function Parameters
/// * `user_data`: An arbitrary pointer provided by the app through `timer.Timer.initNanoseconds()`, for its own use.
/// * `timer`: The current timer being processed.
/// * `interval_nanoseconds`: The current callback time interval.
///
/// ## Return Value
/// Returns the new callback time interval, or `0` to disable further runs of the callback.
///
/// ## Remarks
/// The callback function is passed the current timer interval and returns the next timer interval, in nanoseconds.
/// If the returned value is the same as the one passed in, the periodic alarm continues, otherwise a new alarm is scheduled.
/// If the callback returns `0`, the periodic alarm is canceled and will be removed.
///
/// ## Thread Safety
/// SDL may call this callback at any time from a background thread;
/// the application is responsible for locking resources the callback touches that need to be protected.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn NanosecondsTimerCallback(
    comptime UserData: type,
) type {
    return *const fn (
        user_data: ?*UserData,
        timer: Timer,
        interval_nanoseconds: u64,
    ) u64;
}

/// Wait a specified number of milliseconds before returning.
///
/// ## Function Parameters
/// * `milliseconds`: The number of milliseconds to delay.
///
/// ## Remarks
/// This function waits a specified number of milliseconds before returning.
/// It waits at least the specified time, but possibly longer due to OS scheduling.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn delayMilliseconds(
    milliseconds: u32,
) void {
    c.SDL_Delay(
        @intCast(milliseconds),
    );
}

/// Wait a specified number of nanoseconds before returning.
///
/// ## Function Parameters
/// * `nanoseconds`: The number of nanoseconds to delay.
///
/// ## Remarks
/// This function waits a specified number of nanoseconds before returning.
/// It waits at least the specified time, but possibly longer due to OS scheduling.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn delayNanoseconds(
    nanoseconds: u64,
) void {
    c.SDL_DelayNS(
        @intCast(nanoseconds),
    );
}

/// Wait a specified number of nanoseconds before returning.
///
/// ## Function Parameters
/// * `nanoseconds`: The number of nanoseconds to delay.
///
/// ## Remarks
/// This function waits a specified number of nanoseconds before returning.
/// It will attempt to wait as close to the requested time as possible, busy waiting if necessary, but could return later due to OS scheduling.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn delayNanosecondsPrecise(
    nanoseconds: u64,
) void {
    c.SDL_DelayNS(
        @intCast(nanoseconds),
    );
}

/// Get the number of milliseconds since SDL library initialization.
///
/// ## Return Value
/// Returns an unsigned 64‑bit integer that represents the number of milliseconds that have elapsed since the SDL library was initialized
/// (typically via a call to `init.init()`).
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
///
/// ## Code Examples
/// ```zig
/// var quit = false;
/// var last_time: u32 = 0;
/// while (!quit) {
///     // Do stuff.
///     // ...
///
///     // Print a report once per second.
///     const current_time = timer.getMillisecondsSinceInit();
///     if (current_time > last_time + 1000) {
///         std.debug.pring("Report\n");
///         last_time = current_time;
///     }
/// }
/// ```
pub fn getMillisecondsSinceInit() u64 {
    const ret = c.SDL_GetTicks();
    return @intCast(ret);
}

/// Get the number of nanoseconds since SDL library initialization.
///
/// ## Return Value
/// Returns an unsigned 64‑bit integer that represents the number of nanoseconds that have elapsed since the SDL library was initialized
/// (typically via a call to `init.init()`).
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getNanosecondsSinceInit() u64 {
    const ret = c.SDL_GetTicksNS();
    return @intCast(ret);
}

/// Get the current value of the high resolution counter.
///
/// ## Return Value
/// Returns the current counter value.
///
/// ## Remarks
/// This function is typically used for profiling.
///
/// The counter values are only meaningful relative to each other.
/// Differences between values can be converted to times by using `timer.getPerformanceFrequency()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPerformanceCounter() u64 {
    const ret = c.SDL_GetPerformanceCounter();
    return @intCast(ret);
}

/// Get the count per second of the high resolution counter.
///
/// ## Return Value
/// Returns a platform-specific count per second.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPerformanceFrequency() u64 {
    const ret = c.SDL_GetPerformanceFrequency();
    return @intCast(ret);
}

/// Convert microseconds to nanoseconds.
///
/// ## Function Parameters
/// * `microseconds`: The number of microseconds to convert.
///
/// ## Return Value
/// Return `microseconds` expressed in nanoseconds.
///
/// ## Remarks
/// This only converts whole numbers, not fractional microseconds.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn microsecondsToNanoseconds(
    microseconds: u64,
) u64 {
    const ret = c.SDL_US_TO_NS(
        microseconds,
    );
    return ret;
}

/// Convert milliseconds to nanoseconds.
///
/// ## Function Parameters
/// * `milliseconds`: The number of milliseconds to convert.
///
/// ## Return Value
/// Return `milliseconds` expressed in nanoseconds.
///
/// ## Remarks
/// This only converts whole numbers, not fractional milliseconds.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn millisecondsToNanoseconds(
    milliseconds: u64,
) u64 {
    const ret = c.SDL_MS_TO_NS(
        milliseconds,
    );
    return ret;
}

/// Convert nanoseconds to microseconds.
///
/// ## Function Parameters
/// * `nanoseconds`: The number of nanoseconds to convert.
///
/// ## Return Value
/// Returns `nanoseconds`, expressed in microseconds.
///
/// ## Remarks
/// This performs floating point division.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn nanosecondsToMicroseconds(
    nanoseconds: f64,
) f64 {
    const ret = c.SDL_NS_TO_US(
        nanoseconds,
    );
    return ret;
}

/// Convert nanoseconds into millseconds.
///
/// ## Function Parameters
/// * `nanoseconds`: The number of nanoseconds to convert.
///
/// ## Return Value
/// Returns nanoseconds, expressed in milliseconds.
///
/// ## Remarks
/// This performs floating point division.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn nanosecondsToMilliseconds(
    nanoseconds: f64,
) f64 {
    const ret = c.SDL_NS_TO_MS(
        nanoseconds,
    );
    return ret;
}

/// Convert nanoseconds into seconds.
///
/// ## Function Parameters
/// * `nanoseconds`: The number of nanoseconds to convert.
///
/// ## Return Value
/// Returns `nanoseconds`, expressed in seconds.
///
/// ## Remarks
/// This performs floating point division.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub fn nanosecondsToSeconds(
    nanoseconds: f64,
) f64 {
    const ret = c.SDL_NS_TO_SECONDS(
        nanoseconds,
    );
    return ret;
}

/// Convert seconds to nanoseconds.
///
/// ## Function Parameters
/// * `seconds`: The number of seconds to convert.
///
/// ## Return Value
/// Returns `seconds`, expressed in nanoseconds.
///
/// ## Remarks
/// This only converts whole numbers, not fractional seconds.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn secondsToNanoseconds(
    seconds: u64,
) u64 {
    const ret = c.SDL_SECONDS_TO_NS(
        seconds,
    );
    return ret;
}

/// Definition of the timer ID type.
pub const Timer = struct {
    value: c.SDL_TimerID,

    /// Create a timer from SDL.
    pub fn fromSdl(val: c.SDL_TimerID) ?Timer {
        if (val == 0)
            return null;
        return .{ .value = val };
    }

    /// Conver the timer to SDL.
    pub fn toSdl(self: ?Timer) c.SDL_TimerID {
        if (self) |val|
            return val.value;
        return 0;
    }

    /// Remove a created timer.
    ///
    /// ## Function Parameters
    /// * `self`: The timer to remove.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Timer,
    ) !void {
        return errors.wrapCallBool(c.SDL_RemoveTimer(self.value));
    }

    /// Call a callback function at a future time in milliseconds.
    ///
    /// ## Function Parameters
    /// * `interval_milliseconds`: The timer delay, in milliseconds, passed to callback.
    /// * `UserData`: Type of user data to use.
    /// * `callback`: The `timer.MillisecondsTimerCallback` function to call when the specified interval elapses.
    /// * `user_data`: A pointer that is passed to callback.
    ///
    /// ## Return Value
    /// Returns a timer.
    ///
    /// ## Remarks
    /// The callback function is passed the current timer interval and the user supplied parameter from the `timer.Timer.initMilliseconds()` call
    /// and should return the next timer interval.
    /// If the value returned from the callback is an error, the timer is canceled and will be removed.
    ///
    /// The callback is run on a separate thread, and for short timeouts can potentially be called before this function returns.
    ///
    /// Timers take into account the amount of time it took to execute the callback.
    /// For example, if the callback took 250 ms to execute and returned 1000 (ms), the timer would only wait another 750 ms before its next iteration.
    ///
    /// Timing may be inexact due to OS scheduling.
    /// Be sure to note the current time with `timer.getNanosecondsSinceInit()` or `timer.getPerformanceCounter()` in case your callback needs to adjust for variances.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initMilliseconds(
        interval_milliseconds: u32,
        comptime UserData: type,
        comptime callback: MillisecondsTimerCallback(UserData),
        user_data: ?*UserData,
    ) !Timer {
        const Cb = struct {
            pub fn run(user_data_c: ?*anyopaque, timer_c: c.SDL_TimerID, interval_milliseconds_c: u32) callconv(.c) u32 {
                return callback(@ptrCast(@alignCast(user_data_c)), .{ .value = timer_c }, interval_milliseconds_c);
            }
        };
        const ret = c.SDL_AddTimer(
            @intCast(interval_milliseconds),
            Cb.run,
            @ptrCast(user_data),
        );
        return Timer{ .value = try errors.wrapCall(c.SDL_TimerID, ret, 0) };
    }

    /// Call a callback function at a future time in milliseconds.
    ///
    /// ## Function Parameters
    /// * `interval_nanoseconds`: The timer delay, in nanoseconds, passed to callback.
    /// * `UserData`: Type of user data to use.
    /// * `callback`: The `timer.NanosecondsTimerCallback` function to call when the specified interval elapses.
    /// * `user_data`: A pointer that is passed to callback.
    ///
    /// ## Return Value
    /// Returns a timer.
    ///
    /// ## Remarks
    /// The callback function is passed the current timer interval and the user supplied parameter from the `timer.Timer.initNanoseconds()` call
    /// and should return the next timer interval.
    /// If the value returned from the callback is an error, the timer is canceled and will be removed.
    ///
    /// The callback is run on a separate thread, and for short timeouts can potentially be called before this function returns.
    ///
    /// Timers take into account the amount of time it took to execute the callback.
    /// For example, if the callback took 250 ns to execute and returned 1000 (ns), the timer would only wait another 750 ns before its next iteration.
    ///
    /// Timing may be inexact due to OS scheduling.
    /// Be sure to note the current time with `timer.getNanosecondsSinceInit()` or `timer.getPerformanceCounter()` in case your callback needs to adjust for variances.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initNanoseconds(
        interval_nanoseconds: u64,
        comptime UserData: type,
        comptime callback: NanosecondsTimerCallback(UserData),
        user_data: ?*UserData,
    ) !Timer {
        const Cb = struct {
            pub fn run(user_data_c: ?*anyopaque, timer_c: c.SDL_TimerID, interval_nanoseconds_c: u64) callconv(.c) u64 {
                return callback(@ptrCast(@alignCast(user_data_c)), .{ .value = timer_c }, interval_nanoseconds_c);
            }
        };
        const ret = c.SDL_AddTimerNS(
            @intCast(interval_nanoseconds),
            Cb.run,
            @ptrCast(user_data),
        );
        return Timer{ .value = try errors.wrapCall(c.SDL_TimerID, ret, 0) };
    }
};

fn dummyMsCallback(
    user_data: ?*void,
    timer: Timer,
    interval_milliseconds: u32,
) u32 {
    _ = user_data;
    _ = timer;
    _ = interval_milliseconds;
    return 0;
}

fn dummyNsCallback(
    user_data: ?*void,
    timer: Timer,
    interval_nanoseconds: u64,
) u64 {
    _ = user_data;
    _ = timer;
    _ = interval_nanoseconds;
    return 0;
}

// Test timer related functionality.
test "Timer" {
    errors.refAllDeclsRecursive(@This());

    comptime try std.testing.expectEqual(milliseconds_per_second, nanoseconds_per_second / millisecondsToNanoseconds(1));
    comptime try std.testing.expectEqual(nanoseconds_per_millisecond, millisecondsToNanoseconds(1));
    comptime try std.testing.expectEqual(nanoseconds_per_second, secondsToNanoseconds(1));
    comptime try std.testing.expectEqual(nanoseconds_per_microsecond, microsecondsToNanoseconds(1));
    comptime try std.testing.expectEqual(microseconds_per_second, nanoseconds_per_second / microsecondsToNanoseconds(1));

    comptime try std.testing.expectEqual(0.001, nanosecondsToMicroseconds(1));
    comptime try std.testing.expectEqual(0.000001, nanosecondsToMilliseconds(1));
    comptime try std.testing.expectEqual(0.000000001, nanosecondsToSeconds(1));

    delayMilliseconds(1);
    delayNanoseconds(1);
    delayNanosecondsPrecise(1);

    _ = getPerformanceCounter() / getPerformanceFrequency();
    _ = getMillisecondsSinceInit();
    _ = getNanosecondsSinceInit();

    _ = try Timer.initNanoseconds(3, void, dummyNsCallback, null);
    const timer = try Timer.initMilliseconds(5000, void, dummyMsCallback, null);
    try timer.deinit();
}
