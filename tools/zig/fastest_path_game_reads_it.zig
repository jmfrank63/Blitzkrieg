//! The fastest path (Remastered gameplay,
//! docs/superpowers/specs/2026-10-28-fastest-path-design.md) on the real Game.
//!
//! - **march**: a map written from road3d.xml with a garden (terrain speed
//!   0.3) across the straight way and an asphalt ring (speed 1) round it. A
//!   lorry, a car, a tank and a rifle squad of player 0 are ordered across it
//!   by the map's script, Remastered off (the 2003 finder) and on. Every unit
//!   must arrive (BK_AI_UNIT_TRACE: each segment's places), each kind no later
//!   with the fastest path than with the 2003 one, the lorry by the ring; the
//!   mission's finder must be the one the option chose (BK_PATH_TRACE).
//! - **replay and save**: the march recorded with Remastered on (-sh) and
//!   played back (-lh) by a player whose option is off must move every unit
//!   as the recording did; a save written on must load with the fastest path
//!   in that player's Game.
//! - **shipped maps**: kursk, dessau and vitebsk, the largest shipped maps,
//!   played for a while with Remastered on, the AI's own orders measured:
//!   requests, time per request (max and mean), nodes expanded, budget hits.
//!
//! The Game runs from zig-out/local-test/fastest-path with every user path
//! redirected; logs and scores.txt stay there. BK_VIEWS_GAME names the
//! installed Game; `zig build fastest-path-game-reads-it` sets it.
const std = @import("std");
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/fastest-path";
const mod_name = "fastest_path";
/// A unit's place in the map XML and in the script's orders is in AI units,
/// 32 to a tile; a visual tile (the terrain's) is 2 x 2 AI tiles.
const tile = 32.0;

/// The terrain of the march map, by visual tile: tileset 1's Garden (0.3),
/// Asphalt (1) and Bright Grass (0.8, road3d's own).
const garden: u8 = 140;
const asphalt: u8 = 210;
const grass: u8 = 154;

fn terrainAt(x: usize, y: usize) u8 {
    // Symmetric about the map's middle row, so the XML's row order (the AI's
    // y turned over) does not matter.
    const ring_x = (x == 7 or x == 8 or x == 23 or x == 24) and y >= 2 and y <= 29;
    const ring_y = (y == 2 or y == 3 or y == 28 or y == 29) and x >= 7 and x <= 24;
    if (ring_x or ring_y) return asphalt;
    if (x >= 11 and x <= 20 and y >= 5 and y <= 26) return garden;
    return grass;
}

/// A unit of the march: its name, start and goal (AI tiles), and its script id.
const Marcher = struct { kind: []const u8, name: []const u8, start: [2]f64, goal: [2]f64, script: u32 };
const marchers = [_]Marcher{
    .{ .kind = "lorry", .name = "Opel_Blitz_Cargo", .start = .{ 6, 18 }, .goal = .{ 57, 18 }, .script = 1 },
    .{ .kind = "car", .name = "Opel_Kadett", .start = .{ 6, 25 }, .goal = .{ 57, 25 }, .script = 2 },
    .{ .kind = "tank", .name = "Pz_Kpfw_IV_Ausf_D", .start = .{ 6, 32 }, .goal = .{ 57, 32 }, .script = 3 },
    .{ .kind = "squad", .name = "German_rifle_41", .start = .{ 5, 44 }, .goal = .{ 55, 44 }, .script = 4 },
};
/// How near its goal (AI tiles, either axis) a unit has arrived.
const arrived_within = 4;
/// Game time the march may take, ms.
const march_ms = 120000;

fn aiPoint(t: [2]f64) [2]f64 {
    return .{ (t[0] + 0.5) * tile, (t[1] + 0.5) * tile };
}

/// One BK_AI_UNIT_TRACE line: "<ms> ms unit <id> player <p> at <x> <y> tile <tx> <ty> moving <m>".
const Sample = struct { ms: u32, unit: i32, x: f64, y: f64, tx: i32, ty: i32 };

fn parseSample(line: []const u8) ?Sample {
    var it = std.mem.tokenizeScalar(u8, line, ' ');
    var words: [16][]const u8 = undefined;
    var n: usize = 0;
    while (it.next()) |w| {
        if (n == words.len) break;
        words[n] = w;
        n += 1;
    }
    if (n < 13) return null;
    return .{
        .ms = std.fmt.parseInt(u32, words[0], 10) catch return null,
        .unit = std.fmt.parseInt(i32, words[3], 10) catch return null,
        .x = std.fmt.parseFloat(f64, words[7]) catch return null,
        .y = std.fmt.parseFloat(f64, words[8]) catch return null,
        .tx = std.fmt.parseInt(i32, words[10], 10) catch return null,
        .ty = std.fmt.parseInt(i32, words[11], 10) catch return null,
    };
}

/// What a march did to each kind: when its last unit arrived (ms of game
/// time, null if one never did), the farthest its units went from the
/// straight line (AI tiles), and its units' places, segment by segment, as
/// a text to compare runs by.
const Outcome = struct { arrival: [marchers.len]?u32, swing: [marchers.len]i32, places: []const u8 };

fn measureMarch(allocator: std.mem.Allocator, log: []const u8) !Outcome {
    const samples = try control.lines(allocator, log, "BK_AI_UNIT_TRACE: ");
    // Each unit's kind: the marcher it stood nearest at its first sample.
    var kinds: std.AutoHashMapUnmanaged(i32, usize) = .empty;
    var arrived: std.AutoHashMapUnmanaged(i32, u32) = .empty;
    var places: std.ArrayList(u8) = .empty;
    var out = Outcome{ .arrival = @splat(0), .swing = @splat(0), .places = "" };
    for (samples) |line| {
        const s = parseSample(line) orelse continue;
        try places.print(allocator, "{d} {d} {d:.1} {d:.1}\n", .{ s.ms, s.unit, s.x, s.y });
        const kind = kinds.get(s.unit) orelse blk: {
            var best: usize = 0;
            var best_d: f64 = std.math.inf(f64);
            for (marchers, 0..) |m, k| {
                const p = aiPoint(m.start);
                const d = @abs(p[0] - s.x) + @abs(p[1] - s.y);
                if (d < best_d) {
                    best_d = d;
                    best = k;
                }
            }
            try kinds.put(allocator, s.unit, best);
            break :blk best;
        };
        const m = marchers[kind];
        const swing: i32 = @intCast(@abs(s.ty - @as(i32, @intFromFloat(m.start[1]))));
        out.swing[kind] = @max(out.swing[kind], swing);
        const near = @abs(s.tx - @as(i32, @intFromFloat(m.goal[0]))) <= arrived_within and @abs(s.ty - @as(i32, @intFromFloat(m.goal[1]))) <= arrived_within;
        if (near and !arrived.contains(s.unit)) try arrived.put(allocator, s.unit, s.ms);
    }
    var it = kinds.iterator();
    while (it.next()) |entry| {
        const kind = entry.value_ptr.*;
        if (arrived.get(entry.key_ptr.*)) |ms| {
            if (out.arrival[kind]) |prev| out.arrival[kind] = @max(prev, ms);
        } else out.arrival[kind] = null;
    }
    out.places = places.items;
    return out;
}

test "fastest path march round a slow field, replayed and saved" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build fastest-path-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" }));

    // The option is read as a mission starts: a player whose option is on is
    // a user folder where the main menu turned it on.
    _ = try control.runScheduleWith(io, allocator, game, root, mod_name, "option-on", "", "t3000:set=GamePlay.Remastered=ON,t1000:exit", "", .{});

    const env = [_][2][]const u8{ .{ "BK_AI_UNIT_TRACE", "1" }, .{ "BK_PATH_TRACE", "1" } };
    const march_schedule = std.fmt.comptimePrint("g{d}:exit", .{march_ms});
    const off = try control.runScheduleWith(io, allocator, game, root, mod_name, "march-off", "detour.xml", march_schedule, "", .{
        .env = &env,
        .timeout_s = 420,
        .extra = &.{try std.fmt.allocPrint(allocator, "-sh{s}", .{try std.fs.path.join(allocator, &.{ root, "march-off.rpl" })})},
    });
    const on = try control.runScheduleWith(io, allocator, game, root, mod_name, "march-on", "detour.xml", march_schedule, "", .{
        .env = &env,
        .timeout_s = 420,
        .user = "option-on",
        .extra = &.{try std.fmt.allocPrint(allocator, "-sh{s}", .{try std.fs.path.join(allocator, &.{ root, "march-on.rpl" })})},
    });

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    const off_finder = std.mem.indexOf(u8, off.log, "BK_PATH_TRACE: mission finder fastest=0") != null;
    const on_finder = std.mem.indexOf(u8, on.log, "BK_PATH_TRACE: mission finder fastest=1") != null;
    if (!off_finder or !on_finder) failed = true;
    const m_off = try measureMarch(allocator, off.log);
    const m_on = try measureMarch(allocator, on.log);
    try report.print(allocator, "march (ms of game time to the last unit within {d} tiles of its goal; farthest off the straight row, tiles)\n", .{arrived_within});
    for (marchers, 0..) |m, k| {
        try report.print(allocator, "  {s} ({s}): 2003 {?d} ms, swing {d}; fastest {?d} ms, swing {d}\n", .{ m.kind, m.name, m_off.arrival[k], m_off.swing[k], m_on.arrival[k], m_on.swing[k] });
        if (m_off.arrival[k] == null or m_on.arrival[k] == null) {
            failed = true;
        } else if (m_on.arrival[k].? > m_off.arrival[k].?) failed = true;
    }
    // The lorry goes round by the ring: from its row (18) to the ring's (AI y 4 to 7; the garden begins at 10).
    if (m_on.swing[0] < 11) failed = true;
    const on_requests = try pathRequests(allocator, on.log, "fastest");
    const off_requests = try pathRequests(allocator, off.log, "2003");
    try report.print(allocator, "  requests: 2003 finder {d} (mean {d:.0} us, max {d} us); fastest {d} (mean {d:.0} us, max {d} us, expanded max {d}, budget hits {d}, fallbacks to 2003 {d})\n", .{
        off_requests.count, off_requests.mean_us, off_requests.max_us, on_requests.count, on_requests.mean_us, on_requests.max_us, on_requests.max_expanded, on_requests.budget_hits, (try pathRequests(allocator, on.log, "2003")).count,
    });

    // The recording played back by a player whose option is off: the finder
    // and every unit's places are the recording's.
    const replay = try control.runScheduleWith(io, allocator, game, root, mod_name, "replay-off", "detour.xml", march_schedule, "", .{
        .env = &env,
        .timeout_s = 420,
        .extra = &.{try std.fmt.allocPrint(allocator, "-lh{s}", .{try std.fs.path.join(allocator, &.{ root, "march-on.rpl" })})},
    });
    const m_replay = try measureMarch(allocator, replay.log);
    const replay_finder = std.mem.indexOf(u8, replay.log, "BK_PATH_TRACE: mission finder fastest=1") != null;
    const same_places = samePrefix(m_on.places, m_replay.places);
    try report.print(allocator, "replay of the march (recorded on, played by a player with it off): finder fastest {any}, {d} of {d} unit samples the same\n", .{ replay_finder, same_places, std.mem.count(u8, m_on.places, "\n") });
    if (!replay_finder or same_places < std.mem.count(u8, m_on.places, "\n") * 9 / 10) failed = true;

    // The 2003 recording played back by a player whose option is on: the 2003 finder, the same march.
    const replay_2003 = try control.runScheduleWith(io, allocator, game, root, mod_name, "replay-2003-on", "detour.xml", march_schedule, "", .{
        .env = &env,
        .timeout_s = 420,
        .user = "option-on",
        .extra = &.{try std.fmt.allocPrint(allocator, "-lh{s}", .{try std.fs.path.join(allocator, &.{ root, "march-off.rpl" })})},
    });
    const m_replay_2003 = try measureMarch(allocator, replay_2003.log);
    const replay_2003_finder = std.mem.indexOf(u8, replay_2003.log, "BK_PATH_TRACE: mission finder fastest=0") != null;
    const same_2003 = samePrefix(m_off.places, m_replay_2003.places);
    try report.print(allocator, "replay of the 2003 march (recorded off, played by a player with it on): finder 2003 {any}, {d} of {d} unit samples the same\n", .{ replay_2003_finder, same_2003, std.mem.count(u8, m_off.places, "\n") });
    if (!replay_2003_finder or same_2003 < std.mem.count(u8, m_off.places, "\n") * 9 / 10) failed = true;

    // A save written on, loaded after the player turned the option off.
    _ = try control.runScheduleWith(io, allocator, game, root, mod_name, "save-on", "detour.xml", "g8000:cmdc=" ++ save_command ++ "=fastest.sav;0,g1000:exit", "", .{ .env = &env, .user = "option-on" });
    const loaded = try control.runScheduleWith(io, allocator, game, root, mod_name, "load-off", "", "t3000:set=GamePlay.Remastered=OFF,t1000:cmdc=" ++ load_command ++ "=fastest.sav,t5000:exit", "", .{ .env = &env, .user = "option-on" });
    const load_finder = std.mem.indexOf(u8, loaded.log, "BK_PATH_TRACE: loaded finder fastest=1") != null;
    try report.print(allocator, "save written with it on, loaded with it off: finder fastest {any}\n", .{load_finder});
    if (!load_finder) failed = true;

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the fastest path march failed; the logs are in {s}\n", .{ report.items, root });
        return error.MarchFailed;
    }
}

/// The largest shipped maps, and how long each is played.
const shipped_maps = [_][]const u8{ "kursk.bzm", "dessau.bzm", "vitebsk.bzm" };
const shipped_ms = 60000;

test "fastest path on the largest shipped maps" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build fastest-path-maps-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const maps_scratch = scratch ++ "-maps";
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), maps_scratch });
    cwd.deleteTree(io, maps_scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" }));
    _ = try control.runScheduleWith(io, allocator, game, root, mod_name, "option-on", "", "t3000:set=GamePlay.Remastered=ON,t1000:exit", "", .{});

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.print(allocator, "the AI's own orders over {d} s of game time, Remastered on\n", .{shipped_ms / 1000});
    const schedule = std.fmt.comptimePrint("g{d}:exit", .{shipped_ms});
    for (shipped_maps) |map| {
        const run = try control.runScheduleWith(io, allocator, game, root, mod_name, map[0 .. map.len - 4], map, schedule, "", .{
            .env = &.{.{ "BK_PATH_TRACE", "1" }},
            .timeout_s = 420,
            .user = "option-on",
        });
        const fastest = try pathRequests(allocator, run.log, "fastest");
        const fallback = try pathRequests(allocator, run.log, "2003");
        const big = try pathRequestsLong(allocator, run.log);
        try report.print(allocator, "  {s}: {d} requests, mean {d:.0} us, max {d} us, expanded max {d}; big paths {d}, mean {d:.0} us, max {d} us; budget hits {d}; answered by the 2003 finder {d}\n", .{
            map, fastest.count, fastest.mean_us, fastest.max_us, fastest.max_expanded, big.count, big.mean_us, big.max_us, fastest.budget_hits, fallback.count,
        });
        if (std.mem.indexOf(u8, run.log, "BK_PATH_TRACE: mission finder fastest=1") == null) failed = true;
        if (fastest.count == 0 or fastest.budget_hits != 0) failed = true;
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    std.debug.print("{s}", .{report.items});
    if (failed) {
        std.debug.print("the fastest path on the shipped maps failed; the logs are in {s}\n", .{root});
        return error.MapsFailed;
    }
}

/// The fastest path's big paths only (a unit's order, long=1).
fn pathRequestsLong(allocator: std.mem.Allocator, log: []const u8) !Requests {
    var only: std.ArrayList(u8) = .empty;
    for (try control.lines(allocator, log, "BK_PATH_TRACE: fastest long=1")) |line| try only.print(allocator, "BK_PATH_TRACE: fastest long=1{s}\n", .{line});
    return pathRequests(allocator, only.items, "fastest");
}

/// The console's save and load commands (remastered_game_reads_it.zig).
const save_command = "268501012";
const load_command = "268501013";

/// How many of the first text's lines the second begins with.
fn samePrefix(a: []const u8, b: []const u8) usize {
    var ia = std.mem.splitScalar(u8, a, '\n');
    var ib = std.mem.splitScalar(u8, b, '\n');
    var n: usize = 0;
    while (ia.next()) |la| {
        const lb = ib.next() orelse break;
        if (la.len == 0 or !std.mem.eql(u8, la, lb)) break;
        n += 1;
    }
    return n;
}

const Requests = struct { count: usize = 0, mean_us: f64 = 0, max_us: u64 = 0, max_expanded: u64 = 0, budget_hits: usize = 0 };

/// The BK_PATH_TRACE lines of one finder ("fastest" or "2003").
fn pathRequests(allocator: std.mem.Allocator, log: []const u8, finder: []const u8) !Requests {
    var r = Requests{};
    var total: u64 = 0;
    for (try control.lines(allocator, log, "BK_PATH_TRACE: ")) |line| {
        if (!std.mem.startsWith(u8, line, finder) or line.len <= finder.len or line[finder.len] != ' ') continue;
        const us = fieldValue(line, " us=") orelse continue;
        r.count += 1;
        total += us;
        r.max_us = @max(r.max_us, us);
        if (fieldValue(line, "expanded=")) |e| r.max_expanded = @max(r.max_expanded, e);
        if (fieldValue(line, "status=")) |s| {
            if (s == 4) r.budget_hits += 1;
        }
    }
    if (r.count > 0) r.mean_us = @as(f64, @floatFromInt(total)) / @as(f64, @floatFromInt(r.count));
    return r;
}

fn fieldValue(line: []const u8, key: []const u8) ?u64 {
    const at = std.mem.indexOf(u8, line, key) orelse return null;
    var end = at + key.len;
    while (end < line.len and std.ascii.isDigit(line[end])) end += 1;
    return std.fmt.parseInt(u64, line[at + key.len .. end], 10) catch null;
}

/// mod.xml, detour.xml (road3d.xml with the march's terrain, no road of its
/// own, the marchers and the script) and detour.lua (the orders).
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Fastest path test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    var map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    map = try rewriteTiles(map);
    map = try replaceBetween(allocator, map, "<Roads>", "</Roads>", "<Roads/>");
    map = try std.mem.replaceOwned(u8, allocator, map, "<Scripts></Scripts>", "<Scripts>detour</Scripts>");
    // No side calls its aircraft (road3d's 30 s): the march is not a fight.
    map = try std.mem.replaceOwned(u8, allocator, map, "RelaxTime=\"30\"", "RelaxTime=\"100000\"");

    var objects: std.ArrayList(u8) = .empty;
    var script: std.ArrayList(u8) = .empty;
    try script.appendSlice(allocator, "function Init()\r\n\tRunScript( \"Go\", 2000 );\r\nend;\r\n\r\nfunction Go()\r\n");
    for (marchers, 0..) |m, i| {
        const p = aiPoint(m.start);
        const g = aiPoint(m.goal);
        try objects.print(allocator, "<item Direction=\"16384\" Player=\"0\" ScriptID=\"{d}\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
            "<Position x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{
            m.script, @as(i32, if (std.mem.eql(u8, m.kind, "squad")) 0 else -1), m.name, p[0], p[1], i + 1,
        });
        try script.print(allocator, "\tGiveCommand( 0, {d}, {d:.0}, {d:.0} );\r\n", .{ m.script, g[0], g[1] });
    }
    try script.appendSlice(allocator, "\tSuicide();\r\nend;\r\n");
    try control.writeMap(io, allocator, data, "detour.xml", map, objects.items);
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "detour.lua" }), .data = script.items });
}

/// The terrain's tiles: a tile is two bytes in hex, its index first (the
/// tileset's), then its variant, kept.
fn rewriteTiles(map: []u8) ![]u8 {
    const begin = std.mem.indexOf(u8, map, "<tiles><item size_x=\"32\" size_y=\"32\"/>") orelse return error.NoTiles;
    var at = begin;
    for (0..32) |row| {
        at = (std.mem.indexOfPos(u8, map, at, "<item>") orelse return error.NoTiles) + "<item>".len;
        for (0..32) |x| {
            const hex = std.fmt.bytesToHex([1]u8{terrainAt(x, row)}, .upper);
            map[at + 4 * x] = hex[0];
            map[at + 4 * x + 1] = hex[1];
        }
    }
    return map;
}

fn replaceBetween(allocator: std.mem.Allocator, text: []const u8, open: []const u8, close: []const u8, with: []const u8) ![]u8 {
    const a = std.mem.indexOf(u8, text, open) orelse return error.NotFound;
    const b = std.mem.indexOfPos(u8, text, a, close) orelse return error.NotFound;
    return std.mem.concat(allocator, u8, &.{ text[0..a], with, text[b + close.len ..] });
}
