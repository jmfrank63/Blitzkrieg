//! Backend integration: the phase-02 cycle against real credential-based
//! remotes. P03-M02 drives an S3-compatible server (MinIO); P03-M03 adds
//! WebDAV over `rclone serve webdav`.
//!
//! Everything here is gated the way the daemon suite's live cases are:
//! `BK_TEST_RCLONE` names the pinned rclone, `BK_TEST_MINIO` names a MinIO
//! server binary, and a machine without either passes the suite silently —
//! the gate depends on neither a paid account nor a network, per the packet.
//!
//! The remote is no longer a directory the test can read, so assertions pull
//! files down through `operations/copyfile` and stat them through
//! `operations/stat` — the same rc surface the game itself is limited to.

const std = @import("std");
const builtin = @import("builtin");
const creds = @import("creds.zig");
const daemon = @import("daemon.zig");
const engine = @import("engine.zig");
const plan = @import("plan.zig");
const rc = @import("rc.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;
const net = std.Io.net;

const rclone_env = "BK_TEST_RCLONE";
const minio_env = "BK_TEST_MINIO";

const minio_user = "bkminio";
const minio_password = "bkminio-secret-key";
const bucket = "bk-saves";

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

fn reservePort(target_io: std.Io) !u16 {
    var addr: net.IpAddress = .{ .ip4 = .loopback(0) };
    var server = try addr.listen(target_io, .{ .reuse_address = true });
    const port = server.socket.address.getPort();
    server.deinit(target_io);
    return port;
}

/// True once something accepts on the port. MinIO takes a moment to come up.
fn waitTcp(target_io: std.Io, port: u16, budget_ms: u32) bool {
    var waited: u32 = 0;
    while (waited < budget_ms) {
        const addr: net.IpAddress = .{ .ip4 = .loopback(port) };
        if (addr.connect(target_io, .{ .mode = .stream })) |stream| {
            var open = stream;
            open.close(target_io);
            return true;
        } else |_| {}
        sleepMs(target_io, 100);
        waited += 100;
    }
    return false;
}

/// A MinIO server on a fresh port over a fixture data directory.
const Minio = struct {
    child: std.process.Child,
    port: u16,
    target_io: std.Io,

    fn spawn(gpa: std.mem.Allocator, target_io: std.Io, exe: []const u8, data_dir: []const u8) !Minio {
        const port = try reservePort(target_io);
        var addr_buffer: [32]u8 = undefined;
        const addr = std.fmt.bufPrint(&addr_buffer, "127.0.0.1:{d}", .{port}) catch unreachable;

        var environ = try parentEnviron(gpa);
        defer environ.deinit();
        try environ.put("MINIO_ROOT_USER", minio_user);
        try environ.put("MINIO_ROOT_PASSWORD", minio_password);

        const child = std.process.spawn(target_io, .{
            .argv = &.{ exe, "server", data_dir, "--address", addr },
            .environ_map = &environ,
            .stdin = .ignore,
            .stdout = .ignore,
            .stderr = .ignore,
        }) catch return error.MinioSpawnFailed;

        return .{ .child = child, .port = port, .target_io = target_io };
    }

    fn stop(self: *Minio) void {
        self.child.kill(self.target_io);
        self.* = undefined;
    }
};

fn parentEnviron(gpa: std.mem.Allocator) !std.process.Environ.Map {
    if (builtin.os.tag == .windows) {
        const environ: std.process.Environ = .{ .block = .global };
        return environ.createMap(gpa);
    }
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

/// `config/create` with the given parameters object.
fn createRemote(
    gpa: std.mem.Allocator,
    client: *rc.Client,
    name: []const u8,
    params: std.json.Value,
) !void {
    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "name", .{ .string = name });
    const remote_type = params.object.get("type").?.string;
    try object.put(gpa, "type", .{ .string = remote_type });
    // The type key rides inside `remoteParams`' object too; rclone ignores a
    // duplicate parameter it already got explicitly.
    try object.put(gpa, "parameters", params);

    var reply = try client.call("config/create", .{ .object = object });
    reply.deinit();
}

fn createAlias(gpa: std.mem.Allocator, client: *rc.Client, name: []const u8, target: []const u8) !void {
    var parameters: std.json.ObjectMap = .empty;
    defer parameters.deinit(gpa);
    try parameters.put(gpa, "type", .{ .string = "alias" });
    try parameters.put(gpa, "remote", .{ .string = target });
    try createRemote(gpa, client, name, .{ .object = parameters });
}

/// Pull one remote file into `scratch_dir` and return its content — the
/// remote is not a directory this test can read.
fn remoteRead(
    gpa: std.mem.Allocator,
    client: *rc.Client,
    fs_spec: []const u8,
    name: []const u8,
    scratch_dir: []const u8,
) ![]u8 {
    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "srcFs", .{ .string = fs_spec });
    try object.put(gpa, "srcRemote", .{ .string = name });
    try object.put(gpa, "dstFs", .{ .string = scratch_dir });
    try object.put(gpa, "dstRemote", .{ .string = "pulled.tmp" });
    var reply = try client.call("operations/copyfile", .{ .object = object });
    reply.deinit();

    const pulled = try path.join(gpa, &.{ scratch_dir, "pulled.tmp" });
    defer gpa.free(pulled);
    return std.Io.Dir.cwd().readFileAlloc(io, pulled, gpa, .limited(65536));
}

fn remoteExists(gpa: std.mem.Allocator, client: *rc.Client, fs_spec: []const u8, name: []const u8) !bool {
    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "fs", .{ .string = fs_spec });
    try object.put(gpa, "remote", .{ .string = name });
    var reply = try client.call("operations/stat", .{ .object = object });
    defer reply.deinit();
    const item = reply.value.object.get("item") orelse return false;
    return item != .null;
}

fn remoteDelete(gpa: std.mem.Allocator, client: *rc.Client, fs_spec: []const u8, name: []const u8) !void {
    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "fs", .{ .string = fs_spec });
    try object.put(gpa, "remote", .{ .string = name });
    var reply = try client.call("operations/deletefile", .{ .object = object });
    reply.deinit();
}

/// Upload `content` as `name`, preserving the staging file's mtime — which
/// is "now", making the remote copy newer than anything written before it.
fn remoteWrite(
    gpa: std.mem.Allocator,
    client: *rc.Client,
    fs_spec: []const u8,
    name: []const u8,
    content: []const u8,
    scratch_dir: []const u8,
) !void {
    const staged = try path.join(gpa, &.{ scratch_dir, "staged.tmp" });
    defer gpa.free(staged);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = staged, .data = content });

    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "srcFs", .{ .string = scratch_dir });
    try object.put(gpa, "srcRemote", .{ .string = "staged.tmp" });
    try object.put(gpa, "dstFs", .{ .string = fs_spec });
    try object.put(gpa, "dstRemote", .{ .string = name });
    var reply = try client.call("operations/copyfile", .{ .object = object });
    reply.deinit();
}

/// Whether any entry of `dir` carries `.conflict` in its name.
fn hasConflictFile(gpa: std.mem.Allocator, dir_path: []const u8) bool {
    var dir = std.Io.Dir.cwd().openDir(io, dir_path, .{ .iterate = true }) catch return false;
    defer dir.close(io);
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (std.mem.indexOf(u8, entry.name, ".conflict") != null) return true;
    }
    _ = gpa;
    return false;
}

test "the phase-02 cycle passes against a real S3 remote" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const rclone = liveRclone(gpa, tio) orelse return;
    defer gpa.free(rclone);
    const minio_exe = envVar(gpa, minio_env) orelse return;
    defer gpa.free(minio_exe);
    if (minio_exe.len == 0) return;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const minio_data = try fixture.makeDir("minio");
    defer gpa.free(minio_data);
    const scratch = try fixture.makeDir("scratch");
    defer gpa.free(scratch);
    const profile_dir = try fixture.makeDir("p1");
    defer gpa.free(profile_dir);

    // Enough files that the diverge step's two deletes stay inside the 50%
    // ratio the sentinel arithmetic promises.
    const quick = try path.join(gpa, &.{ profile_dir, "quick.sav" });
    defer gpa.free(quick);
    try fixture.write(quick, "v1");
    const f_local = try path.join(gpa, &.{ profile_dir, "f-local.sav" });
    defer gpa.free(f_local);
    try fixture.write(f_local, "keep-l");
    const f_remote = try path.join(gpa, &.{ profile_dir, "f-remote.sav" });
    defer gpa.free(f_remote);
    try fixture.write(f_remote, "keep-r");
    const pad = try path.join(gpa, &.{ profile_dir, "pad.sav" });
    defer gpa.free(pad);
    try fixture.write(pad, "pad");

    var minio = try Minio.spawn(gpa, tio, minio_exe, minio_data);
    defer minio.stop();
    try std.testing.expect(waitTcp(tio, minio.port, 30_000));
    // TCP up is not API up; MinIO finishes its bucket scan just after.
    sleepMs(tio, 1_000);

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = rclone, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();

    // The two remotes exactly as the credentials packet defines them: the
    // raw S3 backend from `remoteParams`, and the alias whose target carries
    // the bucket, so Path2's session-name contribution stays `bkremote:...`.
    const endpoint = try std.fmt.allocPrint(gpa, "http://127.0.0.1:{d}", .{minio.port});
    defer gpa.free(endpoint);
    const s3_creds: creds.Credentials = .{
        .payload = .{ .s3 = .{
            .s3_provider = "Minio",
            .endpoint = endpoint,
            .bucket = bucket,
            .region = "us-east-1",
            .access_key = minio_user,
            .secret = minio_password,
        } },
    };
    var params = try creds.remoteParams(gpa, s3_creds);
    defer params.deinit();
    try createRemote(gpa, &client, creds.backend_remote_name, params.value);
    const alias_target = try creds.aliasTarget(gpa, s3_creds);
    defer gpa.free(alias_target);
    try createAlias(gpa, &client, creds.sync_remote_name, alias_target);

    var eng = engine.Engine.init(gpa, tio, &client);
    defer eng.deinit();

    const print = try creds.fingerprint(gpa, s3_creds);
    defer gpa.free(print);
    const ctx: engine.RunContext = .{
        .path1 = profile_dir,
        .remote = creds.sync_remote_name,
        .profile = "hero",
        .game_dir = game_dir,
        .profile_id = "hero-id",
        .remote_fingerprint = print,
    };

    // The session name stays within budget by construction: the alias
    // contributes its constant short form no matter how long the endpoint
    // or bucket grow.
    {
        const path2 = try plan.remoteProfileRoot(gpa, ctx.remote, ctx.profile);
        defer gpa.free(path2);
        var projected: usize = 0;
        try plan.checkSessionBudget(
            gpa,
            .{ .path = ctx.path1, .kind = .local },
            .{ .path = path2, .kind = .remote },
            &projected,
        );
        try std.testing.expect(projected < 160);
    }

    // Pair against a bucket that does not exist yet — the first-run state.
    var outcome = eng.pair(ctx) catch |err| {
        try std.testing.expectEqualStrings("", eng.lastErrorText());
        return err;
    };
    defer outcome.deinit(gpa);
    try std.testing.expectEqual(plan.SentinelAction.written, outcome.sentinel);

    const profile_fs = "bkremote:profiles/hero";
    {
        const uploaded = try remoteRead(gpa, &client, profile_fs, "quick.sav", scratch);
        defer gpa.free(uploaded);
        try std.testing.expectEqualStrings("v1", uploaded);
    }
    try std.testing.expect(try remoteExists(gpa, &client, profile_fs, plan.sentinel_name));

    // Diverge: the remote side first, then — measurably newer — the local
    // side, plus one delete on each side.
    try remoteWrite(gpa, &client, profile_fs, "quick.sav", "v2-remote", scratch);
    sleepMs(tio, 2_000);
    try fixture.write(quick, "v2-local");
    try std.Io.Dir.cwd().deleteFile(io, f_local);
    try remoteDelete(gpa, &client, profile_fs, "f-remote.sav");

    const run_id = eng.syncOnce(ctx) catch |err| {
        try std.testing.expectEqualStrings("", eng.lastErrorText());
        return err;
    };
    defer gpa.free(run_id);

    // Converged: newer side on both, conflict loser preserved, and each
    // side's delete recoverable from its own trash — backupDir2 genuinely on
    // the remote filesystem, outside the synced prefix.
    {
        const local_now = try std.Io.Dir.cwd().readFileAlloc(io, quick, gpa, .limited(4096));
        defer gpa.free(local_now);
        try std.testing.expectEqualStrings("v2-local", local_now);
        const remote_now = try remoteRead(gpa, &client, profile_fs, "quick.sav", scratch);
        defer gpa.free(remote_now);
        try std.testing.expectEqualStrings("v2-local", remote_now);
    }
    try std.testing.expect(hasConflictFile(gpa, profile_dir));

    const local_trash = try path.join(gpa, &.{ profile_dir, plan.local_trash_dir_name, run_id, "f-remote.sav" });
    defer gpa.free(local_trash);
    const recovered = try std.Io.Dir.cwd().readFileAlloc(io, local_trash, gpa, .limited(4096));
    defer gpa.free(recovered);
    try std.testing.expectEqualStrings("keep-r", recovered);

    const remote_trash_name = try std.fmt.allocPrint(gpa, "{s}/f-local.sav", .{run_id});
    defer gpa.free(remote_trash_name);
    try std.testing.expect(try remoteExists(gpa, &client, "bkremote:trash/hero", remote_trash_name));
    {
        const from_trash = try remoteRead(gpa, &client, "bkremote:trash/hero", remote_trash_name, scratch);
        defer gpa.free(from_trash);
        try std.testing.expectEqualStrings("keep-l", from_trash);
    }

    // The trash is a sibling of the synced prefix inside the bucket — never
    // under profiles/, where it would sync back down.
    try std.testing.expect(!try remoteExists(gpa, &client, profile_fs, "trash"));
}
