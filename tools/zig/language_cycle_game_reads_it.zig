//! A live language switch on the settings screen, on the real Game, as a user made it: the Game
//! starts in German, the Gameplay tab is opened and the language row's value is clicked on through
//! the list (Default, then the nine language mods by folder name) until it is Russian, then Chinese
//! (zh-Hans), then Default, nothing saved in between. Then the cursor rests on the player name box
//! until its tooltip is up. Once in the base game and, with -Dap2-mod, under Achtung Panzer 2, whose
//! bitmap fonts have no Cyrillic, Devanagari or Han: there bk_text stands in for them only while a
//! language needs it.
//!
//! - No frame shows the new choice over the old language's texts. The click changes the row's value
//!   while the Game handles its messages; the language used to be loaded at the start of the next
//!   frame, so the frame between showed the new value ("Standard" for Default under German, a
//!   language's own name in a font without its letters) beside the old language's labels. Each frame
//!   from just before the Russian, the Chinese and the Default click to six frames after it is
//!   captured; the first frame whose value box differs from the one before the click must be the
//!   first whose tab label differs as well.
//! - No letter is missing: the run fails on a BK_GLYPH_TRACE line (play), the Game's own report of a
//!   character drawn in a font without it. Under the mod the mixed frame printed fifteen of them.
//! - The tooltip says what an English start says: the screen copied the name's tooltip into the
//!   box when it was built and never again, so after the switches it was still German (or the
//!   language the screen was built in, in a font that may lack its letters: a row of empty boxes).
//!   The tooltip's box in the last frame is compared with a run started in English that
//!   only opens the tab and hovers: at most max_tooltip_difference pixels differ.
//!
//! Frames and logs are kept under zig-out/local-test/language-cycle/<style>/. XDG_DATA_HOME,
//! BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT are the scratch folder; the user's data is never
//! touched. BK_LANGUAGE_CYCLE_GAME names the installed Game executable; `zig build
//! language-cycle-game-reads-it` sets it.
const std = @import("std");
const options = @import("ap2_options");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const languages = @import("l10n_languages.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/language-cycle";
const width = 1024;
const height = 768;
const profile = "Johannes";

const Rect = struct { x1: usize, y1: usize, x2: usize, y2: usize };

/// Where a style's screen has its parts, at 1024x768.
const Style = struct {
    name: []const u8,
    args: []const []const u8,
    /// Opens the settings screen and its Gameplay tab; the clicks start at `start`.
    open: []const u8,
    start: u32,
    value_click: []const u8,
    hover: []const u8,
    /// The language row's value, the Gameplay tab's label, and the rows the tooltip lies over (the
    /// list's selection bar is elsewhere: the clicks leave the language row selected).
    value: Rect,
    tab: Rect,
    tooltip: Rect,
};

const styles = [_]Style{
    .{
        .name = "base",
        .args = &.{"-mode=1024x768"},
        .open = "60:settings,100:msg=10003,250:click=260x256",
        .start = 400,
        .value_click = "click=900x371",
        .hover = "hover=870x211",
        .value = .{ .x1 = 760, .y1 = 360, .x2 = 940, .y2 = 384 },
        .tab = .{ .x1 = 60, .y1 = 236, .x2 = 460, .y2 = 278 },
        .tooltip = .{ .x1 = 700, .y1 = 231, .x2 = 1024, .y2 = 272 },
    },
    .{
        .name = "ap2",
        .args = &.{ "-mod=achtungpanzer2", "-mode=1024x768" },
        .open = "400:msg=10005,450:msg=10003,600:click=810x365",
        .start = 700,
        .value_click = "click=450x325",
        .hover = "hover=405x165",
        .value = .{ .x1 = 360, .y1 = 314, .x2 = 476, .y2 = 337 },
        .tab = .{ .x1 = 650, .y1 = 340, .x2 = 970, .y2 = 390 },
        .tooltip = .{ .x1 = 396, .y1 = 187, .x2 = 760, .y2 = 220 },
    },
};

/// The languages the clicks pass through, from German: the list is Default and the language mods
/// in folder order, the clicks step forward and wrap to Default.
const order = [_][]const u8{ "lang-de", "lang-es", "lang-fr", "lang-hi", "lang-id", "lang-ja", "lang-pt-br", "lang-ru", "lang-zh-hans", "Default" };
/// Clicks apart, in frames: each is released two frames after it is pressed and loaded before the next.
const click_spacing = 30;
/// The clicks whose frames are measured: to Russian, to Chinese and to Default.
const measured = [_]usize{ 7, 8, 9 };
const frames_after = 6;
/// Pixels of a box that must differ (any channel by more than 40) for its text to have changed.
const min_changed = 20;
/// Pixels around the tooltip that may differ from the English start: none differ in practice, a
/// handful for the cursor's blending.
const max_tooltip_difference = 40;

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

fn clickFrame(style: Style, click: usize) u32 {
    return style.start + @as(u32, @intCast(click - 1)) * click_spacing;
}

fn hoverFrame(style: Style) u32 {
    return clickFrame(style, order.len - 1) + 60;
}

fn shotFrame(style: Style) u32 {
    return hoverFrame(style) + 200;
}

/// The cycling run's schedule: the clicks, a shot before and after each measured one, the hover.
fn cycleSchedule(allocator: std.mem.Allocator, style: Style) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(allocator, style.open);
    for (1..order.len) |click| {
        const at = clickFrame(style, click);
        const watched = std.mem.indexOfScalar(usize, &measured, click) != null;
        if (watched) try out.print(allocator, ",{d}:shot", .{at - 1});
        try out.print(allocator, ",{d}:{s}", .{ at, style.value_click });
        if (watched) for (0..frames_after + 1) |i| try out.print(allocator, ",{d}:shot", .{at + i});
    }
    try out.print(allocator, ",{d}:{s},{d}:shot,{d}:exit", .{ hoverFrame(style), style.hover, shotFrame(style), shotFrame(style) + 40 });
    return out.items;
}

/// The English start: the tab opened and the box hovered at the same frames.
fn englishSchedule(allocator: std.mem.Allocator, style: Style) ![]u8 {
    return std.fmt.allocPrint(allocator, "{s},{d}:{s},{d}:shot,{d}:exit", .{ style.open, hoverFrame(style), style.hover, shotFrame(style), shotFrame(style) + 40 });
}

fn changed(a: []const u8, b: []const u8, rect: Rect) usize {
    var count: usize = 0;
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        const at = (y * width + x) * 4;
        var most: u32 = 0;
        for (0..3) |i| most = @max(most, @abs(@as(i32, a[at + i]) - @as(i32, b[at + i])));
        if (most > 40) count += 1;
    };
    return count;
}

test "a live language switch on the settings screen: no mixed frame, no missing letter, no stale tooltip" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_LANGUAGE_CYCLE_GAME")) orelse {
        std.debug.print("BK_LANGUAGE_CYCLE_GAME is not set: run zig build language-cycle-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const mods = try std.fs.path.join(allocator, &.{ root, "mods" });
    // Copies of the installed language mods (a link to a folder needs a privilege on Windows); the
    // gameplay mod is linked, where it is at hand.
    for (languages.languages) |language| {
        const data = try std.fs.path.join(allocator, &.{ mods, language.mod, "data" });
        try cwd.createDirPath(io, data);
        for ([_][]const u8{ "mod.xml", "texts.pak" }) |file|
            try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "mods", language.mod, "data", file }), try std.fs.path.join(allocator, &.{ data, file }));
    }
    if (options.mod.len != 0) try cwd.symLink(io, options.mod, try std.fs.path.join(allocator, &.{ mods, "AchtungPanzer2" }), .{ .is_directory = true });

    var failures: usize = 0;
    var styles_run: usize = 0;
    for (styles) |style| {
        if (std.mem.eql(u8, style.name, "ap2") and options.mod.len == 0) continue;
        styles_run += 1;
        const dir = try std.fs.path.join(allocator, &.{ root, style.name });
        const cycle_user = try std.fs.path.join(allocator, &.{ dir, "cycle", "user" });
        const english_user = try std.fs.path.join(allocator, &.{ dir, "english", "user" });
        for ([_][]const u8{ cycle_user, english_user }) |user| {
            const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
            try cwd.createDirPath(io, configs);
            try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" }), try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }));
        }
        _ = try play(io, allocator, game, cycle_user, mods, dir, "choose", &.{}, "30:set=GamePlay.Language=lang-de,60:exit");
        const log = try play(io, allocator, game, cycle_user, mods, dir, "cycle", style.args, try cycleSchedule(allocator, style));
        const english_log = try play(io, allocator, game, english_user, mods, dir, "english", style.args, try englishSchedule(allocator, style));
        for ([_][]const u8{ log, english_log }, [_][]const u8{ "cycle", "english" }) |one, name| {
            l10n_measure.expectNoMissingGlyph(one, try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ dir, name })) catch {
                std.debug.print("{s} {s}: FAIL, a letter was drawn in a font without it\n", .{ style.name, name });
                failures += 1;
            };
        }

        // The languages the clicks loaded, in order: the list must be the one the clicks count on.
        var loaded: std.ArrayList([]const u8) = .empty;
        var lines = std.mem.tokenizeAny(u8, log, "\r\n");
        while (lines.next()) |line| {
            if (!std.mem.startsWith(u8, line, "language: \"")) continue;
            const name = line["language: \"".len..];
            try loaded.append(allocator, name[0 .. std.mem.indexOfScalar(u8, name, '"') orelse name.len]);
        }
        var same_order = loaded.items.len == order.len;
        if (same_order) for (order, loaded.items) |want, got| {
            if (!std.ascii.eqlIgnoreCase(want, got)) same_order = false;
        };
        if (!same_order) {
            std.debug.print("{s}: the clicks loaded {d} languages, not de to Default through the list: see {s}/cycle.log\n", .{ style.name, loaded.items.len, dir });
            failures += 1;
            continue;
        }

        for (measured) |click| {
            const at = clickFrame(style, click);
            const before = try readShot(io, allocator, cycle_user, dir, at - 1, try std.fmt.allocPrint(allocator, "{s}-before", .{order[click]}));
            var value_at: ?usize = null;
            var tab_at: ?usize = null;
            for (0..frames_after + 1) |i| {
                const frame = try readShot(io, allocator, cycle_user, dir, at + @as(u32, @intCast(i)), try std.fmt.allocPrint(allocator, "{s}-{d}", .{ order[click], i }));
                if (value_at == null and changed(before, frame, style.value) >= min_changed) value_at = i;
                if (tab_at == null and changed(before, frame, style.tab) >= min_changed) tab_at = i;
            }
            const ok = value_at != null and tab_at != null and value_at.? == tab_at.?;
            std.debug.print("{s} to {s}: {s}, the value changes {d} frame(s) after the click, the tab label {d}\n", .{
                style.name, order[click], if (ok) "ok" else "FAIL", if (value_at) |v| @as(i64, @intCast(v)) else -1, if (tab_at) |t| @as(i64, @intCast(t)) else -1,
            });
            if (!ok) failures += 1;
        }

        const tooltip = try readShot(io, allocator, cycle_user, dir, shotFrame(style), "tooltip");
        const english = try readShot(io, allocator, english_user, dir, shotFrame(style), "tooltip-english");
        const difference = changed(tooltip, english, style.tooltip);
        const tooltip_ok = difference <= max_tooltip_difference;
        std.debug.print("{s} tooltip after the switches: {s}, {d} pixels differ from the English start (at most {d})\n", .{ style.name, if (tooltip_ok) "ok" else "FAIL", difference, max_tooltip_difference });
        if (!tooltip_ok) failures += 1;
    }
    if (failures != 0) {
        std.debug.print("{d} finding(s); the frames are under {s}\n", .{ failures, root });
        return error.LanguageSwitch;
    }
    try testing.expect(styles_run >= 1);
}

/// A frame of a run, also kept as <dir>/<name>.tga.
fn readShot(io: std.Io, allocator: std.mem.Allocator, user: []const u8, dir: []const u8, frame: u32, name: []const u8) ![]u8 {
    const shot = try std.fmt.allocPrint(allocator, "{s}/autoshot_{d}_1024x768.rgba", .{ user, frame });
    const bytes = std.Io.Dir.cwd().readFileAlloc(io, shot, allocator, .limited(16 << 20)) catch |err| {
        std.debug.print("the Game wrote no frame {d} ({s}); its output is in {s}\n", .{ frame, shot, dir });
        return err;
    };
    try testing.expectEqual(@as(usize, width * height * 4), bytes.len);
    try writeTga(io, allocator, try std.fmt.allocPrint(allocator, "{s}/{s}.tga", .{ dir, name }), bytes);
    return bytes;
}

/// Runs the Game in the scratch folder; it must exit 0 without a leak.
fn play(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, mods: []const u8, dir: []const u8, name: []const u8, args: []const []const u8, auto_ui: []const u8) ![]u8 {
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ dir, name });
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
