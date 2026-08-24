//! Tests for the rc config state machine driver.
//!
//! The replies below are captured, not invented: rclone v1.75.0 (the staged
//! binary), `config/create` for `drive` through a scratch `rcd`, captured
//! 2026-08-25. The done shape is the same daemon completing a webdav create
//! in one step, and the in-band error shape is its answer to a garbage
//! token. The stub asserts the *whole repeated envelope* of every request —
//! a continuation that drops `parameters`, or posts capitalised opt keys,
//! must fail here rather than pass and break against a real service.
//!
//! The live test at the bottom walks the real drive machine to the token
//! question through a real daemon; it never completes the OAuth dance (that
//! needs a browser and a human) and abandons the flow, which is also the
//! path a player's Cancel takes.

const std = @import("std");
const builtin = @import("builtin");
const rc = @import("rc.zig");
const oauth = @import("oauth.zig");
const worker = @import("worker.zig");
const daemon = @import("daemon.zig");

const net = std.Io.net;
const io = std.testing.io;
const path = std.Io.Dir.path;

// -- Captured replies ---------------------------------------------------------

const reply_client_id_warning =
    \\{"Error":"","Option":{"Advanced":false,"Default":false,"DefaultStr":"false","Examples":[{"Help":"Yes","Value":"true"},{"Help":"No","Value":"false"}],"Exclusive":true,"FieldName":"","Help":"rclone's shared Google Drive client_id is being retired and will stop working during 2026.\nCreate your own to avoid interruption: https://rclone.org/drive/#making-your-own-client-id\n\nContinue using the shared client_id anyway?","Hide":0,"IsPassword":false,"Name":"config_shared_client_id","NoPrefix":false,"Required":false,"Sensitive":false,"Type":"bool","Value":null,"ValueStr":"false"},"Result":"","State":"client_id_warning"}
;

const reply_islocal =
    \\{"Error":"","Option":{"Advanced":false,"Default":true,"DefaultStr":"true","Examples":[{"Help":"Yes","Value":"true"},{"Help":"No","Value":"false"}],"Exclusive":true,"FieldName":"","Help":"Use web browser to automatically authenticate rclone with remote?\n * Say Y if the machine running rclone has a web browser you can use\n * Say N if running rclone on a (remote) machine without web browser access\nIf not sure try Y. If Y failed, try N.\n","Hide":0,"IsPassword":false,"Name":"config_is_local","NoPrefix":false,"Required":false,"Sensitive":false,"Type":"bool","Value":null,"ValueStr":"true"},"Result":"","State":"*oauth-islocal,teamdrive,oauth,"}
;

const reply_authorize =
    \\{"Error":"","Option":{"Advanced":false,"Default":"","DefaultStr":"","Exclusive":false,"FieldName":"","Help":"For this to work, you will need rclone available on a machine that has\na web browser available.\n\nFor more help and alternate methods see: https://rclone.org/remote_setup/\n\nExecute the following on the machine with the web browser (same rclone\nversion recommended):\n\n\trclone authorize \"drive\"\n\nThen paste the result.\n","Hide":0,"IsPassword":false,"Name":"config_token","NoPrefix":false,"Required":true,"Sensitive":false,"Type":"string","Value":null,"ValueStr":""},"Result":"","State":"*oauth-authorize,teamdrive,oauth,"}
;

const reply_authorize_error =
    \\{"Error":"Couldn't decode response - try again (make sure you are using a matching version of rclone on both sides: invalid character 'g' looking for beginning of value\n","Option":{"Advanced":false,"Default":"","DefaultStr":"","Exclusive":false,"FieldName":"","Help":"For this to work, you will need rclone available on a machine that has\na web browser available.\n\nThen paste the result.\n","Hide":0,"IsPassword":false,"Name":"config_token","NoPrefix":false,"Required":true,"Sensitive":false,"Type":"string","Value":null,"ValueStr":""},"Result":"","State":"*oauth-authorize,teamdrive,oauth,"}
;

const reply_done =
    \\{"Error":"","Option":null,"Result":"","State":""}
;

const reply_ok_empty =
    \\{}
;

/// The engine's captured auth shape, as an rc-level failure: what the
/// completion path's connection test meets when the freshly configured
/// remote still refuses.
const reply_list_auth_error =
    \\{"error":"couldn't list files: 401 Unauthorized: 401 Unauthorized","status":500}
;

fn httpReply(comptime status_line: []const u8, comptime json_body: []const u8) []const u8 {
    return status_line ++ "\r\n" ++
        "content-type: application/json\r\n" ++
        std.fmt.comptimePrint("content-length: {d}\r\n", .{json_body.len}) ++
        "connection: close\r\n\r\n" ++
        json_body;
}

fn ok(comptime json_body: []const u8) []const u8 {
    return httpReply("HTTP/1.1 200 OK", json_body);
}

// -- The stub -----------------------------------------------------------------

/// rc_test's canned server, widened: every request body is captured, not
/// only the first, because the envelope contract under test is precisely
/// "the continuations repeat everything".
const Stub = struct {
    const max_requests = 12;

    io: std.Io,
    server: net.Server,
    port: u16,
    replies: []const []const u8,
    stop_flag: std.atomic.Value(bool),
    thread: ?std.Thread,
    stopped: bool,
    bodies_buf: [max_requests][4096]u8,
    bodies_len: [max_requests]usize,
    count: usize,

    fn start(self: *Stub, target_io: std.Io, replies: []const []const u8) !void {
        var addr: net.IpAddress = .{ .ip4 = .loopback(0) };
        self.* = .{
            .io = target_io,
            .server = try addr.listen(target_io, .{ .reuse_address = true }),
            .port = 0,
            .replies = replies,
            .stop_flag = .init(false),
            .thread = null,
            .stopped = false,
            .bodies_buf = undefined,
            .bodies_len = .{0} ** max_requests,
            .count = 0,
        };
        self.port = self.server.socket.address.getPort();
        self.thread = try std.Thread.spawn(.{}, run, .{self});
    }

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

    fn endpoint(self: *const Stub) rc.Endpoint {
        return .{ .host = "127.0.0.1", .port = self.port, .user = "u", .pass = "p" };
    }

    /// Read only after `stop` has joined the thread.
    fn body(self: *const Stub, index: usize) []const u8 {
        return self.bodies_buf[index][0..self.bodies_len[index]];
    }

    fn run(self: *Stub) void {
        for (self.replies) |reply| {
            var stream = self.server.accept(self.io) catch return;
            defer stream.close(self.io);
            self.serveOne(stream, reply) catch {};
        }
    }

    fn serveOne(self: *Stub, stream: net.Stream, reply: []const u8) !void {
        const target_io = self.io;
        var read_buf: [8192]u8 = undefined;
        var stream_reader = stream.reader(target_io, &read_buf);
        const reader = &stream_reader.interface;

        var content_len: usize = 0;
        while (true) {
            const line = reader.takeDelimiterInclusive('\n') catch break;
            const trimmed = std.mem.trimEnd(u8, line, "\r\n");
            if (trimmed.len == 0) break;
            if (std.ascii.startsWithIgnoreCase(trimmed, "content-length:")) {
                const raw = std.mem.trim(u8, trimmed["content-length:".len..], " ");
                content_len = std.fmt.parseInt(usize, raw, 10) catch 0;
            }
        }

        if (content_len > 0) {
            var scratch: [8192]u8 = undefined;
            const want = @min(content_len, scratch.len);
            reader.readSliceAll(scratch[0..want]) catch {};
            if (self.count < max_requests) {
                const keep = @min(want, self.bodies_buf[self.count].len);
                @memcpy(self.bodies_buf[self.count][0..keep], scratch[0..keep]);
                self.bodies_len[self.count] = keep;
            }
        }
        self.count += 1;

        var write_buf: [8192]u8 = undefined;
        var stream_writer = stream.writer(target_io, &write_buf);
        try stream_writer.interface.writeAll(reply);
        try stream_writer.interface.flush();
    }
};

// -- Envelope assertions ------------------------------------------------------

const Envelope = struct {
    parsed: std.json.Parsed(std.json.Value),

    fn deinit(self: *Envelope) void {
        self.parsed.deinit();
    }

    fn root(self: *const Envelope) std.json.ObjectMap {
        return self.parsed.value.object;
    }

    fn opt(self: *const Envelope) std.json.ObjectMap {
        return self.root().get("opt").?.object;
    }

    fn str(value: ?std.json.Value) []const u8 {
        return if (value) |v| v.string else "";
    }
};

fn parseEnvelope(gpa: std.mem.Allocator, body: []const u8) !Envelope {
    return .{ .parsed = try std.json.parseFromSlice(std.json.Value, gpa, body, .{}) };
}

/// The invariants every request of the flow must satisfy: the complete
/// envelope — name, type and the full parameters map — plus lowercase
/// opt keys only. rclone rebuilds the remote from each request, and
/// capitalised `State`/`Result` are silently ignored, a failure that
/// looks like a hang.
fn expectFullEnvelope(env: *const Envelope) !void {
    try std.testing.expectEqualStrings("bk-test", Envelope.str(env.root().get("name")));
    try std.testing.expectEqualStrings("drive", Envelope.str(env.root().get("type")));
    const parameters = env.root().get("parameters").?.object;
    try std.testing.expectEqualStrings("abc-test-id", Envelope.str(parameters.get("client_id")));
    try std.testing.expect(env.opt().get("nonInteractive").?.bool);
    try std.testing.expect(env.opt().get("State") == null);
    try std.testing.expect(env.opt().get("Result") == null);
}

fn expectContinuation(env: *const Envelope, state: []const u8, result: []const u8) !void {
    try expectFullEnvelope(env);
    try std.testing.expect(env.opt().get("continue").?.bool);
    try std.testing.expectEqualStrings(state, Envelope.str(env.opt().get("state")));
    try std.testing.expectEqualStrings(result, Envelope.str(env.opt().get("result")));
}

fn testParameters(arena: std.mem.Allocator) !std.json.Value {
    var object: std.json.ObjectMap = .empty;
    try object.put(arena, "client_id", .{ .string = "abc-test-id" });
    return .{ .object = object };
}

// -- Driver tests -------------------------------------------------------------

test "the driver replays the captured oauth sequence to completion" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const replies = [_][]const u8{
        ok(reply_client_id_warning),
        ok(reply_islocal),
        ok(reply_authorize),
        ok(reply_done),
    };
    var stub: Stub = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var client = try rc.Client.init(gpa, tio, stub.endpoint());
    defer client.deinit();

    var params_arena: std.heap.ArenaAllocator = .init(gpa);
    defer params_arena.deinit();
    const params = try testParameters(params_arena.allocator());

    var flow = try oauth.Flow.init(gpa, &client, "bk-test", "drive");
    defer flow.deinit();

    // No provider name anywhere in the driver: every state below is opaque
    // data it carries, never text it interprets.
    const first = try flow.step(params, null);
    try std.testing.expectEqualStrings("config_shared_client_id", first.question.field.name);
    // bool + Exclusive + examples: the shared form rule closes the droplist.
    try std.testing.expect(first.question.field.widget == .droplist_closed);
    try std.testing.expectEqual(@as(usize, 2), first.question.field.examples.len);
    try std.testing.expectEqualStrings("", first.question.error_text);

    const second = try flow.step(params, "true");
    try std.testing.expectEqualStrings("config_is_local", second.question.field.name);

    const third = try flow.step(params, "false");
    try std.testing.expectEqualStrings("config_token", third.question.field.name);
    try std.testing.expect(third.question.field.required);
    try std.testing.expect(third.question.field.widget == .text);

    const fourth = try flow.step(params, "fake-authorize-blob");
    try std.testing.expect(fourth == .done);

    stub.stop();
    try std.testing.expectEqual(@as(usize, 4), stub.count);

    var env0 = try parseEnvelope(gpa, stub.body(0));
    defer env0.deinit();
    try expectFullEnvelope(&env0);
    // The opening request has no continuation keys at all.
    try std.testing.expect(env0.opt().get("continue") == null);
    try std.testing.expect(env0.opt().get("state") == null);
    try std.testing.expect(env0.opt().get("result") == null);

    var env1 = try parseEnvelope(gpa, stub.body(1));
    defer env1.deinit();
    try expectContinuation(&env1, "client_id_warning", "true");

    var env2 = try parseEnvelope(gpa, stub.body(2));
    defer env2.deinit();
    try expectContinuation(&env2, "*oauth-islocal,teamdrive,oauth,", "false");

    var env3 = try parseEnvelope(gpa, stub.body(3));
    defer env3.deinit();
    try expectContinuation(&env3, "*oauth-authorize,teamdrive,oauth,", "fake-authorize-blob");
}

test "an in-band error resurfaces the question with rclone's words" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const replies = [_][]const u8{ ok(reply_authorize_error), ok(reply_done) };
    var stub: Stub = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var client = try rc.Client.init(gpa, tio, stub.endpoint());
    defer client.deinit();
    var params_arena: std.heap.ArenaAllocator = .init(gpa);
    defer params_arena.deinit();
    const params = try testParameters(params_arena.allocator());

    var flow = try oauth.Flow.init(gpa, &client, "bk-test", "drive");
    defer flow.deinit();

    const step = try flow.step(params, null);
    try std.testing.expectEqualStrings("config_token", step.question.field.name);
    try std.testing.expect(std.mem.indexOf(u8, step.question.error_text, "Couldn't decode response") != null);

    // The retry continues from the state the error reply carried.
    const retry = try flow.step(params, "second-attempt");
    try std.testing.expect(retry == .done);
    stub.stop();
    var env = try parseEnvelope(gpa, stub.body(1));
    defer env.deinit();
    try expectContinuation(&env, "*oauth-authorize,teamdrive,oauth,", "second-attempt");
}

test "a stateful reply without a question continues by itself" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    // Synthetic: rclone's machine can hop through states that ask nothing;
    // the driver must carry them opaquely rather than surface an empty
    // question.
    const reply_hop =
        \\{"Error":"","Option":null,"Result":"","State":"internal-hop"}
    ;
    const replies = [_][]const u8{ ok(reply_hop), ok(reply_done) };
    var stub: Stub = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var client = try rc.Client.init(gpa, tio, stub.endpoint());
    defer client.deinit();
    var params_arena: std.heap.ArenaAllocator = .init(gpa);
    defer params_arena.deinit();
    const params = try testParameters(params_arena.allocator());

    var flow = try oauth.Flow.init(gpa, &client, "bk-test", "drive");
    defer flow.deinit();

    const step = try flow.step(params, null);
    try std.testing.expect(step == .done);

    stub.stop();
    try std.testing.expectEqual(@as(usize, 2), stub.count);
    var env = try parseEnvelope(gpa, stub.body(1));
    defer env.deinit();
    try expectContinuation(&env, "internal-hop", "");
}

test "the question serialises with the form wire keys" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const replies = [_][]const u8{ok(reply_client_id_warning)};
    var stub: Stub = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var client = try rc.Client.init(gpa, tio, stub.endpoint());
    defer client.deinit();
    var params_arena: std.heap.ArenaAllocator = .init(gpa);
    defer params_arena.deinit();
    const params = try testParameters(params_arena.allocator());

    var flow = try oauth.Flow.init(gpa, &client, "bk-test", "drive");
    defer flow.deinit();
    const step = try flow.step(params, null);

    const json = try oauth.questionJson(gpa, &step.question);
    defer gpa.free(json);
    var parsed = try std.json.parseFromSlice(std.json.Value, gpa, json, .{});
    defer parsed.deinit();
    const object = parsed.value.object;
    try std.testing.expectEqualStrings("config_shared_client_id", object.get("name").?.string);
    try std.testing.expectEqualStrings("droplist_closed", object.get("widget").?.string);
    try std.testing.expectEqualStrings("", object.get("error").?.string);
    try std.testing.expectEqual(@as(usize, 2), object.get("examples").?.array.items.len);
    try std.testing.expect(object.get("label") != null);
    try std.testing.expect(object.get("help") != null);
    try std.testing.expect(object.get("required") != null);
}

// -- Worker integration -------------------------------------------------------

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

const drive_creds_doc =
    \\{"backend":"drive","remote_root":"","fingerprint":"","options":{"client_id":"abc-test-id"},"secret_options":[],"password_options":[],"rclone_path":null}
;

fn sleepMs(target_io: std.Io, ms: u32) void {
    const duration: std.Io.Clock.Duration = .{
        .raw = .fromMilliseconds(ms),
        .clock = .awake,
    };
    duration.sleep(target_io) catch {};
}

fn pollUntilState(w: *worker.Worker, target_io: std.Io, wanted: worker.State, budget_ms: u32) ?worker.Snapshot {
    var waited: u32 = 0;
    while (waited <= budget_ms) : (waited += 25) {
        const snap = w.poll();
        if (snap.state == wanted) return snap;
        if (snap.state == .failed and wanted != .failed) return null;
        sleepMs(target_io, 25);
    }
    return null;
}

fn seedCreds(fixture: *Fixture, game_dir: []const u8) !void {
    const creds_path = try path.join(std.testing.allocator, &.{ game_dir, "profiles", "cloud.credentials" });
    defer std.testing.allocator.free(creds_path);
    try fixture.write(creds_path, drive_creds_doc);
}

test "the worker surfaces the question, takes the answer, and completion runs the connection test" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    try seedCreds(&fixture, game_dir);

    // In request order: applyCredentials configures bkraw and the alias,
    // the flow asks one question and completes on the answer, and the
    // completion path's connection test opens with a listing the "cloud"
    // refuses — proving the test runs, with the classified tag to show
    // for it. (The full probe against a real service is engine_test's.)
    const replies = [_][]const u8{
        ok(reply_ok_empty), // config/create bkraw
        ok(reply_ok_empty), // config/create bkremote alias
        ok(reply_client_id_warning),
        ok(reply_done),
        httpReply("HTTP/1.1 500 Internal Server Error", reply_list_auth_error),
    };
    var stub: Stub = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = stub.endpoint(),
    });
    defer w.destroy();

    try w.begin(.{ .kind = .config_create });

    const asked = pollUntilState(w, tio, .awaiting_input, 10_000);
    try std.testing.expect(asked != null);

    const question = (try w.configQuestionOwned(gpa)).?;
    defer gpa.free(question);
    try std.testing.expect(std.mem.indexOf(u8, question, "config_shared_client_id") != null);

    try std.testing.expect(w.answerConfig("true"));

    const settled = pollUntilState(w, tio, .failed, 10_000);
    try std.testing.expect(settled != null);
    try std.testing.expect(std.mem.startsWith(u8, settled.?.errorText(), "auth_failed"));

    // The question does not outlive the flow that asked it.
    try std.testing.expect((try w.configQuestionOwned(gpa)) == null);
}

test "cancel while a question waits settles as cancelled" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);
    try seedCreds(&fixture, game_dir);

    const replies = [_][]const u8{
        ok(reply_ok_empty),
        ok(reply_ok_empty),
        ok(reply_client_id_warning),
    };
    var stub: Stub = undefined;
    try stub.start(tio, &replies);
    defer stub.stop();

    var w = try worker.Worker.create(gpa, tio, .{
        .game_dir = game_dir,
        .endpoint = stub.endpoint(),
    });
    defer w.destroy();

    try w.begin(.{ .kind = .config_create });
    const asked = pollUntilState(w, tio, .awaiting_input, 10_000);
    try std.testing.expect(asked != null);

    w.cancel();
    const settled = pollUntilState(w, tio, .failed, 10_000);
    try std.testing.expect(settled != null);
    try std.testing.expectEqualStrings("Cancelled", settled.?.errorText());
}

// -- Live -------------------------------------------------------------------

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

test "a real rclone walks the drive machine to the token question" {
    const gpa = std.testing.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    const tio = threaded.io();

    const binary = envVar(gpa, "BK_TEST_RCLONE") orelse return;
    defer gpa.free(binary);

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();

    // Empty parameters, exactly as the fixture was captured: a client_id
    // of our own would legitimately skip the shared-client-id warning.
    var empty_params: std.json.ObjectMap = .empty;
    defer empty_params.deinit(gpa);
    const params: std.json.Value = .{ .object = empty_params };

    var flow = try oauth.Flow.init(gpa, &client, "bk-test", "drive");
    defer flow.deinit();

    // The same three states the captured fixture holds, from the real
    // machine this time. The flow is abandoned at the token question —
    // completing it needs a human and a browser — which is exactly what a
    // player's Cancel does.
    const first = try flow.step(params, null);
    try std.testing.expectEqualStrings("config_shared_client_id", first.question.field.name);
    const second = try flow.step(params, "true");
    try std.testing.expectEqualStrings("config_is_local", second.question.field.name);
    const third = try flow.step(params, "false");
    try std.testing.expectEqualStrings("config_token", third.question.field.name);
    try std.testing.expect(third.question.field.required);
}
