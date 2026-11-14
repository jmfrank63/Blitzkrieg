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
//! E_WB_01, E_WB_02 at own facing 1 and Rick cast at every step the shadow
//! their turn casts under the Game's sun (the turn's shadow alone, its model
//! flattened on the ground as a vehicle's), against their foot, the way the
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
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const views = @import("bk_views.zig");
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
/// other side E_WB_02 at own facing 1 (the hand test's screenshot: at step 3
/// it shows turn 0, its 2003 side), and the prop Rick.
const casting = [_]Object{
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01", .mod_dir = "buildings/europe/summer/e_wb" },
    .{ .name = "E_WB_02", .folder = "Data/Buildings/europe/summer/e_wb/02", .mod_dir = "buildings/europe/summer/e_wb", .facing = 1 },
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
/// The Game's sun towards the ground (Data/consts.xml, (1, 1, -2)): where a
/// vehicle's shadow falls, and a full 3D object's at a turned step.
const game_sun_ground = [2]f64{ std.math.sqrt1_2, std.math.sqrt1_2 };

const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
const with_shadows = "effects,noise,warfog,haze,grid";
/// Anchor and rigidity: a pixel and a little, as a pixel's cut at the quarter
/// turns' 2:1 stretch falls either side.
const max_anchor_px: f64 = 2.0;
/// A shadow: the Game draws the shadow the turn shown casts under the world's
/// sun for the step (the darkness threshold cuts its soft rim and the ground's
/// grain a little), all of it joined to its object's picture, and it falls
/// the way the sun casts (the centroid of a shadow the picture half hides,
/// a strip along a wall, leans off the sun by up to 32 degrees).
const min_lit_shadow_iou: f64 = 0.8;
const min_attached: f64 = 0.95;
const max_sun_degrees: f64 = 35;

/// The turns `side` makes for an object, as camera-turn-mod makes them: the
/// turns lit by the 2003 light (`1,2,3`) and those lit by the world's sun at
/// facing 0 (`1l1,2l2,3l3`), the pictures the Game shows at every facing; and
/// the shadow turn f + s casts at camera step s under the Game's sun apart
/// from them (`<k>s<s>`, a vehicle's shadow), at the object's facing f (a prop
/// has none).
fn turnsFor(allocator: std.mem.Allocator, object: Object) ![]const u8 {
    var list: std.ArrayList(u8) = .empty;
    try list.appendSlice(allocator, "1,2,3,1l1,2l2,3l3");
    for (0..4) |s| {
        if (object.facing + s > 0) try list.print(allocator, ",{d}s{d}", .{ (object.facing + s) % 4, s });
    }
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
        try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, try turnsFor(allocator, object), turned, work });
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
        \\turn d casts under the Game's sun at step k (`<d>s<k>`; at step 0 and turn 0 the 2003 shadow) laid at the
        \\zero cross, IoU; the share of it joined to the picture; the way from the footprint's middle to its
        \\centroid off the world's sun as step k shows it (degrees; held where the shadow shows a quarter of
        \\its most or more); pixels. Reported: IoU against the step-0 shadow turned with the ground, centroids apart.
        \\
    );
    for (casting, 0..) |object, o| {
        const turned = try std.fs.path.join(allocator, &.{ data, object.mod_dir });
        try cwd.createDirPath(io, turned);
        try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, try turnsFor(allocator, object), turned, work });
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
            cast.* = shadowOf(.{ shadows[k], ground[k], solid[k] }, near, zero[k], foot, k, if (k == 0 and object.facing == 0) sun_ground else game_sun_ground);
            most = @max(most, cast.pixels);
        }
        const base = std.fs.path.basename(object.folder);
        for (0..4) |k| {
            const frames = [3][]const u8{ shadows[k], ground[k], solid[k] };
            const d = (object.facing + k) % 4;
            const lit = if (k == 0 and d == 0) object.folder else try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}_s{d}", .{ turned, base, d, k });
            const sprites = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}-lit{d}", .{ root, object.name, k });
            const expected: ?f64 = if (game_views.runTool(io, allocator, tool, &.{ "decompose", lit, sprites })) |_|
                packShadowIou(frames, near, zero[k], try game_views.decomposed(io, allocator, sprites, "1s"))
            else |_|
                null;
            const joined = try attached(allocator, frames, near, zero[k]);
            const held = casts[k].pixels >= units.min_shadow_pixels and 4 * casts[k].pixels >= most;
            const world = units.sunShadow(.{ shadows[0], ground[0], solid[0] }, zero[0], frames, zero[k], k, near);
            if (expected) |iou| {
                try report.print(allocator, "{s} facing {d} step {d} turn {d}: {d:.3}; {d:.3}; {d:.1}{s}; {d}; {d:.3}, {d:.1}\n", .{ object.name, object.facing, k, d, iou, joined, casts[k].off_sun, if (held) "" else " (not held)", casts[k].pixels, world.iou, world.off_degrees });
                // A shadow almost all under its object (not held) is a sliver
                // along the picture's rim; where the picture is the 2003 art its
                // soft rim darkens the ground as much (E_WB_02 at step 1: 0.71,
                // its 183 pixels a rim), so only a held shadow's IoU counts.
                if (iou < min_lit_shadow_iou and held) failed = true;
            } else {
                try report.print(allocator, "{s} facing {d} step {d} turn {d}: no shadow cast by the world's sun ({s}); {d:.3}; {d:.1}; {d}; {d:.3}, {d:.1}\n", .{ object.name, object.facing, k, d, lit, joined, casts[k].off_sun, casts[k].pixels, world.iou, world.off_degrees });
                failed = true;
            }
            if (joined < min_attached or (held and casts[k].off_sun > max_sun_degrees)) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a full 3D object's shadow is not cast from its foot by the world's sun (IoU floor {d} against the turn's shadow alone, joined {d}, within {d} degrees); the frames are in {s}\n", .{ report.items, min_lit_shadow_iou, min_attached, max_sun_degrees, root });
        return error.DrawnWrong;
    }
}

/// The models written by hand (docs/superpowers/specs/2026-11-02-hand-model-from-picture.md,
/// `camera-turn-hand-game-reads-it`): alone, at their own facings.
const handmade = [_]Object{
    .{ .name = "E_House01_1", .folder = "Data/Buildings/europe/summer/e_house01_1", .mod_dir = "buildings/europe/summer" },
    .{ .name = "E_House01_3", .folder = "Data/Buildings/europe/summer/e_house01_3", .mod_dir = "buildings/europe/summer" },
    .{ .name = "E_House01_1", .folder = "Data/Buildings/europe/summer/e_house01_1", .mod_dir = "buildings/europe/summer", .facing = 2 },
    .{ .name = "E_House01_3", .folder = "Data/Buildings/europe/summer/e_house01_3", .mod_dir = "buildings/europe/summer", .facing = 2 },
};

test "a hand model is what the Game draws at every step: step 0 is its render, not its 2003 picture" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-hand-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root_rel = "zig-out/local-test/camera-turn/hand";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), root_rel });
    cwd.deleteTree(io, root_rel) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    const map = try writeStandingMod(io, allocator, data, "Data/Maps/road3d.xml");
    const ground = try turn.runGame(io, allocator, game, root, "ground", "ground.xml", .{ shadow_look, shadow_look }, "ON", with_shadows, 4);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "object facing step turn pack: IoU / colour at offset; the 2003 picture there, IoU / colour; shadow IoU, joined, pixels\n");
    for (handmade, 0..) |object, o| {
        const base = std.fs.path.basename(object.folder);
        const fixture = try std.fmt.allocPrint(allocator, "tools/zig/fixtures/bk-views/hand-models/{s}.json", .{base});
        cwd.access(io, fixture, .{}) catch {
            try report.print(allocator, "{s}: no fixture, not run\n", .{base});
            continue;
        };
        const turned = try std.fs.path.join(allocator, &.{ data, object.mod_dir });
        try cwd.createDirPath(io, turned);
        // What the mod holds of a hand model: camera-turn-mod's `side`, turn 0 first.
        const final = try std.fmt.allocPrint(allocator, "{s}/{s}_r1", .{ turned, base });
        if (cwd.access(io, final, .{})) |_| {} else |_| try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, "0,1,2,3,1l1,2l2,3l3,0s1,0s2,0s3,1s0,1s1,1s2,1s3,2s0,2s1,2s2,2s3,3s0,3s1,3s2,3s3", turned, work });
        var foot = try footprintMiddle(io, allocator, tool, object.folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{s}", .{ root, base }));
        for (0..object.facing) |_| foot = .{ -foot[1], foot[0] };
        const place = try placeStanding(io, allocator, data, map, object, o, foot, shadow_middle_ai, .{ 240, -200 });
        const label = try std.fmt.allocPrint(allocator, "{s}-f{d}", .{ object.name, object.facing });
        const name = try std.fmt.allocPrint(allocator, "standing{d}.xml", .{o});
        const shots = try turn.runGame(io, allocator, game, root, label, name, .{ shadow_look, shadow_look }, "ON", objects_only, 4);
        var solid: [4][]const u8 = undefined;
        for (&solid, shots) |*s, shot| s.* = try solidShot(allocator, shot);
        const shadows = try turn.runGame(io, allocator, game, root, try std.fmt.allocPrint(allocator, "{s}-shadows", .{label}), name, .{ shadow_look, shadow_look }, "ON", with_shadows, 4);
        const old = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}-2003", .{ root, base });
        _ = try game_views.decompose(io, allocator, tool, object.folder, old);
        const picture_2003 = try game_views.decomposed(io, allocator, old, "1");
        for (0..4) |k| {
            const d = (object.facing + k) % 4;
            // CameraTurn.h's LitModelPath: the turn under the world's sun where the
            // mod has it (facing 0), else the turn's own, at turn 0 the variant's own.
            const folder = if (d == 0) try std.fmt.allocPrint(allocator, "{s}", .{base}) else if (k == 0 or k != d) try std.fmt.allocPrint(allocator, "{s}_r{d}", .{ base, d }) else try std.fmt.allocPrint(allocator, "{s}_r{d}_l{d}", .{ base, d, k });
            const out = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}-{d}", .{ root, label, k });
            _ = try game_views.decompose(io, allocator, tool, try std.fs.path.join(allocator, &.{ turned, folder }), out);
            const sprite = try game_views.decomposed(io, allocator, out, "1");
            const where = turn.turnScreen(place, k);
            const own = game_views.score(shots[k], sprite, where);
            const was = game_views.score(shots[k], picture_2003, where);
            const frames = [3][]const u8{ shadows[k], ground[k], solid[k] };
            const near = nearBox(shots[k], where, 90);
            const cast = if (k == 0 and d == 0) folder else try std.fmt.allocPrint(allocator, "{s}_r{d}_s{d}", .{ base, d, k });
            const cast_out = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}-cast{d}", .{ root, label, k });
            try game_views.runTool(io, allocator, tool, &.{ "decompose", try std.fs.path.join(allocator, &.{ turned, cast }), cast_out });
            const shadow_iou = packShadowIou(frames, near, where, try game_views.decomposed(io, allocator, cast_out, "1s"));
            const joined = try attached(allocator, frames, near, where);
            const pixels = shadowOf(frames, near, where, foot, k, game_sun_ground).pixels;
            try report.print(allocator, "{s} {d} {d} {d} {s}: {d:.3} / {d:.2} at {d},{d}; {d:.3} / {d:.2}; {d:.3}, {d:.3}, {d}\n", .{ base, object.facing, k, d, folder, own.iou, own.colour, own.dx, own.dy, was.iou, was.colour, shadow_iou, joined, pixels });
            if (own.iou < min_iou or own.colour > max_colour or @abs(own.dx) > 1 or @abs(own.dy) > 1) failed = true;
            // Turn 0 must be the render, and not fall back on the 2003 files.
            if (d == 0 and was.colour <= own.colour) failed = true;
            if (joined < min_attached or (pixels >= units.min_shadow_pixels and shadow_iou < min_lit_shadow_iou)) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the Game does not draw a hand model as its packs hold it (IoU floor {d}, colour ceiling {d}); the frames are in {s}\n", .{ report.items, min_iou, max_colour, root });
        return error.DrawnWrong;
    }
}

/// The buildings shown by their kit views (`camera-turn-kit-game-reads-it`;
/// tools/zig/fixtures/bk-views/kit-views, tools/blender/README.md), each alone:
/// E_House01_1, which also has a hand model (the kit view wins, turn 0
/// included); A_H07_1 at own facing 1, whose zero cross lies outside three of
/// its pictures; and the warehouse E_WB_01 with its HP at 0.3, which shows its
/// damaged state over its rubble.
const kit_shown = [_]Object{
    .{ .name = "E_House01_1", .folder = "Data/Buildings/europe/summer/e_house01_1", .mod_dir = "buildings/europe/summer" },
    .{ .name = "A_H07_1", .folder = "Data/Buildings/africa/summer/a_h07_1", .mod_dir = "buildings/africa/summer", .facing = 1 },
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01", .mod_dir = "buildings/europe/summer/e_wb", .hp = "0.3", .state = "2" },
};
const kit_views_dir = "tools/zig/fixtures/bk-views/kit-views";

test "kit views are what the Game draws at every step: each turn's fixture picture at its zero cross, not the turn bk-views side drew" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-kit-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root_rel = "zig-out/local-test/camera-turn/kit";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), root_rel });
    cwd.deleteTree(io, root_rel) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    const map = try writeStandingMod(io, allocator, data, "Data/Maps/road3d.xml");

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "object facing state step turn: the kit view there, IoU / colour at offset; the picture bk-views side drew for that turn (at turn 0 the 2003 or hand picture) there, IoU / colour\n");
    for (kit_shown, 0..) |object, o| {
        const base = std.fs.path.basename(object.folder);
        const kit_dir = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ kit_views_dir, object.folder["Data/Buildings/".len..] });
        const hand = if (cwd.access(io, try std.fmt.allocPrint(allocator, "tools/zig/fixtures/bk-views/hand-models/{s}.json", .{base}), .{})) |_| true else |_| false;
        // What camera-turn-mod makes: the turns `side` draws (a hand model's turn 0
        // too), then the kit views laid over them in the mod's folder.
        const turned = try std.fs.path.join(allocator, &.{ root, "turned", object.name });
        try cwd.createDirPath(io, turned);
        try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, if (hand) "0,1,2,3,1l1,2l2,3l3" else "1,2,3,1l1,2l2,3l3", turned, work });
        const shown = try std.fs.path.join(allocator, &.{ data, object.mod_dir });
        try game_views.runTool(io, allocator, tool, &.{ "kit", kit_dir, object.folder, turned, shown });

        var foot = try footprintMiddle(io, allocator, tool, object.folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{s}", .{ root, base }));
        for (0..object.facing) |_| foot = .{ -foot[1], foot[0] };
        // placeStanding's item with the object's HP.
        var d: [2]i32 = undefined;
        for (&d, foot, [2]i32{ 240, -200 }) |*v, f, a| v.* = 4 * @as(i32, @intFromFloat(@round((@as(f64, @floatFromInt(a)) - f * std.math.sqrt2) / 4)));
        const map_name = try std.fmt.allocPrint(allocator, "kit{d}.xml", .{o});
        try cwd.writeFile(io, .{
            .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", map_name }),
            .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", try std.fmt.allocPrint(allocator, "<Objects><item Direction=\"{d}\" Player=\"-1\" ScriptID=\"-1\" HP=\"{s}\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
                "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"1\" Intention=\"0\" LinkWith=\"0\"/></item></Objects>", .{ object.facing * 16384, object.hp, object.name, shadow_middle_ai + d[0], shadow_middle_ai + d[1] })),
        });
        const place = [2]i32{ turn.centre[0] + @divExact(d[0] + d[1], 2), turn.centre[1] + @divExact(d[0] - d[1], 4) };
        const label = try std.fmt.allocPrint(allocator, "{s}-f{d}", .{ object.name, object.facing });
        const shots = try turn.runGame(io, allocator, game, root, label, map_name, .{ shadow_look, shadow_look }, "ON", objects_only, 4);

        for (0..4) |k| {
            const t = (object.facing + k) % 4;
            // CameraTurn.h's LitModelPath: the folder the Game takes the turn from
            // under light index k (the sun where 2003 had it: the step).
            const folder = if (k != 0) try std.fmt.allocPrint(allocator, "{s}_r{d}_l{d}", .{ base, t, k }) else if (t == 0) base else try std.fmt.allocPrint(allocator, "{s}_r{d}", .{ base, t });
            // The fixture picture of the turn under that light, its zero cross
            // as views.json gives it, over the rubble of the folder shown (the
            // state's ground picture, which the kit views leave as it was and
            // the Game draws with the objects).
            const view = try kitView(io, allocator, kit_dir, object.state, t, k, "l");
            const out = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}-{d}", .{ root, label, k });
            try game_views.runTool(io, allocator, tool, &.{ "decompose", try std.fs.path.join(allocator, &.{ shown, folder }), out });
            const ground: ?game_views.Sprite = game_views.decomposed(io, allocator, out, try std.fmt.allocPrint(allocator, "{s}g", .{object.state})) catch null;
            const expected = try overGround(allocator, view, ground);
            // What the Game would draw without the kit views: the turn `side`
            // drew, lit for the step where it drew that (`_r<k>_l<k>`).
            const side_folder = if (t == 0) base else if (k == 0 or k != t) try std.fmt.allocPrint(allocator, "{s}_r{d}", .{ base, t }) else folder;
            const drew_folder = if (t == 0 and !hand) object.folder else try std.fs.path.join(allocator, &.{ turned, side_folder });
            const drew_out = try std.fmt.allocPrint(allocator, "{s}/sprites/{s}-{d}-side", .{ root, label, k });
            try game_views.runTool(io, allocator, tool, &.{ "decompose", drew_folder, drew_out });
            const drew = try onGround(io, allocator, drew_out, object.state);

            const where = turn.turnScreen(place, k);
            const own = game_views.score(shots[k], expected, where);
            const other = game_views.score(shots[k], drew, where);
            try report.print(allocator, "{s} {d} {s} {d} {d}: {d:.3} / {d:.2} at {d},{d}; {d:.3} / {d:.2}\n", .{ base, object.facing, object.state, k, t, own.iou, own.colour, own.dx, own.dy, other.iou, other.colour });
            if (own.iou < min_iou or own.colour > max_colour or @abs(own.dx) > 1 or @abs(own.dy) > 1) failed = true;
            // The floors must tell the two apart: the turn `side` drew fails them there.
            if (other.iou >= min_iou and other.colour <= max_colour) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the Game does not draw a kit view as its fixture holds it, or the picture bk-views side drew passes for it (IoU floor {d}, colour ceiling {d}, within a pixel); the frames are in {s}\n", .{ report.items, min_iou, max_colour, root });
        return error.DrawnWrong;
    }
}

/// The house the sun tests stand, by its kit views: E_House01_1, which has
/// every turn under every light and a shadow of its own for each.
const sun_house = Object{ .name = "E_House01_1", .folder = "Data/Buildings/europe/summer/e_house01_1", .mod_dir = "buildings/europe/summer" };
/// MC_SHOW_TERRAIN and MC_SHOW_SHADOWS (GameTT/iMission.h): a schedule's
/// `msg=` turns the two passes off and on between shots, so one run of the
/// Game shoots a mission's one sun at each step with the ground and its
/// shadows and then with its objects alone.
const show_terrain = "2097156";
const show_shadows = "2097157";
/// The passes off in every shot of the sun tests.
const sun_passes_off = "effects,noise,warfog,border,haze,grid";

/// A kit view of the house: turn `t` under light index `l`, its picture
/// (`kind` "l") or the shadow its model casts ("s"), the zero cross as
/// views.json gives it.
fn kitView(io: std.Io, allocator: std.mem.Allocator, kit_dir: []const u8, state: []const u8, t: usize, l: usize, kind: []const u8) !game_views.Sprite {
    const cwd = std.Io.Dir.cwd();
    const views_json = try cwd.readFileAlloc(io, try std.fmt.allocPrint(allocator, "{s}/views.json", .{kit_dir}), allocator, .limited(1 << 20));
    const of_turn = (try std.json.parseFromSliceLeaky(std.json.Value, allocator, views_json, .{})).object.get("states").?.object.get(state).?.object.get(try std.fmt.allocPrint(allocator, "{d}", .{t})).?.object;
    const placed = try std.json.parseFromValueLeaky(struct { zero: [2]i32 }, allocator, of_turn.get(kind).?.object.get(try std.fmt.allocPrint(allocator, "{d}", .{l})).?, .{ .ignore_unknown_fields = true });
    const picture = try views.readRgbaPng(allocator, try cwd.readFileAlloc(io, try std.fmt.allocPrint(allocator, "{s}/{s}_{d}_{s}{d}.png", .{ kit_dir, state, t, kind, l }), allocator, .limited(64 << 20)));
    return .{ .image = picture, .anchor = placed.zero };
}

/// The scratch mod of a sun test: road3d as its ground map, and the house's
/// kit views laid over the turns `side` drew, as camera-turn-mod makes them
/// (`kit` makes the shadows alone from the kit views' own).
const SunMod = struct { root: []const u8, data: []const u8, kit_dir: []const u8, map: []const u8, foot: [2]f64 };

fn sunMod(io: std.Io, allocator: std.mem.Allocator, tool: []const u8, root_rel: []const u8) !SunMod {
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), root_rel });
    cwd.deleteTree(io, root_rel) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const map = try writeStandingMod(io, allocator, data, "Data/Maps/road3d.xml");
    const kit_dir = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ kit_views_dir, sun_house.folder["Data/Buildings/".len..] });
    const turned = try std.fs.path.join(allocator, &.{ root, "turned" });
    try cwd.createDirPath(io, turned);
    try game_views.runTool(io, allocator, tool, &.{ "side", sun_house.folder, "0,1,2,3,1l1,2l2,3l3", turned, try std.fs.path.join(allocator, &.{ root, "work" }) });
    try game_views.runTool(io, allocator, tool, &.{ "kit", kit_dir, sun_house.folder, turned, try std.fs.path.join(allocator, &.{ data, sun_house.mod_dir }) });
    const foot = try footprintMiddle(io, allocator, tool, sun_house.folder, try std.fs.path.join(allocator, &.{ root, "sprites", "foot" }));
    return .{ .root = root, .data = data, .kit_dir = kit_dir, .map = map, .foot = foot };
}

/// maps/<name> with one object alone, `what` at `direction` (a house: its
/// footprint's middle `foot`, world units from its zero cross as it stands,
/// put on the spot; a unit: `foot` zero), 240, -200 AI units from the point
/// the shots look at; returns its zero cross on the screen at step 0.
fn sunMap(io: std.Io, allocator: std.mem.Allocator, mod: SunMod, name: []const u8, what: []const u8, player: i32, direction: usize, foot: [2]f64) ![2]i32 {
    var d: [2]i32 = undefined;
    for (&d, foot, [2]i32{ 240, -200 }) |*v, f, a| v.* = 4 * @as(i32, @intFromFloat(@round((@as(f64, @floatFromInt(a)) - f * std.math.sqrt2) / 4)));
    try std.Io.Dir.cwd().writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ mod.data, "maps", name }),
        .data = try std.mem.replaceOwned(u8, allocator, mod.map, "<Objects/>", try std.fmt.allocPrint(allocator, "<Objects><item Direction=\"{d}\" Player=\"{d}\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"1\" Intention=\"0\" LinkWith=\"0\"/></item></Objects>", .{ direction, player, what, shadow_middle_ai + d[0], shadow_middle_ai + d[1] })),
    });
    return .{ turn.centre[0] + @divExact(d[0] + d[1], 2), turn.centre[1] + @divExact(d[0] - d[1], 4) };
}

/// The shots of one mission under one sun: the four camera steps with the
/// ground and the shadows (`lit`), and each with the objects alone.
const SunShots = struct { lit: [4][]u8, alone: [4][]u8, log: []const u8 };

/// Puts Remastered on in the profile of the scratch user "on", so that the
/// missions run with it start with the option on: the sun's turn is picked as
/// a mission starts, and only then (Main/SunTurn.h).
fn sunOption(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8) !void {
    _ = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, "option", "", "60:set=GamePlay.Remastered=ON,120:exit", sun_passes_off, .{ .user = "on" });
}

/// Runs the Game on `map` with the sun forced to turn `sun` (BK_SUN_TURN), as
/// the user `user`, and at each of the four camera steps shoots the scene,
/// takes the terrain and the shadows off, shoots the objects alone and puts
/// the two passes back. Both shots of a step are of one view: the camera's
/// bounds move its anchor by a pixel or two at some steps this near the
/// map's corner (297 becomes 295.6 at step 3, and stays), so a second round of
/// turns would not stand where the first stood. The cursor is parked anew at
/// every step (`hover` holds it for 600 frames; let go on the window's edge it
/// scrolls the view). `paused`: the game is paused before the first shot
/// (SPACE), so that a unit stands as it stood in every shot; the waits then
/// count the timer's absolute time.
fn sunRun(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, name: []const u8, map: []const u8, sun: usize, user: []const u8, paused: bool) !SunShots {
    const wait = if (paused) "t" else "g";
    const toggle = try std.fmt.allocPrint(allocator, ",{s}10:msg=" ++ show_terrain ++ ",{s}10:msg=" ++ show_shadows, .{ wait, wait });
    var schedule: std.ArrayList(u8) = .empty;
    try schedule.print(allocator, "g1000:hover=1000x400,g10:camera={d}x{d}", .{ shadow_look, shadow_look });
    if (paused) try schedule.appendSlice(allocator, ",g1000:key=SPACE");
    for (0..4) |k| {
        if (k != 0) try schedule.print(allocator, ",{s}10:hover=1000x400,{s}10:key=COMMA", .{ wait, wait });
        try schedule.print(allocator, ",{s}{d}:shot{s},{s}{d}:shot{s}", .{ wait, 1500 + k, toggle, wait, 700 + k, toggle });
    }
    try schedule.print(allocator, ",{s}200:exit", .{wait});
    const run = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, name, map, schedule.items, sun_passes_off, .{ .user = user, .env = &.{.{ "BK_SUN_TURN", try std.fmt.allocPrint(allocator, "{d}", .{sun}) }} });
    var shots: SunShots = .{ .lit = undefined, .alone = undefined, .log = run.log };
    for (0..4) |k| {
        shots.lit[k] = try control.readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "{s}{d}", .{ wait, 1500 + k }), root, try std.fmt.allocPrint(allocator, "{s}-lit-step{d}", .{ name, k }));
        shots.alone[k] = try control.readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "{s}{d}", .{ wait, 700 + k }), root, try std.fmt.allocPrint(allocator, "{s}-alone-step{d}", .{ name, k }));
    }
    return shots;
}

/// The sun turn a mission's log says it started under (BK_MAP_TRACE), -1
/// where it says none or several.
fn tracedSun(allocator: std.mem.Allocator, log: []const u8) !i32 {
    const traced = try control.lines(allocator, log, "BK_MAP_TRACE: sun turn ");
    return if (traced.len == 1 and traced[0].len > 0) std.fmt.parseInt(i32, traced[0][0..1], 10) catch -1 else -1;
}

/// The way the Game's sun casts on the ground under sun turn `sun`: the 2003
/// sun's turned by (x, y) -> (-y, x) that many times.
fn sunWay(sun: usize) [2]f64 {
    var way = game_sun_ground;
    for (0..sun) |_| {
        const x = way[0];
        way = .{ -way[1], x };
    }
    return way;
}

/// Where an object's shadow lies from the middle of its footprint, on the
/// ground of the world (not of the screen's step): the mean of its shadow's
/// pixels in sight, turned back by the step, and how many show.
const CastWay = struct { way: [2]f64, pixels: usize };

fn castWay(frames: [3][]const u8, box: [4]i32, zero: [2]i32, foot: [2]f64, k: usize) CastWay {
    const middle = turn.turnOffset(.{ (foot[0] + foot[1]) / std.math.sqrt2, (foot[0] - foot[1]) / (2 * std.math.sqrt2) }, k);
    var sum = [2]f64{ 0, 0 };
    var pixels: usize = 0;
    var y = turn.clear_rows[0];
    while (y < turn.clear_rows[1]) : (y += 1) {
        var x: i32 = 0;
        while (x < turn.width) : (x += 1) {
            if (!inBox(box, zero, x, y) or !isShadow(frames, x, y)) continue;
            pixels += 1;
            sum = .{ sum[0] + @as(f64, @floatFromInt(x - zero[0])) - middle[0], sum[1] + @as(f64, @floatFromInt(y - zero[1])) - middle[1] };
        }
    }
    if (pixels == 0) return .{ .way = .{ 0, 0 }, .pixels = 0 };
    // The screen offset at step 0 that the step shows there, then the ground
    // under it: screen (sx, sy) is world ((sx + 2 sy), (sx - 2 sy)) / sqrt 2.
    const s = turn.unturn(.{ sum[0] / @as(f64, @floatFromInt(pixels)), sum[1] / @as(f64, @floatFromInt(pixels)) }, k);
    return .{ .way = .{ (s[0] + 2 * s[1]) / std.math.sqrt2, (s[0] - 2 * s[1]) / std.math.sqrt2 }, .pixels = pixels };
}

/// The same over the four camera steps together. Each step hides another part
/// of the shadow behind its object (a tank's is a rim round its hull, of which
/// a step shows the near side), which leans the mean of one step's pixels
/// towards the viewer by tens of degrees. Under one sun the shadow is one
/// shape on the world's ground: every step's pixels are brought back to it
/// (cells of 2 world units) and the mean is of their union.
fn worldCastWay(allocator: std.mem.Allocator, frames: [4][3][]const u8, boxes: [4][4]i32, zeros: [4][2]i32, foot: [2]f64) !CastWay {
    var cells: std.AutoHashMap([2]i32, void) = .init(allocator);
    defer cells.deinit();
    for (0..4) |k| {
        var y = turn.clear_rows[0];
        while (y < turn.clear_rows[1]) : (y += 1) {
            var x: i32 = 0;
            while (x < turn.width) : (x += 1) {
                if (!inBox(boxes[k], zeros[k], x, y) or !isShadow(frames[k], x, y)) continue;
                const s = turn.unturn(.{ @floatFromInt(x - zeros[k][0]), @floatFromInt(y - zeros[k][1]) }, k);
                try cells.put(.{ @intFromFloat(@round((s[0] + 2 * s[1]) / (2 * std.math.sqrt2))), @intFromFloat(@round((s[0] - 2 * s[1]) / (2 * std.math.sqrt2))) }, {});
            }
        }
    }
    if (cells.count() == 0) return .{ .way = .{ 0, 0 }, .pixels = 0 };
    var sum = [2]f64{ 0, 0 };
    var it = cells.keyIterator();
    while (it.next()) |cell| sum = .{ sum[0] + 2 * @as(f64, @floatFromInt(cell[0])), sum[1] + 2 * @as(f64, @floatFromInt(cell[1])) };
    const n: f64 = @floatFromInt(cells.count());
    return .{ .way = .{ sum[0] / n - foot[0], sum[1] / n - foot[1] }, .pixels = cells.count() };
}

/// The angle between two ways on the ground, degrees.
fn between(a: [2]f64, b: [2]f64) f64 {
    var angle = std.math.radiansToDegrees(@abs(std.math.atan2(a[1], a[0]) - std.math.atan2(b[1], b[0])));
    if (angle > 180) angle = 360 - angle;
    return angle;
}

/// A picture under another light must not pass for the one drawn: its colour
/// is at least this far off (the lights move the lit side round the house).
const min_other_light_colour: f64 = 4.0;
/// The way a shadow falls is read off the mean of its pixels in sight, which
/// says nothing where the house hides its shadow but for slivers at its sides
/// (the light from the viewer's back: 250 to 600 pixels of some 4000): it is
/// held to the sun's way where at least this many show.
const min_cast_pixels: usize = 1000;

test "one sun for the mission: at every sun turn and camera step the house is its kit view of that turn under light step + sun, and casts that view's shadow" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-kit-sun-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const mod = try sunMod(io, allocator, tool, "zig-out/local-test/camera-turn/kit-sun");
    const root = mod.root;
    // The house at own facing 2: its turn is the step plus two, its light the
    // step plus the sun's turn, so neither is the other nor the step.
    const facing = 2;
    var foot = mod.foot;
    for (0..facing) |_| {
        const x = foot[0];
        foot = .{ -foot[1], x };
    }
    const place = try sunMap(io, allocator, mod, "house.xml", sun_house.name, -1, facing * 16384, foot);
    try sunOption(io, allocator, game, root);
    const ground = try sunRun(io, allocator, game, root, "ground", "ground.xml", 0, "on", false);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "sun step turn light: the kit view there, IoU / colour at offset; the nearest colour of the turn's view under another light; " ++
        "shadow: IoU with the view's shadow, the best IoU with another light's, joined, pixels, degrees off the turned sun's way\n");
    for (0..4) |sun| {
        const shots = try sunRun(io, allocator, game, root, try std.fmt.allocPrint(allocator, "house-sun{d}", .{sun}), "house.xml", sun, "on", false);
        const traced = try tracedSun(allocator, shots.log);
        if (traced != @as(i32, @intCast(sun))) {
            try report.print(allocator, "sun {d}: the mission's log says sun turn {d}\n", .{ sun, traced });
            failed = true;
        }
        for (0..4) |k| {
            const t = (facing + k) % 4;
            const l = (k + sun) % 4;
            const where = turn.turnScreen(place, k);
            const own = game_views.score(shots.alone[k], try kitView(io, allocator, mod.kit_dir, "1", t, l, "l"), where);
            const frames = [3][]const u8{ shots.lit[k], ground.lit[k], try solidShot(allocator, shots.alone[k]) };
            const near = nearBox(shots.alone[k], where, 90);
            const shadow_iou = packShadowIou(frames, near, where, try kitView(io, allocator, mod.kit_dir, "1", t, l, "s"));
            var other_colour: f64 = std.math.inf(f64);
            var other_shadow: f64 = 0;
            for (0..4) |other| {
                if (other == l) continue;
                other_colour = @min(other_colour, game_views.score(shots.alone[k], try kitView(io, allocator, mod.kit_dir, "1", t, other, "l"), where).colour);
                other_shadow = @max(other_shadow, packShadowIou(frames, near, where, try kitView(io, allocator, mod.kit_dir, "1", t, other, "s")));
            }
            const joined = try attached(allocator, frames, near, where);
            const cast = shadowOf(frames, near, where, foot, k, sunWay(sun));
            try report.print(allocator, "{d} {d} {d} {d}: {d:.3} / {d:.2} at {d},{d}; {d:.2}; {d:.3}, {d:.3}, {d:.3}, {d}, {d:.1}\n", .{ sun, k, t, l, own.iou, own.colour, own.dx, own.dy, other_colour, shadow_iou, other_shadow, joined, cast.pixels, cast.off_sun });
            if (own.iou < min_iou or own.colour > max_colour or @abs(own.dx) > 1 or @abs(own.dy) > 1) failed = true;
            if (other_colour < min_other_light_colour) failed = true;
            if (cast.pixels >= units.min_shadow_pixels and (shadow_iou < min_lit_shadow_iou or other_shadow >= shadow_iou or joined < min_attached)) failed = true;
            if (cast.pixels >= min_cast_pixels and cast.off_sun > max_sun_degrees) failed = true;
        }
    }
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the Game does not draw the house and its shadow for the light the step and the sun's turn give (IoU floor {d}, colour ceiling {d}, another light's colour at least {d}, shadow IoU floor {d}, within {d} degrees of the sun where {d} pixels of it show); the frames are in {s}\n", .{ report.items, min_iou, max_colour, min_other_light_colour, min_lit_shadow_iou, max_sun_degrees, min_cast_pixels, root });
        return error.DrawnWrong;
    }
}

/// MAIN_COMMAND_SAVE and MAIN_COMMAND_LOAD (Main/iMainClassIDs.h).
const save_command = "268501012";
const load_command = "268501013";

test "the sun's turn is the mission's: a tank's shadow falls as the house's, a save keeps the sun, and without Remastered the sun is where 2003 had it" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-sun-state-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const mod = try sunMod(io, allocator, tool, "zig-out/local-test/camera-turn/sun-state");
    const root = mod.root;
    const place = try sunMap(io, allocator, mod, "house.xml", sun_house.name, -1, 0, mod.foot);
    const tank_place = try sunMap(io, allocator, mod, "tank.xml", "T-34", 0, 0, .{ 0, 0 });
    try sunOption(io, allocator, game, root);
    const ground = try sunRun(io, allocator, game, root, "ground", "ground.xml", 0, "on", true);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;

    // A mesh's shadow under a turned sun: the T-34's and the house's, each
    // alone on the same spot under sun turn 1, at every camera step, the game
    // paused so that the tank stands as it stood in both rounds of shots.
    {
        const sun = 1;
        const house = try sunRun(io, allocator, game, root, "house-sun1", "house.xml", sun, "on", true);
        const tank = try sunRun(io, allocator, game, root, "tank-sun1", "tank.xml", sun, "on", true);
        var house_frames: [4][3][]const u8 = undefined;
        var tank_frames: [4][3][]const u8 = undefined;
        var house_box: [4][4]i32 = undefined;
        var tank_box: [4][4]i32 = undefined;
        var house_at: [4][2]i32 = undefined;
        var tank_at: [4][2]i32 = undefined;
        try report.appendSlice(allocator, "sun 1, step: the house's shadow pixels in sight, their mean from its footprint's middle in degrees off the turned sun's way (off the 2003 sun's); the T-34's from its place, the same\n");
        for (0..4) |k| {
            house_at[k] = turn.turnScreen(place, k);
            tank_at[k] = turn.turnScreen(tank_place, k);
            house_frames[k] = .{ house.lit[k], ground.lit[k], try solidShot(allocator, house.alone[k]) };
            tank_frames[k] = .{ tank.lit[k], ground.lit[k], try solidShot(allocator, tank.alone[k]) };
            house_box[k] = nearBox(house.alone[k], house_at[k], 90);
            tank_box[k] = nearBox(tank.alone[k], tank_at[k], 60);
            const h = castWay(house_frames[k], house_box[k], house_at[k], mod.foot, k);
            const t = castWay(tank_frames[k], tank_box[k], tank_at[k], .{ 0, 0 }, k);
            try report.print(allocator, "  {d}: {d} px, {d:.1} ({d:.1}); {d} px, {d:.1} ({d:.1})\n", .{ k, h.pixels, between(h.way, sunWay(sun)), between(h.way, sunWay(0)), t.pixels, between(t.way, sunWay(sun)), between(t.way, sunWay(0)) });
        }
        // One step's mean leans towards the viewer; what is held is the mean
        // of the shadow the four steps show together (worldCastWay).
        const h = try worldCastWay(allocator, house_frames, house_box, house_at, mod.foot);
        const t = try worldCastWay(allocator, tank_frames, tank_box, tank_at, .{ 0, 0 });
        try report.print(allocator, "  the four steps together: the house's shadow {d} cells, {d:.1} degrees off the turned sun's way ({d:.1} off the 2003 sun's); the T-34's {d} cells, {d:.1} ({d:.1}); {d:.1} degrees between the two\n", .{
            h.pixels, between(h.way, sunWay(sun)), between(h.way, sunWay(0)), t.pixels, between(t.way, sunWay(sun)), between(t.way, sunWay(0)), between(h.way, t.way),
        });
        if (h.pixels < units.min_shadow_pixels or t.pixels < units.min_shadow_pixels) failed = true;
        if (between(h.way, sunWay(sun)) > max_sun_degrees or between(t.way, sunWay(sun)) > max_sun_degrees or between(h.way, t.way) > max_sun_degrees) failed = true;
    }

    // A save keeps the sun: saved under sun turn 2 at camera step 1, loaded by
    // a Game started with another turn forced. Then the option is taken off in
    // the saved mission: the house's turn stays, its light goes back to the
    // step's.
    {
        const look = try std.fmt.allocPrint(allocator, "g1000:hover=1000x400,g10:camera={d}x{d}", .{ shadow_look, shadow_look });
        const saved = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, "save", "house.xml", try std.fmt.allocPrint(allocator, "{s},g10:key=SPACE,t10:key=COMMA,t1500:shot,t10:cmdc=" ++ save_command ++ "=sun.sav;0," ++
            "t1500:set=GamePlay.Remastered=OFF,t1501:shot,t10:set=GamePlay.Remastered=ON,t500:exit", .{look}), objects_only, .{ .user = "on", .env = &.{.{ "BK_SUN_TURN", "2" }} });
        const loaded = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, "load", "", "g500:set=GamePlay.Remastered=ON,60:cmdc=" ++ load_command ++ "=sun.sav,t2000:shot,t10:key=COMMA,t1502:shot,t500:exit", objects_only, .{ .user = "on", .env = &.{.{ "BK_SUN_TURN", "1" }} });
        const Shot = struct { name: []const u8, frame: []const u8, step: usize, t: usize, l: usize };
        const shots = [_]Shot{
            .{ .name = "saved under sun 2 at step 1", .frame = try control.readShot(io, allocator, saved.user, "t1500", root, "saved-sun2-step1"), .step = 1, .t = 1, .l = 3 },
            .{ .name = "the option taken off there", .frame = try control.readShot(io, allocator, saved.user, "t1501", root, "saved-option-off-step1"), .step = 1, .t = 1, .l = 1 },
            .{ .name = "loaded with sun 1 forced", .frame = try control.readShot(io, allocator, loaded.user, "t2000", root, "loaded-step1"), .step = 1, .t = 1, .l = 3 },
            .{ .name = "turned on to step 2", .frame = try control.readShot(io, allocator, loaded.user, "t1502", root, "loaded-step2"), .step = 2, .t = 2, .l = 0 },
        };
        const saved_sun = try control.lines(allocator, saved.log, "BK_MAP_TRACE: sun turn ");
        const loaded_sun = try control.lines(allocator, loaded.log, "BK_MAP_TRACE: sun turn ");
        try report.print(allocator, "save: the mission started under sun turn {s}; the load says sun turn {s}\n", .{ if (saved_sun.len == 1) saved_sun[0] else "?", if (loaded_sun.len == 1) loaded_sun[0] else "?" });
        if (saved_sun.len != 1 or !std.mem.eql(u8, saved_sun[0], "2") or loaded_sun.len != 1 or !std.mem.eql(u8, loaded_sun[0], "2 (loaded)")) failed = true;
        try report.appendSlice(allocator, "  shot: turn light: the kit view there, IoU / colour at offset; the nearest colour of the turn's view under another light\n");
        for (shots) |shot| {
            const where = turn.turnScreen(place, shot.step);
            const own = game_views.score(shot.frame, try kitView(io, allocator, mod.kit_dir, "1", shot.t, shot.l, "l"), where);
            var other_colour: f64 = std.math.inf(f64);
            for (0..4) |other| {
                if (other != shot.l) other_colour = @min(other_colour, game_views.score(shot.frame, try kitView(io, allocator, mod.kit_dir, "1", shot.t, other, "l"), where).colour);
            }
            try report.print(allocator, "  {s}: {d} {d}: {d:.3} / {d:.2} at {d},{d}; {d:.2}\n", .{ shot.name, shot.t, shot.l, own.iou, own.colour, own.dx, own.dy, other_colour });
            if (own.iou < min_iou or own.colour > max_colour or @abs(own.dx) > 1 or @abs(own.dy) > 1 or other_colour < min_other_light_colour) failed = true;
        }
    }

    // The Game started again begins under another sun than the mission before
    // it had: the player's profile holds that mission's turn (the var
    // Remastered.SunTurn.Last of its config.cfg). The last mission this
    // profile started was the saved one's, under sun turn 2 (the load starts
    // none). Two more Games, each started anew with nothing forced and the
    // number 1 in place of the machine's random one (BK_SUN_RANDOM): after
    // turn 2 that number picks 0, after 0 it picks 2. A Game that did not know
    // the mission before would pick 1 both times.
    {
        const unforced: []const [2][]const u8 = &.{ .{ "BK_SUN_TURN", "-" }, .{ "BK_SUN_RANDOM", "1" } };
        const first = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, "again-1", "house.xml", "g300:exit", objects_only, .{ .user = "on", .env = unforced });
        const second = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, "again-2", "house.xml", "g300:exit", objects_only, .{ .user = "on", .env = unforced });
        const picks = [2]i32{ try tracedSun(allocator, first.log), try tracedSun(allocator, second.log) };
        const config = try std.Io.Dir.cwd().readFileAlloc(io, try std.fs.path.join(allocator, &.{ second.user, "profiles", "Turn", "config.cfg" }), allocator, .limited(4 << 20));
        const held = std.mem.indexOf(u8, config, "<item data=\"2\"><key>Remastered.SunTurn.Last</key>") != null;
        try report.print(allocator, "started again after sun turn 2, the random number 1 both times: sun turn {d}, then {d} (0, then 2: never the turn before; 1 and 1 without the profile's memory); the profile holds the last, 2: {}\n", .{ picks[0], picks[1], held });
        if (picks[0] != 0 or picks[1] != 2 or !held) failed = true;
    }

    // Without Remastered the sun is where 2003 had it, whatever is forced: a
    // user whose profile never had the option, sun turn 2 forced.
    {
        const off = try sunRun(io, allocator, game, root, "option-off-sun2", "house.xml", 2, "off", false);
        const traced = try tracedSun(allocator, off.log);
        const where = place;
        const own = game_views.score(off.alone[0], try kitView(io, allocator, mod.kit_dir, "1", 0, 0, "l"), where);
        const forced = game_views.score(off.alone[0], try kitView(io, allocator, mod.kit_dir, "1", 0, 2, "l"), where);
        const frames = [3][]const u8{ off.lit[0], ground.lit[0], try solidShot(allocator, off.alone[0]) };
        const near = nearBox(off.alone[0], where, 90);
        const shadow_iou = packShadowIou(frames, near, where, try kitView(io, allocator, mod.kit_dir, "1", 0, 0, "s"));
        const forced_shadow = packShadowIou(frames, near, where, try kitView(io, allocator, mod.kit_dir, "1", 0, 2, "s"));
        // The turn key does nothing without the option: the four shots are one view.
        var turned: f64 = 0;
        for (1..4) |k| turned = @max(turned, game_views.score(off.alone[k], try kitView(io, allocator, mod.kit_dir, "1", 0, 0, "l"), where).colour);
        // Nor does its profile take the var of the last sun: its config.cfg is as a 2003 mission leaves it.
        const config = try std.Io.Dir.cwd().readFileAlloc(io, try std.fs.path.join(allocator, &.{ root, "user", "off", "profiles", "Turn", "config.cfg" }), allocator, .limited(4 << 20));
        const remembered = std.mem.indexOf(u8, config, "Remastered.SunTurn") != null;
        try report.print(allocator, "Remastered off, sun turn 2 forced: the mission's log says sun turn {d}; the view of turn 0 under light 0 there, IoU / colour {d:.3} / {d:.2}, under light 2 {d:.2}; " ++
            "its shadow IoU {d:.3}, light 2's {d:.3}; after the turn key the same view's colour at most {d:.2}; its profile holds a last sun: {}\n", .{ traced, own.iou, own.colour, forced.colour, shadow_iou, forced_shadow, turned, remembered });
        if (traced != 0 or own.iou < min_iou or own.colour > max_colour or forced.colour < min_other_light_colour or shadow_iou < min_lit_shadow_iou or forced_shadow >= shadow_iou or turned > max_colour or remembered) failed = true;
    }

    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the sun's turn is not the mission's (the house's and the tank's shadows within {d} degrees of the turned sun's way and of each other; IoU floor {d}, colour ceiling {d}); the frames are in {s}\n", .{ report.items, max_sun_degrees, min_iou, max_colour, root });
        return error.SunWrong;
    }
}

/// The hill of the terrain test: road3d's 33 x 33 altitudes raised to a
/// cosine hill of this height over this radius (terrain cells: 32 sqrt 2
/// world units each, Formats/fmtTerrain.h's fWorldCellSize) in the map's
/// middle, world (724, 724), where the shots look. Its slopes stay below the
/// camera's 30 degrees.
const hill_height: f64 = 120;
const hill_radius: f64 = 14;
const hill_cell: f64 = 32.0 * std.math.sqrt2;
const hill_look = 724;
fn hillHeight(i: i32, j: i32) f64 {
    if (i < 0 or j < 0 or i > 32 or j > 32) return 0;
    const r = @min(std.math.hypot(@as(f64, @floatFromInt(i)) - 16, @as(f64, @floatFromInt(j)) - 16) / hill_radius, 1.0);
    return @round(hill_height * (@cos(r * std.math.pi) + 1) / 2);
}

/// The shade the Map Editor bakes into a vertex of the hill for the 2003 sun
/// (CVertexAltitudeInfo::UpdateShades: the six triangles round the vertex,
/// the rows running against world y, the cosine to (1, 1, -2) held between
/// 0.6 and 1).
fn hillShade(i: i32, j: i32) u8 {
    const cell = hill_cell;
    const z = hillHeight(i, j);
    const edge = struct {
        fn at(x: i32, y: i32, own: f64) f64 {
            return if (x < 0 or y < 0 or x > 32 or y > 32) own else hillHeight(x, y);
        }
    }.at;
    const v = [6][3]f64{
        .{ -cell, cell, edge(i - 1, j - 1, z) - z }, .{ 0, cell, edge(i, j - 1, z) - z },   .{ cell, 0, edge(i + 1, j, z) - z },
        .{ cell, -cell, edge(i + 1, j + 1, z) - z }, .{ 0, -cell, edge(i, j + 1, z) - z }, .{ -cell, 0, edge(i - 1, j, z) - z },
    };
    var n = [3]f64{ 0, 0, 0 };
    for (0..6) |k| {
        const a = v[(k + 1) % 6];
        const b = v[k];
        n = .{ n[0] + a[1] * b[2] - a[2] * b[1], n[1] + a[2] * b[0] - a[0] * b[2], n[2] + a[0] * b[1] - a[1] * b[0] };
    }
    const length = @sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    const sun = [3]f64{ 1.0 / @sqrt(6.0), 1.0 / @sqrt(6.0), -2.0 / @sqrt(6.0) };
    const light = -(n[0] * sun[0] + n[1] * sun[1] + n[2] * sun[2]) / length;
    return @intFromFloat(std.math.clamp(light, 0.6, 1.0) * 255.0);
}

/// The mean brightness of a box of `half` pixels round a point of a shot.
fn brightness(frame: []const u8, at_point: [2]i32, half: i32) f64 {
    var sum: f64 = 0;
    var count: f64 = 0;
    var y = at_point[1] - half;
    while (y <= at_point[1] + half) : (y += 1) {
        var x = at_point[0] - half;
        while (x <= at_point[0] + half) : (x += 1) {
            const p = units.rgb(frame, x, y) orelse continue;
            sum += @as(f64, @floatFromInt(p[0])) + @as(f64, @floatFromInt(p[1])) + @as(f64, @floatFromInt(p[2]));
            count += 3;
        }
    }
    return if (count == 0) 0 else sum / count;
}

/// A slope lit against the slope across the hill, at the least; the light
/// model gives 0.95 against 0.62 at the hill's steepest.
const min_lit_to_shaded: f64 = 1.25;

test "the ground's shade turns with the sun: under each sun turn the hill's lit slope is the one that faces that sun" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-sun-terrain-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root_rel = "zig-out/local-test/camera-turn/sun-terrain";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), root_rel });
    cwd.deleteTree(io, root_rel) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const map = try writeStandingMod(io, allocator, data, "Data/Maps/road3d.xml");
    const start = std.mem.indexOf(u8, map, "<Altitudes>").?;
    const end = std.mem.indexOf(u8, map, "</Altitudes>").?;
    var hill: std.ArrayList(u8) = .empty;
    try hill.appendSlice(allocator, map[0..start]);
    try hill.appendSlice(allocator, "<Altitudes><item size_x=\"33\" size_y=\"33\"/>");
    for (0..33) |j| for (0..33) |i| try hill.print(allocator, "<item Height=\"{d:.0}\" Shade=\"{d}\"/>", .{ hillHeight(@intCast(i), @intCast(j)), hillShade(@intCast(i), @intCast(j)) });
    try hill.appendSlice(allocator, map[end..]);
    // Without road3d's road, which crosses the hill white: the slopes are grass alone.
    const roads = std.mem.indexOf(u8, hill.items, "<Roads>").?;
    const roads_end = std.mem.indexOf(u8, hill.items, "</Roads>").? + "</Roads>".len;
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "hill.xml" }), .data = try std.mem.concat(allocator, u8, &.{ hill.items[0..roads], "<Roads/>", hill.items[roads_end..] }) });

    // The four slopes half way up, by the way each faces on the ground, and
    // where the 2003 camera shows that point: 7 cells from the top, raised by
    // the hill's height there. A slope facing (-1, -1) faces the 2003 sun,
    // whose light travels along (1, 1); sun turn t is lit from that way turned
    // t times by (x, y) -> (-y, x).
    const faces = [4][2]f64{ .{ -1, -1 }, .{ 1, -1 }, .{ 1, 1 }, .{ -1, 1 } };
    var points: [4][2]i32 = undefined;
    for (&points, faces) |*p, f| {
        const reach = 7.0 * hill_cell / std.math.sqrt2;
        const lift = hill_height / 2 * @sqrt(3.0) / 2;
        p.* = .{ turn.centre[0] + @as(i32, @intFromFloat(@round((f[0] + f[1]) * reach / std.math.sqrt2))), turn.centre[1] + @as(i32, @intFromFloat(@round((f[0] - f[1]) * reach / (2 * std.math.sqrt2) - lift))) };
    }

    _ = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, "option", "", "60:set=GamePlay.Remastered=ON,120:exit", "", .{ .user = "on" });
    const ground_only = "shadows,objects,units,effects,noise,warfog,border,haze,grid";
    const schedule = std.fmt.comptimePrint("g1000:hover=1000x400,g10:camera={d}x{d},g1500:shot,g200:exit", .{ hill_look, hill_look });
    var means: [4][4]f64 = undefined;
    for (0..4) |sun| {
        const name = try std.fmt.allocPrint(allocator, "hill-sun{d}", .{sun});
        const run = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, name, "hill.xml", schedule, ground_only, .{ .user = "on", .env = &.{.{ "BK_SUN_TURN", try std.fmt.allocPrint(allocator, "{d}", .{sun}) }} });
        const frame = try control.readShot(io, allocator, run.user, "g1500", root, name);
        for (&means[sun], points) |*m, p| m.* = brightness(frame, p, 20);
    }
    // Remastered off: the forced turn changes nothing on the ground.
    const off = try control.runScheduleWith(io, allocator, game, root, turn.mod_name, "hill-off-sun2", "hill.xml", schedule, ground_only, .{ .user = "off", .env = &.{.{ "BK_SUN_TURN", "2" }} });
    const off_frame = try control.readShot(io, allocator, off.user, "g1500", root, "hill-off-sun2");

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "sun: the four slopes' brightness (facing -x-y, +x-y, +x+y, -x+y), each over its own mean of the four suns; the brightest and the darkest slope; the lit slope over the one across\n");
    for (0..4) |sun| {
        var shares: [4]f64 = undefined;
        for (&shares, 0..) |*s, f| s.* = means[sun][f] * 4 / (means[0][f] + means[1][f] + means[2][f] + means[3][f]);
        var brightest: usize = 0;
        var darkest: usize = 0;
        for (shares, 0..) |s, f| {
            if (s > shares[brightest]) brightest = f;
            if (s < shares[darkest]) darkest = f;
        }
        const ratio = means[sun][sun] / means[sun][(sun + 2) % 4];
        try report.print(allocator, "{d}: {d:.3} {d:.3} {d:.3} {d:.3}; {d} and {d}; {d:.3}\n", .{ sun, shares[0], shares[1], shares[2], shares[3], brightest, darkest, ratio });
        if (brightest != sun or darkest != (sun + 2) % 4 or ratio < min_lit_to_shaded) failed = true;
    }
    var off_differs: f64 = 0;
    for (points, 0..) |p, f| off_differs = @max(off_differs, @abs(brightness(off_frame, p, 20) - means[0][f]));
    try report.print(allocator, "Remastered off, sun turn 2 forced: the slopes' brightness is the 2003 sun's within {d:.2}\n", .{off_differs});
    if (off_differs > 1.0) failed = true;
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the hill's lit slope does not turn with the sun (the lit slope at least {d} times the one across); the frames are in {s}\n", .{ report.items, min_lit_to_shaded, root });
        return error.GroundShadedWrong;
    }
}

/// The maps of the built mod shot for the maintainer (`camera-turn-kit-review`,
/// no checks): nine buildings each, African, European and Russian, and the
/// sun turns each is shot under at the four camera steps. The European map is
/// shot under all four suns: its `-step0` frames show the one view under each
/// sun, each sun's four frames that sun from all four sides.
const kit_review_maps = [_]struct { map: []const u8, suns: []const usize }{
    .{ .map = "kit_views_08", .suns = &.{ 0, 1, 2, 3 } },
    .{ .map = "kit_views_01", .suns = &.{1} },
    .{ .map = "kit_views_17", .suns = &.{2} },
    .{ .map = "kit_views_08_damaged", .suns = &.{3} },
};

test "kit review frames: the built Remastered views mod on its kit maps at the four camera steps under the sun's turns" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-kit-review\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const here = try std.process.currentPathAlloc(io, allocator);
    // The mod camera-turn-mod installed, as `zig build run-remastered` plays it;
    // the Game's user folders and logs go to a scratch folder beside the frames.
    const mods = try std.fs.path.join(allocator, &.{ here, "zig-out/remastered/mods" });
    const out_rel = "zig-out/review/kit-views";
    const out = try std.fs.path.join(allocator, &.{ here, out_rel });
    cwd.deleteTree(io, out_rel) catch {};
    try cwd.createDirPath(io, out_rel);
    // Remastered on in the scratch user's profile before a mission starts: the
    // sun's turn is picked then, here forced (BK_SUN_TURN).
    _ = try control.runScheduleWith(io, allocator, game, out, "remastered_views", "option", "", "60:set=GamePlay.Remastered=ON,120:exit", "", .{ .user = "review", .env = &.{.{ "BK_MODS_ROOT", mods }} });
    for (kit_review_maps) |review| for (review.suns) |sun| {
        var schedule: std.ArrayList(u8) = .empty;
        try schedule.print(allocator, "g1000:hover=1000x400,g10:camera={d}x{d}", .{ camera, camera });
        for (0..4) |k| {
            if (k != 0) try schedule.appendSlice(allocator, ",g10:key=COMMA");
            try schedule.print(allocator, ",g{d}:shot", .{1500 + k});
        }
        try schedule.appendSlice(allocator, ",g200:exit");
        const name = try std.fmt.allocPrint(allocator, "{s}-sun{d}", .{ review.map, sun });
        const run = try control.runScheduleWith(io, allocator, game, out, "remastered_views", name, try std.fmt.allocPrint(allocator, "{s}.xml", .{review.map}), schedule.items, "", .{ .user = "review", .env = &.{ .{ "BK_MODS_ROOT", mods }, .{ "BK_SUN_TURN", try std.fmt.allocPrint(allocator, "{d}", .{sun}) } } });
        for (0..4) |k| _ = try control.readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "g{d}", .{1500 + k}), out, try std.fmt.allocPrint(allocator, "{s}-step{d}", .{ name, k }));
    };
}

/// A picture with its zero cross at `anchor` over a ground picture (a damaged
/// state's rubble), where there is one: what the Game draws there, on the
/// picture's own canvas grown to hold the ground's.
fn overGround(allocator: std.mem.Allocator, sprite: game_views.Sprite, ground: ?game_views.Sprite) !game_views.Sprite {
    const g = ground orelse return sprite;
    const left = @max(sprite.anchor[0], g.anchor[0]);
    const top = @max(sprite.anchor[1], g.anchor[1]);
    const width: usize = @intCast(left + @max(@as(i32, @intCast(sprite.image.width)) - sprite.anchor[0], @as(i32, @intCast(g.image.width)) - g.anchor[0]));
    const height: usize = @intCast(top + @max(@as(i32, @intCast(sprite.image.height)) - sprite.anchor[1], @as(i32, @intCast(g.image.height)) - g.anchor[1]));
    const out: views.Image = .{ .width = @intCast(width), .height = @intCast(height), .pixels = try allocator.alloc([4]u8, width * height) };
    @memset(out.pixels, .{ 0, 0, 0, 0 });
    for ([_]game_views.Sprite{ g, sprite }) |layer| {
        const dx: usize = @intCast(left - layer.anchor[0]);
        const dy: usize = @intCast(top - layer.anchor[1]);
        for (0..layer.image.height) |y| for (0..layer.image.width) |x| {
            const p = layer.image.pixels[y * layer.image.width + x];
            if (p[3] >= 128) out.pixels[(y + dy) * width + x + dx] = p;
        };
    }
    return .{ .image = out, .anchor = .{ left, top } };
}

/// The review of 2026-10-31 (`camera-turn-full3d-review-*`, frames for the
/// maintainer, no checks): objects alone at their facings, then a European
/// village, over road3d with shadows on at the four camera steps. With
/// BK_REVIEW_BEFORE set the Game and bk-views are another checkout's (the
/// build's -Dfull3d-review-before) and the turns are made as its
/// camera-turn-mod made them. Each shot is `<root>/<name>-<k>.png`, and
/// places.txt gives each object's zero cross at each step.
const reviewed = [_]Object{
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01", .mod_dir = "buildings/europe/summer/e_wb" },
    .{ .name = "E_WB_02", .folder = "Data/Buildings/europe/summer/e_wb/02", .mod_dir = "buildings/europe/summer/e_wb", .facing = 1 },
    .{ .name = "E_WB_01", .folder = "Data/Buildings/europe/summer/e_wb/01", .mod_dir = "buildings/europe/summer/e_wb", .facing = 1 },
    .{ .name = "E_House02_2", .folder = "Data/Buildings/europe/summer/e_house02_2", .mod_dir = "buildings/europe/summer" },
    .{ .name = "E_House01_1", .folder = "Data/Buildings/europe/summer/e_house01_1", .mod_dir = "buildings/europe/summer", .facing = 2 },
    .{ .name = "A_H01_1", .folder = "Data/Buildings/africa/summer/a_h01_1", .mod_dir = "buildings/africa/summer" },
    .{ .name = "A_H07_1", .folder = "Data/Buildings/africa/summer/a_h07_1", .mod_dir = "buildings/africa/summer", .facing = 1 },
    .{ .name = "Rick", .folder = "Data/Objects/SimpleObjects/common/summer/rick", .mod_dir = "objects/simpleobjects/common/summer" },
    .{ .name = "DZOT_01", .folder = "Data/Buildings/common/summer/dzot/01", .mod_dir = "buildings/common/summer/dzot" },
    .{ .name = "Fence_SandBag01", .folder = "Data/Objects/SimpleObjects/common/summer/fence_sandbag/01", .mod_dir = "objects/simpleobjects/common/summer/fence_sandbag" },
    .{ .name = "E_CTownHouse01_2", .folder = "Data/Buildings/europe/summer/e_ctownhouse01_2", .mod_dir = "buildings/europe/summer", .facing = 3 },
    .{ .name = "E_JettyBuilding_4", .folder = "Data/Buildings/europe/summer/e_jettybuilding_4", .mod_dir = "buildings/europe/summer" },
    .{ .name = "E_House09_1", .folder = "Data/Buildings/europe/summer/e_house09_1", .mod_dir = "buildings/europe/summer", .facing = 1 },
    .{ .name = "E_FrenchShed01", .folder = "Data/Buildings/europe/summer/e_frenchshed01", .mod_dir = "buildings/europe/summer" },
    .{ .name = "UkraineWell01", .folder = "Data/Objects/SimpleObjects/ussr/summer/ukrainewell/01", .mod_dir = "objects/simpleobjects/ussr/summer/ukrainewell" },
    .{ .name = "Semaphore02", .folder = "Data/Objects/SimpleObjects/common/summer/semaphore/02", .mod_dir = "objects/simpleobjects/common/summer/semaphore" },
};

/// The village: its houses at several facings and props, each footprint's
/// middle this many AI units from the screen's middle.
const village = [_]struct { object: Object, at: [2]i32 }{
    .{ .object = reviewed[3], .at = .{ 0, 0 } },
    .{ .object = reviewed[1], .at = .{ 420, 0 } },
    .{ .object = reviewed[4], .at = .{ -420, 0 } },
    .{ .object = reviewed[12], .at = .{ 0, 420 } },
    .{ .object = reviewed[10], .at = .{ 0, -420 } },
    .{ .object = reviewed[11], .at = .{ 400, -400 } },
    .{ .object = reviewed[0], .at = .{ -400, 400 } },
    .{ .object = reviewed[7], .at = .{ 240, 240 } },
    .{ .object = reviewed[14], .at = .{ -240, -240 } },
    .{ .object = reviewed[9], .at = .{ 260, -150 } },
};

const Review = struct { io: std.Io, allocator: std.mem.Allocator, game: []const u8, tool: []const u8, root: []const u8, data: []const u8, work: []const u8, map: []const u8, before: bool, places: std.ArrayList(u8) = .empty };

fn startReview(allocator: std.mem.Allocator, part: []const u8) !Review {
    const io = testing.io;
    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse return error.MissingGame;
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const before = (try game_views.envValue(allocator, "BK_REVIEW_BEFORE")) != null;
    const root_rel = try std.fmt.allocPrint(allocator, "{s}-review-{s}/{s}", .{ scratch, if (before) "before" else "after", part });
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), root_rel });
    std.Io.Dir.cwd().deleteTree(io, root_rel) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    return .{ .io = io, .allocator = allocator, .game = game, .tool = tool, .root = root, .data = data, .work = try std.fs.path.join(allocator, &.{ root, "work" }), .map = try writeStandingMod(io, allocator, data, "Data/Maps/road3d.xml"), .before = before };
}

/// Makes an object's turns in the review's mod; returns its footprint's
/// middle from its zero cross, turned by its facing.
fn reviewTurns(r: *Review, object: Object) ![2]f64 {
    const turned = try std.fs.path.join(r.allocator, &.{ r.data, object.mod_dir });
    try std.Io.Dir.cwd().createDirPath(r.io, turned);
    try game_views.runTool(r.io, r.allocator, r.tool, &.{ "side", object.folder, if (r.before) "1,2,3,1l1,2l2,3l3" else try turnsFor(r.allocator, object), turned, r.work });
    var foot = try footprintMiddle(r.io, r.allocator, r.tool, object.folder, try std.fmt.allocPrint(r.allocator, "{s}/sprites/{s}", .{ r.root, object.name }));
    for (0..object.facing) |_| foot = .{ -foot[1], foot[0] };
    return foot;
}

/// The map item of an object whose footprint's middle (`foot`) stands
/// `at_ai` from the middle, and its zero cross on the screen at step 0, as
/// placeStanding lays it.
fn reviewItem(r: *Review, object: Object, foot: [2]f64, at_ai: [2]i32, link: usize) !struct { item: []const u8, zero: [2]i32 } {
    var d: [2]i32 = undefined;
    for (&d, foot, at_ai) |*v, f, a| v.* = 4 * @as(i32, @intFromFloat(@round((@as(f64, @floatFromInt(a)) - f * std.math.sqrt2) / 4)));
    return .{
        .item = try std.fmt.allocPrint(r.allocator, "<item Direction=\"{d}\" Player=\"-1\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ object.facing * 16384, object.name, shadow_middle_ai + d[0], shadow_middle_ai + d[1], link }),
        .zero = .{ turn.centre[0] + @divExact(d[0] + d[1], 2), turn.centre[1] + @divExact(d[0] - d[1], 4) },
    };
}

/// Writes maps/<name>.xml with the items and shoots it at the four steps.
fn reviewShoot(r: *Review, name: []const u8, items: []const u8, labels: []const []const u8, zeros: []const [2]i32) !void {
    try std.Io.Dir.cwd().writeFile(r.io, .{
        .sub_path = try std.fs.path.join(r.allocator, &.{ r.data, "maps", try std.fmt.allocPrint(r.allocator, "{s}.xml", .{name}) }),
        .data = try std.mem.replaceOwned(u8, r.allocator, r.map, "<Objects/>", try std.fmt.allocPrint(r.allocator, "<Objects>{s}</Objects>", .{items})),
    });
    _ = try turn.runGame(r.io, r.allocator, r.game, r.root, name, try std.fmt.allocPrint(r.allocator, "{s}.xml", .{name}), .{ shadow_look, shadow_look }, "ON", with_shadows, 4);
    for (labels, zeros) |label, zero| for (0..4) |k| {
        const z = turn.turnScreen(zero, k);
        try r.places.print(r.allocator, "{s} {s} {d} {d} {d}\n", .{ name, label, k, z[0], z[1] });
    };
}

fn reviewObjects(part: []const u8, from: usize, to: usize) !void {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var r = try startReview(allocator, part);
    for (reviewed[from..to], from..) |object, o| {
        const foot = try reviewTurns(&r, object);
        // 110 pixels below the screen's middle at step 0, clear of road3d's
        // crossing, whose roads are drawn over shadows.
        const placed = try reviewItem(&r, object, foot, .{ 240, -200 }, 1);
        const name = try std.fmt.allocPrint(allocator, "{d:0>2}-{s}-f{d}", .{ o, object.name, object.facing });
        try reviewShoot(&r, name, placed.item, &.{object.name}, &.{placed.zero});
    }
    try std.Io.Dir.cwd().writeFile(r.io, .{ .sub_path = try std.fs.path.join(allocator, &.{ r.root, "places.txt" }), .data = r.places.items });
}

test "review frames a: the first half of the objects alone" {
    try reviewObjects("objects", 0, reviewed.len / 2);
}

test "review frames b: the second half of the objects alone" {
    try reviewObjects("objects-b", reviewed.len / 2, reviewed.len);
}

test "review frames village: a European village" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var r = try startReview(allocator, "village");
    var items: std.ArrayList(u8) = .empty;
    var labels: std.ArrayList([]const u8) = .empty;
    var zeros: std.ArrayList([2]i32) = .empty;
    for (village, 1..) |v, link| {
        const foot = try reviewTurns(&r, v.object);
        const placed = try reviewItem(&r, v.object, foot, v.at, link);
        try items.appendSlice(allocator, placed.item);
        try labels.append(allocator, try std.fmt.allocPrint(allocator, "{s}-f{d}", .{ v.object.name, v.object.facing }));
        try zeros.append(allocator, placed.zero);
    }
    try reviewShoot(&r, "village", items.items, labels.items, zeros.items);
    try std.Io.Dir.cwd().writeFile(r.io, .{ .sub_path = try std.fs.path.join(allocator, &.{ r.root, "places.txt" }), .data = r.places.items });
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

fn shadowOf(frames: [3][]const u8, box: [4]i32, zero: [2]i32, foot: [2]f64, k: usize, sun_way: [2]f64) Cast {
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
    const sun = turn.turnOffset(.{ (sun_way[0] + sun_way[1]) / std.math.sqrt2, (sun_way[0] - sun_way[1]) / (2 * std.math.sqrt2) }, k);
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
