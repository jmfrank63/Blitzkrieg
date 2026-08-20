//! Tests for the pairing bootstrap.
//!
//! Two halves, like `daemon_test.zig`. The offline cases prove the refusal
//! logic and the state file without any daemon: refusals happen before the
//! first rc call by design, so a client pointed at a dead endpoint proves
//! exactly that. The live cases drive a real `rclone rcd` against an `alias`
//! remote backed by a fixture directory — which is what lets a test assert
//! the remote's contents with ordinary file reads — and return early when no
//! rclone is available, exactly as the daemon suite does.
//!
//! The "newer side" cases separate modification times with a real sleep
//! rather than by forging timestamps: resync's newer-wins comparison is the
//! thing under test, and a forged mtime tests the forgery.
//!
//! Skips are silent early returns: the Zig 0.16 build runner fails a test
//! step whose binary writes anything to stderr.

const std = @import("std");
const builtin = @import("builtin");
const engine = @import("engine.zig");
const daemon = @import("daemon.zig");
const plan = @import("plan.zig");
const rc = @import("rc.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;

/// Overrides discovery for the live cases, exactly as in `daemon_test.zig`.
const rclone_env = "BK_TEST_RCLONE";

const Fixture = struct {
    tmp: std.testing.TmpDir,
    gpa: std.mem.Allocator,
    root: []u8,

    fn init(gpa: std.mem.Allocator) !Fixture {
        var tmp = std.testing.tmpDir(.{});
        errdefer tmp.cleanup();

        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const len = try tmp.dir.realPath(io, &buffer);
        const root = try gpa.dupe(u8, buffer[0..len]);
        return .{ .tmp = tmp, .gpa = gpa, .root = root };
    }

    fn deinit(self: *Fixture) void {
        self.gpa.free(self.root);
        self.tmp.cleanup();
        self.* = undefined;
    }

    /// Create `name` under the fixture and return its absolute path.
    fn makeDir(self: *Fixture, name: []const u8) ![]u8 {
        try self.tmp.dir.createDirPath(io, name);
        return path.join(self.gpa, &.{ self.root, name });
    }

    /// Write a file at an absolute path, creating parent directories.
    fn write(self: *Fixture, absolute: []const u8, data: []const u8) !void {
        if (path.dirname(absolute)) |dir| try std.Io.Dir.cwd().createDirPath(io, dir);
        try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = absolute, .data = data });
        _ = self;
    }
};

fn readFile(gpa: std.mem.Allocator, parts: []const []const u8) ![]u8 {
    const file_path = try path.join(gpa, parts);
    defer gpa.free(file_path);
    return std.Io.Dir.cwd().readFileAlloc(io, file_path, gpa, .limited(65536));
}

fn expectFileContent(gpa: std.mem.Allocator, parts: []const []const u8, expected: []const u8) !void {
    const content = try readFile(gpa, parts);
    defer gpa.free(content);
    try std.testing.expectEqualStrings(expected, content);
}

// -- Offline: state and refusals ---------------------------------------------

test "pairing state round-trips through its json file" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    try std.testing.expect(engine.loadPairingState(gpa, io, game_dir, "hero") == null);

    try engine.savePairingState(gpa, io, game_dir, "hero", .{
        .paired = true,
        .last_success_unix = 1_755_000_000,
        .remote_fingerprint = "s3:eu-central-1/bk-saves",
    });

    var loaded = engine.loadPairingState(gpa, io, game_dir, "hero").?;
    defer loaded.deinit();
    try std.testing.expect(loaded.state().paired);
    try std.testing.expectEqual(@as(i64, 1_755_000_000), loaded.state().last_success_unix);
    try std.testing.expectEqualStrings("s3:eu-central-1/bk-saves", loaded.state().remote_fingerprint);

    // The record lives under the state root — outside Path1, where it would
    // sync to the other machine and misreport that machine's pairing.
    const state_path = try plan.pairingStatePath(gpa, game_dir, "hero");
    defer gpa.free(state_path);
    const stat = try std.Io.Dir.cwd().statFile(io, state_path, .{});
    try std.testing.expectEqual(std.Io.File.Kind.file, stat.kind);
}

test "corrupt pairing state reads as never paired" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    const state_path = try plan.pairingStatePath(gpa, game_dir, "hero");
    defer gpa.free(state_path);
    try fixture.write(state_path, "{not json");

    // "Never paired" is the safe reading: pairing is offered again, and
    // pairing never runs unattended.
    try std.testing.expect(engine.loadPairingState(gpa, io, game_dir, "hero") == null);
}

test "a paired profile refuses to pair again" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    try engine.savePairingState(gpa, io, game_dir, "hero", .{
        .paired = true,
        .last_success_unix = 1,
        .remote_fingerprint = "fp-original",
    });

    // A dead endpoint, deliberately: both refusals must happen before the
    // engine speaks to any daemon at all.
    var client = try rc.Client.init(gpa, io, .{
        .host = "127.0.0.1",
        .port = 1,
        .user = "x",
        .pass = "x",
    });
    defer client.deinit();
    var eng = engine.Engine.init(gpa, io, &client);
    defer eng.deinit();

    var ctx: engine.RunContext = .{
        .path1 = "irrelevant",
        .remote = "bkremote",
        .profile = "hero",
        .game_dir = game_dir,
        .profile_id = "hero-id",
        .remote_fingerprint = "fp-original",
    };
    // Same remote: already paired, and a resync after divergence overwrites
    // one side, so recovery must stay a player action.
    try std.testing.expectError(error.AlreadyPaired, eng.pair(ctx));

    // Different remote identity: a new pairing decision, not a resume.
    ctx.remote_fingerprint = "fp-elsewhere";
    try std.testing.expectError(error.FingerprintChanged, eng.pair(ctx));
    try std.testing.expectError(error.FingerprintChanged, eng.syncOnce(ctx));
}

test "syncOnce refuses an unpaired profile" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    var client = try rc.Client.init(gpa, io, .{
        .host = "127.0.0.1",
        .port = 1,
        .user = "x",
        .pass = "x",
    });
    defer client.deinit();
    var eng = engine.Engine.init(gpa, io, &client);
    defer eng.deinit();

    try std.testing.expectError(error.NotPaired, eng.syncOnce(.{
        .path1 = "irrelevant",
        .remote = "bkremote",
        .profile = "hero",
        .game_dir = game_dir,
        .profile_id = "hero-id",
        .remote_fingerprint = "fp",
    }));
}

// -- Live: pairing against a real daemon -------------------------------------

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

/// Define an `alias` remote named `name` whose root is `target` — a local
/// fixture directory, which is what lets the tests read "the remote" back
/// with plain file operations while still exercising a genuinely named
/// remote, the only Path2 shape the plan allows.
fn createAliasRemote(
    gpa: std.mem.Allocator,
    client: *rc.Client,
    name: []const u8,
    target: []const u8,
) !void {
    var parameters: std.json.ObjectMap = .empty;
    defer parameters.deinit(gpa);
    try parameters.put(gpa, "remote", .{ .string = target });

    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "name", .{ .string = name });
    try object.put(gpa, "type", .{ .string = "alias" });
    try object.put(gpa, "parameters", .{ .object = parameters });

    var reply = try client.call("config/create", .{ .object = object });
    reply.deinit();
}

fn sleepMs(target_io: std.Io, ms: u32) void {
    const duration: std.Io.Clock.Duration = .{
        .raw = .fromMilliseconds(ms),
        .clock = .awake,
    };
    duration.sleep(target_io) catch {};
}

/// Modification-time separation for the newer-wins cases. Two seconds: wide
/// enough for any filesystem's timestamp granularity, and the comparison
/// under test is rclone's, not ours.
const mtime_gap_ms: u32 = 2_000;

test "pairing an empty remote pairs once and the second run does not resync" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const binary = liveRclone(gpa, tio) orelse return;
    defer gpa.free(binary);

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    // The alias target exists; nothing inside it does. No profile directory,
    // no trash root — every player's actual first sync.
    const cloud = try fixture.makeDir("cloud");
    defer gpa.free(cloud);
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    const save = try path.join(gpa, &.{ profile_dir, "quick.sav" });
    defer gpa.free(save);
    try fixture.write(save, "v1");
    // In the sync set it would push a machine's display mode onto every
    // other machine; the filter file must keep it home.
    const config = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config);
    try fixture.write(config, "GFX.Mode = 4k");

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);

    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    try createAliasRemote(gpa, &client, "bkremote", cloud);

    var eng = engine.Engine.init(gpa, tio, &client);
    defer eng.deinit();

    const ctx: engine.RunContext = .{
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .game_dir = game_dir,
        .profile_id = "hero-id",
        .remote_fingerprint = "alias:cloud",
    };

    var outcome = eng.pair(ctx) catch |err| {
        // On failure the log is the only place bisync explains itself, and
        // an assertion is the only stderr-safe way to surface it.
        try std.testing.expectEqualStrings("", eng.lastErrorText());
        return err;
    };
    defer outcome.deinit(gpa);

    // This machine seeded the sentinel, and the resync delivered it up.
    try std.testing.expectEqual(plan.SentinelAction.written, outcome.sentinel);
    try expectFileContent(gpa, &.{ cloud, "profiles", "hero", "quick.sav" }, "v1");
    try expectFileContent(gpa, &.{ cloud, "profiles", "hero", plan.sentinel_name }, "hero-id\n");
    try expectFileContent(gpa, &.{ profile_dir, plan.sentinel_name }, "hero-id\n");

    // The filter held: the machine-local config never left.
    try std.testing.expectError(
        error.FileNotFound,
        readFile(gpa, &.{ cloud, "profiles", "hero", "config.cfg" }),
    );

    var loaded = engine.loadPairingState(gpa, io, game_dir, "hero").?;
    defer loaded.deinit();
    try std.testing.expect(loaded.state().paired);
    try std.testing.expectEqualStrings("alias:cloud", loaded.state().remote_fingerprint);

    // Pairing is once. The second call must refuse before any rc traffic;
    // the steady-state path is `syncOnce`, whose parameters carry no
    // `resync` key (pinned by `assertNoResyncWhenPaired` in the plan tests).
    try std.testing.expectError(error.AlreadyPaired, eng.pair(ctx));

    try fixture.write(save, "v2");
    const run_id = eng.syncOnce(ctx) catch |err| {
        try std.testing.expectEqualStrings("", eng.lastErrorText());
        return err;
    };
    defer gpa.free(run_id);
    try expectFileContent(gpa, &.{ cloud, "profiles", "hero", "quick.sav" }, "v2");
    try expectFileContent(gpa, &.{ profile_dir, "quick.sav" }, "v2");
}

test "pairing preserves the newer side in both directions and trashes the loser" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const binary = liveRclone(gpa, tio) orelse return;
    defer gpa.free(binary);

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const cloud = try fixture.makeDir("cloud");
    defer gpa.free(cloud);

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);

    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    try createAliasRemote(gpa, &client, "bkremote", cloud);

    var eng = engine.Engine.init(gpa, tio, &client);
    defer eng.deinit();

    // Direction one: the remote holds the newer copy. Without `resyncMode:
    // "newer"` this is the measured catastrophe — Path1 wins, and the newer
    // cloud save is destroyed with no conflict file and no trash entry.
    {
        const profile_dir = try fixture.makeDir("pa");
        defer gpa.free(profile_dir);
        const local_save = try path.join(gpa, &.{ profile_dir, "quick.sav" });
        defer gpa.free(local_save);
        try fixture.write(local_save, "local-old");

        sleepMs(tio, mtime_gap_ms);
        const remote_save = try path.join(gpa, &.{ cloud, "profiles", "pa", "quick.sav" });
        defer gpa.free(remote_save);
        try fixture.write(remote_save, "remote-new");

        var outcome = eng.pair(.{
            .path1 = profile_dir,
            .remote = "bkremote",
            .profile = "pa",
            .game_dir = game_dir,
            .profile_id = "pa-id",
            .remote_fingerprint = "alias:cloud",
        }) catch |err| {
            try std.testing.expectEqualStrings("", eng.lastErrorText());
            return err;
        };
        defer outcome.deinit(gpa);

        // The newer copy survives on both sides...
        try expectFileContent(gpa, &.{ profile_dir, "quick.sav" }, "remote-new");
        try expectFileContent(gpa, &.{ cloud, "profiles", "pa", "quick.sav" }, "remote-new");
        // ...and the loser is recoverable, not merely outvoted: the local
        // copy lost, so it is in this run's backupDir1 on Path1's side.
        try expectFileContent(
            gpa,
            &.{ profile_dir, plan.local_trash_dir_name, outcome.run_id, "quick.sav" },
            "local-old",
        );
    }

    // Direction two: the local copy is newer, the remote loses, and the
    // loser lands in backupDir2 on the remote's side — a different rclone
    // path from direction one, which is why both are exercised.
    {
        const remote_save = try path.join(gpa, &.{ cloud, "profiles", "pb", "quick.sav" });
        defer gpa.free(remote_save);
        try fixture.write(remote_save, "remote-old");

        sleepMs(tio, mtime_gap_ms);
        const profile_dir = try fixture.makeDir("pb");
        defer gpa.free(profile_dir);
        const local_save = try path.join(gpa, &.{ profile_dir, "quick.sav" });
        defer gpa.free(local_save);
        try fixture.write(local_save, "local-new");

        var outcome = eng.pair(.{
            .path1 = profile_dir,
            .remote = "bkremote",
            .profile = "pb",
            .game_dir = game_dir,
            .profile_id = "pb-id",
            .remote_fingerprint = "alias:cloud",
        }) catch |err| {
            try std.testing.expectEqualStrings("", eng.lastErrorText());
            return err;
        };
        defer outcome.deinit(gpa);

        try expectFileContent(gpa, &.{ profile_dir, "quick.sav" }, "local-new");
        try expectFileContent(gpa, &.{ cloud, "profiles", "pb", "quick.sav" }, "local-new");
        try expectFileContent(
            gpa,
            &.{ cloud, "trash", "pb", outcome.run_id, "quick.sav" },
            "remote-old",
        );
    }
}

test "sentinel_not_seeded_when_remote_already_paired_this_profile" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const binary = liveRclone(gpa, tio) orelse return;
    defer gpa.free(binary);

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const cloud = try fixture.makeDir("cloud");
    defer gpa.free(cloud);

    // The second machine's view: the cloud already carries this profile,
    // sentinel included, from some other machine's first pairing.
    const remote_sentinel = try path.join(gpa, &.{ cloud, "profiles", "pc", plan.sentinel_name });
    defer gpa.free(remote_sentinel);
    try fixture.write(remote_sentinel, "first-machine-id\n");
    const remote_save = try path.join(gpa, &.{ cloud, "profiles", "pc", "quick.sav" });
    defer gpa.free(remote_save);
    try fixture.write(remote_save, "from-cloud");

    const profile_dir = try fixture.makeDir("pc");
    defer gpa.free(profile_dir);

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);

    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    try createAliasRemote(gpa, &client, "bkremote", cloud);

    var eng = engine.Engine.init(gpa, tio, &client);
    defer eng.deinit();

    var outcome = eng.pair(.{
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "pc",
        .game_dir = game_dir,
        .profile_id = "second-machine-id",
        .remote_fingerprint = "alias:cloud",
    }) catch |err| {
        try std.testing.expectEqualStrings("", eng.lastErrorText());
        return err;
    };
    defer outcome.deinit(gpa);

    // Writing a second sentinel would give the two copies different
    // modification times, and bisync aborts the resync on `Modtime not equal
    // in listing`. Deferring lets the resync deliver the first machine's.
    try std.testing.expectEqual(plan.SentinelAction.deferred_to_remote, outcome.sentinel);
    try expectFileContent(gpa, &.{ profile_dir, plan.sentinel_name }, "first-machine-id\n");
    try expectFileContent(gpa, &.{ profile_dir, "quick.sav" }, "from-cloud");
}
