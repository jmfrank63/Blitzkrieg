//! M002 R056/D066 on the real Game, on bk_text since M004 S05: the ten languages (English and the nine
//! language mods `language-mods` built, which ship texts only) draw a translated main menu, the first
//! mission's briefing, an options screen with a tooltip and a unit's encyclopedia page through the Game's
//! default text engine. Every language is measured against hb-view of the face bk_text draws it with
//! (`Language.face`): Liberation Serif for the Latin languages and Russian, the Noto face of the script
//! for zh-Hans, ja and hi. Every run of the Game sets BK_GLYPH_TRACE and fails on any line of it
//! (the character a font has no glyph for), and every run but the blank background's must log
//! `bk_text: engine ttf`, so a Game that fell back to the bitmap fonts cannot pass.
//!
//! Per language the Game runs on a scratch profile whose language option is the mod (the option's own
//! `set=`, which the profile keeps), then to its `-reference-scene` capture of the main menu at 1024x768.
//! The background is a capture of a language mod, `text-engine-blank`, that carries the shipped fonts with
//! every glyph's ink removed and the shipped English texts: a size whose fonts come from a mod stays on
//! its bitmap font (M004 S05 T01), so that capture is the menu with no text, and the frame minus it is the
//! ink of the labels and of nothing else. For the header and the five buttons the ink is compared with
//! hb-view's raster of the same string from the same face (the texts are read from the tracked overlay in
//! real Unicode): the normalised correlation must clear a threshold and the label's own reference must
//! beat every other label's by a margin. Then the checks the languages need:
//!
//! - hi: the headline of every word has no gap (D058), the short i matra of मित्रों lies where hb-view
//!   puts it, and the conjuncts (त्र, ख्य) are drawn as hb-view's conjunct and not as the halant spelling;
//! - zh-Hans and ja: where the Simplified and the Japanese face draw a label differently, the frame is
//!   nearer the language's own face;
//! - ja: the lines of a wrapped text are the lines bk_text's wrap predicts and break no kinsoku rule.
//!
//! The first mission of each campaign's first chapter (`briefings`: Allies/Norway 1, German/Poland 1,
//! USSR/Finland 1, historical missions, so their text keys are fixed and no random map enters the frame)
//! is captured the same way, each against its own blank background: the title, the three panel headers
//! and the first visible objective are compared with hb-view like the menu's labels where the language's
//! scope covers the mission's keys (D109; otherwise ink only), and the description body must hold ink.
//! The report names the campaign, the mission and which of the two applies. BK_L10N_LANGS (a comma list of codes, default all) picks the
//! languages, so a run can be sharded. A language whose mod is not built (lang-ja while M003 is work in
//! progress) reports `not run: lang-ja absent (M003 WIP)` and the list of native names tolerates exactly
//! that absence.
//!
//! Two routes widen the pair (M003 S01 T03): an options screen with a hovered tooltip (the settings
//! screen, the cursor parked on its restore-defaults button until the tooltip's dwell has passed) and a
//! unit's encyclopedia page (the Allied Churchill: title, the two panel headers and the description; a second page for the reworded Cromwell).
//! Each is captured against a blank background of its own and measured for every language: ink inside
//! each label's box. Only a language whose scope (`Localization/game/scope.txt`, read through
//! l10n-check's own parser) covers the route's keys is measured further, since a seed-scope language
//! still shows English there: its static labels are compared with hb-view like the menu's.
//!
//! Three more screens hold texts the ELK database lacks, which the language mods carried only from
//! 2026-10-24 on (language-mods adds an original for them): the main menu's Options page (Load Mod),
//! the gameplay mods screen (its caption and Detach All MODs) and, in the first Allied mission, the
//! escape menu (Save Game, Load Game, Help Screen) and its End Mission page (Finish Mission). The two
//! menu screens are routes; the mission is one windowed run whose `shot`s wait on game and timer time.
//! Where the scope covers them, each static label must also match its own string better than the
//! English one (`checkEnglish`), so a screen left in English fails. They are written as
//! `mainmenu-options.png`, `mods.png`, `escape.png` and `escape-end.png`.
//!
//! Last, per language the Game starts on another language (ru, or English for ru) and switches to this
//! one while it runs (the font cache clear of ApplyLanguage, T03): the buttons must show the cold start's
//! frame. Then the choice is made through the option (`set=`), the Game exits, and a restart on the
//! profile, without any mod argument, must show the cold start's frame again (header included).
//!
//! Frames, references and `report.txt` are kept in `zig-out/local-test/l10n-game-reads-it/<code>/` (a
//! shard is a code, so each has a folder of its own): `menu.tga`, `briefing.tga` (and `briefing-german.tga`, `briefing-ussr.tga`), `switch.tga`,
//! `restart.tga`, and the two routes' `options.png` and `encyclopedia.png`; the report names the hb-view
//! correlation of every line. The user's data is never touched: XDG_DATA_HOME and BK_USER_ROOT are the
//! scratch folder and the Game runs from it. BK_L10N_GAME names the installed Game executable;
//! `zig build l10n-game-reads-it` sets it and builds the mods first. The helpers the text-engine tier
//! (text_engine_game_reads_it.zig) also uses are imported from it.
const std = @import("std");
const measure = @import("l10n_measure");
const check = @import("l10n_check");
const te = @import("text_engine_reads");
const testing = std.testing;

const Rect = measure.Rect;
const Gray = measure.Gray;
const Label = te.Label;
const Report = te.Report;
const envValue = te.envValue;
const inkImage = te.inkImage;
const expectLine = te.expectLine;
const installBlankMod = te.installBlankMod;
const writeTga = te.writeTga;
const width = te.width;
const height = te.height;
const ink_threshold = te.ink_threshold;
const blank_mod = te.blank_mod;

const scratch = "zig-out/local-test/l10n-game-reads-it";
/// The profile every run of this tier uses.
const profile_arg = "-profile=L10n";
/// The faces bk_text draws with, staged beside the Game and read by the kinsoku check.
const fonts_dir = "vendor/fonts";

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
/// The small font's light-on-dark box is found from fainter ink: an accent over a capital
/// ("MISIÓN") is a single pale row at that size, which `ink_threshold` drops while hb-view's raster
/// keeps it, so the box lost the accent's height and the proportion with it. The dark headers on
/// light metal keep `ink_threshold`: their embossing leaves a faint halo this would take in.
const small_box_threshold: u8 = 8;
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

/// The header (element 20001, FontSize 2) and the five buttons (FontSize 1) on
/// the 1024x768 canvas: the buttons are 320x50 at x 650, y 267 + 65 * n; the
/// header lies in the panel above them.
const labels = [_]Label{
    .{ .key = "imheader-mainmenu-header", .rect = .{ .x1 = 630, .y1 = 175, .x2 = 990, .y2 = 248 }, .ink = .bright },
    .{ .key = "imheader-gbutton-newgame", .rect = .{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 } },
    .{ .key = "imheader-gbutton-multiplayer", .rect = .{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 } },
    .{ .key = "imheader-gbutton-loadgame", .rect = .{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 } },
    .{ .key = "imheader-gbutton-options", .rect = .{ .x1 = 650, .y1 = 462, .x2 = 970, .y2 = 512 } },
    .{ .key = "imheader-gbutton-exitgame", .rect = .{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 } },
};

/// The briefing of a campaign's first mission as the Game draws it at 1024x768 (boxes measured on a
/// captured English frame of the Allied one; the three campaigns share the ui\common\mission screen,
/// so only the text keys' folder differs): the title in the top bar, the headers of the three panels,
/// and the first objective, whose "+" icon lies left of x 72. The title is gold; the panel texts are
/// grey on metal of different lightness, so each label takes the ink mode that its panel's text shows
/// in (the objectives header and the objective are dark on light metal, the other two panel headers
/// light on dark). The description panel's body wraps, so it is only checked for ink (`body`).
const Briefing = struct {
    /// The campaign's name in the report, and the suffix of its frames (`briefing-<tag>`).
    tag: []const u8,
    /// The file stem of its frames and logs: the Allied one keeps `briefing`, which the report readers know.
    file: []const u8,
    /// UIState.cpp's campaign index (IMC_GERMAN_CAMPAIGN 0, USSR 1, Allies 2).
    index: u8,
    campaign: []const u8,
    chapter: []const u8,
    mission: []const u8,
    /// Where the mission's texts live below game_data_base.
    dir: []const u8,
    /// The first objective the briefing lists: the key of its text (a hidden objective, like the USSR
    /// mission's `0h`, is not drawn).
    objective: []const u8,
    /// The schedule's last action as the Game echoes it: it must be in the log, or the schedule
    /// stopped short of the mission.
    action: []const u8,
    labels: [5]Label,
    body: Label,

    /// BK_AUTO_UI: the campaign, its first chapter, then the chapter's first mission. The chapter
    /// screen resets the finish status, so it is cleared before the mission is named; the briefing
    /// is up by frame 360.
    fn schedule(self: Briefing, allocator: std.mem.Allocator) ![]u8 {
        return std.fmt.allocPrint(allocator, "60:campaign={d}={s},120:chapter={s},240:var=Mission.Last.FinishStatus=-1,300:mission={s}", .{ self.index, self.campaign, self.chapter, self.mission });
    }
};

fn briefingOf(comptime tag: []const u8, comptime file: []const u8, comptime index: u8, comptime campaign: []const u8, comptime chapter: []const u8, comptime mission: []const u8, comptime dir: []const u8, comptime objective: []const u8) Briefing {
    return .{
        .tag = tag,
        .file = file,
        .index = index,
        .campaign = campaign,
        .chapter = chapter,
        .mission = mission,
        .dir = dir,
        .objective = objective,
        .action = "BK_AUTO_UI: frame 300 action mission=" ++ mission,
        .labels = .{
            .{ .key = "header", .dir = dir, .rect = .{ .x1 = 50, .y1 = 40, .x2 = 500, .y2 = 95 } },
            .{ .key = "imheader-mission-objectives", .rect = .{ .x1 = 55, .y1 = 152, .x2 = 440, .y2 = 178 }, .ink = .dark, .small = true },
            .{ .key = "imheader-mission-map", .rect = .{ .x1 = 555, .y1 = 152, .x2 = 980, .y2 = 178 }, .ink = .bright, .small = true },
            .{ .key = "imheader-mission-desc", .rect = .{ .x1 = 555, .y1 = 433, .x2 = 940, .y2 = 458 }, .ink = .bright, .small = true },
            .{ .key = objective, .dir = dir, .rect = .{ .x1 = 72, .y1 = 200, .x2 = 440, .y2 = 228 }, .ink = .dark },
        },
        // The description panel's text lines: "Mission: ...", "Type: ..." and the start of the description.
        .body = .{ .key = "description", .dir = dir, .rect = .{ .x1 = 555, .y1 = 485, .x2 = 935, .y2 = 625 }, .ink = .dark },
    };
}

/// The first mission of each campaign's first chapter (a historical mission, so its text keys are
/// fixed and no random map enters the frame). `campaign_table_test` holds the table to Data/.
const briefings = [_]Briefing{
    briefingOf("allies", "briefing", 2, "scenarios\\campaigns\\allies\\allies", "scenarios\\chapters\\allies\\norway\\1", "scenarios\\scenariomissions\\allies\\norway\\1", "scenarios/scenariomissions/allies/norway", "0h"),
    briefingOf("german", "briefing-german", 0, "scenarios\\campaigns\\german\\german", "scenarios\\chapters\\german\\poland\\1", "scenarios\\scenariomissions\\german\\poland\\1", "scenarios/scenariomissions/german/poland", "0h"),
    briefingOf("ussr", "briefing-ussr", 1, "scenarios\\campaigns\\ussr\\ussr", "scenarios\\chapters\\ussr\\finland\\1", "scenarios\\scenariomissions\\ussr\\finland\\1", "scenarios/scenariomissions/ussr/finland", "1h"),
};
/// The text keys the briefing opens besides the labels' own (the "Mission:",
/// "Type:" and "Historical" lines of the description panel and its caption),
/// as BK_MOD_TRACE shows them.
const briefing_extra_keys = [_][]const u8{ "missiontext", "missiontypetext", "scenariomission", "missiondescriptiontext" };
const briefing_delay: u32 = 420;

/// A screen the Game is driven to under a BK_AUTO_UI schedule and captured in: its labels, and
/// optionally a wrapped body that is only checked for ink.
const Route = struct {
    tag: []const u8,
    schedule: []const u8,
    delay: u32,
    /// The schedule's last action as the Game echoes it, and the screen's reposition trace
    /// line: both must be in the log, or the schedule stopped short of the screen.
    action: []const u8,
    screen: []const u8,
    labels: []const Label,
    body: ?Label = null,
    /// The scroll text that shows `body`, for the kinsoku line check of a language that breaks
    /// between characters.
    scroll: ?ScrollBox = null,
    /// Where the language's scope covers the route, each static label must also match its own
    /// string better than the English one (`checkEnglish`): the screens whose texts stayed English
    /// in the language mods until 2026-10-24.
    english: bool = false,
};

/// A UI_SCROLL_TEXT's text area, from its layout: the element's position plus LeftSpace and
/// TopSpace is where the text starts, the wrap breaks at its width less the scroll bar and both
/// spaces, and the game clips at its lower edge less BottomSpace. The font is the medium one
/// (FontSize 1 or the default) in both screens that use it.
const ScrollBox = struct {
    x: usize,
    y: usize,
    wrap_width: usize,
    bottom: usize,
};
/// Data/UI/common/Mission.xml element 2000: at 536,412, 452 x 229, spaces 20, 20, 71 and 15, a
/// scroll bar 45 wide.
const briefing_box = ScrollBox{ .x = 556, .y = 483, .wrap_width = 367, .bottom = 626 };
/// Data/UI/Encyclopedia.xml element 2000: at 38,130, 462 x 511, the same spaces and scroll bar.
const encyclopedia_box = ScrollBox{ .x = 58, .y = 201, .wrap_width = 377, .bottom = 626 };

/// The settings screen with the cursor parked on its restore-defaults button (the arrow at the
/// bottom left of the 1024x768 canvas, ElementID 10003 of Data/UI/OptionsSettings.xml, whose
/// ToolTip is tt_button_default). Options > Settings is `msg=10003` on the options menu. A
/// reference-scene capture skips the screen's StepLocal (InterfaceScreenBase.cpp), which is
/// where the cursor's position reaches the tooltip, so the schedule lifts X64.ReferenceScene
/// once the screen is up. The hover waits past the chat lines the video page writes while it
/// builds, which would otherwise draw over the caption, and the capture comes after the
/// tooltip's one second dwell. The tooltip box (about 405x54 for the English text) hangs
/// below the cursor, so its box is a strip wider than that (the Russian tooltip is about 585 wide,
/// so the strip ends at 688, inside the tooltip's own border at x 691, and its text rows are 732 to 755; the faint fill below differs from the blank by 26).
const options_dir = "textes/ui/optionssettings";
const options_labels = [_]Label{
    .{ .key = "caption_settings", .dir = options_dir, .rect = .{ .x1 = 55, .y1 = 40, .x2 = 500, .y2 = 95 }, .dynamic = true },
    .{ .key = "tt_button_default", .dir = options_dir, .rect = .{ .x1 = 90, .y1 = 640, .x2 = 1020, .y2 = 768 }, .ink = .yellow, .boxed = true },
};
const options_route = Route{
    .tag = "options",
    .schedule = "60:settings,100:msg=10003,110:var=X64.ReferenceScene=0,400:hover=100x710",
    .delay = 540,
    .action = "BK_AUTO_UI: frame 400 action hover=100x710",
    .screen = "BK_UI_TRACE: reposition \"ui\\OptionsSettings\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    .labels = &options_labels,
};

/// The Allied Churchill's encyclopedia page: MISSION_COMMAND_ENCYCLOPEDIA (iMission.h,
/// MISSION_BASE_VALUE 0x100e0000 + 149 = 269353109) with the configuration `<E_UNIT>;<RPG
/// stats name>` that the total encyclopedia sends when a unit is chosen, which `cmdc=` carries
/// from the main menu. The title (ElementID 20000, FontSize 2) is the unit's `name`, the
/// description (ElementID 2000) its `desc`, wrapped in the left panel beside the scroll bar
/// (x under 445); the headers of the two panels are the strings of Data/UI/Encyclopedia.xml.
/// The picture is the same in every language, so the blank background cancels it.
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
    .scroll = encyclopedia_box,
};
/// The Allied Cromwell's encyclopedia page, the same route as the Churchill's for a unit whose ru name
/// M004 S06 reworded to fit the one-line unit-info box. The title box here (55..900 x 40..95, large
/// font) is wider than the 190x36 unit-info box, so this capture proves only that the reworded name
/// is drawn on bk_text against hb-view, inside its own label box and without a glyph trace. The
/// unit-info fit itself is proved by l10n-check's bk_text fit rule.
const cromwell_dir = "units/technics/allies/tanks/cruiser_mkviii_cromwell_gb";
const cromwell_labels = [_]Label{
    .{ .key = "name", .dir = cromwell_dir, .rect = .{ .x1 = 55, .y1 = 40, .x2 = 900, .y2 = 95 } },
};
const encyclopedia_cromwell_route = Route{
    .tag = "encyclopedia-cromwell",
    .schedule = "60:cmdc=269353109=1;Cruiser_MKVIII_Cromwell_GB",
    .delay = 150,
    .action = "BK_AUTO_UI: frame 60 action cmdc=269353109=1;Cruiser_MKVIII_Cromwell_GB",
    .screen = "BK_UI_TRACE: reposition \"ui\\encyclopedia\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    .labels = &cromwell_labels,
};
/// The main menu's Options page: its Options button (850,487 on the 1024x768 canvas) turns the menu's
/// five buttons, 320x50 at x 650, y 267 + 65 * n as on the first page, into settings, cutscenes,
/// credits, Load Mod and back. Load Mod (imheader-gbutton-mods) is one of the texts the ELK database
/// lacks, which the language mods carried only from 2026-10-24 on. The cursor is parked on the
/// background afterwards, so no button is highlighted.
const menu_options_labels = [_]Label{
    .{ .key = "imheader-gbutton-settings", .rect = .{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 } },
    .{ .key = "imheader-gbutton-cutscenes", .rect = .{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 } },
    .{ .key = "imheader-gbutton-credits", .rect = .{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 } },
    .{ .key = "imheader-gbutton-mods", .rect = .{ .x1 = 650, .y1 = 462, .x2 = 970, .y2 = 512 } },
    .{ .key = "imheader-gbutton-back", .rect = .{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 } },
};
const menu_options_route = Route{
    .tag = "mainmenu-options",
    .schedule = "60:click=850x487,70:hover=300x720",
    .delay = 150,
    .action = "BK_AUTO_UI: frame 70 action hover=300x720",
    .screen = "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    .labels = &menu_options_labels,
    .english = true,
};
/// The gameplay mods screen (MISSION_COMMAND_MODS_LIST, Lists/IMModList.xml; Load Mod opens it): the
/// caption in its top bar (mods/caption, white) and the list's first row, which
/// CInterfaceIMModsList::FillListItem names remove_all_mods (dark on the light list; the language
/// mods are left out of the list). Both are texts the ELK database lacks. The column header
/// (mods/mod_name) holds the caption's text and is not measured, so no two labels share a string.
const mods_dir = "textes/ui/intermission/mainmenu/mods";
const mods_labels = [_]Label{
    .{ .key = "caption", .dir = mods_dir, .rect = .{ .x1 = 300, .y1 = 105, .x2 = 690, .y2 = 150 }, .ink = .bright },
    .{ .key = "remove_all_mods", .dir = mods_dir, .rect = .{ .x1 = 310, .y1 = 206, .x2 = 690, .y2 = 242 }, .ink = .dark },
};
const mods_route = Route{
    .tag = "mods",
    .schedule = "60:cmd=" ++ mods_list_command,
    .delay = 200,
    .action = "BK_AUTO_UI: frame 60 action cmd=" ++ mods_list_command,
    .screen = "BK_UI_TRACE: reposition \"ui\\Lists\\IMModList\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    .labels = &mods_labels,
    .english = true,
};
const routes = [_]Route{ options_route, encyclopedia_route, encyclopedia_cromwell_route, menu_options_route, mods_route };

/// The escape menu of the first Allied mission, which the language mods showed partly in English until
/// 2026-10-24 (Save Game, Load Game, Help Screen: texts the ELK database lacks). The briefing's
/// schedule carried on: `ok` starts the mission, ESC opens the menu 3 s of game time in (the menu
/// pauses the game, so the shots wait on the timer), then a click on End Mission opens its page.
/// A shot, not a reference capture: game time decides when the mission is up. The buttons are 320x34
/// at x 352 in mission.xml's single-player escape dialog, 40 px apart; the root page's first starts
/// at y 248, the End Mission page's at y 288. The page's first button (button_escape_mission_finish)
/// is disabled while the mission runs, so its text is grey; exittowindows names the system the Game
/// runs on, not the overlay's "Windows", so it is measured for ink only.
const escape_dir = "textes/ui/mission/escapemenu";
fn escapeButton(comptime key: []const u8, comptime dir: []const u8, comptime top: usize, comptime ink: te.Ink, comptime dynamic: bool) Label {
    return .{ .key = key, .dir = dir, .rect = .{ .x1 = 356, .y1 = top + 2, .x2 = 662, .y2 = top + 28 }, .ink = ink, .dynamic = dynamic };
}
const strings_dir = "textes/strings";
const escape_labels = [_]Label{
    escapeButton("button_save_game", escape_dir, 248, .gold, false),
    escapeButton("button_load_game", escape_dir, 288, .gold, false),
    escapeButton("options", strings_dir, 328, .gold, false),
    escapeButton("button_help", escape_dir, 368, .gold, false),
    escapeButton("missionobjectives", strings_dir, 408, .gold, false),
    escapeButton("endmission", strings_dir, 448, .gold, false),
    escapeButton("returntogame", strings_dir, 488, .gold, false),
};
const escape_end_labels = [_]Label{
    escapeButton("button_escape_mission_finish", strings_dir, 288, .bright, false),
    escapeButton("restartmission", strings_dir, 328, .gold, false),
    escapeButton("mainmenu", strings_dir, 368, .gold, false),
    escapeButton("exittowindows", strings_dir, 408, .gold, true),
    escapeButton("back", strings_dir, 448, .gold, false),
};
const MissionShot = struct {
    /// The schedule entry whose `shot` writes the frame (`autoshot_<key>_1024x768.rgba`).
    key: []const u8,
    route: Route,
};
const mission_shots = [_]MissionShot{
    .{ .key = "t1500", .route = .{ .tag = "escape", .schedule = "", .delay = 0, .action = " action key=ESC", .screen = "", .labels = &escape_labels, .english = true } },
    .{ .key = "t1600", .route = .{ .tag = "escape-end", .schedule = "", .delay = 0, .action = " action click=510x463", .screen = "", .labels = &escape_end_labels, .english = true } },
};
const mission_tail = ",420:ok,g3000:key=ESC,t1500:shot,t200:click=510x463,t1600:shot,t300:exit";

/// A route's label holds at least this much ink (pixels above `ink_threshold`): the small
/// font's panel headers are the lightest of them.
const min_route_ink_pixels: usize = 60;

const Language = struct {
    code: []const u8,
    /// The mod folder; null is the base game (English), which no mod replaces.
    mod: ?[]const u8,
    /// BCP 47 tag for hb-view.
    tag: []const u8,
    /// The face hb-view draws the reference with: the first face of bk_text's chain that has the
    /// language's glyphs (faces.zig chainFor: Liberation Serif leads for Latin and Cyrillic, so ru
    /// has it too; Noto Sans SC, JP and Devanagari carry the others).
    face: []const u8,
    script: ?[]const u8 = null,
    /// The language breaks lines between characters, so a wrapped text is checked against the
    /// lines the game's wrap predicts (kinsoku): ja now, zh-Hans when its slice measures it.
    kinsoku: bool = false,

    /// The value of the language option that selects it.
    fn option(self: Language) []const u8 {
        return self.mod orelse "Default";
    }
};

const serif = "vendor/fonts/LiberationSerif-Regular.ttf";
const languages = [_]Language{
    .{ .code = "en", .mod = null, .tag = "en", .face = serif },
    .{ .code = "ru", .mod = "lang-ru", .tag = "ru", .face = serif },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .tag = "zh-Hans", .face = "vendor/fonts/NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .tag = "ja", .face = "vendor/fonts/NotoSansJP-Regular.otf", .kinsoku = true },
    .{ .code = "hi", .mod = "lang-hi", .tag = "hi", .face = "vendor/fonts/NotoSansDevanagari-Regular.ttf", .script = "Deva" },
    .{ .code = "es", .mod = "lang-es", .tag = "es", .face = serif },
    .{ .code = "fr", .mod = "lang-fr", .tag = "fr", .face = serif },
    .{ .code = "de", .mod = "lang-de", .tag = "de", .face = serif },
    .{ .code = "pt-BR", .mod = "lang-pt-br", .tag = "pt-BR", .face = serif },
    .{ .code = "id", .mod = "lang-id", .tag = "id", .face = serif },
};
const ru = languages[1];

test "the Game draws the ten languages on bk_text, measured against hb-view (R056, M004 S05)" {
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
    var blank_briefings: [briefings.len][]u8 = undefined;
    for (&briefings, 0..) |*one, at| {
        blank_briefings[at] = try blank_run.briefing(allocator, one, false);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "blank", try std.fmt.allocPrint(allocator, "{s}.tga", .{one.file}) }), blank_briefings[at]);
    }

    var blank_routes: [routes.len][]u8 = undefined;
    for (routes, 0..) |route, at| {
        blank_routes[at] = try blank_run.route(allocator, route, false);
        try writePng(io, allocator, try std.fs.path.join(allocator, &.{ root, "blank", try std.fmt.allocPrint(allocator, "{s}.png", .{route.tag}) }), blank_routes[at]);
    }
    const blank_mission = try blank_run.mission(allocator, false);
    for (mission_shots, blank_mission) |one, frame| try writePng(io, allocator, try std.fs.path.join(allocator, &.{ root, "blank", try std.fmt.allocPrint(allocator, "{s}.png", .{one.route.tag}) }), frame);
    // The keys each language translates: a route is measured against English only where the
    // language's scope holds all of its keys.
    const scopes = (try check.loadScopes(io, allocator, cwd, null)) orelse return error.ScopeMissing;

    var failures: usize = 0;
    // The mods that are not built: only lang-ja may be (M003 WIP), and the tier says so.
    var absent: std.ArrayList([]const u8) = .empty;
    for (languages) |language| if (language.mod) |mod| {
        const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", mod });
        cwd.access(io, mod_folder, .{}) catch {
            if (!std.mem.eql(u8, mod, "lang-ja")) return error.ModMissing;
            try absent.append(allocator, mod);
        };
    };
    languages: for (languages) |language| {
        if (!try selected(allocator, language.code)) continue;
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        cwd.deleteTree(io, try std.fs.path.join(allocator, &.{ scratch, language.code })) catch {};
        if (language.mod) |mod| for (absent.items) |gone| if (std.mem.eql(u8, gone, mod)) {
            const note = try std.fmt.allocPrint(allocator, "not run: {s} absent (M003 WIP)", .{mod});
            std.debug.print("l10n-game-reads-it: {s}\n", .{note});
            try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ scratch, language.code }));
            try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, language.code, "report.txt" }), .data = note });
            continue :languages;
        };
        const run = try Run.init(io, allocator, game, game_root, root, language.code, language.mod, false);
        const menu = try run.capture(allocator, null, null);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "menu.tga" }), menu);

        try report.line("== {s} ({s}) main menu", .{ language.code, language.mod orelse "base game" });
        try checkLanguage(io, allocator, language, run.folder, menu, blank, &report);

        // The first mission's briefing of each campaign. Compared with hb-view where the language's
        // scope holds the mission's keys (the base game and a whole-database language always do),
        // otherwise measured for ink only: the Game shows English there (D109).
        for (&briefings, 0..) |*one, at| {
            const covered = language.mod == null or scopeCovers(scopes, language.code, briefingRoute(one));
            const shot = try run.briefing(allocator, one, language.mod != null and covered);
            try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, try std.fmt.allocPrint(allocator, "{s}.tga", .{one.file}) }), shot);
            try report.line("-- {s} {s} ({s} campaign, mission {s}; {s})", .{ language.code, one.file, one.tag, one.mission, if (covered) "scope covers its keys: compared with hb-view" else "scope does not cover its keys yet: ink only, no glyph trace" });
            try checkBriefing(io, allocator, language, run.folder, shot, blank_briefings[at], one, covered, &report);
        }

        // The options screen with its tooltip and the unit's encyclopedia page.
        for (routes, 0..) |route, at| {
            const covered = language.mod != null and scopeCovers(scopes, language.code, route);
            const route_frame = try run.route(allocator, route, covered);
            try writePng(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, try std.fmt.allocPrint(allocator, "{s}.png", .{route.tag}) }), route_frame);
            try report.line("-- {s} {s} ({s}; {s})", .{ language.code, route.tag, try std.fmt.allocPrint(allocator, "{s}/{s}/{s}.png", .{ scratch, language.code, route.tag }), if (language.mod == null) "base game" else if (covered) "scope covers its keys: compared with hb-view" else "scope does not cover its keys yet: ink only" });
            try checkRoute(io, allocator, language, run.folder, route_frame, blank_routes[at], covered, route, &report);
        }

        // The escape menu in the first Allied mission and its End Mission page.
        {
            var all_covered = language.mod != null;
            for (mission_shots) |one| all_covered = all_covered and scopeCovers(scopes, language.code, one.route);
            const frames = try run.mission(allocator, all_covered);
            for (mission_shots, frames, blank_mission) |one, frame, background| {
                try writePng(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, try std.fmt.allocPrint(allocator, "{s}.png", .{one.route.tag}) }), frame);
                try report.line("-- {s} {s} ({s}/{s}/{s}.png; {s})", .{ language.code, one.route.tag, scratch, language.code, one.route.tag, if (language.mod == null) "base game" else if (all_covered) "scope covers its keys: compared with hb-view" else "scope does not cover its keys yet: ink only" });
                try checkRoute(io, allocator, language, run.folder, frame, background, all_covered, one.route, &report);
            }
        }

        // A live switch: the Game starts on the other language and chooses this one while
        // it runs (the font cache clear of ApplyLanguage). Its buttons are the cold
        // start's. The header (labels[0]) is set once when the menu is built and keeps
        // its text on a live switch, so it is reported and not measured.
        const from = if (!std.mem.eql(u8, language.code, ru.code)) ru else languages[0];
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
        const chosen = try switch_run.play(allocator, &.{ "-windowed", profile_arg }, choose, null, "persist");
        if (language.mod) |mod| try expectLine(chosen, switch_run.folder, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{mod}));
                const restarted = try switch_run.capture(allocator, null, null);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "restart.tga" }), restarted);
        var restart_worst: usize = 0;
        for (labels) |label| restart_worst = @max(restart_worst, changedIn(menu, restarted, label.rect));
        try report.check(restart_worst <= max_live_difference, "{s} restarted on the profile that kept the choice: at most {d} pixels differ from the cold start in any label's box (limit {d})", .{ language.code, restart_worst, max_live_difference });

        // Options > Language and the gameplay mods list, once for the whole tier.
        if (language.mod == null) try checkLists(allocator, blank_run, absent.items, &report);

        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, language.code, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("l10n-game-reads-it: {s}: {d} measures failed; the report is {s}/{s}/report.txt\n", .{ language.code, report.failures, scratch, language.code });
    }
    if (failures != 0) return error.MeasureFailed;
}

/// The text between `<tag>` and the next `<` after its first occurrence in an XML document.
fn firstElement(xml: []const u8, comptime tag: []const u8) ?[]const u8 {
    const open = "<" ++ tag ++ ">";
    const start = (std.mem.indexOf(u8, xml, open) orelse return null) + open.len;
    return xml[start..][0 .. std.mem.indexOfScalar(u8, xml[start..], '<') orelse return null];
}

test "the briefing table names each campaign's first chapter and that chapter's first mission" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const cwd = std.Io.Dir.cwd();
    for (briefings) |one| {
        // Data/Scenarios/<Campaigns|Chapters>/... is mixed case on disk; the table is lower case.
        const campaign_path = try std.fmt.allocPrint(allocator, "Data/{s}.xml", .{one.campaign});
        const chapter_path = try std.fmt.allocPrint(allocator, "Data/{s}.xml", .{one.chapter});
        std.mem.replaceScalar(u8, campaign_path, '\\', '/');
        std.mem.replaceScalar(u8, chapter_path, '\\', '/');
        for ([_]struct { path: []const u8, element: []const u8, expected: []const u8 }{
            .{ .path = campaign_path, .element = "Chapter", .expected = one.chapter },
            .{ .path = chapter_path, .element = "Mission", .expected = one.mission },
        }) |case| {
            const found = (try check.findIgnoreCase(io, allocator, cwd, case.path)) orelse {
                std.debug.print("{s}: {s} is not below Data/\n", .{ one.tag, case.path });
                return error.FileNotFound;
            };
            const xml = try cwd.readFileAlloc(io, found, allocator, .limited(4 << 20));
            const actual = (if (std.mem.eql(u8, case.element, "Chapter")) firstElement(xml, "Chapter") else firstElement(xml, "Mission")) orelse return error.ElementMissing;
            if (!std.ascii.eqlIgnoreCase(actual, case.expected)) {
                std.debug.print("{s}: the first {s} of {s} is {s}, the table says {s}\n", .{ one.tag, case.element, found, actual, case.expected });
                return error.TableDrifted;
            }
        }
    }
}

/// The names Options > Language shows besides Default: each mod's MODName.
const native_names = [_][]const u8{ "Русский", "简体中文", "日本語", "हिन्दी", "Español", "Français", "Deutsch", "Português (Brasil)", "Bahasa Indonesia" };
/// MISSION_COMMAND_MODS_LIST (iMission.h: MISSION_BASE_VALUE 0x100e0000 + 245).
const mods_list_command = "269353205";

/// The gameplay mods list leaves the language mods out and Options > Language
/// lists Default first, then the nine mods by their native names, as the
/// Game's own trace lines (BK_L10N_TRACE) report them from one run: the
/// settings screen and its Options page opened, then the mods list.
fn checkLists(allocator: std.mem.Allocator, run: Run, absent: []const []const u8, report: *Report) !void {
    const log = try run.play(allocator, &.{ "-windowed", profile_arg }, "60:settings,100:msg=10003,240:cancel,300:cancel,360:cmd=" ++ mods_list_command ++ ",420:exit", null, "lists");
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
    // A native name may be missing only for a mod that is not built (lang-ja while M003 is work in progress).
    var all_seen = true;
    for (seen, native_names) |one, name| {
        const tolerated = std.mem.eql(u8, name, "日本語") and absent.len == 1 and std.mem.eql(u8, absent[0], "lang-ja");
        if (tolerated and !one) try report.line("      native name {s}: not listed, tolerated: lang-ja absent (M003 WIP)", .{name});
        all_seen = all_seen and (one or tolerated);
    }
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

/// Whether every character of `text` but white space is a full-width one (U+2E80 and up), drawn at
/// one-em steps by the CJK faces.
fn allWide(text: []const u8) bool {
    var view = std.unicode.Utf8View.initUnchecked(text);
    var it = view.iterator();
    while (it.nextCodepoint()) |cp| {
        if (cp == ' ' or cp == '\r' or cp == '\n') continue;
        if (cp < 0x2e80) return false;
    }
    return true;
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

/// The text of `label` as UTF-8: the language's overlay, and for English (which has none) the shipped original.
fn overlayText(io: std.Io, allocator: std.mem.Allocator, code: []const u8, label: Label) ![]u8 {
    if (!std.mem.eql(u8, code, "en")) return te.overlayText(io, allocator, code, label);
    const key = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ label.dir, label.key });
    // The English the Game draws is Data/AmericanELK's (the USSR mission's first objective reads "two
    // concrete pillboxes" there and "all" in Data/ELK), so that is the reference where it has the key.
    const cwd = std.Io.Dir.cwd();
    const american = try std.fmt.allocPrint(allocator, "Data/AmericanELK/game_data_base/{s}.txt", .{key});
    const bytes = if (try check.findIgnoreCase(io, allocator, cwd, american)) |found|
        try cwd.readFileAlloc(io, found, allocator, .limited(1 << 20))
    else
        (try check.readBase(io, allocator, cwd, key)) orelse return error.FileNotFound;
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return error.NotUtf16;
    const units = try allocator.alloc(u16, (bytes.len - 2) / 2);
    for (units, 0..) |*unit, at| unit.* = std.mem.readInt(u16, bytes[2 + at * 2 ..][0..2], .little);
    return std.unicode.utf16LeToUtf8Alloc(allocator, units);
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
        refs[index] = try chainView(allocator, io, hb_folder, try label.stem(allocator), texts[index], language);
        const full = try inkImage(allocator, frame, blank, label);
        const box = measure.inkBox(full, if (label.small and label.ink == .bright) small_box_threshold else ink_threshold) orelse {
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
        // A tooltip's ink is its yellow over the blank background's metal, about 80 of 255, so the
        // threshold takes in a pixel more of its edges above and below a 27-pixel line than the
        // reference's: the proportion has the small font's tolerance. Correlation and margin do not.
        const aspect_error = if (label.small or label.boxed) small_max_aspect_error else max_aspect_error;
        try report.check(own.correlation >= floor, "{s} {s} \"{s}\": correlation {d:.3} with hb-view (minimum {d:.2}), ink {d}x{d}", .{ language.code, label.key, texts[index], own.correlation, floor, ink.width, ink.height });
        try report.check(own.correlation - best_other >= margin, "{s} {s}: own reference beats the best other by {d:.3} (minimum {d:.2})", .{ language.code, label.key, own.correlation - best_other, margin });
        try report.check(@abs(own.aspect - 1) <= aspect_error, "{s} {s}: width to height is {d:.3} of the reference's (within {d:.2})", .{ language.code, label.key, own.aspect, aspect_error });
    }

    return .{ .hb_folder = hb_folder, .texts = texts, .refs = refs, .inks = inks };
}

fn checkLanguage(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, report: *Report) !void {
    const m = try checkLabels(io, allocator, language, folder, frame, blank, &labels, report);
    if (std.mem.eql(u8, language.code, "hi")) try checkHindi(io, allocator, language, m.hb_folder, m.texts, m.refs, m.inks, report);
    if (isHanLanguage(language)) try checkHan(io, allocator, language, m.hb_folder, &labels, m.texts, m.inks, true, report);
}

/// The briefing as a route, for the scope and ink checks that routes share.
fn briefingRoute(one: *const Briefing) Route {
    return .{ .tag = one.file, .schedule = "", .delay = briefing_delay, .action = one.action, .screen = "", .labels = &one.labels, .body = one.body };
}

/// The briefing's labels as hb-view draws them, and ink in the description's body; where the scope
/// does not cover the mission's keys the labels and the body are only measured for ink.
fn checkBriefing(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, one: *const Briefing, covered: bool, report: *Report) !void {
    if (!covered) return checkRoute(io, allocator, language, folder, frame, blank, false, briefingRoute(one), report);
    const m = try checkLabels(io, allocator, language, folder, frame, blank, &one.labels, report);
    if (isHanLanguage(language)) try checkHan(io, allocator, language, m.hb_folder, &one.labels, m.texts, m.inks, false, report);
    const body = try inkImage(allocator, frame, blank, one.body);
    const box = measure.inkBox(body, ink_threshold);
    try report.check(box != null and box.?.width() >= min_body_width, "{s} {s} description: ink {d} pixels wide in the panel's text lines (at least {d})", .{ language.code, one.file, if (box) |b| b.width() else 0, min_body_width });
    if (language.kinsoku) {
        // CUIConsts::CreateDescription's text of a scenario mission: the mission's name, its type,
        // an empty line, the description's caption and the description.
        var text: std.ArrayList(u8) = .empty;
        const strings = "textes/strings";
        const pieces = [_]struct { dir: []const u8, key: []const u8, join: []const u8 }{
            .{ .dir = strings, .key = "missiontext", .join = " " },
            .{ .dir = one.dir, .key = "header", .join = "\n" },
            .{ .dir = strings, .key = "missiontypetext", .join = " " },
            .{ .dir = strings, .key = "scenariomission", .join = "\n\n" },
            .{ .dir = strings, .key = "missiondescriptiontext", .join = "\n" },
            .{ .dir = one.dir, .key = "description", .join = "" },
        };
        for (pieces) |piece| {
            try text.appendSlice(allocator, try overlayText(io, allocator, language.code, .{ .key = piece.key, .dir = piece.dir, .rect = one.body.rect }));
            try text.appendSlice(allocator, piece.join);
        }
        try checkKinsoku(io, allocator, language, folder, frame, blank, one.file, briefing_box, text.items, report);
    }
}

/// Whether `code`'s scope holds every key the route's labels and body show.
fn scopeCovers(scopes: check.Scopes, code: []const u8, route: Route) bool {
    for (scopes.per_language) |scope| {
        if (!std.mem.eql(u8, scope.code, code)) continue;
        var buffer: [256]u8 = undefined;
        const body: []const Label = if (route.body) |*one| one[0..1] else &.{};
        for ([_][]const Label{ route.labels, body }) |set| for (set) |label| {
            const key = std.fmt.bufPrint(&buffer, "{s}/{s}", .{ label.dir, label.key }) catch return false;
            if (!scope.contains(key)) return false;
        };
        return true;
    }
    return false;
}

/// A route's labels (and body): ink inside each box for every language. Where the language's scope
/// covers the route's keys the texts are the language's own, and its static labels match hb-view
/// (the missing-glyph signal is BK_GLYPH_TRACE, which fails the run itself).
fn checkRoute(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, covered: bool, route: Route, report: *Report) !void {
    const body: []const Label = if (route.body) |*one| one[0..1] else &.{};
    for ([_][]const Label{ route.labels, body }, 0..) |set, which| for (set) |label| {
        const full = try inkImage(allocator, frame, blank, label);
        var lit: usize = 0;
        for (full.pixels) |p| lit += @intFromBool(p > ink_threshold);
        const box = measure.inkBox(full, ink_threshold);
        const box_width = if (box) |b| b.width() else 0;
        const wide = if (which == 1) min_body_width else te.min_ink_width;
        try report.check(lit >= min_route_ink_pixels and box_width >= wide, "{s} {s} {s}: {d} ink pixels, ink {d} pixels wide in the box {d},{d} to {d},{d} (at least {d} and {d})", .{ language.code, route.tag, label.key, lit, box_width, label.rect.x1, label.rect.y1, label.rect.x2, label.rect.y2, min_route_ink_pixels, wide });
    };
    if (covered) {
        var static: std.ArrayList(Label) = .empty;
        for (route.labels) |label| {
            if (label.dynamic) continue;
            if (label.boxed) try checkTooltip(io, allocator, language, folder, frame, blank, label, report) else try static.append(allocator, label);
        }
        if (static.items.len != 0) {
            const m = try checkLabels(io, allocator, language, folder, frame, blank, static.items, report);
            if (isHanLanguage(language)) try checkHan(io, allocator, language, m.hb_folder, static.items, m.texts, m.inks, false, report);
            if (route.english) try checkEnglish(io, allocator, language, m, static.items, report);
        }
    }
    if (route.scroll) |scroll| if (language.kinsoku) {
        if (covered) {
            const text = try overlayText(io, allocator, language.code, route.body.?);
            try checkKinsoku(io, allocator, language, folder, frame, blank, route.tag, scroll, text, report);
        } else try report.line("      kinsoku: {s}: not measured, the scope does not cover the route's keys yet", .{route.tag});
    };
}

/// A label must match its own string better than the English the base game draws there: by
/// `min_margin` in correlation, or the English raster's proportion is off by more than the label's
/// aspect tolerance. A translation equal to the English (MODs in de) is reported, not measured.
fn checkEnglish(io: std.Io, allocator: std.mem.Allocator, language: Language, m: Measured, set: []const Label, report: *Report) !void {
    for (set, 0..) |label, index| {
        const ink = m.inks[index];
        if (ink.width == full_width(label)) continue; // no ink: reported by checkLabels
        const english = try overlayText(io, allocator, "en", label);
        if (std.mem.eql(u8, std.mem.trim(u8, english, " \r\n"), std.mem.trim(u8, m.texts[index], " \r\n"))) {
            try report.line("      {s} {s}: the translation is the English \"{s}\" (same-as-base), not compared with it", .{ language.code, label.key, english });
            continue;
        }
        // Crédits and Credits differ by an accent: no raster tells them apart reliably.
        const distance = try editDistance(allocator, std.mem.trim(u8, english, " \r\n"), std.mem.trim(u8, m.texts[index], " \r\n"));
        if (distance <= 2) {
            try report.line("      {s} {s}: the translation \"{s}\" is {d} characters from the English \"{s}\", not compared with it", .{ language.code, label.key, m.texts[index], distance, english });
            continue;
        }
        const stem = try std.fmt.allocPrint(allocator, "en-{s}", .{try label.stem(allocator)});
        const reference = try measure.hbView(allocator, io, m.hb_folder, stem, english, .{ .font = serif, .language = "en" });
        const own = try matchLabel(allocator, label, ink, m.refs[index]);
        const theirs = try matchLabel(allocator, label, ink, reference);
        const aspect_error = if (label.small or label.boxed) small_max_aspect_error else max_aspect_error;
        const apart = own.correlation - theirs.correlation >= min_margin or @abs(theirs.aspect - 1) > aspect_error;
        try report.check(apart, "{s} {s}: drawn as \"{s}\", not the English \"{s}\" (correlation {d:.3} against {d:.3} for the English, its proportion {d:.3})", .{ language.code, label.key, m.texts[index], english, own.correlation, theirs.correlation, theirs.aspect });
    }
}

/// The Levenshtein distance of two UTF-8 strings in code points.
fn editDistance(allocator: std.mem.Allocator, a: []const u8, b: []const u8) !usize {
    var left: std.ArrayList(u21) = .empty;
    var right: std.ArrayList(u21) = .empty;
    for ([_][]const u8{ a, b }, [_]*std.ArrayList(u21){ &left, &right }) |text, list| {
        var view = try std.unicode.Utf8View.init(text);
        var it = view.iterator();
        while (it.nextCodepoint()) |cp| try list.append(allocator, cp);
    }
    const row = try allocator.alloc(usize, right.items.len + 1);
    for (row, 0..) |*cell, at| cell.* = at;
    for (left.items, 1..) |x, i| {
        var diagonal = row[0];
        row[0] = i;
        for (right.items, 1..) |y, j| {
            const above = row[j];
            row[j] = @min(@min(row[j] + 1, row[j - 1] + 1), diagonal + @intFromBool(x != y));
            diagonal = above;
        }
    }
    return row[right.items.len];
}

test "editDistance counts code points" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    try testing.expectEqual(@as(usize, 1), try editDistance(arena.allocator(), "Credits", "Crédits"));
    try testing.expectEqual(@as(usize, 11), try editDistance(arena.allocator(), "Spiel speichern", "Save Game"));
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
            const apart = try measure.hbView(allocator, io, hb_folder, try std.mem.concat(allocator, u8, &.{ label.key, "-halant" }), texts[index], .{ .font = language.face, .language = language.tag, .script = language.script, .features = measure.halant_spelling });
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

/// Whether the language's labels are told apart from the other Han faces' forms (zh-Hans and ja).
/// hb-view's raster of `text` as bk_text draws it: each character in the first face of the
/// language's chain (BkText faces.zig chainFor) that has it, Liberation Serif, then Noto Sans, then
/// the language's face, so a unit name that mixes Latin and Han ("Mk.IV“丘吉尔”步兵坦克") is not
/// drawn all in Noto. White space stays in the run it is in. A text of one run is drawn as before;
/// runs are drawn apart at one fixed line extent, so their baselines meet, and laid side by side.
fn chainView(allocator: std.mem.Allocator, io: std.Io, folder: []const u8, name: []const u8, whole: []const u8, language: Language) !Gray {
    // A label's trailing line break: hb-view draws a lone CR in a run as a missing-glyph box.
    const text = std.mem.trimEnd(u8, whole, " \r\n");
    const options = measure.HbOptions{ .font = language.face, .language = language.tag, .script = language.script };
    if (std.mem.eql(u8, language.face, serif)) return measure.hbView(allocator, io, folder, name, whole, options);
    const cwd = std.Io.Dir.cwd();
    const chain = [_][]const u8{ serif, "vendor/fonts/NotoSans-Regular.ttf" };
    var tables: [chain.len]check.cmap_table.Cmap = undefined;
    for (chain, 0..) |face, at| tables[at] = try check.cmap_table.Cmap.init(try cwd.readFileAlloc(io, face, allocator, .limited(64 << 20)));
    var runs: std.ArrayList(struct { face: []const u8, start: usize, end: usize }) = .empty;
    var view = try std.unicode.Utf8View.init(text);
    var it = view.iterator();
    var at: usize = 0;
    while (it.nextCodepointSlice()) |slice| : (at += slice.len) {
        const cp = try std.unicode.utf8Decode(slice);
        const space = cp == ' ' or cp == '\r' or cp == '\n' or cp == 0xA0;
        var face: []const u8 = language.face;
        for (chain, tables) |one, table| if (try table.covers(cp)) {
            face = one;
            break;
        };
        if (runs.items.len != 0 and (space or std.mem.eql(u8, runs.items[runs.items.len - 1].face, face))) {
            runs.items[runs.items.len - 1].end = at + slice.len;
        } else try runs.append(allocator, .{ .face = face, .start = at, .end = at + slice.len });
    }
    if (runs.items.len == 0) return measure.hbView(allocator, io, folder, name, whole, options);
    // One run: drawn whole in its face (MOD, all Latin, is Liberation Serif in a Japanese mod too).
    if (runs.items.len == 1) return measure.hbView(allocator, io, folder, name, whole, .{ .font = runs.items[0].face, .language = language.tag, .script = if (std.mem.eql(u8, runs.items[0].face, language.face)) language.script else null });
    // At the references' 96 px em, every face of the chain fits 110 px above the baseline and 40 below.
    var parts: std.ArrayList(Gray) = .empty;
    var total: usize = 0;
    for (runs.items, 0..) |run, index| {
        const part_name = try std.fmt.allocPrint(allocator, "{s}-run{d}", .{ name, index });
        const part = try measure.hbView(allocator, io, folder, part_name, text[run.start..run.end], .{ .font = run.face, .language = language.tag, .script = if (std.mem.eql(u8, run.face, language.face)) language.script else null, .extents = "110,40,0" });
        total += part.width;
        try parts.append(allocator, part);
    }
    const rows = parts.items[0].height;
    const pixels = try allocator.alloc(u8, total * rows);
    @memset(pixels, 0);
    var x0: usize = 0;
    for (parts.items) |part| {
        for (0..@min(rows, part.height)) |y| @memcpy(pixels[y * total + x0 ..][0..part.width], part.pixels[y * part.width ..][0..part.width]);
        x0 += part.width;
    }
    return .{ .width = total, .height = rows, .pixels = pixels };
}

/// Below this margin a Han character is a tie between the two faces, not a win or a loss: the USSR
/// briefing's first objective (混 土 堡, 2026-10-25) differs by 0.044 at most where both faces draw
/// nearly the same strokes, while a character the faces draw differently wins by 0.1 to 1.
const han_tie_below: f64 = 0.05;

fn isHanLanguage(language: Language) bool {
    return std.mem.eql(u8, language.code, "zh-Hans") or std.mem.eql(u8, language.code, "ja");
}

/// The Han characters of `set`'s labels against hb-view's Simplified and Japanese forms. `strict`
/// (the main menu, whose labels hold enough Han characters) makes fewer than three characters that
/// the faces draw differently a failure; a route's labels may hold none, which is a note and
/// leaves the face verdicts unmeasured.
fn checkHan(io: std.Io, allocator: std.mem.Allocator, language: Language, hb_folder: []const u8, set: []const Label, texts: []const []u8, inks: []const Gray, strict: bool, report: *Report) !void {
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
    var ties: usize = 0;
    for (set, 0..) |label, index| {
        if (!hasHan(texts[index])) continue;
        if (!allWide(texts[index])) {
            try report.line("      {s} {s}: mixes Latin with Han, so its characters are not at one-em steps; not judged for the face", .{ language.code, label.key });
            continue;
        }
        const stem = try label.stem(allocator);
        const ink = inks[index];
        const sc = try measure.hbView(allocator, io, hb_folder, try std.mem.concat(allocator, u8, &.{ stem, "-sc" }), texts[index], .{ .font = sc_face, .language = "zh-Hans" });
        const jp = try measure.hbView(allocator, io, hb_folder, try std.mem.concat(allocator, u8, &.{ stem, "-jp" }), texts[index], .{ .font = jp_face, .language = "ja" });
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
            if (@abs(margin) < han_tie_below) ties += 1 else won += @intFromBool(margin > 0);
        }
    }
    const where = if (strict) "" else " in the route's static labels";
    if (!strict and informative - ties < 3) {
        try report.line("      han: {s}{s}: {d} Han characters are drawn differently by the two faces and {d} of them are ties (fewer than 3 that are not, so the face is not judged here)", .{ language.code, where, informative, ties });
        return;
    }
    try report.check(informative >= 3, "{s}: {d} Han characters are drawn differently by the two faces (at least 3)", .{ language.code, informative });
    const mean = margin_sum / @as(f64, @floatFromInt(@max(informative, 1)));
    try report.check(mean >= min_face_margin, "{s}: over those characters the {s} face wins by {d:.3} on average (at least {d:.2}), in {d} of {d}", .{ language.code, own_name, mean, min_face_margin, won, informative });
    try report.check(won * 3 >= (informative - ties) * 2, "{s}: the {s} face is nearer in {d} of the {d} of those characters that are no tie (at least two in three; {d} ties below {d:.2})", .{ language.code, own_name, won, informative - ties, ties, han_tie_below });
}

/// The least a line band's ink may correlate with the hb-view raster of the line the game's wrap
/// predicts for it (the Japanese briefing's lines measure 0.96 and better). Medium text is
/// smoothed like the small one (`small_blur`), since the Game's raster hints its strokes a pixel
/// off a shaper's.
const min_line_correlation: f64 = 0.80;
/// Fewer empty rows than this between two runs of ink join them into one line.
const line_min_gap: usize = 2;

/// A wrapped text as the frame shows it, against the lines the game's wrap predicts (`text`, UTF-8,
/// in the language-mods font that the language's overlay is measured with): no predicted line breaks
/// the kinsoku rules, the frame's ink has one row band for every visible non-empty predicted line, and
/// each band correlates with hb-view's raster of its line. Only the lines the box shows whole are
/// measured: the game clips the next one at the box's lower edge.
fn checkKinsoku(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, tag: []const u8, box: ScrollBox, text: []const u8, report: *Report) !void {
    const cwd = std.Io.Dir.cwd();
    const bk_text = check.text_module;
    // The Game's own wrap: the shaper of the language at the medium size, as the fit rule makes it.
    var fit = try check.Fit.init(allocator, io, fonts_dir);
    defer fit.deinit();
    const slot = for (check.language_table.languages, 0..) |entry, at| {
        if (std.mem.eql(u8, entry.code, language.code)) break at;
    } else return error.UnknownLanguage;
    const shaper = try fit.shaper(slot, .medium);
    const units = try std.unicode.utf8ToUtf16LeAlloc(allocator, text);
    var lines: std.ArrayList(bk_text.wrap.Line) = .empty;
    try bk_text.wrap.wrapText(allocator, shaper, units, @floatFromInt(box.wrap_width), &lines);
    const slices = try allocator.alloc([]const u16, lines.items.len);
    for (lines.items, slices) |line, *out| out.* = units[line.start..line.end];
    const violation = measure.kinsokuViolation(slices, check.wrap.isNoStart, check.wrap.isNoEnd);
    try report.check(violation == null, "kinsoku: {s}: {s}: no predicted line breaks the rules ({s})", .{ language.code, tag, if (violation) |v| try std.fmt.allocPrint(allocator, "line {d} {s}", .{ v.line, v.reason }) else "checked all lines" });

    // The lines whole inside the box: the last needs only the font's height, the others a line space.
    const metrics = bk_text.measure.faceMetrics(shaper, shaper.chain[0]);
    const font_height: i64 = @intFromFloat(@ceil(metrics.ascent + metrics.descent));
    const space: i64 = @intFromFloat(bk_text.measure.lineSpace(shaper));
    const room: i64 = @as(i64, @intCast(box.bottom - box.y));
    const whole: usize = if (room < font_height) 0 else @intCast(@divTrunc(room - font_height, space) + 1);
    const shown = @min(whole, slices.len);
    const region_bottom = box.y + (shown -| 1) * @as(usize, @intCast(space)) + @as(usize, @intCast(font_height));
    const hb_folder = try std.fs.path.join(allocator, &.{ folder, "reference" });
    try cwd.createDirPath(io, hb_folder);
    var references: std.ArrayList(Gray) = .empty;
    for (slices[0..shown], 0..) |line, at| {
        const trimmed = std.mem.trim(u16, line, &[_]u16{ ' ', '\r', '\n' });
        if (trimmed.len == 0) continue;
        const utf8 = try std.unicode.utf16LeToUtf8Alloc(allocator, trimmed);
        const name = try std.fmt.allocPrint(allocator, "kinsoku-{s}-{d}", .{ tag, at });
        try references.append(allocator, try chainView(allocator, io, hb_folder, name, utf8, language));
    }
    const region_label = Label{ .key = tag, .rect = .{ .x1 = box.x, .y1 = box.y, .x2 = box.x + box.wrap_width, .y2 = region_bottom }, .ink = .dark };
    const region = try inkImage(allocator, frame, blank, region_label);
    const found = try measure.lineBands(allocator, region, references.items, ink_threshold, 64, line_min_gap, small_blur);
    try report.line("kinsoku: {s}: predicted {d} lines, measured {d}, min corr {d:.3} ({d} lines in all, {d} whole in the box {d},{d} to {d},{d}, wrap {d} wide, line space {d})", .{ tag, found.predicted, found.measured, if (found.correlations.len == 0) 0 else found.minCorrelation(), slices.len, shown, region_label.rect.x1, region_label.rect.y1, region_label.rect.x2, region_label.rect.y2, box.wrap_width, space });
    for (found.correlations, 0..) |one, at| try report.line("      kinsoku: {s}: band {d} correlates {d:.3} with its predicted line", .{ tag, at, one });
    const fault = found.fault(min_line_correlation);
    try report.check(fault == null, "kinsoku: {s}: {s}: {s} (minimum {d:.2})", .{ language.code, tag, if (fault) |f| switch (f) {
        .count_differs => "the ink's line bands differ in number from the predicted lines: the game broke the text elsewhere",
        .weak_line => "a line band does not match its predicted line",
    } else "every line band matches its predicted line", min_line_correlation });
}

/// The least ink of a tooltip's text pixel over the blank background: above its fill's difference.
const tooltip_text_ink: u8 = 60;

/// What a tooltip's frame leaves inside `inkImage`'s inset: a row or a column lit across nearly all of the
/// region's lit extent is a rule of the frame, not a line of text.
fn dropFrameRules(region: Gray) void {
    var x1: usize = region.width;
    var x2: usize = 0;
    var y1: usize = region.height;
    var y2: usize = 0;
    for (0..region.height) |y| for (0..region.width) |x| {
        if (region.pixels[y * region.width + x] == 0) continue;
        x1 = @min(x1, x);
        x2 = @max(x2, x + 1);
        y1 = @min(y1, y);
        y2 = @max(y2, y + 1);
    };
    if (x2 <= x1 or y2 <= y1) return;
    for (y1..y2) |y| {
        var lit: usize = 0;
        for (x1..x2) |x| lit += @intFromBool(region.pixels[y * region.width + x] != 0);
        if (lit * 10 >= (x2 - x1) * 9) @memset(region.pixels[y * region.width ..][0..region.width], 0);
    }
    for (x1..x2) |x| {
        var lit: usize = 0;
        for (y1..y2) |y| lit += @intFromBool(region.pixels[y * region.width + x] != 0);
        if (lit * 10 >= (y2 - y1) * 9) for (y1..y2) |y| {
            region.pixels[y * region.width + x] = 0;
        };
    }
}

/// A tooltip: the Game wraps it at the hint width in the medium font, so a text that does not fit
/// one line is drawn as several, which no single hb-view raster matches. The lines bk_text's wrap
/// predicts are compared band by band like a kinsoku text, a text of one line as a single band: the
/// plate is semi-transparent (consts.xml ToolTip Ground A=192), so the screen behind it shows
/// through and only pixels as bright as the text count as its ink, which the label check does not
/// do (2026-10-23: the diamond plate behind a one-line Japanese tooltip dropped it to 0.391).
fn checkTooltip(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, label: Label, report: *Report) !void {
    const bk_text = check.text_module;
    var fit = try check.Fit.init(allocator, io, fonts_dir);
    defer fit.deinit();
    const slot = if (std.mem.eql(u8, language.code, "en")) check.Fit.english else for (check.language_table.languages, 0..) |entry, at| {
        if (std.mem.eql(u8, entry.code, language.code)) break at;
    } else return error.UnknownLanguage;
    const hint_width = check.tooltip_width;
    const shaper = try fit.shaper(slot, .medium);
    const text = try overlayText(io, allocator, language.code, label);
    const units = try std.unicode.utf8ToUtf16LeAlloc(allocator, text);
    var lines: std.ArrayList(bk_text.wrap.Line) = .empty;
    try bk_text.wrap.wrapText(allocator, shaper, units, hint_width, &lines);
    const hb_folder = try std.fs.path.join(allocator, &.{ folder, "reference" });
    try std.Io.Dir.cwd().createDirPath(io, hb_folder);
    var references: std.ArrayList(Gray) = .empty;
    for (lines.items, 0..) |line, at| {
        const trimmed = std.mem.trim(u16, units[line.start..line.end], &[_]u16{ ' ', '\r', '\n' });
        if (trimmed.len == 0) continue;
        const utf8 = try std.unicode.utf16LeToUtf8Alloc(allocator, trimmed);
        const name = try std.fmt.allocPrint(allocator, "tooltip-{s}-{d}", .{ language.code, at });
        try references.append(allocator, try chainView(allocator, io, hb_folder, name, utf8, language));
    }
    const region = try inkImage(allocator, frame, blank, label);
    // The tooltip's dark fill differs from the blank background by up to about 26 (the strip's note above), which
    // `ink_threshold` would count; its yellow text adds far more.
    for (region.pixels) |*p| {
        if (p.* < tooltip_text_ink) p.* = 0;
    }
    dropFrameRules(region);
    const found = try measure.lineBands(allocator, region, references.items, tooltip_text_ink, 64, line_min_gap, small_blur);
    try report.line("tooltip {s} \"{s}\": predicted {d} lines at {d:.0} wide, measured {d}, min corr {d:.3}", .{ label.key, text, found.predicted, hint_width, found.measured, if (found.correlations.len == 0) 0 else found.minCorrelation() });
    for (found.correlations, 0..) |one, at| try report.line("      tooltip {s}: band {d} correlates {d:.3} with its predicted line", .{ label.key, at, one });
    // One line is a label's text at a label's size, held to the label minimum; the stricter line
    // minimum is for the bands of a wrapped tooltip.
    const minimum = if (references.items.len == 1) min_correlation else min_line_correlation;
    const fault = found.fault(minimum);
    try report.check(fault == null, "{s} {s}: tooltip lines {s} (minimum {d:.2})", .{ language.code, label.key, if (fault) |f| switch (f) {
        .count_differs => "differ in number from the lines bk_text's wrap predicts",
        .weak_line => "do not match hb-view of the predicted lines",
    } else "match hb-view of the lines bk_text's wrap predicts", minimum });
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
    /// Whether the Game runs with BK_TEXT_ENGINE=bitmap: the blank background's, whose mod brings
    /// bitmap fonts with no ink. Every other run is on the default engine, bk_text.
    bitmap: bool,

    /// Makes the scratch folder and sets the profile's language option to
    /// `mod` (Default for the base game) with a short run of the Game on the
    /// options' own `set=`, which the profile keeps.
    fn init(io: std.Io, allocator: std.mem.Allocator, game: []const u8, game_root: []const u8, root: []const u8, name: []const u8, mod: ?[]const u8, bitmap: bool) !Run {
        const cwd = std.Io.Dir.cwd();
        const folder = try std.fs.path.join(allocator, &.{ root, name });
        const user = try std.fs.path.join(allocator, &.{ folder, "user" });
        // defconf.cfg, the options' declarations, is looked for under the working directory.
        const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
        try cwd.createDirPath(io, configs);
        const defconf = try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }), .data = try cwd.readFileAlloc(io, defconf, allocator, .limited(1 << 20)) });
        const run = Run{ .io = io, .game = game, .folder = folder, .user = user, .mod = mod, .name = name, .bitmap = bitmap };
        const schedule = try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{mod orelse "Default"});
        const log = try run.play(allocator, &.{ "-windowed", profile_arg }, schedule, null, "choose");
        if (mod) |one| try expectLine(log, run.folder, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{one}));
        return run;
    }

    fn play(self: Run, allocator: std.mem.Allocator, args: []const []const u8, schedule: ?[]const u8, delay: ?u32, tag: []const u8) ![]u8 {
        var environ = try testing.environ.createMap(allocator);
        try environ.put("XDG_DATA_HOME", self.user);
        try environ.put("BK_USER_ROOT", self.user);
        try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ self.user, "profiles" }));
        try environ.put("BK_MEM_REPORT", "count");
        try environ.put("BK_DEBUG_LOG", "1");
        try environ.put("BK_MOD_TRACE", "1");
        try environ.put("BK_UI_TRACE", "1");
        try environ.put("BK_NO_HELP", "1");
        try environ.put("BK_GLYPH_TRACE", "1");
        // An unset variable means bk_text; the blank background says bitmap.
        if (self.bitmap) try environ.put("BK_TEXT_ENGINE", "bitmap");
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
        // A Game that fell back to the bitmap fonts logs `engine bitmap` and would otherwise pass.
        if (!self.bitmap) try expectLine(log, self.folder, "bk_text: engine ttf");
        return log;
    }

    /// The main-menu capture (RGBA, alpha zero), optionally under a BK_AUTO_UI
    /// schedule and with the capture held back to `delay` frames.
    fn capture(self: Run, allocator: std.mem.Allocator, schedule: ?[]const u8, delay: ?u32) ![]u8 {
        const log = try self.scene(allocator, schedule, delay, "capture");
        try expectLine(log, self.folder, "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
        return self.frame(allocator, "capture");
    }

    /// A campaign's first mission briefing capture. The log must show the schedule's
    /// mission action reaching the screen, and, for a language whose scope covers the
    /// mission (`keys` set), the Game opening every text key of the briefing from the mod.
    /// A mission the Game cannot open fails with BriefingNotOpened, naming the campaign.
    fn briefing(self: Run, allocator: std.mem.Allocator, one: *const Briefing, keys: bool) ![]u8 {
        const log = try self.scene(allocator, try one.schedule(allocator), briefing_delay, one.file);
        if (std.mem.indexOf(u8, log, "has no random mission entry") != null) {
            std.debug.print("the Game could not open the {s} campaign's briefing (mission {s}); its logs are in {s}\n", .{ one.tag, one.mission, self.folder });
            return error.BriefingNotOpened;
        }
        for ([_][]const u8{ one.action, "BK_UI_TRACE: reposition \"ui\\common\\mission\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000" }) |line| expectLine(log, self.folder, line) catch |err| {
            std.debug.print("the Game did not reach the {s} campaign's briefing (mission {s}); its logs are in {s}\n", .{ one.tag, one.mission, self.folder });
            self.reached(log);
            return err;
        };
        if (keys) {
            for ([_][]const Label{ &one.labels, (&one.body)[0..1] }) |set| for (set) |label| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"{s}\" from LANGUAGE", .{try label.tracedPath(allocator)});
                try expectLine(log, self.folder, line);
            };
            for (briefing_extra_keys) |key| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"textes\\strings\\{s}.txt\" from LANGUAGE", .{key});
                try expectLine(log, self.folder, line);
            }
        }
        return self.frame(allocator, one.file);
    }

    /// A route's capture. The log must show the schedule's last action and the screen's
    /// reposition (a failure names the BK_AUTO_UI step it reached), and, for a language whose
    /// scope covers the route (`keys` set), the Game opening every text of it from the mod.
    fn route(self: Run, allocator: std.mem.Allocator, one: Route, keys: bool) ![]u8 {
        const log = try self.scene(allocator, one.schedule, one.delay, one.tag);
        for ([_][]const u8{ one.action, one.screen }) |line| expectLine(log, self.folder, line) catch |err| {
            self.reached(log);
            return err;
        };
        if (keys) {
            const body: []const Label = if (one.body) |*body_label| body_label[0..1] else &.{};
            for ([_][]const Label{ one.labels, body }) |set| for (set) |label| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"{s}\" from LANGUAGE", .{try label.tracedPath(allocator)});
                try expectLine(log, self.folder, line);
            };
        }
        return self.frame(allocator, one.tag);
    }

    /// The first Allied mission's escape menu and its End Mission page (`mission_shots`), one run of
    /// the Game windowed at 1024x768. The log must show each shot's action and both shots written,
    /// and, for a language whose scope covers the labels (`keys` set), the Game opening every text of
    /// them from the mod.
    fn mission(self: Run, allocator: std.mem.Allocator, keys: bool) ![mission_shots.len][]u8 {
        const schedule = try std.mem.concat(allocator, u8, &.{ try briefings[0].schedule(allocator), mission_tail });
        const log = try self.play(allocator, &.{ "-windowed", "-mode=1024x768", profile_arg }, schedule, null, "mission");
        for ([_][]const u8{ "BK_AUTO_UI: frame 420 action ok", mission_shots[0].route.action, mission_shots[1].route.action }) |line| expectLine(log, self.folder, line) catch |err| {
            self.reached(log);
            return err;
        };
        if (std.mem.count(u8, log, "BK_AUTO_UI: shot written") != mission_shots.len) {
            std.debug.print("{s}: the mission run did not write its {d} shots; its log is in {s}\n", .{ self.name, mission_shots.len, self.folder });
            return error.ShotMissing;
        }
        var frames: [mission_shots.len][]u8 = undefined;
        for (mission_shots, 0..) |one, at| {
            if (keys) for (one.route.labels) |label| {
                const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"{s}\" from LANGUAGE", .{try label.tracedPath(allocator)});
                try expectLine(log, self.folder, line);
            };
            const path = try std.fmt.allocPrint(allocator, "{s}/autoshot_{s}_{d}x{d}.rgba", .{ self.user, one.key, width, height });
            frames[at] = try std.Io.Dir.cwd().readFileAlloc(self.io, path, allocator, .limited(16 << 20));
            try testing.expectEqual(@as(usize, width * height * 4), frames[at].len);
        }
        return frames;
    }

    /// The schedule steps the Game ran, for a route that stopped short.
    fn reached(self: Run, log: []const u8) void {
        var lines = std.mem.splitScalar(u8, log, '\n');
        while (lines.next()) |line| {
            if (std.mem.indexOf(u8, line, "BK_AUTO_UI: frame") != null and std.mem.indexOf(u8, line, " action ") != null) std.debug.print("{s}: reached {s}\n", .{ self.name, line });
        }
    }

    /// One run of the Game to its `-reference-scene` capture, `<tag>.rgba`.
    fn scene(self: Run, allocator: std.mem.Allocator, schedule: ?[]const u8, delay: ?u32, tag: []const u8) ![]u8 {
        const path = try std.fmt.allocPrint(allocator, "{s}/{s}.rgba", .{ self.folder, tag });
        const log = try self.play(allocator, &.{ "-reference-scene", path, "-reference-resolution", "1024", "768", "-windowed", profile_arg }, schedule, delay, tag);
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

/// A PNG of an RGBA frame (alpha dropped: reference captures carry none), written with stored
/// deflate blocks, since the checks read the frames and a person only needs to open them.
fn writePng(io: std.Io, allocator: std.mem.Allocator, path: []const u8, rgba: []const u8) !void {
    const row = 1 + width * 3;
    const raw = try allocator.alloc(u8, row * height);
    for (0..height) |y| {
        raw[y * row] = 0;
        for (0..width) |x| @memcpy(raw[y * row + 1 + x * 3 ..][0..3], rgba[(y * width + x) * 4 ..][0..3]);
    }
    var zlib: std.ArrayList(u8) = .empty;
    try zlib.appendSlice(allocator, &.{ 0x78, 0x01 });
    var at: usize = 0;
    while (at < raw.len) {
        const len = @min(raw.len - at, 65535);
        const last: u8 = @intFromBool(at + len == raw.len);
        try zlib.append(allocator, last);
        try zlib.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToLittle(u16, @intCast(len))));
        try zlib.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToLittle(u16, ~@as(u16, @intCast(len)))));
        try zlib.appendSlice(allocator, raw[at..][0..len]);
        at += len;
    }
    try zlib.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToBig(u32, std.hash.Adler32.hash(raw))));
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(allocator, "\x89PNG\r\n\x1a\n");
    var header: [13]u8 = undefined;
    std.mem.writeInt(u32, header[0..4], width, .big);
    std.mem.writeInt(u32, header[4..8], height, .big);
    header[8..13].* = .{ 8, 2, 0, 0, 0 };
    try pngChunk(allocator, &out, "IHDR", &header);
    try pngChunk(allocator, &out, "IDAT", zlib.items);
    try pngChunk(allocator, &out, "IEND", "");
    try std.Io.Dir.cwd().createDirPath(io, std.fs.path.dirname(path).?);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = out.items });
}

fn pngChunk(allocator: std.mem.Allocator, out: *std.ArrayList(u8), kind: *const [4]u8, data: []const u8) !void {
    try out.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToBig(u32, @intCast(data.len))));
    try out.appendSlice(allocator, kind);
    try out.appendSlice(allocator, data);
    var crc = std.hash.Crc32.init();
    crc.update(kind);
    crc.update(data);
    try out.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToBig(u32, crc.final())));
}
