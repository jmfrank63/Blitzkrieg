const errors = @import("../errors.zig");
const log = @import("../log.zig");
const message_box = @import("../message_box.zig");
const options = @import("options");
const std = @import("std");

const logger = std.log.scoped(.sdl3);

/// An example function to log with SDL using debug prints.
///
/// ## Function Parameters
/// * `user_data`: User data provided to the logging function.
/// * `category`: Which category SDL is logging under, for example "video".
/// * `priority`: Which priority the log message is.
/// * `message`: Actual message to log. This should not be `null`.
pub fn debugPrint(
    user_data: ?*void,
    category: log.Category,
    priority: ?log.Priority,
    message: [:0]const u8,
) void {
    _ = user_data;
    const category_str: ?[]const u8 = switch (category) {
        .application => "Application",
        .errors => "Errors",
        .assert => "Assert",
        .system => "System",
        .audio => "Audio",
        .video => "Video",
        .render => "Render",
        .input => "Input",
        .testing => "Testing",
        .gpu => "Gpu",
        else => null,
    };
    const priority_str: [:0]const u8 = if (priority) |val| switch (val) {
        .trace => "Trace",
        .verbose => "Verbose",
        .debug => "Debug",
        .info => "Info",
        .warn => "Warn",
        .err => "Error",
        .critical => "Critical",
    } else "Unknown";
    if (category_str) |val| {
        std.debug.print("[{s}:{s}] {s}\n", .{ val, priority_str, message });
    } else {
        std.debug.print("[Custom_{d}:{s}] {s}\n", .{ @intFromEnum(category), priority_str, message });
    }
}

/// An example function to log with SDL using the SDL message box.
///
/// ## Function Parameters
/// * `user_data`: User data provided to the logging function.
/// * `category`: Which category SDL is logging under, for example "video".
/// * `priority`: Which priority the log message is.
/// * `message`: Actual message to log. This should not be `null`.
pub fn sdlMessageBox(
    user_data: ?*void,
    category: log.Category,
    priority: ?log.Priority,
    message: [:0]const u8,
) void {
    _ = user_data;
    const category_str: ?[]const u8 = switch (category) {
        .application => "Application",
        .errors => "Errors",
        .assert => "Assert",
        .system => "System",
        .audio => "Audio",
        .video => "Video",
        .render => "Render",
        .input => "Input",
        .testing => "Testing",
        .gpu => "Gpu",
        else => null,
    };
    const priority_str: [:0]const u8 = if (priority) |val| switch (val) {
        .trace => "Trace",
        .verbose => "Verbose",
        .debug => "Debug",
        .info => "Info",
        .warn => "Warn",
        .err => "Error",
        .critical => "Critical",
    } else "Unknown";
    var buf: [options.log_message_stack_size]u8 = undefined;
    const too_long = "SDL message too long, please increase the `log_message_stack_size` build option";
    const text = if (category_str) |val| std.fmt.bufPrintZ(&buf, "{s}: {s}", .{ val, message }) catch too_long else message;
    message_box.showSimple(
        .{
            .information_dialog = priority == .info,
            .warning_dialog = priority == .warn,
            .error_dialog = priority == .err,
        },
        priority_str,
        text,
        null,
    ) catch {};
}

/// An example function to log with SDL using zig log.
///
/// ## Function Parameters
/// * `user_data`: User data provided to the logging function.
/// * `category`: Which category SDL is logging under, for example "video".
/// * `priority`: Which priority the log message is.
/// * `message`: Actual message to log. This should not be `null`.
pub fn zigLog(
    user_data: ?*void,
    category: log.Category,
    priority: ?log.Priority,
    message: [:0]const u8,
) void {
    _ = user_data;
    const category_str: ?[]const u8 = switch (category) {
        .application => "Application",
        .errors => "Errors",
        .assert => "Assert",
        .system => "System",
        .audio => "Audio",
        .video => "Video",
        .render => "Render",
        .input => "Input",
        .testing => "Testing",
        .gpu => "Gpu",
        else => null,
    };
    const priority_str: [:0]const u8 = if (priority) |val| switch (val) {
        .trace => "Trace",
        .verbose => "Verbose",
        .debug => "Debug",
        .info => "Info",
        .warn => "Warn",
        .err => "Error",
        .critical => "Critical",
    } else "Unknown";
    const pri = priority orelse .info;
    if (category_str) |val| {
        const fmt = "[{s}:{s}] {s}";
        switch (pri) {
            .err, .critical => logger.err(fmt, .{ val, priority_str, message }),
            .warn => logger.warn(fmt, .{ val, priority_str, message }),
            .info => logger.info(fmt, .{ val, priority_str, message }),
            else => logger.debug(fmt, .{ val, priority_str, message }),
        }
    } else {
        const fmt = "[Custom_{d}:{s}] {s}";
        switch (pri) {
            .err, .critical => logger.err(fmt, .{ @intFromEnum(category), priority_str, message }),
            .warn => logger.warn(fmt, .{ @intFromEnum(category), priority_str, message }),
            .info => logger.info(fmt, .{ @intFromEnum(category), priority_str, message }),
            else => logger.debug(fmt, .{ @intFromEnum(category), priority_str, message }),
        }
    }
}

test "extras loggers" {
    errors.refAllDeclsRecursive(@This());
}
