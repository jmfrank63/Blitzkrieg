//! The camera step in saves, replays and multiplayer (M006 S05,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md). With the
//! Remastered option on, the real Game is driven by BK_AUTO_UI:
//!
//! - save and load: a mission saved at step 2 and again at step 0; loading the
//!   first must bring the view back at step 2, the same picture as when it
//!   was saved, and loading the second after another turn must bring the
//!   2003 view (step 0), as every save made before the step existed does.
//! - replay: a mission is recorded (`-sh`) with the view turned, a unit
//!   clicked where the turn draws it and ordered to a ground point clicked the
//!   same way, under fire from an enemy tank; the replay (`-lh`) is played
//!   twice, at step 0 and turned back and forth with the view moved away. The
//!   units' state (BK_AI_STATE_TRACE: centre, direction, HP and morale of every
//!   unit, the multiplayer checksum's terms, once a second of game time) must
//!   be the same in both plays at every second: the step never enters the
//!   game state or the commands.
//! - the multiplayer start step: four maps with the player's camera anchor in
//!   each corner; the rule (BK_MAP_TRACE "start turn") must give the step that
//!   puts that corner at the bottom of the screen (screen down is world
//!   (1, -1) at step 0, and step k shows the world turned k times by
//!   (x, y) -> (-y, x)), and a single player mission must still start at
//!   step 0.
//!
//! The Game runs from zig-out/local-test/camera-turn/state with every user path
//! redirected; frames, logs and scores.txt stay there. BK_VIEWS_GAME names the
//! installed Game; `zig build camera-turn-state-game-reads-it` sets it.
const std = @import("std");
const turn = @import("camera_turn_game_reads_it.zig");
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/state";
const mod_name = "camera_turn_state";
const objects_only = "shadows,effects,terrain,noise,warfog,border,haze,grid";
/// MAIN_COMMAND_SAVE and MAIN_COMMAND_LOAD (Main/iMainClassIDs.h).
const save_command = "268501012";
const load_command = "268501013";

/// road3d is 32 tiles a side, a tile 32 sqrt 2 world units: its middle, and
/// the player's camera anchor in each corner, 200 units in from the edges,
/// with the step that puts that corner at the bottom of the screen: (+, -) is
/// down at step 0, and (x, y) -> (-y, x) takes (+, +) there in 3 steps,
/// (-, +) in 2 and (-, -) in 1.
const map_middle = 16.0 * 32.0 * std.math.sqrt2;
const Corner = struct { map: []const u8, sign: [2]f64, step: i32 };
const corners = [_]Corner{
    .{ .map = "corner0.xml", .sign = .{ 1, -1 }, .step = 0 },
    .{ .map = "corner1.xml", .sign = .{ -1, -1 }, .step = 1 },
    .{ .map = "save.xml", .sign = .{ 1, 1 }, .step = 3 },
    .{ .map = "fight.xml", .sign = .{ -1, 1 }, .step = 2 },
};
fn anchorOf(corner: Corner) [2]f64 {
    return .{ map_middle + corner.sign[0] * (map_middle - 200), map_middle + corner.sign[1] * (map_middle - 200) };
}

/// The replay's units (AI offsets from the map's middle): a sniper and a squad
/// of player 0, a T-34 of player 1 in range of them; the sniper is ordered to
/// `order_at`.
const sniper_at = [2]f64{ -100, -150 };
const order_at = [2]f64{ -150, -450 };

/// Floors: the loaded view is the saved one but for the odd pixel; the plays
/// cover at least this many seconds of game time together.
const max_load_differ = 0.01;
const min_common_seconds = 10;

test "the camera step in saves, replays and the multiplayer start" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-state-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" }));

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    var start_logs: [corners.len][]const u8 = undefined;

    // Save at step 2, then at step 0; load the first, turn once, load the second.
    {
        const schedule = "g1000:hover=1000x400,g10:camera=512x512,g10:set=GamePlay.Remastered=ON,g10:anchor,g10:key=SPACE,t10:key=.,t300:key=.,t1500:shot,t10:anchor," ++
            "t10:cmdc=" ++ save_command ++ "=turned.sav;0,t1500:key=.,t300:key=.,t300:anchor,t10:cmdc=" ++ save_command ++ "=plain.sav;0,t1500:exit";
        const saved = try control.runSchedule(io, allocator, game, root, mod_name, "save", "save.xml", schedule, objects_only);
        start_logs[2] = saved.log;
        const load_schedule = "g500:set=GamePlay.Remastered=ON,60:cmdc=" ++ load_command ++ "=turned.sav,t2000:anchor,t11:shot,t10:key=.,t300:anchor," ++
            "t10:cmdc=" ++ load_command ++ "=plain.sav,t2000:anchor,t500:exit";
        const loaded = try control.runScheduleWith(io, allocator, game, root, mod_name, "load", "", load_schedule, objects_only, .{ .user = "save" });
        const saved_anchors = try control.parseAnchors(allocator, saved.log);
        const loaded_anchors = try control.parseAnchors(allocator, loaded.log);
        if (saved_anchors.len != 3 or loaded_anchors.len != 3) {
            std.debug.print("save: {d} and {d} anchors; the logs are in {s}\n", .{ saved_anchors.len, loaded_anchors.len, root });
            return error.MissingTrace;
        }
        const before = try control.readShot(io, allocator, saved.user, "t1500", root, "saved-at-step-2");
        const after = try control.readShot(io, allocator, loaded.user, "t11", root, "loaded-at-step-2");
        const differ = differing(before, after);
        try report.print(allocator, "save: steps {d} (mission start), {d} (saved), {d} (saved again); load: step {d} at {d:.1},{d:.1} (saved at {d:.1},{d:.1}), {d:.4} of the view's pixels differ; turned to {d}, then the other save loads at step {d}\n", .{
            saved_anchors[0].step, saved_anchors[1].step, saved_anchors[2].step, loaded_anchors[0].step, loaded_anchors[0].anchor[0], loaded_anchors[0].anchor[1], saved_anchors[1].anchor[0], saved_anchors[1].anchor[1], differ, loaded_anchors[1].step, loaded_anchors[2].step,
        });
        if (saved_anchors[0].step != 0 or saved_anchors[1].step != 2 or saved_anchors[2].step != 0) failed = true;
        if (loaded_anchors[0].step != 2 or loaded_anchors[1].step != 3 or loaded_anchors[2].step != 0) failed = true;
        if (differ > max_load_differ) failed = true;
    }

    // Record a replay at step 1, then play it at step 0 and turned.
    {
        const history = try std.fs.path.join(allocator, &.{ root, "history.rep" });
        const sniper = control.screenAt(sniper_at, 1);
        const to = control.screenAt(order_at, 1);
        const record_schedule = try std.fmt.allocPrint(allocator, "g1000:hover=1000x400,g10:camera=512x512,g10:set=GamePlay.Remastered=ON,g10:anchor,g500:key=.,g300:click={d}x{d},g300:world=TraceSelection(0),g10:rclick={d}x{d},g12000:world=TraceSelection(0),g10:exit", .{ sniper[0], sniper[1] - 12, to[0], to[1] });
        const recorded = try control.runScheduleWith(io, allocator, game, root, mod_name, "record", "fight.xml", record_schedule, "", .{ .extra = &.{try std.fmt.allocPrint(allocator, "-sh{s}", .{history})}, .state_trace = true });
        start_logs[3] = recorded.log;
        const history_size = (try cwd.readFileAlloc(io, history, allocator, .limited(16 << 20))).len;
        const lh = try std.fmt.allocPrint(allocator, "-lh{s}", .{history});
        const plain = try control.runScheduleWith(io, allocator, game, root, mod_name, "play-0", "fight.xml", "g1000:hover=1000x400,g10:camera=512x512,g10:set=GamePlay.Remastered=ON,g16000:exit", "", .{ .extra = &.{lh}, .state_trace = true });
        const turned = try control.runScheduleWith(io, allocator, game, root, mod_name, "play-turned", "fight.xml", "g1000:hover=1000x400,g10:camera=512x512,g10:set=GamePlay.Remastered=ON,g500:key=.,g700:key=.,g1100:key=COMMA,g1300:key=.,g1700:key=.,g500:camera=200x900,g2000:key=.,g16000:exit", "", .{ .extra = &.{lh}, .state_trace = true });
        const a = try control.lines(allocator, plain.log, "BK_AI_STATE_TRACE: ");
        const b = try control.lines(allocator, turned.log, "BK_AI_STATE_TRACE: ");
        const r = try control.lines(allocator, recorded.log, "BK_AI_STATE_TRACE: ");
        const common = @min(a.len, b.len);
        var same: usize = 0;
        var distinct: usize = 0;
        for (0..common) |i| {
            if (std.mem.eql(u8, a[i], b[i])) same += 1;
            if (i == 0 or !std.mem.eql(u8, a[i], a[i - 1])) distinct += 1;
        }
        var as_recorded: usize = 0;
        for (0..@min(common, r.len)) |i| {
            if (std.mem.eql(u8, a[i], r[i])) as_recorded += 1;
        }
        const recorded_selections = try control.lines(allocator, recorded.log, "BK_SELECTED: ");
        try report.print(allocator, "replay: recorded at step 1 ({d} bytes), the sniper clicked and ordered where the turn draws them: {d} selection lines ({s} ... {s})\n", .{ history_size, recorded_selections.len, if (recorded_selections.len > 0) recorded_selections[0] else "none", if (recorded_selections.len > 1) recorded_selections[recorded_selections.len - 1] else "none" });
        try report.print(allocator, "  plays at step 0 and turned: {d} of {d} seconds the same state ({d} distinct states; first {s}, last {s}); the recording run's own (another random seed) the same for {d}\n", .{ same, common, distinct, if (common > 0) a[0] else "none", if (common > 0) a[common - 1] else "none", as_recorded });
        if (common < min_common_seconds or same != common or distinct < 3) failed = true;
        // The order was given where the turn draws the ground: the sniper walked there.
        const target = [2]f64{ (control.middle + order_at[0]) / std.math.sqrt2, (control.middle + order_at[1]) / std.math.sqrt2 };
        const walked = if (recorded_selections.len >= 2) placeOf(recorded_selections[recorded_selections.len - 1]) else null;
        const miss = if (walked) |w| std.math.hypot(w[0] - target[0], w[1] - target[1]) else 1e9;
        try report.print(allocator, "  the recorded order to {d:.0},{d:.0}: the sniper {d:.1} from it\n", .{ target[0], target[1], miss });
        if (miss > 16) failed = true;
    }

    // The multiplayer start step's rule on four corners.
    for (corners[0..2], 0..) |corner, i| {
        const run = try control.runSchedule(io, allocator, game, root, mod_name, corner.map[0 .. corner.map.len - 4], corner.map, "g1000:set=GamePlay.Remastered=ON,g10:anchor,g10:exit", objects_only);
        start_logs[i] = run.log;
    }
    try report.appendSlice(allocator, "multiplayer start step: corner of the player's anchor: the rule's step (expected); the single player mission's step at the start\n");
    for (corners, start_logs) |corner, log| {
        const traced = try control.lines(allocator, log, "BK_MAP_TRACE: start turn ");
        const step = if (traced.len == 1) std.fmt.parseInt(i32, traced[0][0..1], 10) catch -1 else -1;
        const anchors = try control.parseAnchors(allocator, log);
        const at = anchorOf(corner);
        try report.print(allocator, "  {s} {d:.0},{d:.0}: {d} ({d}); {d}\n", .{ corner.map, at[0], at[1], step, corner.step, if (anchors.len > 0) anchors[0].step else -1 });
        if (step != corner.step or anchors.len == 0 or anchors[0].step != 0) failed = true;
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the camera step is kept wrong; the frames and logs are in {s}\n", .{ report.items, root });
        return error.StateWrong;
    }
}

/// The place at the end of a BK_SELECTED line.
fn placeOf(line: []const u8) ?[2]f64 {
    const at = std.mem.lastIndexOf(u8, line, " at ") orelse return null;
    var parts = std.mem.tokenizeScalar(u8, line[at + 4 ..], ',');
    const x = std.fmt.parseFloat(f64, parts.next() orelse return null) catch return null;
    const y = std.fmt.parseFloat(f64, parts.next() orelse return null) catch return null;
    return .{ x, y };
}

/// The fraction of the view's pixels (but the interface rows and the pause
/// line) whose colours differ by a channel sum of more than 48.
fn differing(a: []const u8, b: []const u8) f64 {
    var differ: f64 = 0;
    var count: f64 = 0;
    var y: i32 = turn.clear_rows[0];
    while (y < turn.clear_rows[1]) : (y += 1) {
        if (y >= 360 and y <= 405) continue;
        var x: i32 = 0;
        while (x < turn.width) : (x += 1) {
            const i: usize = @intCast((y * turn.width + x) * 4);
            var sum: i32 = 0;
            for (0..3) |c| sum += @intCast(@abs(@as(i32, a[i + c]) - @as(i32, b[i + c])));
            if (sum > 48) differ += 1;
            count += 1;
        }
    }
    return differ / count;
}

/// mod.xml and the maps, written from road3d.xml, each with the player's
/// camera anchor in its corner.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn state test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const road3d = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    for (corners) |corner| {
        const at = anchorOf(corner);
        const before = "<SeasonFolder>";
        const anchored = try std.mem.replaceOwned(u8, allocator, road3d, before, try std.fmt.allocPrint(allocator, "<PlayerCameraAnchors><item x=\"{d:.0}\" y=\"{d:.0}\" z=\"0\"/></PlayerCameraAnchors>{s}", .{ at[0], at[1], before }));
        try testing.expectEqual(@as(usize, 1), std.mem.count(u8, road3d, before));
        var objects: std.ArrayList(u8) = .empty;
        var link: u32 = 1;
        if (std.mem.eql(u8, corner.map, "save.xml")) {
            for ([_]u32{ 0, 2, 4 }) |e| {
                const angle = @as(f64, @floatFromInt(e)) * std.math.pi / 4.0;
                try control.item(allocator, &objects, "German_sniper", .{ 180 * @cos(angle), 180 * @sin(angle) }, 0, e * 8192, 0, &link);
            }
            try control.item(allocator, &objects, "Swing01", .{ 350, -350 }, -1, 0, -1, &link);
            try control.item(allocator, &objects, "E_House01_1", .{ -380, -380 }, -1, 0, -1, &link);
        } else if (std.mem.eql(u8, corner.map, "fight.xml")) {
            try control.item(allocator, &objects, "German_sniper", sniper_at, 0, 0, 0, &link);
            try control.item(allocator, &objects, "German_rifle_41", .{ -250, 0 }, 0, 0, 0, &link);
            try control.item(allocator, &objects, "T-34", .{ 250, 100 }, 1, 0, -1, &link);
        }
        try control.writeMap(io, allocator, data, corner.map, anchored, objects.items);
    }
}
