const errors = @import("../errors.zig");
const log = @import("../log.zig");
const message_box = @import("../message_box.zig");
const options = @import("options");
const std = @import("std");

const logger = std.log.scoped(.sdl3);

/// An example function to handle errors from SDL in a debug print.
///
/// ## Function Parameters
/// * `err`: A slice to an error message, or `null` if the error message is not known.
///
/// ## Remarks
/// Remember that the error callback is thread-local, thus you need to set it for each thread!
pub fn debugPrint(
    err: ?[]const u8,
) void {
    if (err) |val| {
        std.debug.print("******* [SDL3 Error! {s}] *******\n", .{val});
    } else {
        std.debug.print("******* [Unknown SDL3 Error!] *******\n", .{});
    }
}

/// An example function to handle errors from SDL as SDL error messages.
///
/// ## Function Parameters
/// * `err`: A slice to an error message, or `null` if the error message is not known.
///
/// ## Remarks
/// Remember that the error callback is thread-local, thus you need to set it for each thread!
pub fn sdlLog(
    err: ?[]const u8,
) void {
    if (err) |val| {
        log.Category.errors.logError("{s}", .{val}) catch {};
    } else {
        log.Category.errors.logError("Unknown", .{}) catch {};
    }
}

/// An example function to handle errors from SDL in an SDL message box.
///
/// ## Function Parameters
/// * `err`: A slice to an error message, or `null` if the error message is not known.
///
/// ## Remarks
/// Remember that the error callback is thread-local, thus you need to set it for each thread!
pub fn sdlMessageBox(
    err: ?[]const u8,
) void {
    var buf: [options.log_message_stack_size]u8 = undefined;
    const err_msg = if (err) |val| std.fmt.bufPrintZ(&buf, "{s}", .{val}) catch "SDL message too long, please increase the `log_message_stack_size` build option" else "Unknown SDL error";
    message_box.showSimple(.{ .error_dialog = true }, "SDL Error", err_msg, null) catch {};
}

/// An example function to handle errors from SDL in a zig error log.
///
/// ## Function Parameters
/// * `err`: A slice to an error message, or `null` if the error message is not known.
///
/// ## Remarks
/// Remember that the error callback is thread-local, thus you need to set it for each thread!
pub fn zigLog(
    err: ?[]const u8,
) void {
    if (err) |val| {
        logger.err("[Error:General] {s}", .{val});
    } else {
        logger.err("[Error:Unknown]", .{});
    }
}

test "extra error handlers" {
    errors.refAllDeclsRecursive(@This());
}
