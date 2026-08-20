//! Sync engine. This packet owns the first of its duties: establishing a
//! profile's pairing without letting it destroy the other side.
//!
//! Pairing is the one moment bisync runs with `resync: true`, and resync is
//! the mode with teeth: `conflictResolve` is ignored during it, the default
//! is Path1 wins, and the measured failure was a machine holding an older
//! save silently overwriting the newer cloud copy — no conflict file, no
//! trash entry. Everything here exists to make that impossible:
//!
//! - The remote base directories are created first, because bisync aborts a
//!   first resync against a missing root with `directory not found`, and the
//!   empty remote is every player's first sync, not an edge case.
//! - The sentinel is written only after asking the remote whether it already
//!   carries one — two independently seeded copies differ in modification
//!   time and abort the resync as out of sync.
//! - The parameters come from `plan.bisyncParams` in `.pairing` mode, which
//!   carries `resyncMode: "newer"` and both run-scoped trashes, so the loser
//!   of every resync comparison is recoverable on its own side.
//! - A profile that has already paired is refused: a resync after real
//!   divergence overwrites one side, so recovery is an explicit player
//!   action, never an automatic retry. A changed remote fingerprint is a new
//!   pairing needing confirmation, not a resumable session.
//!
//! Pairing state lives at `<stateRoot>/state/<profile>.json` — outside Path1,
//! because inside the profile it would sync to the other machine and
//! misreport that machine's pairing.
//!
//! **Everything here blocks the calling thread.** Like `rc.zig`, this file is
//! worker-thread material; P02-M02 owns the thread and the poll snapshot.

const std = @import("std");
const builtin = @import("builtin");
const rc = @import("rc.zig");
const plan = @import("plan.zig");

const Allocator = std.mem.Allocator;
const Io = std.Io;
const path = Io.Dir.path;

/// How often a running bisync job is asked how it is doing.
const job_poll_ms: u32 = 250;

/// How long one bisync run may take before the engine stops waiting. Profile
/// payloads are small — saves and options — so minutes mean a wedged run, not
/// a slow one. The job keeps running server-side; P02-M03 owns what to tell
/// the player.
const job_timeout_ms: u32 = 120_000;

/// What this machine knows about one profile's pairing. Serialised as JSON at
/// `<stateRoot>/state/<profile>.json`; unknown fields are ignored on read so
/// later packets can extend it without a migration.
pub const PairingState = struct {
    paired: bool,
    last_success_unix: i64,
    /// The remote identity at pairing time — bucket, endpoint, whatever the
    /// credentials packet chooses to fingerprint. A mismatch means the player
    /// pointed the profile somewhere new, and that is a fresh pairing
    /// decision, not a session to resume.
    remote_fingerprint: []const u8,
};

/// A parsed pairing state and the arena the strings live in.
pub const LoadedPairingState = struct {
    parsed: std.json.Parsed(PairingState),

    pub fn state(self: *const LoadedPairingState) PairingState {
        return self.parsed.value;
    }

    pub fn deinit(self: *LoadedPairingState) void {
        self.parsed.deinit();
        self.* = undefined;
    }
};

/// The pairing state for `profile`, or null when there is none or it cannot
/// be read. Corruption reads as "never paired": the safe consequence is that
/// pairing is offered again, and pairing refuses to run unattended anyway.
pub fn loadPairingState(
    gpa: Allocator,
    io: Io,
    game_dir: []const u8,
    profile: []const u8,
) ?LoadedPairingState {
    const state_path = plan.pairingStatePath(gpa, game_dir, profile) catch return null;
    defer gpa.free(state_path);

    const text = Io.Dir.cwd().readFileAlloc(io, state_path, gpa, .limited(4096)) catch return null;
    defer gpa.free(text);

    const parsed = std.json.parseFromSlice(PairingState, gpa, text, .{
        .ignore_unknown_fields = true,
        // The default only copies strings that contain escapes; every other
        // field would borrow `text`, which is freed on return.
        .allocate = .alloc_always,
    }) catch return null;
    return .{ .parsed = parsed };
}

pub const SaveStateError = Allocator.Error || error{StateUnwritable};

/// Persist `state`, creating the state directory when absent. Write-then-read
/// is the durability contract here, not atomicity: the state answers "has
/// this profile ever paired", and a torn write reads as "no", which re-offers
/// pairing rather than corrupting anything.
pub fn savePairingState(
    gpa: Allocator,
    io: Io,
    game_dir: []const u8,
    profile: []const u8,
    state: PairingState,
) SaveStateError!void {
    const state_path = try plan.pairingStatePath(gpa, game_dir, profile);
    defer gpa.free(state_path);

    const dir = path.dirname(state_path) orelse return error.StateUnwritable;
    Io.Dir.cwd().createDirPath(io, dir) catch return error.StateUnwritable;

    const text = std.json.Stringify.valueAlloc(gpa, state, .{}) catch return error.StateUnwritable;
    defer gpa.free(text);

    Io.Dir.cwd().writeFile(io, .{ .sub_path = state_path, .data = text }) catch
        return error.StateUnwritable;
}

/// Everything one pairing or sync run needs to know. The caller gathers it —
/// discovery, credentials and the short link are other packets' property —
/// and the engine owns doing it in the right order.
pub const RunContext = struct {
    /// Path1 as bisync sees it: the short link in production, a fixture
    /// directory in tests. The engine treats it as opaque.
    path1: []const u8,
    /// The rclone remote name, without the colon.
    remote: []const u8,
    /// The profile name under `<remote>:profiles/`.
    profile: []const u8,
    /// Where `cloudsync/` lives. Absolute.
    game_dir: []const u8,
    /// The sentinel's content when this machine seeds it.
    profile_id: []const u8,
    /// The remote identity to record — and to compare on every later run.
    remote_fingerprint: []const u8,
};

pub const PairError = error{
    /// The engine's cancel flag went true while the run was in flight. The
    /// rclone job keeps running server-side; only the wait is abandoned.
    Cancelled,
    /// This profile is already paired here. A resync after real divergence
    /// overwrites one side; recovery is a player action through P02-M03.
    AlreadyPaired,
    /// Paired, but against a different remote identity. New pairing decision.
    FingerprintChanged,
    /// The bisync job finished and reported failure. `lastErrorText` holds
    /// what rclone said.
    SyncFailed,
    /// Machine-local state could not be written.
    StateUnwritable,
    SentinelUnwritable,
    SessionNameTooLong,
} || rc.RcError || Allocator.Error;

pub const SyncError = error{
    Cancelled,
    /// No successful pairing recorded for this profile on this machine.
    NotPaired,
    FingerprintChanged,
    SyncFailed,
    StateUnwritable,
    SessionNameTooLong,
} || rc.RcError || Allocator.Error;

/// What a pairing did, for the caller's report.
pub const PairOutcome = struct {
    /// This run's id — the name of both trash directories, which is how a
    /// caller (or a test) finds what the resync displaced. Owned.
    run_id: []u8,
    /// What happened about the sentinel.
    sentinel: plan.SentinelAction,

    pub fn deinit(self: *PairOutcome, gpa: Allocator) void {
        gpa.free(self.run_id);
        self.* = undefined;
    }
};

/// Drives bisync runs over one rc client. Blocking by design; the worker
/// thread in P02-M02 is the only intended caller in the shipped game.
pub const Engine = struct {
    gpa: Allocator,
    io: Io,
    client: *rc.Client,
    /// What rclone said about the most recent failed run: the job's error
    /// text, and the run log when there is one. P02-M03 classifies it.
    last_error_owned: ?[]u8 = null,
    /// When set, checked between the bounded phases of a run — before the
    /// job starts and between status polls. A true value abandons the wait
    /// with `error.Cancelled`; the worker's shutdown path owns setting it.
    cancel: ?*const std.atomic.Value(bool) = null,
    /// The classification of the most recent `error.SyncFailed`, `.unknown`
    /// until one happens. Transport-level errors never get here; classify
    /// those with `classifyTransport` at the call site.
    last_outcome: Outcome = .unknown,

    pub fn init(gpa: Allocator, io: Io, client: *rc.Client) Engine {
        return .{ .gpa = gpa, .io = io, .client = client };
    }

    pub fn deinit(self: *Engine) void {
        self.clearLastError();
        self.* = undefined;
    }

    pub fn lastErrorText(self: *const Engine) []const u8 {
        return self.last_error_owned orelse "";
    }

    /// First pairing for a profile. Refuses when already paired — resync is
    /// the bootstrap, never the recovery path — and otherwise: remote
    /// directories, sentinel decision, filters, one bisync with
    /// `resync: true` + `resyncMode: "newer"`, then the state record.
    pub fn pair(self: *Engine, ctx: RunContext) PairError!PairOutcome {
        if (loadPairingState(self.gpa, self.io, ctx.game_dir, ctx.profile)) |*loaded| {
            var owned = loaded.*;
            defer owned.deinit();
            if (owned.state().paired) {
                if (!std.mem.eql(u8, owned.state().remote_fingerprint, ctx.remote_fingerprint))
                    return error.FingerprintChanged;
                return error.AlreadyPaired;
            }
            // A record that says "not paired" is a previous attempt that
            // never succeeded; pairing again is exactly right.
        }

        // The remote base directories first: bisync requires both roots to
        // exist and otherwise dies with `error reading source root directory:
        // directory not found` — against a brand-new remote, which is every
        // player's first sync.
        const path2 = try plan.remoteProfileRoot(self.gpa, ctx.remote, ctx.profile);
        defer self.gpa.free(path2);
        try self.mkdirRemote(path2);
        const trash_root = try std.fmt.allocPrint(
            self.gpa,
            "{s}:trash/{s}",
            .{ ctx.remote, ctx.profile },
        );
        defer self.gpa.free(trash_root);
        try self.mkdirRemote(trash_root);

        // Ask the remote before seeding: two independently created sentinels
        // differ in modification time and abort the resync as out of sync.
        const remote_has_sentinel = try self.remoteFileExists(path2, plan.sentinel_name);
        const sentinel = plan.ensureSentinel(
            self.gpa,
            self.io,
            ctx.path1,
            ctx.profile_id,
            remote_has_sentinel,
        ) catch |err| return switch (err) {
            error.SentinelUnwritable => error.SentinelUnwritable,
            error.OutOfMemory => error.OutOfMemory,
        };

        const run_id = try self.prepareRun(ctx);
        errdefer self.gpa.free(run_id);

        var params = try plan.bisyncParams(self.gpa, .{
            .path1 = ctx.path1,
            .remote = ctx.remote,
            .profile = ctx.profile,
            .game_dir = ctx.game_dir,
            .run_id = run_id,
            .mode = .pairing,
        });
        defer params.deinit();

        try self.runBisync(params.value);

        try self.recordSuccess(ctx);
        return .{ .run_id = run_id, .sentinel = sentinel };
    }

    /// One steady-state sync, blocking until the job finishes. The pairing
    /// state gates it: never paired means no sync, and a changed fingerprint
    /// means the player must confirm a new pairing first. The parameters
    /// carry no `resync` key — `assertNoResyncWhenPaired` in the plan tests
    /// pins that.
    pub fn syncOnce(self: *Engine, ctx: RunContext) SyncError![]u8 {
        {
            var loaded = loadPairingState(self.gpa, self.io, ctx.game_dir, ctx.profile) orelse
                return error.NotPaired;
            defer loaded.deinit();
            if (!loaded.state().paired) return error.NotPaired;
            if (!std.mem.eql(u8, loaded.state().remote_fingerprint, ctx.remote_fingerprint))
                return error.FingerprintChanged;
        }

        const run_id = try self.prepareRun(ctx);
        errdefer self.gpa.free(run_id);

        var params = try plan.bisyncParams(self.gpa, .{
            .path1 = ctx.path1,
            .remote = ctx.remote,
            .profile = ctx.profile,
            .game_dir = ctx.game_dir,
            .run_id = run_id,
            .mode = .steady,
        });
        defer params.deinit();

        try self.runBisync(params.value);

        try self.recordSuccess(ctx);
        return run_id;
    }

    /// The machine-local prerequisites every run needs: the state root and
    /// workdir directories, and a fresh filters file — rewritten each run
    /// with identical bytes, so bisync's filters-changed MD5 check never
    /// trips. Returns the new run id.
    fn prepareRun(self: *Engine, ctx: RunContext) (Allocator.Error || error{StateUnwritable})![]u8 {
        const workdir = try plan.workdirPath(self.gpa, ctx.game_dir);
        defer self.gpa.free(workdir);
        Io.Dir.cwd().createDirPath(self.io, workdir) catch return error.StateUnwritable;

        const filters = try plan.filtersFilePath(self.gpa, ctx.game_dir);
        defer self.gpa.free(filters);
        plan.writeFiltersFile(self.io, filters) catch return error.StateUnwritable;

        return plan.runId(self.gpa, self.io);
    }

    fn recordSuccess(self: *Engine, ctx: RunContext) (Allocator.Error || error{StateUnwritable})!void {
        savePairingState(self.gpa, self.io, ctx.game_dir, ctx.profile, .{
            .paired = true,
            .last_success_unix = Io.Clock.now(.real, self.io).toSeconds(),
            .remote_fingerprint = ctx.remote_fingerprint,
        }) catch |err| return switch (err) {
            error.StateUnwritable => error.StateUnwritable,
            error.OutOfMemory => error.OutOfMemory,
        };
    }

    /// Start the job and poll it to completion. `_async` plus polling rather
    /// than a synchronous call, because a bisync can outlive any reasonable
    /// single-request deadline and the rc transport's budget is per POST.
    fn runBisync(self: *Engine, params: std.json.Value) (rc.RcError || error{ SyncFailed, Cancelled } || Allocator.Error)!void {
        self.clearLastError();

        if (self.cancelled()) return error.Cancelled;
        const job = try self.client.callAsync("sync/bisync", params);

        var waited: u32 = 0;
        while (true) {
            if (self.cancelled()) return error.Cancelled;
            var status = try self.client.jobStatus(job);
            defer status.deinit();

            if (status.finished) {
                if (status.success) return;
                // The rc error is terse ("bisync aborted"); the run log in
                // `output.output` is where rclone explains itself. Keep both.
                self.recordError(status.error_text, status.outputText());
                return error.SyncFailed;
            }

            if (waited >= job_timeout_ms) return error.Timeout;
            sleepMs(self.io, job_poll_ms);
            waited += job_poll_ms;
        }
    }

    fn cancelled(self: *const Engine) bool {
        const flag = self.cancel orelse return false;
        return flag.load(.acquire);
    }

    /// `operations/mkdir` on the root of `fs_spec`. Creating a directory that
    /// exists is success by rclone's definition, so this is safe to repeat.
    fn mkdirRemote(self: *Engine, fs_spec: []const u8) (rc.RcError || Allocator.Error)!void {
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        try object.put(self.gpa, "fs", .{ .string = fs_spec });
        try object.put(self.gpa, "remote", .{ .string = "" });

        var reply = try self.client.call("operations/mkdir", .{ .object = object });
        reply.deinit();
    }

    /// Whether `name` exists directly under `fs_spec`, by `operations/stat`:
    /// a null `item` is rclone's "no such file".
    fn remoteFileExists(
        self: *Engine,
        fs_spec: []const u8,
        name: []const u8,
    ) (rc.RcError || Allocator.Error)!bool {
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        try object.put(self.gpa, "fs", .{ .string = fs_spec });
        try object.put(self.gpa, "remote", .{ .string = name });

        var reply = try self.client.call("operations/stat", .{ .object = object });
        defer reply.deinit();

        const top = switch (reply.value) {
            .object => |o| o,
            else => return error.BadJson,
        };
        const item = top.get("item") orelse return false;
        return item != .null;
    }

    /// The classification of the most recent failed run.
    pub fn lastOutcome(self: *const Engine) Outcome {
        return self.last_outcome;
    }

    fn recordError(self: *Engine, error_text: []const u8, run_log: ?[]const u8) void {
        self.clearLastError();
        self.last_outcome = classify(
            .{ .message = error_text, .status = 500 },
            run_log orelse "",
        );
        // What is kept is already redacted and bounded: the raw log never
        // leaves this function, so nothing downstream can leak it.
        self.last_error_owned = if (run_log) |log| owned: {
            const tail = redactedLogTail(self.gpa, log) catch
                break :owned self.gpa.dupe(u8, error_text) catch null;
            defer self.gpa.free(tail);
            break :owned std.fmt.allocPrint(self.gpa, "{s}\n{s}", .{ error_text, tail }) catch null;
        } else self.gpa.dupe(u8, error_text) catch null;
    }

    fn clearLastError(self: *Engine) void {
        if (self.last_error_owned) |owned| self.gpa.free(owned);
        self.last_error_owned = null;
    }
};

fn sleepMs(io: Io, ms: u32) void {
    const duration: Io.Clock.Duration = .{
        .raw = .fromMilliseconds(ms),
        .clock = .awake,
    };
    duration.sleep(io) catch {};
}

// -- Failure classification --------------------------------------------------
//
// The rc reply for a failed bisync says only `{"error": "bisync aborted",
// "status": 500}` — the cause exists solely in the run log, so
// classification reads the log, never the reply. Every pattern below is a
// captured real failure, not an invented one; the texts are in
// `docs/superpowers/evidence/cloud-sync/failure-texts-v1.75-windows.md`.
//
// Order is load-bearing: an auth failure and an unreachable remote both end
// with the same `Bisync aborted. Must run --resync to recover.` trailer
// (captured), so the cause patterns must be tested before the trailer — or a
// wrong password would be answered with an offer to re-pair.

/// What went wrong, in terms a UI can offer a real choice about.
pub const Outcome = enum {
    /// bisync has no usable prior listings. Recovery is a *confirmed*
    /// re-pair; resync overwrites one side, so it is never automatic.
    needs_resync,
    /// The delete-ratio breaker tripped: a genuine mass-delete event. Never
    /// auto-retried with `force` — silently overriding this guard is the
    /// one behaviour the design exists to prevent.
    too_many_deletes,
    /// The session name is over the filename budget.
    name_too_long,
    /// The two sides' listings disagree (the double-seeded-sentinel shape).
    /// Recovery is a confirmed re-pair.
    out_of_sync,
    /// The cloud rejected the credentials.
    auth_failed,
    /// The cloud could not be reached at all.
    remote_unreachable,
    /// The daemon itself is not answering.
    daemon_gone,
    /// The run outlived its budget.
    timed_out,
    unknown,
};

/// What the UI should offer for each outcome. The mapping is total, so a new
/// `Outcome` without a decision here fails to compile.
pub const Recovery = enum {
    /// Offer a confirmed re-pair through the pairing flow.
    confirm_repair,
    /// Ask "the cloud copy looks emptied, mirror that?" — a player decision,
    /// never a `force` retry.
    confirm_mirror_delete,
    /// Report the projected session-name length against the budget and point
    /// at the short link.
    report_name_budget,
    /// Open the credentials dialog.
    open_credentials,
    /// Offer a retry.
    retry,
    /// Nothing smarter to offer: show the redacted log tail.
    show_log,
};

pub fn recovery(outcome: Outcome) Recovery {
    return switch (outcome) {
        .needs_resync, .out_of_sync => .confirm_repair,
        .too_many_deletes => .confirm_mirror_delete,
        .name_too_long => .report_name_budget,
        .auth_failed => .open_credentials,
        .timed_out, .remote_unreachable, .daemon_gone => .retry,
        .unknown => .show_log,
    };
}

/// Classify a failed run from its rc failure and run log. The log is
/// consulted first and the terse reply only as a fallback, because the reply
/// never carries the cause.
pub fn classify(failure: rc.RcFailure, log: []const u8) Outcome {
    if (classifyText(log)) |outcome| return outcome;
    if (classifyText(failure.message)) |outcome| return outcome;
    return .unknown;
}

fn classifyText(text: []const u8) ?Outcome {
    if (text.len == 0) return null;

    // Causes before consequences: the resync trailer appears under auth and
    // network failures too (captured), and must lose to them.
    const auth_patterns = [_][]const u8{
        "401 unauthorized",
        "403 forbidden",
        "accessdenied",
        "signaturedoesnotmatch",
        "invalidaccesskeyid",
        "authorizationheadermalformed",
    };
    for (auth_patterns) |pattern| {
        if (containsIgnoreCase(text, pattern)) return .auth_failed;
    }

    const unreachable_patterns = [_][]const u8{
        // Go's dialer prefixes both POSIX "connection refused" and Windows
        // "connectex: No connection could be made..." with this.
        "dial tcp",
        "no such host",
        "connection refused",
        "network is unreachable",
        "i/o timeout",
    };
    for (unreachable_patterns) |pattern| {
        if (containsIgnoreCase(text, pattern)) return .remote_unreachable;
    }

    if (containsIgnoreCase(text, "too many deletes")) return .too_many_deletes;

    // POSIX says "file name too long"; Windows wraps "The filename,
    // directory name, or volume label syntax is incorrect." in bisync's
    // canned "syntax error detected in your path(s)" (both captured).
    if (containsIgnoreCase(text, "file name too long")) return .name_too_long;
    if (containsIgnoreCase(text, "syntax error detected in your path")) return .name_too_long;

    if (containsIgnoreCase(text, "path1 and path2 are out of sync")) return .out_of_sync;
    if (containsIgnoreCase(text, "must run --resync to recover")) return .needs_resync;

    return null;
}

/// Transport-level failures never reach the log; they are classified from
/// the error alone. `Unauthorized` here is the *daemon* refusing our nonce —
/// a foreign process on our port — not the cloud rejecting credentials.
pub fn classifyTransport(err: rc.RcError) Outcome {
    return switch (err) {
        error.Timeout => .timed_out,
        error.Transport, error.Unauthorized => .daemon_gone,
        error.RcFailed, error.BadJson => .unknown,
    };
}

fn containsIgnoreCase(haystack: []const u8, needle: []const u8) bool {
    return std.ascii.indexOfIgnoreCase(haystack, needle) != null;
}

// -- Log redaction ------------------------------------------------------------

/// How much of a failing run log is kept for support purposes.
pub const log_tail_lines = 200;

/// Markers whose value, up to the next delimiter, is credential material.
/// The capture that motivates this: a connection-string remote puts
/// `user=bk,pass=<obscured>` into the *filesystem name*, which rclone then
/// prints in every error line.
const redact_markers = [_][]const u8{
    "pass=",
    "password=",
    "secret_access_key=",
    "access_key_id=",
    "token=",
    "authorization: basic ",
    "authorization: bearer ",
};

const redacted_placeholder = "[redacted]";

/// The last `log_tail_lines` lines of `log`, with credential values struck
/// out. This is what may travel in a support report; the raw log never
/// leaves the machine through the ABI.
pub fn redactedLogTail(gpa: Allocator, log: []const u8) Allocator.Error![]u8 {
    const tail = lastLines(log, log_tail_lines);

    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(gpa);

    var index: usize = 0;
    scan: while (index < tail.len) {
        for (redact_markers) |marker| {
            if (std.ascii.startsWithIgnoreCase(tail[index..], marker)) {
                try out.appendSlice(gpa, tail[index..][0..marker.len]);
                try out.appendSlice(gpa, redacted_placeholder);
                index += marker.len;
                // Swallow the value: everything up to a delimiter that can
                // end a credential in a URL, connection string, header or
                // config line.
                while (index < tail.len) : (index += 1) {
                    switch (tail[index]) {
                        ' ', ',', '"', '\'', ':', ';', '\n', '\r', ')', ']', '&' => break,
                        else => {},
                    }
                }
                continue :scan;
            }
        }
        try out.append(gpa, tail[index]);
        index += 1;
    }

    return out.toOwnedSlice(gpa);
}

fn lastLines(text: []const u8, count: usize) []const u8 {
    if (text.len == 0) return text;
    var lines: usize = 0;
    var index = text.len;
    // A trailing newline does not make an empty extra line.
    if (text[index - 1] == '\n') index -= 1;
    while (index > 0) : (index -= 1) {
        if (text[index - 1] == '\n') {
            lines += 1;
            if (lines == count) return text[index..];
        }
    }
    return text;
}
