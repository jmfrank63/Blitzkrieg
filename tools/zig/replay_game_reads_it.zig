//! Mission replays on the real Game, slice 1 of
//! docs/superpowers/specs/2026-10-26-mission-replay-design.md. A mission map is
//! written at test time from the shipped `Data/Maps/river3d.xml` into a scratch
//! mod: two houses, two German rifle squads inside the first (player 0) and two
//! Soviet ones in the open 140 map units north of it (player 1), so the two sides
//! fight at once and the random hits decide who falls.
//!
//! - Record: the Game plays the mission with `-sh<file>` under a BK_AUTO_UI
//!   schedule that also gives an order (the house clicked, a squad picked from
//!   its who-is-inside strip and sent into the second house), so the replay
//!   holds commands, not only the start.
//! - Play back: two more runs play the replay with `-lh<file>`. BK_REPLAY_TRACE
//!   prints the AI's state hash every second of game time
//!   (`CCommandsHistory::AddStateHash`) and every command recorded and played;
//!   both playbacks must have every hash the recording took, equal, and play
//!   the recorded commands at their segments.
//! - Save and load: a run saves the mission after 6 s of game time and goes on;
//!   two fresh processes load that save and go on for 9 s. Their hashes after
//!   the load must be equal: the save holds the random generator's state.
//!
//! The Game runs from a scratch folder with HOME, XDG_*, BK_USER_ROOT,
//! BK_PROFILES_ROOT and BK_MODS_ROOT there, so the user's data is never
//! touched; the logs and the replay stay in zig-out/local-test/replay.
//! BK_REPLAY_GAME names the installed Game; `zig build replay-game-reads-it`
//! sets it.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/replay";
const mod_name = "replay_test";
const map_name = "replay_fight.xml";

/// MAIN_COMMAND_SAVE and MAIN_COMMAND_LOAD (Main/iMainClassIDs.h: 0x10010000 + 20, + 21).
const save_command = "268501012";
const load_command = "268501013";
/// CMD_GAME_PAUSE (Main/iMainCommands.h): a loaded game starts paused, this resumes it.
const pause_toggle = "0x00100014";

/// The map's objects, in the map XML's own form (fmtMap.cpp SMapObjectInfo):
/// positions in the visual world, a squad's FrameIndex is its formation, and a
/// link with Intention 0 puts the squad inside the object it links with. The
/// houses and German squads are remastered-game-reads-it's, so its clicks hold.
const objects =
    obj("E_House01_1", 400, 500, -1, 1, 0, -1) ++
    obj("E_House01_1", 550, 700, -1, 2, 0, -1) ++
    obj("German_rifle_41", 400, 650, 0, 3, 1, 0) ++
    obj("German_rifle_41", 420, 650, 0, 4, 1, 0) ++
    obj("USSR_rifle_41", 380, 360, 1, 5, 0, 0) ++
    obj("USSR_rifle_41", 440, 360, 1, 6, 0, 0);

fn obj(comptime name: []const u8, comptime x: u32, comptime y: u32, comptime player: i32, comptime link: u32, comptime link_with: u32, comptime frame: i32) []const u8 {
    return std.fmt.comptimePrint(
        "<item Direction=\"0\" Player=\"{d}\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"{d}\"/></item>",
        .{ player, frame, name, x, y, link, link_with },
    );
}

const count_units = "units=0x0x1000000";
/// The recording: house A clicked, its strip's first squad picked, sent into house B; then the fight goes on.
const record_schedule = "g1000:" ++ count_units ++ ",g10:camera=400x500,g10:click=330x330,g500:world=TraceSelection(0)," ++
    "g10:click=21x21,g500:world=TraceSelection(0),g10:rclick=505x310,g20000:" ++ count_units ++ ",g10:exit";
/// A playback runs longer than the recording, so it holds every hash the recording took.
const playback_schedule = "g1000:" ++ count_units ++ ",g10:camera=400x500,g26000:" ++ count_units ++ ",g10:exit";
const save_schedule = "g6000:" ++ count_units ++ ",g10:cmdc=" ++ save_command ++ "=replay_mid.sav;0,g9000:" ++ count_units ++ ",g10:exit";
/// The load starts at the main menu; the loaded game is paused until the toggle.
const load_schedule = "60:cmdc=" ++ load_command ++ "=replay_mid.sav,t2000:msg=" ++ pause_toggle ++ ",g9000:" ++ count_units ++ ",g10:exit";

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupe(u8, value);
}

const Hash = struct { segment: u32, value: u32 };

const Run = struct {
    log: []const u8,
    log_path: []const u8,
    hashes: []const Hash,
    recorded: []const u32,
    played: []const u32,
    out_of_sync: usize,
    /// Each registered group's units that the playback found no unit for, summed.
    unknown_units: usize,
    /// The units of every registered group, summed.
    group_units: usize,
    /// Each `units=` line's text after "total ".
    units: []const []const u8,
};

test "a replay plays back with the recording's state hashes, and a save goes on alike in every process that loads it" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_REPLAY_GAME")) orelse {
        std.debug.print("BK_REPLAY_GAME is not set: run zig build replay-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, root);
    const replay = try std.fs.path.join(allocator, &.{ root, "fight.rpl" });

    var failures: usize = 0;

    // Record, then play back twice.
    const recording = try runGame(io, allocator, game, root, "record", "record", record_schedule, try std.fmt.allocPrint(allocator, "-sh{s}", .{replay}));
    std.debug.print("record: {d} state hashes, {d} commands at segments {any}, units {s} -> {s}\n", .{ recording.hashes.len, recording.recorded.len, recording.recorded, first(recording.units), last(recording.units) });
    if (recording.hashes.len < 15) {
        std.debug.print("the recording took {d} state hashes, expected at least 15; its output is in {s}\n", .{ recording.hashes.len, recording.log_path });
        return error.TooFewHashes;
    }
    if (recording.recorded.len == 0) {
        std.debug.print("the recording holds no command; its output is in {s}\n", .{recording.log_path});
        return error.NoCommand;
    }
    // Some soldiers fall: the fight drew random numbers.
    if (recording.units.len < 2 or std.mem.eql(u8, recording.units[0], recording.units[recording.units.len - 1])) {
        std.debug.print("no unit fell in the recording; its output is in {s}\n", .{recording.log_path});
        return error.NoFight;
    }
    const replay_text = try cwd.readFileAlloc(io, replay, allocator, .limited(16 << 20));
    if (std.mem.indexOf(u8, replay_text, "StateHashes") == null) {
        std.debug.print("{s} holds no StateHashes\n", .{replay});
        failures += 1;
    }
    for ([_][]const u8{ "playback-1", "playback-2" }) |name| {
        const playback = try runGame(io, allocator, game, root, name, name, playback_schedule, try std.fmt.allocPrint(allocator, "-lh{s}", .{replay}));
        const compared = compareHashes(recording.hashes, playback.hashes);
        std.debug.print("{s}: {d} of the recording's {d} state hashes equal, first difference at segment {d}, {d} commands played at {any}, out-of-sync reports {d}, group units {d} ({d} unknown), units {s} -> {s}\n", .{ name, compared.equal, recording.hashes.len, compared.first_difference, playback.played.len, playback.played, playback.out_of_sync, playback.group_units, playback.unknown_units, first(playback.units), last(playback.units) });
        if (compared.equal != recording.hashes.len) {
            std.debug.print("{s} left the recording; its output is in {s}\n", .{ name, playback.log_path });
            failures += 1;
        }
        if (!std.mem.eql(u32, recording.recorded, playback.played)) {
            std.debug.print("{s} played commands at {any}, the recording has them at {any}\n", .{ name, playback.played, recording.recorded });
            failures += 1;
        }
        // The recorded squad has ten soldiers; a 64-bit Game used to record every second ID as 0.
        if (playback.group_units < 2 or playback.unknown_units != 0) {
            std.debug.print("{s} registered groups of {d} units, {d} of them unknown; its output is in {s}\n", .{ name, playback.group_units, playback.unknown_units, playback.log_path });
            failures += 1;
        }
        if (playback.out_of_sync != 0) {
            std.debug.print("{s} reported itself out of sync; its output is in {s}\n", .{ name, playback.log_path });
            failures += 1;
        }
    }

    // Save mid-mission, load in two fresh processes.
    const saved = try runGame(io, allocator, game, root, "save", "save", save_schedule, null);
    const loads = [_]Run{
        try runGame(io, allocator, game, root, "save", "load-1", load_schedule, null),
        try runGame(io, allocator, game, root, "save", "load-2", load_schedule, null),
    };
    const loaded = compareHashes(loads[0].hashes, loads[1].hashes);
    std.debug.print("load-1: {d} state hashes after the load, load-2: {d}; {d} equal, first difference at segment {d}; units {s} and {s}\n", .{ loads[0].hashes.len, loads[1].hashes.len, loaded.equal, loaded.first_difference, last(loads[0].units), last(loads[1].units) });
    // For slice 3 (load transparency), reported, not asserted: does a loaded world go on like the run that saved it?
    const transparent = compareHashes(loads[0].hashes, saved.hashes);
    std.debug.print("load-1 against the saving run going on: {d} of {d} equal, first difference at segment {d}\n", .{ transparent.equal, loads[0].hashes.len, transparent.first_difference });
    if (loads[0].hashes.len < 5 or loads[1].hashes.len < 5 or loaded.equal < 5 or loaded.first_difference != 0) {
        std.debug.print("the two loads of one save went on differently; their output is in {s} and {s}\n", .{ loads[0].log_path, loads[1].log_path });
        failures += 1;
    }
    try testing.expectEqual(@as(usize, 0), failures);
}

fn first(lines: []const []const u8) []const u8 {
    return if (lines.len == 0) "(none)" else lines[0];
}
fn last(lines: []const []const u8) []const u8 {
    return if (lines.len == 0) "(none)" else lines[lines.len - 1];
}

/// How many of `expected`'s hashes `actual` holds equal at the same segment,
/// and the first segment where a hash both hold differs (0 for none).
fn compareHashes(expected: []const Hash, actual: []const Hash) struct { equal: usize, first_difference: u32 } {
    var equal: usize = 0;
    var first_difference: u32 = 0;
    for (expected) |want| {
        for (actual) |got| {
            if (got.segment != want.segment) continue;
            if (got.value == want.value) {
                equal += 1;
            } else if (first_difference == 0) {
                first_difference = want.segment;
            }
            break;
        }
    }
    return .{ .equal = equal, .first_difference = first_difference };
}

/// The mod: mod.xml and the mission map, river3d.xml with the objects in.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, root: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const maps = try std.fs.path.join(allocator, &.{ data, "maps" });
    try cwd.createDirPath(io, maps);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Replay test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    const with_objects = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", "<Objects>" ++ objects ++ "</Objects>");
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ maps, map_name }), .data = with_objects });
}

/// Runs the Game in the scratch profile `user_name` under `schedule`: on the
/// map with `history` (`-sh` or `-lh`) or none, or to the main menu for a load
/// (a null history and a load schedule). It must exit 0 without a leak. Its
/// output is kept as `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, user_name: []const u8, name: []const u8, schedule: []const u8, history: ?[]const u8) !Run {
    const cwd = std.Io.Dir.cwd();
    const user = try std.fs.path.join(allocator, &.{ root, "user", user_name });
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
    try environ.put("BK_REPLAY_TRACE", "1");
    try environ.put("BK_AUTO_UI", schedule);
    const mod_argument = "-mod=" ++ mod_name;
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-windowed", "-mode=1024x768", "-profile=Replay", mod_argument });
    const is_load = std.mem.indexOf(u8, schedule, load_command) != null and std.mem.startsWith(u8, schedule, "60:");
    if (history) |argument| try argv.append(allocator, argument);
    if (!is_load) try argv.append(allocator, map_name);
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

    var hashes: std.ArrayList(Hash) = .empty;
    var recorded: std.ArrayList(u32) = .empty;
    var played: std.ArrayList(u32) = .empty;
    var units: std.ArrayList([]const u8) = .empty;
    var out_of_sync: usize = 0;
    var unknown_units: usize = 0;
    var group_units: usize = 0;
    var lines = std.mem.tokenizeAny(u8, log, "\r\n");
    while (lines.next()) |line| {
        if (std.mem.indexOf(u8, line, "BK_REPLAY_TRACE: state hash at segment ")) |at| {
            var fields = std.mem.tokenizeScalar(u8, line[at + "BK_REPLAY_TRACE: state hash at segment ".len ..], ' ');
            const segment = try std.fmt.parseInt(u32, fields.next() orelse return error.BadTrace, 10);
            const value = try std.fmt.parseInt(u32, fields.next() orelse return error.BadTrace, 16);
            try hashes.append(allocator, .{ .segment = segment, .value = value });
        } else if (std.mem.indexOf(u8, line, "BK_REPLAY_TRACE: recorded command at segment ")) |at| {
            try recorded.append(allocator, try std.fmt.parseInt(u32, line[at + "BK_REPLAY_TRACE: recorded command at segment ".len ..], 10));
        } else if (std.mem.indexOf(u8, line, "BK_REPLAY_TRACE: played command at segment ")) |at| {
            try played.append(allocator, try std.fmt.parseInt(u32, line[at + "BK_REPLAY_TRACE: played command at segment ".len ..], 10));
        } else if (std.mem.indexOf(u8, line, "BK_REPLAY_TRACE: registered a group of ")) |at| {
            var fields = std.mem.tokenizeAny(u8, line[at + "BK_REPLAY_TRACE: registered a group of ".len ..], " ,");
            group_units += try std.fmt.parseInt(usize, fields.next() orelse return error.BadTrace, 10);
            _ = fields.next();
            unknown_units += try std.fmt.parseInt(usize, fields.next() orelse return error.BadTrace, 10);
        } else if (std.mem.indexOf(u8, line, "BK_REPLAY_TRACE: out of sync")) |_| {
            out_of_sync += 1;
        } else if (std.mem.indexOf(u8, line, "BK_AUTO_UI: units near 0,0 r 1000000: total ")) |at| {
            try units.append(allocator, line[at + "BK_AUTO_UI: units near 0,0 r 1000000: total ".len ..]);
        }
    }
    return .{ .log = log, .log_path = log_path, .hashes = hashes.items, .recorded = recorded.items, .played = played.items, .out_of_sync = out_of_sync, .unknown_units = unknown_units, .group_units = group_units, .units = units.items };
}
