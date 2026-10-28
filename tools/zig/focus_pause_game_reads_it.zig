//! The focus pause (round 2 of M006's hand tests,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md). The real Game is
//! driven by BK_AUTO_UI on a map of a fight: a Pz IV, a sniper and two rifle
//! squads of player 0 against a T-34 of player 1. A few seconds into the
//! fight the harness's blur= action pushes an SDL focus-lost event, holds the
//! window unfocused for eight seconds of wall time and pushes the focus back,
//! through the same frame code a real focus change goes through. With
//! Remastered on and off:
//!
//! - game time and the absolute clock must not move across the pause
//!   (BK_AUTO_UI's focus lines), while the wall clock moves its eight seconds;
//! - no shot may be fired while the window is unfocused (BK_SHOT_TRACE), and
//!   the fight must go on before and after it;
//! - the mission's frame rate measurement (BK_FPS_TRACE, the one that lowers
//!   the game speed below Options.GFX.MinFPS, set to 10 here) must read the
//!   period across the pause at least half the frame rate of the one before,
//!   and that period must not lower the game speed. Counted as one frame, the
//!   pause read 5 to 7 fps on win-home and lowered it; 10 stays under what a
//!   loaded machine draws (14 fps with other builds running), and a speed
//!   lowered by a slow period elsewhere is not the pause's.
//!
//! The Game runs from zig-out/local-test/focus-pause with every user path
//! redirected; logs and scores.txt stay there. BK_VIEWS_GAME names the
//! installed Game; `zig build focus-pause-game-reads-it` sets it.
const std = @import("std");
const control = @import("camera_turn_control_game_reads_it.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/focus-pause";
const mod_name = "focus_pause";
/// How long the window stays unfocused, wall milliseconds.
const pause_ms = 8000;
/// What a clock may move across the pause: the frame that pushes the focus
/// back is drawn before the pushed event is read.
const max_clock_step = 100;

/// One BK_AUTO_UI focus line: "lost|back at <wall> ms game <game> ms abs <abs> ms".
const Clocks = struct { wall: i64, game: i64, abs: i64 };

fn parseClocks(line: []const u8) ?Clocks {
    var parts = std.mem.tokenizeScalar(u8, line, ' ');
    var values: [3]i64 = undefined;
    var n: usize = 0;
    while (parts.next()) |part| {
        if (n == 3) break;
        values[n] = std.fmt.parseInt(i64, part, 10) catch continue;
        n += 1;
    }
    if (n != 3) return null;
    return .{ .wall = values[0], .game = values[1], .abs = values[2] };
}

/// One BK_FPS_TRACE line: "<fps> fps at game <ms> ms, speed <n>".
const Period = struct { fps: f64, speed: i64 };

fn parsePeriod(line: []const u8) ?Period {
    const space = std.mem.indexOfScalar(u8, line, ' ') orelse return null;
    const at = std.mem.lastIndexOf(u8, line, "speed ") orelse return null;
    return .{
        .fps = std.fmt.parseFloat(f64, line[0..space]) catch return null,
        .speed = std.fmt.parseInt(i64, std.mem.trim(u8, line[at + 6 ..], " \r"), 10) catch return null,
    };
}

test "the focus pause stops the game, its clocks and its frame rate measurement" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build focus-pause-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" }));

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    for ([_][]const u8{ "ON", "OFF" }) |remastered| {
        const name = try std.fmt.allocPrint(allocator, "remastered-{s}", .{remastered});
        const schedule = try std.fmt.allocPrint(allocator, "g1000:hover=1000x400,g10:var=Options.GFX.MinFPS=10,g10:set=GamePlay.Remastered={s},g5000:blur={d},g12000:exit", .{ remastered, pause_ms });
        const run = try control.runScheduleWith(io, allocator, game, root, mod_name, name, "fight.xml", schedule, "", .{
            .env = &.{ .{ "BK_SHOT_TRACE", "1" }, .{ "BK_FPS_TRACE", "1" } },
        });

        // The log in order, cut at the focus lines.
        const lost_at = std.mem.indexOf(u8, run.log, "BK_AUTO_UI: focus lost at ") orelse return error.MissingTrace;
        const back_at = std.mem.indexOf(u8, run.log, "BK_AUTO_UI: focus back at ") orelse return error.MissingTrace;
        const lost = parseClocks(run.log[lost_at + 26 ..]) orelse return error.MissingTrace;
        const back = parseClocks(run.log[back_at + 26 ..]) orelse return error.MissingTrace;
        const before = run.log[0..lost_at];
        const during = run.log[lost_at..back_at];
        const after = run.log[back_at..];
        const shots = [3]usize{ std.mem.count(u8, before, "BK_SHOT_TRACE: "), std.mem.count(u8, during, "BK_SHOT_TRACE: "), std.mem.count(u8, after, "BK_SHOT_TRACE: ") };

        var periods: std.ArrayList(Period) = .empty;
        for (try control.lines(allocator, run.log, "BK_FPS_TRACE: ")) |line| {
            const period = parsePeriod(line) orelse return error.MissingTrace;
            try periods.append(allocator, period);
        }
        const periods_before = (try control.lines(allocator, before, "BK_FPS_TRACE: ")).len;
        const across: ?Period = if (periods_before > 0 and periods_before < periods.items.len) periods.items[periods_before] else null;
        const last_before: ?Period = if (periods_before > 0) periods.items[periods_before - 1] else null;

        try report.print(allocator, "Remastered {s}: unfocused {d} ms of wall time; game time {d} -> {d} ms, absolute time {d} -> {d} ms; shots {d} before, {d} unfocused, {d} after\n", .{
            remastered, back.wall - lost.wall, lost.game, back.game, lost.abs, back.abs, shots[0], shots[1], shots[2],
        });
        try report.print(allocator, "  frame rate periods:", .{});
        for (periods.items) |period| try report.print(allocator, " {d:.1} (speed {d})", .{ period.fps, period.speed });
        try report.print(allocator, "; {d} before the pause\n", .{periods_before});

        if (back.wall - lost.wall < pause_ms) failed = true;
        if (back.game - lost.game > max_clock_step or back.abs - lost.abs > max_clock_step) failed = true;
        if (shots[0] == 0 or shots[1] != 0 or shots[2] == 0) failed = true;
        if (across == null or last_before == null) {
            failed = true;
        } else if (across.?.fps < 0.5 * last_before.?.fps or across.?.speed != last_before.?.speed) failed = true;
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}the focus pause lets something run on; the logs are in {s}\n", .{ report.items, root });
        return error.PauseLeaks;
    }
}

/// mod.xml and fight.xml, written from road3d.xml.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Focus pause test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const road3d = try cwd.readFileAlloc(io, "Data/Maps/road3d.xml", allocator, .limited(4 << 20));
    var objects: std.ArrayList(u8) = .empty;
    var link: u32 = 1;
    try control.item(allocator, &objects, "German_sniper", .{ -100, -150 }, 0, 0, 0, &link);
    try control.item(allocator, &objects, "German_rifle_41", .{ -250, 0 }, 0, 0, 0, &link);
    try control.item(allocator, &objects, "German_rifle_41", .{ -200, 150 }, 0, 0, 0, &link);
    try control.item(allocator, &objects, "Pz_Kpfw_IV_Ausf_D", .{ -300, -300 }, 0, 0, -1, &link);
    try control.item(allocator, &objects, "T-34", .{ 250, 100 }, 1, 0, -1, &link);
    try control.writeMap(io, allocator, data, "fight.xml", road3d, objects.items);
}
