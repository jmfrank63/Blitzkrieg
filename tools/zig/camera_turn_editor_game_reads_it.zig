//! The Map Editor turns buildings by the same rule as the Game (M006 S06,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md).
//!
//! `bk-views side` makes the turned sides of the 2003 e_ctownhouse01_2 and
//! e_frenchshed01 into a scratch mod at their Data paths (as
//! camera-turn-game-reads-it does). A map written from road3d.xml holds two
//! town houses, the shed and the bench Bench01 at facing 0 and a German
//! sniper, each 300 AI units along both axes from the map's middle, so no
//! picture comes near another's at any camera step.
//!
//! - editor: the installed Map Editor opens the map with the mod, selects each
//!   object (band_select) and turns it with Q and E (one side a press): the
//!   town houses to facings 2 and 3, the shed and the bench to 1. The engine
//!   frame around each (objects only) must show its turned side, better than
//!   any other side; a building without turned pictures (E_House01_1, placed
//!   and deleted again) must refuse the turn. It saves the map as XML into the
//!   mod.
//! - on: the real Game, Remastered on, plays the saved map: with the camera
//!   on each object in turn, at camera steps 0 to 3 it must draw the side of
//!   turn (facing + step) mod 4, a prop of four sides its variant of that turn.
//! - off: Remastered off, every object draws its 2003 picture and the turn
//!   key changes nothing.
//! - ai on / ai off: the shed at facing 1 stands turned with its picture (the
//!   option on when the map loads): the sniper, ordered along a line the
//!   shed's own footprint crosses and its turned one does not, walks straight
//!   through where the 2003 footprint was; ordered into the shed, it enters at
//!   a turned entrance. With the option off it walks around the 2003
//!   footprint and enters at a 2003 entrance.
//!
//! The runs keep their frames, logs and scores.txt in
//! zig-out/local-test/camera-turn/editor. BK_VIEWS_GAME names the installed
//! Game, BK_VIEWS_EDITOR the installed Map Editor and BK_VIEWS_TOOL
//! bk-views; `zig build camera-turn-editor-game-reads-it` sets them.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const views = @import("bk_views.zig");
const bk_image = @import("bk_image.zig");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/editor";
const mod_name = "camera_turn_editor";
const width = turn.width;
const height = turn.height;
const centre = turn.centre;
const sqrt2 = std.math.sqrt2;
/// road3d's middle in AI units (the map XML's), and the size of a terrain
/// tile in world units (BkEditorWorldToTile's tiles, y from the far edge).
const middle = control.middle;
const tile = 32.0 * sqrt2;
const map_tiles = 32.0;
const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";

const Object = struct {
    name: []const u8,
    /// Its folder in Data and whether its turns are bk-views' `_r<k>` (a
    /// building) or its family's variants `01` to `04` (a prop of four sides).
    folder: []const u8,
    building: bool,
    /// AI offset from the middle, the keys that turn it in the editor and the
    /// facing they leave it at.
    at: [2]f64,
    keys: []const u8,
    facing: usize,
};

const objects = [_]Object{
    .{ .name = "E_CTownHouse01_2", .folder = "Data/Buildings/europe/summer/e_ctownhouse01_2", .building = true, .at = .{ -300, -300 }, .keys = "EE", .facing = 2 },
    .{ .name = "E_CTownHouse01_2", .folder = "Data/Buildings/europe/summer/e_ctownhouse01_2", .building = true, .at = .{ 300, 300 }, .keys = "Q", .facing = 3 },
    .{ .name = "E_FrenchShed01", .folder = "Data/Buildings/europe/summer/e_frenchshed01", .building = true, .at = .{ 300, -300 }, .keys = "E", .facing = 1 },
    .{ .name = "Bench01", .folder = "Data/Objects/SimpleObjects/ussr/summer/bench/01", .building = false, .at = .{ -300, 300 }, .keys = "E", .facing = 1 },
};
const shed = 2;

/// The sniper (AI offset from the middle) and where it is ordered: a line 50
/// AI units in front of the shed (+y), from 400 left of it to 250 right of it.
/// The tiles the shed locks (its 1.xml in AI units, origin 239, 13, laid on
/// the AI's 32-unit tiles from its place, 1024, 424): x -224..0, y -8..120
/// from its place, across the line; turned once (origin 115, 239, a grid 4
/// wide and 7 deep) x -128..0, y -232..-8, clear of it. Its entrances face +x,
/// turned they face +y.
const sniper_at = [2]f64{ objects[shed].at[0] - 400, objects[shed].at[1] + 50 };
const order_to = [2]f64{ objects[shed].at[0] + 250, objects[shed].at[1] + 50 };
const shed_box = [4]f64{ -224, -8, 0, 120 };
const shed_turned_box = [4]f64{ -128, -232, 0, -8 };
const shed_entrances = [2][2]f64{ .{ 1, 66 }, .{ 1, 98 } };

/// The editor draws the 2003 pixels (as the Game: building-views-game-reads-it);
/// a small prop's outline is a few pixels of edge (camera-turn-world-game-reads-it).
const min_iou: f64 = 0.95;
const min_prop_iou: f64 = 0.9;
const max_colour = 2.0;
/// A path and an entrance, AI units.
const max_straight = 15.0;
const min_detour = 40.0;
/// A soldier goes in within a tile of the entrance it walked to (it entered
/// 1 to 31 units from it); the other sides' entrances lie 66 and more away.
const max_entrance = 40.0;
const min_other_entrance = 60.0;
/// The editor's frame: the side's own colours where it is found, and how much
/// further off any other side is there.
const max_editor_colour = 2.0;
const min_editor_margin = 10.0;

/// k quarter turns (x, y) -> (-y, x).
fn turnAi(p: [2]f64, k: usize) [2]f64 {
    var q = p;
    for (0..k) |_| {
        const x = q[0];
        q = .{ -q[1], x };
    }
    return q;
}

fn worldOf(d: [2]f64) [2]f64 {
    return .{ (middle + d[0]) / sqrt2, (middle + d[1]) / sqrt2 };
}

test "a turn of an AI point and its footprint box" {
    try testing.expectEqual([2]f64{ -2, 1 }, turnAi(.{ 1, 2 }, 1));
    try testing.expectEqual([2]f64{ 1, 2 }, turnAi(.{ 1, 2 }, 4));
    // The shed's entrances face +x; turned once they face +y.
    try testing.expectEqual([2]f64{ -66, 1 }, turnAi(shed_entrances[0], 1));
}

test "the Map Editor turns buildings and a prop of four sides by the Game's rule, the Game draws and stands them so" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-editor-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const editor = (try game_views.envValue(allocator, "BK_VIEWS_EDITOR")) orelse return error.MissingEditor;
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const turned = try std.fs.path.join(allocator, &.{ data, "buildings/europe/summer" });
    try cwd.createDirPath(io, turned);
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    for (objects[0..3], 0..) |object, i| {
        if (i != 1) try game_views.runTool(io, allocator, tool, &.{ "side", object.folder, "1,2,3", turned, work });
    }
    try writeMod(io, allocator, data);

    // Every object's four sides: a building's turns, a prop's variants.
    var sprites: [objects.len][4]game_views.Sprite = undefined;
    for (objects, 0..) |object, o| for (0..4) |k| {
        const base = std.fs.path.basename(object.folder);
        const folder = if (k == 0) object.folder else if (object.building)
            try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}", .{ turned, base, k })
        else
            try std.fmt.allocPrint(allocator, "{s}{d}", .{ object.folder[0 .. object.folder.len - 1], k + 1 });
        sprites[o][k] = try game_views.decompose(io, allocator, tool, folder, try std.fmt.allocPrint(allocator, "{s}/sprites/{d}_{d}", .{ root, o, k }));
    };

    var failed = false;
    var report: std.ArrayList(u8) = .empty;

    // The Map Editor turns each object and saves the map.
    // The editor saves outside the mod's data (which it treats as read-only,
    // as a game's), and the map is copied into the mod for the Game.
    const edited = try std.fs.path.join(allocator, &.{ root, "saved", "edited.xml" });
    const shots = try runEditor(io, allocator, editor, root, try std.fs.path.join(allocator, &.{ data, "maps", "base.xml" }), edited);
    try cwd.copyFile(edited, cwd, try std.fs.path.join(allocator, &.{ data, "maps", "edited.xml" }), io, .{});
    // The sniper for the AI's runs joins a copy of it.
    {
        const map = try cwd.readFileAlloc(io, edited, allocator, .limited(8 << 20));
        const end = std.mem.indexOf(u8, map, "</Objects>") orelse return error.NoObjects;
        var sniper: std.ArrayList(u8) = .empty;
        var link: u32 = objects.len + 1;
        try control.item(allocator, &sniper, "German_sniper", sniper_at, 0, 16384 * 3, 0, &link);
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "ai.xml" }), .data = try std.mem.concat(allocator, u8, &.{ map[0..end], sniper.items, map[end..] }) });
    }
    // The editor's frame is the window with its panels and the ground (its
    // camera kept inside the map): each side is looked for by its colours.
    try report.appendSlice(allocator, "editor: object facing: mean colour difference of its side where it is found (x,y); best of another side there\n");
    for (objects, shots, 0..) |object, frame, o| {
        const own = findSide(frame, sprites[o][object.facing]);
        var other_best: f64 = std.math.floatMax(f64);
        for (0..4) |k| {
            if (k != object.facing) other_best = @min(other_best, sideColourNear(frame, sprites[o][k], own.at));
        }
        try report.print(allocator, "{s} {d}: {d:.2} ({d},{d}); {d:.2}\n", .{ object.name, object.facing, own.colour, own.at[0], own.at[1], other_best });
        if (own.colour > max_editor_colour or other_best < own.colour + min_editor_margin) failed = true;
    }
    const saved = try cwd.readFileAlloc(io, edited, allocator, .limited(8 << 20));
    for (objects) |object| {
        const want = try std.fmt.allocPrint(allocator, "Direction=\"{d}\"", .{object.facing * 16384});
        if (std.mem.indexOf(u8, saved, want) == null) {
            try report.print(allocator, "the saved map has no {s}\n", .{want});
            failed = true;
        }
    }

    // Remastered on: each object draws turn (facing + step) mod 4.
    try report.appendSlice(allocator, "on: object facing step: turn: IoU / colour (offset); best IoU of another turn\n");
    {
        var schedule: std.ArrayList(u8) = .empty;
        try schedule.appendSlice(allocator, "g1000:hover=1000x400,g10:set=GamePlay.Remastered=ON");
        for (objects, 0..) |object, o| {
            const look = worldOf(object.at);
            try schedule.print(allocator, ",g10:camera={d:.1}x{d:.1}", .{ look[0], look[1] });
            for (0..4) |k| try schedule.print(allocator, ",g10:key=COMMA,g{d}:shot", .{1500 + o * 10 + k});
        }
        try schedule.appendSlice(allocator, ",g200:exit");
        // The schedule turns once before each shot: the k-th shot of an object is at step k + 1 + 4 o, so step (k + 1) mod 4.
        const run = try control.runSchedule(io, allocator, game, root, mod_name, "on", "edited.xml", schedule.items, objects_only);
        for (objects, 0..) |object, o| for (0..4) |k| {
            const step = (k + 1) % 4;
            const frame = try control.readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "g{d}", .{1500 + o * 10 + k}), root, try std.fmt.allocPrint(allocator, "on-{d}-{d}", .{ o, step }));
            const t = (object.facing + step) % 4;
            const own = game_views.score(frame, sprites[o][t], centre);
            var other_best: f64 = 0;
            for (0..4) |j| {
                if (j != t) other_best = @max(other_best, game_views.score(frame, sprites[o][j], centre).iou);
            }
            try report.print(allocator, "{s} {d} {d}: {d}: {d:.3} / {d:.2} ({d},{d}); {d:.3}\n", .{ object.name, object.facing, step, t, own.iou, own.colour, own.dx, own.dy, other_best });
            if (own.iou < (if (object.building) min_iou else min_prop_iou) or own.colour > max_colour or other_best >= own.iou) failed = true;
        };
    }

    // Remastered off: the 2003 pictures, and the key changes nothing.
    try report.appendSlice(allocator, "off: object: IoU / colour of its 2003 picture before and after the key (offset); pixels changed\n");
    {
        var schedule: std.ArrayList(u8) = .empty;
        try schedule.appendSlice(allocator, "g1000:hover=1000x400,g10:set=GamePlay.Remastered=OFF");
        for (objects, 0..) |object, o| {
            const look = worldOf(object.at);
            try schedule.print(allocator, ",g10:camera={d:.1}x{d:.1},g{d}:shot,g10:key=COMMA,g{d}:shot", .{ look[0], look[1], 1600 + o * 10, 1601 + o * 10 });
        }
        try schedule.appendSlice(allocator, ",g200:exit");
        const run = try control.runSchedule(io, allocator, game, root, mod_name, "off", "edited.xml", schedule.items, objects_only);
        for (objects, 0..) |object, o| {
            var frames: [2][]u8 = undefined;
            for (&frames, 0..) |*frame, k| frame.* = try control.readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "g{d}", .{1600 + o * 10 + k}), root, try std.fmt.allocPrint(allocator, "off-{d}-{d}", .{ o, k }));
            const before = game_views.score(frames[0], sprites[o][0], centre);
            const after = game_views.score(frames[1], sprites[o][0], centre);
            // Pixels the key changed, the message line and the panels left out.
            var changed: usize = 0;
            for (@intCast(turn.clear_rows[0])..@intCast(turn.clear_rows[1])) |y| for (0..width) |x| {
                const i = y * width + x;
                if (!std.mem.eql(u8, frames[0][i * 4 ..][0..3], frames[1][i * 4 ..][0..3])) changed += 1;
            };
            try report.print(allocator, "{s}: {d:.3} / {d:.2} ({d},{d}), {d:.3} / {d:.2}; {d}\n", .{ object.name, before.iou, before.colour, before.dx, before.dy, after.iou, after.colour, changed });
            // Within a pixel: the camera's place is a world point rounded to a tenth.
            const floor = if (object.building) min_iou else min_prop_iou;
            if (before.iou < floor or before.colour > max_colour or @abs(before.dx) > 1 or @abs(before.dy) > 1 or after.iou < floor or changed != 0) failed = true;
        }
    }

    // The sniper's walk and its way into the shed, the option on and off when the map loads.
    try report.appendSlice(allocator, "ai: mode: walk samples, largest distance from the line, samples in the 2003 / turned footprint, end from the target; entering at, nearest turned / 2003 entrance (AI units)\n");
    for ([_][]const u8{ "ON", "OFF" }) |mode| {
        const on = mode[1] == 'N';
        // The option is a profile setting: the drawing run of the mode set it
        // and quit, so this run, in its user folder, loads the map with it.
        const run = try control.runScheduleWith(io, allocator, game, root, mod_name, try std.fmt.allocPrint(allocator, "ai-{s}", .{mode}), "ai.xml", try aiSchedule(allocator, on), objects_only, .{ .user = if (on) "on" else "off" });
        const selections = try control.parseSelections(allocator, run.log);
        var result = measureAi(selections);
        // The shed holds it at the end.
        if (std.mem.indexOf(u8, run.log, "BK_CONTAINER_TRACE: building \"E_FrenchShed01\"") == null) result.entered_ok = false;
        try report.print(allocator, "{s}: {d}, {d:.1}, {d} / {d}, {d:.1}; {d:.0},{d:.0}, {d:.1} / {d:.1}\n", .{ mode, result.samples, result.off_line, result.in_own, result.in_turned, result.end, result.entered[0], result.entered[1], result.turned_entrance, result.own_entrance });
        const walked = result.samples >= 10 and result.end < 30 and result.entered_ok;
        if (on and !(walked and result.off_line <= max_straight and result.in_own >= 3 and result.in_turned == 0 and result.turned_entrance <= max_entrance and result.own_entrance >= min_other_entrance)) failed = true;
        if (!on and !(walked and result.off_line >= min_detour and result.in_own == 0 and result.own_entrance <= max_entrance and result.turned_entrance >= min_other_entrance)) failed = true;
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a turned object is drawn or stands wrong; the frames and logs are in {s}\n", .{ report.items, root });
        return error.TurnedWrong;
    }
}

/// mod.xml and maps/base.xml: road3d with the objects at facing 0.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn editor test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    var items: std.ArrayList(u8) = .empty;
    try items.appendSlice(allocator, "<Objects>");
    var link: u32 = 1;
    // Player 2, the map's neutral one: the editor's Check Map (which a save
    // runs) takes -1 for an invalid owner.
    for (objects) |object| try control.item(allocator, &items, object.name, object.at, 2, 0, -1, &link);
    try items.appendSlice(allocator, "</Objects>");
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "base.xml" }),
        .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
    });
}

/// The tiles band_select takes for an object's place (BkEditorWorldToTile:
/// world x and the far edge's distance in y, a terrain tile each), a tile
/// around it.
fn bandAround(allocator: std.mem.Allocator, at: [2]f64) ![]const u8 {
    const w = worldOf(at);
    const tx: i32 = @intFromFloat(@floor(w[0] / tile));
    const ty: i32 = @intFromFloat(@floor((map_tiles * tile - w[1]) / tile));
    return std.fmt.allocPrint(allocator, "{d}-{d}-{d}-{d}", .{ tx - 1, ty - 1, tx + 1, ty + 1 });
}

/// The installed Map Editor, hidden, under BK_EDITOR_AUTO: opens `map` with
/// the mod, turns each object with Q and E, refuses a building without
/// turned pictures, takes the engine frame around each object (objects only)
/// and saves the map to `out` (XML). Returns the frames.
fn runEditor(io: std.Io, allocator: std.mem.Allocator, editor: []const u8, root: []const u8, map: []const u8, out: []const u8) ![objects.len]bk_image.Image {
    const cwd = std.Io.Dir.cwd();
    const user = try std.fs.path.join(allocator, &.{ root, "user", "editor" });
    const shots_dir = try std.fs.path.join(allocator, &.{ root, "editor-shots" });
    try cwd.createDirPath(io, user);
    var s: std.ArrayList(u8) = .empty;
    var frame: u32 = 20;
    const Step = struct {
        fn add(list: *std.ArrayList(u8), a: std.mem.Allocator, at: *u32, comptime fmt: []const u8, args: anytype) !void {
            if (list.items.len != 0) try list.append(a, ',');
            try list.print(a, "{d}:", .{at.*});
            try list.print(a, fmt, args);
            at.* += 3;
        }
    }.add;
    try Step(&s, allocator, &frame, "tool=select", .{});
    for (objects, 1..) |object, link| {
        try Step(&s, allocator, &frame, "do=band_select:{s}", .{try bandAround(allocator, object.at)});
        try Step(&s, allocator, &frame, "expect=selection_count:1", .{});
        try Step(&s, allocator, &frame, "expect=angle:{d}:0", .{link});
        for (object.keys) |key| try Step(&s, allocator, &frame, "key={c}", .{key});
        try Step(&s, allocator, &frame, "expect=angle:{d}:{d}", .{ link, object.facing * 90 });
    }
    // A building without turned pictures keeps its side: placed at the
    // middle, turned, refused, deleted.
    try Step(&s, allocator, &frame, "do=camera_at:{d:.0}:{d:.0}", .{ middle / sqrt2, middle / sqrt2 });
    try Step(&s, allocator, &frame, "tool=place", .{});
    try Step(&s, allocator, &frame, "do=placer_name:E_House01_1", .{});
    try Step(&s, allocator, &frame, "click=c0x0", .{});
    try Step(&s, allocator, &frame, "expect=objects:{d}", .{objects.len + 1});
    try Step(&s, allocator, &frame, "tool=select", .{});
    try Step(&s, allocator, &frame, "key=E", .{});
    try Step(&s, allocator, &frame, "expect=status:pictures", .{});
    try Step(&s, allocator, &frame, "expect=angle:@0:0", .{});
    try Step(&s, allocator, &frame, "key=DELETE", .{});
    try Step(&s, allocator, &frame, "expect=objects:{d}", .{objects.len});
    try Step(&s, allocator, &frame, "do=band_select:0-0-0-0", .{});
    for (objects, 0..) |object, o| {
        const w = worldOf(object.at);
        try Step(&s, allocator, &frame, "do=camera_at:{d:.1}:{d:.1}", .{ w[0], w[1] });
        frame += 10;
        try Step(&s, allocator, &frame, "shot=object{d}", .{o});
    }
    // Check Map's findings stop a save; its log says which.
    try Step(&s, allocator, &frame, "do=check_map", .{});
    try Step(&s, allocator, &frame, "saveas={s}", .{out});
    frame += 10;
    try Step(&s, allocator, &frame, "exit", .{});

    var environ = try testing.environ.createMap(allocator);
    try environ.put("HOME", user);
    try environ.put("XDG_DATA_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-data" }));
    try environ.put("XDG_CONFIG_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-config" }));
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MODS_ROOT", try std.fs.path.join(allocator, &.{ root, "mods" }));
    try environ.put("BK_EDITOR_SETTINGS", try std.fs.path.join(allocator, &.{ user, "mapeditor.cfg" }));
    try environ.put("BK_EDITOR_AUTO_DIR", shots_dir);
    try environ.put("BK_EDITOR_AUTO", s.items);
    try environ.put("BK_SCENE_DISABLE", objects_only);
    try environ.put("BK_MEM_REPORT", "count");
    const log_path = try std.fmt.allocPrint(allocator, "{s}/editor.log", .{root});
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ editor, "--hidden", "-mod=" ++ mod_name, map },
        .cwd = .{ .path = std.fs.path.dirname(editor).? },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(240), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("the Map Editor ended {any}; its output is in {s}\n", .{ result.term, log_path });
        return error.EditorFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    var frames: [objects.len]bk_image.Image = undefined;
    for (&frames, 0..) |*f, o| {
        f.* = try bk_image.readTga(allocator, try cwd.readFileAlloc(io, try std.fmt.allocPrint(allocator, "{s}/object{d}.tga", .{ shots_dir, o }), allocator, .limited(64 << 20)));
        // The engine's B, G, R, A to the PNG's R, G, B, A.
        const rgba = try allocator.alloc([4]u8, f.pixels.len);
        for (rgba, f.pixels) |*out_pixel, p| out_pixel.* = .{ p[2], p[1], p[0], 255 };
        try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(allocator, "{s}/editor-{d}.png", .{ root, o }), .data = try views.writePng(allocator, .{ .width = f.width, .height = f.height, .pixels = rgba }) });
    }
    return frames;
}

/// The mean colour difference of a sprite's opaque pixels with its zero cross
/// at `at` on an editor frame (B, G, R), every `stride`-th pixel each way;
/// a pixel off the frame counts as fully different.
fn sideColour(frame: bk_image.Image, sprite: game_views.Sprite, at: [2]i32, stride: usize) f64 {
    var sum: f64 = 0;
    var count: f64 = 0;
    var y: usize = 0;
    while (y < sprite.image.height) : (y += stride) {
        var x: usize = 0;
        while (x < sprite.image.width) : (x += stride) {
            const p = sprite.image.pixels[y * sprite.image.width + x];
            if (p[3] != 255 or @max(p[0], p[1], p[2]) <= 24) continue;
            count += 1;
            const fx = at[0] + @as(i32, @intCast(x)) - sprite.anchor[0];
            const fy = at[1] + @as(i32, @intCast(y)) - sprite.anchor[1];
            if (fx < 0 or fy < 0 or fx >= @as(i32, @intCast(frame.width)) or fy >= @as(i32, @intCast(frame.height))) {
                sum += 255;
                continue;
            }
            const f = frame.pixels[@as(usize, @intCast(fy)) * frame.width + @as(usize, @intCast(fx))];
            for (0..3) |c| sum += @abs(@as(f64, @floatFromInt(f[2 - c])) - @as(f64, @floatFromInt(p[c]))) / 3;
        }
    }
    return if (count == 0) 255 else sum / count;
}

/// The best colour of a sprite within 6 pixels of `at`.
fn sideColourNear(frame: bk_image.Image, sprite: game_views.Sprite, at: [2]i32) f64 {
    var best: f64 = std.math.floatMax(f64);
    var dy: i32 = -6;
    while (dy <= 6) : (dy += 1) {
        var dx: i32 = -6;
        while (dx <= 6) : (dx += 1) best = @min(best, sideColour(frame, sprite, .{ at[0] + dx, at[1] + dy }, 1));
    }
    return best;
}

/// Where in the editor's frame a sprite is drawn, by its colours: every 4
/// pixels over the frame with every third of its pixels, then to the pixel.
fn findSide(frame: bk_image.Image, sprite: game_views.Sprite) struct { colour: f64, at: [2]i32 } {
    var best: f64 = std.math.floatMax(f64);
    var at = [2]i32{ 0, 0 };
    var y: i32 = 0;
    while (y < @as(i32, @intCast(frame.height))) : (y += 4) {
        var x: i32 = 0;
        while (x < @as(i32, @intCast(frame.width))) : (x += 4) {
            const c = sideColour(frame, sprite, .{ x, y }, 3);
            if (c < best) {
                best = c;
                at = .{ x, y };
            }
        }
    }
    var fine: f64 = std.math.floatMax(f64);
    var fine_at = at;
    var dy: i32 = -4;
    while (dy <= 4) : (dy += 1) {
        var dx: i32 = -4;
        while (dx <= 4) : (dx += 1) {
            const c = sideColour(frame, sprite, .{ at[0] + dx, at[1] + dy }, 1);
            if (c < fine) {
                fine = c;
                fine_at = .{ at[0] + dx, at[1] + dy };
            }
        }
    }
    return .{ .colour = fine, .at = fine_at };
}

/// The sniper selected and ordered along the line, then into the shed, the
/// selection traced every 200 ms of game time on the way, every 50 into the
/// shed.
fn aiSchedule(allocator: std.mem.Allocator, on: bool) ![]const u8 {
    var s: std.ArrayList(u8) = .empty;
    const start = worldOf(sniper_at);
    try s.print(allocator, "g1000:hover=1000x400,g10:camera={d:.1}x{d:.1},g500:click={d}x{d},g300:world=TraceSelection(0)", .{ start[0], start[1], centre[0], centre[1] - 12 });
    const to = control.screenAt(.{ order_to[0] - sniper_at[0], order_to[1] - sniper_at[1] }, 0);
    try s.print(allocator, ",g10:rclick={d}x{d}", .{ to[0], to[1] });
    for (0..60) |_| try s.appendSlice(allocator, ",g200:world=TraceSelection(0)");
    // Into the shed: a right click on its picture, above its footprint's middle.
    const look = worldOf(objects[shed].at);
    const box = if (on) shed_turned_box else shed_box;
    const pick = control.screenAt(.{ (box[0] + box[2]) / 2, (box[1] + box[3]) / 2 }, 0);
    try s.print(allocator, ",g10:camera={d:.1}x{d:.1},g300:rclick={d}x{d}", .{ look[0], look[1], pick[0], pick[1] - 25 });
    for (0..200) |_| try s.appendSlice(allocator, ",g50:world=TraceSelection(0)");
    try s.appendSlice(allocator, ",g10:world=TraceContainers(0),g200:exit");
    return s.items;
}

fn inBox(p: [2]f64, box: [4]f64) bool {
    return p[0] > box[0] and p[0] < box[2] and p[1] > box[1] and p[1] < box[3];
}

const AiResult = struct {
    samples: usize = 0,
    off_line: f64 = 0,
    in_own: usize = 0,
    in_turned: usize = 0,
    end: f64 = std.math.floatMax(f64),
    entered: [2]f64 = .{ 0, 0 },
    entered_ok: bool = false,
    turned_entrance: f64 = std.math.floatMax(f64),
    own_entrance: f64 = std.math.floatMax(f64),
};

/// The walk (the sniper's places, AI offsets from the shed, while it is
/// outside before the second order) and where it was last seen outside
/// before it was inside.
fn measureAi(selections: []const control.Selection) AiResult {
    var r: AiResult = .{};
    if (selections.len < 2) return r;
    const s = objects[shed].at;
    var last_outside: ?[2]f64 = null;
    for (selections[1..], 1..) |sel, i| {
        // Inside a building the sniper is a passenger, or no longer selected.
        const gone = std.mem.indexOf(u8, sel.summary, "inside=1") != null or sel.places.len == 0;
        const p = if (sel.places.len == 1) [2]f64{ sel.places[0][0] * sqrt2 - middle - s[0], sel.places[0][1] * sqrt2 - middle - s[1] } else [2]f64{ 0, 0 };
        if (i <= 60) {
            if (gone) continue;
            r.samples += 1;
            r.off_line = @max(r.off_line, @abs(p[1] - 50));
            if (inBox(p, shed_box)) r.in_own += 1;
            if (inBox(p, shed_turned_box)) r.in_turned += 1;
            r.end = std.math.hypot(p[0] - 250, p[1] - 50);
        } else if (!gone) {
            last_outside = p;
        } else if (!r.entered_ok) {
            r.entered_ok = last_outside != null;
            if (last_outside) |q| {
                r.entered = q;
                for (shed_entrances) |e| {
                    const t = turnAi(e, 1);
                    r.turned_entrance = @min(r.turned_entrance, std.math.hypot(q[0] - t[0], q[1] - t[1]));
                    r.own_entrance = @min(r.own_entrance, std.math.hypot(q[0] - e[0], q[1] - e[1]));
                }
            }
        }
    }
    return r;
}
