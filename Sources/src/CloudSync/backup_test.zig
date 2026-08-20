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
    const game_config = try path.join(gpa, &.{ game_dir, "config.cfg" });
    defer gpa.free(game_config);
    try fixture.write(game_config, "GFX.Mode = 4k");
    // A config.cfg *inside* the profile is the filter's problem, and it must
    // stay home no matter what the backup path does.
    const profile_config = try path.join(gpa, &.{ profile_dir, "config.cfg" });
    defer gpa.free(profile_config);
    try fixture.write(profile_config, "GFX.Mode = stray");

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
    try std.testing.expectEqualStrings("GFX.Mode = 4k", stored);

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
    // The local stray config is untouched and unsynced.
    const stray = try std.Io.Dir.cwd().readFileAlloc(io, profile_config, gpa, .limited(4096));
    defer gpa.free(stray);
    try std.testing.expectEqualStrings("GFX.Mode = stray", stray);
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
