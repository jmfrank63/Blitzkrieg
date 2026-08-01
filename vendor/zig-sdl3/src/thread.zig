const c = @import("c");
const errors = @import("errors.zig");
const properties = @import("properties.zig");
const std = @import("std");

/// A unique numeric ID that identifies a thread.
///
/// ## Remarks
/// These are different from `thread.Thread` objects, which are generally what an application will operate on, but having a way to uniquely identify a thread can be useful at times.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Id = packed struct {
    value: c.SDL_ThreadID,
};

/// The SDL thread priority.
///
/// ## Remarks
/// SDL will make system changes as necessary in order to apply the thread priority.
/// Code which attempts to control thread state related to priority should be aware that calling `thread.setCurrentPriority()` may alter such state.
/// `hints.Type.thread_priority_policy` can be used to control aspects of this behavior.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Priority = enum(c.SDL_ThreadPriority) {
    low = c.SDL_THREAD_PRIORITY_LOW,
    normal = c.SDL_THREAD_PRIORITY_NORMAL,
    high = c.SDL_THREAD_PRIORITY_HIGH,
    time_critical = c.SDL_THREAD_PRIORITY_TIME_CRITICAL,
};

/// The SDL thread state.
///
/// ## Remarks
/// The current state of a thread can be checked by calling `thread.Thread.getState()`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const State = enum(c.SDL_ThreadState) {
    /// The thread is currently running.
    alive = c.SDL_THREAD_ALIVE,
    /// The thread is detached and can't be waited on.
    detached = c.SDL_THREAD_DETACHED,
    /// The thread has finished and should be cleaned up with `thread.Thread.wait()`.
    complete = c.SDL_THREAD_COMPLETE,

    /// From and SDL value.
    pub fn fromSdl(value: c.SDL_ThreadState) ?State {
        if (value == c.SDL_THREAD_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?State) c.SDL_ThreadState {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_THREAD_UNKNOWN;
    }
};

/// The SDL thread object.
///
/// ## Remarks
/// These are opaque data.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Thread = extern struct {
    value: *c.SDL_Thread,

    /// Properties to use for thread creation.
    ///
    /// ## Type Parameters
    /// * `UserData`: User data type.
    /// * `entry_function`: A function that will be called at the start of the new thread's life.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub fn InitProperties(
        comptime UserData: type,
        comptime entry_function: ThreadFunction(UserData),
    ) type {
        return struct {
            /// The name of the new thread, which might be available to debuggers.
            name: ?[:0]const u8 = null,
            /// An arbitrary app-defined pointer, which is passed to the entry function on the new thread, as its only parameter.
            user_data: ?*anyopaque = null,
            /// The size, in bytes, of the new thread's stack. Defaults to 0 (system-defined default).
            stack_size: ?usize = null,

            // Callback.
            fn run(user_data_c: ?*anyopaque) callconv(.c) c_int {
                return entry_function(@ptrCast(@alignCast(user_data_c)));
            }

            // Get properties, must free after.
            pub fn toSdl(self: @This()) !properties.Group {
                const ret = try properties.Group.init();
                try ret.set(c.SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, .{ .pointer = @constCast(&@This().run) });
                if (self.name) |val|
                    try ret.set(c.SDL_PROP_THREAD_CREATE_NAME_STRING, .{ .string = val });
                if (self.user_data) |val|
                    try ret.set(c.SDL_PROP_THREAD_CREATE_USERDATA_POINTER, .{ .pointer = val });
                if (self.stack_size) |val|
                    try ret.set(c.SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, .{ .number = @intCast(val) });
                return ret;
            }
        };
    }

    /// Let a thread clean up on exit without intervention.
    ///
    /// ## Function Parameters
    /// * `self`: The thread that was returned from the `thread.Thread.init()` call that started this thread.
    ///
    /// ## Remarks
    /// A thread may be "detached" to signify that it should not remain until another thread has called `thread.Thread.wait()` on it.
    /// Detaching a thread is useful for long-running threads that nothing needs to synchronize with or further manage.
    /// When a detached thread is done, it simply goes away.
    ///
    /// There is no way to recover the return code of a detached thread.
    /// If you need this, don't detach the thread and instead use `thread.Thread.wait()`.
    ///
    /// Once a thread is detached, you should usually assume the `thread.Thread` isn't safe to reference again,
    /// as it will become invalid immediately upon the detached thread's exit, instead of remaining until someone has called `thread.Thread.wait()` to finally clean it up.
    /// As such, don't detach the same thread more than once.
    ///
    /// If a thread has already exited when passed to `thread.Thread.detach()`, it will stop waiting for a call to `thread.Thread.wait()` and clean up immediately.
    /// It is not safe to detach a thread that might be used with `thread.Thread.wait()`.
    ///
    /// You may not call `thread.Thread.wait()` on a thread that has been detached.
    /// Use either that function or this one, but not both, or behavior is undefined.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn detach(
        self: Thread,
    ) void {
        c.SDL_DetachThread(self.value);
    }

    /// Create a new thread with a default stack size.
    ///
    /// ## Function Parameters
    /// * `UserData`: Type of user data.
    /// * `func`: The function to call in the new thread.
    /// * `name`: The name of the thread.
    /// * `data`: A pointer that is passed to `func`.
    ///
    /// ## Return Value
    /// Returns an opaque pointer to the new thread object on success.
    ///
    /// ## Remarks
    /// This is a convenience function, equivalent to calling `thread.Thread.initWithProperties()` with the following properties set:
    /// * `entry_function`: `func`
    /// * `name`: `name`
    /// * `user_data`: `data`
    ///
    /// Note that this "function" is actually a macro that calls an internal function with two extra parameters not listed here;
    /// they are hidden through preprocessor macros and are needed to support various C runtimes at the point of the function call.
    /// Language bindings that aren't using the C headers will need to deal with this.
    ///
    /// Usually, apps should just call this function the same way on every platform and let the macros hide the details.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        comptime UserData: type,
        comptime func: ThreadFunction(UserData),
        name: ?[:0]const u8,
        data: ?*anyopaque,
    ) !Thread {
        const Cb = struct {
            fn run(user_data_c: ?*anyopaque) callconv(.c) c_int {
                return func(@ptrCast(@alignCast(user_data_c)));
            }
        };
        return .{ .value = try errors.wrapCallNull(*c.SDL_Thread, c.SDL_CreateThread(Cb.run, if (name) |val| val.ptr else null, data)) };
    }

    /// Create a new thread with with the specified properties.
    ///
    /// ## Function Parameters
    /// * `UserData`: User data type.
    /// * `entry_function`: The thread entry function.
    /// * `props`: The properties to use.
    ///
    /// ## Return Value
    /// Returns an opaque pointer to the new thread object on success.
    ///
    /// ## Remarks
    /// SDL makes an attempt to report `thread.Thread.InitProperties.name` to the system, so that debuggers can display it.
    /// Not all platforms support this.
    ///
    /// Thread naming is a little complicated: Most systems have very small limits for the string length (Haiku has 32 bytes, Linux currently has 16, Visual C++ 6.0 has nine!),
    /// and possibly other arbitrary rules.
    /// You'll have to see what happens with your system's debugger.
    /// The name should be UTF-8 (but using the naming limits of C identifiers is a better bet).
    /// There are no requirements for thread naming conventions, so long as the string is null-terminated UTF-8, but these guidelines are helpful in choosing a name:
    /// https://stackoverflow.com/questions/149932/naming-conventions-for-threads
    ///
    /// If a system imposes requirements, SDL will try to munge the string for it (truncate, etc), but the original string contents will be available from `thread.Thread.getName()`.
    ///
    /// The size (in bytes) of the new stack can be specified with `thread.Thread.InitProperties.stack_size`.
    /// Zero means "use the system default" which might be wildly different between platforms.
    /// x86 Linux generally defaults to eight megabytes, an embedded device might be a few kilobytes instead.
    /// You generally need to specify a stack that is a multiple of the system's page size (in many cases, this is 4 kilobytes, but check your system documentation).
    ///
    /// Note that this "function" is actually a macro that calls an internal function with two extra parameters not listed here;
    /// they are hidden through preprocessor macros and are needed to support various C runtimes at the point of the function call.
    /// Language bindings that aren't using the C headers will need to deal with this.
    ///
    /// The actual symbol in SDL is `SDL_CreateThreadWithPropertiesRuntime`, so there is no symbol clash,
    /// but trying to load an SDL shared library and look for `SDL_CreateThreadWithProperties` will fail.
    ///
    /// Usually, apps should just call this function the same way on every platform and let the macros hide the details.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initWithProperties(
        comptime UserData: type,
        comptime entry_function: ThreadFunction(UserData),
        props: InitProperties(UserData, entry_function),
    ) !Thread {
        const init_props = try props.toSdl();
        defer init_props.deinit();
        return .{ .value = try errors.wrapCallNull(*c.SDL_Thread, c.SDL_CreateThreadWithProperties(init_props.value)) };
    }

    /// Get the thread identifier for the specified thread.
    ///
    /// ## Function Parameters
    /// * `self`: The thread to query.
    ///
    /// ## Return Value
    /// Returns the ID of the specified thread.
    ///
    /// ## Remarks
    /// This thread identifier is as reported by the underlying operating system.
    /// If SDL is running on a platform that does not support threads the return value will always be zero.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn getId(
        self: Thread,
    ) Id {
        return .{ .value = c.SDL_GetThreadID(self.value) };
    }

    /// Get the thread name as it was specified in `thread.Thread.init()`.
    ///
    /// ## Function Parameters
    /// * `self`: The thread to query.
    ///
    /// ## Return Value
    /// Returns a slice to a UTF-8 string that names the specified thread, or `null` if it doesn't have a name.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Thread,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetThreadName(self.value);
        if (ret) |val|
            return std.mem.span(val);
        return null;
    }

    /// Get the current state of a thread.
    ///
    /// ## Function Parameters
    /// * `self`: The thread to query.
    ///
    /// ## Return Value
    /// Returns the current state of a thread, or `null` if the thread is invalid.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getState(
        self: Thread,
    ) ?State {
        return State.fromSdl(c.SDL_GetThreadState(self.value));
    }

    /// Wait for a thread to finish.
    ///
    /// ## Function Parameters
    /// * `self`: The thread that was returned from the `thread.Thread.init()` call that started this thread.
    ///
    /// ## Return Value
    /// Returns the value returned from the thread or `null` if the thread is detached or isn't valid.
    ///
    /// ## Remarks
    /// Threads that haven't been detached will remain until this function cleans them up.
    /// Not doing so is a resource leak.
    ///
    /// Once a thread has been cleaned up through this function, the `thread.Thread` that references it becomes invalid and should not be referenced again.
    /// As such, only one thread may call `thread.Thread.wait()` on another.
    ///
    /// You may not wait on a thread that has been used in a call to `thread.Thread.detach()`.
    /// Use either that function or this one, but not both, or behavior is undefined.
    ///
    /// Note that the thread is freed by this function and is not valid afterward.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn wait(
        self: Thread,
    ) ?c_int {
        var status: c_int = undefined;
        c.SDL_WaitThread(self.value, &status);
        if (status == -1)
            return null;
        return status;
    }
};

/// The function passed to `thread.Thread.init()` as the new thread's entry point.
///
/// ## Function Parameters
/// * `user_data`: What was passed as data to `thread.Thread.init()`.
///
/// ## Return Value
/// Returns a value that can be reported through `thread.Thread.wait()`.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn ThreadFunction(
    comptime UserData: type,
) type {
    return *const fn (
        user_data: ?*UserData,
    ) c_int;
}

/// The callback used to cleanup data passed to `thread.TlsId.set()`.
///
/// ## Function Parameters
/// * `value`: A pointer previously handed to `thread.TlsId.set()`.
///
/// ## Remarks
/// This is called when a thread exits, to allow an app to free any resources.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn TlsDestructorCallback(
    comptime ValueType: type,
) type {
    return *const fn (
        value: ?*ValueType,
    ) void;
}

/// Thread local storage ID.
///
/// ## Remarks
/// An app can create these and then set data for these IDs that is unique to each thread.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const TlsId = struct {
    value: c.SDL_TLSID,

    /// Initialize thread local storage.
    ///
    /// ## Return Value
    /// Returns a new thread local storage object.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init() TlsId {
        return .{ .value = .{} };
    }

    /// Get the current thread's value associated with a thread local storage ID.
    ///
    /// ## Function Parameters
    /// * `self`: The thread local storage ID.
    ///
    /// ## Return Value
    /// Returns the value associated with the ID for the current thread or errors if no value has been set.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn get(
        self: *TlsId,
    ) !*anyopaque {
        return errors.wrapCallNull(*anyopaque, c.SDL_GetTLS(&self.value));
    }

    /// Set the current thread's value associated with a thread local storage ID.
    ///
    /// ## Function Parameters
    /// * `self`: The thread local storage ID.
    /// * `value`: The value to associate with the ID for the current thread.
    /// * `destructor`: A function called when the thread exits, to free the value, may be `null`.
    ///
    /// ## Remarks
    /// If the thread local storage ID is not initialized, a new ID will be created in a thread-safe way,
    /// so all calls using a pointer to the same ID will refer to the same local storage.
    ///
    /// Note that replacing a value from a previous call to this function on the same thread does not call the previous value's destructor!
    ///
    /// If `destructor` is `null` it is assumed that value does not need to be cleaned up.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn set(
        self: *TlsId,
        comptime ValueType: type,
        value: ?*const ValueType,
        comptime destructor: ?TlsDestructorCallback(ValueType),
    ) !void {
        const Cb = struct {
            pub fn run(value_c: ?*anyopaque) callconv(.c) void {
                destructor.?(@ptrCast(@alignCast(value_c)));
            }
        };
        return errors.wrapCallBool(c.SDL_SetTLS(&self.value, value, if (destructor != null) Cb.run else null));
    }
};

/// Cleanup all TLS data for this thread.
///
/// ## Remarks
/// If you are creating your threads outside of SDL and then calling SDL functions, you should call this function before your thread exits, to properly clean up SDL memory.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn cleanupTls() void {
    c.SDL_CleanupTLS();
}

/// Get the thread identifier for the current thread.
///
/// ## Return Value
/// Returns the ID of the current thread.
///
/// ## Remarks
/// This thread identifier is as reported by the underlying operating system. If SDL is running on a platform that does not support threads the return value will always be zero.
///
/// This function also returns a valid thread ID when called from the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getCurrentId() Id {
    return .{ .value = c.SDL_GetCurrentThreadID() };
}

/// Set the priority for the current thread.
///
/// ## Function Parameters
/// * `priority`: The priority to set.
///
/// ## Remarks
/// Note that some platforms will not let you alter the priority (or at least, promote the thread to a higher priority) at all,
/// and some require you to be an administrator account.
/// Be prepared for this to fail.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setCurrentPriority(
    priority: Priority,
) !void {
    return errors.wrapCallBool(c.SDL_SetCurrentThreadPriority(@intFromEnum(priority)));
}

fn threadFunc(user_data: ?*void) c_int {
    _ = user_data;
    return 3;
}

fn tlsDestructor(value: ?*void) void {
    _ = value;
}

// Thread testing.
test "Thread" {
    errors.refAllDeclsRecursive(@This());

    const t1 = try Thread.init(void, threadFunc, "Test", null);
    try std.testing.expectEqualStrings("Test", t1.getName().?);
    _ = t1.getId();
    _ = t1.getState();
    try std.testing.expectEqual(3, t1.wait());

    const t2 = try Thread.initWithProperties(void, threadFunc, .{});
    t2.detach();

    var id = TlsId.init();
    _ = id.get() catch {};
    id.set(void, null, null) catch {};

    setCurrentPriority(.low) catch {};
    _ = getCurrentId();
    cleanupTls();
}
