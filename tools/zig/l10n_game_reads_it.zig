//! M002 R056/D066 on the real Game: the ten languages (English and the nine
//! language mods `language-mods` built) draw a translated main menu and the
//! first mission's briefing. The four mods with generated fonts (ru, zh-Hans, ja,
//! hi) draw what an independent text stack draws; the five on the game's bitmap
//! fonts (es, fr, de, pt-BR, id) are measured for ink, for a frame that differs
//! from English and for missing-glyph boxes, and no hb-view equivalence is
//! claimed for them (the bitmap fonts are not Noto).
//!
//! Per language the Game runs on a scratch profile whose language option is
//! the mod (the option's own `set=`, which the profile keeps), then to its
//! `-reference-scene` capture of the main menu at 1024x768. A second language
//! mod, `l10n-blank`, carries the same fonts with every glyph's ink removed
//! and the shipped English texts, so its capture is the menu's background
//! with no text; the frame minus that background is the ink of the labels
//! and of nothing else. For the header and the five buttons the ink is
//! compared with hb-view's raster of the same string from the same face (the
//! texts are read from the tracked overlay in real Unicode, so the shaper that
//! wrote the Hindi clusters is not in the reference): the normalised
//! correlation must clear a threshold and the label's own reference must beat
//! every other label's by a margin. Then the checks the languages need:
//!
//! - hi: the headline of every word has no gap (D058), the short i matra of
//!   मित्रों lies where hb-view puts it, and the conjuncts (त्र, ख्य) are
//!   drawn as hb-view's conjunct and not as the halant spelling;
//! - zh-Hans and ja: where the Simplified and the Japanese face draw a label
//!   differently, the frame is nearer the language's own face.
//!
//! The first campaign mission's briefing (Allies, Operation Jupiter: Battle for
//! Norway, a historical mission, so its text keys are fixed and no random map
//! enters the frame) is captured the same way, against its own blank
//! background, for the languages that have `briefing` set: the title, the three
//! panel headers and the first objective are compared with hb-view like the
//! menu's labels, and the description body must hold ink. BK_L10N_LANGS (a
//! comma list of codes, default all) picks the languages, so a run can be
//! sharded.
//!
//! Last, per language the Game starts on another language (ru, or English for
//! ru) and switches to this one while it runs (the font cache clear of
//! ApplyLanguage, T03): the buttons must show the cold start's frame. Then the
//! choice is made through the option (`set=`), the Game exits, and a restart on
//! the profile, without any mod argument, must show the cold start's frame
//! again (header included).
//!
//! Frames, references and `report.txt` are kept in
//! `zig-out/local-test/l10n-game-reads-it/<code>/` (a shard is a code, so each
//! has a folder of its own): `menu.tga`, `briefing.tga`, `switch.tga`, `restart.tga`.
//! The user's data is never touched: XDG_DATA_HOME and BK_USER_ROOT are the
//! scratch folder and the Game runs from it. BK_L10N_GAME names the installed Game executable;
//! `zig build l10n-game-reads-it` sets it and builds the mods first.
const std = @import("std");
const elk = @import("elk");
const zip = @import("zip");
const measure = @import("l10n_measure");
const database = elk.database;
const packages = elk.packages;
const testing = std.testing;

const Rect = measure.Rect;
const Gray = measure.Gray;

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

const scratch = "zig-out/local-test/l10n-game-reads-it";
const width = 1024;
const height = 768;
const blank_mod = "l10n-blank";

/// Whether BK_L10N_LANGS (a comma list of codes, default all) names `code`.
fn selected(allocator: std.mem.Allocator, code: []const u8) !bool {
    const list = (try envValue(allocator, "BK_L10N_LANGS")) orelse return true;
    var codes = std.mem.splitScalar(u8, list, ',');
    while (codes.next()) |one| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, one, " "), code)) return true;
    return false;
}

// Thresholds. Each was measured on the first passing run (the values are in
// the report) and set where it leaves room for the filter's and the
// rasterisers' differences but not for a wrong glyph, wrong shaping or a
// wrong face. Frame ink is the luminance the label adds to the background
// (0..255); a pixel counts as ink above `ink_threshold`.
const ink_threshold: u8 = 24;
/// The least correlation of a label's ink with its own hb-view raster.
const min_correlation: f64 = 0.50;
/// How much better the own reference must correlate than the best other
/// label's reference resampled onto the same box.
const min_margin: f64 = 0.10;
/// The small font's labels (`Label.small`, about 11 pixels high) are compared after
/// smoothing with this radius, and their own reference must be the nearest of the set
/// with a lower correlation and a tolerance: at that size the Game's raster and a
/// shaper's differ in the strokes and in proportion, and the panel headers share
/// words ("任务地图" and "任务描述" share two of four characters), so a sibling's
/// reference may match a little better.
const small_blur: usize = 2;
const small_min_correlation: f64 = 0.30;
const small_min_margin: f64 = -0.03;
const small_max_aspect_error: f64 = 0.20;
/// The frame box's width to height against the reference's: within this
/// fraction of 1.
const max_aspect_error: f64 = 0.12;
/// Hindi: the longest run of columns without ink on a word's headline, in
/// frame pixels (D058: the bar is continuous to within a pixel). The bar is
/// about a pixel thick and filtered across two rows, so a column counts as lit
/// from a small share of the label colour (the dips in a good bar reach 12 to
/// 20, an empty column is 0 to 2 on the warmth scale).
const max_headline_gap: usize = 1;
const headline_threshold: u8 = 8;
/// Hindi: the first third of the word that holds the short i matra (the
/// matra and its consonant) against hb-view's first third, and against its
/// last third.
const min_matra_correlation: f64 = 0.4;
const min_matra_margin: f64 = 0.05;
/// Hindi: the columns of a word's conjunct (from where the shaped reference and
/// the halant spelling first differ to the end of the word) must correlate with
/// the shaped reference better than with the halant spelling by
/// `measure.conjunct_margin`. The whole label is no measure of this: the two
/// differ in one cluster, so their correlations with a correct frame were 0.666
/// and 0.683 on macOS.
/// The briefing's description lines: at least this wide in ink (the first
/// line, "Mission: ...", is already over 100 pixels).
const min_body_width: usize = 100;
/// Han: a character cell whose two faces' rasters correlate above this is
/// not informative about the face.
const han_informative_below: f64 = 0.90;
/// Han: the own face's correlation beats the other's by this much, on average
/// over the informative labels and on the 化 label by itself.
const min_face_margin: f64 = 0.01;

/// The pixels of a button's box that may differ between a language started cold
/// and the same one switched to while the Game runs (the menu's backdrop is
/// animated; the buttons' own glyphs must not).
const max_live_difference: usize = 25;

/// Bitmap-font languages: a label's box holds at least this much ink (pixels
/// above `ink_threshold`) and is at least this wide.
const min_bitmap_ink_pixels: usize = 150;
const min_bitmap_width: usize = 20;
/// A label counts as drawn differently from English when this many pixels of
/// its box differ by more than `frame_tolerance`; all labels but one must (a
/// translation can equal the English word, as French "Options" does).
const min_text_change: usize = 60;
const frame_tolerance: u8 = 16;
/// A missing glyph is a solid box: 8 by 12 pixels, 95% lit. The bitmap fonts' own
/// letters fill such a window to 83% at most (measured on the English frames).
const solid_w: usize = 8;
const solid_h: usize = 12;
const solid_fill: f64 = 0.95;

const Ink = enum {
    /// Gold text (the buttons, the briefing's title): the ink is the warmth the
    /// label adds.
    gold,
    /// Grey or white text: the ink is the brightness the label adds.
    bright,
    /// Dark text over light metal (the briefing's panel headers and lines),
    /// with a pale emboss that must not count: the ink is the brightness the
    /// label takes away.
    dark,
};

const Label = struct {
    /// The overlay key below `dir`.
    key: []const u8,
    rect: Rect,
    /// The header of the main menu: FontSize 2 (the large font), grey text. The
    /// buttons use the medium font and gold text.
    ink: Ink = .gold,
    /// The folder below `Localization/game/<code>/game_data_base/`.
    dir: []const u8 = "textes/strings",
    /// Text of the small font (about 11 pixels high): compared after smoothing both
    /// images (`measure.blur`), since the Game's raster hints its strokes a pixel off a
    /// shaper's.
    small: bool = false,

    /// The path the Game traces when it opens the text, as the mod's pak names it.
    fn tracedPath(self: Label, allocator: std.mem.Allocator) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.txt", .{ self.dir, self.key });
        std.mem.replaceScalar(u8, path, '/', '\\');
        return path;
    }

    /// A file name for the label's hb-view raster.
    fn stem(self: Label, allocator: std.mem.Allocator) ![]u8 {
        const name = try std.fmt.allocPrint(allocator, "{s}-{s}", .{ self.dir, self.key });
        std.mem.replaceScalar(u8, name, '/', '-');
        return name;
    }
};

/// The header (element 20001, FontSize 2) and the five buttons (FontSize 1) on
/// the 1024x768 canvas, as in bk_font_game_reads_it.zig: the buttons are 320x50
/// at x 650, y 267 + 65 * n; the header lies in the panel above them.
const labels = [_]Label{
    .{ .key = "imheader-mainmenu-header", .rect = .{ .x1 = 630, .y1 = 175, .x2 = 990, .y2 = 248 }, .ink = .bright },
    .{ .key = "imheader-gbutton-newgame", .rect = .{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 } },
    .{ .key = "imheader-gbutton-multiplayer", .rect = .{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 } },
    .{ .key = "imheader-gbutton-loadgame", .rect = .{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 } },
    .{ .key = "imheader-gbutton-options", .rect = .{ .x1 = 650, .y1 = 462, .x2 = 970, .y2 = 512 } },
    .{ .key = "imheader-gbutton-exitgame", .rect = .{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 } },
};

/// The briefing of the first Allied mission as the Game draws it at 1024x768
/// (boxes measured on a captured English frame): the title in the top bar, the
/// headers of the three panels, and the first objective, whose "+" icon lies
/// left of x 72. The title is gold; the panel texts are grey on metal of
/// different lightness, so each label takes the ink mode that its panel's
/// text shows in (the objectives header and the objective are dark on light
/// metal, the other two panel headers light on dark). The description panel's
/// body wraps, so it is only checked for ink (`briefing_body`).
const mission_dir = "scenarios/scenariomissions/allies/norway";
const briefing_labels = [_]Label{
    .{ .key = "header", .dir = mission_dir, .rect = .{ .x1 = 50, .y1 = 40, .x2 = 500, .y2 = 95 } },
    .{ .key = "imheader-mission-objectives", .rect = .{ .x1 = 55, .y1 = 152, .x2 = 440, .y2 = 178 }, .ink = .dark },
    .{ .key = "imheader-mission-map", .rect = .{ .x1 = 555, .y1 = 152, .x2 = 980, .y2 = 178 }, .ink = .bright, .small = true },
    .{ .key = "imheader-mission-desc", .rect = .{ .x1 = 555, .y1 = 433, .x2 = 940, .y2 = 458 }, .ink = .bright, .small = true },
    .{ .key = "0h", .dir = mission_dir, .rect = .{ .x1 = 72, .y1 = 200, .x2 = 440, .y2 = 228 }, .ink = .dark },
};
/// The description panel's text lines: "Mission: ...", "Type: ..." and the
/// start of the description.
const briefing_body = Label{ .key = "description", .dir = mission_dir, .rect = .{ .x1 = 555, .y1 = 485, .x2 = 935, .y2 = 625 }, .ink = .dark };
/// The text keys the briefing opens besides the labels' own (the "Mission:",
/// "Type:" and "Historical" lines of the description panel and its caption),
/// as BK_MOD_TRACE shows them.
const briefing_extra_keys = [_][]const u8{ "missiontext", "missiontypetext", "scenariomission", "missiondescriptiontext" };
/// BK_AUTO_UI: the Allies campaign (index 2), its first chapter, then the
/// chapter's first mission. The chapter screen resets the finish status, so it
/// is cleared before the mission is named; the briefing is up by frame 360.
const briefing_schedule = "60:campaign=2=scenarios\\campaigns\\allies\\allies,120:chapter=scenarios\\chapters\\allies\\norway\\1," ++
    "240:var=Mission.Last.FinishStatus=-1,300:mission=scenarios\\scenariomissions\\allies\\norway\\1";
const briefing_delay: u32 = 420;

const Language = struct {
    code: []const u8,
    /// The mod folder; null is the base game (English), which no mod replaces.
    mod: ?[]const u8,
    /// BCP 47 tag for hb-view.
    tag: []const u8 = "",
    /// The Noto face of the mod's generated fonts; null where the language keeps
    /// the game's bitmap fonts (the five texts-only mods and English), so there
    /// is no hb-view reference.
    face: ?[]const u8 = null,
    script: ?[]const u8 = null,

    /// The value of the language option that selects it.
    fn option(self: Language) []const u8 {
        return self.mod orelse "Default";
    }
};

const languages = [_]Language{
    .{ .code = "en", .mod = null },
    .{ .code = "ru", .mod = "lang-ru", .tag = "ru", .face = "vendor/fonts/NotoSans-Regular.ttf" },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .tag = "zh-Hans", .face = "vendor/fonts/NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .tag = "ja", .face = "vendor/fonts/NotoSansJP-Regular.otf" },
    .{ .code = "hi", .mod = "lang-hi", .tag = "hi", .face = "vendor/fonts/NotoSansDevanagari-Regular.ttf", .script = "Deva" },
    .{ .code = "es", .mod = "lang-es" },
    .{ .code = "fr", .mod = "lang-fr" },
    .{ .code = "de", .mod = "lang-de" },
    .{ .code = "pt-BR", .mod = "lang-pt-br" },
    .{ .code = "id", .mod = "lang-id" },
};
const ru = languages[1];

/// Failures are collected, then printed together with the failing measure
/// first.
const Report = struct {
    allocator: std.mem.Allocator,
    lines: std.ArrayList(u8) = .empty,
    failures: usize = 0,

    fn line(self: *Report, comptime fmt: []const u8, args: anytype) !void {
        try self.lines.print(self.allocator, fmt ++ "\n", args);
    }

    fn check(self: *Report, ok: bool, comptime fmt: []const u8, args: anytype) !void {
        if (ok) return self.line("ok    " ++ fmt, args);
        self.failures += 1;
        std.debug.print("FAIL  " ++ fmt ++ "\n", args);
        try self.line("FAIL  " ++ fmt, args);
    }
};

test "the Game draws the ten languages' main menus and briefings (R056)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const game = (try envValue(allocator, "BK_L10N_GAME")) orelse {
        std.debug.print("BK_L10N_GAME is not set: run zig build l10n-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const cwd_path = try std.process.currentPathAlloc(io, allocator);
    const root = try std.fs.path.join(allocator, &.{ cwd_path, scratch });
    try cwd.createDirPath(io, scratch);

    // The background: the blank language, in a profile of its own. The shards share
    // the folder of the mods, so each installs the blank mod and removes it at the end.
    const blank_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", blank_mod });
    defer cwd.deleteTree(io, blank_folder) catch {};
    try installBlankMod(io, allocator, game_root, root);
    const blank_root = try std.fs.path.join(allocator, &.{ root, "blank" });
    cwd.deleteTree(io, blank_root) catch {};
    const blank_run = try Run.init(io, allocator, game, game_root, root, "blank", blank_mod, true);
    const blank = try blank_run.capture(allocator, null, null);
    try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "blank", "menu.tga" }), blank);
    const blank_briefing = try blank_run.briefing(allocator, false);
    try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "blank", "briefing.tga" }), blank_briefing);

    var failures: usize = 0;
    for (languages) |language| {
        if (!try selected(allocator, language.code)) continue;
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        cwd.deleteTree(io, try std.fs.path.join(allocator, &.{ scratch, language.code })) catch {};
        const run = try Run.init(io, allocator, game, game_root, root, language.code, language.mod, language.face != null);
        const menu = try run.capture(allocator, null, null);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "menu.tga" }), menu);
        const shot = try run.briefing(allocator, language.mod != null);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "briefing.tga" }), shot);

        // The English frames the bitmap-font languages differ from: the base game, in a
        // profile of its own.
        var english_menu: ?[]u8 = null;
        var english_briefing: ?[]u8 = null;
        if (language.mod == null) {
            english_menu = menu;
            english_briefing = shot;
        } else if (language.face == null) {
            const english_name = try std.fs.path.join(allocator, &.{ language.code, "english" });
            const english = try Run.init(io, allocator, game, game_root, root, english_name, null, false);
            english_menu = try english.capture(allocator, null, null);
            english_briefing = try english.briefing(allocator, false);
        }

        try report.line("== {s} ({s}) main menu", .{ language.code, language.mod orelse "base game" });
        try checkLanguage(io, allocator, language, run.folder, menu, blank, english_menu, &report);
        try report.line("-- {s} briefing", .{language.code});
        try checkBriefing(io, allocator, language, run.folder, shot, blank_briefing, english_briefing, &report);

        // A live switch: the Game starts on the other language and chooses this one while
        // it runs (the font cache clear of ApplyLanguage). Its buttons are the cold
        // start's. The header (labels[0]) is set once when the menu is built and keeps
        // its text on a live switch, so it is reported and not measured.
        const from = if (language.mod == null or !std.mem.eql(u8, language.code, ru.code)) ru else languages[0];
        const switch_name = try std.fs.path.join(allocator, &.{ language.code, "switch" });
        var switch_run = try Run.init(io, allocator, game, game_root, root, switch_name, from.mod, false);
        const schedule = try std.fmt.allocPrint(allocator, "60:var=Text.Language={s}", .{language.option()});
        const live = try switch_run.capture(allocator, schedule, 160);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "switch.tga" }), live);
        var worst: usize = 0;
        for (labels[1..]) |label| worst = @max(worst, changedIn(menu, live, label.rect));
        try report.line("      the header differs from {s} started cold in {d} pixels (not measured: its text is set when the menu is built)", .{ language.code, changedIn(menu, live, labels[0].rect) });
        try report.check(worst <= max_live_difference, "{s} then {s} live: at most {d} pixels differ from {s} started cold in any button's box (limit {d})", .{ from.code, language.code, worst, language.code, max_live_difference });

        // A restart: the choice made through the option is kept by the profile, and the
        // Game started on it without a mod argument shows the cold start's menu.
        const choose = try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{language.option()});
        const chosen = try switch_run.play(allocator, &.{ "-windowed", "-profile=L10n" }, choose, null, "persist");
        if (language.mod) |mod| try expectLine(chosen, switch_run.folder, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{mod}));
        switch_run.fonts = language.face != null;
        const restarted = try switch_run.capture(allocator, null, null);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "restart.tga" }), restarted);
        var restart_worst: usize = 0;
        for (labels) |label| restart_worst = @max(restart_worst, changedIn(menu, restarted, label.rect));
        try report.check(restart_worst <= max_live_difference, "{s} restarted on the profile that kept the choice: at most {d} pixels differ from the cold start in any label's box (limit {d})", .{ language.code, restart_worst, max_live_difference });

        // Options > Language and the gameplay mods list, once for the whole tier.
        if (language.mod == null) try checkLists(allocator, blank_run, &report);

        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, language.code, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("l10n-game-reads-it: {s}: {d} measures failed; the report is {s}/{s}/report.txt\n", .{ language.code, report.failures, scratch, language.code });
    }
    if (failures != 0) return error.MeasureFailed;
}

/// The names Options > Language shows besides Default: each mod's MODName.
const native_names = [_][]const u8{ "Русский", "简体中文", "日本語", "हिन्दी", "Español", "Français", "Deutsch", "Português (Brasil)", "Bahasa Indonesia" };
/// MISSION_COMMAND_MODS_LIST (iMission.h: MISSION_BASE_VALUE 0x100e0000 + 245).
const mods_list_command = "269353205";

/// The gameplay mods list leaves the language mods out and Options > Language
/// lists Default first, then the nine mods by their native names, as the
/// Game's own trace lines (BK_L10N_TRACE) report them from one run: the
/// settings screen and its Options page opened, then the mods list.
fn checkLists(allocator: std.mem.Allocator, run: Run, report: *Report) !void {
    const log = try run.play(allocator, &.{ "-windowed", "-profile=L10n" }, "60:settings,100:msg=10003,240:cancel,300:cancel,360:cmd=" ++ mods_list_command ++ ",420:exit", null, "lists");
    var entries: usize = 0;
    var language_entries: usize = 0;
    var default_first = false;
    var seen: [native_names.len]bool = @splat(false);
    var leaked: usize = 0;
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |line| {
        if (std.mem.indexOf(u8, line, "BK_L10N_TRACE: mods list entry ")) |_| {
            entries += 1;
            if (std.ascii.findIgnoreCase(line, "lang-") != null or std.ascii.findIgnoreCase(line, blank_mod) != null) leaked += 1;
        } else if (std.mem.indexOf(u8, line, "BK_L10N_TRACE: language entry ")) |_| {
            if (language_entries == 0) default_first = std.mem.indexOf(u8, line, "entry 0 \"Default\"") != null;
            language_entries += 1;
            for (native_names, 0..) |name, i| {
                const quoted = try std.fmt.allocPrint(allocator, "name \"{s}\"", .{name});
                if (std.mem.indexOf(u8, line, quoted) != null) seen[i] = true;
            }
        }
    }
    var all_seen = true;
    for (seen) |one| all_seen = all_seen and one;
    try report.line("== option and mods lists ({d} mods list entries, {d} language entries)", .{ entries, language_entries });
    try report.check(entries > 0 and leaked == 0, "the gameplay mods list opened and shows no language mod ({d} entries, {d} language mods in it)", .{ entries, leaked });
    try report.check(default_first and all_seen, "Options > Language lists Default first and the nine native names (default first {any}, names all present {any})", .{ default_first, all_seen });
}

fn changedIn(a: []const u8, b: []const u8, rect: Rect) usize {
    var changed: usize = 0;
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        const at = (y * width + x) * 4;
        if (!std.mem.eql(u8, a[at..][0..3], b[at..][0..3])) changed += 1;
    };
    return changed;
}

/// The label's ink, per pixel: what the frame adds to the background in the
/// label's own colour. The buttons' gold text over grey metal adds warmth
/// (red minus blue); it is darker than the metal, and so is its shadow, so
/// brightness would count both; grey text over a dark panel adds brightness.
fn inkImage(allocator: std.mem.Allocator, frame: []const u8, background: []const u8, label: Label) !Gray {
    const rect = label.rect;
    const pixels = try allocator.alloc(u8, rect.width() * rect.height());
    for (0..rect.height()) |y| for (0..rect.width()) |x| {
        const at = ((rect.y1 + y) * width + rect.x1 + x) * 4;
        const added = switch (label.ink) {
            .bright => luma(frame[at..][0..3]) - luma(background[at..][0..3]),
            .dark => luma(background[at..][0..3]) - luma(frame[at..][0..3]),
            .gold => warmth(frame[at..][0..3]) - warmth(background[at..][0..3]),
        };
        pixels[y * rect.width() + x] = @intCast(std.math.clamp(added, 0, 255));
    };
    return .{ .width = rect.width(), .height = rect.height(), .pixels = pixels };
}

fn luma(rgb: []const u8) i32 {
    return @intCast((@as(u32, rgb[0]) * 299 + @as(u32, rgb[1]) * 587 + @as(u32, rgb[2]) * 114) / 1000);
}

fn warmth(rgb: []const u8) i32 {
    return @as(i32, rgb[0]) - rgb[2];
}

/// The overlay's text for `key` as UTF-8.
fn overlayText(io: std.Io, allocator: std.mem.Allocator, code: []const u8, label: Label) ![]u8 {
    const path = try std.fmt.allocPrint(allocator, "Localization/game/{s}/game_data_base/{s}/{s}.txt", .{ code, label.dir, label.key });
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(1 << 16));
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return error.NotUtf16;
    const units = try allocator.alloc(u16, (bytes.len - 2) / 2);
    for (units, 0..) |*unit, at| unit.* = std.mem.readInt(u16, bytes[2 + at * 2 ..][0..2], .little);
    return std.unicode.utf16LeToUtf8Alloc(allocator, units);
}

fn hasHan(text: []const u8) bool {
    var view = std.unicode.Utf8View.initUnchecked(text);
    var it = view.iterator();
    while (it.nextCodepoint()) |cp| if (cp >= 0x4e00 and cp <= 0x9fff) return true;
    return false;
}

fn hasCodepoint(text: []const u8, wanted: u21) bool {
    var view = std.unicode.Utf8View.initUnchecked(text);
    var it = view.iterator();
    while (it.nextCodepoint()) |cp| if (cp == wanted) return true;
    return false;
}

/// What `checkLabels` measured, for the checks that go on from it.
const Measured = struct {
    hb_folder: []const u8,
    texts: []const []u8,
    refs: []const Gray,
    inks: []const Gray,
};

fn matchLabel(allocator: std.mem.Allocator, label: Label, ink: Gray, reference: Gray) !measure.Match {
    return if (label.small) measure.matchReferenceBlurred(allocator, ink, reference, 64, small_blur) else measure.matchReference(allocator, ink, reference, 64);
}

fn checkLabels(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, set: []const Label, report: *Report) !Measured {
    const hb_folder = try std.fs.path.join(allocator, &.{ folder, "reference" });
    try std.Io.Dir.cwd().createDirPath(io, hb_folder);
    const texts = try allocator.alloc([]u8, set.len);
    const refs = try allocator.alloc(Gray, set.len);
    const inks = try allocator.alloc(Gray, set.len);
    for (set, 0..) |label, index| {
        texts[index] = try overlayText(io, allocator, language.code, label);
        refs[index] = try measure.hbView(allocator, io, hb_folder, try label.stem(allocator), texts[index], .{ .font = language.face.?, .language = language.tag, .script = language.script });
        const full = try inkImage(allocator, frame, blank, label);
        const box = measure.inkBox(full, ink_threshold) orelse {
            try report.check(false, "{s} {s}: no ink in the label's box (the Game drew nothing there)", .{ language.code, label.key });
            inks[index] = full;
            continue;
        };
        inks[index] = try measure.crop(allocator, full, box);
    }

    for (set, 0..) |label, index| {
        const ink = inks[index];
        if (ink.width == full_width(label)) continue; // no ink: reported above
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

    return .{ .hb_folder = hb_folder, .texts = texts, .refs = refs, .inks = inks };
}

fn checkLanguage(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, english: ?[]const u8, report: *Report) !void {
    if (language.face == null) return checkBitmap(allocator, language, frame, blank, english, &labels, report);
    const m = try checkLabels(io, allocator, language, folder, frame, blank, &labels, report);
    if (std.mem.eql(u8, language.code, "hi")) try checkHindi(io, allocator, language, m.hb_folder, m.texts, m.refs, m.inks, report);
    if (std.mem.eql(u8, language.code, "zh-Hans") or std.mem.eql(u8, language.code, "ja")) try checkHan(io, allocator, language, m.hb_folder, m.texts, m.inks, report);
}

/// The briefing's labels as hb-view draws them, and ink in the description's
/// body.
fn checkBriefing(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, english: ?[]const u8, report: *Report) !void {
    if (language.face == null) {
        try checkBitmap(allocator, language, frame, blank, english, &briefing_labels, report);
    } else {
        _ = try checkLabels(io, allocator, language, folder, frame, blank, &briefing_labels, report);
    }
    const body = try inkImage(allocator, frame, blank, briefing_body);
    const box = measure.inkBox(body, ink_threshold);
    try report.check(box != null and box.?.width() >= min_body_width, "{s} briefing description: ink {d} pixels wide in the panel's text lines (at least {d})", .{ language.code, if (box) |b| b.width() else 0, min_body_width });
}

/// The bitmap-font languages (and English): no hb-view reference exists for the
/// game's own fonts, so every label's box holds ink and is wide enough, no
/// glyph is the solid box of a missing code point, and, for a translation, all
/// the labels but one are drawn differently from English.
fn checkBitmap(allocator: std.mem.Allocator, language: Language, frame: []const u8, blank: []const u8, english: ?[]const u8, set: []const Label, report: *Report) !void {
    var different: usize = 0;
    for (set) |label| {
        const full = try inkImage(allocator, frame, blank, label);
        var lit: usize = 0;
        for (full.pixels) |p| lit += @intFromBool(p > ink_threshold);
        const box = measure.inkBox(full, ink_threshold);
        const box_width = if (box) |b| b.width() else 0;
        try report.check(lit >= min_bitmap_ink_pixels and box_width >= min_bitmap_width, "{s} {s}: {d} ink pixels, ink {d} pixels wide (at least {d} and {d})", .{ language.code, label.key, lit, box_width, min_bitmap_ink_pixels, min_bitmap_width });
        try report.check(!measure.hasSolidBox(full, ink_threshold, solid_w, solid_h, solid_fill), "{s} {s}: no missing-glyph box ({d}x{d} window, {d:.0}% lit) in the label", .{ language.code, label.key, solid_w, solid_h, solid_fill * 100 });
        if (english) |reference| {
            const changed = measure.changedPixels(frame, reference, width, label.rect, frame_tolerance);
            if (language.mod != null) {
                different += @intFromBool(changed >= min_text_change);
                try report.line("      {s} {s}: {d} pixels differ from English", .{ language.code, label.key, changed });
            }
        }
    }
    if (english != null and language.mod != null) try report.check(different + 1 >= set.len, "{s}: {d} of {d} labels are drawn differently from English (all but one, at least {d} pixels each)", .{ language.code, different, set.len, min_text_change });
}

fn full_width(label: Label) usize {
    return label.rect.width();
}

fn checkHindi(io: std.Io, allocator: std.mem.Allocator, language: Language, hb_folder: []const u8, texts: []const []u8, refs: []const Gray, inks: []const Gray, report: *Report) !void {
    for (labels, 0..) |label, index| {
        const ink = inks[index];
        // The headline of every word: continuous.
        const split = try measure.words(allocator, ink, ink_threshold, 3);
        for (split, 0..) |word, n| {
            const head = measure.headline(ink, word, headline_threshold);
            try report.check(head.longest_gap <= max_headline_gap, "hi {s} word {d} (columns {d}..{d}): headline row {d} has a gap of {d} columns (at most {d}, D058)", .{ label.key, n, word.x1, word.x2, head.row, head.longest_gap, max_headline_gap });
        }
        // Conjuncts: the word drawn as hb-view's conjunct, not the halant
        // spelling, measured on the conjunct's own columns.
        if (hasCodepoint(texts[index], 0x094d)) {
            const apart = try measure.hbView(allocator, io, hb_folder, try std.mem.concat(allocator, u8, &.{ label.key, "-halant" }), texts[index], .{ .font = language.face.?, .language = language.tag, .script = language.script, .features = measure.halant_spelling });
            const joined_cut = try measure.crop(allocator, refs[index], measure.inkBox(refs[index], 64).?);
            const apart_cut = try measure.crop(allocator, apart, measure.inkBox(apart, 64).?);
            const text_words = std.mem.count(u8, texts[index], " ") + 1;
            // Cut at the text's own word count (measure.wordsN), not at a fixed gap.
            const frame_words = try measure.wordsN(allocator, ink, ink_threshold, text_words);
            const joined_words = try measure.wordsN(allocator, joined_cut, 64, text_words);
            const apart_words = try measure.wordsN(allocator, apart_cut, 64, text_words);
            if (frame_words == null or joined_words == null or apart_words == null) {
                try report.check(false, "hi {s}: conjuncts: {d} words in the text, but the frame, the reference or the halant spelling has fewer gaps", .{ label.key, text_words });
            } else {
                var at: usize = 0;
                var words_in = std.mem.splitScalar(u8, texts[index], ' ');
                while (words_in.next()) |text_word| : (at += 1) {
                    if (!hasCodepoint(text_word, 0x094d)) continue;
                    const c = try measure.conjunctMatch(allocator, ink, frame_words.?[at], joined_cut, joined_words.?[at], apart_cut, apart_words.?[at], 2 * 255);
                    try report.check(c.shaped - c.spelled >= measure.conjunct_margin, "hi {s} word {d}: conjunct columns correlate {d:.3} with the shaped reference, {d:.3} with the halant spelling (margin at least {d:.2})", .{ label.key, at, c.shaped, c.spelled, measure.conjunct_margin });
                }
            }
        }
        // The short i matra: its bar lies where hb-view puts it, in the first
        // word that has the matra.
        if (hasCodepoint(texts[index], 0x093f)) {
            const ref_box = measure.inkBox(refs[index], 64).?;
            const ref_cut = try measure.crop(allocator, refs[index], ref_box);
            const scaled = try measure.resample(allocator, ref_cut, ink.width, ink.height);
            const text_words = std.mem.count(u8, texts[index], " ") + 1;
            const frame_words_or = try measure.wordsN(allocator, ink, ink_threshold, text_words);
            const ref_words_or = try measure.wordsN(allocator, scaled, 64, text_words);
            if (frame_words_or == null or ref_words_or == null) {
                try report.check(false, "hi {s}: {d} words in the text, but the frame or the reference has fewer gaps", .{ label.key, text_words });
                continue;
            }
            const frame_words = frame_words_or.?;
            const ref_words = ref_words_or.?;
            var at: usize = 0;
            var words_in = std.mem.splitScalar(u8, texts[index], ' ');
            const n = while (words_in.next()) |text_word| : (at += 1) {
                if (hasCodepoint(text_word, 0x093f)) break at;
            } else unreachable;
            // The word's first third (the matra and its consonant) is more like
            // the reference's first third than like its middle or its last.
            const mine = measure.window(frame_words[n], 0, 0.3);
            const own = try measure.correlationIn(allocator, ink, scaled, mine, measure.window(ref_words[n], 0, 0.3));
            const middle = try measure.correlationIn(allocator, ink, scaled, mine, measure.window(ref_words[n], 0.35, 0.65));
            const end = try measure.correlationIn(allocator, ink, scaled, mine, measure.window(ref_words[n], 0.7, 1));
            const other = @max(middle, end);
            try report.check(own - other >= min_matra_margin and own >= min_matra_correlation, "hi {s} word {d}: the short i matra's end of the word correlates {d:.3} with hb-view's, {d:.3} with its middle and {d:.3} with its other end (at least {d:.2}, a margin of {d:.2})", .{ label.key, n, own, middle, end, min_matra_correlation, min_matra_margin });
        }
    }
}

fn checkHan(io: std.Io, allocator: std.mem.Allocator, language: Language, hb_folder: []const u8, texts: []const []u8, inks: []const Gray, report: *Report) !void {
    const sc_face = "vendor/fonts/NotoSansSC-Regular.otf";
    const jp_face = "vendor/fonts/NotoSansJP-Regular.otf";
    const own_is_sc = std.mem.eql(u8, language.code, "zh-Hans");
    const own_name = if (own_is_sc) "Simplified Chinese" else "Japanese";
    // Whole labels barely tell the faces apart (most strokes are alike), so the
    // verdict is taken per Han character: the CJK faces advance one em per
    // character, so the label's ink box divides into equal cells, one for each
    // code point, in the frame and in both references alike.
    var informative: usize = 0;
    var margin_sum: f64 = 0;
    var won: usize = 0;
    for (labels, 0..) |label, index| {
        if (!hasHan(texts[index])) continue;
        const ink = inks[index];
        const sc = try measure.hbView(allocator, io, hb_folder, try std.mem.concat(allocator, u8, &.{ label.key, "-sc" }), texts[index], .{ .font = sc_face, .language = "zh-Hans" });
        const jp = try measure.hbView(allocator, io, hb_folder, try std.mem.concat(allocator, u8, &.{ label.key, "-jp" }), texts[index], .{ .font = jp_face, .language = "ja" });
        const sc_scaled = try onGrid(allocator, sc, ink);
        const jp_scaled = try onGrid(allocator, jp, ink);
        var view = std.unicode.Utf8View.initUnchecked(texts[index]);
        var count: usize = 0;
        var counter = view.iterator();
        while (counter.nextCodepoint()) |_| count += 1;
        var it = view.iterator();
        var at: usize = 0;
        while (it.nextCodepoint()) |cp| : (at += 1) {
            if (cp < 0x4e00 or cp > 0x9fff) continue;
            const cell = Rect{ .x1 = ink.width * at / count, .y1 = 0, .x2 = ink.width * (at + 1) / count, .y2 = ink.height };
            const with_sc = try measure.correlationIn(allocator, ink, sc_scaled, cell, cell);
            const with_jp = try measure.correlationIn(allocator, ink, jp_scaled, cell, cell);
            const faces = try measure.correlationIn(allocator, sc_scaled, jp_scaled, cell, cell);
            const margin = if (own_is_sc) with_sc - with_jp else with_jp - with_sc;
            var utf8: [4]u8 = undefined;
            const len = try std.unicode.utf8Encode(cp, &utf8);
            try report.line("      {s} {s} {s}: SC {d:.3}, JP {d:.3} ({s} by {d:.3}); the faces' cells correlate {d:.3}", .{ language.code, label.key, utf8[0..len], with_sc, with_jp, if (own_is_sc) "SC" else "JP", margin, faces });
            if (faces >= han_informative_below) continue;
            informative += 1;
            margin_sum += margin;
            won += @intFromBool(margin > 0);
        }
    }
    try report.check(informative >= 3, "{s}: {d} Han characters are drawn differently by the two faces (at least 3)", .{ language.code, informative });
    const mean = margin_sum / @as(f64, @floatFromInt(@max(informative, 1)));
    try report.check(mean >= min_face_margin, "{s}: over those characters the {s} face wins by {d:.3} on average (at least {d:.2}), in {d} of {d}", .{ language.code, own_name, mean, min_face_margin, won, informative });
    try report.check(won * 3 >= informative * 2, "{s}: the {s} face is nearer in {d} of {d} of those characters (at least two in three)", .{ language.code, own_name, won, informative });
}

/// A reference raster cut to its ink and resampled onto the grid of a frame's label ink.
fn onGrid(allocator: std.mem.Allocator, reference: Gray, ink: Gray) !Gray {
    const cut = try measure.crop(allocator, reference, measure.inkBox(reference, 64).?);
    return measure.resample(allocator, cut, ink.width, ink.height);
}

/// One scratch profile of the Game: `<root>/<name>/user` is XDG_DATA_HOME,
/// BK_USER_ROOT and the working directory.
const Run = struct {
    io: std.Io,
    game: []const u8,
    folder: []const u8,
    user: []const u8,
    mod: ?[]const u8,
    name: []const u8,
    /// Whether the language's mod brings fonts, so the menu capture must show the Game
    /// opening them.
    fonts: bool,

    /// Makes the scratch folder and sets the profile's language option to
    /// `mod` (Default for the base game) with a short run of the Game on the
    /// options' own `set=`, which the profile keeps.
    fn init(io: std.Io, allocator: std.mem.Allocator, game: []const u8, game_root: []const u8, root: []const u8, name: []const u8, mod: ?[]const u8, fonts: bool) !Run {
        const cwd = std.Io.Dir.cwd();
        const folder = try std.fs.path.join(allocator, &.{ root, name });
        const user = try std.fs.path.join(allocator, &.{ folder, "user" });
        // defconf.cfg, the options' declarations, is looked for under the working directory.
        const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
        try cwd.createDirPath(io, configs);
        const defconf = try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }), .data = try cwd.readFileAlloc(io, defconf, allocator, .limited(1 << 20)) });
        const run = Run{ .io = io, .game = game, .folder = folder, .user = user, .mod = mod, .name = name, .fonts = fonts };
        const schedule = try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{mod orelse "Default"});
        const log = try run.play(allocator, &.{ "-windowed", "-profile=L10n" }, schedule, null, "choose");
        if (mod) |one| try expectLine(log, run.folder, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{one}));
        return run;
    }

    fn play(self: Run, allocator: std.mem.Allocator, args: []const []const u8, schedule: ?[]const u8, delay: ?u32, tag: []const u8) ![]u8 {
        var environ = try testing.environ.createMap(allocator);
        try environ.put("XDG_DATA_HOME", self.user);
        try environ.put("BK_USER_ROOT", self.user);
        try environ.put("BK_MEM_REPORT", "count");
        try environ.put("BK_DEBUG_LOG", "1");
        try environ.put("BK_MOD_TRACE", "1");
        try environ.put("BK_UI_TRACE", "1");
        try environ.put("BK_NO_HELP", "1");
        if (schedule) |text| try environ.put("BK_AUTO_UI", text);
        if (delay) |frames| try environ.put("BK_REFERENCE_DELAY", try std.fmt.allocPrint(allocator, "{d}", .{frames}));
        var argv: std.ArrayList([]const u8) = .empty;
        try argv.append(allocator, self.game);
        try argv.appendSlice(allocator, args);
        const result = try std.process.run(allocator, self.io, .{
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
        return log;
    }

    /// The main-menu capture (RGBA, alpha zero), optionally under a BK_AUTO_UI
    /// schedule and with the capture held back to `delay` frames.
    fn capture(self: Run, allocator: std.mem.Allocator, schedule: ?[]const u8, delay: ?u32) ![]u8 {
        const log = try self.scene(allocator, schedule, delay, "capture");
        try expectLine(log, self.folder, "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
        // The fonts come from the language mod: both sizes the menu uses, tfd and atlas.
        if (self.fonts) {
            for ([_][]const u8{ "Fonts\\medium\\1.tfd", "Fonts\\medium\\1_h.dds", "Fonts\\large\\1.tfd", "Fonts\\large\\1_h.dds" }) |file| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"{s}\" from LANGUAGE", .{file});
                try expectLine(log, self.folder, line);
            }
        }
        return self.frame(allocator, "capture");
    }

    /// The first mission's briefing capture. The log must show the schedule's
    /// mission action reaching the screen, and, for a language that is not the
    /// blank one (`keys` set), the Game opening every text key of the briefing
    /// from the mod.
    fn briefing(self: Run, allocator: std.mem.Allocator, keys: bool) ![]u8 {
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

    /// One run of the Game to its `-reference-scene` capture, `<tag>.rgba`.
    fn scene(self: Run, allocator: std.mem.Allocator, schedule: ?[]const u8, delay: ?u32, tag: []const u8) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.rgba", .{ self.folder, tag });
        const log = try self.play(allocator, &.{ "-reference-scene", path, "-reference-resolution", "1024", "768", "-windowed", "-profile=L10n" }, schedule, delay, tag);
        try expectLine(log, self.folder, "BK_REFERENCE_SCENE: capture complete");
        return log;
    }

    fn frame(self: Run, allocator: std.mem.Allocator, tag: []const u8) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.rgba", .{ self.folder, tag });
        const bytes = try std.Io.Dir.cwd().readFileAlloc(self.io, path, allocator, .limited(16 << 20));
        try testing.expectEqual(@as(usize, width * height * 4), bytes.len);
        return bytes;
    }
};

fn expectLine(log: []const u8, folder: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its logs are in {s}\n", .{ line, folder });
    return error.GameLineMissing;
}

/// `l10n-blank`: the shipped English texts and the ru fonts with their ink
/// removed (every texel of the atlas zero, the header kept), installed as a
/// mod of the game so the Game's background capture needs nothing but the
/// language option.
fn installBlankMod(io: std.Io, allocator: std.mem.Allocator, game_root: []const u8, root: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const source = try std.fs.path.join(allocator, &.{ game_root, "mods", "lang-ru", "data", "texts.pak" });
    const bytes = try cwd.readFileAlloc(io, source, allocator, .limited(256 << 20));
    var archive = try zip.Archive.parse(testing.allocator, bytes);
    defer archive.deinit();
    var fonts: std.ArrayList(packages.Entry) = .empty;
    for ([_][]const u8{ "tiny", "small", "medium", "large" }) |size| for ([_][]const u8{ "1.tfd", "1_h.dds" }) |file| {
        const inside = try std.fmt.allocPrint(allocator, "fonts/{s}/{s}", .{ size, file });
        const entry = archive.find(inside) orelse return error.FontMissing;
        const data = try archive.extract(allocator, entry);
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
    const mod_xml = "<?xml version=\"1.0\"?>\r\n<base><MODName>l10n blank</MODName><MODVersion>1.0</MODVersion><MODType>language</MODType><MODDesc></MODDesc></base>\r\n";
    try database.writeFileAtomic(io, testing.allocator, try std.fs.path.join(allocator, &.{ std.fs.path.dirname(written.path).?, "mod.xml" }), mod_xml);
}

/// An uncompressed 32-bit top-down TGA of an RGBA frame, the frames the tier
/// keeps next to its report.
fn writeTga(io: std.Io, allocator: std.mem.Allocator, path: []const u8, rgba: []const u8) !void {
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
