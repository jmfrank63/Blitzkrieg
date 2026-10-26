//! The replay-rng-audit tier (slice 2 of
//! docs/superpowers/specs/2026-10-26-mission-replay-design.md): a replay holds
//! commands, not random draws, so every draw from the AI's generator that the
//! simulation did not make itself (IAILogic::Segment(), a command's execution,
//! the AI's Init) shifts every draw after it on playback. A debug Game run with
//! BK_RNG_AUDIT=1 counts such draws between the mission start and its end and
//! names each caller once (StreamIOZig/legacy_bridge.cpp, RandomGen).
//!
//! The mission is replay_game_common.zig's, played with real orders (a squad
//! picked from a house's strip and sent into another house, Call HQ with the
//! squad selected, a save) and recorded; then the recording is played back.
//! Both runs must report the trap armed, draws inside the simulation and none
//! outside it. The logs stay in zig-out/local-test/replay-rng-audit.
//! BK_REPLAY_GAME names the installed debug Game; `zig build replay-rng-audit`
//! sets it.
const std = @import("std");
const common = @import("replay_game_common.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/replay-rng-audit";
const count_units = common.count_units;
/// The recording's orders, then a save while the fight goes on.
const audit_schedule = "g1000:" ++ count_units ++ ",g10:camera=400x500,g10:click=330x330,g500:world=TraceSelection(0)," ++
    "g10:click=21x21,g500:world=TraceSelection(0),g10:rclick=505x310,g3000:msg=" ++ common.call_hq ++
    ",g2000:cmdc=" ++ common.save_command ++ "=audit_mid.sav;0,g15000:" ++ count_units ++ ",g10:exit";
const playback_schedule = "g1000:" ++ count_units ++ ",g10:camera=400x500,g24000:" ++ count_units ++ ",g10:exit";

test "a mission with orders draws from the AI's random generator only inside the simulation, recorded and played back" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try common.envValue(allocator, "BK_REPLAY_GAME")) orelse {
        std.debug.print("BK_REPLAY_GAME is not set: run zig build replay-rng-audit\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try common.writeMod(io, allocator, root);
    const replay = try std.fs.path.join(allocator, &.{ root, "audit.rpl" });

    var failures: usize = 0;
    const runs = [_]struct { name: []const u8, schedule: []const u8, history: []const u8 }{
        .{ .name = "record", .schedule = audit_schedule, .history = try std.fmt.allocPrint(allocator, "-sh{s}", .{replay}) },
        .{ .name = "playback", .schedule = playback_schedule, .history = try std.fmt.allocPrint(allocator, "-lh{s}", .{replay}) },
    };
    for (runs) |r| {
        const run = try common.runGame(io, allocator, game, root, r.name, r.name, r.schedule, r.history, .{ .rng_audit = true });
        std.debug.print("{s}: armed {}, {s}; {d} commands, script calls:\n", .{ r.name, run.rng_armed, run.rng_summary orelse "(no summary)", if (std.mem.eql(u8, r.name, "record")) run.recorded.len else run.played.len });
        common.printLines(r.name, run.script_calls);
        common.printLines("draw outside the simulation", run.rng_callers);
        if (!run.rng_armed or run.rng_summary == null) {
            std.debug.print("{s}: the trap was not armed and disarmed (a release build, or the mission never ended); its output is in {s}\n", .{ r.name, run.log_path });
            failures += 1;
            continue;
        }
        const counts = parseSummary(run.rng_summary.?) orelse {
            std.debug.print("{s}: cannot read the trap's summary \"{s}\"\n", .{ r.name, run.rng_summary.? });
            failures += 1;
            continue;
        };
        if (counts.inside == 0) {
            std.debug.print("{s}: no draw inside the simulation, so the trap saw no fight; its output is in {s}\n", .{ r.name, run.log_path });
            failures += 1;
        }
        if (counts.outside != 0 or run.rng_callers.len != 0) {
            std.debug.print("{s}: {d} draws outside the simulation from {d} callers; its output is in {s}\n", .{ r.name, counts.outside, run.rng_callers.len, run.log_path });
            failures += 1;
        }
        if (std.mem.eql(u8, r.name, "record") and run.script_calls.len == 0) {
            std.debug.print("record: Call HQ ran no script call; its output is in {s}\n", .{run.log_path});
            failures += 1;
        }
    }
    try testing.expectEqual(@as(usize, 0), failures);
}

/// "<inside> draws inside the simulation, <outside> outside from <callers> callers".
fn parseSummary(summary: []const u8) ?struct { inside: u64, outside: u64 } {
    var fields = std.mem.tokenizeAny(u8, summary, " ,");
    const inside = std.fmt.parseInt(u64, fields.next() orelse return null, 10) catch return null;
    while (fields.next()) |field| {
        if (!std.mem.eql(u8, field, "simulation")) continue;
        const outside = std.fmt.parseInt(u64, fields.next() orelse return null, 10) catch return null;
        return .{ .inside = inside, .outside = outside };
    }
    return null;
}
