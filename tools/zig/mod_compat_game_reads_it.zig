//! A mod made for the original game, in a folder with a Cyrillic name, on the
//! real Game. The mod is written at test time into `Mods/Тестмод` of the
//! installed game from tracked data: its `mod.xml` names it in Russian, it
//! carries one label's text (the main menu's Exit Game, in Russian) and the
//! shipped `Data/UI/MainMenu.xml` with the original game's key for the
//! menu's two smoke videos, BinkFile. The port renamed that key to VideoFile
//! when Bink went (ac58c0d26), and the Achtung Panzer 2 mod's main menu,
//! which still says BinkFile, played no video: the button asked for
//! ".ogv".
//!
//! The Game runs twice to its own `-reference-scene` capture of the main
//! menu at 1024x768: without a mod (the control) and with it. With the mod
//! it must decode both smoke videos (its own `Open video first frame
//! decoded` lines), open the label's text from the mod (BK_MOD_TRACE) and
//! draw the label: the Exit Game button differs from the control's. A third
//! run opens Options > Load Mod: the list must hold the mod. It used to look
//! in "Mods", which a case-sensitive file system does not have (the installs
//! make "mods"), and listed no mod at all on Linux. The frames and the log of
//! each run are kept in zig-out/local-test/mod-compat/game-reads-it.
//!
//! The mod sits in `Mods/`, as the player had copied Achtung Panzer 2, and a
//! language mod in the installs' `mods/`: on a case-sensitive file system the
//! two are one mods folder. The Game took the first spelling it found, listed
//! only Default under Options > Language and did not find a mod in the other
//! one. The third run opens Options > Settings first, so Options > Language
//! must list the language too. It runs from the scratch folder, not the
//! install, with a fresh profile: the settings screen found no defconf.cfg
//! there and the Debug Game stopped on an empty division list.
//!
//! The user's data is never touched: the Game runs from a scratch folder
//! with XDG_DATA_HOME and BK_USER_ROOT there, in a dedicated profile. The mod
//! is removed from the installation afterwards. BK_MOD_COMPAT_GAME names the
//! installed Game executable; `zig build mod-compat-game-reads-it` sets it.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/mod-compat/game-reads-it";
/// CICChangeMOD lower-cases the mod's name before it opens the folder, but
/// only its ASCII letters: the capital Т stays, and the folder is found.
const mod_name = "Тестмод";
/// The language mod in the installs' `mods/`, beside the mod's `Mods/`.
const language_name = "ModsCaseLanguage";
/// MISSION_COMMAND_MODS_LIST (iMission.h: MISSION_BASE_VALUE 0x100e0000 + 245).
const mods_list_command = "269353205";
const label_text = "Выйти из мода";
const width = 1024;
const height = 768;
/// The Exit Game button on the 1024x768 canvas (bk_font_game_reads_it.zig
/// places the main menu's buttons), exclusive right and bottom.
const exit_button = struct { x1: usize = 650, y1: usize = 527, x2: usize = 970, y2: usize = 577 }{};

/// The value of an environment variable the build step sets, or null.
fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

test "a mod with a Cyrillic folder and the original game's BinkFile key plays and draws (AP2 compatibility)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_MOD_COMPAT_GAME")) orelse {
        std.debug.print("BK_MOD_COMPAT_GAME is not set: run zig build mod-compat-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    try cwd.createDirPath(io, user);

    // A mods folder this test makes goes again afterwards (only if it is empty: the deferred
    // deletes of the mods below run first).
    const upper_mods = try std.fs.path.join(allocator, &.{ game_root, "Mods" });
    const lower_mods = try std.fs.path.join(allocator, &.{ game_root, "mods" });
    const made_upper = if (cwd.access(io, upper_mods, .{})) false else |_| true;
    const made_lower = if (cwd.access(io, lower_mods, .{})) false else |_| true;
    defer if (made_upper) cwd.deleteDir(io, upper_mods) catch {};
    defer if (made_lower) cwd.deleteDir(io, lower_mods) catch {};
    const mod_folder = try std.fs.path.join(allocator, &.{ upper_mods, mod_name });
    cwd.deleteTree(io, mod_folder) catch {};
    defer cwd.deleteTree(io, mod_folder) catch {};
    try writeMod(io, allocator, mod_folder);
    const language_folder = try std.fs.path.join(allocator, &.{ lower_mods, language_name });
    cwd.deleteTree(io, language_folder) catch {};
    defer cwd.deleteTree(io, language_folder) catch {};
    try writeLanguage(io, allocator, language_folder);

    const control = try runGame(io, allocator, game, user, root, "control", null);
    const modded = try runGame(io, allocator, game, user, root, "mod", mod_name);
    for ([_][]const u8{ control, modded }) |frame| try testing.expectEqual(@as(usize, width * height * 4), frame.len);

    var changed: usize = 0;
    for (exit_button.y1..exit_button.y2) |y| for (exit_button.x1..exit_button.x2) |x| {
        const at = (y * width + x) * 4;
        if (!std.mem.eql(u8, control[at..][0..3], modded[at..][0..3])) changed += 1;
    };
    {
        errdefer std.debug.print("the Exit Game button differs from the control in {d} pixels\n", .{changed});
        try testing.expect(changed >= 200);
    }

    // Options > Settings (Options.Language lists the languages), then the mods list; both name
    // each entry in a BK_L10N_TRACE line. The profile is a fresh one.
    const list = try runAuto(io, allocator, game, user, root, "mods-list", "60:settings,100:msg=10003,240:cancel,300:cancel,360:cmd=" ++ mods_list_command ++ ",420:exit");
    const entry = try std.fmt.allocPrint(allocator, "\"{s}\"", .{mod_name});
    const language = try std.fmt.allocPrint(allocator, "\"{s}\" name", .{language_name});
    var listed = false;
    var language_listed = false;
    var lines = std.mem.splitScalar(u8, list, '\n');
    while (lines.next()) |line| {
        if (std.mem.indexOf(u8, line, "BK_L10N_TRACE: mods list entry ") != null and std.mem.endsWith(u8, std.mem.trimEnd(u8, line, "\r"), entry)) listed = true;
        if (std.mem.indexOf(u8, line, "BK_L10N_TRACE: language entry ") != null and std.mem.indexOf(u8, line, language) != null) language_listed = true;
    }
    if (!listed) {
        std.debug.print("the mods list did not hold {s}; the Game's output is in {s}/mods-list.log\n", .{ mod_name, root });
        return error.ModNotListed;
    }
    if (!language_listed) {
        std.debug.print("Options > Language did not list {s}; the Game's output is in {s}/mods-list.log\n", .{ language_name, root });
        return error.LanguageNotListed;
    }
}

/// Runs the Game in the scratch profile under a BK_AUTO_UI schedule; it must exit 0. Returns its
/// output, kept as `<name>.log`.
fn runAuto(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, root: []const u8, name: []const u8, schedule: []const u8) ![]u8 {
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ root, "profiles" }));
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_GLYPH_TRACE", "1");
    try environ.put("BK_NO_HELP", "1");
    try environ.put("BK_AUTO_UI", schedule);
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", "-profile=ModCompatLists" },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    try l10n_measure.expectNoMissingGlyph(log, log_path);
    return log;
}

/// A language mod as the installs' are, to Options > Language: data/texts.pak (an empty
/// archive: the language is listed, never chosen) and a loose data/mod.xml of MODType
/// language, which keeps it out of the mods list.
fn writeLanguage(io: std.Io, allocator: std.mem.Allocator, folder: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const data = try std.fs.path.join(allocator, &.{ folder, "data" });
    try cwd.createDirPath(io, data);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>" ++ language_name ++ "</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n\t<MODType>language</MODType>\r\n</base>\r\n",
    });
    // The end-of-central-directory record of a zip with no entries.
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "texts.pak" }), .data = "PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" });
}

/// The mod: mod.xml, the label's text (UTF-16 LE with a BOM, as the game
/// reads texts) and the shipped main menu with BinkFile for VideoFile.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, mod_folder: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const data = try std.fs.path.join(allocator, &.{ mod_folder, "data" });
    const strings = try std.fs.path.join(allocator, &.{ data, "textes", "strings" });
    const ui = try std.fs.path.join(allocator, &.{ data, "ui" });
    try cwd.createDirPath(io, strings);
    try cwd.createDirPath(io, ui);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Тестовый мод</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });

    var text: std.ArrayList(u8) = .empty;
    try text.appendSlice(allocator, &.{ 0xff, 0xfe });
    var units = (try std.unicode.Utf8View.init(label_text)).iterator();
    while (units.nextCodepoint()) |c| {
        var unit: [2]u8 = undefined;
        std.mem.writeInt(u16, &unit, @intCast(c), .little);
        try text.appendSlice(allocator, &unit);
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ strings, "imheader-gbutton-exitgame.txt" }), .data = text.items });

    const menu = try cwd.readFileAlloc(io, "Data/UI/MainMenu.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 4), std.mem.count(u8, menu, "VideoFile>"));
    const original = try std.mem.replaceOwned(u8, allocator, menu, "VideoFile>", "BinkFile>");
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ ui, "MainMenu.xml" }), .data = original });
}

/// Runs the Game on `mod` (none for the control) to its main-menu capture and
/// returns the frame (RGBA, alpha zero). It must exit 0 and report the
/// capture; with the mod it must decode both smoke videos and open the
/// label's text from the mod. Its output is kept beside the capture as
/// `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, root: []const u8, name: []const u8, mod: ?[]const u8) ![]u8 {
    const capture = try std.fmt.allocPrint(allocator, "{s}/{s}.rgba", .{ root, name });
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ root, "profiles" }));
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_MOD_TRACE", "1");
    try environ.put("BK_GLYPH_TRACE", "1");
    const mod_argument = try std.fmt.allocPrint(allocator, "-mod={s}", .{mod orelse ""});
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-reference-scene", capture, "-reference-resolution", "1024", "768", "-windowed", "-profile=ModCompat" });
    if (mod != null) try argv.append(allocator, mod_argument);
    const result = try timed_run.run(allocator, io, .{
        .argv = argv.items,
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    try l10n_measure.expectNoMissingGlyph(log, log_path);
    try expectLine(log, log_path, "BK_REFERENCE_SCENE: capture complete");
    if (mod != null) {
        try expectLine(log, log_path, "Open video first frame decoded: \"movies\\interface\\smoke1.ogv\"");
        try expectLine(log, log_path, "Open video first frame decoded: \"movies\\interface\\smoke2.ogv\"");
        try expectLine(log, log_path, "BK_MOD_TRACE: open \"textes\\strings\\imheader-gbutton-exitgame.txt\" from MOD");
    }
    return std.Io.Dir.cwd().readFileAlloc(io, capture, allocator, .limited(16 << 20));
}

fn expectLine(log: []const u8, log_path: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}\n", .{ line, log_path });
    return error.GameLineMissing;
}
