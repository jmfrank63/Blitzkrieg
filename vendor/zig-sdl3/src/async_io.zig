const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");
const thread = @import("thread.zig");

/// The asynchronous I/O operation structure.
///
/// ## Remarks
/// This operates as an opaque handle.
/// One can then request read or write operations on it.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const File = struct {
    value: *c.SDL_AsyncIO,

    /// Use this function to create a new IO object for reading from and/or writing to a named file.
    ///
    /// ## Function Parameters
    /// * `file`: A UTF-8 string representing the filename to open.
    /// * `mode`: Mode used for opening the file.
    ///
    /// ## Return Value
    /// Returns an IO file.
    /// This is to be closed with `Queue.closeFile`.
    ///
    /// ## Remarks
    /// This function supports Unicode filenames, but they must be encoded in UTF-8 format, regardless of the underlying operating system.
    ///
    /// This call is not asynchronous; it will open the file before returning, under the assumption that doing so is generally a fast operation.
    /// Future reads and writes to the opened file will be async, however.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        file: [:0]const u8,
        mode: IoMode,
    ) !File {
        return .{ .value = try errors.wrapCallNull(*c.SDL_AsyncIO, c.SDL_AsyncIOFromFile(
            file.ptr,
            switch (mode) {
                .read_only => "r",
                .write_only => "w",
                .read_write_update => "r+",
                .read_write_replace => "w+",
            },
        )) };
    }

    /// Use this function to get the size of the data stream.
    ///
    /// ## Function Parameters
    /// * `self`: The file to get the size of the data stream from.
    ///
    /// ## Return Value
    /// Returns the size of the data stream.
    ///
    /// ## Remarks
    /// This call is *not* asynchronous; it assumes that obtaining this info is a non-blocking operation in most reasonable cases.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSize(
        self: File,
    ) !usize {
        return @intCast(try errors.wrapCall(i64, c.SDL_GetAsyncIOSize(self.value), -1));
    }
};

/// Mode for opening an async IO file.
///
/// ## Version
/// This enum is provided by zig-sdl3.
pub const IoMode = enum {
    /// Mode "r".
    /// Open a file for reading only.
    /// It must exist.
    read_only,
    /// Mode "w".
    /// Open a file for writing only.
    /// It will create missing files or truncate existing ones.
    write_only,
    /// Mode "r+".
    /// Open a file for update both reading and writing.
    /// It must exist.
    read_write_update,
    /// Mode "w+".
    /// Create an empty file for both reading and writing.
    /// If a file with the same name already exists its content is erased and the file is treated as a new empty file.
    read_write_replace,
};

/// Information about a completed asynchronous I/O request.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Outcome = struct {
    /// What generated this task.
    /// This will be invalid if it was closed!
    file: File,
    /// What sort of task was this?
    /// Read, write, etc?
    task_type: TaskType,
    /// The result of the work (success, failure, cancellation).
    result: Result,
    /// Buffer where data was read/written.
    /// The length of this is what was actually read/written.
    buffer: []u8,
    /// Offset in the `File` where data was read/written.
    offset: usize,
    /// Number of bytes the task was to read/write.
    bytes_requested: usize,
    /// Pointer provided by the app when starting the task.
    user_data: ?*anyopaque,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_AsyncIOOutcome) Outcome {
        return .{
            .file = .{ .value = value.asyncio.? },
            .task_type = @enumFromInt(value.type),
            .result = @enumFromInt(value.result),
            .buffer = @as([*]u8, @ptrCast(@alignCast(value.buffer.?)))[0..@intCast(value.bytes_transferred)],
            .offset = @intCast(value.offset),
            .bytes_requested = @intCast(value.bytes_requested),
            .user_data = value.userdata,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Outcome) c.SDL_AsyncIOOutcome {
        return .{
            .asyncio = self.file.value,
            .type = @intFromEnum(self.task_type),
            .result = @intFromEnum(self.result),
            .buffer = self.buffer.ptr,
            .offset = @intCast(self.offset),
            .bytes_requested = @intCast(self.bytes_requested),
            .bytes_transferred = @intCast(self.buffer.len),
            .userdata = self.user_data,
        };
    }
};

/// A queue of completed asynchronous I/O tasks.
///
/// ## Remarks
/// When starting an asynchronous operation, you specify a queue for the new task.
/// A queue can be asked later if any tasks in it have completed, allowing an app to manage multiple pending tasks in one place, in whatever order they complete.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Queue = struct {
    value: *c.SDL_AsyncIOQueue,

    /// Close and free any allocated resources for an async I/O object.
    ///
    /// ## Function Parameters
    /// * `self`: A queue to add the new file to.
    /// * `file`: The file to close.
    /// * `flush`: True if data should sync to disk before the task completes.
    /// * `user_data`: An app-defined pointer that will be provided with the task results.
    ///
    /// ## Remarks
    /// Closing a file is also an asynchronous task!
    /// If a write failure were to happen during the closing process, for example, the task results will report it as usual.
    ///
    /// Closing a file that has been written to does not guarantee the data has made it to physical media; it may remain in the operating system's file cache,
    /// for later writing to disk.
    /// This means that a successfully-closed file can be lost if the system crashes or loses power in this small window.
    /// To prevent this, call this function with the flush parameter set to true.
    /// This will make the operation take longer, and perhaps increase system load in general, but a successful result guarantees that the data has made it to physical storage.
    /// Don't use this for temporary files, caches, and unimportant data, and definitely use it for crucial irreplaceable files, like game saves.
    ///
    /// This function guarantees that the close will happen after any other pending tasks to asyncio, so it's safe to open a file, start several operations,
    /// close the file immediately, then check for all results later.
    /// This function will not block until the tasks have completed.
    ///
    /// Once this function returns true, asyncio is no longer valid, regardless of any future outcomes.
    /// Any completed tasks might still contain this pointer in their `Outcome` data, in case the app was using this value to track information,
    /// but it should not be used again.
    ///
    /// If this function returns an error, the close wasn't started at all, and it's safe to attempt to close again later.
    ///
    /// The newly-created task will be added to the queue when it completes its work.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, but two threads should not attempt to close the same object.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn closeFile(
        self: Queue,
        file: File,
        flush: bool,
        user_data: ?*anyopaque,
    ) !void {
        return errors.wrapCallBool(c.SDL_CloseAsyncIO(
            file.value,
            flush,
            self.value,
            user_data,
        ));
    }

    /// Destroy a previously-created async I/O task queue.
    ///
    /// ## Function Parameters
    /// * `self`: The task queue to destroy.
    ///
    /// ## Remarks
    /// If there are still tasks pending for this queue, this call will block until those tasks are finished.
    /// All those tasks will be deallocated.
    /// Their results will be lost to the app.
    ///
    /// Any pending reads from `loadFile` that are still in this queue will have their buffers deallocated by this function, to prevent a memory leak.
    ///
    /// Once this function is called, the queue is no longer valid and should not be used,
    /// including by other threads that might access it while destruction is blocking on pending tasks.
    ///
    /// Do not destroy a queue that still has threads waiting on it through `waitResult`.
    /// You can call `signal` first to unblock those threads, and take measures (such as `thread.Thread.wait`) to make sure they have finished their wait
    /// and won't wait on the queue again.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, so long as no other thread is waiting on the queue with `waitResult`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Queue,
    ) void {
        c.SDL_DestroyAsyncIOQueue(self.value);
    }

    /// Query an async I/O task queue for completed tasks.
    ///
    /// ## Function Parameters
    /// * `self`: The async I/O task queue to query.
    ///
    /// ## Return Value
    /// Returns the outcome if a task completed, `null` otherwise.
    ///
    /// ## Remarks
    /// If a task assigned to this queue has finished, this will return the outcome with the details of the task.
    /// If no task in the queue has finished, this function will return `null`.
    /// This function does not block.
    ///
    /// If a task has completed, this function will free its resources and the task pointer will no longer be valid.
    /// The task will be removed from the queue.
    ///
    /// It is safe for multiple threads to call this function on the same queue at once; a completed task will only go to one of the threads.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getResult(
        self: Queue,
    ) ?Outcome {
        var outcome: c.SDL_AsyncIOOutcome = undefined;
        const ret = c.SDL_GetAsyncIOResult(
            self.value,
            &outcome,
        );
        if (!ret)
            return null;
        return Outcome.fromSdl(outcome);
    }

    /// Create a task queue for tracking multiple I/O operations.
    ///
    /// ## Returns
    /// Returns a new task queue object.
    ///
    /// ## Remarks
    /// Async I/O operations are assigned to a queue when started.
    /// The queue can be checked for completed tasks thereafter.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init() !Queue {
        return .{ .value = try errors.wrapCallNull(*c.SDL_AsyncIOQueue, c.SDL_CreateAsyncIOQueue()) };
    }

    /// Load all the data from a file path, asynchronously.
    ///
    /// ## Function Parameters
    /// * `self`: A queue to add the new file to.
    /// * `file`: The path to read all available data from.
    /// * `user_data`: An app-defined pointer that will be provided with the task results.
    ///
    /// ## Remarks
    /// This function returns as quickly as possible; it does not wait for the read to complete.
    /// On a successful return, this work will continue in the background.
    /// If the work begins, even failure is asynchronous: a failing return value from this function only means the work couldn't start at all.
    ///
    /// The data is allocated with a zero byte at the end (null terminated) for convenience.
    /// This extra byte is not included in `Outcome`'s `buffer.len` value.
    ///
    /// This function will allocate the buffer to contain the file.
    /// It must be deallocated by calling `free` on `Outcome`'s buffer field after completion.
    ///
    /// An `Queue` must be specified.
    /// The newly-created task will be added to it when it completes its work.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn loadFile(
        self: Queue,
        file: [:0]const u8,
        user_data: ?*anyopaque,
    ) !void {
        return errors.wrapCallBool(c.SDL_LoadFileAsync(
            file,
            self.value,
            user_data,
        ));
    }

    /// Start an async read.
    ///
    /// ## Function Parameters
    /// * `self`: A queue to add the file to.
    /// * `file`: The file to read from.
    /// * `data`: Data to read from the file.
    /// * `offset`: Offset to read from in the file.
    /// * `user_data`: An app-defined pointer that will be provided with the task results.
    ///
    /// ## Remarks
    /// This function may read less bytes than requested.
    ///
    /// This function returns as quickly as possible; it does not wait for the read to complete.
    /// On a successful return, this work will continue in the background.
    /// If the work begins, even failure is asynchronous: a failing return value from this function only means the work couldn't start at all.
    ///
    /// The `data` must remain available until the work is done, and may be accessed by the system at any time until then.
    /// Do not allocate it on the stack, as this might take longer than the life of the calling function to complete!
    ///
    /// The newly-created task will be added to the queue when it completes its work.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readFile(
        self: Queue,
        file: File,
        data: []u8,
        offset: usize,
        user_data: ?*anyopaque,
    ) !void {
        return errors.wrapCallBool(c.SDL_ReadAsyncIO(
            file.value,
            data.ptr,
            @intCast(offset),
            @intCast(data.len),
            self.value,
            user_data,
        ));
    }

    /// Wake up any threads that are blocking in `Queue.waitResult`.
    ///
    /// ## Function Parameters
    /// * `self`: The async I/O task queue to signal.
    ///
    /// ## Remarks
    /// This will unblock any threads that are sleeping in a call to `Queue.waitResult` for the specified queue, and cause them to return from that function.
    ///
    /// This can be useful when destroying a queue to make sure nothing is touching it indefinitely.
    /// In this case, once this call completes, the caller should take measures to make sure any previously-blocked threads have returned from their wait
    /// and will not touch the queue again (perhaps by setting a flag to tell the threads to terminate and then using `thread.Thread.wait` to make sure they've done so).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn signal(
        self: Queue,
    ) void {
        c.SDL_SignalAsyncIOQueue(self.value);
    }

    /// Block until an async I/O task queue has a completed task.
    ///
    /// ## Function Parameters
    /// * `self`: The async I/O task queue to wait on.
    /// * `timeout_milliseconds`: The maximum time to wait, in milliseconds, or `null` to wait indefinitely.
    ///
    /// ## Return Value
    /// Returns an outcome if the task has completed, or `null` otherwise.
    ///
    /// ## Remarks
    /// This function puts the calling thread to sleep until there a task assigned to the queue that has finished.
    ///
    /// If a task assigned to the queue has finished, this will return the outcome with the details of the task.
    /// If no task in the queue has finished, this function will return `null`.
    ///
    /// If a task has completed, this function will free its resources and the task pointer will no longer be valid.
    /// The task will be removed from the queue.
    ///
    /// It is safe for multiple threads to call this function on the same queue at once; a completed task will only go to one of the threads.
    ///
    /// Note that by the nature of various platforms, more than one waiting thread may wake to handle a single task, but only one will obtain it,
    /// so `timeout_milliseconds` is a maximum wait time, and this function may return false sooner.
    ///
    /// This function may return `null` if there was a system error, the OS inadvertently awoke multiple threads,
    /// or if `Queue.signal` was called to wake up all waiting threads without a finished task.
    ///
    /// A timeout can be used to specify a maximum wait time, but rather than polling, it is possible to have a timeout of `null` to wait forever,
    /// and use `Queue.signal` to wake up the waiting threads later.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn waitResult(
        self: Queue,
        timeout_milliseconds: ?usize,
    ) ?Outcome {
        var outcome: c.SDL_AsyncIOOutcome = undefined;
        const ret = c.SDL_WaitAsyncIOResult(
            self.value,
            &outcome,
            if (timeout_milliseconds) |val| @intCast(val) else -1,
        );
        if (!ret)
            return null;
        return Outcome.fromSdl(outcome);
    }

    /// Start an async write.
    ///
    /// ## Function Parameters
    /// * `self`: A queue to add the file to.
    /// * `file`: The file to write to.
    /// * `data`: Data to write to the file.
    /// * `offset`: Offset to write to in the file.
    /// * `user_data`: An app-defined pointer that will be provided with the task results.
    ///
    /// ## Remarks
    /// This function returns as quickly as possible; it does not wait for the write to complete.
    /// On a successful return, this work will continue in the background.
    /// If the work begins, even failure is asynchronous: a failing return value from this function only means the work couldn't start at all.
    ///
    /// The `data` must remain available until the work is done, and may be accessed by the system at any time until then.
    /// Do not allocate it on the stack, as this might take longer than the life of the calling function to complete!
    ///
    /// The newly-created task will be added to the queue when it completes its work.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeFile(
        self: Queue,
        file: File,
        data: []const u8,
        offset: usize,
        user_data: ?*anyopaque,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteAsyncIO(
            file.value,
            @constCast(data.ptr),
            @intCast(offset),
            @intCast(data.len),
            self.value,
            user_data,
        ));
    }
};

/// Possible outcomes of an asynchronous I/O task.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Result = enum(c.SDL_AsyncIOResult) {
    /// Request was completed without error.
    complete = c.SDL_ASYNCIO_COMPLETE,
    /// Request failed for some reason.
    failure = c.SDL_ASYNCIO_FAILURE,
    /// Request was canceled before completing.
    canceled = c.SDL_ASYNCIO_CANCELED,
};

/// Types of asynchronous I/O tasks.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const TaskType = enum(c.SDL_AsyncIOTaskType) {
    /// A read operation.
    read = c.SDL_ASYNCIO_TASK_READ,
    /// A write operation.
    write = c.SDL_ASYNCIO_TASK_WRITE,
    /// A close operation.
    close = c.SDL_ASYNCIO_TASK_CLOSE,
};

// Test asynchronous IO.
test "Async IO" {
    errors.refAllDeclsRecursive(@This());
}
