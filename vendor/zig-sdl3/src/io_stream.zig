const c = @import("c");
const errors = @import("errors.zig");
const properties = @import("properties.zig");
const sdl3 = @import("sdl3.zig");
const std = @import("std");

/// Status, set by a read or write operation.
///
/// ## Version
/// This error is available since SDL 3.2.0.
pub const Error = error{
    /// Read or write I/O error.
    Err,
    /// Non blocking I/O, not ready.
    NotReady,
    /// Tried to write a read-only buffer
    ReadOnly,
    /// Tried to read a write-only buffer.
    WriteOnly,
};

/// Mode for `io_steam.Stream.initFromFile()`.
///
/// ## Version
/// This enum is provided by zig-sdl3.
pub const FileMode = enum {
    /// Open a text file for reading.
    /// The file must exist.
    read_text,
    /// Create an empty text file for writing.
    /// If a file with the same name already exists its content is erased and the file is treated as a new empty file.
    write_text,
    /// Append to a text file.
    /// Writing operations append data at the end of the file.
    /// The file is created if it does not exist.
    append_text,
    /// Open a text file for update both reading and writing.
    /// The file must exist.
    read_write_update_text,
    /// Create an empty text file for both reading and writing.
    /// If a file with the same name already exists its content is erased and the file is treated as a new empty file.
    read_write_replace_text,
    /// Open a text file for reading and appending.
    /// All writing operations are performed at the end of the file, protecting the previous content to be overwritten.
    /// You can reposition the internal pointer to anywhere in the file for reading, but writing operations will move it back to the end of file.
    /// The file is created if it does not exist.
    read_append_text,
    /// Open a binary file for reading.
    /// The file must exist.
    read_binary,
    /// Create an empty binary file for writing.
    /// If a file with the same name already exists its content is erased and the file is treated as a new empty file.
    write_binary,
    /// Append to a binary file.
    /// Writing operations append data at the end of the file.
    /// The file is created if it does not exist.
    append_binary,
    /// Open a binary file for update both reading and writing.
    /// The file must exist.
    read_write_update_binary,
    /// Create an empty binary file for both reading and writing.
    /// If a file with the same name already exists its content is erased and the file is treated as a new empty file.
    read_write_replace_binary,
    /// Open a binary file for reading and appending.
    /// All writing operations are performed at the end of the file, protecting the previous content to be overwritten.
    /// You can reposition the internal pointer to anywhere in the file for reading, but writing operations will move it back to the end of file.
    /// The file is created if it does not exist.
    read_append_binary,
};

/// The function pointers that drive an `io_stream.Stream`.
///
/// ## Remarks
/// Applications can provide this struct to `io_stream.Stream.init()` to create their own implementation of a stream.
/// This is not necessarily required, as SDL already offers several common types of I/O streams,
/// via functions like `io_stream.Stream.initFromFile()` and `io_stream.Stream.initFromMem()`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub fn Interface(comptime UserData: type) type {
    return struct {
        /// Returns the number of bytes in the stream.
        ///
        /// ## Function Parameters
        /// * `user_data`: User context.
        ///
        /// ## Return Value
        /// The total size of the data stream.
        size: ?*const fn (user_data: ?*UserData) anyerror!usize = null,

        /// Seek to `offset` relative to `whence`.
        ///
        /// ## Function Parameters
        /// * `user_data`: User context.
        /// * `offset`: Offset to seek from.
        /// * `whence`: Where to seek relative to.
        ///
        /// ## Return Value
        /// The final offset in the data stream.
        seek: ?*const fn (user_data: ?*UserData, offset: isize, whence: Whence) anyerror!usize = null,

        /// Read up to `size` bytes from the data stream to the area pointed at by `ptr`.
        ///
        /// ## Function Parameters
        /// * `user_data`: User context.
        /// * `buf`: The buffer to read into.
        /// * `status`: Output status.
        ///
        /// ## Return Value
        /// The actual read buffer data.
        read: ?*const fn (user_data: ?*UserData, buf: []u8) Error!?[]u8 = null,

        /// Write up to `size` bytes to the data stream from the area pointed at by `ptr`.
        ///
        /// ## Function Parameters
        /// * `user_data`: User context.
        /// * `data`: Data to write.
        ///
        /// ## Return Value
        /// The number of bytes written.
        write: ?*const fn (user_data: ?*UserData, data: []const u8) Error!usize = null,

        /// If the stream is buffering, make sure the data is written out.
        ///
        /// ## Function Parameters
        /// * `user_data`: User context.
        flush: ?*const fn (user_data: ?*UserData) Error!void = null,

        /// Close and free any allocated resources.
        ///
        /// ## Function Parameters
        /// * `user_data`: User context.
        ///
        /// ## Remarks
        /// This does not guarantee file writes will sync to physical media; they can be in the system's file cache, waiting to go to disk.
        /// The stream is still destroyed even if this fails, so clean up anything even if flushing buffers, etc, returns an error.
        close: ?*const fn (user_data: ?*UserData) anyerror!void = null,

        /// Convert to an SDL value.
        pub fn toSdl(comptime self: Interface(UserData)) c.SDL_IOStreamInterface {
            const Cb = struct {
                /// Returns the number of bytes in the stream.
                ///
                /// ## Function Parameters
                /// * `user_data`: User context.
                ///
                /// ## Return Value
                /// The total size of the data stream, or `-1` on error.
                pub fn size_c(user_data_c: ?*anyopaque) callconv(.c) i64 {
                    const ret = self.size.?(@ptrCast(@alignCast(user_data_c))) catch return -1;
                    return @intCast(ret);
                }

                /// Seek to `offset` relative to `whence`.
                ///
                /// ## Function Parameters
                /// * `user_data`: User context.
                /// * `offset`: Offset to seek from.
                /// * `whence`: Where to seek relative to.
                ///
                /// ## Return Value
                /// The final offset in the data stream, or `-1` on error.
                pub fn seek_c(user_data_c: ?*anyopaque, offset_c: i64, whence_c: c.SDL_IOWhence) callconv(.c) i64 {
                    const ret = self.seek.?(@ptrCast(@alignCast(user_data_c)), @intCast(offset_c), @enumFromInt(whence_c)) catch return -1;
                    return @intCast(ret);
                }

                /// Read up to `size` bytes from the data stream to the area pointed at by `ptr`.
                ///
                /// ## Function Parameters
                /// * `user_data`: User context.
                /// * `ptr`: Pointer to read into.
                /// * `size`: Maximum amount of data to read.
                /// * `status`: Output status.
                ///
                /// ## Remarks
                /// Set `status` on incomplete read.
                ///
                /// ## Return Value
                /// The number of bytes read.
                pub fn read_c(user_data_c: ?*anyopaque, ptr_c: ?*anyopaque, size_c_val: usize, status_c: [*c]c.SDL_IOStatus) callconv(.c) usize {
                    const ret = self.read.?(@ptrCast(@alignCast(user_data_c)), @as([*]u8, @ptrCast(@alignCast(ptr_c)))[0..@intCast(size_c_val)]) catch |err| {
                        status_c.* = switch (err) {
                            error.Err => c.SDL_IO_STATUS_ERROR,
                            error.NotReady => c.SDL_IO_STATUS_NOT_READY,
                            error.ReadOnly => c.SDL_IO_STATUS_READONLY,
                            error.WriteOnly => c.SDL_IO_STATUS_WRITEONLY,
                        };
                        return 0;
                    };
                    status_c.* = c.SDL_IO_STATUS_READY;
                    return if (ret) |val| @intCast(val.len) else 0;
                }

                /// Write up to `size` bytes to the data stream from the area pointed at by `ptr`.
                ///
                /// ## Function Parameters
                /// * `user_data`: User context.
                /// * `ptr`: Pointer to read into.
                /// * `size`: Maximum amount of data to write.
                /// * `status`: Output status.
                ///
                /// ## Remarks
                /// Set `status` on incomplete write.
                ///
                /// ## Return Value
                /// The number of bytes written.
                pub fn write_c(user_data_c: ?*anyopaque, ptr_c: ?*const anyopaque, size_c_val: usize, status_c: [*c]c.SDL_IOStatus) callconv(.c) usize {
                    const ret = self.write.?(@ptrCast(@alignCast(user_data_c)), @as([*]const u8, @ptrCast(@alignCast(ptr_c)))[0..@intCast(size_c_val)]) catch |err| {
                        status_c.* = switch (err) {
                            error.Err => c.SDL_IO_STATUS_ERROR,
                            error.NotReady => c.SDL_IO_STATUS_NOT_READY,
                            error.ReadOnly => c.SDL_IO_STATUS_READONLY,
                            error.WriteOnly => c.SDL_IO_STATUS_WRITEONLY,
                        };
                        return 0;
                    };
                    status_c.* = c.SDL_IO_STATUS_READY;
                    return ret;
                }

                /// If the stream is buffering, make sure the data is written out.
                ///
                /// ## Function Parameters
                /// * `user_data`: User context.
                ///
                /// ## Remarks
                /// Set `status` on failure.
                ///
                /// ## Return Value
                /// True if successful.
                pub fn flush_c(user_data_c: ?*anyopaque, status: [*c]c.SDL_IOStatus) callconv(.c) bool {
                    self.flush.?(@ptrCast(@alignCast(user_data_c))) catch |err| {
                        status.* = switch (err) {
                            error.Err => c.SDL_IO_STATUS_ERROR,
                            error.NotReady => c.SDL_IO_STATUS_NOT_READY,
                            error.ReadOnly => c.SDL_IO_STATUS_READONLY,
                            error.WriteOnly => c.SDL_IO_STATUS_WRITEONLY,
                        };
                        return false;
                    };
                    status.* = c.SDL_IO_STATUS_READY;
                    return true;
                }

                /// Close and free any allocated resources.
                ///
                /// ## Function Parameters
                /// * `user_data`: User context.
                ///
                /// ## Remarks
                /// This does not guarantee file writes will sync to physical media; they can be in the system's file cache, waiting to go to disk.
                /// The stream is still destroyed even if this fails, so clean up anything even if flushing buffers, etc, returns an error.
                ///
                /// ## Return Value
                /// True if successful.
                pub fn close_c(user_data_c: ?*anyopaque) callconv(.c) bool {
                    self.close.?(@ptrCast(@alignCast(user_data_c))) catch return false;
                    return true;
                }
            };
            return .{
                .version = @sizeOf(c.SDL_IOStreamInterface),
                .size = if (self.size != null) Cb.size_c else null,
                .seek = if (self.seek != null) Cb.seek_c else null,
                .read = if (self.read != null) Cb.read_c else null,
                .write = if (self.write != null) Cb.write_c else null,
                .flush = if (self.flush != null) Cb.flush_c else null,
                .close = if (self.close != null) Cb.close_c else null,
            };
        }
    };
}

/// The read/write operation structure.
///
/// ## Remarks
/// This operates as an opaque handle.
/// There are several APIs to create various types of I/O streams,
/// or an app can supply an `io_stream.Interface` to `io_stream.Stream.initInterface()` to provide their own stream implementation behind this struct's abstract interface.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Stream = struct {
    value: *c.SDL_IOStream,

    /// Properties that can be obtained from a stream.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// Memory slice if `io_stream.Stream.initFromMem()` or `io_stream.Stream.initFromConstMem()` was called.
        memory: ?[]u8,
        /// Data that can be get/set for `io_stream.Stream.initFromDynamicMem()`.
        dynamic_memory: ?struct {
            /// A pointer to the internal memory of the stream.
            /// This can be set to `null` to transfer ownership of the memory to the application, which should free the memory with `free()`.
            /// If this is done, the next operation on the stream must be `io_stream.Stream.deinit()`.
            ptr: ?*anyopaque,
            /// Memory will be allocated in multiples of this size, defaulting to `1024`.
            chunk_size: ?usize = null,
        },
        /// This will be called with the passed `mem` pointer when closing the stream.
        /// By default it is unset, i.e., the memory will not be freed.
        memory_free_function: ?sdl3.FreeFuncC,
        /// Data that may set by `io_stream.Stream.initFromFile()`.
        /// A pointer, that can be cast to a win32 `HANDLE`, that this stream is using to access the filesystem.
        /// If the program isn't running on Windows, or SDL used some other method to access the filesystem, this property will not be set.
        window_handle: ?*anyopaque,
        /// Data that may set by `io_stream.Stream.initFromFile()`.
        /// A pointer, that can be cast to a stdio `FILE *`, that this stream is using to access the filesystem.
        /// If SDL used some other method to access the filesystem, this property will not be set.
        /// PLEASE NOTE that if SDL is using a different C runtime than your app, trying to use this pointer will almost certainly result in a crash!
        /// This is mostly a problem on Windows; make sure you build SDL and your app with the same compiler and settings to avoid it.
        stdio_file: ?*anyopaque,
        /// Data that may set by `io_stream.Stream.initFromFile()`.
        /// A file descriptor that this stream is using to access the filesystem.
        file_descriptor: ?i64,
        /// Data that may set by `io_stream.Stream.initFromFile()`.
        /// A pointer, that can be cast to an Android NDK `AAsset *`, that this stream is using to access the filesystem.
        /// If SDL used some other method to access the filesystem, this property will not be set.
        android_aasset: ?*anyopaque,

        /// Convert from SDL properties.
        pub fn fromProperties(props: properties.Group) Properties {
            return .{
                .memory = if (props.get(c.SDL_PROP_IOSTREAM_MEMORY_POINTER)) |val|
                    @as([*]u8, @ptrCast(@alignCast(val.pointer.?)))[0..@intCast(props.get(c.SDL_PROP_IOSTREAM_MEMORY_SIZE_NUMBER).?.number)]
                else
                    null,
                .dynamic_memory = if (props.get(c.SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER)) |val| .{
                    .ptr = val.pointer,
                    .chunk_size = if (props.get(c.SDL_PROP_IOSTREAM_DYNAMIC_CHUNKSIZE_NUMBER)) |chunk_size| @intCast(chunk_size.number) else null,
                } else null,
                .memory_free_function = if (props.get(c.SDL_PROP_IOSTREAM_MEMORY_FREE_FUNC_POINTER)) |val| if (val.pointer) |pv| @ptrCast(@alignCast(pv)) else null else null,
                .window_handle = if (props.get(c.SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER)) |val| val.pointer.? else null,
                .stdio_file = if (props.get(c.SDL_PROP_IOSTREAM_STDIO_FILE_POINTER)) |val| val.pointer.? else null,
                .file_descriptor = if (props.get(c.SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER)) |val| val.number else null,
                .android_aasset = if (props.get(c.SDL_PROP_IOSTREAM_ANDROID_AASSET_POINTER)) |val| val.pointer.? else null,
            };
        }

        /// Set properties.
        pub fn setProperties(self: Properties, props: properties.Group) !void {
            if (self.memory) |val| {
                try props.set(c.SDL_PROP_IOSTREAM_MEMORY_POINTER, .{ .pointer = val.ptr });
                try props.set(c.SDL_PROP_IOSTREAM_MEMORY_SIZE_NUMBER, .{ .number = @intCast(val.len) });
            } else {
                try props.clear(c.SDL_PROP_IOSTREAM_MEMORY_POINTER);
                try props.clear(c.SDL_PROP_IOSTREAM_MEMORY_SIZE_NUMBER);
            }
            if (self.dynamic_memory) |val| {
                try props.set(c.SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, .{ .pointer = val.ptr });
                if (val.chunk_size) |chunk_size| {
                    try props.set(c.SDL_PROP_IOSTREAM_DYNAMIC_CHUNKSIZE_NUMBER, .{ .number = @intCast(chunk_size) });
                }
            } else {
                try props.clear(c.SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER);
                try props.clear(c.SDL_PROP_IOSTREAM_DYNAMIC_CHUNKSIZE_NUMBER);
            }
            if (self.memory_free_function) |val| {
                try props.set(c.SDL_PROP_IOSTREAM_MEMORY_FREE_FUNC_POINTER, .{ .pointer = @constCast(val) });
            } else try props.clear(c.SDL_PROP_IOSTREAM_MEMORY_FREE_FUNC_POINTER);
            if (self.window_handle) |val| {
                try props.set(c.SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER, .{ .pointer = val });
            } else try props.clear(c.SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER);
            if (self.stdio_file) |val| {
                try props.set(c.SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, .{ .pointer = val });
            } else try props.clear(c.SDL_PROP_IOSTREAM_STDIO_FILE_POINTER);
            if (self.file_descriptor) |val| {
                try props.set(c.SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER, .{ .number = val });
            } else try props.clear(c.SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER);
            if (self.android_aasset) |val| {
                try props.set(c.SDL_PROP_IOSTREAM_ANDROID_AASSET_POINTER, .{ .pointer = val });
            } else try props.clear(c.SDL_PROP_IOSTREAM_ANDROID_AASSET_POINTER);
        }
    };

    /// Close and free an allocated stream structure.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to close.
    ///
    /// ## Remarks
    /// `io_stream.Stream.deinit()` closes and cleans up the stream.
    /// It releases any resources used by the stream and frees the `io_stream.Stream` itself.
    /// This returns an error if the stream failed to flush to its output (e.g. to disk).
    ///
    /// Note that if this fails to flush the stream for any reason, this function reports an error, but the `io_stream.Stream` is still invalid once this function returns.
    ///
    /// This call flushes any buffered writes to the operating system, but there are no guarantees that those writes have gone to physical media;
    /// they might be in the OS's file cache, waiting to go to disk later.
    /// If it's absolutely crucial that writes go to disk immediately, so they are definitely stored even if the power fails before the file cache would have caught up,
    /// one should call `io_stream.Stream.flush()` before closing.
    /// Note that flushing takes time and makes the system and your app operate less efficiently, so do so sparingly.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_CloseIO(
            self.value,
        ));
    }

    /// Flush any buffered data in the stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to flush.
    ///
    /// ## Remarks
    /// This function makes sure that any buffered data is written to the stream.
    /// Normally this isn't necessary but if the stream is a pipe or socket it guarantees that any pending data is sent.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn flush(
        self: Stream,
    ) !void {
        return errors.wrapCallBool(c.SDL_FlushIO(
            self.value,
        ));
    }

    /// Get the properties associated with a stream.
    ///
    /// ## Function Parameters
    /// * `self`: Stream structure.
    ///
    /// ## Return Value
    /// Returns the stream's properties.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Stream,
    ) !Properties {
        return Properties.fromProperties(.{
            .value = try errors.wrapCall(c.SDL_PropertiesID, c.SDL_GetIOProperties(
                self.value,
            ), 0),
        });
    }

    /// Use this function to get the size of the data stream in a stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to get the size from.
    ///
    /// ## Return Value
    /// Returns the size of the data stream.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSize(
        self: Stream,
    ) !usize {
        const ret = c.SDL_GetIOSize(self.value);
        if (ret < 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
        return @intCast(ret);
    }

    /// Create a custom stream.
    ///
    /// ## Function Parameters
    /// * `UserData`: Type of user data.
    /// * `interface`: The interface that implements this stream.
    /// * `user_data`: User data that will be passed to the interface functions.
    ///
    /// ## Return Value
    /// Returns the created stream.
    ///
    /// ## Remarks
    /// Applications do not need to use this function unless they are providing their own `io_stream.Stream` implementation.
    /// If you just need a stream to read/write a common data source, you should use the built-in implementations in SDL,
    /// like `io_stream.Stream.initFromFile()` or `io_stream.Stream.initFromMem()`, etc.
    ///
    /// This function makes a copy of iface and the caller does not need to keep it around after this call.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        comptime UserData: type,
        comptime interface: Interface(UserData),
        user_data: ?*UserData,
    ) !Stream {
        const interface_sdl = interface.toSdl();
        return .{
            .value = try errors.wrapCallNull(*c.SDL_IOStream, c.SDL_OpenIO(
                &interface_sdl,
                @ptrCast(user_data),
            )),
        };
    }

    /// Use this function to prepare a read-only memory buffer for use with a stream.
    ///
    /// ## Function Parameters
    /// * `data`: Read only buffer to make into a stream.
    ///
    /// ## Return Value
    /// Returns a stream.
    ///
    /// ## Remarks
    /// This function sets up a stream struct based on a memory area of a certain size.
    /// It assumes the memory area is not writable.
    ///
    /// Attempting to write to this stream will report an error without writing to the memory buffer.
    ///
    /// This memory buffer is not copied by the stream; the pointer you provide must remain valid until you close the stream.
    /// Closing the stream will not free the original buffer.
    ///
    /// If you need to write to a memory buffer, you should use `io_stream.Stream.initFromMem()` with a writable buffer of memory instead.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromConstMem(
        data: []const u8,
    ) !Stream {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_IOStream, c.SDL_IOFromConstMem(
                data.ptr,
                data.len,
            )),
        };
    }

    /// Use this function to create a stream that is backed by dynamically allocated memory.
    ///
    /// ## Return Value
    /// Returns a stream.
    ///
    /// ## Remarks
    /// You may adjust the stream's properties to access the memory and allocations.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromDynamicMem() !Stream {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_IOStream, c.SDL_IOFromDynamicMem()),
        };
    }

    /// Use this function to create a new stream structure for reading from and/or writing to a named file.
    ///
    /// ## Function Parameters
    /// * `path`: A UTF-8 string representing the filename to open.
    /// * `mode`: An ASCII string representing the mode to be used for opening the file.
    ///
    /// ## Return Value
    /// Returns a stream.
    ///
    /// ## Remarks
    /// This function supports Unicode filenames, but they must be encoded in UTF-8 format, regardless of the underlying operating system.
    ///
    /// In Android, `io_stream.Stream.initFromFile()` can be used to open `content://` URIs.
    /// As a fallback, `io_stream.Stream.initFromFile()` will transparently open a matching filename in the app's assets.
    ///
    /// Closing the stream will close SDL's internal file handle.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromFile(
        path: [:0]const u8,
        mode: FileMode,
    ) !Stream {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_IOStream, c.SDL_IOFromFile(path.ptr, switch (mode) {
                .append_binary => "ab",
                .append_text => "a",
                .read_append_binary => "a+b",
                .read_append_text => "a+",
                .read_binary => "rb",
                .read_text => "r",
                .read_write_replace_binary => "w+b",
                .read_write_replace_text => "w+",
                .read_write_update_binary => "r+b",
                .read_write_update_text => "r+",
                .write_binary => "wb",
                .write_text => "w",
            })),
        };
    }

    /// File data for a zig file stream.
    pub const FsFileData = struct {
        /// The IO interface.
        io: std.Io,
        /// File to read.
        file: std.Io.File,
        /// File position to read/write.
        position: u64 = 0,
    };

    /// Initialize an SDL IO stream from a zig filesystem file.
    ///
    /// ## Function
    /// * `file_data`: The file data to initialize an SDL stream interface for.
    ///
    /// ## Return Value
    /// Returns the stream.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn initFromFsFile(
        file_data: *FsFileData,
    ) !Stream {
        const FsFileInterface = struct {
            const UserData = FsFileData;

            fn interface_close(user_data: ?*UserData) anyerror!void {
                user_data.?.file.close(user_data.?.io);
            }

            fn interface_flush(user_data: ?*UserData) Error!void {
                user_data.?.file.sync(user_data.?.io) catch return error.Err;
            }

            fn interface_read(user_data: ?*UserData, buf: []u8) Error!?[]u8 {
                const ret = user_data.?.file.readPositionalAll(
                    user_data.?.io,
                    buf,
                    user_data.?.position,
                ) catch |err| return switch (err) {
                    error.NotOpenForReading => error.WriteOnly,
                    error.SystemResources => error.NotReady,
                    else => error.Err,
                };
                if (ret == 0)
                    return null;
                user_data.?.position += @intCast(ret);
                return buf[0..ret];
            }

            fn interface_seek(user_data: ?*UserData, offset: isize, whence: Whence) anyerror!usize {
                switch (whence) {
                    .set => user_data.?.position = @intCast(offset),
                    .cur => user_data.?.position = @intCast(@as(isize, @intCast(user_data.?.position)) + offset),
                    .end => user_data.?.position = @as(u64, @intCast(try interface_size(user_data))) - @as(u64, @intCast(offset)),
                }
                return @intCast(user_data.?.position);
            }

            fn interface_size(user_data: ?*UserData) anyerror!usize {
                return @intCast(try user_data.?.file.length(user_data.?.io));
            }

            fn interface_write(user_data: ?*UserData, data: []const u8) Error!usize {
                const ret = user_data.?.file.writePositional(
                    user_data.?.io,
                    &.{data},
                    user_data.?.position,
                ) catch |err| return switch (err) {
                    error.NotOpenForWriting => error.ReadOnly,
                    error.DeviceBusy, error.SystemResources => error.NotReady,
                    else => error.Err,
                };
                user_data.?.position += @intCast(ret);
                return ret;
            }

            const interface = Interface(UserData){
                .close = &interface_close,
                .flush = &interface_flush,
                .read = &interface_read,
                .seek = &interface_seek,
                .size = &interface_size,
                .write = &interface_write,
            };
        };
        return init(
            FsFileInterface.UserData,
            FsFileInterface.interface,
            file_data,
        );
    }

    /// Use this function to prepare a read-write memory buffer for use with a stream.
    ///
    /// ## Function Parameters
    /// * `data`: Buffer to make into a stream.
    ///
    /// ## Return Value
    /// Returns a stream.
    ///
    /// ## Remarks
    /// This function sets up a stream struct based on a memory area of a certain size, for both read and write access.
    ///
    /// This memory buffer is not copied by the stream; the pointer you provide must remain valid until you close the stream.
    /// Closing the stream will not free the original buffer.
    ///
    /// If you need to make sure the stream never writes to the memory buffer, you should use `io_stream.Stream.initFromConstMem()` with a read-only buffer of memory instead.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromMem(
        data: []u8,
    ) !Stream {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_IOStream, c.SDL_IOFromMem(
                data.ptr,
                data.len,
            )),
        };
    }

    /// Get the data type for initializing reader/writer stream interface.
    ///
    /// ## Function Parameters
    /// * `UserData`: The type of user data.
    ///
    /// ## Return Value
    /// Returns the data that must be passed to the `initFromReaderWriter` function.
    pub fn InitFromReaderWriterData(
        comptime UserData: type,
    ) type {
        return struct {
            /// Optional reader available to use for the IO stream.
            io_reader: ?*std.Io.Reader,
            /// Optional writer available to use for the IO stream.
            io_writer: ?*std.Io.Writer,
            /// User data to pass for IO stream callbacks.
            user_data: UserData,
        };
    }

    /// Initialize a stream interface with a reader, writer, and some callback functions.
    ///
    /// ## Function Parameters
    /// * `UserData`: User data type for callback functions.
    /// * `close_cb`: An optional callback that provides the stream the ability to close, otherwise it is a no-op.
    /// * `seek_cb`: An optional callback that provides the stream the ability to seek (will error when seeking otherwise).
    ///     The callback takes the reader and writer passed to this and must return the new absolute position in the stream.
    /// * `size_cb`: An optional callback that provides the stream the ability to get its total size (will error otherwise).
    ///     The callback takes the reader and writer passed to this and must return its total size.
    /// * `data`: Data to pass to the stream callback functions.
    ///
    /// ## Return Value
    /// Returns a stream.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn initFromReaderWriter(
        comptime UserData: type,
        comptime close_cb: ?*const fn (reader: ?*std.Io.Reader, writer: ?*std.Io.Writer, user_data: UserData) anyerror!void,
        comptime seek_cb: ?*const fn (reader: ?*std.Io.Reader, writer: ?*std.Io.Writer, offset: isize, whence: Whence, user_data: UserData) anyerror!usize,
        comptime size_cb: ?*const fn (reader: ?*std.Io.Reader, writer: ?*std.Io.Writer, user_data: UserData) anyerror!usize,
        data: *InitFromReaderWriterData(UserData),
    ) !Stream {
        const ReaderWriterInterface = struct {
            const InterfaceUserData = InitFromReaderWriterData(UserData);

            fn interface_close(interface_user_data: ?*InterfaceUserData) anyerror!void {
                return close_cb.?(interface_user_data.?.io_reader, interface_user_data.?.io_writer, interface_user_data.?.user_data);
            }

            fn interface_flush(interface_user_data: ?*InterfaceUserData) Error!void {
                if (interface_user_data.?.io_writer) |val|
                    val.flush() catch return error.Err;
            }

            fn interface_read(interface_user_data: ?*InterfaceUserData, buf: []u8) Error!?[]u8 {
                if (interface_user_data.?.io_reader) |val| {
                    if (interface_user_data.?.io_writer) |val2|
                        val2.advance(buf.len);
                    var stream_writer = std.Io.Writer.fixed(buf);
                    const read_len = val.stream(&stream_writer, .limited(buf.len)) catch |err| switch (err) {
                        error.ReadFailed, error.WriteFailed => return error.Err,
                        error.EndOfStream => return null,
                    };
                    return buf[0..read_len];
                }
                return error.WriteOnly;
            }

            fn interface_seek(interface_user_data: ?*InterfaceUserData, offset: isize, whence: Whence) anyerror!usize {
                return seek_cb.?(interface_user_data.?.io_reader, interface_user_data.?.io_writer, offset, whence, interface_user_data.?.user_data);
            }

            fn interface_size(interface_user_data: ?*InterfaceUserData) anyerror!usize {
                return size_cb.?(interface_user_data.?.io_reader, interface_user_data.?.io_writer, interface_user_data.?.user_data);
            }

            fn interface_write(interface_user_data: ?*InterfaceUserData, io_data: []const u8) Error!usize {
                if (interface_user_data.?.io_writer) |val| {
                    const ret = val.write(io_data) catch return error.Err;
                    if (interface_user_data.?.io_reader) |val2|
                        val2.discardAll(io_data.len) catch return error.Err;
                    return ret;
                } else {
                    return error.ReadOnly;
                }
            }

            const interface = Interface(InterfaceUserData){
                .close = if (close_cb != null) &interface_close else null,
                .flush = &interface_flush,
                .read = &interface_read,
                .seek = if (seek_cb != null) &interface_seek else null,
                .size = if (size_cb != null) &interface_size else null,
                .write = &interface_write,
            };
        };
        return init(
            ReaderWriterInterface.InterfaceUserData,
            ReaderWriterInterface.interface,
            data,
        );
    }

    /// Load all the data from an SDL data stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read all available data from.
    /// * `close_when_done`: If true, close the stream before returning, even on error.
    ///
    /// ## Return Value
    /// Returns the data.
    /// This should be freed with `free()`.
    ///
    /// ## Remarks
    /// The data is null-terminated for convenience.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn loadFile(
        self: Stream,
        close_when_done: bool,
    ) ![:0]u8 {
        var len: usize = undefined;
        return @as([*:0]u8, @ptrCast(@alignCast(try errors.wrapCallNull(*anyopaque, c.SDL_LoadFile_IO(
            self.value,
            &len,
            close_when_done,
        )))))[0..len :0];
    }

    /// Read from a data source.
    ///
    /// ## Function Parameters
    /// * `self`: Stream structure.
    /// * `buf`: Buffer to read into.
    ///
    /// ## Return Value
    /// Returns the slice of bytes read, this re-uses the same pointer for `buf`.
    /// If `null` is returned, then the end of file has been reached.
    ///
    /// ## Remarks
    /// This function may read less bytes than requested.
    ///
    /// This function will return `null` when the data stream is completely read, and will not return an error.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn read(
        self: Stream,
        buf: []u8,
    ) !?[]u8 {
        const ret = c.SDL_ReadIO(self.value, buf.ptr, buf.len);
        if (ret == 0) {
            const status = c.SDL_GetIOStatus(self.value);
            switch (status) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => return null,
            }
        }
        return buf.ptr[0..ret];
    }

    /// Use this function to read a signed byte from a stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readS8(
        self: Stream,
    ) !?i8 {
        var ret: i8 = undefined;
        if (!c.SDL_ReadS8(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 16 bits of big-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readS16Be(
        self: Stream,
    ) !?i16 {
        var ret: i16 = undefined;
        if (!c.SDL_ReadS16BE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 16 bits of little-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readS16Le(
        self: Stream,
    ) !?i16 {
        var ret: i16 = undefined;
        if (!c.SDL_ReadS16LE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 32 bits of big-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readS32Be(
        self: Stream,
    ) !?i32 {
        var ret: i32 = undefined;
        if (!c.SDL_ReadS32BE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 32 bits of little-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readS32Le(
        self: Stream,
    ) !?i32 {
        var ret: i32 = undefined;
        if (!c.SDL_ReadS32LE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 64 bits of big-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readS64Be(
        self: Stream,
    ) !?i64 {
        var ret: i64 = undefined;
        if (!c.SDL_ReadS64BE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 64 bits of little-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readS64Le(
        self: Stream,
    ) !?i64 {
        var ret: i64 = undefined;
        if (!c.SDL_ReadS64LE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read a byte from a stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readU8(
        self: Stream,
    ) !?u8 {
        var ret: u8 = undefined;
        if (!c.SDL_ReadU8(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 16 bits of big-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readU16Be(
        self: Stream,
    ) !?u16 {
        var ret: u16 = undefined;
        if (!c.SDL_ReadU16BE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 16 bits of little-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readU16Le(
        self: Stream,
    ) !?u16 {
        var ret: u16 = undefined;
        if (!c.SDL_ReadU16LE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 32 bits of big-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readU32Be(
        self: Stream,
    ) !?u32 {
        var ret: u32 = undefined;
        if (!c.SDL_ReadU32BE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 32 bits of little-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readU32Le(
        self: Stream,
    ) !?u32 {
        var ret: u32 = undefined;
        if (!c.SDL_ReadU32LE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 64 bits of big-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readU64Be(
        self: Stream,
    ) !?u64 {
        var ret: u64 = undefined;
        if (!c.SDL_ReadU64BE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Use this function to read 64 bits of little-endian data from a stream and return in native format.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to read from.
    ///
    /// ## Return Value
    /// Returns a value if successful, `null` if the end of file is reached, or an error if an error was encountered.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the data returned will be in the native byte order.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readU64Le(
        self: Stream,
    ) !?u64 {
        var ret: u64 = undefined;
        if (!c.SDL_ReadU64LE(self.value, &ret)) {
            switch (c.SDL_GetIOStatus(self.value)) {
                c.SDL_IO_STATUS_ERROR => {
                    errors.callErrorCallback();
                    return error.Err;
                },
                c.SDL_IO_STATUS_NOT_READY => {
                    errors.callErrorCallback();
                    return error.NotReady;
                },
                c.SDL_IO_STATUS_READONLY => {
                    errors.callErrorCallback();
                    return error.ReadOnly;
                },
                c.SDL_IO_STATUS_WRITEONLY => {
                    errors.callErrorCallback();
                    return error.WriteOnly;
                },
                else => {
                    return null;
                },
            }
        }
        return ret;
    }

    /// Stream reader type.
    ///
    /// ## Version
    /// This type is provided by zig-sdl3.
    pub const Reader = struct {
        stream: Stream,
        err: ?Error = null,
        interface: std.Io.Reader,

        pub fn init(stream: Stream, buffer: []u8) Reader {
            return .{
                .stream = stream,
                .interface = .{
                    .buffer = buffer,
                    .vtable = &.{ .stream = streamFn },
                    .seek = 0,
                    .end = 0,
                },
            };
        }

        fn streamFn(r: *std.Io.Reader, w: *std.Io.Writer, limit: std.Io.Limit) std.Io.Reader.StreamError!usize {
            const self: *@This() = @alignCast(@fieldParentPtr("interface", r));
            const dest = limit.slice(try w.writableSliceGreedy(1));
            const bytes_read = self.stream.read(dest) catch |err| {
                self.err = err;
                return error.ReadFailed;
            };

            if (bytes_read) |read_slice| {
                w.advance(read_slice.len);
                return read_slice.len;
            } else {
                return error.EndOfStream;
            }
        }
    };

    /// Get a reader to the stream.
    ///
    /// ## Function Parameters
    /// * `self`: Stream to get the reader to.
    ///
    /// ## Return Value
    /// Returns a reader.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn reader(
        self: Stream,
        buffer: []u8,
    ) Reader {
        return Reader.init(self, buffer);
    }

    /// Save all the data into an SDL data stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write all data to.
    /// * `data`: The data to be written.
    /// * `close_when_done`: Close the stream before returning, even if an error occurs.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn saveFile(
        self: Stream,
        data: []const u8,
        close_when_done: bool,
    ) !void {
        return errors.wrapCallBool(c.SDL_SaveFile_IO(
            self.value,
            data.ptr,
            data.len,
            close_when_done,
        ));
    }

    /// Seek within a data stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream structure.
    /// * `offset`: An offset in bytes, relative to whence location; can be negative.
    /// * `whence`: Where to seek from.
    ///
    /// ## Return Value
    /// Returns the final offset in the data stream after the seek.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn seek(
        self: Stream,
        offset: isize,
        whence: Whence,
    ) !usize {
        return @intCast(try errors.wrapCall(i64, c.SDL_SeekIO(
            self.value,
            @intCast(offset),
            @intFromEnum(whence),
        ), -1));
    }

    pub fn setProperties(
        self: Stream,
        props: Properties,
    ) !void {
        const val = try errors.wrapCall(c.SDL_PropertiesID, c.SDL_GetIOProperties(self.value), 0);
        const group = properties.Group{ .value = val };
        try props.setProperties(group);
    }

    /// Determine the current read/write offset in a data stream.
    ///
    /// ## Function Parameters
    /// * `self`: A data stream object from which to get the current offset.
    ///
    /// ## Return Value
    /// Returns the current offset in the stream.
    ///
    /// ## Remarks
    /// This is actually a wrapper function that calls the `io_stream.Stream.seek()` method, with an offset of `0` bytes from `io_stream.Whence.cur`,
    /// to simplify application development.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn tell(
        self: Stream,
    ) !usize {
        const ret = c.SDL_TellIO(
            self.value,
        );
        return @intCast(try errors.wrapCall(i64, ret, -1));
    }

    /// Write to a data stream.
    ///
    /// ## Function Parameters
    /// * `self`: Stream structure.
    /// * `data`: Data to write.
    /// * `size_written`: Size written, only populated when this function returns an error. It will be less than the `data.len`.
    ///
    /// ## Remarks
    /// This function writes exactly the data to the stream.
    /// If this fails for any reason, it'll return less than size to demonstrate how far the write progressed.
    ///
    /// On error, this function still attempts to write as much as possible, so it will fill `size_written` positive value less than the requested write size if there is an error.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn write(
        self: Stream,
        data: []const u8,
    ) !usize {
        const ret = c.SDL_WriteIO(self.value, data.ptr, data.len);
        if (ret < data.len) {
            const status = c.SDL_GetIOStatus(self.value);
            if (status != c.SDL_IO_STATUS_READY) {
                return switch (status) {
                    c.SDL_IO_STATUS_ERROR => error.Err,
                    c.SDL_IO_STATUS_NOT_READY => error.NotReady,
                    c.SDL_IO_STATUS_READONLY => error.ReadOnly,
                    c.SDL_IO_STATUS_WRITEONLY => error.WriteOnly,
                    else => blk: {
                        errors.callErrorCallback();
                        break :blk error.Err;
                    },
                };
            }
        }
        return ret;
    }

    /// Use this function to write a signed byte to a stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The byte value to write.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeS8(
        self: Stream,
        value: i8,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteS8(
            self.value,
            value,
        ));
    }

    /// Use this function to write 16 bits in native format to a stream as big-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in big-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeS16Be(
        self: Stream,
        value: i16,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteS16BE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 16 bits in native format to a stream as little-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in little-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeS16Le(
        self: Stream,
        value: i16,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteS16LE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 32 bits in native format to a stream as big-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in big-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeS32Be(
        self: Stream,
        value: i32,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteS32BE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 32 bits in native format to a stream as little-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in little-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeS32Le(
        self: Stream,
        value: i32,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteS32LE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 64 bits in native format to a stream as big-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in big-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeS64Be(
        self: Stream,
        value: i64,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteS64BE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 64 bits in native format to a stream as little-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in little-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeS64Le(
        self: Stream,
        value: i64,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteS64LE(
            self.value,
            value,
        ));
    }

    /// Use this function to write a byte to a stream.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The byte value to write.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeU8(
        self: Stream,
        value: u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteU8(
            self.value,
            value,
        ));
    }

    /// Use this function to write 16 bits in native format to a stream as big-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in big-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeU16Be(
        self: Stream,
        value: u16,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteU16BE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 16 bits in native format to a stream as little-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in little-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeU16Le(
        self: Stream,
        value: u16,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteU16LE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 32 bits in native format to a stream as big-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in big-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeU32Be(
        self: Stream,
        value: u32,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteU32BE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 32 bits in native format to a stream as little-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in little-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeU32Le(
        self: Stream,
        value: u32,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteU32LE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 64 bits in native format to a stream as big-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in big-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeU64Be(
        self: Stream,
        value: u64,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteU64BE(
            self.value,
            value,
        ));
    }

    /// Use this function to write 64 bits in native format to a stream as little-endian data.
    ///
    /// ## Function Parameters
    /// * `self`: The stream to write to.
    /// * `value`: The data to be written, in native format.
    ///
    /// ## Remarks
    /// SDL byteswaps the data only if necessary, so the application always specifies native format, and the data written will be in little-endian format.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeU64Le(
        self: Stream,
        value: u64,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteU64LE(
            self.value,
            value,
        ));
    }

    /// Stream writer type.
    ///
    /// ## Version
    /// This type is provided by zig-sdl3.
    pub const Writer = struct {
        stream: Stream,
        err: ?Error = null,
        interface: std.Io.Writer,

        pub fn init(stream: Stream, buffer: []u8) Writer {
            return .{
                .stream = stream,
                .interface = .{
                    .buffer = buffer,
                    .vtable = &.{ .drain = drainFn },
                },
            };
        }

        fn doWrite(self: *@This(), buf: []const u8) std.Io.Writer.Error!usize {
            return self.stream.write(buf) catch |err| {
                self.err = err;
                return error.WriteFailed;
            };
        }

        fn drainFn(w: *std.Io.Writer, data: []const []const u8, splat: usize) std.Io.Writer.Error!usize {
            const self: *@This() = @alignCast(@fieldParentPtr("interface", w));
            var written_total: usize = 0;

            const buffered = w.buffered();
            if (buffered.len > 0) {
                const n = try self.doWrite(buffered);
                written_total += n;
                if (n < buffered.len) return w.consume(written_total);
            }

            if (data.len > 0) {
                for (data[0 .. data.len - 1]) |d| {
                    const n = try self.doWrite(d);
                    written_total += n;
                    if (n < d.len) return w.consume(written_total);
                }

                const pattern = data[data.len - 1];
                for (0..splat) |_| {
                    const n = try self.doWrite(pattern);
                    written_total += n;
                    if (n < pattern.len) return w.consume(written_total);
                }
            }

            return w.consume(written_total);
        }
    };

    /// Get a writer to the stream.
    ///
    pub fn writer(
        self: Stream,
        buffer: []u8,
    ) Writer {
        return Writer.init(self, buffer);
    }
};

/// Possible `whence` values for `io_stream.Stream` seeking.
///
/// ## Remarks
/// These map to the same "whence" concept that `fseek` or `lseek` use in the standard C runtime.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Whence = enum(c.SDL_IOWhence) {
    /// Seek from the beginning of data.
    set = c.SDL_IO_SEEK_SET,
    /// Seek relative to current read point.
    cur = c.SDL_IO_SEEK_CUR,
    /// Seek relative to the end of data.
    end = c.SDL_IO_SEEK_END,
};

/// Load all the data from a file path.
///
/// ## Function Parameters
/// * `path`: The path to read all available data from.
///
/// ## Return Value
/// Returns the data.
/// This should be freed with `free()`.
///
/// ## Remarks
/// The data is null-terminated for convenience.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn loadFile(
    path: [:0]const u8,
) ![:0]u8 {
    var len: usize = undefined;
    return @as([*:0]u8, @ptrCast(@alignCast(try errors.wrapCallNull(*anyopaque, c.SDL_LoadFile(
        path.ptr,
        &len,
    )))))[0..len :0];
}

/// Save all the data into a file path.
///
/// ## Function Parameters
/// * `path`: The path to write all available data into.
/// * `data`: The data to write to the file.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn saveFile(
    path: [:0]const u8,
    data: []const u8,
) !void {
    return errors.wrapCallBool(c.SDL_SaveFile(
        path.ptr,
        data.ptr,
        data.len,
    ));
}

var test_global: bool = false;

fn testStreamClose(reader: ?*std.Io.Reader, writer: ?*std.Io.Writer, user_data: usize) anyerror!void {
    _ = reader;
    _ = writer;
    _ = user_data;
    test_global = true;
}

fn testStreamSeek(reader: ?*std.Io.Reader, writer: ?*std.Io.Writer, offset: isize, whence: Whence, user_data: usize) anyerror!usize {
    const seek_pos = switch (whence) {
        .cur => @as(usize, @intCast(@as(isize, @intCast(reader.?.seek)) + offset)),
        .set => @as(usize, @intCast(offset)),
        .end => user_data - @as(usize, @intCast(offset)),
    };
    reader.?.seek = seek_pos;
    writer.?.end = seek_pos;
    return seek_pos;
}

fn testStreamSize(reader: ?*std.Io.Reader, writer: ?*std.Io.Writer, user_data: usize) anyerror!usize {
    _ = reader;
    _ = writer;
    return user_data;
}

// Stream tests.
test "Stream" {
    errors.refAllDeclsRecursive(@This());

    const io = std.testing.io;

    // Test stream source.
    var buffer: [64]u8 = undefined;
    var reader = std.Io.Reader.fixed(&buffer);
    var writer = std.Io.Writer.fixed(&buffer);
    var reader_writer_data = Stream.InitFromReaderWriterData(usize){
        .io_reader = &reader,
        .io_writer = &writer,
        .user_data = buffer.len,
    };
    const stream = try Stream.initFromReaderWriter(usize, &testStreamClose, &testStreamSeek, &testStreamSize, &reader_writer_data);
    try stream.writeU8(7);
    try std.testing.expect(buffer[0] == 7);
    buffer[1] = 3;
    try std.testing.expectEqual(3, try stream.readU8());
    try std.testing.expectEqual(64, try stream.getSize());
    try std.testing.expectEqual(50, stream.seek(50, .set));
    try std.testing.expectEqual(41, stream.seek(23, .end));
    try std.testing.expectEqual(43, stream.seek(2, .cur));

    // Test writer/reader.
    _ = try stream.seek(0, .set);
    var writer_buffer: [64]u8 = undefined;
    var stream_writer = stream.writer(&writer_buffer);
    try stream_writer.interface.print("Hello {s}!", .{"World"});
    try stream_writer.interface.flush();

    _ = try stream.seek(0, .set);
    const hello_world = "Hello World!";
    var hello_world_buf: [hello_world.len]u8 = undefined;
    var reader_buffer: [64]u8 = undefined;
    var stream_reader = stream.reader(&reader_buffer);
    try stream_reader.interface.readSliceAll(&hello_world_buf);
    try std.testing.expectEqualStrings(hello_world, &hello_world_buf);

    try stream.deinit();
    try std.testing.expectEqual(true, test_global);

    // Reader/writer test 2.
    var reader_writer_data2 = Stream.InitFromReaderWriterData(void){
        .io_reader = null,
        .io_writer = &writer,
        .user_data = {},
    };
    reader.seek = 0;
    writer.end = 0;
    var stream2 = try Stream.initFromReaderWriter(void, null, null, null, &reader_writer_data2);
    test_global = false;
    try stream2.writeS8(-2);
    try std.testing.expectEqual(error.WriteOnly, stream2.readS8());
    try stream2.deinit();
    try std.testing.expectEqual(false, test_global);

    // Test file interface.
    if (@import("builtin").os.tag == .linux) {
        var tmp_dir = try std.Io.Dir.openDirAbsolute(io, "/tmp", .{});
        defer tmp_dir.close(io);

        const tmp_file = try tmp_dir.createFile(io, "iostream_test", .{ .read = true });
        var file_data = Stream.FsFileData{
            .io = io,
            .file = tmp_file,
        };
        const sdl_stream = try Stream.initFromFsFile(&file_data);
        try sdl_stream.writeU8(7);
        try sdl_stream.writeU8(3);
        try std.testing.expectEqual(0, sdl_stream.seek(0, .set));
        try std.testing.expectEqual(7, try sdl_stream.readU8());
        try std.testing.expectEqual(3, try sdl_stream.readU8());
        try std.testing.expectEqual(62, try sdl_stream.write(&([_]u8{0} ** 62)));
        try std.testing.expectEqual(64, try sdl_stream.getSize());
        try std.testing.expectEqual(50, sdl_stream.seek(50, .set));
        try std.testing.expectEqual(41, sdl_stream.seek(23, .end));
        try std.testing.expectEqual(43, sdl_stream.seek(2, .cur));
        try sdl_stream.deinit();
    }
}
