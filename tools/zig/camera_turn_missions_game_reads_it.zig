//! The turn preload over the missions (M006 round 2, fix/preload-crash,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md, "Tutorial 6"):
//! the sixth tutorial crashed the Game with Remastered on a few seconds into
//! the mission. Its script destroys a bridge at two seconds of game time,
//! which deletes two birches, while the preload still listed every map object
//! of the first frame: it asked a deleted birch, emptied in place and without
//! a desc, for its pictures.
//!
//! The real Game plays each tutorial mission (given on the command line as
//! its mission XML, so its script runs as from the menu) and a sample of
//! campaign missions, unpaused, with `-mod=remastered_views` (an empty
//! scratch mod of that name, the mods folder laid out as `zig build
//! camera-turn-mod` does; under lang-ru with a copy of the installed one
//! beside it). Each run sets Remastered on at 1.6 s of game time, so that the
//! preload is under way when the sixth tutorial's birches go at 2.08 s: with
//! Remastered on from the first frame a release Game on an M1 Pro was done
//! with it before then; set on at 1.5 s it had half the birches' entries left
//! there, and set on at 1.9 s a debug Game on win-home listed the objects only
//! after the birches had gone. Then it waits for the preload, presses the turn
//! keys four times, sets Remastered off again (the option is kept in the
//! profile) and exits. BK_TURN_MISSIONS names the set, a build step each, under ten
//! minutes of missions on a debug Game: `tutorials` (Default), `tutorials-ru`
//! (lang-ru) and `campaign` (Default).
//!
//! Held, per run: the Game exits cleanly, BK_TURN_TRACE reports the preload
//! done and four turns drawn, and nothing leaks at exit; on the sixth
//! tutorial the preload must have skipped objects gone from the world, or
//! the run did not cover the crash.
//!
//! The Game runs from zig-out/local-test/camera-turn/missions-<set> with every
//! user path redirected; the logs stay there, with the preload's time per
//! mission in scores.txt. BK_VIEWS_GAME names the installed Game;
//! `zig build camera-turn-<set>-game-reads-it` sets both.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const mod_name = "remastered_views";
const language = "lang-ru";
const tutorials = [_][]const u8{
    "scenarios\\tutorials\\tutorial1\\1.xml", "scenarios\\tutorials\\tutorial2\\1.xml",
    "scenarios\\tutorials\\tutorial3\\1.xml", "scenarios\\tutorials\\tutorial4\\1.xml",
    "scenarios\\tutorials\\tutorial5\\1.xml", "scenarios\\tutorials\\tutorial6\\1.xml",
};
/// The first mission of a campaign of each side.
const campaign = [_][]const u8{
    "scenarios\\scenariomissions\\german\\poland\\1.xml",
    "scenarios\\scenariomissions\\allies\\normandy\\1.xml",
    "scenarios\\scenariomissions\\ussr\\moscow\\1.xml",
};
/// The missions of a set, the language they run in and their schedule: the
/// game time the preload gets before the keys, then the keys. A tutorial's
/// preload took up to 1.6 s of its slices over 200 frames of a debug Game on
/// win-home, done well within 13 s; the campaign's maps are larger (Tula's
/// 2631 objects ran 9 frames a second there), so they get 40 s.
const Set = struct { name: []const u8, missions: []const []const u8, language: []const u8, schedule: []const u8 };
const tutorial_schedule = "g1600:set=GamePlay.Remastered=ON,g400:hover=1000x400,g11000:key=.,g500:key=.,g500:key=COMMA,g500:key=COMMA,g500:set=GamePlay.Remastered=OFF,g500:exit";
const campaign_schedule = "g1600:set=GamePlay.Remastered=ON,g400:hover=1000x400,g38000:key=.,g500:key=.,g500:key=COMMA,g500:key=COMMA,g500:set=GamePlay.Remastered=OFF,g500:exit";
const sets = [_]Set{
    .{ .name = "tutorials", .missions = &tutorials, .language = "Default", .schedule = tutorial_schedule },
    .{ .name = "tutorials-ru", .missions = &tutorials, .language = language, .schedule = tutorial_schedule },
    .{ .name = "campaign", .missions = &campaign, .language = "Default", .schedule = campaign_schedule },
};
const profile = "Turn";

test "the missions play with the turn preload and the turn keys" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-tutorials-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const set_name = (try game_views.envValue(allocator, "BK_TURN_MISSIONS")) orelse "tutorials";
    const set = for (sets) |candidate| {
        if (std.mem.eql(u8, candidate.name, set_name)) break candidate;
    } else return error.UnknownSet;
    const cwd = std.Io.Dir.cwd();
    const scratch = try std.fmt.allocPrint(allocator, "zig-out/local-test/camera-turn/missions-{s}", .{set.name});
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    try cwd.createDirPath(io, user);
    const mod = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    try cwd.createDirPath(io, mod);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ mod, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn missions test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    if (!std.mem.eql(u8, set.language, "Default")) {
        const installed = try std.fs.path.join(allocator, &.{ std.fs.path.dirname(game).?, "mods", set.language, "data" });
        try game_views.copyFolder(io, allocator, installed, try std.fs.path.join(allocator, &.{ root, "mods", set.language, "data" }));
    }

    const options = try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{set.language});
    _ = try runGame(io, allocator, game, root, user, "options", options, &.{});

    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "mission, language: preload done in (ms of frames), objects gone from the world; turns drawn\n");
    var failed = false;
    for (set.missions, 0..) |mission, k| {
        const name = try std.fmt.allocPrint(allocator, "mission-{d}", .{k});
        const log = try runGame(io, allocator, game, root, user, name, set.schedule, &.{ "-mod=" ++ mod_name, mission });
        const preload = preloadDone(log);
        const turns = turnCount(log);
        const gone: f64 = if (preload) |p| p[1] else -1;
        try report.print(allocator, "{s}, {s}: {d:.0}, {d:.0}; {d}\n", .{ mission, set.language, if (preload) |p| p[0] else -1, gone, turns });
        if (preload == null or turns != 4) failed = true;
        if (std.mem.indexOf(u8, mission, "tutorial6") != null and gone <= 0) failed = true;
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a mission did not finish its preload or its four turns; the logs are in {s}\n", .{ report.items, root });
        return error.TurnsNotDrawn;
    }
}

/// One run of the Game on the profile, its log kept as <name>.log.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, user: []const u8, name: []const u8, auto_ui: []const u8, extra: []const []const u8) ![]const u8 {
    const cwd = std.Io.Dir.cwd();
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
    try environ.put("BK_AUTO_UI", auto_ui);
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-windowed", "-mode=1024x768", "-profile=" ++ profile });
    try argv.appendSlice(allocator, extra);
    const result = try timed_run.run(allocator, io, .{
        .argv = argv.items,
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    return log;
}

/// The milliseconds of frames and the objects gone from the world that
/// BK_TURN_TRACE's "preload done" line reports.
fn preloadDone(log: []const u8) ?[2]f64 {
    const marker = "BK_TURN_TRACE: preload done in ";
    const at = std.mem.indexOf(u8, log, marker) orelse return null;
    const end = std.mem.indexOfScalarPos(u8, log, at, '\n') orelse log.len;
    var it = std.mem.tokenizeAny(u8, log[at + marker.len .. end], " ,\r");
    const ms = std.fmt.parseFloat(f64, it.next() orelse return null) catch return null;
    var previous: []const u8 = "";
    while (it.next()) |token| {
        if (std.mem.eql(u8, token, "gone")) return .{ ms, std.fmt.parseFloat(f64, previous) catch return null };
        previous = token;
    }
    return null;
}

/// The turns' first frames BK_TURN_TRACE printed ("step N frame 0").
fn turnCount(log: []const u8) usize {
    var count: usize = 0;
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |line| {
        const at = std.mem.indexOf(u8, line, "BK_TURN_TRACE: step ") orelse continue;
        var it = std.mem.tokenizeScalar(u8, line[at + "BK_TURN_TRACE: step ".len ..], ' ');
        _ = it.next() orelse continue;
        if (!std.mem.eql(u8, it.next() orelse continue, "frame")) continue;
        if (std.mem.eql(u8, it.next() orelse continue, "0")) count += 1;
    }
    return count;
}

test "the trace readers: the preload's time and the turns" {
    const log = "BK_TURN_TRACE: mission load 2240 ms\r\n" ++
        "BK_TURN_TRACE: preload done in 1218 ms over 84 frames, 6 gone from the world\n" ++
        "BK_TURN_TRACE: step 3 frame 0 total 5 resident 1 textures 1\n" ++
        "BK_TURN_TRACE: step 3 frame 1 total 5 resident 1 textures 1\n" ++
        "BK_TURN_TRACE: step 2 frame 0 total 5 resident 1 textures 1\n";
    try testing.expectEqual(@as(?[2]f64, .{ 1218, 6 }), preloadDone(log));
    try testing.expectEqual(@as(usize, 2), turnCount(log));
    try testing.expectEqual(@as(?[2]f64, null), preloadDone("no trace\n"));
}
