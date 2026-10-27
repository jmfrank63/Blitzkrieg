//! The village of the hand test (M006 S07,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md): the maintainer
//! played Poland (the first German mission, tutorial1's map) with the camera
//! turn and reported that its houses do not turn and stand off their places,
//! differently per resolution.
//!
//! Four of that village's variants (e_jettybuilding_4, the church with the
//! clock; e_house02_2, e_house01_2, e_house09_1) get their turns from
//! `bk-views side` into a scratch mod at their Data paths, as
//! `zig build camera-turn-mod` puts the batch's. A map written from
//! `Data/Maps/river3d.xml` (flat, so a building's place on the screen is the
//! 2003 camera's projection of its map position) holds them on a ring around
//! the screen centre, one on each AI axis (240 by 120 pixels right or left
//! and up or down from the centre), so each quarter turn takes the ring onto
//! itself. The real Game
//! (Remastered on, objects only) shoots all four steps in windows of
//! 1024 x 768, 1280 x 1024 and 1920 x 1080. In every shot each building must
//!
//! - show its turn k (the outline IoU of its `_r<k>` sprite above the floor,
//!   better than any other turn of it at that place), and
//! - stand where the turn of the 2003 camera puts it: a screen offset (dx, dy)
//!   from the centre (w / 2 - 1, h / 2 - 1) goes to (2 dy, -dx / 2) per step;
//!   the best of the places within 6 pixels must be that one.
//!
//! The Game runs from zig-out/local-test/camera-turn/village with every user
//! path redirected (camera_turn_game_reads_it.runGameSized); frames, logs and
//! scores.txt stay there. BK_VIEWS_GAME names the installed Game and
//! BK_VIEWS_TOOL bk-views; `zig build camera-turn-village-game-reads-it` sets
//! both.
const std = @import("std");
const views = @import("bk_views.zig");
const turn = @import("camera_turn_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/village";
const mod_buildings = "buildings/europe/summer";
/// `camera=` 512 puts river3d's middle (AI 724) under the screen centre.
const look = 512;
const middle: f64 = 512.0 * std.math.sqrt2;
/// The ring's radius in AI units: 240 by 120 pixels from the centre, so the
/// tallest (the church, about 200 pixels) clears the top message line.
const ring = 480.0;

const Building = struct { name: []const u8, folder: []const u8 };
const buildings = [_]Building{
    .{ .name = "E_JettyBuilding_4", .folder = "Data/Buildings/europe/summer/e_jettybuilding_4" },
    .{ .name = "E_House02_2", .folder = "Data/Buildings/europe/summer/e_house02_2" },
    .{ .name = "E_House01_2", .folder = "Data/Buildings/europe/summer/e_house01_2" },
    .{ .name = "E_House09_1", .folder = "Data/Buildings/europe/summer/e_house09_1" },
};
const sizes = [_][2]u32{ .{ 1024, 768 }, .{ 1280, 1024 }, .{ 1920, 1080 } };

const min_iou = 0.9;
/// Rows the interface may cover: the message lines at the top, the panels at
/// the bottom (their height in pixels at every resolution).
const top_rows = 90;
const bottom_rows = 170;
const reach = 6;

/// Building i's place: quarter i of the ring (AI x, y, -x, -y), AI units.
fn place(i: usize) [2]f64 {
    const angle = @as(f64, @floatFromInt(i)) * std.math.pi / 2.0;
    return .{ middle + ring * @cos(angle), middle + ring * @sin(angle) };
}

/// Its zero cross's offset from the screen centre at step 0: AI x half a
/// pixel right and a quarter down, AI y half right and a quarter up.
fn offset(i: usize) [2]f64 {
    const at = place(i);
    const dx = @round(at[0]) - middle;
    const dy = @round(at[1]) - middle;
    return .{ (dx + dy) / 2, (dx - dy) / 4 };
}

test "the village ring: a quarter turn takes each place to the next" {
    for (0..4) |i| {
        const d = turn.turnOffset(offset(i), 1);
        const next = offset((i + 1) % 4);
        try testing.expect(@abs(d[0] - next[0]) < 1 and @abs(d[1] - next[1]) < 1);
    }
}

test "the village's houses turn and stand where the turn puts them at every resolution" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-village-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const turned = try std.fs.path.join(allocator, &.{ data, mod_buildings });
    try cwd.createDirPath(io, turned);
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    try sideAll(io, tool, turned, work);
    try writeMod(io, allocator, data);

    var sprites: [buildings.len][4]game_views.Sprite = undefined;
    for (buildings, 0..) |building, b| for (0..4) |k| {
        const base = std.fs.path.basename(building.folder);
        const folder = if (k == 0) building.folder else try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}", .{ turned, base, k });
        sprites[b][k] = try game_views.decompose(io, allocator, tool, folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{s}_{d}", .{ root, base, k }));
    };

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "size step building: IoU / colour at x,y (offset from the turned place); best IoU of another turn there\n");
    const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
    inline for (sizes) |size| {
        const name = std.fmt.comptimePrint("village-{d}x{d}", .{ size[0], size[1] });
        const frames = try turn.runGameSized(io, allocator, game, root, name, "village.xml", .{ look, look }, "ON", objects_only, 4, false, size);
        const centre = [2]f64{ @floatFromInt(size[0] / 2 - 1), @floatFromInt(size[1] / 2 - 1) };
        for (frames, 0..) |frame, k| for (buildings, 0..) |building, b| {
            const d = turn.turnOffset(offset(b), k);
            const at = [2]i32{ @intFromFloat(@round(centre[0] + d[0])), @intFromFloat(@round(centre[1] + d[1])) };
            const own = score(frame, size, sprites[b][k], at);
            var other: f64 = 0;
            for (0..4) |o| {
                if (o != k) other = @max(other, score(frame, size, sprites[b][o], at).iou);
            }
            try report.print(allocator, "{d}x{d} {d} {s}: {d:.3} / {d:.2} at {d},{d} ({d},{d}); {d:.3}\n", .{ size[0], size[1], k, building.name, own.iou, own.colour, at[0], at[1], own.dx, own.dy, other });
            if (own.iou < min_iou or other >= own.iou or own.dx != 0 or own.dy != 0) failed = true;
        };
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a house of the village shows the wrong side or stands off its turned place (IoU floor {d}, offset 0,0); the frames are in {s}\n", .{ report.items, min_iou, root });
        return error.DrawnWrong;
    }
}
