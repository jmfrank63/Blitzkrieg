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
//! A second test shoots the same map's ground alone (objects, units, fog and
//! the rest off) at all four steps in the three window sizes: a half turn is
//! the point reflection about the centre, so step 2 must show step 0
//! reflected with no offset (9 x 9 pixel blocks, the best of the offsets
//! within 3 pixels). The odd steps, whose tiles show their pictures drawn
//! upright for the screen (so a half turn between them does not reflect the
//! grain, and no offset stands out), are reported. Before S07 the ground stood
//! a pixel low at the half turn in 1024 x 768 and not in the other sizes.
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

test "the ground stands where the turn puts it at every resolution" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-ground-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const ground_scratch = scratch ++ "-ground";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), ground_scratch });
    cwd.deleteTree(io, ground_scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" }));

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "size from -> to: best offset (difference), difference at 0,0\n");
    const ground_only = "shadows,objects,units,effects,noise,warfog,border,haze,grid";
    inline for (sizes) |size| {
        const name = std.fmt.comptimePrint("ground-{d}x{d}", .{ size[0], size[1] });
        const frames = try turn.runGameSized(io, allocator, game, root, name, "village.xml", .{ look, look }, "ON", ground_only, 4, false, size);
        const pairs = [_][2]usize{ .{ 0, 2 }, .{ 1, 3 }, .{ 0, 1 }, .{ 0, 3 } };
        for (pairs) |pair| {
            const from, const to = pair;
            const found = try groundOffset(allocator, size, frames[from], frames[to], to - from);
            try report.print(allocator, "{d}x{d} {d} -> {d}: {d},{d} ({d:.2}), {d:.2}\n", .{ size[0], size[1], from, to, found.dx, found.dy, found.best, found.still });
            if (from == 0 and to == 2 and (found.dx != 0 or found.dy != 0)) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the ground stands off where a half turn puts it; the frames are in {s}\n", .{ report.items, root });
        return error.DrawnWrong;
    }
}

/// Sums over the rectangle above and left of each pixel (RGB together), so a
/// block's mean takes four look-ups.
fn integral(allocator: std.mem.Allocator, size: [2]u32, frame: []const u8) ![]f64 {
    const stride = size[0] + 1;
    const sums = try allocator.alloc(f64, stride * (size[1] + 1));
    @memset(sums, 0);
    for (0..size[1]) |y| {
        var row: f64 = 0;
        for (0..size[0]) |x| {
            const i = (y * size[0] + x) * 4;
            row += @as(f64, @floatFromInt(frame[i])) + @as(f64, @floatFromInt(frame[i + 1])) + @as(f64, @floatFromInt(frame[i + 2]));
            sums[(y + 1) * stride + x + 1] = sums[y * stride + x + 1] + row;
        }
    }
    return sums;
}

/// The mean of the 9 x 9 block around (x, y), null near the edges and under the interface.
fn blockMean(sums: []const f64, size: [2]u32, x: i64, y: i64) ?f64 {
    if (x < 4 or y < top_rows + 4 or x + 5 > size[0] or y + 5 > size[1] - bottom_rows) return null;
    const stride: i64 = size[0] + 1;
    const at = struct {
        fn f(s: []const f64, i: i64) f64 {
            return s[@intCast(i)];
        }
    }.f;
    return (at(sums, (y + 5) * stride + x + 5) - at(sums, (y - 4) * stride + x + 5) - at(sums, (y + 5) * stride + x - 4) + at(sums, (y - 4) * stride + x - 4)) / 243;
}

const GroundFit = struct { dx: i32, dy: i32, best: f64, still: f64 };

/// The offset within 3 pixels that best lays `after` on `before` turned k
/// times about the centre (w / 2 - 1, h / 2 - 1), the mean block difference
/// there and at no offset.
fn groundOffset(allocator: std.mem.Allocator, size: [2]u32, before: []const u8, after: []const u8, k: usize) !GroundFit {
    const a = try integral(allocator, size, before);
    const b = try integral(allocator, size, after);
    const cx: i64 = size[0] / 2 - 1;
    const cy: i64 = size[1] / 2 - 1;
    var found: GroundFit = .{ .dx = 0, .dy = 0, .best = std.math.floatMax(f64), .still = 0 };
    var oy: i32 = -3;
    while (oy <= 3) : (oy += 1) {
        var ox: i32 = -3;
        while (ox <= 3) : (ox += 1) {
            var sum: f64 = 0;
            var n: f64 = 0;
            var y: i64 = -90;
            while (y <= 90) : (y += 6) {
                var x: i64 = -180;
                while (x <= 180) : (x += 6) {
                    const u = turn.unturn(.{ @floatFromInt(x), @floatFromInt(y) }, k);
                    const want = blockMean(a, size, cx + @as(i64, @intFromFloat(@round(u[0]))), cy + @as(i64, @intFromFloat(@round(u[1])))) orelse continue;
                    const got = blockMean(b, size, cx + x + ox, cy + y + oy) orelse continue;
                    sum += @abs(want - got);
                    n += 1;
                }
            }
            const difference = if (n == 0) std.math.floatMax(f64) else sum / n;
            if (ox == 0 and oy == 0) found.still = difference;
            if (difference < found.best) {
                found.best = difference;
                found.dx = ox;
                found.dy = oy;
            }
        }
    }
    // A tie keeps the place the turn puts it.
    if (found.still <= found.best) {
        found.dx = 0;
        found.dy = 0;
        found.best = found.still;
    }
    return found;
}

/// `bk-views side` for every building at once (each is a minute or so alone).
fn sideAll(io: std.Io, tool: []const u8, out: []const u8, work: []const u8) !void {
    var children: [buildings.len]std.process.Child = undefined;
    for (buildings, &children) |building, *child| {
        child.* = try std.process.spawn(io, .{
            .argv = &.{ tool, "side", building.folder, "1,2,3", out, work },
            .stdin = .ignore,
            .stdout = .ignore,
            .stderr = .ignore,
        });
    }
    var failed = false;
    for (buildings, &children) |building, *child| {
        const term = try child.wait(io);
        if (!term.success()) {
            std.debug.print("bk-views side {s} ended {any}\n", .{ building.folder, term });
            failed = true;
        }
    }
    if (failed) return error.ToolFailed;
}

/// mod.xml and maps/village.xml: river3d with the buildings on the ring.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn village test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    var items: std.ArrayList(u8) = .empty;
    try items.appendSlice(allocator, "<Objects>");
    for (buildings, 0..) |building, i| {
        const at = place(i);
        try items.print(allocator, "<item Direction=\"0\" Player=\"-1\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ building.name, at[0], at[1], i + 1 });
    }
    try items.appendSlice(allocator, "</Objects>");
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "village.xml" }),
        .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
    });
}

fn drawn(p: []const u8) bool {
    return @max(p[0], p[1], p[2]) > 24;
}

/// building-views-game-reads-it's score in a frame of `size`, the best place
/// within `reach` pixels, rows under the interface left out.
fn score(frame: []const u8, size: [2]u32, sprite: game_views.Sprite, screen: [2]i32) game_views.Score {
    const w: i64 = size[0];
    const h: i64 = size[1];
    var box = [4]i64{ sprite.image.width, sprite.image.height, 0, 0 };
    for (sprite.image.pixels, 0..) |p, i| {
        if (p[3] < 128 or !drawn(&p)) continue;
        const x: i64 = @intCast(i % sprite.image.width);
        const y: i64 = @intCast(i / sprite.image.width);
        box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x + 1), @max(box[3], y + 1) };
    }
    var best: game_views.Score = .{ .iou = -1, .colour = 255, .dx = 0, .dy = 0 };
    var oy: i32 = -reach;
    while (oy <= reach) : (oy += 1) {
        var ox: i32 = -reach;
        while (ox <= reach) : (ox += 1) {
            var both: f64 = 0;
            var either: f64 = 0;
            var colour: f64 = 0;
            var y = box[1] - 16;
            while (y < box[3] + 16) : (y += 1) {
                var x = box[0] - 16;
                while (x < box[2] + 16) : (x += 1) {
                    const sx = x + screen[0] + ox - sprite.anchor[0];
                    const sy = y + screen[1] + oy - sprite.anchor[1];
                    if (sx < 0 or sx >= w or sy < top_rows or sy >= h - bottom_rows) continue;
                    const pixel: ?[4]u8 = if (x < 0 or y < 0 or x >= sprite.image.width or y >= sprite.image.height) null else sprite.image.pixels[@intCast(y * sprite.image.width + x)];
                    const in_sprite = if (pixel) |p| p[3] >= 128 and drawn(&p) else false;
                    const f = frame[@intCast((sy * w + sx) * 4)..][0..3];
                    const on_screen = drawn(f);
                    if (in_sprite and on_screen) {
                        both += 1;
                        for (0..3) |c| colour += @abs(@as(f64, @floatFromInt(f[c])) - @as(f64, @floatFromInt(pixel.?[c])));
                    }
                    if (in_sprite or on_screen) either += 1;
                }
            }
            const iou = if (either == 0) 0 else both / either;
            if (iou > best.iou) best = .{ .iou = iou, .colour = if (both == 0) 255 else colour / (3 * both), .dx = ox, .dy = oy };
        }
    }
    return best;
}
