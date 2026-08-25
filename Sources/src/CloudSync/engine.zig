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
const catalogue = @import("catalogue.zig");
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

/// Retire every pairing record that names a remote other than `fingerprint`,
/// returning how many were removed. This is the machine-local half of "a
/// changed fingerprint is a new pairing needing confirmation": the
/// confirmation is the player's deliberate credentials save, and once the
/// stored connection identity has rotated, a record still naming the old
/// identity can only refuse with FingerprintChanged forever — no UI owns
/// any other way out. Removing it routes the next sync through the designed
/// NotPaired -> pair bootstrap. Records naming the current identity are kept
/// (saving the same remote is a no-op here), unreadable records are left
/// alone (they already read as "never paired"), and an empty fingerprint
/// retires nothing — a degenerate document must never erase the machine's
/// pairing knowledge wholesale. Failures are swallowed file-wise: a record
/// that cannot be deleted keeps refusing exactly as it did before this
/// function existed.
pub fn retireMismatchedPairings(
    gpa: Allocator,
    io: Io,
    game_dir: []const u8,
    fingerprint: []const u8,
) usize {
    if (fingerprint.len == 0) return 0;

    const state_root = plan.stateRoot(gpa, game_dir) catch return 0;
    defer gpa.free(state_root);
    const state_dir_path = path.join(gpa, &.{ state_root, "state" }) catch return 0;
    defer gpa.free(state_dir_path);

    var dir = Io.Dir.cwd().openDir(io, state_dir_path, .{ .iterate = true }) catch return 0;
    defer dir.close(io);

    // Collect first, delete after: removing entries out from under a live
    // directory iterator is platform-defined behaviour.
    var stale: std.ArrayList([]u8) = .empty;
    defer {
        for (stale.items) |name| gpa.free(name);
        stale.deinit(gpa);
    }
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (entry.kind != .file) continue;
        if (!std.mem.endsWith(u8, entry.name, ".json")) continue;
        const profile = entry.name[0 .. entry.name.len - ".json".len];

        var loaded = loadPairingState(gpa, io, game_dir, profile) orelse continue;
        const matches = std.mem.eql(u8, loaded.state().remote_fingerprint, fingerprint);
        loaded.deinit();
        if (matches) continue;

        const owned = gpa.dupe(u8, entry.name) catch continue;
        stale.append(gpa, owned) catch {
            gpa.free(owned);
            continue;
        };
    }

    var retired: usize = 0;
    for (stale.items) |name| {
        dir.deleteFile(io, name) catch continue;
        retired += 1;
    }
    return retired;
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
    /// When set, the provider catalogue is refreshed after a clean sync, and
    /// only then. This is the opportunistic half of the bootstrap the design
    /// requires: the catalogue cannot be fetched at startup, because
    /// `GameMain.cpp` reaches `Available()` only when cloud sync is already
    /// enabled — so a fresh profile would never trigger it. The other half is
    /// the deliberate fetch when the credentials dialog opens. Here the daemon
    /// is already running and the check is one small version call, so a stale
    /// catalogue costs nothing to notice. Like pruning, it runs after
    /// `recordSuccess` and every failure is swallowed: a sync that succeeded
    /// stays succeeded.
    refresh_catalogue: bool = false,
    /// When set, both trashes are pruned after a clean sync — and only then:
    /// the call sites sit after `recordSuccess`, so a failed run can never
    /// prune what it may have just displaced. Prune failures are hygiene,
    /// not correctness, and never fail the sync that triggered them.
    prune: ?PruneOptions = null,
    /// Credential-derived redaction, owned (see `setSecretRedactions`):
    /// `name=` markers for every secret-designated option and the plaintext
    /// values themselves. The static marker table cannot know what an
    /// arbitrary catalogue backend calls its secrets.
    extra_markers: [][]u8 = &.{},
    extra_values: [][]u8 = &.{},

    pub fn init(gpa: Allocator, io: Io, client: *rc.Client) Engine {
        return .{ .gpa = gpa, .io = io, .client = client };
    }

    pub fn deinit(self: *Engine) void {
        self.clearSecretRedactions();
        self.clearLastError();
        self.* = undefined;
    }

    /// Replace the credential-derived redaction set: `names` are the
    /// secret-designated option names (struck as `name=` wherever an error
    /// or log prints them, obscured values included), `values` their
    /// plaintext values (struck wherever they appear; shorter than
    /// `min_secret_value_len` is skipped). Called by the owner at every job
    /// start from the freshly loaded credentials — the same cadence the
    /// daemon's config is re-applied on — and with empty slices when no
    /// credentials document exists. The engine owns the copies.
    pub fn setSecretRedactions(
        self: *Engine,
        names: []const []const u8,
        values: []const []const u8,
    ) Allocator.Error!void {
        var markers: std.ArrayList([]u8) = .empty;
        errdefer {
            for (markers.items) |item| self.gpa.free(item);
            markers.deinit(self.gpa);
        }
        for (names) |name| {
            const marker = try std.fmt.allocPrint(self.gpa, "{s}=", .{name});
            errdefer self.gpa.free(marker);
            try markers.append(self.gpa, marker);
        }

        var owned_values: std.ArrayList([]u8) = .empty;
        errdefer {
            for (owned_values.items) |item| self.gpa.free(item);
            owned_values.deinit(self.gpa);
        }
        for (values) |value| {
            if (value.len < min_secret_value_len) continue;
            const copy = try self.gpa.dupe(u8, value);
            errdefer self.gpa.free(copy);
            try owned_values.append(self.gpa, copy);
        }

        const marker_slice = try markers.toOwnedSlice(self.gpa);
        errdefer {
            for (marker_slice) |item| self.gpa.free(item);
            self.gpa.free(marker_slice);
        }
        const value_slice = try owned_values.toOwnedSlice(self.gpa);

        self.clearSecretRedactions();
        self.extra_markers = marker_slice;
        self.extra_values = value_slice;
    }

    fn clearSecretRedactions(self: *Engine) void {
        for (self.extra_markers) |item| self.gpa.free(item);
        self.gpa.free(self.extra_markers);
        for (self.extra_values) |item| self.gpa.free(item);
        self.gpa.free(self.extra_values);
        self.extra_markers = &.{};
        self.extra_values = &.{};
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
        self.pruneBothTrashes(ctx);
        self.refreshCatalogue(ctx);
        return run_id;
    }

    /// Bring the cached provider catalogue up to date, best-effort, after a
    /// clean sync. Nothing here can fail the run: an rclone that will not
    /// answer, a disk that will not take the file, or a reply this build
    /// cannot read all leave the previous cache exactly as it was.
    fn refreshCatalogue(self: *Engine, ctx: RunContext) void {
        if (!self.refresh_catalogue) return;
        _ = catalogue.refreshCache(self.gpa, self.io, self.client, ctx.game_dir) catch {};
    }

    /// Prune both sides, best-effort, after a clean finish. Every failure is
    /// swallowed: a sync that succeeded stays succeeded.
    fn pruneBothTrashes(self: *Engine, ctx: RunContext) void {
        const opts = self.prune orelse return;
        const now_unix = Io.Clock.now(.real, self.io).toSeconds();

        if (path.join(self.gpa, &.{ ctx.path1, plan.local_trash_dir_name })) |trash_dir| {
            defer self.gpa.free(trash_dir);
            _ = pruneTrash(self.gpa, self.io, trash_dir, opts, now_unix) catch {};
        } else |_| {}

        _ = self.pruneRemoteTrash(ctx.remote, ctx.profile, opts, now_unix) catch {};
    }

    /// Prune the remote trash, whole run directories at a time, via
    /// `operations/list` + `operations/purge`. A missing trash root reports
    /// zero of each rather than failing: an empty remote is normal.
    pub fn pruneRemoteTrash(
        self: *Engine,
        remote: []const u8,
        profile: []const u8,
        opts: PruneOptions,
        now_unix: i64,
    ) (rc.RcError || Allocator.Error)!PruneReport {
        var report: PruneReport = .{};

        const trash_fs = try std.fmt.allocPrint(self.gpa, "{s}:trash/{s}", .{ remote, profile });
        defer self.gpa.free(trash_fs);

        var names: std.ArrayList([]u8) = .empty;
        defer {
            for (names.items) |name| self.gpa.free(name);
            names.deinit(self.gpa);
        }

        {
            var object: std.json.ObjectMap = .empty;
            defer object.deinit(self.gpa);
            try object.put(self.gpa, "fs", .{ .string = trash_fs });
            try object.put(self.gpa, "remote", .{ .string = "" });
            var opt: std.json.ObjectMap = .empty;
            defer opt.deinit(self.gpa);
            try opt.put(self.gpa, "dirsOnly", .{ .bool = true });
            try object.put(self.gpa, "opt", .{ .object = opt });

            var reply = self.client.call("operations/list", .{ .object = object }) catch |err|
                switch (err) {
                    // No trash root yet — nothing has ever been displaced.
                    error.RcFailed => return report,
                    else => |e| return e,
                };
            defer reply.deinit();

            const top = switch (reply.value) {
                .object => |o| o,
                else => return error.BadJson,
            };
            const list = top.get("list") orelse return report;
            const entries = switch (list) {
                .array => |a| a,
                else => return error.BadJson,
            };
            for (entries.items) |item| {
                const entry = switch (item) {
                    .object => |o| o,
                    else => continue,
                };
                const is_dir = entry.get("IsDir") orelse continue;
                if (is_dir != .bool or !is_dir.bool) continue;
                const name_value = entry.get("Name") orelse continue;
                const name = switch (name_value) {
                    .string => |s| s,
                    else => continue,
                };
                if (runIdTimestamp(name) == null) continue;
                try names.append(self.gpa, try self.gpa.dupe(u8, name));
            }
        }

        sortNewestFirst(names.items);

        for (names.items, 0..) |name, index| {
            if (keepRun(name, index, opts, now_unix)) {
                report.kept += 1;
                continue;
            }
            var object: std.json.ObjectMap = .empty;
            defer object.deinit(self.gpa);
            try object.put(self.gpa, "fs", .{ .string = trash_fs });
            try object.put(self.gpa, "remote", .{ .string = name });
            var reply = self.client.call("operations/purge", .{ .object = object }) catch {
                report.kept += 1;
                continue;
            };
            reply.deinit();
            report.removed += 1;
        }
        return report;
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

    /// What `testConnection` answers.
    pub const TestResult = struct {
        ok: bool,
        /// `.unknown` when ok; the classified failure otherwise — the same
        /// vocabulary the sync path speaks, so the dialog needs one voice.
        outcome: Outcome,
    };

    /// An `operations/list` of the remote's root, then a write probe under
    /// it, all under a tightened deadline: tell the player what is wrong
    /// *before* the next sync discovers it. Blocking, like everything here
    /// — the worker runs it. The listing proves the credentials resolve;
    /// the probe proves the folder accepts writes and deletes, which
    /// bisync requires and which a read-only share or write-only token
    /// would otherwise reveal mid-sync. Backend-agnostic by construction:
    /// if this ever needs a per-backend branch, something upstream is
    /// hardcoded, and that is a bug there, not a case to add here.
    ///
    /// Unlike bisync, a plain operation returns its real cause in the rc
    /// reply, so classification reads `lastFailure` directly; there is no
    /// run log. The stored error text passes through the same redaction as
    /// every other failure.
    pub fn testConnection(self: *Engine, remote: []const u8) Allocator.Error!TestResult {
        self.clearLastError();
        self.last_outcome = .unknown;

        // A connection test is a settings-screen probe, not a transfer: it
        // deserves a tighter budget than a sync POST.
        const saved = self.client.deadline;
        self.client.deadline = .{ .connect_ms = 5_000, .read_ms = 10_000 };
        defer self.client.deadline = saved;

        const fs_spec = try std.fmt.allocPrint(self.gpa, "{s}:", .{remote});
        defer self.gpa.free(fs_spec);

        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        try object.put(self.gpa, "fs", .{ .string = fs_spec });
        try object.put(self.gpa, "remote", .{ .string = "" });
        var opt: std.json.ObjectMap = .empty;
        defer opt.deinit(self.gpa);
        try opt.put(self.gpa, "dirsOnly", .{ .bool = true });
        try object.put(self.gpa, "opt", .{ .object = opt });
        // Without this, backends that retry internally (S3 above all) chew
        // through their backoff schedule against a dead endpoint until the
        // POST deadline fires, and an unreachable server misclassifies as a
        // timeout. One attempt, tightly bounded, is what a probe means.
        var config: std.json.ObjectMap = .empty;
        defer config.deinit(self.gpa);
        try config.put(self.gpa, "Retries", .{ .integer = 1 });
        try config.put(self.gpa, "LowLevelRetries", .{ .integer = 1 });
        try config.put(self.gpa, "ConnectTimeout", .{ .string = "3s" });
        try config.put(self.gpa, "Timeout", .{ .string = "5s" });
        try object.put(self.gpa, "_config", .{ .object = config });

        var reply = self.client.call("operations/list", .{ .object = object }) catch |err| {
            const outcome: Outcome = switch (err) {
                error.RcFailed => blk: {
                    const failure = self.client.lastFailure() orelse break :blk .unknown;
                    self.recordError(failure.message, null);
                    break :blk classify(failure, "");
                },
                else => other: {
                    self.recordError(@errorName(err), null);
                    break :other classifyTransport(err);
                },
            };
            self.last_outcome = outcome;
            return .{ .ok = false, .outcome = outcome };
        };
        reply.deinit();
        return self.probeWritability(remote);
    }

    /// The fixed half of the probe object's name. Fixed so a leftover is
    /// recognisably ours; the nonce keeps two machines probing the same
    /// folder apart.
    pub const probe_prefix = "bk-write-probe-";

    /// Write a probe object under the remote's root, read it back, delete
    /// it. Runs after a successful listing, so a refusal here is about
    /// writability, not credentials or reachability — whatever status code
    /// it wears — and maps to `.remote_unwritable`. The probe is deleted
    /// on every path where deletion is permitted; when the delete itself
    /// is refused, the error text names the exact file left behind,
    /// because demanding cleanup is impossible in precisely the condition
    /// under test.
    fn probeWritability(self: *Engine, remote: []const u8) Allocator.Error!TestResult {
        if (self.cancelled()) {
            self.recordError("Cancelled", null);
            return .{ .ok = false, .outcome = .unknown };
        }

        var raw: [8]u8 = undefined;
        self.io.randomSecure(&raw) catch self.io.random(&raw);
        const nonce = std.fmt.bytesToHex(raw, .lower);

        const probe_name = try std.fmt.allocPrint(self.gpa, probe_prefix ++ "{s}.txt", .{&nonce});
        defer self.gpa.free(probe_name);
        const content = try std.fmt.allocPrint(
            self.gpa,
            "Blitzkrieg cloud sync write probe {s}. Safe to delete.\n",
            .{&nonce},
        );
        defer self.gpa.free(content);

        const temp_dir = try tempDirPath(self.gpa);
        defer self.gpa.free(temp_dir);
        const local_source = try path.join(self.gpa, &.{ temp_dir, probe_name });
        defer self.gpa.free(local_source);

        Io.Dir.cwd().writeFile(self.io, .{
            .sub_path = local_source,
            .data = content,
            .flags = .{ .truncate = true },
        }) catch {
            self.recordError("the probe file could not be written locally", null);
            return .{ .ok = false, .outcome = .unknown };
        };
        defer Io.Dir.cwd().deleteFile(self.io, local_source) catch {};

        const fs_spec = try std.fmt.allocPrint(self.gpa, "{s}:", .{remote});
        defer self.gpa.free(fs_spec);

        // Up. `copyfile` from the local file, not `uploadfile`: the rc
        // client posts JSON and nothing else, and multipart would mean a
        // second transport for the sake of a probe.
        self.probeOperation("operations/copyfile", &.{
            .{ "srcFs", temp_dir },
            .{ "srcRemote", probe_name },
            .{ "dstFs", fs_spec },
            .{ "dstRemote", probe_name },
        }) catch |err| {
            if (err == error.OutOfMemory) return error.OutOfMemory;
            const outcome = self.probeFailure(@errorCast(err), "the service refused the write probe");
            self.last_outcome = outcome;
            return .{ .ok = false, .outcome = outcome };
        };

        // From here on the object exists remotely: every following failure
        // still attempts the delete, and only then reports.
        var verdict: ?Outcome = null;

        // Back down under a distinct name, and byte-compared: a service
        // that acknowledges a write it lost would otherwise pass.
        const back_name = try std.fmt.allocPrint(self.gpa, "{s}.back", .{probe_name});
        defer self.gpa.free(back_name);
        const local_back = try path.join(self.gpa, &.{ temp_dir, back_name });
        defer self.gpa.free(local_back);
        defer Io.Dir.cwd().deleteFile(self.io, local_back) catch {};

        self.probeOperation("operations/copyfile", &.{
            .{ "srcFs", fs_spec },
            .{ "srcRemote", probe_name },
            .{ "dstFs", temp_dir },
            .{ "dstRemote", back_name },
        }) catch |err| {
            if (err == error.OutOfMemory) return error.OutOfMemory;
            verdict = self.probeFailure(@errorCast(err), "the probe could not be read back");
        };
        if (verdict == null) {
            const written = Io.Dir.cwd().readFileAlloc(self.io, local_back, self.gpa, .limited(4096)) catch null;
            defer if (written) |bytes| self.gpa.free(bytes);
            if (written == null or !std.mem.eql(u8, written.?, content)) {
                self.recordError("the probe did not read back what was written", null);
                verdict = .remote_unwritable;
            }
        }

        self.probeOperation("operations/deletefile", &.{
            .{ "fs", fs_spec },
            .{ "remote", probe_name },
        }) catch |err| {
            // The leftover report wins over an earlier read-back verdict:
            // it is the one message that asks the player to act, and the
            // probe file may now be sitting in their folder either way.
            if (err == error.OutOfMemory) return error.OutOfMemory;
            const detail: []const u8 = switch (err) {
                error.RcFailed => if (self.client.lastFailure()) |failure| failure.message else "",
                else => @errorName(err),
            };
            const text = std.fmt.allocPrint(
                self.gpa,
                "the service accepted the probe file but refused to delete it, which sync needs; " ++
                    "remove '{s}' from the configured folder yourself. {s}",
                .{ probe_name, detail },
            ) catch null;
            if (text) |owned| {
                defer self.gpa.free(owned);
                self.recordError(owned, null);
            } else {
                self.recordError("the service accepted the probe file but refused to delete it", null);
            }
            verdict = .remote_unwritable;
        };

        if (verdict) |outcome| {
            self.last_outcome = outcome;
            return .{ .ok = false, .outcome = outcome };
        }
        return .{ .ok = true, .outcome = .unknown };
    }

    /// One rc call of the probe: single-attempt and tightly bounded, like
    /// the listing — a probe that grinds through backoff schedules is not
    /// answering the question it was asked.
    fn probeOperation(
        self: *Engine,
        method: []const u8,
        pairs: []const [2][]const u8,
    ) (rc.RcError || Allocator.Error)!void {
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        for (pairs) |entry| try object.put(self.gpa, entry[0], .{ .string = entry[1] });
        var config: std.json.ObjectMap = .empty;
        defer config.deinit(self.gpa);
        try config.put(self.gpa, "Retries", .{ .integer = 1 });
        try config.put(self.gpa, "LowLevelRetries", .{ .integer = 1 });
        try config.put(self.gpa, "ConnectTimeout", .{ .string = "3s" });
        try config.put(self.gpa, "Timeout", .{ .string = "5s" });
        try object.put(self.gpa, "_config", .{ .object = config });

        var reply = try self.client.call(method, .{ .object = object });
        reply.deinit();
    }

    /// Classify a probe-step failure. The listing already succeeded, so an
    /// rc-level refusal is the service refusing the operation — that is
    /// `.remote_unwritable`, whatever status it wears — while transport
    /// errors keep their own meanings: a daemon or network that died
    /// mid-probe says nothing about writability.
    fn probeFailure(self: *Engine, err: rc.RcError, fallback_text: []const u8) Outcome {
        switch (err) {
            error.RcFailed => {
                if (self.client.lastFailure()) |failure| {
                    self.recordError(failure.message, null);
                } else {
                    self.recordError(fallback_text, null);
                }
                return .remote_unwritable;
            },
            else => {
                self.recordError(@errorName(err), null);
                return classifyTransport(err);
            },
        }
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

    /// Classify a failure and store its user-facing text. Everything stored
    /// here is redacted — the error text as much as the log tail, because
    /// rclone repeats the filesystem name (connection-string secrets
    /// included) in the error message itself, and a connection-test failure
    /// has no log at all. Classification reads the raw text; only the
    /// stored copy is scrubbed. On allocation failure the text is dropped,
    /// never kept raw.
    pub fn recordError(self: *Engine, error_text: []const u8, run_log: ?[]const u8) void {
        self.clearLastError();
        self.last_outcome = classify(
            .{ .message = error_text, .status = 500 },
            run_log orelse "",
        );
        const extra: ExtraRedactions = .{
            .markers = self.extra_markers,
            .values = self.extra_values,
        };
        const safe_text = redactedText(self.gpa, error_text, extra) catch null;
        self.last_error_owned = owned: {
            const text = safe_text orelse break :owned null;
            const log = run_log orelse break :owned text;
            defer self.gpa.free(text);
            const tail = redactedLogTail(self.gpa, log, extra) catch
                break :owned self.gpa.dupe(u8, text) catch null;
            defer self.gpa.free(tail);
            break :owned std.fmt.allocPrint(self.gpa, "{s}\n{s}", .{ text, tail }) catch null;
        };
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

// -- Trash retention ----------------------------------------------------------
//
// Two trashes, one per side, one directory per run — that layout is P01-M04's;
// this section keeps both bounded without ever pruning what protects a
// player. Retention works on whole run directories, never individual files:
// a run is the unit of recovery, and half a run is not a recovery. At least
// `min_keep_runs` most-recent runs survive regardless of age, so a burst of
// syncs cannot age out every copy at once. And pruning runs only after a
// clean finish — the sole call sites in this module sit after
// `recordSuccess` — so a failed run can never delete the files it may have
// just displaced.

pub const PruneOptions = struct {
    /// Runs older than this are eligible for pruning...
    max_age_days: u32 = 30,
    /// ...except the newest `min_keep_runs`, which are kept no matter what.
    min_keep_runs: usize = 5,
};

pub const PruneReport = struct {
    kept: usize = 0,
    removed: usize = 0,
};

/// Prune one side's trash directory on the local filesystem. Only entries
/// whose names parse as run ids are considered at all: anything else in the
/// trash was not put there by a run, and this code never deletes what it did
/// not create.
pub fn pruneTrash(
    gpa: Allocator,
    io: Io,
    trash_dir: []const u8,
    opts: PruneOptions,
    now_unix: i64,
) Allocator.Error!PruneReport {
    var report: PruneReport = .{};

    var dir = Io.Dir.cwd().openDir(io, trash_dir, .{ .iterate = true }) catch return report;
    defer dir.close(io);

    var names: std.ArrayList([]u8) = .empty;
    defer {
        for (names.items) |name| gpa.free(name);
        names.deinit(gpa);
    }

    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (entry.kind != .directory) continue;
        if (runIdTimestamp(entry.name) == null) continue;
        try names.append(gpa, try gpa.dupe(u8, entry.name));
    }

    sortNewestFirst(names.items);

    for (names.items, 0..) |name, index| {
        if (keepRun(name, index, opts, now_unix)) {
            report.kept += 1;
            continue;
        }
        const run_path = try path.join(gpa, &.{ trash_dir, name });
        defer gpa.free(run_path);
        Io.Dir.cwd().deleteTree(io, run_path) catch {
            // A run that would not delete is a run still protecting someone.
            report.kept += 1;
            continue;
        };
        report.removed += 1;
    }
    return report;
}

fn sortNewestFirst(names: [][]u8) void {
    // Run ids are zero-padded UTC stamps: byte order is time order.
    std.mem.sort([]u8, names, {}, struct {
        fn newerFirst(_: void, a: []u8, b: []u8) bool {
            return std.mem.order(u8, a, b) == .gt;
        }
    }.newerFirst);
}

fn keepRun(name: []const u8, index: usize, opts: PruneOptions, now_unix: i64) bool {
    if (index < opts.min_keep_runs) return true;
    const stamp = runIdTimestamp(name) orelse return true;
    const age_seconds = now_unix - stamp;
    return age_seconds <= @as(i64, opts.max_age_days) * 86_400;
}

/// The UTC second a run id names, or null when the name is not a run id.
/// The inverse of `plan.runId`'s stamp half.
pub fn runIdTimestamp(name: []const u8) ?i64 {
    if (name.len != 25) return null;
    if (name[8] != 'T' or name[15] != 'Z' or name[16] != '-') return null;
    for (name[0..8]) |c| if (!std.ascii.isDigit(c)) return null;
    for (name[9..15]) |c| if (!std.ascii.isDigit(c)) return null;
    for (name[17..]) |c| if (!std.ascii.isHex(c)) return null;

    const year = std.fmt.parseInt(i64, name[0..4], 10) catch return null;
    const month = std.fmt.parseInt(u32, name[4..6], 10) catch return null;
    const day = std.fmt.parseInt(u32, name[6..8], 10) catch return null;
    const hour = std.fmt.parseInt(i64, name[9..11], 10) catch return null;
    const minute = std.fmt.parseInt(i64, name[11..13], 10) catch return null;
    const second = std.fmt.parseInt(i64, name[13..15], 10) catch return null;
    if (month < 1 or month > 12 or day < 1) return null;
    // Against the month's real length, not a flat 31: `runId` can never
    // emit Feb 31, so a name wearing an impossible date is foreign by
    // definition — and `daysFromCivil` would happily normalise it into
    // March, making a directory we did not create eligible for pruning.
    if (day > daysInMonth(year, month)) return null;
    if (hour > 23 or minute > 59 or second > 59) return null;

    return daysFromCivil(year, month, day) * 86_400 + hour * 3_600 + minute * 60 + second;
}

fn daysInMonth(year: i64, month: u32) u32 {
    return switch (month) {
        1, 3, 5, 7, 8, 10, 12 => 31,
        4, 6, 9, 11 => 30,
        2 => if (isLeapYear(year)) @as(u32, 29) else 28,
        else => unreachable, // month is checked to be 1..12 before this
    };
}

fn isLeapYear(year: i64) bool {
    return @mod(year, 4) == 0 and (@mod(year, 100) != 0 or @mod(year, 400) == 0);
}

/// Howard Hinnant's days-from-civil: days since 1970-01-01 for a proleptic
/// Gregorian date.
fn daysFromCivil(year: i64, month: u32, day: u32) i64 {
    const y = if (month <= 2) year - 1 else year;
    const era = @divFloor(y, 400);
    const yoe = y - era * 400;
    const mp = @mod(@as(i64, month) + 9, 12);
    const doy = @divFloor(153 * mp + 2, 5) + day - 1;
    const doe = yoe * 365 + @divFloor(yoe, 4) - @divFloor(yoe, 100) + doy;
    return era * 146_097 + doe - 719_468;
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
    /// The cloud answered, but the configured root — the bucket, the WebDAV
    /// directory — is not there. Usually a typo in the credentials; on a
    /// brand-new account it just means the first pairing has not created it
    /// yet, and the dialog should say so rather than alarm.
    remote_missing,
    /// The cloud lists but refused the write probe — or accepted it and
    /// refused the delete, which bisync needs just as much. Only the
    /// connection test produces this: a sync failure has richer log
    /// evidence and classifies through the patterns above.
    remote_unwritable,
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
        // A missing root is almost always a mistyped bucket or path; the
        // fresh-account case is the dialog's copy to soften. An unwritable
        // remote is a property of the account or folder chosen — the
        // credentials dialog is where a different one gets picked.
        .auth_failed, .remote_missing, .remote_unwritable => .open_credentials,
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
        // A credential missing outright — rclone refuses to build the fs.
        // Captured live: a cleared secret probes as exactly this.
        "secret_access_key not found",
        "access_key_id not found",
        // An OAuth token that can no longer be refreshed. Captured live
        // (v1.75.0, revoked refresh token): `couldn't fetch token:
        // invalid_grant: maybe token expired? - try refreshing with
        // "rclone config reconnect …"`. auth_failed routes the player to
        // the credentials dialog, which is where a new sign-in starts.
        "couldn't fetch token",
        "invalid_grant",
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

    if (containsIgnoreCase(text, "directory not found")) return .remote_missing;

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

/// A secret value shorter than this is not struck by the value scan: three
/// bytes cannot be recognised as a leaked secret, and striking them would
/// censor arbitrary letters out of the message.
pub const min_secret_value_len = 4;

/// What the loaded credentials designate secret, beyond the static marker
/// table: option names (struck as `name=` like the built-in markers, which
/// catches encodings of the value this code cannot know — rclone's
/// obscured form above all) and plaintext values (struck wherever a server
/// or a log echoes them). Everything borrowed.
pub const ExtraRedactions = struct {
    markers: []const []const u8 = &.{},
    values: []const []const u8 = &.{},
};

/// The last `log_tail_lines` lines of `log`, with credential values struck
/// out. This is what may travel in a support report; the raw log never
/// leaves the machine through the ABI.
pub fn redactedLogTail(gpa: Allocator, log: []const u8, extra: ExtraRedactions) Allocator.Error![]u8 {
    return redactedText(gpa, lastLines(log, log_tail_lines), extra);
}

/// A copy of `text` with credential values struck out. Applied to every
/// string a failure stores — the rc error message no less than the log
/// tail, since rclone prints the filesystem name inside both.
pub fn redactedText(gpa: Allocator, text: []const u8, extra: ExtraRedactions) Allocator.Error![]u8 {
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(gpa);

    var index: usize = 0;
    scan: while (index < text.len) {
        for (redact_markers) |marker| {
            if (std.ascii.startsWithIgnoreCase(text[index..], marker)) {
                index = try strikeMarkedValue(gpa, &out, text, index, marker.len);
                continue :scan;
            }
        }
        for (extra.markers) |marker| {
            if (std.ascii.startsWithIgnoreCase(text[index..], marker)) {
                index = try strikeMarkedValue(gpa, &out, text, index, marker.len);
                continue :scan;
            }
        }
        try out.append(gpa, text[index]);
        index += 1;
    }

    var result = try out.toOwnedSlice(gpa);
    for (extra.values) |value| {
        if (value.len < min_secret_value_len) continue;
        result = try strikeValue(gpa, result, value);
    }
    return result;
}

/// The marker at `start` and its `[redacted]` placeholder appended to
/// `out`; returns the index past the swallowed value.
fn strikeMarkedValue(
    gpa: Allocator,
    out: *std.ArrayList(u8),
    text: []const u8,
    start: usize,
    marker_len: usize,
) Allocator.Error!usize {
    try out.appendSlice(gpa, text[start..][0..marker_len]);
    try out.appendSlice(gpa, redacted_placeholder);
    var index = start + marker_len;
    // Swallow the value: everything up to a delimiter that can end a
    // credential in a URL, connection string, header or config line.
    while (index < text.len) : (index += 1) {
        switch (text[index]) {
            ' ', ',', '"', '\'', ':', ';', '\n', '\r', ')', ']', '&' => break,
            else => {},
        }
    }
    return index;
}

/// Every occurrence of `value` in `text` replaced by the placeholder.
/// Takes ownership of `text`: the input is freed whenever a replacement
/// (or an error) makes a new buffer, so the caller keeps only the result.
fn strikeValue(gpa: Allocator, text: []u8, value: []const u8) Allocator.Error![]u8 {
    if (std.mem.indexOf(u8, text, value) == null) return text;
    defer gpa.free(text);

    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(gpa);
    var index: usize = 0;
    while (std.mem.indexOfPos(u8, text, index, value)) |hit| {
        try out.appendSlice(gpa, text[index..hit]);
        try out.appendSlice(gpa, redacted_placeholder);
        index = hit + value.len;
    }
    try out.appendSlice(gpa, text[index..]);
    return out.toOwnedSlice(gpa);
}

/// A writable local directory for the probe's source file: the platform
/// temp directory from the environment — libc's copy, because this code
/// is compiled into a library the game loads and never sees the
/// environment `main` was handed — with a plain fallback when nothing is
/// advertised. Owned by the caller.
fn tempDirPath(gpa: Allocator) Allocator.Error![]u8 {
    if (builtin.os.tag == .windows) {
        const environ: std.process.Environ = .{ .block = .global };
        var map = environ.createMap(gpa) catch |err| switch (err) {
            error.OutOfMemory => return error.OutOfMemory,
            else => return gpa.dupe(u8, "."),
        };
        defer map.deinit();
        for ([_][]const u8{ "TEMP", "TMP" }) |key| {
            if (map.get(key)) |value| {
                if (value.len != 0) return gpa.dupe(u8, value);
            }
        }
        return gpa.dupe(u8, ".");
    }
    for ([_][:0]const u8{ "TMPDIR", "TMP" }) |key| {
        if (std.c.getenv(key.ptr)) |value| {
            const span = std.mem.span(value);
            if (span.len != 0) return gpa.dupe(u8, span);
        }
    }
    return gpa.dupe(u8, "/tmp");
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
