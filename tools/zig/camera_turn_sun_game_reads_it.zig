//! The sun stays in the world at every camera step (M006 round 2,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md, "The sun fixed in
//! the world"). With the Remastered option on, the turn key (`.`) turns the
//! view a quarter turn about the point at the screen centre; the light and
//! every shadow keep coming from the world's sun, so a shadow turns with the
//! ground and a building shows its turned side lit as it stands in the world.
//!
//! A map written from road3d.xml holds, on rings around the centre (the
//! point the camera turns about) that a quarter turn takes onto themselves,
//! a T-34 (a mesh), a lime tree (_Lime, a sprite with a 2003 shadow sprite), a
//! German sniper (infantry) and, on a smaller ring clear of the pause label at
//! the centre, a 2003 shed (E_FrenchShed01), two eighths apart. The shed's turns lit by
//! the world's sun (`<variant>_r<k>_l<k>`, `bk-views side` with `1l1,2l2,3l3`)
//! and lit by the 2003 light fixed to the camera (`_r<k>`) are made into a
//! scratch mod.
//!
//! - Shadows, Remastered on, the game paused (SPACE): the map with the
//!   terrain on, once with shadows and once without; an object's shadow is
//!   where the first is darker. Each object's shadow at step k must be its own
//!   step-0 shadow turned with the ground about it, on the ground neither
//!   picture of it covers (`sunShadow`: IoU at the best offset within 4
//!   pixels), better than that shadow not turned (the sun turning with the
//!   camera), and the centroids of the two must point the same way, at every
//!   step where it shows 100 pixels or more (a shadow cast behind its object
//!   may be all hidden at a step). The sniper's shadow is drawn in its 2003
//!   pictures, not by the shadow pass: reported, not held.
//! - Objects only: the shed at step k must be its world-lit turn's sprite
//!   (outline IoU and colour, as building-views-game-reads-it), closer in
//!   colour than its turn lit by the camera's light; the tree its picture
//!   relit for step k (`bk-views relight`, `1_l<k>`), closer than its 2003
//!   picture, and its lit side must follow the sun: the left half of the
//!   crown brighter than the right at step 0, the right brighter at step 2.
//! - Remastered off: the key changes no pixel of the shots with shadows.
//!
//! The Game runs from zig-out/local-test/camera-turn/sun with every user path
//! redirected (camera_turn_game_reads_it.runGame); frames, logs and scores.txt
//! stay there. BK_VIEWS_GAME names the installed Game and BK_VIEWS_TOOL
//! bk-views; `zig build camera-turn-sun-game-reads-it` sets both.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const units = @import("camera_turn_units_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/sun";
const centre = turn.centre;

/// The middle of road3d's 32 x 32 tiles.
const look = [2]u32{ 512, 512 };
const ai_per_world = std.math.sqrt2;

const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
const with_shadows = "effects,noise,warfog,haze,grid";
const without_shadows = "shadows,effects,noise,warfog,haze,grid";

const shed = "E_FrenchShed01";
const shed_folder = "Data/Buildings/europe/summer/e_frenchshed01";
const mod_buildings = "buildings/europe/summer";
const tree_folder = "Data/Objects/Flora/USSR/summer/_NEW/_Lime";
const mod_tree = "objects/flora/USSR/summer/_NEW/_Lime";

/// An object of the map: its objects.xml name, its player (-1 for none), the
/// ring and eighth it stands at, and the pixels around its place its shadow
/// may reach.
const Placed = struct { name: []const u8, player: i32, ring: f64, eighth: u32, reach: [4]i32 };
const ring = 560.0;
const placed = [_]Placed{
    .{ .name = shed, .player = -1, .ring = 400, .eighth = 6, .reach = .{ -130, -150, 130, 90 } },
    .{ .name = "T-34", .player = 0, .ring = ring, .eighth = 0, .reach = .{ -90, -80, 90, 70 } },
    .{ .name = "_Lime", .player = -1, .ring = ring, .eighth = 2, .reach = .{ -90, -170, 90, 60 } },
    .{ .name = "German_sniper", .player = 0, .ring = ring, .eighth = 4, .reach = .{ -40, -50, 40, 30 } },
};

/// Floors. A shadow seen at step k is its step-0 shadow turned; the
/// threshold that finds it (darker by 30 over the three channels) cuts it
/// differently on grass and on the road it may be turned onto (a T-34's
/// at step 2: 0.40 to 0.46), so the floor is low and the shadow not turned
/// must score lower. A shadow shows min_shadow_pixels or more to be held.
const min_shadow_iou: f64 = 0.35;
const min_shadow_pixels = 100;
/// building-views-game-reads-it's floors: the Game draws the packs' pixels unchanged.
const min_iou = 0.95;
const max_colour = 2.0;
/// A crown's soft edge blends with the black backdrop: the Game draws the
/// 2003 tree at 3.09 from its decomposed picture.
const max_tree_colour = 4.0;

test "the sun stays in the world: shadows of a building, a tank, a tree and a soldier turn with the ground, the building is lit as it stands" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-sun-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const turned = try std.fs.path.join(allocator, &.{ data, mod_buildings });
    try cwd.createDirPath(io, turned);
    try game_views.runTool(io, allocator, tool, &.{ "side", shed_folder, "1,2,3,1l1,2l2,3l3", turned, try std.fs.path.join(allocator, &.{ root, "work" }) });
    const relit = try std.fs.path.join(allocator, &.{ data, mod_tree });
    try game_views.runTool(io, allocator, tool, &.{ "relight", tree_folder, relit });
    try writeMod(io, allocator, data);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;

    // The shed: step k draws its turn lit by the world's sun.
    const base = std.fs.path.basename(shed_folder);
    var lit: [4]game_views.Sprite = undefined;
    var fixed: [4]game_views.Sprite = undefined;
    for (0..4) |k| {
        lit[k] = try game_views.decompose(io, allocator, tool, if (k == 0) shed_folder else try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}_l{d}", .{ turned, base, k, k }), try std.fmt.allocPrint(allocator, "{s}/sprites/lit{d}", .{ root, k }));
        fixed[k] = try game_views.decompose(io, allocator, tool, if (k == 0) shed_folder else try std.fmt.allocPrint(allocator, "{s}/{s}_r{d}", .{ turned, base, k }), try std.fmt.allocPrint(allocator, "{s}/sprites/fixed{d}", .{ root, k }));
    }
    try report.appendSlice(allocator, "shed: step: lit by the world's sun IoU / colour (offset); lit by the camera's light IoU / colour\n");
    const objects = try turn.runGameWith(io, allocator, game, root, "objects", "sun.xml", look, "ON", objects_only, 4, true);
    for (objects, 0..) |frame, k| {
        const at = screenOf(placed[0].ring, (placed[0].eighth + 2 * @as(u32, @intCast(k))) % 8);
        const own = game_views.score(frame, lit[k], at);
        const other = game_views.score(frame, fixed[k], at);
        try report.print(allocator, "{d}: {d:.3} / {d:.2} ({d},{d}); {d:.3} / {d:.2}\n", .{ k, own.iou, own.colour, own.dx, own.dy, other.iou, other.colour });
        if (own.iou < min_iou or own.colour > max_colour or (k != 0 and other.colour <= own.colour)) failed = true;
    }

    // The tree: step k draws its picture relit for step k, its lit side on the sun's side.
    var tree: [4]game_views.Sprite = undefined;
    tree[0] = try game_views.decompose(io, allocator, tool, tree_folder, try std.fmt.allocPrint(allocator, "{s}/sprites/tree", .{root}));
    try game_views.runTool(io, allocator, tool, &.{ "decompose", relit, try std.fmt.allocPrint(allocator, "{s}/sprites/tree-lit", .{root}) });
    for (1..4) |k| tree[k] = try game_views.decomposed(io, allocator, try std.fmt.allocPrint(allocator, "{s}/sprites/tree-lit", .{root}), try std.fmt.allocPrint(allocator, "1_l{d}", .{k}));
    try report.appendSlice(allocator, "tree: step: relit IoU / colour (offset); 2003 picture IoU / colour; crown left against right\n");
    var sides: [4]f64 = undefined;
    for (objects, 0..) |frame, k| {
        const at = screenOf(placed[2].ring, (placed[2].eighth + 2 * @as(u32, @intCast(k))) % 8);
        const own = game_views.score(frame, tree[k], at);
        const old = game_views.score(frame, tree[0], at);
        sides[k] = leftAgainstRight(frame, tree[k], .{ at[0] + own.dx, at[1] + own.dy });
        try report.print(allocator, "{d}: {d:.3} / {d:.2} ({d},{d}); {d:.3} / {d:.2}; {d:.3}\n", .{ k, own.iou, own.colour, own.dx, own.dy, old.iou, old.colour, sides[k] });
        if (own.iou < min_iou or own.colour > max_tree_colour or (k != 0 and old.colour <= own.colour)) failed = true;
    }
    if (sides[0] <= 1 or sides[2] >= 1) failed = true;

    // Shadows: where the frame with shadows is darker than the one without.
    try report.appendSlice(allocator, "shadows: step name: IoU against its own step-0 shadow turned / not turned; centroids apart (degrees); pixels\n");
    const shadowed = try turn.runGameWith(io, allocator, game, root, "shadows", "sun.xml", look, "ON", with_shadows, 4, true);
    const plain = try turn.runGameWith(io, allocator, game, root, "no-shadows", "sun.xml", look, "ON", without_shadows, 4, true);
    var held: [placed.len]usize = @splat(0);
    for (1..4) |k| for (placed, 0..) |object, i| {
        // The object that stood at this one's turned place at step 0 is the next one on the ring.
        const from = screenOf(object.ring, object.eighth);
        const to = screenOf(object.ring, (object.eighth + 2 * @as(u32, @intCast(k))) % 8);
        const world = units.sunShadow(.{ shadowed[0], plain[0], objects[0] }, from, .{ shadowed[k], plain[k], objects[k] }, to, k, object.reach);
        const screen = units.sunShadow(.{ shadowed[0], plain[0], objects[0] }, from, .{ shadowed[k], plain[k], objects[k] }, to, 0, object.reach);
        try report.print(allocator, "{d} {s}: {d:.3} / {d:.3}; {d:.1}; {d}\n", .{ k, object.name, world.iou, screen.iou, world.off_degrees, world.pixels });
        if (std.mem.eql(u8, object.name, "German_sniper")) continue;
        if (world.pixels >= min_shadow_pixels) {
            held[i] += 1;
            if (world.iou < min_shadow_iou or world.iou <= screen.iou or world.off_degrees > units.max_shadow_degrees) failed = true;
        }
    };

    for (placed, held) |object, n| {
        try report.print(allocator, "{s}: held at {d} steps\n", .{ object.name, n });
        if (n == 0 and !std.mem.eql(u8, object.name, "German_sniper")) failed = true;
    }

    // Remastered off: the key changes nothing between the interface's rows.
    const off = try turn.runGameWith(io, allocator, game, root, "off", "sun.xml", look, "OFF", with_shadows, 2, true);
    var changed: usize = 0;
    for (@intCast(turn.clear_rows[0] * turn.width)..@intCast(turn.clear_rows[1] * turn.width)) |p| {
        if (!std.mem.eql(u8, off[0][p * 4 ..][0..3], off[1][p * 4 ..][0..3])) changed += 1;
    }
    try report.print(allocator, "off: pixels changed by the key: {d}\n", .{changed});
    if (changed != 0) failed = true;

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the light or a shadow does not stay with the world's sun; the frames are in {s}\n", .{ report.items, root });
        return error.DrawnWrong;
    }
}

/// The mean brightness of the frame over the sprite's left half against its
/// right half (split at its zero cross's column), the sprite's zero cross at
/// `at`.
fn leftAgainstRight(frame: []const u8, sprite: game_views.Sprite, at: [2]i32) f64 {
    var sums = [2]f64{ 0, 0 };
    var counts = [2]f64{ 0, 0 };
    for (sprite.image.pixels, 0..) |p, i| {
        if (p[3] < 128) continue;
        const sx: i32 = @intCast(i % sprite.image.width);
        const sy: i32 = @intCast(i / sprite.image.width);
        const x = at[0] + sx - sprite.anchor[0];
        const y = at[1] + sy - sprite.anchor[1];
        if (x < 0 or y < 0 or x >= turn.width or y >= turn.height) continue;
        const q = turn.pixel(frame, x, y);
        const side: usize = @intFromBool(sx >= sprite.anchor[0]);
        sums[side] += q[0] + q[1] + q[2];
        counts[side] += 1;
    }
    return (sums[0] / @max(1, counts[0])) / @max(1, sums[1] / @max(1, counts[1]));
}

/// mod.xml and maps/sun.xml, written from road3d.xml.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn sun test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    var items: std.ArrayList(u8) = .empty;
    try items.appendSlice(allocator, "<Objects>");
    for (placed, 1..) |object, link| {
        const at = ringPoint(object.ring, object.eighth);
        // Units face along the ring, as in camera-turn-units-game-reads-it;
        // a squad's FrameIndex is its formation (0), a unit's and a static
        // object's unused.
        const unit = object.player >= 0;
        const infantry = std.mem.eql(u8, object.name, "German_sniper");
        try items.print(allocator, "<item Direction=\"{d}\" Player=\"{d}\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ if (unit) object.eighth * 8192 else 0, object.player, @as(i32, if (infantry) 0 else -1), object.name, at[0], at[1], link });
    }
    try items.appendSlice(allocator, "</Objects>");
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "sun.xml" }),
        .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
    });
}

/// A ring point in AI units around the AI point under the screen centre.
fn ringPoint(radius: f64, eighth: u32) [2]f64 {
    const middle = @as(f64, @floatFromInt(look[0])) * ai_per_world;
    const angle = @as(f64, @floatFromInt(eighth)) * std.math.pi / 4.0;
    return .{ middle + radius * @cos(angle), middle + radius * @sin(angle) };
}

/// Where the 2003 camera puts a ring point at step 0 (AI x half a pixel right
/// and a quarter down, AI y half right and a quarter up).
fn screenOf(radius: f64, eighth: u32) [2]i32 {
    const middle = @as(f64, @floatFromInt(look[0])) * ai_per_world;
    const at = ringPoint(radius, eighth);
    const dx = at[0] - middle;
    const dy = at[1] - middle;
    return .{ centre[0] + @as(i32, @intFromFloat(@round((dx + dy) / 2))), centre[1] + @as(i32, @intFromFloat(@round((dx - dy) / 4))) };
}

test "a quarter turn takes the ring onto itself, two eighths on" {
    try testing.expectEqual(turn.turnScreen(screenOf(ring, 0), 1), screenOf(ring, 2));
    try testing.expectEqual(turn.turnScreen(screenOf(400, 6), 1), screenOf(400, 0));
}
