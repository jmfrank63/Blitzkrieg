const c = @import("c");
const errors = @import("errors.zig");
const filesystem = @import("filesystem.zig");
const properties = @import("properties.zig");
const std = @import("std");

/// Helper for storage paths.
///
/// ## Provided by zig-sdl3.
pub const Path = struct {
    data: std.array_list.Managed(u8),
    const separator: u8 = '/';

    /// Get the current base name of the path.
    ///
    /// ## Function Parameters
    /// * `self`: The path.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn baseName(
        self: Path,
    ) ?[:0]const u8 {
        if (self.data.items.len <= 1)
            return null;
        const start = if (std.mem.lastIndexOf(u8, self.data.items, &.{separator})) |val| val + 1 else 0;
        return self.data.items[start .. self.data.items.len - 1 :0];
    }

    /// Deinitialize the path.
    ///
    /// ## Function Parameters
    /// * `self`: The path.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn deinit(
        self: Path,
    ) void {
        self.data.deinit();
    }

    /// Get the current path.
    ///
    /// ## Return Value
    /// Returns the current path as a string.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn get(
        self: Path,
    ) [:0]const u8 {
        return @as([*:0]const u8, @ptrCast(self.data.items.ptr))[0 .. self.data.items.len - 1 :0];
    }

    /// Initialize a path.
    ///
    /// ## Function Parameters
    /// * `allocator`: The memory allocator to use.
    /// * `path`: Optional path to start out with. This must use the proper path separators and end with a path separator.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn init(
        allocator: std.mem.Allocator,
        path: ?[:0]const u8,
    ) !Path {
        if (path) |val| {
            var data = try std.array_list.Managed(u8).initCapacity(allocator, val.len + 1);
            data.appendSliceAssumeCapacity(val[0 .. val.len - 1]);
            data.appendAssumeCapacity(0);
            return .{
                .data = data,
            };
        } else {
            var data = try std.array_list.Managed(u8).initCapacity(allocator, 1);
            data.appendAssumeCapacity(0);
            return .{
                .data = data,
            };
        }
    }

    /// Join this path with a new one.
    ///
    /// ## Function Parameters
    /// * `self`: The path to join to.
    /// * `path`: The child path to join with. This should not have any path separators in it.
    ///
    /// ## Remarks
    /// This function can be undone by `Path.parent()`.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn join(
        self: *Path,
        path: []const u8,
    ) !void {
        try self.data.resize(self.data.items.len - 1);
        try self.data.append(separator);
        try self.data.appendSlice(path);
        try self.data.append(0);
    }

    /// Go up to the parent path.
    ///
    /// ## Function Parameters
    /// * `self`: The path to get the parent of.
    ///
    /// ## Return Value
    /// Returns if any going up was done.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn parent(
        self: *Path,
    ) bool {
        if (self.data.items.len <= 1)
            return false;
        if (std.mem.lastIndexOf(u8, self.data.items, &.{separator})) |val| {
            self.data.shrinkRetainingCapacity(val + 1);
            self.data.items[val] = 0;
            return true;
        }
        self.data.shrinkRetainingCapacity(1);
        self.data.items[0] = 1;
        return true;
    }
};

/// Function interface for `Storage`.
///
/// ## Remarks
/// Apps that want to supply a custom implementation of `Storage` will fill in all the functions in this struct,
/// and then pass it to `Storage.init()` to create a custom `Storage` object.
///
/// It is not usually necessary to do this; SDL provides standard implementations for many things you might expect to do with a `Storage`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub fn Interface(comptime UserData: type) type {
    return struct {
        /// Called when storage is closed.
        deinit: *const fn (user_data: ?*UserData) anyerror!void,

        /// Returns whether the storage is currently ready for access.
        ready: ?*const fn (user_data: ?*UserData) bool,

        /// Enumerate a directory, optional for write-only storage.
        enumerate: ?*const fn (
            user_data: ?*UserData,
            path: [:0]const u8,
            callback: ?*const fn (
                user_data_c: ?*anyopaque,
                dir_name_c: [*c]const u8,
                name_c: [*c]const u8,
            ) callconv(.c) c.SDL_EnumerationResult,
            callback_user_data: ?*anyopaque,
        ) callconv(.c) bool,

        /// Get path information, optional for write-only storage.
        info: ?*const fn (user_data: ?*UserData, path: [:0]const u8) anyerror!filesystem.PathInfo,

        /// Read a file from storage, optional for write-only storage.
        read_file: ?*const fn (user_data: ?*UserData, path: [:0]const u8, dst: []u8) anyerror!void,

        /// Write a file to storage, optional for read-only storage.
        write_file: ?*const fn (user_data: ?*UserData, path: [:0]const u8, src: []const u8) anyerror!void,

        /// Create a directory, optional for read-only storage.
        mkdir: ?*const fn (user_data: ?*UserData, path: [:0]const u8) anyerror!void,

        /// Remove a file or empty directory, optional for read-only storage.
        remove: ?*const fn (user_data: ?*UserData, path: [:0]const u8) anyerror!void,

        /// Rename a path, optional for read-only storage.
        rename: ?*const fn (user_data: ?*UserData, old_path: [:0]const u8, new_path: [:0]const u8) anyerror!void,

        /// Copy a file, optional for read-only storage.
        copy: ?*const fn (user_data: ?*UserData, old_path: [:0]const u8, new_path: [:0]const u8) anyerror!void,

        /// Get the space remaining, optional for read-only storage.
        space_remaining: ?*const fn (user_data: ?*UserData) u64,
    };
}

/// An abstract interface for filesystem access.
///
/// ## Remarks
/// This is an opaque datatype.
/// One can create this object using standard SDL functions like `storage.openTitle()` or `storage.openUser()`, etc,
/// or create an object with a custom implementation using `storage.open()`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Storage = extern struct {
    value: *c.SDL_Storage,

    /// Copy a file in a writable storage container.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container.
    /// * `old_path`: The old path.
    /// * `new_path`: The new path.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn copyFile(
        self: Storage,
        old_path: [:0]const u8,
        new_path: [:0]const u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_CopyStorageFile(self.value, old_path.ptr, new_path.ptr));
    }

    /// Create a directory in a writable storage container.
    ///
    /// ## Function Parameters
    /// * `storage`: A storage container.
    /// * `path`: The path of the directory to create.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn createDirectory(
        self: Storage,
        path: [:0]const u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_CreateStorageDirectory(self.value, path.ptr));
    }

    /// Closes and frees a storage container.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container to close.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Storage,
    ) !void {
        return errors.wrapCallBool(c.SDL_CloseStorage(self.value));
    }

    /// Opens up a container using a client-provided storage interface.
    ///
    /// ## Function Parameters
    /// * `UserData`: Type of user data.
    /// * `interface`: The interface that implements this storage.
    /// * `user_data`: The pointer that will be passed to the interface functions.
    ///
    /// ## Remarks
    /// Applications do not need to use this function unless they are providing their own `Storage` implementation.
    /// If you just need an `Storage`, you should use the built-in implementations in SDL, like `Storage.initTitle()` or `Storage.initUser()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        comptime UserData: type,
        comptime interface: Interface(UserData),
        user_data: ?*UserData,
    ) !Storage {
        const Cb = struct {
            fn deinit(user_data_c: ?*anyopaque) callconv(.c) bool {
                interface.deinit(@ptrCast(@alignCast(user_data_c))) catch return false;
                return true;
            }
            fn ready(user_data_c: ?*anyopaque) callconv(.c) bool {
                return interface.ready.?(@ptrCast(@alignCast(user_data_c)));
            }
            fn enumerate(
                user_data_c: ?*anyopaque,
                path: [*c]const u8,
                callback: ?*const fn (
                    user_data_c: ?*anyopaque,
                    dir_name_c: [*c]const u8,
                    name_c: [*c]const u8,
                ) callconv(.c) c.SDL_EnumerationResult,
                callback_user_data: ?*anyopaque,
            ) callconv(.c) bool {
                interface.enumerate.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(path), callback, callback_user_data) catch return false;
                return true;
            }
            fn info(user_data_c: ?*anyopaque, path: [*c]const u8, path_info: [*c]c.SDL_PathInfo) callconv(.c) bool {
                const ret = interface.info.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(path)) catch return false;
                path_info.* = ret.toSdl();
                return true;
            }
            fn read_file(user_data_c: ?*anyopaque, path: [*c]const u8, destination: ?*anyopaque, length: u64) callconv(.c) bool {
                interface.read_file.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(path), @as([*]u8, @ptrCast(destination))[0..@intCast(length)]) catch return false;
                return true;
            }
            fn write_file(user_data_c: ?*anyopaque, path: [*c]const u8, source: ?*const anyopaque, length: u64) callconv(.c) bool {
                interface.write_file.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(path), @as([*]const u8, @ptrCast(source))[0..@intCast(length)]) catch return false;
                return true;
            }
            fn mkdir(user_data_c: ?*anyopaque, path: [*c]const u8) callconv(.c) bool {
                interface.mkdir.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(path)) catch return false;
                return true;
            }
            fn remove(user_data_c: ?*anyopaque, path: [*c]const u8) callconv(.c) bool {
                interface.remove.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(path)) catch return false;
                return true;
            }
            fn rename(user_data_c: ?*anyopaque, old_path: [*c]const u8, new_path: [*c]const u8) callconv(.c) bool {
                interface.rename.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(old_path), std.mem.span(new_path)) catch return false;
                return true;
            }
            fn copy(user_data_c: ?*anyopaque, old_path: [*c]const u8, new_path: [*c]const u8) callconv(.c) bool {
                interface.copy.?(@ptrCast(@alignCast(user_data_c)), std.mem.span(old_path), std.mem.span(new_path)) catch return false;
                return true;
            }
            fn space_remaining(user_data_c: ?*anyopaque) callconv(.c) u64 {
                return interface.space_remaining.?(@ptrCast(@alignCast(user_data_c)));
            }
        };
        const iface = c.SDL_StorageInterface{
            .close = Cb.deinit,
            .ready = if (interface.ready != null) Cb.ready else null,
            .enumerate = if (interface.enumerate != null) Cb.enumerate else null,
            .info = if (interface.info != null) Cb.info else null,
            .read_file = if (interface.read_file != null) Cb.read_file else null,
            .write_file = if (interface.write_file != null) Cb.write_file else null,
            .mkdir = if (interface.mkdir != null) Cb.mkdir else null,
            .remove = if (interface.remove != null) Cb.remove else null,
            .rename = if (interface.rename != null) Cb.rename else null,
            .copy = if (interface.copy != null) Cb.copy else null,
            .space_remaining = if (interface.space_remaining != null) Cb.space_remaining else null,
        };
        return .{
            .value = try errors.wrapCallNull(*c.SDL_Storage, c.SDL_OpenStorage(&iface, @ptrCast(user_data))),
        };
    }

    /// Opens up a container for local filesystem storage.
    ///
    /// ## Function Parameters
    /// * `path`: Base path prepended to all storage paths, or `null` for no base path.
    ///
    /// ## Return Value
    /// Returns a filesystem storage container.
    ///
    /// ## Remarks
    /// This is provided for development and tools.
    /// Portable applications should use `Storage.initTitle()` for access to game data and `Storage.initUser()` for access to user data.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFile(
        path: ?[:0]const u8,
    ) !Storage {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_Storage, c.SDL_OpenFileStorage(if (path) |val| val.ptr else null)),
        };
    }

    /// Opens up a read-only container for the application's filesystem.
    ///
    /// ## Function Parameters
    /// * `override`: A path to override the backend's default title root.
    /// * `props`: A property list that may contain backend-specific information.
    ///
    /// ## Return Value
    /// Returns a title storage container
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initTitle(
        override: ?[:0]const u8,
        props: ?properties.Group,
    ) !Storage {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_Storage, c.SDL_OpenTitleStorage(if (override) |val| val.ptr else null, if (props) |val| val.value else 0)),
        };
    }

    /// Opens up a container for a user's unique read/write filesystem.
    ///
    /// ## Function Parameters
    /// * `org`: The name of your organization.
    /// * `app`: The name of your application.
    /// * `props`: A property list that may contain backend-specific information.
    ///
    /// ## Return Value
    /// Returns a user storage container.
    ///
    /// ## Remarks
    /// when the client is ready to read/write files.
    /// This allows the backend to properly batch file operations and flush them when the container has been closed; ensuring safe and optimal save I/O.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initUser(
        org: [:0]const u8,
        app: [:0]const u8,
        props: ?properties.Group,
    ) !Storage {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_Storage, c.SDL_OpenUserStorage(org.ptr, app.ptr, if (props) |val| val.value else 0)),
        };
    }

    /// Enumerate a directory in a storage container through a callback function.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container.
    /// * `path`: The path of the directory to enumerate or `null` for root.
    /// * `UserData`: Type for user data.
    /// * `callback`: A function that is called for each entry in the directory.
    /// * `user_data`: A pointer that is passed to callback.
    ///
    /// ## Remarks
    /// This function provides every directory entry through an app-provided callback, called once for each directory entry,
    /// until all results have been provided or the callback returns either `filesystem.EnumerationResult.success` or `filesystem.EnumerationResult.failure`.
    ///
    /// This will return and error if there was a system problem in general, or if a callback returns `filesystem.EnumerationResult.failure`.
    /// A successful return means a callback returned `filesystem.EnumerationResult.success` to halt enumeration, or all directory entries were enumerated.
    ///
    /// If path is `null`, this is treated as a request to enumerate the root of the storage container's tree.
    /// An empty string also works for this.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn enumerateDirectory(
        storage: Storage,
        path: ?[:0]const u8,
        comptime UserData: type,
        comptime callback: filesystem.EnumerateDirectoryCallback(UserData),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            pub fn run(
                user_data_c: ?*anyopaque,
                dir_name_c: [*c]const u8,
                name_c: [*c]const u8,
            ) callconv(.c) c.SDL_EnumerationResult {
                return @intFromEnum(callback(@ptrCast(@alignCast(user_data_c)), std.mem.span(dir_name_c), std.mem.span(name_c)));
            }
        };
        return errors.wrapCallBool(c.SDL_EnumerateStorageDirectory(storage.value, if (path) |val| val.ptr else null, Cb.run, @ptrCast(user_data)));
    }

    /// Data for getting all properties.
    const GetAllData = struct {
        allocator: std.mem.Allocator,
        arr: *std.array_list.Managed([:0]const u8),
        err: ?std.mem.Allocator.Error,
    };

    /// Callback for getting all directory items.
    fn getAllDirectoryItemsCb(user_data: ?*GetAllData, dir_name: [:0]const u8, name: [:0]const u8) filesystem.EnumerationResult {
        _ = dir_name;
        const data_ptr: *GetAllData = @ptrCast(@alignCast(user_data));
        const copy = data_ptr.allocator.allocSentinel(u8, name.len, 0) catch |err| {
            data_ptr.err = err;
            return .failure;
        };
        @memcpy(copy, name);
        data_ptr.arr.append(copy) catch |err| {
            data_ptr.err = err;
            return .failure;
        };
        return .run;
    }

    /// Free all directory items obtained through `filesystem.getAllDirectoryItems()`.
    ///
    /// ## Function Parameters
    /// * `allocator`: Memory allocator originally used to allocate storage for items.
    /// * `items`: Items to free.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn freeAllDirectoryItems(
        allocator: std.mem.Allocator,
        items: std.array_list.Managed([:0]const u8),
    ) void {
        for (items.items) |item| {
            allocator.free(item);
        }
        items.deinit();
    }

    /// Get all the items in a directory.
    ///
    /// ## Function Parameters
    /// * `self`: The storage to get all.
    /// * `allocator`: Memory allocator to use to allocate storage for items.
    /// * `path`: Path to iterate over.
    ///
    /// ## Return Value
    /// Returns a list of all the items in the directory.
    /// This should be freed by `filesystem.freeAllDirectoryItems()`.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getAllDirectoryItems(
        self: Storage,
        allocator: std.mem.Allocator,
        path: [:0]const u8,
    ) !std.array_list.Managed([:0]const u8) {
        var arr = std.array_list.Managed([:0]const u8).init(allocator);
        var data = GetAllData{
            .allocator = allocator,
            .arr = &arr,
            .err = null,
        };
        try self.enumerateDirectory(path, GetAllData, getAllDirectoryItemsCb, &data);
        return arr;
    }

    /// Query the size of a file within a storage container.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container to query.
    /// * `path`: The relative path of the file to query.
    ///
    /// ## Return Value
    /// Returns the file's length.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFileSize(
        self: Storage,
        path: [:0]const u8,
    ) !u64 {
        var size: u64 = undefined;
        try errors.wrapCallBool(c.SDL_GetStorageFileSize(self.value, path.ptr, &size));
        return size;
    }

    /// Get information about if a filesystem path in a storage container exists.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container to query.
    /// * `path`: The path to query.
    ///
    /// ## Return Value
    /// Returns if the path exists.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPathExists(
        self: Storage,
        path: [:0]const u8,
    ) bool {
        return c.SDL_GetStoragePathInfo(self.value, path.ptr, null);
    }

    /// Get information about a filesystem path in a storage container.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container to query.
    /// * `path`: The path to query.
    ///
    /// ## Return Value
    /// Returns the path info for the path.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPathInfo(
        self: Storage,
        path: [:0]const u8,
    ) !filesystem.PathInfo {
        var info: c.SDL_PathInfo = undefined;
        try errors.wrapCallBool(c.SDL_GetStoragePathInfo(self.value, path.ptr, &info));
        return filesystem.PathInfo.fromSdl(info);
    }

    /// Queries the remaining space in a storage container.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container to query.
    ///
    /// ## Return Value
    /// Returns the amount of remaining space, in bytes.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSpaceRemaining(
        self: Storage,
    ) u64 {
        return c.SDL_GetStorageSpaceRemaining(self.value);
    }

    /// Enumerate a directory tree, filtered by pattern, and return a list.
    ///
    /// ## Function Parameters
    /// * `storage`: A storage container.
    /// * `path`: The path of the directory to enumerate, or `null` for root.
    /// * `pattern`: The pattern that files in the directory must match. Can be `null`.
    /// * `flags`: Flags to effect the search.
    ///
    /// ## Return Value
    /// Returns a slice of strings on success.
    /// This should be freed with `free()`
    ///
    /// ## Remarks
    /// Files are filtered out if they don't match the string in pattern, which may contain wildcard characters '*' (match everything) and '?' (match one character).
    /// If pattern is `null`, no filtering is done and all results are returned.
    /// Subdirectories are permitted, and are specified with a path separator of '/'.
    /// Wildcard characters '*' and '?' never match a path separator.
    ///
    /// The `flags` may have `case_insensitive` to `true` to make the pattern matching case-insensitive.
    ///
    /// If path is `null`, this is treated as a request to enumerate the root of the storage container's tree.
    /// An empty string also works for this.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn globDirectory(
        self: Storage,
        path: ?[:0]const u8,
        pattern: ?[:0]const u8,
        flags: filesystem.GlobFlags,
    ) ![][*:0]u8 {
        var count: c_int = undefined;
        const ret: [*][*:0]u8 = @ptrCast(try errors.wrapCallCPtr([*c]u8, c.SDL_GlobStorageDirectory(
            self.value,
            if (path) |val| val.ptr else null,
            if (pattern) |val| val.ptr else null,
            flags.toSdl(),
            &count,
        )));
        return ret[0..@intCast(count)];
    }

    /// Synchronously read a file from a storage container into a client-provided buffer.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container to read from.
    /// * `path`: The relative path of the file to read.
    /// * `destination`: A client-provided buffer to read the file into.
    ///
    /// ## Remarks
    /// The value of length must match the length of the file exactly; call `Storage.getFileSize()` to get this value.
    /// This behavior may be relaxed in a future release.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readFile(
        self: Storage,
        path: [:0]const u8,
        destination: []u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_ReadStorageFile(self.value, path.ptr, destination.ptr, @intCast(destination.len)));
    }

    /// Remove a file or an empty directory in a writable storage container.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container.
    /// * `path`: The path to remove.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn removePath(
        self: Storage,
        path: [:0]const u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_RemoveStoragePath(self.value, path.ptr));
    }

    /// Rename a file or directory in a writable storage container.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container.
    /// * `old_path`: The old path.
    /// * `new_path`: The new path.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn renamePath(
        self: Storage,
        old_path: [:0]const u8,
        new_path: [:0]const u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_RenameStoragePath(self.value, old_path, new_path));
    }

    /// Checks if the storage container is ready to use.
    ///
    /// ## Function Parameters
    /// * `self`: A storage container to query.
    ///
    /// ## Return Value
    /// Returns true if the container is ready, false otherwise.
    ///
    /// ## Remarks
    /// This function should be called in regular intervals until it returns true - however, it is not recommended to spinwait on this call,
    /// as the backend may depend on a synchronous message loop.
    /// You might instead poll this in your game's main loop while processing events and drawing a loading screen.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn ready(
        self: Storage,
    ) bool {
        return c.SDL_StorageReady(self.value);
    }

    /// Synchronously write a file from client memory into a storage container.
    ///
    /// ## Function Parameters
    /// * `storage`: A storage container to write to.
    /// * `path`: The relative path of the file to write.
    /// * `source`: A client-provided buffer to write from.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writeFile(
        self: Storage,
        path: [:0]const u8,
        source: []const u8,
    ) !void {
        return errors.wrapCallBool(c.SDL_WriteStorageFile(self.value, path.ptr, source.ptr, @intCast(source.len)));
    }
};

// Storage testing.
test "Storage" {
    errors.refAllDeclsRecursive(@This());

    // Test path helper.
    var path = try Path.init(std.testing.allocator, "/home/gota/test/");
    defer path.deinit();
    try std.testing.expectEqualStrings("/home/gota/test", path.get());
    try std.testing.expectEqualStrings("test", path.baseName().?);
    try path.join("file.txt");
    try std.testing.expectEqualStrings("/home/gota/test/file.txt", path.get());
    try std.testing.expectEqualStrings("file.txt", path.baseName().?);
    try std.testing.expectEqual(true, path.parent());
    try std.testing.expectEqualStrings("/home/gota/test", path.get());
    try std.testing.expectEqualStrings("test", path.baseName().?);
    try std.testing.expectEqual(true, path.parent());
    try std.testing.expectEqualStrings("/home/gota", path.get());
    try std.testing.expectEqualStrings("gota", path.baseName().?);
    try std.testing.expectEqual(true, path.parent());
    try std.testing.expectEqualStrings("/home", path.get());
    try std.testing.expectEqualStrings("home", path.baseName().?);
    try std.testing.expectEqual(true, path.parent());
    try std.testing.expectEqualStrings("", path.get());
    try std.testing.expectEqual(null, path.baseName());
    try std.testing.expectEqual(false, path.parent());
    try std.testing.expectEqualStrings("", path.get());
    try std.testing.expectEqual(null, path.baseName());

    // Test path 2.
    var path2 = try Path.init(std.testing.allocator, null);
    defer path2.deinit();
    try std.testing.expectEqual(false, path.parent());
    try std.testing.expectEqualStrings("", path.get());
    try std.testing.expectEqual(null, path.baseName());
}
