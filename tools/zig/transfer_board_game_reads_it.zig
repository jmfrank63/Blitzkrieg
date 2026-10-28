//! Orders from inside to inside on the real Game
//! (docs/superpowers/specs/2026-10-25-remastered-option-design.md, "Inside to
//! inside in one order"). With the Remastered option on, infantry selected
//! inside a truck, a building or an entrenchment and right-clicked onto
//! another vehicle, building or entrenchment gets out and into it in one
//! order; a full target, or the container the squad sits in, shows the 2003
//! "no" cursor and refuses the order while the squad still sits.
//!
//! A mission map is written at test time from the shipped `Data/Maps/river3d.xml`
//! into a scratch mod: two houses (E_House01_1, 40 places), three Opel Blitz
//! trucks (20 seats each), a straight entrenchment of six pieces and German
//! rifle squads of ten: one in truck A, one in house A, one in the
//! entrenchment and two filling truck C. The Game runs short schedules under
//! BK_AUTO_UI (each under 90 game seconds: later on river3d the trucks and
//! houses get shelled), picks the passengers through the who-is-inside strip
//! or a frame, points at the target and right-clicks it. What is measured:
//!
//! - `world=TraceContainers(0)` after each order: who sits where (the
//!   client's passenger lists, which the AI's entrance and exit messages fill);
//! - BK_CURSOR_TRACE: the cursor shown when the right button went down (mode
//!   4 is board, 0 the 2003 "no" cursor; Scene/Cursor.cpp).
//!
//! Remastered on: truck to truck, truck to house, house to house, house to
//! truck, entrenchment to truck, each soldier ends in the target; the board
//! cursor over each; a full truck and the squad's own truck show the "no"
//! cursor and the order leaves everyone where they were. Then truck to truck
//! and truck to house again a quarter camera turn on (the turn key, step 1).
//! Remastered off: the original game, measured on main before the change
//! (2026-10-28): a house's squad ordered onto the full truck gets out and
//! stays outside, the entrenchment's squad boards a truck, both under the
//! board cursor.
//!
//! The clicks are screen positions at 1024x768 with the camera at the map's
//! (400, 500); the strip's first cell is at (21, 21). The Game runs from a
//! scratch folder with every user path redirected, so the user's data is never
//! touched; logs stay in zig-out/local-test/transfer-board. BK_TRANSFER_GAME
//! names the installed Game; `zig build transfer-board-game-reads-it` sets it.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/transfer-board";
const mod_name = "transfer_test";
const map_name = "transfer_board.xml";

/// The containers as BK_CONTAINER_TRACE prints them (the visual world's coordinates).
const truck_a = "\"Opel_Blitz_Cargo\" at 460,233 ";
const truck_b = "\"Opel_Blitz_Cargo\" at 530,368 ";
const truck_c = "\"Opel_Blitz_Cargo\" at 530,587 ";
const house_a = "\"E_House01_1\" at 283,354 ";
const house_b = "\"E_House01_1\" at 389,495 ";

/// Cursor modes (Common/Actions.h EUserAction): board, and the 2003 "no" cursor.
const board: u32 = 4;
const no_cursor: u32 = 0;

/// The map's objects in the map XML's own form (fmtMap.cpp SMapObjectInfo):
/// the AI's coordinates; a squad's FrameIndex is its formation, an
/// entrenchment piece's its packed type (1 line, 2 fireplace, 4 terminator);
/// a link with Intention 0 puts a squad inside the object it links with.
const objects =
    obj("E_House01_1", 400, 500, -1, 1, 0, -1, 0) ++
    obj("E_House01_1", 550, 700, -1, 2, 0, -1, 0) ++
    obj("German_rifle_41", 400, 650, 0, 3, 1, 0, 0) ++
    obj("Opel_Blitz_Cargo", 650, 330, 0, 5, 0, -1, 0) ++
    obj("German_rifle_41", 650, 250, 0, 6, 5, 0, 0) ++
    obj("Opel_Blitz_Cargo", 750, 520, 0, 7, 0, -1, 0) ++
    obj("Opel_Blitz_Cargo", 750, 830, 0, 8, 0, -1, 0) ++
    obj("German_rifle_41", 750, 900, 0, 9, 8, 0, 0) ++
    obj("German_rifle_41", 770, 900, 0, 10, 8, 0, 0) ++
    obj("Entrenchment", 400, 180, 0, 20, 0, 4, 32767) ++
    obj("Entrenchment", 696, 180, 0, 21, 0, 4, 0) ++
    obj("Entrenchment", 437, 180, 0, 22, 0, 2, 0) ++
    obj("Entrenchment", 511, 180, 0, 23, 0, 1, 0) ++
    obj("Entrenchment", 585, 180, 0, 24, 0, 2, 0) ++
    obj("Entrenchment", 659, 180, 0, 25, 0, 1, 0) ++
    obj("German_rifle_41", 500, 120, 0, 11, 22, 0, 0);
/// The six pieces as one section, terminators at its ends (MapGeometry's
/// PlanEntrenchment order); a vector of ints is a "data" list in the XML.
const entrenchments = "<Entrenchments><item><Sections><item><data>" ++
    "<item data=\"20\"/><item data=\"22\"/><item data=\"23\"/><item data=\"24\"/><item data=\"25\"/><item data=\"21\"/>" ++
    "</data></item></Sections></item></Entrenchments>";

fn obj(comptime name: []const u8, comptime x: u32, comptime y: u32, comptime player: i32, comptime link: u32, comptime link_with: u32, comptime frame: i32, comptime dir: u32) []const u8 {
    return std.fmt.comptimePrint(
        "<item Direction=\"{d}\" Player=\"{d}\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"{d}\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"{d}\"/></item>",
        .{ dir, player, frame, name, x, y, link, link_with },
    );
}

/// Screen points of the objects at one camera step.
const Screen = struct { truck_a: []const u8, truck_b: []const u8, truck_c: []const u8, house_a: []const u8, house_b: []const u8 };
const step0: Screen = .{ .truck_a = "370x480", .truck_b = "510x450", .truck_c = "664x378", .house_a = "330x330", .house_b = "520x320" };
/// The same a quarter turn on (step 1), found with pick= at that step.
const step1: Screen = .{ .truck_a = "760x450", .truck_b = "680x360", .truck_c = "545x285", .house_a = "528x432", .house_b = "480x300" };
/// A frame around the entrenchment's soldiers at step 0.
const trench_frame = "150x470x330x570";

fn start(comptime mode: []const u8) []const u8 {
    return "g500:set=GamePlay.Remastered=" ++ mode ++ ",g1000:camera=400x500,g10:world=TraceContainers(0),";
}
/// Selects every occupant of the container at `at`: the container, then its strip's first cell (Remastered selects all).
fn pickInside(comptime at: []const u8) []const u8 {
    return "g10:click=" ++ at ++ ",g500:click=21x21,g500:world=TraceSelection(0),";
}
/// Points at `at` and right-clicks it, then waits `wait` game ms and dumps the containers.
fn order(comptime at: []const u8, comptime wait: []const u8) []const u8 {
    return "g10:hover=" ++ at ++ ",g300:rclick=" ++ at ++ ",g" ++ wait ++ ":world=TraceContainers(0),";
}
const finish = "g10:shot,g500:exit";

/// Truck A to truck B, then truck B to house B.
fn trucksSchedule(comptime s: Screen, comptime turn: []const u8) []const u8 {
    return start("ON") ++ turn ++ pickInside(s.truck_a) ++ order(s.truck_b, "25000") ++ pickInside(s.truck_b) ++ order(s.house_b, "25000") ++ finish;
}
/// House A to house B, then house B to truck B.
const houses_schedule = start("ON") ++ pickInside(step0.house_a) ++ order(step0.house_b, "25000") ++
    pickInside(step0.house_b) ++ order(step0.truck_b, "25000") ++ finish;
/// Truck A's squad points at its own truck and at the full truck C and is
/// ordered onto C; then the entrenchment's squad boards truck A.
const full_schedule = start("ON") ++ pickInside(step0.truck_a) ++ order(step0.truck_a, "2000") ++ order(step0.truck_c, "15000") ++
    "g10:drag=" ++ trench_frame ++ ",g500:world=TraceSelection(0)," ++ order(step0.truck_a, "25000") ++ finish;
/// The original: house A's squad onto the full truck C, the entrenchment's onto truck B.
const off_schedule = start("OFF") ++ pickInside(step0.house_a) ++ order(step0.truck_c, "20000") ++
    "g10:drag=" ++ trench_frame ++ ",g500:world=TraceSelection(0)," ++ order(step0.truck_b, "25000") ++ finish;

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupe(u8, value);
}

const Run = struct {
    log_path: []const u8,
    /// Each TraceContainers dump, its lines joined.
    containers: []const []const u8,
    /// The cursor mode shown at each right click.
    cursors: []const u32,
};

test "orders from inside to inside: one order with Remastered, refused when full, the original unchanged" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_TRANSFER_GAME")) orelse {
        std.debug.print("BK_TRANSFER_GAME is not set: run zig build transfer-board-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    try writeMod(io, allocator, root);

    const at_start = [_][]const u8{ truck_a ++ "passangers=10", house_a ++ "passangers=10", truck_c ++ "passangers=20" };
    // Truck to truck, truck to house.
    {
        const run = try runGame(io, allocator, game, root, "on-trucks", comptime trucksSchedule(step0, ""));
        try expectContainers(run, 0, &at_start, &.{});
        try expectCursors(run, &.{ board, board });
        try expectContainers(run, 1, &.{ truck_b ++ "passangers=10", house_a ++ "passangers=10" }, &.{truck_a});
        try expectContainers(run, 2, &.{ house_b ++ "passangers=10", house_a ++ "passangers=10" }, &.{ truck_a, truck_b });
    }
    // House to house, house to truck.
    {
        const run = try runGame(io, allocator, game, root, "on-houses", houses_schedule);
        try expectCursors(run, &.{ board, board });
        try expectContainers(run, 1, &.{ house_b ++ "passangers=10", truck_a ++ "passangers=10" }, &.{house_a});
        try expectContainers(run, 2, &.{ truck_b ++ "passangers=10", truck_a ++ "passangers=10" }, &.{ house_a, house_b });
    }
    // Its own truck and a full one refuse; entrenchment to truck.
    {
        const run = try runGame(io, allocator, game, root, "on-full", full_schedule);
        try expectCursors(run, &.{ no_cursor, no_cursor, board });
        try expectContainers(run, 1, &at_start, &.{});
        try expectContainers(run, 2, &at_start, &.{});
        try expectContainers(run, 3, &.{ truck_a ++ "passangers=20", truck_c ++ "passangers=20", house_a ++ "passangers=10" }, &.{});
    }
    // A quarter turn on: truck to truck, truck to house.
    {
        const run = try runGame(io, allocator, game, root, "on-turned", comptime trucksSchedule(step1, "g10:key=.,g1000:anchor,"));
        try expectCursors(run, &.{ board, board });
        try expectContainers(run, 1, &.{ truck_b ++ "passangers=10", house_a ++ "passangers=10" }, &.{truck_a});
        try expectContainers(run, 2, &.{ house_b ++ "passangers=10", house_a ++ "passangers=10" }, &.{ truck_a, truck_b });
    }
    // Remastered off: the original game.
    {
        const run = try runGame(io, allocator, game, root, "off", off_schedule);
        try expectCursors(run, &.{ board, board });
        try expectContainers(run, 1, &.{ truck_a ++ "passangers=10 selectable=0", truck_c ++ "passangers=20 selectable=0" }, &.{house_a});
        try expectContainers(run, 2, &.{ truck_b ++ "passangers=10 selectable=0", truck_c ++ "passangers=20" }, &.{house_a});
    }
}

/// The mod: mod.xml and the mission map, river3d.xml with the objects and the entrenchment in.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, root: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const maps = try std.fs.path.join(allocator, &.{ data, "maps" });
    try cwd.createDirPath(io, maps);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Transfer test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Entrenchments/>"));
    const with_objects = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", "<Objects>" ++ objects ++ "</Objects>");
    const with_trench = try std.mem.replaceOwned(u8, allocator, with_objects, "<Entrenchments/>", entrenchments);
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ maps, map_name }), .data = with_trench });
}

/// Runs the Game in a scratch profile named `name` under `schedule` on the
/// map; it must exit 0 without a leak. Its output is kept as `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, name: []const u8, schedule: []const u8) !Run {
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
    try environ.put("BK_AI_TRACE", "1");
    try environ.put("BK_CURSOR_TRACE", "1");
    try environ.put("BK_AUTO_UI", schedule);
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", "-mode=1024x768", "-profile=Transfer", "-mod=" ++ mod_name, map_name },
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

    var containers: std.ArrayList([]const u8) = .empty;
    var cursors: std.ArrayList(u32) = .empty;
    var dump: std.ArrayList(u8) = .empty;
    var mode: u32 = no_cursor;
    var lines = std.mem.tokenizeAny(u8, log, "\r\n");
    while (lines.next()) |line| {
        if (std.mem.indexOf(u8, line, "BK_CURSOR_TRACE: mode=")) |at| {
            const rest = line[at + "BK_CURSOR_TRACE: mode=".len ..];
            const end = std.mem.indexOfScalar(u8, rest, ' ') orelse rest.len;
            mode = std.fmt.parseInt(u32, rest[0..end], 10) catch mode;
        }
        if (std.mem.indexOf(u8, line, " action rclick=") != null) try cursors.append(allocator, mode);
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
    return .{ .log_path = log_path, .containers = containers.items, .cursors = cursors.items };
}

fn expectCursors(run: Run, expected: []const u32) !void {
    if (std.mem.eql(u32, run.cursors, expected)) return;
    std.debug.print("cursors at the right clicks: expected {any}, got {any}; the Game's output is in {s}\n", .{ expected, run.cursors, run.log_path });
    return error.CursorDiffers;
}

/// Dump `index` holds a line containing each of `present` and none naming any of `absent`.
fn expectContainers(run: Run, index: usize, present: []const []const u8, absent: []const []const u8) !void {
    const dump = if (index < run.containers.len) run.containers[index] else "";
    var ok = index < run.containers.len;
    for (present) |want| {
        if (std.mem.indexOf(u8, dump, want) == null) ok = false;
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
