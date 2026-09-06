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

test "a rotated identity retires only the pairings naming the old remote" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const game_dir = try fixture.makeDir("game");
    defer gpa.free(game_dir);

    try engine.savePairingState(gpa, io, game_dir, "hero", .{
        .paired = true,
        .last_success_unix = 1,
        .remote_fingerprint = "fp-old",
    });
    try engine.savePairingState(gpa, io, game_dir, "ally", .{
        .paired = true,
        .last_success_unix = 2,
        .remote_fingerprint = "fp-new",
    });

    // An empty fingerprint retires nothing: a degenerate document must
    // never mass-delete the machine's pairing knowledge.
    try std.testing.expectEqual(@as(usize, 0), engine.retireMismatchedPairings(gpa, io, game_dir, ""));
    {
        var still = engine.loadPairingState(gpa, io, game_dir, "hero") orelse return error.TestUnexpectedResult;
        still.deinit();
    }

    // The rotation: records naming the old identity go, the current one
    // stays, and the retired profile reads as never paired - which is what
    // routes the next sync through the designed NotPaired -> pair bootstrap.
    try std.testing.expectEqual(@as(usize, 1), engine.retireMismatchedPairings(gpa, io, game_dir, "fp-new"));
    try std.testing.expect(engine.loadPairingState(gpa, io, game_dir, "hero") == null);
    var kept = engine.loadPairingState(gpa, io, game_dir, "ally").?;
    defer kept.deinit();
    try std.testing.expectEqualStrings("fp-new", kept.state().remote_fingerprint);

    // Idempotent: nothing left to retire.
    try std.testing.expectEqual(@as(usize, 0), engine.retireMismatchedPairings(gpa, io, game_dir, "fp-new"));

    // A game dir with no state directory at all is the fresh-install case.
    const empty_dir = try fixture.makeDir("empty");
    defer gpa.free(empty_dir);
    try std.testing.expectEqual(@as(usize, 0), engine.retireMismatchedPairings(gpa, io, empty_dir, "fp-new"));
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

// -- Failure classification ---------------------------------------------------
//
// Every fixture below is a captured real failure — rclone v1.75.0 on Windows,
// texts recorded in docs/superpowers/evidence/cloud-sync/
// failure-texts-v1.75-windows.md — because a classifier tested against
// invented strings classifies inventions.

/// The rc reply for every failed bisync, verbatim. The cause is never here.
const bare_reply: rc.RcFailure = .{ .message = "bisync aborted", .status = 500 };

const fixture_too_many_deletes =
    \\2026/08/21 00:44:42 ERROR : Safety abort: too many deletes (>50%, 2 of 3) on Path1 "C:\bk-ef\a\". Run with --force if desired.
    \\2026/08/21 00:44:42 NOTICE: Bisync aborted. Please try again.
    \\2026/08/21 00:44:42 NOTICE: Failed to bisync: too many deletes
;

const fixture_needs_resync =
    \\2026/08/21 00:44:42 ERROR : Bisync critical error: cannot find prior Path1 or Path2 listings, likely due to critical error on prior run
    \\2026/08/21 00:44:42 ERROR : Bisync aborted. Must run --resync to recover.
    \\2026/08/21 00:44:42 NOTICE: Failed to bisync: bisync aborted
;

const fixture_auth_failed =
    \\2026/08/21 00:45:35 ERROR : webdav root '': error reading source root directory: couldn't list files: 401 Unauthorized: 401 Unauthorized
    \\2026/08/21 00:45:35 ERROR : Bisync critical error: couldn't list files: 401 Unauthorized: 401 Unauthorized
    \\2026/08/21 00:45:35 ERROR : Bisync aborted. Must run --resync to recover.
    \\2026/08/21 00:45:35 NOTICE: Failed to bisync with 2 errors: last error was: bisync aborted
;

const fixture_unreachable =
    \\2026/08/21 00:45:40 ERROR : webdav root '': error reading source root directory: couldn't list files: Propfind "http://127.0.0.1:19999/": dial tcp 127.0.0.1:19999: connectex: No connection could be made because the target machine actively refused it.
    \\2026/08/21 00:45:40 ERROR : Bisync critical error: couldn't list files: Propfind "http://127.0.0.1:19999/": dial tcp 127.0.0.1:19999: connectex: No connection could be made because the target machine actively refused it.
    \\2026/08/21 00:45:40 ERROR : Bisync aborted. Must run --resync to recover.
    \\2026/08/21 00:45:40 NOTICE: Failed to bisync with 2 errors: last error was: bisync aborted
;

const fixture_name_too_long_windows =
    \\2026/08/21 00:44:17 NOTICE: Failed to bisync: syntax error detected in your path(s). Please check your command and try again.
    \\ error: CreateFile C:\wd\C__deep_a..C__deep_b: The filename, directory name, or volume label syntax is incorrect.
;

/// The POSIX flavour, measured on macOS during planning.
const fixture_name_too_long_posix =
    \\ERROR : Bisync critical error: file name too long
;

/// The double-seeded-sentinel abort, measured during planning.
const fixture_out_of_sync =
    \\ERROR : Modtime not equal in listing: .bkprofile
    \\ERROR : Bisync critical error: path1 and path2 are out of sync, run --resync to recover
;

/// The empty-remote pairing abort, measured during planning (and prevented
/// by engine.pair's mkdir; classification is the belt-and-braces).
const fixture_remote_missing =
    \\ERROR : webdav root 'profiles/hero': error reading source root directory: directory not found
    \\ERROR : Bisync critical error: directory not found
    \\ERROR : Bisync aborted. Must run --resync to recover.
;

test "each captured failure classifies to its outcome" {
    const cases = [_]struct { log: []const u8, expected: engine.Outcome }{
        .{ .log = fixture_too_many_deletes, .expected = .too_many_deletes },
        .{ .log = fixture_needs_resync, .expected = .needs_resync },
        .{ .log = fixture_auth_failed, .expected = .auth_failed },
        .{ .log = fixture_unreachable, .expected = .remote_unreachable },
        .{ .log = fixture_name_too_long_windows, .expected = .name_too_long },
        .{ .log = fixture_name_too_long_posix, .expected = .name_too_long },
        .{ .log = fixture_out_of_sync, .expected = .out_of_sync },
        .{ .log = fixture_remote_missing, .expected = .remote_missing },
        // Captured live through the connection probe: a cleared secret means
        // rclone cannot even build the fs — a credentials problem.
        .{ .log = "error in ListJSON: secret_access_key not found", .expected = .auth_failed },
    };
    for (cases) |case| {
        try std.testing.expectEqual(case.expected, engine.classify(bare_reply, case.log));
    }

    // The bare reply with no log is the degenerate case: nothing to read,
    // nothing to invent.
    try std.testing.expectEqual(engine.Outcome.unknown, engine.classify(bare_reply, ""));
}

test "the resync trailer loses to the cause above it" {
    // Both captures end with the identical `Must run --resync to recover.`
    // trailer. A classifier that reads the trailer first answers a wrong
    // password with an offer to re-pair — which is why order is pinned here.
    try std.testing.expect(
        std.mem.indexOf(u8, fixture_auth_failed, "Must run --resync") != null,
    );
    try std.testing.expect(
        std.mem.indexOf(u8, fixture_unreachable, "Must run --resync") != null,
    );
    try std.testing.expectEqual(engine.Outcome.auth_failed, engine.classify(bare_reply, fixture_auth_failed));
    try std.testing.expectEqual(engine.Outcome.remote_unreachable, engine.classify(bare_reply, fixture_unreachable));
}

test "every outcome maps to its recovery and the delete guard is never forced" {
    try std.testing.expectEqual(engine.Recovery.confirm_repair, engine.recovery(.needs_resync));
    try std.testing.expectEqual(engine.Recovery.confirm_repair, engine.recovery(.out_of_sync));
    // A tripped delete breaker is the guard *working*: the only recovery is
    // the player's explicit "mirror that?" decision. There is deliberately
    // no Recovery arm that retries with force, so it cannot be reached.
    try std.testing.expectEqual(engine.Recovery.confirm_mirror_delete, engine.recovery(.too_many_deletes));
    try std.testing.expectEqual(engine.Recovery.report_name_budget, engine.recovery(.name_too_long));
    try std.testing.expectEqual(engine.Recovery.open_credentials, engine.recovery(.auth_failed));
    try std.testing.expectEqual(engine.Recovery.open_credentials, engine.recovery(.remote_missing));
    try std.testing.expectEqual(engine.Recovery.retry, engine.recovery(.timed_out));
    try std.testing.expectEqual(engine.Recovery.retry, engine.recovery(.remote_unreachable));
    try std.testing.expectEqual(engine.Recovery.retry, engine.recovery(.daemon_gone));
    try std.testing.expectEqual(engine.Recovery.show_log, engine.recovery(.unknown));
}

test "transport errors classify without a log" {
    try std.testing.expectEqual(engine.Outcome.timed_out, engine.classifyTransport(error.Timeout));
    try std.testing.expectEqual(engine.Outcome.daemon_gone, engine.classifyTransport(error.Transport));
    // rc-level 401 is the daemon refusing our nonce — a foreign process on
    // our port — not the cloud rejecting credentials.
    try std.testing.expectEqual(engine.Outcome.daemon_gone, engine.classifyTransport(error.Unauthorized));
    try std.testing.expectEqual(engine.Outcome.unknown, engine.classifyTransport(error.RcFailed));
    try std.testing.expectEqual(engine.Outcome.unknown, engine.classifyTransport(error.BadJson));
}

test "the support log tail is bounded and redacted" {
    const gpa = std.testing.allocator;

    // Redaction exists because rclone puts the whole connection string —
    // obscured password included — into the filesystem name it prints in
    // error lines (captured with the unquoted-url variant). One line with
    // every marker shape:
    const secrets = "url=http://h,user=bk,pass=OBSCURED123: password=hunter2 " ++
        "access_key_id=AKIAXYZ secret_access_key=SsEeCc token=t0k3n " ++
        "Authorization: Basic QWxhZGRpbg== done\n";
    const scrubbed = try engine.redactedLogTail(gpa, secrets, .{});
    defer gpa.free(scrubbed);
    for ([_][]const u8{ "OBSCURED123", "hunter2", "AKIAXYZ", "SsEeCc", "t0k3n", "QWxhZGRpbg" }) |secret| {
        try std.testing.expect(std.mem.indexOf(u8, scrubbed, secret) == null);
    }
    try std.testing.expect(std.mem.indexOf(u8, scrubbed, "pass=[redacted]") != null);
    // The non-secret content survives — a fully censored log helps nobody.
    try std.testing.expect(std.mem.indexOf(u8, scrubbed, "user=bk") != null);
    try std.testing.expect(std.mem.indexOf(u8, scrubbed, "done") != null);

    // A capture with no credentials passes through intact.
    const untouched = try engine.redactedLogTail(gpa, fixture_unreachable, .{});
    defer gpa.free(untouched);
    try std.testing.expectEqualStrings(fixture_unreachable, untouched);

    // 250 numbered lines in, the last 200 out.
    var big: std.ArrayList(u8) = .empty;
    defer big.deinit(gpa);
    var line: usize = 0;
    var scratch: [32]u8 = undefined;
    while (line < 250) : (line += 1) {
        const rendered = std.fmt.bufPrint(&scratch, "line-{d}\n", .{line}) catch unreachable;
        try big.appendSlice(gpa, rendered);
    }

    const tail = try engine.redactedLogTail(gpa, big.items, .{});
    defer gpa.free(tail);
    try std.testing.expect(std.mem.startsWith(u8, tail, "line-50\n"));
    try std.testing.expect(std.mem.indexOf(u8, tail, "line-49\n") == null);
    try std.testing.expect(std.mem.indexOf(u8, tail, "line-249") != null);
}

test "the stored error text is redacted, with and without a run log" {
    const gpa = std.testing.allocator;

    // No daemon involved: the client is never asked to connect, it only
    // holds an endpoint, so recordError can be driven directly.
    var client = try rc.Client.init(gpa, io, .{
        .host = "127.0.0.1",
        .port = 1,
        .user = "u",
        .pass = "p",
    });
    defer client.deinit();
    var eng = engine.Engine.init(gpa, io, &client);
    defer eng.deinit();

    // The same capture shape that motivates the marker table: rclone
    // repeats the filesystem name — connection-string secrets included —
    // in the error text itself, not only in the run log. Connection-test
    // failures have no log at all, so the text is the whole exposure.
    const failure = "couldn't connect to " ++
        "\":webdav,url=http://127.0.0.1:81,user=bk,pass=OBSCURED-ERR-1:\"" ++
        ": connection refused";

    eng.recordError(failure, null);
    try std.testing.expect(std.mem.indexOf(u8, eng.lastErrorText(), "OBSCURED-ERR-1") == null);
    try std.testing.expect(std.mem.indexOf(u8, eng.lastErrorText(), "pass=[redacted]") != null);
    try std.testing.expect(std.mem.indexOf(u8, eng.lastErrorText(), "connection refused") != null);

    // With a log, both halves of the composed text are clean.
    eng.recordError(failure, "NOTICE: retrying\npass=OBSCURED-LOG-2 refused\n");
    const composed = eng.lastErrorText();
    try std.testing.expect(std.mem.indexOf(u8, composed, "OBSCURED-ERR-1") == null);
    try std.testing.expect(std.mem.indexOf(u8, composed, "OBSCURED-LOG-2") == null);
    try std.testing.expect(std.mem.indexOf(u8, composed, "retrying") != null);
}

test "catalogue-designated secrets are struck from the stored text" {
    const gpa = std.testing.allocator;

    var client = try rc.Client.init(gpa, io, .{
        .host = "127.0.0.1",
        .port = 1,
        .user = "u",
        .pass = "p",
    });
    defer client.deinit();
    var eng = engine.Engine.init(gpa, io, &client);
    defer eng.deinit();

    // The generic schema marks arbitrary catalogue options as secret — the
    // static marker table cannot know their names. The owner hands the
    // engine what the loaded credentials designate: names (struck as
    // `name=`, catching encodings of the value this code cannot know) and
    // plaintext values (struck wherever a server or log echoes them).
    try eng.setSecretRedactions(
        &.{ "client_secret", "sse_kms_key_id" },
        &.{ "very-secret-value-9", "kms" },
    );

    eng.recordError(
        "oauth2: cannot fetch token: client_secret=OBSCURED-GEN-1 rejected",
        "NOTICE: the server said very-secret-value-9 is expired\nkms alias kept\n",
    );
    const text = eng.lastErrorText();
    try std.testing.expect(std.mem.indexOf(u8, text, "OBSCURED-GEN-1") == null);
    try std.testing.expect(std.mem.indexOf(u8, text, "client_secret=[redacted]") != null);
    try std.testing.expect(std.mem.indexOf(u8, text, "very-secret-value-9") == null);
    try std.testing.expect(std.mem.indexOf(u8, text, "is expired") != null);
    // A three-byte "secret" is not struck: it cannot be recognised as a
    // leak, and striking it would censor arbitrary letters.
    try std.testing.expect(std.mem.indexOf(u8, text, "kms alias kept") != null);

    // Re-setting replaces the previous set entirely.
    try eng.setSecretRedactions(&.{}, &.{});
    eng.recordError("client_secret=SET-ANEW-2", null);
    try std.testing.expect(std.mem.indexOf(u8, eng.lastErrorText(), "SET-ANEW-2") != null);
}

// -- Connection test -----------------------------------------------------------

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

fn reservePort(target_io: std.Io) !u16 {
    var addr: std.Io.net.IpAddress = .{ .ip4 = .loopback(0) };
    var server = try addr.listen(target_io, .{ .reuse_address = true });
    const port = server.socket.address.getPort();
    server.deinit(target_io);
    return port;
}

fn waitTcp(target_io: std.Io, port: u16, budget_ms: u32) bool {
    var waited: u32 = 0;
    while (waited < budget_ms) {
        const addr: std.Io.net.IpAddress = .{ .ip4 = .loopback(port) };
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

fn createConfigRemote(
    gpa: std.mem.Allocator,
    client: *rc.Client,
    name: []const u8,
    fields: []const [2][]const u8,
) !void {
    var parameters: std.json.ObjectMap = .empty;
    defer parameters.deinit(gpa);
    for (fields) |field| try parameters.put(gpa, field[0], .{ .string = field[1] });

    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "name", .{ .string = name });
    try object.put(gpa, "type", parameters.get("type").?);
    try object.put(gpa, "parameters", .{ .object = parameters });
    var opt: std.json.ObjectMap = .empty;
    defer opt.deinit(gpa);
    try opt.put(gpa, "obscure", .{ .bool = true });
    try object.put(gpa, "opt", .{ .object = opt });

    var reply = try client.call("config/create", .{ .object = object });
    reply.deinit();
}

test "connection outcomes are classified against a live server" {
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
    const dav_data = try fixture.makeDir("dav");
    defer gpa.free(dav_data);

    const port = try reservePort(tio);
    var addr_buffer: [32]u8 = undefined;
    const addr = std.fmt.bufPrint(&addr_buffer, "127.0.0.1:{d}", .{port}) catch unreachable;
    var serve_environ = try parentEnviron(gpa);
    defer serve_environ.deinit();
    var serve = std.process.spawn(tio, .{
        .argv = &.{ binary, "serve", "webdav", dav_data, "--addr", addr, "--user", "dav", "--pass", "dav-secret-123" },
        .environ_map = &serve_environ,
        .stdin = .ignore,
        .stdout = .ignore,
        .stderr = .ignore,
    }) catch return error.ServeSpawnFailed;
    defer serve.kill(tio);
    try std.testing.expect(waitTcp(tio, port, 15_000));

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    var eng = engine.Engine.init(gpa, tio, &client);
    defer eng.deinit();

    const url = try std.fmt.allocPrint(gpa, "http://127.0.0.1:{d}", .{port});
    defer gpa.free(url);

    // Good configuration: the probe answers ok and classifies nothing.
    try createConfigRemote(gpa, &client, "cok", &.{
        .{ "type", "webdav" }, .{ "url", url }, .{ "vendor", "owncloud" },
        .{ "user", "dav" },    .{ "pass", "dav-secret-123" },
    });
    const good = try eng.testConnection("cok");
    try std.testing.expect(good.ok);

    // Wrong password: authentication, and never the secret in the text.
    try createConfigRemote(gpa, &client, "cbad", &.{
        .{ "type", "webdav" }, .{ "url", url }, .{ "vendor", "owncloud" },
        .{ "user", "dav" },    .{ "pass", "wrong-password-42" },
    });
    const bad_auth = try eng.testConnection("cbad");
    try std.testing.expect(!bad_auth.ok);
    try std.testing.expectEqual(engine.Outcome.auth_failed, bad_auth.outcome);
    try std.testing.expect(std.mem.indexOf(u8, eng.lastErrorText(), "wrong-password-42") == null);

    // Nothing listening: unreachable, distinctly from auth.
    try createConfigRemote(gpa, &client, "coff", &.{
        .{ "type", "webdav" }, .{ "url", "http://127.0.0.1:9" }, .{ "vendor", "owncloud" },
        .{ "user", "dav" },    .{ "pass", "dav-secret-123" },
    });
    const unreachable_result = try eng.testConnection("coff");
    try std.testing.expect(!unreachable_result.ok);
    try std.testing.expectEqual(engine.Outcome.remote_unreachable, unreachable_result.outcome);

    // Server fine, configured root absent: the missing-bucket shape.
    try createConfigRemote(gpa, &client, "cmiss", &.{
        .{ "type", "alias" }, .{ "remote", "cok:no-such-dir" },
    });
    const missing = try eng.testConnection("cmiss");
    try std.testing.expect(!missing.ok);
    try std.testing.expectEqual(engine.Outcome.remote_missing, missing.outcome);
}

test "the connection test probes writability, not just listing" {
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
    const dav_ro = try fixture.makeDir("dav-ro");
    defer gpa.free(dav_ro);
    const dav_rw = try fixture.makeDir("dav-rw");
    defer gpa.free(dav_rw);

    var serve_environ = try parentEnviron(gpa);
    defer serve_environ.deinit();

    // Read-only: exactly the backend shape the probe exists to catch — the
    // listing succeeds and a sync would still be impossible.
    const port_ro = try reservePort(tio);
    var addr_ro_buffer: [32]u8 = undefined;
    const addr_ro = std.fmt.bufPrint(&addr_ro_buffer, "127.0.0.1:{d}", .{port_ro}) catch unreachable;
    var serve_ro = std.process.spawn(tio, .{
        .argv = &.{ binary, "serve", "webdav", dav_ro, "--addr", addr_ro, "--read-only" },
        .environ_map = &serve_environ,
        .stdin = .ignore,
        .stdout = .ignore,
        .stderr = .ignore,
    }) catch return error.ServeSpawnFailed;
    defer serve_ro.kill(tio);
    try std.testing.expect(waitTcp(tio, port_ro, 15_000));

    const port_rw = try reservePort(tio);
    var addr_rw_buffer: [32]u8 = undefined;
    const addr_rw = std.fmt.bufPrint(&addr_rw_buffer, "127.0.0.1:{d}", .{port_rw}) catch unreachable;
    var serve_rw = std.process.spawn(tio, .{
        .argv = &.{ binary, "serve", "webdav", dav_rw, "--addr", addr_rw },
        .environ_map = &serve_environ,
        .stdin = .ignore,
        .stdout = .ignore,
        .stderr = .ignore,
    }) catch return error.ServeSpawnFailed;
    defer serve_rw.kill(tio);
    try std.testing.expect(waitTcp(tio, port_rw, 15_000));

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);
    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    var eng = engine.Engine.init(gpa, tio, &client);
    defer eng.deinit();

    const url_ro = try std.fmt.allocPrint(gpa, "http://127.0.0.1:{d}", .{port_ro});
    defer gpa.free(url_ro);
    const url_rw = try std.fmt.allocPrint(gpa, "http://127.0.0.1:{d}", .{port_rw});
    defer gpa.free(url_rw);

    // Listable but not writable is its own outcome — mapping it onto
    // auth_failed or remote_missing would tell the player the wrong thing.
    try createConfigRemote(gpa, &client, "cro", &.{
        .{ "type", "webdav" }, .{ "url", url_ro }, .{ "vendor", "owncloud" },
    });
    const read_only = try eng.testConnection("cro");
    try std.testing.expect(!read_only.ok);
    try std.testing.expectEqual(engine.Outcome.remote_unwritable, read_only.outcome);
    try std.testing.expectEqual(engine.Recovery.open_credentials, engine.recovery(.remote_unwritable));

    // The writable serve passes the whole round trip — and keeps nothing:
    // the probe object is deleted after reading back, and the served
    // directory is empty again.
    try createConfigRemote(gpa, &client, "crw", &.{
        .{ "type", "webdav" }, .{ "url", url_rw }, .{ "vendor", "owncloud" },
    });
    const writable = try eng.testConnection("crw");
    try std.testing.expect(writable.ok);

    var served = try std.Io.Dir.cwd().openDir(tio, dav_rw, .{ .iterate = true });
    defer served.close(tio);
    var it = served.iterate();
    var leftovers: usize = 0;
    while (try it.next(tio)) |_| leftovers += 1;
    try std.testing.expectEqual(@as(usize, 0), leftovers);
}

// -- Trash retention ----------------------------------------------------------

test "runIdTimestamp parses the stamp and rejects imposters" {
    // Pinned against independently known epoch values, because the pruning
    // arithmetic rests on this function.
    try std.testing.expectEqual(@as(i64, 0), engine.runIdTimestamp("19700101T000000Z-00000000").?);
    try std.testing.expectEqual(@as(i64, 951_868_800), engine.runIdTimestamp("20000301T000000Z-abcdef01").?);
    try std.testing.expectEqual(@as(i64, 1_709_208_000), engine.runIdTimestamp("20240229T120000Z-deadbeef").?);

    for ([_][]const u8{
        "not-a-run",
        "20260821T110000Z", // too short: no nonce
        "20260821X110000Z-aaaaaaaa", // wrong stamp separator
        "20260821T110000Z_aaaaaaaa", // wrong nonce separator
        "20261341T110000Z-aaaaaaaa", // month 13
        "20260821T256161Z-aaaaaaaa", // hour 25
        "20260821T110000Z-zzzzzzzz", // nonce not hex
        "2026082aT110000Z-aaaaaaaa", // stamp not digits
        // Impossible calendar dates: runId can never emit one, so a name
        // wearing one is foreign by definition — and daysFromCivil would
        // happily normalise Feb 31 into March, making it prune-eligible.
        "20260231T000000Z-aaaaaaaa", // Feb 31
        "20230229T000000Z-aaaaaaaa", // Feb 29 of a non-leap year
        "20260431T000000Z-aaaaaaaa", // Apr 31
        "21000229T000000Z-aaaaaaaa", // Feb 29 of a non-leap century
    }) |imposter| {
        try std.testing.expect(engine.runIdTimestamp(imposter) == null);
    }
}

test "local trash pruning keeps the newest runs and prunes by age only past them" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    const trash = try fixture.makeDir("trash");
    defer gpa.free(trash);

    // Mixed ages around a fixed "now", each run holding the same recurring
    // filename — the reason runs are the unit of retention at all.
    const now = engine.runIdTimestamp("20260821T120000Z-00000000").?;
    const runs = [_]struct { name: []const u8, content: []const u8 }{
        .{ .name = "20260821T110000Z-aaaaaaaa", .content = "v5" }, // 1 hour
        .{ .name = "20260810T110000Z-bbbbbbbb", .content = "v4" }, // 11 days
        .{ .name = "20260701T110000Z-cccccccc", .content = "v3" }, // 51 days
        .{ .name = "20260601T110000Z-dddddddd", .content = "v2" }, // 81 days
        .{ .name = "20260501T110000Z-eeeeeeee", .content = "v1" }, // 112 days
    };
    for (runs) |run| {
        const save = try path.join(gpa, &.{ trash, run.name, "saves", "quick.sav" });
        defer gpa.free(save);
        try fixture.write(save, run.content);
    }
    // Not a run id: never created by a run, never deleted by pruning.
    const foreign = try path.join(gpa, &.{ trash, "not-a-run", "keep.txt" });
    defer gpa.free(foreign);
    try fixture.write(foreign, "untouched");

    const report = try engine.pruneTrash(gpa, io, trash, .{
        .max_age_days = 30,
        .min_keep_runs = 2,
    }, now);
    try std.testing.expectEqual(@as(usize, 2), report.kept);
    try std.testing.expectEqual(@as(usize, 3), report.removed);

    // The two survivors carry two versions of the same recurring filename —
    // the recovery property a shared trash root destroyed (measured).
    try expectFileContent(gpa, &.{ trash, runs[0].name, "saves", "quick.sav" }, "v5");
    try expectFileContent(gpa, &.{ trash, runs[1].name, "saves", "quick.sav" }, "v4");
    for (runs[2..]) |gone| {
        const dir = try path.join(gpa, &.{ trash, gone.name });
        defer gpa.free(dir);
        try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, dir, .{}));
    }
    try expectFileContent(gpa, &.{ trash, "not-a-run", "keep.txt" }, "untouched");

    // min_keep_runs holds against age alone: with everything ancient, the
    // newest two still survive a second pass.
    const later = now + 400 * 86_400;
    const aged = try engine.pruneTrash(gpa, io, trash, .{
        .max_age_days = 30,
        .min_keep_runs = 2,
    }, later);
    try std.testing.expectEqual(@as(usize, 2), aged.kept);
    try std.testing.expectEqual(@as(usize, 0), aged.removed);

    // A trash that does not exist is quietly nothing to do.
    const missing = try path.join(gpa, &.{ fixture.root, "no-trash-here" });
    defer gpa.free(missing);
    const empty = try engine.pruneTrash(gpa, io, missing, .{}, now);
    try std.testing.expectEqual(@as(usize, 0), empty.kept);
    try std.testing.expectEqual(@as(usize, 0), empty.removed);
}

test "remote trash pruning works run-wise over the rc api" {
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

    // The remote side's trash, as bisync leaves it: run-scoped directories
    // that are siblings of profiles/, with relative paths preserved.
    const now = engine.runIdTimestamp("20260821T120000Z-00000000").?;
    for ([_][]const u8{
        "20260821T110000Z-aaaaaaaa",
        "20260601T110000Z-dddddddd",
        "20260501T110000Z-eeeeeeee",
    }) |run| {
        const save = try path.join(gpa, &.{ cloud, "trash", "hero", run, "saves", "quick.sav" });
        defer gpa.free(save);
        try fixture.write(save, run);
    }
    const foreign = try path.join(gpa, &.{ cloud, "trash", "hero", "not-a-run", "keep.txt" });
    defer gpa.free(foreign);
    try fixture.write(foreign, "untouched");

    var d = try daemon.Daemon.spawn(gpa, tio, .{ .binary = binary, .game_dir = game_dir });
    defer d.shutdown();
    try d.waitReady(daemon.ready_timeout_ms);

    var client = try rc.Client.init(gpa, tio, d.endpoint());
    defer client.deinit();
    try createAliasRemote(gpa, &client, "bkremote", cloud);

    var eng = engine.Engine.init(gpa, tio, &client);
    defer eng.deinit();

    const report = try eng.pruneRemoteTrash("bkremote", "hero", .{
        .max_age_days = 30,
        .min_keep_runs = 1,
    }, now);
    try std.testing.expectEqual(@as(usize, 1), report.kept);
    try std.testing.expectEqual(@as(usize, 2), report.removed);

    // The newest run survives, the old ones are gone whole, the foreign
    // directory is untouched.
    try expectFileContent(
        gpa,
        &.{ cloud, "trash", "hero", "20260821T110000Z-aaaaaaaa", "saves", "quick.sav" },
        "20260821T110000Z-aaaaaaaa",
    );
    for ([_][]const u8{ "20260601T110000Z-dddddddd", "20260501T110000Z-eeeeeeee" }) |gone| {
        const dir = try path.join(gpa, &.{ cloud, "trash", "hero", gone });
        defer gpa.free(dir);
        try std.testing.expectError(error.FileNotFound, std.Io.Dir.cwd().statFile(io, dir, .{}));
    }
    try expectFileContent(gpa, &.{ cloud, "trash", "hero", "not-a-run", "keep.txt" }, "untouched");

    // A profile with no trash root yet: zero of each, no error — the empty
    // remote is the normal first-run state everywhere in this plan.
    const none = try eng.pruneRemoteTrash("bkremote", "nobody", .{}, now);
    try std.testing.expectEqual(@as(usize, 0), none.kept);
    try std.testing.expectEqual(@as(usize, 0), none.removed);
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
