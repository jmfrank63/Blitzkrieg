//! Offline tests for the rclone rc JSON client.
//!
//! Nothing here touches the network or the rclone binary. Every case is served
//! by an in-process stub HTTP server bound to 127.0.0.1 on an ephemeral port,
//! replying with canned rc payloads. The hung-server case is the load-bearing
//! one: it proves the per-call deadline actually fires, which is the mechanism
//! the "the game never blocks forever on a socket" invariant rests on.

const std = @import("std");
const rc = @import("rc.zig");

const net = std.Io.net;

/// A canned HTTP server. It accepts one connection per scripted reply, reads
/// the request head (and body, so the client's write never gets an RST), then
/// writes the reply verbatim. In `hang` mode it accepts a connection and never
/// writes a single byte, which is what a wedged rclone daemon looks like.
const Stub = struct {
    io: std.Io,
    server: net.Server,
    port: u16,
    replies: []const []const u8,
    hang: bool,
    stop_flag: std.atomic.Value(bool),
    thread: ?std.Thread,
    stopped: bool,
    /// Request head and body of the first request, captured for assertions.
    /// Read only after `stop` has joined the server thread.
    head_buf: [4096]u8,
    head_len: usize,
    body_buf: [4096]u8,
    body_len: usize,
    captured: bool,

    fn start(self: *Stub, io: std.Io, replies: []const []const u8, hang: bool) !void {
        var addr: net.IpAddress = .{ .ip4 = .loopback(0) };
        self.* = .{
            .io = io,
            .server = try addr.listen(io, .{ .reuse_address = true }),
            .port = 0,
            .replies = replies,
            .hang = hang,
            .stop_flag = .init(false),
            .thread = null,
            .stopped = false,
            .head_buf = undefined,
            .head_len = 0,
            .body_buf = undefined,
            .body_len = 0,
            .captured = false,
        };
        self.port = self.server.socket.address.getPort();
        self.thread = try std.Thread.spawn(.{}, run, .{self});
    }

    /// Idempotent: tests call this explicitly before reading the captured
    /// request, and again from a `defer`.
    fn stop(self: *Stub) void {
        if (self.stopped) return;
        self.stopped = true;
        self.stop_flag.store(true, .release);
        if (self.thread) |t| {
            t.join();
            self.thread = null;
        }
        self.server.deinit(self.io);
    }

    fn head(self: *const Stub) []const u8 {
        return self.head_buf[0..self.head_len];
    }

    fn body(self: *const Stub) []const u8 {
        return self.body_buf[0..self.body_len];
    }

    fn run(self: *Stub) void {
        const io = self.io;
        if (self.hang) {
            var stream = self.server.accept(io) catch return;
            defer stream.close(io);
            while (!self.stop_flag.load(.acquire)) {
                const tick: std.Io.Clock.Duration = .{
                    .raw = .fromMilliseconds(10),
                    .clock = .awake,
                };
                tick.sleep(io) catch break;
            }
            return;
        }
        for (self.replies) |reply| {
            var stream = self.server.accept(io) catch return;
            defer stream.close(io);
            self.serveOne(stream, reply) catch {};
        }
    }

    fn serveOne(self: *Stub, stream: net.Stream, reply: []const u8) !void {
        const io = self.io;
        var read_buf: [8192]u8 = undefined;
        var stream_reader = stream.reader(io, &read_buf);
        const reader = &stream_reader.interface;

        const capture = !self.captured;
        var head_len: usize = 0;
        var content_len: usize = 0;
        while (true) {
            const line = reader.takeDelimiterInclusive('\n') catch break;
            if (capture and head_len + line.len <= self.head_buf.len) {
                @memcpy(self.head_buf[head_len..][0..line.len], line);
                head_len += line.len;
            }
            const trimmed = std.mem.trimEnd(u8, line, "\r\n");
            if (trimmed.len == 0) break;
            if (std.ascii.startsWithIgnoreCase(trimmed, "content-length:")) {
                const raw = std.mem.trim(u8, trimmed["content-length:".len..], " ");
                content_len = std.fmt.parseInt(usize, raw, 10) catch 0;
            }
        }

        var body_len: usize = 0;
        if (content_len > 0) {
            var scratch: [8192]u8 = undefined;
            const want = @min(content_len, scratch.len);
            reader.readSliceAll(scratch[0..want]) catch {};
            if (capture) {
                body_len = @min(want, self.body_buf.len);
                @memcpy(self.body_buf[0..body_len], scratch[0..body_len]);
            }
        }

        if (capture) {
            self.head_len = head_len;
            self.body_len = body_len;
            self.captured = true;
        }

        var write_buf: [8192]u8 = undefined;
        var stream_writer = stream.writer(io, &write_buf);
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

fn testClient(gpa: std.mem.Allocator, io: std.Io, port: u16) !rc.Client {
    return rc.Client.init(gpa, io, .{
        .host = "127.0.0.1",
        .port = port,
        .user = "u",
        .pass = "p",
    });
}

test "call parses a 200 rc reply and sends an authenticated JSON POST" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const replies = [_][]const u8{httpReply("HTTP/1.1 200 OK", "{\"version\":\"v1.75.0\"}")};
    var stub: Stub = undefined;
    try stub.start(io, &replies, false);
    defer stub.stop();

    var client = try testClient(gpa, io, stub.port);
    defer client.deinit();

    var reply = try client.call("core/version", .null);
    defer reply.deinit();

    try std.testing.expectEqualStrings("v1.75.0", reply.value.object.get("version").?.string);

    stub.stop();
    const request_head = stub.head();
    try std.testing.expect(std.mem.startsWith(u8, request_head, "POST /core/version HTTP/1.1\r\n"));
    // "u:p" base64-encodes to "dTpw".
    try std.testing.expect(std.mem.find(u8, request_head, "authorization: Basic dTpw\r\n") != null);
    try std.testing.expect(std.mem.find(u8, request_head, "content-type: application/json\r\n") != null);
}

test "an rc failure arrives as HTTP 500 and keeps its message" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const replies = [_][]const u8{httpReply(
        "HTTP/1.1 500 Internal Server Error",
        "{\"error\":\"bisync aborted\",\"status\":500}",
    )};
    var stub: Stub = undefined;
    try stub.start(io, &replies, false);
    defer stub.stop();

    var client = try testClient(gpa, io, stub.port);
    defer client.deinit();

    try std.testing.expectError(error.RcFailed, client.call("sync/bisync", .null));
    const failure = client.lastFailure().?;
    try std.testing.expectEqualStrings("bisync aborted", failure.message);
    try std.testing.expectEqual(@as(i64, 500), failure.status);
}

test "401 maps to Unauthorized" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const replies = [_][]const u8{httpReply(
        "HTTP/1.1 401 Unauthorized",
        "{\"error\":\"authentication failed\"}",
    )};
    var stub: Stub = undefined;
    try stub.start(io, &replies, false);
    defer stub.stop();

    var client = try testClient(gpa, io, stub.port);
    defer client.deinit();

    try std.testing.expectError(error.Unauthorized, client.call("core/version", .null));
}

test "malformed JSON maps to BadJson" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const replies = [_][]const u8{httpReply("HTTP/1.1 200 OK", "{\"version\": not json")};
    var stub: Stub = undefined;
    try stub.start(io, &replies, false);
    defer stub.stop();

    var client = try testClient(gpa, io, stub.port);
    defer client.deinit();

    try std.testing.expectError(error.BadJson, client.call("core/version", .null));
}

test "an async job is polled from unfinished to finished" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const io = threaded.io();

    const replies = [_][]const u8{
        httpReply("HTTP/1.1 200 OK", "{\"jobid\":42}"),
        httpReply("HTTP/1.1 200 OK", "{\"id\":42,\"finished\":false,\"success\":false}"),
        httpReply(
            "HTTP/1.1 200 OK",
            "{\"id\":42,\"finished\":true,\"success\":true," ++
                "\"output\":{\"output\":\"bisync log tail\"}}",
        ),
    };
    var stub: Stub = undefined;
    try stub.start(io, &replies, false);
    defer stub.stop();

    var client = try testClient(gpa, io, stub.port);
    defer client.deinit();

    var params: std.json.ObjectMap = .empty;
    defer params.deinit(gpa);
    try params.put(gpa, "path1", .{ .string = "p1" });

    const job = try client.callAsync("sync/bisync", .{ .object = params });
    try std.testing.expectEqual(@as(rc.JobId, 42), job);

    var pending = try client.jobStatus(job);
    defer pending.deinit();
    try std.testing.expect(!pending.finished);

    var done = try client.jobStatus(job);
    defer done.deinit();
    try std.testing.expect(done.finished);
    try std.testing.expect(done.success);
    try std.testing.expectEqualStrings("", done.error_text);
    // output.output is where a bisync abort explains itself.
    try std.testing.expectEqualStrings("bisync log tail", done.outputText().?);

    stub.stop();
    // The _async flag must be on the wire, not just in our heads.
    try std.testing.expect(std.mem.find(u8, stub.body(), "\"_async\":true") != null);
    try std.testing.expect(std.mem.startsWith(u8, stub.head(), "POST /sync/bisync HTTP/1.1\r\n"));
}

test "a server that accepts and never replies fails the call with Timeout" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const io = threaded.io();

    var stub: Stub = undefined;
    try stub.start(io, &[_][]const u8{}, true);
    defer stub.stop();

    var client = try testClient(gpa, io, stub.port);
    defer client.deinit();
    client.deadline = .{ .connect_ms = 500, .read_ms = 300 };

    const started = std.Io.Clock.awake.now(io);
    try std.testing.expectError(error.Timeout, client.call("core/version", .null));
    const elapsed_ms = started.durationTo(std.Io.Clock.awake.now(io)).toMilliseconds();

    // Nothing is printed on success: the build step treats any stderr output
    // from a test binary as a failure. The bounds are the evidence instead.
    // Lower bound: the call really waited for the deadline rather than failing
    // early for some other reason. Upper bound: it did not wait past it.
    try std.testing.expect(elapsed_ms >= 250);
    try std.testing.expect(elapsed_ms < 1500);
}
