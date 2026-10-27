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
//! In the same 1024 x 768 shots the minimap frame's north mark (`S -> N` on
//! its upper left bar) must point where the map's north is: at step 1 on the
//! upper right bar mirrored, at step 2 on the left one turned half round, at
//! step 3 on the right one upside down, its band's pixels where that puts
//! them (mean RGB difference to step 0's), and at odd steps the left plate's
//! letters gone.
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
        // The north mark, in the window whose interface is not scaled.
        if (size[0] == 1024) {
            for (1..4) |k| {
                const mark = northMark(frames[0], frames[k], k);
                try report.print(allocator, "north mark, step {d}: {d:.1} where the turn puts it, {d:.1} as at step 0; dark pixels left on the left plate {d} of {d} at step 0\n", .{ k, mark.turned, mark.still, mark.dark, mark.dark_0 });
                if (mark.turned > max_mark or mark.still < 2 * mark.turned or (k % 2 == 1 and mark.dark * 4 > mark.dark_0)) failed = true;
            }
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
            // `.` turns one step on, `,` one back.
            const step = if (key[0] == '.') shot else (4 - shot) % 4;
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

/// The north mark's band on the frame's upper left bar (ui\missionpanels,
/// whose top left is the panel's, at (0, h - 155) in 1024 x 768): pixels
/// between (52, 26)-(52, 35) and (86, 9)-(86, 18), inside its edges.
const panel_top = 768 - 155;
const max_mark = 30.0;

fn inBand(x: i32, y: i32) bool {
    if (x <= 52 or x >= 85) return false;
    const top = 26.0 - @as(f64, @floatFromInt(x - 52)) / 2;
    const yf: f64 = @floatFromInt(y);
    return yf > top + 1 and yf < top + 8;
}

/// A band pixel at step k: on the upper right bar (mirrored about x = 135)
/// at odd steps, turned half round about the band's middle at steps 2 and 3,
/// so its arrow points where the map's north is (MiniMap's DrawNorthMark).
fn markPlace(p: [2]i32, k: usize) [2]i32 {
    var q = p;
    if (k >= 2) q = .{ 137 - q[0], 43 - q[1] };
    if (k % 2 == 1) q = .{ 269 - q[0], q[1] };
    return q;
}

fn panelPixel(frame: []const u8, p: [2]i32) [3]f64 {
    const i: usize = @intCast(((panel_top + p[1]) * 1024 + p[0]) * 4);
    return .{ @floatFromInt(frame[i]), @floatFromInt(frame[i + 1]), @floatFromInt(frame[i + 2]) };
}

/// Step k's frame against step 0's: the mean RGB difference of the band's
/// pixels where the turn puts them and where they stood, and how many of the
/// left band's pixels are dark (the letters) at step k and at step 0.
fn northMark(before: []const u8, after: []const u8, k: usize) struct { turned: f64, still: f64, dark: usize, dark_0: usize } {
    var turned: f64 = 0;
    var still: f64 = 0;
    var n: f64 = 0;
    var dark: usize = 0;
    var dark_0: usize = 0;
    var y: i32 = 0;
    while (y < 40) : (y += 1) {
        var x: i32 = 40;
        while (x < 100) : (x += 1) {
            if (!inBand(x, y)) continue;
            const was = panelPixel(before, .{ x, y });
            const now = panelPixel(after, markPlace(.{ x, y }, k));
            const same = panelPixel(after, .{ x, y });
            for (0..3) |c| {
                turned += @abs(was[c] - now[c]);
                still += @abs(was[c] - same[c]);
            }
            n += 3;
            if (was[0] + was[1] + was[2] < 300) dark_0 += 1;
            if (same[0] + same[1] + same[2] < 300) dark += 1;
        }
    }
    return .{ .turned = turned / n, .still = still / n, .dark = dark, .dark_0 = dark_0 };
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
