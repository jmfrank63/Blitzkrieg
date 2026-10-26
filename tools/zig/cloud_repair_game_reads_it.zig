//! The Cloud tab's Repair on the real Game. The main menu answers a sync that
//! failed needs_resync or out_of_sync with "Repair from Settings > Cloud", so
//! the tab has to hold one: in Sync now's place while the last run failed
//! that way, behind a confirmation box that says what a resync does, and the
//! pairing's resync when the box is confirmed.
//!
//! Everything runs against a local-filesystem remote in the scratch folder,
//! never a real cloud: the credentials name rclone's `local` backend with the
//! scratch `c` folder as the remote root, and the rclone is the one the
//! install ships beside the Game. Two launches of the profile `CR`:
//!
//! - pair: Cloud.Provider set to `local`, Options > Settings > Cloud, Sync
//!   now. The first run of a profile pairs (`sync finished (paired)`).
//! - repair: before the launch Path2's listing loses its entries and a save
//!   appears on the remote only. Sync now fails needs_resync (bisync's
//!   `Empty prior Path2 listing`, a failure the engine leaves to the player),
//!   and the Repair button is clicked where Sync now was. The first box is
//!   cancelled, which must start nothing; the second is confirmed, and the
//!   repair settles paired. The remote-only save is then in the profile, the
//!   local save still on both sides.
//!
//! The trace lines and the files are the measurement. The Game runs from the
//! scratch folder with HOME, XDG_*, BK_USER_ROOT and BK_PROFILES_ROOT there,
//! so the user's profiles, settings, short links and cloud setup are never
//! touched; the logs stay in zig-out/local-test/cr. The names are short on
//! purpose: bisync names its listings after both paths, and a checkout deep
//! in a worktree must still fit the file-name budget. BK_CLOUD_REPAIR_GAME
//! names the installed Game; `zig build cloud-repair-game-reads-it` sets it.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/cr";
const profile = "CR";

/// Options > Settings, then the Cloud tab: the fifth division (10007 + 4).
const open_cloud_tab = "60:set=Cloud.Provider=local,70:settings,110:msg=10003,200:msg=10011,";
/// Sync now (10015); the entry after it waits the time a local sync needs,
/// daemon start included.
const sync_now = "260:msg=10015,";

const pair_schedule = open_cloud_tab ++ sync_now ++ "t15000:exit";
/// The Repair button sits where Sync now was: the Cloud list at (527, 130),
/// the button at (153, 455) in it, 118x46, so its middle at 1024x768 is
/// (739, 608). The click goes through the list's 7777 path a player's click
/// takes.
const repair_click = "click=739x608";
const repair_schedule = open_cloud_tab ++ sync_now ++
    "t15000:" ++ repair_click ++ ",t1500:cancel,t1500:" ++ repair_click ++ ",t1500:ok,t15000:exit";

const local_save = "quick.sav";
const local_bytes = "saved on this machine";
const remote_save = "remote_only.sav";
const remote_bytes = "saved on the other machine";

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupe(u8, value);
}

test "Settings > Cloud offers Repair after a needs_resync sync, asks, and the repair brings the remote save" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_CLOUD_REPAIR_GAME")) orelse {
        std.debug.print("BK_CLOUD_REPAIR_GAME is not set: run zig build cloud-repair-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "u" });
    const cloud = try std.fs.path.join(allocator, &.{ root, "c" });
    const profile_dir = try std.fs.path.join(allocator, &.{ user, "profiles", profile });
    const remote_dir = try std.fs.path.join(allocator, &.{ cloud, "profiles", profile });
    try cwd.createDirPath(io, profile_dir);
    try cwd.createDirPath(io, cloud);

    // The local backend, rooted at the scratch cloud folder. No secrets.
    const document = try std.fmt.allocPrint(allocator,
        "{{\"backend\":\"local\",\"remote_root\":{f},\"fingerprint\":\"local:#cloud-repair-test\"," ++
            "\"options\":{{}},\"secret_options\":[],\"password_options\":[],\"rclone_path\":null}}",
        .{std.json.fmt(cloud, .{})},
    );
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ user, "profiles", "cloud.credentials" }), .data = document });
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ profile_dir, local_save }), .data = local_bytes });

    // pair: the first Sync now of the profile pairs it.
    {
        const log = try runGame(io, allocator, game, root, user, "pair", pair_schedule);
        try expectLine(log, "cloud sync: sync now requested, sync begun for \"" ++ profile ++ "\"", "pair");
        try expectLine(log, "cloud sync: sync finished (paired)", "pair");
        try expectFile(io, allocator, remote_dir, local_save, local_bytes);
    }

    // The damage: Path2's listing without entries, and a save the other
    // machine left on the remote.
    try emptyPath2Listing(io, allocator, try std.fs.path.join(allocator, &.{ user, "cloudsync", "workdir" }));
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ remote_dir, remote_save }), .data = remote_bytes });

    // repair: Sync now fails needs_resync; Repair is clicked, its box
    // cancelled, clicked again and confirmed.
    {
        const log = try runGame(io, allocator, game, root, user, "repair", repair_schedule);
        try expectLine(log, "cloud sync: sync failed: needs_resync", "repair");
        try expectLine(log, "cloud sync: repair requested, repair begun for \"" ++ profile ++ "\"", "repair");
        // The sync's failure comes first, the repair's success after it.
        const failed_at = std.mem.indexOf(u8, log, "cloud sync: sync failed: needs_resync").?;
        const paired_at = std.mem.lastIndexOf(u8, log, "cloud sync: sync finished (paired)") orelse {
            std.debug.print("repair: no \"sync finished (paired)\" line; the Game's output is in {s}/repair.log\n", .{root});
            return error.RepairDidNotFinish;
        };
        try testing.expect(paired_at > failed_at);
        // Two clicks on Repair, one repair: the cancelled box started nothing.
        try testing.expectEqual(@as(usize, 2), std.mem.count(u8, log, "action " ++ repair_click));
        try testing.expectEqual(@as(usize, 1), std.mem.count(u8, log, "cloud sync: repair requested"));
    }
    // The remote save arrived, and nothing was deleted on either side.
    try expectFile(io, allocator, profile_dir, remote_save, remote_bytes);
    try expectFile(io, allocator, profile_dir, local_save, local_bytes);
    try expectFile(io, allocator, remote_dir, local_save, local_bytes);
    try expectFile(io, allocator, remote_dir, remote_save, remote_bytes);
    const state = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ user, "cloudsync", "state", profile ++ ".json" }), allocator, .limited(1 << 16));
    try testing.expect(std.mem.indexOf(u8, state, "\"paired\":true") != null);
}

/// Every Path2 listing in the workdir rewritten to one line bisync ignores.
fn emptyPath2Listing(io: std.Io, allocator: std.mem.Allocator, workdir: []const u8) !void {
    var dir = try std.Io.Dir.cwd().openDir(io, workdir, .{ .iterate = true });
    defer dir.close(io);
    var names: std.ArrayList([]const u8) = .empty;
    var it = dir.iterate();
    while (try it.next(io)) |entry| {
        if (std.mem.endsWith(u8, entry.name, ".path2.lst")) try names.append(allocator, try allocator.dupe(u8, entry.name));
    }
    if (names.items.len == 0) {
        std.debug.print("no Path2 listing in {s} after the pairing run\n", .{workdir});
        return error.NoListing;
    }
    for (names.items) |name| try dir.writeFile(io, .{ .sub_path = name, .data = "garbage\n" });
}

/// Runs the Game in the scratch user folder under `schedule` from the main
/// menu; it must exit 0 without a leak. Its output is kept as `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, user: []const u8, name: []const u8, schedule: []const u8) ![]const u8 {
    const cwd = std.Io.Dir.cwd();
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("HOME", user);
    try environ.put("XDG_DATA_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-data" }));
    try environ.put("XDG_CONFIG_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-config" }));
    try environ.put("XDG_CACHE_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-cache" }));
    try environ.put("XDG_STATE_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-state" }));
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MODS_ROOT", try std.fs.path.join(allocator, &.{ root, "mods" }));
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_NO_HELP", "1");
    try environ.put("BK_AUTO_UI", schedule);
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", "-mode=1024x768", "-profile=" ++ profile },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    return log;
}

fn expectLine(log: []const u8, line: []const u8, name: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("{s}: no \"{s}\" line; the Game's output is in " ++ scratch ++ "/{s}.log\n", .{ name, line, name });
    return error.LineMissing;
}

fn expectFile(io: std.Io, allocator: std.mem.Allocator, dir: []const u8, name: []const u8, expected: []const u8) !void {
    const file_path = try std.fs.path.join(allocator, &.{ dir, name });
    const bytes = std.Io.Dir.cwd().readFileAlloc(io, file_path, allocator, .limited(1 << 16)) catch |err| {
        std.debug.print("{s}: {s}\n", .{ file_path, @errorName(err) });
        return err;
    };
    if (std.mem.eql(u8, bytes, expected)) return;
    std.debug.print("{s} holds \"{s}\", expected \"{s}\"\n", .{ file_path, bytes, expected });
    return error.FileDiffers;
}
