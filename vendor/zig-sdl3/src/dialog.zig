const c = @import("c");
const errors = @import("errors.zig");
const properties = @import("properties.zig");
const std = @import("std");
const video = @import("video.zig");

/// Callback used by file dialog functions.
///
/// ## Function Parameters
/// * `user_data`: An app provided pointer for callback's use.
/// * `file_list`: The file(s) chosen by the user.
/// * `filter`: Index of the selected filter.
/// * `err`: If an error was encountered and everything else should be ignored.
///
/// ## Remarks
/// The specific usage is described in each function.
///
/// If `file_list` is:
/// * If `null`, the user either didn't choose any file or canceled the dialog.
/// * If non-`null`, the user chose one or more files. The argument is a null-terminated array of pointers to UTF-8 encoded strings, each containing a path.
///
/// The `file_list` argument should not be freed; it will automatically be freed when the callback returns.
///
/// The filter argument is the index of the filter that was selected,
/// or `null` if no filter was selected or if the platform or method doesn't support fetching the selected filter.
///
/// In Android, the `file_list` are `content://` URIs.
/// They should be opened using `io.fromFile()` with appropriate modes.
/// This applies both to open and save file dialog.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
///
/// ## Code Examples
/// TODO!!!
pub fn FileCallback(
    comptime UserData: type,
) type {
    return *const fn (
        user_data: ?*UserData,
        file_list: ?[]const [*:0]const u8,
        filter: ?usize,
        err: bool,
    ) void;
}

/// Data for a file callback.
///
/// ## Version
/// This struct is provided by zig-sdl3.
fn FileCallbackData(comptime UserData: type) type {
    return struct {
        /// User data structure.
        user_data: ?*UserData,
        /// This is `null` if a seclection was not made, or at least one entry otherwise.
        file_list: ?[]const [*:0]const u8,
        /// Filter index if used and supported, `null` otherwise.
        filter: ?usize,
        /// If an error state was encountered.
        err: bool,
    };
}

/// An entry for filters for file dialogs.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
///
/// ## Code Examples
/// This structure is most often used as an array:
///
/// ```zig
/// const filters = [_]FileFilter {
///     { .name = "PNG images",  .pattern = "png" },
///     { .name = "JPEG images", .pattern = "jpg;jpeg" },
///     { .name = "All images",  .pattern = "png;jpg;jpeg" },
///     { .name = "All files",   .pattern = "*" },
/// };
/// ```
pub const FileFilter = extern struct {
    /// A user-readable label for the filter (for example, "Office document").
    name: [*:0]const u8,
    /// A semicolon-separated list of file extensions (for example, "doc;docx").
    /// File extensions may only contain alphanumeric characters, hyphens, underscores and periods.
    /// Alternatively, the whole string can be a single asterisk ("*"), which serves as an "All files" filter.
    pattern: [*:0]const u8,

    // Size tests.
    comptime {
        errors.assertStructsEqual(FileFilter, c.SDL_DialogFileFilter);
    }
};

/// File dialog properties.
///
/// ## Remarks
/// Note that each platform may or may not support any of the properties.
///
/// ## Version
/// This struct is provided by zig-sdl3.
pub const Properties = struct {
    /// Filters for file-based selections.
    /// Ignored if the dialog is an "Open Folder" dialog.
    /// If non-`null`, the array of filters must remain valid at least until the callback is invoked.
    filters: ?[]const FileFilter = null,
    /// The window that the dialog should be modal for.
    window: ?video.Window = null,
    /// The default folder or file to start the dialog at.
    location: ?[:0]const u8 = null,
    /// True to allow the user to select more than one entry.
    many: ?bool = null,
    /// The title for the dialog.
    title: ?[:0]const u8 = null,
    /// The label that the accept button should have.
    accept: ?[:0]const u8 = null,
    /// The label that the cancel button should have.
    cancel: ?[:0]const u8 = null,

    /// Convert to properties. This must be freed after.
    pub fn toProperties(self: Properties) !properties.Group {
        const ret = try properties.Group.init();
        errdefer ret.deinit();
        if (self.filters) |val| {
            try ret.set(c.SDL_PROP_FILE_DIALOG_FILTERS_POINTER, .{ .pointer = @constCast(@as([*]const c.SDL_DialogFileFilter, @ptrCast(val.ptr))) });
            try ret.set(c.SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, .{ .number = @intCast(val.len) });
        }
        if (self.window) |val|
            try ret.set(c.SDL_PROP_FILE_DIALOG_WINDOW_POINTER, .{ .pointer = val.value });
        if (self.location) |val|
            try ret.set(c.SDL_PROP_FILE_DIALOG_LOCATION_STRING, .{ .string = val });
        if (self.many) |val|
            try ret.set(c.SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, .{ .boolean = val });
        if (self.title) |val|
            try ret.set(c.SDL_PROP_FILE_DIALOG_TITLE_STRING, .{ .string = val });
        if (self.accept) |val|
            try ret.set(c.SDL_PROP_FILE_DIALOG_ACCEPT_STRING, .{ .string = val });
        if (self.cancel) |val|
            try ret.set(c.SDL_PROP_FILE_DIALOG_CANCEL_STRING, .{ .string = val });
        return ret;
    }
};

/// Various types of file dialogs.
///
/// ## Remarks
/// This is used by `dialog.showWithProperties()` to decide what kind of dialog to present to the user.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Type = enum(c.SDL_FileDialogType) {
    open_file = c.SDL_FILEDIALOG_OPENFILE,
    save_file = c.SDL_FILEDIALOG_SAVEFILE,
    open_folder = c.SDL_FILEDIALOG_OPENFOLDER,
};

/// Sanatize data from a dialog callback.
///
/// ## Function Parameters
/// * `UserData`: The type of user data expected.
/// * `user_data`: User data from the callback.
/// * `file_list`: File list from the callback.
/// * `filter`: Filter from the callback.
///
/// ## Return Value
/// Returns a sanatized struct where `user_data` has been casted to `UserData` if present,
/// `file_list` is `null` if a selection was not made or at least one entry otherwise, and `filter` is a filter index if used and supported (`null` otherwise).
///
/// ## Version
/// This function is provided by zig-sdl3.
fn sanatizeFileCallback(
    comptime UserData: type,
    user_data: ?*anyopaque,
    file_list: [*c]const [*c]const u8,
    filter: c_int,
) FileCallbackData(UserData) {
    var list: ?[]const [*:0]const u8 = null;
    var err: bool = false;
    if (file_list != null) {
        if (file_list.* != null) {
            const items = std.mem.span(@as([*c]const usize, @ptrCast(file_list)));
            list = @as([*]const [*:0]const u8, @ptrCast(items.ptr))[0..items.len];
        }
    } else {
        err = true;
        _ = errors.wrapCallNull(FileCallbackData(UserData), null) catch {};
    }
    return .{
        .user_data = @ptrCast(@alignCast(user_data)),
        .file_list = list,
        .filter = if (filter < 0) null else @intCast(filter),
        .err = err,
    };
}

/// Displays a dialog that lets the user select a file on their filesystem.
///
/// ## Function Parameters
/// * `callback`: A function pointer to be invoked when the user selects a file and accepts, or cancels the dialog, or an error occurs.
/// * `user_data`: An optional pointer to pass extra data to the callback when it will be invoked.
/// * `window`: The window that the dialog should be modal for, may be `null`. Not all platforms support this option.
/// * `filters`: Slice of filters, may be `null`. Not all platforms support this option, and platforms that do support it may allow the user to ignore the filters.
/// * `default_location`: The default folder or file to start the dialog at, may be `null`. Not all platforms support this option.
/// * `allow_many`: If the user will be allowed to select multiple entries. Not all platforms support this option.
///
/// ## Remarks
/// This is an asynchronous function; it will return immediately, and the result will be passed to the callback.
///
/// The callback will be invoked with a null-terminated list of files the user chose.
/// The list will be empty if the user canceled the dialog, and it will be `null` if an error occurred.
///
/// Note that the callback may be called from a different thread than the one the function was invoked on.
///
/// Depending on the platform, the user may be allowed to input paths that don't yet exist.
///
/// On Linux, dialogs may require XDG Portals, which requires DBus, which requires an event-handling loop.
/// Apps that do not use SDL to handle events should add a call to `events.pump()` in their main loop.
///
/// ## Thread Safety
/// This function should be called only from the main thread.
/// The callback may be invoked from the same thread or from a different one, depending on the OS's constraints.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn showOpenFile(
    comptime UserData: type,
    comptime callback: FileCallback(UserData),
    user_data: ?*UserData,
    window: ?video.Window,
    filters: ?[]const FileFilter,
    default_location: ?[:0]const u8,
    allow_many: bool,
) void {
    const Cb = struct {
        fn run(user_data_c: ?*anyopaque, file_list_c: [*c]const [*c]const u8, filter_c: c_int) callconv(.c) void {
            const data = sanatizeFileCallback(UserData, user_data_c, file_list_c, filter_c);
            callback(data.user_data, data.file_list, data.filter, data.err);
        }
    };
    c.SDL_ShowOpenFileDialog(
        Cb.run,
        @ptrCast(user_data),
        if (window) |val| val.value else null,
        if (filters) |val| @ptrCast(val.ptr) else null,
        if (filters) |val| @intCast(val.len) else 0,
        if (default_location) |val| val.ptr else null,
        allow_many,
    );
}

/// Displays a dialog that lets the user select a folder on their filesystem.
///
/// ## Function Parameters
/// * `UserData`: User data type.
/// * `callback`: A function pointer to be invoked when the user selects a file and accepts, or cancels the dialog, or an error occurs.
/// * `user_data`: An optional pointer to pass extra data to the callback when it will be invoked.
/// * `window`: The window that the dialog should be modal for, may be `null`. Not all platforms support this option.
/// * `default_location`: The default folder or file to start the dialog at, may be `null`. Not all platforms support this option.
/// * `allow_many`: If non-zero, the user will be allowed to select multiple entries. Not all platforms support this option.
///
/// ## Remarks
/// This is an asynchronous function; it will return immediately, and the result will be passed to the callback.
///
/// The callback will be invoked with a null-terminated list of files the user chose.
/// The list will be empty if the user canceled the dialog, and it will be `null` if an error occurred.
///
/// Note that the callback may be called from a different thread than the one the function was invoked on.
///
/// Depending on the platform, the user may be allowed to input paths that don't yet exist.
///
/// On Linux, dialogs may require XDG Portals, which requires DBus, which requires an event-handling loop.
/// Apps that do not use SDL to handle events should add a call to `events.pump()` in their main loop.
///
/// ## Thread Safety
/// This function should be called only from the main thread.
/// The callback may be invoked from the same thread or from a different one, depending on the OS's constraints.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn showOpenFolder(
    comptime UserData: type,
    comptime callback: FileCallback(UserData),
    user_data: ?*UserData,
    window: ?video.Window,
    default_location: ?[:0]const u8,
    allow_many: bool,
) void {
    const Cb = struct {
        fn run(user_data_c: ?*anyopaque, file_list_c: [*c]const [*c]const u8, filter_c: c_int) callconv(.c) void {
            const data = sanatizeFileCallback(UserData, user_data_c, file_list_c, filter_c);
            callback(data.user_data, data.file_list, data.filter, data.err);
        }
    };
    c.SDL_ShowOpenFolderDialog(
        Cb.run,
        @ptrCast(user_data),
        if (window) |val| val.value else null,
        if (default_location) |val| val.ptr else null,
        allow_many,
    );
}

/// Displays a dialog that lets the user choose a new or existing file on their filesystem.
///
/// ## Function Parameters
/// * `UserData`: User data type.
/// * `callback`: A function pointer to be invoked when the user selects a file and accepts, or cancels the dialog, or an error occurs.
/// * `user_data`: An optional pointer to pass extra data to the callback when it will be invoked.
/// * `window`: The window that the dialog should be modal for, may be `null`. Not all platforms support this option.
/// * `filters`: Slice of filters, may be `null`. Not all platforms support this option, and platforms that do support it may allow the user to ignore the filters.
/// * `default_location`: The default folder or file to start the dialog at, may be `null`. Not all platforms support this option.
///
/// ## Remarks
/// This is an asynchronous function; it will return immediately, and the result will be passed to the callback.
///
/// The callback will be invoked with a null-terminated list of files the user chose.
/// The list will be empty if the user canceled the dialog, and it will be `null` if an error occurred.
///
/// Note that the callback may be called from a different thread than the one the function was invoked on.
///
/// The chosen file may or may not already exist.
///
/// On Linux, dialogs may require XDG Portals, which requires DBus, which requires an event-handling loop.
/// Apps that do not use SDL to handle events should add a call to `events.pump()` in their main loop.
///
/// ## Thread Safety
/// This function should be called only from the main thread.
/// The callback may be invoked from the same thread or from a different one, depending on the OS's constraints.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn showSaveFile(
    comptime UserData: type,
    comptime callback: FileCallback(UserData),
    user_data: ?*UserData,
    window: ?video.Window,
    filters: ?[]const FileFilter,
    default_location: ?[:0]const u8,
) void {
    const Cb = struct {
        fn run(user_data_c: ?*anyopaque, file_list_c: [*c]const [*c]const u8, filter_c: c_int) callconv(.c) void {
            const data = sanatizeFileCallback(UserData, user_data_c, file_list_c, filter_c);
            callback(data.user_data, data.file_list, data.filter, data.err);
        }
    };
    c.SDL_ShowSaveFileDialog(
        Cb.run,
        @ptrCast(user_data),
        if (window) |val| val.value else null,
        if (filters) |val| @ptrCast(val.ptr) else null,
        if (filters) |val| @intCast(val.len) else 0,
        if (default_location) |val| val.ptr else null,
    );
}

/// Create and launch a file dialog with the specified properties.
///
/// ## Function Parameters
/// * `dialog_type`: The type of file dialog.
/// * `UserData`: User data type.
/// * `callback`: A function pointer to be invoked when the user selects a file and accepts, or cancels the dialog, or an error occurs.
/// * `user_data`: An optional pointer to pass extra data to the callback when it will be invoked.
/// * `props`: The properties to use.
///
/// ## Return Value
/// Returns created properties that must be freed later.
///
/// ## Thread Safety
/// This function should be called only from the main thread.
/// The callback may be invoked from the same thread or from a different one, depending on the OS's constraints.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn showWithProperties(
    dialog_type: Type,
    comptime UserData: type,
    comptime callback: FileCallback(UserData),
    user_data: ?*UserData,
    props: Properties,
) !properties.Group {
    const Cb = struct {
        fn run(user_data_c: ?*anyopaque, file_list_c: [*c]const [*c]const u8, filter_c: c_int) callconv(.c) void {
            const data = sanatizeFileCallback(UserData, user_data_c, file_list_c, filter_c);
            callback(data.user_data, data.file_list, data.filter, data.err);
        }
    };
    const ret = try props.toProperties();
    c.SDL_ShowFileDialogWithProperties(
        @intFromEnum(dialog_type),
        Cb.run,
        @ptrCast(user_data),
        ret.value,
    );
    return ret;
}

// Dialog tests.
test "Dialog" {
    errors.refAllDeclsRecursive(@This());
}
