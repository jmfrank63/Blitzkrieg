//! The Achtung Panzer 2 mod's menus in the nine languages, on the real Game, where the mod is at
//! hand: its content is not ours to track, so the tier reads the mod from the path -Dap2-mod names
//! (the mod's folder, holding data/) and is skipped without it. The mod and the installed language
//! mods are linked, never copied or written, into a scratch mods folder (BK_MODS_ROOT), and each
//! language's profile has it as its language option. -Dap2-langs=ru,de limits the run to some codes.
//!
//! The mod's layouts were made for its own fonts: its buttons are one line of its 40 pixel large font
//! in a 320x50 box. Where the mod's fonts lack a language's letters bk_text stands in for them at the
//! mod's cell heights, at full size: a caption is never scaled down to fit its box, so a translation
//! the box cannot hold on one line is shortened in Localization/game instead. The Game opens the main
//! menu and the sub-menus the main menu reaches without a mission (Load Game, New Game and its Custom
//! Game, Options) and captures each at 1024x768, in every language and once in the mod's English,
//! which its own bitmap fonts draw. Each sub-menu is a 400 pixel title (element 20001, 88 pixels down
//! a 400x172 box at 610,95) over 320x50 buttons 65 pixels apart from 650,267 (MainMenu.xml).
//!
//! A caption's ink is the pixels of the language's frame of its text colour (the title's 0xff815335,
//! the buttons' black or the grey of a disabled one) that a frame of the same screen without text
//! does not have: the background is the mod with its font atlases cleared (linkBlankMod), so the art
//! is the same, and its rules, logo and cartridge have the same colours. The mod's English is
//! measured as each language is. The ink must
//! - lie inside its control's rectangle: none above, below or beside it, none between controls;
//! - be one line: no run of empty rows between its first and its last row of ink;
//! - be whole: not touch the rectangle's edges, where a line too long or too tall for it is cut;
//! - be at full size: at least minInkRows rows tall, which a caption scaled down to its box (as
//!   3dca9feee did) is not.
//! The title of a sub-menu and its buttons must say different things. The base game's Load Game title
//! (LoadGame\caption) and first button (LoadGame\loadgame) are both "Load Saved Game", and the mod's
//! English title is its lower case "load saved game", so the precedence rightly shows the language's
//! own text of each key (TextPrecedence.h: the mod's text is the base game's English after
//! normalising), and translated word for word the menu read the same words twice. The titles now say
//! "Load Game" in each language's glossary words. Each text the menus show is the language's own (no
//! BK_TEXT_TRACE line has the mod's English as the source of a caption's key), and the language's
//! title text differs from every button text of its menu (Localization/game, the texts the language
//! mod is built from).
//! The log names the stand-in's face: Noto Sans for the large font (a Gill Sans) and the medium
//! (Courier-like), not Liberation Serif.
//!
//! The run ends on the settings screen's Gameplay tab with the cursor on the player name box, whose
//! tooltip (the medium font, drawn by the scene over everything) must be drawn inside its frame
//! (checkTooltip); every run fails on a BK_GLYPH_TRACE line, the Game's own report of a letter its
//! font has no glyph for, which a user saw as a tooltip of empty boxes.
//!
//! The frames are kept as zig-out/local-test/ap2-menu/game-reads-it/<code>/<screen>.tga,
//! english/<screen>.tga and blank/<screen>.tga, with the logs. All nine languages take about 20
//! minutes (two Game runs each, about a minute a run): -Dap2-langs=ru,ja,hi, de,es,fr and
//! id,pt-BR,zh-Hans keep each run under the 10 minutes of a foreground command. The user's data is
//! never touched: XDG_DATA_HOME, BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT are the scratch
//! folder. BK_AP2_GAME names the installed Game
//! executable; `zig build ap2-menu-game-reads-it -Dap2-mod=<folder>` sets it.
const std = @import("std");
const options = @import("ap2_options");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const languages = @import("l10n_languages.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/ap2-menu/game-reads-it";
const width = 1024;
const height = 768;

/// The main menu, then IMC_LOAD_GAME (10004) and back (10001), New Game (IMC_OK, 10002) and its
/// single mission button (10004), the Custom Game menu, back to the main menu, its options
/// (IMC_SETTINGS, 10005) and the settings screen (10003) on its Gameplay tab with the cursor on the
/// player name box until its tooltip is up. New Game opens the player's profile card over the menu
/// each time it is entered (CUINewGameState::Show), which takes the messages until IMC_CANCEL closes
/// it. Each screen is captured 180 frames after it was opened, the tooltip 200 after the hover.
const schedule = "400:shot,420:msg=10004,600:shot,620:msg=10001,640:msg=10002,740:msg=10001,920:shot,940:msg=10004,1120:shot," ++
    "1140:msg=10001,1240:msg=10001,1340:msg=10001,1360:msg=10005,1540:shot,1560:msg=10003,1720:click=810x365,1770:hover=405x165,1970:shot,2010:exit";
/// The frame of the tooltip of the settings screen's player name box (Gameplay tab, the box at
/// 342..472 x 153..181): "Enter your name here" in the medium font, the Russian of which a user saw
/// as a row of empty boxes.
const tooltip_frame = 1970;
/// Where the tooltip is looked for: right of and below the cursor at 405,165.
const tooltip_area = Rect{ .x1 = 300, .y1 = 140, .x2 = 1020, .y2 = 300 };
/// The tooltip frame colour of the mod's style (Scene.Colors.ToolTip.Frame, 42,24,23, of
/// Data/UI/ModStyles/achtungpanzer2/consts.xml); its text is black.
const tooltip_border = Rgb{ 0x2a, 0x18, 0x17 };

const Rect = struct { x1: usize, y1: usize, x2: usize, y2: usize };
const Rgb = [3]u8;
const title_color = Rgb{ 0x81, 0x53, 0x35 };

/// A caption of a screen: its ELK key (relative to game_data_base, no extension, lower case) and
/// whether it is the title, which is the brown text over the box at 610,183; a button's box is its
/// row of the menu.
const Caption = struct { name: []const u8, key: []const u8 };
const Screen = struct { name: []const u8, frame: u32, title: Caption, buttons: []const Caption };

const back = Caption{ .name = "back", .key = "textes/strings/imheader-gbutton-back" };
const screens = [_]Screen{
    .{ .name = "main", .frame = 400, .title = .{ .name = "title", .key = "textes/strings/imheader-mainmenu-header" }, .buttons = &.{
        .{ .name = "new game", .key = "textes/strings/imheader-gbutton-newgame" },
        .{ .name = "multiplayer", .key = "textes/strings/imheader-gbutton-multiplayer" },
        .{ .name = "load game", .key = "textes/strings/imheader-gbutton-loadgame" },
        .{ .name = "options", .key = "textes/strings/imheader-gbutton-options" },
        .{ .name = "exit game", .key = "textes/strings/imheader-gbutton-exitgame" },
    } },
    .{ .name = "load", .frame = 600, .title = .{ .name = "title", .key = "textes/ui/intermission/mainmenu/loadgame/caption" }, .buttons = &.{
        .{ .name = "load saved game", .key = "textes/ui/intermission/mainmenu/loadgame/loadgame" },
        .{ .name = "load replay", .key = "textes/ui/intermission/mainmenu/loadgame/loadreplay" },
        back,
    } },
    .{ .name = "options", .frame = 1540, .title = .{ .name = "title", .key = "textes/strings/imheader-options-header" }, .buttons = &.{
        .{ .name = "settings", .key = "textes/strings/imheader-gbutton-settings" },
        .{ .name = "cutscenes", .key = "textes/strings/imheader-gbutton-cutscenes" },
        .{ .name = "credits", .key = "textes/strings/imheader-gbutton-credits" },
        .{ .name = "mods", .key = "textes/strings/imheader-gbutton-mods" },
        back,
    } },
    .{ .name = "new-game", .frame = 920, .title = .{ .name = "title", .key = "textes/strings/imheader-newgame-header" }, .buttons = &.{
        .{ .name = "tutorial", .key = "textes/strings/imheader-gbutton-tutorial" },
        .{ .name = "campaigns", .key = "textes/strings/imheader-gbutton-scenario" },
        .{ .name = "custom game", .key = "textes/strings/imheader-gbutton-custom" },
        .{ .name = "player profile", .key = "textes/ui/intermission/mainmenu/newgame/button_player_profile" },
        back,
    } },
    .{ .name = "custom-game", .frame = 1120, .title = .{ .name = "title", .key = "textes/ui/intermission/mainmenu/customgame/caption" }, .buttons = &.{
        .{ .name = "custom campaigns", .key = "textes/ui/intermission/mainmenu/customgame/customcampaigns" },
        .{ .name = "custom chapters", .key = "textes/ui/intermission/mainmenu/customgame/customchapters" },
        .{ .name = "custom missions", .key = "textes/ui/intermission/mainmenu/customgame/custommissions" },
        back,
    } },
};

const title_rect = Rect{ .x1 = 610, .y1 = 183, .x2 = 1010, .y2 = 267 };
fn buttonRect(row: usize) Rect {
    return .{ .x1 = 650, .y1 = 267 + 65 * row, .x2 = 970, .y2 = 317 + 65 * row };
}
/// The menu's paper around the captions, from below the mod's logo to below the last button, right
/// of the profile card's edge (604).
fn panel(screen: Screen) Rect {
    return .{ .x1 = 606, .y1 = 160, .x2 = 1020, .y2 = 267 + 65 * screen.buttons.len + 10 };
}

/// The fewest rows of ink a caption drawn at full size has. Measured on the nine languages and the
/// mod's English: a Cyrillic caption without a descender ("Кампании", the large stand-in's capital
/// height) has 21, the mod's own bitmap fonts 23 (its English and the Latin languages), Hindi's
/// "वापस" (no vowel sign above the headline or below the line) 18. A caption scaled down to its box
/// (3dca9feee, about 0.7 of the cell for the Russian Load Game buttons) falls below.
fn minInkRows(code: []const u8) usize {
    return if (std.mem.eql(u8, code, "hi")) 16 else 20;
}

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

/// A pixel of a caption's text in one frame against the other: changed, and of the title's brown or
/// a darker shade of it (the New Game title is drawn at half its colour under the profile card),
/// keeping the colour's proportions (the edges of black text on the paper are a grey brown about as
/// dark) or, for a button, dark (black, or the grey of a disabled button; the glyphs' edges blend
/// into the paper).
fn isInk(frame: []const u8, other: []const u8, x: usize, y: usize, title: bool) bool {
    const at = (y * width + x) * 4;
    const p = frame[at..][0..3];
    const q = other[at..][0..3];
    var change: u32 = 0;
    for (0..3) |i| change = @max(change, @abs(@as(i32, p[i]) - @as(i32, q[i])));
    if (change <= 8) return false;
    if (!title) return @max(p[0], @max(p[1], p[2])) < 140 and @abs(@as(i32, p[0]) - @as(i32, p[2])) < 24;
    var dot: f64 = 0;
    var norm: f64 = 0;
    for (0..3) |i| {
        dot += @as(f64, @floatFromInt(p[i])) * @as(f64, @floatFromInt(title_color[i]));
        norm += @as(f64, @floatFromInt(title_color[i])) * @as(f64, @floatFromInt(title_color[i]));
    }
    const shade = dot / norm;
    if (shade < 0.35 or shade > 1.15) return false;
    var distance: f64 = 0;
    for (0..3) |i| distance += @abs(@as(f64, @floatFromInt(p[i])) - shade * @as(f64, @floatFromInt(title_color[i])));
    // The spread of red over blue a shade of the brown has: a blend of black text into the paper is
    // as dark but greyer.
    const spread = @as(f64, @floatFromInt(@as(i32, p[0]) - @as(i32, p[2])));
    const color_spread = shade * @as(f64, @floatFromInt(@as(i32, title_color[0]) - @as(i32, title_color[2])));
    return distance < 30 and @abs(spread - color_spread) < 0.3 * color_spread;
}

/// Rows a band of ink must span to count as a line of text, not a mark above one (the dots of Ё).
const min_line_rows = 5;

fn inside(rect: Rect, x: usize, y: usize) bool {
    return x >= rect.x1 and x < rect.x2 and y >= rect.y1 and y < rect.y2;
}

const Measure = struct { inside: usize, outside: usize, lines: usize, box: ?Rect };

/// The caption's ink inside its rectangle and in the panel outside every caption's rectangle, the
/// number of lines its ink inside forms (bands of rows split by two empty rows, at least
/// min_line_rows tall), and the box of that ink.
fn measure(frame: []const u8, other: []const u8, screen: Screen, rect: Rect, title: bool) Measure {
    var result = Measure{ .inside = 0, .outside = 0, .lines = 0, .box = null };
    const area = panel(screen);
    for (area.y1..area.y2) |y| for (area.x1..area.x2) |x| {
        if (!isInk(frame, other, x, y, title)) continue;
        if (inside(rect, x, y)) {
            result.inside += 1;
            if (result.box) |*box| {
                box.x1 = @min(box.x1, x);
                box.y1 = @min(box.y1, y);
                box.x2 = @max(box.x2, x + 1);
                box.y2 = @max(box.y2, y + 1);
            } else result.box = .{ .x1 = x, .y1 = y, .x2 = x + 1, .y2 = y + 1 };
            continue;
        }
        // Another caption's rectangle holds that caption's ink, which a shade of the other colour can
        // pass for (a black glyph's edge on the paper is a dark brown); a caption spilling into it
        // touches its own rectangle's edge, and that is found below.
        var other_caption = inside(title_rect, x, y);
        for (0..screen.buttons.len) |row| {
            if (inside(buttonRect(row), x, y)) other_caption = true;
        }
        if (!other_caption) result.outside += 1;
    };
    const box = result.box orelse return result;
    var empty: usize = 0;
    var band: usize = 0;
    for (box.y1..box.y2 + 2) |y| {
        var ink = false;
        if (y < box.y2) for (rect.x1..rect.x2) |x| {
            if (isInk(frame, other, x, y, title)) ink = true;
        };
        if (ink) {
            band += 1 + (if (band > 0) empty else 0);
            empty = 0;
            continue;
        }
        empty += 1;
        if (empty == 2) {
            if (band >= min_line_rows) result.lines += 1;
            band = 0;
        }
    }
    return result;
}

/// The Game's frames of a run, one per screen.
const Shots = [screens.len + 1][]const u8;

fn wanted(code: []const u8) bool {
    if (options.langs.len == 0) return true;
    var it = std.mem.splitScalar(u8, options.langs, ',');
    while (it.next()) |one| if (std.ascii.eqlIgnoreCase(one, code)) return true;
    return false;
}

test "Achtung Panzer 2's menus in nine languages: every caption is one whole line inside its control, at full size" {
    if (options.mod.len == 0) return error.SkipZigTest;
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_AP2_GAME")) orelse {
        std.debug.print("BK_AP2_GAME is not set: run zig build ap2-menu-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    const mods = try std.fs.path.join(allocator, &.{ root, "mods" });
    const blank_mods = try std.fs.path.join(allocator, &.{ root, "blank-mods" });
    const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
    try cwd.createDirPath(io, configs);
    try cwd.createDirPath(io, mods);
    try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" }), try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }));
    for (languages.languages) |language| {
        if (!wanted(language.code)) continue;
        try cwd.symLink(io, try std.fs.path.join(allocator, &.{ game_root, "mods", language.mod }), try std.fs.path.join(allocator, &.{ mods, language.mod }), .{ .is_directory = true });
    }
    try cwd.symLink(io, options.mod, try std.fs.path.join(allocator, &.{ mods, "AchtungPanzer2" }), .{ .is_directory = true });
    try linkBlankMod(io, allocator, blank_mods);

    // The background: the mod with its fonts blanked, in English, a profile that never chose a
    // language. Then the mod's English, measured as every language is.
    const blank_root = try std.fs.path.join(allocator, &.{ root, "blank" });
    try cwd.createDirPath(io, blank_root);
    const blank_log = try play(io, allocator, game, user, blank_mods, blank_root, "menu", "Ap2MenuBlank", &.{ "-mod=achtungpanzer2", "-mode=1024x768" }, schedule);
    const blank = try readShots(io, allocator, user, blank_root, blank_log);

    var failures: usize = 0;
    const english_code = "en";
    for (0..languages.languages.len + 1) |index| {
        const english = index == 0;
        const language = if (english) languages.Language{ .code = english_code, .mod = "", .name = "English", .face = "" } else languages.languages[index - 1];
        if (!english and !wanted(language.code)) continue;
        const dir = try std.fs.path.join(allocator, &.{ root, if (english) "english" else language.code });
        try cwd.createDirPath(io, dir);
        const profile = try std.fmt.allocPrint(allocator, "Ap2Menu-{s}", .{language.code});
        if (!english) _ = try play(io, allocator, game, user, mods, dir, "choose", profile, &.{}, try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{language.mod}));
        const log = try play(io, allocator, game, user, mods, dir, "menu", profile, &.{ "-mod=achtungpanzer2", "-mode=1024x768" }, schedule);
        if (!english) try expectLine(log, dir, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{language.mod}));
        const shots = try readShots(io, allocator, user, dir, log);
        for (screens, 0..) |screen, s| {
            for (0..screen.buttons.len + 1) |i| {
                const title = i == 0;
                const caption = if (title) screen.title else screen.buttons[i - 1];
                const rect = if (title) title_rect else buttonRect(i - 1);
                const m = measure(shots[s], blank[s], screen, rect, title);
                const box = m.box orelse Rect{ .x1 = 0, .y1 = 0, .x2 = 0, .y2 = 0 };
                const whole = m.box != null and box.x1 > rect.x1 and box.y1 > rect.y1 and box.x2 < rect.x2 and box.y2 < rect.y2;
                const full = box.y2 - box.y1 >= minInkRows(language.code);
                const ok = m.inside >= 150 and m.outside == 0 and m.lines == 1 and whole and full;
                std.debug.print("{s} {s} {s}: {s}, {d} ink pixels inside {d},{d}-{d},{d}, {d} outside every caption, {d} line(s), ink box {d},{d}-{d},{d} ({d}x{d})\n", .{
                    language.code, screen.name, caption.name, if (ok) "ok" else "FAIL", m.inside, rect.x1, rect.y1, rect.x2, rect.y2, m.outside, m.lines, box.x1, box.y1, box.x2, box.y2, box.x2 - box.x1, box.y2 - box.y1,
                });
                if (!ok) failures += 1;
            }
            if (!english) failures += try expectOwnTexts(io, allocator, log, language.code, screen);
        }
        if (!english) failures += try expectSansStandIns(allocator, log, language.code);
        if (!checkTooltip(language.code, shots[screens.len])) failures += 1;
    }
    if (failures != 0) {
        std.debug.print("{d} finding(s); the frames are under {s}\n", .{ failures, root });
        return error.CaptionOutsideControl;
    }
}

/// The tooltip of the player name box: a frame of the style's border colour, and inside it at least
/// a word of dark ink that keeps a pixel clear of the frame on every side. A missing letter would be
/// a hollow box of the same ink; the Game's own BK_GLYPH_TRACE line for it fails the run (play), so
/// the frame proves the tooltip is drawn and inside its box, the log that its letters exist.
fn checkTooltip(code: []const u8, frame: []const u8) bool {
    // The frame's top and bottom lines: the first row with a run of at least min_frame_run pixels
    // of the frame colour, and the next such row at least min_frame_rows below it that starts in
    // the same column (the art around has the colour in shorter strokes).
    const min_frame_run = 40;
    const min_frame_rows = 10;
    var top: ?Rect = null;
    var box: ?Rect = null;
    for (tooltip_area.y1..tooltip_area.y2) |y| {
        const run = longestRun(frame, y);
        if (run.x2 - run.x1 < min_frame_run) continue;
        if (top) |line| {
            if (y < line.y1 + min_frame_rows or run.x1 + 1 < line.x1 or run.x1 > line.x1 + 1) continue;
            box = .{ .x1 = line.x1, .y1 = line.y1, .x2 = line.x2, .y2 = y + 1 };
            break;
        }
        top = .{ .x1 = run.x1, .y1 = y, .x2 = run.x2, .y2 = y + 1 };
    }
    const found = box orelse {
        std.debug.print("{s} tooltip: FAIL, no tooltip frame near the player name box\n", .{code});
        return false;
    };
    return checkTooltipText(code, frame, found);
}

/// The longest run of the frame colour on a row of tooltip_area, as the columns it spans.
fn longestRun(frame: []const u8, y: usize) struct { x1: usize, x2: usize } {
    var best_x1: usize = 0;
    var best_x2: usize = 0;
    var start: ?usize = null;
    for (tooltip_area.x1..tooltip_area.x2 + 1) |x| {
        const is_frame = x < tooltip_area.x2 and blk: {
            const p = frame[(y * width + x) * 4 ..][0..3];
            var distance: u32 = 0;
            for (0..3) |i| distance += @abs(@as(i32, p[i]) - @as(i32, tooltip_border[i]));
            break :blk distance <= 12;
        };
        if (is_frame) {
            if (start == null) start = x;
            continue;
        }
        if (start) |first| if (x - first > best_x2 - best_x1) {
            best_x1 = first;
            best_x2 = x;
        };
        start = null;
    }
    return .{ .x1 = best_x1, .x2 = best_x2 };
}

fn checkTooltipText(code: []const u8, frame: []const u8, box: Rect) bool {
    // The frame is two lines; the text is inside them.
    const inner = Rect{ .x1 = box.x1 + 2, .y1 = box.y1 + 2, .x2 = box.x2 - 2, .y2 = box.y2 - 2 };
    var pixels: usize = 0;
    var ink: ?Rect = null;
    for (inner.y1..inner.y2) |y| for (inner.x1..inner.x2) |x| {
        const p = frame[(y * width + x) * 4 ..][0..3];
        if (@max(p[0], @max(p[1], p[2])) >= 110) continue;
        pixels += 1;
        if (ink) |*one| {
            one.x1 = @min(one.x1, x);
            one.y1 = @min(one.y1, y);
            one.x2 = @max(one.x2, x + 1);
            one.y2 = @max(one.y2, y + 1);
        } else ink = .{ .x1 = x, .y1 = y, .x2 = x + 1, .y2 = y + 1 };
    };
    const text = ink orelse Rect{ .x1 = 0, .y1 = 0, .x2 = 0, .y2 = 0 };
    const ok = box.x2 - box.x1 >= 40 and pixels >= 150 and ink != null and text.x1 > inner.x1 and text.y1 > inner.y1 and text.x2 < inner.x2 and text.y2 < inner.y2;
    std.debug.print("{s} tooltip: {s}, frame {d},{d}-{d},{d}, {d} ink pixels in {d},{d}-{d},{d}\n", .{ code, if (ok) "ok" else "FAIL", box.x1, box.y1, box.x2, box.y2, pixels, text.x1, text.y1, text.x2, text.y2 });
    return ok;
}

/// The background mod: a folder named as the mod (its UI style, Data/UI/ModStyles/achtungpanzer2, is
/// chosen by the name) whose every entry links to the mod's own but its fonts, whose atlases are
/// copies with every texel cleared (32-bit A8R8G8B8 after the 128 byte header): the menus' art
/// without a letter of their text. The mod itself is never written.
fn linkBlankMod(io: std.Io, allocator: std.mem.Allocator, blank_mods: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const folder = try std.fs.path.join(allocator, &.{ blank_mods, "AchtungPanzer2" });
    const data = try std.fs.path.join(allocator, &.{ folder, "data" });
    try cwd.createDirPath(io, data);
    const source = try std.fs.path.join(allocator, &.{ options.mod, "data" });
    var dir = try cwd.openDir(io, source, .{ .iterate = true });
    defer dir.close(io);
    var it = dir.iterate();
    while (try it.next(io)) |entry| {
        if (std.ascii.eqlIgnoreCase(entry.name, "fonts")) continue;
        try cwd.symLink(io, try std.fs.path.join(allocator, &.{ source, entry.name }), try std.fs.path.join(allocator, &.{ data, entry.name }), .{ .is_directory = entry.kind == .directory });
    }
    for ([_][]const u8{ "tiny", "small", "medium", "large" }) |size| {
        const into = try std.fs.path.join(allocator, &.{ data, "fonts", size });
        try cwd.createDirPath(io, into);
        const from = try std.fs.path.join(allocator, &.{ source, "fonts", size });
        try cwd.symLink(io, try std.fs.path.join(allocator, &.{ from, "1.tfd" }), try std.fs.path.join(allocator, &.{ into, "1.tfd" }), .{});
        const atlas = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ from, "1_h.dds" }), allocator, .limited(64 << 20));
        if (atlas.len <= 128) return error.BadAtlas;
        @memset(atlas[128..], 0);
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ into, "1_h.dds" }), .data = atlas });
    }
}

/// Each caption key the mod has shows the language's text (its BK_TEXT_TRACE source is the language,
/// its translation memory or a light rewording, not the mod's English), and the language's title of
/// the screen says something else than each of its buttons. A key the mod lacks is the language's
/// own (the language is mounted above the base game). The language's texts are the overlay's, which
/// the language mod is built from.
fn expectOwnTexts(io: std.Io, allocator: std.mem.Allocator, log: []const u8, code: []const u8, screen: Screen) !usize {
    var failures: usize = 0;
    for (0..screen.buttons.len + 1) |i| {
        const caption = if (i == 0) screen.title else screen.buttons[i - 1];
        const mod_line = try std.fmt.allocPrint(allocator, "BK_TEXT_TRACE: mod {s}.txt", .{try backslashes(allocator, caption.key)});
        if (std.mem.indexOf(u8, log, mod_line) != null) {
            std.debug.print("{s} {s} {s}: the mod's English, not the language's text ({s})\n", .{ code, screen.name, caption.name, mod_line });
            failures += 1;
        }
    }
    const title = try overlayText(io, allocator, code, screen.title.key) orelse return failures;
    for (screen.buttons) |button| {
        const text = try overlayText(io, allocator, code, button.key) orelse continue;
        if (!std.mem.eql(u8, title, text)) continue;
        std.debug.print("{s} {s}: the title and the {s} button say the same \"{s}\"\n", .{ code, screen.name, button.name, title });
        failures += 1;
    }
    return failures;
}

fn backslashes(allocator: std.mem.Allocator, key: []const u8) ![]u8 {
    const out = try allocator.dupe(u8, key);
    std.mem.replaceScalar(u8, out, '/', '\\');
    return out;
}

/// The overlay's text of a key, UTF-8, folded to lower case (ASCII and Cyrillic, as the Game's
/// precedence normalises) with its white space trimmed; null when the overlay lacks it.
fn overlayText(io: std.Io, allocator: std.mem.Allocator, code: []const u8, key: []const u8) !?[]u8 {
    const path = try std.fmt.allocPrint(allocator, "Localization/game/{s}/game_data_base/{s}.txt", .{ code, key });
    const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(1 << 20)) catch |err| switch (err) {
        error.FileNotFound => return null,
        else => return err,
    };
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return error.NotUtf16;
    const units = try allocator.alloc(u16, (bytes.len - 2) / 2);
    for (units, 0..) |*unit, i| {
        var c = std.mem.readInt(u16, bytes[2 + 2 * i ..][0..2], .little);
        if ((c >= 'A' and c <= 'Z') or (c >= 0x410 and c <= 0x42f)) c += 0x20 else if (c >= 0x400 and c <= 0x40f) c += 0x50;
        unit.* = c;
    }
    const text = try std.unicode.utf16LeToUtf8Alloc(allocator, units);
    return @constCast(std.mem.trim(u8, text, " \r\n\t"));
}

/// The large and medium sizes, where bk_text stands in for them, stand in as Noto Sans; a language
/// with letters beyond the mod's fonts (not a Latin one) must have them stand in.
fn expectSansStandIns(allocator: std.mem.Allocator, log: []const u8, code: []const u8) !usize {
    var failures: usize = 0;
    const latin = for (languages.languages) |language| {
        if (std.mem.eql(u8, language.code, code)) break std.mem.startsWith(u8, language.face, "Liberation");
    } else false;
    for ([_][]const u8{ "large", "medium" }) |size| {
        const line = try std.fmt.allocPrint(allocator, "font ttf fonts\\{s}:", .{size});
        const at = std.mem.lastIndexOf(u8, log, line) orelse {
            if (latin) continue;
            std.debug.print("{s}: the {s} size never drew on bk_text\n", .{ code, size });
            failures += 1;
            continue;
        };
        const end = std.mem.indexOfScalarPos(u8, log, at, '\n') orelse log.len;
        if (std.mem.indexOf(u8, log[at..end], "stand-in") == null) continue;
        if (std.mem.indexOf(u8, log[at..end], "reason mod font lacks the language's letters, sans stand-in") != null) continue;
        std.debug.print("{s}: the {s} size's stand-in is not the sans face: {s}\n", .{ code, size, log[at..end] });
        failures += 1;
    }
    return failures;
}

/// The run's frame of every screen, each also kept as <dir>/<screen>.tga.
fn readShots(io: std.Io, allocator: std.mem.Allocator, user: []const u8, dir: []const u8, log: []const u8) !Shots {
    try expectLine(log, dir, "BK_AUTO_UI: shot written");
    var shots: Shots = undefined;
    for (0..screens.len + 1) |i| {
        const name = if (i < screens.len) screens[i].name else "tooltip";
        const at = if (i < screens.len) screens[i].frame else tooltip_frame;
        const shot = try std.fmt.allocPrint(allocator, "{s}/autoshot_{d}_1024x768.rgba", .{ user, at });
        const frame = std.Io.Dir.cwd().readFileAlloc(io, shot, allocator, .limited(16 << 20)) catch |err| {
            std.debug.print("the Game wrote no frame of the {s} screen ({s}); its output is in {s}/menu.log\n", .{ name, shot, dir });
            return err;
        };
        try testing.expectEqual(@as(usize, width * height * 4), frame.len);
        try writeTga(io, allocator, try std.fmt.allocPrint(allocator, "{s}/{s}.tga", .{ dir, name }), frame);
        try std.Io.Dir.cwd().deleteFile(io, shot);
        shots[i] = frame;
    }
    return shots;
}

/// Runs the Game in the scratch folder; it must exit 0 without a leak or a missing glyph.
fn play(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, mods: []const u8, dir: []const u8, name: []const u8, profile_name: []const u8, args: []const []const u8, auto_ui: []const u8) ![]u8 {
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ dir, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MODS_ROOT", mods);
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_GLYPH_TRACE", "1");
    try environ.put("BK_TEXT_TRACE", "1");
    try environ.put("BK_NO_HELP", "1");
    try environ.put("BK_AUTO_UI", auto_ui);
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{ game, "-windowed", try std.fmt.allocPrint(allocator, "-profile={s}", .{profile_name}) });
    try argv.appendSlice(allocator, args);
    const result = try timed_run.run(allocator, io, .{
        .argv = argv.items,
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(600), .clock = .awake } },
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

/// The frame as an uncompressed 32-bit TGA, top row first.
fn writeTga(io: std.Io, allocator: std.mem.Allocator, path: []const u8, rgba: []const u8) !void {
    const out = try allocator.alloc(u8, 18 + rgba.len);
    @memset(out[0..18], 0);
    out[2] = 2;
    std.mem.writeInt(u16, out[12..14], width, .little);
    std.mem.writeInt(u16, out[14..16], height, .little);
    out[16] = 32;
    out[17] = 0x28;
    var i: usize = 0;
    while (i < rgba.len) : (i += 4) {
        out[18 + i] = rgba[i + 2];
        out[18 + i + 1] = rgba[i + 1];
        out[18 + i + 2] = rgba[i];
        out[18 + i + 3] = rgba[i + 3];
    }
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = out });
}

fn copyFile(io: std.Io, allocator: std.mem.Allocator, from: []const u8, to: []const u8) !void {
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, from, allocator, .limited(512 << 20));
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = to, .data = bytes });
}

fn expectLine(log: []const u8, dir: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}/menu.log\n", .{ line, dir });
    return error.GameLineMissing;
}
