const builtin = @import("builtin");
const c = @import("c");
const extras = @import("extras.zig");
const std = @import("std");

/// Callback for when an SDL error occurs.
///
/// This is per-thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub threadlocal var error_callback: ?*const fn (
    err: ?[:0]const u8,
) void = extras.error_handlers.sdlLog;

/// An SDL error.
pub const Error = error{
    SdlError,
};

/// Ensure two structs are equal in memory.
///
/// ## Function Parameters
/// * `A`: First struct.
/// * `B`: Second struct.
///
/// ## Version
/// This is provided by zig-sdl3.
pub inline fn assertStructsEqual(
    comptime A: anytype,
    comptime B: anytype,
) void {
    comptime {
        std.debug.assert(@sizeOf(A) == @sizeOf(B));
        const info_a = @typeInfo(A).@"struct";
        const info_b = @typeInfo(B).@"struct";
        const num_fields = info_a.fields.len;
        std.debug.assert(info_b.fields.len == num_fields);
        for (0..num_fields) |ind| {
            std.debug.assert(@offsetOf(A, info_a.fields[ind].name) == @offsetOf(B, info_b.fields[ind].name));
            std.debug.assert(@sizeOf(@FieldType(A, info_a.fields[ind].name)) == @sizeOf(@FieldType(B, info_b.fields[ind].name)));
        }
    }
}

/// Call the thread-local user-provided error callback function.
///
/// ## Remarks
/// You probably want to set `error_callback` for this to do anything.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn callErrorCallback() void {
    if (@This().error_callback) |cb| {
        cb(get());
    }
}

/// Clear any previous error message for this thread.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn clear() void {
    _ = c.SDL_ClearError();
}

/// Standardize error reporting on unsupported operations.
///
/// ## Function Parameters
/// * `err`: Error to report.
///
/// ## Remarks
/// This simply calls `errors.set()` with a standardized error string, for convenience, consistency, and clarity.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn invalidParamError(
    err: [:0]const u8,
) !void {
    _ = c.SDL_InvalidParamError(err.ptr);
    return error.SdlError;
}

/// Returns a message with information about the specific error that occurred,
/// or null if there hasn't been an error message set since the last call to `errors.clear()`.
///
/// ## Return Value
/// The last error reported from SDL.
///
/// ## Remarks
///
/// It is possible for multiple errors to occur before calling `errors.get()`.
/// Only the last error is returned.
///
/// The standard way of getting errors is replaced with the zig method.
/// Rely on standard zig error handling, and use the `errors.error_callback` in case callbacks for when an error is encountered.
///
/// SDL will not clear the error string for successful API calls.
/// You must check return values for failure cases before you can assume the error string applies.
///
/// Error strings are set per-thread, so an error set in a different thread will not interfere with the current thread's operation.
///
/// The returned value is a thread-local string which will remain valid until the current thread's error string is changed.
/// The caller should make a copy if the value is needed after the next SDL API call.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn get() ?[:0]const u8 {
    const ret = c.SDL_GetError();
    const converted_ret = std.mem.span(ret);
    if (std.mem.eql(u8, converted_ret, ""))
        return null;
    return converted_ret;
}

/// Given a type, recursively references all the declarations inside, so that the semantic analyzer sees them.
/// For deep types, you may use `@setEvalBranchQuota`.
pub fn refAllDeclsRecursive(comptime T: type) void {
    if (!builtin.is_test) return;
    inline for (comptime std.meta.declarations(T)) |decl| {
        if (@TypeOf(@field(T, decl.name)) == type) {
            switch (@typeInfo(@field(T, decl.name))) {
                .@"struct", .@"enum", .@"union", .@"opaque" => refAllDeclsRecursive(@field(T, decl.name)),
                else => {},
            }
        }
        _ = &@field(T, decl.name);
    }
}

/// Set the SDL error message for the current thread.
///
/// ## Function Parameters
/// * `err`: New error to set.
///
/// ## Remarks
/// Calling this function will replace any previous error message that was set.
///
/// This function will always return an error.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn set(
    err: [:0]const u8,
) !void {
    _ = c.SDL_SetError(
        "%s",
        err.ptr,
    );
    return error.SdlError;
}

/// Set an error indicating that memory allocation failed.
///
/// ## Remarks
/// This will always return an error.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn signalOutOfMemory() !void {
    _ = c.SDL_OutOfMemory();
    return error.SdlError;
}

/// Standardize error reporting on unsupported operations.
///
/// ## Remarks
/// This simply calls `errors.set()` with a standardized error string, for convenience, consistency, and clarity.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn unsupported() !void {
    _ = c.SDL_Unsupported();
    return error.SdlError;
}

/// Wrap an SDL call with an error check.
///
/// ## Function Parameters
/// * `Result`: Resulting type expected from the SDL call.
/// * `result`: Value returned from the SDL call.
/// * `error_condition`: If `result` matches this, then an error will be returned.
///
/// ## Remarks
/// If the result of the call matches the error_condition, call the error callback and return an error.
/// If the result does not match, then return the result.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn wrapCall(
    comptime Result: type,
    result: Result,
    error_condition: Result,
) !Result {
    if (result != error_condition)
        return result;
    callErrorCallback();
    return error.SdlError;
}

/// Wrap an SDL call that returns a success with an error check.
///
/// ## Function Parameters
/// * `result`: Boolean that will result in an error if `false`.
///
/// ## Return Value
/// Returns an error if the result is false, otherwise returns void.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn wrapCallBool(
    result: bool,
) !void {
    _ = try wrapCall(bool, result, false);
}

/// Unwrap a C pointer.
///
// ## Function Parameters
/// * `Result`: Return value type that is pointed to by a C pointer.
/// * `result`: Return value that if `null` will return an error.
///
/// ## Return Value
/// Returns a pointer to an array of values.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn wrapCallCPtr(
    comptime Result: type,
    result: [*c]Result,
) ![*]Result {
    if (result) |val|
        return val;
    callErrorCallback();
    return error.SdlError;
}

/// Unwrap a C pointer to a constant.
///
/// ## Function Parameters
/// * `Result`: Return value type that is pointed to by a constant C pointer.
/// * `result`: Return value that if `null` will return an error.
///
/// ## Return Value
/// Returns a pointer to an array of constant values.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn wrapCallCPtrConst(
    comptime Result: type,
    result: [*c]const Result,
) ![*]const Result {
    if (result) |val|
        return val;
    callErrorCallback();
    return error.SdlError;
}

/// Unwrap a C string.
///
/// ## Function Parameters
/// * `result`: Raw C pointer to a string. If this is `null`, an error is returned.
///
/// ## Return Value
/// Returns an unwrapped C string.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn wrapCallCString(
    result: [*c]const u8,
) ![:0]const u8 {
    if (result != null)
        return std.mem.span(result);
    callErrorCallback();
    return error.SdlError;
}

/// Unwrap a C string that is not constant.
///
/// ## Function Parameters
/// * `result`: Raw C pointer to a string. If this is `null`, an error is returned.
///
/// ## Return Value
/// Returns an unwrapped C string.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn wrapCallCStringMut(
    result: [*c]u8,
) ![:0]u8 {
    if (result != null)
        return std.mem.span(result);
    callErrorCallback();
    return error.SdlError;
}

/// Wrap an SDL call that returns success if not null.
///
/// ## Function Parameters
/// * `Result`: Type that is nullable that has been returned by SDL.
/// * `result`: Actual result value from SDL that will result in an error if `null`.
///
/// ## Return Value
/// Returns an error if the result is `null`, otherwise unwraps it.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn wrapCallNull(
    comptime Result: type,
    result: ?Result,
) !Result {
    if (result) |val|
        return val;
    callErrorCallback();
    return error.SdlError;
}

// Make sure error getting and setting works properly.
test "Error" {
    refAllDeclsRecursive(@This());

    clear();
    callErrorCallback();
    try std.testing.expectEqual(null, get());
    try std.testing.expectError(error.SdlError, invalidParamError("Hello world"));
    try std.testing.expectEqualStrings("Parameter 'Hello world' is invalid", get().?);
    try std.testing.expectError(error.SdlError, signalOutOfMemory());
    try std.testing.expectEqualStrings("Out of memory", get().?);
    try std.testing.expectError(error.SdlError, set("Hello world"));
    try std.testing.expectEqualStrings("Hello world", get().?);
    try std.testing.expectError(error.SdlError, unsupported());
    try std.testing.expectEqualStrings("That operation is not supported", get().?);

    const val1: u8 = 5;
    var val2: u8 = 7;
    const c_ptr1: [*c]const u8 = &val1;
    const c_ptr2: [*c]u8 = &val2;
    try std.testing.expectEqual(@as([*]const u8, @ptrCast(&val1)), try wrapCallCPtrConst(u8, c_ptr1));
    try std.testing.expectError(error.SdlError, wrapCallCPtrConst(u8, null));
    try std.testing.expectEqual(@as([*]u8, @ptrCast(&val2)), try wrapCallCPtr(u8, c_ptr2));
    try std.testing.expectError(error.SdlError, wrapCallCPtr(u8, null));

    const c_str: [*c]const u8 = "C string unwrap test";
    try std.testing.expectEqualStrings("C string unwrap test", try wrapCallCString(c_str));
    try std.testing.expectError(error.SdlError, wrapCallCString(null));

    var c_str_mut: [*c]u8 = null;
    c_str_mut = @constCast("C mut string unwrap test");
    try std.testing.expectEqualStrings("C mut string unwrap test", try wrapCallCStringMut(c_str_mut));
    try std.testing.expectError(error.SdlError, wrapCallCStringMut(null));

    try std.testing.expectEqual(0, try wrapCall(u8, 0, 1));
    try std.testing.expectError(error.SdlError, wrapCall(u8, 1, 1));

    try wrapCallBool(true);
    try std.testing.expectError(error.SdlError, wrapCallBool(false));

    _ = try wrapCallNull(i32, 3);
    try std.testing.expectError(error.SdlError, wrapCallNull(i32, null));

    clear();
    try std.testing.expectEqual(null, get());
}
