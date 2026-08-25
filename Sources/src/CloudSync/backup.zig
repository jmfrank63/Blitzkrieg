//! Config backup: one-way, per-host snapshots of `config.cfg`, using none of
//! bisync's machinery. One `operations/copyfile` per snapshot — no listings,
//! no session state, no delete ratios.
//!
//! `config.cfg` is never synced: it carries `GFX.Mode`, `GFX.Monitor` and
//! `GFX.FullScreen`, and this codebase spent real work making display
//! choices persist per machine. It is worth having a copy off the machine
//! anyway, so after a clean sync the worker pushes one — to
//! `<remote>:config-backups/<profile>/<host>/<stamp>.cfg`, a **sibling** of
//! `profiles/`, never a child of Path2: anything beneath the synced prefix
//! is by definition synced back down onto every machine, which for a backup
//! history would be the exact leak the split exists to prevent.
//!
//! Snapshots are keyed by host so machines never overwrite each other's
//! history — "restore the desktop's settings onto the laptop" only means
//! something if both histories survive.

const std = @import("std");
const engine = @import("engine.zig");
const plan = @import("plan.zig");
const rc = @import("rc.zig");

const Allocator = std.mem.Allocator;
const Io = std.Io;

/// The file this module snapshots, relative to the game root.
pub const config_file_name = "config.cfg";

/// The host name reduced by the same rules as `NProfile::Sanitize`
/// (`ProfilePaths.h`): printable ASCII only, the Windows-forbidden
/// characters dropped, leading spaces and trailing spaces-and-dots trimmed.
/// The empty fallback is "host" rather than Sanitize's "Player", because an
/// anonymous machine is not a person.
pub fn sanitizeHost(gpa: Allocator, name: []const u8) Allocator.Error![]u8 {
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(gpa);

    for (name) |c| {
        if (c < 32 or c > 126) continue;
        if (std.mem.indexOfScalar(u8, "/\\:*?\"<>|", c) != null) continue;
        try out.append(gpa, c);
    }
    while (out.items.len > 0 and out.items[0] == ' ') _ = out.orderedRemove(0);
    while (out.items.len > 0 and
        (out.items[out.items.len - 1] == ' ' or out.items[out.items.len - 1] == '.'))
    {
        _ = out.pop();
    }
    if (out.items.len == 0) {
        out.deinit(gpa);
        return gpa.dupe(u8, "host");
    }
    return out.toOwnedSlice(gpa);
}

pub const SnapshotContext = struct {
    /// The directory holding `config.cfg` — the game root, where
    /// `SerializeConfig` writes it.
    config_dir: []const u8,
    /// The sync remote's name (`bkremote`); the backup rides the same alias.
    remote: []const u8,
    profile: []const u8,
    /// Raw host name; sanitised here.
    host: []const u8,
};

pub const SnapshotError = rc.RcError || Allocator.Error;

/// Push one snapshot. Returns the destination file name (owned) so a caller
/// or test can find it; the stamp is a full run id — sortable, and unique
/// even within one second.
///
/// Blocking, like every rc call: the worker runs it after a clean sync, and
/// only when the backup option is enabled. A failed snapshot never fails the
/// sync that triggered it — the caller swallows.
pub fn snapshotConfig(
    gpa: Allocator,
    io: Io,
    client: *rc.Client,
    ctx: SnapshotContext,
) SnapshotError![]u8 {
    const host = try sanitizeHost(gpa, ctx.host);
    defer gpa.free(host);

    const dst_root = try plan.remoteConfigBackupRoot(gpa, ctx.remote, ctx.profile, host);
    defer gpa.free(dst_root);

    const run_id = try plan.runId(gpa, io);
    defer gpa.free(run_id);
    const dst_name = try std.fmt.allocPrint(gpa, "{s}.cfg", .{run_id});
    errdefer gpa.free(dst_name);

    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "srcFs", .{ .string = ctx.config_dir });
    try object.put(gpa, "srcRemote", .{ .string = config_file_name });
    try object.put(gpa, "dstFs", .{ .string = dst_root });
    try object.put(gpa, "dstRemote", .{ .string = dst_name });

    var reply = try client.call("operations/copyfile", .{ .object = object });
    reply.deinit();
    return dst_name;
}

// -- Listing and retention ----------------------------------------------------

pub const BackupEntry = struct {
    /// Stable opaque handle the UI passes back: `<host>/<file>`, the path
    /// within the profile's backup root.
    id: []const u8,
    host: []const u8,
    /// Unix seconds parsed from the snapshot's run-id stem; 0 when the name
    /// is not one of ours (still listed — the player may want it).
    timestamp: i64,
    size: u64,
    /// The full rc path, ready for a restore's `srcFs`.
    remote_path: []const u8,
};

/// A fetched listing and the arena its strings live in.
pub const BackupList = struct {
    arena: std.heap.ArenaAllocator,
    entries: []BackupEntry,

    pub fn deinit(self: *BackupList) void {
        self.arena.deinit();
        self.* = undefined;
    }
};

pub const ListError = rc.RcError || Allocator.Error;

/// Every snapshot for `profile`, all hosts, newest first. A backup root that
/// does not exist yet lists as empty — no machine has ever snapshotted.
pub fn listBackups(
    gpa: Allocator,
    client: *rc.Client,
    remote: []const u8,
    profile: []const u8,
) ListError!BackupList {
    var arena: std.heap.ArenaAllocator = .init(gpa);
    errdefer arena.deinit();
    const alloc = arena.allocator();

    const root = try std.fmt.allocPrint(alloc, "{s}:config-backups/{s}", .{ remote, profile });

    var object: std.json.ObjectMap = .empty;
    defer object.deinit(gpa);
    try object.put(gpa, "fs", .{ .string = root });
    try object.put(gpa, "remote", .{ .string = "" });
    var opt: std.json.ObjectMap = .empty;
    defer opt.deinit(gpa);
    try opt.put(gpa, "recurse", .{ .bool = true });
    try opt.put(gpa, "filesOnly", .{ .bool = true });
    try object.put(gpa, "opt", .{ .object = opt });

    var entries: std.ArrayList(BackupEntry) = .empty;
    defer entries.deinit(gpa);

    var reply = client.call("operations/list", .{ .object = object }) catch |err| switch (err) {
        error.RcFailed => return .{ .arena = arena, .entries = &.{} },
        else => |e| return e,
    };
    defer reply.deinit();

    const top = switch (reply.value) {
        .object => |o| o,
        else => return error.BadJson,
    };
    const list = top.get("list") orelse return .{ .arena = arena, .entries = &.{} };
    const items = switch (list) {
        .array => |a| a,
        else => return error.BadJson,
    };

    for (items.items) |item| {
        const entry = switch (item) {
            .object => |o| o,
            else => continue,
        };
        if (entry.get("IsDir")) |is_dir| {
            if (is_dir == .bool and is_dir.bool) continue;
        }
        const rel = switch (entry.get("Path") orelse continue) {
            .string => |s| s,
            else => continue,
        };
        // Exactly `<host>/<file>`: anything deeper or shallower was not put
        // there by a snapshot.
        const slash = std.mem.indexOfScalar(u8, rel, '/') orelse continue;
        if (std.mem.indexOfScalarPos(u8, rel, slash + 1, '/') != null) continue;
        const file = rel[slash + 1 ..];
        if (!std.mem.endsWith(u8, file, ".cfg")) continue;

        const size: u64 = if (entry.get("Size")) |s| switch (s) {
            .integer => |v| if (v >= 0) @intCast(v) else 0,
            else => 0,
        } else 0;

        const stem = file[0 .. file.len - ".cfg".len];
        const timestamp = engine.runIdTimestamp(stem) orelse 0;

        try entries.append(gpa, .{
            .id = try alloc.dupe(u8, rel),
            .host = try alloc.dupe(u8, rel[0..slash]),
            .timestamp = timestamp,
            .size = size,
            .remote_path = try std.fmt.allocPrint(alloc, "{s}/{s}", .{ root, rel }),
        });
    }

    std.mem.sort(BackupEntry, entries.items, {}, newestFirstEntry);
    return .{ .arena = arena, .entries = try alloc.dupe(BackupEntry, entries.items) };
}

fn newestFirstEntry(_: void, a: BackupEntry, b: BackupEntry) bool {
    if (a.timestamp != b.timestamp) return a.timestamp > b.timestamp;
    return std.mem.order(u8, a.id, b.id) == .gt;
}

/// Delete the oldest snapshots beyond `keep_per_host` — per host, never
/// globally: a machine idle for months must not lose its history because
/// another machine is busy. The newest entry of every host survives whatever
/// the retention setting says. Returns how many were removed.
pub fn pruneBackups(
    gpa: Allocator,
    client: *rc.Client,
    remote: []const u8,
    profile: []const u8,
    keep_per_host: u32,
) ListError!u32 {
    var list = try listBackups(gpa, client, remote, profile);
    defer list.deinit();

    const keep = @max(keep_per_host, 1);
    const root = try std.fmt.allocPrint(gpa, "{s}:config-backups/{s}", .{ remote, profile });
    defer gpa.free(root);

    var removed: u32 = 0;
    // The list is globally newest-first, so within each host it is too:
    // count down each host's allowance and delete past it.
    for (list.entries, 0..) |entry, index| {
        // Only snapshots are retention's to delete. The listing shows a
        // foreign file deliberately (the player should see what is in the
        // tree), but its stem is not a run id, and this code never deletes
        // what no snapshot created.
        const slash = std.mem.indexOfScalar(u8, entry.id, '/') orelse continue;
        const file = entry.id[slash + 1 ..];
        if (!std.mem.endsWith(u8, file, ".cfg")) continue;
        if (engine.runIdTimestamp(file[0 .. file.len - ".cfg".len]) == null) continue;

        var seen: u32 = 0;
        for (list.entries[0..index]) |earlier| {
            if (std.mem.eql(u8, earlier.host, entry.host)) seen += 1;
        }
        if (seen < keep) continue;

        var object: std.json.ObjectMap = .empty;
        defer object.deinit(gpa);
        try object.put(gpa, "fs", .{ .string = root });
        try object.put(gpa, "remote", .{ .string = entry.id });
        var reply = client.call("operations/deletefile", .{ .object = object }) catch continue;
        reply.deinit();
        removed += 1;
    }
    return removed;
}

// -- Staged restore ------------------------------------------------------------
//
// A restore is staged, never applied live: the game rewrites `config.cfg`
// from its in-memory options at shutdown and from eight other
// `SerializeConfig` call sites, so a restored file written mid-session is
// discarded before the player ever sees it. The download lands in
// `<profile>/.cloudsync-restore/<nonce>/` as one atomic transaction —
// payload, `meta.json` (mode, source, SHA-256, nonce, creation time), and a
// `COMMIT` marker written last. Which stage is *current* is a separate
// question from whether a stage is *whole*: an `ACTIVE` file naming the
// chosen nonce, published by rename, answers it — timestamps would lie under
// a clock rollback, equal stamps, or coarse resolution.
//
// The apply step is purely local — no daemon, no network, no credentials —
// because a restore already downloaded has to finish regardless of the
// feature's current state. It merges at apply time against `config.cfg` as
// it stands at that startup, snapshots the pre-restore config once per
// stage nonce (so a crash-interrupted apply retried never captures the
// already-restored file), installs via temp-then-rename, and tears down
// `ACTIVE` first so a crash mid-cleanup leaves unreferenced debris rather
// than a pointer to nothing.

pub const restore_dir_name = ".cloudsync-restore";
pub const active_name = "ACTIVE";
pub const commit_name = "COMMIT";
pub const payload_name = "payload.cfg";
pub const meta_name = "meta.json";
/// Undo snapshots live here, keyed by stage nonce; `LATEST_UNDO` names the
/// most recent, rename-published, because a random nonce carries no order.
pub const undo_dir_relative = ".cloudsync-trash/config";
pub const latest_undo_name = "LATEST_UNDO";

pub const RestoreMode = enum {
    /// Every key from the backup except those under `GFX.`, which are kept
    /// from the local file: restoring another machine's settings must not
    /// import its monitor layout.
    merge_keep_local_gfx,
    /// Verbatim. Survivable — an alien resolution falls back to Auto, a
    /// missing monitor to display 0 — but both failures are silent, which is
    /// why the UI warns before offering it.
    full,
};

pub const StageError = rc.RcError || Allocator.Error || error{StageUnwritable};

/// Download `entry_id` into a fresh stage and publish it. A failed download
/// deletes only its own partial stage: the previously published one, and
/// `ACTIVE`, remain exactly as they were — a player is never left with
/// neither the old restore nor the new. Returns the new stage's nonce.
pub fn stageRestore(
    gpa: Allocator,
    io: Io,
    client: *rc.Client,
    profile_dir: []const u8,
    remote: []const u8,
    profile: []const u8,
    entry_id: []const u8,
    mode: RestoreMode,
) StageError![]u8 {
    const root = try std.Io.Dir.path.join(gpa, &.{ profile_dir, restore_dir_name });
    defer gpa.free(root);
    Io.Dir.cwd().createDirPath(io, root) catch return error.StageUnwritable;

    const nonce = try plan.runId(gpa, io);
    errdefer gpa.free(nonce);
    const stage = try std.Io.Dir.path.join(gpa, &.{ root, nonce });
    defer gpa.free(stage);
    Io.Dir.cwd().createDirPath(io, stage) catch return error.StageUnwritable;
    errdefer Io.Dir.cwd().deleteTree(io, stage) catch {};

    // The download, straight into the stage.
    {
        const src_root = try std.fmt.allocPrint(gpa, "{s}:config-backups/{s}", .{ remote, profile });
        defer gpa.free(src_root);
        var object: std.json.ObjectMap = .empty;
        defer object.deinit(gpa);
        try object.put(gpa, "srcFs", .{ .string = src_root });
        try object.put(gpa, "srcRemote", .{ .string = entry_id });
        try object.put(gpa, "dstFs", .{ .string = stage });
        try object.put(gpa, "dstRemote", .{ .string = payload_name });
        var reply = try client.call("operations/copyfile", .{ .object = object });
        reply.deinit();
    }

    const payload_path = try std.Io.Dir.path.join(gpa, &.{ stage, payload_name });
    defer gpa.free(payload_path);
    const payload = Io.Dir.cwd().readFileAlloc(io, payload_path, gpa, .limited(1 << 22)) catch
        return error.StageUnwritable;
    defer gpa.free(payload);

    try commitAndPublish(gpa, io, root, stage, nonce, payload, mode, entry_id);
    return nonce;
}

/// The shared back half of staging: metadata with the payload's hash,
/// `COMMIT` last, then the `ACTIVE` rename — the single point after which
/// this stage is the one that applies, last writer winning outright.
fn commitAndPublish(
    gpa: Allocator,
    io: Io,
    root: []const u8,
    stage: []const u8,
    nonce: []const u8,
    payload: []const u8,
    mode: RestoreMode,
    entry_id: []const u8,
) (Allocator.Error || error{StageUnwritable})!void {
    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(payload, &digest, .{});
    var hex_buffer: [64]u8 = undefined;
    const hex = std.fmt.bufPrint(&hex_buffer, "{x}", .{&digest}) catch unreachable;

    const created = Io.Clock.now(.real, io).toSeconds();
    const meta = try stageMetaJson(gpa, mode, entry_id, hex, nonce, created);
    defer gpa.free(meta);
    const meta_path = try std.Io.Dir.path.join(gpa, &.{ stage, meta_name });
    defer gpa.free(meta_path);
    Io.Dir.cwd().writeFile(io, .{ .sub_path = meta_path, .data = meta }) catch
        return error.StageUnwritable;

    const commit_path = try std.Io.Dir.path.join(gpa, &.{ stage, commit_name });
    defer gpa.free(commit_path);
    Io.Dir.cwd().writeFile(io, .{ .sub_path = commit_path, .data = "1" }) catch
        return error.StageUnwritable;

    publishPointer(gpa, io, root, active_name, nonce) catch return error.StageUnwritable;
}

/// The stage's `meta.json`, through the JSON serializer — never a format
/// template: the entry id is a remote path the listing deliberately admits
/// foreign names into, and a quote or backslash spliced raw would produce
/// metadata the apply step rejects as corrupt after a successful download.
pub fn stageMetaJson(
    gpa: Allocator,
    mode: RestoreMode,
    entry_id: []const u8,
    sha256_hex: []const u8,
    nonce: []const u8,
    created_unix: i64,
) Allocator.Error![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    var json: std.json.Stringify = .{ .writer = &out.writer };
    meta: {
        json.beginObject() catch break :meta;
        json.objectField("mode") catch break :meta;
        json.write(@tagName(mode)) catch break :meta;
        json.objectField("entry_id") catch break :meta;
        json.write(entry_id) catch break :meta;
        json.objectField("sha256") catch break :meta;
        json.write(sha256_hex) catch break :meta;
        json.objectField("nonce") catch break :meta;
        json.write(nonce) catch break :meta;
        json.objectField("created_unix") catch break :meta;
        json.write(created_unix) catch break :meta;
        json.endObject() catch break :meta;
        return out.toOwnedSlice();
    }
    return error.OutOfMemory;
}

fn publishPointer(
    gpa: Allocator,
    io: Io,
    dir: []const u8,
    comptime name: []const u8,
    value: []const u8,
) !void {
    const tmp = try std.Io.Dir.path.join(gpa, &.{ dir, name ++ ".tmp" });
    defer gpa.free(tmp);
    const final = try std.Io.Dir.path.join(gpa, &.{ dir, name });
    defer gpa.free(final);
    try Io.Dir.cwd().writeFile(io, .{ .sub_path = tmp, .data = value });
    try Io.Dir.rename(.cwd(), tmp, .cwd(), final, io);
}

pub const ApplyOutcome = enum { applied, nothing_staged };

pub const ApplyError = Allocator.Error || error{
    /// The stage `ACTIVE` names exists and fails validation — no `COMMIT`,
    /// a payload that does not match its hash, unreadable metadata. We were
    /// told to apply specific content and that content is wrong; falling
    /// back to another stage would silently apply a restore the player did
    /// not choose, so this stops the caller instead.
    StageCorrupt,
    /// The merged result could not be installed.
    ConfigUnwritable,
};

const StageMeta = struct {
    mode: []const u8,
    entry_id: []const u8,
    sha256: []const u8,
    nonce: []const u8,
    created_unix: i64,
};

/// Apply the published stage, if any. Purely local; safe to call
/// unconditionally at startup. `ACTIVE` naming a directory that is absent
/// entirely is outside interference (correct teardown cannot produce it):
/// the pointer is cleared, debris swept, and nothing staged reported —
/// distinctly from a present-but-invalid stage, which is the hard error.
pub fn applyPendingRestore(
    gpa: Allocator,
    io: Io,
    profile_dir: []const u8,
) ApplyError!ApplyOutcome {
    const root = try std.Io.Dir.path.join(gpa, &.{ profile_dir, restore_dir_name });
    defer gpa.free(root);
    const active_path = try std.Io.Dir.path.join(gpa, &.{ root, active_name });
    defer gpa.free(active_path);

    const active_raw = Io.Dir.cwd().readFileAlloc(io, active_path, gpa, .limited(256)) catch {
        // No pointer: the ordinary state, and the state a crash between the
        // two teardown removals leaves — sweep whatever survived it.
        gcStages(gpa, io, root);
        return .nothing_staged;
    };
    defer gpa.free(active_raw);
    const nonce = std.mem.trim(u8, active_raw, " \t\r\n");

    const stage = try std.Io.Dir.path.join(gpa, &.{ root, nonce });
    defer gpa.free(stage);
    if (Io.Dir.cwd().statFile(io, stage, .{})) |_| {} else |_| {
        Io.Dir.cwd().deleteFile(io, active_path) catch {};
        gcStages(gpa, io, root);
        return .nothing_staged;
    }

    // From here the stage exists and every defect is a hard error.
    const commit_path = try std.Io.Dir.path.join(gpa, &.{ stage, commit_name });
    defer gpa.free(commit_path);
    _ = Io.Dir.cwd().statFile(io, commit_path, .{}) catch return error.StageCorrupt;

    const meta_path = try std.Io.Dir.path.join(gpa, &.{ stage, meta_name });
    defer gpa.free(meta_path);
    const meta_text = Io.Dir.cwd().readFileAlloc(io, meta_path, gpa, .limited(4096)) catch
        return error.StageCorrupt;
    defer gpa.free(meta_text);
    const parsed_meta = std.json.parseFromSlice(StageMeta, gpa, meta_text, .{
        .ignore_unknown_fields = true,
        .allocate = .alloc_always,
    }) catch return error.StageCorrupt;
    defer parsed_meta.deinit();
    const mode = std.meta.stringToEnum(RestoreMode, parsed_meta.value.mode) orelse
        return error.StageCorrupt;

    const payload_path = try std.Io.Dir.path.join(gpa, &.{ stage, payload_name });
    defer gpa.free(payload_path);
    const payload = Io.Dir.cwd().readFileAlloc(io, payload_path, gpa, .limited(1 << 22)) catch
        return error.StageCorrupt;
    defer gpa.free(payload);

    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(payload, &digest, .{});
    var hex_buffer: [64]u8 = undefined;
    const hex = std.fmt.bufPrint(&hex_buffer, "{x}", .{&digest}) catch unreachable;
    if (!std.mem.eql(u8, hex, parsed_meta.value.sha256)) return error.StageCorrupt;

    const config_path = try std.Io.Dir.path.join(gpa, &.{ profile_dir, config_file_name });
    defer gpa.free(config_path);
    const local = Io.Dir.cwd().readFileAlloc(io, config_path, gpa, .limited(1 << 22)) catch
        try gpa.dupe(u8, "");
    defer gpa.free(local);

    // The undo snapshot, keyed by stage nonce and written exactly once: a
    // retried apply after a crash reuses the first one, so the *original*
    // config stays recoverable however many times the apply is interrupted.
    const undo_dir = try std.Io.Dir.path.join(gpa, &.{ profile_dir, undo_dir_relative });
    defer gpa.free(undo_dir);
    Io.Dir.cwd().createDirPath(io, undo_dir) catch return error.ConfigUnwritable;
    const undo_file = try std.fmt.allocPrint(gpa, "{s}.cfg", .{nonce});
    defer gpa.free(undo_file);
    const undo_path = try std.Io.Dir.path.join(gpa, &.{ undo_dir, undo_file });
    defer gpa.free(undo_path);
    // A leftover `.tmp` here is the debris of a crash mid-copy, never
    // something to reuse — sweep on the way past, so "skip if present" below
    // only ever trusts a name the rename made whole.
    sweepUndoTmp(gpa, io, undo_dir);
    if (Io.Dir.cwd().statFile(io, undo_path, .{})) |_| {} else |_| {
        // Atomic: a crash mid-copy must not leave a truncated snapshot the
        // retry would then trust.
        const undo_tmp = try std.fmt.allocPrint(gpa, "{s}.tmp", .{undo_path});
        defer gpa.free(undo_tmp);
        Io.Dir.cwd().writeFile(io, .{ .sub_path = undo_tmp, .data = local }) catch
            return error.ConfigUnwritable;
        Io.Dir.rename(.cwd(), undo_tmp, .cwd(), undo_path, io) catch
            return error.ConfigUnwritable;
    }
    publishPointer(gpa, io, undo_dir, latest_undo_name, nonce) catch
        return error.ConfigUnwritable;

    // Merge at apply time, against the config as it stands right now —
    // settings changed since staging fold in rather than freezing.
    const merged = switch (mode) {
        .full => try gpa.dupe(u8, payload),
        .merge_keep_local_gfx => try mergeConfig(gpa, local, payload),
    };
    defer gpa.free(merged);

    const config_tmp = try std.fmt.allocPrint(gpa, "{s}.tmp-restore", .{config_path});
    defer gpa.free(config_tmp);
    Io.Dir.cwd().writeFile(io, .{ .sub_path = config_tmp, .data = merged }) catch
        return error.ConfigUnwritable;
    Io.Dir.rename(.cwd(), config_tmp, .cwd(), config_path, io) catch
        return error.ConfigUnwritable;

    // Teardown: ACTIVE first — a single atomic unlink — then the debris. The
    // reverse order would leave a pointer to nothing, which the hard-error
    // rule above turns into a bricked startup.
    Io.Dir.cwd().deleteFile(io, active_path) catch {};
    gcStages(gpa, io, root);
    return .applied;
}

/// Sweep every stage directory — called only when `ACTIVE` is absent, which
/// makes everything under the restore root unreferenced debris. A root that
/// still has an `ACTIVE` is left entirely alone.
pub fn gcStages(gpa: Allocator, io: Io, root: []const u8) void {
    const active_path = std.Io.Dir.path.join(gpa, &.{ root, active_name }) catch return;
    defer gpa.free(active_path);
    if (Io.Dir.cwd().statFile(io, active_path, .{})) |_| {
        return;
    } else |_| {}

    var dir = Io.Dir.cwd().openDir(io, root, .{ .iterate = true }) catch return;
    defer dir.close(io);
    var names: std.ArrayList([]u8) = .empty;
    defer {
        for (names.items) |name| gpa.free(name);
        names.deinit(gpa);
    }
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        const copy = gpa.dupe(u8, entry.name) catch continue;
        names.append(gpa, copy) catch {
            gpa.free(copy);
            continue;
        };
    }
    for (names.items) |name| {
        const child = std.Io.Dir.path.join(gpa, &.{ root, name }) catch continue;
        defer gpa.free(child);
        Io.Dir.cwd().deleteTree(io, child) catch {};
    }
}

fn sweepUndoTmp(gpa: Allocator, io: Io, undo_dir: []const u8) void {
    var dir = Io.Dir.cwd().openDir(io, undo_dir, .{ .iterate = true }) catch return;
    defer dir.close(io);
    var names: std.ArrayList([]u8) = .empty;
    defer {
        for (names.items) |name| gpa.free(name);
        names.deinit(gpa);
    }
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (!std.mem.endsWith(u8, entry.name, ".tmp")) continue;
        const copy = gpa.dupe(u8, entry.name) catch continue;
        names.append(gpa, copy) catch {
            gpa.free(copy);
            continue;
        };
    }
    for (names.items) |name| {
        const child = std.Io.Dir.path.join(gpa, &.{ undo_dir, name }) catch continue;
        defer gpa.free(child);
        Io.Dir.cwd().deleteFile(io, child) catch {};
    }
}

// -- Undo ----------------------------------------------------------------------
//
// Restoring is itself reversible, in two distinct senses the UI must name
// separately: a restore that is *staged but unapplied* is cancelled —
// nothing has happened yet, so there is nothing to reinstate — while a
// restore that has *been applied* is reversed by staging the pre-restore
// snapshot back through the very same protocol. An undo that wrote
// `config.cfg` live would be discarded by the shutdown rewrite exactly as an
// unstaged restore is; going through the stage means the P06-M02 startup
// apply installs it, and the apply's own snapshot of the pre-undo config
// makes undo-of-undo a redo for free.

pub const UndoAction = enum {
    /// A staged, unapplied restore was discarded. `config.cfg` untouched.
    cancelled_stage,
    /// The `LATEST_UNDO` snapshot is staged as a full-mode restore; the next
    /// startup applies it. An undo reinstates the file as it was, so it
    /// never merges.
    staged_reinstate,
};

pub const UndoAvailability = enum {
    none,
    /// A staged restore exists and can be cancelled before it applies.
    cancellable,
    /// An applied restore can be reversed from its snapshot.
    reinstatable,
};

pub const UndoError = Allocator.Error || error{
    /// Neither a stage to cancel nor a snapshot to reinstate.
    NothingToUndo,
    /// `LATEST_UNDO` names a snapshot that cannot be read.
    UndoCorrupt,
    StageUnwritable,
};

/// What undo would do right now. The third answer — busy, while a restore
/// download holds the operation slot — belongs to the job layer: the worker
/// runs one job at a time, and the ABI reports busy from there. Reporting
/// available during a download is what lets a stale `LATEST_UNDO` invite an
/// undo the finishing download then silently overwrites.
pub fn restoreUndoAvailability(
    gpa: Allocator,
    io: Io,
    profile_dir: []const u8,
) Allocator.Error!UndoAvailability {
    const active_path = try std.Io.Dir.path.join(gpa, &.{ profile_dir, restore_dir_name, active_name });
    defer gpa.free(active_path);
    if (Io.Dir.cwd().statFile(io, active_path, .{})) |_| {
        return .cancellable;
    } else |_| {}

    const undo_dir = try std.Io.Dir.path.join(gpa, &.{ profile_dir, undo_dir_relative });
    defer gpa.free(undo_dir);
    const pointer_path = try std.Io.Dir.path.join(gpa, &.{ undo_dir, latest_undo_name });
    defer gpa.free(pointer_path);
    const pointer = Io.Dir.cwd().readFileAlloc(io, pointer_path, gpa, .limited(256)) catch
        return .none;
    defer gpa.free(pointer);
    const nonce = std.mem.trim(u8, pointer, " \t\r\n");

    const snapshot_file = try std.fmt.allocPrint(gpa, "{s}.cfg", .{nonce});
    defer gpa.free(snapshot_file);
    const snapshot_path = try std.Io.Dir.path.join(gpa, &.{ undo_dir, snapshot_file });
    defer gpa.free(snapshot_path);
    if (Io.Dir.cwd().statFile(io, snapshot_path, .{})) |_| {
        return .reinstatable;
    } else |_| {}
    return .none;
}

/// Undo, branching on which state it finds. Purely local, like the apply —
/// reversing a restore must work with rclone gone and the feature off.
pub fn undoRestore(gpa: Allocator, io: Io, profile_dir: []const u8) UndoError!UndoAction {
    const root = try std.Io.Dir.path.join(gpa, &.{ profile_dir, restore_dir_name });
    defer gpa.free(root);
    const active_path = try std.Io.Dir.path.join(gpa, &.{ root, active_name });
    defer gpa.free(active_path);

    if (Io.Dir.cwd().statFile(io, active_path, .{})) |_| {
        // Staged, unapplied: cancel. Same teardown direction as the apply —
        // the pointer first, a single atomic unlink, then the debris.
        // Directory-first would recreate the dangling-ACTIVE state P04-M03
        // must treat as a hard error.
        Io.Dir.cwd().deleteFile(io, active_path) catch {};
        gcStages(gpa, io, root);
        return .cancelled_stage;
    } else |_| {}

    const undo_dir = try std.Io.Dir.path.join(gpa, &.{ profile_dir, undo_dir_relative });
    defer gpa.free(undo_dir);
    const pointer_path = try std.Io.Dir.path.join(gpa, &.{ undo_dir, latest_undo_name });
    defer gpa.free(pointer_path);
    const pointer = Io.Dir.cwd().readFileAlloc(io, pointer_path, gpa, .limited(256)) catch
        return error.NothingToUndo;
    defer gpa.free(pointer);
    const undo_nonce = std.mem.trim(u8, pointer, " \t\r\n");

    const snapshot_file = try std.fmt.allocPrint(gpa, "{s}.cfg", .{undo_nonce});
    defer gpa.free(snapshot_file);
    const snapshot_path = try std.Io.Dir.path.join(gpa, &.{ undo_dir, snapshot_file });
    defer gpa.free(snapshot_path);
    const snapshot = Io.Dir.cwd().readFileAlloc(io, snapshot_path, gpa, .limited(1 << 22)) catch
        return error.UndoCorrupt;
    defer gpa.free(snapshot);

    // Reinstate through the same staging path as a restore: payload, meta,
    // COMMIT last, ACTIVE rename. Full mode — the snapshot *is* the file as
    // it was.
    Io.Dir.cwd().createDirPath(io, root) catch return error.StageUnwritable;
    const nonce = try plan.runId(gpa, io);
    defer gpa.free(nonce);
    const stage = try std.Io.Dir.path.join(gpa, &.{ root, nonce });
    defer gpa.free(stage);
    Io.Dir.cwd().createDirPath(io, stage) catch return error.StageUnwritable;
    errdefer Io.Dir.cwd().deleteTree(io, stage) catch {};

    const payload_path = try std.Io.Dir.path.join(gpa, &.{ stage, payload_name });
    defer gpa.free(payload_path);
    Io.Dir.cwd().writeFile(io, .{ .sub_path = payload_path, .data = snapshot }) catch
        return error.StageUnwritable;

    const source = try std.fmt.allocPrint(gpa, "undo:{s}", .{undo_nonce});
    defer gpa.free(source);
    try commitAndPublish(gpa, io, root, stage, nonce, snapshot, .full, source);
    return .staged_reinstate;
}

// -- The GFX-preserving merge -------------------------------------------------

/// Every key from the backup except those under `GFX.`, which are kept from
/// the local file. `config.cfg` is the option system's XML: `<item>` blocks
/// carrying a `<KeyName>` each. The merge walks the backup's text and
/// substitutes the local block wherever the key is GFX-prefixed and the
/// local file has one; everything else — other items, binds, surrounding
/// structure — passes through from the backup byte-for-byte.
pub fn mergeConfig(
    gpa: Allocator,
    local_cfg: []const u8,
    restored_cfg: []const u8,
) Allocator.Error![]u8 {
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(gpa);

    var index: usize = 0;
    while (nextItem(restored_cfg, index)) |block| {
        try out.appendSlice(gpa, restored_cfg[index..block.start]);
        const item = restored_cfg[block.start..block.end];
        if (keyNameOf(item)) |key| {
            if (std.mem.startsWith(u8, key, "GFX.")) {
                if (findItemByKey(local_cfg, key)) |local_item| {
                    try out.appendSlice(gpa, local_item);
                    index = block.end;
                    continue;
                }
            }
        }
        try out.appendSlice(gpa, item);
        index = block.end;
    }
    try out.appendSlice(gpa, restored_cfg[index..]);
    return out.toOwnedSlice(gpa);
}

const ItemSpan = struct { start: usize, end: usize };

fn nextItem(text: []const u8, from: usize) ?ItemSpan {
    const start = std.mem.indexOfPos(u8, text, from, "<item") orelse return null;
    const close = std.mem.indexOfPos(u8, text, start, "</item>") orelse return null;
    return .{ .start = start, .end = close + "</item>".len };
}

fn keyNameOf(item: []const u8) ?[]const u8 {
    const open = std.mem.indexOf(u8, item, "<KeyName>") orelse return null;
    const value_start = open + "<KeyName>".len;
    const close = std.mem.indexOfPos(u8, item, value_start, "</KeyName>") orelse return null;
    return std.mem.trim(u8, item[value_start..close], " \t\r\n");
}

fn findItemByKey(text: []const u8, key: []const u8) ?[]const u8 {
    var index: usize = 0;
    while (nextItem(text, index)) |block| {
        const item = text[block.start..block.end];
        if (keyNameOf(item)) |item_key| {
            if (std.mem.eql(u8, item_key, key)) return item;
        }
        index = block.end;
    }
    return null;
}
