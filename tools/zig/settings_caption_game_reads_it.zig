//! The settings screen's caption, "Settings of <profile>" (Textes\UI\OptionsSettings\caption_settings_of
//! and the profile's name, InterfaceOptionsSettings.cpp SetCaptionTexts), on the real Game in English
//! and under lang-ru, for a profile named Johannes, captured at 1024x768 and measured.
//!
//! - The base game draws it left aligned (Data/UI/OptionsSettings.xml element 20000: TextAlign 0x21 at
//!   30,30 with a 30 pixel text inset), so its ink starts at 60 in both languages, plus the first
//!   letter's side bearing (bearing_tolerance: the English 'S' starts at 63, the Russian 'Н' at 61).
//! - The Achtung Panzer 2 style of the screen (Data/UI/ModStyles/achtungpanzer2/OptionsSettings.xml,
//!   -Dap2-mod; skipped without it) centres it in its 400 pixel window at 610,180, over the tab buttons
//!   centred at 810. A centred text is shifted by its TextPos x (UIBasic.cpp, the text rectangle moves
//!   as a whole), and the style kept the base game's inset of 30: every caption was centred at 840.
//!   The English one is short enough to pass for centred; the Russian "Настройки: Johannes", in the
//!   wider stand-in face, ran to the paper's right edge. The ink's centre must be within
//!   centre_tolerance of 810 in both languages.
//! The ink is the pixels of the caption's colour (the base game's gold 0xFFe29b00, the mod's brown
//! 0xff815335) inside the caption's window: nothing else in it has that colour.
//!
//! The frames are kept as zig-out/local-test/settings-caption/<style>-<code>.tga with
//! the logs. XDG_DATA_HOME, BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT are the scratch folder; the
//! user's data is never touched. BK_SETTINGS_CAPTION_GAME names the installed Game executable;
//! `zig build settings-caption-game-reads-it` sets it.
const std = @import("std");
const options = @import("ap2_options");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/settings-caption";
const width = 1024;
const height = 768;
const profile = "Johannes";

/// How far the ink's centre may be from the centre of its window: the first and the last letter's
/// side bearings differ by a pixel or two.
const centre_tolerance = 2.0;
/// How far right of a left aligned text's start its first column of ink may be: the first letter's
/// left side bearing (3 pixels for the base game's large 'S').
const bearing_tolerance = 4.0;

const Rect = struct { x1: usize, y1: usize, x2: usize, y2: usize };
const Rgb = [3]u8;

const Style = struct {
    name: []const u8,
    /// The Game's arguments for the style (the mod) and the frame its schedule captures.
    args: []const []const u8,
    schedule: []const u8,
    frame: u32,
    window: Rect,
    color: Rgb,
    /// Left aligned: where the ink starts; centred: null, the window's centre.
    left: ?f64,
};

/// The base game's main menu opens Options > Settings from `settings`; the mod's from its options
/// button (IMC_SETTINGS, 10005) and then Settings (10003).
const styles = [_]Style{
    .{ .name = "base", .args = &.{"-mode=1024x768"}, .schedule = "60:settings,100:msg=10003,300:shot,340:exit", .frame = 300, .window = .{ .x1 = 30, .y1 = 30, .x2 = 880, .y2 = 100 }, .color = .{ 0xe2, 0x9b, 0x00 }, .left = 60 },
    .{ .name = "ap2", .args = &.{ "-mod=achtungpanzer2", "-mode=1024x768" }, .schedule = "400:msg=10005,450:msg=10003,700:shot,740:exit", .frame = 700, .window = .{ .x1 = 610, .y1 = 180, .x2 = 1010, .y2 = 230 }, .color = .{ 0x81, 0x53, 0x35 }, .left = null },
};

const Language = struct { code: []const u8, mod: ?[]const u8 };
const caption_languages = [_]Language{ .{ .code = "en", .mod = null }, .{ .code = "ru", .mod = "lang-ru" } };

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

const Ink = struct { box: Rect, pixels: usize };

/// The caption's ink in its window: pixels within 40 of its colour (summed over the channels).
fn ink(frame: []const u8, window: Rect, color: Rgb) ?Ink {
    var result: ?Ink = null;
    for (window.y1..window.y2) |y| for (window.x1..window.x2) |x| {
        const p = frame[(y * width + x) * 4 ..][0..3];
        var distance: u32 = 0;
        for (0..3) |i| distance += @abs(@as(i32, p[i]) - @as(i32, color[i]));
        if (distance >= 40) continue;
        if (result) |*one| {
            one.box.x1 = @min(one.box.x1, x);
            one.box.y1 = @min(one.box.y1, y);
            one.box.x2 = @max(one.box.x2, x + 1);
            one.box.y2 = @max(one.box.y2, y + 1);
            one.pixels += 1;
        } else result = .{ .box = .{ .x1 = x, .y1 = y, .x2 = x + 1, .y2 = y + 1 }, .pixels = 1 };
    };
    return result;
}

test "the settings caption sits where its layout puts it, in English and Russian" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_SETTINGS_CAPTION_GAME")) orelse {
        std.debug.print("BK_SETTINGS_CAPTION_GAME is not set: run zig build settings-caption-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const mods = try std.fs.path.join(allocator, &.{ root, "mods" });
    try cwd.createDirPath(io, mods);
    // A copy of the installed lang-ru (a link to a folder needs a privilege on Windows); the mod is
    // linked, as ap2-menu-game-reads-it does, only where it is at hand.
    const language_data = try std.fs.path.join(allocator, &.{ mods, "lang-ru", "data" });
    try cwd.createDirPath(io, language_data);
    for ([_][]const u8{ "mod.xml", "texts.pak" }) |file|
        try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "mods", "lang-ru", "data", file }), try std.fs.path.join(allocator, &.{ language_data, file }));
    if (options.mod.len != 0) try cwd.symLink(io, options.mod, try std.fs.path.join(allocator, &.{ mods, "AchtungPanzer2" }), .{ .is_directory = true });

    var failures: usize = 0;
    var measured: usize = 0;
    for (caption_languages) |language| {
        // One user folder per language: the profile is Johannes in both.
        const user = try std.fs.path.join(allocator, &.{ root, language.code, "user" });
        const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
        try cwd.createDirPath(io, configs);
        try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" }), try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }));
        if (language.mod) |mod| _ = try play(io, allocator, game, user, mods, root, try std.fmt.allocPrint(allocator, "{s}-choose", .{language.code}), &.{}, try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{mod}));
        for (styles) |style| {
            if (std.mem.eql(u8, style.name, "ap2") and options.mod.len == 0) continue;
            const name = try std.fmt.allocPrint(allocator, "{s}-{s}", .{ style.name, language.code });
            const log = try play(io, allocator, game, user, mods, root, name, style.args, style.schedule);
            if (language.mod) |mod| try expectLine(log, root, name, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{mod}));
            const shot = try std.fmt.allocPrint(allocator, "{s}/autoshot_{d}_1024x768.rgba", .{ user, style.frame });
            const frame = try cwd.readFileAlloc(io, shot, allocator, .limited(16 << 20));
            try testing.expectEqual(@as(usize, width * height * 4), frame.len);
            try writeTga(io, allocator, try std.fmt.allocPrint(allocator, "{s}/{s}.tga", .{ root, name }), frame);
            try cwd.deleteFile(io, shot);
            const found = ink(frame, style.window, style.color) orelse {
                std.debug.print("{s}: no ink of the caption's colour in {d},{d}-{d},{d}\n", .{ name, style.window.x1, style.window.y1, style.window.x2, style.window.y2 });
                failures += 1;
                continue;
            };
            const box = found.box;
            const centre = @as(f64, @floatFromInt(box.x1 + box.x2)) / 2.0;
            const expected = style.left orelse @as(f64, @floatFromInt(style.window.x1 + style.window.x2)) / 2.0;
            const at = if (style.left != null) @as(f64, @floatFromInt(box.x1)) else centre;
            // The first column of a left aligned text is its glyph's left side bearing to the right.
            const off = if (style.left != null) at - expected else @abs(at - expected);
            const ok = found.pixels >= 200 and off >= 0.0 and off <= @as(f64, if (style.left != null) bearing_tolerance else centre_tolerance);
            std.debug.print("{s}: {s}, {d} ink pixels, box {d},{d}-{d},{d}, {s} {d:.1} where the layout puts {d:.1} ({d:.1} off)\n", .{
                name, if (ok) "ok" else "FAIL", found.pixels, box.x1, box.y1, box.x2, box.y2, if (style.left != null) "first column" else "centre", at, expected, at - expected,
            });
            if (!ok) failures += 1;
            measured += 1;
        }
    }
    if (failures != 0) {
        std.debug.print("{d} caption(s) not where the layout puts them; the frames are under {s}\n", .{ failures, root });
        return error.CaptionMisplaced;
    }
    try testing.expect(measured >= 2);
}

/// Runs the Game in the scratch folder; it must exit 0 without a leak or a missing glyph.
fn play(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, mods: []const u8, root: []const u8, name: []const u8, args: []const []const u8, auto_ui: []const u8) ![]u8 {
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("HOME", user);
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
    try argv.appendSlice(allocator, &.{ game, "-windowed", "-profile=" ++ profile });
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
        out[18 + i + 3] = 255;
    }
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = out });
}

fn copyFile(io: std.Io, allocator: std.mem.Allocator, from: []const u8, to: []const u8) !void {
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, from, allocator, .limited(512 << 20));
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = to, .data = bytes });
}

fn expectLine(log: []const u8, root: []const u8, name: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}/{s}.log\n", .{ line, root, name });
    return error.GameLineMissing;
}
