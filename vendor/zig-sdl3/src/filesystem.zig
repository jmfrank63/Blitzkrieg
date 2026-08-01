const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");
const time = @import("time.zig");

/// Helper for filesystem paths.
///
/// ## Provided by zig-sdl3.
pub const Path = struct {
    data: std.array_list.Managed(u8),
    separator: u8,

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
        const start = if (std.mem.lastIndexOf(u8, self.data.items, &.{self.separator})) |val| val + 1 else 0;
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

    /// Get the path separator.
    ///
    /// ## Return Value
    /// Returns the path separator (usually `\` for Windows and `/` for others).
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getSeparator() !u8 {
        const base = try getBasePath();
        return base[base.len - 1];
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
                .separator = val[val.len - 1],
            };
        } else {
            var data = try std.array_list.Managed(u8).initCapacity(allocator, 1);
            const separator = try getSeparator();
            data.appendAssumeCapacity(0);
            return .{
                .data = data,
                .separator = separator,
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
        try self.data.append(self.separator);
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
        if (std.mem.lastIndexOf(u8, self.data.items, &.{self.separator})) |val| {
            self.data.shrinkRetainingCapacity(val + 1);
            self.data.items[val] = 0;
            return true;
        }
        self.data.shrinkRetainingCapacity(1);
        self.data.items[0] = 1;
        return true;
    }
};

/// Callback for directory enumeration.
///
/// ## Function Parameters
/// * `user_data`: An app-controlled pointer that is passed to the callback.
/// * `dir_name`: The directory that is being enumerated.
/// * `name`: The next entry in the enumeration.
///
/// ## Return Value
/// Returns how the enumeration should proceed.
///
/// ## Remarks
/// Enumeration of directory entries will continue until either all entries have been provided to the callback, or the callback has requested a stop through its return value.
///
/// Returning `filesystem.EnumerationResult.run` will let enumeration proceed, calling the callback with further entries.
/// `filesystem.EnumerationResult.success` and `filesystem.EnumerationResult.failure` will terminate the enumeration early,
/// and dictate the return value of the enumeration function itself.
///
/// The `dir_name` is guaranteed to end with a path separator ('\' on Windows, '/' on most other platforms).
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn EnumerateDirectoryCallback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        dir_name: [:0]const u8,
        name: [:0]const u8,
    ) EnumerationResult;
}

/// Possible results from an enumeration callback.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const EnumerationResult = enum(c.SDL_EnumerationResult) {
    /// Value that requests that enumeration continue.
    run = c.SDL_ENUM_CONTINUE,
    /// Value that requests that enumeration stop, successfully.
    success = c.SDL_ENUM_SUCCESS,
    /// Value that requests that enumeration stop, as a failure.
    failure = c.SDL_ENUM_FAILURE,
};

/// The type of the OS-provided default folder for a specific purpose.
///
/// ## Remarks
/// Note that the Trash folder isn't included here, because trashing files usually involves extra OS-specific functionality to remember the file's original location.
///
/// The folders supported per platform are:
///
/// | Folder | Windows | macOS/iOS | tvOS | Unix (XDG) | Haiku | Emscripten |
/// |--------|---------|-----------|------|------------|-------|------------|
/// | HOME        | X      | X  |   | X | X | X |
/// | DESKTOP     | X      | X  |   | X | X |   |
/// | DOCUMENTS   | X      | X  |   | X |   |   |
/// | DOWNLOADS   | Vista+ | X  |   | X |   |   |
/// | MUSIC       | X      | X  |   | X |   |   |
/// | PICTURES    | X      | X  |   | X |   |   |
/// | PUBLICSHARE |        | X  |   | X |   |   |
/// | SAVEDGAMES  | Vista+ |    |   |   |   |   |
/// | SCREENSHOTS | Vista+ |    |   |   |   |   |
/// | TEMPLATES   | X      | X  |   | X |   |   |
/// | VIDEOS      | X      | X* |   | X |   |   |
///
/// X*: Note that on macOS/iOS, the Videos folder is called "Movies".
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Folder = enum(c.SDL_Folder) {
    /// The folder which contains all of the current user's data, preferences, and documents.
    /// It usually contains most of the other folders.
    /// If a requested folder does not exist, the home folder can be considered a safe fallback to store a user's documents.
    home = c.SDL_FOLDER_HOME,
    /// The folder of files that are displayed on the desktop.
    /// Note that the existence of a desktop folder does not guarantee that the system does show icons on its desktop;
    /// certain GNU/Linux distros with a graphical environment may not have desktop icons.
    desktop = c.SDL_FOLDER_DESKTOP,
    /// User document files, possibly application-specific.
    /// This is a good place to save a user's projects.
    documents = c.SDL_FOLDER_DOCUMENTS,
    /// Standard folder for user files downloaded from the internet.
    downloads = c.SDL_FOLDER_DOWNLOADS,
    /// Music files that can be played using a standard music player (mp3, ogg...)./
    music = c.SDL_FOLDER_MUSIC,
    /// Image files that can be displayed using a standard viewer (png, jpg...).
    pictures = c.SDL_FOLDER_PICTURES,
    /// Files that are meant to be shared with other users on the same computer.
    public_share = c.SDL_FOLDER_PUBLICSHARE,
    /// Save files for games.
    saved_games = c.SDL_FOLDER_SAVEDGAMES,
    /// Application screenshots.
    screenshots = c.SDL_FOLDER_SCREENSHOTS,
    /// Template files to be used when the user requests the desktop environment to create a new file in a certain folder, such as "New Text File.txt".
    /// Any file in the Templates folder can be used as a starting point for a new file.
    templates = c.SDL_FOLDER_TEMPLATES,
    /// Video files that can be played using a standard video player (mp4, webm...).
    videos = c.SDL_FOLDER_VIDEOS,
};

/// Flags for path matching.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const GlobFlags = struct {
    case_insensitive: bool = false,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_GlobFlags) GlobFlags {
        return .{
            .case_insensitive = value & c.SDL_GLOB_CASEINSENSITIVE > 0,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: GlobFlags) c.SDL_GlobFlags {
        var ret: c.SDL_GlobFlags = 0;
        if (self.case_insensitive)
            ret |= c.SDL_GLOB_CASEINSENSITIVE;
        return ret;
    }
};

/// Information about a path on the filesystem.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const PathInfo = struct {
    /// The path type.
    path_type: PathType,
    /// The file size in bytes.
    file_size: u64,
    /// The time when the path was created.
    create_time: time.Time,
    /// The last time when the path was modified.
    modify_time: time.Time,
    /// The last time when the path was read.
    access_time: time.Time,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_PathInfo) PathInfo {
        return .{
            .path_type = PathType.fromSdl(value.type).?,
            .file_size = value.size,
            .create_time = .{ .value = value.create_time },
            .modify_time = .{ .value = value.modify_time },
            .access_time = .{ .value = value.access_time },
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: PathInfo) c.SDL_PathInfo {
        return .{
            .type = PathType.toSdl(self.path_type),
            .size = self.file_size,
            .create_time = self.create_time.value,
            .modify_time = self.modify_time.value,
            .access_time = self.access_time.value,
        };
    }
};

/// Types of filesystem entries.
///
/// ## Remarks
/// Note that there may be other sorts of items on a filesystem: devices, symlinks, named pipes, etc.
/// They are currently reported as `filesystem.PathType.other`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const PathType = enum(c.SDL_PathType) {
    /// A normal file.
    file = c.SDL_PATHTYPE_FILE,
    /// A directory.
    directory = c.SDL_PATHTYPE_DIRECTORY,
    /// Something completely different like a device node (not a symlink, those are always followed).
    other = c.SDL_PATHTYPE_OTHER,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_PathType) ?PathType {
        if (value == c.SDL_PATHTYPE_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?PathType) c.SDL_PathType {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_PATHTYPE_NONE;
    }
};

/// Copy a file.
///
/// ## Function Parameters
/// * `old_path`: The old path.
/// * `new_path`: The new path.
///
/// ## Remarks
/// If the file at newpath already exists, it will be overwritten with the contents of the file at oldpath.
///
/// This function will block until the copy is complete, which might be a significant time for large files on slow disks.
/// On some platforms, the copy can be handed off to the OS itself, but on others SDL might just open both paths, and read from one and write to the other.
///
/// Note that this is not an atomic operation!
/// If something tries to read from `new_path` while the copy is in progress, it will see an incomplete copy of the data,
/// and if the calling thread terminates (or the power goes out) during the copy, `new_path`'s previous contents will be gone, replaced with an incomplete copy of the data.
/// To avoid this risk, it is recommended that the app copy to a temporary file in the same directory as `new_path`, and if the copy is successful,
/// use `filesystem.renamePath` to replace `new_path` with the temporary file.
/// This will ensure that reads of `new_path` will either see a complete copy of the data, or it will see the pre-copy state of `new_path`.
///
/// This function attempts to synchronize the newly-copied data to disk before returning, if the platform allows it,
/// so that the renaming trick will not have a problem in a system crash or power failure,
/// where the file could be renamed but the contents never made it from the system file cache to the physical disk.
///
/// If the copy fails for any reason, the state of `new_path` is undefined.
/// It might be half a copy, it might be the untouched data of what was already there, or it might be a zero-byte file, etc.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn copyFile(
    old_path: [:0]const u8,
    new_path: [:0]const u8,
) !void {
    return errors.wrapCallBool(c.SDL_CopyFile(old_path.ptr, new_path.ptr));
}

/// Create a directory, and any missing parent directories.
///
/// ## Function Parameters
/// * `path`: The path of the directory to create.
///
/// ## Remarks
/// This reports success if `path` already exists as a directory.
///
/// If parent directories are missing, it will also create them.
/// Note that if this fails, it will not remove any parent directories it already made.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn createDirectory(
    path: [:0]const u8,
) !void {
    return errors.wrapCallBool(c.SDL_CreateDirectory(path.ptr));
}

/// Enumerate a directory through a callback function.
///
/// ## Function Parameters
/// * `path`: The path of the directory to enumerate.
/// * `UserData`: Type representing user data.
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
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn enumerateDirectory(
    path: [:0]const u8,
    comptime UserData: type,
    comptime callback: EnumerateDirectoryCallback(UserData),
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
    return errors.wrapCallBool(c.SDL_EnumerateDirectory(path.ptr, Cb.run, @ptrCast(user_data)));
}

/// Data for getting all properties.
const GetAllData = struct {
    allocator: std.mem.Allocator,
    arr: *std.array_list.Managed([:0]const u8),
    err: ?std.mem.Allocator.Error,
};

/// Callback for getting all directory items.
fn getAllDirectoryItemsCb(user_data: ?*GetAllData, dir_name: [:0]const u8, name: [:0]const u8) EnumerationResult {
    _ = dir_name;
    const data_ptr: *GetAllData = user_data.?;
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
    allocator: std.mem.Allocator,
    path: [:0]const u8,
) !std.array_list.Managed([:0]const u8) {
    var arr = std.array_list.Managed([:0]const u8).init(allocator);
    var data = GetAllData{
        .allocator = allocator,
        .arr = &arr,
        .err = null,
    };
    try enumerateDirectory(path, GetAllData, getAllDirectoryItemsCb, &data);
    return arr;
}

/// Get the directory where the application was run from.
///
/// ## Return Value
/// Returns an absolute path in UTF-8 encoding to the application data directory.
/// An error will be returned when the platform doesn't implement this functionality.
///
/// ## Remarks
/// SDL caches the result of this call internally, but the first call to this function is not necessarily fast, so plan accordingly.
///
/// ### macOS and iOS Specific Functionality:
/// If the application is in a ".app" bundle, this function returns the Resource directory (e.g. `MyApp.app/Contents/Resources/`).
/// This behaviour can be overridden by adding a property to the Info.plist file.
/// Adding a string key with the name `SDL_FILESYSTEM_BASE_DIR_TYPE` with a supported value will change the behaviour.
///
/// Supported values for the `SDL_FILESYSTEM_BASE_DIR_TYPE` property (Given an application in /Applications/SDLApp/MyApp.app):
/// * `resource`: Bundle resource directory (the default). For example: `/Applications/SDLApp/MyApp.app/Contents/Resources`
/// * `bundle`: The Bundle directory. For example: `/Applications/SDLApp/MyApp.app/`
/// * `parent`: The containing directory of the bundle. For example: `/Applications/SDLApp/`
///
/// ### Nintendo 3DS Specific Functionality:
/// This function returns "romfs" directory of the application as it is uncommon to store resources outside the executable.
/// As such it is not a writable directory.
///
/// The returned path is guaranteed to end with a path separator ('\' on Windows, '/' on most other platforms).
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getBasePath() ![:0]const u8 {
    return errors.wrapCallCString(c.SDL_GetBasePath());
}

/// Get what the system believes is the "current working directory."
///
/// ## Return Value
/// Returns a UTF-8 string of the current working directory in platform-dependent notation.
/// This should be freed with `free()`.
///
/// ## Remarks
/// For systems without a concept of a current working directory, this will still attempt to provide something reasonable.
///
/// SDL does not provide a means to change the current working directory; for platforms without this concept, this would cause surprises with file access outside of SDL.
///
/// The returned path is guaranteed to end with a path separator ('\' on Windows, '/' on most other platforms).
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getCurrentDirectory() ![:0]u8 {
    const ret: [*:0]u8 = @ptrCast(try errors.wrapCallCPtr(u8, c.SDL_GetCurrentDirectory()));
    return std.mem.span(ret);
}

/// Check if a path exists.
///
/// ## Function Parameters
/// * `path`: The path to query.
///
/// ## Return Value
/// Returns if the path exists.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPathExists(
    path: [:0]const u8,
) bool {
    return c.SDL_GetPathInfo(path.ptr, null);
}

/// Get information about a filesystem path.
///
/// ## Function Parameters
/// * `path`: The path to query.
///
/// ## Return Value
/// Returns information about the path.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPathInfo(
    path: [:0]const u8,
) !PathInfo {
    var info: c.SDL_PathInfo = undefined;
    try errors.wrapCallBool(c.SDL_GetPathInfo(path.ptr, &info));
    return PathInfo.fromSdl(info);
}

/// Get the user-and-app-specific path where files can be written.
///
/// ## Function Parameters
/// * `org`: The name of your organization.
/// * `app`: The name of your application.
///
/// ## Return Value
/// Returns a UTF-8 string of the user directory in platform-dependent notation.
/// This should be freed with `free()`.
///
/// ## Remarks
/// Get the "pref dir".
/// This is meant to be where users can write personal files (preferences and save games, etc) that are specific to your application.
/// This directory is unique per user, per application.
///
/// This function will decide the appropriate location in the native filesystem, create the directory if necessary,
/// and return a string of the absolute path to the directory in UTF-8 encoding.
///
/// On Windows, the string might look like:
/// `C:\\Users\\bob\\AppData\\Roaming\\My Company\\My Program Name\\`
///
/// On Linux, the string might look like:
/// `/home/bob/.local/share/My Program Name/`
///
/// On macOS, the string might look like:
/// `/Users/bob/Library/Application Support/My Program Name/`
///
/// You should assume the path returned by this function is the only safe place to write files
/// (and that `filesystem.getBasePath()`, while it might be writable, or even the parent of the returned path, isn't where you should be writing things).
///
/// Both the `org` and `app` strings may become part of a directory name, so please follow these rules:
/// * Try to use the same `org` string (including case-sensitivity) for all your applications that use this function.
/// * Always use a unique `app` string for each one, and make sure it never changes for an app once you've decided on it.
/// * Unicode characters are legal, as long as they are UTF-8 encoded, but...
/// * ...only use letters, numbers, and spaces. Avoid punctuation like "Game Name 2: Bad Guy's Revenge!" ... "Game Name 2" is sufficient.
/// * The returned path is guaranteed to end with a path separator ('\' on Windows, '/' on most other platforms).
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPrefPath(
    org: [:0]const u8,
    app: [:0]const u8,
) ![:0]u8 {
    return errors.wrapCallCStringMut(c.SDL_GetPrefPath(org.ptr, app.ptr));
}

/// Finds the most suitable user folder for a specific purpose.
///
/// ## Function Parameters
/// * `folder`: The type of folder to find.
///
/// ## Return Value
/// Returns a full path to the folder requested.
///
/// ## Remarks
/// Many OSes provide certain standard folders for certain purposes, such as storing pictures, music or videos for a certain user.
/// This function gives the path for many of those special locations.
///
/// This function is specifically for user folders, which are meant for the user to access and manage.
/// For application-specific folders, meant to hold data for the application to manage, see `filesystem.getBasePath()` and `filesystem.getPrefPath()`.
///
/// The returned path is guaranteed to end with a path separator ('\' on Windows, '/' on most other platforms).
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getUserFolder(
    folder: Folder,
) ![:0]const u8 {
    return errors.wrapCallCString(c.SDL_GetUserFolder(@intFromEnum(folder)));
}

/// Enumerate a directory tree, filtered by pattern, and return a list.
///
/// ## Function Parameters
/// * `path`: The path of the directory to enumerate.
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
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn globDirectory(
    path: [:0]const u8,
    pattern: ?[:0]const u8,
    flags: GlobFlags,
) ![][*:0]u8 {
    var count: c_int = undefined;
    const ret: [*][*:0]u8 = @ptrCast(try errors.wrapCallCPtr([*c]u8, c.SDL_GlobDirectory(path.ptr, if (pattern) |val| val.ptr else null, flags.toSdl(), &count)));
    return ret[0..@intCast(count)];
}

/// Remove a file or an empty directory.
///
/// ## Function Parameters
/// * `path`: The path to remove from the filesystem.
///
/// ## Remarks
/// Directories that are not empty will fail; this function will not recursely delete directory trees.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn removePath(
    path: [:0]const u8,
) !void {
    return errors.wrapCallBool(c.SDL_RemovePath(path.ptr));
}

/// Rename a file or directory.
///
/// ## Function Parameters
/// * `old_path`: The old path.
/// * `new_path`: The new path.
///
/// ## Remarks
/// If the file at `new_path` already exists, it will replaced.
///
/// Note that this will not copy files across filesystems/drives/volumes, as that is a much more complicated (and possibly time-consuming) operation.
///
/// Which is to say, if this function fails, `filesystem.copyFile()` to a temporary file in the same directory as `new_path`,
/// then `filesystem.renamePath()` from the temporary file to `new_path` and `filesystem.remove_path()` on `old_path` might work for files.
/// Renaming a non-empty directory across filesystems is dramatically more complex, however.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn renamePath(
    old_path: [:0]const u8,
    new_path: [:0]const u8,
) !void {
    return errors.wrapCallBool(c.SDL_RenamePath(old_path, new_path));
}

// Filesystem related tests.
test "Filesystem" {
    errors.refAllDeclsRecursive(@This());

    // Test path helper.
    var path = try Path.init(std.testing.allocator, "/home/gota/test/");
    defer path.deinit();
    try std.testing.expectEqual(std.fs.path.sep, try Path.getSeparator());
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
