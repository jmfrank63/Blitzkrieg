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
//! A bars test (the hand test's round 4) lays Poland's warehouse E_WB_01 and
//! europe's other storages on the same ring, player 1's so that their HP bars
//! show, and finds each bar at all four steps in 1024 x 768: at a turned step
//! its middle must stand over the middle of the building's turned picture.
//!
//! A second test shoots the same map's ground alone (objects, units, fog and
//! the rest off) at all four steps in the three window sizes: a half turn is
//! the point reflection about the centre, so step 2 must show step 0
//! reflected with no offset (9 x 9 pixel blocks, the best of the offsets
//! within 3 pixels). The odd steps, whose tiles show their pictures drawn
//! upright for the screen (so a half turn between them does not reflect the
//! grain, and no offset stands out), are reported. Before S07 the ground stood
//! a pixel low at the half turn in 1024 x 768 and not in the other sizes.
//! In the same shots the minimap frame's north mark (`S -> N` on its upper
//! left bar) must point where the map's north is, every letter upright: at
//! step 1 `N <- S` on the upper right bar, at step 2 `N <- S` on the left one,
//! at step 3 `S -> N` on the right one. Each glyph's place must hold that
//! glyph of the texture (S, N or the arrow, mirrored only where the arrow
//! turns; on the right bar the 2003 glyph turned a quarter turn along it,
//! not laid with upright columns), the left bar must show the plain 2003
//! plate where the mark left it, and no pixel may turn yellow off the bars'
//! labels. With the option off the key
//! must leave the bars as they are.
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

/// The buildings whose HP bars are measured (the hand test's round 4: "the bar
/// over the warehouse in the village also should stay over the middle of the
/// warehouse"): Poland's warehouse E_WB_01 (9 x 6 cells, its zero cross 48
/// AI units along and 83 across from its footprint's middle) and europe's
/// other storages, on the village ring, each player 1's: a storage is the
/// building whose bar shows unselected.
const bar_buildings = [_]Building{
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01" },
    .{ .name = "E_WB_02", .folder = "Data/Buildings/europe/summer/e_wb/02" },
    .{ .name = "E_WT_01", .folder = "Data/Buildings/europe/summer/e_wt/01" },
    .{ .name = "E_WT_02", .folder = "Data/Buildings/europe/summer/e_wt/02" },
};
/// At a turned step a bar's middle stands within this many pixels of its
/// building's picture's middle (the picture's outline and its bound box in
/// the pack differ by a pixel or two); step 0 is the 2003 view, reported.
const max_bar_off = 3;

test "the buildings' bars stand over their middles at every step" {
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
    const bars_scratch = scratch ++ "-bars";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), bars_scratch });
    cwd.deleteTree(io, bars_scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    var children: [bar_buildings.len]std.process.Child = undefined;
    for (bar_buildings, &children) |building, *child| {
        const out = try std.fs.path.join(allocator, &.{ data, mod_buildings, std.fs.path.dirname(building.folder["Data/Buildings/europe/summer/".len..]) orelse "" });
        try cwd.createDirPath(io, out);
        // A working folder each: e_wb/01 and e_wt/01 both name theirs 01.work, and the
        // runs go at once.
        const own_work = try std.fs.path.join(allocator, &.{ work, building.folder["Data/Buildings/europe/summer/".len..] });
        child.* = try std.process.spawn(io, .{ .argv = &.{ tool, "side", building.folder, "1,2,3", out, own_work }, .stdin = .ignore, .stdout = .ignore, .stderr = .ignore });
    }
    for (bar_buildings, &children) |building, *child| {
        const term = try child.wait(io);
        if (!term.success()) {
            std.debug.print("bk-views side {s} ended {any}\n", .{ building.folder, term });
            return error.ToolFailed;
        }
    }
    try writeBarsMod(io, allocator, data);

    var sprites: [bar_buildings.len][4]game_views.Sprite = undefined;
    for (bar_buildings, 0..) |building, b| for (0..4) |k| {
        const variant = building.folder["Data/Buildings/europe/summer/".len..];
        const folder = if (k == 0) building.folder else try std.fmt.allocPrint(allocator, "{s}/{s}/{s}_r{d}", .{ data, mod_buildings, variant, k });
        sprites[b][k] = try game_views.decompose(io, allocator, tool, folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{d}_{d}", .{ root, b, k }));
    };

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "step building: bar middle - picture middle (pixels, across), bar at x,y\n");
    const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
    const size = [2]u32{ 1024, 768 };
    const frames = try turn.runGameSized(io, allocator, game, root, "bars-1024x768", "bars.xml", .{ look, look }, "ON", objects_only, 4, false, size);
    const centre = [2]f64{ @floatFromInt(size[0] / 2 - 1), @floatFromInt(size[1] / 2 - 1) };
    for (frames, 0..) |frame, k| for (bar_buildings, 0..) |building, b| {
        const d = turn.turnOffset(offset(b), k);
        const at = [2]i32{ @intFromFloat(@round(centre[0] + d[0])), @intFromFloat(@round(centre[1] + d[1])) };
        const box = spriteBox(sprites[b][k], at);
        const bar = findBar(frame, size, box) orelse {
            try report.print(allocator, "{d} {s}: no bar found above {d},{d}..{d},{d}\n", .{ k, building.name, box[0], box[1], box[2], box[3] });
            failed = true;
            continue;
        };
        const off = bar[0] - @as(f64, @floatFromInt(box[0] + box[2])) / 2;
        try report.print(allocator, "{d} {s}: {d:.1}, bar at {d:.1},{d}\n", .{ k, building.name, off, bar[0], bar[1] });
        if (k != 0 and @abs(off) > max_bar_off) failed = true;
    };
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a building's bar leaves its middle at a turned step (more than {d} pixels at a turned step); the frames are in {s}\n", .{ report.items, max_bar_off, root });
        return error.DrawnWrong;
    }
}

/// The screen box (x0, y0, x1, y1) of a sprite's opaque drawn pixels with its zero cross at `at`.
fn spriteBox(sprite: game_views.Sprite, at: [2]i32) [4]i32 {
    var box = [4]i32{ std.math.maxInt(i32), std.math.maxInt(i32), std.math.minInt(i32), std.math.minInt(i32) };
    for (sprite.image.pixels, 0..) |p, i| {
        if (p[3] < 128 or !drawn(&p)) continue;
        const x = at[0] + @as(i32, @intCast(i % sprite.image.width)) - sprite.anchor[0];
        const y = at[1] + @as(i32, @intCast(i / sprite.image.width)) - sprite.anchor[1];
        box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x + 1), @max(box[3], y + 1) };
    }
    return box;
}

/// The middle (x) and row of the longest run of bar green (an HP bar's fill
/// over the black of a shot without terrain) of 20 pixels or more in the
/// rows from 60 above the box's top to 30 below it, across the box and 60
/// pixels either side.
fn findBar(frame: []const u8, size: [2]u32, box: [4]i32) ?[2]f64 {
    var best: ?[2]f64 = null;
    var longest: i32 = 19;
    var y = @max(box[1] - 60, top_rows);
    while (y < @min(box[1] + 30, @as(i32,@intCast(size[1])) - bottom_rows)) : (y += 1) {
        var run: i32 = 0;
        var x = @max(box[0] - 60, 0);
        const end = @min(box[2] + 60, @as(i32, @intCast(size[0])));
        while (x <= end) : (x += 1) {
            const green = x < end and blk: {
                const p = frame[@intCast((y * @as(i32, @intCast(size[0])) + x) * 4)..][0..3];
                break :blk p[1] >= 50 and @as(u32, p[1]) > 2 * @as(u32, p[0]) and @as(u32, p[1]) > 2 * @as(u32, p[2]);
            };
            if (green) {
                run += 1;
                continue;
            }
            if (run > longest) {
                longest = run;
                best = .{ @as(f64, @floatFromInt(x - run)) + @as(f64, @floatFromInt(run)) / 2, @floatFromInt(y) };
            }
            run = 0;
        }
    }
    return best;
}

/// mod.xml and maps/bars.xml: river3d with the bar buildings on the ring, each player 1's.
fn writeBarsMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn bars test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    var items: std.ArrayList(u8) = .empty;
    try items.appendSlice(allocator, "<Objects>");
    for (bar_buildings, 0..) |building, i| {
        const at = place(i);
        try items.print(allocator, "<item Direction=\"0\" Player=\"1\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ building.name, at[0], at[1], i + 1 });
    }
    try items.appendSlice(allocator, "</Objects>");
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "bars.xml" }),
        .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
    });
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
    const texture = try cwd.readFileAlloc(io, "Data/UI/missionpanels_h.dds", allocator, .limited(1 << 20));
    try testing.expectEqual(@as(usize, 128 + 256 * 256 * 4), texture.len);

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
        if (!try northMark(allocator, &report, texture, size, &frames)) failed = true;
    }
    // Remastered off the key turns nothing: the frame's bars stay the 2003 ones.
    const off = try turn.runGameSized(io, allocator, game, root, "ground-off-1024x768", "village.xml", .{ look, look }, "OFF", ground_only, 4, false, .{ 1024, 768 });
    const off_panel = findPanel(texture, .{ 1024, 768 }, off[0]);
    for (1..4) |k| {
        const changed = barsChanged(off_panel, off[0], off[k]);
        try report.print(allocator, "off 1024x768 shot {d}: pixels of the bars changed by the key {d}\n", .{ k, changed });
        if (changed != 0) failed = true;
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the ground stands off where a half turn puts it, or the north mark is drawn wrong; the frames are in {s}\n", .{ report.items, root });
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

/// Poland's village (Data/Maps/German/Poland/Poland.bzm, the map of the
/// maintainer's hand test): its buildings around the church and the barn, AI
/// units from the screen's middle in the hand test as the map holds them
/// (bk-map to-xml), and the folder each draws. The row of three houses 470 to
/// 560 units further north is left out: with them the village does not fit
/// on road3d with room for its footprints.
const PolandBuilding = struct { name: []const u8, dir: []const u8, variant: []const u8, at: [2]f64 };
const poland = [_]PolandBuilding{
    .{ .name = "E_Stella_02", .dir = "e_stella/02", .variant = "02", .at = .{ 424, 18 } },
    .{ .name = "E_House09_1", .dir = "e_house09_1", .variant = "e_house09_1", .at = .{ 74, 27 } },
    .{ .name = "E_JettyBuilding_4", .dir = "e_jettybuilding_4", .variant = "e_jettybuilding_4", .at = .{ 286, 16 } },
    .{ .name = "E_House03_4", .dir = "e_house03_4", .variant = "e_house03_4", .at = .{ 21, 428 } },
    .{ .name = "E_House02_1", .dir = "e_house02_1", .variant = "e_house02_1", .at = .{ -112, 17 } },
    .{ .name = "E_House02_2", .dir = "e_house02_2", .variant = "e_house02_2", .at = .{ -240, 17 } },
    .{ .name = "E_WB_01", .dir = "e_wb/01", .variant = "01", .at = .{ 462, 419 } },
};
/// The village's middle, so the map (road3d, 1448 AI units across) holds it
/// around its own middle.
const poland_middle = [2]f64{ 111, 222 };
/// A map place lands between pixels, which the prediction and the Game round
/// apart by a pixel at most (also at step 0, the 2003 drawing): a place within
/// a pixel of the turned one is where the turn puts it. A picture whose best
/// place still differs from the frame by more than `hidden` (mean RGB) is
/// behind a neighbour (the wayside cross behind the church at the half turn:
/// 57.3); the ones found differ by 0 to 19.4.
const hidden = 40.0;

fn polandOffset(b: PolandBuilding) [2]f64 {
    const dx = @round(middle + b.at[0] - poland_middle[0]) - middle;
    const dy = @round(middle + b.at[1] - poland_middle[1]) - middle;
    return .{ (dx + dy) / 2, (dx - dy) / 4 };
}

test "Poland's village: every building's turned picture stands at its map place turned, both ways, at two sizes" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-poland-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const poland_scratch = scratch ++ "-poland";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), poland_scratch });
    cwd.deleteTree(io, poland_scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });

    // Each variant's sides, all at once, into its folder's parent in the mod.
    var children: [poland.len]?std.process.Child = undefined;
    for (poland, &children, 0..) |b, *child, i| {
        child.* = null;
        for (poland[0..i]) |o| {
            if (std.mem.eql(u8, o.dir, b.dir)) break;
        } else {
            const out = try std.fs.path.join(allocator, &.{ data, mod_buildings, std.fs.path.dirname(b.dir) orelse "" });
            try cwd.createDirPath(io, out);
            child.* = try std.process.spawn(io, .{
                .argv = &.{ tool, "side", try std.fmt.allocPrint(allocator, "Data/Buildings/europe/summer/{s}", .{b.dir}), "1,2,3", out, work },
                .stdin = .ignore,
                .stdout = .ignore,
                .stderr = .ignore,
            });
        }
    }
    for (&children, poland) |*child, b| if (child.*) |*c| {
        const term = try c.wait(io);
        if (!term.success()) {
            std.debug.print("bk-views side {s} ended {any}\n", .{ b.dir, term });
            return error.ToolFailed;
        }
    };
    try writeMapOf(io, allocator, data);

    var sprites: [poland.len][4]game_views.Sprite = undefined;
    for (poland, 0..) |b, i| for (0..4) |k| {
        const folder = if (k == 0) try std.fmt.allocPrint(allocator, "Data/Buildings/europe/summer/{s}", .{b.dir}) else try std.fmt.allocPrint(allocator, "{s}/{s}/{s}_r{d}", .{ data, mod_buildings, b.dir, k });
        sprites[i][k] = try game_views.decompose(io, allocator, tool, folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{d}_{d}", .{ root, i, k }));
    };

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "key size shot step building: mean colour difference at the turned place (offset of the best place within 8 pixels)\n");
    const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
    const runs = [_]struct { []const u8, [2]u32 }{ .{ ".", .{ 1024, 768 } }, .{ "COMMA", .{ 1024, 768 } }, .{ ".", .{ 1920, 1080 } }, .{ "COMMA", .{ 1920, 1080 } } };
    for (runs) |run| {
        const key, const size = run;
        const name = try std.fmt.allocPrint(allocator, "poland-{s}-{d}x{d}", .{ if (key[0] == '.') "period" else "comma", size[0], size[1] });
        const frames = try turn.runGameTurning(io, allocator, game, root, name, "poland.xml", .{ look, look }, "ON", objects_only, 4, false, size, key);
        const centre = [2]f64{ @floatFromInt(size[0] / 2 - 1), @floatFromInt(size[1] / 2 - 1) };
        for (frames, 0..) |frame, shot| {
            // `,` turns one step on, `.` one back (the keys swapped on 2026-10-28).
            const step = if (key[0] == '.') (4 - shot) % 4 else shot;
            for (poland, 0..) |b, i| {
                const d = turn.turnOffset(polandOffset(b), step);
                const at = [2]i32{ @intFromFloat(@round(centre[0] + d[0])), @intFromFloat(@round(centre[1] + d[1])) };
                const found = placeOf(frame, size, sprites[i][step], at, hidden) orelse {
                    try report.print(allocator, "{s} {d}x{d} {d} {d} {s}: off the clear screen or hidden behind a neighbour\n", .{ key, size[0], size[1], shot, step, b.name });
                    continue;
                };
                try report.print(allocator, "{s} {d}x{d} {d} {d} {s}: {d:.1} ({d},{d})\n", .{ key, size[0], size[1], shot, step, b.name, found.colour, found.dx, found.dy });
                if (@abs(found.dx) > 1 or @abs(found.dy) > 1) failed = true;
            }
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a building of Poland's village stands off its turned place; the frames are in {s}\n", .{ report.items, root });
        return error.DrawnWrong;
    }
}

/// mod.xml and maps/poland.xml: road3d (dry and flat) with Poland's village
/// around its middle, owned by player 0 as in Poland (E_WB_01, a storage,
/// traps the debug Game at load when it belongs to nobody).
fn writeMapOf(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn Poland test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    var items: std.ArrayList(u8) = .empty;
    try items.appendSlice(allocator, "<Objects>");
    for (poland, 1..) |b, link| {
        try items.print(allocator, "<item Direction=\"0\" Player=\"0\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ b.name, middle + b.at[0] - poland_middle[0], middle + b.at[1] - poland_middle[1], link });
    }
    try items.appendSlice(allocator, "</Objects>");
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "poland.xml" }),
        .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
    });
}

/// Where a building's picture is in a crowded frame: the place within 8 pixels
/// of `at` where its opaque pixels differ least from the frame (a neighbour in
/// front hides some of them alike at every place); null when the picture is
/// not wholly between the message lines and the panels, or nowhere differs
/// from the frame by less than `ceiling`.
fn placeOf(frame: []const u8, size: [2]u32, sprite: game_views.Sprite, at: [2]i32, ceiling: f64) ?struct { colour: f64, dx: i32, dy: i32 } {
    const w: i32 = @intCast(size[0]);
    const h: i32 = @intCast(size[1]);
    var lo = [2]i32{ std.math.maxInt(i32), std.math.maxInt(i32) };
    var hi = [2]i32{ 0, 0 };
    for (sprite.image.pixels, 0..) |p, i| if (p[3] == 255) {
        const x: i32 = @intCast(i % sprite.image.width);
        const y: i32 = @intCast(i / sprite.image.width);
        lo = .{ @min(lo[0], x), @min(lo[1], y) };
        hi = .{ @max(hi[0], x), @max(hi[1], y) };
    };
    const reach8 = 8;
    if (at[0] + lo[0] - sprite.anchor[0] - reach8 < 0 or at[0] + hi[0] - sprite.anchor[0] + reach8 >= w) return null;
    if (at[1] + lo[1] - sprite.anchor[1] - reach8 < top_rows or at[1] + hi[1] - sprite.anchor[1] + reach8 >= h - bottom_rows) return null;
    var best: f64 = std.math.floatMax(f64);
    var best_at = [2]i32{ 0, 0 };
    var oy: i32 = -reach8;
    while (oy <= reach8) : (oy += 1) {
        var ox: i32 = -reach8;
        while (ox <= reach8) : (ox += 1) {
            var sum: f64 = 0;
            var n: f64 = 0;
            var y = lo[1];
            while (y <= hi[1]) : (y += 2) {
                var x = lo[0];
                while (x <= hi[0]) : (x += 2) {
                    const p = sprite.image.pixels[@intCast(y * @as(i32, @intCast(sprite.image.width)) + x)];
                    if (p[3] != 255) continue;
                    const sx = at[0] + ox + x - sprite.anchor[0];
                    const sy = at[1] + oy + y - sprite.anchor[1];
                    const f = frame[@intCast((sy * w + sx) * 4)..][0..3];
                    for (0..3) |c| sum += @abs(@as(f64, @floatFromInt(f[c])) - @as(f64, @floatFromInt(p[c])));
                    n += 3;
                }
            }
            const mean = if (n == 0) 255 else sum / n;
            if (mean < best or (mean == best and ox == 0 and oy == 0)) {
                best = mean;
                best_at = .{ ox, oy };
            }
        }
    }
    if (best > ceiling) return null;
    return .{ .colour = best, .dx = best_at[0], .dy = best_at[1] };
}

/// The north mark (UIMiniMap.cpp DrawNorthMark): its glyphs in the band of
/// ui\missionpanels along the frame's upper left bar, under the line
/// y = 52 - x / 2, rows 2 to 8; on the upper right bar (the left one mirrored
/// about x = 135, a texel lower) the band's line is y = x / 2 - 82.
const Glyph = struct { name: []const u8, from: f64, width: f64 };
const glyph_s: Glyph = .{ .name = "S", .from = 53, .width = 8 };
const glyph_arrow: Glyph = .{ .name = "arrow", .from = 61, .width = 15 };
const glyph_n: Glyph = .{ .name = "N", .from = 76, .width = 8 };
const Laid = struct { glyph: Glyph, mirrored: bool };
const reads_on = [3]Laid{ .{ .glyph = glyph_s, .mirrored = false }, .{ .glyph = glyph_arrow, .mirrored = false }, .{ .glyph = glyph_n, .mirrored = false } };
const reads_back = [3]Laid{ .{ .glyph = glyph_n, .mirrored = false }, .{ .glyph = glyph_arrow, .mirrored = true }, .{ .glyph = glyph_s, .mirrored = false } };
/// The mean RGB difference a glyph may keep from step 0's where it is laid
/// (the interface is resampled in the larger windows).
const max_glyph = 30.0;

fn bandTop(u: f64, right: bool) f64 {
    return if (right) u / 2 - 82 else 52 - u / 2;
}

/// The panel (the minimap's parent) in a shot: the texture's top left on the
/// screen and the interface's scale, min(w / 1024, h / 768).
const Panel = struct {
    texture: []const u8,
    size: [2]u32,
    origin: [2]i32,
    scale: f64,

    fn texel(p: Panel, u: f64, v: f64) [3]f64 {
        const i: usize = @intCast(128 + (@as(i32, @intFromFloat(@floor(v))) * 256 + @as(i32, @intFromFloat(@floor(u)))) * 4);
        return .{ @floatFromInt(p.texture[i + 2]), @floatFromInt(p.texture[i + 1]), @floatFromInt(p.texture[i]) };
    }
    /// The screen pixel over texture point (u, v), null off the screen.
    fn screen(p: Panel, frame: []const u8, u: f64, v: f64) ?[3]f64 {
        const x = p.origin[0] + @as(i32, @intFromFloat(@floor(u * p.scale)));
        const y = p.origin[1] + @as(i32, @intFromFloat(@floor(v * p.scale)));
        if (x < 0 or y < 0 or x >= @as(i32, @intCast(p.size[0])) or y >= @as(i32, @intCast(p.size[1]))) return null;
        const i: usize = @intCast((y * @as(i32, @intCast(p.size[0])) + x) * 4);
        return .{ @floatFromInt(frame[i]), @floatFromInt(frame[i + 1]), @floatFromInt(frame[i + 2]) };
    }
};

/// Where the panel stands in a step-0 shot: the place within a few pixels of
/// the bottom left corner where the upper left bar's texels differ least.
fn findPanel(texture: []const u8, size: [2]u32, frame: []const u8) Panel {
    const scale = @min(@as(f64, @floatFromInt(size[0])) / 1024, @as(f64, @floatFromInt(size[1])) / 768);
    const base: i32 = @intFromFloat(@round(@as(f64, @floatFromInt(size[1])) - 155 * scale));
    var best: Panel = .{ .texture = texture, .size = size, .origin = .{ 0, base }, .scale = scale };
    var least: f64 = std.math.floatMax(f64);
    var oy = base - 8;
    while (oy <= base + 8) : (oy += 1) {
        var ox: i32 = -4;
        while (ox <= 4) : (ox += 1) {
            const p: Panel = .{ .texture = texture, .size = size, .origin = .{ ox, oy }, .scale = scale };
            var sum: f64 = 0;
            var u: f64 = 38;
            while (u < 96) : (u += 1) {
                var v: f64 = 5;
                while (v < 45) : (v += 1) {
                    const got = p.screen(frame, u + 0.5, v + 0.5) orelse [3]f64{ 255, 255, 255 };
                    const want = p.texel(u, v);
                    for (0..3) |c| sum += @abs(got[c] - want[c]);
                }
            }
            if (sum < least) {
                least = sum;
                best = p;
            }
        }
    }
    return best;
}

/// The mean RGB difference between a glyph as the 2003 frame shows it at step
/// 0 (columns reversed when mirrored) and the shot where it would lie from
/// column `at` of a bar (the right bar's glyphs start at 185, under 53 of the
/// widened plate laid there from 178). On the right bar a glyph is the left
/// one turned a quarter turn on the ground when `turned` (row r of column c
/// 2 r to the left on the band's line), else laid with upright columns.
fn glyphDifference(p: Panel, reference: []const u8, frame: []const u8, at: f64, right: bool, turned: bool, laid: Laid) f64 {
    var sum: f64 = 0;
    var n: f64 = 0;
    var x: f64 = 0;
    while (x < laid.glyph.width) : (x += 1) {
        const du = at + x + 0.5;
        const su = if (laid.mirrored) laid.glyph.from + laid.glyph.width - x - 0.5 else laid.glyph.from + x + 0.5;
        var r: f64 = 2;
        while (r < 8) : (r += 1) {
            const got = (if (right and turned) p.screen(frame, du - 2 * (r + 0.5), bandTop(du, true)) else p.screen(frame, du, bandTop(du, right) + r + 0.5)) orelse return 255;
            const want = p.screen(reference, su, bandTop(su, false) + r + 0.5) orelse return 255;
            for (0..3) |c| sum += @abs(got[c] - want[c]);
            n += 3;
        }
    }
    return sum / n;
}

/// The dark (letter) pixels where the glyphs stand on a bar.
fn darkOnBar(p: Panel, frame: []const u8, right: bool) usize {
    var dark: usize = 0;
    var u: f64 = if (right) 185.5 else 53.5;
    const end = u + 31;
    while (u < end) : (u += 1) {
        var r: f64 = 2;
        while (r < 8) : (r += 1) {
            const got = p.screen(frame, u, bandTop(u, right) + r + 0.5) orelse continue;
            if (got[0] + got[1] + got[2] < 300) dark += 1;
        }
    }
    return dark;
}

/// The mean RGB difference a left bar without the mark may keep from the right
/// bar's plain plate at step 0, mirrored.
const max_plain = 20.0;

/// The left bar's widened stretch (columns 46 to 92, rows 0 to 9 under its
/// line) in `frame` against the right bar's plain plate there at step 0,
/// mirrored about x = 135 a texel lower: the yellow pixels of each, and the
/// mean RGB difference over the plain plate's rows (4 to 9; above them the
/// frame is transparent and shows the turned view), the least of the
/// reference moved by up to a pixel (a scaled panel's mirror lands between
/// pixels).
fn plainLeft(p: Panel, reference: []const u8, frame: []const u8) struct { difference: f64, yellow: usize, yellow_plain: usize } {
    var least: f64 = 255;
    var yellow_now: usize = 0;
    var yellow_plain: usize = 0;
    var oy: f64 = -1;
    while (oy <= 1) : (oy += 1) {
        var ox: f64 = -1;
        while (ox <= 1) : (ox += 1) {
            var sum: f64 = 0;
            var n: f64 = 0;
            var u: f64 = 46.5;
            while (u < 92) : (u += 1) {
                var r: f64 = 0.5;
                while (r < 9) : (r += 1) {
                    const got = p.screen(frame, u, bandTop(u, false) + r) orelse continue;
                    const want = p.screen(reference, 270 - u + ox / p.scale, bandTop(270 - u, true) + r + oy / p.scale) orelse continue;
                    if (r > 4) {
                        for (0..3) |c| sum += @abs(got[c] - want[c]);
                        n += 3;
                    }
                    if (ox == 0 and oy == 0) {
                        if (yellow(got)) yellow_now += 1;
                        if (yellow(want)) yellow_plain += 1;
                    }
                }
            }
            if (n > 0) least = @min(least, sum / n);
        }
    }
    return .{ .difference = least, .yellow = yellow_now, .yellow_plain = yellow_plain };
}

/// Whether texture point (u, v) is on a bar's label (with a margin) or in the
/// minimap's diamond (its window at (6, 3), 256 x 128, where the turned map is).
fn onLabelOrMap(u: f64, v: f64) bool {
    if (@abs(u - 134) / 128 + @abs(v - 67) / 64 < 1.08) return true;
    if (u >= 42 and u < 96 and v >= bandTop(u, false) - 3 and v < bandTop(u, false) + 12) return true;
    return u >= 156 and u < 228 and v >= bandTop(u, true) - 3 and v < bandTop(u, true) + 12;
}

fn yellow(c: [3]f64) bool {
    return c[0] > 200 and c[1] > 140 and c[2] < 100;
}

/// Pixels on the panel or the 40 texels above it, off the labels and the map,
/// that are yellow in `after` and were not in `before`.
fn yellowAdded(p: Panel, before: []const u8, after: []const u8) usize {
    var added: usize = 0;
    const top: i32 = p.origin[1] - @as(i32, @intFromFloat(40 * p.scale));
    const bottom: i32 = p.origin[1] + @as(i32, @intFromFloat(70 * p.scale));
    const right: i32 = p.origin[0] + @as(i32, @intFromFloat(264 * p.scale));
    var y = @max(top, 0);
    while (y < bottom) : (y += 1) {
        var x = @max(p.origin[0], 0);
        while (x < right) : (x += 1) {
            const u = (@as(f64, @floatFromInt(x - p.origin[0])) + 0.5) / p.scale;
            const v = (@as(f64, @floatFromInt(y - p.origin[1])) + 0.5) / p.scale;
            if (onLabelOrMap(u, v)) continue;
            const i: usize = @intCast((y * @as(i32, @intCast(p.size[0])) + x) * 4);
            const was = [3]f64{ @floatFromInt(before[i]), @floatFromInt(before[i + 1]), @floatFromInt(before[i + 2]) };
            const now = [3]f64{ @floatFromInt(after[i]), @floatFromInt(after[i + 1]), @floatFromInt(after[i + 2]) };
            if (yellow(now) and !yellow(was)) added += 1;
        }
    }
    return added;
}

/// Pixels of both bars' labels that differ between two shots.
fn barsChanged(p: Panel, before: []const u8, after: []const u8) usize {
    var changed: usize = 0;
    var y: i32 = p.origin[1];
    while (y < p.origin[1] + @as(i32, @intFromFloat(50 * p.scale))) : (y += 1) {
        var x: i32 = p.origin[0];
        while (x < p.origin[0] + @as(i32, @intFromFloat(264 * p.scale))) : (x += 1) {
            const u = (@as(f64, @floatFromInt(x - p.origin[0])) + 0.5) / p.scale;
            const v = (@as(f64, @floatFromInt(y - p.origin[1])) + 0.5) / p.scale;
            if (@abs(u - 134) / 128 + @abs(v - 67) / 64 < 1.08 or !onLabelOrMap(u, v)) continue;
            const i: usize = @intCast((y * @as(i32, @intCast(p.size[0])) + x) * 4);
            if (!std.mem.eql(u8, before[i..][0..3], after[i..][0..3])) changed += 1;
        }
    }
    return changed;
}

/// The north mark at each step of a run: `S -> N` on the left bar at step 0
/// (the 2003 frame), `N <- S` on the right one at step 1 and on the left one
/// at step 2, `S -> N` on the right one at step 3: each place's best glyph of
/// the texture (S, N or the arrow, as drawn or mirrored) must be the one laid
/// there. No
/// pixel turns yellow off the labels and the map (the panel and above it).
/// Where the mark left it, the left bar shows the plain plate (plainLeft);
/// a letter on the right bar matches the 2003 one turned along that bar.
fn northMark(allocator: std.mem.Allocator, report: *std.ArrayList(u8), texture: []const u8, size: [2]u32, frames: []const []u8) !bool {
    const p = findPanel(texture, size, frames[0]);
    try report.print(allocator, "north mark {d}x{d}: panel at {d},{d}, scale {d:.3}; dark pixels on the left plate at step 0 {d}\n", .{ size[0], size[1], p.origin[0], p.origin[1], p.scale, darkOnBar(p, frames[0], false) });
    var ok = true;
    for (frames, 0..) |frame, k| {
        const right = k % 2 == 1;
        const order = if (k == 0 or k == 3) reads_on else reads_back;
        var at: f64 = if (right) 185 else 53;
        try report.print(allocator, "  step {d} on the {s} bar:", .{ k, if (right) "right" else "left" });
        for (order) |laid| {
            const glyphs = [_]Glyph{ glyph_s, glyph_arrow, glyph_n };
            var best: ?Laid = null;
            var least: f64 = std.math.floatMax(f64);
            for (glyphs) |g| for ([_]bool{ false, true }) |m| {
                if (g.width != laid.glyph.width) continue;
                const d = glyphDifference(p, frames[0], frame, at, right, true, .{ .glyph = g, .mirrored = m });
                if (d < least) {
                    least = d;
                    best = .{ .glyph = g, .mirrored = m };
                }
            };
            const own = glyphDifference(p, frames[0], frame, at, right, true, laid);
            try report.print(allocator, " {s}{s} {d:.1} (best {s}{s})", .{ laid.glyph.name, if (laid.mirrored) " mirrored" else "", own, best.?.glyph.name, if (best.?.mirrored) " mirrored" else "" });
            if (own > max_glyph or best.?.glyph.from != laid.glyph.from or best.?.mirrored != laid.mirrored) ok = false;
            // A letter on the right bar must match the 2003 one turned along the
            // bar better than the same letter laid with upright columns.
            if (right and laid.glyph.width == glyph_s.width) {
                const upright = glyphDifference(p, frames[0], frame, at, right, false, laid);
                try report.print(allocator, " [upright {d:.1}]", .{upright});
                if (upright <= own) ok = false;
            }
            at += laid.glyph.width;
        }
        const dark = darkOnBar(p, frame, false);
        const added = yellowAdded(p, frames[0], frame);
        const plain = plainLeft(p, frames[0], frame);
        try report.print(allocator, "; dark pixels on the left plate {d}; yellow added off the labels {d}; left label against the plain plate {d:.1}, yellow {d} (plain {d})\n", .{ dark, added, plain.difference, plain.yellow, plain.yellow_plain });
        if (added != 0) ok = false;
        // Where the mark left it, the left bar is the plain 2003 plate.
        if (right and (plain.difference > max_plain or plain.yellow * 10 > plain.yellow_plain * 11 or plain.yellow * 10 < plain.yellow_plain * 9)) ok = false;
    }
    return ok;
}
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
