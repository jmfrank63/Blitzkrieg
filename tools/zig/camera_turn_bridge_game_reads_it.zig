//! An engineering truck repairs a damaged bridge at every camera step (M006
//! round 2, docs/superpowers/specs/2026-10-26-camera-turn-design.md). The
//! maintainer's hand test: with the map turned, the repair truck did not find
//! the bridge.
//!
//! A map written from road3d.xml holds four bridges, one of each shipped kind
//! with both variants (the wooden little, asphalt, railway and wooden heavy
//! bridge, along x and y), laid as the Map Editor lays them (a begin span, line
//! spans, an end span; NMapGeometry::PlanBridge), each with an Opel Blitz
//! engineering truck of player 0 beside it. The bridges are damaged in play
//! (the map's script, DamageObject on the begin span, 0.6 of its HP). The real
//! Game is driven by BK_AUTO_UI; at step k the truck of bridge k is framed, the
//! view centred on bridge k and:
//!
//! - every point of a grid over the view where the shot shows something and
//!   the cursor picks the bridge must offer the repair (`pick=`, the order
//!   DetermineBestAutoAction gives the cursor there, CWorldClient::TracePick);
//! - a right click on the bridge's middle must order the repair (BK_AI_TRACE),
//!   and the bridge's HP must rise (BK_SPAN_TRACE).
//!
//! Run twice: Remastered on, bridge k ordered at step k; Remastered off, the
//! turn key pressed (it must do nothing), every bridge at step 0.
//!
//! The Game runs from zig-out/local-test/camera-turn/bridge with every user
//! path redirected; frames, logs and scores.txt stay there. BK_VIEWS_GAME names
//! the installed Game; `zig build camera-turn-bridge-game-reads-it` sets it.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/bridge";
const mod_name = "camera_turn_bridge";
const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
const ai_per_world = std.math.sqrt2;

/// A bridge of the map: its objects.xml name, whether it runs along x, its
/// line span's length (world units: Length * 16, as the Map Editor plans it),
/// how many line spans and its HP.
const Bridge = struct { name: []const u8, along_x: bool, line: f64, lines: usize, hp: f64 };
const bridges = [_]Bridge{
    .{ .name = "WoodenLittle_01", .along_x = true, .line = 32, .lines = 3, .hp = 250 },
    .{ .name = "AsphaltBridge_02", .along_x = false, .line = 64, .lines = 1, .hp = 1500 },
    .{ .name = "RailwayBridge_Little_01", .along_x = true, .line = 128, .lines = 0, .hp = 1500 },
    .{ .name = "WoodenBig_Heavy_02", .along_x = false, .line = 96, .lines = 1, .hp = 500 },
};
/// Each bridge's begin span (AI offsets from the map's middle), one in each
/// quarter, and its truck's offset from there.
const begins = [_][2]f64{ .{ -300, -300 }, .{ 300, -300 }, .{ 300, 300 }, .{ -300, 300 } };
const truck_at = [2]f64{ -250, -250 };
const script_base = 100;
/// The repair (USER_ACTION_ENGINEER_REPAIR_BUILDING, Common/Actions.h).
const repair_action = 25;
/// The pick grid: n x n points `pitch` pixels apart around the screen's centre.
const grid_n = 41;
const grid_pitch = 10;
const grid_origin = [2]i32{ turn.centre[0] - 200, turn.centre[1] - 200 };

/// Floors: every grid point on the drawn bridge that picks it offers the
/// repair (before the fix the girders, left at the HP they were made with,
/// offered none: up to 17 % of them); enough of them to count (the wooden
/// little bridge shows 113 at step 0); the HP a repair
/// adds in the time given (0.4 to 0.53 and more in the first runs).
const min_offered = 1.0;
const min_points = 100;
const min_rise = 0.1;

/// The AI point of the bridge's span j (0 begin, lines, last end).
fn spanAt(i: usize, j: usize) [2]f64 {
    const b = bridges[i];
    const along = if (j == 0) 0 else if (j == b.lines + 1) @as(f64, @floatFromInt(b.lines)) * b.line * ai_per_world else (@as(f64, @floatFromInt(j - 1)) + 0.5) * b.line * ai_per_world;
    const p = [2]f64{ control.middle + begins[i][0], control.middle + begins[i][1] };
    return if (b.along_x) .{ p[0] + along, p[1] } else .{ p[0], p[1] + along };
}

/// The world point (`camera=`) of the bridge's middle.
fn middleOf(i: usize) [2]f64 {
    const a = spanAt(i, 0);
    const b = spanAt(i, bridges[i].lines + 1);
    return .{ (a[0] + b[0]) / 2 / ai_per_world, (a[1] + b[1]) / 2 / ai_per_world };
}

/// The schedule of one run: bridge i is ordered at steps[i].
fn schedule(allocator: std.mem.Allocator, remastered: []const u8, steps: []const usize) ![]const u8 {
    var s: std.ArrayList(u8) = .empty;
    try s.print(allocator, "g1000:hover=1000x400,g10:set=GamePlay.Remastered={s},g10:lua=Damage(),g500:world=TraceSpans(0)", .{remastered});
    var at: usize = 0;
    for (steps, 0..) |k, i| {
        while (at != k) : (at = (at + 1) % 4) try s.appendSlice(allocator, ",g300:key=COMMA");
        // Remastered off: the key is pressed and must do nothing.
        if (k == 0 and i != 0) try s.appendSlice(allocator, ",g300:key=.");
        const truck = [2]f64{ (control.middle + begins[i][0] + truck_at[0]) / ai_per_world, (control.middle + begins[i][1] + truck_at[1]) / ai_per_world };
        try s.print(allocator, ",g10:camera={d:.0}x{d:.0},g300:drag={d}x{d}x{d}x{d},g500:world=TraceSelection(0),g1000:hover=512x200", .{ truck[0], truck[1], turn.centre[0] - 50, turn.centre[1] - 60, turn.centre[0] + 50, turn.centre[1] + 20 });
        const m = middleOf(i);
        try s.print(allocator, ",g10:camera={d:.0}x{d:.0},g300:anchor,g{d}:shot", .{ m[0], m[1], 800 + i });
        try s.print(allocator, ",g10:pick={d}x{d}x{d}x{d},g300:hover=512x200", .{ grid_origin[0], grid_origin[1], grid_pitch, grid_n });
        try s.print(allocator, ",g300:rclick={d}x{d},g500:hover=512x200", .{ turn.centre[0], turn.centre[1] });
    }
    try s.appendSlice(allocator, ",g30000:world=TraceSpans(0),g300:exit");
    return s.items;
}

test "a repair truck repairs a damaged bridge clicked at every camera step" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-bridge-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn bridge test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    try writeMap(io, allocator, data);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    const env = [_][2][]const u8{.{ "BK_AI_TRACE", "1" }};
    const runs = [_]struct { name: []const u8, remastered: []const u8, steps: [4]usize }{
        .{ .name = "on", .remastered = "ON", .steps = .{ 0, 1, 2, 3 } },
        .{ .name = "off", .remastered = "OFF", .steps = .{ 0, 0, 0, 0 } },
    };
    for (runs) |r| {
        const run = try control.runScheduleWith(io, allocator, game, root, mod_name, r.name, "bridge.xml", try schedule(allocator, r.remastered, &r.steps), objects_only, .{ .env = &env });
        const picks = try control.lines(allocator, run.log, "BK_PICK_TRACE: ");
        const orders = try control.lines(allocator, run.log, "BK_AI_TRACE: order click at ");
        const anchors = try control.parseAnchors(allocator, run.log);
        const hp = try spanHp(allocator, run.log);
        if (picks.len != bridges.len * grid_n * grid_n or orders.len != bridges.len or anchors.len != bridges.len or hp.len != 2) {
            std.debug.print("{s}: {d} picks, {d} orders, {d} anchors, {d} span traces; the Game's output is in {s}\n", .{ r.name, picks.len, orders.len, anchors.len, hp.len, run.log_path });
            return error.MissingTrace;
        }
        try report.print(allocator, "Remastered {s}: bridge, step: grid points on the drawn bridge offering the repair / picking it; the order at the middle; HP before -> after\n", .{r.remastered});
        for (bridges, 0..) |b, i| {
            const frame = try control.readShot(io, allocator, run.user, try std.fmt.allocPrint(allocator, "g{d}", .{800 + i}), root, try std.fmt.allocPrint(allocator, "{s}-{d}", .{ r.name, i }));
            var picking: usize = 0;
            var offered: usize = 0;
            for (picks[i * grid_n * grid_n ..][0 .. grid_n * grid_n], 0..) |line, p| {
                const x = grid_origin[0] + @as(i32, @intCast(p % grid_n)) * grid_pitch;
                const y = grid_origin[1] + @as(i32, @intCast(p / grid_n)) * grid_pitch;
                if (y < turn.clear_rows[0] or y >= turn.clear_rows[1]) continue;
                const px = turn.pixel(frame, x, y);
                if (@max(px[0], px[1], px[2]) <= 24) continue;
                // The first thing picked, which the cursor takes.
                const first = std.mem.indexOfScalar(u8, line, '"') orelse continue;
                if (!std.mem.startsWith(u8, line[first + 1 ..], b.name)) continue;
                picking += 1;
                if (actionOf(line) == repair_action) offered += 1;
            }
            const fraction = if (picking == 0) 0 else @as(f64, @floatFromInt(offered)) / @as(f64, @floatFromInt(picking));
            const ordered = std.mem.indexOf(u8, orders[i], std.fmt.comptimePrint("action {d} ", .{repair_action})) != null;
            const before = hp[0][i];
            const after = hp[1][i];
            try report.print(allocator, "{s}, {d}: {d} / {d} ({d:.3}); {s}; {d:.3} -> {d:.3}\n", .{ b.name, anchors[i].step, offered, picking, fraction, orders[i], before, after });
            if (anchors[i].step != r.steps[i] or picking < min_points or fraction < min_offered or !ordered or after < before + min_rise) failed = true;
        }
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a bridge was not repaired at a camera step; the frames and logs are in {s}\n", .{ report.items, root });
        return error.BridgeNotRepaired;
    }
}

/// The cursor's order in a pick line: "<x>,<y>: action <n>: ...".
fn actionOf(line: []const u8) i32 {
    const at = std.mem.indexOf(u8, line, "action ") orelse return -1;
    var end = at + "action ".len;
    while (end < line.len and std.ascii.isDigit(line[end])) end += 1;
    return std.fmt.parseInt(i32, line[at + "action ".len .. end], 10) catch -1;
}

/// Each TraceSpans: the HP of each bridge's begin span (the bridge gives all
/// its spans the same HP), found by its place.
fn spanHp(allocator: std.mem.Allocator, log: []const u8) ![]const [bridges.len]f64 {
    var out: std.ArrayList([bridges.len]f64) = .empty;
    var it = std.mem.tokenizeAny(u8, log, "\r\n");
    var current: ?[bridges.len]f64 = null;
    while (it.next()) |line| {
        if (std.mem.indexOf(u8, line, "Got command: TraceSpans") != null) {
            if (current) |c| try out.append(allocator, c);
            current = @splat(-1);
            continue;
        }
        const at = std.mem.indexOf(u8, line, "BK_SPAN_TRACE: ") orelse continue;
        var parts = std.mem.tokenizeAny(u8, line[at..], " ,");
        _ = parts.next();
        _ = parts.next();
        _ = parts.next();
        const x = try std.fmt.parseFloat(f64, parts.next().?);
        const y = try std.fmt.parseFloat(f64, parts.next().?);
        _ = parts.next();
        const value = try std.fmt.parseFloat(f64, parts.next().?);
        for (0..bridges.len) |i| {
            const p = spanAt(i, 0);
            if (@abs(x - p[0] / ai_per_world) < 1 and @abs(y - p[1] / ai_per_world) < 1) current.?[i] = value;
        }
    }
    if (current) |c| try out.append(allocator, c);
    return out.items;
}

/// bridge.xml from road3d.xml (the spans, their bridges and the trucks) and
/// bridge.lua, whose Damage() the schedule calls.
fn writeMap(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const map = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Bridges/>"));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Scripts></Scripts>"));
    var items: std.ArrayList(u8) = .empty;
    var list: std.ArrayList(u8) = .empty;
    var lua: std.ArrayList(u8) = .empty;
    try list.appendSlice(allocator, "<Bridges>");
    try lua.appendSlice(allocator, "function Damage()\r\n");
    var link: u32 = 1;
    for (bridges, 0..) |b, i| {
        try list.appendSlice(allocator, "<item><data>");
        for (0..b.lines + 2) |j| {
            var p = spanAt(i, j);
            // As PlanBridge: the begin a tenth back along x, the end a tenth on along y.
            if (j == 0 and b.along_x) p[0] -= 0.1;
            if (j == b.lines + 1 and !b.along_x) p[1] += 0.1;
            const frame: u32 = if (j == 0) 1 else if (j == b.lines + 1) 4 else 2;
            const script: i64 = if (j == 0) script_base + @as(i64, @intCast(i)) else -1;
            try items.print(allocator, "<item Direction=\"0\" Player=\"-1\" ScriptID=\"{d}\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
                "<Position x=\"{d:.1}\" y=\"{d:.1}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ script, frame, b.name, p[0], p[1], link });
            try list.print(allocator, "<item data=\"{d}\"/>", .{link});
            link += 1;
        }
        try list.appendSlice(allocator, "</data></item>");
        // DamageObject names the begin span; the bridge gives its other spans the same HP.
        try lua.print(allocator, "\tDamageObject({d}, {d:.0});\r\n", .{ script_base + i, b.hp * 0.6 });
        try control.item(allocator, &items, "Opel_Blitz_Engineering", .{ begins[i][0] + truck_at[0], begins[i][1] + truck_at[1] }, 0, 0, 0, &link);
    }
    try list.appendSlice(allocator, "</Bridges>");
    try lua.appendSlice(allocator, "end\r\n");
    const with_bridges = try std.mem.replaceOwned(u8, allocator, map, "<Bridges/>", list.items);
    const scripted = try std.mem.replaceOwned(u8, allocator, with_bridges, "<Scripts></Scripts>", "<Scripts>maps\\bridge</Scripts>");
    try control.writeMap(io, allocator, data, "bridge.xml", scripted, items.items);
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", "bridge.lua" }), .data = lua.items });
}
