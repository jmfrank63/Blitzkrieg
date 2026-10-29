//! The tutorial list and the custom mission list hold each entry in a row two lines high
//! (Data/UI/Lists/SaveLoadNameIcon2.xml, 43 pixels; the medium font's line space is 21), and the
//! selection frame is drawn around that row. A header that wraps to a third line spills out of its
//! row and its frame (the maintainer saw it in Russian, 2026-10-29). This tier opens both lists on
//! the real Game in English and the nine language mods, captures them at 1024x768 and measures:
//!
//! - each entry's 43 pixel box holds one or two lines (runs of inked pixel rows apart by line_gap
//!   blank rows), and the list shows its six tutorials or three custom missions. A third line,
//!   centred like two, puts part of each of its three lines inside the box;
//! - no text ink lies more than spill rows above the first box or below the last. Two lines of
//!   Devanagari reach up to 4 rows past their box with the marks above and below them (the Hindi
//!   list, 2026-10-29), and a third line of any face reaches 8 or more. The ink within spill
//!   rows of a box but outside it is counted in the report;
//! - the first entry, which the list selects, has its ink in its box between the selection frame's
//!   top and bottom rules.
//!
//! Ink is a pixel whose channels sum below ink_threshold: the rows' text is black on grey stone,
//! whose darkest grain stays above it. The selection frame's rules are pixel rows dark across
//! nearly the whole column (rule_share), which no line of text is; they are the frame, not ink.
//!
//! The geometry is the layouts' (IMTutorialList.xml, IMCustomMission.xml): container 100 centred at
//! 256,69, list 1000 25 below it (281,94) with HeaderSize 41, HeaderTopSpace 60, the list's top
//! space 5 and VSubSpace 2, so the first row's element is at y 202 and the rows are 47 apart (43
//! plus VSubSpace on both sides); the column starts at the list's left space 10 plus HSubSpace 4, the
//! text at TextPos 20 past that (x 315). The tutorial column (412 wide, as in 2003) lets a line run
//! to x 699, over the scrollbar's border at 692-693. The column is measured to the border, and the
//! ink from 694 to 699 is counted in the report but does not fail the tier: the 2003 English lines
//! stop short of it, but Chinese breaks between any two characters, so its wrapped lines fill the
//! column to 699, and no rewording avoids that (2026-10-29: zh-Hans 10 pixels, de 3, id 4). A narrower
//! column was tried and wrapped the English tutorials 2 and 3 to three lines. The custom mission
//! name column (262 wide) ends at 549.
//!
//! The frames are kept as zig-out/local-test/list-lines/<code>/<list>.tga with the logs.
//! XDG_DATA_HOME, BK_USER_ROOT, BK_PROFILES_ROOT and BK_MODS_ROOT are the scratch folder; the
//! user's data is never touched. BK_LIST_LINES_GAME names the installed Game executable and
//! BK_LIST_LINES_LANGS the comma separated codes to run (all ten without it);
//! `zig build list-lines-game-reads-it` and its per-language shards set them.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

const scratch = "zig-out/local-test/list-lines";
const width = 1024;
const height = 768;

/// MISSION_BASE_VALUE + 175 and + 151 (Sources/src/GameTT/iMission.h).
const tutorial_list_command = "269353135";
const custom_mission_command = "269353111";

const List = struct {
    name: []const u8,
    /// The BK_AUTO_UI frame whose shot holds the list.
    shot: []const u8,
    x1: usize,
    x2: usize,
    entries: usize,
    /// The columns right of the scrollbar's border the column's text could reach.
    over: ?[2]usize = null,
};

const lists = [_]List{
    .{ .name = "tutorials", .shot = "300", .x1 = 315, .x2 = 692, .entries = 6, .over = .{ 694, 700 } },
    .{ .name = "custom-missions", .shot = "640", .x1 = 315, .x2 = 549, .entries = 3 },
};
/// Both lists in one run: the tutorial list, back to the main menu, the custom mission list.
const schedule = "60:cmd=" ++ tutorial_list_command ++ ",300:shot,320:cancel,400:cmd=" ++ custom_mission_command ++ ",640:shot,680:exit";

/// The first row's element top, the rows' pitch and the element's height.
const first_row = 202;
const row_pitch = 47;
const row_height = 43;
/// The pixel row of the rule under the column headers, whose text is above it.
const header_rule = 191;
const ink_threshold = 150;
const line_gap = 3;
const spill = 4;
/// The share of the column a pixel row of the selection frame's rule darkens.
const rule_share = 0.95;

const Language = struct { code: []const u8, mod: ?[]const u8 };
const languages = [_]Language{
    .{ .code = "en", .mod = null },
    .{ .code = "ru", .mod = "lang-ru" },
    .{ .code = "zh-hans", .mod = "lang-zh-hans" },
    .{ .code = "ja", .mod = "lang-ja" },
    .{ .code = "hi", .mod = "lang-hi" },
    .{ .code = "es", .mod = "lang-es" },
    .{ .code = "fr", .mod = "lang-fr" },
    .{ .code = "de", .mod = "lang-de" },
    .{ .code = "pt-br", .mod = "lang-pt-br" },
    .{ .code = "id", .mod = "lang-id" },
};

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

fn selected(wanted: ?[]const u8, code: []const u8) bool {
    const codes = wanted orelse return true;
    var it = std.mem.splitScalar(u8, codes, ',');
    while (it.next()) |one| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, one, " "), code)) return true;
    return false;
}

/// The inked pixels of pixel row y within the text column; 0 for a rule of the selection frame.
fn inkAt(frame: []const u8, list: List, y: usize) usize {
    var count: usize = 0;
    for (list.x1..list.x2) |x| {
        const p = frame[(y * width + x) * 4 ..][0..3];
        if (@as(u32, p[0]) + p[1] + p[2] < ink_threshold) count += 1;
    }
    return if (isRule(count, list)) 0 else count;
}

fn isRule(count: usize, list: List) bool {
    return @as(f64, @floatFromInt(count)) >= rule_share * @as(f64, @floatFromInt(list.x2 - list.x1));
}

fn isRuleRow(frame: []const u8, list: List, y: usize) bool {
    var count: usize = 0;
    for (list.x1..list.x2) |x| {
        const p = frame[(y * width + x) * 4 ..][0..3];
        if (@as(u32, p[0]) + p[1] + p[2] < ink_threshold) count += 1;
    }
    return isRule(count, list);
}

const Entry = struct { lines: usize = 0, top: usize = 0, bottom: usize = 0, ink: usize = 0 };

/// One list's capture measured: each entry's lines and ink, the ink more than spill rows outside
/// the boxes (`outside`) and within them (`spilled`), the ink over the scrollbar's border and the
/// selection frame's inside (between its top and bottom rules) around the first entry.
const Measured = struct { entries: [8]Entry, shown: usize, outside: usize, spilled: usize, over: usize, frame_top: ?usize, frame_bottom: ?usize };

fn measureList(frame: []const u8, list: List) Measured {
    var result: Measured = .{ .entries = @splat(.{}), .shown = 0, .outside = 0, .spilled = 0, .over = 0, .frame_top = null, .frame_bottom = null };
    if (list.over) |columns| for (header_rule + 1..first_row + row_pitch * list.entries) |y| for (columns[0]..columns[1]) |x| {
        const p = frame[(y * width + x) * 4 ..][0..3];
        if (@as(u32, p[0]) + p[1] + p[2] < ink_threshold) result.over += 1;
    };
    // From below the column header's rule to a row's pitch past the last box: the ink outside the
    // boxes, by its distance from the nearest box (every gap row is within spill of one).
    const last_bottom = first_row + row_pitch * (list.entries - 1) + row_height;
    for (header_rule + 1..first_row + row_pitch * (list.entries + 1)) |y| {
        const inside = y >= first_row and (y - first_row) % row_pitch < row_height and (y - first_row) / row_pitch < list.entries;
        if (inside) continue;
        const near = (y < first_row and first_row - y <= spill) or (y >= first_row and y < last_bottom) or (y >= last_bottom and y - last_bottom < spill);
        if (near) result.spilled += inkAt(frame, list, y) else result.outside += inkAt(frame, list, y);
    }
    for (result.entries[0..list.entries], 0..) |*entry, at| {
        const top = first_row + at * row_pitch;
        // A new line starts after line_gap blank pixel rows: a descender's tip can stand one blank row
        // below its letter (the English "SPGs"), the gap between two lines is 4 to 6 rows.
        var blank: usize = line_gap;
        for (top..top + row_height) |y| {
            const count = inkAt(frame, list, y);
            if (count == 0) {
                blank += 1;
                continue;
            }
            if (blank >= line_gap) entry.lines += 1;
            if (entry.ink == 0) entry.top = y;
            entry.bottom = y + 1;
            entry.ink += count;
            blank = 0;
        }
        if (entry.ink != 0) result.shown += 1;
    }
    // The frame's rules nearest above and below the first entry's ink.
    const first = result.entries[0];
    if (first.ink != 0) {
        var y = first.top;
        while (y > first_row - row_pitch) : (y -= 1) if (isRuleRow(frame, list, y)) {
            result.frame_top = y + 1;
            break;
        };
        y = first.bottom;
        while (y < first_row + row_pitch + row_height) : (y += 1) if (isRuleRow(frame, list, y)) {
            result.frame_bottom = y;
            break;
        };
    }
    return result;
}

/// A grey frame with the first row's selection rules and, for each entry, `lines` black text
/// lines 15 pixels high, 21 apart, centred in the entry's box as the Game centres them.
fn syntheticFrame(allocator: std.mem.Allocator, list: List, lines: []const usize) ![]u8 {
    const frame = try allocator.alloc(u8, width * height * 4);
    @memset(frame, 120);
    const paint = struct {
        fn row(f: []u8, x1: usize, x2: usize, y: usize) void {
            for (x1..x2) |x| @memset(f[(y * width + x) * 4 ..][0..3], 0);
        }
    }.row;
    for ([_]usize{ 200, 201, 202, 242, 243, 244 }) |y| paint(frame, list.x1 - 20, list.x2, y);
    for (lines, 0..) |n, at| {
        const centre = first_row + at * row_pitch + row_height / 2;
        const block = (n - 1) * 21 + 15;
        const top = centre - block / 2;
        for (0..n) |line| for (top + line * 21..top + line * 21 + 15) |y| paint(frame, list.x1 + 2, list.x1 + 200, y);
    }
    return frame;
}

test "the measure counts two lines inside their row and frame, and a third line outside" {
    const list = lists[0];
    const two = try syntheticFrame(testing.allocator, list, &.{ 2, 2, 1, 2, 2, 2 });
    defer testing.allocator.free(two);
    const fits = measureList(two, list);
    try testing.expectEqual(@as(usize, 0), fits.outside);
    try testing.expectEqual(list.entries, fits.shown);
    for (fits.entries[0..list.entries], [_]usize{ 2, 2, 1, 2, 2, 2 }) |entry, lines| try testing.expectEqual(lines, entry.lines);
    try testing.expectEqual(@as(?usize, 203), fits.frame_top);
    try testing.expectEqual(@as(?usize, 242), fits.frame_bottom);
    const three = try syntheticFrame(testing.allocator, list, &.{ 3, 2, 2, 2, 2, 2 });
    defer testing.allocator.free(three);
    const spills = measureList(three, list);
    // The first entry's third line reaches above its box and the rule; the next entries are fine.
    try testing.expect(spills.outside != 0);
    try testing.expectEqual(@as(usize, 3), spills.entries[0].lines);
    try testing.expectEqual(@as(usize, 2), spills.entries[1].lines);
}

test "every entry of the tutorial and custom mission lists fits its two-line row" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_LIST_LINES_GAME")) orelse {
        std.debug.print("BK_LIST_LINES_GAME is not set: run zig build list-lines-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const wanted = try envValue(allocator, "BK_LIST_LINES_LANGS");
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });

    var failures: usize = 0;
    var measured: usize = 0;
    for (languages) |language| {
        if (!selected(wanted, language.code)) continue;
        const folder = try std.fs.path.join(allocator, &.{ root, language.code });
        cwd.deleteTree(io, folder) catch {};
        const user = try std.fs.path.join(allocator, &.{ folder, "user" });
        try cwd.createDirPath(io, user);
        // A copy of the installed language mod (a link to a folder needs a privilege on Windows).
        const mods = try std.fs.path.join(allocator, &.{ folder, "mods" });
        try cwd.createDirPath(io, mods);
        if (language.mod) |mod| {
            const data = try std.fs.path.join(allocator, &.{ mods, mod, "data" });
            try cwd.createDirPath(io, data);
            for ([_][]const u8{ "mod.xml", "texts.pak" }) |file|
                try copyFile(io, allocator, try std.fs.path.join(allocator, &.{ game_root, "mods", mod, "data", file }), try std.fs.path.join(allocator, &.{ data, file }));
            _ = try play(io, allocator, game, user, mods, folder, "choose", try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{mod}));
        }
        const log = try play(io, allocator, game, user, mods, folder, "lists", schedule);
        if (language.mod) |mod| try expectLine(log, folder, "lists", try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{mod}));
        if (std.mem.count(u8, log, "BK_AUTO_UI: shot written") != lists.len) {
            std.debug.print("{s}: the Game did not write its {d} shots; its log is in {s}\n", .{ language.code, lists.len, folder });
            return error.ShotMissing;
        }
        var report: std.ArrayList(u8) = .empty;
        for (lists) |list| {
            const shot = try std.fmt.allocPrint(allocator, "{s}/autoshot_{s}_1024x768.rgba", .{ user, list.shot });
            const frame = try cwd.readFileAlloc(io, shot, allocator, .limited(16 << 20));
            try testing.expectEqual(@as(usize, width * height * 4), frame.len);
            try writeTga(io, allocator, try std.fmt.allocPrint(allocator, "{s}/{s}.tga", .{ folder, list.name }), frame);
            try cwd.deleteFile(io, shot);
            const m = measureList(frame, list);
            // The ink over the scrollbar's border is reported, not gated (see the top of the file).
            var ok = m.outside == 0 and m.shown == list.entries;
            for (m.entries[0..list.entries], 0..) |entry, at| {
                if (entry.lines == 0 or entry.lines > 2) ok = false;
                try say(allocator, &report, "{s} {s} entry {d}: {d} line(s), ink rows {d}-{d}, {d} ink pixels\n", .{ language.code, list.name, at + 1, entry.lines, entry.top, entry.bottom, entry.ink });
            }
            const framed = if (m.frame_top != null and m.frame_bottom != null) m.frame_top.? <= m.entries[0].top and m.entries[0].bottom <= m.frame_bottom.? else false;
            if (!framed) ok = false;
            try say(allocator, &report, "{s} {s}: {s}, {d} of {d} entries shown, {d} ink pixels more than {d} rows outside the rows' boxes, {d} within them (reported), {d} over the scrollbar's border (reported), selection frame inside {?d}-{?d} around the first entry's ink {d}-{d}\n", .{
                language.code, list.name, if (ok) "ok" else "FAIL", m.shown, list.entries, m.outside, spill, m.spilled, m.over, m.frame_top, m.frame_bottom, m.entries[0].top, m.entries[0].bottom,
            });
            if (!ok) failures += 1;
            measured += 1;
        }
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ folder, "report.txt" }), .data = report.items });
    }
    if (failures != 0) {
        std.debug.print("{d} list capture(s) with an entry over two lines, outside its row or its selection frame; the frames are under {s}\n", .{ failures, root });
        return error.EntryOverTwoLines;
    }
    try testing.expect(measured != 0);
}

/// One line of the shard's report.txt, printed too.
fn say(allocator: std.mem.Allocator, report: *std.ArrayList(u8), comptime format: []const u8, args: anytype) !void {
    const line = try std.fmt.allocPrint(allocator, format, args);
    std.debug.print("{s}", .{line});
    try report.appendSlice(allocator, line);
}

/// Runs the Game in the scratch folder; it must exit 0 without a leak or a missing glyph.
fn play(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, mods: []const u8, folder: []const u8, name: []const u8, auto_ui: []const u8) ![]u8 {
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ folder, name });
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
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", "-mode=1024x768" },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(300), .clock = .awake } },
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

fn expectLine(log: []const u8, folder: []const u8, name: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}/{s}.log\n", .{ line, folder, name });
    return error.GameLineMissing;
}
