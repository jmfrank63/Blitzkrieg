//! The files inspector with a large mod (docs/superpowers/specs/2026-10-26-camera-turn-design.md,
//! "Dark videos at startup"). The maintainer's Game started with
//! -mod=remastered_views, a mod of 154,000 loose files, and spent 8 s with a
//! black, Not Responding window before the first video: the files inspector,
//! which collects the tutorials, custom missions, chapters and campaigns and
//! the multiplayer maps, listed every file of the game and the mod to find
//! them, at startup and again when the mod was mounted.
//!
//! The real Game is started twice to its main menu, once without a mod and
//! once with a mod of `bulk_files` loose files outside those folders and one
//! custom mission. From the Game's own log (BK_DEBUG_LOG, "files inspector:
//! <ms> ms, <folder> <streams>, ..."):
//!
//! - every inspection must take under `max_inspect_ms` (generous: it took
//!   3 to 5 s for the game's data alone before, 36 ms after, on win-home);
//! - with the mod, the custom missions must be one more than without it, and
//!   every other folder the same: the mod's mission is found, and nothing
//!   outside the folders is.
//!
//! The Game runs from zig-out/local-test/mod-scan with every user path
//! redirected; logs and scores.txt stay there. BK_VIEWS_GAME names the
//! installed Game; `zig build mod-scan-game-reads-it` sets it.
const std = @import("std");
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/mod-scan";
const mod_name = "bulk_scan";
/// Loose files the mod carries where the inspector does not look.
const bulk_files = 20000;
const max_inspect_ms = 1500;

/// One "files inspector:" line: its time and the streams of each folder.
const Inspection = struct { ms: u32, folders: []const u8 };

fn parseInspections(allocator: std.mem.Allocator, log: []const u8) ![]const Inspection {
    var out: std.ArrayList(Inspection) = .empty;
    for (try control.lines(allocator, log, "files inspector: ")) |line| {
        const space = std.mem.indexOfScalar(u8, line, ' ') orelse return error.MissingTrace;
        const comma = std.mem.indexOfScalar(u8, line, ',') orelse line.len;
        try out.append(allocator, .{ .ms = try std.fmt.parseInt(u32, line[0..space], 10), .folders = std.mem.trim(u8, line[@min(comma + 1, line.len)..], " \r") });
    }
    return out.items;
}

/// The streams the inspection counted in `folder`.
fn streams(folders: []const u8, folder: []const u8) ?u32 {
    var it = std.mem.tokenizeSequence(u8, folders, ", ");
    while (it.next()) |entry| {
        const space = std.mem.lastIndexOfScalar(u8, entry, ' ') orelse continue;
        if (std.mem.eql(u8, entry[0..space], folder)) return std.fmt.parseInt(u32, entry[space + 1 ..], 10) catch null;
    }
    return null;
}

test "the files inspector looks only in its folders, with a large mod mounted" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build mod-scan-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" }));

    const plain = try control.runScheduleWith(io, allocator, game, root, mod_name, "no-mod", "", "t2000:exit", "", .{ .mod = false });
    const modded = try control.runScheduleWith(io, allocator, game, root, mod_name, "bulk-mod", "", "t2000:exit", "", .{});
    const plain_runs = try parseInspections(allocator, plain.log);
    const modded_runs = try parseInspections(allocator, modded.log);

    var failed = plain_runs.len == 0 or modded_runs.len < 2;
    var report: std.ArrayList(u8) = .empty;
    for ([_]struct { []const u8, []const Inspection }{ .{ "without the mod", plain_runs }, .{ "with the mod", modded_runs } }) |run| {
        for (run[1]) |inspection| {
            try report.print(allocator, "{s}: {d} ms, {s}\n", .{ run[0], inspection.ms, inspection.folders });
            if (inspection.ms > max_inspect_ms) failed = true;
        }
    }
    if (!failed) {
        const before = plain_runs[plain_runs.len - 1].folders;
        const after = modded_runs[modded_runs.len - 1].folders;
        for ([_][]const u8{ "maps\\multiplayer\\", "scenarios\\custom\\campaigns\\", "scenarios\\custom\\chapters\\", "scenarios\\custom\\missions\\", "scenarios\\tutorials\\" }) |folder| {
            const count_before = streams(before, folder) orelse return error.MissingTrace;
            const count_after = streams(after, folder) orelse return error.MissingTrace;
            const added: u32 = if (std.mem.eql(u8, folder, "scenarios\\custom\\missions\\")) 1 else 0;
            if (count_after != count_before + added) failed = true;
        }
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the files inspector is slow or lists the wrong files (bound {d} ms); the logs are in {s}\n", .{ report.items, max_inspect_ms, root });
        return error.InspectorSlow;
    }
}

/// mod.xml, a custom mission and `bulk_files` small files under objects\.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "scenarios", "custom", "missions", "bulk_scan" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Files inspector test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "scenarios", "custom", "missions", "bulk_scan", "1.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base/>\r\n",
    });
    var folder: usize = 0;
    while (folder < bulk_files / 100) : (folder += 1) {
        const dir = try std.fmt.allocPrint(allocator, "{s}/objects/bulk/{d:0>3}", .{ data, folder });
        try cwd.createDirPath(io, dir);
        var file: usize = 0;
        while (file < 100) : (file += 1) {
            try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(allocator, "{s}/{d:0>3}.txt", .{ dir, file }), .data = "x" });
        }
    }
}
