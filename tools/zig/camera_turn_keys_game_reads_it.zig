//! The camera turn keys as the maintainer pressed them (M006 round 2,
//! docs/superpowers/specs/2026-10-26-camera-turn-design.md, "Keys of an older
//! config"): his profile's config.cfg was written while `.` was camera_turn
//! and `,` camera_turn_back, before the keys were swapped on 2026-10-28. The
//! Game merges the default binds a config lacks into it at every start
//! (CInputBinder::Repair), so each key was bound to both commands and a press
//! turned the camera a quarter one way and back in the same frame: the view
//! never turned. The other tiers start from a fresh profile and set the
//! option with `set=`, so none of them saw it.
//!
//! Two runs of the real Game with one profile, seeded with a config holding
//! only those two older binds in the mission's section:
//!
//! - options: the main menu's settings screen, its Gameplay tab, a click on
//!   the Remastered row's value (OFF to ON, the SetRemastered action), OK,
//!   exit; the config written at exit must hold the option ON. Before it a
//!   click on the Language row's value must choose a language mod: the mods
//!   folder (BK_MODS_ROOT) is laid out as `zig-out/remastered/mods` of the
//!   hand run (`zig build camera-turn-mod`), remastered_views and copies of
//!   the installed language mods, and with BK_MODS_ROOT it is the only one;
//!   that hand run's folder held remastered_views alone, and Options >
//!   Language offered only Default.
//! - keys: the second tutorial's map with `-mod=remastered_views` (an empty
//!   scratch mod of that name); `.`, `,` and `,` pressed through the bind
//!   chain (`key=`). BK_TURN_TRACE prints the step each turn's frame shows:
//!   it must be 3, 0, 1 (`.` turns clockwise, step 3; `,` back). The config
//!   written at exit must bind camera_turn to `,` and camera_turn_back to `.`
//!   and nothing else, so the older binds are gone for good.
//!
//! The Game runs from zig-out/local-test/camera-turn/keys with every user
//! path redirected; logs and frames stay there. BK_VIEWS_GAME names the
//! installed Game; `zig build camera-turn-keys-game-reads-it` sets it.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const game_views = @import("building_views_game_reads_it.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/camera-turn/keys";
const map = "tutorial\\tutorial2.bzm";
const mod_name = "remastered_views";
const profile = "Turn";
/// The config a profile kept from before the swap: the mission's turn binds
/// as they were, nothing else (the Game fills in every other default).
const older_config = "<?xml version=\"1.0\"?>\r\n<base><Binds><Sections><item><Name>game_mission</Name><Commands>" ++
    "<item><Name>camera_turn</Name><Type>event down</Type><Controls><item>.</item></Controls></item>" ++
    "<item><Name>camera_turn_back</Name><Type>event down</Type><Controls><item>,</item></Controls></item>" ++
    "</Commands></item></Sections></Binds></base>\r\n";
/// The settings screen's Gameplay tab, its Language row's value and its
/// Remastered row's value at 1024x768 (the tab and the language row as the
/// language tier clicks them; the Remastered row measured).
const options_schedule = "60:settings,100:msg=10003,250:click=260x256,300:click=900x371,350:shot,400:click=900x411,450:shot,500:ok,560:exit";
const languages = [_][]const u8{ "lang-de", "lang-es", "lang-fr", "lang-hi", "lang-id", "lang-ja", "lang-pt-br", "lang-ru", "lang-zh-hans" };
const presses = [_][]const u8{ ".", "COMMA", "COMMA" };
const expected_steps = [_]u32{ 3, 0, 1 };

test "the turn keys turn the camera with a config written before they were swapped, Remastered set on the settings screen" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try game_views.envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build camera-turn-keys-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    const config_path = try std.fs.path.join(allocator, &.{ user, "profiles", profile, "config.cfg" });
    try cwd.createDirPath(io, std.fs.path.dirname(config_path).?);
    try cwd.writeFile(io, .{ .sub_path = config_path, .data = older_config });
    const mod = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    try cwd.createDirPath(io, mod);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ mod, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Camera turn keys test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });

    for (languages) |language| {
        // A language mod is data/mod.xml and data/texts.pak.
        const installed = try std.fs.path.join(allocator, &.{ std.fs.path.dirname(game).?, "mods", language, "data" });
        try game_views.copyFolder(io, allocator, installed, try std.fs.path.join(allocator, &.{ root, "mods", language, "data" }));
    }

    _ = try runGame(io, allocator, game, root, user, "options", options_schedule, &.{});
    const after_options = try cwd.readFileAlloc(io, config_path, allocator, .limited(1 << 20));
    const remastered = optionValue(after_options, "GamePlay.Remastered") orelse "";
    const language = optionValue(after_options, "GamePlay.Language") orelse "";
    if (!std.mem.eql(u8, remastered, "ON") or !std.mem.startsWith(u8, language, "lang-")) {
        std.debug.print("the settings screen's clicks left Remastered {s} and Language {s} in {s}; the shots are in {s}\n", .{ remastered, language, config_path, user });
        return error.OptionNotSet;
    }

    var schedule: std.ArrayList(u8) = .empty;
    try schedule.appendSlice(allocator, "g1000:hover=1000x400");
    for (presses) |key| try schedule.print(allocator, ",g500:key={s}", .{key});
    try schedule.appendSlice(allocator, ",g500:exit");
    const log = try runGame(io, allocator, game, root, user, "keys", schedule.items, &.{ "-mod=" ++ mod_name, map });

    const steps = try turnSteps(allocator, log);
    const after_keys = try cwd.readFileAlloc(io, config_path, allocator, .limited(1 << 20));
    const binds = [_][2][]const u8{ .{ "camera_turn", "," }, .{ "camera_turn_back", "." } };
    var binds_ok = true;
    for (binds) |bind| {
        const controls = try boundControls(allocator, after_keys, bind[0]);
        if (controls.len != 1 or !std.mem.eql(u8, controls[0], bind[1])) binds_ok = false;
    }
    const report = try std.fmt.allocPrint(allocator, "steps shown after {s}, {s}, {s}: {any} (expected {any}); binds in the config written at exit {s}\n", .{ presses[0], presses[1], presses[2], steps, expected_steps, if (binds_ok) "as the defaults" else "NOT as the defaults" });
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report });
    if (!std.mem.eql(u32, steps, &expected_steps) or !binds_ok) {
        std.debug.print("{s}the logs are in {s}\n", .{ report, root });
        return error.TurnKeysDead;
    }
}

/// One run of the Game on the profile, its log kept as <name>.log.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, user: []const u8, name: []const u8, schedule: []const u8, extra: []const []const u8) ![]const u8 {
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
    try environ.put("BK_AUTO_UI", schedule);
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-windowed", "-mode=1024x768", "-profile=" ++ profile });
    try argv.appendSlice(allocator, extra);
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
    return log;
}

/// The value of a config's option: its item's first Var, not the Default's.
fn optionValue(config: []const u8, name: []const u8) ?[]const u8 {
    var buffer: [128]u8 = undefined;
    const key_name = std.fmt.bufPrint(&buffer, "<KeyName>{s}</KeyName>", .{name}) catch return null;
    const key = std.mem.indexOf(u8, config, key_name) orelse return null;
    const item = std.mem.lastIndexOf(u8, config[0..key], "<item ") orelse return null;
    const start = (std.mem.indexOfPos(u8, config[0..key], item, "<Var>") orelse return null) + "<Var>".len;
    const end = std.mem.indexOfPos(u8, config[0..key], start, "</Var>") orelse return null;
    return config[start..end];
}

/// The step of every turn's first frame BK_TURN_TRACE printed, in order.
fn turnSteps(allocator: std.mem.Allocator, log: []const u8) ![]u32 {
    var steps: std.ArrayList(u32) = .empty;
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |line| {
        const at = std.mem.indexOf(u8, line, "BK_TURN_TRACE: step ") orelse continue;
        var it = std.mem.tokenizeScalar(u8, line[at + "BK_TURN_TRACE: step ".len ..], ' ');
        const step = std.fmt.parseInt(u32, it.next() orelse continue, 10) catch continue;
        if (!std.mem.eql(u8, it.next() orelse continue, "frame")) continue;
        if (!std.mem.eql(u8, it.next() orelse continue, "0")) continue;
        try steps.append(allocator, step);
    }
    return steps.items;
}

/// The controls of every bind of a command in the config's mission section.
fn boundControls(allocator: std.mem.Allocator, config: []const u8, command: []const u8) ![]const []const u8 {
    const section_at = std.mem.indexOf(u8, config, "<Name>game_mission</Name>") orelse return &.{};
    const section_end = std.mem.indexOfPos(u8, config, section_at, "</Commands>") orelse config.len;
    const section = config[section_at..section_end];
    const name = try std.fmt.allocPrint(allocator, "<Name>{s}</Name>", .{command});
    var controls: std.ArrayList([]const u8) = .empty;
    var from: usize = 0;
    while (std.mem.indexOfPos(u8, section, from, name)) |at| {
        from = at + name.len;
        const open = std.mem.indexOfPos(u8, section, from, "<Controls><item>") orelse break;
        const start = open + "<Controls><item>".len;
        const end = std.mem.indexOfPos(u8, section, start, "</item>") orelse break;
        try controls.append(allocator, section[start..end]);
    }
    return controls.items;
}

test "the config readers: the option, the steps traced and a command's binds" {
    const config = "<base><Options><Vars><item EditorType=\"3\" Order=\"6\"><Var>ON</Var><Action>SetRemastered</Action><KeyName>GamePlay.Remastered</KeyName></item>" ++
        "<item><Var>OFF</Var><KeyName>Cloud.Enabled</KeyName></item></Vars></Options><Binds><Sections><item><Name>game_mission</Name><Commands>" ++
        "<item><Name>camera_turn</Name><Type>event down</Type><Controls><item>.</item></Controls></item>" ++
        "<item><Name>camera_turn_back</Name><Type>event down</Type><Controls><item>,</item></Controls></item>" ++
        "<item><Name>camera_turn</Name><Type>event down</Type><Controls><item>,</item></Controls></item>" ++
        "</Commands></item></Sections></Binds></base>";
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    try testing.expectEqualStrings("ON", optionValue(config, "GamePlay.Remastered").?);
    try testing.expectEqualStrings("OFF", optionValue("<item a=\"1\"><Var>OFF</Var><Default><Var>ON</Var></Default><KeyName>GamePlay.Remastered</KeyName></item>", "GamePlay.Remastered").?);
    try testing.expect(optionValue(config, "GamePlay.Language") == null);
    const turn = try boundControls(allocator, config, "camera_turn");
    try testing.expectEqual(@as(usize, 2), turn.len);
    try testing.expectEqualStrings(".", turn[0]);
    try testing.expectEqualStrings(",", turn[1]);
    const back = try boundControls(allocator, config, "camera_turn_back");
    try testing.expectEqual(@as(usize, 1), back.len);
    const log = "BK_TURN_TRACE: load 0 frame 0 total 1\r\nBK_TURN_TRACE: step 3 frame 0 total 2\r\nBK_TURN_TRACE: step 3 frame 1 total 2\r\nBK_TURN_TRACE: step 0 frame 0 total 2\r\n";
    try testing.expectEqualSlices(u32, &.{ 3, 0 }, try turnSteps(allocator, log));
}
