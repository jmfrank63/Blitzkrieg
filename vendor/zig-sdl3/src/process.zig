const c = @import("c");
const io_stream = @import("io_stream.zig");
const errors = @import("errors.zig");
const properties = @import("properties.zig");
const sdl3 = @import("sdl3.zig");
const std = @import("std");

/// An opaque handle representing a system process.
///
/// This datatype is available since SDL 3.2.0.
pub const Process = extern struct {
    value: *c.SDL_Process,

    /// Description of where standard I/O should be directed when creating a process.
    ///
    /// ## Remarks
    /// If a standard I/O stream is set to `process.IO.Inherited`, it will go to the same place as the application's I/O stream.
    /// This is the default for standard output and standard error.
    ///
    /// If a standard I/O stream is set to `process.IO.Ignored`, it is connected to `NUL` on Windows and `/dev/null` on POSIX systems.
    /// This is the default for standard input.
    ///
    /// If a standard I/O stream is set to `process.IO.App`, it is connected to a new `io_stream.Stream` that is available to the application.
    /// Standard input will be available as `Process.CreateProperties.stdin_redirect` and allows `Process.getInput()`,
    /// standard output will be available as `Process.CreateProperties.stdout_redirect` and allows `Process.read()` and `process.getOutput()`.
    /// and standard error will be available as `Process.CreateProperties.stderr_redirect` in the properties for the created process.
    ///
    /// If a standard I/O stream is set to `Process.Io.redirect`, it is connected to an existing stream provided by the application.
    /// Standard input is provided using `Process.CreateProperties.stdin_redirect`, standard output is provided using `Process.CreateProperties.stdout_redirect`,
    /// and standard error is provided using `Process.CreateProperties.stderr_redirect` in the creation properties.
    /// These existing streams should be closed by the application once the new process is created.
    ///
    /// In order to use a stream with `Process.Io.redirect`, it must have `io_stream.Properties.window_handle` or `io_stream.Properties.file_descriptor` set.
    /// This is true for streams representing files and process I/O.
    ///
    /// ## Version
    /// This enum is available since SDL 3.2.0.
    pub const Io = enum(c.SDL_ProcessIO) {
        /// The I/O stream is inherited from the application.
        inherited = c.SDL_PROCESS_STDIO_INHERITED,
        /// The I/O stream is ignored.
        ignored = c.SDL_PROCESS_STDIO_NULL,
        /// The I/O stream is connected to a new `io_stream.Stream` that the application can read or write.
        app = c.SDL_PROCESS_STDIO_APP,
        /// The I/O stream is redirected to an existing `io_stream.Stream`.
        redirect = c.SDL_PROCESS_STDIO_REDIRECT,
    };

    /// Process creation properties.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub const CreateProperties = struct {
        /// A slice of strings containing the program to run, any arguments, and a `null` pointer.
        args: [:null]const ?[*:0]const u8,
        /// An environment pointer.
        /// If this property is set, it will be the entire environment for the process, otherwise the current environment is used.
        environment: ?sdl3.Environment = null,
        /// A UTF-8 encoded string representing the working directory for the process, defaults to the current working directory.
        working_directory: ?[:0]const u8,
        /// A IO value describing where standard input for the process comes from, defaults to `Process.Io.ignored` if unspecified.
        stdin: ?Io = null,
        /// A stream pointer when `stdin` is set to `Process.Io.redirect`.
        stdin_redirect: ?io_stream.Stream = null,
        /// A IO value describing where standard output for the process comes goes to, defaults to `Process.Io.inherited` if unspecified.
        stdout: ?Io = null,
        /// A stream pointer when `stdout` is set to `Process.Io.redirect`.
        stdout_redirect: ?io_stream.Stream = null,
        /// A IO value describing where standard error for the process comes goes to, defaults to `Process.Io.inherited` if unspecified.
        stderr: ?Io = null,
        /// A stream pointer when `stderr` is set to `Process.Io.redirect`.
        stderr_redirect: ?io_stream.Stream = null,
        /// True if the error output of the process should be redirected into the standard output of the process. This property has no effect if `stderr` is set.
        stderr_to_stdout: ?bool = null,
        /// True if the process should run in the background.
        /// In this case the default input and output is `Process.Io.ignored` and the exitcode of the process is not available, and will always be `0`.
        background: ?bool = null,

        /// Convert to properties.
        pub fn toProperties(self: CreateProperties) !properties.Group {
            const ret = try properties.Group.init();
            try ret.set(c.SDL_PROP_PROCESS_CREATE_ARGS_POINTER, .{ .pointer = @ptrCast(@constCast(self.args.ptr)) });
            if (self.environment) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, .{ .pointer = val.value });
            if (self.working_directory) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, .{ .string = val });
            if (self.stdin) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, .{ .number = @intFromEnum(val) });
            if (self.stdin_redirect) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_STDIN_POINTER, .{ .pointer = val.value });
            if (self.stdout) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, .{ .number = @intFromEnum(val) });
            if (self.stdout_redirect) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, .{ .pointer = val.value });
            if (self.stderr) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, .{ .number = @intFromEnum(val) });
            if (self.stderr_redirect) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_STDERR_POINTER, .{ .pointer = val.value });
            if (self.stderr_to_stdout) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, .{ .boolean = val });
            if (self.background) |val|
                try ret.set(c.SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, .{ .boolean = val });
            return ret;
        }
    };

    /// Process properties.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub const Properties = struct {
        /// The process ID of the process.
        pid: ?i64,
        /// Stream that can be used to write input to the process, if `Process.CreateProperties.stdin` is set to `Process.Io.app`.
        stdin: ?*io_stream.Stream,
        /// Non-blocking stream that can be used to read output from the process, if `Process.CreateProperties.stdout` is set to `Process.Io.app`.
        stdout: ?*io_stream.Stream,
        /// Non-blocking stream that can be used to read error output from the process, if `Process.CreateProperties.stderr` is set to `Process.Io.app`.
        stderr: ?*io_stream.Stream,
        /// True if the process is running in the background.
        background: ?bool,

        /// Convert from an SDL value.
        pub fn fromSdl(value: properties.Group) Properties {
            return .{
                .pid = if (value.get(c.SDL_PROP_PROCESS_PID_NUMBER)) |val| val.number else null,
                .stdin = if (value.get(c.SDL_PROP_PROCESS_STDIN_POINTER)) |val| @ptrCast(@alignCast(val.pointer)) else null,
                .stdout = if (value.get(c.SDL_PROP_PROCESS_STDOUT_POINTER)) |val| @ptrCast(@alignCast(val.pointer)) else null,
                .stderr = if (value.get(c.SDL_PROP_PROCESS_STDERR_POINTER)) |val| @ptrCast(@alignCast(val.pointer)) else null,
                .background = if (value.get(c.SDL_PROP_PROCESS_BACKGROUND_BOOLEAN)) |val| val.boolean else null,
            };
        }
    };

    /// Destroy a previously created process object.
    ///
    /// ## Function Parameters
    /// * `self`: The process object to destroy.
    ///
    /// ## Remarks
    /// Note that this does not stop the process, just destroys the SDL object used to track it.
    /// If you want to stop the process you should use `Process.kill()`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Process,
    ) void {
        c.SDL_DestroyProcess(self.value);
    }

    /// Create a new process.
    ///
    /// ## Function Parameters
    /// * `init`: The path and arguments for the new process.
    /// * `pipe_stdio`: True to create pipes to the process's standard input and from the process's standard output, false for the process to have no input and inherit the application's standard output.
    ///
    /// ## Return Value
    /// Returns the newly created and running process.
    ///
    /// ## Remarks
    /// The path to the executable is supplied in `args[0]`.
    ///
    /// Setting `pipe_stdio` to true is equivalent to setting `Process.CreateProperties.stdin` and `Process.CreateProperties.stdout` to `Process.Io.app`,
    /// and will allow the use of `Process.read()` or `Process.getInput()` and `process.getOutput()`.
    ///
    /// See `Process.initWithProperties()` for more details.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        args: []const [*:0]u8,
        pipe_stdio: bool,
    ) !Process {
        const ret = c.SDL_CreateProcess(
            @ptrCast(args.ptr),
            pipe_stdio,
        );
        return .{ .value = try errors.wrapCallNull(*c.SDL_Process, ret) };
    }

    /// Create a new process with the specified properties.
    ///
    /// ## Function Parameters
    /// * `props`: The properties to use.
    ///
    /// ## Return Value
    /// Returns the newly created and running process, or `null` if the process couldn't be created.
    ///
    /// ## Remarks
    /// On POSIX platforms, `wait()` and `waitpid(-1, ...)` should not be called,
    /// and `SIGCHLD` should not be ignored or handled because those would prevent SDL from properly tracking the lifetime of the underlying process.
    /// You should use `Process.wait()` instead.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initWithProperties(
        props: CreateProperties,
    ) !?Process {
        const vals = try props.toProperties();
        defer vals.deinit();
        const ret = c.SDL_CreateProcessWithProperties(vals.value);
        if (ret) |val| {
            return .{ .value = val };
        }
        return null;
    }

    /// Get the `io_stream.Stream` associated with process standard input.
    ///
    /// ## Function Parameters
    /// * `self`: The process to get the input stream for.
    ///
    /// ## Return Value
    /// Returns the input stream.
    ///
    /// ## Remarks
    /// The process must have been created with `Process.init()` and `pipe_stdio` set to true,
    /// or with `Process.initWithProperties()` and `stdin` set to `Process.Stdio.app`.
    ///
    /// Writing to this stream can return less data than expected if the process hasn't read its input.
    /// It may be blocked waiting for its output to be read, if so you may need to call `Process.getOutput()` and read the output in parallel with writing input.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getInput(
        self: Process,
    ) !io_stream.Stream {
        return .{ .value = try errors.wrapCallNull(*c.SDL_IOStream, c.SDL_GetProcessOutput(self.value)) };
    }

    /// Get the `io_stream.Stream` associated with process standard output.
    ///
    /// ## Function Parameters
    /// * `self`: The process to get the output stream for.
    ///
    /// ## Return Value
    /// Returns the output stream.
    ///
    /// ## Remarks
    /// The process must have been created with `Process.init()` and `pipe_stdio` set to true,
    /// or with `Process.initWithProperties()` and `stdout` set to `Process.Io.app`.
    ///
    /// Reading from this stream can return `null` with `io_stream.Stream.getStatus()` returning `io_stream.Status.not_ready` if no output is available yet.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getOutput(
        self: Process,
    ) !io_stream.Stream {
        return .{ .value = try errors.wrapCallNull(*c.SDL_IOStream, c.SDL_GetProcessOutput(self.value)) };
    }

    /// Get the properties associated with a process.
    ///
    /// ## Function Parameters
    /// * `self`: The process to query.
    ///
    /// ## Return Value
    /// Returns read-only properties.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Process,
    ) !Properties {
        return Properties.fromSdl(.{ .value = try errors.wrapCall(c.SDL_PropertiesID, c.SDL_GetProcessProperties(self.value), 0) });
    }

    /// Stop a process.
    ///
    /// ## Function Parameters
    /// * `self`: The process to stop.
    /// * `force`: True to terminate the process immediately, false to try to stop the process gracefully. In general you should try to stop the process gracefully first as terminating a process may leave it with half-written data or in some other unstable state.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn kill(
        self: Process,
        force: bool,
    ) !void {
        return errors.wrapCallBool(c.SDL_KillProcess(self.value, force));
    }

    /// Read all the output from a process.
    ///
    /// ## Function Parameters
    /// * `self`: The process to read.
    ///
    /// ## Return Value
    /// Returns the data read, must be freed with `free()`.
    /// Also returns the exit code if the process exited.
    ///
    /// ## Remarks
    /// If a process was created with I/O enabled, you can use this function to read the output.
    /// This function blocks until the process is complete, capturing all output, and providing the process exit code.
    ///
    /// The data is allocated with a zero byte at the end (null terminated) for convenience.
    /// This extra byte is not included in the value reported via datasize.
    ///
    /// The data should be freed with `free()`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn read(
        self: Process,
    ) !struct { []u8, c_int } {
        var size: usize = undefined;
        var exit_code: c_int = undefined;
        const ret = c.SDL_ReadProcess(self.value, &size, &exit_code);
        return .{
            @as([*]u8, @ptrCast(@alignCast(try errors.wrapCallNull(*anyopaque, ret))))[0..@intCast(size)],
            exit_code,
        };
    }

    /// Wait for a process to finish.
    ///
    /// ## Function Parameters
    /// * `self`: The process to wait for.
    /// * `block`: If true, block until the process finishes; otherwise, report on the process' status.
    ///
    /// ## Return Value
    /// Returns the process exit code if the process exited, or `null` otherwise.
    pub fn wait(
        self: Process,
        block: bool,
    ) ?c_int {
        var exit_code: c_int = undefined;
        const ret = c.SDL_WaitProcess(self.value, block, &exit_code);
        if (!ret)
            return null;
        return exit_code;
    }
};

// Process creation and such.
test "Process" {
    errors.refAllDeclsRecursive(@This());
}
