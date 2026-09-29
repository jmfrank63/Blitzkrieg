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

/// What the game reported it consumed from the map (04-04, research Pattern
/// 6): one `BK_MAP_TRACE: <kind> key=value ...` line per item, printed only
/// when the game runs with BK_MAP_TRACE set (GameTT/iMissionInternal.cpp and
/// the AI's own loaders). Every scalar is optional and every list starts
/// empty: null or `seen == 0` means the game did not report that item (an
/// older Game, a truncated log, a map without it), which is not the same as a
/// reported zero. Unknown kinds and keys are ignored, so a newer game never
/// breaks an older parser. Names (an area's, the script's) are printed in
/// double quotes by the game so they may hold spaces.
pub const CameraSource = enum { player, neutral, units, unknown };
pub const TraceCamera = struct { x: f32, y: f32, z: f32, source: CameraSource };

/// A copy of a name or a Lua line, cut to fit: the summary owns its bytes, so
/// it outlives the log it was read from.
pub const text_capacity = 96;
pub const TraceText = struct {
    buffer: [text_capacity]u8 = undefined,
    len: u8 = 0,

    fn from(text: []const u8) TraceText {
        var result: TraceText = .{};
        const n = @min(text.len, text_capacity);
        @memcpy(result.buffer[0..n], text[0..n]);
        result.len = @intCast(n);
        return result;
    }

    pub fn slice(self: *const TraceText) []const u8 {
        return self.buffer[0..self.len];
    }
};

/// The first `capacity` items of a kind, and how many the game printed in all.
fn Kept(comptime T: type, comptime capacity: usize) type {
    return struct {
        items: [capacity]T = undefined,
        seen: u32 = 0,

        fn add(self: *@This(), item: T) void {
            if (self.seen < capacity) self.items[self.seen] = item;
            self.seen +|= 1;
        }

        pub fn slice(self: *const @This()) []const T {
            return self.items[0..@min(self.seen, capacity)];
        }
    };
}

pub const TraceScript = struct { name: TraceText, loaded: bool, init: bool };
/// An area's centre is in AI (map) units, raw as the game stores it.
pub const TraceArea = struct { name: TraceText, cx: f32, cy: f32 };
/// `held` is how many map objects the game held back for reinforcement group `id`.
pub const TraceGroup = struct { id: i32, held: u32 };
pub const TraceGeneral = struct { side: i32, parcels: u32, mobile: u32 };
/// A general's parcel: centre and radius in AI units, `dir` the stored WORD direction.
pub const TraceParcel = struct { side: i32, idx: u32, kind: i32, cx: f32, cy: f32, r: f32, dir: u32 };

pub const max_areas = 32;
pub const max_groups = 32;
pub const max_generals = 8;
pub const max_parcels = 32;
pub const max_lua_lines = 16;

pub const MapTraceSummary = struct {
    /// The first `camera` line: where the mission put the view at its start.
    camera: ?TraceCamera = null,
    roads: ?u32 = null,
    rivers: ?u32 = null,
    script: ?TraceScript = null,
    areas: Kept(TraceArea, max_areas) = .{},
    groups: Kept(TraceGroup, max_groups) = .{},
    bridges: ?u32 = null,
    entrenchments: ?u32 = null,
    startcmd_launched: ?u32 = null,
    reserve_applied: ?u32 = null,
    generals: Kept(TraceGeneral, max_generals) = .{},
    parcels: Kept(TraceParcel, max_parcels) = .{},
    /// The map's own Lua `Trace` calls, in order, the text as the script made it.
    lua: Kept(TraceText, max_lua_lines) = .{},
};

const map_trace_prefix = "BK_MAP_TRACE: ";

/// The value of `key=` among the space-separated `key=value` tokens of `rest`.
fn tokenValue(rest: []const u8, key: []const u8) ?[]const u8 {
    var tokens = std.mem.tokenizeScalar(u8, rest, ' ');
    while (tokens.next()) |token| {
        const eq = std.mem.indexOfScalar(u8, token, '=') orelse continue;
        if (std.mem.eql(u8, token[0..eq], key)) return token[eq + 1 ..];
    }
    return null;
}

fn floatField(rest: []const u8, key: []const u8) ?f32 {
    const text = tokenValue(rest, key) orelse return null;
    const value = std.fmt.parseFloat(f32, text) catch return null;
    return if (std.math.isFinite(value)) value else null;
}

fn countField(rest: []const u8, key: []const u8) ?u32 {
    return std.fmt.parseInt(u32, tokenValue(rest, key) orelse return null, 10) catch null;
}

fn intField(rest: []const u8, key: []const u8) ?i32 {
    return std.fmt.parseInt(i32, tokenValue(rest, key) orelse return null, 10) catch null;
}

/// What follows the closing quote of the first quoted field: the numbers
/// after a name are read from here, so a name that looks like `cx=5` cannot
/// stand in for one.
fn afterQuoted(rest: []const u8) []const u8 {
    const open = std.mem.indexOfScalar(u8, rest, '"') orelse return "";
    const close = std.mem.indexOfScalarPos(u8, rest, open + 1, '"') orelse return "";
    return rest[close + 1 ..];
}

/// Reads the trace lines out of a test game's log. Lines are split at `\n`
/// with a trailing `\r` trimmed; anything that does not start with the prefix
/// is not ours. A line cut short (the game was killed mid-write) simply
/// leaves the fields it never reached unset.
pub fn parseMapTrace(log: []const u8) MapTraceSummary {
    var summary: MapTraceSummary = .{};
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw_line| {
        const line = std.mem.trimEnd(u8, raw_line, "\r");
        if (!std.mem.startsWith(u8, line, map_trace_prefix)) continue;
        const body = line[map_trace_prefix.len..];
        const space = std.mem.indexOfScalar(u8, body, ' ') orelse body.len;
        const kind = body[0..space];
        const rest = body[space..];
        if (std.mem.eql(u8, kind, "camera")) {
            if (summary.camera != null) continue;
            const x = floatField(rest, "x") orelse continue;
            const y = floatField(rest, "y") orelse continue;
            const z = floatField(rest, "z") orelse continue;
            const name = tokenValue(rest, "source") orelse "";
            const source: CameraSource = if (std.mem.eql(u8, name, "player")) .player else if (std.mem.eql(u8, name, "neutral")) .neutral else if (std.mem.eql(u8, name, "units")) .units else .unknown;
            summary.camera = .{ .x = x, .y = y, .z = z, .source = source };
        } else if (std.mem.eql(u8, kind, "terrain")) {
            if (summary.roads == null) summary.roads = countField(rest, "roads");
            if (summary.rivers == null) summary.rivers = countField(rest, "rivers");
        } else if (std.mem.eql(u8, kind, "script")) {
            if (summary.script != null) continue;
            const name = quotedField(rest, "name=\"") orelse continue;
            const numbers = afterQuoted(rest);
            const loaded = countField(numbers, "loaded") orelse continue;
            const init = countField(numbers, "init") orelse continue;
            summary.script = .{ .name = .from(name), .loaded = loaded != 0, .init = init != 0 };
        } else if (std.mem.eql(u8, kind, "area")) {
            const name = quotedField(rest, "name=\"") orelse continue;
            const numbers = afterQuoted(rest);
            const cx = floatField(numbers, "cx") orelse continue;
            const cy = floatField(numbers, "cy") orelse continue;
            summary.areas.add(.{ .name = .from(name), .cx = cx, .cy = cy });
        } else if (std.mem.eql(u8, kind, "group")) {
            const id = intField(rest, "id") orelse continue;
            const held = countField(rest, "held") orelse continue;
            summary.groups.add(.{ .id = id, .held = held });
        } else if (std.mem.eql(u8, kind, "bridges")) {
            if (summary.bridges == null) summary.bridges = countField(rest, "n");
        } else if (std.mem.eql(u8, kind, "entrenchments")) {
            if (summary.entrenchments == null) summary.entrenchments = countField(rest, "n");
        } else if (std.mem.eql(u8, kind, "startcmd")) {
            if (summary.startcmd_launched == null) summary.startcmd_launched = countField(rest, "launched");
        } else if (std.mem.eql(u8, kind, "reserve")) {
            if (summary.reserve_applied == null) summary.reserve_applied = countField(rest, "applied");
        } else if (std.mem.eql(u8, kind, "general")) {
            const side = intField(rest, "side") orelse continue;
            const parcels = countField(rest, "parcels") orelse continue;
            const mobile = countField(rest, "mobile") orelse continue;
            summary.generals.add(.{ .side = side, .parcels = parcels, .mobile = mobile });
        } else if (std.mem.eql(u8, kind, "parcel")) {
            const side = intField(rest, "side") orelse continue;
            const idx = countField(rest, "idx") orelse continue;
            const parcel_kind = intField(rest, "type") orelse continue;
            const cx = floatField(rest, "cx") orelse continue;
            const cy = floatField(rest, "cy") orelse continue;
            const r = floatField(rest, "r") orelse continue;
            const dir = countField(rest, "dir") orelse continue;
            summary.parcels.add(.{ .side = side, .idx = idx, .kind = parcel_kind, .cx = cx, .cy = cy, .r = r, .dir = dir });
        } else if (std.mem.eql(u8, kind, "lua")) {
            const text = if (std.mem.startsWith(u8, rest, " ")) rest[1..] else rest;
            summary.lua.add(.from(text));
        }
    }
    return summary;
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

test "parseMapTrace: the camera and the terrain's roads and rivers" {
    const log =
        "BK_AUTO_UI: frame 120 at 1 ms game 2 ms\n" ++
        "BK_MAP_TRACE: camera x=1200 y=2500 z=31 source=player\r\n" ++
        "BK_MAP_TRACE: terrain roads=3 rivers=0\n" ++
        "BK_MAP_TRACE: someday-item foo=1\n";
    const trace = parseMapTrace(log);
    try std.testing.expectEqual(@as(f32, 1200), trace.camera.?.x);
    try std.testing.expectEqual(@as(f32, 2500), trace.camera.?.y);
    try std.testing.expectEqual(@as(f32, 31), trace.camera.?.z);
    try std.testing.expectEqual(CameraSource.player, trace.camera.?.source);
    try std.testing.expectEqual(@as(?u32, 3), trace.roads);
    try std.testing.expectEqual(@as(?u32, 0), trace.rivers);
}

test "parseMapTrace: the other camera sources, and one the parser does not know" {
    try std.testing.expectEqual(CameraSource.neutral, parseMapTrace("BK_MAP_TRACE: camera x=1 y=2 z=3 source=neutral\n").camera.?.source);
    try std.testing.expectEqual(CameraSource.units, parseMapTrace("BK_MAP_TRACE: camera x=1 y=2 z=3 source=units\n").camera.?.source);
    try std.testing.expectEqual(CameraSource.unknown, parseMapTrace("BK_MAP_TRACE: camera x=1 y=2 z=3 source=teleport\n").camera.?.source);
    // Only the first camera line counts: the mission starts once.
    try std.testing.expectEqual(@as(f32, 1), parseMapTrace("BK_MAP_TRACE: camera x=1 y=2 z=3 source=player\nBK_MAP_TRACE: camera x=9 y=9 z=9 source=units\n").camera.?.x);
}

test "parseMapTrace: a log without any trace line reports nothing, not zeros" {
    const trace = parseMapTrace("BK_AUTO_UI: shot written\nBK_SOUND_TRACE: dump t=1\n");
    try std.testing.expectEqual(@as(?TraceCamera, null), trace.camera);
    try std.testing.expectEqual(@as(?u32, null), trace.roads);
    try std.testing.expectEqual(@as(?u32, null), trace.rivers);
    try std.testing.expectEqual(@as(?TraceCamera, null), parseMapTrace("").camera);
}

test "parseMapTrace: a truncated or malformed line leaves its fields unset" {
    // Killed mid-write: the y and z never arrived, so there is no camera.
    try std.testing.expectEqual(@as(?TraceCamera, null), parseMapTrace("BK_MAP_TRACE: camera x=12").camera);
    try std.testing.expectEqual(@as(?TraceCamera, null), parseMapTrace("BK_MAP_TRACE: camera x=1 y=oops z=3 source=player\n").camera);
    try std.testing.expectEqual(@as(?TraceCamera, null), parseMapTrace("BK_MAP_TRACE: camera x=nan y=1 z=3 source=player\n").camera);
    try std.testing.expectEqual(@as(?TraceCamera, null), parseMapTrace("BK_MAP_TRACE: camera\n").camera);
    try std.testing.expectEqual(@as(?TraceCamera, null), parseMapTrace("BK_MAP_TRACE: ").camera);
    // "roads=" cut off before its digits, and a rivers value that is not one.
    const cut = parseMapTrace("BK_MAP_TRACE: terrain roads= rivers=x2\n");
    try std.testing.expectEqual(@as(?u32, null), cut.roads);
    try std.testing.expectEqual(@as(?u32, null), cut.rivers);
    // A line that only resembles ours is not ours.
    try std.testing.expectEqual(@as(?TraceCamera, null), parseMapTrace("xBK_MAP_TRACE: camera x=1 y=2 z=3 source=player\n").camera);
}

test "parseMapTrace: the script, by its file name" {
    const trace = parseMapTrace("BK_MAP_TRACE: script name=\"coldwinter\" loaded=1 init=1\n");
    try std.testing.expectEqualStrings("coldwinter", trace.script.?.name.slice());
    try std.testing.expect(trace.script.?.loaded);
    try std.testing.expect(trace.script.?.init);
    const missing = parseMapTrace("BK_MAP_TRACE: script name=\"\" loaded=0 init=0\n");
    try std.testing.expectEqualStrings("", missing.script.?.name.slice());
    try std.testing.expect(!missing.script.?.loaded);
    try std.testing.expect(!missing.script.?.init);
    try std.testing.expectEqual(@as(?TraceScript, null), parseMapTrace("").script);
}

test "parseMapTrace: areas keep their names (with spaces) and centres, and the first 32" {
    const one = parseMapTrace("BK_MAP_TRACE: area name=\"Hill 4 cx=9\" cx=120 cy=340\r\nBK_MAP_TRACE: area name=\"A2\" cx=-5 cy=6\n");
    try std.testing.expectEqual(@as(u32, 2), one.areas.seen);
    try std.testing.expectEqualStrings("Hill 4 cx=9", one.areas.slice()[0].name.slice());
    try std.testing.expectEqual(@as(f32, 120), one.areas.slice()[0].cx);
    try std.testing.expectEqual(@as(f32, 340), one.areas.slice()[0].cy);
    try std.testing.expectEqual(@as(f32, -5), one.areas.slice()[1].cx);
    // More lines than the summary keeps: all are counted, the first are kept.
    var log: [40 * 48]u8 = undefined;
    var len: usize = 0;
    for (0..40) |n| len += (std.fmt.bufPrint(log[len..], "BK_MAP_TRACE: area name=\"a{d}\" cx={d} cy=0\n", .{ n, n }) catch unreachable).len;
    const many = parseMapTrace(log[0..len]);
    try std.testing.expectEqual(@as(u32, 40), many.areas.seen);
    try std.testing.expectEqual(@as(usize, max_areas), many.areas.slice().len);
    try std.testing.expectEqualStrings("a31", many.areas.slice()[31].name.slice());
    try std.testing.expectEqual(@as(u32, 0), parseMapTrace("BK_MAP_TRACE: camera x=1 y=2 z=3 source=player\n").areas.seen);
}

test "parseMapTrace: groups, bridges, entrenchments, start commands, reserve positions" {
    const log =
        "BK_MAP_TRACE: group id=1 held=6\n" ++
        "BK_MAP_TRACE: group id=12 held=0\n" ++
        "BK_MAP_TRACE: bridges n=2\n" ++
        "BK_MAP_TRACE: entrenchments n=0\n" ++
        "BK_MAP_TRACE: startcmd launched=3\n" ++
        "BK_MAP_TRACE: reserve applied=4\n";
    const trace = parseMapTrace(log);
    try std.testing.expectEqual(@as(u32, 2), trace.groups.seen);
    try std.testing.expectEqual(@as(i32, 1), trace.groups.slice()[0].id);
    try std.testing.expectEqual(@as(u32, 6), trace.groups.slice()[0].held);
    try std.testing.expectEqual(@as(i32, 12), trace.groups.slice()[1].id);
    try std.testing.expectEqual(@as(u32, 0), trace.groups.slice()[1].held);
    try std.testing.expectEqual(@as(?u32, 2), trace.bridges);
    try std.testing.expectEqual(@as(?u32, 0), trace.entrenchments);
    try std.testing.expectEqual(@as(?u32, 3), trace.startcmd_launched);
    try std.testing.expectEqual(@as(?u32, 4), trace.reserve_applied);
    // Not reported is not zero.
    const none = parseMapTrace("BK_AUTO_UI: shot written\n");
    try std.testing.expectEqual(@as(?u32, null), none.bridges);
    try std.testing.expectEqual(@as(?u32, null), none.entrenchments);
    try std.testing.expectEqual(@as(?u32, null), none.startcmd_launched);
    try std.testing.expectEqual(@as(?u32, null), none.reserve_applied);
    try std.testing.expectEqual(@as(u32, 0), none.groups.seen);
}

test "parseMapTrace: generals and their parcels" {
    const log =
        "BK_MAP_TRACE: general side=1 parcels=2 mobile=1\n" ++
        "BK_MAP_TRACE: parcel side=1 idx=0 type=1 cx=800 cy=900 r=320 dir=16384\n" ++
        "BK_MAP_TRACE: parcel side=1 idx=1 type=2 cx=100 cy=200 r=64 dir=0\n";
    const trace = parseMapTrace(log);
    try std.testing.expectEqual(@as(u32, 1), trace.generals.seen);
    try std.testing.expectEqual(@as(i32, 1), trace.generals.slice()[0].side);
    try std.testing.expectEqual(@as(u32, 2), trace.generals.slice()[0].parcels);
    try std.testing.expectEqual(@as(u32, 1), trace.generals.slice()[0].mobile);
    try std.testing.expectEqual(@as(u32, 2), trace.parcels.seen);
    const first = trace.parcels.slice()[0];
    try std.testing.expectEqual(@as(i32, 1), first.side);
    try std.testing.expectEqual(@as(u32, 0), first.idx);
    try std.testing.expectEqual(@as(i32, 1), first.kind);
    try std.testing.expectEqual(@as(f32, 800), first.cx);
    try std.testing.expectEqual(@as(f32, 900), first.cy);
    try std.testing.expectEqual(@as(f32, 320), first.r);
    try std.testing.expectEqual(@as(u32, 16384), first.dir);
    try std.testing.expectEqual(@as(i32, 2), trace.parcels.slice()[1].kind);
}

test "parseMapTrace: the Lua Trace lines keep their text, cut to fit, the first 16" {
    const trace = parseMapTrace("BK_MAP_TRACE: lua hello 12 world\r\nBK_MAP_TRACE: lua second\n");
    try std.testing.expectEqual(@as(u32, 2), trace.lua.seen);
    try std.testing.expectEqualStrings("hello 12 world", trace.lua.slice()[0].slice());
    try std.testing.expectEqualStrings("second", trace.lua.slice()[1].slice());
    var long_line: [text_capacity * 2 + 32]u8 = undefined;
    const prefix = "BK_MAP_TRACE: lua ";
    @memcpy(long_line[0..prefix.len], prefix);
    @memset(long_line[prefix.len..][0 .. text_capacity * 2], 'x');
    const cut = parseMapTrace(long_line[0 .. prefix.len + text_capacity * 2]);
    try std.testing.expectEqual(@as(usize, text_capacity), cut.lua.slice()[0].slice().len);
    var log: [20 * 32]u8 = undefined;
    var len: usize = 0;
    for (0..20) |n| len += (std.fmt.bufPrint(log[len..], "BK_MAP_TRACE: lua line {d}\n", .{n}) catch unreachable).len;
    const many = parseMapTrace(log[0..len]);
    try std.testing.expectEqual(@as(u32, 20), many.lua.seen);
    try std.testing.expectEqual(@as(usize, max_lua_lines), many.lua.slice().len);
    try std.testing.expectEqualStrings("line 15", many.lua.slice()[15].slice());
}

test "parseMapTrace: truncated lines of every kind add nothing" {
    const log =
        "BK_MAP_TRACE: script name=\"cold\n" ++
        "BK_MAP_TRACE: script name=\"cold\" loaded=1\n" ++
        "BK_MAP_TRACE: area name=\"Ar\n" ++
        "BK_MAP_TRACE: area name=\"Area\" cx=1\n" ++
        "BK_MAP_TRACE: group id=3 held=\n" ++
        "BK_MAP_TRACE: group id=x held=1\n" ++
        "BK_MAP_TRACE: bridges n=\n" ++
        "BK_MAP_TRACE: entrenchments\n" ++
        "BK_MAP_TRACE: startcmd launched=-1\n" ++
        "BK_MAP_TRACE: reserve appl\n" ++
        "BK_MAP_TRACE: general side=1 parcels=2\n" ++
        "BK_MAP_TRACE: parcel side=1 idx=0 type=1 cx=8 cy=9 r=3\n";
    const trace = parseMapTrace(log);
    try std.testing.expectEqual(@as(?TraceScript, null), trace.script);
    try std.testing.expectEqual(@as(u32, 0), trace.areas.seen);
    try std.testing.expectEqual(@as(u32, 0), trace.groups.seen);
    try std.testing.expectEqual(@as(?u32, null), trace.bridges);
    try std.testing.expectEqual(@as(?u32, null), trace.entrenchments);
    try std.testing.expectEqual(@as(?u32, null), trace.startcmd_launched);
    try std.testing.expectEqual(@as(?u32, null), trace.reserve_applied);
    try std.testing.expectEqual(@as(u32, 0), trace.generals.seen);
    try std.testing.expectEqual(@as(u32, 0), trace.parcels.seen);
}
