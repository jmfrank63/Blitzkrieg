//! Spawns `Game` beside `MapEditor` for Test in game (D-01..D-09): builds its
//! argv as an array, never a shell string (T-03-02-01 - a mod folder with a
//! space stays exactly one element), redirects its stdout/stderr to a log
//! file, and polls its lifecycle without ever blocking the caller's own
//! frame loop.
//!
//! std-only: it imports nothing of the window toolkit or the engine adapter
//! the rest of the app uses, so this file's own tests (buildArgv, describe)
//! run on the host with no engine, no GPU and no staged installation - see
//! `zig build test-map-editor-testlaunch`.
//!
//! D-06's "still running? Restart or Keep" prompt needs the game's exit
//! without stalling a frame: `Running.poll` never calls `std.process.Child.wait`,
//! which blocks until the child exits. It reimplements the OS's non-blocking
//! probe itself (POSIX `waitpid(..., WNOHANG)`; Windows
//! `WaitForSingleObject(handle, 0)` + `GetExitCodeProcess`), matching the
//! pattern `Sources/src/CloudSync/daemon.zig` already uses for its own child.
//!
//! T-03-02-05: Windows is deliberately not put in a kill-on-close job the way
//! the cloud-sync daemon is - a quit editor must not kill a game the player is
//! still playing (accepted risk: a test game can outlive the editor that
//! started it).
const std = @import("std");
const builtin = @import("builtin");
const Io = std.Io;

/// D-02: the editor's own test profile, never the player's.
pub const profile_name = "MapEditorTest";
/// BkEditorTestMapPath's file_name argument, and the direct-map-launch
/// argument on Game's own command line (Game/GameMain.cpp's IsParamMapName).
pub const map_file_name = "mapeditor_test.bzm";

pub const Options = struct {
    /// D-08: the Game executable installed beside MapEditor - not
    /// configurable, this is only ever `gamePath`'s own answer.
    game_path: []const u8,
    /// D-09: null or empty means the base game (-mod=None); otherwise the
    /// mod's folder name, exactly as -mod= takes it.
    mod_folder: ?[]const u8 = null,
    /// D-05: a 1-based display ordinal, the editor window's own
    /// (SDL_GetDisplayForWindow's position in SDL_GetDisplays). Null omits
    /// -monitor and lets the game choose its own default display.
    monitor: ?u32 = null,
    /// Truncated and opened fresh for this launch; stdout and stderr both go
    /// here.
    log_path: []const u8,
    /// Extra environment variables (BK_AUTO_UI, BK_NO_HELP for
    /// --game-reads-it). Empty means "inherit the parent's environment
    /// unchanged" - the common case, and cheaper than copying it.
    extra_env: []const [2][]const u8 = &.{},
};

/// The most argv can ever hold: game_path, -editor-test, -profile=, -mod=,
/// -windowed, -monitor<n>, the map name.
pub const max_argv = 7;

/// Backing storage for buildArgv's two formatted arguments, owned by the
/// caller so buildArgv itself takes no allocator.
pub const ArgvStorage = struct {
    mod_arg: [320]u8 = undefined,
    monitor_arg: [16]u8 = undefined,
    argv: [max_argv][]const u8 = undefined,
};

/// `<game_path> -editor-test -profile=MapEditorTest -mod=<folder>|-mod=None
/// -windowed [-monitor<n>] mapeditor_test.bzm`, as an argv array - never
/// joined into a single command string, so a mod folder with a space in it
/// (or any shell-meaningful character) stays exactly one element and is
/// never reinterpreted by a shell (T-03-02-01).
pub fn buildArgv(storage: *ArgvStorage, options: Options) []const []const u8 {
    var n: usize = 0;
    storage.argv[n] = options.game_path;
    n += 1;
    storage.argv[n] = "-editor-test";
    n += 1;
    storage.argv[n] = "-profile=" ++ profile_name;
    n += 1;
    const mod_arg = if (options.mod_folder) |folder|
        std.fmt.bufPrint(&storage.mod_arg, "-mod={s}", .{folder}) catch "-mod=None"
    else
        "-mod=None";
    storage.argv[n] = mod_arg;
    n += 1;
    storage.argv[n] = "-windowed";
    n += 1;
    if (options.monitor) |monitor| {
        storage.argv[n] = std.fmt.bufPrint(&storage.monitor_arg, "-monitor{d}", .{monitor}) catch "-monitor1";
        n += 1;
    }
    storage.argv[n] = map_file_name;
    n += 1;
    return storage.argv[0..n];
}

/// D-08: the Game executable installed beside MapEditor - `<the running
/// executable's own directory>/Game(.exe)`, so there is nothing to configure.
/// buffer must outlive the returned slice; it is written into directly.
pub fn gamePath(io: Io, buffer: *[Io.Dir.max_path_bytes]u8) ![]const u8 {
    const dir_len = try std.process.executableDirPath(io, buffer);
    const name = if (builtin.os.tag == .windows) "Game.exe" else "Game";
    const separator: u8 = if (builtin.os.tag == .windows) '\\' else '/';
    if (dir_len + 1 + name.len > buffer.len) return error.NameTooLong;
    buffer[dir_len] = separator;
    @memcpy(buffer[dir_len + 1 ..][0..name.len], name);
    return buffer[0 .. dir_len + 1 + name.len];
}

pub const Exit = struct { code: ?u32, signal: ?u32, lifetime_ms: u64 };

pub const Outcome = enum { clean, early_failure, failure };

/// clean: exited 0. early_failure: a nonzero exit or a signal within five
/// seconds of launch - almost always a launch problem (missing Game, a bad
/// argv), which is what the spec's "Errors -> Test launch" wants named.
/// failure: the same, but later - the player was actually playing and the
/// game then died.
pub fn describe(exit: Exit) Outcome {
    const failed = (exit.code orelse 1) != 0 or exit.signal != null;
    if (!failed) return .clean;
    return if (exit.lifetime_ms < 5000) .early_failure else .failure;
}

const five_seconds: Io.Clock.Duration = .{ .raw = .fromSeconds(5), .clock = .awake };

fn sleepMs(io: Io, ms: u32) void {
    const duration: Io.Clock.Duration = .{ .raw = .fromMilliseconds(ms), .clock = .awake };
    duration.sleep(io) catch {};
}

/// A spawned Game. D-03: the editor keeps running and drawing while this
/// exists, so every method here is either non-blocking or explicitly
/// documented as the one that is not.
pub const Running = struct {
    child: std.process.Child,
    started: Io.Clock.Timestamp,
    terminate_requested: ?Io.Clock.Timestamp = null,
    sigkill_sent: bool = false,
    exit: ?Exit = null,

    /// Non-blocking: never `std.process.Child.wait`, which blocks until the
    /// child exits - that would freeze the editor's own frame. Once an exit
    /// has been observed it is cached and returned again on every later call.
    pub fn poll(self: *Running, io: Io) ?Exit {
        if (self.exit) |exit| return exit;
        if (builtin.os.tag == .windows) {
            const handle = self.child.id orelse return null;
            if (WaitForSingleObject(handle, 0) != wait_object_0) return null;
            var code: std.os.windows.DWORD = 0;
            _ = GetExitCodeProcess(handle, &code);
            std.os.windows.CloseHandle(handle);
            std.os.windows.CloseHandle(self.child.thread_handle);
            self.child.id = null;
            const exit: Exit = .{ .code = code, .signal = null, .lifetime_ms = self.lifetimeMs(io) };
            self.exit = exit;
            return exit;
        }
        const pid = self.child.id orelse return null;
        var raw_status: if (builtin.link_libc) c_int else u32 = undefined;
        const rc = std.posix.system.waitpid(pid, &raw_status, std.posix.W.NOHANG);
        switch (std.posix.errno(rc)) {
            .SUCCESS => {},
            .INTR => return self.poll(io),
            .CHILD => return null,
            else => return null,
        }
        if (rc == 0) {
            // WNOHANG's own "still running" answer: the status word above was
            // never written. Still running (or stopped, which WNOHANG with no
            // WUNTRACED should not report): the terminate() escalation lives
            // here since the caller's own poll cadence is the only clock this
            // module has.
            if (self.terminate_requested) |requested| {
                if (!self.sigkill_sent and Io.Clock.Timestamp.now(io, .awake).compare(.gte, requested.addDuration(five_seconds))) {
                    std.posix.kill(pid, .KILL) catch {};
                    self.sigkill_sent = true;
                }
            }
            return null;
        }
        self.child.id = null;
        const status: u32 = @bitCast(raw_status);
        var exit: Exit = .{ .code = null, .signal = null, .lifetime_ms = self.lifetimeMs(io) };
        if (std.posix.W.IFEXITED(status)) exit.code = std.posix.W.EXITSTATUS(status) else if (std.posix.W.IFSIGNALED(status)) exit.signal = @intFromEnum(std.posix.W.TERMSIG(status));
        self.exit = exit;
        return exit;
    }

    /// D-06's "Restart with this version": SIGTERM (Windows has no graceful
    /// signal, so TerminateProcess there is immediate); a later `poll` sends
    /// SIGKILL if the process is still running five seconds after this call.
    pub fn terminate(self: *Running, io: Io) void {
        if (self.exit != null) return;
        const pid = self.child.id orelse return;
        if (builtin.os.tag == .windows) {
            _ = TerminateProcess(pid, 1);
            return;
        }
        std.posix.kill(pid, .TERM) catch {};
        self.terminate_requested = Io.Clock.Timestamp.now(io, .awake);
    }

    /// Headless modes only (--game-reads-it): blocks the calling thread until
    /// the game exits or timeout_ms passes. An interactive frame loop must
    /// never call this - use `poll`.
    pub fn waitBlocking(self: *Running, io: Io, timeout_ms: u32) ?Exit {
        const deadline: Io.Clock.Timestamp = .fromNow(io, .{ .raw = .fromMilliseconds(@intCast(timeout_ms)), .clock = .awake });
        while (true) {
            if (self.poll(io)) |exit| return exit;
            if (Io.Clock.Timestamp.now(io, .awake).compare(.gte, deadline)) return null;
            sleepMs(io, 50);
        }
    }

    fn lifetimeMs(self: *const Running, io: Io) u64 {
        const elapsed = self.started.durationTo(Io.Clock.Timestamp.now(io, .awake)).raw.nanoseconds;
        if (elapsed <= 0) return 0;
        return @intCast(@divTrunc(elapsed, std.time.ns_per_ms));
    }
};

/// Opens log_path truncated (creating its directory first if needed) and
/// spawns game_path with buildArgv's argv: stdin ignored, stdout and stderr
/// both to the log, no console window of its own (Windows). environ is
/// copied into a fresh map only when extra_env is non-empty - the common
/// launch (no extra_env) inherits the parent's environment directly, which
/// needs no copy at all.
pub fn start(gpa: std.mem.Allocator, io: Io, environ: std.process.Environ, options: Options) !Running {
    var storage: ArgvStorage = .{};
    const argv = buildArgv(&storage, options);

    if (std.fs.path.dirname(options.log_path)) |directory| Io.Dir.cwd().createDirPath(io, directory) catch {};
    var log = try Io.Dir.cwd().createFile(io, options.log_path, .{ .truncate = true });
    defer log.close(io);

    var map: ?std.process.Environ.Map = null;
    defer if (map) |*m| m.deinit();
    var environ_map: ?*const std.process.Environ.Map = null;
    if (options.extra_env.len != 0) {
        map = try environ.createMap(gpa);
        for (options.extra_env) |pair| try map.?.put(pair[0], pair[1]);
        environ_map = &map.?;
    }

    const child = try std.process.spawn(io, .{
        .argv = argv,
        .environ_map = environ_map,
        .stdin = .ignore,
        .stdout = .{ .file = log },
        .stderr = .{ .file = log },
        .create_no_window = true,
    });

    return .{ .child = child, .started = Io.Clock.Timestamp.now(io, .awake) };
}

// -- Windows process control (poll/terminate only; no job object - T-03-02-05) --

const wait_object_0: std.os.windows.DWORD = 0;

extern "kernel32" fn WaitForSingleObject(
    hHandle: std.os.windows.HANDLE,
    dwMilliseconds: std.os.windows.DWORD,
) callconv(.winapi) std.os.windows.DWORD;

extern "kernel32" fn GetExitCodeProcess(
    hProcess: std.os.windows.HANDLE,
    lpExitCode: *std.os.windows.DWORD,
) callconv(.winapi) std.os.windows.BOOL;

extern "kernel32" fn TerminateProcess(
    hProcess: std.os.windows.HANDLE,
    uExitCode: std.os.windows.UINT,
) callconv(.winapi) std.os.windows.BOOL;

test "buildArgv: no mod, no monitor - the base game, windowed" {
    var storage: ArgvStorage = .{};
    const argv = buildArgv(&storage, .{ .game_path = "/stage/Game", .log_path = "log" });
    try std.testing.expectEqual(@as(usize, 6), argv.len);
    try std.testing.expectEqualStrings("/stage/Game", argv[0]);
    try std.testing.expectEqualStrings("-editor-test", argv[1]);
    try std.testing.expectEqualStrings("-profile=MapEditorTest", argv[2]);
    try std.testing.expectEqualStrings("-mod=None", argv[3]);
    try std.testing.expectEqualStrings("-windowed", argv[4]);
    try std.testing.expectEqualStrings("mapeditor_test.bzm", argv[5]);
}

test "buildArgv: a mod folder with a space stays one argv element, plus a monitor" {
    var storage: ArgvStorage = .{};
    const argv = buildArgv(&storage, .{ .game_path = "/stage/Game", .mod_folder = "Achtung Panzer 2", .monitor = 2, .log_path = "log" });
    try std.testing.expectEqual(@as(usize, 7), argv.len);
    try std.testing.expectEqualStrings("-mod=Achtung Panzer 2", argv[3]);
    try std.testing.expectEqualStrings("-monitor2", argv[5]);
    try std.testing.expectEqualStrings("mapeditor_test.bzm", argv[6]);
}

test "describe: a clean exit, an early failure, and a later failure" {
    try std.testing.expectEqual(Outcome.clean, describe(.{ .code = 0, .signal = null, .lifetime_ms = 200 }));
    try std.testing.expectEqual(Outcome.early_failure, describe(.{ .code = 1, .signal = null, .lifetime_ms = 200 }));
    try std.testing.expectEqual(Outcome.early_failure, describe(.{ .code = null, .signal = 11, .lifetime_ms = 4999 }));
    try std.testing.expectEqual(Outcome.failure, describe(.{ .code = 1, .signal = null, .lifetime_ms = 30000 }));
    try std.testing.expectEqual(Outcome.clean, describe(.{ .code = 0, .signal = null, .lifetime_ms = 30000 }));
}
