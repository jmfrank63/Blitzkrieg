const errors = @import("errors.zig");
const options = @import("options");
const std = @import("std");
const timer = @import("timer.zig");

/// Helpful error handler functions that can be used to set SDL3's error function.
///
/// ## Remarks
/// Set `errors.error_callback` to one of these entries.
pub const error_handlers = @import("extras/error_handlers.zig");

/// GPU extras.
pub const gpu = @import("extras/gpu.zig");

/// Helpful logger functions that can be used to set SDL3' logging function.
///
/// ## Remarks
/// Call `log.setLogOutputFunction` to one of these entries.
pub const loggers = @import("extras/loggers.zig");

pub const FramerateCapper = @import("extras/framerate_capper.zig").FramerateCapper;

test "extra" {
    errors.refAllDeclsRecursive(@This());
}
