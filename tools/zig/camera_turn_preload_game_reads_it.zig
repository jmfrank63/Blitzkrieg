//! The turn preload (M006 round 2, docs/superpowers/specs/2026-10-26-camera-turn-design.md,
//! "The turn preload"): the maintainer found a camera turn on the second
//! tutorial's map taking 3 to 4 seconds and asked for it to be near instant.
//!
//! The 27 building variants of `Data/Maps/Tutorial/tutorial2.bzm` get their
//! turns lit by the world's sun (`bk-views side ... 1,2,3,1l1,2l2,3l3`) and
//! the flora its relit pictures (`bk-views relight`) into a scratch mod at
//! their Data paths, as `zig build camera-turn-mod` puts the batch's. The real
//! Game opens the map with Remastered on, pauses, waits for the preload and
//! presses the turn key four times one way (`.`) and four times the other
//! (`,`), a shot after each. It runs twice: as built, and with the preload and
//! the staged turn off (`BK_TURN_PRELOAD=0`, every object drawn anew on the
//! turn's frame). `BK_TURN_TRACE=1` prints each turn's frame and the two after
//! it with their phases, the mission's load and its first frames, resident
//! and texture memory, and, once every object is drawn for the new step, the
//! pictures the turn drew (a count and an order-free sum of their names'
//! hashes).
//!
//! Held:
//! - every turn draws the same pictures in both runs (count and sum);
//! - the preload is done before the first press;
//! - every turn's frame of the preloaded run takes at most half the mean of
//!   the other run's; both are reported, with the frames after the turn,
//!   load times and memory, in `scores.txt`;
//! - no leak at exit in either run.
//!
//! The shots of the two runs are compared and the differing pixels reported,
//! not held: two runs of the same Game differ where the map animates by the
//! clock that runs on while paused (river water, some ground decals), and
//! where two sprites overlap the renderer orders them by their textures'
//! addresses (`CSpritesSortFunctional`), which any change in what was
//! allocated first moves (measured: the preload, or the staging alone, swaps
//! a few overlapping crowns; the pictures drawn are the same).
//!
//! The Game runs from zig-out/local-test/camera-turn/preload with every user
//! path redirected; frames, logs and scores.txt stay there. BK_VIEWS_GAME
//! names the installed Game and BK_VIEWS_TOOL bk-views;
//! `zig build camera-turn-preload-game-reads-it` sets both.
const std = @import("std");
const views = @import("bk_views.zig");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const game_views = @import("building_views_game_reads_it.zig");
const turn = @import("camera_turn_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/preload";
const map = "tutorial\\tutorial2.bzm";
/// The building variants the second tutorial's map holds (the Game's "no
/// pictures of" lines with an empty mod), under Data/Buildings.
const variants = [_][]const u8{
    "common/summer/dzot/01",              "europe/summer/e_chapel_1",       "europe/summer/e_house01_1",
    "europe/summer/e_house01_2",          "europe/summer/e_house02_1",      "europe/summer/e_house02_2",
    "europe/summer/e_house02_4",          "europe/summer/e_house03_2",      "europe/summer/e_house03_4",
    "europe/summer/e_house04_4",          "europe/summer/e_jettybuilding_1", "europe/summer/e_shed01_3",
    "europe/summer/e_shed02_4",           "europe/summer/e_stella/01",      "europe/summer/e_wb/01",
    "europe/summer/e_wb/02",              "europe/summer/e_welllittle/01",  "europe/summer/e_welllittle/02",
    "europe/summer/e_wt/01",              "europe/summer/e_wt/02",          "ussr/summer/house02",
    "ussr/summer/house05",                "ussr/summer/house09",            "ussr/summer/house10",
    "ussr/summer/house11",                "ussr/summer/shed01",             "ussr/summer/shed02",
};
/// The keys pressed, a shot after each: four quarter turns one way, four back.
const presses = [_][]const u8{ ".", ".", ".", ".", "COMMA", "COMMA", "COMMA", "COMMA" };
const shots = presses.len + 1;

/// What BK_TURN_TRACE printed: per press the turn's frame and the two after
/// it (ms), the mission's load, its first frame, whether the preload was done
/// before the first press, and the memory after the last turn (MB).
const Trace = struct {
    turn: [presses.len][3]f64 = @splat(@splat(0)),
    drawn: [presses.len][2]u64 = @splat(@splat(0)),
    load: f64 = 0,
    first_frame: f64 = 0,
    preload_done: ?f64 = null,
    resident: f64 = 0,
    textures: f64 = 0,
};

test "a camera turn on the second tutorial switches what is drawn: quick, and drawn as without the preload" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-preload-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try game_views.envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", turn.mod_name, "data" });
    const work = try std.fs.path.join(allocator, &.{ root, "work" });
    for (variants) |variant| {
        const out = try std.fs.path.join(allocator, &.{ data, "buildings", std.fs.path.dirname(variant).? });
        try cwd.createDirPath(io, out);
        try game_views.runTool(io, allocator, tool, &.{ "side", try std.fmt.allocPrint(allocator, "Data/Buildings/{s}", .{variant}), "1,2,3,1l1,2l2,3l3", out, work });
    }
    try game_views.runTool(io, allocator, tool, &.{ "relight", "Data/Objects/Flora", try std.fs.path.join(allocator, &.{ data, "objects", "flora" }) });
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn preload test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });

    const on = try runGame(io, allocator, game, root, "preloaded", true);
    const off = try runGame(io, allocator, game, root, "whole", false);

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "press key: turn's frame, the two after (ms), preloaded / whole; pictures drawn; pixels differing\n");
    var off_mean: f64 = 0;
    for (off.trace.turn) |t| off_mean += t[0] / @as(f64, presses.len);
    for (0..shots) |k| {
        var differing: usize = 0;
        for (0..turn.width * turn.height) |p| {
            if (!std.mem.eql(u8, on.frames[k][p * 4 ..][0..3], off.frames[k][p * 4 ..][0..3])) differing += 1;
        }
        if (k == 0) {
            try report.print(allocator, "start: pixels differing {d}\n", .{differing});
            continue;
        }
        const a = on.trace.turn[k - 1];
        const b = off.trace.turn[k - 1];
        const da = on.trace.drawn[k - 1];
        const db = off.trace.drawn[k - 1];
        try report.print(allocator, "{d} {s}: {d:.1}, {d:.1}, {d:.1} / {d:.1}, {d:.1}, {d:.1}; {d} {x:0>8} / {d} {x:0>8}; {d}\n", .{ k, presses[k - 1], a[0], a[1], a[2], b[0], b[1], b[2], da[0], da[1], db[0], db[1], differing });
        if (a[0] <= 0 or a[0] > off_mean / 2) failed = true;
        if (da[0] == 0 or da[0] != db[0] or da[1] != db[1]) failed = true;
    }
    try report.print(allocator, "whole turn's frame, mean: {d:.1} ms\n", .{off_mean});
    try report.print(allocator, "mission load: {d:.0} / {d:.0} ms; its first frame {d:.0} / {d:.0} ms\n", .{ on.trace.load, off.trace.load, on.trace.first_frame, off.trace.first_frame });
    try report.print(allocator, "preload done in {d:.0} ms of frames\n", .{on.trace.preload_done orelse -1});
    try report.print(allocator, "after the last turn: resident {d:.0} / {d:.0} MB, textures {d:.0} / {d:.0} MB\n", .{ on.trace.resident, off.trace.resident, on.trace.textures, off.trace.textures });
    if (on.trace.preload_done == null) failed = true;

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a turn is slow or drawn other than without the preload; the frames are in {s}\n", .{ report.items, root });
        return error.DrawnWrong;
    }
}

const Run = struct { frames: [shots][]u8, trace: Trace };

/// The Game on the tutorial map, paused, a shot before the first press and
/// after each.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, name: []const u8, preload: bool) !Run {
    const cwd = std.Io.Dir.cwd();
    const user = try std.fs.path.join(allocator, &.{ root, "user", name });
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
    try environ.put("BK_TURN_TRACE", "1");
    if (!preload) try environ.put("BK_TURN_PRELOAD", "0");
    // Paused (SPACE) the game time stands still, so the waits count the
    // timer's absolute time; the first is long enough for the preload.
    var schedule: std.ArrayList(u8) = .empty;
    try schedule.appendSlice(allocator, "g1000:hover=1000x400,g10:set=GamePlay.Remastered=ON,g10:key=SPACE,t8000:shot");
    for (presses, 0..) |key, k| try schedule.print(allocator, ",t10:key={s},t{d}:shot", .{ key, 3001 + k });
    try schedule.appendSlice(allocator, ",t200:exit");
    try environ.put("BK_AUTO_UI", schedule.items);
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", try std.fmt.allocPrint(allocator, "-mode={d}x{d}", .{ turn.width, turn.height }), "-profile=Turn", "-mod=" ++ turn.mod_name, map },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(300), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    var run: Run = .{ .frames = undefined, .trace = try parseTrace(log) };
    for (&run.frames, 0..) |*frame, k| {
        const shot_name = try std.fmt.allocPrint(allocator, "autoshot_t{d}_{d}x{d}.rgba", .{ if (k == 0) 8000 else 3000 + k, turn.width, turn.height });
        frame.* = cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ user, shot_name }), allocator, .limited(64 << 20)) catch |err| {
            std.debug.print("no shot {s} from the Game ({s}); its output is in {s}\n", .{ shot_name, name, log_path });
            return err;
        };
        try testing.expectEqual(@as(usize, turn.width * turn.height * 4), frame.len);
        const image: views.Image = .{ .width = turn.width, .height = turn.height, .pixels = std.mem.bytesAsSlice([4]u8, frame.*) };
        try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(allocator, "{s}/{s}-{d}.png", .{ root, name, k }), .data = try views.writePng(allocator, image) });
    }
    return run;
}

/// The value after `key ` in a trace line.
fn field(line: []const u8, key: []const u8) ?f64 {
    var it = std.mem.tokenizeScalar(u8, line, ' ');
    while (it.next()) |token| {
        if (!std.mem.eql(u8, token, key)) continue;
        return std.fmt.parseFloat(f64, it.next() orelse return null) catch null;
    }
    return null;
}

fn parseTrace(log: []const u8) !Trace {
    var trace: Trace = .{};
    var press: ?usize = null;
    // The drawn lines of every turn, the option's own (Remastered set after
    // the load) first: the presses' are the last ones.
    var drawn: [32][2]u64 = undefined;
    var drawn_count: usize = 0;
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        const at = std.mem.indexOf(u8, line, "BK_TURN_TRACE: ") orelse continue;
        const text = line[at + "BK_TURN_TRACE: ".len ..];
        if (std.mem.startsWith(u8, text, "mission load ")) {
            trace.load = field(text, "load") orelse 0;
        } else if (std.mem.startsWith(u8, text, "load 0 frame 0 ")) {
            trace.first_frame = field(text, "total") orelse 0;
        } else if (std.mem.startsWith(u8, text, "drawn for step ")) {
            if (drawn_count == drawn.len) return error.TurnsNotTraced;
            var it = std.mem.tokenizeAny(u8, text, " :,");
            var values: [2]u64 = .{ 0, 0 };
            var previous: []const u8 = "";
            while (it.next()) |token| {
                if (std.mem.eql(u8, token, "pictures")) values[0] = std.fmt.parseInt(u64, previous, 10) catch 0;
                if (std.mem.eql(u8, previous, "sum")) values[1] = std.fmt.parseInt(u64, token, 16) catch 0;
                previous = token;
            }
            drawn[drawn_count] = values;
            drawn_count += 1;
        } else if (std.mem.startsWith(u8, text, "preload done in ")) {
            if (press == null) trace.preload_done = field(text, "in");
        } else if (std.mem.startsWith(u8, text, "step ")) {
            const frame: usize = @intFromFloat(field(text, "frame") orelse continue);
            if (frame == 0) press = if (press) |p| p + 1 else 0;
            const p = press orelse continue;
            if (p >= presses.len or frame >= 3) continue;
            trace.turn[p][frame] = field(text, "total") orelse 0;
            trace.resident = field(text, "resident") orelse 0;
            trace.textures = field(text, "textures") orelse 0;
        }
    }
    if (press == null or press.? + 1 != presses.len or drawn_count < presses.len) return error.TurnsNotTraced;
    for (0..presses.len) |p| trace.drawn[p] = drawn[drawn_count - presses.len + p];
    return trace;
}

test "the trace's lines are read" {
    const log =
        "BK_TURN_TRACE: mission load 2240 ms\r\n" ++
        "BK_TURN_TRACE: load 0 frame 0 total 2527.97 world 6.18 resident 485 textures 25\n" ++
        "BK_TURN_TRACE: preload done in 1218 ms over 84 frames\n" ++
        "BK_TURN_TRACE: drawn for step 0: 12 pictures, sum 0000000a\n" ++
        "BK_TURN_TRACE: step 1 frame 0 total 55.48 world 34.48 resident 537 textures 37\n" ++
        "BK_TURN_TRACE: drawn for step 1: 1267 pictures, sum 89abcdef\n" ++
        "BK_TURN_TRACE: step 1 frame 1 total 13.63 world 4.03 resident 539 textures 37\n" ++
        "BK_TURN_TRACE: step 2 frame 0 total 36.16 world 23.10 resident 541 textures 37\n" ++
        "BK_TURN_TRACE: step 3 frame 0 total 1 resident 1 textures 1\n" ++
        "BK_TURN_TRACE: step 0 frame 0 total 1 resident 1 textures 1\n" ++
        "BK_TURN_TRACE: step 3 frame 0 total 1 resident 1 textures 1\n" ++
        "BK_TURN_TRACE: step 2 frame 0 total 1 resident 1 textures 1\n" ++
        "BK_TURN_TRACE: step 1 frame 0 total 1 resident 1 textures 1\n" ++
        "BK_TURN_TRACE: step 0 frame 0 total 2 resident 542 textures 38\n" ++
        "BK_TURN_TRACE: drawn for step 2: 1 pictures, sum 00000001\n" ++
        "BK_TURN_TRACE: drawn for step 3: 1 pictures, sum 00000001\n" ++
        "BK_TURN_TRACE: drawn for step 0: 1 pictures, sum 00000001\n" ++
        "BK_TURN_TRACE: drawn for step 3: 1 pictures, sum 00000001\n" ++
        "BK_TURN_TRACE: drawn for step 2: 1 pictures, sum 00000001\n" ++
        "BK_TURN_TRACE: drawn for step 1: 1 pictures, sum 00000001\n" ++
        "BK_TURN_TRACE: drawn for step 0: 2 pictures, sum 00000002\n";
    const trace = try parseTrace(log);
    try testing.expectEqual(@as(f64, 2240), trace.load);
    try testing.expectEqual(@as(f64, 2527.97), trace.first_frame);
    try testing.expectEqual(@as(?f64, 1218), trace.preload_done);
    try testing.expectEqual(@as(f64, 55.48), trace.turn[0][0]);
    try testing.expectEqual(@as(f64, 13.63), trace.turn[0][1]);
    try testing.expectEqual(@as(f64, 36.16), trace.turn[1][0]);
    try testing.expectEqual(@as(f64, 2), trace.turn[7][0]);
    try testing.expectEqual(@as(f64, 542), trace.resident);
    try testing.expectEqual(@as(f64, 38), trace.textures);
    try testing.expectEqual([2]u64{ 1267, 0x89abcdef }, trace.drawn[0]);
    try testing.expectEqual([2]u64{ 2, 2 }, trace.drawn[7]);
}
