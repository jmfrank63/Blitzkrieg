//! The Game camera turns in quarter turns (M006 S01,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md). With the
//! Remastered option on, the turn key (`.`) turns the view about the point at
//! the screen centre; every building then draws its variant turned as many
//! times (`<variant>_r<k>`, M005's sibling folders, made here by `bk-views
//! side` into a scratch mod at the Data paths the Game looks them up by) and
//! the ground is drawn turned.
//!
//! One map written from the shipped `Data/Maps/river3d.xml` holds the 2003
//! e_ctownhouse01_2 and e_frenchshed01 by their objects.xml names. The real
//! Game runs it three times under a BK_AUTO_UI schedule that presses the key
//! three times and takes a shot at each of the four steps:
//!
//! - objects only (BK_SCENE_DISABLE): each building must be drawn where the
//!   quarter turn of the 2003 camera puts it and with its turned side's
//!   sprite (outline IoU and colour as building-views-game-reads-it, and
//!   better than any other side of it at the same place). The turn is
//!   predicted from the 2003 camera (tools/zig/fixtures/bk-views/camera-2003.json:
//!   world x one pixel right and half a pixel down, world y right and up) and
//!   the side convention (x, y) -> (-y, x), not from the engine: a screen
//!   offset (dx, dy) from the centre becomes (2 dy, -dx / 2).
//! - terrain only: every ground pixel around the centre must be found where
//!   the same turn takes it in the step-0 frame, and much closer there than
//!   under no turn or the opposite turn.
//! - objects only with the option off: the key does nothing, the 2003 sides
//!   stay where they were.
//!
//! S02 adds a second map, facing.xml: the town house at own facing 3 (map
//! Direction 49152) and the shed damaged at own facing 2. With the option on
//! each must show turn (facing + step) mod 4 of its variant in its damage
//! state; with it off (the last run) the 2003 picture, as the 2003 game
//! ignored a building's Direction. The town house's turn 2 is side 4, which
//! the 2003 e_ctownhouse01_4 shows: bk-views side must have made it from that
//! picture.
//!
//! The Game runs from zig-out/local-test/camera-turn/game with every user path
//! redirected; frames (`<run>-<step>.png`), sprites, logs and scores.txt stay
//! there. BK_VIEWS_GAME names the installed Game and BK_VIEWS_TOOL bk-views;
//! `zig build camera-turn-game-reads-it` sets both.
const std = @import("std");
const views = @import("bk_views.zig");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/game";
const mod_name = "camera_turn_test";
/// Where the turned sides go in the mod: the folder of the 2003 variants, so
/// `buildings\Europe\Summer\<variant>_r<k>` finds them.
const mod_buildings = "buildings/europe/summer";
const width = 1024;
const height = 768;
/// `camera=` 512 puts the map's middle under the screen's (511, 383)
/// (building-views-game-reads-it), the point the camera turns about.
const camera = 512;
const centre = [2]i32{ 511, 383 };

const Building = struct {
    /// Its objects.xml name.
    name: []const u8,
    /// Its 2003 variant folder; the turns are `<folder name>_r<k>` in the mod.
    folder: []const u8,
    /// Where it stands in the map XML, and where its zero cross is on the screen at step 0.
    at: [2]u32,
    screen: [2]i32,
};

const buildings = [_]Building{
    .{ .name = "E_CTownHouse01_2", .folder = "Data/Buildings/europe/summer/e_ctownhouse01_2", .at = .{ 574, 374 }, .screen = .{ 261, 433 } },
    .{ .name = "E_FrenchShed01", .folder = "Data/Buildings/europe/summer/e_frenchshed01", .at = .{ 1074, 874 }, .screen = .{ 761, 433 } },
};

/// A building of facing.xml: which of `buildings`, at the same place, with its
/// own facing (quarter turns; the map Direction is 16384 times it), its HP and
/// the damage state that HP draws (above 0.5 the first, above 0 the second).
const Placed = struct { building: usize, facing: usize, hp: []const u8, state: []const u8 };
const facing_map = [_]Placed{
    .{ .building = 0, .facing = 3, .hp = "1", .state = "1" },
    .{ .building = 1, .facing = 2, .hp = "0.3", .state = "2" },
};

/// The 2003 variant that shows the town house's turn 2 (side 4).
const sibling_folder = "Data/Buildings/europe/summer/e_ctownhouse01_4";

/// building-views-game-reads-it's floors: the Game draws the packs' pixels unchanged.
const min_iou = 0.95;
const max_colour = 2.0;
/// The ground: the mean RGB difference of the turned frame against the step-0
/// frame turned, and how much closer that must be than under a wrong turn.
const max_ground_difference = 12.0;
const min_ground_margin = 2.0;
/// Steps k and k + 2 over the whole screen but the interface.
const max_half_turn_difference = 3.0;

/// A screen offset from the centre after k quarter turns of the world: world x
/// projects to (1, 1/2), world y to (1, -1/2), and the turn takes x to y, so
/// (dx, dy) goes to (2 dy, -dx / 2).
fn turnOffset(d: [2]f64, k: usize) [2]f64 {
    var q = d;
    for (0..k) |_| {
        const x = q[0];
        q = .{ 2 * q[1], -x / 2 };
    }
    return q;
}

fn turnScreen(screen: [2]i32, k: usize) [2]i32 {
    const d = turnOffset(.{ @floatFromInt(screen[0] - centre[0]), @floatFromInt(screen[1] - centre[1]) }, k);
    return .{ centre[0] + @as(i32, @intFromFloat(@round(d[0]))), centre[1] + @as(i32, @intFromFloat(@round(d[1]))) };
}

test "a quarter turn moves a screen offset as the 2003 camera turns the world" {
    // World x is (1, 0.5) on the screen and world y (1, -0.5): x turned is y.
    try testing.expectEqual([2]f64{ 1, -0.5 }, turnOffset(.{ 1, 0.5 }, 1));
    try testing.expectEqual([2]f64{ -1, -0.5 }, turnOffset(.{ 1, 0.5 }, 2));
    try testing.expectEqual([2]f64{ 1, 0.5 }, turnOffset(.{ 1, 0.5 }, 4));
}

test "the Game camera turns in quarter turns: buildings show their turned sides where the turn puts them, the ground turns" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const turned = try std.fs.path.join(allocator, &.{ data, mod_buildings });
    try cwd.createDirPath(io, turned);
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    for (buildings) |building| try game_views.runTool(io, allocator, tool, &.{ "side", building.folder, "1,2,3", turned, work });
    try writeMod(io, allocator, data);

    var sprites: [buildings.len][4]game_views.Sprite = undefined;
    var placed_sprites: [facing_map.len][4]game_views.Sprite = undefined;
    for (buildings, 0..) |building, b| for (0..4) |k| {
        const base = std.fs.path.basename(building.folder);
        const folder = if (k == 0) building.folder else try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}", .{ turned, base, k });
        const out = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}_{d}", .{ root, base, k });
        sprites[b][k] = try game_views.decompose(io, allocator, tool, folder, out);
        for (facing_map, 0..) |placed, p| {
            if (placed.building == b) placed_sprites[p][k] = try onGround(io, allocator, out, placed.state);
        }
    };

    var failed = false;
    var report: std.ArrayList(u8) = .empty;

    // The town house's turn 2 is the 2003 side 4, picture for picture.
    const sibling = try game_views.decompose(io, allocator, tool, sibling_folder, try std.fmt.allocPrint(allocator, "{s}/sprites/sibling", .{root}));
    const same = comparePictures(sprites[0][2], sibling);
    try report.print(allocator, "turn 2 of {s} against {s}: IoU {d:.3}, colour {d:.2}; its zero cross {d},{d} from the turned one\n", .{ buildings[0].name, std.fs.path.basename(sibling_folder), same.iou, same.colour, same.dx, same.dy });
    if (same.iou < 0.999 or same.colour > 0.5) failed = true;
    const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";

    // Remastered on: each step draws the turned side where the turn puts it.
    try report.appendSlice(allocator, "on, objects: step building: IoU / colour at x,y (offset); best IoU of another side there\n");
    const on = try runGame(io, allocator, game, root, "on", "turn.xml", "ON", objects_only, 4);
    for (on, 0..) |frame, k| for (buildings, 0..) |building, b| {
        const at = turnScreen(building.screen, k);
        const own = game_views.score(frame, sprites[b][k], at);
        var other_best: f64 = 0;
        for (0..4) |o| {
            if (o != k) other_best = @max(other_best, game_views.score(frame, sprites[b][o], at).iou);
        }
        try report.print(allocator, "{d} {s}: {d:.3} / {d:.2} at {d},{d} ({d},{d}); {d:.3}\n", .{ k, building.name, own.iou, own.colour, at[0], at[1], own.dx, own.dy, other_best });
        if (own.iou < min_iou or own.colour > max_colour or other_best >= own.iou) failed = true;
    };

    // Remastered on, the ground alone: step k's frame is step 0's turned k times.
    try report.appendSlice(allocator, "on, terrain: step: mean RGB difference turned / not turned / turned the other way; mean brightness\n");
    const ground = try runGame(io, allocator, game, root, "ground", "turn.xml", "ON", "shadows,objects,units,effects,noise,warfog,border,haze,grid", 4);
    for (1..4) |k| {
        const turned_diff = groundDifference(ground[0], ground[k], k);
        const still = groundDifference(ground[0], ground[k], 0);
        const other_way = groundDifference(ground[0], ground[k], 4 - k);
        const brightness = meanBrightness(ground[k]);
        try report.print(allocator, "{d}: {d:.2} / {d:.2} / {d:.2}; {d:.1}\n", .{ k, turned_diff, still, other_way, brightness });
        const wrong = if (k == 2) still else @min(still, other_way);
        if (turned_diff > max_ground_difference or wrong < min_ground_margin * turned_diff or brightness < 20) failed = true;
    }
    // A half turn is the point reflection about the centre, which keeps the
    // whole screen on the screen: so steps k and k + 2 must agree out to the
    // screen's edges, where a tile culled as if unturned leaves a gap.
    for (0..2) |k| {
        const half = halfTurnDifference(ground[k], ground[k + 2]);
        try report.print(allocator, "{d} and {d}, the whole screen: {d:.2}\n", .{ k, k + 2, half });
        if (half > max_half_turn_difference) failed = true;
    }

    // Remastered on, each building of facing.xml alone (turned, the town house
    // stands over the shed at some steps): it shows turn (facing + step) of its
    // state.
    try report.appendSlice(allocator, "on, facing: step building facing state: turn: IoU / colour at x,y (offset); best IoU of another turn there\n");
    for (facing_map, 0..) |placed, p| for (try runGame(io, allocator, game, root, try std.fmt.allocPrint(allocator, "facing{d}", .{p}), try std.fmt.allocPrint(allocator, "facing{d}.xml", .{p}), "ON", objects_only, 4), 0..) |frame, k| {
        const building = buildings[placed.building];
        const at = turnScreen(building.screen, k);
        const t = (placed.facing + k) % 4;
        const own = game_views.score(frame, placed_sprites[p][t], at);
        var other_best: f64 = 0;
        for (0..4) |o| {
            if (o != t) other_best = @max(other_best, game_views.score(frame, placed_sprites[p][o], at).iou);
        }
        try report.print(allocator, "{d} {s} {d} {s}: {d}: {d:.3} / {d:.2} at {d},{d} ({d},{d}); {d:.3}\n", .{ k, building.name, placed.facing, placed.state, t, own.iou, own.colour, at[0], at[1], own.dx, own.dy, other_best });
        if (own.iou < min_iou or own.colour > max_colour or other_best >= own.iou) failed = true;
    };

    // Remastered off, facing.xml: the key does nothing, and every building
    // shows its 2003 picture where it stands, its Direction ignored.
    try report.appendSlice(allocator, "off, facing: shot building: IoU / colour (offset); pixels changed by the key\n");
    const off = try runGame(io, allocator, game, root, "off", "facing.xml", "OFF", objects_only, 2);
    for (off, 0..) |frame, shot| for (facing_map, 0..) |placed, p| {
        const building = buildings[placed.building];
        const own = game_views.score(frame, placed_sprites[p][0], building.screen);
        try report.print(allocator, "{d} {s}: {d:.3} / {d:.2} ({d},{d})\n", .{ shot, building.name, own.iou, own.colour, own.dx, own.dy });
        if (own.iou < min_iou or own.colour > max_colour or own.dx != 0 or own.dy != 0) failed = true;
    };
    var changed: usize = 0;
    for (0..width * height) |i| {
        if (!std.mem.eql(u8, off[0][i * 4 ..][0..3], off[1][i * 4 ..][0..3])) changed += 1;
    }
    try report.print(allocator, "{d}\n", .{changed});
    if (changed != 0) failed = true;

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the camera turn is drawn wrong (IoU floor {d}, colour ceiling {d}, ground ceiling {d}, margin {d}); the frames are in {s}\n", .{ report.items, min_iou, max_colour, max_ground_difference, min_ground_margin, root });
        return error.DrawnWrong;
    }
}

/// mod.xml, maps/turn.xml (river3d with the two buildings at facing 0),
/// maps/facing.xml (`facing_map`) and facing<i>.xml (its building i alone).
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    var turn_map: [buildings.len]Placed = undefined;
    for (&turn_map, 0..) |*placed, i| placed.* = .{ .building = i, .facing = 0, .hp = "1", .state = "1" };
    const maps = [_]struct { []const u8, []const Placed }{
        .{ "turn.xml", &turn_map },
        .{ "facing.xml", &facing_map },
        .{ "facing0.xml", facing_map[0..1] },
        .{ "facing1.xml", facing_map[1..2] },
    };
    for (maps) |entry| {
        const name, const placements = entry;
        var items: std.ArrayList(u8) = .empty;
        try items.appendSlice(allocator, "<Objects>");
        for (placements, 0..) |placed, i| {
            const building = buildings[placed.building];
            try items.print(allocator, "<item Direction=\"{d}\" Player=\"-1\" ScriptID=\"-1\" HP=\"{s}\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
                "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ placed.facing * 16384, placed.hp, building.name, building.at[0], building.at[1], i + 1 });
        }
        try items.appendSlice(allocator, "</Objects>");
        try cwd.writeFile(io, .{
            .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", name }),
            .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
        });
    }
}

/// The sprite of a damage state over its ground picture (`<state>g`, the
/// rubble a damaged building lies in, drawn with the objects), where the
/// variant has one: what the Game draws there. decompose lays both on one canvas.
fn onGround(io: std.Io, allocator: std.mem.Allocator, out: []const u8, state: []const u8) !game_views.Sprite {
    const sprite = try game_views.decomposed(io, allocator, out, state);
    const ground = game_views.decomposed(io, allocator, out, try std.fmt.allocPrint(allocator, "{s}g", .{state})) catch return sprite;
    for (sprite.image.pixels, ground.image.pixels) |*p, g| {
        if (p[3] < 128 and g[3] > p[3]) p.* = g;
    }
    return sprite;
}

/// Two pictures laid on each other by the top left corners of their opaque
/// pixels: the outline IoU, the mean colour difference where both are opaque,
/// and where b's zero cross falls from a's then (pixels).
fn comparePictures(a: game_views.Sprite, b: game_views.Sprite) struct { iou: f64, colour: f64, dx: i32, dy: i32 } {
    const corner = struct {
        fn f(image: views.Image) [2]i32 {
            var lo = [2]i32{ std.math.maxInt(i32), std.math.maxInt(i32) };
            for (image.pixels, 0..) |p, i| if (p[3] >= 128) {
                lo = .{ @min(lo[0], @as(i32, @intCast(i % image.width))), @min(lo[1], @as(i32, @intCast(i / image.width))) };
            };
            return lo;
        }
    }.f;
    const la = corner(a.image);
    const lb = corner(b.image);
    var both: f64 = 0;
    var colour: f64 = 0;
    var count_a: f64 = 0;
    for (0..a.image.height) |y| for (0..a.image.width) |x| {
        const pa = a.image.pixels[y * a.image.width + x];
        if (pa[3] < 128) continue;
        count_a += 1;
        const bx = @as(i32, @intCast(x)) - la[0] + lb[0];
        const by = @as(i32, @intCast(y)) - la[1] + lb[1];
        if (bx < 0 or by < 0 or bx >= b.image.width or by >= b.image.height) continue;
        const pb = b.image.pixels[@as(usize, @intCast(by)) * b.image.width + @as(usize, @intCast(bx))];
        if (pb[3] < 128) continue;
        both += 1;
        for (0..3) |c| colour += @abs(@as(f64, @floatFromInt(pa[c])) - @as(f64, @floatFromInt(pb[c])));
    };
    var count_b: f64 = 0;
    for (b.image.pixels) |p| count_b += @floatFromInt(@intFromBool(p[3] >= 128));
    const either = count_a + count_b - both;
    return .{
        .iou = if (either == 0) 0 else both / either,
        .colour = if (both == 0) 255 else colour / (3 * both),
        .dx = (b.anchor[0] - lb[0]) - (a.anchor[0] - la[0]),
        .dy = (b.anchor[1] - lb[1]) - (a.anchor[1] - la[1]),
    };
}

/// Runs the Game on `map` with the Remastered option set to `remastered`
/// and the passes `disable` off, takes `shots` shots with the turn key pressed
/// between them and returns them (RGBA), also kept as `<name>-<k>.png`; it must
/// exit 0 without a leak.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, name: []const u8, map: []const u8, remastered: []const u8, disable: []const u8, comptime shots: usize) ![shots][]u8 {
    const cwd = std.Io.Dir.cwd();
    const user = try std.fs.path.join(allocator, &.{ root, "user", name });
    try cwd.createDirPath(io, user);
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
    try environ.put("BK_SCENE_DISABLE", disable);
    // The cursor is parked first (on the window's edge it scrolls the view, see
    // building-views-game-reads-it), then the camera set and the option chosen;
    // each shot has its own wait so that its file name is its own.
    var schedule: std.ArrayList(u8) = .empty;
    try schedule.print(allocator, "g1000:hover=1000x400,g10:camera={d}x{d},g10:set=GamePlay.Remastered={s}", .{ camera, camera, remastered });
    for (0..shots) |k| {
        if (k != 0) try schedule.appendSlice(allocator, ",g10:key=.");
        try schedule.print(allocator, ",g{d}:shot", .{1500 + k});
    }
    try schedule.appendSlice(allocator, ",g200:exit");
    try environ.put("BK_AUTO_UI", schedule.items);
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", std.fmt.comptimePrint("-mode={d}x{d}", .{ width, height }), "-profile=Turn", "-mod=" ++ mod_name, map },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(240), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    var frames: [shots][]u8 = undefined;
    for (&frames, 0..) |*frame, k| {
        const shot_name = try std.fmt.allocPrint(allocator, "autoshot_g{d}_{d}x{d}.rgba", .{ 1500 + k, width, height });
        frame.* = cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ user, shot_name }), allocator, .limited(64 << 20)) catch |err| {
            std.debug.print("no shot {s} from the Game ({s}); its output is in {s}\n", .{ shot_name, name, log_path });
            return err;
        };
        try testing.expectEqual(@as(usize, width * height * 4), frame.len);
        const image: views.Image = .{ .width = width, .height = height, .pixels = std.mem.bytesAsSlice([4]u8, frame.*) };
        try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(allocator, "{s}/{s}-{d}.png", .{ root, name, k }), .data = try views.writePng(allocator, image) });
    }
    return frames;
}

fn pixel(frame: []const u8, x: i32, y: i32) [3]f64 {
    const i: usize = @intCast((y * width + x) * 4);
    return .{ @floatFromInt(frame[i]), @floatFromInt(frame[i + 1]), @floatFromInt(frame[i + 2]) };
}

/// The mean RGB difference between the ground around the centre in `before`
/// and where k quarter turns take it in `after`, at the best offset within 2
/// pixels (the anchor snaps to whole screen steps along the turned axes). The
/// region, 400 x 200 pixels, stays on the screen when turned.
fn groundDifference(before: []const u8, after: []const u8, k: usize) f64 {
    var best: f64 = std.math.floatMax(f64);
    var oy: i32 = -2;
    while (oy <= 2) : (oy += 1) {
        var ox: i32 = -2;
        while (ox <= 2) : (ox += 1) {
            var sum: f64 = 0;
            var count: f64 = 0;
            var dy: i32 = -100;
            while (dy <= 100) : (dy += 2) {
                var dx: i32 = -200;
                while (dx <= 200) : (dx += 2) {
                    const to = turnScreen(.{ centre[0] + dx, centre[1] + dy }, k);
                    const a = pixel(before, centre[0] + dx, centre[1] + dy);
                    const b = pixel(after, to[0] + ox, to[1] + oy);
                    for (0..3) |c| sum += @abs(a[c] - b[c]);
                    count += 3;
                }
            }
            best = @min(best, sum / count);
        }
    }
    return best;
}

/// The mean RGB difference between `a` and `b` reflected about the centre,
/// over the screen's full width and the rows that are clear of the interface
/// in both (the message line at the top, the panels at the bottom), at the
/// best offset within 2 pixels.
fn halfTurnDifference(a: []const u8, b: []const u8) f64 {
    var best: f64 = std.math.floatMax(f64);
    var oy: i32 = -2;
    while (oy <= 2) : (oy += 1) {
        var ox: i32 = -2;
        while (ox <= 2) : (ox += 1) {
            var sum: f64 = 0;
            var count: f64 = 0;
            var y: i32 = 172;
            while (y < 594) : (y += 2) {
                var x: i32 = 2;
                while (x < width - 4) : (x += 2) {
                    const p = pixel(a, x, y);
                    const q = pixel(b, 2 * centre[0] - x + ox, 2 * centre[1] - y + oy);
                    for (0..3) |c| sum += @abs(p[c] - q[c]);
                    count += 3;
                }
            }
            best = @min(best, sum / count);
        }
    }
    return best;
}

fn meanBrightness(frame: []const u8) f64 {
    var sum: f64 = 0;
    var dy: i32 = -100;
    while (dy <= 100) : (dy += 4) {
        var dx: i32 = -200;
        while (dx <= 200) : (dx += 4) {
            const p = pixel(frame, centre[0] + dx, centre[1] + dy);
            sum += (p[0] + p[1] + p[2]) / 3;
        }
    }
    return sum / (51.0 * 101.0);
}
