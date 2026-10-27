//! The ground, roads, rivers, flora, fences and objects at every camera step
//! (M006 S03, docs/superpowers/specs/2026-10-26-camera-turn-design.md). With
//! the Remastered option on, the turn key (`.`) turns the view a quarter turn
//! about the point at the screen centre; S01 and S02 measured the buildings,
//! this tier the rest of the world, each against the step-0 frame turned.
//!
//! - The ground of the shipped map dessau (Data/Maps/dessau.bzm) where it
//!   holds a road, field edges, a river with a bridge, trees, fences and
//!   objects, and of road3d (Data/Maps/road3d.xml), whose whole map is on the
//!   screen with its border around it: objects, units, shadows and effects
//!   off, so the frame is the terrain, the roads and rivers, the war fog and
//!   the border. Step k's frame must show step 0's turned k times at the scale
//!   of the ground's shapes (9 x 9 pixel blocks: the outlines of fields,
//!   roads, river banks and fog), much closer than not turned or turned the
//!   other way, and on road3d's grass a tile picture must keep its detail:
//!   the ratio of its horizontal to its vertical gradients stays that of step
//!   0 (a picture of the 45 degree view laid on the turned ground stretches
//!   2:1 at an odd step: ln -0.149 in S01's river3d shots; dessau's fields,
//!   roads and river turn their own edges, so there it is only reported). The camera turns about its anchor at height 0, so raised ground
//!   moves by its height on a turn: the comparison finds that lift.
//! - The same dessau view with everything drawn: closer to the step-0 frame
//!   turned than to it not turned or turned the other way (its buildings, with
//!   no turned sides in the data here, and its trees are not the ground
//!   turned, so no margin is asked).
//! - Objects alone on a map written from road3d.xml, on a ring around the
//!   centre (a quarter turn takes the ring onto itself), each scored where the
//!   turn puts it against what its kind must show, cut from the step-0 frame:
//!   a tree (_Lime, square footprint) and a box (ShellBox01, none) their own
//!   picture; a swing (Swing01, 1 x 2) its own picture at even steps and the
//!   picture mirrored about its zero cross at odd steps, better than not
//!   mirrored; a dirt ravine (DirtRavine01, a terra object flat on the ground)
//!   its picture turned as the ground turns, better than not turned; four
//!   VillageFence segments (one segment of each kind per direction), one of
//!   each direction, each the picture of the segment whose direction is its
//!   own plus the step, standing on its own footprint turned (S07: a 2003
//!   segment's zero cross is at one end of its footprint, so that picture
//!   moves by what puts its footprint's middle on the own one's, `fenceShift`),
//!   better than its own at an odd step and than the turned picture left at
//!   the zero cross (S03's drawing, off its footprint at two of four steps).
//! - S04: the four 2003 sides of the bench and the wooden column with a leg
//!   (variants 01 to 04, two eighths apart, so each place must show the
//!   variant that stood there at step 0, better than the own picture); line
//!   spans of the wooden little bridge along x and y, alternating, one place
//!   empty (at an odd step the other axis's span, at the half turn its own
//!   with the girders traded, so each place shows the span that stood there;
//!   the empty place must have moved with the turn).
//! - Remastered off, the fences, props and bridge spans: the key changes no
//!   pixel.
//!
//! The Game runs from zig-out/local-test/camera-turn/world with every user
//! path redirected (camera_turn_game_reads_it.runGame); frames, logs and
//! scores.txt stay there. BK_VIEWS_GAME names the installed Game; `zig build
//! camera-turn-world-game-reads-it` sets it.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/world";
const width = turn.width;
const height = turn.height;
const centre = turn.centre;

/// Where dessau shows a road, field edges, the river with a bridge, trees,
/// fences and objects within a quarter turn of the screen (world units, the
/// `camera=` of BK_AUTO_UI; AI 1700, 2100).
const dessau_look = [2]u32{ 1202, 1485 };
/// The middle of road3d's 32 x 32 tiles.
const road3d_look = [2]u32{ 512, 512 };
/// AI units of the map XML per world unit (AI2Vis: 32 sqrt 2 / 64).
const ai_per_world = std.math.sqrt2;

const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
const ground_only = "shadows,objects,units,effects,noise,haze,grid";

/// What an object must show at step k, from its picture in the step-0 frame.
const Kind = enum {
    /// Its own picture (a footprint a quarter turn keeps).
    own,
    /// Its own picture, mirrored about its zero cross at an odd step.
    mirrored,
    /// Its picture turned as the ground turns.
    flat,
    /// The picture of the segment whose direction is its own plus the step.
    fence,
    /// A prop of four sides (variants 01 to 04): the variant of its own plus the step.
    sides,
    /// A bridge span: at an odd step the span of the variant along the other
    /// axis, at the half turn its own with its girders traded.
    bridge,
};
/// An object of the test maps: its objects.xml name, its map FrameIndex (a
/// fence's direction packed as the map holds it: normal type 0x10000 plus the
/// direction's bit) and where it stands on the ring, in eighths of a turn.
const Placed = struct { name: []const u8, kind: Kind, eighth: u32, frame: i64 = -1, ring: f64 = ring, reach: [4]i32 = default_reach };
const fences = [_]Placed{
    .{ .name = "VillageFence", .kind = .fence, .eighth = 0, .frame = 0x10001 },
    .{ .name = "VillageFence", .kind = .fence, .eighth = 2, .frame = 0x10002 },
    .{ .name = "VillageFence", .kind = .fence, .eighth = 4, .frame = 0x10004 },
    .{ .name = "VillageFence", .kind = .fence, .eighth = 6, .frame = 0x10008 },
};
const props = [_]Placed{
    .{ .name = "_Lime", .kind = .own, .eighth = 1 },
    .{ .name = "Swing01", .kind = .mirrored, .eighth = 3 },
    .{ .name = "DirtRavine01", .kind = .flat, .eighth = 5 },
    .{ .name = "ShellBox01", .kind = .own, .eighth = 7 },
};
/// Benches and wooden columns on the ring, each variant 01 to 04 two eighths
/// on from the one before, so a quarter turn must show at each place the
/// variant that stood there; inside it, on a smaller ring, line spans (a
/// bridge object's map FrameIndex is its span type: 1 begin, 2 line, 4 end)
/// of the wooden little bridge along x (`_01`) and along y (`_02`), the
/// variant changing every quarter turn, so each place must show the span that
/// stood there, eighth 6 empty so the turn is seen to move the gap.
const sides = [_]Placed{
    .{ .name = "Bench01", .kind = .sides, .eighth = 0, .reach = side_reach },
    .{ .name = "BigColumnWithLeg_01", .kind = .sides, .eighth = 1, .reach = side_reach },
    .{ .name = "Bench02", .kind = .sides, .eighth = 2, .reach = side_reach },
    .{ .name = "BigColumnWithLeg_02", .kind = .sides, .eighth = 3, .reach = side_reach },
    .{ .name = "Bench03", .kind = .sides, .eighth = 4, .reach = side_reach },
    .{ .name = "BigColumnWithLeg_03", .kind = .sides, .eighth = 5, .reach = side_reach },
    .{ .name = "Bench04", .kind = .sides, .eighth = 6, .reach = side_reach },
    .{ .name = "BigColumnWithLeg_04", .kind = .sides, .eighth = 7, .reach = side_reach },
    .{ .name = "WoodenLittle_01", .kind = .bridge, .eighth = 0, .frame = 2, .ring = bridge_ring, .reach = bridge_reach },
    .{ .name = "WoodenLittle_02", .kind = .bridge, .eighth = 2, .frame = 2, .ring = bridge_ring, .reach = bridge_reach },
    .{ .name = "WoodenLittle_01", .kind = .bridge, .eighth = 4, .frame = 2, .ring = bridge_ring, .reach = bridge_reach },
};
const side_reach = [4]i32{ -60, -110, 60, 30 };
const bridge_ring = 240.0;
const bridge_reach = [4]i32{ -90, -60, 90, 50 };
const bridge_gap = 6;
/// The ring's radius in AI units: on the screen 368 by 184 pixels, so a
/// tall tree stays clear of the interface and of its neighbours.
const ring = 520.0;
/// The pixels around an object's zero cross that are its picture.
const default_reach = [4]i32{ -150, -230, 150, 70 };

/// Floors: the Game draws the 2003 pixels (own, mirrored, another segment)
/// or resamples them (flat). The colour allows for a sprite's soft edge,
/// blended with the backdrop, whose shade changes with the height on the
/// screen.
const min_iou: f64 = 0.9;
const min_flat_iou: f64 = 0.8;
const max_colour: f64 = 8.0;
const max_flat_colour: f64 = 16.0;
/// The ground at block scale against the step-0 frame turned, and how much
/// closer than a wrong turn.
const max_ground = 6.0;
const max_scene = 24.0;
const min_margin = 2.0;
/// |ln| of a tile picture's gradient ratio against step 0.
const max_anisotropy = 0.05;

test "the ground, roads, rivers, flora, fences and objects turn with the camera" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-world-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" }));

    var failed = false;
    var report: std.ArrayList(u8) = .empty;

    // The ground: dessau's road, field edges, river and fog, road3d's roads and border.
    try report.appendSlice(allocator, "ground: map step: blocks turned (the ground's lift, pixels) / not turned / the other way; gradient ratio ln against step 0\n");
    const grounds = [_]struct { []const u8, [2]u32, [2]i32, bool }{
        .{ "dessau.bzm", dessau_look, .{ 200, 100 }, false },
        .{ "road3d.xml", road3d_look, .{ 380, 190 }, true },
    };
    for (grounds) |entry| {
        const map, const look, const region, const grass = entry;
        const frames = try turn.runGame(io, allocator, game, root, try std.fmt.allocPrint(allocator, "ground-{s}", .{map[0 .. map.len - 4]}), map, look, "ON", ground_only, 4);
        const ratio0 = gradientRatio(frames[0], region);
        for (1..4) |k| {
            const turned = try turn.blockDifference(allocator, frames[0], frames[k], k, k, region);
            const still = try turn.blockDifference(allocator, frames[0], frames[k], 0, k, region);
            const other = try turn.blockDifference(allocator, frames[0], frames[k], 4 - k, k, region);
            const anisotropy = @log(gradientRatio(frames[k], region) / ratio0);
            try report.print(allocator, "{s} {d}: {d:.2} ({d}) / {d:.2} / {d:.2}; {d:.3}\n", .{ map, k, turned.difference, turned.lift, still.difference, other.difference, anisotropy });
            const wrong = if (k == 2) still.difference else @min(still.difference, other.difference);
            if (turned.difference > max_ground or wrong < min_margin * turned.difference or (grass and @abs(anisotropy) > max_anisotropy)) failed = true;
        }
        // A half turn keeps the whole screen on the screen: steps k and k + 2
        // agree out to its edges, where road3d's border is.
        for (0..2) |k| {
            const half = try turn.blockDifference(allocator, frames[k], frames[k + 2], 2, 2, .{ 500, 200 });
            try report.print(allocator, "{s} {d} and {d}, the whole screen: {d:.2} ({d})\n", .{ map, k, k + 2, half.difference, half.lift });
            if (half.difference > max_ground) failed = true;
        }
    }

    // dessau with everything drawn: the objects stand where the turn puts them.
    try report.appendSlice(allocator, "scene: step: blocks turned / not turned / the other way\n");
    const scene = try turn.runGame(io, allocator, game, root, "scene-dessau", "dessau.bzm", dessau_look, "ON", "haze", 4);
    for (1..4) |k| {
        const turned = try turn.blockDifference(allocator, scene[0], scene[k], k, k, .{ 200, 100 });
        const still = try turn.blockDifference(allocator, scene[0], scene[k], 0, k, .{ 200, 100 });
        const other = try turn.blockDifference(allocator, scene[0], scene[k], 4 - k, k, .{ 200, 100 });
        try report.print(allocator, "{d}: {d:.2} ({d}) / {d:.2} / {d:.2}\n", .{ k, turned.difference, turned.lift, still.difference, other.difference });
        const wrong = if (k == 2) still.difference else @min(still.difference, other.difference);
        if (turned.difference > max_scene or wrong <= turned.difference) failed = true;
    }

    // The objects, each where the turn puts it.
    try report.appendSlice(allocator, "objects: step name kind: IoU / colour (offset); IoU of the wrong picture\n");
    for ([_]struct { []const u8, []const Placed }{ .{ "fences", &fences }, .{ "props", &props }, .{ "sides", &sides } }) |entry| {
        const name, const placed = entry;
        const frames = try turn.runGame(io, allocator, game, root, try std.fmt.allocPrint(allocator, "objects-{s}", .{name}), try std.fmt.allocPrint(allocator, "{s}.xml", .{name}), road3d_look, "ON", objects_only, 4);
        for (1..4) |k| for (placed, 0..) |object, i| {
            // A bridge span moved to the gap's place has no step-0 picture to be scored against.
            if (object.kind == .bridge and (object.eighth + 2 * k) % 8 == bridge_gap) continue;
            const own = scoreObject(frames[0], frames[k], placed, i, k, true, true);
            const wrong: ?Score = switch (object.kind) {
                .own => null,
                .fence => if (!std.meta.eql(fenceShift(object.eighth / 2, k), .{ 0, 0 })) scoreObject(frames[0], frames[k], placed, i, k, true, false) else if (k % 2 == 1) scoreObject(frames[0], frames[k], placed, i, k, false, false) else null,
                .mirrored, .bridge => if (k % 2 == 1) scoreObject(frames[0], frames[k], placed, i, k, false, false) else null,
                .flat => scoreObject(frames[0], frames[k], placed, i, k, false, false),
                .sides => scoreObject(frames[0], frames[k], placed, i, k, false, false),
            };
            try report.print(allocator, "{d} {s} {s}: {d:.3} / {d:.2} ({d},{d})", .{ k, object.name, @tagName(object.kind), own.iou, own.colour, own.dx, own.dy });
            if (wrong) |w| try report.print(allocator, "; {d:.3}\n", .{w.iou}) else try report.appendSlice(allocator, "\n");
            const flat = object.kind == .flat;
            if (own.iou < (if (flat) min_flat_iou else min_iou) or own.colour > (if (flat) max_flat_colour else max_colour)) failed = true;
            if (wrong) |w| if (w.iou >= own.iou) {
                failed = true;
            };
        };
        if (placed[placed.len - 1].kind == .bridge) for (1..4) |k| {
            // The gap moved with the turn: nothing is drawn where it went.
            const at = turn.turnScreen(screenOf(bridge_ring, bridge_gap), k);
            var pixels: usize = 0;
            var y = at[1] - 60;
            while (y <= at[1] + 40) : (y += 1) {
                var x = at[0] - 80;
                while (x <= at[0] + 80) : (x += 1) {
                    if (drawn(rgb(frames[k], x, y))) pixels += 1;
                }
            }
            try report.print(allocator, "{d} bridge gap at its turned place: {d} pixels drawn\n", .{ k, pixels });
            if (pixels > 20) failed = true;
        };
    }

    // Remastered off: the key does nothing to any object.
    const off = try turn.runGame(io, allocator, game, root, "off", "all.xml", road3d_look, "OFF", objects_only, 2);
    var changed: usize = 0;
    for (0..width * height) |i| {
        if (!std.mem.eql(u8, off[0][i * 4 ..][0..3], off[1][i * 4 ..][0..3])) changed += 1;
    }
    try report.print(allocator, "off: pixels changed by the key: {d}\n", .{changed});
    if (changed != 0) failed = true;

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the world is drawn wrong at a camera step; the frames are in {s}\n", .{ report.items, root });
        return error.DrawnWrong;
    }
}

/// mod.xml and the object maps, written from road3d.xml: fences.xml,
/// props.xml, sides.xml and all.xml (the fences, the props and the bridge
/// spans).
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn world test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    const all = fences ++ props ++ sides[8..].*;
    for ([_]struct { []const u8, []const Placed }{ .{ "fences.xml", &fences }, .{ "props.xml", &props }, .{ "sides.xml", &sides }, .{ "all.xml", &all } }) |entry| {
        const name, const placed = entry;
        var items: std.ArrayList(u8) = .empty;
        try items.appendSlice(allocator, "<Objects>");
        for (placed, 1..) |object, link| {
            const at = ringPoint(object.ring, object.eighth);
            try items.print(allocator, "<item Direction=\"0\" Player=\"-1\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
                "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ object.frame, object.name, at[0], at[1], link });
        }
        try items.appendSlice(allocator, "</Objects>");
        try cwd.writeFile(io, .{
            .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", name }),
            .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
        });
    }
}

/// The middle of a VillageFence segment's footprint from its zero cross (AI
/// units), per direction, from Data/Fences/ussr/summer/villagefence/1.xml:
/// the centre segments of directions 0 and 2 are 1 x 3 cells with origin
/// (16, 16), of 1 and 3 3 x 1 with origin (80, 16) (the XML's origins are
/// world units, AI2Vis of these); a footprint starts at the zero cross less
/// the origin.
const fence_middle = [4][2]f64{ .{ 0, 32 }, .{ -32, 0 }, .{ 0, 32 }, .{ -32, 0 } };

/// Where the picture of the segment a fence of direction d shows at step k
/// (direction d + k) stands from where the turn puts its zero cross, on the
/// screen: on the own footprint turned, R^k m(d) - m(d + k) in the turned
/// view's AI frame, R being (x, y) -> (-y, x); at step 0 nothing.
fn fenceShift(d: u32, k: usize) [2]i32 {
    var m = fence_middle[d % 4];
    for (0..k) |_| {
        const was = m;
        m = .{ -was[1], was[0] };
    }
    const to = fence_middle[(d + @as(u32, @intCast(k))) % 4];
    const dx = m[0] - to[0];
    const dy = m[1] - to[1];
    return .{ @intFromFloat(@round((dx + dy) / 2)), @intFromFloat(@round((dx - dy) / 4)) };
}

test "a turned fence segment stands on its own footprint turned" {
    // Direction 0's footprint runs along AI y from its zero cross; turned
    // twice it runs the other way, 64 units (32 pixels left, 16 down) on.
    try testing.expectEqual([2]i32{ 0, 0 }, fenceShift(0, 0));
    try testing.expectEqual([2]i32{ 0, 0 }, fenceShift(0, 1));
    try testing.expectEqual([2]i32{ -32, 16 }, fenceShift(0, 2));
    try testing.expectEqual([2]i32{ 32, 16 }, fenceShift(0, 3));
}

/// A ring point in AI units around the AI point under the screen centre.
fn ringPoint(radius: f64, eighth: u32) [2]f64 {
    const middle = @as(f64, @floatFromInt(road3d_look[0])) * ai_per_world;
    const angle = @as(f64, @floatFromInt(eighth)) * std.math.pi / 4.0;
    return .{ middle + radius * @cos(angle), middle + radius * @sin(angle) };
}

/// Where the 2003 camera puts an AI point at step 0: AI x half a pixel right
/// and a quarter down, AI y half right and a quarter up (the map XML's
/// Position; building-views-game-reads-it's places fit it to the pixel).
fn screenOf(radius: f64, eighth: u32) [2]i32 {
    const middle = @as(f64, @floatFromInt(road3d_look[0])) * ai_per_world;
    const at = ringPoint(radius, eighth);
    const dx = at[0] - middle;
    const dy = at[1] - middle;
    return .{ centre[0] + @as(i32, @intFromFloat(@round((dx + dy) / 2))), centre[1] + @as(i32, @intFromFloat(@round((dx - dy) / 4))) };
}

fn rgb(frame: []const u8, x: i32, y: i32) ?[3]u8 {
    if (x < 0 or y < 0 or x >= width or y >= height) return null;
    const i: usize = @intCast((y * width + x) * 4);
    return frame[i..][0..3].*;
}

fn drawn(p: ?[3]u8) bool {
    const q = p orelse return false;
    return @max(q[0], q[1], q[2]) > 24;
}

const Score = struct { iou: f64, colour: f64, dx: i32, dy: i32 };

/// Object i of `placed` at step k against what its kind must show there
/// (`right`) or the picture that would be wrong (its own, not mirrored, not
/// turned), both cut from the step-0 frame around a zero cross; the best
/// offset within 4 pixels.
fn scoreObject(before: []const u8, after: []const u8, placed: []const Placed, i: usize, k: usize, right: bool, on_footprint: bool) Score {
    const object = placed[i];
    // A fence segment, a side of a prop and a bridge span show what stood
    // at the place they move to.
    const from_eighth = if (right and (object.kind == .fence or object.kind == .sides or object.kind == .bridge)) @as(u32, @intCast((object.eighth + 2 * k) % 8)) else object.eighth;
    const p0 = screenOf(object.ring, from_eighth);
    var pk = turn.turnScreen(screenOf(object.ring, object.eighth), k);
    if (object.kind == .fence and right and on_footprint) {
        const shift = fenceShift(object.eighth / 2, k);
        pk = .{ pk[0] + shift[0], pk[1] + shift[1] };
    }
    const reach = object.reach;
    const flat = object.kind == .flat and right;
    const mirror = object.kind == .mirrored and right and k % 2 == 1;
    // The box on the turned screen that holds the expected picture.
    var box = [4]i32{ reach[0], reach[1], reach[2], reach[3] };
    if (flat) {
        var lo = [2]f64{ 1e9, 1e9 };
        var hi = [2]f64{ -1e9, -1e9 };
        for (0..4) |c| {
            const corner = turn.turnOffset(.{ @floatFromInt(if (c & 1 == 0) reach[0] else reach[2]), @floatFromInt(if (c & 2 == 0) reach[1] else reach[3]) }, k);
            lo = .{ @min(lo[0], corner[0]), @min(lo[1], corner[1]) };
            hi = .{ @max(hi[0], corner[0]), @max(hi[1], corner[1]) };
        }
        box = .{ @intFromFloat(lo[0]), @intFromFloat(lo[1]), @intFromFloat(hi[0]), @intFromFloat(hi[1]) };
    }
    var best: Score = .{ .iou = -1, .colour = 255, .dx = 0, .dy = 0 };
    var oy: i32 = -4;
    while (oy <= 4) : (oy += 1) {
        var ox: i32 = -4;
        while (ox <= 4) : (ox += 1) {
            var both: f64 = 0;
            var either: f64 = 0;
            var colour: f64 = 0;
            var y = box[1];
            while (y <= box[3]) : (y += 1) {
                var x = box[0];
                while (x <= box[2]) : (x += 1) {
                    var d = [2]i32{ x, y };
                    if (flat) {
                        const u = turn.unturn(.{ @floatFromInt(x), @floatFromInt(y) }, k);
                        d = .{ @intFromFloat(@round(u[0])), @intFromFloat(@round(u[1])) };
                    }
                    if (mirror) d[0] = -d[0];
                    const inside = d[0] >= reach[0] and d[0] <= reach[2] and d[1] >= reach[1] and d[1] <= reach[3];
                    const sy = p0[1] + d[1];
                    const ty = pk[1] + y + oy;
                    // Under the interface in either shot: neither.
                    if (sy < turn.clear_rows[0] or sy >= turn.clear_rows[1] or ty < turn.clear_rows[0] or ty >= turn.clear_rows[1]) continue;
                    const want = if (inside) rgb(before, p0[0] + d[0], sy) else null;
                    const got = rgb(after, pk[0] + x + ox, ty);
                    const w = drawn(want);
                    const g = drawn(got);
                    if (w or g) either += 1;
                    if (w and g) {
                        both += 1;
                        for (0..3) |c| colour += @abs(@as(f64, @floatFromInt(want.?[c])) - @as(f64, @floatFromInt(got.?[c])));
                    }
                }
            }
            const iou = if (either == 0) 0 else both / either;
            if (iou > best.iou) best = .{ .iou = iou, .colour = if (both == 0) 255 else colour / (3 * both), .dx = ox, .dy = oy };
        }
    }
    return best;
}

/// The sum of horizontal over the sum of vertical brightness steps between
/// neighbouring pixels within `region` of the centre: a tile picture holds
/// twice the detail along x as along y.
fn gradientRatio(frame: []const u8, region: [2]i32) f64 {
    var gx: f64 = 0;
    var gy: f64 = 0;
    var y: i32 = -region[1];
    while (y < region[1]) : (y += 1) {
        var x: i32 = -region[0];
        while (x < region[0]) : (x += 1) {
            const p = luma(frame, centre[0] + x, centre[1] + y);
            gx += @abs(luma(frame, centre[0] + x + 1, centre[1] + y) - p);
            gy += @abs(luma(frame, centre[0] + x, centre[1] + y + 1) - p);
        }
    }
    return gx / @max(1, gy);
}

fn luma(frame: []const u8, x: i32, y: i32) f64 {
    const p = turn.pixel(frame, x, y);
    return (p[0] + p[1] + p[2]) / 3;
}

test "a ring point lands where the 2003 camera puts it, and a quarter turn takes the ring onto itself" {
    // AI x is half a pixel right and a quarter down.
    try testing.expectEqual([2]i32{ centre[0] + 260, centre[1] + 130 }, screenOf(ring, 0));
    try testing.expectEqual(turn.turnScreen(screenOf(ring, 0), 1), screenOf(ring, 2));
    try testing.expectEqual([2]f64{ 3, 5 }, turn.unturn(turn.turnOffset(.{ 3, 5 }, 3), 3));
}
