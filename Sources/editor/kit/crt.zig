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
    if (builtin.mode != .debug) return;
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

const windows_console = if (builtin.os.tag == .windows) struct {
    // Plain externs, matching this file's windows_crt above and
    // Sources/src/CloudSync/daemon.zig's extern "kernel32" style - none of
    // these are declared by Zig's own (very small) std/os/windows/kernel32.zig.
    extern "kernel32" fn GetStdHandle(nStdHandle: std.os.windows.DWORD) callconv(.winapi) ?std.os.windows.HANDLE;
    extern "kernel32" fn SetStdHandle(nStdHandle: std.os.windows.DWORD, hHandle: std.os.windows.HANDLE) callconv(.winapi) std.os.windows.BOOL;
    extern "kernel32" fn AttachConsole(dwProcessId: std.os.windows.DWORD) callconv(.winapi) std.os.windows.BOOL;
    extern "kernel32" fn CreateFileW(
        lpFileName: [*:0]const u16,
        dwDesiredAccess: std.os.windows.DWORD,
        dwShareMode: std.os.windows.DWORD,
        lpSecurityAttributes: ?*anyopaque,
        dwCreationDisposition: std.os.windows.DWORD,
        dwFlagsAndAttributes: std.os.windows.DWORD,
        hTemplateFile: ?std.os.windows.HANDLE,
    ) callconv(.winapi) std.os.windows.HANDLE;
} else struct {};

// winbase.h / wincon.h constants, named here for the same reason as
// windows_crt's constants above: not declared by Zig's own windows.zig.
const STD_OUTPUT_HANDLE: std.os.windows.DWORD = @bitCast(@as(i32, -11));
const STD_ERROR_HANDLE: std.os.windows.DWORD = @bitCast(@as(i32, -12));
const ATTACH_PARENT_PROCESS: std.os.windows.DWORD = @bitCast(@as(i32, -1));
const GENERIC_READ: std.os.windows.DWORD = 0x80000000;
const GENERIC_WRITE: std.os.windows.DWORD = 0x40000000;
const FILE_SHARE_READ: std.os.windows.DWORD = 0x1;
const FILE_SHARE_WRITE: std.os.windows.DWORD = 0x2;
const OPEN_EXISTING: std.os.windows.DWORD = 3;

/// Windows only: the packaged MapEditor.exe runs with the `.windows`
/// subsystem (no console window on a normal double-click -
/// build.zig's configureMapEditorExecutable), so launched from cmd.exe or
/// PowerShell it starts with no console of its own: GetStdHandle(STD_ERROR_HANDLE)
/// comes back null or INVALID_HANDLE_VALUE, exactly as it would from a
/// double-click launch with no console at all. --check/--smoke/
/// --game-reads-it and BK_EDITOR_AUTO runs (main.zig) still need their
/// PASS/FAIL lines to reach that terminal or a CI log, so each calls this
/// first, before printing anything: AttachConsole(ATTACH_PARENT_PROCESS)
/// joins the shell's own console (a no-op if the parent has none, e.g.
/// Explorer - AttachConsole then simply fails and this returns), then
/// CONOUT$ is opened and installed as both the standard-output and
/// standard-error handle. Zig has no separate "current stdio" table a libc
/// freopen would need to update: std.Io.File.stdout()/stderr() read the
/// process parameters block directly on every call, and SetStdHandle writes
/// into that same block, so nothing else needs to change once this returns.
/// A parent that already redirected/piped stderr (CI, `zig build ...
/// -Dtest-mode=run`) already has a valid handle here, so this is a no-op -
/// it never fights a real redirection. The plain interactive/double-click
/// launch never calls this (main.zig) - there is nothing to attach to and
/// nothing it prints. No-op on every platform but Windows.
pub fn attachParentConsole() void {
    if (builtin.os.tag != .windows) return;
    if (hasUsableStandardHandle(STD_ERROR_HANDLE)) return;
    if (!windows_console.AttachConsole(ATTACH_PARENT_PROCESS).toBool()) return;
    const conout = windows_console.CreateFileW(
        std.unicode.utf8ToUtf16LeStringLiteral("CONOUT$"),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        null,
        OPEN_EXISTING,
        0,
        null,
    );
    if (conout == std.os.windows.INVALID_HANDLE_VALUE) return;
    _ = windows_console.SetStdHandle(STD_OUTPUT_HANDLE, conout);
    _ = windows_console.SetStdHandle(STD_ERROR_HANDLE, conout);
}

fn hasUsableStandardHandle(std_handle: std.os.windows.DWORD) bool {
    const handle = windows_console.GetStdHandle(std_handle) orelse return false;
    return handle != std.os.windows.INVALID_HANDLE_VALUE;
}
