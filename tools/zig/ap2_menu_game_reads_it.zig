//! The Achtung Panzer 2 mod's load menu under lang-ru, on the real Game, where the mod is at hand:
//! its content is not ours to track, so the tier reads the mod from the path -Dap2-mod names (the
//! mod's folder, holding data/) and is skipped without it. The mod is linked, never copied or
//! written, into a scratch mods folder (BK_MODS_ROOT) beside a copy of the installed lang-ru, and
//! the profile's language option is lang-ru.
//!
//! The mod's fonts have no Cyrillic, so bk_text stands in for them at the mod's cell heights, and
//! the mod's layouts were made for its own fonts. The Game opens the main menu's Load Game
//! sub-menu (window 2006 of the mod's MainMenu.xml: its 400 pixel title over three 320x50 buttons,
//! all in the mod's 40 pixel large font) and captures it at 1024x768, once in Russian and once in
//! the mod's English, which its own bitmap fonts draw. Each caption's ink is the pixels of the
//! Russian frame of its own text colour (MainMenu.xml: the title's 0xff815335, the buttons' black)
//! that the English frame does not have: the menu's art is the same in both, and its rules, logo
//! and cartridge have the title's brown and the buttons' black too. The ink must
//! - lie inside its control's rectangle: none above, below or beside it, none between controls;
//! - be one line: no run of empty rows between its first and its last row of ink;
//! - be whole: not touch the rectangle's edges, where a line too long or too tall for it is cut.
//! Under lang-ru the title and the first two buttons wrapped to two lines that overlapped the
//! controls below; the stand-in now fits each caption to its box (TextGpu::FitToBox).
//! The log names the stand-in's face: Noto Sans for the large font (a Gill Sans) and the medium
//! (Courier-like), not Liberation Serif.
//!
//! The frames are kept as zig-out/local-test/ap2-menu/game-reads-it/menu.tga and english.tga, with
//! the logs. The user's data is never touched: XDG_DATA_HOME, BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT
//! are the scratch folder. BK_AP2_GAME names the installed Game executable; `zig build
//! ap2-menu-game-reads-it -Dap2-mod=<folder>` sets it.
const std = @import("std");
const options = @import("ap2_options");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/ap2-menu/game-reads-it";
const language = "lang-ru";
const profile = "Ap2Menu";
const english_profile = "Ap2MenuEnglish";
const width = 1024;
const height = 768;

/// IMC_LOAD_GAME, the main menu's Load Game button (10004 of window 2000), then the capture.
const schedule = "400:msg=10004,600:shot,640:exit";
const shot_name = "autoshot_600_1024x768.rgba";

const Rect = struct { x1: usize, y1: usize, x2: usize, y2: usize };
const Rgb = [3]u8;

/// A caption: where its text is laid out on the 1024x768 frame and its colour. Window 2006 sits at
/// 610,267 (400 wide); its buttons are centred in it, 65 pixels apart. The title (20001, at 610,95,
/// 400x172) draws from 88 pixels down to the sub-menu.
const Caption = struct { name: []const u8, rect: Rect, color: Rgb };
const title_color = Rgb{ 0x81, 0x53, 0x35 };
const black = Rgb{ 0, 0, 0 };
const captions = [_]Caption{
    .{ .name = "title", .rect = .{ .x1 = 610, .y1 = 183, .x2 = 1010, .y2 = 267 }, .color = title_color },
    .{ .name = "load saved game", .rect = .{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 }, .color = black },
    .{ .name = "load replay", .rect = .{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 }, .color = black },
    .{ .name = "back", .rect = .{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 }, .color = black },
};
/// The menu's paper around the captions, from below the mod's logo to the fourth (empty) row.
const panel = Rect{ .x1 = 600, .y1 = 160, .x2 = 1020, .y2 = 460 };

/// The Russian frame and the English one, RGBA rows.
const Frames = struct { russian: []const u8, english: []const u8 };

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

/// A pixel of a caption's Russian text: of its colour, or for black text anything as dark (the
/// glyphs' edges blend into the paper), and not the same in the English frame, so not the art. A
/// coloured text's pixel also keeps the colour's spread of red over blue: the edges of black text
/// on the paper are a grey brown that is as near the title's brown.
fn isInk(frames: Frames, x: usize, y: usize, color: Rgb) bool {
    const at = (y * width + x) * 4;
    const p = frames.russian[at..][0..3];
    const q = frames.english[at..][0..3];
    var change: u32 = 0;
    for (0..3) |i| change = @max(change, @abs(@as(i32, p[i]) - @as(i32, q[i])));
    if (change <= 8) return false;
    if (std.mem.eql(u8, &color, &black)) return @max(p[0], @max(p[1], p[2])) < 90;
    var distance: u32 = 0;
    for (0..3) |i| distance += @abs(@as(i32, p[i]) - @as(i32, color[i]));
    const spread = @as(i32, p[0]) - @as(i32, p[2]);
    const color_spread = @as(i32, color[0]) - @as(i32, color[2]);
    return distance < 45 and @abs(spread - color_spread) < 20;
}

/// Rows a band of ink must span to count as a line of text, not a mark above one (the dots of Ё).
const min_line_rows = 5;

fn inside(rect: Rect, x: usize, y: usize) bool {
    return x >= rect.x1 and x < rect.x2 and y >= rect.y1 and y < rect.y2;
}

const Measure = struct { inside: usize, outside: usize, lines: usize, box: ?Rect };

/// The caption's ink inside its rectangle and in the panel outside every caption of its colour,
/// the number of lines its ink inside forms (bands of rows split by two empty rows, at least
/// min_line_rows tall), and the box of that ink.
fn measure(frames: Frames, caption: Caption) Measure {
    var result = Measure{ .inside = 0, .outside = 0, .lines = 0, .box = null };
    for (panel.y1..panel.y2) |y| for (panel.x1..panel.x2) |x| {
        if (!isInk(frames, x, y, caption.color)) continue;
        if (inside(caption.rect, x, y)) {
            result.inside += 1;
            if (result.box) |*box| {
                box.x1 = @min(box.x1, x);
                box.y1 = @min(box.y1, y);
                box.x2 = @max(box.x2, x + 1);
                box.y2 = @max(box.y2, y + 1);
            } else result.box = .{ .x1 = x, .y1 = y, .x2 = x + 1, .y2 = y + 1 };
            continue;
        }
        var other = false;
        for (captions) |one| {
            if (std.mem.eql(u8, &one.color, &caption.color) and inside(one.rect, x, y)) other = true;
        }
        if (!other) result.outside += 1;
    };
    const box = result.box orelse return result;
    var empty: usize = 0;
    var band: usize = 0;
    for (box.y1..box.y2 + 2) |y| {
        var ink = false;
        if (y < box.y2) for (caption.rect.x1..caption.rect.x2) |x| {
            if (isInk(frames, x, y, caption.color)) ink = true;
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

test "Achtung Panzer 2's load menu in Russian: every caption is one whole line inside its control" {
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
    const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
    try cwd.createDirPath(io, configs);
    try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" }), try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }));
    const language_data = try std.fs.path.join(allocator, &.{ mods, language, "data" });
    try cwd.createDirPath(io, language_data);
    for ([_][]const u8{ "mod.xml", "texts.pak" }) |file|
        try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "mods", language, "data", file }), try std.fs.path.join(allocator, &.{ language_data, file }));
    try cwd.symLink(io, options.mod, try std.fs.path.join(allocator, &.{ mods, "AchtungPanzer2" }), .{ .is_directory = true });

    // The mod's English first: a profile that never chose a language.
    const english_log = try play(io, allocator, game, user, mods, root, "english", english_profile, &.{ "-mod=achtungpanzer2", "-mode=1024x768" }, schedule);
    try expectLine(english_log, root, "BK_AUTO_UI: shot written");
    const shot = try std.fs.path.join(allocator, &.{ user, shot_name });
    const english = try cwd.readFileAlloc(io, shot, allocator, .limited(16 << 20));
    try testing.expectEqual(@as(usize, width * height * 4), english.len);
    try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "english.tga" }), english);
    try cwd.deleteFile(io, shot);

    _ = try play(io, allocator, game, user, mods, root, "choose", profile, &.{}, "30:set=GamePlay.Language=" ++ language ++ ",60:exit");
    const log = try play(io, allocator, game, user, mods, root, "menu", profile, &.{ "-mod=achtungpanzer2", "-mode=1024x768" }, schedule);
    try expectLine(log, root, "language: \"" ++ language ++ "\" mounted");
    try expectLine(log, root, "BK_AUTO_UI: shot written");
    const frame = try cwd.readFileAlloc(io, shot, allocator, .limited(16 << 20));
    try testing.expectEqual(@as(usize, width * height * 4), frame.len);
    try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "menu.tga" }), frame);
    const frames = Frames{ .russian = frame, .english = english };

    var failures: usize = 0;
    for (captions) |caption| {
        const m = measure(frames, caption);
        const box = m.box orelse Rect{ .x1 = 0, .y1 = 0, .x2 = 0, .y2 = 0 };
        std.debug.print("{s}: {d} ink pixels inside {d},{d}-{d},{d}, {d} of its colour outside every caption, {d} line(s), ink box {d},{d}-{d},{d}\n", .{
            caption.name, m.inside, caption.rect.x1, caption.rect.y1, caption.rect.x2, caption.rect.y2, m.outside, m.lines, box.x1, box.y1, box.x2, box.y2,
        });
        const whole = m.box != null and box.x1 > caption.rect.x1 and box.y1 > caption.rect.y1 and box.x2 < caption.rect.x2 and box.y2 < caption.rect.y2;
        if (m.inside < 200 or m.outside != 0 or m.lines != 1 or !whole) failures += 1;
    }
    var serif_sizes: usize = 0;
    for ([_][]const u8{ "large", "medium" }) |size| {
        const line = try std.fmt.allocPrint(allocator, "font ttf fonts\\{s}:", .{size});
        const at = std.mem.lastIndexOf(u8, log, line) orelse {
            std.debug.print("the {s} size never drew on bk_text; the Game's output is in {s}/menu.log\n", .{ size, root });
            return error.FontEngine;
        };
        const end = std.mem.indexOfScalarPos(u8, log, at, '\n') orelse log.len;
        if (std.mem.indexOf(u8, log[at..end], "reason mod font lacks the language's letters, sans stand-in") != null) continue;
        std.debug.print("the {s} size's stand-in is not the sans face: {s}\n", .{ size, log[at..end] });
        serif_sizes += 1;
    }
    if (failures != 0) {
        std.debug.print("{d} caption(s) not one whole line inside their control; the frame is {s}/menu.tga\n", .{ failures, root });
        return error.CaptionOutsideControl;
    }
    if (serif_sizes != 0) return error.StandInFace;
}

/// Runs the Game in the scratch folder; it must exit 0 without a leak or a missing glyph.
fn play(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, mods: []const u8, root: []const u8, name: []const u8, profile_name: []const u8, args: []const []const u8, auto_ui: []const u8) ![]u8 {
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MODS_ROOT", mods);
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_GLYPH_TRACE", "1");
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

fn expectLine(log: []const u8, root: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}/menu.log\n", .{ line, root });
    return error.GameLineMissing;
}
