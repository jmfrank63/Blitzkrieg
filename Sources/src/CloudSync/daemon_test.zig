//! Offline tests for rclone discovery and the version gate.
//!
//! No test here needs a real rclone, and none may be influenced by one: a
//! machine with rclone in `/usr/local/bin` and a machine without it must
//! produce identical results. Every case therefore builds its own search
//! space in a temp directory — a fake `PATH`, a fake game directory — and
//! hands it to the `*In` entry points, whose whole reason to exist is that
//! injection. The binaries in it are `/bin/sh` scripts that print a chosen
//! version banner, so a case can pick the version it wants tested.
//!
//! POSIX only for the script-backed cases: a shell script is not an
//! executable image on Windows. They return early there rather than skipping,
//! because the Zig 0.16 build runner fails a test step whose binary writes
//! anything at all to stderr, and a skip is written to stderr.

const std = @import("std");
const builtin = @import("builtin");
const daemon = @import("daemon.zig");
const rc = @import("rc.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;

/// A temp directory plus the helpers to populate it with fake install
/// locations. `root` is absolute, because `PATH` entries and an explicit
/// override are absolute in the field.
const Fixture = struct {
    tmp: std.testing.TmpDir,
    gpa: std.mem.Allocator,
    root: []u8,

    fn init(gpa: std.mem.Allocator) !Fixture {
        var tmp = std.testing.tmpDir(.{});
        errdefer tmp.cleanup();
        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const len = try tmp.dir.realPath(io, &buffer);
        return .{
            .tmp = tmp,
            .gpa = gpa,
            .root = try gpa.dupe(u8, buffer[0..len]),
        };
    }

    fn deinit(self: *Fixture) void {
        self.gpa.free(self.root);
        self.tmp.cleanup();
        self.* = undefined;
    }

    /// An absolute path inside the fixture. Nothing is created.
    fn join(self: *Fixture, parts: []const []const u8) ![]u8 {
        var all: std.ArrayList([]const u8) = .empty;
        defer all.deinit(self.gpa);
        try all.append(self.gpa, self.root);
        try all.appendSlice(self.gpa, parts);
        return path.join(self.gpa, all.items);
    }

    /// Create `<root>/<dir>` and return its absolute path.
    fn makeDir(self: *Fixture, dir: []const u8) ![]u8 {
        try self.tmp.dir.createDirPath(io, dir);
        return self.join(&.{dir});
    }

    /// Write an executable stub at `<root>/<dir>/<name>` that answers
    /// `rclone version` with `banner`, and return its absolute path.
    fn stub(self: *Fixture, dir: []const u8, name: []const u8, banner: []const u8) ![]u8 {
        return self.stubWithPermissions(dir, name, banner, .executable_file);
    }

    /// An executable stub whose body is written verbatim, for the cases that
    /// need it to do something other than print a version.
    fn script(self: *Fixture, dir: []const u8, name: []const u8, body: []const u8) ![]u8 {
        try self.tmp.dir.createDirPath(io, dir);
        const relative = try path.join(self.gpa, &.{ dir, name });
        defer self.gpa.free(relative);

        var file = try self.tmp.dir.createFile(io, relative, .{ .permissions = .executable_file });
        defer file.close(io);
        var buffer: [1024]u8 = undefined;
        var writer = file.writer(io, &buffer);
        try writer.interface.writeAll(body);
        try writer.interface.flush();

        return self.join(&.{ dir, name });
    }

    fn stubWithPermissions(
        self: *Fixture,
        dir: []const u8,
        name: []const u8,
        banner: []const u8,
        permissions: std.Io.File.Permissions,
    ) ![]u8 {
        try self.tmp.dir.createDirPath(io, dir);
        const relative = try path.join(self.gpa, &.{ dir, name });
        defer self.gpa.free(relative);

        var file = try self.tmp.dir.createFile(io, relative, .{ .permissions = permissions });
        defer file.close(io);

        var buffer: [512]u8 = undefined;
        var writer = file.writer(io, &buffer);
        try writer.interface.print(
            \\#!/bin/sh
            \\echo "{s}"
            \\echo "- os/version: darwin 26.5.2 (64 bit)"
            \\
        , .{banner});
        try writer.interface.flush();

        return self.join(&.{ dir, name });
    }
};

/// `PATH` as the operating system spells it: entries joined by `:` on POSIX
/// and `;` on Windows.
fn pathList(gpa: std.mem.Allocator, entries: []const []const u8) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(gpa);
    for (entries, 0..) |entry, index| {
        if (index != 0) try out.append(gpa, path.delimiter);
        try out.appendSlice(gpa, entry);
    }
    return out.toOwnedSlice(gpa);
}

test "a PATH entry is discovered when nothing more specific exists" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const expected = try fixture.stub("path-bin", daemon.exe_name, "rclone v1.75.0");
    defer gpa.free(expected);
    const bin_dir = try fixture.join(&.{"path-bin"});
    defer gpa.free(bin_dir);
    const path_env = try pathList(gpa, &.{ "/nonexistent-a", bin_dir, "/nonexistent-b" });
    defer gpa.free(path_env);

    const found = (try daemon.discoverIn(gpa, io, .{
        .game_dir = "",
        .path_env = path_env,
    })).?;
    defer gpa.free(found);

    try std.testing.expectEqualStrings(expected, found);
}

test "explicit_path_wins_over_path_entry" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // The override is deliberately the *older* of the two, so the assertion on
    // the probed version cannot pass by accident if the PATH copy were used.
    const explicit = try fixture.stub("elsewhere", daemon.exe_name, "rclone v1.66.0");
    defer gpa.free(explicit);
    const shadowed = try fixture.stub("path-bin", daemon.exe_name, "rclone v1.75.0");
    defer gpa.free(shadowed);
    const bin_dir = try fixture.join(&.{"path-bin"});
    defer gpa.free(bin_dir);
    const path_env = try pathList(gpa, &.{bin_dir});
    defer gpa.free(path_env);

    const found = (try daemon.discoverIn(gpa, io, .{
        .explicit = explicit,
        .game_dir = "",
        .path_env = path_env,
    })).?;
    defer gpa.free(found);
    try std.testing.expectEqualStrings(explicit, found);

    var availability = try daemon.resolveIn(gpa, io, .{
        .explicit = explicit,
        .game_dir = "",
        .path_env = path_env,
    });
    defer availability.deinit(gpa);

    try std.testing.expect(availability == .ready);
    try std.testing.expectEqualStrings(explicit, availability.ready.path);
    try std.testing.expectEqual(
        daemon.Version{ .major = 1, .minor = 66, .patch = 0 },
        availability.ready.version,
    );
}

test "the game directory shadows PATH" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const beside_game = try fixture.stub("game", daemon.exe_name, "rclone v1.70.0");
    defer gpa.free(beside_game);
    const shadowed = try fixture.stub("path-bin", daemon.exe_name, "rclone v1.75.0");
    defer gpa.free(shadowed);
    const game_dir = try fixture.join(&.{"game"});
    defer gpa.free(game_dir);
    const bin_dir = try fixture.join(&.{"path-bin"});
    defer gpa.free(bin_dir);
    const path_env = try pathList(gpa, &.{bin_dir});
    defer gpa.free(path_env);

    const found = (try daemon.discoverIn(gpa, io, .{
        .game_dir = game_dir,
        .path_env = path_env,
    })).?;
    defer gpa.free(found);

    try std.testing.expectEqualStrings(beside_game, found);
}

test "an absent rclone is null, not an error" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const empty_dir = try fixture.makeDir("empty");
    defer gpa.free(empty_dir);
    const path_env = try pathList(gpa, &.{ empty_dir, "/nonexistent" });
    defer gpa.free(path_env);

    const found = try daemon.discoverIn(gpa, io, .{
        .game_dir = empty_dir,
        .path_env = path_env,
    });
    try std.testing.expect(found == null);

    var availability = try daemon.resolveIn(gpa, io, .{
        .game_dir = empty_dir,
        .path_env = path_env,
    });
    defer availability.deinit(gpa);
    try std.testing.expect(availability == .unavailable);
    try std.testing.expectEqual(daemon.Reason.not_found, availability.unavailable.reason);
    try std.testing.expect(availability.unavailable.path == null);
}

test "an explicit path that leads nowhere never falls back to PATH" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const shadowed = try fixture.stub("path-bin", daemon.exe_name, "rclone v1.75.0");
    defer gpa.free(shadowed);
    const bin_dir = try fixture.join(&.{"path-bin"});
    defer gpa.free(bin_dir);
    const path_env = try pathList(gpa, &.{bin_dir});
    defer gpa.free(path_env);
    const missing = try fixture.join(&.{ "typo", daemon.exe_name });
    defer gpa.free(missing);

    // Silently using the PATH copy would report a working cloud sync that the
    // player's override says nothing about, and hide the typo forever.
    const found = try daemon.discoverIn(gpa, io, .{
        .explicit = missing,
        .game_dir = "",
        .path_env = path_env,
    });
    try std.testing.expect(found == null);

    var availability = try daemon.resolveIn(gpa, io, .{
        .explicit = missing,
        .game_dir = "",
        .path_env = path_env,
    });
    defer availability.deinit(gpa);
    try std.testing.expect(availability == .unavailable);
    try std.testing.expectEqual(daemon.Reason.not_found, availability.unavailable.reason);
}

test "probeVersion reads the leading version out of the version banner" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const binary = try fixture.stub("bin", daemon.exe_name, "rclone v1.75.0");
    defer gpa.free(binary);

    const version = try daemon.probeVersionIn(gpa, io, binary);
    try std.testing.expectEqual(
        daemon.Version{ .major = 1, .minor = 75, .patch = 0 },
        version,
    );
}

test "a version below the minimum is rejected as too_old" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // 1.65 predates resyncMode, which every resync in this plan depends on.
    const binary = try fixture.stub("bin", daemon.exe_name, "rclone v1.65.2");
    defer gpa.free(binary);
    const bin_dir = try fixture.join(&.{"bin"});
    defer gpa.free(bin_dir);
    const path_env = try pathList(gpa, &.{bin_dir});
    defer gpa.free(path_env);

    var availability = try daemon.resolveIn(gpa, io, .{
        .game_dir = "",
        .path_env = path_env,
    });
    defer availability.deinit(gpa);

    try std.testing.expect(availability == .unavailable);
    try std.testing.expectEqual(daemon.Reason.too_old, availability.unavailable.reason);
    // The UI has to name the version it rejected, not just complain.
    try std.testing.expectEqual(
        daemon.Version{ .major = 1, .minor = 65, .patch = 2 },
        availability.unavailable.version.?,
    );
    try std.testing.expectEqualStrings(binary, availability.unavailable.path.?);
}

test "a file that cannot be run is rejected as not_executable" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const not_executable = try fixture.stubWithPermissions(
        "bin",
        daemon.exe_name,
        "rclone v1.75.0",
        .default_file,
    );
    defer gpa.free(not_executable);

    var availability = try daemon.resolveIn(gpa, io, .{
        .explicit = not_executable,
        .game_dir = "",
        .path_env = "",
    });
    defer availability.deinit(gpa);

    // The distinction matters: a chmod fixes this one, a download fixes
    // not_found, and neither fixes too_old.
    try std.testing.expect(availability == .unavailable);
    try std.testing.expectEqual(daemon.Reason.not_executable, availability.unavailable.reason);
    try std.testing.expectEqualStrings(not_executable, availability.unavailable.path.?);
}

test "a new enough binary resolves to ready with its path and version" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const binary = try fixture.stub("bin", daemon.exe_name, "rclone v1.66.0");
    defer gpa.free(binary);
    const bin_dir = try fixture.join(&.{"bin"});
    defer gpa.free(bin_dir);
    const path_env = try pathList(gpa, &.{bin_dir});
    defer gpa.free(path_env);

    var availability = try daemon.resolveIn(gpa, io, .{
        .game_dir = "",
        .path_env = path_env,
    });
    defer availability.deinit(gpa);

    try std.testing.expect(availability == .ready);
    try std.testing.expect(availability.reason() == null);
    try std.testing.expectEqualStrings(binary, availability.ready.path);
    // The minimum itself is accepted; the gate is "at least", not "newer than".
    try std.testing.expectEqual(daemon.MIN_RCLONE, availability.ready.version);
}

test "the allocator-only entry points need no Io from the caller" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // An explicit path is the one arm of discovery whose result cannot depend
    // on the machine, which is what lets these three be tested at all.
    const binary = try fixture.stub("bin", daemon.exe_name, "rclone v1.75.0");
    defer gpa.free(binary);

    const discovered = try daemon.discover(gpa, binary);
    try std.testing.expect(discovered != null);
    const found = discovered.?;
    defer gpa.free(found);
    try std.testing.expectEqualStrings(binary, found);

    try std.testing.expectEqual(
        daemon.Version{ .major = 1, .minor = 75, .patch = 0 },
        try daemon.probeVersion(gpa, binary),
    );

    var availability = try daemon.resolve(gpa, binary);
    defer availability.deinit(gpa);
    try std.testing.expect(availability == .ready);
    try std.testing.expectEqualStrings(binary, availability.ready.path);
}

test "parseVersion accepts the shapes rclone actually prints" {
    const cases = [_]struct { text: []const u8, want: ?daemon.Version }{
        .{ .text = "rclone v1.75.0\n- os/version: darwin\n", .want = .{ .major = 1, .minor = 75, .patch = 0 } },
        // A build from source, and a beta, both carry a suffix after the patch.
        .{ .text = "rclone v1.66.0-beta.7519.deadbeef\n", .want = .{ .major = 1, .minor = 66, .patch = 0 } },
        .{ .text = "rclone v1.70.3-DEV\n", .want = .{ .major = 1, .minor = 70, .patch = 3 } },
        // Only the first line counts: later lines are full of other numbers.
        .{ .text = "rclone v1.66.0\n- go/version: go1.26.5\n", .want = .{ .major = 1, .minor = 66, .patch = 0 } },
        .{ .text = "rclone v2.0\n", .want = .{ .major = 2, .minor = 0, .patch = 0 } },
        .{ .text = "", .want = null },
        .{ .text = "not rclone at all\n", .want = null },
        // A shell error, or the wrong binary entirely, must not parse.
        .{ .text = "/bin/sh: rclone: command not found\n", .want = null },
        .{ .text = "rclone v\n", .want = null },
    };
    for (cases) |case| {
        try std.testing.expectEqual(case.want, daemon.parseVersion(case.text));
    }
}

test "atLeast compares major before minor before patch" {
    const v: daemon.Version = .{ .major = 1, .minor = 66, .patch = 0 };
    try std.testing.expect(v.atLeast(.{ .major = 1, .minor = 66, .patch = 0 }));
    try std.testing.expect(v.atLeast(.{ .major = 1, .minor = 65, .patch = 9 }));
    try std.testing.expect(v.atLeast(.{ .major = 0, .minor = 99, .patch = 0 }));
    try std.testing.expect(!v.atLeast(.{ .major = 1, .minor = 66, .patch = 1 }));
    try std.testing.expect(!v.atLeast(.{ .major = 1, .minor = 67, .patch = 0 }));
    try std.testing.expect(!v.atLeast(.{ .major = 2, .minor = 0, .patch = 0 }));
}

// -- daemon supervision ------------------------------------------------------
//
// The cases below are the other half of this file: the ones above never run
// rclone, these ones want to. A live daemon is the only way to prove that the
// argument vector is right, that `core/version` answers over the loopback
// socket, and that nothing survives `shutdown` — a stub cannot fake any of
// those. So they look for a real binary, `BK_TEST_RCLONE` first and ordinary
// discovery second, and return early when there is none. A machine without
// rclone still passes the suite; it just proves less.
//
// The reaping cases are the opposite: they must *not* have a daemon, because
// the thing being proved is that a record which cannot be authenticated is
// left alone. They forge a record naming this very test process, so a
// supervisor that killed on a pid match alone would take the test runner down
// with it.

/// Overrides discovery for the live cases. Set it to a known-good rclone to
/// exercise them on a machine that has no installed copy.
const rclone_env = "BK_TEST_RCLONE";

fn envVar(gpa: std.mem.Allocator, name: []const u8) ?[]u8 {
    if (builtin.os.tag == .windows) {
        const environ: std.process.Environ = .{ .block = .global };
        return environ.getAlloc(gpa, name) catch null;
    }
    var name_z: [128]u8 = undefined;
    if (name.len >= name_z.len) return null;
    @memcpy(name_z[0..name.len], name);
    name_z[name.len] = 0;
    const raw = std.c.getenv(name_z[0..name.len :0]) orelse return null;
    return gpa.dupe(u8, std.mem.span(raw)) catch null;
}

/// A real rclone new enough to drive, or null when this machine has none.
/// Caller owns the result.
fn liveRclone(gpa: std.mem.Allocator, target_io: std.Io) ?[]u8 {
    if (envVar(gpa, rclone_env)) |override| {
        defer gpa.free(override);
        if (override.len != 0) {
            var forced = daemon.resolveIn(gpa, target_io, .{
                .explicit = override,
                .game_dir = "",
                .path_env = "",
            }) catch return null;
            defer forced.deinit(gpa);
            if (forced == .ready) return gpa.dupe(u8, forced.ready.path) catch null;
            return null;
        }
    }
    var found = daemon.resolveIn(gpa, target_io, .{}) catch return null;
    defer found.deinit(gpa);
    if (found == .ready) return gpa.dupe(u8, found.ready.path) catch null;
    return null;
}

/// Write `<game_dir>/cloudsync/daemon.json` by hand. The literal JSON is the
/// point: it pins the on-disk shape the supervisor has to keep reading across
/// releases, and it is how a forged record gets made.
fn forgeRecord(
    gpa: std.mem.Allocator,
    game_dir: []const u8,
    pid: i64,
    start_time: i64,
    port: u16,
) !void {
    const dir = try path.join(gpa, &.{ game_dir, daemon.state_dir_name });
    defer gpa.free(dir);
    try std.Io.Dir.cwd().createDirPath(io, dir);

    const file = try path.join(gpa, &.{ dir, daemon.record_file_name });
    defer gpa.free(file);

    const text = try std.fmt.allocPrint(
        gpa,
        "{{\"pid\":{d},\"process_start_time\":{d}," ++
            "\"nonce\":\"{s}\",\"port\":{d}}}\n",
        .{ pid, start_time, "f" ** daemon.nonce_len, port },
    );
    defer gpa.free(text);

    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = file, .data = text });
}

fn recordExists(gpa: std.mem.Allocator, game_dir: []const u8) !bool {
    const file = try path.join(gpa, &.{ game_dir, daemon.state_dir_name, daemon.record_file_name });
    defer gpa.free(file);
    std.Io.Dir.cwd().access(io, file, .{}) catch return false;
    return true;
}

test "a spawned daemon answers core/version and leaves nothing running" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const target_io = threaded.io();

    const binary = liveRclone(gpa, target_io) orelse return;
    defer gpa.free(binary);

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    var d = try daemon.Daemon.spawn(gpa, target_io, .{
        .binary = binary,
        .game_dir = game_dir,
    });
    var shut_down = false;
    defer if (!shut_down) d.shutdown();

    // Nothing was there to reap, and the port and credentials are this
    // launch's, not a constant.
    try std.testing.expectEqual(daemon.ReapOutcome.none, d.reap.outcome);
    try std.testing.expect(d.port != 0);
    try std.testing.expectEqual(daemon.nonce_len, d.nonce.len);

    try d.waitReady(daemon.ready_timeout_ms);

    // The daemon is genuinely ours: it answers on the loopback port with the
    // nonce-derived credentials and nothing else.
    var client = try rc.Client.init(gpa, target_io, d.endpoint());
    defer client.deinit();
    var reply = try client.call("core/version", .null);
    defer reply.deinit();
    try std.testing.expect(reply.value == .object);
    const version = reply.value.object.get("version") orelse return error.MissingVersion;
    try std.testing.expect(version == .string);
    try std.testing.expect(version.string.len != 0);
    try std.testing.expect(version.string[0] == 'v');

    // A wrong password must not be enough, or "answers core/version" would
    // prove nothing about identity.
    var wrong = try rc.Client.init(gpa, target_io, .{
        .host = "127.0.0.1",
        .port = d.port,
        .user = "not-the-nonce",
        .pass = "not-the-nonce",
    });
    defer wrong.deinit();
    try std.testing.expectError(error.Unauthorized, wrong.call("core/version", .null));

    // The player's own rclone.conf is out of reach: the daemon itself reports
    // the config inside the game directory, which is the only proof that the
    // child's RCLONE_CONFIG took effect rather than merely being set.
    try std.testing.expect(std.mem.startsWith(u8, d.config_path, game_dir));
    try std.testing.expect(std.mem.startsWith(u8, d.log_path, game_dir));
    var paths = try client.call("config/paths", .null);
    defer paths.deinit();
    const config = paths.value.object.get("config") orelse return error.MissingConfigPath;
    try std.testing.expect(config == .string);
    try std.testing.expectEqualStrings(d.config_path, config.string);

    const pid = d.pid;
    try std.testing.expect(daemon.processStartTime(target_io, pid) != null);
    try std.testing.expect(try recordExists(gpa, game_dir));

    d.shutdown();
    shut_down = true;
    // Idempotent, and safe to call again from an error path.
    d.shutdown();

    try std.testing.expect(daemon.processStartTime(target_io, pid) == null);
    try std.testing.expect(!try recordExists(gpa, game_dir));
}

/// Plays init. In the field the stale daemon was orphaned by a crash and the
/// operating system reaps it; here it is still this process's child, so
/// without this it would linger as a zombie holding its pid and `reapStale`
/// would never see it go.
fn reapZombie(d: *daemon.Daemon, target_io: std.Io) void {
    _ = d.child.wait(target_io) catch {};
}

test "a confirmed leftover daemon from a previous launch is killed" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const target_io = threaded.io();

    const binary = liveRclone(gpa, target_io) orelse return;
    defer gpa.free(binary);

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    // A running, recorded daemon *is* what a crashed launch leaves behind, so
    // this is the leftover: nothing about the record says who wrote it.
    var d = try daemon.Daemon.spawn(gpa, target_io, .{
        .binary = binary,
        .game_dir = game_dir,
    });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    const pid = d.pid;

    const init_process = try std.Thread.spawn(.{}, reapZombie, .{ &d, target_io });
    const reap = try daemon.reapStale(gpa, target_io, game_dir);
    init_process.join();

    // All three checks agreed, so this one is ours and gets to die.
    try std.testing.expectEqual(daemon.ReapOutcome.reaped, reap.outcome);
    try std.testing.expectEqual(@as(?i64, pid), reap.pid);
    try std.testing.expect(daemon.processStartTime(target_io, pid) == null);
    try std.testing.expect(!try recordExists(gpa, game_dir));
}

test "a daemon that never answers times out with its log attached" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const target_io = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    // A binary that starts, complains into the log rclone was told to use,
    // and then never binds anything — a busy port, an unwritable config, a
    // build that is not rclone at all. The player has to be told which, and
    // the rc reply cannot say because there is no rc.
    const binary = try fixture.script("bin", daemon.exe_name,
        \\#!/bin/sh
        \\while [ $# -gt 0 ]; do
        \\  if [ "$1" = "--log-file" ]; then
        \\    echo "Failed to start remote control: listen tcp: address already in use" > "$2"
        \\  fi
        \\  shift
        \\done
        \\exec sleep 30
        \\
    );
    defer gpa.free(binary);

    var d = try daemon.Daemon.spawn(gpa, target_io, .{
        .binary = binary,
        .game_dir = game_dir,
    });
    defer d.shutdown();

    // A short budget: the point is the expiry path, not the fifteen seconds.
    try std.testing.expectError(error.DaemonTimeout, d.waitReady(1_000));
    try std.testing.expectEqual(daemon.Failure.daemon_timeout, d.failure.?);
    // The tail is the diagnostic: an rc reply cannot explain a daemon that
    // never came up far enough to serve rc.
    try std.testing.expect(std.mem.indexOf(u8, d.logTail(), "address already in use") != null);
}

test "a forged record with a mismatched start time is refused, not acted on" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    // This process, with a start time one second off. A supervisor that
    // trusted the pid alone would kill the test runner here.
    const self_pid = daemon.currentPid();
    const started = daemon.processStartTime(io, self_pid) orelse return;
    try forgeRecord(gpa, game_dir, self_pid, started + 1, 1);

    const reap = try daemon.reapStale(gpa, io, game_dir);
    try std.testing.expectEqual(daemon.ReapOutcome.refused_foreign_process, reap.outcome);
    try std.testing.expectEqual(@as(?i64, self_pid), reap.pid);

    // Still here.
    try std.testing.expect(daemon.processStartTime(io, self_pid) != null);
}

test "a live pid whose start time matches but cannot authenticate is refused" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    // Everything the OS can tell us agrees, and the record is still a lie:
    // this process is not an rclone daemon. The rc probe is the only thing
    // standing between it and a kill.
    const self_pid = daemon.currentPid();
    const started = daemon.processStartTime(io, self_pid) orelse return;
    const dead_port = try daemon.reservePort(io);
    try forgeRecord(gpa, game_dir, self_pid, started, dead_port);

    const reap = try daemon.reapStale(gpa, io, game_dir);
    try std.testing.expectEqual(daemon.ReapOutcome.refused_unauthenticated, reap.outcome);
    try std.testing.expect(daemon.processStartTime(io, self_pid) != null);
}

test "a record naming a pid that no longer exists is simply cleared" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    // Above PID_MAX everywhere this ships.
    try forgeRecord(gpa, game_dir, 4_000_000, 1, 5572);

    const reap = try daemon.reapStale(gpa, io, game_dir);
    try std.testing.expectEqual(daemon.ReapOutcome.already_gone, reap.outcome);
    try std.testing.expect(!try recordExists(gpa, game_dir));
}

test "a missing or unreadable record reaps nothing" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    const empty = try daemon.reapStale(gpa, io, game_dir);
    try std.testing.expectEqual(daemon.ReapOutcome.none, empty.outcome);

    const dir = try path.join(gpa, &.{ game_dir, daemon.state_dir_name });
    defer gpa.free(dir);
    try std.Io.Dir.cwd().createDirPath(io, dir);
    const file = try path.join(gpa, &.{ dir, daemon.record_file_name });
    defer gpa.free(file);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = file, .data = "{ not json" });

    const garbage = try daemon.reapStale(gpa, io, game_dir);
    try std.testing.expectEqual(daemon.ReapOutcome.none, garbage.outcome);
}

test "a record naming pid 0 or -1 is ignored rather than signalled" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    // POSIX `kill` reads 0 as "my whole process group" and -1 as "everything
    // I am allowed to signal". A corrupt or truncated record must not be a
    // way to reach either.
    for ([_]i64{ 0, -1 }) |pid| {
        try forgeRecord(gpa, game_dir, pid, 1, 5572);
        const reap = try daemon.reapStale(gpa, io, game_dir);
        try std.testing.expectEqual(daemon.ReapOutcome.none, reap.outcome);
    }
}

test "processStartTime is stable for a live process and null for a dead one" {
    const self_pid = daemon.currentPid();
    const first = daemon.processStartTime(io, self_pid) orelse return;
    const second = daemon.processStartTime(io, self_pid) orelse return error.StartTimeVanished;
    // A start time that moved would fail every identity check for no reason.
    try std.testing.expectEqual(first, second);
    try std.testing.expect(daemon.processStartTime(io, @intCast(4_000_000)) == null);
}

test "each launch reserves its own port and mints its own nonce" {
    const first_port = try daemon.reservePort(io);
    const second_port = try daemon.reservePort(io);
    try std.testing.expect(first_port != 0);
    try std.testing.expect(second_port != 0);
    // Not a hard guarantee of the kernel, but a fixed port would fail this
    // every time rather than occasionally.
    try std.testing.expect(first_port != second_port);

    const a = daemon.newNonce(io);
    const b = daemon.newNonce(io);
    try std.testing.expectEqual(daemon.nonce_len, a.len);
    try std.testing.expect(!std.mem.eql(u8, &a, &b));
    for (a) |c| try std.testing.expect(std.ascii.isHex(c));
    // The user and the password are different halves; one leaking must not
    // hand over the other.
    try std.testing.expect(!std.mem.eql(u8, a[0 .. daemon.nonce_len / 2], a[daemon.nonce_len / 2 ..]));
}
