//! The worker thread: every rc call runs here, so a stalled socket can never
//! cost the game a frame.
//!
//! `_async` is not enough. It makes the *rclone job* asynchronous server-side;
//! the initiating POST and every `job/status` POST are still synchronous HTTP
//! requests that block whoever sends them. Without this thread, the plan's
//! central invariant — no socket wait on the main thread, ever — has no
//! mechanism at all.
//!
//! The division of labour is strict:
//!
//! - The worker task owns the daemon, the `rc.Client` and the `Engine`
//!   exclusively. No other thread may touch them; there is no shared
//!   connection to reason about.
//! - The main thread gets exactly two operations: `begin`, which copies the
//!   job's strings and enqueues — daemon spawn and readiness happen inside
//!   the worker and are observed through `poll`, never awaited in `begin` —
//!   and `poll`, which copies the published snapshot under a futex and
//!   performs **no I/O of any kind**.
//! - rc traffic is paced by wall clock, not by frame rate: the job-status
//!   interval lives in `engine.zig`, so a 200 Hz menu polling every frame
//!   still produces four status calls a second, not two hundred.
//!
//! The rclone binary is not read out of any shared cache by pointer: the
//! worker asks its `BinarySource` for an **owned copy** at spawn time, and the
//! source's implementation (the ABI's discovery cache, in production) does the
//! copying under its own lock. A borrowed pointer into that cache can be freed
//! by a credentials save on the UI thread mid-spawn; an owned copy cannot.
//!
//! Shutdown is bounded: the cancel flag is checked between every bounded
//! phase of a run, and each in-flight POST carries its own deadline, so
//! `destroy` waits at most one deadline plus one poll interval — never for
//! the rclone job itself, which keeps running server-side and is reaped with
//! the daemon.

const std = @import("std");
const builtin = @import("builtin");
const backup = @import("backup.zig");
const catalogue = @import("catalogue.zig");
const creds = @import("creds.zig");
const rc = @import("rc.zig");
const daemon = @import("daemon.zig");
const engine = @import("engine.zig");
const oauth = @import("oauth.zig");
const plan = @import("plan.zig");

const Allocator = std.mem.Allocator;
const Io = std.Io;
const path = Io.Dir.path;

/// How often the idle worker looks for newly enqueued work. Also the upper
/// bound `destroy` waits on an idle worker.
const idle_poll_ms: u32 = 25;

/// New states are appended, never inserted: the ABI pins the numerics.
pub const State = enum(u8) { idle, starting, pairing, syncing, done, failed, testing, awaiting_input };

/// How the last finished job ended. `.none` until the first job finishes.
/// Appended, never reordered, for the same reason.
pub const Outcome = enum(u8) { none, paired, synced, failed, connection_ok, backups_listed, restore_staged, undo_done, catalogue_ready };

/// Reserved for the sync indicator; the worker publishes states today and
/// null progress, and nothing downstream may assume otherwise.
pub const Progress = struct {
    elapsed_ms: u64,
};

/// Longest error text the snapshot carries. Longer rclone output is truncated
/// here and preserved in full by the engine for P02-M03's classification.
pub const error_text_max = 512;

/// What `poll` returns: a value copy, safe to hold across frames, valid until
/// the caller drops it. The error text lives inline for exactly that reason —
/// a slice into worker-owned memory could be freed by the next transition.
pub const Snapshot = struct {
    state: State = .idle,
    outcome: Outcome = .none,
    progress: ?Progress = null,
    error_len: usize = 0,
    error_buf: [error_text_max]u8 = undefined,

    pub fn errorText(self: *const Snapshot) []const u8 {
        return self.error_buf[0..self.error_len];
    }
};

/// An owned copy of the resolved rclone path, on demand. The production
/// implementation copies out of the ABI's discovery cache under that cache's
/// lock and releases before returning, which is the P00-M04 contract this
/// type exists to enforce; tests hand back a fixture path.
pub const BinarySource = struct {
    context: ?*anyopaque = null,
    /// Returns an allocation of `gpa` the worker frees, or null when no
    /// usable rclone is known.
    resolve: *const fn (context: ?*anyopaque, gpa: Allocator) ?[]u8,
};

pub const JobKind = enum { pair, sync, test_connection, list_backups, restore_stage, restore_undo, fetch_catalogue, config_create };

/// What `ensureCatalogue` did. `.cached` means the caller may read the cache
/// now; `.fetching` means a job is running and the answer arrives through
/// `poll` as `.catalogue_ready` — or as `.failed`, which is not an error
/// state: the cache simply stays as it was and the dialog offers a retry.
pub const CatalogueState = enum { cached, fetching };

/// A job as the caller describes it. Every slice is copied by `begin`; none
/// needs to outlive the call.
pub const JobSpec = struct {
    kind: JobKind,
    /// The sync context. Empty for a job that has none — `.fetch_catalogue`
    /// describes the rclone binary, not any profile or remote.
    path1: []const u8 = "",
    remote: []const u8 = "",
    profile: []const u8 = "",
    profile_id: []const u8 = "",
    remote_fingerprint: []const u8 = "",
    /// Snapshot `config.cfg` after a clean sync — the `Cloud.Config.Backup`
    /// option, passed per job by the caller who owns option state. A failed
    /// snapshot never fails the sync that triggered it.
    backup_config: bool = false,
    /// This machine's name, for the per-host backup key. Sanitised in
    /// `backup.zig`.
    host: []const u8 = "",
    /// Retention applied after a successful snapshot: keep this many per
    /// host, newest always surviving.
    backup_keep_per_host: u32 = 10,
    /// For `.restore_stage`: the backup entry to download, as
    /// `bk_cloudsync_backup_entry` reported it.
    entry_id: []const u8 = "",
    /// For `.restore_stage`: how the apply step will treat the payload.
    restore_mode: backup.RestoreMode = .merge_keep_local_gfx,
};

pub const Options = struct {
    /// Where `cloudsync/` lives. Copied.
    game_dir: []const u8,
    /// A ready rc endpoint to use instead of spawning a daemon. Tests point
    /// this at stub servers; production leaves it null. Borrowed — the
    /// strings must outlive the worker.
    endpoint: ?rc.Endpoint = null,
    /// Per-POST deadlines for the worker's client; null keeps the defaults.
    deadline: ?rc.Deadline = null,
    /// How the worker obtains the rclone binary when it must spawn.
    binary_source: ?BinarySource = null,
    /// Where the short profile link lives. Tests point this at a fixture;
    /// production leaves the defaults (`%LOCALAPPDATA%\bk` and friends).
    /// Borrowed — the strings must outlive the worker.
    link_roots: plan.Roots = .{},
};

pub const CreateError = Allocator.Error || error{ConcurrencyUnavailable};

pub const BeginError = error{
    /// A job is already queued or running. One at a time is the contract;
    /// the caller polls to completion first.
    Busy,
} || Allocator.Error;

const JobBox = struct {
    arena: std.heap.ArenaAllocator,
    kind: JobKind,
    ctx: engine.RunContext,
    backup_config: bool,
    host: []const u8,
    backup_keep_per_host: u32,
    entry_id: []const u8,
    restore_mode: backup.RestoreMode,
};

pub const Worker = struct {
    gpa: Allocator,
    io: Io,
    game_dir: []u8,
    endpoint: ?rc.Endpoint,
    deadline: ?rc.Deadline,
    binary_source: ?BinarySource,
    link_roots: plan.Roots,

    mutex: Io.Mutex = .init,
    snapshot: Snapshot = .{},
    pending: ?*JobBox = null,

    stop_flag: std.atomic.Value(bool) = .init(false),
    cancel_flag: std.atomic.Value(bool) = .init(false),
    task: ?Io.Future(void) = null,

    /// The most recent `.list_backups` result, mutex-guarded, replaced by
    /// the next listing and freed on destroy. Readers copy out through
    /// `backupEntryJson`; nobody holds a pointer in.
    backup_list: ?backup.BackupList = null,

    /// The full text of the most recent failure, mutex-guarded: the
    /// snapshot's inline buffer carries one status line
    /// (`error_text_max`), while the engine's redacted 200-line support
    /// tail is far longer — this is where it survives for
    /// `errorDetailOwned`. Replaced by the next failure, cleared by the
    /// next success, freed on destroy.
    error_detail: ?[]u8 = null,

    /// The raw bytes of `cloud.credentials` as the current job applied
    /// them, gpa-owned. The read-back compares against this before
    /// touching the file: the dialog may save new credentials while a job
    /// runs, and folding the *previous* configuration's section into the
    /// *new* document would restore an old account's token over the one
    /// the player just chose. Null when the job found no document.
    applied_creds_raw: ?[]u8 = null,

    /// The config machine's mailboxes, mutex-guarded. While a
    /// `.config_create` job waits on a human, the pending question sits in
    /// `config_question` (the form's wire JSON plus an `error` key) with
    /// the snapshot at `.awaiting_input`; `answerConfig` fills
    /// `config_answer` and the job resumes. Both are cleared when the job
    /// ends, whatever way it ends.
    config_question: ?[]u8 = null,
    config_answer: ?[]u8 = null,

    /// Heap-allocated because the worker task holds `self` for its lifetime;
    /// the pointer must not move.
    pub fn create(gpa: Allocator, io: Io, options: Options) CreateError!*Worker {
        const self = try gpa.create(Worker);
        errdefer gpa.destroy(self);

        const game_dir = try gpa.dupe(u8, options.game_dir);
        errdefer gpa.free(game_dir);

        self.* = .{
            .gpa = gpa,
            .io = io,
            .game_dir = game_dir,
            .endpoint = options.endpoint,
            .deadline = options.deadline,
            .binary_source = options.binary_source,
            .link_roots = options.link_roots,
        };

        // `concurrent`, not `async`: the whole point is a task that runs
        // while the caller does not wait on it.
        self.task = io.concurrent(runWorker, .{self}) catch
            return error.ConcurrencyUnavailable;
        return self;
    }

    /// Cancel whatever is in flight, stop the task, tear down the session,
    /// free everything. Bounded: one POST deadline plus one poll interval,
    /// and a daemon still starting is abandoned at its next readiness probe
    /// rather than waited out.
    pub fn destroy(self: *Worker) void {
        self.stop_flag.store(true, .release);
        self.cancel_flag.store(true, .release);
        if (self.task) |*task| task.await(self.io);

        if (self.pending) |box| freeJob(self.gpa, box);
        if (self.backup_list) |*list| list.deinit();
        if (self.error_detail) |owned| self.gpa.free(owned);
        if (self.config_question) |owned| self.gpa.free(owned);
        if (self.config_answer) |owned| self.gpa.free(owned);
        if (self.applied_creds_raw) |owned| self.gpa.free(owned);
        self.gpa.free(self.game_dir);
        const gpa = self.gpa;
        self.* = undefined;
        gpa.destroy(self);
    }

    /// Enqueue one job and return immediately. The snapshot flips to
    /// `.starting` here so the very next `poll` already reflects the request;
    /// everything slow — spawn, readiness, the run itself — happens on the
    /// worker and is observed through `poll`.
    pub fn begin(self: *Worker, spec: JobSpec) BeginError!void {
        var box = try self.gpa.create(JobBox);
        errdefer self.gpa.destroy(box);
        box.arena = .init(self.gpa);
        errdefer box.arena.deinit();

        const arena = box.arena.allocator();
        box.kind = spec.kind;
        box.ctx = .{
            .path1 = try arena.dupe(u8, spec.path1),
            .remote = try arena.dupe(u8, spec.remote),
            .profile = try arena.dupe(u8, spec.profile),
            .game_dir = self.game_dir,
            .profile_id = try arena.dupe(u8, spec.profile_id),
            .remote_fingerprint = try arena.dupe(u8, spec.remote_fingerprint),
        };
        box.backup_config = spec.backup_config;
        box.host = try arena.dupe(u8, spec.host);
        box.backup_keep_per_host = spec.backup_keep_per_host;
        box.entry_id = try arena.dupe(u8, spec.entry_id);
        box.restore_mode = spec.restore_mode;

        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);

        if (self.pending != null) return error.Busy;
        switch (self.snapshot.state) {
            .starting, .pairing, .syncing, .testing, .awaiting_input => return error.Busy,
            .idle, .done, .failed => {},
        }

        self.cancel_flag.store(false, .release);
        self.pending = box;
        self.snapshot.state = .starting;
        self.snapshot.error_len = 0;
    }

    /// A value copy of the current snapshot. No I/O, no allocation; the only
    /// wait is a futex held for the length of a struct copy.
    pub fn poll(self: *Worker) Snapshot {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        return self.snapshot;
    }

    /// Abandon the wait on the current run. The job keeps running
    /// server-side; the worker reports `.failed` with a cancellation text.
    pub fn cancel(self: *Worker) void {
        self.cancel_flag.store(true, .release);
    }

    /// Make sure a provider catalogue is available, without ever blocking the
    /// caller on socket work.
    ///
    /// The cheap answer first: the cached document's version stamp is a local
    /// file read, no daemon and no rc call, so a cache that already describes
    /// `running` is reported straight back and nothing is enqueued. Only a
    /// miss or a version change becomes a job — and a job it must be, because
    /// fetching means spawning a daemon and making an rc call, which is
    /// exactly the wait this thread exists to keep off the caller.
    ///
    /// `.fetching` is reported through the ordinary snapshot, so a caller
    /// polls it like any other job and `cancel` abandons it like any other
    /// job: a player who opens the dialog and closes it again strands
    /// nothing.
    pub fn ensureCatalogue(self: *Worker, running: daemon.Version) BeginError!CatalogueState {
        const stamp = try catalogue.cachedVersion(self.gpa, self.io, self.game_dir);
        if (catalogue.matchesVersion(stamp, running)) return .cached;
        try self.begin(.{ .kind = .fetch_catalogue });
        return .fetching;
    }

    /// Serialise entry `index` of the most recent backup listing into `out`
    /// as NUL-terminated JSON and return its length, or null when there is
    /// no listing or no such entry. A value copy under the mutex — the
    /// listing can be replaced the moment this returns.
    pub fn backupEntryJson(self: *Worker, index: usize, out: []u8) ?usize {
        if (out.len == 0) return null;
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);

        const list = self.backup_list orelse return null;
        if (index >= list.entries.len) return null;
        const entry = list.entries[index];

        var writer: std.Io.Writer = .fixed(out[0 .. out.len - 1]);
        var json: std.json.Stringify = .{ .writer = &writer };
        writeEntry(&json, entry) catch return null;
        const length = writer.buffered().len;
        out[length] = 0;
        return length;
    }

    // -- worker side ---------------------------------------------------------

    /// The engine session, built lazily on the first job and torn down when
    /// the task ends. Lives in the task's frame: `Engine` borrows the client
    /// by pointer, so the storage must be stable for the session's lifetime.
    const Session = struct {
        daemon_box: ?daemon.Daemon = null,
        client: ?rc.Client = null,
        eng: ?engine.Engine = null,

        fn deinit(self: *Session) void {
            if (self.eng) |*e| e.deinit();
            if (self.client) |*c| c.deinit();
            if (self.daemon_box) |*d| d.shutdown();
            self.* = .{};
        }
    };

    fn runWorker(self: *Worker) void {
        var session: Session = .{};
        defer session.deinit();

        while (!self.stop_flag.load(.acquire)) {
            const box = self.takePending() orelse {
                sleepMs(self.io, idle_poll_ms);
                continue;
            };
            defer freeJob(self.gpa, box);
            self.execute(&session, box);
        }

        // The last chance to ask: the deferred teardown destroys the rc
        // client, and a token rclone refreshed during the final job is
        // lost unless it is read back while there is still someone to
        // answer.
        self.readBackConfig(&session);
    }

    fn takePending(self: *Worker) ?*JobBox {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        const box = self.pending;
        self.pending = null;
        return box;
    }

    fn execute(self: *Worker, session: *Session, box: *JobBox) void {
        // Undo is purely local — no daemon, no client — and running it as a
        // job is precisely the point: the worker's one-job-at-a-time
        // discipline is the operation slot that keeps it from racing an
        // in-flight restore download over `ACTIVE`.
        if (box.kind == .restore_undo) {
            self.publishState(.testing);
            _ = backup.undoRestore(self.gpa, self.io, box.ctx.path1) catch |err| {
                self.publishFailureText(@errorName(err));
                return;
            };
            self.publishDone(.undo_done);
            return;
        }

        // The catalogue describes the rclone binary, not any configured
        // remote, so this job takes the daemon and skips everything that
        // belongs to a sync: no short link, and no credentials. Applying a
        // broken credential here would let it hide the very provider list the
        // player needs in order to fix it.
        if (box.kind == .fetch_catalogue) {
            if (self.cancelled()) {
                self.publishFailureText("Cancelled");
                return;
            }
            const eng = self.ensureSession(session) orelse return;
            self.publishState(.testing);
            if (self.cancelled()) {
                self.publishFailureText("Cancelled");
                return;
            }
            // `refreshCache` asks the daemon its version first and only
            // fetches when the stamp no longer matches, so a job enqueued
            // against a cache that was filled meanwhile costs one small call.
            _ = catalogue.refreshCache(self.gpa, self.io, eng.client, self.game_dir) catch |err| {
                self.publishFailureText(catalogueFailureText(err));
                return;
            };
            self.publishDone(.catalogue_ready);
            return;
        }

        // Path1 must never be the install path: bisync spends the 255-byte
        // session-name budget on Path1's absolute bytes, so every
        // transfer-shaped job runs through the short link
        // (`<linkRoot>/p<slot>`), created or reused here — which also makes
        // the budget projection measure the exact bytes rclone will mangle,
        // and holds Path1 constant so only the profile name (Path2) varies.
        // A machine where the link cannot be made falls back to the raw
        // path; the budget check still refuses a name past the limit.
        var short_link: ?plan.ShortLink = null;
        defer if (short_link) |*link| link.deinit(self.gpa);
        if (box.kind == .pair or box.kind == .sync or box.kind == .restore_stage) {
            if (plan.ensureShortLinkIn(self.gpa, self.io, self.link_roots, box.ctx.path1)) |link| {
                short_link = link;
                box.ctx.path1 = link.path;
            } else |_| {}
        }

        const eng = self.ensureSession(session) orelse return;

        // Credentials are re-applied to the daemon at every job start, not
        // once per session: the dialog edits `cloud.credentials` while the
        // daemon keeps running, and a connection test must probe what was
        // just saved — not whatever the session was built with. (The daemon
        // BINARY is still the session's: an rclone_path change takes effect
        // on the next daemon, not mid-session.)
        if (!self.applyCredentials(session)) return;

        // And read back after every session job, failed ones included: the
        // token refresh happens before the operation that then failed, and
        // a refreshed token never read back is lost with the daemon.
        defer self.readBackConfig(session);

        self.publishState(switch (box.kind) {
            .pair => .pairing,
            // Transfer-shaped work reads as syncing; probe-shaped fetches
            // share the testing state — nothing a UI needs to distinguish
            // while it spins. The config machine spins the same way
            // between questions; `.awaiting_input` is published only when
            // one is actually waiting.
            .sync, .restore_stage => .syncing,
            .test_connection, .list_backups, .config_create => .testing,
            // Both handled above and returned from: undo before the session
            // exists, the catalogue fetch immediately after it.
            .restore_undo, .fetch_catalogue => unreachable,
        });

        switch (box.kind) {
            .pair => {
                var outcome = eng.pair(box.ctx) catch |err| {
                    self.publishRunFailure(eng, err);
                    return;
                };
                outcome.deinit(self.gpa);
                self.publishDone(.paired);
            },
            .sync => {
                const run_id = eng.syncOnce(box.ctx) catch |err| {
                    self.publishRunFailure(eng, err);
                    return;
                };
                self.gpa.free(run_id);
                // After a clean finish only, and best-effort: the sync that
                // succeeded stays succeeded whatever the snapshot does.
                if (box.backup_config) {
                    // The active profile owns config.cfg (iMainInternal's
                    // ResolveConfigFileName), so the snapshot source is the
                    // profile directory — path1 — not the game root.
                    const snapshot: ?[]u8 = backup.snapshotConfig(self.gpa, self.io, eng.client, .{
                        .config_dir = box.ctx.path1,
                        .remote = box.ctx.remote,
                        .profile = box.ctx.profile,
                        .host = box.host,
                    }) catch null;
                    if (snapshot) |name| {
                        self.gpa.free(name);
                        // Retention only after a snapshot actually landed —
                        // a failed upload must not become the trigger that
                        // deletes old copies.
                        _ = backup.pruneBackups(
                            self.gpa,
                            eng.client,
                            box.ctx.remote,
                            box.ctx.profile,
                            box.backup_keep_per_host,
                        ) catch 0;
                    }
                }
                self.publishDone(.synced);
            },
            .restore_stage => {
                const nonce = backup.stageRestore(
                    self.gpa,
                    self.io,
                    eng.client,
                    box.ctx.path1,
                    box.ctx.remote,
                    box.ctx.profile,
                    box.entry_id,
                    box.restore_mode,
                ) catch |err| {
                    self.publishFailure(err, eng.lastErrorText());
                    return;
                };
                self.gpa.free(nonce);
                self.publishDone(.restore_staged);
            },
            .list_backups => {
                const list = backup.listBackups(self.gpa, eng.client, box.ctx.remote, box.ctx.profile) catch |err| {
                    self.publishFailure(err, eng.lastErrorText());
                    return;
                };
                self.mutex.lockUncancelable(self.io);
                if (self.backup_list) |*previous| previous.deinit();
                self.backup_list = list;
                self.mutex.unlock(self.io);
                self.publishDone(.backups_listed);
            },
            .restore_undo, .fetch_catalogue => unreachable, // handled above
            .config_create => self.runConfigCreate(eng),
            .test_connection => {
                const result = eng.testConnection(box.ctx.remote) catch {
                    self.publishFailureText("out of memory testing the connection");
                    return;
                };
                if (result.ok) {
                    self.publishDone(.connection_ok);
                } else {
                    // The classified outcome leads the text, so the caller
                    // can branch on it while the human still gets rclone's
                    // (already redacted) words. Full-length on the heap:
                    // the snapshot truncation is publishFailureText's.
                    const text = std.fmt.allocPrint(self.gpa, "{s}: {s}", .{
                        @tagName(result.outcome),
                        eng.lastErrorText(),
                    }) catch null;
                    if (text) |owned| {
                        defer self.gpa.free(owned);
                        self.publishFailureText(owned);
                    } else {
                        self.publishFailureText(@tagName(result.outcome));
                    }
                }
            },
        }
    }

    /// The daemon, client and engine, built once. Returns null after
    /// publishing the failure — a session that cannot be built is a failed
    /// job, not a crashed worker.
    fn ensureSession(self: *Worker, session: *Session) ?*engine.Engine {
        if (session.eng) |*e| return e;

        const endpoint: rc.Endpoint = if (self.endpoint) |ep| ep else blk: {
            const source = self.binary_source orelse {
                self.publishFailureText("no rclone binary available");
                return null;
            };
            // An owned copy, made under the source's own lock. Freed as soon
            // as the daemon has been started from it.
            const binary = source.resolve(source.context, self.gpa) orelse {
                self.publishFailureText("no rclone binary available");
                return null;
            };
            defer self.gpa.free(binary);

            session.daemon_box = daemon.Daemon.spawn(self.gpa, self.io, .{
                .binary = binary,
                .game_dir = self.game_dir,
            }) catch {
                self.publishFailureText("rclone daemon failed to start");
                return null;
            };
            // Abortable on the cancel flag — set by both `cancel` and
            // `destroy` — because this wait is the one place the worker
            // otherwise could not observe it, and a shutdown during daemon
            // startup would sit out the whole readiness window.
            session.daemon_box.?.waitReadyAbortable(
                daemon.ready_timeout_ms,
                &self.cancel_flag,
            ) catch |err| {
                self.publishFailureText(switch (err) {
                    error.Aborted => "Cancelled",
                    error.DaemonTimeout => "rclone daemon never became ready",
                });
                session.daemon_box.?.shutdown();
                session.daemon_box = null;
                return null;
            };
            break :blk session.daemon_box.?.endpoint();
        };

        session.client = rc.Client.init(self.gpa, self.io, endpoint) catch {
            self.publishFailureText("rc client could not be created");
            return null;
        };
        if (self.deadline) |deadline| session.client.?.deadline = deadline;

        // Credentials are applied per job by `execute`, not here: a session
        // outlives many edits of `cloud.credentials`, and every job must run
        // with what is on disk at its own start.
        session.eng = engine.Engine.init(self.gpa, self.io, &session.client.?);
        session.eng.?.cancel = &self.cancel_flag;
        // The opportunistic half of the catalogue bootstrap: after a clean
        // sync the daemon is already up and warm, so a stale or missing
        // catalogue costs one small version call to notice and a single fetch
        // to fix — with no dialog open and nothing waiting on it.
        session.eng.?.refresh_catalogue = true;
        return &session.eng.?;
    }

    /// Configure `bkraw` and the `bkremote` alias from
    /// `<game_dir>/profiles/cloud.credentials`, when present — run at every
    /// job start, so an edit through the dialog reaches the daemon's config
    /// before the next probe or sync. True on success or absence — a machine
    /// without the file (the worker tests pre-write `rclone.conf` by hand)
    /// uses whatever the config already holds; false after publishing the
    /// failure. The engine's secret-redaction set refreshes on the same
    /// cadence, from the same load: whatever this document designates
    /// secret must never surface in a failure text.
    fn applyCredentials(self: *Worker, session: *Session) bool {
        const client = &session.client.?;
        const eng = &session.eng.?;

        const creds_path = path.join(self.gpa, &.{ self.game_dir, creds.default_path }) catch {
            self.publishFailureText("out of memory reading credentials");
            return false;
        };
        defer self.gpa.free(creds_path);

        // One read under the document lock, consumed twice: the parse that
        // configures the daemon and the baseline the read-back compares
        // must describe the SAME bytes. Two reads would leave a gap a
        // dialog save could land in — the baseline then vouching for a
        // document the daemon never saw — and the lock orders the capture
        // after any save already in flight.
        creds.document_mutex.lockUncancelable(self.io);
        const raw_document = self.readCredsRaw(creds_path);
        creds.document_mutex.unlock(self.io);

        const raw = raw_document orelse {
            eng.setSecretRedactions(&.{}, &.{}) catch {};
            self.setAppliedCreds(null);
            return true;
        };
        var loaded = (creds.parse(self.gpa, raw) catch null) orelse {
            // Present but unreadable: nothing applied, nothing to read
            // back against.
            self.gpa.free(raw);
            eng.setSecretRedactions(&.{}, &.{}) catch {};
            self.setAppliedCreds(null);
            return true;
        };
        defer loaded.deinit();
        // The baseline takes ownership of the very bytes just parsed.
        self.setAppliedCreds(raw);

        var names: std.ArrayList([]const u8) = .empty;
        defer names.deinit(self.gpa);
        var values: std.ArrayList([]const u8) = .empty;
        defer values.deinit(self.gpa);
        for (loaded.creds.options) |opt| {
            if (!opt.withheld()) continue;
            names.append(self.gpa, opt.name) catch {
                self.publishFailureText("out of memory preparing redaction");
                return false;
            };
            if (opt.value.len == 0) continue;
            values.append(self.gpa, opt.value) catch {
                self.publishFailureText("out of memory preparing redaction");
                return false;
            };
        }
        // Refusing the job is deliberate: running it with an incomplete
        // redaction set is the one failure mode worse than not running.
        eng.setSecretRedactions(names.items, values.items) catch {
            self.publishFailureText("out of memory preparing redaction");
            return false;
        };

        var params = creds.remoteParams(self.gpa, loaded.creds) catch {
            self.publishFailureText("out of memory building remote parameters");
            return false;
        };
        defer params.deinit();
        self.configCreate(client, creds.backend_remote_name, params.value) catch {
            self.publishFailureText("cloud credentials could not be applied to the daemon");
            return false;
        };

        const target = creds.aliasTarget(self.gpa, loaded.creds) catch {
            self.publishFailureText("out of memory building the sync alias");
            return false;
        };
        defer self.gpa.free(target);
        var alias: std.json.ObjectMap = .empty;
        defer alias.deinit(self.gpa);
        alias.put(self.gpa, "type", .{ .string = "alias" }) catch return false;
        alias.put(self.gpa, "remote", .{ .string = target }) catch return false;
        self.configCreate(client, creds.sync_remote_name, .{ .object = alias }) catch {
            self.publishFailureText("cloud credentials could not be applied to the daemon");
            return false;
        };
        return true;
    }

    /// `config/create` with `opt.obscure`: rclone transforms password-typed
    /// fields itself, and everything else passes through. `nonInteractive`
    /// because an OAuth backend would otherwise run its whole browser
    /// dance inside this call — headless, blocking, killed by the POST
    /// deadline. The parameters are persisted either way (verified against
    /// v1.75.0); a `State` in the reply is the machine offering its
    /// questions, which are the config job's business, not this one's.
    fn configCreate(
        self: *Worker,
        client: *rc.Client,
        name: []const u8,
        params: std.json.Value,
    ) !void {
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        try object.put(self.gpa, "name", .{ .string = name });
        try object.put(self.gpa, "type", params.object.get("type").?);
        try object.put(self.gpa, "parameters", params);
        var opt: std.json.ObjectMap = .empty;
        defer opt.deinit(self.gpa);
        try opt.put(self.gpa, "obscure", .{ .bool = true });
        try opt.put(self.gpa, "nonInteractive", .{ .bool = true });
        try object.put(self.gpa, "opt", .{ .object = opt });

        var reply = try client.call("config/create", .{ .object = object });
        reply.deinit();
    }

    fn cancelled(self: *const Worker) bool {
        return self.cancel_flag.load(.acquire);
    }

    fn publishState(self: *Worker, state: State) void {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        self.snapshot.state = state;
    }

    fn publishDone(self: *Worker, outcome: Outcome) void {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        self.snapshot.state = .done;
        self.snapshot.outcome = outcome;
        self.snapshot.error_len = 0;
        if (self.error_detail) |owned| self.gpa.free(owned);
        self.error_detail = null;
    }

    fn publishFailure(self: *Worker, err: anyerror, detail: []const u8) void {
        // The engine's text when it has one — that is what rclone actually
        // said — and the error name otherwise, so `.failed` is never mute.
        const text = if (detail.len != 0) detail else @errorName(err);
        self.publishFailureText(text);
    }

    /// A pair or sync failure. When the engine classified the run, the
    /// outcome tag leads the text — the contract testConnection already
    /// keeps — so a caller can branch on the first word while the human
    /// reads rclone's (redacted) words after it. A cancellation is its own
    /// bare word: the engine's stored text may belong to an earlier run,
    /// and "the player skipped" needs no elaboration. Every other error
    /// (NotPaired above all — the facade matches that text exactly to
    /// retry as a pairing) passes through untouched.
    fn publishRunFailure(self: *Worker, eng: *engine.Engine, err: anyerror) void {
        if (err == error.Cancelled) {
            self.publishFailureText("Cancelled");
            return;
        }
        if (err == error.SyncFailed) {
            // Composed on the heap at full length: the engine text carries
            // the redacted support tail, and publishFailureText is where
            // the one-line snapshot truncation happens — composing into a
            // snapshot-sized buffer here would lose the tail before the
            // detail channel ever saw it (and its overflow fallback used
            // to drop everything but the tag).
            const text = std.fmt.allocPrint(self.gpa, "{s}: {s}", .{
                @tagName(eng.lastOutcome()),
                eng.lastErrorText(),
            }) catch null;
            if (text) |owned| {
                defer self.gpa.free(owned);
                self.publishFailureText(owned);
            } else {
                self.publishFailureText(@tagName(eng.lastOutcome()));
            }
            return;
        }
        self.publishFailure(err, eng.lastErrorText());
    }

    /// The interactive `config/create` machine as a job: drive the flow,
    /// park every question in the mailbox with the snapshot flipped to
    /// `.awaiting_input`, resume on `answerConfig`, and on completion run
    /// the same connection test every saved credential change gets — a
    /// remote that authorised but cannot list or write must say so now,
    /// not at the next sync. `applyCredentials` has already run, so the
    /// raw remote and the sync alias exist before the machine reworks the
    /// raw one, and the engine's secret redactions are current.
    ///
    /// The wait on a human is bounded only by cancel and destroy — there
    /// is no honest timeout for a person reading an OAuth page — while
    /// every rc exchange inside the flow keeps the client's per-POST
    /// deadline.
    fn runConfigCreate(self: *Worker, eng: *engine.Engine) void {
        const creds_path = path.join(self.gpa, &.{ self.game_dir, creds.default_path }) catch {
            self.publishFailureText("out of memory reading credentials");
            return;
        };
        defer self.gpa.free(creds_path);
        var loaded = (creds.load(self.gpa, self.io, creds_path) catch null) orelse {
            self.publishFailureText("no credentials are saved to configure");
            return;
        };
        defer loaded.deinit();

        var params = creds.remoteParams(self.gpa, loaded.creds) catch {
            self.publishFailureText("out of memory building remote parameters");
            return;
        };
        defer params.deinit();
        // config/create names the type at the top level; a second copy
        // inside `parameters` would write a literal `type` key into the
        // remote's section.
        _ = params.value.object.orderedRemove("type");

        var flow = oauth.Flow.init(self.gpa, eng.client, creds.backend_remote_name, loaded.creds.backend) catch {
            self.publishFailureText("out of memory starting the configuration flow");
            return;
        };
        defer flow.deinit();
        defer self.clearConfigMailboxes();

        const redactions: engine.ExtraRedactions = .{
            .markers = eng.extra_markers,
            .values = eng.extra_values,
        };

        var answer: ?[]u8 = null;
        defer if (answer) |owned| self.gpa.free(owned);
        var hops: usize = 0;

        while (true) {
            if (self.cancelled()) {
                self.publishFailureText("Cancelled");
                return;
            }
            // Every exchange runs as an rc `_async` job: the continuation
            // that starts a browser dance blocks server-side until the
            // consent callback lands, and rclone aborts the dance when the
            // request's connection dies — a synchronous POST under the
            // per-POST deadline would take it down at the deadline.
            const job = flow.beginStepAsync(params.value, if (answer) |a| a else null) catch |err| {
                self.publishStepError(&flow, err, redactions);
                return;
            };
            if (answer) |owned| {
                self.gpa.free(owned);
                answer = null;
            }

            const exchange = self.awaitExchange(&flow, eng, job, redactions) orelse return;
            switch (exchange) {
                .done => break,
                .auto_continue => {
                    // The machine's own hop, continued with an empty
                    // result — bounded like the synchronous driver bounds
                    // it, because a machine that never settles must not
                    // spin the worker forever.
                    hops += 1;
                    if (hops > 16) {
                        self.publishFailureText("rclone's configuration reply had an unexpected shape");
                        return;
                    }
                    answer = self.gpa.dupe(u8, "") catch {
                        self.publishFailureText("out of memory driving the configuration");
                        return;
                    };
                },
                .question => |q| {
                    hops = 0;
                    // The in-band error goes through redaction before it
                    // can reach a status line.
                    const safe_error = engine.redactedText(self.gpa, q.error_text, redactions) catch null;
                    defer if (safe_error) |owned| self.gpa.free(owned);
                    var shown = q;
                    shown.error_text = safe_error orelse "";
                    const question_json = oauth.questionJson(self.gpa, &shown) catch {
                        self.publishFailureText("out of memory rendering the question");
                        return;
                    };
                    self.parkQuestion(question_json);

                    while (true) {
                        if (self.cancelled() or self.stop_flag.load(.acquire)) {
                            self.publishFailureText("Cancelled");
                            return;
                        }
                        if (self.takeAnswer()) |taken| {
                            answer = taken;
                            break;
                        }
                        sleepMs(self.io, idle_poll_ms);
                    }
                },
            }
        }

        // Completion runs the same probe a saved credential change gets.
        const test_result = eng.testConnection(creds.sync_remote_name) catch {
            self.publishFailureText("out of memory testing the connection");
            return;
        };
        if (test_result.ok) {
            self.publishDone(.connection_ok);
            return;
        }
        const text = std.fmt.allocPrint(self.gpa, "{s}: {s}", .{
            @tagName(test_result.outcome),
            eng.lastErrorText(),
        }) catch null;
        if (text) |owned| {
            defer self.gpa.free(owned);
            self.publishFailureText(owned);
        } else {
            self.publishFailureText(@tagName(test_result.outcome));
        }
    }

    /// How often a pending config exchange is polled. Wall clock, like the
    /// engine's job polling: a fast menu must not turn into a fast poller.
    const config_poll_ms: u32 = 250;

    /// How long the browser dance may wait for the player. Long, because
    /// a human is reading a consent page; finite, because an abandoned
    /// flow must not hold the job slot forever. Cancel works the whole
    /// time.
    const consent_wait_ms: u32 = 5 * 60 * 1000;

    /// Poll one async config exchange to completion. While the job runs,
    /// `config/oauthstatus` is asked whether a consent URL is waiting;
    /// the first one seen parks as a consent card — role "consent", the
    /// same mailbox the field questions use — so the dialog can open the
    /// browser and show a visible waiting state (on macOS the game runs
    /// in its own Space; a silent wait would look like a hang). The URL
    /// crosses to the dialog and nowhere else: it carries a state secret
    /// and is never logged. Publishes and returns null on failure,
    /// cancellation, or timeout.
    fn awaitExchange(
        self: *Worker,
        flow: *oauth.Flow,
        eng: *engine.Engine,
        job: rc.JobId,
        redactions: engine.ExtraRedactions,
    ) ?oauth.Flow.Exchange {
        var waited: u32 = 0;
        var consent_parked = false;
        while (true) {
            if (self.cancelled() or self.stop_flag.load(.acquire)) {
                self.stopDance(eng);
                self.publishFailureText("Cancelled");
                return null;
            }
            var status = eng.client.jobStatus(job) catch |err| {
                self.publishFailure(err, "");
                return null;
            };
            defer status.deinit();
            if (status.finished) {
                if (consent_parked) self.unparkConsent();
                if (!status.success) {
                    // rclone's words — "oauth authentication was
                    // cancelled", "No code returned by remote server" —
                    // through the same redaction as every failure text.
                    const safe = engine.redactedText(self.gpa, status.error_text, redactions) catch null;
                    defer if (safe) |owned| self.gpa.free(owned);
                    self.publishFailureText(safe orelse "the configuration step failed");
                    return null;
                }
                const exchange = flow.finishStep(status.output) catch |err| {
                    self.publishStepError(flow, err, redactions);
                    return null;
                };
                return exchange;
            }

            if (!consent_parked) {
                if (self.pendingConsentCard(eng)) |card| {
                    self.parkQuestion(card);
                    consent_parked = true;
                }
            }

            if (waited >= consent_wait_ms) {
                self.stopDance(eng);
                if (consent_parked) self.unparkConsent();
                self.publishFailureText("timed_out: the sign-in was not completed in time");
                return null;
            }
            sleepMs(self.io, config_poll_ms);
            waited += config_poll_ms;
        }
    }

    /// One `config/oauthstatus` ask, rendered as a consent card when a
    /// dance is waiting. Null on any failure: the status poll is advisory
    /// and the job poll is the source of truth.
    fn pendingConsentCard(self: *Worker, eng: *engine.Engine) ?[]u8 {
        var reply = eng.client.call("config/oauthstatus", .{ .object = .empty }) catch return null;
        defer reply.deinit();
        const url = oauth.pendingAuthUrl(reply.value) orelse return null;
        return oauth.consentJson(self.gpa, url) catch null;
    }

    const ClassifyContext = struct {
        cat: *const catalogue.Catalogue,
        backend: []const u8,
    };

    fn classifyFromCatalogue(context: ?*const anyopaque, name: []const u8) ?creds.ReadBackFlags {
        const ctx: *const ClassifyContext = @alignCast(@ptrCast(context orelse return null));
        const backend = ctx.cat.backend(ctx.backend) orelse return null;
        const option = backend.option(name) orelse return null;
        return .{ .secret = option.isSecret(), .is_password = option.is_password };
    }

    /// Fold rclone's stored section back into `cloud.credentials` — the
    /// refreshed OAuth token above all: rclone refreshes into *its*
    /// config, and a token never read back is lost when the daemon exits.
    /// Runs after every session job, success or failure alike, and once
    /// more at teardown while the client still exists to ask. The cached
    /// catalogue classifies fields the document has never seen; with no
    /// cache, `applyReadBack`'s safe default stands. Best-effort
    /// throughout: a failed read-back must not fail the job it follows,
    /// and the next one asks again.
    /// The raw document bytes, or null when unreadable or absent.
    fn readCredsRaw(self: *Worker, creds_path: []const u8) ?[]u8 {
        return Io.Dir.cwd().readFileAlloc(
            self.io,
            creds_path,
            self.gpa,
            .limited(creds.max_document_bytes),
        ) catch null;
    }

    /// Under the document lock: true when the on-disk document still
    /// matches the applied baseline. On mismatch the save's identity
    /// rotation is re-applied for the document that won — the job that
    /// just finished started under the OLD identity, and its success
    /// record (`recordSuccess` writes the fingerprint the run began with)
    /// may have resurrected a pairing the save already retired. Retiring
    /// it again here means the next sync takes the designed NotPaired →
    /// pair bootstrap instead of refusing FingerprintChanged until a
    /// second save. False tells the caller to leave the document alone.
    fn baselineIntact(self: *Worker, creds_path: []const u8) bool {
        creds.document_mutex.lockUncancelable(self.io);
        defer creds.document_mutex.unlock(self.io);
        return self.baselineIntactLocked(creds_path);
    }

    fn baselineIntactLocked(self: *Worker, creds_path: []const u8) bool {
        const applied = self.applied_creds_raw orelse return false;
        const current = self.readCredsRaw(creds_path) orelse return false;
        defer self.gpa.free(current);
        if (std.mem.eql(u8, current, applied)) return true;

        var winner = (creds.parse(self.gpa, current) catch null) orelse return false;
        defer winner.deinit();
        _ = engine.retireMismatchedPairings(self.gpa, self.io, self.game_dir, winner.creds.fingerprint);
        return false;
    }

    /// Install the applied-document baseline: owned bytes, or null to
    /// clear. The read-back refuses to touch a document that no longer
    /// matches it — the dialog saved while the job ran, and the player's
    /// save wins.
    fn setAppliedCreds(self: *Worker, raw: ?[]u8) void {
        if (self.applied_creds_raw) |owned| self.gpa.free(owned);
        self.applied_creds_raw = raw;
    }

    /// Refresh the baseline from disk — only ever called under the
    /// document lock, right after this worker's own save published.
    fn rememberAppliedCreds(self: *Worker, creds_path: []const u8) void {
        self.setAppliedCreds(self.readCredsRaw(creds_path));
    }

    fn readBackConfig(self: *Worker, session: *Session) void {
        if (session.client == null) return;
        const client = &session.client.?;

        // Nothing was applied: nothing to read back against.
        if (self.applied_creds_raw == null) return;

        const creds_path = path.join(self.gpa, &.{ self.game_dir, creds.default_path }) catch return;
        defer self.gpa.free(creds_path);

        // Before any network: a document that already changed has nothing
        // to read back into, and possibly a stale pairing to retire — the
        // guard must not hinge on the request below succeeding.
        if (!self.baselineIntact(creds_path)) return;

        // The blocking request happens BEFORE the document is re-examined:
        // the re-check and the publish below form one critical section
        // under the document lock, and a network wait must never sit
        // inside it.
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(self.gpa);
        object.put(self.gpa, "name", .{ .string = creds.backend_remote_name }) catch return;
        var reply = client.call("config/get", .{ .object = object }) catch return;
        defer reply.deinit();
        const section = switch (reply.value) {
            .object => |o| o,
            else => return,
        };

        var cat = catalogue.loadCached(self.gpa, self.io, self.game_dir) catch return;
        defer cat.deinit();

        // Check-and-publish, atomic against every other writer of the
        // document: the dialog may save new credentials at any moment —
        // during the job, or inside the very request above — and the
        // daemon's section then describes the *previous* document. The
        // player's save wins; the next job applies it and reads back
        // against it.
        creds.document_mutex.lockUncancelable(self.io);
        defer creds.document_mutex.unlock(self.io);

        if (!self.baselineIntactLocked(creds_path)) return;

        var loaded = (creds.load(self.gpa, self.io, creds_path) catch null) orelse return;
        defer loaded.deinit();

        // The section must be this document's backend. A leftover from a
        // provider switch carries another backend's fields — nothing in it
        // is ours to import.
        const section_type = section.get("type") orelse return;
        if (section_type != .string) return;
        if (!std.mem.eql(u8, section_type.string, loaded.creds.backend)) return;

        const ctx: ClassifyContext = .{ .cat = &cat, .backend = loaded.creds.backend };
        const changed = creds.applyReadBack(&loaded, section, .{
            .context = @ptrCast(&ctx),
            .lookup = classifyFromCatalogue,
        }) catch return;
        if (!changed) return;
        // The same atomic temp-then-rename the credentials file always
        // uses: a token half-written is an unusable credential.
        creds.save(self.gpa, self.io, creds_path, loaded.creds) catch return;
        // Our own save is the applied document plus the merge: the
        // teardown read-back must still recognise it as unchanged.
        self.rememberAppliedCreds(creds_path);
    }

    /// Best-effort `config/oauthstop`: an abandoned dance settles its job
    /// instead of waiting on a browser that will never answer. An error
    /// means no dance was running — equally settled.
    fn stopDance(self: *Worker, eng: *engine.Engine) void {
        _ = self;
        var reply = eng.client.call("config/oauthstop", .{ .object = .empty }) catch return;
        reply.deinit();
    }

    /// Take a parked consent card down — the dance moved on — dropping
    /// any answer nobody asked for.
    fn unparkConsent(self: *Worker) void {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        if (self.config_question) |owned| self.gpa.free(owned);
        self.config_question = null;
        if (self.config_answer) |owned| self.gpa.free(owned);
        self.config_answer = null;
        if (self.snapshot.state == .awaiting_input) self.snapshot.state = .testing;
    }

    fn publishStepError(
        self: *Worker,
        flow: *oauth.Flow,
        err: anyerror,
        redactions: engine.ExtraRedactions,
    ) void {
        switch (err) {
            error.OutOfMemory => self.publishFailureText("out of memory driving the configuration"),
            // rclone's own words — through the same redaction as every
            // failure text, because an in-band error can echo the
            // parameters it rejected.
            error.ConfigFailed => {
                const safe = engine.redactedText(self.gpa, flow.lastError(), redactions) catch null;
                defer if (safe) |owned| self.gpa.free(owned);
                self.publishFailureText(safe orelse "the service refused the configuration");
            },
            error.BadReply, error.Runaway => self.publishFailureText(
                "rclone's configuration reply had an unexpected shape",
            ),
            else => self.publishFailure(err, ""),
        }
    }

    /// Post a question (owned JSON, taken over) and flip the snapshot to
    /// `.awaiting_input`. Any stale answer is dropped: it belonged to an
    /// earlier question.
    fn parkQuestion(self: *Worker, question_json: []u8) void {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        if (self.config_question) |owned| self.gpa.free(owned);
        self.config_question = question_json;
        if (self.config_answer) |owned| self.gpa.free(owned);
        self.config_answer = null;
        self.snapshot.state = .awaiting_input;
    }

    /// Take the pending answer, if one arrived. Taking it also takes the
    /// question down — answered — and flips the snapshot back to the
    /// spinner state while the machine steps.
    fn takeAnswer(self: *Worker) ?[]u8 {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        const taken = self.config_answer orelse return null;
        self.config_answer = null;
        if (self.config_question) |owned| self.gpa.free(owned);
        self.config_question = null;
        self.snapshot.state = .testing;
        return taken;
    }

    fn clearConfigMailboxes(self: *Worker) void {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        if (self.config_question) |owned| self.gpa.free(owned);
        self.config_question = null;
        if (self.config_answer) |owned| self.gpa.free(owned);
        self.config_answer = null;
    }

    /// Publish a failure: the snapshot keeps at most `error_text_max`
    /// bytes — one status line — while the full text is kept aside for
    /// `errorDetailOwned`. Pub for the truncation test; the worker itself
    /// is the only production caller.
    pub fn publishFailureText(self: *Worker, text: []const u8) void {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        self.snapshot.state = .failed;
        self.snapshot.outcome = .failed;
        const len = @min(text.len, error_text_max);
        @memcpy(self.snapshot.error_buf[0..len], text[0..len]);
        self.snapshot.error_len = len;
        if (self.error_detail) |owned| self.gpa.free(owned);
        // On allocation failure the snapshot's truncation still stands;
        // an absent detail falls back to the summary, never to nothing.
        self.error_detail = self.gpa.dupe(u8, text) catch null;
    }

    /// An owned copy of the full text of the most recent failure — the
    /// engine's error line plus the redacted support tail — or null when
    /// nothing has failed or the last job succeeded. The snapshot's
    /// `errorText` is the one-line truncation of the same text. Copied
    /// under the lock, because the worker replaces it the moment another
    /// job fails; the worker runs one job at a time, so while a failed
    /// job is the one being polled, this is that job's detail.
    pub fn errorDetailOwned(self: *Worker, gpa: Allocator) Allocator.Error!?[]u8 {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        const detail = self.error_detail orelse return null;
        return try gpa.dupe(u8, detail);
    }

    /// The question the config machine is waiting on — the form's wire
    /// JSON plus an `error` key — or null when none is pending. An owned
    /// copy under the lock; the pending question can change the moment the
    /// lock drops.
    pub fn configQuestionOwned(self: *Worker, gpa: Allocator) Allocator.Error!?[]u8 {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        const question = self.config_question orelse return null;
        return try gpa.dupe(u8, question);
    }

    /// Answer the pending config question. False when no question is
    /// waiting, an answer is already queued, or the copy failed — the flow
    /// then keeps waiting, and the caller may retry or cancel.
    pub fn answerConfig(self: *Worker, result: []const u8) bool {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        if (self.config_question == null) return false;
        if (self.config_answer != null) return false;
        self.config_answer = self.gpa.dupe(u8, result) catch return false;
        return true;
    }
};

/// A catalogue failure is reported, never thrown and never fatal: the cache
/// stays as it was, the dialog says so and offers a retry, and nothing about
/// the rest of cloud sync is blocked by it.
fn catalogueFailureText(err: anyerror) []const u8 {
    return switch (err) {
        error.RcCallFailed => "the provider catalogue could not be fetched",
        error.BadCatalogue => "rclone returned a provider catalogue this build cannot read",
        error.VersionUnreadable => "rclone did not report a version this build can read",
        error.CatalogueUnwritable => "the provider catalogue could not be cached",
        else => @errorName(err),
    };
}

fn freeJob(gpa: Allocator, box: *JobBox) void {
    box.arena.deinit();
    gpa.destroy(box);
}

fn writeEntry(json: *std.json.Stringify, entry: backup.BackupEntry) !void {
    try json.beginObject();
    try json.objectField("id");
    try json.write(entry.id);
    try json.objectField("host");
    try json.write(entry.host);
    try json.objectField("timestamp");
    try json.write(entry.timestamp);
    try json.objectField("size");
    try json.write(entry.size);
    try json.objectField("remote_path");
    try json.write(entry.remote_path);
    try json.endObject();
}

fn sleepMs(io: Io, ms: u32) void {
    const duration: Io.Clock.Duration = .{
        .raw = .fromMilliseconds(ms),
        .clock = .awake,
    };
    duration.sleep(io) catch {};
}
