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
