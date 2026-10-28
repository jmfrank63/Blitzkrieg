//! What the maintainer's first hand test left open (M006 S08,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md), on the real Game
//! driven by BK_AUTO_UI:
//!
//! - storage: a map holding the barn E_WB_01 (a storage) placed for no player
//!   (Player -1) must load and draw it, Remastered off as in 2003. The debug
//!   Game trapped there (the diplomacy's tables read at player 255).
//! - fire: E_FrenchShed01 standing turned by its own facing (map Direction
//!   16384 f, f = 0..3, Remastered on when the map loads, its turned pictures
//!   made by bk-views side into the scratch mod) with a German squad inside,
//!   two T-34s and a Soviet squad around it, all placed turned with it. With
//!   BK_FIRE_TRACE the Game prints where each soldier is put to fire, which
//!   armour side each shot hits and where each slot's flash is drawn:
//!   - a soldier stands at the building's place plus the slot's 1.xml
//!     `Position` turned f times by (x, y) -> (-y, x), facing the slot's
//!     direction plus f quarters;
//!   - a hit from the tank in front of the turned front hits the front, the one
//!     at the turned left the left, the squad at the turned right the right,
//!     at every facing (the 2003 front, never turned, would give the side f on);
//!   - a flash stands over the soldier who fires (its ground point at the
//!     soldier's) and on the slot's 1.xml `PicturePosition` as the 2003 camera
//!     draws the building unturned: the window, not the 1.xml `WorldPosition`,
//!     which only the 2003 view draws at the window (a median 43 world units
//!     off it in the shipped buildings).
//! - multiplayer: two Games on one machine, a host (`-host`, the room made
//!   and launched by its own buttons, `msg=`) and a client (`-connect127.0.0.1`
//!   on its own port, BK_NET_CLIENT_PORT), each starting at the step its corner
//!   gives (S05) and the client turning while they play: the game state of
//!   every second (BK_AI_STATE_TRACE, the multiplayer checksum's terms) must
//!   be the same in both for a minute of play, and both must exit without a
//!   leak.
//!
//! The Game runs from zig-out/local-test/camera-turn/open with every user path
//! redirected; frames, logs and scores.txt stay there. BK_VIEWS_GAME names the
//! installed Game and BK_VIEWS_TOOL bk-views; `zig build
//! camera-turn-open-game-reads-it` sets both.
const std = @import("std");
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const turn = @import("camera_turn_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/open";
const mod_name = "camera_turn_open";
const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
const sqrt2 = std.math.sqrt2;
const shed_folder = "Data/Buildings/europe/summer/e_frenchshed01";

/// The shed's footprint middle from its place (AI units: x -224..0,
/// y -8..120) and the attackers, offsets from its place in its own frame: a
/// T-34 in front (+x), one at its left (+y), the Soviet squad at its right
/// (-y); each map turns them with the shed. RPG_FRONT, RPG_LEFT, RPG_RIGHT.
const shed_middle = [2]f64{ -112, 56 };
const Attacker = struct { name: []const u8, at: [2]f64, side: u32 };
const attackers = [_]Attacker{
    .{ .name = "T-34", .at = .{ 380, 56 }, .side = 0 },
    .{ .name = "T-34", .at = .{ -112, 480 }, .side = 1 },
    .{ .name = "USSR_rifle_41", .at = .{ -112, -300 }, .side = 3 },
};
/// The German squad starts beside the shed and goes in at once (its map link).
const squad_at = [2]f64{ 40, 56 };

/// A hit belongs to the attacker whose direction from the shed's middle is
/// nearest its attack direction, within this (direction units of 65536 a turn).
const max_attack_off = 6000;
/// Floors: a soldier on its turned slot (AI units: the AI holds a slot's place
/// in whole units, 1.34 off the 1.xml float at most, at facing 0 as at the
/// others), its direction (units), a flash's ground point from the soldier (AI
/// units) and its picture point from the 1.xml one (pixels: 0.90 at most, the
/// same whole units, at every facing); the share of a facing's hits on the
/// expected side (a shot at a corner may land on the next side).
const max_slot_off = 1.5;
const max_dir_off = 2;
const max_flash_ground = 1.0;
const max_flash_picture = 1.0;
const min_side_share = 0.9;
const min_hits = 3;

fn turnAi(p: [2]f64, k: usize) [2]f64 {
    var q = p;
    for (0..k % 4) |_| {
        const x = q[0];
        q = .{ -q[1], x };
    }
    return q;
}

/// The AI's direction of a vector: 0 along +y, 16384 along -x (counter-clockwise).
fn directionOf(v: [2]f64) f64 {
    const a = std.math.atan2(-v[0], v[1]);
    return @mod(a / (2 * std.math.pi) * 65536, 65536);
}

fn directionOff(a: f64, b: f64) f64 {
    const d = @mod(a - b, 65536);
    return @min(d, 65536 - d);
}

test "the AI's direction convention and a quarter turn" {
    try testing.expectApproxEqAbs(@as(f64, 0), directionOf(.{ 0, 1 }), 1e-9);
    try testing.expectApproxEqAbs(@as(f64, 16384), directionOf(.{ -1, 0 }), 1e-9);
    try testing.expectApproxEqAbs(@as(f64, 49152), directionOf(.{ 1, 0 }), 1e-9);
    try testing.expectEqual([2]f64{ -2, 1 }, turnAi(.{ 1, 2 }, 1));
    try testing.expectEqual(@as(f64, 100), directionOff(65500, 64));
}

/// A slot of the shed's 1.xml: its ground place and picture point (world
/// units and pixels of the 2003 camera) and its direction in degrees.
const Slot = struct { position: [2]f64, picture: [2]f64, direction: f64 };

fn attribute(text: []const u8, name: []const u8) !f64 {
    var key_buf: [32]u8 = undefined;
    const key = try std.fmt.bufPrint(&key_buf, "{s}=\"", .{name});
    const at = std.mem.indexOf(u8, text, key) orelse return error.NoAttribute;
    const rest = text[at + key.len ..];
    return std.fmt.parseFloat(f64, rest[0 .. std.mem.indexOfScalar(u8, rest, '"') orelse return error.NoAttribute]);
}

fn readSlots(allocator: std.mem.Allocator, xml: []const u8) ![]const Slot {
    const begin = std.mem.indexOf(u8, xml, "<FireSlots>") orelse return error.NoSlots;
    const end = std.mem.indexOf(u8, xml, "</FireSlots>") orelse return error.NoSlots;
    var slots: std.ArrayList(Slot) = .empty;
    var items = std.mem.splitSequence(u8, xml[begin..end], "<item ");
    _ = items.next();
    while (items.next()) |item| {
        const position = item[std.mem.indexOf(u8, item, "<Position ") orelse return error.NoSlots ..];
        const picture = item[std.mem.indexOf(u8, item, "<PicturePosition ") orelse return error.NoSlots ..];
        try slots.append(allocator, .{
            .position = .{ try attribute(position, "x"), try attribute(position, "y") },
            .picture = .{ try attribute(picture, "x"), try attribute(picture, "y") },
            .direction = try attribute(item, "Direction"),
        });
    }
    return slots.items;
}

/// The numbers of a BK_FIRE_TRACE line, in order.
fn numbers(allocator: std.mem.Allocator, line: []const u8) ![]const f64 {
    var out: std.ArrayList(f64) = .empty;
    var it = std.mem.tokenizeAny(u8, line, " ,");
    while (it.next()) |token| {
        const value = std.fmt.parseFloat(f64, token) catch continue;
        try out.append(allocator, value);
    }
    return out.items;
}

test "a storage placed for no player loads and is drawn" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-storage-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const storage_scratch = scratch ++ "-storage";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), storage_scratch });
    cwd.deleteTree(io, storage_scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    try writeModXml(io, allocator, data);
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    var objects: std.ArrayList(u8) = .empty;
    var link: u32 = 1;
    try control.item(allocator, &objects, "E_WB_01", .{ 0, 0 }, -1, 0, -1, &link);
    try control.writeMap(io, allocator, data, "storage.xml", map, objects.items);

    // Remastered stays off (a new profile's default): the 2003 path.
    const run = try control.runSchedule(io, allocator, game, root, mod_name, "storage", "storage.xml", "g1000:hover=1000x400,g10:camera=512x512,g500:shot,g10:exit", objects_only);
    const frame = try control.readShot(io, allocator, run.user, "g500", root, "storage");
    // The barn is drawn around the screen's middle (the backdrop is black).
    var drawn: usize = 0;
    var y: i32 = turn.centre[1] - 120;
    while (y < turn.centre[1] + 120) : (y += 1) {
        var x: i32 = turn.centre[0] - 160;
        while (x < turn.centre[0] + 160) : (x += 1) {
            const p = turn.pixel(frame, x, y);
            if (@max(p[0], p[1], p[2]) > 24) drawn += 1;
        }
    }
    const report = try std.fmt.allocPrint(allocator, "storage: E_WB_01 of Player -1 loaded; {d} pixels drawn around the middle\n", .{drawn});
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report });
    if (drawn < 4000) {
        std.debug.print("{s}the barn is not drawn; the frame is in {s}\n", .{ report, root });
        return error.DrawnWrong;
    }
}

test "a building standing turned fires from its turned slots and is hit on its turned sides" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-fire-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const fire_scratch = scratch ++ "-fire";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), fire_scratch });
    cwd.deleteTree(io, fire_scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const turned = try std.fs.path.join(allocator, &.{ data, "buildings/europe/summer" });
    try cwd.createDirPath(io, turned);
    try game_views.runTool(io, allocator, tool, &.{ "side", shed_folder, "1,2,3", turned, try std.fs.path.join(allocator, &.{ root, "work" }) });
    try writeModXml(io, allocator, data);
    const road3d = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    for (0..4) |f| {
        var objects: std.ArrayList(u8) = .empty;
        try objects.print(allocator, "<item Direction=\"{d}\" Player=\"0\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>E_FrenchShed01</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"1\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ f * 16384, control.middle, control.middle });
        const squad = turnAi(squad_at, f);
        try objects.print(allocator, "<item Direction=\"0\" Player=\"0\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"0\"><Name>German_rifle_41</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"2\" Intention=\"0\" LinkWith=\"1\"/></item>", .{ control.middle + squad[0], control.middle + squad[1] });
        var link: u32 = 3;
        for (attackers) |a| try control.item(allocator, &objects, a.name, turnAi(a.at, f), 1, 0, if (a.side == 3) 0 else -1, &link);
        try control.writeMap(io, allocator, data, try std.fmt.allocPrint(allocator, "fire{d}.xml", .{f}), road3d, objects.items);
    }

    const xml = try cwd.readFileAlloc(io, shed_folder ++ "/1.xml", allocator, .limited(1 << 20));
    const slots = try readSlots(allocator, xml);

    // The option is on before the maps load (the AI turns a building's stats at
    // load): a run to the main menu sets it in the profile the others use.
    _ = try control.runScheduleWith(io, allocator, game, root, mod_name, "option", "", "60:set=GamePlay.Remastered=ON,120:exit", objects_only, .{ .user = "on" });

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    var sides_seen: [4]bool = @splat(false);
    for (0..4) |f| {
        const name = try std.fmt.allocPrint(allocator, "fire{d}", .{f});
        const run = try control.runScheduleWith(io, allocator, game, root, mod_name, name, try std.fmt.allocPrint(allocator, "{s}.xml", .{name}), "g1000:hover=1000x400,g24000:exit", objects_only, .{ .user = "on", .env = &.{.{ "BK_FIRE_TRACE", "1" }} });
        const traced = try control.lines(allocator, run.log, "BK_FIRE_TRACE: ");

        // Soldiers on the turned slots.
        var soldiers: [32]?[2]f64 = @splat(null);
        var slot_count: usize = 0;
        var slot_worst: f64 = 0;
        var dir_worst: f64 = 0;
        var centre = [2]f64{ control.middle, control.middle };
        for (traced) |line| {
            if (!std.mem.startsWith(u8, line, "slot ")) continue;
            const v = try numbers(allocator, line);
            // slot n, building cx cy, turn t, soldier x y, dir w
            const n: usize = @intFromFloat(v[0]);
            centre = .{ v[1], v[2] };
            const want = turnAi(.{ slots[n].position[0] * sqrt2, slots[n].position[1] * sqrt2 }, f);
            const off = std.math.hypot(v[4] - v[1] - want[0], v[5] - v[2] - want[1]);
            const want_dir = @mod(@floor(slots[n].direction * 65536.0 / 360.0) + 16384.0 * @as(f64, @floatFromInt(f)), 65536);
            slot_worst = @max(slot_worst, off);
            dir_worst = @max(dir_worst, directionOff(v[6], want_dir));
            if (@as(usize, @intFromFloat(v[3])) != f) failed = true;
            if (n < soldiers.len) soldiers[n] = .{ v[4], v[5] };
            slot_count += 1;
        }
        try report.print(allocator, "facing {d}: {d} soldiers put to fire, farthest {d:.2} AI units off its slot turned, direction {d:.0} off\n", .{ f, slot_count, slot_worst, dir_worst });
        if (slot_count == 0 or slot_worst > max_slot_off or dir_worst > max_dir_off) failed = true;

        // Hits on the turned sides.
        var hits: [attackers.len][4]usize = @splat(@splat(0));
        var unclaimed: usize = 0;
        for (traced) |line| {
            if (!std.mem.startsWith(u8, line, "hit ")) continue;
            const v = try numbers(allocator, line);
            // hit building cx cy, turn t, attack w, side s
            const middle = turnAi(shed_middle, f);
            var best: ?usize = null;
            var best_off: f64 = max_attack_off;
            for (attackers, 0..) |a, i| {
                const at = turnAi(a.at, f);
                const off = directionOff(v[3], directionOf(.{ at[0] - middle[0], at[1] - middle[1] }));
                if (off < best_off) {
                    best_off = off;
                    best = i;
                }
            }
            const side: usize = @intFromFloat(v[4]);
            if (best) |i| {
                if (side < 4) hits[i][side] += 1;
            } else unclaimed += 1;
        }
        var claimed: usize = 0;
        var right: usize = 0;
        for (attackers, hits) |a, h| {
            for (h, 0..) |count, side| {
                claimed += count;
                if (side == a.side) right += count;
                if (count > 0 and side == a.side) sides_seen[side] = true;
            }
            try report.print(allocator, "  {s} at {d:.0},{d:.0}: hits on front, left, back, right {d}, {d}, {d}, {d} (want side {d}; the 2003 front gives {d})\n", .{ a.name, turnAi(a.at, f)[0], turnAi(a.at, f)[1], h[0], h[1], h[2], h[3], a.side, (a.side + f) % 4 });
        }
        const share = if (claimed == 0) 0 else @as(f64, @floatFromInt(right)) / @as(f64, @floatFromInt(claimed));
        try report.print(allocator, "  {d} hits from the attackers ({d} from elsewhere), {d:.3} on the expected side\n", .{ claimed, unclaimed, share });
        if (claimed < min_hits or share < min_side_share) failed = true;

        // Flashes at the window the soldier fires from.
        var flash_count: usize = 0;
        var ground_worst: f64 = 0;
        var picture_worst: f64 = 0;
        var world_off: f64 = 0;
        for (traced) |line| {
            if (!std.mem.startsWith(u8, line, "flash ")) continue;
            const v = try numbers(allocator, line);
            // flash slot n at x y z (world units)
            const n: usize = @intFromFloat(v[0]);
            const soldier = if (n < soldiers.len) soldiers[n] orelse continue else continue;
            ground_worst = @max(ground_worst, std.math.hypot(v[1] * sqrt2 - soldier[0], v[2] * sqrt2 - soldier[1]));
            // Unturned about the building and drawn by the 2003 camera.
            const d = turnAi(.{ v[1] - centre[0] / sqrt2, v[2] - centre[1] / sqrt2 }, 4 - f);
            const picture = [2]f64{ (d[0] + d[1]) / sqrt2, (d[0] - d[1]) / (2 * sqrt2) - v[3] * std.math.sqrt(3.0) / 2 };
            picture_worst = @max(picture_worst, std.math.hypot(picture[0] - slots[n].picture[0], picture[1] - slots[n].picture[1]));
            const w = try worldPositionOf(xml, n);
            world_off = @max(world_off, std.math.hypot(w[0] - slots[n].position[0], w[1] - slots[n].position[1]) * sqrt2);
            flash_count += 1;
        }
        try report.print(allocator, "  {d} flashes: ground point {d:.2} AI units from the soldier at most, picture point {d:.2} px from 1.xml's; 1.xml's WorldPosition up to {d:.1} AI units from the soldier\n", .{ flash_count, ground_worst, picture_worst, world_off });
        if (flash_count == 0 or ground_worst > max_flash_ground or picture_worst > max_flash_picture) failed = true;
    }
    var distinct: usize = 0;
    for (sides_seen) |seen| distinct += @intFromBool(seen);
    try report.print(allocator, "sides hit as expected: {d} distinct\n", .{distinct});
    if (distinct < 2) failed = true;

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a building standing turned fires or is hit off its turned slots and sides; the logs are in {s}\n", .{ report.items, root });
        return error.FireWrong;
    }
}

/// One Game of the multiplayer pair, run on its own thread.
const Player = struct {
    name: []const u8,
    schedule: []const u8,
    extra: []const []const u8,
    env: []const [2][]const u8,
    delay_s: i64,
    game: []const u8,
    root: []const u8,
    run: ?control.Run = null,
    failure: ?anyerror = null,
};

fn playOne(player: *Player) void {
    const io = testing.io;
    // Each thread its own arena: the run's strings outlive it for the scores.
    const arena = std.heap.page_allocator.create(std.heap.ArenaAllocator) catch |err| {
        player.failure = err;
        return;
    };
    arena.* = .init(std.heap.page_allocator);
    io.sleep(.fromSeconds(player.delay_s), .awake) catch {};
    player.run = control.runScheduleWith(io, arena.allocator(), player.game, player.root, mod_name, player.name, "", player.schedule, "", .{
        .extra = player.extra,
        .env = player.env,
        .state_trace = true,
        .timeout_s = 480,
        .mod = false,
    }) catch |err| {
        player.failure = err;
        return;
    };
}

/// The game state of every second of a log (BK_AI_STATE_TRACE), by time.
fn states(allocator: std.mem.Allocator, log: []const u8) !std.StringHashMapUnmanaged([]const u8) {
    var out: std.StringHashMapUnmanaged([]const u8) = .empty;
    for (try control.lines(allocator, log, "BK_AI_STATE_TRACE: ")) |line| {
        const at = std.mem.indexOfScalar(u8, line, ' ') orelse continue;
        try out.put(allocator, line[0..at], line);
    }
    return out;
}

/// The steps the camera stood at in a log: the start turn, then each `anchor`.
fn steps(allocator: std.mem.Allocator, log: []const u8) ![]const i32 {
    var out: std.ArrayList(i32) = .empty;
    for (try control.lines(allocator, log, "BK_MAP_TRACE: start turn ")) |line| try out.append(allocator, std.fmt.parseInt(i32, line[0..1], 10) catch -1);
    for (try control.parseAnchors(allocator, log)) |anchor| try out.append(allocator, anchor.step);
    return out.items;
}

test "two players at different camera steps play the same game" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-multiplayer-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const mp_scratch = scratch ++ "-multiplayer";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), mp_scratch });
    cwd.deleteTree(io, mp_scratch) catch {};
    try writeModXml(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" }));

    // The host creates a room on the first multiplayer map (its create screen's
    // own choice) and launches 80 s on; the client joins 35 s after the host
    // starts, says it is ready, and turns its view while they play. A `t` wait
    // that spans the mission's load counts from the load's end. Each player's
    // own start step comes from its corner (S05).
    var players = [_]Player{
        // Each player frames what it sees and orders it elsewhere: the orders
        // travel to both machines and each computes the paths (the host's
        // Remastered is on, so the fastest path, BK_PATH_TRACE).
        .{ .name = "host", .schedule = "t3000:set=GamePlay.Remastered=ON,t3000:msg=10004,t80000:msg=10005,g2000:anchor,g1000:drag=40x120x980x640,g500:world=TraceSelection(0),g300:hover=520x300,g300:rclick=520x300,g84000:anchor,g10:exit", .extra = &.{"-host"}, .env = &.{.{ "BK_PATH_TRACE", "1" }}, .delay_s = 0, .game = game, .root = root },
        .{ .name = "client", .schedule = "t3000:set=GamePlay.Remastered=ON,t12000:msg=10004,t50000:anchor,t10:key=COMMA,t300:anchor,t2000:drag=40x120x980x640,t500:world=TraceSelection(0),t300:hover=520x460,t300:rclick=520x460,t15000:key=COMMA,t300:anchor,t15000:key=.,t300:anchor,t30000:exit", .extra = &.{"-connect127.0.0.1"}, .env = &.{ .{ "BK_NET_CLIENT_PORT", "8890" }, .{ "BK_PATH_TRACE", "1" } }, .delay_s = 35, .game = game, .root = root },
    };
    var threads: [players.len]std.Thread = undefined;
    for (&players, &threads) |*player, *thread| thread.* = try std.Thread.spawn(.{}, playOne, .{player});
    for (threads) |thread| thread.join();
    for (players) |player| {
        if (player.failure) |err| {
            std.debug.print("the {s} Game failed ({any}); its output is in {s}\n", .{ player.name, err, root });
            return err;
        }
    }

    const host = players[0].run.?.log;
    const client = players[1].run.?.log;
    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    const host_states = try states(allocator, host);
    const client_states = try states(allocator, client);
    var common: usize = 0;
    var same: usize = 0;
    var distinct: std.StringHashMapUnmanaged(void) = .empty;
    var it = host_states.iterator();
    while (it.next()) |entry| {
        const other = client_states.get(entry.key_ptr.*) orelse continue;
        common += 1;
        if (std.mem.eql(u8, entry.value_ptr.*, other)) same += 1;
        const state = std.mem.lastIndexOf(u8, entry.value_ptr.*, "state=") orelse 0;
        try distinct.put(allocator, entry.value_ptr.*[state..], {});
    }
    const host_steps = try steps(allocator, host);
    const client_steps = try steps(allocator, client);
    try report.print(allocator, "host steps {any}, client steps {any}\n", .{ host_steps, client_steps });
    try report.print(allocator, "{d} seconds of game time in both, {d} the same state, {d} distinct states (host {d}, client {d} seconds)\n", .{ common, same, distinct.count(), host_states.count(), client_states.count() });
    for (players) |player| {
        const leaks = try control.lines(allocator, player.run.?.log, "bk_mem: ");
        try report.print(allocator, "{s} at exit: {s}\n", .{ player.name, if (leaks.len > 0) leaks[0] else "no report" });
    }
    // A minute of play in both, every second the same, the scene changing.
    if (common < min_mp_seconds or same != common or distinct.count() < 10) failed = true;
    // The host's Remastered decides the finder for both, and both compute the
    // same paths from the same orders (the trace without its timing).
    const host_paths = try fastestPaths(allocator, host);
    const client_paths = try fastestPaths(allocator, client);
    const both_fastest = std.mem.indexOf(u8, host, "BK_PATH_TRACE: mission finder fastest=1") != null and std.mem.indexOf(u8, client, "BK_PATH_TRACE: mission finder fastest=1") != null;
    // The two stop at different times: the paths both logs hold must be the same.
    const n_host = std.mem.count(u8, host_paths, "\n");
    const n_client = std.mem.count(u8, client_paths, "\n");
    const shorter = if (n_host < n_client) host_paths else client_paths;
    const longer = if (n_host < n_client) client_paths else host_paths;
    const same_paths = std.mem.startsWith(u8, longer, shorter);
    try report.print(allocator, "fastest path in both {any}; paths computed: host {d}, client {d}, the same {any}\n", .{ both_fastest, n_host, n_client, same_paths });
    if (!both_fastest or @min(n_host, n_client) == 0 or !same_paths) failed = true;
    // The two stood at different steps, and the client turned while playing.
    if (host_steps.len < 2 or client_steps.len < 3) failed = true;
    if (!failed) {
        var differ = false;
        for (client_steps[1..]) |s| differ = differ or s != host_steps[0];
        var turned = false;
        for (client_steps[1..], client_steps[0 .. client_steps.len - 1]) |s, before| turned = turned or s != before;
        if (!differ or !turned) failed = true;
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the two players do not play the same game at their steps; the logs are in {s}\n", .{ report.items, root });
        return error.OutOfSync;
    }
}

/// The fastest path's requests of a log (BK_PATH_TRACE), each without the
/// time it took, one per line.
fn fastestPaths(allocator: std.mem.Allocator, log: []const u8) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    for (try control.lines(allocator, log, "BK_PATH_TRACE: fastest ")) |line| {
        const end = std.mem.indexOf(u8, line, " us=") orelse line.len;
        try out.print(allocator, "{s}\n", .{line[0..end]});
    }
    return out.items;
}

/// A minute of play common to both logs.
const min_mp_seconds = 60;

/// The ground place of the n-th slot's 1.xml `WorldPosition` (world units).
fn worldPositionOf(xml: []const u8, n: usize) ![2]f64 {
    const begin = std.mem.indexOf(u8, xml, "<FireSlots>") orelse return error.NoSlots;
    var items = std.mem.splitSequence(u8, xml[begin..], "<item ");
    _ = items.next();
    var i: usize = 0;
    while (items.next()) |item| : (i += 1) {
        if (i != n) continue;
        const world = item[std.mem.indexOf(u8, item, "<WorldPosition ") orelse return error.NoSlots ..];
        return .{ try attribute(world, "x"), try attribute(world, "y") };
    }
    return error.NoSlots;
}

fn writeModXml(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn open test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
}
