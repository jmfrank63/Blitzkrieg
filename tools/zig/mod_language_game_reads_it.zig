//! A gameplay mod made in English under a language mod, on the real Game: the case of Achtung
//! Panzer 2 played with lang-ru, from tracked data only. The tier writes two mods into a scratch
//! mods folder (BK_MODS_ROOT) beside a copy of the installed lang-ru, and the profile's language
//! option is lang-ru.
//!
//! - langcompat: the shipped main menu with its Multiplayer button's text key renamed to a key only
//!   the mod has, and the texts NTextPrecedence picks between (Sources/src/Main/TextPrecedence.h):
//!   Options as the base game's English byte for byte and Load Game in other case and spacing (the
//!   language's translation of the key wins), New Game lightly reworded ("New Games", the
//!   translation of the key wins too), Exit Game rewritten (the mod's own text stays), the
//!   new key's text the base game's English "Multiplayer" (the translation of that English wins). It
//!   ships the menu's two smoke videos as empty .bik files, as Achtung Panzer 2 does to keep its
//!   picture clear.
//! - langcontrol: the same menu with the original key and the same empty videos, no texts.
//!
//! The Game captures the main menu at 1024x768 with each (`-reference-scene`), and once without a
//! mod. Each text's source is the Game's own BK_TEXT_TRACE line; the frames prove what is drawn:
//! the New Game, Options, Load Game and Multiplayer buttons of langcompat are the control's to the pixel (the
//! same Russian words), its Exit Game button is not (the mod's English). Neither mod plays a smoke
//! video (the Game logs the skip and decodes no smoke frame), while the base game without a mod
//! does, so its video windows differ from langcontrol's.
//!
//! Then a save is written in the first Allied mission with the profile's language Default and
//! loaded with it lang-ru: the load must not switch the session back to Default (a save holds the
//! globals of its moment, the language among them; loading Achtung Panzer 2's save in Russian put
//! the mod's bitmap fonts back mid-mission, and its Russian texts drew as boxes).
//!
//! Frames and logs are kept in zig-out/local-test/mod-language/game-reads-it. The user's data is
//! never touched: XDG_DATA_HOME, BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT are the scratch
//! folder. BK_MOD_LANGUAGE_GAME names the installed Game executable; `zig build
//! mod-language-game-reads-it` sets it and installs the language mods first.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/mod-language/game-reads-it";
const width = 1024;
const height = 768;
const language = "lang-ru";

const Rect = struct { x1: usize, y1: usize, x2: usize, y2: usize };
/// The main menu's buttons on the 1024x768 canvas (bk_font_game_reads_it.zig places them).
const new_game_button = Rect{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 };
const multiplayer_button = Rect{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 };
const load_button = Rect{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 };
const options_button = Rect{ .x1 = 650, .y1 = 462, .x2 = 970, .y2 = 512 };
const exit_button = Rect{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 };
/// The first smoke video's window (MainMenu.xml: 368x336 at 0,113), clear of every label.
const smoke_window = Rect{ .x1 = 0, .y1 = 113, .x2 = 368, .y2 = 449 };

/// BK_AUTO_UI: the Allies campaign, its first chapter, its first mission; MAIN_COMMAND_SAVE and
/// MAIN_COMMAND_LOAD through cmdc (text_engine_game_reads_it.zig writes and loads its save so).
const save_name = "modlanguage.sav";
const save_schedule = "60:campaign=2=scenarios\\campaigns\\allies\\allies,120:chapter=scenarios\\chapters\\allies\\norway\\1," ++
    "240:var=Mission.Last.FinishStatus=-1,300:mission=scenarios\\scenariomissions\\allies\\norway\\1,420:ok," ++
    "g3000:cmdc=268501012=" ++ save_name ++ ";0,g500:exit";
const load_schedule = "60:cmdc=268501013=" ++ save_name ++ ",t1500:exit";

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

const Context = struct {
    io: std.Io,
    allocator: std.mem.Allocator,
    game: []const u8,
    root: []const u8,
    user: []const u8,
    mods: []const u8,

    /// Runs the Game in the scratch folder; it must exit 0 without a leak or a missing glyph.
    /// Returns its output, kept as `<name>.log`.
    fn play(self: Context, name: []const u8, profile: []const u8, args: []const []const u8, schedule: ?[]const u8) ![]u8 {
        const log_path = try std.fmt.allocPrint(self.allocator, "{s}/{s}.log", .{ self.root, name });
        var environ = try testing.environ.createMap(self.allocator);
        try environ.put("XDG_DATA_HOME", self.user);
        try environ.put("BK_USER_ROOT", self.user);
        try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(self.allocator, &.{ self.user, "profiles" }));
        try environ.put("BK_MODS_ROOT", self.mods);
        try environ.put("BK_MEM_REPORT", "count");
        try environ.put("BK_DEBUG_LOG", "1");
        try environ.put("BK_TEXT_TRACE", "1");
        try environ.put("BK_GLYPH_TRACE", "1");
        try environ.put("BK_NO_HELP", "1");
        if (schedule) |text| try environ.put("BK_AUTO_UI", text) else _ = environ.swapRemove("BK_AUTO_UI");
        var argv: std.ArrayList([]const u8) = .empty;
        try argv.appendSlice(self.allocator, &.{ self.game, "-windowed", try std.fmt.allocPrint(self.allocator, "-profile={s}", .{profile}) });
        try argv.appendSlice(self.allocator, args);
        const result = try timed_run.run(self.allocator, self.io, .{
            .argv = argv.items,
            .cwd = .{ .path = self.user },
            .environ_map = &environ,
            .timeout = .{ .duration = .{ .raw = .fromSeconds(300), .clock = .awake } },
        });
        const log = try std.mem.concat(self.allocator, u8, &.{ result.stdout, result.stderr });
        try std.Io.Dir.cwd().writeFile(self.io, .{ .sub_path = log_path, .data = log });
        if (!result.term.success()) {
            std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
            return error.GameFailed;
        }
        try l10n_measure.expectNoLeak(log, log_path);
        try l10n_measure.expectNoMissingGlyph(log, log_path);
        return log;
    }

    /// The profile's language option, set with a short run of the Game on the options' own `set=`.
    fn chooseLanguage(self: Context, name: []const u8, profile: []const u8, choice: []const u8) !void {
        _ = try self.play(name, profile, &.{}, try std.fmt.allocPrint(self.allocator, "30:set=GamePlay.Language={s},60:exit", .{choice}));
    }

    /// The main menu captured with `mod` (none: the base game) and the run's output.
    fn capture(self: Context, name: []const u8, mod: ?[]const u8) !struct { frame: []u8, log: []u8 } {
        const path = try std.fmt.allocPrint(self.allocator, "{s}/{s}.rgba", .{ self.root, name });
        var args: std.ArrayList([]const u8) = .empty;
        try args.appendSlice(self.allocator, &.{ "-reference-scene", path, "-reference-resolution", "1024", "768" });
        if (mod) |one| try args.append(self.allocator, try std.fmt.allocPrint(self.allocator, "-mod={s}", .{one}));
        const log = try self.play(name, "ModLanguage", args.items, null);
        try expectLine(log, name, "BK_REFERENCE_SCENE: capture complete");
        try expectLine(log, name, "language: \"" ++ language ++ "\" mounted");
        const frame = try std.Io.Dir.cwd().readFileAlloc(self.io, path, self.allocator, .limited(16 << 20));
        try testing.expectEqual(@as(usize, width * height * 4), frame.len);
        return .{ .frame = frame, .log = log };
    }
};

test "a gameplay mod's texts under a language mod: the language translates what the mod did not change, the mod's own videos stay off, a save keeps the language" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_MOD_LANGUAGE_GAME")) orelse {
        std.debug.print("BK_MOD_LANGUAGE_GAME is not set: run zig build mod-language-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    const mods = try std.fs.path.join(allocator, &.{ root, "mods" });
    // defconf.cfg, the options' declarations, is looked for under the working directory.
    const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
    try cwd.createDirPath(io, configs);
    try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" }), try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }));
    for ([_][]const u8{ "mod.xml", "texts.pak" }) |file| {
        const target = try std.fs.path.join(allocator, &.{ mods, language, "data" });
        try cwd.createDirPath(io, target);
        try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "mods", language, "data", file }), try std.fs.path.join(allocator, &.{ target, file }));
    }
    try writeMod(io, allocator, mods, "langcompat", true);
    try writeMod(io, allocator, mods, "langcontrol", false);

    const context = Context{ .io = io, .allocator = allocator, .game = game, .root = root, .user = user, .mods = mods };
    try context.chooseLanguage("choose", "ModLanguage", language);
    const base = try context.capture("base", null);
    const control = try context.capture("control", "langcontrol");
    const modded = try context.capture("mod", "langcompat");

    // The source of each text, as the Game's text manager picked it.
    for ([_][]const u8{
        "BK_TEXT_TRACE: language textes\\strings\\imheader-gbutton-options.txt",
        "BK_TEXT_TRACE: language textes\\strings\\imheader-gbutton-loadgame.txt",
        "BK_TEXT_TRACE: reworded textes\\strings\\imheader-gbutton-newgame.txt",
        "BK_TEXT_TRACE: mod textes\\strings\\imheader-gbutton-exitgame.txt",
        "BK_TEXT_TRACE: memory textes\\strings\\langcompat-multiplayer.txt",
    }) |line| try expectLine(modded.log, "mod", line);

    // The mods' empty videos play nothing; the base game's play.
    for ([_][]const u8{ "smoke1", "smoke2" }) |video| {
        const decoded = try std.fmt.allocPrint(allocator, "Open video first frame decoded: \"movies\\interface\\{s}.ogv\"", .{video});
        const skipped = try std.fmt.allocPrint(allocator, "Open video: \"movies\\interface\\{s}.ogv\" is replaced by the mod's .bik", .{video});
        try expectLine(base.log, "base", decoded);
        for ([_][]const u8{ control.log, modded.log }, [_][]const u8{ "control", "mod" }) |log, name| {
            try expectLine(log, name, skipped);
            if (std.mem.indexOf(u8, log, decoded) != null) {
                std.debug.print("the Game ({s}) decoded {s}, which the mod replaced with an empty .bik; its output is in {s}/{s}.log\n", .{ name, video, root, name });
                return error.ReplacedVideoPlayed;
            }
        }
    }

    const Check = struct { name: []const u8, rect: Rect, same: bool };
    for ([_]Check{
        .{ .name = "Options (the base game's English)", .rect = options_button, .same = true },
        .{ .name = "Load Game (other case and spacing)", .rect = load_button, .same = true },
        .{ .name = "New Game (lightly reworded)", .rect = new_game_button, .same = true },
        .{ .name = "Multiplayer (a key of the mod's, English of the base game)", .rect = multiplayer_button, .same = true },
        .{ .name = "Exit Game (the mod's own text)", .rect = exit_button, .same = false },
    }) |check| {
        const changed = changedPixels(control.frame, modded.frame, check.rect);
        errdefer std.debug.print("{s}: {d} pixels differ from the control's button\n", .{ check.name, changed });
        if (check.same) try testing.expectEqual(@as(usize, 0), changed) else try testing.expect(changed >= 200);
    }
    {
        const changed = changedPixels(base.frame, control.frame, smoke_window);
        errdefer std.debug.print("the smoke video's window differs between the base game (video) and the control (none) in {d} pixels\n", .{changed});
        try testing.expect(changed >= 1000);
    }

    // A save written in Default, loaded in Russian: the session stays Russian.
    try context.chooseLanguage("save-choose", "ModLanguageSave", "Default");
    _ = try context.play("save", "ModLanguageSave", &.{"-mode=1024x768"}, save_schedule);
    const save_path = try std.fs.path.join(allocator, &.{ user, "profiles", "ModLanguageSave", "saves", save_name });
    const saved = cwd.readFileAlloc(io, save_path, allocator, .limited(512 << 20)) catch |err| {
        std.debug.print("the save {s} was not written ({s}); the Game's output is in {s}/save.log\n", .{ save_path, @errorName(err), root });
        return error.SaveMissing;
    };
    try testing.expect(saved.len > 1024);
    try context.chooseLanguage("load-choose", "ModLanguageSave", language);
    const loaded = try context.play("load", "ModLanguageSave", &.{"-mode=1024x768"}, load_schedule);
    const end = std.mem.indexOf(u8, loaded, "LOADTRACE: CICLoad::Exec end") orelse {
        std.debug.print("the load did not run to its end; the Game's output is in {s}/load.log\n", .{root});
        return error.LoadFailed;
    };
    try expectLine(loaded[0..end], "load", "language: \"" ++ language ++ "\" mounted");
    if (std.mem.indexOf(u8, loaded[end..], "language: \"") != null) {
        std.debug.print("loading a save written in Default switched the session's language; the Game's output is in {s}/load.log\n", .{root});
        return error.LanguageSwitchedByLoad;
    }
}

/// A mod for the shipped main menu: mod.xml, the empty smoke videos and the menu; `texts` adds the
/// texts and renames the Multiplayer button's key.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, mods: []const u8, name: []const u8, texts: bool) !void {
    const cwd = std.Io.Dir.cwd();
    const data = try std.fs.path.join(allocator, &.{ mods, name, "data" });
    const strings = try std.fs.path.join(allocator, &.{ data, "textes", "strings" });
    const ui = try std.fs.path.join(allocator, &.{ data, "ui" });
    const movies = try std.fs.path.join(allocator, &.{ data, "movies", "interface" });
    for ([_][]const u8{ strings, ui, movies }) |folder| try cwd.createDirPath(io, folder);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = try std.fmt.allocPrint(allocator, "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>{s}</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n", .{name}),
    });
    for ([_][]const u8{ "smoke1.bik", "smoke2.bik" }) |video| try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ movies, video }), .data = "" });

    var menu = try cwd.readFileAlloc(io, "Data/UI/MainMenu.xml", allocator, .limited(4 << 20));
    if (texts) {
        try testing.expectEqual(@as(usize, 1), std.mem.count(u8, menu, "<TextKey>imheader-gbutton-multiplayer</TextKey>"));
        menu = try std.mem.replaceOwned(u8, allocator, menu, "<TextKey>imheader-gbutton-multiplayer</TextKey>", "<TextKey>langcompat-multiplayer</TextKey>");
        try copyFile(io, allocator, "Data/Textes/strings/imheader-gbutton-options.txt", try std.fs.path.join(allocator, &.{ strings, "imheader-gbutton-options.txt" }));
        for ([_][2][]const u8{
            .{ "imheader-gbutton-loadgame.txt", "  load   GAME\r\n" },
            .{ "imheader-gbutton-newgame.txt", "New Games" },
            .{ "imheader-gbutton-exitgame.txt", "Leave the test mod" },
            .{ "langcompat-multiplayer.txt", "Multiplayer" },
        }) |text| try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ strings, text[0] }), .data = try utf16(allocator, text[1]) });
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ ui, "MainMenu.xml" }), .data = menu });
}

/// UTF-16 LE with a byte order mark, as the Game reads texts.
fn utf16(allocator: std.mem.Allocator, text: []const u8) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(allocator, &.{ 0xff, 0xfe });
    var units = (try std.unicode.Utf8View.init(text)).iterator();
    while (units.nextCodepoint()) |c| {
        var unit: [2]u8 = undefined;
        std.mem.writeInt(u16, &unit, @intCast(c), .little);
        try out.appendSlice(allocator, &unit);
    }
    return out.items;
}

fn copyFile(io: std.Io, allocator: std.mem.Allocator, from: []const u8, to: []const u8) !void {
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, from, allocator, .limited(64 << 20));
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = to, .data = bytes });
}

fn changedPixels(a: []const u8, b: []const u8, rect: Rect) usize {
    var count: usize = 0;
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        const at = (y * width + x) * 4;
        if (!std.mem.eql(u8, a[at..][0..3], b[at..][0..3])) count += 1;
    };
    return count;
}

fn expectLine(log: []const u8, name: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game ({s}) did not print \"{s}\"; its output is in " ++ scratch ++ "/{s}.log\n", .{ name, line, name });
    return error.GameLineMissing;
}
