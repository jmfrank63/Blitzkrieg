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
//! The Game runs from zig-out/local-test/camera-turn/full3d with every user
//! path redirected (camera_turn_game_reads_it.runGame); frames, sprites, logs
//! and scores.txt stay there. BK_VIEWS_GAME names the installed Game and
//! BK_VIEWS_TOOL bk-views; `zig build camera-turn-full3d-game-reads-it` sets
//! both.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
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
