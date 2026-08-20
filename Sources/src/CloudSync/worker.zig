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
const creds = @import("creds.zig");
const rc = @import("rc.zig");
const daemon = @import("daemon.zig");
const engine = @import("engine.zig");

const Allocator = std.mem.Allocator;
const Io = std.Io;
const path = Io.Dir.path;

/// How often the idle worker looks for newly enqueued work. Also the upper
/// bound `destroy` waits on an idle worker.
const idle_poll_ms: u32 = 25;

/// New states are appended, never inserted: the ABI pins the numerics.
pub const State = enum(u8) { idle, starting, pairing, syncing, done, failed, testing };

/// How the last finished job ended. `.none` until the first job finishes.
/// Appended, never reordered, for the same reason.
pub const Outcome = enum(u8) { none, paired, synced, failed, connection_ok, backups_listed };

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

pub const JobKind = enum { pair, sync, test_connection, list_backups };

/// A job as the caller describes it. Every slice is copied by `begin`; none
/// needs to outlive the call.
pub const JobSpec = struct {
    kind: JobKind,
    path1: []const u8,
    remote: []const u8,
    profile: []const u8,
    profile_id: []const u8,
    remote_fingerprint: []const u8,
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
};

pub const Worker = struct {
    gpa: Allocator,
    io: Io,
    game_dir: []u8,
    endpoint: ?rc.Endpoint,
    deadline: ?rc.Deadline,
    binary_source: ?BinarySource,

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
        };

        // `concurrent`, not `async`: the whole point is a task that runs
        // while the caller does not wait on it.
        self.task = io.concurrent(runWorker, .{self}) catch
            return error.ConcurrencyUnavailable;
        return self;
    }

    /// Cancel whatever is in flight, stop the task, tear down the session,
    /// free everything. Bounded: one POST deadline plus one poll interval.
    pub fn destroy(self: *Worker) void {
        self.stop_flag.store(true, .release);
        self.cancel_flag.store(true, .release);
        if (self.task) |*task| task.await(self.io);

        if (self.pending) |box| freeJob(self.gpa, box);
        if (self.backup_list) |*list| list.deinit();
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

        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);

        if (self.pending != null) return error.Busy;
        switch (self.snapshot.state) {
            .starting, .pairing, .syncing, .testing => return error.Busy,
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
    }

    fn takePending(self: *Worker) ?*JobBox {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        const box = self.pending;
        self.pending = null;
        return box;
    }

    fn execute(self: *Worker, session: *Session, box: *JobBox) void {
        const eng = self.ensureSession(session) orelse return;

        self.publishState(switch (box.kind) {
            .pair => .pairing,
            .sync => .syncing,
            // Probe-shaped fetches share the testing state: short, networked,
            // and nothing a UI needs to distinguish while it spins.
            .test_connection, .list_backups => .testing,
        });

        switch (box.kind) {
            .pair => {
                var outcome = eng.pair(box.ctx) catch |err| {
                    self.publishFailure(err, eng.lastErrorText());
                    return;
                };
                outcome.deinit(self.gpa);
                self.publishDone(.paired);
            },
            .sync => {
                const run_id = eng.syncOnce(box.ctx) catch |err| {
                    self.publishFailure(err, eng.lastErrorText());
                    return;
                };
                self.gpa.free(run_id);
                // After a clean finish only, and best-effort: the sync that
                // succeeded stays succeeded whatever the snapshot does.
                if (box.backup_config) {
                    const snapshot: ?[]u8 = backup.snapshotConfig(self.gpa, self.io, eng.client, .{
                        .config_dir = self.game_dir,
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
                    // (already redacted) words.
                    var buffer: [error_text_max]u8 = undefined;
                    const text = std.fmt.bufPrint(&buffer, "{s}: {s}", .{
                        @tagName(result.outcome),
                        eng.lastErrorText(),
                    }) catch @tagName(result.outcome);
                    self.publishFailureText(text);
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
            session.daemon_box.?.waitReady(daemon.ready_timeout_ms) catch {
                self.publishFailureText("rclone daemon never became ready");
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

        // When a credentials file exists, its remotes are (re)created in the
        // daemon's config before the first job runs: the raw backend plus
        // the short alias every sync names. A machine without the file — the
        // worker tests pre-write `rclone.conf` by hand — skips this and uses
        // whatever the config already holds.
        if (!self.applyCredentials(&session.client.?)) {
            session.client.?.deinit();
            session.client = null;
            return null;
        }

        session.eng = engine.Engine.init(self.gpa, self.io, &session.client.?);
        session.eng.?.cancel = &self.cancel_flag;
        return &session.eng.?;
    }

    /// Configure `bkraw` and the `bkremote` alias from
    /// `<game_dir>/profiles/cloud.credentials`, when present. True on
    /// success or absence; false after publishing the failure.
    fn applyCredentials(self: *Worker, client: *rc.Client) bool {
        const creds_path = path.join(self.gpa, &.{ self.game_dir, creds.default_path }) catch {
            self.publishFailureText("out of memory reading credentials");
            return false;
        };
        defer self.gpa.free(creds_path);

        var loaded = (creds.load(self.gpa, self.io, creds_path) catch null) orelse return true;
        defer loaded.deinit();

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
    /// fields itself, and everything else passes through.
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
        try object.put(self.gpa, "opt", .{ .object = opt });

        var reply = try client.call("config/create", .{ .object = object });
        reply.deinit();
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
    }

    fn publishFailure(self: *Worker, err: anyerror, detail: []const u8) void {
        // The engine's text when it has one — that is what rclone actually
        // said — and the error name otherwise, so `.failed` is never mute.
        const text = if (detail.len != 0) detail else @errorName(err);
        self.publishFailureText(text);
    }

    fn publishFailureText(self: *Worker, text: []const u8) void {
        self.mutex.lockUncancelable(self.io);
        defer self.mutex.unlock(self.io);
        self.snapshot.state = .failed;
        self.snapshot.outcome = .failed;
        const len = @min(text.len, error_text_max);
        @memcpy(self.snapshot.error_buf[0..len], text[0..len]);
        self.snapshot.error_len = len;
    }
};

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
