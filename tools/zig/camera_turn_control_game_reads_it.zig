//! The controls at every camera step (M006 S05,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md). With the
//! Remastered option on and the view turned by the turn keys (`.` and `,`),
//! the real Game is driven by BK_AUTO_UI and measured by code:
//!
//! - control: a map written from road3d.xml with three German snipers of
//!   player 0 on a ring (eighths 0, 2 and 4), the mirrored Swing01 and the
//!   house E_House01_1; objects only. At each step the shot is held against the
//!   picks of a grid of points over each object (`pick=`, CWorldClient::TracePick):
//!   every grid point well inside the object's drawn picture must pick it, the
//!   mirrored swing too, part of which lies off its 2003 outline at an odd
//!   step. A click where sniper 0 is drawn must select it (at step 1 that is
//!   where sniper 2 stood at step 0), a frame drawn around where snipers 0 and
//!   2 are drawn must select exactly those two. The arrow key, the screen's
//!   right edge and a trackpad swipe must move the view along the screen's
//!   right as the turn lays it on the ground. At the last step sniper 0 is
//!   ordered to a ground point clicked where the turn draws it and must walk
//!   there.
//! - minimap: the shipped coldwinter (large enough that the view frame lies
//!   inside the map), the view on the map's middle: at every step the view
//!   frame must keep its step-0 shape (the picture turns with the view, so the
//!   frame stays upright) and the picture must be the step-0 picture turned; a
//!   click on the minimap must bring the view frame to the clicked point.
//! - hill: a map with a hill; the ground point under the screen's centre must
//!   stay where it was through three turns and one back (`,`), where the 2003
//!   pivot (the anchor at height 0) moves it.
//!   S06: at every step a click finds the ground there (`ground=`,
//!   CScene::GetPos3 through GFXGPU's GetViewVolumeCrosses): at the centre the
//!   pivot's own ground, on the slope a point of the hill the 2003 camera draws
//!   at the clicked pixel; at step 0 a sniper ordered there walks to it, not
//!   to the point at height 0 the port answered before.
//! - draw order: four clusters of a shed, four snipers in front of and behind
//!   it and four trees; the one in the middle is shot at the four steps, the
//!   others, the middle one turned 1, 2 and 3 times (places, unit facings and
//!   the shed's own facing), at step 0. Step k of the first must be the k-th of
//!   the others pixel for pixel: what is in front stays in front.
//!
//! The Game runs from zig-out/local-test/camera-turn/control with every user
//! path redirected; frames, logs and scores.txt stay there. BK_VIEWS_GAME names
//! the installed Game and BK_VIEWS_TOOL bk-views; `zig build
//! camera-turn-control-game-reads-it` sets both.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const views = @import("bk_views.zig");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/control";
const mod_name = "camera_turn_control";
const width = turn.width;
const height = turn.height;
const centre = turn.centre;
/// AI units of the map XML per world unit, and road3d's middle in AI units.
const ai_per_world = std.math.sqrt2;
pub const middle = 512.0 * ai_per_world;
const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";

/// Where an offset (AI units) from the AI point under the screen centre is
/// drawn at step k: the 2003 camera puts AI x half a pixel right and a
/// quarter down, AI y half right and a quarter up; a step turns the screen
/// offset as the world turns (turn.turnOffset).
pub fn screenAt(d: [2]f64, k: usize) [2]i32 {
    const s = turn.turnOffset(.{ (d[0] + d[1]) / 2, (d[0] - d[1]) / 4 }, k);
    return .{ centre[0] + @as(i32, @intFromFloat(@round(s[0]))), centre[1] + @as(i32, @intFromFloat(@round(s[1]))) };
}

/// The world unit vector of the screen's right at step k: world (1, 1) at
/// step 0, and the view at step k shows the world turned k times by
/// (x, y) -> (-y, x), so the screen's right is (1, 1) turned back k times.
fn screenRight(k: usize) [2]f64 {
    var v = [2]f64{ std.math.sqrt1_2, std.math.sqrt1_2 };
    for (0..k) |_| {
        const x = v[0];
        v = .{ v[1], -x };
    }
    return v;
}

const ring = 180.0;
fn ringOffset(eighth: u32) [2]f64 {
    const angle = @as(f64, @floatFromInt(eighth)) * std.math.pi / 4.0;
    return .{ ring * @cos(angle), ring * @sin(angle) };
}
const snipers = [_]u32{ 0, 2, 4 };
const swing_at = [2]f64{ 350, -350 };
const house_at = [2]f64{ -380, -380 };
/// The ground point sniper 0 is ordered to at step 3.
const order_at = [2]f64{ -150, 380 };

/// A pick grid: `half` pixels each way of the point `up` pixels above the
/// zero cross, every `pitch` pixels.
const Grid = struct { name: []const u8, at: [2]f64, up: i32, half: i32, pitch: i32 };
const grids = [_]Grid{
    .{ .name = "Swing01", .at = swing_at, .up = 20, .half = 40, .pitch = 2 },
    .{ .name = "E_House01_1", .at = house_at, .up = 60, .half = 96, .pitch = 6 },
    .{ .name = "German_Sniper", .at = .{ ring, 0 }, .up = 14, .half = 24, .pitch = 2 },
};
fn gridCount(grid: Grid) usize {
    return @intCast(@divExact(2 * grid.half, grid.pitch) + 1);
}
fn gridOrigin(grid: Grid, k: usize) [2]i32 {
    const at = screenAt(grid.at, k);
    return .{ at[0] - grid.half, at[1] - grid.up - grid.half };
}

/// The minimap test's map: the shipped coldwinter, 96 tiles a side (a world
/// cell is 32 sqrt 2 units), large enough that the view frame lies inside it.
const minimap_map = "Multiplayer\\coldwinter.bzm";
const minimap_look = 96.0 * 32.0 * std.math.sqrt2 / 2.0;
/// The minimap's middle on a 1024 x 768 window (the view frame's middle with
/// the view on the map's middle; measured, and checked in each run) and the
/// minimap offset clicked at step 0, turned at each step as the picture turns:
/// the minimap lays the map out as the 2003 camera does, so the turn is
/// turn.turnOffset there too.
const minimap_middle = [2]i32{ 133, 678 };
const minimap_click = [2]f64{ 40, 8 };
fn minimapClick(k: usize) [2]i32 {
    const d = turn.turnOffset(minimap_click, k);
    return .{ minimap_middle[0] + @as(i32, @intFromFloat(@round(d[0]))), minimap_middle[1] + @as(i32, @intFromFloat(@round(d[1]))) };
}
const minimap_box = [4]i32{ 0, 610, 270, 760 };
const frame_colour = [3]u8{ 175, 175, 175 };
const shadow_colour = [3]u8{ 79, 79, 79 };

/// Floors. Every grid point on a drawn pixel of an object picks it (a sprite
/// is picked by its rectangle, which holds its picture); a step's minimap
/// frame lies on the step-0 frame turned; a minimap click and a turn keep
/// their ground within a few world units (the view snaps to whole screen
/// steps); a scroll goes within a few degrees of the screen's right.
const min_pick_recall = 0.97;
const min_frame_iou = 0.8;
const max_ground_shift = 6.0;
const min_scroll_cos = 0.98;
const max_order_miss = 16.0;
const max_cluster_differ = 0.02;
/// The minimap's picture at a step against step 0's turned: the picture
/// itself turned, so nearly the same pixels (its edges and the frame differ);
/// against step 0's not turned it must be much further.
const max_minimap_difference = 20.0;
const min_minimap_margin = 0.6;

test "a ring point and the screen's right turn as the view does" {
    try testing.expectEqual(screenAt(ringOffset(2), 0), screenAt(ringOffset(0), 1));
    const r = screenRight(1);
    try testing.expectApproxEqAbs(@as(f64, std.math.sqrt1_2), r[0], 1e-9);
    try testing.expectApproxEqAbs(@as(f64, -std.math.sqrt1_2), r[1], 1e-9);
    try testing.expectEqual([2]i32{ 173, 686 }, minimapClick(0));
    try testing.expectEqual([2]i32{ 149, 658 }, minimapClick(1));
}

test "picking, selection, orders, scrolling, the minimap, the pivot and draw order at every camera step" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-control-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const turned = try std.fs.path.join(allocator, &.{ data, "buildings/europe/summer" });
    try cwd.createDirPath(io, turned);
    try game_views.runTool(io, allocator, tool, &.{ "side", "Data/Buildings/europe/summer/e_frenchshed01", "1,2,3", turned, try std.fs.path.join(allocator, &.{ root, "work" }) });
    try writeMod(io, allocator, data);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;

    // control
    {
        const run = try runSchedule(io, allocator, game, root, mod_name, "control", "control.xml", try controlSchedule(allocator), objects_only);
        const picks = try lines(allocator, run.log, "BK_PICK_TRACE: ");
        const anchors = try parseAnchors(allocator, run.log);
        const selections = try parseSelections(allocator, run.log);
        if (anchors.len != 4 * 6 or selections.len != 4 * 2 + 1 or picks.len != 4 * pickCount()) {
            std.debug.print("control: {d} anchors, {d} selections, {d} picks; the Game's output is in {s}\n", .{ anchors.len, selections.len, picks.len, run.log_path });
            return error.MissingTrace;
        }
        var frames: [4][]u8 = undefined;
        for (0..4) |k| frames[k] = try readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "g{d}", .{800 + k}), root, try std.fmt.allocPrint(allocator, "control-{d}", .{k}));

        try report.appendSlice(allocator, "picks: step object: grid points inside its picture picking it / grid points inside its picture (recall), of them outside its step-0 picture's box (what a pick by the 2003 outline misses); points picking it off its drawn box grown by 8 pixels\n");
        var line: usize = 0;
        for (0..4) |k| for (grids) |grid| {
            const origin = gridOrigin(grid, k);
            const n = gridCount(grid);
            var inside: usize = 0;
            var hit: usize = 0;
            var stray: usize = 0;
            var outside_2003: usize = 0;
            const box = drawnBox(frames[k], origin, n, grid.pitch);
            // The step-0 picture's box around its zero cross, laid at this step's zero cross.
            const box0 = drawnBox(frames[0], gridOrigin(grid, 0), n, grid.pitch);
            const at0 = screenAt(grid.at, 0);
            const at = screenAt(grid.at, k);
            for (0..n) |row| for (0..n) |column| {
                const x = origin[0] + @as(i32, @intCast(column)) * grid.pitch;
                const y = origin[1] + @as(i32, @intCast(row)) * grid.pitch;
                const entry = picks[line];
                line += 1;
                const picked = std.ascii.findIgnoreCase(entry, grid.name) != null;
                if (y - 2 < turn.clear_rows[0] or y + 2 >= turn.clear_rows[1]) continue;
                if (drawnInside(frames[k], x, y)) {
                    inside += 1;
                    if (picked) hit += 1;
                    const rx = x - at[0] + at0[0];
                    const ry = y - at[1] + at0[1];
                    if (rx < box0[0] or rx > box0[2] or ry < box0[1] or ry > box0[3]) outside_2003 += 1;
                } else if (picked and (x < box[0] - 8 or x > box[2] + 8 or y < box[1] - 8 or y > box[3] + 8)) stray += 1;
            };
            const recall = if (inside == 0) 0 else @as(f64, @floatFromInt(hit)) / @as(f64, @floatFromInt(inside));
            try report.print(allocator, "{d} {s}: {d} / {d} ({d:.3}), {d}; {d}\n", .{ k, grid.name, hit, inside, recall, outside_2003, stray });
            if (inside < 8 or recall < min_pick_recall) failed = true;
            // The swing is mirrored at an odd step: part of what is drawn lies off its 2003 outline.
            if (std.mem.eql(u8, grid.name, "Swing01") and k % 2 == 1 and outside_2003 < 3) failed = true;
        };

        try report.appendSlice(allocator, "selection: step: clicked where sniper 0 is drawn -> selected; framed snipers 0 and 2 -> selected\n");
        for (0..4) |k| {
            const clicked = selections[2 * k];
            const framed = selections[2 * k + 1];
            try report.print(allocator, "{d}: {s}; {s}\n", .{ k, try placesText(allocator, clicked.places), try placesText(allocator, framed.places) });
            if (!sameSet(clicked.places, &.{0})) failed = true;
            if (!sameSet(framed.places, &.{ 0, 2 })) failed = true;
        }
        const order = selections[8];
        const target = worldOf(order_at);
        const miss = if (order.places.len == 1) std.math.hypot(order.places[0][0] - target[0], order.places[0][1] - target[1]) else 1e9;
        const clicked_at = screenAt(order_at, 3);
        try report.print(allocator, "order at step 3, clicked at {d},{d}, to {d:.0},{d:.0}: sniper 0 at {s}, {d:.1} away\n", .{ clicked_at[0], clicked_at[1], target[0], target[1], try placesText(allocator, order.places), miss });
        if (miss > max_order_miss) failed = true;

        try report.appendSlice(allocator, "scroll: step: key / edge / swipe: world move (cos to the screen's right)\n");
        for (0..4) |k| {
            const a = anchors[6 * k ..][0..6];
            const right = screenRight(k);
            try report.print(allocator, "{d}:", .{k});
            for (0..3) |s| {
                const dx = a[1 + 2 * s].anchor[0] - a[2 * s].anchor[0];
                const dy = a[1 + 2 * s].anchor[1] - a[2 * s].anchor[1];
                const length = std.math.hypot(dx, dy);
                const cos = if (length == 0) 0 else (dx * right[0] + dy * right[1]) / length;
                try report.print(allocator, " {d:.1},{d:.1} ({d:.3})", .{ dx, dy, cos });
                if (length < 1 or cos < min_scroll_cos) failed = true;
            }
            try report.appendSlice(allocator, "\n");
        }
    }

    // minimap: coldwinter, the view on the map's middle at every step, then
    // a click on the minimap where the turn takes the step-0 click.
    {
        var schedule: std.ArrayList(u8) = .empty;
        try schedule.print(allocator, "g1000:hover=1000x400,g10:camera={d:.0}x{d:.0},g10:set=GamePlay.Remastered=ON", .{ minimap_look, minimap_look });
        for (0..4) |k| {
            const m = minimapClick(k);
            try schedule.print(allocator, ",g{d}:shot,g10:click={d}x{d},g300:anchor,g{d}:shot,g10:camera={d:.0}x{d:.0},g300:key=.", .{ 800 + k, m[0], m[1], 200 + k, minimap_look, minimap_look });
        }
        try schedule.appendSlice(allocator, ",g300:exit");
        const run = try runSchedule(io, allocator, game, root, mod_name, "minimap", minimap_map, schedule.items, "");
        const anchors = try parseAnchors(allocator, run.log);
        if (anchors.len != 4) return error.MissingTrace;
        var middle_shots: [4][]u8 = undefined;
        var on_middle: [4]Mask = undefined;
        var moved: [4]Mask = undefined;
        for (0..4) |k| {
            middle_shots[k] = try readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "g{d}", .{800 + k}), root, try std.fmt.allocPrint(allocator, "minimap-middle-{d}", .{k}));
            on_middle[k] = try frameMask(allocator, middle_shots[k]);
            moved[k] = try frameMask(allocator, try readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "g{d}", .{200 + k}), root, try std.fmt.allocPrint(allocator, "minimap-clicked-{d}", .{k})));
        }
        const found_middle = maskMiddle(on_middle[0]);
        try report.print(allocator, "minimap: the view frame's middle {d},{d} (expected {d},{d})\n", .{ found_middle[0], found_middle[1], minimap_middle[0], minimap_middle[1] });
        if (@abs(found_middle[0] - minimap_middle[0]) > 2 or @abs(found_middle[1] - minimap_middle[1]) > 2) failed = true;
        try report.appendSlice(allocator, "  step: the frame on the map's middle against step 0's (IoU, pixels); the picture against step 0's turned / not turned (mean RGB difference); clicked x,y -> the frame's middle there, the view's ground\n");
        for (0..4) |k| {
            const same = maskIou(on_middle[0], on_middle[k]);
            const picture = minimapDifference(middle_shots[0], middle_shots[k], k);
            const picture_still = minimapDifference(middle_shots[0], middle_shots[k], 0);
            const click = minimapClick(k);
            const landed = maskMiddle(moved[k]);
            const ground = anchors[k].anchor;
            try report.print(allocator, "  {d}: {d:.3} ({d}); {d:.2} / {d:.2}; {d},{d} -> {d},{d}, {d:.0},{d:.0}\n", .{ k, same, on_middle[k].items.len, picture, picture_still, click[0], click[1], landed[0], landed[1], ground[0], ground[1] });
            if (on_middle[k].items.len < 100 or same < min_frame_iou) failed = true;
            if (@abs(landed[0] - click[0]) > 2 or @abs(landed[1] - click[1]) > 2) failed = true;
            if (picture > max_minimap_difference or (k != 0 and picture > min_minimap_margin * picture_still)) failed = true;
        }
    }

    // hill: the ground under the screen's centre through the turns.
    {
        // With each anchor the ground at the centre and at a point on the
        // slope (S06: GetViewVolumeCrosses); back at step 0 the sniper is
        // framed and ordered to the slope point.
        const ground_trace = ",g10:ground=511x383,g10:ground=" ++ std.fmt.comptimePrint("{d}x{d}", .{ slope_click[0], slope_click[1] });
        const run = try runSchedule(io, allocator, game, root, mod_name, "hill", "hill.xml", "g1000:hover=1000x400,g10:camera=382x512,g10:set=GamePlay.Remastered=ON,g500:anchor" ++ ground_trace ++ ",g10:key=.,g300:anchor" ++ ground_trace ++ ",g10:key=.,g300:anchor" ++ ground_trace ++ ",g10:key=.,g300:anchor" ++ ground_trace ++ ",g10:key=COMMA,g300:anchor" ++ ground_trace ++
            ",g10:key=COMMA,g300:key=COMMA,g300:anchor" ++ ground_trace ++ ",g10:drag=40x100x1000x590,g500:rclick=" ++ std.fmt.comptimePrint("{d}x{d}", .{ slope_click[0], slope_click[1] }) ++ ",g12000:world=TraceSelection(0),g300:exit", objects_only);
        const anchors = try parseAnchors(allocator, run.log);
        const grounds = try parseGrounds(allocator, run.log);
        const selections = try parseSelections(allocator, run.log);
        if (anchors.len != 6 or grounds.len != 12 or selections.len != 1 or selections[0].places.len != 1) return error.MissingTrace;
        const steps = [_]i32{ 0, 1, 2, 3, 2, 0 };
        const g0 = anchors[0].ground;
        const drift = std.math.sqrt2 * std.math.hypot(anchors[0].anchor[0] - g0[0], anchors[0].anchor[1] - g0[1]);
        try report.print(allocator, "hill: ground under the centre {d:.1},{d:.1} at height {d:.1}; the 2003 pivot would move it {d:.1} a turn\n", .{ g0[0], g0[1], g0[2], drift });
        if (g0[2] < 20 or drift < 40) failed = true;
        for (anchors, steps) |a, step| {
            const shift = std.math.hypot(a.ground[0] - g0[0], a.ground[1] - g0[1]);
            try report.print(allocator, "  step {d}: anchor {d:.1},{d:.1}, ground {d:.1},{d:.1},{d:.1}, {d:.1} from step 0's\n", .{ a.step, a.anchor[0], a.anchor[1], a.ground[0], a.ground[1], a.ground[2], shift });
            if (a.step != step or shift > max_ground_shift) failed = true;
        }
        // The ground a click finds (CScene::GetPos3): at the centre the point
        // GroundUnderCentre finds by its own walk down the line of sight (to the
        // AI's height steps); on the slope a point above the plane at height 0
        // (where the plane answered before) that the turned 2003 camera draws at
        // the clicked pixel.
        try report.appendSlice(allocator, "hill clicks: step: the centre's ground from GroundUnderCentre's; the slope's ground x,y,z, drawn at (pixels from the click)\n");
        for (anchors, 0..) |a, i| {
            const at_centre = grounds[2 * i];
            const slope = grounds[2 * i + 1];
            const off = std.math.hypot(at_centre[0] - a.ground[0], at_centre[1] - a.ground[1]);
            const drawn_at = projectGround(slope, a.anchor, @intCast(a.step));
            const miss = std.math.hypot(drawn_at[0] - @as(f64, @floatFromInt(slope_click[0])), drawn_at[1] - @as(f64, @floatFromInt(slope_click[1])));
            try report.print(allocator, "  {d}: {d:.1}; {d:.1},{d:.1},{d:.1} ({d:.1})\n", .{ a.step, off, slope[0], slope[1], slope[2], miss });
            if (off > 3 or slope[2] < 1 or miss > 2) failed = true;
        }
        // Step 0, the 2003 view: the sniper ordered to the slope point walks to
        // the ground there, not to where the line of sight meets height 0.
        const target = grounds[grounds.len - 1];
        const a0 = anchors[anchors.len - 1];
        const plane = planeGround(a0.anchor);
        const walked = selections[0].places[0];
        const to_target = std.math.hypot(walked[0] - target[0], walked[1] - target[1]);
        const to_plane = std.math.hypot(walked[0] - plane[0], walked[1] - plane[1]);
        try report.print(allocator, "hill order at step {d}: the sniper at {d:.0},{d:.0}, {d:.1} from the clicked ground, {d:.1} from the plane point {d:.0},{d:.0}\n", .{ a0.step, walked[0], walked[1], to_target, to_plane, plane[0], plane[1] });
        if (a0.step != 0 or to_target > 15 or to_plane < 40) failed = true;
    }

    // draw order: the middle cluster at steps 0..3, then the others at step 0.
    {
        var schedule: std.ArrayList(u8) = .empty;
        try schedule.appendSlice(allocator, "g1000:hover=1000x400,g10:camera=724x724,g10:set=GamePlay.Remastered=ON,g10:key=SPACE");
        for (0..4) |k| {
            if (k != 0) try schedule.appendSlice(allocator, ",t10:key=.");
            try schedule.print(allocator, ",t{d}:shot", .{1500 + k});
        }
        try schedule.appendSlice(allocator, ",t10:key=.");
        for (clusters[1..], 1..) |c, j| try schedule.print(allocator, ",t10:camera={d:.0}x{d:.0},t{d}:shot", .{ (middle + c[0]) / ai_per_world, (middle + c[1]) / ai_per_world, 1600 + j });
        try schedule.appendSlice(allocator, ",t200:exit");
        const run = try runSchedule(io, allocator, game, root, mod_name, "order", "clusters.xml", schedule.items, objects_only);
        var middle_frames: [4][]u8 = undefined;
        for (0..4) |k| middle_frames[k] = try readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "t{d}", .{1500 + k}), root, try std.fmt.allocPrint(allocator, "order-{d}", .{k}));
        try report.appendSlice(allocator, "draw order: step: pixels differing from the cluster with the shed's facing turned, at step 0 / drawn pixels (best offset); against the step-0 shot\n");
        for (1..4) |k| {
            const other = try readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "t{d}", .{1600 + k}), root, try std.fmt.allocPrint(allocator, "order-facing-{d}", .{k}));
            const against = compareCluster(middle_frames[k], other);
            const unturned = compareCluster(middle_frames[k], middle_frames[0]);
            try report.print(allocator, "{d}: {d} / {d} ({d:.4}) at {d},{d}; {d:.4}\n", .{ k, against.differ, against.drawn, against.fraction(), against.dx, against.dy, unturned.fraction() });
            if (against.drawn < 2000 or against.fraction() > max_cluster_differ) failed = true;
        }
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a control is wrong at a camera step; the frames and logs are in {s}\n", .{ report.items, root });
        return error.ControlWrong;
    }
}

fn pickCount() usize {
    var count: usize = 0;
    for (grids) |grid| count += gridCount(grid) * gridCount(grid);
    return count;
}

/// The world point (the selection trace's units) of an AI offset from the middle.
fn worldOf(d: [2]f64) [2]f64 {
    return .{ (middle + d[0]) / ai_per_world, (middle + d[1]) / ai_per_world };
}

fn controlSchedule(allocator: std.mem.Allocator) ![]const u8 {
    var s: std.ArrayList(u8) = .empty;
    try s.appendSlice(allocator, "g1000:hover=1000x400,g10:camera=512x512,g10:set=GamePlay.Remastered=ON");
    for (0..4) |k| {
        try s.print(allocator, ",g{d}:shot", .{800 + k});
        // Sniper 0 clicked where it is drawn, then framed with sniper 2.
        const s0 = screenAt(ringOffset(0), k);
        const s2 = screenAt(ringOffset(2), k);
        try s.print(allocator, ",g10:click={d}x{d},g300:world=TraceSelection(0)", .{ s0[0], s0[1] - 12 });
        try s.print(allocator, ",g10:drag={d}x{d}x{d}x{d},g500:world=TraceSelection(0)", .{ @min(s0[0], s2[0]) - 25, @min(s0[1], s2[1]) - 45, @max(s0[0], s2[0]) + 25, @max(s0[1], s2[1]) + 15 });
        // The scrolls, each from the same place.
        try s.appendSlice(allocator, ",g300:anchor,g10:key=RIGHT*10,g500:anchor,g10:camera=512x512");
        try s.appendSlice(allocator, ",g300:anchor,g10:hover=1023x384,g300:hover=512x200,g10:anchor,g10:camera=512x512");
        try s.appendSlice(allocator, ",g300:anchor,g10:swipe=0.3x0,g300:anchor,g10:camera=512x512");
        // The pick grids last: a frame that answers thousands of picks is long,
        // and the game time it takes would let a wait after a click pass before
        // the click is taken.
        for (grids) |grid| {
            const o = gridOrigin(grid, k);
            try s.print(allocator, ",g300:pick={d}x{d}x{d}x{d}", .{ o[0], o[1], grid.pitch, gridCount(grid) });
        }
        // The game time those frames take passes here, not between a click and what follows it.
        try s.appendSlice(allocator, ",g500:hover=512x200");
        if (k != 3) try s.appendSlice(allocator, ",g300:key=.");
    }
    const s0 = screenAt(ringOffset(0), 3);
    const to = screenAt(order_at, 3);
    try s.print(allocator, ",g300:click={d}x{d},g300:rclick={d}x{d},g9000:world=TraceSelection(0),g10:exit", .{ s0[0], s0[1] - 12, to[0], to[1] });
    return s.items;
}

/// The clusters of the draw-order map (AI offsets from the middle): the
/// middle one with the shed at facing 0, the others at facing 1, 2 and 3.
/// The middle one stands on the map's middle (AI 1024, 1024), the others 700
/// AI units along each axis away, so that no other cluster comes near the
/// compared box at any step.
const clusters = [_][2]f64{ .{ 300, 300 }, .{ -400, -400 }, .{ 1000, 1000 }, .{ -400, 1000 } };
/// A cluster (AI offsets from its middle): the shed, four snipers around where
/// its picture is drawn (its zero cross is its footprint's front corner, the
/// picture lies up and left of it), some in front of it and some behind, and
/// four trees around it.
const Placed = struct { name: []const u8, at: [2]f64, player: i32, direction: u32, frame: i32, turns: bool };
const cluster = blk: {
    var list: [9]Placed = undefined;
    list[0] = .{ .name = "E_FrenchShed01", .at = .{ 0, 0 }, .player = -1, .direction = 0, .frame = -1, .turns = true };
    for (0..4) |i| {
        const angle = @as(f64, @floatFromInt(i)) * std.math.pi / 2.0;
        list[1 + i] = .{ .name = "German_sniper", .at = .{ -90 + 70 * @cos(angle), -130 + 70 * @sin(angle) }, .player = 0, .direction = i * 16384, .frame = 0, .turns = true };
        const tree = angle + std.math.pi / 4.0;
        list[5 + i] = .{ .name = "_Lime", .at = .{ 300 * @cos(tree), 300 * @sin(tree) }, .player = -1, .direction = 0, .frame = -1, .turns = false };
    }
    break :blk list;
};

/// A point on the hill's slope the hill run clicks, and the sniper it orders there.
const slope_click = [2]i32{ 611, 333 };
const hill_sniper = [2]f64{ -104, 80 };

/// The `ground=` action's lines: "<x>,<y>: <x>,<y>,<z>".
fn parseGrounds(allocator: std.mem.Allocator, log: []const u8) ![]const [3]f64 {
    var out: std.ArrayList([3]f64) = .empty;
    for (try lines(allocator, log, "BK_GROUND_TRACE: ")) |line| {
        const colon = std.mem.indexOf(u8, line, ": ") orelse return error.BadTrace;
        var it = std.mem.tokenizeScalar(u8, line[colon + 2 ..], ',');
        var g: [3]f64 = undefined;
        for (&g) |*v| v.* = try std.fmt.parseFloat(f64, std.mem.trim(u8, it.next() orelse return error.BadTrace, " "));
        try out.append(allocator, g);
    }
    return out.items;
}

/// Where the 2003 camera turned `step` times draws a world point: the anchor
/// (at height 0) at the screen's centre, world x one pixel right and half
/// down, y one right and half up, height 0.866 up a unit
/// (tools/zig/fixtures/bk-views/camera-2003.json), the offset turned first.
fn projectGround(g: [3]f64, anchor: [2]f64, step: usize) [2]f64 {
    var d = [2]f64{ g[0] - anchor[0], g[1] - anchor[1] };
    for (0..step) |_| {
        const x = d[0];
        d = .{ -d[1], x };
    }
    return .{ @as(f64, @floatFromInt(centre[0])) + std.math.sqrt1_2 * (d[0] + d[1]), @as(f64, @floatFromInt(centre[1])) + std.math.sqrt1_2 / 2.0 * (d[0] - d[1]) - 0.866025 * g[2] };
}

/// The point at height 0 the slope click's line of sight meets at step 0: the
/// ground the 2003 port answered on every hill before S06.
fn planeGround(anchor: [2]f64) [2]f64 {
    const sum = @as(f64, @floatFromInt(slope_click[0] - centre[0])) / std.math.sqrt1_2;
    const diff = @as(f64, @floatFromInt(slope_click[1] - centre[1])) / (std.math.sqrt1_2 / 2.0);
    return .{ anchor[0] + (sum + diff) / 2, anchor[1] + (sum - diff) / 2 };
}

/// mod.xml, control.xml, hill.xml and clusters.xml, written from road3d.xml.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn control test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));

    var control: std.ArrayList(u8) = .empty;
    var link: u32 = 1;
    for (snipers) |e| try item(allocator, &control, "German_sniper", ringOffset(e), 0, e * 8192, 0, &link);
    try item(allocator, &control, "Swing01", swing_at, -1, 0, -1, &link);
    try item(allocator, &control, "E_House01_1", house_at, -1, 0, -1, &link);
    try writeMap(io, allocator, data, "control.xml", map, control.items);

    var order: std.ArrayList(u8) = .empty;
    link = 1;
    for (clusters, 0..) |c, j| {
        // Cluster j is the middle one turned j times by (x, y) -> (-y, x)
        // about its middle: every place turned, every own facing a quarter on.
        for (cluster) |o| {
            var d = o.at;
            for (0..j) |_| {
                const x = d[0];
                d = .{ -d[1], x };
            }
            const facing: u32 = @intCast(j * 16384);
            try item(allocator, &order, o.name, .{ c[0] + d[0], c[1] + d[1] }, o.player, if (o.turns) (o.direction + facing) & 0xffff else 0, o.frame, &link);
        }
    }
    try writeMap(io, allocator, data, "clusters.xml", map, order.items);

    // A hill of height 120 and 14 tiles' radius in the middle: its slopes stay
    // below the camera's 30 degrees, so the ground under the centre is seen at
    // every step.
    const start = std.mem.indexOf(u8, map, "<Altitudes>").?;
    const end = std.mem.indexOf(u8, map, "</Altitudes>").?;
    var hill: std.ArrayList(u8) = .empty;
    try hill.appendSlice(allocator, map[0..start]);
    try hill.appendSlice(allocator, "<Altitudes><item size_x=\"33\" size_y=\"33\"/>");
    for (0..33) |j| for (0..33) |i| {
        const r = @min(std.math.hypot(@as(f64, @floatFromInt(i)) - 16, @as(f64, @floatFromInt(j)) - 16) / 14.0, 1.0);
        try hill.print(allocator, "<item Height=\"{d:.0}\" Shade=\"208\"/>", .{120 * (@cos(r * std.math.pi) + 1) / 2});
    };
    try hill.appendSlice(allocator, map[end..]);
    var hill_items: std.ArrayList(u8) = .empty;
    link = 1;
    try item(allocator, &hill_items, "German_sniper", hill_sniper, 0, 0, 0, &link);
    try writeMap(io, allocator, data, "hill.xml", hill.items, hill_items.items);
}

pub fn item(allocator: std.mem.Allocator, out: *std.ArrayList(u8), name: []const u8, d: [2]f64, player: i32, direction: u32, frame: i32, link: *u32) !void {
    try out.print(allocator, "<item Direction=\"{d}\" Player=\"{d}\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
        "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ direction, player, frame, name, middle + d[0], middle + d[1], link.* });
    link.* += 1;
}

pub fn writeMap(io: std.Io, allocator: std.mem.Allocator, data: []const u8, name: []const u8, map: []const u8, objects: []const u8) !void {
    const with = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", try std.fmt.allocPrint(allocator, "<Objects>{s}</Objects>", .{objects}));
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", name }), .data = with });
}

pub const Run = struct { log: []const u8, log_path: []const u8, user: []const u8 };

/// What runScheduleWith may add: the scratch user folder when another run's
/// (its saves) is wanted, more arguments, and BK_AI_STATE_TRACE.
pub const RunOptions = struct { user: ?[]const u8 = null, extra: []const []const u8 = &.{}, state_trace: bool = false };

/// Runs the Game in its own scratch user folder under `schedule` (BK_AUTO_UI)
/// with the mod and the passes `disable` off, on `map` or, when it is empty,
/// to the main menu; it must exit 0 without a leak.
pub fn runScheduleWith(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, mod: []const u8, name: []const u8, map: []const u8, schedule: []const u8, disable: []const u8, options: RunOptions) !Run {
    const cwd = std.Io.Dir.cwd();
    const user = try std.fs.path.join(allocator, &.{ root, "user", options.user orelse name });
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
    try environ.put("BK_MAP_TRACE", "1");
    if (options.state_trace) try environ.put("BK_AI_STATE_TRACE", "1");
    if (disable.len != 0) try environ.put("BK_SCENE_DISABLE", disable);
    try environ.put("BK_AUTO_UI", schedule);
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-windowed", std.fmt.comptimePrint("-mode={d}x{d}", .{ width, height }), "-profile=Turn", try std.fmt.allocPrint(allocator, "-mod={s}", .{mod}) });
    try argv.appendSlice(allocator, options.extra);
    if (map.len != 0) try argv.append(allocator, map);
    const result = try timed_run.run(allocator, io, .{
        .argv = argv.items,
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
    return .{ .log = log, .log_path = log_path, .user = user };
}

pub fn runSchedule(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, mod: []const u8, name: []const u8, map: []const u8, schedule: []const u8, disable: []const u8) !Run {
    return runScheduleWith(io, allocator, game, root, mod, name, map, schedule, disable, .{});
}

/// The shot the schedule entry keyed `key` took, also kept as `<name>.png`.
pub fn readShot(io: std.Io, allocator: std.mem.Allocator, user: []const u8, key: []const u8, root: []const u8, name: []const u8) ![]u8 {
    const cwd = std.Io.Dir.cwd();
    const shot_name = try std.fmt.allocPrint(allocator, "autoshot_{s}_{d}x{d}.rgba", .{ key, width, height });
    const frame = cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ user, shot_name }), allocator, .limited(64 << 20)) catch |err| {
        std.debug.print("no shot {s} in {s}\n", .{ shot_name, user });
        return err;
    };
    try testing.expectEqual(@as(usize, width * height * 4), frame.len);
    const image: views.Image = .{ .width = width, .height = height, .pixels = std.mem.bytesAsSlice([4]u8, frame) };
    try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(allocator, "{s}/{s}.png", .{ root, name }), .data = try views.writePng(allocator, image) });
    return frame;
}

/// The rest of every line holding `prefix`, in order.
pub fn lines(allocator: std.mem.Allocator, log: []const u8, prefix: []const u8) ![]const []const u8 {
    var out: std.ArrayList([]const u8) = .empty;
    var it = std.mem.tokenizeAny(u8, log, "\r\n");
    while (it.next()) |line| {
        if (std.mem.indexOf(u8, line, prefix)) |at| try out.append(allocator, line[at + prefix.len ..]);
    }
    return out.items;
}

pub const Anchor = struct { anchor: [2]f64, ground: [3]f64, step: i32 };

/// The `anchor` action's lines: "<x>,<y> ground <x>,<y>,<z> step <k>".
pub fn parseAnchors(allocator: std.mem.Allocator, log: []const u8) ![]const Anchor {
    var out: std.ArrayList(Anchor) = .empty;
    for (try lines(allocator, log, "BK_AUTO_UI: anchor ")) |line| {
        var it = std.mem.tokenizeAny(u8, line, ", ");
        var a: Anchor = undefined;
        a.anchor[0] = try std.fmt.parseFloat(f64, it.next().?);
        a.anchor[1] = try std.fmt.parseFloat(f64, it.next().?);
        _ = it.next();
        for (&a.ground) |*g| g.* = try std.fmt.parseFloat(f64, it.next().?);
        _ = it.next();
        a.step = try std.fmt.parseInt(i32, it.next().?, 10);
        try out.append(allocator, a);
    }
    return out.items;
}

pub const Selection = struct { summary: []const u8, places: []const [2]f64 };

/// Each TraceSelection: its summary line and the places of what it selected.
pub fn parseSelections(allocator: std.mem.Allocator, log: []const u8) ![]const Selection {
    var out: std.ArrayList(Selection) = .empty;
    var places: std.ArrayList([2]f64) = .empty;
    var it = std.mem.tokenizeAny(u8, log, "\r\n");
    while (it.next()) |line| {
        if (std.mem.indexOf(u8, line, "BK_SELECTION_TRACE: ")) |at| {
            if (out.items.len != 0) out.items[out.items.len - 1].places = try places.toOwnedSlice(allocator);
            try out.append(allocator, .{ .summary = line[at + "BK_SELECTION_TRACE: ".len ..], .places = &.{} });
        } else if (std.mem.indexOf(u8, line, "BK_SELECTED: ")) |_| {
            const at = std.mem.lastIndexOf(u8, line, " at ").?;
            var parts = std.mem.tokenizeScalar(u8, line[at + 4 ..], ',');
            try places.append(allocator, .{ try std.fmt.parseFloat(f64, parts.next().?), try std.fmt.parseFloat(f64, parts.next().?) });
        }
    }
    if (out.items.len != 0) out.items[out.items.len - 1].places = try places.toOwnedSlice(allocator);
    return out.items;
}

/// Whether `places` are exactly the snipers at the ring's eighths `eighths`.
fn sameSet(places: []const [2]f64, eighths: []const u32) bool {
    if (places.len != eighths.len) return false;
    for (eighths) |e| {
        const want = worldOf(ringOffset(e));
        var found = false;
        for (places) |p| {
            if (std.math.hypot(p[0] - want[0], p[1] - want[1]) < 3) found = true;
        }
        if (!found) return false;
    }
    return true;
}

fn placesText(allocator: std.mem.Allocator, places: []const [2]f64) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    for (places) |p| try out.print(allocator, "({d:.0},{d:.0})", .{ p[0], p[1] });
    if (places.len == 0) try out.appendSlice(allocator, "nothing");
    return out.items;
}

fn rgb(frame: []const u8, x: i32, y: i32) ?[3]u8 {
    if (x < 0 or y < 0 or x >= width or y >= height) return null;
    const i: usize = @intCast((y * width + x) * 4);
    return frame[i..][0..3].*;
}

fn drawn(frame: []const u8, x: i32, y: i32) bool {
    const p = rgb(frame, x, y) orelse return false;
    return @max(p[0], p[1], p[2]) > 24;
}

/// A point well inside a picture: it and the points two pixels each way are drawn.
fn drawnInside(frame: []const u8, x: i32, y: i32) bool {
    return drawn(frame, x, y) and drawn(frame, x - 2, y) and drawn(frame, x + 2, y) and drawn(frame, x, y - 2) and drawn(frame, x, y + 2);
}

/// The box of the drawn pixels inside a pick grid.
fn drawnBox(frame: []const u8, origin: [2]i32, n: usize, pitch: i32) [4]i32 {
    var box = [4]i32{ std.math.maxInt(i32), std.math.maxInt(i32), std.math.minInt(i32), std.math.minInt(i32) };
    const span: i32 = @as(i32, @intCast(n - 1)) * pitch;
    var y = origin[1];
    while (y <= origin[1] + span) : (y += 1) {
        if (y < turn.clear_rows[0] or y >= turn.clear_rows[1]) continue;
        var x = origin[0];
        while (x <= origin[0] + span) : (x += 1) {
            if (!drawn(frame, x, y)) continue;
            box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x), @max(box[3], y) };
        }
    }
    return box;
}

/// The mean RGB difference over the minimap's diamond between `after` and
/// `before` turned k times about the minimap's middle (each pixel the one it
/// came from).
fn minimapDifference(before: []const u8, after: []const u8, k: usize) f64 {
    var sum: f64 = 0;
    var count: f64 = 0;
    var dy: i32 = -50;
    while (dy <= 50) : (dy += 1) {
        var dx: i32 = -100;
        while (dx <= 100) : (dx += 1) {
            if (@as(f64, @floatFromInt(@abs(dx))) / 100.0 + @as(f64, @floatFromInt(@abs(dy))) / 50.0 > 1.0) continue;
            const from = turn.unturn(.{ @floatFromInt(dx), @floatFromInt(dy) }, k);
            const p = rgb(after, minimap_middle[0] + dx, minimap_middle[1] + dy) orelse continue;
            const q = rgb(before, minimap_middle[0] + @as(i32, @intFromFloat(@round(from[0]))), minimap_middle[1] + @as(i32, @intFromFloat(@round(from[1])))) orelse continue;
            for (0..3) |c| sum += @abs(@as(f64, @floatFromInt(p[c])) - @as(f64, @floatFromInt(q[c])));
            count += 3;
        }
    }
    return if (count == 0) 255 else sum / count;
}

/// The minimap's view frame: its pixels of the frame's colour with the
/// frame's shadow a pixel right and down (a winter map's picture holds the
/// frame's grey too).
const Mask = std.ArrayList([2]i32);
fn frameMask(allocator: std.mem.Allocator, frame: []const u8) !Mask {
    var mask: Mask = .empty;
    var y = minimap_box[1];
    while (y < minimap_box[3]) : (y += 1) {
        var x = minimap_box[0];
        while (x < minimap_box[2]) : (x += 1) {
            if (std.mem.eql(u8, &rgb(frame, x, y).?, &frame_colour) and std.mem.eql(u8, &rgb(frame, x + 1, y + 1).?, &shadow_colour)) try mask.append(allocator, .{ x, y });
        }
    }
    return mask;
}

fn maskMiddle(mask: Mask) [2]i32 {
    var box = [4]i32{ std.math.maxInt(i32), std.math.maxInt(i32), std.math.minInt(i32), std.math.minInt(i32) };
    for (mask.items) |p| box = .{ @min(box[0], p[0]), @min(box[1], p[1]), @max(box[2], p[0]), @max(box[3], p[1]) };
    return .{ @divTrunc(box[0] + box[2], 2), @divTrunc(box[1] + box[3], 2) };
}

/// The IoU of two masks, a point counted when the other has one within a pixel.
fn maskIou(a: Mask, b: Mask) f64 {
    var both: f64 = 0;
    for (a.items) |q| {
        for (b.items) |r| {
            if (@abs(r[0] - q[0]) <= 1 and @abs(r[1] - q[1]) <= 1) {
                both += 1;
                break;
            }
        }
    }
    const total: f64 = @floatFromInt(a.items.len + b.items.len);
    return if (total == 0) 0 else both / (total - both);
}

const Compared = struct {
    differ: usize,
    drawn: usize,
    dx: i32,
    dy: i32,
    fn fraction(self: Compared) f64 {
        return if (self.drawn == 0) 1 else @as(f64, @floatFromInt(self.differ)) / @as(f64, @floatFromInt(self.drawn));
    }
};

/// The "Pause" line the paused game writes over the screen's centre.
const pause_line = [4]i32{ 460, 360, 575, 405 };

/// Two shots around the screen's centre (but the pause line): the pixels drawn in either, and of
/// them those whose colours differ (a channel sum of 48), at the best offset
/// within 3 pixels.
fn compareCluster(a: []const u8, b: []const u8) Compared {
    var best: Compared = .{ .differ = std.math.maxInt(usize), .drawn = 0, .dx = 0, .dy = 0 };
    var oy: i32 = -3;
    while (oy <= 3) : (oy += 1) {
        var ox: i32 = -3;
        while (ox <= 3) : (ox += 1) {
            var c: Compared = .{ .differ = 0, .drawn = 0, .dx = ox, .dy = oy };
            var y: i32 = centre[1] - 200;
            while (y <= centre[1] + 100) : (y += 1) {
                if (y < turn.clear_rows[0] or y >= turn.clear_rows[1]) continue;
                var x: i32 = centre[0] - 190;
                while (x <= centre[0] + 190) : (x += 1) {
                    if (x >= pause_line[0] and x <= pause_line[2] and y >= pause_line[1] and y <= pause_line[3]) continue;
                    const p = rgb(a, x, y).?;
                    const q = rgb(b, x + ox, y + oy) orelse continue;
                    if (@max(p[0], p[1], p[2]) <= 24 and @max(q[0], q[1], q[2]) <= 24) continue;
                    c.drawn += 1;
                    var sum: i32 = 0;
                    for (0..3) |ch| sum += @intCast(@abs(@as(i32, p[ch]) - @as(i32, q[ch])));
                    if (sum > 48) c.differ += 1;
                }
            }
            if (c.differ < best.differ) best = c;
        }
    }
    return best;
}
