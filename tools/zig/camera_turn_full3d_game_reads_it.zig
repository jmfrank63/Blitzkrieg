//! The Game draws the turns of full 3D objects (docs/superpowers/specs/
//! 2026-10-29-full-3d-objects-design.md): props and a damaged building,
//! every state of each rendered from its own textured model by `bk-views
//! side` into a scratch mod at the Data paths the Game looks them up by
//! (`<folder>_r<k>`).
//!
//! Each object stands alone on a map written from the shipped
//! `Data/Maps/river3d.xml`, off the screen's middle so that a turn moves it:
//! the props UkraineWell01 and Semaphore02, which show turn k at camera step k
//! (a prop has no facing of its own), and the warehouse E_WB_01 at own facing
//! 1 with its HP at 0.3, which shows its damage state 2 (`2.mod` rendered) at
//! turn (facing + step) mod 4, over its turned rubble. The real Game runs
//! each map with Remastered on under a BK_AUTO_UI schedule that presses the
//! turn key (`,`, one step on) three times and takes a shot at each of the
//! four steps, objects only (BK_SCENE_DISABLE): at every step the object must
//! be drawn where the quarter turn of the 2003 camera puts it with the
//! picture of its turn, as the packs hold it (outline IoU and colour as
//! camera-turn-game-reads-it; at turn 0, the 2003 picture, as
//! camera-turn-world-game-reads-it measures 2003 props), and better than any
//! other turn of it there.
//!
//! Two more tests (the maintainer's hand test of 2026-10-30, built by
//! `camera-turn-full3d-anchor-game-reads-it` and `-shadow-`): the barn
//! E_WB_01, the L-shaped house A_H01_1, the house E_House02_2 and the prop
//! Rick each turn in place, the turned model standing on the ground where the
//! 2003 picture stands at step 0 (measured on the pictures' drawn extents,
//! which a body standing still in the world ties across the steps); and
//! E_WB_01, E_WB_02 at own facing 3 and Rick cast at every step the shadow
//! of their pictures lit by the world's sun, against their foot, the way the
//! sun casts.
//!
//! The Game runs from zig-out/local-test/camera-turn/full3d with every user
//! path redirected (camera_turn_game_reads_it.runGame); frames, sprites, logs
//! and scores.txt stay there. BK_VIEWS_GAME names the installed Game and
//! BK_VIEWS_TOOL bk-views; `zig build camera-turn-full3d-game-reads-it` sets
//! both.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const units = @import("camera_turn_units_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/full3d";
const camera = 512;

const Object = struct {
    /// Its objects.xml name.
    name: []const u8,
    /// Its 2003 folder; the turns are `<folder name>_r<k>` beside it in the mod.
    folder: []const u8,
    /// Where `side` writes the turns in the mod's data folder.
    mod_dir: []const u8,
    /// Its own facing (quarter turns, map Direction 16384 times it), HP and
    /// the damage state that HP draws.
    facing: usize = 0,
    hp: []const u8 = "1",
    state: []const u8 = "1",
};

const objects = [_]Object{
    .{ .name = "UkraineWell01", .folder = "Data/Objects/SimpleObjects/ussr/summer/ukrainewell/01", .mod_dir = "objects/simpleobjects/ussr/summer/ukrainewell" },
    .{ .name = "Semaphore02", .folder = "Data/Objects/SimpleObjects/common/summer/semaphore/02", .mod_dir = "objects/simpleobjects/common/summer/semaphore" },
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01", .mod_dir = "buildings/europe/summer/e_wb", .facing = 1, .hp = "0.3", .state = "2" },
};

/// Where an object stands on the map and its zero cross on the screen at step 0.
const at = [2]u32{ 574, 374 };
const screen = [2]i32{ 261, 433 };

/// A rendered turn: the Game draws its one-bit pixels unchanged.
const min_iou: f64 = 0.95;
const max_colour: f64 = 2.0;
/// Turn 0 is the 2003 picture, its edges soft: a small prop's outline is a few
/// pixels of edge (camera-turn-world-game-reads-it's floors for the 2003 pictures).
const min_2003_iou: f64 = 0.9;
const max_2003_colour: f64 = 8.0;

test "the Game turns full 3D props and a damaged building: each shows its turn's rendered picture where the turn puts it" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-full3d-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    try writeMod(io, allocator, data);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "step object facing state: turn: IoU / colour at x,y (offset); best IoU of another turn there\n");
    for (objects, 0..) |object, o| {
        const turned = try std.fs.path.join(allocator, &.{ data, object.mod_dir });
        try cwd.createDirPath(io, turned);
        try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, "1,2,3", turned, work });
        // The pictures the Game must show: the 2003 one at turn 0, else the turn's rendered one.
        var sprites: [4]game_views.Sprite = undefined;
        const base = std.fs.path.basename(object.folder);
        for (&sprites, 0..) |*sprite, k| {
            const folder = if (k == 0) object.folder else try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}", .{ turned, base, k });
            const out = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}_{d}", .{ root, object.name, k });
            _ = try game_views.decompose(io, allocator, tool, folder, out);
            sprite.* = try onGround(io, allocator, out, object.state);
        }
        const frames = try turn.runGame(io, allocator, game, root, object.name, try std.fmt.allocPrint(allocator, "object{d}.xml", .{o}), .{ camera, camera }, "ON", "shadows,effects,terrain,noise,warfog,border,haze,grid", 4);
        for (frames, 0..) |frame, k| {
            const where = turn.turnScreen(screen, k);
            const t = (object.facing + k) % 4;
            const own = game_views.score(frame, sprites[t], where);
            var other_best: f64 = 0;
            for (0..4) |other| {
                if (other != t) other_best = @max(other_best, game_views.score(frame, sprites[other], where).iou);
            }
            try report.print(allocator, "{d} {s} {d} {s}: {d}: {d:.3} / {d:.2} at {d},{d} ({d},{d}); {d:.3}\n", .{ k, object.name, object.facing, object.state, t, own.iou, own.colour, where[0], where[1], own.dx, own.dy, other_best });
            const rendered = t != 0;
            if (own.iou < (if (rendered) min_iou else min_2003_iou) or own.colour > (if (rendered) max_colour else max_2003_colour) or other_best >= own.iou) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a full 3D object's turn is drawn wrong (IoU floor {d}, colour ceiling {d}; at turn 0, the 2003 picture, {d} and {d}); the frames are in {s}\n", .{ report.items, min_iou, max_colour, min_2003_iou, max_2003_colour, root });
        return error.DrawnWrong;
    }
}

/// The objects the turn-in-place test stands, each alone: the barn of the
/// maintainer's hand test (E_WB_01, a 9 x 6 footprint, ramps on its front),
/// the large L-shaped African house A_H01_1, the house E_House02_2 with its
/// back wing, and the prop Rick.
const standing = [_]Object{
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01", .mod_dir = "buildings/europe/summer/e_wb" },
    .{ .name = "A_H01_1", .folder = "Data/Buildings/africa/summer/a_h01_1", .mod_dir = "buildings/africa/summer" },
    .{ .name = "E_House02_2", .folder = "Data/Buildings/europe/summer/e_house02_2", .mod_dir = "buildings/europe/summer" },
    .{ .name = "Rick", .folder = "Data/Objects/SimpleObjects/common/summer/rick", .mod_dir = "objects/simpleobjects/common/summer" },
};

/// The objects the shadow test stands, each alone: the barn E_WB_01, its
/// other side E_WB_02 at own facing 3 (the hand test's screenshot: at step 1
/// it shows turn 0, its 2003 side), and the prop Rick.
const casting = [_]Object{
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01", .mod_dir = "buildings/europe/summer/e_wb" },
    .{ .name = "E_WB_02", .folder = "Data/Buildings/europe/summer/e_wb/02", .mod_dir = "buildings/europe/summer/e_wb", .facing = 3 },
    .{ .name = "Rick", .folder = "Data/Objects/SimpleObjects/common/summer/rick", .mod_dir = "objects/simpleobjects/common/summer" },
};

/// The AI point under the screen's centre (`camera` world units, AI = world
/// times the square root of 2).
const middle_ai = 724;
/// The shadow test looks at road3d away from its road (drawn over shadows),
/// at world (297, 297): AI 420.02.
const shadow_look = 297;
const shadow_middle_ai = 420;
/// World units an AI tile, as bk-views' footprints (1.xml origins in them).
const tile_world: f64 = 16.0 * std.math.sqrt2;
/// The 2003 sun towards the ground (bk-views `light_2003`, azimuth 245
/// degrees in the world frame): where a shadow falls from its object.
const sun_ground = [2]f64{ -@cos(std.math.degreesToRadians(245.0)), -@sin(std.math.degreesToRadians(245.0)) };

const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
const with_shadows = "effects,noise,warfog,haze,grid";
/// Anchor and rigidity: a pixel and a little, as a pixel's cut at the quarter
/// turns' 2:1 stretch falls either side.
const max_anchor_px: f64 = 2.0;
/// A shadow: the Game draws the shadow of the pictures lit by the world's
/// sun for the step (the darkness threshold cuts its soft rim and the ground's
/// grain a little), all of it joined to its object's picture, and it falls
/// the way the sun casts (the centroid of a shadow the picture half hides,
/// a strip along a wall, leans off the sun by up to 32 degrees).
const min_lit_shadow_iou: f64 = 0.8;
const min_attached: f64 = 0.95;
const max_sun_degrees: f64 = 35;

/// The turns `side` makes for an object at own facing f, as camera-turn-mod
/// makes them for every building: the turns lit by the 2003 light (`1,2,3`;
/// the Game asks for them before it asks for one lit by the world's sun) and
/// turn f + s lit by the world's sun as camera step s sees it (`<k>l<s>`).
fn turnsFor(allocator: std.mem.Allocator, facing: usize) ![]const u8 {
    var list: std.ArrayList(u8) = .empty;
    try list.appendSlice(allocator, "1,2,3");
    for (1..4) |s| try list.print(allocator, ",{d}l{d}", .{ (facing + s) % 4, s });
    return list.items;
}

test "the Game turns full 3D objects in place: each stands where its 2003 picture stands at every step" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-full3d-anchor-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root_rel = scratch ++ "-anchor";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), root_rel });
    cwd.deleteTree(io, root_rel) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    const map = try writeStandingMod(io, allocator, data, "Data/Maps/river3d.xml");

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator,
        \\Per step k the object's drawn extent, from its zero cross: left L, right R, bottom B (pixel indices).
        \\Rigid (0 for a body that stands still in the world; a half turn takes an offset d to -1 - d):
        \\  the middles of steps 0 and 2, M0 + M2 + 1 (the 2003 picture and the model), R1 + L3 + 1, L1 + R3 + 1.
        \\Anchor: where the turned model meets the ground in front against where the 2003 picture does at step 0,
        \\  (R1 + L1) / 2 + B2 - B0 and -(L3 + R3) / 2 + B2 - B0 (step-0 screen pixels; front and back eaves taken alike).
        \\
    );
    for (standing, 0..) |object, o| {
        const turned = try std.fs.path.join(allocator, &.{ data, object.mod_dir });
        try cwd.createDirPath(io, turned);
        try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, try turnsFor(allocator, 0), turned, work });
        const foot = try footprintMiddle(io, allocator, tool, object.folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{s}", .{ root, object.name }));
        // The footprint's middle a little off the screen's middle, so that a turn moves it.
        const place = try placeStanding(io, allocator, data, map, object, o, foot, middle_ai, .{ 40, 0 });
        const shots = try turn.runGame(io, allocator, game, root, object.name, try std.fmt.allocPrint(allocator, "standing{d}.xml", .{o}), .{ camera, camera }, "ON", objects_only, 4);

        var e: [4]Extent = undefined;
        for (0..4) |k| {
            const zero = turn.turnScreen(place, k);
            e[k] = extent(shots[k], zero);
            try report.print(allocator, "{s} step {d}: zero cross {d},{d}; L {d} R {d} B {d}\n", .{ object.name, k, zero[0], zero[1], e[k].left, e[k].right, e[k].bottom });
        }
        const rigid = [3]f64{ (e[0].left + e[0].right) / 2 + (e[2].left + e[2].right) / 2 + 1, e[1].right + e[3].left + 1, e[1].left + e[3].right + 1 };
        const front = [2]f64{ (e[1].right + e[1].left) / 2 + e[2].bottom - e[0].bottom, -(e[3].left + e[3].right) / 2 + e[2].bottom - e[0].bottom };
        try report.print(allocator, "{s}: rigid {d:.1} {d:.1} {d:.1}; anchor {d:.1} {d:.1}\n", .{ object.name, rigid[0], rigid[1], rigid[2], front[0], front[1] });
        for (rigid) |r| {
            if (@abs(r) > max_anchor_px) failed = true;
        }
        for (front) |f| {
            if (@abs(f) > max_anchor_px) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a full 3D object does not turn in place (within {d} pixels); the frames are in {s}\n", .{ report.items, max_anchor_px, root });
        return error.DrawnWrong;
    }
}

test "a full 3D object's shadow at every camera step is cast by the world's sun from its foot" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-full3d-shadow-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root_rel = scratch ++ "-shadow";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), root_rel });
    cwd.deleteTree(io, root_rel) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    // road3d: no water, whose waves would read as shadows between two runs.
    const map = try writeStandingMod(io, allocator, data, "Data/Maps/road3d.xml");
    // The ground alone, shadows on: what a shadow darkens.
    const ground = try turn.runGame(io, allocator, game, root, "ground", "ground.xml", .{ shadow_look, shadow_look }, "ON", with_shadows, 4);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator,
        \\Shadow at step k (object facing f, turn d = f + k shown): its pixels on the ground against the shadow
        \\of the pictures lit by the world's sun as step k sees them (`<d>l<k>`, at step 0 turn f's) laid at the
        \\zero cross, IoU; the share of it joined to the picture; the way from the footprint's middle to its
        \\centroid off the world's sun as step k shows it (degrees; held where the shadow shows a quarter of
        \\its most or more); pixels. Reported: IoU against the step-0 shadow turned with the ground, centroids apart.
        \\
    );
    for (casting, 0..) |object, o| {
        const turned = try std.fs.path.join(allocator, &.{ data, object.mod_dir });
        try cwd.createDirPath(io, turned);
        try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, try turnsFor(allocator, object.facing), turned, work });
        const own = try footprintMiddle(io, allocator, tool, object.folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{s}", .{ root, object.name }));
        // The footprint stands turned by the facing about the zero cross.
        var foot = own;
        for (0..object.facing) |_| foot = .{ -foot[1], foot[0] };
        const place = try placeStanding(io, allocator, data, map, object, o, foot, shadow_middle_ai, .{ 40, 0 });
        const name = try std.fmt.allocPrint(allocator, "standing{d}.xml", .{o});
        const shots = try turn.runGame(io, allocator, game, root, object.name, name, .{ shadow_look, shadow_look }, "ON", objects_only, 4);
        // Its dark pixels inside its outline are no shadow.
        var solid: [4][]const u8 = undefined;
        for (&solid, shots) |*s, shot| s.* = try solidShot(allocator, shot);
        const shadows = try turn.runGame(io, allocator, game, root, try std.fmt.allocPrint(allocator, "{s}-shadows", .{object.name}), name, .{ shadow_look, shadow_look }, "ON", with_shadows, 4);
        var zero: [4][2]i32 = undefined;
        for (&zero, 0..) |*z, k| z.* = turn.turnScreen(place, k);
        // The ground near the object at any step, from its zero cross.
        var near = nearBox(shots[0], zero[0], 90);
        for (1..4) |k| {
            const b = nearBox(shots[k], zero[k], 90);
            near = .{ @min(near[0], b[0]), @min(near[1], b[1]), @max(near[2], b[2]), @max(near[3], b[3]) };
        }
        var casts: [4]Cast = undefined;
        var most: usize = 0;
        for (&casts, 0..) |*cast, k| {
            cast.* = shadowOf(.{ shadows[k], ground[k], solid[k] }, near, zero[k], foot, k);
            most = @max(most, cast.pixels);
        }
        const base = std.fs.path.basename(object.folder);
        for (0..4) |k| {
            const frames = [3][]const u8{ shadows[k], ground[k], solid[k] };
            const d = (object.facing + k) % 4;
            const lit = if (k == 0 and d == 0) object.folder else if (k == 0) try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}", .{ turned, base, d }) else try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}_l{d}", .{ turned, base, d, k });
            const sprites = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}-lit{d}", .{ root, object.name, k });
            const expected: ?f64 = if (game_views.decompose(io, allocator, tool, lit, sprites)) |_|
                packShadowIou(frames, near, zero[k], try game_views.decomposed(io, allocator, sprites, "1s"))
            else |_|
                null;
            const joined = try attached(allocator, frames, near, zero[k]);
            const held = casts[k].pixels >= units.min_shadow_pixels and 4 * casts[k].pixels >= most;
            const world = units.sunShadow(.{ shadows[0], ground[0], solid[0] }, zero[0], frames, zero[k], k, near);
            if (expected) |iou| {
                try report.print(allocator, "{s} facing {d} step {d} turn {d}: {d:.3}; {d:.3}; {d:.1}{s}; {d}; {d:.3}, {d:.1}\n", .{ object.name, object.facing, k, d, iou, joined, casts[k].off_sun, if (held) "" else " (not held)", casts[k].pixels, world.iou, world.off_degrees });
                if (iou < min_lit_shadow_iou) failed = true;
            } else {
                try report.print(allocator, "{s} facing {d} step {d} turn {d}: no pictures lit by the world's sun ({s}); {d:.3}; {d:.1}; {d}; {d:.3}, {d:.1}\n", .{ object.name, object.facing, k, d, lit, joined, casts[k].off_sun, casts[k].pixels, world.iou, world.off_degrees });
                failed = true;
            }
            if (joined < min_attached or (held and casts[k].off_sun > max_sun_degrees)) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a full 3D object's shadow is not cast from its foot by the world's sun (IoU floor {d} against the lit pictures' shadow, joined {d}, within {d} degrees); the frames are in {s}\n", .{ report.items, min_lit_shadow_iou, min_attached, max_sun_degrees, root });
        return error.DrawnWrong;
    }
}

/// The box around an object's picture in a shot of objects alone grown by
/// `grow` pixels, from its zero cross (left, top, right, bottom): where its
/// shadow may lie.
fn nearBox(frame: []const u8, zero: [2]i32, grow: i32) [4]i32 {
    var box = [4]i32{ std.math.maxInt(i32), std.math.maxInt(i32), std.math.minInt(i32), std.math.minInt(i32) };
    var y = turn.clear_rows[0];
    while (y < turn.clear_rows[1]) : (y += 1) {
        var x: i32 = 0;
        while (x < turn.width) : (x += 1) {
            if (!units.drawn(units.rgb(frame, x, y))) continue;
            box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x), @max(box[3], y) };
        }
    }
    if (box[2] < box[0]) return .{ 0, 0, 0, 0 };
    return .{ box[0] - grow - zero[0], box[1] - grow - zero[1], box[2] + grow - zero[0], box[3] + grow - zero[1] };
}

/// A picture's drawn extent in a shot of objects alone, between the rows the
/// interface leaves (pixel indices from the zero cross, both ends drawn).
const Extent = struct { left: f64, right: f64, bottom: f64 };

fn extent(frame: []const u8, zero: [2]i32) Extent {
    var lo: i32 = std.math.maxInt(i32);
    var hi: i32 = std.math.minInt(i32);
    var bottom: i32 = std.math.minInt(i32);
    var y = turn.clear_rows[0];
    while (y < turn.clear_rows[1]) : (y += 1) {
        var x: i32 = 0;
        while (x < turn.width) : (x += 1) {
            if (!units.drawn(units.rgb(frame, x, y))) continue;
            lo = @min(lo, x);
            hi = @max(hi, x);
            bottom = @max(bottom, y);
        }
    }
    if (hi < lo) return .{ .left = 0, .right = 0, .bottom = 0 };
    return .{ .left = @floatFromInt(lo - zero[0]), .right = @floatFromInt(hi - zero[0]), .bottom = @floatFromInt(bottom - zero[1]) };
}

/// A shot of objects alone with the dark pixels inside a picture (timbers,
/// door gaps, which read as the black backdrop) painted white: the ground a
/// shadow may lie on is what the backdrop reaches from the screen's edge.
fn solidShot(allocator: std.mem.Allocator, frame: []const u8) ![]u8 {
    const w: usize = turn.width;
    const h: usize = turn.height;
    const out = try allocator.dupe(u8, frame);
    const outside = try allocator.alloc(bool, w * h);
    defer allocator.free(outside);
    @memset(outside, false);
    var queue: std.ArrayList(usize) = .empty;
    defer queue.deinit(allocator);
    for (0..h) |y| for (0..w) |x| {
        if (x != 0 and y != 0 and x != w - 1 and y != h - 1) continue;
        const i = y * w + x;
        if (units.drawn(frame[i * 4 ..][0..3].*)) continue;
        outside[i] = true;
        try queue.append(allocator, i);
    };
    var head: usize = 0;
    while (head < queue.items.len) : (head += 1) {
        const i = queue.items[head];
        const x = i % w;
        const y = i / w;
        const next = [4]?usize{ if (x > 0) i - 1 else null, if (x + 1 < w) i + 1 else null, if (y > 0) i - w else null, if (y + 1 < h) i + w else null };
        for (next) |n| {
            const j = n orelse continue;
            if (outside[j] or units.drawn(frame[j * 4 ..][0..3].*)) continue;
            outside[j] = true;
            try queue.append(allocator, j);
        }
    }
    // And the ring of a pixel round the outline, whose blend with the black
    // backdrop reads as darker than the ground.
    for (0..h) |y| for (0..w) |x| {
        const i = y * w + x;
        const ring = (x > 0 and !outside[i - 1]) or (x + 1 < w and !outside[i + 1]) or (y > 0 and !outside[i - w]) or (y + 1 < h and !outside[i + w]);
        if (!outside[i] or ring) out[i * 4 ..][0..3].* = .{ 255, 255, 255 };
    };
    return out;
}

fn isShadow(frames: [3][]const u8, x: i32, y: i32) bool {
    return !units.inInterface(y) and !units.drawn(units.rgb(frames[2], x, y)) and units.shadowed(frames[0], frames[1], x, y);
}

/// An object's shadow (`frames`: with shadows, the ground alone, objects
/// alone): its pixels, and the angle on the ground (screen y doubled) between
/// the way from the footprint's middle to their centroid and the way the
/// world's sun casts at step k.
const Cast = struct { pixels: usize, off_sun: f64 };

fn shadowOf(frames: [3][]const u8, box: [4]i32, zero: [2]i32, foot: [2]f64, k: usize) Cast {
    // The footprint's middle on the screen at step k, from the zero cross.
    const middle = turn.turnOffset(.{ (foot[0] + foot[1]) / std.math.sqrt2, (foot[0] - foot[1]) / (2 * std.math.sqrt2) }, k);
    var sum = [2]f64{ 0, 0 };
    var pixels: usize = 0;
    var y = turn.clear_rows[0];
    while (y < turn.clear_rows[1]) : (y += 1) {
        var x: i32 = 0;
        while (x < turn.width) : (x += 1) {
            if (!inBox(box, zero, x, y) or !isShadow(frames, x, y)) continue;
            pixels += 1;
            sum = .{ sum[0] + @as(f64, @floatFromInt(x - zero[0])) - middle[0], sum[1] + 2 * (@as(f64, @floatFromInt(y - zero[1])) - middle[1]) };
        }
    }
    if (pixels == 0) return .{ .pixels = 0, .off_sun = 0 };
    const sun = turn.turnOffset(.{ (sun_ground[0] + sun_ground[1]) / std.math.sqrt2, (sun_ground[0] - sun_ground[1]) / (2 * std.math.sqrt2) }, k);
    var angle = std.math.radiansToDegrees(@abs(std.math.atan2(sum[1], sum[0]) - std.math.atan2(2 * sun[1], sun[0])));
    if (angle > 180) angle = 360 - angle;
    return .{ .pixels = pixels, .off_sun = angle };
}

/// The share of a shadow's pixels joined to its object's picture through
/// shadow and picture pixels, a gap of a pixel bridged: a shadow cast from
/// the object's foot lies against it. Only the shadow in `box` (from the
/// zero cross) counts.
fn attached(allocator: std.mem.Allocator, frames: [3][]const u8, box: [4]i32, zero: [2]i32) !f64 {
    const w: usize = turn.width;
    const seen = try allocator.alloc(bool, w * turn.height);
    defer allocator.free(seen);
    @memset(seen, false);
    var queue: std.ArrayList([2]i32) = .empty;
    defer queue.deinit(allocator);
    var total: usize = 0;
    var y = turn.clear_rows[0];
    while (y < turn.clear_rows[1]) : (y += 1) {
        var x: i32 = 0;
        while (x < turn.width) : (x += 1) {
            if (inBox(box, zero, x, y) and isShadow(frames, x, y)) total += 1;
            if (!units.drawn(units.rgb(frames[2], x, y))) continue;
            seen[@as(usize, @intCast(y)) * w + @as(usize, @intCast(x))] = true;
            try queue.append(allocator, .{ x, y });
        }
    }
    if (total == 0) return 1;
    var joined: usize = 0;
    var i: usize = 0;
    while (i < queue.items.len) : (i += 1) {
        const p = queue.items[i];
        var dy: i32 = -2;
        while (dy <= 2) : (dy += 1) {
            var dx: i32 = -2;
            while (dx <= 2) : (dx += 1) {
                const x = p[0] + dx;
                const yy = p[1] + dy;
                if (x < 0 or x >= turn.width or units.inInterface(yy)) continue;
                const s = &seen[@as(usize, @intCast(yy)) * w + @as(usize, @intCast(x))];
                if (s.* or !inBox(box, zero, x, yy) or !isShadow(frames, x, yy)) continue;
                s.* = true;
                joined += 1;
                try queue.append(allocator, .{ x, yy });
            }
        }
    }
    return @as(f64, @floatFromInt(joined)) / @as(f64, @floatFromInt(total));
}

fn inBox(box: [4]i32, zero: [2]i32, x: i32, y: i32) bool {
    return x >= zero[0] + box[0] and x <= zero[0] + box[2] and y >= zero[1] + box[1] and y <= zero[1] + box[3];
}

/// The IoU of a shot's shadow near the object (`frames`: with shadows, the
/// ground alone, objects alone) and the shadow picture `pack` laid at the
/// zero cross, both on the ground the object's picture leaves in sight; the
/// best of the offsets within a pixel (the shadow pass cuts a pixel at the
/// quarter turns' 2:1 stretch either side, as the picture's place).
fn packShadowIou(frames: [3][]const u8, box: [4]i32, zero: [2]i32, pack: game_views.Sprite) f64 {
    var best: f64 = 0;
    var oy: i32 = -1;
    while (oy <= 1) : (oy += 1) {
        var ox: i32 = -1;
        while (ox <= 1) : (ox += 1) best = @max(best, packShadowIouAt(frames, box, .{ zero[0] + ox, zero[1] + oy }, pack));
    }
    return best;
}

fn packShadowIouAt(frames: [3][]const u8, box: [4]i32, zero: [2]i32, pack: game_views.Sprite) f64 {
    var both: f64 = 0;
    var either: f64 = 0;
    var y = zero[1] + box[1];
    while (y <= zero[1] + box[3]) : (y += 1) {
        var x = zero[0] + box[0];
        while (x <= zero[0] + box[2]) : (x += 1) {
            if (units.inInterface(y) or units.rgb(frames[0], x, y) == null or units.drawn(units.rgb(frames[2], x, y))) continue;
            const px = x - zero[0] + pack.anchor[0];
            const py = y - zero[1] + pack.anchor[1];
            const want = px >= 0 and py >= 0 and px < pack.image.width and py < pack.image.height and pack.image.pixels[@intCast(py * @as(i32, @intCast(pack.image.width)) + px)][3] >= 128;
            const got = units.shadowed(frames[0], frames[1], x, y);
            if (want or got) either += 1;
            if (want and got) both += 1;
        }
    }
    return if (either == 0) 1 else both / either;
}

/// The middle of a 2003 object's footprint in world units from its zero
/// cross (its passability's occupied box and origin, as bk-views reads them).
fn footprintMiddle(io: std.Io, allocator: std.mem.Allocator, tool: []const u8, folder: []const u8, out: []const u8) ![2]f64 {
    try game_views.runTool(io, allocator, tool, &.{ "decompose", folder, out });
    const json = try std.Io.Dir.cwd().readFileAlloc(io, try std.fs.path.join(allocator, &.{ out, "building.json" }), allocator, .limited(1 << 20));
    const parsed = try std.json.parseFromSliceLeaky(struct { origin: [2]f64, passability: []const []const u8 }, allocator, json, .{ .ignore_unknown_fields = true });
    var box = [4]usize{ std.math.maxInt(usize), std.math.maxInt(usize), 0, 0 };
    for (parsed.passability, 0..) |row, y| for (0..row.len / 2) |x| {
        if (std.mem.eql(u8, row[2 * x ..][0..2], "00")) continue;
        box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x + 1), @max(box[3], y + 1) };
    };
    if (box[2] == 0) return .{ 0, 0 };
    return .{
        @as(f64, @floatFromInt(box[0] + box[2])) * tile_world / 2 - parsed.origin[0],
        @as(f64, @floatFromInt(box[1] + box[3])) * tile_world / 2 - parsed.origin[1],
    };
}

/// maps/standing<o>.xml with the object alone at its own facing, its
/// footprint's middle (`foot`, world units from the zero cross, turned by the
/// facing) at `at_ai` AI units from the middle (its place a multiple of 4 AI
/// units from it, so its zero cross falls on a whole pixel); returns its zero
/// cross on the screen at step 0.
fn placeStanding(io: std.Io, allocator: std.mem.Allocator, data: []const u8, map: []const u8, object: Object, o: usize, foot: [2]f64, middle: i32, at_ai: [2]i32) ![2]i32 {
    var d: [2]i32 = undefined;
    for (&d, foot, at_ai) |*v, f, a| v.* = 4 * @as(i32, @intFromFloat(@round((@as(f64, @floatFromInt(a)) - f * std.math.sqrt2) / 4)));
    const items = try std.fmt.allocPrint(allocator, "<Objects><item Direction=\"{d}\" Player=\"-1\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
        "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"1\" Intention=\"0\" LinkWith=\"0\"/></item></Objects>", .{ object.facing * 16384, object.name, middle + d[0], middle + d[1] });
    try std.Io.Dir.cwd().writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", try std.fmt.allocPrint(allocator, "standing{d}.xml", .{o}) }),
        .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items),
    });
    return .{ turn.centre[0] + @divExact(d[0] + d[1], 2), turn.centre[1] + @divExact(d[0] - d[1], 4) };
}

/// mod.xml and maps/ground.xml (`from` as shipped, no objects); returns the
/// map the objects' maps are written from.
fn writeStandingMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8, from: []const u8) ![]const u8 {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, from, allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "ground.xml" }), .data = map });
    return map;
}

/// mod.xml and maps/object<i>.xml: river3d with object i alone.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    for (objects, 0..) |object, o| {
        const items = try std.fmt.allocPrint(allocator, "<Objects><item Direction=\"{d}\" Player=\"-1\" ScriptID=\"-1\" HP=\"{s}\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"1\" Intention=\"0\" LinkWith=\"0\"/></item></Objects>", .{ object.facing * 16384, object.hp, object.name, at[0], at[1] });
        try cwd.writeFile(io, .{
            .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", try std.fmt.allocPrint(allocator, "object{d}.xml", .{o}) }),
            .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items),
        });
    }
}

/// The sprite of a state over its ground picture (`<state>g`, the rubble a
/// damaged building lies in, drawn with the objects), where it has one: what
/// the Game draws there.
fn onGround(io: std.Io, allocator: std.mem.Allocator, out: []const u8, state: []const u8) !game_views.Sprite {
    const sprite = try game_views.decomposed(io, allocator, out, state);
    const ground = game_views.decomposed(io, allocator, out, try std.fmt.allocPrint(allocator, "{s}g", .{state})) catch return sprite;
    for (sprite.image.pixels, ground.image.pixels) |*p, g| {
        if (p[3] < 128 and g[3] > p[3]) p.* = g;
    }
    return sprite;
}
