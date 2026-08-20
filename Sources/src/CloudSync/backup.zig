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
