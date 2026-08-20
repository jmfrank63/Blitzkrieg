//! rclone binary discovery, version gate, and the supervisor for the
//! `rclone rcd` child the rest of the module talks to.
//!
//! rclone is an optional external dependency: the game neither bundles nor
//! downloads it, so on most machines it is simply absent. Absence is not a
//! failure — it disables cloud sync — which is why `discover` answers `null`
//! rather than an error, and why the only error this file can return is
//! `OutOfMemory`.
//!
//! When a binary *is* present but unusable, the caller needs to know which
//! kind of unusable it is: `.too_old` asks the player to upgrade, `.not_found`
//! asks them to install or point at one, `.not_executable` asks them to fix a
//! permission. A bare "unavailable" leaves them with nothing to act on, so
//! `resolve` returns a typed `Reason` and, where one exists, the path and
//! version it rejected.
//!
//! Search order: the explicit path from `cloud.credentials`, then the game
//! directory, then `PATH`. The explicit path never falls back — the settings
//! dialog offers the override precisely because `PATH` may not reach a working
//! rclone, and quietly using a different binary than the one the player named
//! would report a working sync that their override says nothing about, while
//! hiding a typo forever.
//!
//! **Everything here blocks the calling thread**, `probeVersion` most of all:
//! it spawns a process and waits for it. Like `rc.zig`, this belongs on the
//! worker thread, never on the game's main loop. The probe is bounded by
//! `probe_timeout_ms` — a binary that hangs is as useless as one that is
//! missing, and the bound is `std.process.run`'s own `Io` timeout, which
//! kills the child on expiry, not a socket or file option.
//!
//! The `*In` variants exist for two reasons: a caller that already owns an
//! `Io` should not build a second one, and the tests must be able to search a
//! fabricated `PATH` so that their result cannot depend on whether the machine
//! running them happens to have rclone installed.

const std = @import("std");
const builtin = @import("builtin");

const Allocator = std.mem.Allocator;
const Io = std.Io;
const path = Io.Dir.path;

/// The file name to look for in a directory.
pub const exe_name = if (builtin.os.tag == .windows) "rclone.exe" else "rclone";

/// The oldest rclone this plan can drive. 1.66 is where `resyncMode` arrived,
/// where `backupDir1`/`backupDir2` became rc parameters, and where
/// `bilib.BasePath` began writing listing files under readable names instead
/// of the older `{hexstring}` form. Every one of those is load-bearing here.
pub const MIN_RCLONE: Version = .{ .major = 1, .minor = 66, .patch = 0 };

/// How long `rclone version` may take before the binary is written off. It
/// normally answers in single-digit milliseconds; this bound is for the
/// pathological cases — a binary on a stalled network mount, or one waiting on
/// something that will never arrive.
pub const probe_timeout_ms: u32 = 10_000;

pub const Version = struct {
    major: u32,
    minor: u32,
    patch: u32,

    pub fn order(self: Version, other: Version) std.math.Order {
        if (self.major != other.major) return std.math.order(self.major, other.major);
        if (self.minor != other.minor) return std.math.order(self.minor, other.minor);
        return std.math.order(self.patch, other.patch);
    }

    /// True when this version satisfies `minimum`, the minimum included.
    pub fn atLeast(self: Version, minimum: Version) bool {
        return self.order(minimum) != .lt;
    }
};

/// Why cloud sync is unavailable. The settings screen names the reason, so
/// these are three separate states rather than one: a chmod fixes
/// `.not_executable`, an install or an override fixes `.not_found`, and only
/// an upgrade fixes `.too_old`.
pub const Reason = enum { not_found, too_old, not_executable };

/// Where to look. A `null` field means "ask the operating system": the
/// executable's own directory for `game_dir`, the `PATH` variable for
/// `path_env`. An empty string means "this location holds nothing", which is
/// how a test excludes the machine it runs on from the search.
pub const Search = struct {
    /// `Cloud.Rclone.Path` from `cloud.credentials`. Empty is the same as
    /// unset; anything else is taken verbatim, and is the only candidate.
    explicit: ?[]const u8 = null,
    game_dir: ?[]const u8 = null,
    path_env: ?[]const u8 = null,
};

pub const Found = struct {
    /// Owned by the `Availability`.
    path: []const u8,
    version: Version,
};

pub const Rejected = struct {
    reason: Reason,
    /// The candidate that was rejected, when there was one at all. Owned by
    /// the `Availability`. Always null for `.not_found`.
    path: ?[]const u8,
    /// The version behind a `.too_old` rejection, so the UI can print what it
    /// found next to what it needs.
    version: ?Version,
};

pub const Availability = union(enum) {
    ready: Found,
    unavailable: Rejected,

    /// Null when rclone is usable.
    pub fn reason(self: Availability) ?Reason {
        return switch (self) {
            .ready => null,
            .unavailable => |rejected| rejected.reason,
        };
    }

    pub fn deinit(self: *Availability, gpa: Allocator) void {
        switch (self.*) {
            .ready => |found| gpa.free(found.path),
            .unavailable => |rejected| if (rejected.path) |owned| gpa.free(owned),
        }
        self.* = undefined;
    }
};

/// A missing rclone is not an error, so the only failure here is allocation.
pub const DiscoverError = Allocator.Error;

pub const ProbeError = error{
    /// Nothing is at that path any more.
    NotFound,
    /// It is there, but the operating system refused to run it, or it never
    /// answered.
    NotExecutable,
    /// It ran, but did not print a version this file can read — which means it
    /// is not the program we think it is.
    UnreadableVersion,
} || Allocator.Error;

/// Locate an rclone binary. Returns the path, owned by `gpa`, or `null` when
/// there is none — see the search order at the top of this file.
pub fn discover(gpa: Allocator, explicit: ?[]const u8) DiscoverError!?[]const u8 {
    var threaded: Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return discoverIn(gpa, threaded.io(), .{ .explicit = explicit });
}

pub fn discoverIn(gpa: Allocator, io: Io, search: Search) DiscoverError!?[]const u8 {
    if (search.explicit) |explicit| {
        if (explicit.len != 0) {
            // Executability is deliberately not required here: an unreadable
            // permission on a path the player typed must surface as
            // `.not_executable`, not vanish into "nothing found".
            if (!isRegularFile(io, explicit)) return null;
            return try gpa.dupe(u8, explicit);
        }
    }

    var game_buffer: [Io.Dir.max_path_bytes]u8 = undefined;
    const game_dir: []const u8 = if (search.game_dir) |dir| dir else game: {
        const len = std.process.executableDirPath(io, &game_buffer) catch break :game "";
        break :game game_buffer[0..len];
    };
    if (game_dir.len != 0) {
        if (try candidateIn(gpa, io, game_dir)) |found| return found;
    }

    var owned_path_env: ?[]u8 = null;
    defer if (owned_path_env) |owned| gpa.free(owned);
    const path_env: []const u8 = if (search.path_env) |value| value else env: {
        owned_path_env = pathVariable(gpa);
        break :env owned_path_env orelse "";
    };

    var entries = std.mem.splitScalar(u8, path_env, path.delimiter);
    while (entries.next()) |raw| {
        // Windows tolerates quoted PATH entries; the quotes are not part of
        // the directory name.
        const entry = std.mem.trim(u8, raw, "\"");
        if (entry.len == 0) continue;
        if (try candidateIn(gpa, io, entry)) |found| return found;
    }
    return null;
}

/// Run `<binary> version` and read the leading `vMAJOR.MINOR.PATCH`.
pub fn probeVersion(gpa: Allocator, binary: []const u8) ProbeError!Version {
    var threaded: Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return probeVersionIn(gpa, threaded.io(), binary);
}

pub fn probeVersionIn(gpa: Allocator, io: Io, binary: []const u8) ProbeError!Version {
    const result = std.process.run(gpa, io, .{
        .argv = &.{ binary, "version" },
        // The banner is nine short lines; anything larger is not rclone.
        .stdout_limit = .limited(64 * 1024),
        .stderr_limit = .limited(64 * 1024),
        .timeout = .{ .duration = .{
            .raw = .fromMilliseconds(probe_timeout_ms),
            .clock = .awake,
        } },
    }) catch |err| return switch (err) {
        error.OutOfMemory => error.OutOfMemory,
        error.FileNotFound, error.NotDir => error.NotFound,
        // AccessDenied, PermissionDenied, IsDir, InvalidExe, Timeout, and the
        // rest all mean the same thing to a player: this file did not run.
        else => error.NotExecutable,
    };
    defer gpa.free(result.stdout);
    defer gpa.free(result.stderr);

    switch (result.term) {
        .exited => |code| if (code != 0) return error.UnreadableVersion,
        else => return error.UnreadableVersion,
    }
    return parseVersion(result.stdout) orelse error.UnreadableVersion;
}

/// Discover a binary and hold it to `MIN_RCLONE`, reporting which of the three
/// ways it failed. Only allocation can error; every other outcome is a state
/// the UI is expected to show.
pub fn resolve(gpa: Allocator, explicit: ?[]const u8) Allocator.Error!Availability {
    var threaded: Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return resolveIn(gpa, threaded.io(), .{ .explicit = explicit });
}

pub fn resolveIn(gpa: Allocator, io: Io, search: Search) Allocator.Error!Availability {
    const candidate = try discoverIn(gpa, io, search) orelse return .{ .unavailable = .{
        .reason = .not_found,
        .path = null,
        .version = null,
    } };
    errdefer gpa.free(candidate);

    const version = probeVersionIn(gpa, io, candidate) catch |err| switch (err) {
        error.OutOfMemory => return error.OutOfMemory,
        // Discovery saw the file a moment ago; it has since gone.
        error.NotFound => return .{ .unavailable = .{
            .reason = .not_found,
            .path = candidate,
            .version = null,
        } },
        error.NotExecutable, error.UnreadableVersion => return .{ .unavailable = .{
            .reason = .not_executable,
            .path = candidate,
            .version = null,
        } },
    };

    if (!version.atLeast(MIN_RCLONE)) return .{ .unavailable = .{
        .reason = .too_old,
        .path = candidate,
        .version = version,
    } };

    return .{ .ready = .{ .path = candidate, .version = version } };
}

/// Read a version out of `rclone version` output. Only the first line is
/// considered: the lines below it are full of other version numbers —
/// `os/version`, `go/version` — and any of them would be a plausible-looking
/// wrong answer.
pub fn parseVersion(text: []const u8) ?Version {
    const line_end = std.mem.findScalar(u8, text, '\n') orelse text.len;
    var tokens = std.mem.tokenizeAny(u8, text[0..line_end], " \t\r");
    while (tokens.next()) |token| {
        if (token.len < 2 or token[0] != 'v') continue;
        if (!std.ascii.isDigit(token[1])) continue;
        return parseTriple(token[1..]);
    }
    return null;
}

/// `MAJOR.MINOR[.PATCH]`, stopping at the first character that is neither a
/// digit nor the expected separator: releases print `1.75.0`, betas print
/// `1.66.0-beta.7519.deadbeef`, and a build from source prints `1.70.3-DEV`.
fn parseTriple(text: []const u8) ?Version {
    var cursor: usize = 0;
    const major = takeNumber(text, &cursor) orelse return null;
    if (cursor >= text.len or text[cursor] != '.') return null;
    cursor += 1;
    const minor = takeNumber(text, &cursor) orelse return null;

    var patch: u32 = 0;
    if (cursor < text.len and text[cursor] == '.') {
        cursor += 1;
        patch = takeNumber(text, &cursor) orelse return null;
    }
    return .{ .major = major, .minor = minor, .patch = patch };
}

fn takeNumber(text: []const u8, cursor: *usize) ?u32 {
    const start = cursor.*;
    while (cursor.* < text.len and std.ascii.isDigit(text[cursor.*])) cursor.* += 1;
    if (cursor.* == start) return null;
    return std.fmt.parseInt(u32, text[start..cursor.*], 10) catch null;
}

/// `<dir>/rclone`, if that is a file this process may execute. A directory in
/// the search space that holds something unrunnable under the name is skipped
/// rather than accepted, so one stray file cannot shadow a real installation
/// further down `PATH`.
fn candidateIn(gpa: Allocator, io: Io, dir: []const u8) Allocator.Error!?[]const u8 {
    const candidate = try path.join(gpa, &.{ dir, exe_name });
    if (!isExecutableFile(io, candidate)) {
        gpa.free(candidate);
        return null;
    }
    return candidate;
}

fn isRegularFile(io: Io, file_path: []const u8) bool {
    // Symlinks are followed: a packaged rclone is routinely a link into a
    // versioned directory.
    const stat = Io.Dir.cwd().statFile(io, file_path, .{}) catch return false;
    return stat.kind == .file;
}

fn isExecutableFile(io: Io, file_path: []const u8) bool {
    if (!isRegularFile(io, file_path)) return false;
    if (builtin.os.tag == .windows) return true;
    Io.Dir.cwd().access(io, file_path, .{ .execute = true }) catch return false;
    return true;
}

/// The process `PATH`, copied out. Zig 0.16 hands the environment to `main`,
/// which this module does not have — it is compiled into a library the game
/// loads — so the variable is read from the platform instead.
fn pathVariable(gpa: Allocator) ?[]u8 {
    if (builtin.os.tag == .windows) {
        const environ: std.process.Environ = .{ .block = .global };
        return environ.getAlloc(gpa, "PATH") catch null;
    }
    const raw = std.c.getenv("PATH") orelse return null;
    return gpa.dupe(u8, std.mem.span(raw)) catch null;
}

// -- daemon supervision ------------------------------------------------------
//
// The game owns the `rclone rcd` process it talks to, which means it must be
// able to start one privately, know when it is ready, and — the hard part —
// recognise its own leftovers after a crash without ever touching a process
// that merely looks like one.
//
// Privately: the address is 127.0.0.1 on a port taken from the kernel this
// launch, the credentials are minted from this launch's nonce, and the child's
// `RCLONE_CONFIG` points inside the game directory. A fixed port would collide
// with a second copy of the game and hand a local process a predictable
// target; a fixed credential would make every installation share one password.
// The config redirection matters just as much: the daemon must never read or
// write the player's own `rclone.conf`, and rclone will happily do both if it
// is allowed to find one.
//
// Recognising its own: `<gamedir>/cloudsync/daemon.json` records
// `{ pid, process_start_time, nonce, port }`. A pid alone is not identity —
// pids recycle, and on a machine that reboots between launches the recorded
// number will belong to something else entirely. So all three of these must
// agree before anything is killed:
//
//   1. the pid is running;
//   2. its start time equals the recorded one;
//   3. it answers `core/version` on the recorded port with the credentials
//      derived from the recorded nonce.
//
// (1) and (2) together survive pid recycling; (3) survives the case where the
// kernel hands out the same pid at the same second to an unrelated program.
// When any of them fails the process is **left alone** and a fresh daemon
// starts on a new port. Leaking one rclone is a bug that costs memory until
// the next reboot; killing a process that was never ours can cost a player
// their work, and there is no version of this file where that trade is worth
// making.
//
// The outcome is returned rather than printed, because the game has no logging
// façade yet and a test binary that writes to stderr fails its build step.
// `Daemon.reap` carries the refused pid for whoever surfaces it later.

const rc = @import("rc.zig");

/// Machine-local supervisor state, a sibling of `profiles/` and never synced:
/// every field in it describes one process on one machine.
pub const state_dir_name = "cloudsync";
pub const record_file_name = "daemon.json";
pub const log_file_name = "rcd.log";
/// The child's `RCLONE_CONFIG`. It need not exist — rclone creates it on
/// demand — but naming it keeps the daemon away from `~/.config/rclone`.
pub const config_file_name = "rclone.conf";

/// Hex characters in the per-launch nonce. The first half becomes `--rc-user`
/// and the second half `--rc-pass`, so each is 128 bits and neither can be
/// derived from the other half leaking. One nonce, two credentials, keeps the
/// record file to the four fields the design names.
pub const nonce_len = 64;

/// Measured cold start on macOS arm64 is under three seconds. The rest of the
/// budget is for a machine where the binary is on a cold or networked disk.
pub const ready_timeout_ms: u32 = 15_000;

/// How often `waitReady` re-asks. Short enough that a fast start is not
/// penalised, long enough not to spin.
const ready_poll_ms: u32 = 100;

/// How long a stale daemon gets to die politely before it is killed outright.
const reap_grace_ms: u32 = 5_000;

/// Bytes of `rcd.log` attached to a readiness failure. The interesting part —
/// a port already bound, a config that cannot be written — is always at the
/// end.
const log_tail_bytes: usize = 4096;

pub const Pid = if (builtin.os.tag == .windows) u32 else std.posix.pid_t;

pub const SpawnError = error{
    /// `<gamedir>/cloudsync` could not be created.
    StateDirUnavailable,
    /// The kernel would not hand out a loopback port.
    PortUnavailable,
    /// The binary did not start. Discovery said it was executable, so this is
    /// a race or a broken image.
    SpawnFailed,
} || Allocator.Error;

/// Expiry is `.daemon_timeout`, and `Daemon.logTail` holds what the daemon
/// managed to say before giving up.
pub const Failure = enum { daemon_timeout };

pub const ReadyError = error{DaemonTimeout};

/// What `reapStale` did about the record it found. Only `.reaped` involves
/// killing anything.
pub const ReapOutcome = enum {
    /// No record, or one that could not be read. Nothing to do.
    none,
    /// The recorded pid is not running. The record is deleted.
    already_gone,
    /// All three identity checks passed; the process was terminated.
    reaped,
    /// The pid is alive but its start time is not the recorded one, so it is
    /// a different process wearing a recycled number. Left running.
    refused_foreign_process,
    /// Pid and start time agree, but the process did not answer `core/version`
    /// with the recorded nonce's credentials, so it is not our daemon. Left
    /// running.
    refused_unauthenticated,
};

pub const Reap = struct {
    outcome: ReapOutcome,
    /// The pid the record named, kept for whoever reports a refusal.
    pid: ?i64 = null,
};

/// The on-disk record, and the only thing standing between a crashed game and
/// an orphaned daemon.
pub const Record = struct {
    pid: i64,
    process_start_time: i64,
    nonce: []const u8,
    port: u16,
};

pub const Options = struct {
    /// The binary `resolve` accepted.
    binary: []const u8,
    /// Where `cloudsync/` lives. Absolute.
    game_dir: []const u8,
};

/// A running `rclone rcd` and everything needed to identify it again.
///
/// The struct is returned by value, so it must be stored in a stable location
/// before `endpoint` is called: the endpoint borrows `nonce` in place rather
/// than allocating a copy of a secret.
pub const Daemon = struct {
    gpa: Allocator,
    io: Io,
    child: std.process.Child,
    pid: Pid,
    port: u16,
    nonce: [nonce_len]u8,
    /// `<gamedir>/cloudsync`, and the three paths beneath it. All owned.
    state_dir: []u8,
    record_path: []u8,
    log_path: []u8,
    config_path: []u8,
    /// Held open for the daemon's lifetime so the tail can be read without
    /// racing the child's own handle, and closed by `shutdown`.
    log: ?Io.File,
    /// Windows only: the child is in a job object marked
    /// `KILL_ON_JOB_CLOSE`, so the operating system reaps it even if the game
    /// dies without running any of this code.
    job: if (builtin.os.tag == .windows) ?std.os.windows.HANDLE else void,
    /// What startup found left over from a previous launch.
    reap: Reap,
    failure: ?Failure,
    log_tail_owned: ?[]u8,
    stopped: bool,

    /// `--rc-user`: the first half of the nonce.
    pub fn user(self: *const Daemon) []const u8 {
        return self.nonce[0 .. nonce_len / 2];
    }

    /// `--rc-pass`: the second half.
    pub fn pass(self: *const Daemon) []const u8 {
        return self.nonce[nonce_len / 2 ..];
    }

    pub fn endpoint(self: *const Daemon) rc.Endpoint {
        return .{
            .host = "127.0.0.1",
            .port = self.port,
            .user = self.user(),
            .pass = self.pass(),
        };
    }

    /// Whatever the daemon wrote before a readiness failure. Empty until one
    /// happens, because an rc reply is a better diagnostic when there is one.
    pub fn logTail(self: *const Daemon) []const u8 {
        return self.log_tail_owned orelse "";
    }

    /// Reap a previous launch's leftovers, reserve a port, mint a nonce, and
    /// start the child. The daemon is not usable until `waitReady` returns.
    pub fn spawn(gpa: Allocator, io: Io, options: Options) SpawnError!Daemon {
        const state_dir = try path.join(gpa, &.{ options.game_dir, state_dir_name });
        errdefer gpa.free(state_dir);
        Io.Dir.cwd().createDirPath(io, state_dir) catch return error.StateDirUnavailable;

        const record_path = try path.join(gpa, &.{ state_dir, record_file_name });
        errdefer gpa.free(record_path);
        const log_path = try path.join(gpa, &.{ state_dir, log_file_name });
        errdefer gpa.free(log_path);
        const config_path = try path.join(gpa, &.{ state_dir, config_file_name });
        errdefer gpa.free(config_path);

        // Before anything binds a port: a leftover daemon from a crashed run
        // is the reason this whole record mechanism exists.
        const reap = try reapStale(gpa, io, options.game_dir);

        const port = reservePort(io) catch return error.PortUnavailable;
        const nonce = newNonce(io);

        // Truncate rather than append: the tail attached to a failure must
        // describe this launch, not the last one.
        var log = Io.Dir.cwd().createFile(io, log_path, .{ .read = true, .truncate = true }) catch
            return error.StateDirUnavailable;
        errdefer log.close(io);

        var addr_buffer: [32]u8 = undefined;
        const addr = std.fmt.bufPrint(&addr_buffer, "127.0.0.1:{d}", .{port}) catch unreachable;

        // Loopback only, on this launch's port, with this launch's
        // credentials; `--rc-serve=false` leaves the JSON API and drops the
        // file browser rclone would otherwise put on the same socket; the log
        // goes to a file rather than to the game's stderr, and INFO is the
        // level at which rclone says why it refused to start.
        const argv = [_][]const u8{
            options.binary,
            "rcd",
            "--rc-addr",
            addr,
            "--rc-serve=false",
            "--rc-user",
            nonce[0 .. nonce_len / 2],
            "--rc-pass",
            nonce[nonce_len / 2 ..],
            "--log-file",
            log_path,
            "--log-level",
            "INFO",
        };

        var environ = try childEnviron(gpa, config_path);
        defer environ.deinit();

        var child = std.process.spawn(io, .{
            .argv = &argv,
            .environ_map = &environ,
            // The game's own streams are not the daemon's to write on, and a
            // test binary that inherits a child's stderr fails its build step.
            // `--log-file` is where rclone is meant to talk.
            .stdin = .ignore,
            .stdout = .ignore,
            .stderr = .ignore,
            // Windows only: assign to the job before a single instruction of
            // rclone runs, so nothing escapes the job by spawning first.
            .start_suspended = builtin.os.tag == .windows,
        }) catch return error.SpawnFailed;
        errdefer child.kill(io);

        const pid: Pid = switch (builtin.os.tag) {
            .windows => windowsProcessId(child.id.?),
            else => child.id.?,
        };

        const job = if (builtin.os.tag == .windows) blk: {
            const handle = attachToKillOnCloseJob(child.id.?);
            resumeThread(child.thread_handle);
            break :blk handle;
        } else {};

        // Written only after the child exists: a record naming a pid that was
        // never started is a record that can only mislead the next launch. A
        // record that cannot be written at all leaks this daemon if the game
        // crashes, which is the acceptable half of the trade; so is the `0`
        // below, which is a start time no live process can match and so fails
        // every future identity check closed.
        writeRecord(gpa, io, record_path, .{
            .pid = pid,
            .process_start_time = processStartTime(io, pid) orelse 0,
            .nonce = &nonce,
            .port = port,
        }) catch {};

        return .{
            .gpa = gpa,
            .io = io,
            .child = child,
            .pid = pid,
            .port = port,
            .nonce = nonce,
            .state_dir = state_dir,
            .record_path = record_path,
            .log_path = log_path,
            .config_path = config_path,
            .log = log,
            .job = job,
            .reap = reap,
            .failure = null,
            .log_tail_owned = null,
            .stopped = false,
        };
    }

    /// Poll `core/version` until the daemon answers or `timeout_ms` expires.
    /// Expiry records `.daemon_timeout` and attaches the log tail, which is
    /// the only place rclone explains a refused port or an unwritable config.
    pub fn waitReady(self: *Daemon, timeout_ms: u32) ReadyError!void {
        const budget: Io.Clock.Duration = .{
            .raw = .fromMilliseconds(timeout_ms),
            .clock = .awake,
        };
        const expiry: Io.Clock.Timestamp = .fromNow(self.io, budget);

        while (true) {
            if (self.probeVersion()) return;
            const now = Io.Clock.awake.now(self.io);
            if (now.nanoseconds >= expiry.raw.nanoseconds) break;
            sleepMs(self.io, ready_poll_ms);
        }

        self.failure = .daemon_timeout;
        self.captureLogTail();
        return error.DaemonTimeout;
    }

    /// One `core/version` round trip with a short budget: while the daemon is
    /// still starting the connection is refused outright, so the deadline only
    /// matters for the case where something else already owns the port and
    /// never answers.
    fn probeVersion(self: *Daemon) bool {
        return answersCoreVersion(self.gpa, self.io, self.endpoint());
    }

    fn captureLogTail(self: *Daemon) void {
        if (self.log_tail_owned) |old| {
            self.gpa.free(old);
            self.log_tail_owned = null;
        }
        const file = self.log orelse return;
        const size = file.length(self.io) catch return;
        const take: usize = @intCast(@min(size, @as(u64, log_tail_bytes)));
        if (take == 0) return;
        const buffer = self.gpa.alloc(u8, take) catch return;
        const read = file.readPositionalAll(self.io, buffer, size - take) catch {
            self.gpa.free(buffer);
            return;
        };
        if (read == buffer.len) {
            self.log_tail_owned = buffer;
            return;
        }
        // A short read only happens if the child truncated the log underneath
        // us. Hand back the whole allocation if it cannot be shrunk: freeing a
        // slice shorter than what was allocated is not something an allocator
        // has to tolerate.
        self.log_tail_owned = self.gpa.realloc(buffer, read) catch buffer;
    }

    /// Terminate the child, wait for it, close the log, drop the record, and
    /// release everything owned. Idempotent, and safe to call from an error
    /// path — including one taken before `waitReady` ever succeeded.
    pub fn shutdown(self: *Daemon) void {
        if (self.stopped) return;
        self.stopped = true;

        // `kill` is SIGTERM plus wait4 on POSIX and NtTerminateProcess plus a
        // wait on Windows, so the pid is fully gone when it returns.
        self.child.kill(self.io);

        if (builtin.os.tag == .windows) {
            if (self.job) |handle| std.os.windows.CloseHandle(handle);
            self.job = null;
        }

        if (self.log) |file| file.close(self.io);
        self.log = null;

        Io.Dir.cwd().deleteFile(self.io, self.record_path) catch {};

        if (self.log_tail_owned) |tail| self.gpa.free(tail);
        self.log_tail_owned = null;
        self.gpa.free(self.config_path);
        self.gpa.free(self.log_path);
        self.gpa.free(self.record_path);
        self.gpa.free(self.state_dir);
        self.config_path = &.{};
        self.log_path = &.{};
        self.record_path = &.{};
        self.state_dir = &.{};
    }
};

/// Inspect `<game_dir>/cloudsync/daemon.json` and act on it only when all
/// three identity checks agree. See the header: a refusal leaves the process
/// running on purpose.
pub fn reapStale(gpa: Allocator, io: Io, game_dir: []const u8) Allocator.Error!Reap {
    const record_path = try path.join(gpa, &.{ game_dir, state_dir_name, record_file_name });
    defer gpa.free(record_path);

    const text = Io.Dir.cwd().readFileAlloc(io, record_path, gpa, .limited(64 * 1024)) catch
        return .{ .outcome = .none };
    defer gpa.free(text);

    const parsed = std.json.parseFromSlice(Record, gpa, text, .{
        .ignore_unknown_fields = true,
    }) catch return .{ .outcome = .none };
    defer parsed.deinit();
    const record = parsed.value;

    if (record.nonce.len != nonce_len) return .{ .outcome = .none };
    // Nothing below this line may ever see a non-positive pid: POSIX `kill`
    // reads 0 as "my whole process group" and -1 as "every process I may
    // signal", so a corrupt record could otherwise take the machine down.
    if (record.pid <= 0) return .{ .outcome = .none };
    const pid = std.math.cast(Pid, record.pid) orelse return .{ .outcome = .none };

    // (1) Is anything running under that number at all?
    const started = processStartTime(io, pid) orelse {
        Io.Dir.cwd().deleteFile(io, record_path) catch {};
        return .{ .outcome = .already_gone, .pid = record.pid };
    };

    // (2) Is it the same process the record was written about? A recycled pid
    // fails here, which is the whole reason the start time is stored.
    if (started != record.process_start_time) {
        return .{ .outcome = .refused_foreign_process, .pid = record.pid };
    }

    // (3) Does it speak rc, on that port, with that nonce's credentials? Only
    // our daemon can.
    const endpoint: rc.Endpoint = .{
        .host = "127.0.0.1",
        .port = record.port,
        .user = record.nonce[0 .. nonce_len / 2],
        .pass = record.nonce[nonce_len / 2 ..],
    };
    if (!answersCoreVersion(gpa, io, endpoint)) {
        return .{ .outcome = .refused_unauthenticated, .pid = record.pid };
    }

    terminate(io, pid);
    Io.Dir.cwd().deleteFile(io, record_path) catch {};
    return .{ .outcome = .reaped, .pid = record.pid };
}

/// A port the kernel is willing to give out, learned by binding it and letting
/// go. Something else can take it in the gap before rclone binds it, which is
/// unavoidable with this technique and is why a failed start reports the log
/// rather than guessing.
pub fn reservePort(io: Io) !u16 {
    var address: Io.net.IpAddress = .{ .ip4 = .loopback(0) };
    var server = try address.listen(io, .{});
    defer server.deinit(io);
    return server.socket.address.getPort();
}

/// A fresh credential pair as hex. `randomSecure` is the 0.16 replacement for
/// the removed `std.crypto.random`; it draws from the operating system's
/// entropy source rather than a seeded generator, and `random` — itself a
/// CSPRNG — covers the platforms where no such source exists.
pub fn newNonce(io: Io) [nonce_len]u8 {
    var raw: [nonce_len / 2]u8 = undefined;
    io.randomSecure(&raw) catch io.random(&raw);

    const digits = "0123456789abcdef";
    var out: [nonce_len]u8 = undefined;
    for (raw, 0..) |byte, index| {
        out[index * 2] = digits[byte >> 4];
        out[index * 2 + 1] = digits[byte & 0x0f];
    }
    return out;
}

pub fn currentPid() Pid {
    return switch (builtin.os.tag) {
        .windows => windowsCurrentProcessId(),
        else => std.c.getpid(),
    };
}

/// When the process with `pid` started, in whatever unit the platform counts
/// in — epoch seconds on Darwin, ticks since boot on Linux, a FILETIME on
/// Windows. The number is never interpreted, only compared for equality with
/// the one recorded earlier, so the unit does not have to be portable; what
/// matters is that it changes when the pid is reused.
///
/// Null means "no process is running under that pid", which includes the case
/// where the platform refuses to say.
pub fn processStartTime(io: Io, pid: Pid) ?i64 {
    return switch (builtin.os.tag) {
        .macos, .ios, .tvos, .watchos, .visionos => darwinStartTime(pid),
        .linux => linuxStartTime(io, pid),
        .windows => windowsStartTime(pid),
        else => null,
    };
}

/// `sysctl({CTL_KERN, KERN_PROC, KERN_PROC_PID, pid})` fills a `kinfo_proc`.
/// Its first member is `kp_proc`, a `struct extern_proc`, which opens with a
/// union whose `timeval` arm is `p_starttime` — so the start time is the first
/// eight bytes, and `p_pid` follows at offset 40. Both offsets are checked
/// against the pid we asked for, so a layout change fails closed rather than
/// returning some other field's bytes as a timestamp.
///
/// This is `libc`'s public `sysctl`, not a private entry point: `ps` reads the
/// same MIB. `proc_pidinfo(PROC_PIDTBSDINFO)` gives the same value with a
/// flatter struct and is the alternative if this ever stops holding.
fn darwinStartTime(pid: Pid) ?i64 {
    const CTL_KERN: c_int = 1;
    const KERN_PROC: c_int = 14;
    const KERN_PROC_PID: c_int = 1;

    var mib = [4]c_int{ CTL_KERN, KERN_PROC, KERN_PROC_PID, @intCast(pid) };
    // `sizeof(struct kinfo_proc)` is 648 on 64-bit Darwin; the slack is so a
    // future field cannot turn this into an overflow.
    var buffer: [1024]u8 align(8) = undefined;
    var length: usize = buffer.len;
    if (std.c.sysctl(&mib, mib.len, &buffer, &length, null, 0) != 0) return null;
    // A pid nobody owns comes back successful and empty.
    if (length < 44) return null;

    const endian = builtin.cpu.arch.endian();
    if (std.mem.readInt(i32, buffer[40..44], endian) != @as(i32, @intCast(pid))) return null;
    return std.mem.readInt(i64, buffer[0..8], endian);
}

/// Field 22 of `/proc/<pid>/stat`, the start time in clock ticks since boot.
/// Everything before it is skipped from the last `)` rather than the first,
/// because `comm` is unquoted and may contain both spaces and parentheses.
fn linuxStartTime(io: Io, pid: Pid) ?i64 {
    var path_buffer: [64]u8 = undefined;
    const stat_path = std.fmt.bufPrint(&path_buffer, "/proc/{d}/stat", .{pid}) catch return null;

    var buffer: [4096]u8 = undefined;
    const text = Io.Dir.cwd().readFile(io, stat_path, &buffer) catch return null;
    const close = std.mem.findScalarLast(u8, text, ')') orelse return null;

    // The first token after `)` is field 3, so field 22 is index 19.
    var tokens = std.mem.tokenizeAny(u8, text[close + 1 ..], " \n");
    var index: usize = 0;
    while (tokens.next()) |token| : (index += 1) {
        if (index == 19) return std.fmt.parseInt(i64, token, 10) catch null;
    }
    return null;
}

fn windowsStartTime(pid: Pid) ?i64 {
    const win = std.os.windows;
    const handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, .FALSE, pid) orelse return null;
    defer win.CloseHandle(handle);

    var creation: win.FILETIME = undefined;
    var exited: win.FILETIME = undefined;
    var kernel: win.FILETIME = undefined;
    var user_time: win.FILETIME = undefined;
    if (!GetProcessTimes(handle, &creation, &exited, &kernel, &user_time).toBool()) return null;
    // A handle can outlive the process it names. An exit time means the pid is
    // already free to be reused, so it is not "running" for our purposes.
    if (exited.dwLowDateTime != 0 or exited.dwHighDateTime != 0) return null;
    return @bitCast(fileTimeToU64(creation));
}

/// End a process that is *not* our child — no `wait` is possible, so death is
/// observed by the pid disappearing. Politely first; the daemon has listing
/// state to flush.
fn terminate(io: Io, pid: Pid) void {
    if (builtin.os.tag == .windows) {
        const win = std.os.windows;
        const handle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, .FALSE, pid) orelse return;
        defer win.CloseHandle(handle);
        _ = TerminateProcess(handle, 1);
        _ = WaitForSingleObject(handle, reap_grace_ms);
        return;
    }

    std.posix.kill(pid, .TERM) catch return;
    if (waitForExit(io, pid, reap_grace_ms)) return;
    std.posix.kill(pid, .KILL) catch return;
    _ = waitForExit(io, pid, reap_grace_ms);
}

fn waitForExit(io: Io, pid: Pid, timeout_ms: u32) bool {
    const budget: Io.Clock.Duration = .{
        .raw = .fromMilliseconds(timeout_ms),
        .clock = .awake,
    };
    const expiry: Io.Clock.Timestamp = .fromNow(io, budget);
    while (true) {
        if (processStartTime(io, pid) == null) return true;
        const now = Io.Clock.awake.now(io);
        if (now.nanoseconds >= expiry.raw.nanoseconds) return false;
        sleepMs(io, 50);
    }
}

/// True when something on `endpoint` answers `core/version` with a version
/// string, using exactly those credentials. Any failure — refused connection,
/// 401, a different program that happens to serve HTTP there — is false.
fn answersCoreVersion(gpa: Allocator, io: Io, endpoint: rc.Endpoint) bool {
    var client = rc.Client.init(gpa, io, endpoint) catch return false;
    defer client.deinit();
    // Short: a daemon that is up answers immediately, and one that is not
    // refuses the connection immediately. The budget is for a stranger on the
    // port that accepts and then says nothing.
    client.deadline = .{ .connect_ms = 1_000, .read_ms = 2_000 };

    var reply = client.call("core/version", .null) catch return false;
    defer reply.deinit();

    const object = switch (reply.value) {
        .object => |o| o,
        else => return false,
    };
    const version = object.get("version") orelse return false;
    return version == .string and version.string.len != 0;
}

fn writeRecord(gpa: Allocator, io: Io, record_path: []const u8, record: Record) !void {
    // Hand-rolled rather than serialised: the nonce is hex and the rest are
    // integers, so there is nothing to escape, and the literal shape here is
    // the format the next launch has to be able to read.
    const text = try std.fmt.allocPrint(
        gpa,
        "{{\"pid\":{d},\"process_start_time\":{d}," ++
            "\"nonce\":\"{s}\",\"port\":{d}}}\n",
        .{ record.pid, record.process_start_time, record.nonce, record.port },
    );
    defer gpa.free(text);
    try Io.Dir.cwd().writeFile(io, .{ .sub_path = record_path, .data = text });
}

/// The parent environment plus `RCLONE_CONFIG`. Inheriting matters — rclone
/// reads `HOME`, `TMPDIR`, and the proxy variables — but the config path must
/// not be inherited, or a player with `RCLONE_CONFIG` already set would have
/// the game writing remotes into their own file.
fn childEnviron(gpa: Allocator, config_path: []const u8) Allocator.Error!std.process.Environ.Map {
    var map = try parentEnviron(gpa);
    errdefer map.deinit();
    try map.put("RCLONE_CONFIG", config_path);
    return map;
}

fn parentEnviron(gpa: Allocator) Allocator.Error!std.process.Environ.Map {
    if (builtin.os.tag == .windows) {
        const environ: std.process.Environ = .{ .block = .global };
        return environ.createMap(gpa) catch |err| switch (err) {
            error.OutOfMemory => error.OutOfMemory,
            else => std.process.Environ.Map.init(gpa),
        };
    }
    // This module is compiled into a library the game loads, so it never sees
    // the environment `main` was handed; libc's copy is the only one there is.
    var map: std.process.Environ.Map = .init(gpa);
    errdefer map.deinit();
    var index: usize = 0;
    while (std.c.environ[index]) |entry| : (index += 1) {
        const pair = std.mem.span(entry);
        const split = std.mem.findScalar(u8, pair, '=') orelse continue;
        try map.put(pair[0..split], pair[split + 1 ..]);
    }
    return map;
}

fn sleepMs(io: Io, ms: u32) void {
    const duration: Io.Clock.Duration = .{
        .raw = .fromMilliseconds(ms),
        .clock = .awake,
    };
    duration.sleep(io) catch {};
}

// -- Windows process control -------------------------------------------------
//
// A job object with `KILL_ON_JOB_CLOSE` is the only mechanism on any of these
// platforms that reaps the child when the *parent* dies without running its
// own cleanup, which is exactly the crash case the record file exists to
// paper over. It is preferred there and the identity check stays as the
// fallback, because a job dies with the process that created it: a game that
// crashes and is restarted by the player still needs the record to recognise
// a daemon that outlived a job whose handle was inherited elsewhere.
//
// None of this can run on macOS. It is compile-verified only.

const PROCESS_TERMINATE: std.os.windows.DWORD = 0x0001;
const PROCESS_QUERY_LIMITED_INFORMATION: std.os.windows.DWORD = 0x1000;
const SYNCHRONIZE: std.os.windows.DWORD = 0x0010_0000;
const JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE: std.os.windows.DWORD = 0x2000;
const JobObjectExtendedLimitInformation: c_int = 9;

const JOBOBJECT_BASIC_LIMIT_INFORMATION = extern struct {
    PerProcessUserTimeLimit: std.os.windows.LARGE_INTEGER,
    PerJobUserTimeLimit: std.os.windows.LARGE_INTEGER,
    LimitFlags: std.os.windows.DWORD,
    MinimumWorkingSetSize: std.os.windows.SIZE_T,
    MaximumWorkingSetSize: std.os.windows.SIZE_T,
    ActiveProcessLimit: std.os.windows.DWORD,
    Affinity: std.os.windows.ULONG_PTR,
    PriorityClass: std.os.windows.DWORD,
    SchedulingClass: std.os.windows.DWORD,
};

const IO_COUNTERS = extern struct {
    ReadOperationCount: std.os.windows.ULONGLONG,
    WriteOperationCount: std.os.windows.ULONGLONG,
    OtherOperationCount: std.os.windows.ULONGLONG,
    ReadTransferCount: std.os.windows.ULONGLONG,
    WriteTransferCount: std.os.windows.ULONGLONG,
    OtherTransferCount: std.os.windows.ULONGLONG,
};

const JOBOBJECT_EXTENDED_LIMIT_INFORMATION = extern struct {
    BasicLimitInformation: JOBOBJECT_BASIC_LIMIT_INFORMATION,
    IoInfo: IO_COUNTERS,
    ProcessMemoryLimit: std.os.windows.SIZE_T,
    JobMemoryLimit: std.os.windows.SIZE_T,
    PeakProcessMemoryUsed: std.os.windows.SIZE_T,
    PeakJobMemoryUsed: std.os.windows.SIZE_T,
};

extern "kernel32" fn OpenProcess(
    dwDesiredAccess: std.os.windows.DWORD,
    bInheritHandle: std.os.windows.BOOL,
    dwProcessId: std.os.windows.DWORD,
) callconv(.winapi) ?std.os.windows.HANDLE;

extern "kernel32" fn GetProcessTimes(
    hProcess: std.os.windows.HANDLE,
    lpCreationTime: *std.os.windows.FILETIME,
    lpExitTime: *std.os.windows.FILETIME,
    lpKernelTime: *std.os.windows.FILETIME,
    lpUserTime: *std.os.windows.FILETIME,
) callconv(.winapi) std.os.windows.BOOL;

extern "kernel32" fn TerminateProcess(
    hProcess: std.os.windows.HANDLE,
    uExitCode: std.os.windows.UINT,
) callconv(.winapi) std.os.windows.BOOL;

extern "kernel32" fn WaitForSingleObject(
    hHandle: std.os.windows.HANDLE,
    dwMilliseconds: std.os.windows.DWORD,
) callconv(.winapi) std.os.windows.DWORD;

extern "kernel32" fn GetProcessId(
    hProcess: std.os.windows.HANDLE,
) callconv(.winapi) std.os.windows.DWORD;

extern "kernel32" fn GetCurrentProcessId() callconv(.winapi) std.os.windows.DWORD;

extern "kernel32" fn ResumeThread(
    hThread: std.os.windows.HANDLE,
) callconv(.winapi) std.os.windows.DWORD;

extern "kernel32" fn CreateJobObjectW(
    lpJobAttributes: ?*anyopaque,
    lpName: ?std.os.windows.LPCWSTR,
) callconv(.winapi) ?std.os.windows.HANDLE;

extern "kernel32" fn SetInformationJobObject(
    hJob: std.os.windows.HANDLE,
    JobObjectInformationClass: c_int,
    lpJobObjectInformation: *anyopaque,
    cbJobObjectInformationLength: std.os.windows.DWORD,
) callconv(.winapi) std.os.windows.BOOL;

extern "kernel32" fn AssignProcessToJobObject(
    hJob: std.os.windows.HANDLE,
    hProcess: std.os.windows.HANDLE,
) callconv(.winapi) std.os.windows.BOOL;

fn windowsProcessId(handle: std.os.windows.HANDLE) Pid {
    return GetProcessId(handle);
}

fn windowsCurrentProcessId() Pid {
    return GetCurrentProcessId();
}

fn fileTimeToU64(value: std.os.windows.FILETIME) u64 {
    return (@as(u64, value.dwHighDateTime) << 32) | @as(u64, value.dwLowDateTime);
}

fn resumeThread(handle: std.os.windows.HANDLE) void {
    _ = ResumeThread(handle);
}

/// Best effort: a job the child cannot leave, destroyed with the game. A null
/// return means the child is only covered by the identity check, which is why
/// that check is not Windows-optional.
fn attachToKillOnCloseJob(process_handle: std.os.windows.HANDLE) ?std.os.windows.HANDLE {
    const job = CreateJobObjectW(null, null) orelse return null;
    var limits: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = std.mem.zeroes(
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION,
    );
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(
        job,
        JobObjectExtendedLimitInformation,
        &limits,
        @sizeOf(JOBOBJECT_EXTENDED_LIMIT_INFORMATION),
    ).toBool()) {
        std.os.windows.CloseHandle(job);
        return null;
    }
    if (!AssignProcessToJobObject(job, process_handle).toBool()) {
        std.os.windows.CloseHandle(job);
        return null;
    }
    return job;
}
