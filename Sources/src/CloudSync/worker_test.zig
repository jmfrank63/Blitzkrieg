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
const catalogue = @import("catalogue.zig");
const creds = @import("creds.zig");
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

test "cancel interrupts a daemon that is still becoming ready" {
    // The readiness wait is fifteen seconds; a cancel or destroy landing
    // inside it must not sit the whole window out. The "daemon" is a script
    // that starts and never answers — POSIX only, a script is not an
    // executable image on Windows.
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    const never_ready = try path.join(gpa, &.{ fixture.root, "never-ready" });
    defer gpa.free(never_ready);
    try std.Io.Dir.cwd().writeFile(io, .{
        .sub_path = never_ready,
        .data = "#!/bin/sh\nsleep 60\n",
        .flags = .{ .permissions = .fromMode(0o755) },
    });

    var binary_slice: []const u8 = never_ready;
    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .binary_source = .{
            .context = @ptrCast(&binary_slice),
            .resolve = resolveFixedBinary,
        },
    });
    defer w.destroy();

    try w.begin(.{
        .kind = .pair,
        .path1 = game_dir,
        .remote = "bkremote",
        .profile = "hero",
        .profile_id = "hero-id",
        .remote_fingerprint = "print",
    });
    w.cancel();

    // Settled well inside the readiness window, and by abandonment — the
    // timeout path has its own distinct text.
    const before = nowNs(tio);
    const settled = pollUntilSettled(w, tio, 10_000);
    try std.testing.expect(nowNs(tio) - before < 10_000_000_000);
    try std.testing.expectEqual(worker.State.failed, settled.state);
    try std.testing.expectEqualStrings("Cancelled", settled.errorText());
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


// -- The catalogue job -------------------------------------------------------
//
// `ensureCatalogue` is a job, not a function call, because fetching means
// spawning a daemon and making an rc call. The cases below pin the three
// decisions it makes — cache hit, cache miss, version change — and the fourth
// thing the packet asks for: that an unavailable daemon is reported through
// the snapshot rather than thrown at the caller.

/// A canned rc server: one accept per scripted reply, request line recorded so
/// a test can prove which calls were made and how many.
const CannedServer = struct {
    io: std.Io,
    server: net.Server,
    port: u16,
    replies: []const []const u8,
    thread: ?std.Thread,
    stopped: bool,
    lines: [8][96]u8,
    line_lens: [8]usize,
    served: usize,
    /// When set, the reply at this index is withheld after its request has
    /// been read: `held` flips so the test knows the call is in flight,
    /// and the reply goes out only on `release_hold` (or `stop`). This is
    /// how a test opens a window *inside* one blocking rc request.
    hold_at: ?usize,
    held: std.atomic.Value(bool),
    release_flag: std.atomic.Value(bool),

    fn start(self: *CannedServer, target_io: std.Io, replies: []const []const u8) !void {
        var addr: net.IpAddress = .{ .ip4 = .loopback(0) };
        self.* = .{
            .io = target_io,
            .server = try addr.listen(target_io, .{ .reuse_address = true }),
            .port = 0,
            .replies = replies,
            .thread = null,
            .stopped = false,
            .lines = undefined,
            .line_lens = @splat(0),
            .served = 0,
            .hold_at = null,
            .held = .init(false),
            .release_flag = .init(false),
        };
        self.port = self.server.socket.address.getPort();
        self.thread = try std.Thread.spawn(.{}, run, .{self});
    }

    fn releaseHold(self: *CannedServer) void {
        self.release_flag.store(true, .release);
    }

    fn stop(self: *CannedServer) void {
        if (self.stopped) return;
        self.stopped = true;
        // A held reply would pin the join forever.
        self.release_flag.store(true, .release);
        // Unblock an `accept` still waiting for a call the test never made.
        // A scripted reply that went unused would otherwise pin this join
        // forever, turning any earlier assertion failure into a hang instead
        // of a report.
        var spare: usize = 0;
        while (spare < self.replies.len) : (spare += 1) {
            var addr: net.IpAddress = .{ .ip4 = .loopback(self.port) };
            if (addr.connect(self.io, .{ .mode = .stream })) |stream| {
                var opened = stream;
                opened.close(self.io);
            } else |_| {}
        }
        if (self.thread) |t| {
            t.join();
            self.thread = null;
        }
        self.server.deinit(self.io);
    }

    fn endpoint(self: *const CannedServer) rc.Endpoint {
        return .{ .host = "127.0.0.1", .port = self.port, .user = "u", .pass = "p" };
    }

    fn requestLine(self: *const CannedServer, index: usize) []const u8 {
        return self.lines[index][0..self.line_lens[index]];
    }

    fn run(self: *CannedServer) void {
        for (self.replies) |reply| {
            var stream = self.server.accept(self.io) catch return;
            defer stream.close(self.io);
            self.serveOne(stream, reply) catch {};
        }
    }

    fn serveOne(self: *CannedServer, stream: net.Stream, reply: []const u8) !void {
        var read_buf: [8192]u8 = undefined;
        var stream_reader = stream.reader(self.io, &read_buf);
        const reader = &stream_reader.interface;

        var first = true;
        var content_len: usize = 0;
        while (true) {
            const line = reader.takeDelimiterInclusive('\n') catch break;
            const trimmed = std.mem.trimEnd(u8, line, "\r\n");
            if (first) {
                first = false;
                const slot = self.served;
                if (slot < self.lines.len) {
                    const len = @min(trimmed.len, self.lines[slot].len);
                    @memcpy(self.lines[slot][0..len], trimmed[0..len]);
                    self.line_lens[slot] = len;
                }
            }
            if (trimmed.len == 0) break;
            if (std.ascii.startsWithIgnoreCase(trimmed, "content-length:")) {
                const raw = std.mem.trim(u8, trimmed["content-length:".len..], " ");
                content_len = std.fmt.parseInt(usize, raw, 10) catch 0;
            }
        }
        self.served += 1;

        var scratch: [8192]u8 = undefined;
        var remaining = content_len;
        while (remaining > 0) {
            const want = @min(remaining, scratch.len);
            reader.readSliceAll(scratch[0..want]) catch break;
            remaining -= want;
        }

        // The request is fully read; hold the reply open if scripted so
        // the test can act inside this call's window.
        if (self.hold_at != null and self.hold_at.? == self.served - 1) {
            self.held.store(true, .release);
            while (!self.release_flag.load(.acquire)) {
                const tick: std.Io.Clock.Duration = .{
                    .raw = .fromMilliseconds(10),
                    .clock = .awake,
                };
                tick.sleep(self.io) catch break;
            }
        }

        var write_buf: [8192]u8 = undefined;
        var stream_writer = stream.writer(self.io, &write_buf);
        try stream_writer.interface.writeAll(reply);
        try stream_writer.interface.flush();
    }
};

fn httpReply(comptime status_line: []const u8, comptime json_body: []const u8) []const u8 {
    return status_line ++ "\r\n" ++
        "content-type: application/json\r\n" ++
        std.fmt.comptimePrint("content-length: {d}\r\n", .{json_body.len}) ++
        "connection: close\r\n\r\n" ++
        json_body;
}

const v1_75_0: daemon.Version = .{ .major = 1, .minor = 75, .patch = 0 };
const v1_74_2: daemon.Version = .{ .major = 1, .minor = 74, .patch = 2 };

/// A two-backend catalogue: enough to prove the document survived the round
/// trip, small enough to serve from a canned reply.
const tiny_catalogue =
    "{\"providers\":[" ++
    "{\"Name\":\"alpha\",\"Description\":\"A\",\"Prefix\":\"alpha\",\"Options\":[{\"Name\":\"one\",\"Type\":\"string\"}]}," ++
    "{\"Name\":\"beta\",\"Description\":\"B\",\"Prefix\":\"beta\",\"Options\":[]}" ++
    "]}";

fn writeCache(
    gpa: std.mem.Allocator,
    game_dir: []const u8,
    body: []const u8,
    version: daemon.Version,
) !void {
    const document = try catalogue.stampDocument(gpa, body, version);
    defer gpa.free(document);
    try catalogue.cache(gpa, io, game_dir, document);
}

test "a version-matched cache answers ensureCatalogue without enqueueing a job" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    try writeCache(gpa, game_dir, tiny_catalogue, v1_75_0);

    // No endpoint and no binary source at all: if this enqueued anything, the
    // job would fail for want of a daemon, and the state would not stay idle.
    var w = try worker.Worker.create(gpa, tio, .{ .game_dir = game_dir });
    defer w.destroy();

    try std.testing.expectEqual(worker.CatalogueState.cached, try w.ensureCatalogue(v1_75_0));

    sleepMs(tio, 100);
    try std.testing.expectEqual(worker.State.idle, w.poll().state);
    try std.testing.expectEqual(worker.Outcome.none, w.poll().outcome);
}

test "a version change enqueues a refetch even though a cache exists" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    try writeCache(gpa, game_dir, tiny_catalogue, v1_74_2);

    const replies = [_][]const u8{
        httpReply("HTTP/1.1 200 OK", "{\"version\":\"v1.75.0\"}"),
        httpReply("HTTP/1.1 200 OK", tiny_catalogue),
    };
    var stub: CannedServer = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = stub.endpoint(),
        .deadline = .{ .connect_ms = 2_000, .read_ms = 2_000 },
    });
    defer w.destroy();

    try std.testing.expectEqual(worker.CatalogueState.fetching, try w.ensureCatalogue(v1_75_0));

    const settled = pollUntilSettled(w, tio, 30_000);
    try std.testing.expectEqualStrings("", settled.errorText());
    try std.testing.expectEqual(worker.State.done, settled.state);
    try std.testing.expectEqual(worker.Outcome.catalogue_ready, settled.outcome);

    stub.stop();
    // Exactly the two calls the refresh needs, and no credentials work: a
    // catalogue fetch must not depend on a configured remote.
    try std.testing.expectEqual(@as(usize, 2), stub.served);
    try std.testing.expect(std.mem.startsWith(u8, stub.requestLine(0), "POST /core/version "));
    try std.testing.expect(std.mem.startsWith(u8, stub.requestLine(1), "POST /config/providers "));

    // The cache now describes the running binary, so the next ask is free.
    var reloaded = try catalogue.loadCached(gpa, io, game_dir);
    defer reloaded.deinit();
    try std.testing.expectEqual(@as(usize, 2), reloaded.backends.len);
    try std.testing.expect(reloaded.rclone_version != null);
    try std.testing.expectEqual(
        std.math.Order.eq,
        reloaded.rclone_version.?.order(v1_75_0),
    );
    try std.testing.expectEqual(worker.CatalogueState.cached, try w.ensureCatalogue(v1_75_0));
}

test "a missing cache fetches once, and the second ask is answered from disk" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    const replies = [_][]const u8{
        httpReply("HTTP/1.1 200 OK", "{\"version\":\"v1.75.0\"}"),
        httpReply("HTTP/1.1 200 OK", tiny_catalogue),
    };
    var stub: CannedServer = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = stub.endpoint(),
        .deadline = .{ .connect_ms = 2_000, .read_ms = 2_000 },
    });
    defer w.destroy();

    // Nothing cached yet — the fresh-install case the whole bootstrap exists
    // for. `ensureCatalogue` must not have waited on the socket to say so.
    const asked = nowNs(tio);
    try std.testing.expectEqual(worker.CatalogueState.fetching, try w.ensureCatalogue(v1_75_0));
    try std.testing.expect(nowNs(tio) - asked < frame_ns);

    const settled = pollUntilSettled(w, tio, 30_000);
    try std.testing.expectEqual(worker.State.done, settled.state);
    try std.testing.expectEqual(worker.Outcome.catalogue_ready, settled.outcome);

    stub.stop();
    try std.testing.expectEqual(@as(usize, 2), stub.served);

    var loaded = try catalogue.loadCached(gpa, io, game_dir);
    defer loaded.deinit();
    const alpha = loaded.backend("alpha") orelse return error.MissingBackend;
    try std.testing.expectEqual(@as(usize, 1), alpha.options.len);

    // The stub has no replies left, so a second fetch would fail the job.
    // It is answered from the cache instead.
    try std.testing.expectEqual(worker.CatalogueState.cached, try w.ensureCatalogue(v1_75_0));
}

test "the full failure detail survives the snapshot's truncation" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    var w = try worker.Worker.create(gpa, tio, .{ .game_dir = game_dir });
    defer w.destroy();

    // Nothing failed yet: no detail.
    try std.testing.expect((try w.errorDetailOwned(gpa)) == null);

    // A failure text longer than the snapshot's one-line budget — the
    // engine's redacted 200-line support tail is exactly this shape. The
    // snapshot truncates for the status line; the detail must not.
    var long: [2000]u8 = undefined;
    @memset(&long, 'x');
    const marker = "the-final-line-of-the-tail";
    @memcpy(long[long.len - marker.len ..], marker);
    w.publishFailureText(&long);

    const snap = w.poll();
    try std.testing.expectEqual(worker.error_text_max, snap.errorText().len);

    const detail = (try w.errorDetailOwned(gpa)).?;
    defer gpa.free(detail);
    try std.testing.expectEqual(long.len, detail.len);
    try std.testing.expect(std.mem.endsWith(u8, detail, marker));
}

test "an unavailable daemon is reported through the snapshot, not thrown" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    // No endpoint, no binary: there is no rclone to ask.
    var w = try worker.Worker.create(gpa, tio, .{ .game_dir = game_dir });
    defer w.destroy();

    try std.testing.expectEqual(worker.CatalogueState.fetching, try w.ensureCatalogue(v1_75_0));

    const settled = pollUntilSettled(w, tio, 10_000);
    try std.testing.expectEqual(worker.State.failed, settled.state);
    try std.testing.expectEqual(worker.Outcome.failed, settled.outcome);
    try std.testing.expect(settled.errorText().len != 0);

    // A failed fetch is not an error state: no cache was written, the list is
    // simply still empty, and the worker is ready for the retry.
    var loaded = try catalogue.loadCached(gpa, io, game_dir);
    defer loaded.deinit();
    try std.testing.expect(loaded.isEmpty());
}

test "a catalogue fetch is cancellable like any other job" {
    const gpa = std.testing.allocator;

    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    // A listening socket that answers nothing: the worker either sees the
    // cancel before it reaches the transport, or gets there and is bounded by
    // its own POST deadline. Both are the same promise — the fetch ends.
    var stub: CannedServer = undefined;
    try stub.start(tio, &.{});
    defer stub.stop();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = stub.endpoint(),
        .deadline = .{ .connect_ms = 1_000, .read_ms = 1_000 },
    });
    defer w.destroy();

    try std.testing.expectEqual(worker.CatalogueState.fetching, try w.ensureCatalogue(v1_75_0));

    // The player closes the dialog again. The job must end rather than strand
    // the worker, and it must end without a cache.
    w.cancel();

    const settled = pollUntilSettled(w, tio, 10_000);
    try std.testing.expectEqual(worker.State.failed, settled.state);
    try std.testing.expectEqual(worker.Outcome.failed, settled.outcome);
    try std.testing.expect(settled.errorText().len != 0);

    var loaded = try catalogue.loadCached(gpa, io, game_dir);
    defer loaded.deinit();
    try std.testing.expect(loaded.isEmpty());

    // The worker took the cancellation, not a wound: the next job is accepted.
    try std.testing.expectEqual(worker.CatalogueState.fetching, try w.ensureCatalogue(v1_75_0));
}

// -- Token read-back ----------------------------------------------------------

fn ok200(comptime json_body: []const u8) []const u8 {
    return httpReply("HTTP/1.1 200 OK", json_body);
}

const readback_seed_doc =
    \\{"backend":"drive","remote_root":"","fingerprint":"drive:#seed",
    \\"options":{"pass":"hunter2","token":"OLD-TOKEN"},
    \\"secret_options":["pass","token"],"password_options":["pass"],
    \\"rclone_path":null}
;

fn readDoc(gpa: std.mem.Allocator, game_dir: []const u8) !creds.Loaded {
    const at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(at);
    return (try creds.load(gpa, io, at)).?;
}

test "a refreshed token is read back after the job and once more at teardown" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const creds_at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(creds_at);
    try fixture.write(creds_at, readback_seed_doc);
    // No catalogue cache on purpose: the packet's cache-deleted case —
    // classification falls back to secret-never-password.

    // In request order: applyCredentials (two creates), the connection
    // test (list, probe up, probe down, probe delete — the stub cannot
    // host the probe's local read-back, so the job itself settles
    // unwritable, which is irrelevant here: read-back runs after failed
    // jobs too, because the refresh precedes the failure), then
    // config/get after the job and once more at teardown.
    const replies = [_][]const u8{
        ok200("{}"), // config/create bkraw
        ok200("{}"), // config/create bkremote
        ok200("{}"), // operations/list
        ok200("{}"), // probe copy up
        ok200("{}"), // probe copy down
        ok200("{}"), // probe delete
        ok200(
            \\{"type":"drive","pass":"OBSCURED-BY-RCLONE","token":"NEW-TOKEN","team_drive":"td-1"}
        ), // config/get after the job
        ok200(
            \\{"type":"drive","pass":"OBSCURED-BY-RCLONE","token":"NEWER-TOKEN-2","team_drive":"td-1"}
        ), // config/get at teardown
    };
    var server: CannedServer = undefined;
    try server.start(tio, &replies);
    defer server.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = server.endpoint(),
    });

    try w.begin(.{ .kind = .test_connection, .remote = creds.sync_remote_name });

    // The read-back lands after the snapshot settles; poll the file.
    var updated = false;
    var waited: u32 = 0;
    while (waited < 10_000) : (waited += 50) {
        var doc = readDoc(gpa, game_dir) catch {
            sleepMs(tio, 50);
            continue;
        };
        defer doc.deinit();
        if (std.mem.eql(u8, doc.creds.option("token").?.value, "NEW-TOKEN")) {
            updated = true;
            // The password survives byte-for-byte with its flags — rclone
            // returned it obscured, and the obscured form must never
            // replace the plaintext.
            const pass = doc.creds.option("pass").?;
            try std.testing.expectEqualStrings("hunter2", pass.value);
            try std.testing.expect(pass.is_password and pass.secret);
            // The machine-written field arrived, secret by the safe
            // default — there is no catalogue cache to say otherwise.
            const imported = doc.creds.option("team_drive").?;
            try std.testing.expectEqualStrings("td-1", imported.value);
            try std.testing.expect(imported.secret and !imported.is_password);
            // Secrets are outside the identity: no rotation.
            try std.testing.expectEqualStrings("drive:#seed", doc.creds.fingerprint);
            break;
        }
        sleepMs(tio, 50);
    }
    try std.testing.expect(updated);

    // destroy tears the session down — and the last read-back must run
    // before the client goes, or a token refreshed during the final job
    // is lost. The teardown config/get answers a newer token still.
    w.destroy();
    var final = try readDoc(gpa, game_dir);
    defer final.deinit();
    try std.testing.expectEqualStrings("NEWER-TOKEN-2", final.creds.option("token").?.value);
    try std.testing.expectEqualStrings("hunter2", final.creds.option("pass").?.value);
}

test "an unrefreshable token reports auth_failed and stays out of logs and error text" {
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

    // A token provider that refuses every refresh the way a revoked
    // grant does — the captured invalid_grant shape.
    const refused = httpReply(
        "HTTP/1.1 400 Bad Request",
        "{\"error\":\"invalid_grant\",\"error_description\":\"Token has been revoked.\"}",
    );
    const deny = [_][]const u8{ refused, refused, refused, refused };
    var provider: CannedServer = undefined;
    try provider.start(tio, &deny);
    defer provider.stop();

    const doc = try std.fmt.allocPrint(gpa,
        \\{{"backend":"drive","remote_root":"","fingerprint":"drive:#x",
        \\"options":{{"client_id":"fake-id","client_secret":"fake-secret",
        \\"token_url":"http://127.0.0.1:{d}/token",
        \\"token":"{{\"access_token\":\"expired-token-value\",\"token_type\":\"Bearer\",\"refresh_token\":\"dead-refresh-value\",\"expiry\":\"2020-01-01T00:00:00Z\"}}"}},
        \\"secret_options":["client_secret","token"],"password_options":[],
        \\"rclone_path":null}}
    , .{provider.port});
    defer gpa.free(doc);
    const creds_at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(creds_at);
    try fixture.write(creds_at, doc);

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = d.endpoint(),
    });
    defer w.destroy();

    try w.begin(.{ .kind = .test_connection, .remote = creds.sync_remote_name });
    var settled: ?worker.Snapshot = null;
    var waited: u32 = 0;
    while (waited < 30_000) : (waited += 50) {
        const snap = w.poll();
        if (snap.state == .failed or snap.state == .done) {
            settled = snap;
            break;
        }
        sleepMs(tio, 50);
    }
    try std.testing.expect(settled != null);

    // The unrefreshable token is an authentication failure — the outcome
    // that routes the player to the credentials dialog — and neither the
    // token nor the refresh secret appears in the text.
    const text = settled.?.errorText();
    try std.testing.expect(std.mem.startsWith(u8, text, "auth_failed"));
    try std.testing.expect(std.mem.indexOf(u8, text, "expired-token-value") == null);
    try std.testing.expect(std.mem.indexOf(u8, text, "dead-refresh-value") == null);

    // Nor the daemon's own log: the token lives in cloud.credentials and
    // rclone's config, nowhere else.
    const log_at = try path.join(gpa, &.{ game_dir, "cloudsync", "rcd.log" });
    defer gpa.free(log_at);
    const log_text = std.Io.Dir.cwd().readFileAlloc(io, log_at, gpa, .limited(1 << 20)) catch
        try gpa.dupe(u8, "");
    defer gpa.free(log_text);
    try std.testing.expect(std.mem.indexOf(u8, log_text, "expired-token-value") == null);
    try std.testing.expect(std.mem.indexOf(u8, log_text, "dead-refresh-value") == null);
}

test "a save during the job wins over the read-back" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const creds_at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(creds_at);
    try fixture.write(creds_at, readback_seed_doc);

    // A config job parks on a question — the deterministic moment a
    // player can save new credentials while the job is in flight. The
    // read-back must then step aside: merging the old session's section
    // over the new document would restore the old account's token over
    // the one the player just chose.
    const players_new_doc =
        \\{"backend":"drive","remote_root":"","fingerprint":"drive:#new",
        \\"options":{"token":"PLAYERS-NEW-TOKEN"},
        \\"secret_options":["token"],"password_options":[],
        \\"rclone_path":null}
    ;
    const replies = [_][]const u8{
        ok200("{}"), // config/create bkraw
        ok200("{}"), // config/create bkremote
        ok200("{\"jobid\":1}"), // async opener
        ok200(
            \\{"finished":true,"success":true,"error":"","output":{"State":"s1","Option":{"Name":"q1","Type":"string"},"Error":"","Result":""}}
        ),
        ok200("{\"jobid\":2}"), // async continuation
        ok200(
            \\{"finished":true,"success":true,"error":"","output":{"State":"","Option":null,"Error":"","Result":""}}
        ),
        httpReply("HTTP/1.1 500 Internal Server Error",
            \\{"error":"couldn't list files: 401 Unauthorized","status":500}
        ), // completion's connection test
        ok200(
            \\{"type":"drive","token":"STOLEN-MERGE"}
        ), // config/get — must never be consumed
    };
    var server: CannedServer = undefined;
    try server.start(tio, &replies);
    defer server.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = server.endpoint(),
    });

    try w.begin(.{ .kind = .config_create });

    var waited: u32 = 0;
    while (waited < 10_000) : (waited += 25) {
        if (w.poll().state == .awaiting_input) break;
        sleepMs(tio, 25);
    }
    try std.testing.expectEqual(worker.State.awaiting_input, w.poll().state);

    // The player saves while the machine waits.
    try fixture.write(creds_at, players_new_doc);
    try std.testing.expect(w.answerConfig("x"));

    waited = 0;
    while (waited < 10_000) : (waited += 25) {
        const snap = w.poll();
        if (snap.state == .failed or snap.state == .done) break;
        sleepMs(tio, 25);
    }

    // Give the deferred read-back its moment, then the teardown one too.
    sleepMs(tio, 300);
    w.destroy();

    var doc = try readDoc(gpa, game_dir);
    defer doc.deinit();
    try std.testing.expectEqualStrings("PLAYERS-NEW-TOKEN", doc.creds.option("token").?.value);
    try std.testing.expectEqualStrings("drive:#new", doc.creds.fingerprint);
}

test "a section of the wrong backend is never merged" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const creds_at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(creds_at);
    try fixture.write(creds_at, readback_seed_doc);

    // The daemon still holds a previous backend's section — a provider
    // switch mid-session. Its fields belong to that backend and must not
    // be imported into this document.
    const replies = [_][]const u8{
        ok200("{}"), // config/create bkraw
        ok200("{}"), // config/create bkremote
        ok200("{}"), // operations/list
        ok200("{}"), // probe copy up
        ok200("{}"), // probe copy down
        ok200("{}"), // probe delete
        ok200(
            \\{"type":"s3","token":"WRONG-BACKEND","secret_access_key":"LEFTOVER"}
        ), // config/get after the job: not our backend
        ok200(
            \\{"type":"s3","token":"WRONG-BACKEND","secret_access_key":"LEFTOVER"}
        ), // and at teardown
    };
    var server: CannedServer = undefined;
    try server.start(tio, &replies);
    defer server.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = server.endpoint(),
    });

    try w.begin(.{ .kind = .test_connection, .remote = creds.sync_remote_name });

    var waited: u32 = 0;
    while (waited < 10_000) : (waited += 25) {
        const snap = w.poll();
        if (snap.state == .failed or snap.state == .done) break;
        sleepMs(tio, 25);
    }
    sleepMs(tio, 300);
    w.destroy();

    var doc = try readDoc(gpa, game_dir);
    defer doc.deinit();
    try std.testing.expectEqualStrings("OLD-TOKEN", doc.creds.option("token").?.value);
    try std.testing.expect(doc.creds.option("secret_access_key") == null);
}

test "a save during the read-back's own request still wins" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const creds_at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(creds_at);
    try fixture.write(creds_at, readback_seed_doc);

    // The TOCTOU window the baseline check alone cannot close: the save
    // lands after the read-back's document comparison but before its
    // merge is published — inside the blocking config/get itself. The
    // held reply is that window, made deterministic.
    const replies = [_][]const u8{
        ok200("{}"), // config/create bkraw
        ok200("{}"), // config/create bkremote
        ok200("{}"), // operations/list
        ok200("{}"), // probe copy up
        ok200("{}"), // probe copy down
        ok200("{}"), // probe delete
        ok200(
            \\{"type":"drive","token":"STOLEN-LATE"}
        ), // config/get, held open while the player saves
    };
    var server: CannedServer = undefined;
    try server.start(tio, &replies);
    defer server.stop();
    server.hold_at = 6;

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = server.endpoint(),
    });

    try w.begin(.{ .kind = .test_connection, .remote = creds.sync_remote_name });

    var waited: u32 = 0;
    while (waited < 10_000) : (waited += 25) {
        if (server.held.load(.acquire)) break;
        sleepMs(tio, 25);
    }
    try std.testing.expect(server.held.load(.acquire));

    // The player saves while config/get is in flight.
    const players_late_doc =
        \\{"backend":"drive","remote_root":"","fingerprint":"drive:#late",
        \\"options":{"token":"PLAYERS-LATE-TOKEN"},
        \\"secret_options":["token"],"password_options":[],
        \\"rclone_path":null}
    ;
    try fixture.write(creds_at, players_late_doc);
    server.releaseHold();

    waited = 0;
    while (waited < 10_000) : (waited += 25) {
        const snap = w.poll();
        if (snap.state == .failed or snap.state == .done) break;
        sleepMs(tio, 25);
    }
    sleepMs(tio, 300);
    w.destroy();

    var doc = try readDoc(gpa, game_dir);
    defer doc.deinit();
    try std.testing.expectEqualStrings("PLAYERS-LATE-TOKEN", doc.creds.option("token").?.value);
    try std.testing.expectEqualStrings("drive:#late", doc.creds.fingerprint);
}

test "the applied baseline is captured under the document lock" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const creds_at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(creds_at);
    try fixture.write(creds_at, readback_seed_doc);

    const replies = [_][]const u8{
        ok200("{}"), // config/create bkraw
        ok200("{}"), // config/create bkremote
        ok200("{}"), // operations/list
        ok200("{}"), // probe copy up
        ok200("{}"), // probe copy down
        ok200("{}"), // probe delete
        ok200(
            \\{"type":"drive","token":"READBACK-TOKEN"}
        ), // config/get after the job
        ok200(
            \\{"type":"drive","token":"READBACK-TOKEN"}
        ), // and at teardown
    };
    var server: CannedServer = undefined;
    try server.start(tio, &replies);
    defer server.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = server.endpoint(),
    });

    // Hold the document lock the way a dialog save does. The job's
    // credential capture must WAIT on it — parse and baseline are one
    // atomic read — so no rc call may happen while the lock is held.
    // (Judged without `try` until the lock is released and the worker
    // destroyed: an early unwind here would strand the worker on the
    // mutex and hang the whole suite instead of reporting.)
    creds.document_mutex.lockUncancelable(tio);
    try w.begin(.{ .kind = .test_connection, .remote = creds.sync_remote_name });
    sleepMs(tio, 600);
    const blocked_before_any_call = server.served == 0;

    // The save completes before the capture, strictly ordered by the
    // lock: the job must configure from THIS document and read back into
    // it.
    const players_doc =
        \\{"backend":"drive","remote_root":"","fingerprint":"drive:#new2",
        \\"options":{"token":"PLAYERS-TOKEN-3"},
        \\"secret_options":["token"],"password_options":[],
        \\"rclone_path":null}
    ;
    try fixture.write(creds_at, players_doc);
    creds.document_mutex.unlock(tio);

    var waited: u32 = 0;
    while (waited < 10_000) : (waited += 25) {
        const snap = w.poll();
        if (snap.state == .failed or snap.state == .done) break;
        sleepMs(tio, 25);
    }
    sleepMs(tio, 300);
    w.destroy();

    // Everything released and torn down: now judge. The capture must
    // have waited for the lock, and the baseline then matched the
    // document the daemon was configured from, so the read-back merged
    // into the player's document — token updated, identity intact.
    try std.testing.expect(blocked_before_any_call);
    var doc = try readDoc(gpa, game_dir);
    defer doc.deinit();
    try std.testing.expectEqualStrings("READBACK-TOKEN", doc.creds.option("token").?.value);
    try std.testing.expectEqualStrings("drive:#new2", doc.creds.fingerprint);
}

test "a run finishing after an identity rotation cannot resurrect the retired pairing" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    const creds_at = try path.join(gpa, &.{ game_dir, creds.default_path });
    defer gpa.free(creds_at);
    try fixture.write(creds_at, readback_seed_doc);

    // A config job parked on its question is the deterministic stand-in
    // for any long-running job. While it waits: the player saves a new
    // identity (whose save-path retirement finds nothing yet), and THEN
    // the old run's success record lands — the resurrection this test
    // exists to kill. The worker's post-job pass must notice the document
    // changed under the job and retire the stale record again.
    const replies = [_][]const u8{
        ok200("{}"), // config/create bkraw
        ok200("{}"), // config/create bkremote
        ok200("{\"jobid\":1}"), // async opener
        ok200(
            \\{"finished":true,"success":true,"error":"","output":{"State":"s1","Option":{"Name":"q1","Type":"string"},"Error":"","Result":""}}
        ),
        ok200("{\"jobid\":2}"), // async continuation
        ok200(
            \\{"finished":true,"success":true,"error":"","output":{"State":"","Option":null,"Error":"","Result":""}}
        ),
        httpReply("HTTP/1.1 500 Internal Server Error",
            \\{"error":"couldn't list files: 401 Unauthorized","status":500}
        ), // completion's connection test
    };
    var server: CannedServer = undefined;
    try server.start(tio, &replies);
    defer server.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = server.endpoint(),
    });

    try w.begin(.{ .kind = .config_create });
    var waited: u32 = 0;
    while (waited < 10_000) : (waited += 25) {
        if (w.poll().state == .awaiting_input) break;
        sleepMs(tio, 25);
    }
    try std.testing.expectEqual(worker.State.awaiting_input, w.poll().state);

    // The rotation: the player saves a new identity mid-job. The save
    // path retires records naming other remotes — there are none yet.
    const rotated_doc =
        \\{"backend":"webdav","remote_root":"","fingerprint":"webdav:#rotated",
        \\"options":{"url":"http://127.0.0.1:9"},
        \\"secret_options":[],"password_options":[],
        \\"rclone_path":null}
    ;
    try fixture.write(creds_at, rotated_doc);
    _ = engine.retireMismatchedPairings(gpa, io, game_dir, "webdav:#rotated");

    // And the in-flight run's success record lands AFTER it, still naming
    // the identity the run started with.
    try engine.savePairingState(gpa, io, game_dir, "hero", .{
        .paired = true,
        .last_success_unix = 100,
        .remote_fingerprint = "drive:#seed",
    });

    try std.testing.expect(w.answerConfig("x"));
    waited = 0;
    while (waited < 10_000) : (waited += 25) {
        const snap = w.poll();
        if (snap.state == .failed or snap.state == .done) break;
        sleepMs(tio, 25);
    }
    sleepMs(tio, 300);
    w.destroy();

    // The stale record is gone: the next sync takes the NotPaired -> pair
    // bootstrap instead of refusing FingerprintChanged until the player
    // saves a second time.
    if (engine.loadPairingState(gpa, io, game_dir, "hero")) |stale| {
        var owned = stale;
        owned.deinit();
        return error.TestUnexpectedResult;
    }
    // And the rotated document itself was not touched by any read-back.
    var doc = try readDoc(gpa, game_dir);
    defer doc.deinit();
    try std.testing.expectEqualStrings("webdav:#rotated", doc.creds.fingerprint);
}
