//! M004 S02 on the real Game: the installed Game started with BK_TEXT_ENGINE=ttf draws and measures
//! its text through FontGpu/TextGpu on bk_text (Liberation Serif and the Noto faces staged beside the
//! Game, an R8 glyph atlas in the SDL GPU backend), and the shapes it draws are measured by code.
//!
//! Per language (en, de, hi; one shard each, BK_TEXT_ENGINE_LANGS names the codes) the Game runs on
//! a scratch profile whose language option is the language's mod (hi: `lang-hi`, the Hindi texts
//! as plain Unicode, no shaper and no fonts), then to its `-reference-scene` capture of the main
//! menu and of the first Allied mission's briefing at 1024x768. The background of both is a capture
//! of the bitmap path on a blank mod (the shipped English texts on fonts with every glyph's ink
//! removed), so the frame minus that background is the ink of the labels and of nothing else: the
//! engine changes how text is drawn and nothing else on the screen. Every run fails on a
//! BK_GLYPH_TRACE line, and every ttf run on a log without the bridge's `engine ttf` line (a bridge
//! that fell back to the bitmap fonts logs `engine bitmap` and would otherwise pass for ttf).
//!
//! - en, de: ink inside each label's box, wide enough, no solid missing-glyph box; de's labels are
//!   drawn differently from English's on the same engine.
//! - hi: each label's ink is compared with hb-view's raster of the same string from Noto Sans
//!   Devanagari (the texts are read from the tracked overlay, so what draws them is not in the
//!   reference): the normalised correlation clears a threshold, the label's own reference beats
//!   every other label's by a margin and the proportions agree (D066, D094; a missing hb-view
//!   fails, D069).
//! - all three: the same capture of three more frames on the same blank background: the options screen
//!   with the restore-defaults button's tooltip, the Churchill's encyclopedia page (title, two panel
//!   headers and the description) and the first mission running (the objective popup's title and
//!   text, and the chat line that announces the objective). Each label's box holds ink, wide enough,
//!   with no solid missing-glyph box; the options and encyclopedia texts the language's overlay does
//!   not translate are English, so there is no hb-view reference for them. Each measure is a line of
//!   `report.txt` with the box, the ink count and the thresholds.
//! - en only, once: a save written on the default (bitmap) path in the first Allied mission loads
//!   under BK_TEXT_ENGINE=ttf: the load completes, and the frame after it holds the load's green
//!   chat line as ink, which the frame before it does not.
//!
//! Frames (`menu.tga`, `briefing.tga`, `options.tga`, `encyclopedia.tga`, `mission.tga`, `saved.tga`, `loaded.tga`) and `report.txt` are kept in
//! `zig-out/local-test/text-engine-game-reads-it/<code>/`. The user's data is never touched:
//! XDG_DATA_HOME and BK_USER_ROOT are the scratch folder. A profile keeps its saves under the
//! Game's own folder, so the save proof uses a profile of its own and removes it. BK_TEXT_ENGINE_GAME
//! names the installed Game executable; `zig build text-engine-game-reads-it` sets it and builds the
//! mods first. The tier runs on Linux and macOS, where hb-view and a window exist; it compiles on
//! every target.
const std = @import("std");
const elk = @import("elk");
const zip = @import("zip");
const measure = @import("l10n_measure");
const database = elk.database;
const packages = elk.packages;
const testing = std.testing;

const Rect = measure.Rect;
const Gray = measure.Gray;

pub fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

const scratch = "zig-out/local-test/text-engine-game-reads-it";
pub const width = 1024;
pub const height = 768;
pub const blank_mod = "text-engine-blank";
pub const profile_arg = "-profile=TextEngine";
const save_profile = "TextEngineSave";
const save_name = "ttfcompat.sav";

/// Whether BK_TEXT_ENGINE_LANGS (a comma list of codes, default all) names `code`.
fn selected(allocator: std.mem.Allocator, code: []const u8) !bool {
    const list = (try envValue(allocator, "BK_TEXT_ENGINE_LANGS")) orelse return true;
    var codes = std.mem.splitScalar(u8, list, ',');
    while (codes.next()) |one| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, one, " "), code)) return true;
    return false;
}

// Thresholds, the l10n tier's (tools/zig/l10n_game_reads_it.zig) where the measure is the same one.
// Frame ink is the luminance the label adds to the background (0..255); a pixel counts as ink above
// `ink_threshold`.
pub const ink_threshold: u8 = 24;
const min_correlation: f64 = 0.50;
const min_margin: f64 = 0.10;
const small_blur: usize = 2;
const small_min_correlation: f64 = 0.30;
const small_min_margin: f64 = -0.03;
const small_max_aspect_error: f64 = 0.20;
const small_box_threshold: u8 = 8;
const max_aspect_error: f64 = 0.12;
/// The briefing's description lines: at least this wide in ink.
const min_body_width: usize = 100;
/// A label's box holds at least this much ink and is at least this wide.
pub const min_ink_pixels: usize = 60;
pub const min_ink_width: usize = 20;
/// A label counts as drawn differently from English when this many pixels of its box differ by more
/// than `frame_tolerance`; all labels but one must (a translation can equal the English word).
const min_text_change: usize = 60;
const frame_tolerance: u8 = 16;
/// A missing glyph is a solid box: 8 by 12 pixels, 95% lit.
pub const solid_w: usize = 8;
pub const solid_h: usize = 12;
pub const solid_fill: f64 = 0.95;

pub const Ink = enum {
    /// Gold text (the buttons, the briefing's title): the ink is the warmth the label adds.
    gold,
    /// Grey or white text: the ink is the brightness the label adds.
    bright,
    /// Dark text over light metal: the ink is the brightness the label takes away.
    dark,
    /// The tooltip's yellow text on its semi-transparent plate: the ink is the yellowness (the lesser
    /// of red and green over blue) the text adds. Brightness cannot tell it, because the blank run's
    /// plate is sized by the inkless bitmap font and ends elsewhere, so the screen behind shows
    /// through one plate where the other covers it (2026-10-23: a Japanese tooltip's last character
    /// lost its ink that way); the plate and the brown screen are hardly yellow, the text is.
    yellow,
};

pub const Label = struct {
    /// The overlay key below `dir`.
    key: []const u8,
    rect: Rect,
    ink: Ink = .gold,
    /// The folder below `Localization/game/<code>/game_data_base/`.
    dir: []const u8 = "textes/strings",
    /// Text of the small font: compared after smoothing both images.
    small: bool = false,
    /// The text is not the overlay's alone (the options caption holds the profile's name), so no hb-view
    /// reference of the overlay's string matches it.
    dynamic: bool = false,
    /// The text is a tooltip: a box framed in the tooltip yellow whose size follows the text, so only
    /// the ink inside the frame counts (`inkImage`), `rect` being where it may hang.
    boxed: bool = false,

    /// The path the Game traces when it opens the text, as the mod's pak names it.
    pub fn tracedPath(self: Label, allocator: std.mem.Allocator) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.txt", .{ self.dir, self.key });
        std.mem.replaceScalar(u8, path, '/', '\\');
        return path;
    }

    /// A file name for the label's hb-view raster.
    pub fn stem(self: Label, allocator: std.mem.Allocator) ![]u8 {
        const name = try std.fmt.allocPrint(allocator, "{s}-{s}", .{ self.dir, self.key });
        std.mem.replaceScalar(u8, name, '/', '-');
        return name;
    }
};

/// The header and the five buttons of the main menu on the 1024x768 canvas.
pub const labels = [_]Label{
    .{ .key = "imheader-mainmenu-header", .rect = .{ .x1 = 630, .y1 = 175, .x2 = 990, .y2 = 248 }, .ink = .bright },
    .{ .key = "imheader-gbutton-newgame", .rect = .{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 } },
    .{ .key = "imheader-gbutton-multiplayer", .rect = .{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 } },
    .{ .key = "imheader-gbutton-loadgame", .rect = .{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 } },
    .{ .key = "imheader-gbutton-options", .rect = .{ .x1 = 650, .y1 = 462, .x2 = 970, .y2 = 512 } },
    .{ .key = "imheader-gbutton-exitgame", .rect = .{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 } },
};

/// The briefing of the first Allied mission (Operation Jupiter: Battle for Norway, a historical
/// mission, so its text keys are fixed): the title, the three panel headers and the first objective.
const mission_dir = "scenarios/scenariomissions/allies/norway";
pub const briefing_labels = [_]Label{
    .{ .key = "header", .dir = mission_dir, .rect = .{ .x1 = 50, .y1 = 40, .x2 = 500, .y2 = 95 } },
    .{ .key = "imheader-mission-objectives", .rect = .{ .x1 = 55, .y1 = 152, .x2 = 440, .y2 = 178 }, .ink = .dark, .small = true },
    .{ .key = "imheader-mission-map", .rect = .{ .x1 = 555, .y1 = 152, .x2 = 980, .y2 = 178 }, .ink = .bright, .small = true },
    .{ .key = "imheader-mission-desc", .rect = .{ .x1 = 555, .y1 = 433, .x2 = 940, .y2 = 458 }, .ink = .bright, .small = true },
    .{ .key = "0h", .dir = mission_dir, .rect = .{ .x1 = 72, .y1 = 200, .x2 = 440, .y2 = 228 }, .ink = .dark },
};
/// The description panel's wrapped text: only checked for ink.
pub const briefing_body = Label{ .key = "description", .dir = mission_dir, .rect = .{ .x1 = 555, .y1 = 485, .x2 = 935, .y2 = 625 }, .ink = .dark };
pub const briefing_extra_keys = [_][]const u8{ "missiontext", "missiontypetext", "scenariomission", "missiondescriptiontext" };
/// BK_AUTO_UI: the Allies campaign (index 2), its first chapter, then the chapter's first mission.
const briefing_schedule = "60:campaign=2=scenarios\\campaigns\\allies\\allies,120:chapter=scenarios\\chapters\\allies\\norway\\1," ++
    "240:var=Mission.Last.FinishStatus=-1,300:mission=scenarios\\scenariomissions\\allies\\norway\\1";
const briefing_delay: u32 = 420;

/// MAIN_COMMAND_SAVE and MAIN_COMMAND_LOAD (Main/iMainClassIDs.h: MAIN_BASE_VALUE 0x10010000 + 20, + 21),
/// sent through `cmdc=<id>=<configuration>`.
const save_command = "268501012";
const load_command = "268501013";
/// The briefing's schedule carried on: `ok` starts the mission and the save follows once it runs,
/// 10.5 s of game time in, after the objective's chat line (shown by 4.8 s, for 5 s) has gone: a save
/// keeps the chat lines on screen, and the load's own line must come second, in `chat_box`.
const save_schedule = briefing_schedule ++ ",420:ok,g10500:cmdc=" ++ save_command ++ "=" ++ save_name ++ ";0,g500:shot,g500:exit";
/// The load: a shot of the main menu as the control, the load from it, a shot of the loaded mission.
/// The loaded game starts paused, so its shot waits on the timer's absolute time (t<ms>), the clock
/// the load's chat line expires on: 1.5 s after the load command, or the first frame after a longer load.
const load_schedule = "50:shot,60:cmdc=" ++ load_command ++ "=" ++ save_name ++ ",t1500:shot,t500:exit";
/// Where the load's chat line ("Game loaded: <name>", pure green) is drawn on the loaded frame, and
/// what counts as its colour; the pixels of a mission's terrain and unit bars that are this green
/// stay under `max_control_pixels`.
const chat_box = Rect{ .x1 = 0, .y1 = 78, .x2 = 500, .y2 = 126 };
const min_chat_pixels: usize = 400;
const max_control_pixels: usize = 100;

/// One more screen: what the schedule does, the frame it must reach and the labels measured on it.
pub const Route = struct {
    tag: []const u8,
    schedule: []const u8,
    delay: u32,
    /// The log lines that show the schedule's last action and the screen reaching the display.
    action: []const u8,
    screen: []const u8,
    labels: []const Label,
    /// A wrapped text, only checked for ink.
    body: ?Label = null,
};

/// The settings screen with the cursor parked on its restore-defaults button (l10n_game_reads_it.zig
/// has the reasoning): the hover comes after X64.ReferenceScene is lifted and the tooltip's dwell has
/// passed. The tooltip hangs below the cursor, so its box is the strip where it may be; the caption
/// holds the profile's name and is measured for ink only.
const options_dir = "textes/ui/optionssettings";
const options_labels = [_]Label{
    .{ .key = "caption_settings", .dir = options_dir, .rect = .{ .x1 = 55, .y1 = 40, .x2 = 500, .y2 = 95 } },
    .{ .key = "tt_button_default", .dir = options_dir, .rect = .{ .x1 = 90, .y1 = 700, .x2 = 1020, .y2 = 768 }, .ink = .yellow, .boxed = true },
};
const options_route = Route{
    .tag = "options",
    .schedule = "60:settings,100:msg=10003,110:var=X64.ReferenceScene=0,400:hover=100x710",
    .delay = 540,
    .action = "BK_AUTO_UI: frame 400 action hover=100x710",
    .screen = "BK_UI_TRACE: reposition \"ui\\OptionsSettings\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    .labels = &options_labels,
};

/// The Allied Churchill's encyclopedia page: MISSION_COMMAND_ENCYCLOPEDIA (269353109) with the
/// configuration `<E_UNIT>;<RPG stats name>` the total encyclopedia sends when a unit is chosen. The
/// title is the unit's `name`, the description its `desc` in the left panel.
const unit_dir = "units/technics/allies/tanks/infantry_mkiv_churchill_gb";
const encyclopedia_labels = [_]Label{
    .{ .key = "name", .dir = unit_dir, .rect = .{ .x1 = 55, .y1 = 40, .x2 = 900, .y2 = 95 } },
    .{ .key = "imheader-encyclopedia-desc", .rect = .{ .x1 = 40, .y1 = 150, .x2 = 440, .y2 = 178 }, .ink = .bright, .small = true },
    .{ .key = "imheader-encyclopedia-picture", .rect = .{ .x1 = 540, .y1 = 150, .x2 = 980, .y2 = 178 }, .ink = .bright, .small = true },
};
const encyclopedia_route = Route{
    .tag = "encyclopedia",
    .schedule = "60:cmdc=269353109=1;Infantry_MKIV_Churchill_GB",
    .delay = 150,
    .action = "BK_AUTO_UI: frame 60 action cmdc=269353109=1;Infantry_MKIV_Churchill_GB",
    .screen = "BK_UI_TRACE: reposition \"ui\\encyclopedia\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    .labels = &encyclopedia_labels,
    .body = .{ .key = "desc", .dir = unit_dir, .rect = .{ .x1 = 55, .y1 = 195, .x2 = 440, .y2 = 630 }, .ink = .dark },
};
pub const routes = [_]Route{ options_route, encyclopedia_route };

/// The first mission some seconds in (5.5 s of game time): the objective's popup in the
/// top right with its title bar and its text, and the chat line that announces the objective at the
/// top left, over terrain. The scene is the same in every language, so the blank run's frame cancels
/// all but the text.
const mission_labels = [_]Label{
    .{ .key = "popup title", .rect = .{ .x1 = 685, .y1 = 6, .x2 = 970, .y2 = 32 }, .ink = .bright },
    .{ .key = "popup text", .rect = .{ .x1 = 650, .y1 = 42, .x2 = 960, .y2 = 90 }, .ink = .bright },
    .{ .key = "objective chat line", .rect = .{ .x1 = 0, .y1 = 34, .x2 = 640, .y2 = 68 }, .ink = .bright },
};
/// The shot is 5.5 s of game time into the mission, not a frame: the objective's chat line comes
/// between 2.7 and 4.8 s and stays for UI.TextAnimTime (5 s), and a frame number is a different time
/// on every build and display (frame 900 was 3.7 s on a release Game on a 120 Hz Mac, 13.6 s on a
/// debug one, after the line had gone).
const mission_schedule = briefing_schedule ++ ",420:ok,g5500:shot,g500:exit";
const mission_shot = "g5500";

/// A route's label holds at least this much ink (pixels above `ink_threshold`): the small font's
/// panel headers are the lightest of them.
pub const min_route_ink_pixels: usize = 60;

pub const Language = struct {
    code: []const u8,
    /// The mod folder; null is the base game (English), which no mod replaces.
    mod: ?[]const u8,
    /// BCP 47 tag for hb-view.
    tag: []const u8 = "",
    /// The Noto face hb-view draws the reference with; null for the Latin languages, which have no
    /// reference and are measured for ink and for a difference from English.
    face: ?[]const u8 = null,
    script: ?[]const u8 = null,

    /// The value of the language option that selects it.
    fn option(self: Language) []const u8 {
        return self.mod orelse "Default";
    }
};

const languages = [_]Language{
    .{ .code = "en", .mod = null },
    .{ .code = "de", .mod = "lang-de" },
    .{ .code = "hi", .mod = "lang-hi", .tag = "hi", .face = "vendor/fonts/NotoSansDevanagari-Regular.ttf", .script = "Deva" },
};

/// Failures are collected, then printed together with the failing measure first.
pub const Report = struct {
    allocator: std.mem.Allocator,
    lines: std.ArrayList(u8) = .empty,
    failures: usize = 0,

    pub fn line(self: *Report, comptime fmt: []const u8, args: anytype) !void {
        try self.lines.print(self.allocator, fmt ++ "\n", args);
    }

    pub fn check(self: *Report, ok: bool, comptime fmt: []const u8, args: anytype) !void {
        if (ok) return self.line("ok    " ++ fmt, args);
        self.failures += 1;
        std.debug.print("FAIL  " ++ fmt ++ "\n", args);
        try self.line("FAIL  " ++ fmt, args);
    }
};

test "the Game draws and measures its text through bk_text under BK_TEXT_ENGINE=ttf (M004 S02)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const game = (try envValue(allocator, "BK_TEXT_ENGINE_GAME")) orelse {
        std.debug.print("BK_TEXT_ENGINE_GAME is not set: run zig build text-engine-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const cwd_path = try std.process.currentPathAlloc(io, allocator);
    const root = try std.fs.path.join(allocator, &.{ cwd_path, scratch });
    try cwd.createDirPath(io, scratch);

    var failures: usize = 0;
    for (languages) |language| {
        if (!try selected(allocator, language.code)) continue;
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        cwd.deleteTree(io, try std.fs.path.join(allocator, &.{ scratch, language.code })) catch {};

        // The background: the bitmap path on the blank mod, so the menu and the briefing with no text.
        const blank_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", blank_mod });
        defer cwd.deleteTree(io, blank_folder) catch {};
        try installBlankMod(io, allocator, game_root, root);
        const blank_name = try std.fs.path.join(allocator, &.{ language.code, "blank" });
        const blank_run = try Run.init(io, allocator, game, game_root, root, blank_name, blank_mod, false, true);
        const blank = try blank_run.capture(allocator);
        const blank_briefing = try blank_run.briefing(allocator, false);

        const run = try Run.init(io, allocator, game, game_root, root, language.code, language.mod, true, false);
        const menu = try run.capture(allocator);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "menu.tga" }), menu);
        const shot = try run.briefing(allocator, language.mod != null);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "briefing.tga" }), shot);

        // The English frames on the same engine, for the language that must differ from them.
        var english_menu: ?[]u8 = null;
        var english_briefing: ?[]u8 = null;
        if (language.mod == null) {
            english_menu = menu;
            english_briefing = shot;
        } else if (language.face == null) {
            const english_name = try std.fs.path.join(allocator, &.{ language.code, "english" });
            const english = try Run.init(io, allocator, game, game_root, root, english_name, null, true, false);
            english_menu = try english.capture(allocator);
            english_briefing = try english.briefing(allocator, false);
        }

        try report.line("== {s} ({s}) on BK_TEXT_ENGINE=ttf: main menu", .{ language.code, language.mod orelse "base game" });
        try checkSet(io, allocator, language, run.folder, menu, blank, english_menu, &labels, &report);
        try report.line("-- {s} briefing", .{language.code});
        try checkSet(io, allocator, language, run.folder, shot, blank_briefing, english_briefing, &briefing_labels, &report);
        const body = try inkImage(allocator, shot, blank_briefing, briefing_body);
        const box = measure.inkBox(body, ink_threshold);
        try report.check(box != null and box.?.width() >= min_body_width, "{s} briefing description: ink {d} pixels wide in the panel's text lines (at least {d})", .{ language.code, if (box) |b| b.width() else 0, min_body_width });

        // The options screen with its tooltip, the unit's encyclopedia page and the mission in progress.
        for (routes) |route| {
            const background = try blank_run.route(allocator, route);
            const frame = try run.route(allocator, route);
            try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, try std.fmt.allocPrint(allocator, "{s}.tga", .{route.tag}) }), frame);
            try report.line("-- {s} {s}", .{ language.code, route.tag });
            try checkInk(allocator, language, frame, background, route.labels, &report);
            if (route.body) |body_label| {
                const body_ink = try inkImage(allocator, frame, background, body_label);
                const body_box = measure.inkBox(body_ink, ink_threshold);
                try report.check(body_box != null and body_box.?.width() >= min_body_width, "{s} {s} {s}: ink {d} pixels wide in the panel's text lines (at least {d})", .{ language.code, route.tag, body_label.key, if (body_box) |b| b.width() else 0, min_body_width });
            }
        }
        const mission_background = try blank_run.mission(allocator);
        const mission_frame = try run.mission(allocator);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "mission.tga" }), mission_frame);
        try report.line("-- {s} mission in progress", .{language.code});
        try checkInk(allocator, language, mission_frame, mission_background, &mission_labels, &report);

        if (language.mod == null) try saveProof(io, allocator, game, game_root, root, &report);

        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, language.code, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("text-engine-game-reads-it: {s}: {d} measures failed; the report is {s}/{s}/report.txt\n", .{ language.code, report.failures, scratch, language.code });
    }
    if (failures != 0) return error.MeasureFailed;
}

/// A save written on the bitmap escape hatch in the first Allied mission loads under the ttf engine: the
/// load runs to its end, and the frame after it has the load's chat line as ink where the frame
/// before it (the main menu) has none.
fn saveProof(io: std.Io, allocator: std.mem.Allocator, game: []const u8, game_root: []const u8, root: []const u8, report: *Report) !void {
    const cwd = std.Io.Dir.cwd();
    const profile_folder = try std.fs.path.join(allocator, &.{ game_root, "profiles", save_profile });
    cwd.deleteTree(io, profile_folder) catch {};
    defer cwd.deleteTree(io, profile_folder) catch {};
    try report.line("-- save written on the bitmap path, loaded under ttf, and the reverse ({s}/saves/{s})", .{ profile_folder, save_name });

    var writer = try Run.init(io, allocator, game, game_root, root, "en/save", null, false, false);
    writer.profile = "-profile=" ++ save_profile;
    cwd.deleteTree(io, profile_folder) catch {};
    const written = try writer.play(allocator, &.{ "-windowed", "-mode=1024x768", writer.profile }, save_schedule, null, "write");
    try report.check(std.mem.indexOf(u8, written, "bk_text: engine ttf") == null, "the save is written on the bitmap escape hatch (the log has no 'engine ttf' line)", .{});
    const save_path = try std.fs.path.join(allocator, &.{ profile_folder, "saves", save_name });
    const bytes = cwd.readFileAlloc(io, save_path, allocator, .limited(512 << 20)) catch |err| {
        try report.check(false, "the save {s} was written ({s})", .{ save_path, @errorName(err) });
        return;
    };
    try report.check(bytes.len > 1024, "the save {s} was written: {d} bytes (more than 1024)", .{ save_path, bytes.len });
    try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "en", "saved.tga" }), try writer.shot(allocator, "g500"));

    var reader = writer;
    reader.ttf = true;
    reader.name = "en/load";
    const read = try reader.play(allocator, &.{ "-windowed", "-mode=1024x768", reader.profile }, load_schedule, null, "load");
    try report.check(std.mem.indexOf(u8, read, "LOADTRACE: CICLoad::Exec end") != null, "the ttf Game loaded the save: the load ran to 'CICLoad::Exec end'", .{});
    try report.check(std.mem.indexOf(u8, read, "to load - skipping") == null and std.mem.indexOf(u8, read, "version mismatch") == null and std.mem.indexOf(u8, read, "CICLoad::Exec open failed") == null, "the load found the file and its version", .{});
    const before = try reader.shot(allocator, "50");
    const after = try reader.shot(allocator, "t1500");
    try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "en", "loaded.tga" }), after);
    const control = chatPixels(before);
    const loaded = chatPixels(after);
    try report.check(control <= max_control_pixels, "the frame before the load has {d} chat-green pixels in {d},{d} to {d},{d} (at most {d})", .{ control, chat_box.x1, chat_box.y1, chat_box.x2, chat_box.y2, max_control_pixels });
    try report.check(loaded >= min_chat_pixels, "the frame after the load has text ink: {d} chat-green pixels where the load's message is drawn (at least {d})", .{ loaded, min_chat_pixels });

    // The other direction: the same profile's save is overwritten on ttf and loads on the bitmap fonts.
    cwd.deleteTree(io, profile_folder) catch {};
    var ttf_writer = writer;
    ttf_writer.ttf = true;
    ttf_writer.name = "en/save-ttf";
    const ttf_written = try ttf_writer.play(allocator, &.{ "-windowed", "-mode=1024x768", ttf_writer.profile }, save_schedule, null, "write");
    try report.check(std.mem.indexOf(u8, ttf_written, "bk_text: engine ttf") != null, "the second save is written on bk_text (the log has the 'engine ttf' line)", .{});
    const ttf_bytes = cwd.readFileAlloc(io, save_path, allocator, .limited(512 << 20)) catch |err| {
        try report.check(false, "the save {s} was written on ttf ({s})", .{ save_path, @errorName(err) });
        return;
    };
    try report.check(ttf_bytes.len > 1024, "the ttf save {s} was written: {d} bytes (more than 1024)", .{ save_path, ttf_bytes.len });
    var bitmap_reader = writer;
    bitmap_reader.name = "en/load-bitmap";
    const bitmap_read = try bitmap_reader.play(allocator, &.{ "-windowed", "-mode=1024x768", bitmap_reader.profile }, load_schedule, null, "load");
    try report.check(std.mem.indexOf(u8, bitmap_read, "LOADTRACE: CICLoad::Exec end") != null, "the bitmap Game loaded the ttf save: the load ran to 'CICLoad::Exec end'", .{});
    try report.check(std.mem.indexOf(u8, bitmap_read, "to load - skipping") == null and std.mem.indexOf(u8, bitmap_read, "version mismatch") == null and std.mem.indexOf(u8, bitmap_read, "CICLoad::Exec open failed") == null, "the bitmap load found the file and its version", .{});
    try report.check(chatPixels(try bitmap_reader.shot(allocator, "t1500")) >= min_chat_pixels, "the bitmap frame after the load has the load's chat line as ink", .{});
}

/// The pixels of the chat line's colour (pure green, 0xff00ff00) in `chat_box`.
fn chatPixels(frame: []const u8) usize {
    var count: usize = 0;
    for (chat_box.y1..chat_box.y2) |y| for (chat_box.x1..chat_box.x2) |x| {
        const at = (y * width + x) * 4;
        count += @intFromBool(frame[at + 1] > 200 and frame[at] < 80 and frame[at + 2] < 80);
    };
    return count;
}

fn luma(rgb: []const u8) i32 {
    return @intCast((@as(u32, rgb[0]) * 299 + @as(u32, rgb[1]) * 587 + @as(u32, rgb[2]) * 114) / 1000);
}

fn warmth(rgb: []const u8) i32 {
    return @as(i32, rgb[0]) - rgb[2];
}

fn yellowness(rgb: []const u8) i32 {
    return @as(i32, @min(rgb[0], rgb[1])) - rgb[2];
}

/// The label's ink, per pixel: what the frame adds to the background in the label's own colour.
pub fn inkImage(allocator: std.mem.Allocator, frame: []const u8, background: []const u8, label: Label) !Gray {
    const rect = label.rect;
    const pixels = try allocator.alloc(u8, rect.width() * rect.height());
    for (0..rect.height()) |y| for (0..rect.width()) |x| {
        const at = ((rect.y1 + y) * width + rect.x1 + x) * 4;
        const added = switch (label.ink) {
            .bright => luma(frame[at..][0..3]) - luma(background[at..][0..3]),
            .dark => luma(background[at..][0..3]) - luma(frame[at..][0..3]),
            .gold => warmth(frame[at..][0..3]) - warmth(background[at..][0..3]),
            .yellow => yellowness(frame[at..][0..3]) - yellowness(background[at..][0..3]),
        };
        pixels[y * rect.width() + x] = @intCast(std.math.clamp(added, 0, 255));
    };
    if (label.boxed) {
        // The frame: the bounding box of the tooltip yellow in the rectangle (frame and text are drawn
        // in it); inside its border, the text.
        var frame_box = Rect{ .x1 = rect.width(), .y1 = rect.height(), .x2 = 0, .y2 = 0 };
        for (0..rect.height()) |y| for (0..rect.width()) |x| {
            const rgb = frame[((rect.y1 + y) * width + rect.x1 + x) * 4 ..][0..3];
            if (!isTooltipYellow(rgb)) continue;
            frame_box = .{ .x1 = @min(frame_box.x1, x), .y1 = @min(frame_box.y1, y), .x2 = @max(frame_box.x2, x + 1), .y2 = @max(frame_box.y2, y + 1) };
        };
        for (0..rect.height()) |y| for (0..rect.width()) |x| {
            const inside = x >= frame_box.x1 + tooltip_border and x + tooltip_border < frame_box.x2 and y >= frame_box.y1 + tooltip_border and y + tooltip_border < frame_box.y2;
            if (!inside) pixels[y * rect.width() + x] = 0;
        };
    }
    return .{ .width = rect.width(), .height = rect.height(), .pixels = pixels };
}

/// The tooltip's frame and text colour, about (205, 205, 0); the gold of the buttons has less green
/// and more blue.
fn isTooltipYellow(rgb: []const u8) bool {
    return rgb[0] > 180 and rgb[1] > 180 and rgb[2] < 40 and @abs(@as(i32, rgb[0]) - rgb[1]) < 24;
}
/// The tooltip frame's line width, with a pixel to spare.
const tooltip_border: usize = 3;

/// The overlay's text for `key` as UTF-8.
pub fn overlayText(io: std.Io, allocator: std.mem.Allocator, code: []const u8, label: Label) ![]u8 {
    const path = try std.fmt.allocPrint(allocator, "Localization/game/{s}/game_data_base/{s}/{s}.txt", .{ code, label.dir, label.key });
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(1 << 16));
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return error.NotUtf16;
    const units = try allocator.alloc(u16, (bytes.len - 2) / 2);
    for (units, 0..) |*unit, at| unit.* = std.mem.readInt(u16, bytes[2 + at * 2 ..][0..2], .little);
    return std.unicode.utf16LeToUtf8Alloc(allocator, units);
}

fn changedIn(a: []const u8, b: []const u8, rect: Rect) usize {
    var changed: usize = 0;
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        const at = (y * width + x) * 4;
        if (!std.mem.eql(u8, a[at..][0..3], b[at..][0..3])) changed += 1;
    };
    return changed;
}

fn matchLabel(allocator: std.mem.Allocator, label: Label, ink: Gray, reference: Gray) !measure.Match {
    return if (label.small) measure.matchReferenceBlurred(allocator, ink, reference, 64, small_blur) else measure.matchReference(allocator, ink, reference, 64);
}

/// One screen's labels: ink inside each box, wide enough, no solid missing-glyph box; where the
/// language has a face, each label against hb-view's raster of its overlay text; where English is
/// given, all but one label drawn differently from it.
fn checkSet(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, english: ?[]const u8, set: []const Label, report: *Report) !void {
    var different: usize = 0;
    const inks = try allocator.alloc(Gray, set.len);
    const refs = try allocator.alloc(Gray, set.len);
    const texts = try allocator.alloc([]u8, set.len);
    const hb_folder = try std.fs.path.join(allocator, &.{ folder, "reference" });
    if (language.face != null) try std.Io.Dir.cwd().createDirPath(io, hb_folder);
    var drawn = true;
    for (set, 0..) |label, index| {
        const full = try inkImage(allocator, frame, blank, label);
        var lit: usize = 0;
        for (full.pixels) |p| lit += @intFromBool(p > ink_threshold);
        const box = measure.inkBox(full, if (label.small and label.ink == .bright) small_box_threshold else ink_threshold);
        const box_width = if (box) |b| b.width() else 0;
        try report.check(lit >= min_ink_pixels and box_width >= min_ink_width, "{s} {s}: {d} ink pixels, ink {d} pixels wide in the box {d},{d} to {d},{d} (at least {d} and {d})", .{ language.code, label.key, lit, box_width, label.rect.x1, label.rect.y1, label.rect.x2, label.rect.y2, min_ink_pixels, min_ink_width });
        try report.check(!measure.hasSolidBox(full, ink_threshold, solid_w, solid_h, solid_fill), "{s} {s}: no missing-glyph box ({d}x{d} window, {d:.0}% lit) in the label", .{ language.code, label.key, solid_w, solid_h, solid_fill * 100 });
        if (english) |reference| if (language.mod != null) {
            const changed = changedIn(frame, reference, label.rect);
            different += @intFromBool(changed >= min_text_change);
            try report.line("      {s} {s}: {d} pixels differ from English", .{ language.code, label.key, changed });
        };
        if (language.face) |face| {
            texts[index] = try overlayText(io, allocator, language.code, label);
            refs[index] = try measure.hbView(allocator, io, hb_folder, try label.stem(allocator), texts[index], .{ .font = face, .language = language.tag, .script = language.script });
            if (box) |b| {
                inks[index] = try measure.crop(allocator, full, b);
            } else {
                drawn = false;
                inks[index] = full;
            }
        }
    }
    if (english != null and language.mod != null and language.face == null) try report.check(different + 1 >= set.len, "{s}: {d} of {d} labels are drawn differently from English (all but one, at least {d} pixels each)", .{ language.code, different, set.len, min_text_change });
    if (language.face == null or !drawn) return;
    for (set, 0..) |label, index| {
        const ink = inks[index];
        const own = try matchLabel(allocator, label, ink, refs[index]);
        var best_other: f64 = -1;
        for (set, 0..) |_, other| {
            if (other == index) continue;
            best_other = @max(best_other, (try matchLabel(allocator, label, ink, refs[other])).correlation);
        }
        const floor = if (label.small) small_min_correlation else min_correlation;
        const margin = if (label.small) small_min_margin else min_margin;
        const aspect_error = if (label.small) small_max_aspect_error else max_aspect_error;
        try report.check(own.correlation >= floor, "{s} {s} \"{s}\": correlation {d:.3} with hb-view (minimum {d:.2}), ink {d}x{d}", .{ language.code, label.key, texts[index], own.correlation, floor, ink.width, ink.height });
        try report.check(own.correlation - best_other >= margin, "{s} {s}: own reference beats the best other by {d:.3} (minimum {d:.2})", .{ language.code, label.key, own.correlation - best_other, margin });
        try report.check(@abs(own.aspect - 1) <= aspect_error, "{s} {s}: width to height is {d:.3} of the reference's (within {d:.2})", .{ language.code, label.key, own.aspect, aspect_error });
    }
}

/// Ink inside each label's box, wide enough, no solid missing-glyph box, and, per label, a report
/// line with the box, the ink count and the thresholds. The routes' texts are English where the
/// language's overlay does not cover them, so there is no per-language reference here.
fn checkInk(allocator: std.mem.Allocator, language: Language, frame: []const u8, background: []const u8, set: []const Label, report: *Report) !void {
    for (set) |label| {
        const full = try inkImage(allocator, frame, background, label);
        var lit: usize = 0;
        for (full.pixels) |p| lit += @intFromBool(p > ink_threshold);
        const box = measure.inkBox(full, if (label.small and label.ink == .bright) small_box_threshold else ink_threshold);
        const box_width = if (box) |b| b.width() else 0;
        const wanted = if (label.small) min_route_ink_pixels else min_ink_pixels;
        try report.check(lit >= wanted and box_width >= min_ink_width, "{s} {s}: {d} ink pixels, ink {d} pixels wide in the box {d},{d} to {d},{d} (at least {d} and {d})", .{ language.code, label.key, lit, box_width, label.rect.x1, label.rect.y1, label.rect.x2, label.rect.y2, wanted, min_ink_width });
        try report.check(!measure.hasSolidBox(full, ink_threshold, solid_w, solid_h, solid_fill), "{s} {s}: no missing-glyph box ({d}x{d} window, {d:.0}% lit) in the label", .{ language.code, label.key, solid_w, solid_h, solid_fill * 100 });
    }
}

/// One scratch profile of the Game: `<root>/<name>/user` is XDG_DATA_HOME, BK_USER_ROOT and the
/// working directory.
pub const Run = struct {
    io: std.Io,
    game: []const u8,
    folder: []const u8,
    user: []const u8,
    mod: ?[]const u8,
    name: []const u8,
    profile: []const u8 = profile_arg,
    /// Whether the Game runs with BK_TEXT_ENGINE=ttf.
    ttf: bool,
    /// Whether the mod brings bitmap fonts, so the menu capture must show the Game opening them.
    fonts: bool,
    /// Whether the Game runs with BK_TEXT_ENGINE=bitmap, the explicit escape hatch (the side-by-side tier's old frames);
    /// every run that is not `ttf` is one.
    bitmap: bool = false,

    /// Makes the scratch folder and sets the profile's language option to `mod` (Default for the
    /// base game) with a short run of the Game on the options' own `set=`, which the profile keeps.
    pub fn init(io: std.Io, allocator: std.mem.Allocator, game: []const u8, game_root: []const u8, root: []const u8, name: []const u8, mod: ?[]const u8, ttf: bool, fonts: bool) !Run {
        const cwd = std.Io.Dir.cwd();
        const folder = try std.fs.path.join(allocator, &.{ root, name });
        const user = try std.fs.path.join(allocator, &.{ folder, "user" });
        // defconf.cfg, the options' declarations, is looked for under the working directory.
        const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
        try cwd.createDirPath(io, configs);
        const defconf = try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }), .data = try cwd.readFileAlloc(io, defconf, allocator, .limited(1 << 20)) });
        const run = Run{ .io = io, .game = game, .folder = folder, .user = user, .mod = mod, .name = name, .ttf = ttf, .fonts = fonts };
        const schedule = try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{mod orelse "Default"});
        const log = try run.play(allocator, &.{ "-windowed", profile_arg }, schedule, null, "choose");
        if (mod) |one| try expectLine(log, run.folder, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{one}));
        return run;
    }

    pub fn play(self: Run, allocator: std.mem.Allocator, args: []const []const u8, schedule: ?[]const u8, delay: ?u32, tag: []const u8) ![]u8 {
        var environ = try testing.environ.createMap(allocator);
        try environ.put("XDG_DATA_HOME", self.user);
        try environ.put("BK_USER_ROOT", self.user);
        try environ.put("BK_MEM_REPORT", "count");
        try environ.put("BK_DEBUG_LOG", "1");
        try environ.put("BK_MOD_TRACE", "1");
        try environ.put("BK_UI_TRACE", "1");
        try environ.put("BK_NO_HELP", "1");
        try environ.put("BK_GLYPH_TRACE", "1");
        // An unset variable means ttf now, so a bitmap run says so.
        try environ.put("BK_TEXT_ENGINE", if (self.ttf) "ttf" else "bitmap");
        if (schedule) |text| try environ.put("BK_AUTO_UI", text);
        if (delay) |frames| try environ.put("BK_REFERENCE_DELAY", try std.fmt.allocPrint(allocator, "{d}", .{frames}));
        var argv: std.ArrayList([]const u8) = .empty;
        try argv.append(allocator, self.game);
        try argv.appendSlice(allocator, args);
        const result = try measure.timed_run.run(allocator, self.io, .{
            .argv = argv.items,
            .cwd = .{ .path = self.user },
            .environ_map = &environ,
            .timeout = .{ .duration = .{ .raw = .fromSeconds(240), .clock = .awake } },
        });
        const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
        const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ self.folder, tag });
        try std.Io.Dir.cwd().writeFile(self.io, .{ .sub_path = log_path, .data = log });
        if (!result.term.success()) {
            std.debug.print("Game ({s} {s}) ended {any}; its output is in {s}\n", .{ self.name, tag, result.term, log_path });
            return error.GameFailed;
        }
        try measure.expectNoMissingGlyph(log, log_path);
        try measure.expectNoLeak(log, log_path);
        // Any BK_GLYPH_TRACE line, whatever its wording, is a character the font could not draw.
        if (std.mem.indexOf(u8, log, "BK_GLYPH_TRACE") != null) {
            std.debug.print("a BK_GLYPH_TRACE line is in the Game's output {s}\n", .{log_path});
            return error.MissingGlyph;
        }
        if (self.ttf) try expectLine(log, self.folder, "bk_text: engine ttf");
        return log;
    }

    /// The main-menu capture (RGBA, alpha zero).
    pub fn capture(self: Run, allocator: std.mem.Allocator) ![]u8 {
        const log = try self.scene(allocator, null, null, "capture");
        try expectLine(log, self.folder, "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
        if (self.fonts) {
            for ([_][]const u8{ "Fonts\\medium\\1.tfd", "Fonts\\medium\\1_h.dds", "Fonts\\large\\1.tfd", "Fonts\\large\\1_h.dds" }) |file| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"{s}\" from LANGUAGE", .{file});
                try expectLine(log, self.folder, line);
            }
        }
        return self.frame(allocator, "capture");
    }

    /// The first mission's briefing capture. The log must show the schedule's mission action reaching
    /// the screen, and, for a language mod (`keys` set), the Game opening every text key of the
    /// briefing from the mod.
    pub fn briefing(self: Run, allocator: std.mem.Allocator, keys: bool) ![]u8 {
        const log = try self.scene(allocator, briefing_schedule, briefing_delay, "briefing");
        try expectLine(log, self.folder, "BK_AUTO_UI: frame 300 action mission=scenarios\\scenariomissions\\allies\\norway\\1");
        try expectLine(log, self.folder, "BK_UI_TRACE: reposition \"ui\\common\\mission\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
        if (std.mem.indexOf(u8, log, "has no random mission entry") != null) {
            std.debug.print("the Game could not open the briefing's mission; its logs are in {s}\n", .{self.folder});
            return error.BriefingNotOpened;
        }
        if (keys) {
            for (briefing_labels ++ [_]Label{briefing_body}) |label| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"{s}\" from LANGUAGE", .{try label.tracedPath(allocator)});
                try expectLine(log, self.folder, line);
            }
            for (briefing_extra_keys) |key| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"textes\\strings\\{s}.txt\" from LANGUAGE", .{key});
                try expectLine(log, self.folder, line);
            }
        }
        return self.frame(allocator, "briefing");
    }

    /// A route's capture. The log must show the schedule's last action and the screen's reposition (a
    /// failure names the BK_AUTO_UI step it reached).
    pub fn route(self: Run, allocator: std.mem.Allocator, one: Route) ![]u8 {
        const log = try self.scene(allocator, one.schedule, one.delay, one.tag);
        for ([_][]const u8{ one.action, one.screen }) |line| expectLine(log, self.folder, line) catch |err| {
            var lines = std.mem.splitScalar(u8, log, '\n');
            while (lines.next()) |text| {
                if (std.mem.indexOf(u8, text, "BK_AUTO_UI: frame") != null and std.mem.indexOf(u8, text, " action ") != null) std.debug.print("{s}: reached {s}\n", .{ self.name, text });
            }
            return err;
        };
        return self.frame(allocator, one.tag);
    }

    /// The first mission running: the briefing's `ok` starts it and the `shot` action keyed
    /// `mission_shot` writes the frame. Windowed at 1024x768 like the save proof, which runs the same
    /// schedule.
    pub fn mission(self: Run, allocator: std.mem.Allocator) ![]u8 {
        const log = try self.play(allocator, &.{ "-windowed", "-mode=1024x768", self.profile }, mission_schedule, null, "mission");
        try expectLine(log, self.folder, "BK_AUTO_UI: frame 420 action ok");
        return self.shot(allocator, mission_shot);
    }

    /// One run of the Game to its `-reference-scene` capture, `<tag>.rgba`.
    pub fn scene(self: Run, allocator: std.mem.Allocator, schedule: ?[]const u8, delay: ?u32, tag: []const u8) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.rgba", .{ self.folder, tag });
        const log = try self.play(allocator, &.{ "-reference-scene", path, "-reference-resolution", "1024", "768", "-windowed", profile_arg }, schedule, delay, tag);
        try expectLine(log, self.folder, "BK_REFERENCE_SCENE: capture complete");
        return log;
    }

    pub fn frame(self: Run, allocator: std.mem.Allocator, tag: []const u8) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.rgba", .{ self.folder, tag });
        const bytes = try std.Io.Dir.cwd().readFileAlloc(self.io, path, allocator, .limited(16 << 20));
        try testing.expectEqual(@as(usize, width * height * 4), bytes.len);
        return bytes;
    }

    /// The frame a BK_AUTO_UI `shot` action wrote at the schedule entry keyed `key` (a frame, or
    /// g<ms> for game time).
    pub fn shot(self: Run, allocator: std.mem.Allocator, key: []const u8) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/autoshot_{s}_{d}x{d}.rgba", .{ self.user, key, width, height });
        const bytes = try std.Io.Dir.cwd().readFileAlloc(self.io, path, allocator, .limited(16 << 20));
        try testing.expectEqual(@as(usize, width * height * 4), bytes.len);
        return bytes;
    }
};

pub fn expectLine(log: []const u8, folder: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    if (std.mem.startsWith(u8, line, "BK_MOD_TRACE: open ") and std.ascii.findIgnoreCase(log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its logs are in {s}\n", .{ line, folder });
    return error.GameLineMissing;
}

/// `text-engine-blank`: the shipped English texts and the shipped fonts with their ink removed (every
/// texel of the atlas zero, the header kept), installed as a mod of the game so the bitmap path's
/// background capture needs nothing but the language option.
pub fn installBlankMod(io: std.Io, allocator: std.mem.Allocator, game_root: []const u8, root: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    // The shipped fonts: the language mods carry none since M004 S05. Only the ink matters here, and it is removed.
    var fonts: std.ArrayList(packages.Entry) = .empty;
    for ([_][]const u8{ "tiny", "small", "medium", "large" }) |size| for ([_][]const u8{ "1.tfd", "1_h.dds" }) |file| {
        const source = try std.fmt.allocPrint(allocator, "Data/Fonts/{s}/{s}", .{ size, file });
        const data = try cwd.readFileAlloc(io, source, allocator, .limited(64 << 20));
        if (std.mem.endsWith(u8, file, ".dds")) @memset(data[128..], 0);
        try fonts.append(allocator, .{ .name = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ size, file }), .bytes = data });
    };
    const db_folder = try std.fs.path.join(allocator, &.{ root, "blank-db" });
    try database.copyTree(io, "Data/ELK/game_data_base", try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    const elk_xml = try std.fs.path.join(allocator, &.{ db_folder, "elk.xml" });
    try cwd.writeFile(io, .{ .sub_path = elk_xml, .data = try cwd.readFileAlloc(io, "Data/ELK/elk.xml", allocator, .limited(1 << 20)) });
    var db = try database.Database.open(testing.allocator, io, elk_xml);
    defer db.deinit();
    const written = try packages.exportTo(testing.allocator, io, &db, 0, game_root, .{ .mod = blank_mod }, .{ .fonts = fonts.items });
    defer testing.allocator.free(written.path);
    const mod_xml = "<?xml version=\"1.0\"?>\r\n<base><MODName>text engine blank</MODName><MODVersion>1.0</MODVersion><MODType>language</MODType><MODDesc></MODDesc></base>\r\n";
    try database.writeFileAtomic(io, testing.allocator, try std.fs.path.join(allocator, &.{ std.fs.path.dirname(written.path).?, "mod.xml" }), mod_xml);
}

/// An uncompressed 32-bit top-down TGA of an RGBA frame, the frames the tier keeps next to its report.
pub fn writeTga(io: std.Io, allocator: std.mem.Allocator, path: []const u8, rgba: []const u8) !void {
    const out = try allocator.alloc(u8, 18 + rgba.len);
    @memset(out[0..18], 0);
    out[2] = 2;
    std.mem.writeInt(u16, out[12..14], width, .little);
    std.mem.writeInt(u16, out[14..16], height, .little);
    out[16] = 32;
    out[17] = 0x28; // top-down, 8 alpha bits
    for (0..width * height) |i| {
        out[18 + i * 4 + 0] = rgba[i * 4 + 2];
        out[18 + i * 4 + 1] = rgba[i * 4 + 1];
        out[18 + i * 4 + 2] = rgba[i * 4 + 0];
        out[18 + i * 4 + 3] = 255;
    }
    try std.Io.Dir.cwd().createDirPath(io, std.fs.path.dirname(path).?);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = out });
}
