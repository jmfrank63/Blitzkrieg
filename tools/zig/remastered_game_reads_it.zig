//! The Remastered gameplay option on the real Game
//! (docs/superpowers/specs/2026-10-25-remastered-option-design.md). A mission
//! map is written at test time from the shipped `Data/Maps/river3d.xml`, a
//! small empty map, into a scratch mod: two houses (E_House01_1, 40 places),
//! two German rifle squads inside house A, a third aboard an Opel Blitz truck.
//! The Game runs it under a BK_AUTO_UI schedule with the option set OFF (the
//! original game) and ON, clicks house A, its who-is-inside strip, orders the
//! occupants onto house B with a right click, then does the same from the
//! truck. `world=TraceSelection(0)` and `world=TraceContainers(0)` print what
//! is selected and who sits where, and those lines are the measurement:
//!
//! - OFF: a strip click selects the clicked squad (10) and the strip closes;
//!   that squad changes into house B (the original already does that); the
//!   truck's load cannot be selected and a strip click on it changes nothing.
//! - ON: a strip click selects every occupant (20) and the strip stays;
//!   shift-clicks toggle a squad and the last one hands the selection back to
//!   the house; all twenty change into house B; the truck's load can be
//!   selected from its strip and ordered straight into house B.
//!
//! Two more pairs of runs save the mission in one mode and load it in the
//! other: the truck's load follows the mode that loads it.
//!
//! The clicks are screen positions at 1024x768 with the camera at the map's
//! (400, 500); the strip's first cell is at (21, 21), its second row at
//! (21, 55). The Game runs from a scratch folder with HOME, XDG_*,
//! BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT there, so the user's data
//! is never touched; the frames and logs stay in zig-out/local-test/remastered.
//! BK_REMASTERED_GAME names the installed Game; `zig build
//! remastered-game-reads-it` sets it.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/remastered";
const mod_name = "remastered_test";
const map_name = "remastered_strip.xml";

/// MAIN_COMMAND_SAVE and MAIN_COMMAND_LOAD (Main/iMainClassIDs.h: 0x10010000 + 20, + 21).
const save_command = "268501012";
const load_command = "268501013";
/// MC_ADD_ACTION_ON / OFF (GameTT/iMission.h): the shift modifier.
const shift_on = "0x00200023";
const shift_off = "0x00200024";

/// Where the houses are, as BK_CONTAINER_TRACE prints them (the AI's coordinates).
const house_a = "\"E_House01_1\" at 283,354";
const house_b = "\"E_House01_1\" at 389,495";
const truck = "\"Opel_Blitz_Cargo\"";

/// The map's objects, in the map XML's own form (fmtMap.cpp SMapObjectInfo):
/// positions in the visual world, a squad's FrameIndex is its formation, and
/// a link with Intention 0 puts the squad inside the object it links with.
const objects =
    obj("E_House01_1", 400, 500, -1, 1, 0, -1) ++
    obj("E_House01_1", 550, 700, -1, 2, 0, -1) ++
    obj("German_rifle_41", 400, 650, 0, 3, 1, 0) ++
    obj("German_rifle_41", 420, 650, 0, 4, 1, 0) ++
    obj("Opel_Blitz_Cargo", 650, 330, 0, 5, 0, -1) ++
    obj("German_rifle_41", 650, 250, 0, 6, 5, 0);

fn obj(comptime name: []const u8, comptime x: u32, comptime y: u32, comptime player: i32, comptime link: u32, comptime link_with: u32, comptime frame: i32) []const u8 {
    return std.fmt.comptimePrint(
        "<item Direction=\"0\" Player=\"{d}\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"{d}\"/></item>",
        .{ player, frame, name, x, y, link, link_with },
    );
}

/// The schedule both modes share up to the house: the option, the camera, house A, the strip.
fn openingSchedule(comptime mode: []const u8) []const u8 {
    return "g500:set=GamePlay.Remastered=" ++ mode ++ ",g1000:camera=400x500,g10:world=TraceContainers(0)," ++
        "g10:click=330x330,g500:world=TraceSelection(0),g10:click=21x21,g500:world=TraceSelection(0),";
}
/// The order onto house B, then the truck: clicked, its strip, ordered onto house B.
const closing_schedule =
    "g10:rclick=505x310,g20000:world=TraceContainers(0)," ++
    "g10:click=360x480,g500:world=TraceSelection(0),g10:click=21x21,g500:world=TraceSelection(0)," ++
    "g10:rclick=505x310,g25000:world=TraceContainers(0),g10:shot,g500:exit";
const off_schedule = openingSchedule("OFF") ++ closing_schedule;
/// ON also toggles both squads out with shift-clicks (the selection goes back
/// to the house) and takes all occupants again with a plain click.
const on_schedule = openingSchedule("ON") ++
    "g10:msg=" ++ shift_on ++ ",g10:click=21x21,g300:msg=" ++ shift_off ++ ",g200:world=TraceSelection(0)," ++
    "g10:msg=" ++ shift_on ++ ",g10:click=21x55,g300:msg=" ++ shift_off ++ ",g200:world=TraceSelection(0)," ++
    "g10:click=21x21,g500:world=TraceSelection(0)," ++ closing_schedule;

fn saveSchedule(comptime mode: []const u8) []const u8 {
    return "g500:set=GamePlay.Remastered=" ++ mode ++ ",g1500:world=TraceContainers(0),g10:cmdc=" ++ save_command ++
        "=remastered_" ++ mode ++ ".sav;0,g1000:exit";
}
/// The load starts at the main menu and the loaded game is paused, so it counts the timer's absolute time.
fn loadSchedule(comptime mode: []const u8, comptime saved: []const u8) []const u8 {
    return "g500:set=GamePlay.Remastered=" ++ mode ++ ",60:cmdc=" ++ load_command ++ "=remastered_" ++ saved ++ ".sav," ++
        "t2000:world=TraceContainers(0),t10:camera=400x500,t200:click=360x480,t500:click=21x21,t500:world=TraceSelection(0),t500:exit";
}

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupe(u8, value);
}

const Run = struct {
    log: []const u8,
    log_path: []const u8,
    selections: []const []const u8,
    /// Each TraceContainers dump, its lines joined.
    containers: []const []const u8,
};

test "the who-is-inside strip and a change of building, Remastered off and on, and saves across the modes" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_REMASTERED_GAME")) orelse {
        std.debug.print("BK_REMASTERED_GAME is not set: run zig build remastered-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, root);

    // OFF: the original game.
    {
        const run = try runGame(io, allocator, game, root, "off", "off", off_schedule, true);
        try expectSelection(run, 0, "units=0 buildings=1 inside=0 containers=0 strip=20 strip_cells=2");
        try expectSelection(run, 1, "units=10 buildings=0 inside=10 containers=1 strip=0 strip_cells=0");
        try expectContainers(run, 0, &.{ house_a ++ " passangers=20 selectable=20", truck ++ " at 460,233 passangers=10 selectable=0" }, &.{});
        try expectContainers(run, 1, &.{ house_a ++ " passangers=10 selectable=10", house_b ++ " passangers=10 selectable=10" }, &.{});
        try expectSelection(run, 2, "units=1 buildings=0 inside=0 containers=0 strip=10 strip_cells=1");
        try expectSelection(run, 3, "units=1 buildings=0 inside=0 containers=0 strip=10 strip_cells=1");
        try expectContainers(run, 2, &.{ house_b ++ " passangers=10 selectable=10", truck ++ " at 460,233 passangers=10 selectable=0" }, &.{});
    }
    // ON: the player's extended game.
    {
        const run = try runGame(io, allocator, game, root, "on", "on", on_schedule, true);
        try expectSelection(run, 0, "units=0 buildings=1 inside=0 containers=0 strip=20 strip_cells=2");
        try expectSelection(run, 1, "units=20 buildings=0 inside=20 containers=1 strip=20 strip_cells=2");
        try expectSelection(run, 2, "units=10 buildings=0 inside=10 containers=1 strip=20 strip_cells=2");
        try expectSelection(run, 3, "units=0 buildings=1 inside=0 containers=0 strip=20 strip_cells=2");
        try expectSelection(run, 4, "units=20 buildings=0 inside=20 containers=1 strip=20 strip_cells=2");
        try expectContainers(run, 0, &.{ house_a ++ " passangers=20 selectable=20", truck ++ " at 460,233 passangers=10 selectable=10" }, &.{});
        try expectContainers(run, 1, &.{house_b ++ " passangers=20 selectable=20"}, &.{house_a});
        try expectSelection(run, 5, "units=1 buildings=0 inside=0 containers=0 strip=10 strip_cells=1");
        try expectSelection(run, 6, "units=10 buildings=0 inside=10 containers=1 strip=10 strip_cells=1");
        try expectContainers(run, 2, &.{house_b ++ " passangers=30 selectable=30"}, &.{ house_a, truck });
    }
    // A save written ON loads OFF, and one written OFF loads ON: the truck's load follows the loading mode.
    {
        const saved_on = try runGame(io, allocator, game, root, "save-on", "save-on", saveSchedule("ON"), true);
        try expectContainers(saved_on, 0, &.{truck ++ " at 460,233 passangers=10 selectable=10"}, &.{});
        const loaded_off = try runGame(io, allocator, game, root, "save-on", "load-off", loadSchedule("OFF", "ON"), false);
        try expectContainers(loaded_off, 0, &.{truck ++ " at 460,233 passangers=10 selectable=0"}, &.{});
        try expectSelection(loaded_off, 0, "units=1 buildings=0 inside=0 containers=0 strip=10 strip_cells=1");

        const saved_off = try runGame(io, allocator, game, root, "save-off", "save-off", saveSchedule("OFF"), true);
        try expectContainers(saved_off, 0, &.{truck ++ " at 460,233 passangers=10 selectable=0"}, &.{});
        const loaded_on = try runGame(io, allocator, game, root, "save-off", "load-on", loadSchedule("ON", "OFF"), false);
        try expectContainers(loaded_on, 0, &.{truck ++ " at 460,233 passangers=10 selectable=10"}, &.{});
        try expectSelection(loaded_on, 0, "units=10 buildings=0 inside=10 containers=1 strip=10 strip_cells=1");
    }
}

/// The mod: mod.xml and the mission map, river3d.xml with the objects in.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, root: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const maps = try std.fs.path.join(allocator, &.{ data, "maps" });
    try cwd.createDirPath(io, maps);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Remastered test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    const with_objects = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", "<Objects>" ++ objects ++ "</Objects>");
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ maps, map_name }), .data = with_objects });
}

/// Runs the Game in the scratch profile `user` under `schedule`, on the map
/// or (for a load) to the main menu; it must exit 0 without a leak. Its
/// output is kept as `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, user_name: []const u8, name: []const u8, schedule: []const u8, with_map: bool) !Run {
    const cwd = std.Io.Dir.cwd();
    const user = try std.fs.path.join(allocator, &.{ root, "user", user_name });
    try cwd.createDirPath(io, user);
    // The Game finds defconf.cfg beside its working folder (iMainInternal.cpp
    // ResolveConfigFileName), and without it the options have no actions.
    const defconf = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ std.fs.path.dirname(game).?, "defconf.cfg" }), allocator, .limited(1 << 20));
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ user, "defconf.cfg" }), .data = defconf });

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
    try environ.put("BK_AUTO_UI", schedule);
    const mod_argument = "-mod=" ++ mod_name;
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-windowed", "-mode=1024x768", "-profile=Remastered", mod_argument });
    if (with_map) try argv.append(allocator, map_name);
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

    var selections: std.ArrayList([]const u8) = .empty;
    var containers: std.ArrayList([]const u8) = .empty;
    var dump: std.ArrayList(u8) = .empty;
    var lines = std.mem.tokenizeAny(u8, log, "\r\n");
    while (lines.next()) |line| {
        if (std.mem.indexOf(u8, line, "BK_SELECTION_TRACE: ")) |at| try selections.append(allocator, line[at + "BK_SELECTION_TRACE: ".len ..]);
        if (std.mem.indexOf(u8, line, "BK_CONTAINER_TRACE: ")) |at| {
            const entry = line[at + "BK_CONTAINER_TRACE: ".len ..];
            if (std.mem.eql(u8, entry, "end")) {
                try containers.append(allocator, try dump.toOwnedSlice(allocator));
            } else {
                try dump.appendSlice(allocator, entry);
                try dump.append(allocator, '\n');
            }
        }
    }
    return .{ .log = log, .log_path = log_path, .selections = selections.items, .containers = containers.items };
}

fn expectSelection(run: Run, index: usize, expected: []const u8) !void {
    if (index < run.selections.len and std.mem.eql(u8, run.selections[index], expected)) return;
    std.debug.print("selection {d}: expected \"{s}\", got \"{s}\"; the Game's output is in {s}\n", .{ index, expected, if (index < run.selections.len) run.selections[index] else "(none)", run.log_path });
    return error.SelectionDiffers;
}

/// Dump `index` holds a line ending in each of `present` and none naming any of `absent`.
fn expectContainers(run: Run, index: usize, present: []const []const u8, absent: []const []const u8) !void {
    const dump = if (index < run.containers.len) run.containers[index] else "";
    var ok = index < run.containers.len;
    for (present) |want| {
        var found = false;
        var lines = std.mem.tokenizeScalar(u8, dump, '\n');
        while (lines.next()) |line| {
            if (std.mem.indexOf(u8, line, want) != null) found = true;
        }
        if (!found) ok = false;
    }
    for (absent) |not_wanted| {
        if (std.mem.indexOf(u8, dump, not_wanted) != null) ok = false;
    }
    if (ok) return;
    std.debug.print("containers {d}: expected", .{index});
    for (present) |want| std.debug.print(" [{s}]", .{want});
    std.debug.print(" and none of", .{});
    for (absent) |not_wanted| std.debug.print(" [{s}]", .{not_wanted});
    std.debug.print(", got:\n{s}the Game's output is in {s}\n", .{ dump, run.log_path });
    return error.ContainersDiffer;
}
