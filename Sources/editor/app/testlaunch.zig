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

/// The exits `Running.terminate` produces, as `poll` reports them: Windows'
/// TerminateProcess ends the game with this code; POSIX ends it by SIGTERM,
/// or by SIGKILL five seconds later if it is still running. `describe` calls
/// every one of them a failure - it cannot know the editor asked for it -
/// so the caller that asked (TestLaunchPrompt's Restart) must.
pub const windows_terminate_exit_code: u32 = 1;
pub const posix_terminate_signal: u32 = 15; // SIGTERM
pub const posix_kill_signal: u32 = 9; // SIGKILL

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

/// What a test game's log says about one map sound, read from the lines
/// BK_SOUND_TRACE=1 writes (Scene/SoundScene.cpp): `map sound object="N"
/// pos=(X,Y) instance=I` when the mission hands the sound to the sound scene
/// (instance 0: the scene refused the name), and `add id=.. name="<path>"
/// object="N" ... pos=(X,Y) ...` every time the scene starts it, which it
/// does only while the view is near. Positions match within one unit: the
/// log rounds them. `looped_starts` counts the starts marked looped=1: a
/// loop the view stays near starts once and plays on, where it used to be
/// stopped and started again every 3 s (CMapSoundCell::Update).
pub const MapSoundTrace = struct { registered: bool = false, started: bool = false, looped_starts: u32 = 0 };

pub fn mapSoundTrace(log: []const u8, object: []const u8, x: f32, y: f32) MapSoundTrace {
    var result: MapSoundTrace = .{};
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw_line| {
        const line = std.mem.trimEnd(u8, raw_line, "\r");
        const is_registration = std.mem.startsWith(u8, line, "BK_SOUND_TRACE: map sound ");
        const is_start = std.mem.startsWith(u8, line, "BK_SOUND_TRACE: add ");
        if (!is_registration and !is_start) continue;
        const name = quotedField(line, "object=\"") orelse continue;
        if (!std.ascii.eqlIgnoreCase(name, object)) continue;
        const pos = tracePos(line) orelse continue;
        if (@abs(pos[0] - x) > 1 or @abs(pos[1] - y) > 1) continue;
        if (is_start) {
            result.started = true;
            if (std.mem.indexOf(u8, line, " looped=1 ") != null) result.looped_starts += 1;
        } else if (std.mem.indexOf(u8, line, "instance=")) |at| {
            const instance = std.fmt.parseInt(u32, line[at + "instance=".len ..], 10) catch 0;
            if (instance != 0) result.registered = true;
        }
    }
    return result;
}

/// A player's unit count from the game's `units=` report (BK_AUTO_UI,
/// Game/GameMain.cpp): "BK_AUTO_UI: units near X,Y r R: total N; player P: M
/// ...". The game names only players with units, so a line without this
/// player is a real 0. Null when the log has no such line at all - the query
/// never ran, which must not be mistaken for an empty spot. The first line
/// counts.
pub fn playerUnitsNear(log: []const u8, player: u32) ?u32 {
    const at = std.mem.indexOf(u8, log, "BK_AUTO_UI: units near ") orelse return null;
    const end = std.mem.indexOfScalarPos(u8, log, at, '\n') orelse log.len;
    const line = std.mem.trimEnd(u8, log[at..end], "\r");
    var marker_buffer: [32]u8 = undefined;
    const marker = std.fmt.bufPrint(&marker_buffer, "; player {d}: ", .{player}) catch return null;
    const found = std.mem.indexOf(u8, line, marker) orelse return 0;
    const rest = line[found + marker.len ..];
    var digits: usize = 0;
    while (digits < rest.len and std.ascii.isDigit(rest[digits])) : (digits += 1) {}
    return std.fmt.parseInt(u32, rest[0..digits], 10) catch null;
}

fn quotedField(line: []const u8, key: []const u8) ?[]const u8 {
    const at = std.mem.indexOf(u8, line, key) orelse return null;
    const rest = line[at + key.len ..];
    const end = std.mem.indexOfScalar(u8, rest, '"') orelse return null;
    return rest[0..end];
}

fn tracePos(line: []const u8) ?[2]f32 {
    const at = std.mem.indexOf(u8, line, "pos=(") orelse return null;
    const rest = line[at + "pos=(".len ..];
    const comma = std.mem.indexOfScalar(u8, rest, ',') orelse return null;
    const close = std.mem.indexOfScalar(u8, rest, ')') orelse return null;
    if (close < comma) return null;
    const px = std.fmt.parseFloat(f32, rest[0..comma]) catch return null;
    const py = std.fmt.parseFloat(f32, rest[comma + 1 .. close]) catch return null;
    return .{ px, py };
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
            _ = TerminateProcess(pid, windows_terminate_exit_code);
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
/// spawns game_path with buildArgv's argv, in game_path's own directory:
/// stdin ignored, stdout and stderr both to the log, no console window of
/// its own (Windows). environ is copied into a fresh map only when
/// extra_env is non-empty - the common launch (no extra_env) inherits the
/// parent's environment directly, which needs no copy at all.
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
        // The game's own directory, not the editor's working directory: the
        // game writes some files (BK_AUTO_UI's autoshots, its traces) where
        // it runs, and an editor started from a shortcut or another shell
        // has some unrelated cwd.
        .cwd = if (std.fs.path.dirname(options.game_path)) |directory| .{ .path = directory } else .inherit,
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

test "describe: the exits terminate() causes read as failures, so its caller has to know it asked" {
    try std.testing.expectEqual(Outcome.failure, describe(.{ .code = windows_terminate_exit_code, .signal = null, .lifetime_ms = 30_000 }));
    try std.testing.expectEqual(Outcome.failure, describe(.{ .code = null, .signal = posix_terminate_signal, .lifetime_ms = 30_000 }));
    try std.testing.expectEqual(Outcome.early_failure, describe(.{ .code = null, .signal = posix_kill_signal, .lifetime_ms = 4_000 }));
}

test "the POSIX terminate signals are the platform's own SIGTERM and SIGKILL" {
    if (builtin.os.tag == .windows) return error.SkipZigTest;
    try std.testing.expectEqual(posix_terminate_signal, @as(u32, @intFromEnum(std.posix.SIG.TERM)));
    try std.testing.expectEqual(posix_kill_signal, @as(u32, @intFromEnum(std.posix.SIG.KILL)));
}

test "mapSoundTrace: registered, then started, at the placed point" {
    const log =
        "BK_AUTO_UI: frame 120 at 1 ms game 2 ms\n" ++
        "BK_SOUND_TRACE: map sound object=\"Amb_Water_circle\" pos=(1500,2400) instance=1\r\n" ++
        "BK_SOUND_TRACE: add id=7 name=\"Sounds\\Ambient\\water\\circle\" object=\"Amb_Water_circle\" looped=1 mix=1 pos=(1500,2401) t=1004000\n";
    const trace = mapSoundTrace(log, "amb_water_circle", 1500.4, 2400.4);
    try std.testing.expect(trace.registered);
    try std.testing.expect(trace.started);
    try std.testing.expectEqual(@as(u32, 1), trace.looped_starts);
}

test "mapSoundTrace: a loop started again counts every start; a one-shot counts none" {
    const add_loop = "BK_SOUND_TRACE: add id=6 name=\"Sounds\\Ambient\\water\\circle\" object=\"Amb_Water_circle\" looped=1 mix=0 pos=(1974,2314) t=1000050\n";
    const remove_loop = "BK_SOUND_TRACE: remove id=6 known=1 found=1 name=\"Sounds\\Ambient\\water\\circle\" looped=1 t=1003059\n";
    const restarted = mapSoundTrace(add_loop ++ remove_loop ++ add_loop ++ remove_loop ++ add_loop, "Amb_Water_circle", 1974, 2314);
    try std.testing.expectEqual(@as(u32, 3), restarted.looped_starts);
    const one_shot = "BK_SOUND_TRACE: add id=23 name=\"Sounds\\Weapons\\other\\aviacannon30\" object=\"30mm_aviacannon\" looped=0 mix=0 pos=(3224,2505) t=1006065\n";
    const shots = mapSoundTrace(one_shot ++ one_shot, "30mm_aviacannon", 3224, 2505);
    try std.testing.expect(shots.started);
    try std.testing.expectEqual(@as(u32, 0), shots.looped_starts);
}

test "mapSoundTrace: registered but never started - the view never came near" {
    const log = "BK_SOUND_TRACE: map sound object=\"Amb_Water_circle\" pos=(1500,2400) instance=3\n";
    const trace = mapSoundTrace(log, "Amb_Water_circle", 1500, 2400);
    try std.testing.expect(trace.registered);
    try std.testing.expect(!trace.started);
}

test "mapSoundTrace: another place, another sound, or a refused name is not this sound" {
    const log =
        "BK_SOUND_TRACE: map sound object=\"Amb_Water_circle\" pos=(1500,2400) instance=0\n" ++
        "BK_SOUND_TRACE: add id=7 name=\"Sounds\\Ambient\\water\\circle\" object=\"Amb_Water_circle\" looped=1 mix=1 pos=(1510,2400) t=1\n" ++
        "BK_SOUND_TRACE: add id=8 name=\"Sounds\\Ambient\\field\\1\" object=\"Amb_Field\" looped=0 mix=1 pos=(1500,2400) t=1\n" ++
        "BK_SOUND_TRACE: dump t=1 camera=(1500,2400) cells=1 sounds=1\n";
    const trace = mapSoundTrace(log, "Amb_Water_circle", 1500, 2400);
    try std.testing.expect(!trace.registered);
    try std.testing.expect(!trace.started);
    try std.testing.expect(!mapSoundTrace("", "Amb_Water_circle", 0, 0).started);
}

test "playerUnitsNear: a player's count, a player the game did not name, and no query at all" {
    const log =
        "BK_AUTO_UI: frame 400 at 1 ms game 2 ms\n" ++
        "BK_AUTO_UI: units near 38,41 r 5: total 23; player 0: 14; player 1: 9\r\n" ++
        "BK_AUTO_UI: shot written\n";
    try std.testing.expectEqual(@as(?u32, 14), playerUnitsNear(log, 0));
    try std.testing.expectEqual(@as(?u32, 9), playerUnitsNear(log, 1));
    try std.testing.expectEqual(@as(?u32, 0), playerUnitsNear(log, 2));
    // Player 10's count is not player 0's.
    try std.testing.expectEqual(@as(?u32, 0), playerUnitsNear("BK_AUTO_UI: units near 1,1 r 5: total 3; player 10: 3\n", 0));
    // An empty spot: the line is there, naming nobody.
    try std.testing.expectEqual(@as(?u32, 0), playerUnitsNear("BK_AUTO_UI: units near 1,1 r 5: total 0\n", 0));
    try std.testing.expectEqual(@as(?u32, null), playerUnitsNear("BK_AUTO_UI: shot written\n", 0));
    try std.testing.expectEqual(@as(?u32, null), playerUnitsNear("", 0));
}
