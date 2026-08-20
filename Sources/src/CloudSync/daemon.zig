//! rclone binary discovery and version gate.
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
