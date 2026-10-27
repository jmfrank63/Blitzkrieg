//! Mission replays on the real Game, slices 1 and 2 of
//! docs/superpowers/specs/2026-10-26-mission-replay-design.md. The mission and
//! the scratch profile are replay_game_common.zig's.
//!
//! - Record: the Game plays the mission with `-sh<file>` under a BK_AUTO_UI
//!   schedule that also gives orders (the house clicked, a squad picked from its
//!   who-is-inside strip and sent into the second house, then Call HQ), so the
//!   replay holds commands, not only the start. Call HQ runs the mission
//!   script's CallHQ, which hands a Soviet squad to the player, draws a number
//!   and asks for the selection, whose answer is a recorded command too.
//! - Play back: two more runs play the replay with `-lh<file>`. BK_REPLAY_TRACE
//!   prints the AI's state hash every second of game time
//!   (`CCommandsHistory::AddStateHash`) and every command recorded and played;
//!   both playbacks must have every hash the recording took, equal, play the
//!   recorded commands at their segments, run the same script calls and print
//!   the script's lines with the same unit counts.
//! - Save and load: a run saves the mission after 6 s of game time and goes on;
//!   two fresh processes load that save and go on for 9 s. Their hashes after
//!   the load must be equal (the save holds the random generator's state), and
//!   equal to the saving run's at the same segments (slice 3, load transparency).
//!
//! The Game runs from a scratch folder with HOME, XDG_*, BK_USER_ROOT,
//! BK_PROFILES_ROOT and BK_MODS_ROOT there, so the user's data is never
//! touched; the logs and the replay stay in zig-out/local-test/replay.
//! BK_REPLAY_GAME names the installed Game; `zig build replay-game-reads-it`
//! sets it.
const std = @import("std");
const common = @import("replay_game_common.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/replay";
const count_units = common.count_units;
const record_schedule = common.record_schedule;
/// A playback runs longer than the recording, so it holds every hash the recording took.
const playback_schedule = "g1000:" ++ count_units ++ ",g10:camera=400x500,g26000:" ++ count_units ++ ",g10:exit";
const save_schedule = "g6000:" ++ count_units ++ ",g10:cmdc=" ++ common.save_command ++ "=replay_mid.sav;0,g9000:" ++ count_units ++ ",g10:exit";
/// The load starts at the main menu; the loaded game is paused until the toggle.
const load_schedule = "60:cmdc=" ++ common.load_command ++ "=replay_mid.sav,t2000:msg=" ++ common.pause_toggle ++ ",g9000:" ++ count_units ++ ",g10:exit";

const Run = common.Run;
const runGame = common.runGame;
const compareHashes = common.compareHashes;
const first = common.first;
const last = common.last;

test "a replay plays back with the recording's state hashes, and a save goes on alike in every process that loads it" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try common.envValue(allocator, "BK_REPLAY_GAME")) orelse {
        std.debug.print("BK_REPLAY_GAME is not set: run zig build replay-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try common.writeMod(io, allocator, root);
    const replay = try std.fs.path.join(allocator, &.{ root, "fight.rpl" });

    var failures: usize = 0;

    // Record, then play back twice.
    const recording = try runGame(io, allocator, game, root, "record", "record", record_schedule, try std.fmt.allocPrint(allocator, "-sh{s}", .{replay}), .{});
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
    // Call HQ went through the transceiver and changed the world: the script's
    // squad left party 1, and the selection's answer (the soldiers of the squad
    // sent into house B still alive) was recorded.
    common.printLines("record: script call", recording.script_calls);
    common.printLines("record: script line", recording.lua);
    const hq = callHqEffect(recording.lua);
    if (!hasPrefix(recording.script_calls, "CallHQ()") or hq == null or hq.?.after >= hq.?.before) {
        std.debug.print("the recording ran no Call HQ that handed the squad over; its output is in {s}\n", .{recording.log_path});
        failures += 1;
    }
    if (!hasAnswer(recording.script_calls)) {
        std.debug.print("the recording holds no answer with the selected squad; its output is in {s}\n", .{recording.log_path});
        failures += 1;
    }
    const replay_text = try cwd.readFileAlloc(io, replay, allocator, .limited(16 << 20));
    if (std.mem.indexOf(u8, replay_text, "StateHashes") == null) {
        std.debug.print("{s} holds no StateHashes\n", .{replay});
        failures += 1;
    }
    for ([_][]const u8{ "playback-1", "playback-2" }) |name| {
        const playback = try runGame(io, allocator, game, root, name, name, playback_schedule, try std.fmt.allocPrint(allocator, "-lh{s}", .{replay}), .{});
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
        // The recorded script calls ran again, on the same world: the same lines with the same counts.
        if (!common.sameLines(recording.script_calls, playback.script_calls) or !common.sameLines(recording.lua, playback.lua)) {
            common.printLines("playback: script call", playback.script_calls);
            common.printLines("playback: script line", playback.lua);
            std.debug.print("{s} ran other script calls or lines than the recording; its output is in {s}\n", .{ name, playback.log_path });
            failures += 1;
        }
    }

    // Save mid-mission, load in two fresh processes.
    const saved = try runGame(io, allocator, game, root, "save", "save", save_schedule, null, .{});
    const loads = [_]Run{
        try runGame(io, allocator, game, root, "save", "load-1", load_schedule, null, .{}),
        try runGame(io, allocator, game, root, "save", "load-2", load_schedule, null, .{}),
    };
    const loaded = compareHashes(loads[0].hashes, loads[1].hashes);
    std.debug.print("load-1: {d} state hashes after the load, load-2: {d}; {d} equal, first difference at segment {d}; units {s} and {s}\n", .{ loads[0].hashes.len, loads[1].hashes.len, loaded.equal, loaded.first_difference, last(loads[0].units), last(loads[1].units) });
    // Slice 3: a loaded world goes on exactly like the run that saved it, at
    // every hash both took (replay_load_transparency.zig names what differs).
    const transparent = compareHashes(loads[0].hashes, saved.hashes);
    const both = common.commonSegments(loads[0].hashes, saved.hashes);
    std.debug.print("load-1 against the saving run going on: {d} of the {d} hashes both took equal, first difference at segment {d}\n", .{ transparent.equal, both, transparent.first_difference });
    if (loads[0].hashes.len < 5 or loads[1].hashes.len < 5 or loaded.equal < 5 or loaded.first_difference != 0) {
        std.debug.print("the two loads of one save went on differently; their output is in {s} and {s}\n", .{ loads[0].log_path, loads[1].log_path });
        failures += 1;
    }
    if (both < 5 or transparent.equal != both) {
        std.debug.print("the load went on differently from the run that saved it; their output is in {s} and {s}\n", .{ saved.log_path, loads[0].log_path });
        failures += 1;
    }
    try testing.expectEqual(@as(usize, 0), failures);
}

fn hasPrefix(lines: []const []const u8, prefix: []const u8) bool {
    for (lines) |line| if (std.mem.startsWith(u8, line, prefix)) return true;
    return false;
}

/// The selection's answer with soldiers in it, every one of them known.
fn hasAnswer(calls: []const []const u8) bool {
    for (calls) |call| {
        if (std.mem.startsWith(u8, call, "ReturnScriptIDs of ") and !std.mem.startsWith(u8, call, "ReturnScriptIDs of 0 ") and std.mem.endsWith(u8, call, ", 0 unknown")) return true;
    }
    return false;
}

/// Party 1's units before and after the script's CallHQ handed its squad over.
fn callHqEffect(lua: []const []const u8) ?struct { before: u32, after: u32 } {
    var before: ?u32 = null;
    var after: ?u32 = null;
    for (lua) |line| {
        const prefix = "CallHQ: party 1 has ";
        if (!std.mem.startsWith(u8, line, prefix)) continue;
        var fields = std.mem.tokenizeScalar(u8, line[prefix.len..], ' ');
        const count = std.fmt.parseInt(u32, fields.next() orelse continue, 10) catch continue;
        if (std.mem.endsWith(u8, line, " after")) after = count else before = count;
    }
    return if (before != null and after != null) .{ .before = before.?, .after = after.? } else null;
}

