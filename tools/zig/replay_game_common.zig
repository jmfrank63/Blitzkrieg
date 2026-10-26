//! What the replay tiers on the real Game share (replay_game_reads_it.zig,
//! replay_rng_audit.zig, docs/superpowers/specs/2026-10-26-mission-replay-design.md):
//! the test mission and its script, the scratch profile a Game runs in, and the
//! trace lines read back from its output.
//!
//! The mission is written at test time from the shipped `Data/Maps/river3d.xml`
//! into a scratch mod: two houses, two German rifle squads inside the first
//! (player 0, script group 200) and two Soviet ones in the open 140 map units
//! north of it (player 1, the first of them script group 100), so the two sides
//! fight at once and the random hits decide who falls. Its script answers Call HQ
//! (USER_ACTION_CALL_HQ, `msg=110`): it hands the Soviet squad of group 100 to
//! player 0, draws a number from the AI's generator and asks the client for the
//! selection. The answer names the selected soldiers; a soldier is in no script
//! group (its squad is), so the script's feedback is not called for them, but
//! the answer is a recorded command all the same. A playback that missed Call
//! HQ would leave the recording's state hashes and draw other numbers.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

pub const mod_name = "replay_test";
pub const map_name = "replay_fight.xml";

/// MAIN_COMMAND_SAVE and MAIN_COMMAND_LOAD (Main/iMainClassIDs.h: 0x10010000 + 20, + 21).
pub const save_command = "268501012";
pub const load_command = "268501013";
/// CMD_GAME_PAUSE (Main/iMainCommands.h): a loaded game starts paused, this resumes it.
pub const pause_toggle = "0x00100014";
/// USER_ACTION_CALL_HQ (Common/Actions.h): an instant action, always available.
pub const call_hq = "110";

/// The map's objects, in the map XML's own form (fmtMap.cpp SMapObjectInfo):
/// positions in the visual world, a squad's FrameIndex is its formation, and a
/// link with Intention 0 puts the squad inside the object it links with. The
/// houses and German squads are remastered-game-reads-it's, so its clicks hold.
const objects =
    obj("E_House01_1", 400, 500, -1, -1, 1, 0, -1) ++
    obj("E_House01_1", 550, 700, -1, -1, 2, 0, -1) ++
    obj("German_rifle_41", 400, 650, 0, 200, 3, 1, 0) ++
    obj("German_rifle_41", 420, 650, 0, 200, 4, 1, 0) ++
    obj("USSR_rifle_41", 380, 360, 1, 100, 5, 0, 0) ++
    obj("USSR_rifle_41", 440, 360, 1, -1, 6, 0, 0);

fn obj(comptime name: []const u8, comptime x: u32, comptime y: u32, comptime player: i32, comptime script_id: i32, comptime link: u32, comptime link_with: u32, comptime frame: i32) []const u8 {
    return std.fmt.comptimePrint(
        "<item Direction=\"0\" Player=\"{d}\" ScriptID=\"{d}\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"{d}\"/></item>",
        .{ player, script_id, frame, name, x, y, link, link_with },
    );
}

/// The mission's script, beside the map (Formats/fmtMapScriptPath.h). Each Trace
/// reaches the log through BK_MAP_TRACE, so a playback's lines can be compared
/// with the recording's: the same counts prove it ran on the same world.
const script =
    "function CallHQ()\r\n" ++
    "\tTrace(\"CallHQ: party 1 has %g units\", GetNUnitsInParty(1));\r\n" ++
    "\tTrace(\"CallHQ: drew %g\", RandomInt(1000));\r\n" ++
    "\tChangePlayer(100, 0);\r\n" ++
    "\tTrace(\"CallHQ: party 1 has %g units after\", GetNUnitsInParty(1));\r\n" ++
    "\tAskClient(\"GetSelectedUnits()\");\r\n" ++
    "end;\r\n" ++
    "\r\n" ++
    "function GetSelectedUnitsFeedBack(n)\r\n" ++
    "\tTrace(\"selected script group %g\", n);\r\n" ++
    "\tDamageObject(n, 20);\r\n" ++
    "end;\r\n";

pub const count_units = "units=0x0x1000000";
/// The recording: house A clicked, its strip's first squad picked, sent into house B;
/// HQ called while that squad is selected; then the fight goes on.
pub const record_schedule = "g1000:" ++ count_units ++ ",g10:camera=400x500,g10:click=330x330,g500:world=TraceSelection(0)," ++
    "g10:click=21x21,g500:world=TraceSelection(0),g10:rclick=505x310,g3000:msg=" ++ call_hq ++ ",g17000:" ++ count_units ++ ",g10:exit";

pub fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupe(u8, value);
}

pub const Hash = struct { segment: u32, value: u32 };

pub const Run = struct {
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
    /// Each script call a command ran (`BK_REPLAY_TRACE: script call ...`), its text.
    script_calls: []const []const u8,
    /// The mission script's Trace lines, in order.
    lua: []const []const u8,
    /// BK_RNG_AUDIT: whether the trap was armed, each caller it reported, and its summary.
    rng_armed: bool,
    rng_callers: []const []const u8,
    rng_summary: ?[]const u8,
};

pub const Options = struct {
    /// Runs the Game with BK_RNG_AUDIT=1 (a debug build reports every draw outside the simulation).
    rng_audit: bool = false,
};

pub fn first(lines: []const []const u8) []const u8 {
    return if (lines.len == 0) "(none)" else lines[0];
}
pub fn last(lines: []const []const u8) []const u8 {
    return if (lines.len == 0) "(none)" else lines[lines.len - 1];
}

/// How many of `expected`'s hashes `actual` holds equal at the same segment,
/// and the first segment where a hash both hold differs (0 for none).
pub fn compareHashes(expected: []const Hash, actual: []const Hash) struct { equal: usize, first_difference: u32 } {
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

pub fn printLines(label: []const u8, lines: []const []const u8) void {
    if (lines.len == 0) std.debug.print("{s}: (none)\n", .{label});
    for (lines) |line| std.debug.print("{s}: {s}\n", .{ label, line });
}

pub fn sameLines(a: []const []const u8, b: []const []const u8) bool {
    if (a.len != b.len) return false;
    for (a, b) |x, y| if (!std.mem.eql(u8, x, y)) return false;
    return true;
}

/// The mod: mod.xml, the mission map (river3d.xml with the objects and the
/// script's name in) and the script beside it.
pub fn writeMod(io: std.Io, allocator: std.mem.Allocator, root: []const u8) !void {
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
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Scripts></Scripts>"));
    const with_objects = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", "<Objects>" ++ objects ++ "</Objects>");
    const with_script = try std.mem.replaceOwned(u8, allocator, with_objects, "<Scripts></Scripts>", "<Scripts>replay_fight</Scripts>");
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ maps, map_name }), .data = with_script });
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ maps, "replay_fight.lua" }), .data = script });
}

/// Runs the Game in the scratch profile `user_name` under `schedule`: on the
/// map with `history` (`-sh` or `-lh`) or none, or to the main menu for a load
/// (a null history and a load schedule). It must exit 0 without a leak. Its
/// output is kept as `<name>.log`.
pub fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, user_name: []const u8, name: []const u8, schedule: []const u8, history: ?[]const u8, options: Options) !Run {
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
    try environ.put("BK_MAP_TRACE", "1");
    if (options.rng_audit) try environ.put("BK_RNG_AUDIT", "1");
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
    var script_calls: std.ArrayList([]const u8) = .empty;
    var lua: std.ArrayList([]const u8) = .empty;
    var rng_callers: std.ArrayList([]const u8) = .empty;
    var rng_armed = false;
    var rng_summary: ?[]const u8 = null;
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
        } else if (std.mem.indexOf(u8, line, "BK_REPLAY_TRACE: script call ")) |at| {
            try script_calls.append(allocator, line[at + "BK_REPLAY_TRACE: script call ".len ..]);
        } else if (std.mem.indexOf(u8, line, "BK_REPLAY_TRACE: out of sync")) |_| {
            out_of_sync += 1;
        } else if (std.mem.indexOf(u8, line, "BK_AUTO_UI: units near 0,0 r 1000000: total ")) |at| {
            try units.append(allocator, line[at + "BK_AUTO_UI: units near 0,0 r 1000000: total ".len ..]);
        } else if (std.mem.indexOf(u8, line, "BK_MAP_TRACE: lua ")) |at| {
            try lua.append(allocator, line[at + "BK_MAP_TRACE: lua ".len ..]);
        } else if (std.mem.indexOf(u8, line, "BK_RNG_AUDIT: armed")) |_| {
            rng_armed = true;
        } else if (std.mem.indexOf(u8, line, "BK_RNG_AUDIT: draw outside the simulation ")) |at| {
            try rng_callers.append(allocator, line[at + "BK_RNG_AUDIT: draw outside the simulation ".len ..]);
        } else if (std.mem.indexOf(u8, line, "BK_RNG_AUDIT: disarmed: ")) |at| {
            rng_summary = line[at + "BK_RNG_AUDIT: disarmed: ".len ..];
        }
    }
    return .{
        .log = log,
        .log_path = log_path,
        .hashes = hashes.items,
        .recorded = recorded.items,
        .played = played.items,
        .out_of_sync = out_of_sync,
        .unknown_units = unknown_units,
        .group_units = group_units,
        .units = units.items,
        .script_calls = script_calls.items,
        .lua = lua.items,
        .rng_armed = rng_armed,
        .rng_callers = rng_callers.items,
        .rng_summary = rng_summary,
    };
}
