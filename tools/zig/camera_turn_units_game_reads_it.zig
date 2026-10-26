//! Units at every camera step (M006 S04,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md). With the
//! Remastered option on, the turn key (`.`) turns the view a quarter turn
//! about the point at the screen centre: infantry draws the facing a quarter
//! turn on (two of its eight pictures), vehicles and guns turn as 3D with the
//! sun, so their light and shadows fall to the same side of the screen as the
//! 2003 pictures', and health bars stay above their units.
//!
//! A map written from road3d.xml holds, on rings around the centre that a
//! quarter turn takes onto themselves, units of player 0, each facing along
//! the ring (map Direction = its eighth times 8192; the AI's convention: 0
//! faces +y, a quarter -x, so a quarter turn of the world adds 16384): a
//! German sniper (one soldier, at half health so its bar shows) at seven of
//! the eight eighths of an inner ring, a T-34 at the
//! even eighths of an outer ring and a 5 cm Pak 38 at its odd ones. A quarter
//! turn takes a unit at eighth e to the place of eighth e + 2 and shows it as
//! the 2003 camera shows a unit a quarter turn on, which is the one standing
//! there at step 0: each unit at step k is scored against the step-0 frame
//! around the place it moves to (outline IoU and colour at the best offset
//! within 4 pixels), and a sniper also against its own step-0 picture there,
//! its facing not shifted, which must score lower. The inner ring's empty
//! eighth moves on with the turn: it must be empty at its turned place (a key
//! that did not turn the view would leave it where it was).
//!
//! - Units, Remastered on, the terrain off (the units, their bars, nothing
//!   else), the game paused (SPACE) so animations stand still.
//! - Shadows, Remastered on, paused: the same map with the terrain on, once
//!   with shadows and once without; a tank's shadow is where the first is
//!   darker, and must be the shadow of the tank at the turned place at step 0
//!   (the sun turns with the camera).
//! - Remastered off, paused: the key changes no pixel of any unit.
//!
//! The Game runs from zig-out/local-test/camera-turn/units with every user path
//! redirected (camera_turn_game_reads_it.runGame); frames, logs and scores.txt
//! stay there. BK_VIEWS_GAME names the installed Game; `zig build
//! camera-turn-units-game-reads-it` sets it.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/units";
const width = turn.width;
const height = turn.height;
const centre = turn.centre;

/// The middle of road3d's 32 x 32 tiles.
const look = [2]u32{ 512, 512 };
/// AI units of the map XML per world unit.
const ai_per_world = std.math.sqrt2;

const units_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
const with_shadows = "effects,noise,warfog,haze,grid";
const without_shadows = "shadows,effects,noise,warfog,haze,grid";

const Unit = struct { name: []const u8, ring: f64, eighth: u32, hp: []const u8, reach: [4]i32, infantry: bool };
const inner = 220.0;
const outer = 560.0;
const sniper_reach = [4]i32{ -24, -56, 24, 16 };
const vehicle_reach = [4]i32{ -70, -70, 70, 50 };
/// The inner ring's eighth without a sniper.
const empty_eighth = 7;
const units = blk: {
    var list: [7 + 8]Unit = undefined;
    for (0..7) |i| {
        const e: u32 = @intCast(i);
        list[e] = .{ .name = "German_sniper", .ring = inner, .eighth = e, .hp = "0.5", .reach = sniper_reach, .infantry = true };
    }
    for (0..8) |i| {
        const e: u32 = @intCast(i);
        list[7 + e] = .{ .name = if (e % 2 == 0) "T-34" else "5_cm_Pak38", .ring = outer, .eighth = e, .hp = "1", .reach = vehicle_reach, .infantry = false };
    }
    break :blk list;
};

/// Floors. The game is paused for the shots, so a sniper and a tank are the
/// pixels of the step-0 one at the place it moves to (a sprite's picture; a
/// mesh drawn by the 2003 camera a quarter turn on, lit from the same side of
/// the screen). A gun's crew (sprites around it) stands in animations whose
/// phases differ from gun to gun, so a gun is held to its outline, and its
/// shadow, mixed with the crew's moves between two runs, is only reported.
const min_iou: f64 = 0.98;
const max_colour: f64 = 1.0;
const min_gun_iou: f64 = 0.85;
const max_gun_colour: f64 = 12.0;
const min_shadow_iou: f64 = 0.95;

fn isGun(unit: Unit) bool {
    return !unit.infantry and !std.mem.eql(u8, unit.name, "T-34");
}

test "units turn with the camera: infantry facings shift by two a step, vehicles and guns turn as 3D, shadows and bars follow" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-units-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" }));

    var failed = false;
    var report: std.ArrayList(u8) = .empty;

    try report.appendSlice(allocator, "units: step name eighth: IoU / colour (offset); facing not shifted IoU\n");
    const frames = try turn.runGameWith(io, allocator, game, root, "units", "units.xml", look, "ON", units_only, 4, true);
    for (1..4) |step| {
        const k: u32 = @intCast(step);
        for (units) |unit| {
            const to = screenOf(unit.ring, (unit.eighth + 2 * k) % 8);
            // The step-0 unit at the place this one moves to: none where the inner ring is empty.
            if (unit.infantry and (unit.eighth + 2 * k) % 8 == empty_eighth) continue;
            const right = score(frames[0], to, frames[k], to, unit.reach);
            try report.print(allocator, "{d} {s} {d}: {d:.3} / {d:.2} ({d},{d})", .{ k, unit.name, unit.eighth, right.iou, right.colour, right.dx, right.dy });
            if (right.iou < (if (isGun(unit)) min_gun_iou else min_iou) or right.colour > (if (isGun(unit)) max_gun_colour else max_colour)) failed = true;
            if (unit.infantry) {
                const still = score(frames[0], screenOf(unit.ring, unit.eighth), frames[k], to, unit.reach);
                try report.print(allocator, "; {d:.3}\n", .{still.iou});
                if (still.iou >= right.iou) failed = true;
            } else try report.appendSlice(allocator, "\n");
        }
        // The empty eighth, turned.
        const gap = screenOf(inner, (empty_eighth + 2 * k) % 8);
        const drawn_there = drawnPixels(frames[k], gap, sniper_reach);
        try report.print(allocator, "{d} empty eighth at its turned place: {d} pixels drawn\n", .{ k, drawn_there });
        if (drawn_there > 20) failed = true;
    }

    // Shadows: where the frame with shadows is darker than the one without.
    try report.appendSlice(allocator, "shadows: step name eighth: IoU of the shadow against the step-0 shadow at the turned place (pixels)\n");
    const lit = try turn.runGameWith(io, allocator, game, root, "shadows", "units.xml", look, "ON", with_shadows, 4, true);
    const unlit = try turn.runGameWith(io, allocator, game, root, "no-shadows", "units.xml", look, "ON", without_shadows, 4, true);
    for (1..4) |step| for (units) |unit| {
        const k: u32 = @intCast(step);
        if (unit.infantry) continue;
        const to = screenOf(unit.ring, (unit.eighth + 2 * k) % 8);
        const shadow = shadowScore(lit[0], unlit[0], lit[k], unlit[k], to);
        try report.print(allocator, "{d} {s} {d}: {d:.3} ({d})\n", .{ k, unit.name, unit.eighth, shadow.iou, shadow.pixels });
        if (!isGun(unit) and (shadow.pixels < 50 or shadow.iou < min_shadow_iou)) failed = true;
    };

    // Remastered off: the key does nothing to any unit.
    const off = try turn.runGameWith(io, allocator, game, root, "off", "units.xml", look, "OFF", units_only, 2, true);
    var changed: usize = 0;
    for (units) |unit| {
        const at = screenOf(unit.ring, unit.eighth);
        var y = at[1] + unit.reach[1];
        while (y <= at[1] + unit.reach[3]) : (y += 1) {
            var x = at[0] + unit.reach[0];
            while (x <= at[0] + unit.reach[2]) : (x += 1) {
                const a = rgb(off[0], x, y) orelse continue;
                if (!std.mem.eql(u8, &a, &rgb(off[1], x, y).?)) changed += 1;
            }
        }
    }
    try report.print(allocator, "off: unit pixels changed by the key: {d}\n", .{changed});
    if (changed != 0) failed = true;

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a unit is drawn wrong at a camera step; the frames are in {s}\n", .{ report.items, root });
        return error.DrawnWrong;
    }
}

/// mod.xml and units.xml, written from road3d.xml.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn units test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    var items: std.ArrayList(u8) = .empty;
    try items.appendSlice(allocator, "<Objects>");
    for (units, 1..) |unit, link| {
        const at = ringPoint(unit.ring, unit.eighth);
        // A squad's FrameIndex is its formation (0); a unit's is unused.
        try items.print(allocator, "<item Direction=\"{d}\" Player=\"0\" ScriptID=\"-1\" HP=\"{s}\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ unit.eighth * 8192, unit.hp, @as(i32, if (unit.infantry) 0 else -1), unit.name, at[0], at[1], link });
    }
    try items.appendSlice(allocator, "</Objects>");
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "units.xml" }),
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

fn rgb(frame: []const u8, x: i32, y: i32) ?[3]u8 {
    if (x < 0 or y < 0 or x >= width or y >= height) return null;
    const i: usize = @intCast((y * width + x) * 4);
    return frame[i..][0..3].*;
}

fn drawn(p: ?[3]u8) bool {
    const q = p orelse return false;
    return @max(q[0], q[1], q[2]) > 24;
}

fn inInterface(y: i32) bool {
    return y < turn.clear_rows[0] or y >= turn.clear_rows[1];
}

const Score = struct { iou: f64, colour: f64, dx: i32, dy: i32 };

/// The pixels around `from` in `before` against those around `to` in
/// `after`, the best offset within 4 pixels.
fn score(before: []const u8, from: [2]i32, after: []const u8, to: [2]i32, reach: [4]i32) Score {
    var best: Score = .{ .iou = -1, .colour = 255, .dx = 0, .dy = 0 };
    var oy: i32 = -4;
    while (oy <= 4) : (oy += 1) {
        var ox: i32 = -4;
        while (ox <= 4) : (ox += 1) {
            var both: f64 = 0;
            var either: f64 = 0;
            var colour: f64 = 0;
            var y = reach[1];
            while (y <= reach[3]) : (y += 1) {
                if (inInterface(from[1] + y) or inInterface(to[1] + y + oy)) continue;
                var x = reach[0];
                while (x <= reach[2]) : (x += 1) {
                    const want = rgb(before, from[0] + x, from[1] + y);
                    const got = rgb(after, to[0] + x + ox, to[1] + y + oy);
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

fn drawnPixels(frame: []const u8, at: [2]i32, reach: [4]i32) usize {
    var count: usize = 0;
    var y = at[1] + reach[1];
    while (y <= at[1] + reach[3]) : (y += 1) {
        var x = at[0] + reach[0];
        while (x <= at[0] + reach[2]) : (x += 1) {
            if (drawn(rgb(frame, x, y))) count += 1;
        }
    }
    return count;
}

/// A shadow pixel: the frame with shadows darker than the one without.
fn shadowed(lit: []const u8, unlit: []const u8, x: i32, y: i32) bool {
    const a = rgb(lit, x, y) orelse return false;
    const b = rgb(unlit, x, y).?;
    var sum: i32 = 0;
    for (0..3) |c| sum += @as(i32, b[c]) - @as(i32, a[c]);
    return sum > 30;
}

/// The shadow around `at` at step k against the step-0 shadow there, the
/// best offset within 4 pixels; `pixels` counts the step-0 shadow.
fn shadowScore(lit0: []const u8, unlit0: []const u8, litk: []const u8, unlitk: []const u8, at: [2]i32) struct { iou: f64, pixels: usize } {
    const reach = [4]i32{ -110, -90, 110, 90 };
    var best: f64 = 0;
    var pixels: usize = 0;
    var oy: i32 = -4;
    while (oy <= 4) : (oy += 1) {
        var ox: i32 = -4;
        while (ox <= 4) : (ox += 1) {
            var both: f64 = 0;
            var either: f64 = 0;
            var count: usize = 0;
            var y = at[1] + reach[1];
            while (y <= at[1] + reach[3]) : (y += 1) {
                if (inInterface(y) or inInterface(y + oy)) continue;
                var x = at[0] + reach[0];
                while (x <= at[0] + reach[2]) : (x += 1) {
                    const w = shadowed(lit0, unlit0, x, y);
                    const g = shadowed(litk, unlitk, x + ox, y + oy);
                    if (w) count += 1;
                    if (w or g) either += 1;
                    if (w and g) both += 1;
                }
            }
            pixels = count;
            if (either != 0 and both / either > best) best = both / either;
        }
    }
    return .{ .iou = best, .pixels = pixels };
}

test "a quarter turn takes each ring onto itself, two eighths on" {
    try testing.expectEqual(turn.turnScreen(screenOf(inner, 0), 1), screenOf(inner, 2));
    try testing.expectEqual(turn.turnScreen(screenOf(outer, 2), 1), screenOf(outer, 4));
}
