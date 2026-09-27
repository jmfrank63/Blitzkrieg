//! What every executable of the app needs from the MSVC CRT on Windows:
//! MapEditor (main.zig) and the engine-tier test (c_bridge_test.zig) are both
//! hosts of the engine, linked without libc (linkMsvcRuntime), entered through
//! mainCRTStartup, and both must never stop on a debug-CRT message box.
const std = @import("std");
const builtin = @import("builtin");

const windows_crt = if (builtin.os.tag == .windows) struct {
    // Plain externs, not extern "c": the CRT is linked by the build
    // (linkMsvcRuntime), and naming libc here would make Zig link its own.
    extern fn _set_error_mode(mode: c_int) c_int;
    extern fn _set_abort_behavior(flags: c_uint, mask: c_uint) c_uint;
    extern fn _CrtSetReportMode(report_type: c_int, mode: c_int) c_int;
    extern fn _CrtSetReportFile(report_type: c_int, file: ?*anyopaque) ?*anyopaque;
} else struct {};

/// A failed assert in a Windows debug build prints and then calls abort(),
/// which the debug CRT reports as a "Debug Error!" message box that nobody
/// on a CI runner can click. Report both to stderr instead
/// (tools/zig/editor_bridge_test.cpp main does the same).
pub fn routeCrtReportsToStderr() void {
    if (builtin.os.tag != .windows) return;
    const OUT_TO_STDERR = 1; // stdlib.h _OUT_TO_STDERR
    const WRITE_ABORT_MSG = 0x1; // stdlib.h _WRITE_ABORT_MSG
    const CALL_REPORTFAULT = 0x2; // stdlib.h _CALL_REPORTFAULT
    _ = windows_crt._set_error_mode(OUT_TO_STDERR);
    _ = windows_crt._set_abort_behavior(0, WRITE_ABORT_MSG | CALL_REPORTFAULT);
    // _CrtSetReportMode/_CrtSetReportFile exist only in the debug CRT, which
    // the build links exactly in Debug (linkMsvcRuntime); in a release CRT they
    // are macros that do nothing.
    if (builtin.mode != .Debug) return;
    const CRT_ERROR = 1; // crtdbg.h _CRT_ERROR
    const CRT_ASSERT = 2; // crtdbg.h _CRT_ASSERT
    const CRTDBG_MODE_FILE = 0x1; // crtdbg.h _CRTDBG_MODE_FILE
    const CRTDBG_FILE_STDERR: ?*anyopaque = @ptrFromInt(@as(usize, @bitCast(@as(isize, -5)))); // ((_HFILE)-5)
    _ = windows_crt._CrtSetReportMode(CRT_ASSERT, CRTDBG_MODE_FILE);
    _ = windows_crt._CrtSetReportFile(CRT_ASSERT, CRTDBG_FILE_STDERR);
    _ = windows_crt._CrtSetReportMode(CRT_ERROR, CRTDBG_MODE_FILE);
    _ = windows_crt._CrtSetReportFile(CRT_ERROR, CRTDBG_FILE_STDERR);
}

/// On Windows the engine's C++ statics are linked into the executable, and
/// only the CRT's own entry point (mainCRTStartup) initialises the CRT and
/// runs their constructors; Zig's entry point runs neither. The build makes
/// mainCRTStartup the entry, and it calls a C main, which Zig exports itself
/// only when it links libc - which the app does not, so that the engine's CRT
/// is the only one. Each executable exports its own C main; this is what it
/// hands Zig's main: the arguments as Zig's own Windows entry reads them,
/// from the PEB.
pub fn minimalFromPeb() std.process.Init.Minimal {
    return .{
        .args = .{ .vector = std.os.windows.peb().ProcessParameters.CommandLine.slice() },
        .environ = .{ .block = .global },
    };
}

/// True where the executable must export its own C main (see minimalFromPeb).
pub const exports_c_main = builtin.os.tag == .windows and !builtin.link_libc;
