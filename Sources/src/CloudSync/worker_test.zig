//! Tests for the worker thread.
//!
//! The load-bearing case is the hung transport: a server that accepts and
//! never writes a byte — the fixture EXECUTION.md prescribes, because a real
//! daemon cannot be relied on to hang on demand — while the main thread
//! measures every `poll`. The invariant under test is the plan's central one:
//! no socket wait ever reaches the calling thread, so `poll` stays under a
//! 60 Hz frame even while the worker is stuck in a POST, and the run ends in
//! `.failed` on the deadline rather than hanging.
//!
//! The live case is gated on a real rclone exactly as in `engine_test.zig`,
//! and drives pair-then-sync entirely through `begin`/`poll` — including the
//! daemon spawn, which `begin` must never wait for.

const std = @import("std");
const builtin = @import("builtin");
const worker = @import("worker.zig");
const engine = @import("engine.zig");
const daemon = @import("daemon.zig");
const rc = @import("rc.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;
const net = std.Io.net;

const rclone_env = "BK_TEST_RCLONE";

/// One frame at 60 Hz, in nanoseconds: the packet's bound on `poll`.
const frame_ns: i96 = 16_666_666;

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

/// Accepts connections and never writes a byte — a wedged daemon, on demand.
const HungServer = struct {
    io: std.Io,
    server: net.Server,
    port: u16,
    stop_flag: std.atomic.Value(bool),
    thread: ?std.Thread,
    stopped: bool,

    fn start(self: *HungServer, target_io: std.Io) !void {
        var addr: net.IpAddress = .{ .ip4 = .loopback(0) };
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

    fn endpoint(self: *const HungServer) rc.Endpoint {
        return .{ .host = "127.0.0.1", .port = self.port, .user = "u", .pass = "p" };
    }

    fn run(self: *HungServer) void {
        // Exactly one accept, then starve it — the rc client opens one
        // connection per call, and a second blocking `accept` would leave
        // `stop`'s join waiting on it forever (the same shape as rc_test's
        // hang fixture, for the same reason).
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

fn sleepMs(target_io: std.Io, ms: u32) void {
    const duration: std.Io.Clock.Duration = .{
        .raw = .fromMilliseconds(ms),
        .clock = .awake,
    };
    duration.sleep(target_io) catch {};
}

fn nowNs(target_io: std.Io) i96 {
    return std.Io.Clock.now(.awake, target_io).nanoseconds;
}

test "poll stays under a frame while the transport hangs and the run fails on deadline" {
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

    // Paired already, so the worker goes straight to the transport — which
    // never answers, and the per-POST deadline is what must end the run.
    try engine.savePairingState(gpa, io, game_dir, "hero", .{
        .paired = true,
        .last_success_unix = 1,
        .remote_fingerprint = "fp",
    });

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = hung.endpoint(),
        .deadline = .{ .connect_ms = 1_000, .read_ms = 1_000 },
    });
    defer w.destroy();

    const begin_started = nowNs(tio);
    try w.begin(.{
        .kind = .sync,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "fp",
    });
    // `begin` enqueues; it must not have waited on the hung socket.
    try std.testing.expect(nowNs(tio) - begin_started < frame_ns);

    // While one job is queued or running, a second is refused.
    try std.testing.expectError(error.Busy, w.begin(.{
        .kind = .sync,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "fp",
    }));

    // The main loop: poll at frame-ish cadence, measure every call, and
    // require the deadline to surface as `.failed` rather than a hang.
    var max_poll_ns: i96 = 0;
    var snapshot: worker.Snapshot = .{};
    var waited_ms: u32 = 0;
    const budget_ms: u32 = 30_000;
    while (waited_ms < budget_ms) {
        const before = nowNs(tio);
        snapshot = w.poll();
        const elapsed = nowNs(tio) - before;
        max_poll_ns = @max(max_poll_ns, elapsed);

        if (snapshot.state == .failed) break;
        sleepMs(tio, 4);
        waited_ms += 4;
    }

    try std.testing.expectEqual(worker.State.failed, snapshot.state);
    try std.testing.expectEqual(worker.Outcome.failed, snapshot.outcome);
    try std.testing.expect(snapshot.errorText().len != 0);
    // The packet's number: one frame at 60 Hz, with the transport wedged the
    // whole time.
    try std.testing.expect(max_poll_ns < frame_ns);
}

test "destroy during an in-flight run is bounded by the deadline" {
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

    try engine.savePairingState(gpa, io, game_dir, "hero", .{
        .paired = true,
        .last_success_unix = 1,
        .remote_fingerprint = "fp",
    });

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = hung.endpoint(),
        .deadline = .{ .connect_ms = 1_000, .read_ms = 1_000 },
    });

    try w.begin(.{
        .kind = .sync,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "fp",
    });

    // Let the worker get properly stuck in the POST before pulling the plug.
    sleepMs(tio, 200);

    // Shutdown while the transport is wedged: the cancel flag plus the
    // per-POST deadline bound the wait. Five seconds is generous — the
    // budget is one 1 s deadline plus the idle interval — and a destroy
    // that needs more than that is the hang this test exists to catch.
    const before = nowNs(tio);
    w.destroy();
    const elapsed = nowNs(tio) - before;
    try std.testing.expect(elapsed < 5_000_000_000);
}

// -- Live: the full path through begin/poll ----------------------------------

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

/// The test's stand-in for the ABI's discovery cache: hands the worker an
/// owned copy of a fixed path, which is the whole contract.
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

test "a worker pairs and syncs through begin and poll alone" {
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

    // The named remote the daemon will find: its config file exists before
    // the worker ever spawns it, exactly as the credentials packet will
    // arrange in production.
    const conf = try path.join(gpa, &.{ game_dir, "cloudsync", "rclone.conf" });
    defer gpa.free(conf);
    const conf_text = try std.fmt.allocPrint(
        gpa,
        "[bkremote]\ntype = alias\nremote = {s}\n",
        .{cloud},
    );
    defer gpa.free(conf_text);
    try fixture.write(conf, conf_text);

    var binary_slice: []const u8 = binary;
    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .binary_source = .{
            .context = @ptrCast(&binary_slice),
            .resolve = resolveFixedBinary,
        },
    });
    defer w.destroy();

    try std.testing.expectEqual(worker.State.idle, w.poll().state);

    // `begin` returns before the daemon exists: the spawn and readiness wait
    // happen on the worker and surface through `poll` as `.starting`.
    const begin_started = nowNs(tio);
    try w.begin(.{
        .kind = .pair,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "alias:cloud",
    });
    try std.testing.expect(nowNs(tio) - begin_started < frame_ns);

    const paired = pollUntilSettled(w, tio, 60_000);
    try std.testing.expectEqualStrings("", paired.errorText());
    try std.testing.expectEqual(worker.State.done, paired.state);
    try std.testing.expectEqual(worker.Outcome.paired, paired.outcome);

    const uploaded = try path.join(gpa, &.{ cloud, "profiles", "hero", "quick.sav" });
    defer gpa.free(uploaded);
    const uploaded_content = try std.Io.Dir.cwd().readFileAlloc(io, uploaded, gpa, .limited(4096));
    defer gpa.free(uploaded_content);
    try std.testing.expectEqualStrings("v1", uploaded_content);

    // Steady state through the same two calls, daemon reused.
    try fixture.write(save, "v2");
    try w.begin(.{
        .kind = .sync,
        .path1 = profile_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "alias:cloud",
    });

    const synced = pollUntilSettled(w, tio, 60_000);
    try std.testing.expectEqualStrings("", synced.errorText());
    try std.testing.expectEqual(worker.State.done, synced.state);
    try std.testing.expectEqual(worker.Outcome.synced, synced.outcome);

    const resynced = try std.Io.Dir.cwd().readFileAlloc(io, uploaded, gpa, .limited(4096));
    defer gpa.free(resynced);
    try std.testing.expectEqualStrings("v2", resynced);
}
