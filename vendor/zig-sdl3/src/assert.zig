const c = @import("c");
const errors = @import("errors.zig");
const sdl3 = @import("sdl3.zig");
const std = @import("std");

/// Information about an assertion failure.
///
/// ## Remarks
/// This structure is filled in with information about a triggered assertion, used by the assertion handler, then added to the assertion report.
/// This is returned as a linked list from `getReport`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const AssertData = extern struct {
    /// True if app should always continue when assertion is triggered.
    always_ignore: bool = false,
    /// Number of times this assertion has been triggered.
    trigger_count: c_uint = 0,
    /// A string of this assert's test code.
    condition: ?[*:0]const u8 = null,
    /// The source file where this assert lives.
    filename: ?[*:0]const u8 = null,
    /// The line in `filename` where this assert lives.
    linenum: c_int = 0,
    /// The name of the function where this assert lives.
    function: ?[*:0]const u8 = null,
    /// Next item in the linked list.
    next: ?*const AssertData = null,

    // Test equality.
    comptime {
        errors.assertStructsEqual(AssertData, c.SDL_AssertData);
    }
};

/// A callback that fires when an SDL assertion fails.
///
/// ## Function Parameters
/// * `assert_data`: Assert data structure corresponding to the current assertion.
/// * `user_data`: What was passed as userdata to `setHandler`.
///
/// ## Return Value
/// Returns an assertion state value indicating how to handle the failure.
///
/// ## Thread Safety
/// This callback may be called from any thread that triggers an assert at any time.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn Handler(comptime UserData: type) type {
    return *const fn (
        assert_data: AssertData,
        user_data: ?*UserData,
    ) State;
}

/// A C callback that fires when an SDL assertion fails.
///
/// ## Function Parameters
/// * `assert_data`: A pointer to the `c.SDL_AssertData` structure corresponding to the current assertion.
/// * `user_data`: What was passed as userdata to `setHandler`.
///
/// ## Return Value
/// Returns a `c.SDL_AssertState` value indicating how to handle the failure.
///
/// ## Thread Safety
/// This callback may be called from any thread that triggers an assert at any time.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const HandlerC = *const fn (
    assert_data: [*c]const c.SDL_AssertData,
    user_data: ?*anyopaque,
) callconv(.c) c.SDL_AssertState;

/// Possible outcomes from a triggered assertion.
///
/// ## Remarks
/// When an enabled assertion triggers, it may call the assertion handler (possibly one provided by the app via `setHandler`,
/// which will return one of these values, possibly after asking the user.
///
/// Then SDL will respond based on this outcome (loop around to retry the condition, try to break in a debugger, kill the program, or ignore the problem).
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const State = enum(c.SDL_AssertState) {
    /// Retry the assert immediately.
    retry = c.SDL_ASSERTION_RETRY,
    /// Make the debugger trigger a breakpoint.
    breakpoint = c.SDL_ASSERTION_BREAK,
    /// Terminate the program.
    abort = c.SDL_ASSERTION_ABORT,
    /// Ignore the assert.
    ignore = c.SDL_ASSERTION_IGNORE,
    /// Ignore the assert from now on.
    always_ignore = c.SDL_ASSERTION_ALWAYS_IGNORE,
};

/// An assertion test that is normally performed only in debug builds.
///
/// ## Function Parameters
/// * `condition`: Boolean value to test.
/// * `condition_name`: Name of the condition to test.
/// * `location`: Where the assertion occurs.
///
/// ## Return Value
/// Returns whether or not to try the assertion again.
///
/// ## Remarks
/// This macro is enabled when the `c.SDL_ASSERT_LEVEL` is >= 2, otherwise it is disabled.
/// This is meant to only do these tests in debug builds, so they can tend to be more expensive, and they are meant to bring everything to a halt when they fail,
/// with the programmer there to assess the problem.
///
/// In short: you can sprinkle these around liberally and assume they will evaporate out of the build when building for end-users.
///
/// One can set the environment variable "SDL_ASSERT" to one of several strings ("abort", "break", "retry", "ignore", "always_ignore") to force a default behavior,
/// which may be desirable for automation purposes.
/// If your platform requires GUI interfaces to happen on the main thread but you're debugging an assertion in a background thread,
/// it might be desirable to set this to "break" so that your debugger takes control as soon as assert is triggered,
/// instead of risking a bad UI interaction (deadlock, etc) in the application.
///
/// ## Thread Safety
/// It is safe to call this macro from any thread.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub fn assert(
    condition: bool,
    comptime condition_name: ?[*:0]const u8,
    comptime location: std.builtin.SourceLocation,
) bool {
    if (c.SDL_ASSERT_LEVEL >= 2) {
        return assertAlways(condition, condition_name, location);
    }
    return false;
}

/// The assertion is always enabled no matter what `c.SDL_ASSERT_LEVEL` is set to.
///
/// ## Function Parameters
/// * `condition`: Boolean value to test.
/// * `condition_name`: Name of the condition to test.
/// * `location`: Where the assertion occurs.
///
/// ## Return Value
/// Returns whether or not to try the assertion again.
///
/// ## Remarks
/// You almost never want to use this, as it could trigger on an end-user's system, crashing your program.
/// One can set the environment variable "SDL_ASSERT" to one of several strings ("abort", "break", "retry", "ignore", "always_ignore") to force a default behavior,
/// which may be desirable for automation purposes.
/// If your platform requires GUI interfaces to happen on the main thread but you're debugging an assertion in a background thread,
/// it might be desirable to set this to "break" so that your debugger takes control as soon as assert is triggered,
/// instead of risking a bad UI interaction (deadlock, etc) in the application.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn assertAlways(
    condition: bool,
    comptime condition_name: ?[*:0]const u8,
    comptime location: std.builtin.SourceLocation,
) bool {
    const AssertDataHolder = getAssertData(condition_name, location);
    if (!condition) {
        const report_response = report(&AssertDataHolder.assert_data, location);
        switch (report_response) {
            .retry => return true,
            .breakpoint => triggerBreakpoint(),
            else => {},
        }
        return false;
    }
    return false;
}

/// An assertion test that is performed only when built with paranoid settings.
///
/// ## Function Parameters
/// * `condition`: Boolean value to test.
/// * `condition_name`: Name of the condition to test.
/// * `location`: Where the assertion occurs.
///
/// ## Return Value
/// Returns whether or not to try the assertion again.
///
/// ## Remarks
/// This macro is enabled when the `c.SDL_ASSERT_LEVEL` is >= 3, otherwise it is disabled.
/// This is a higher level than both release and debug,
/// so these tests are meant to be expensive and only run when specifically looking for extremely unexpected failure cases in a special build.
///
/// One can set the environment variable "SDL_ASSERT" to one of several strings ("abort", "break", "retry", "ignore", "always_ignore") to force a default behavior,
/// which may be desirable for automation purposes.
/// If your platform requires GUI interfaces to happen on the main thread but you're debugging an assertion in a background thread,
/// it might be desirable to set this to "break" so that your debugger takes control as soon as assert is triggered,
/// instead of risking a bad UI interaction (deadlock, etc) in the application.
///
/// ## Thread Safety
/// It is safe to call this macro from any thread.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub fn assertParanoid(
    condition: bool,
    comptime condition_name: ?[*:0]const u8,
    comptime location: std.builtin.SourceLocation,
) bool {
    if (c.SDL_ASSERT_LEVEL >= 1) {
        return assertAlways(condition, condition_name, location);
    }
    return false;
}

/// An assertion test that is performed even in release builds.
///
/// ## Function Parameters
/// * `condition`: Boolean value to test.
/// * `condition_name`: Name of the condition to test.
/// * `location`: Where the assertion occurs.
///
/// ## Return Value
/// Returns whether or not to try the assertion again.
///
/// ## Remarks
/// This macro is enabled when the `c.SDL_ASSERT_LEVEL` is >= 1, otherwise it is disabled.
/// This is meant to be for tests that are cheap to make and extremely unlikely to fail; generally it is frowned upon to have an assertion failure in a release build,
/// so these assertions generally need to be of more than life-and-death importance if there's a chance they might trigger.
/// You should almost always consider handling these cases more gracefully than an assert allows.
///
/// One can set the environment variable "SDL_ASSERT" to one of several strings ("abort", "break", "retry", "ignore", "always_ignore") to force a default behavior,
/// which may be desirable for automation purposes.
/// If your platform requires GUI interfaces to happen on the main thread but you're debugging an assertion in a background thread,
/// it might be desirable to set this to "break" so that your debugger takes control as soon as assert is triggered,
/// instead of risking a bad UI interaction (deadlock, etc) in the application.
///
/// ## Thread Safety
/// It is safe to call this macro from any thread.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub fn assertRelease(
    condition: bool,
    comptime condition_name: ?[*:0]const u8,
    comptime location: std.builtin.SourceLocation,
) bool {
    if (c.SDL_ASSERT_LEVEL >= 1) {
        return assertAlways(condition, condition_name, location);
    }
    return false;
}

inline fn getAssertData(
    comptime condition_name: ?[*:0]const u8,
    comptime location: std.builtin.SourceLocation,
) type {
    return struct {
        const source_location: std.builtin.SourceLocation = location;
        var assert_data: AssertData = .{
            .condition = condition_name,
        };
    };
}

/// Get the default assertion handler.
///
/// ## Return Value
/// Returns the default `Handler` that is called when an assert triggers.
///
/// ## Remarks
/// This returns the function pointer that is called by default when an assertion is triggered.
/// This is an internal function provided by SDL, that is used for assertions when `setHandler` hasn't been used to provide a different function.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDefaultHandler() HandlerC {
    return c.SDL_GetDefaultAssertionHandler().?;
}

/// Get the current assertion handler.
///
/// ## Return Value
/// Returns the current assertion handler and the `user_data` associated with it.
///
/// ## Remarks
/// This returns the function pointer that is called when an assertion is triggered.
/// This is either the value last passed to `setHandler`, or if no application-specified function is set,
/// is equivalent to calling `getDefaultHandler`.
///
/// The `user_data` was passed to `setHandler`.
/// This value will always be `null` for the default handler.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getHandler() struct { HandlerC, ?*anyopaque } {
    var user_data: ?*anyopaque = undefined;
    const handler = c.SDL_GetAssertionHandler(&user_data).?;
    return .{ handler, user_data };
}

/// Get a list of all assertion failures.
///
/// ## Return Value
/// Returns a list of all failed assertions or `null` if the list is empty.
/// This memory should not be modified or freed by the application.
/// This pointer remains valid until the next call to `sdl3.shutdown` or `resetReport`.
///
/// ## Remarks
/// This function gets all assertions triggered since the last call to `resetReport`, or the start of the program.
///
/// The proper way to examine this data looks something like this:
/// ```zig
/// const report = sdl3.assert.getReport();
/// var item: ?*const sdl3.assert.AssertData = if (report) |val| &val else null;
/// while (item) |val| {
///     std.debug.print("'{s}', {s} ({s}:{d}), triggered {d} times, always ignore: {s}\n", .{
///         val.condition orelse "[Unknown Condition]",
///         val.function orelse "[Unknown Function]",
///         val.filename orelse "[Unknown Filename]",
///         val.linenum,
///         val.trigger_count,
///         if (val.always_ignore) "yes" else "no",
///     });
///     item = val.next;
/// }
/// ```
///
/// ## Thread Safety
/// This function is not thread safe. Other threads calling `resetReport` simultaneously, may render the returned pointer invalid.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getReport() ?AssertData {
    const ret = c.SDL_GetAssertionReport();
    if (ret) |val|
        return @bitCast(val.*);
    return null;
}

/// Report an assertion.
///
/// ## Function Parameters
/// * `data`: Assert data structure. Should be unique for this call.
/// * `location`: Source location from `@src()`.
///
/// ## Return Value
/// Returns assert state.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn report(
    data: *AssertData,
    location: std.builtin.SourceLocation,
) State {
    return @enumFromInt(c.SDL_ReportAssertion(
        @ptrCast(data),
        location.fn_name,
        location.file,
        @intCast(location.line),
    ));
}

/// Clear the list of all assertion failures.
///
/// ## Remarks
/// This function will clear the list of all assertions triggered up to that point.
/// Immediately following this call, `getReport` will return no items.
/// In addition, any previously-triggered assertions will be reset to a `trigger_count` of zero, and their `always_ignore` state will be `false`.
///
/// ## Thread Safety
/// This function is not thread safe.
/// Other threads triggering an assertion, or simultaneously calling this function may cause memory leaks or crashes.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn resetReport() void {
    c.SDL_ResetAssertionReport();
}

/// Set an application-defined assertion handler.
///
/// ## Function Parameters
/// * `UserData`: Type of user data.
/// * `handler`: The `Handler` function to call when an assertion fails or `null` for the default handler.
/// * `user_data`: A pointer that is passed to handler.
///
/// ## Remarks
/// This function allows an application to show its own assertion UI and/or force the response to an assertion failure.
/// If the application doesn't provide this, SDL will try to do the right thing, popping up a system-specific GUI dialog, and probably minimizing any fullscreen windows.
///
/// This callback may fire from any thread, but it runs wrapped in a mutex, so it will only fire from one thread at a time.
///
/// This callback is NOT reset to SDL's internal handler upon `sdl3.shutdown`!
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setHandler(
    comptime UserData: type,
    comptime handler: ?Handler(UserData),
    user_data: ?*UserData,
) void {
    const Cb = struct {
        pub fn run(assert_data_c: [*c]const c.SDL_AssertData, user_data_c: ?*anyopaque) callconv(.c) c.SDL_AssertState {
            return @intFromEnum(handler.?(@bitCast(assert_data_c.*), @ptrCast(@alignCast(user_data_c))));
        }
    };
    c.SDL_SetAssertionHandler(if (handler != null) Cb.run else null, @ptrCast(user_data));
}

/// Attempt to tell an attached debugger to pause.
///
/// Remarks
/// This allows an app to programmatically halt ("break") the debugger as if it had hit a breakpoint, allowing the developer to examine program state, etc.
///
/// If the program is not running under a debugger, this will likely terminate the app, possibly without warning.
/// This is the same as zig's `@breakpoint()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub inline fn triggerBreakpoint() void {
    @breakpoint();
}

const TestHandlerCallbackData = struct {
    last_data: ?AssertData = null,
};

fn testAssertCallback(assert_data: AssertData, user_data: ?*TestHandlerCallbackData) State {
    user_data.?.last_data = assert_data;
    return .ignore;
}

// Test asserting functionality.
test "Assert" {
    errors.refAllDeclsRecursive(@This());

    const handler, const user_data = getHandler();
    try std.testing.expectEqual(getDefaultHandler(), handler);
    try std.testing.expectEqual(null, user_data);

    var data = TestHandlerCallbackData{};
    setHandler(TestHandlerCallbackData, testAssertCallback, &data);

    try std.testing.expectEqual(null, getReport());

    var assert_data1 = AssertData{};
    var assert_data2 = AssertData{};
    _ = report(&assert_data1, @src());
    _ = report(&assert_data2, @src());
    _ = assertAlways(false, "a", @src());
    _ = assertAlways(false, "b", @src());

    const report4 = getReport().?;
    const report3 = report4.next.?;
    const report2 = report3.next.?;
    const report1 = report2.next.?;
    try std.testing.expectEqual(null, report1.next);
    try std.testing.expectEqual(464, report1.linenum);
    try std.testing.expectEqualStrings("test.Assert", std.mem.span(report1.function.?));
    try std.testing.expectEqualStrings("assert.zig", std.mem.span(report1.filename.?));
    try std.testing.expectEqual(465, report2.linenum);
    try std.testing.expectEqualStrings("test.Assert", std.mem.span(report2.function.?));
    try std.testing.expectEqualStrings("assert.zig", std.mem.span(report2.filename.?));
    try std.testing.expectEqual(466, report3.linenum);
    try std.testing.expectEqualStrings("a", std.mem.span(report3.condition.?));
    try std.testing.expectEqualStrings("b", std.mem.span(report4.condition.?));

    resetReport();
    try std.testing.expectEqual(null, getReport());

    setHandler(void, null, null);
    try std.testing.expectEqual(getDefaultHandler(), getHandler()[0]);
}
