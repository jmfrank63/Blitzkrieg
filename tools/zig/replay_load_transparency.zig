//! The replay-load-transparency tier (slice 3 of
//! docs/superpowers/specs/2026-10-26-mission-replay-design.md): a world loaded
//! from a save must simulate exactly like the world that was saved, or a replay
//! that went through a quickload plays another battle than the player saw.
//!
//! For each mission a Game runs it, saves after a while and goes on; a fresh
//! process loads that save and runs as long again. BK_STATE_HASH_EVERY prints
//! the AI's state hash every few segments in both runs, and every hash the
//! load took must equal the saving run's at the same segment. With
//! BK_STATE_HASH_DETAIL=1 the Game also prints the hash's parts (the AI's time
//! and random state, each unit, formation, shell, active script and the
//! script's globals): at the first segment that differs the tier names the
//! first parts that do, so a failure says what went apart, not only when.
//!
//! The missions: the replay tiers' fight with its script and orders (a squad
//! sent into a house, Call HQ) before the save; the same fight without a
//! script; a fight of tanks and guns, twice; and the shipped German campaign's Poland
//! mission (read from Data, its many active scripts and its AI both sides),
//! each must also show that its world changed (units fell, or the hash moved).
//! The Game runs in scratch profiles under zig-out/local-test/replay-load-transparency.
//! BK_REPLAY_GAME names the installed Game; `zig build replay-load-transparency` sets it.
const std = @import("std");
const common = @import("replay_game_common.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/replay-load-transparency";
const count_units = common.count_units;
const save_command = common.save_command;

/// Dense enough to pin the first segment that differs, sparse enough for a debug Game.
const trace_env = [_][2][]const u8{ .{ "BK_STATE_HASH_EVERY", "2" }, .{ "BK_STATE_HASH_DETAIL", "1" } };

/// Tanks and guns on both sides, 500 map units apart, so shells fly and tanks burn.
const armour_objects =
    common.obj("Pz_Kpfw_IV_Ausf_D", 380, 300, 0, -1, 1, 0, -1) ++
    common.obj("Pz_Kpfw_IV_Ausf_D", 460, 300, 0, -1, 2, 0, -1) ++
    common.obj("10_5mmLeFh18", 420, 200, 0, -1, 3, 0, -1) ++
    common.obj("German_rifle_41", 500, 260, 0, -1, 4, 0, 0) ++
    common.obj("T-34", 380, 800, 1, -1, 5, 0, -1) ++
    common.obj("T-34", 460, 800, 1, -1, 6, 0, -1) ++
    common.obj("76-mm ZIS-3", 420, 900, 1, -1, 7, 0, -1) ++
    common.obj("USSR_rifle_41", 500, 840, 1, -1, 8, 0, 0);

const Mission = struct {
    name: []const u8,
    map: []const u8,
    /// Game time in ms before the save, and the orders given before it (each ends with a comma).
    orders: []const u8,
    before_ms: u32,
    after_ms: u32,
    /// The world must lose units (a fight) or at least change its hash (a mission's AI).
    expect_fight: bool,
};

const missions = [_]Mission{
    .{
        .name = "fight",
        .map = common.map_name,
        // The replay tiers' orders: house A clicked, its first squad sent into house B, then Call HQ.
        .orders = "g1000:" ++ count_units ++ ",g10:camera=400x500,g10:click=330x330,g500:world=TraceSelection(0)," ++
            "g10:click=21x21,g500:world=TraceSelection(0),g10:rclick=505x310,g3000:msg=" ++ common.call_hq ++ ",",
        .before_ms = 1000,
        .after_ms = 9000,
        .expect_fight = true,
    },
    .{ .name = "noscript", .map = "lt_noscript.xml", .orders = "g1000:" ++ count_units ++ ",", .before_ms = 5000, .after_ms = 9000, .expect_fight = true },
    .{ .name = "armour", .map = "lt_armour.xml", .orders = "g1000:" ++ count_units ++ ",", .before_ms = 7000, .after_ms = 12000, .expect_fight = true },
    // Again, in two more processes: what a save leaves out may show only in some of
    // them (a loaded unit's timeOfReveal held whatever its memory held, on Windows).
    .{ .name = "armour-2", .map = "lt_armour.xml", .orders = "g1000:" ++ count_units ++ ",", .before_ms = 7000, .after_ms = 12000, .expect_fight = true },
    .{ .name = "poland", .map = "scenarios/scenariomissions/german/poland/1.xml", .orders = "g1000:" ++ count_units ++ ",", .before_ms = 7000, .after_ms = 9000, .expect_fight = false },
};

test "a world loaded from a save goes on exactly like the run that saved it" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try common.envValue(allocator, "BK_REPLAY_GAME")) orelse {
        std.debug.print("BK_REPLAY_GAME is not set: run zig build replay-load-transparency\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try common.writeMod(io, allocator, root);
    try common.writeMap(io, allocator, root, "lt_noscript.xml", common.fight_objects, null);
    try common.writeMap(io, allocator, root, "lt_armour.xml", armour_objects, null);

    // BK_LOAD_TRANSPARENCY_ONLY=<name> runs one mission, for finding a cause.
    const only = try common.envValue(allocator, "BK_LOAD_TRANSPARENCY_ONLY");
    var failures: usize = 0;
    for (missions) |mission| {
        if (only) |name| if (!std.mem.eql(u8, name, mission.name)) continue;
        const save_name = try std.fmt.allocPrint(allocator, "lt_{s}.sav", .{mission.name});
        const save_schedule = try std.fmt.allocPrint(allocator, "{s}g{d}:cmdc={s}={s};0,g{d}:{s},g10:exit", .{ mission.orders, mission.before_ms, save_command, save_name, mission.after_ms, count_units });
        const load_schedule = try std.fmt.allocPrint(allocator, "60:cmdc={s}={s},t2000:msg={s},g{d}:{s},g10:exit", .{ common.load_command, save_name, common.pause_toggle, mission.after_ms, count_units });
        const saved = try common.runGame(io, allocator, game, root, mission.name, try std.fmt.allocPrint(allocator, "{s}-save", .{mission.name}), save_schedule, null, .{ .map = mission.map, .env = &trace_env });
        const loaded = try common.runGame(io, allocator, game, root, mission.name, try std.fmt.allocPrint(allocator, "{s}-load", .{mission.name}), load_schedule, null, .{ .map = mission.map, .env = &trace_env });

        const compared = try compareRuns(allocator, saved, loaded);
        const first_load = if (loaded.dense.len == 0) 0 else loaded.dense[0].segment;
        std.debug.print("{s}: the load traced {d} segments from segment {d}, {d} of them also in the saving run, {d} equal in hash and parts, first difference at segment {d}; units {s} -> {s}, after the load {s}\n", .{ mission.name, loaded.dense.len, first_load, compared.both, compared.equal, compared.first_difference, common.first(saved.units), common.last(saved.units), common.last(loaded.units) });
        // A run of after_ms at every second segment traces after_ms / 100 segments; most of them must be compared.
        const wanted = mission.after_ms / 50 / 2 * 3 / 4;
        if (compared.both < wanted) {
            std.debug.print("{s}: {d} segments traced by both runs, expected at least {d}; the outputs are in {s} and {s}\n", .{ mission.name, compared.both, wanted, saved.log_path, loaded.log_path });
            failures += 1;
            continue;
        }
        if (compared.equal != compared.both) {
            std.debug.print("{s}: the loaded world left the saving run; the outputs are in {s} and {s}\n", .{ mission.name, saved.log_path, loaded.log_path });
            try printFirstDifferences(allocator, saved.log, loaded.log, compared.first_difference);
            failures += 1;
        }
        // The worlds compared must have done something.
        if (mission.expect_fight) {
            if (saved.units.len < 2 or std.mem.eql(u8, saved.units[0], saved.units[saved.units.len - 1])) {
                std.debug.print("{s}: no unit fell in the saving run; its output is in {s}\n", .{ mission.name, saved.log_path });
                failures += 1;
            }
        } else if (distinctHashes(loaded.dense) < loaded.dense.len / 2) {
            std.debug.print("{s}: the world after the load hardly changed ({d} distinct hashes of {d}); its output is in {s}\n", .{ mission.name, distinctHashes(loaded.dense), loaded.dense.len, loaded.log_path });
            failures += 1;
        }
    }
    try testing.expectEqual(@as(usize, 0), failures);
}

/// The segments both runs traced, how many of them have the same hash and the
/// same detail lines, and the first that does not (0 for none).
fn compareRuns(allocator: std.mem.Allocator, saved: common.Run, loaded: common.Run) !struct { both: usize, equal: usize, first_difference: u32 } {
    var both: usize = 0;
    var equal: usize = 0;
    var first_difference: u32 = 0;
    for (loaded.dense) |got| {
        for (saved.dense) |want| {
            if (want.segment != got.segment) continue;
            both += 1;
            const same = want.value == got.value and
                common.sameLines(try detailLines(allocator, saved.log, got.segment), try detailLines(allocator, loaded.log, got.segment));
            if (same) {
                equal += 1;
            } else if (first_difference == 0) {
                first_difference = got.segment;
            }
            break;
        }
    }
    return .{ .both = both, .equal = equal, .first_difference = first_difference };
}

fn distinctHashes(hashes: []const common.Hash) usize {
    var distinct: usize = 0;
    for (hashes, 0..) |hash, i| {
        if (i == 0 or hash.value != hashes[i - 1].value) distinct += 1;
    }
    return distinct;
}

/// The detail lines (`BK_STATE_HASH_DETAIL: <segment> ...`) of one segment.
fn detailLines(allocator: std.mem.Allocator, log: []const u8, segment: u32) ![]const []const u8 {
    const prefix = try std.fmt.allocPrint(allocator, "BK_STATE_HASH_DETAIL: {d} ", .{segment});
    var lines: std.ArrayList([]const u8) = .empty;
    var it = std.mem.tokenizeAny(u8, log, "\r\n");
    while (it.next()) |line| {
        if (std.mem.indexOf(u8, line, prefix)) |at| try lines.append(allocator, line[at + prefix.len ..]);
    }
    return lines.items;
}

/// Names what differs first: the detail lines of the first segment whose hash
/// differs, the saving run's against the load's, up to a dozen.
fn printFirstDifferences(allocator: std.mem.Allocator, saved_log: []const u8, loaded_log: []const u8, segment: u32) !void {
    const saved = try detailLines(allocator, saved_log, segment);
    const loaded = try detailLines(allocator, loaded_log, segment);
    var shown: usize = 0;
    var i: usize = 0;
    while (i < @max(saved.len, loaded.len) and shown < 12) : (i += 1) {
        const a = if (i < saved.len) saved[i] else "(none)";
        const b = if (i < loaded.len) loaded[i] else "(none)";
        if (std.mem.eql(u8, a, b)) continue;
        std.debug.print("  segment {d} saving run: {s}\n  segment {d} load:       {s}\n", .{ segment, a, segment, b });
        shown += 1;
    }
    if (shown == 0) std.debug.print("  segment {d}: the detail lines are equal ({d} of them); what differs is outside them\n", .{ segment, saved.len });
}
