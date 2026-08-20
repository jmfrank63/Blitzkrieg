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
