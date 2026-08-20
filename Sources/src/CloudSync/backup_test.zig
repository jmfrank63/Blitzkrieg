//! Tests for the config snapshot.
//!
//! Offline: the host sanitiser, which follows `NProfile::Sanitize`'s rules
//! exactly. Live (gated on `BK_TEST_RCLONE` like every live suite): one
//! snapshot lands at the per-host path outside the synced prefix, and a
//! worker-driven sync with the backup option on proves the whole chain —
//! snapshot after a clean finish, nothing pulled back down, and `config.cfg`
//! still absent from the sync set.

const std = @import("std");
const builtin = @import("builtin");
const backup = @import("backup.zig");
const daemon = @import("daemon.zig");
const engine = @import("engine.zig");
const plan = @import("plan.zig");
const rc = @import("rc.zig");
const worker = @import("worker.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;

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

    fn makeDir(self: *Fixture, name: []const u8) ![]u8 {
        try self.tmp.dir.createDirPath(io, name);
        return path.join(self.gpa, &.{ self.root, name });
    }

    fn write(self: *Fixture, absolute: []const u8, data: []const u8) !void {
        if (path.dirname(absolute)) |dir| try std.Io.Dir.cwd().createDirPath(io, dir);
        try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = absolute, .data = data });
        _ = self;
    }
};

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

fn sleepMs(target_io: std.Io, ms: u32) void {
    const duration: std.Io.Clock.Duration = .{
        .raw = .fromMilliseconds(ms),
        .clock = .awake,
    };
    duration.sleep(target_io) catch {};
}

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

/// The single `.cfg` file directly under `dir`, or null.
fn onlySnapshotIn(gpa: std.mem.Allocator, dir_path: []const u8) !?[]u8 {
    var dir = std.Io.Dir.cwd().openDir(io, dir_path, .{ .iterate = true }) catch return null;
    defer dir.close(io);
    var found: ?[]u8 = null;
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (!std.mem.endsWith(u8, entry.name, ".cfg")) continue;
        if (found != null) {
            gpa.free(found.?);
            return error.TestUnexpectedResult;
        }
        found = try gpa.dupe(u8, entry.name);
    }
    return found;
}

test "host names are sanitised by the profile rules" {
    const gpa = std.testing.allocator;

    const cases = [_][2][]const u8{
        .{ "Desktop-PC", "Desktop-PC" },
        .{ "bad/host:na*me?", "badhostname" },
        .{ "  padded  ", "padded" },
        .{ "dots...", "dots" },
        .{ "wide\xc3\xbcname", "widename" },
        .{ "", "host" },
        .{ "///", "host" },
    };
    for (cases) |case| {
        const got = try backup.sanitizeHost(gpa, case[0]);
        defer gpa.free(got);
        try std.testing.expectEqualStrings(case[1], got);
    }
}

test "a snapshot lands per host outside the synced prefix" {
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

    const config = try path.join(gpa, &.{ game_dir, "config.cfg" });
    defer gpa.free(config);
    try fixture.write(config, "GFX.Mode = 3840x2160\nGFX.Monitor = 1\n");

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    try createAliasRemote(gpa, &client, "bkremote", cloud);

    const name = try backup.snapshotConfig(gpa, tio, &client, .{
        .config_dir = game_dir,
        .remote = "bkremote",
        .profile = "hero",
        .host = "Desktop/PC:1",
    });
    defer gpa.free(name);
    try std.testing.expect(std.mem.endsWith(u8, name, ".cfg"));

    // At the per-host path, sanitised, content intact.
    const stored = try path.join(gpa, &.{ cloud, "config-backups", "hero", "DesktopPC1", name });
    defer gpa.free(stored);
    const content = try std.Io.Dir.cwd().readFileAlloc(io, stored, gpa, .limited(4096));
    defer gpa.free(content);
    try std.testing.expectEqualStrings("GFX.Mode = 3840x2160\nGFX.Monitor = 1\n", content);

    // A sibling of profiles/, never a child: the snapshot created no
    // profiles/ tree at all, and the backup root sits beside it.
    const profiles_dir = try path.join(gpa, &.{ cloud, "profiles" });
    defer gpa.free(profiles_dir);
    try std.testing.expectError(
        error.FileNotFound,
        std.Io.Dir.cwd().statFile(io, profiles_dir, .{}),
    );
}

test "backups list newest-first across hosts and prune per host" {
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

    // Two hosts, mixed ages, plus things a listing must ignore: a non-cfg
    // file and a nested directory no snapshot would create.
    const tree = [_][2][]const u8{
        .{ "config-backups/hero/HostA/20260810T100000Z-aaaaaaaa.cfg", "a-old" },
        .{ "config-backups/hero/HostA/20260820T100000Z-bbbbbbbb.cfg", "a-mid" },
        .{ "config-backups/hero/HostA/20260821T100000Z-cccccccc.cfg", "a-new" },
        .{ "config-backups/hero/HostB/20260601T100000Z-dddddddd.cfg", "b-only" },
        .{ "config-backups/hero/HostA/notes.txt", "not a snapshot" },
        .{ "config-backups/hero/HostA/deep/stray.cfg", "not ours either" },
    };
    for (tree) |leaf| {
        const at = try path.join(gpa, &.{ cloud, leaf[0] });
        defer gpa.free(at);
        try fixture.write(at, leaf[1]);
    }

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    try createAliasRemote(gpa, &client, "bkremote", cloud);

    var list = try backup.listBackups(gpa, &client, "bkremote", "hero");
    defer list.deinit();
    try std.testing.expectEqual(@as(usize, 4), list.entries.len);

    // Newest first, across hosts.
    try std.testing.expectEqualStrings("HostA/20260821T100000Z-cccccccc.cfg", list.entries[0].id);
    try std.testing.expectEqualStrings("HostA/20260820T100000Z-bbbbbbbb.cfg", list.entries[1].id);
    try std.testing.expectEqualStrings("HostA/20260810T100000Z-aaaaaaaa.cfg", list.entries[2].id);
    try std.testing.expectEqualStrings("HostB/20260601T100000Z-dddddddd.cfg", list.entries[3].id);

    try std.testing.expectEqualStrings("HostA", list.entries[0].host);
    try std.testing.expectEqual(engine.runIdTimestamp("20260821T100000Z-cccccccc").?, list.entries[0].timestamp);
    try std.testing.expectEqual(@as(u64, 5), list.entries[0].size);
    try std.testing.expect(std.mem.startsWith(
        u8,
        list.entries[0].remote_path,
        "bkremote:config-backups/hero/HostA/",
    ));

    // Retention: per host, never globally. HostA keeps its newest and loses
    // two; HostB's only — and therefore newest — entry survives even though
    // it is the oldest file in the whole tree.
    const removed = try backup.pruneBackups(gpa, &client, "bkremote", "hero", 1);
    try std.testing.expectEqual(@as(u32, 2), removed);

    for ([_][]const u8{
        "config-backups/hero/HostA/20260821T100000Z-cccccccc.cfg",
        "config-backups/hero/HostB/20260601T100000Z-dddddddd.cfg",
        "config-backups/hero/HostA/notes.txt",
    }) |kept| {
        const at = try path.join(gpa, &.{ cloud, kept });
        defer gpa.free(at);
        _ = try std.Io.Dir.cwd().statFile(io, at, .{});
    }
    for ([_][]const u8{
        "config-backups/hero/HostA/20260820T100000Z-bbbbbbbb.cfg",
        "config-backups/hero/HostA/20260810T100000Z-aaaaaaaa.cfg",
    }) |gone| {
        const at = try path.join(gpa, &.{ cloud, gone });
        defer gpa.free(at);
        try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, at, .{}));
    }

    // keep_per_host of zero still keeps the newest: the retention setting
    // can bound history, never erase it.
    const zero_removed = try backup.pruneBackups(gpa, &client, "bkremote", "hero", 0);
    try std.testing.expectEqual(@as(u32, 0), zero_removed);

    // A profile with no backups lists as empty rather than failing.
    var none = try backup.listBackups(gpa, &client, "bkremote", "nobody");
    defer none.deinit();
    try std.testing.expectEqual(@as(usize, 0), none.entries.len);
}

// -- The staged restore --------------------------------------------------------
//
// Everything below except the live staging case is pure filesystem work and
// runs on every machine: the apply step is deliberately local-only, because
// a restore already downloaded has to finish with rclone absent and cloud
// sync disabled.

const local_xml =
    \\<base><Options><Vars>
    \\  <item Order="1"><Var>1024x768x32</Var><KeyName>GFX.Mode</KeyName></item>
    \\  <item Order="2"><Var>1</Var><KeyName>GFX.Monitor.Index</KeyName></item>
    \\  <item Order="3"><Var>OFF</Var><KeyName>GFX.FullScreen</KeyName></item>
    \\  <item Order="4"><Var>30</Var><KeyName>Sound.Volume</KeyName></item>
    \\</Vars></Options></base>
;

const restored_xml =
    \\<base><Options><Vars>
    \\  <item Order="1"><Var>3840x2160x32</Var><KeyName>GFX.Mode</KeyName></item>
    \\  <item Order="2"><Var>2</Var><KeyName>GFX.Monitor.Index</KeyName></item>
    \\  <item Order="3"><Var>ON</Var><KeyName>GFX.FullScreen</KeyName></item>
    \\  <item Order="4"><Var>80</Var><KeyName>Sound.Volume</KeyName></item>
    \\  <item Order="5"><Var>ON</Var><KeyName>Announcer</KeyName></item>
    \\</Vars></Options></base>
;

test "the merge preserves local GFX and adopts every other key" {
    const gpa = std.testing.allocator;

    const merged = try backup.mergeConfig(gpa, local_xml, restored_xml);
    defer gpa.free(merged);

    // The desktop's monitor layout stays home...
    try std.testing.expect(std.mem.indexOf(u8, merged, "1024x768x32") != null);
    try std.testing.expect(std.mem.indexOf(u8, merged, "3840x2160x32") == null);
    try std.testing.expect(std.mem.indexOf(u8, merged, "<Var>1</Var><KeyName>GFX.Monitor.Index") != null);
    try std.testing.expect(std.mem.indexOf(u8, merged, "<Var>OFF</Var><KeyName>GFX.FullScreen") != null);
    // ...while everything else arrives from the backup.
    try std.testing.expect(std.mem.indexOf(u8, merged, "<Var>80</Var><KeyName>Sound.Volume") != null);
    try std.testing.expect(std.mem.indexOf(u8, merged, "Announcer") != null);
}

/// A committed stage built by hand — the shape `stageRestore` writes.
fn buildStage(
    gpa: std.mem.Allocator,
    fixture: *Fixture,
    profile_dir: []const u8,
    nonce: []const u8,
    payload: []const u8,
    mode_text: []const u8,
    corrupt_hash: bool,
    with_commit: bool,
) !void {
    const stage = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, nonce });
    defer gpa.free(stage);

    const payload_path = try path.join(gpa, &.{ stage, backup.payload_name });
    defer gpa.free(payload_path);
    try fixture.write(payload_path, payload);

    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(payload, &digest, .{});
    var hex_buffer: [64]u8 = undefined;
    const hex = std.fmt.bufPrint(&hex_buffer, "{x}", .{&digest}) catch unreachable;

    const meta = try std.fmt.allocPrint(
        gpa,
        "{{\"mode\":\"{s}\",\"entry_id\":\"HostA/x.cfg\",\"sha256\":\"{s}\"," ++
            "\"nonce\":\"{s}\",\"created_unix\":1000}}",
        .{ mode_text, if (corrupt_hash) "0" ** 64 else hex, nonce },
    );
    defer gpa.free(meta);
    const meta_path = try path.join(gpa, &.{ stage, backup.meta_name });
    defer gpa.free(meta_path);
    try fixture.write(meta_path, meta);

    if (with_commit) {
        const commit_path = try path.join(gpa, &.{ stage, backup.commit_name });
        defer gpa.free(commit_path);
        try fixture.write(commit_path, "1");
    }
}

fn setActive(gpa: std.mem.Allocator, fixture: *Fixture, profile_dir: []const u8, nonce: []const u8) !void {
    const active = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, backup.active_name });
    defer gpa.free(active);
    try fixture.write(active, nonce);
}

fn readProfileFile(gpa: std.mem.Allocator, profile_dir: []const u8, rel: []const u8) ![]u8 {
    const at = try path.join(gpa, &.{ profile_dir, rel });
    defer gpa.free(at);
    return std.Io.Dir.cwd().readFileAlloc(io, at, gpa, .limited(1 << 20));
}

test "apply with nothing staged sweeps debris and reports it cheaply" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    // No restore root at all: the common case, and the cheap one.
    try std.testing.expectEqual(
        backup.ApplyOutcome.nothing_staged,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );

    // Debris without an ACTIVE — the state a crash between the two teardown
    // removals leaves — is ordinary, and swept.
    try buildStage(gpa, &fixture, profile_dir, "20260821T000000Z-11111111", "junk", "full", false, true);
    try std.testing.expectEqual(
        backup.ApplyOutcome.nothing_staged,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const stage = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, "20260821T000000Z-11111111" });
    defer gpa.free(stage);
    try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, stage, .{}));
}

test "a staged merge applies, snapshots once, and tears down in order" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    const nonce = "20260821T010000Z-aaaa1111";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "merge_keep_local_gfx", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );

    const merged = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(merged);
    try std.testing.expect(std.mem.indexOf(u8, merged, "1024x768x32") != null);
    try std.testing.expect(std.mem.indexOf(u8, merged, "<Var>80</Var><KeyName>Sound.Volume") != null);

    // The undo snapshot is the pre-restore original, keyed by nonce, and
    // LATEST_UNDO names it.
    const undo = try readProfileFile(gpa, profile_dir, ".cloudsync-trash/config/" ++ nonce ++ ".cfg");
    defer gpa.free(undo);
    try std.testing.expectEqualStrings(local_xml, undo);
    const pointer = try readProfileFile(gpa, profile_dir, ".cloudsync-trash/config/LATEST_UNDO");
    defer gpa.free(pointer);
    try std.testing.expectEqualStrings(nonce, pointer);

    // Teardown left neither pointer nor stage.
    const active = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, backup.active_name });
    defer gpa.free(active);
    try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, active, .{}));
    const stage = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, nonce });
    defer gpa.free(stage);
    try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, stage, .{}));

    // And a second call is the cheap nothing-staged path.
    try std.testing.expectEqual(
        backup.ApplyOutcome.nothing_staged,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
}

test "a full restore adopts everything including GFX" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    const nonce = "20260821T020000Z-bbbb2222";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "full", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const applied = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(applied);
    try std.testing.expectEqualStrings(restored_xml, applied);
}

test "the merge runs against the config as it stands at apply time" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    const nonce = "20260821T030000Z-cccc3333";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "merge_keep_local_gfx", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);

    // The player kept playing after staging: a new resolution landed in
    // config.cfg. The merge must keep *this* one, not the one from staging
    // time — merging early would quietly revert it.
    const changed_local =
        \\<base><Options><Vars>
        \\  <item Order="1"><Var>2560x1440x32</Var><KeyName>GFX.Mode</KeyName></item>
        \\  <item Order="4"><Var>55</Var><KeyName>Sound.Volume</KeyName></item>
        \\</Vars></Options></base>
    ;
    try fixture.write(config_path, changed_local);

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const merged = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(merged);
    try std.testing.expect(std.mem.indexOf(u8, merged, "2560x1440x32") != null);
    try std.testing.expect(std.mem.indexOf(u8, merged, "3840x2160x32") == null);
    // Non-GFX still comes from the backup, not from the changed local.
    try std.testing.expect(std.mem.indexOf(u8, merged, "<Var>80</Var><KeyName>Sound.Volume") != null);
}

test "a stage ACTIVE names that fails validation is a hard error" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    // Missing COMMIT: incomplete, and named by ACTIVE — refuse.
    try buildStage(gpa, &fixture, profile_dir, "20260821T040000Z-dddd4444", restored_xml, "full", false, false);
    try setActive(gpa, &fixture, profile_dir, "20260821T040000Z-dddd4444");
    try std.testing.expectError(
        error.StageCorrupt,
        backup.applyPendingRestore(gpa, io, profile_dir),
    );

    // A wrong payload hash is the same refusal: we were told to apply
    // specific content and this is not it.
    try buildStage(gpa, &fixture, profile_dir, "20260821T050000Z-eeee5555", restored_xml, "full", true, true);
    try setActive(gpa, &fixture, profile_dir, "20260821T050000Z-eeee5555");
    try std.testing.expectError(
        error.StageCorrupt,
        backup.applyPendingRestore(gpa, io, profile_dir),
    );
}

test "ACTIVE naming an absent stage clears and reports nothing staged" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    // Correct teardown cannot produce this: outside interference or partial
    // disk loss. Distinctly from the corrupt case, it must not stop the
    // game — clear, sweep, nothing staged.
    try buildStage(gpa, &fixture, profile_dir, "20260821T060000Z-ffff6666", restored_xml, "full", false, true);
    try setActive(gpa, &fixture, profile_dir, "20260821T990000Z-00000000");

    try std.testing.expectEqual(
        backup.ApplyOutcome.nothing_staged,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const active = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, backup.active_name });
    defer gpa.free(active);
    try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, active, .{}));
}

test "ACTIVE decides between committed stages regardless of timestamps" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // Two whole stages, equal meta timestamps (buildStage writes 1000 for
    // both): COMMIT can say "whole" twice, only ACTIVE says "current".
    // First round: the pointer names the *older* nonce and wins anyway.
    {
        const profile_dir = try fixture.makeDir("pa");
        defer gpa.free(profile_dir);
        const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
        defer gpa.free(config_path);
        try fixture.write(config_path, "");
        try buildStage(gpa, &fixture, profile_dir, "20260801T000000Z-aaaa0000", "payload-old", "full", false, true);
        try buildStage(gpa, &fixture, profile_dir, "20260821T000000Z-bbbb0000", "payload-new", "full", false, true);
        try setActive(gpa, &fixture, profile_dir, "20260801T000000Z-aaaa0000");

        try std.testing.expectEqual(
            backup.ApplyOutcome.applied,
            try backup.applyPendingRestore(gpa, io, profile_dir),
        );
        const applied = try readProfileFile(gpa, profile_dir, "config.cfg");
        defer gpa.free(applied);
        try std.testing.expectEqualStrings("payload-old", applied);
    }
    // Second round, reversed: ACTIVE names the newer one.
    {
        const profile_dir = try fixture.makeDir("pb");
        defer gpa.free(profile_dir);
        const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
        defer gpa.free(config_path);
        try fixture.write(config_path, "");
        try buildStage(gpa, &fixture, profile_dir, "20260801T000000Z-aaaa0000", "payload-old", "full", false, true);
        try buildStage(gpa, &fixture, profile_dir, "20260821T000000Z-bbbb0000", "payload-new", "full", false, true);
        try setActive(gpa, &fixture, profile_dir, "20260821T000000Z-bbbb0000");

        try std.testing.expectEqual(
            backup.ApplyOutcome.applied,
            try backup.applyPendingRestore(gpa, io, profile_dir),
        );
        const applied = try readProfileFile(gpa, profile_dir, "config.cfg");
        defer gpa.free(applied);
        try std.testing.expectEqualStrings("payload-new", applied);
    }
}

test "a partial newer stage never displaces the published one" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);
    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, "");

    // The published stage, and beside it the wreck of a download that died
    // before COMMIT — the exact state a failed re-stage leaves. The player
    // still has the old restore.
    try buildStage(gpa, &fixture, profile_dir, "20260810T000000Z-aaaa0000", "published", "full", false, true);
    try setActive(gpa, &fixture, profile_dir, "20260810T000000Z-aaaa0000");
    try buildStage(gpa, &fixture, profile_dir, "20260821T000000Z-bbbb0000", "half-downloaded", "full", false, false);

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const applied = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(applied);
    try std.testing.expectEqualStrings("published", applied);
}

test "a crash between config rename and cleanup retries idempotently" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);
    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    const nonce = "20260821T070000Z-abab7777";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "merge_keep_local_gfx", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );

    // The crash window: config renamed, stage not yet torn down. The next
    // launch sees the stage still committed and applies again.
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "merge_keep_local_gfx", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);
    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );

    // Re-merging is harmless; a second snapshot would not be. Keyed by
    // nonce and written once, the undo still holds the *original* — not the
    // already-restored file the retry ran against.
    const undo = try readProfileFile(gpa, profile_dir, ".cloudsync-trash/config/" ++ nonce ++ ".cfg");
    defer gpa.free(undo);
    try std.testing.expectEqualStrings(local_xml, undo);
}

// -- Undo ----------------------------------------------------------------------

test "undo reinstates the original across a simulated restart, and undoes itself" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);
    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    // Restore applied (merge), then undone, then the undo applied at the
    // "next startup": the original comes back byte for byte.
    const nonce = "20260821T080000Z-cdcd8888";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "merge_keep_local_gfx", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);
    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const merged = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(merged);

    try std.testing.expectEqual(
        backup.UndoAvailability.reinstatable,
        try backup.restoreUndoAvailability(gpa, io, profile_dir),
    );
    try std.testing.expectEqual(
        backup.UndoAction.staged_reinstate,
        try backup.undoRestore(gpa, io, profile_dir),
    );
    // Staged, not yet applied: config still the merged one, and the UI's
    // state has flipped from "undo applied restore" to "cancel pending".
    try std.testing.expectEqual(
        backup.UndoAvailability.cancellable,
        try backup.restoreUndoAvailability(gpa, io, profile_dir),
    );

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const back = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(back);
    try std.testing.expectEqualStrings(local_xml, back);

    // The undo's own apply snapshotted the pre-undo (merged) config, so
    // undo-of-undo is a redo for free.
    try std.testing.expectEqual(
        backup.UndoAvailability.reinstatable,
        try backup.restoreUndoAvailability(gpa, io, profile_dir),
    );
    try std.testing.expectEqual(
        backup.UndoAction.staged_reinstate,
        try backup.undoRestore(gpa, io, profile_dir),
    );
    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const redone = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(redone);
    try std.testing.expectEqualStrings(merged, redone);
}

test "undo recovers a change written after staging but before application" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);
    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    const nonce = "20260821T090000Z-efef9999";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "merge_keep_local_gfx", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);

    // A whole session passed between staging and startup; the config the
    // player actually had immediately before application is this one, and
    // it is this one the undo must reproduce.
    const changed = "<base><Options><Vars><item><Var>77</Var><KeyName>Sound.Volume</KeyName></item></Vars></Options></base>";
    try fixture.write(config_path, changed);

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    try std.testing.expectEqual(
        backup.UndoAction.staged_reinstate,
        try backup.undoRestore(gpa, io, profile_dir),
    );
    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const recovered = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(recovered);
    try std.testing.expectEqualStrings(changed, recovered);
}

test "undo cancels a staged unapplied restore rather than reinstating" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);
    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    const nonce = "20260821T100000Z-baba0000";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "full", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);
    try std.testing.expectEqual(
        backup.UndoAvailability.cancellable,
        try backup.restoreUndoAvailability(gpa, io, profile_dir),
    );

    // Nothing has applied, so there is nothing to reinstate: the stage is
    // discarded — pointer first, directories second — and the config never
    // moves.
    try std.testing.expectEqual(
        backup.UndoAction.cancelled_stage,
        try backup.undoRestore(gpa, io, profile_dir),
    );
    const untouched = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(untouched);
    try std.testing.expectEqualStrings(local_xml, untouched);
    const active = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, backup.active_name });
    defer gpa.free(active);
    try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, active, .{}));
    try std.testing.expectEqual(
        backup.UndoAvailability.none,
        try backup.restoreUndoAvailability(gpa, io, profile_dir),
    );

    // And with nothing staged and nothing applied, undo is a typed refusal.
    try std.testing.expectError(error.NothingToUndo, backup.undoRestore(gpa, io, profile_dir));
}

test "a crash during snapshot creation cannot corrupt the undo" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);
    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);

    const nonce = "20260821T110000Z-dede1111";
    try buildStage(gpa, &fixture, profile_dir, nonce, restored_xml, "full", false, true);
    try setActive(gpa, &fixture, profile_dir, nonce);

    // The first crash window is after the config rename; this is the one
    // before it — mid-copy. The residue is a truncated temp under the name
    // the retry would otherwise trust once renamed.
    const tmp_residue = try path.join(gpa, &.{
        profile_dir,
        backup.undo_dir_relative,
        nonce ++ ".cfg.tmp",
    });
    defer gpa.free(tmp_residue);
    try fixture.write(tmp_residue, "trunc");

    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const undo = try readProfileFile(gpa, profile_dir, ".cloudsync-trash/config/" ++ nonce ++ ".cfg");
    defer gpa.free(undo);
    try std.testing.expectEqualStrings(local_xml, undo);
    try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, tmp_residue, .{}));
}

test "trash pruning never touches the undo snapshots" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const trash = try fixture.makeDir("p1/.cloudsync-trash");
    defer gpa.free(trash);

    // An ancient run directory, prunable; beside it the config/ undo store,
    // whose name is not a run id and which the pruner therefore never
    // considers — the structural exemption, asserted.
    const old_run = try path.join(gpa, &.{ trash, "20200101T000000Z-aaaaaaaa", "x.sav" });
    defer gpa.free(old_run);
    try fixture.write(old_run, "old");
    const undo_snapshot = try path.join(gpa, &.{ trash, "config", "20200101T000000Z-bbbbbbbb.cfg" });
    defer gpa.free(undo_snapshot);
    try fixture.write(undo_snapshot, "the only undo path");

    const now = engine.runIdTimestamp("20260821T120000Z-00000000").?;
    const report = try engine.pruneTrash(gpa, io, trash, .{
        .max_age_days = 1,
        .min_keep_runs = 0,
    }, now);
    try std.testing.expectEqual(@as(usize, 1), report.removed);

    const kept = try std.Io.Dir.cwd().readFileAlloc(io, undo_snapshot, gpa, .limited(256));
    defer gpa.free(kept);
    try std.testing.expectEqualStrings("the only undo path", kept);
}

/// Accepts a connection and never answers — the operation-slot race fixture.
const HungServer = struct {
    io: std.Io,
    server: std.Io.net.Server,
    port: u16,
    stop_flag: std.atomic.Value(bool),
    thread: ?std.Thread,
    stopped: bool,

    fn start(self: *HungServer, target_io: std.Io) !void {
        var addr: std.Io.net.IpAddress = .{ .ip4 = .loopback(0) };
        self.* = .{
            .io = target_io,
            .server = try addr.listen(target_io, .{ .reuse_address = true }),
            .port = 0,
            .stop_flag = .init(false),
            .thread = null,
            .stopped = false,
        };
        self.port = self.server.socket.address.getPort();
        self.thread = try std.Thread.spawn(.{}, run, .{self});
    }

    fn stop(self: *HungServer) void {
        if (self.stopped) return;
        self.stopped = true;
        self.stop_flag.store(true, .release);
        if (self.thread) |t| {
            t.join();
            self.thread = null;
        }
        self.server.deinit(self.io);
    }

    fn run(self: *HungServer) void {
        var stream = self.server.accept(self.io) catch return;
        defer stream.close(self.io);
        while (!self.stop_flag.load(.acquire)) {
            const tick: std.Io.Clock.Duration = .{
                .raw = .fromMilliseconds(10),
                .clock = .awake,
            };
            tick.sleep(self.io) catch break;
        }
    }
};

test "undo reports busy while a restore download holds the slot" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var hung: HungServer = undefined;
    try hung.start(tio);
    defer hung.stop();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    // A LATEST_UNDO from an earlier restore: exactly the state that makes a
    // naive availability say "undo me" while a new download is in flight.
    const undo_snapshot = try path.join(gpa, &.{ profile_dir, backup.undo_dir_relative, "20260801T000000Z-cafe0001.cfg" });
    defer gpa.free(undo_snapshot);
    try fixture.write(undo_snapshot, local_xml);
    const pointer = try path.join(gpa, &.{ profile_dir, backup.undo_dir_relative, backup.latest_undo_name });
    defer gpa.free(pointer);
    try fixture.write(pointer, "20260801T000000Z-cafe0001");

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = .{ .host = "127.0.0.1", .port = hung.port, .user = "u", .pass = "p" },
        .deadline = .{ .connect_ms = 1_000, .read_ms = 1_000 },
    });
    defer w.destroy();

    // The download starts and wedges against the hung transport.
    try w.begin(.{
        .kind = .restore_stage,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "",
        .remote_fingerprint = "",
        .entry_id = "HostA/x.cfg",
    });
    sleepMs(tio, 200);

    // Undo while the slot is held: refused outright, and ACTIVE untouched —
    // the silent alternative is an undo the finishing download overwrites.
    try std.testing.expectError(error.Busy, w.begin(.{
        .kind = .restore_undo,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "",
        .remote_fingerprint = "",
    }));
    const active = try path.join(gpa, &.{ profile_dir, backup.restore_dir_name, backup.active_name });
    defer gpa.free(active);
    try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, active, .{}));

    // Once the slot frees — the wedged download fails on its deadline — the
    // same undo is accepted and publishes its stage.
    var snapshot = pollUntilSettled(w, tio, 30_000);
    try std.testing.expectEqual(worker.State.failed, snapshot.state);

    try w.begin(.{
        .kind = .restore_undo,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "",
        .remote_fingerprint = "",
    });
    snapshot = pollUntilSettled(w, tio, 30_000);
    try std.testing.expectEqualStrings("", snapshot.errorText());
    try std.testing.expectEqual(worker.Outcome.undo_done, snapshot.outcome);
    const published = try readProfileFile(gpa, profile_dir, backup.restore_dir_name ++ "/" ++ backup.active_name);
    defer gpa.free(published);
    try std.testing.expect(published.len > 0);

    // And the published reinstate applies to the snapshot's content.
    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const applied = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(applied);
    try std.testing.expectEqualStrings(local_xml, applied);
}

test "staging downloads into a fresh stage and a failed download leaves the old one" {
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
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    const remote_backup = try path.join(gpa, &.{ cloud, "config-backups", "hero", "HostA", "20260820T000000Z-12121212.cfg" });
    defer gpa.free(remote_backup);
    try fixture.write(remote_backup, restored_xml);

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    try createAliasRemote(gpa, &client, "bkremote", cloud);

    const nonce = try backup.stageRestore(
        gpa,
        tio,
        &client,
        profile_dir,
        "bkremote",
        "hero",
        "HostA/20260820T000000Z-12121212.cfg",
        .merge_keep_local_gfx,
    );
    defer gpa.free(nonce);

    // Committed and published.
    const active = try readProfileFile(gpa, profile_dir, backup.restore_dir_name ++ "/" ++ backup.active_name);
    defer gpa.free(active);
    try std.testing.expectEqualStrings(nonce, active);

    // A second staging whose download fails — the entry does not exist —
    // must leave the published stage and pointer exactly as they were.
    try std.testing.expectError(error.RcFailed, backup.stageRestore(
        gpa,
        tio,
        &client,
        profile_dir,
        "bkremote",
        "hero",
        "HostA/no-such-backup.cfg",
        .merge_keep_local_gfx,
    ));
    const still_active = try readProfileFile(gpa, profile_dir, backup.restore_dir_name ++ "/" ++ backup.active_name);
    defer gpa.free(still_active);
    try std.testing.expectEqualStrings(nonce, still_active);

    // And the published stage applies, locally, daemonless from here.
    const config_path = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(config_path);
    try fixture.write(config_path, local_xml);
    try std.testing.expectEqual(
        backup.ApplyOutcome.applied,
        try backup.applyPendingRestore(gpa, io, profile_dir),
    );
    const merged = try readProfileFile(gpa, profile_dir, "config.cfg");
    defer gpa.free(merged);
    try std.testing.expect(std.mem.indexOf(u8, merged, "1024x768x32") != null);
    try std.testing.expect(std.mem.indexOf(u8, merged, "<Var>80</Var><KeyName>Sound.Volume") != null);
}

test "the worker snapshots after a clean sync and nothing syncs back down" {
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
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    const save = try path.join(gpa, &.{ profile_dir, "quick.sav" });
    defer gpa.free(save);
    try fixture.write(save, "v1");
    // The active profile owns config.cfg: it lives *inside* Path1, which is
    // exactly why the filter must keep it out of the sync set while the
    // snapshot path carries it up separately.
    const profile_config = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(profile_config);
    try fixture.write(profile_config, "GFX.Mode = profile-owned");

    const conf = try path.join(gpa, &.{ game_dir, "cloudsync", "rclone.conf" });
    defer gpa.free(conf);
    const conf_text = try std.fmt.allocPrint(
        gpa,
        "[bkremote]\ntype = alias\nremote = {s}\n",
        .{cloud},
    );
    defer gpa.free(conf_text);
    try fixture.write(conf, conf_text);

    const binary_slice: []const u8 = binary;
    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .binary_source = .{
            .context = @constCast(@ptrCast(&binary_slice)),
            .resolve = resolveFixedBinary,
        },
    });
    defer w.destroy();

    // Pair without the option: no snapshot may appear.
    try w.begin(.{
        .kind = .pair,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "alias:cloud",
    });
    var snapshot = pollUntilSettled(w, tio, 120_000);
    try std.testing.expectEqualStrings("", snapshot.errorText());
    try std.testing.expectEqual(worker.State.done, snapshot.state);
    const backups_root = try path.join(gpa, &.{ cloud, "config-backups" });
    defer gpa.free(backups_root);
    try std.testing.expectError(
        error.FileNotFound,
        std.Io.Dir.cwd().statFile(io, backups_root, .{}),
    );

    // Sync with the option on: exactly one snapshot for this host.
    try fixture.write(save, "v2");
    try w.begin(.{
        .kind = .sync,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "alias:cloud",
        .backup_config = true,
        .host = "TestRig",
    });
    snapshot = pollUntilSettled(w, tio, 120_000);
    try std.testing.expectEqualStrings("", snapshot.errorText());
    try std.testing.expectEqual(worker.Outcome.synced, snapshot.outcome);

    const host_dir = try path.join(gpa, &.{ cloud, "config-backups", "hero", "TestRig" });
    defer gpa.free(host_dir);
    const first = (try onlySnapshotIn(gpa, host_dir)).?;
    defer gpa.free(first);
    const first_path = try path.join(gpa, &.{ host_dir, first });
    defer gpa.free(first_path);
    const stored = try std.Io.Dir.cwd().readFileAlloc(io, first_path, gpa, .limited(4096));
    defer gpa.free(stored);
    try std.testing.expectEqualStrings("GFX.Mode = profile-owned", stored);

    // The backup history never syncs back down, and config.cfg never went
    // up: the split the layout plus the filter set exists to guarantee.
    try fixture.write(save, "v3");
    try w.begin(.{
        .kind = .sync,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "alias:cloud",
    });
    snapshot = pollUntilSettled(w, tio, 120_000);
    try std.testing.expectEqual(worker.Outcome.synced, snapshot.outcome);

    const pulled = try path.join(gpa, &.{ profile_dir, "config-backups" });
    defer gpa.free(pulled);
    try std.testing.expectError(
        error.FileNotFound,
        std.Io.Dir.cwd().statFile(io, pulled, .{}),
    );
    const uploaded_config = try path.join(gpa, &.{ cloud, "profiles", "hero", "config.cfg" });
    defer gpa.free(uploaded_config);
    try std.testing.expectError(
        error.FileNotFound,
        std.Io.Dir.cwd().statFile(io, uploaded_config, .{}),
    );
    // The profile's config is untouched and unsynced.
    const untouched = try std.Io.Dir.cwd().readFileAlloc(io, profile_config, gpa, .limited(4096));
    defer gpa.free(untouched);
    try std.testing.expectEqualStrings("GFX.Mode = profile-owned", untouched);
}

fn resolveFixedBinary(context: ?*anyopaque, gpa: std.mem.Allocator) ?[]u8 {
    const binary: *const []const u8 = @ptrCast(@alignCast(context.?));
    return gpa.dupe(u8, binary.*) catch null;
}

fn pollUntilSettled(
    w: *worker.Worker,
    target_io: std.Io,
    budget_ms: u32,
) worker.Snapshot {
    var waited: u32 = 0;
    while (waited < budget_ms) {
        const snapshot = w.poll();
        switch (snapshot.state) {
            .done, .failed => return snapshot,
            else => {},
        }
        sleepMs(target_io, 25);
        waited += 25;
    }
    return w.poll();
}
