//! M004 S05: old and new side by side, for the maintainer's approval before the Game's default text
//! engine flips. The installed Game runs the same four screens twice on one install, once with
//! BK_TEXT_ENGINE=bitmap (the 2003 bitmap fonts, the old look) and once with BK_TEXT_ENGINE=ttf (bk_text,
//! the new one): the main menu, the first Allied mission's briefing, the options screen with the
//! restore-defaults tooltip and the Churchill's encyclopedia page. The capture routes, labels and
//! thresholds are text_engine_game_reads_it.zig's, imported, not copied.
//!
//! Per language (en, de, ru, hi; one shard each, BK_TEXT_ENGINE_LANGS names the codes) the bitmap run
//! uses the language's mod as the language tier builds it (`lang-de`, `lang-ru`, `lang-hi`: texts only
//! since M004 S05, so the old side shows the Game's own fonts where they lack a glyph; the approved
//! captures of T02 were taken with the fonted mods) and the ttf run a texts-only mod of the same overlay (`lang-<code>-sbs`, built here
//! and removed after; `lang-hi` itself for Hindi, which is texts-only already): under bk_text a mod that ships fonts keeps drawing with
//! them, so the new look needs the texts alone. A language whose bitmap mod is not installed is noted
//! in the report and its bitmap side left out; the ttf side is still captured.
//!
//! Kept in `zig-out/local-test/text-engine-sidebyside/<code>/`: `<screen>-bitmap.png`,
//! `<screen>-ttf.png`, `<screen>-sidebyside.png` (old left, new right) and `report.txt`. The report is
//! measured by code, never by eye: per label the ink pixels inside its box, the ink's width, the
//! overflow (ink pixels in a ring of `ring` pixels around the box, where nothing of the label belongs),
//! and per frame the pixels that differ between the engines, the Game's `bk_text: engine` line and the
//! count of BK_GLYPH_TRACE lines (a run that has one fails). A label with no ink on an engine, or a
//! solid missing-glyph box, fails the tier. Approval itself is the maintainer's: the report only says
//! what was captured. The user's data is never touched (scratch XDG_DATA_HOME and BK_USER_ROOT).
const std = @import("std");
const elk = @import("elk");
const zip = @import("zip");
const measure = @import("l10n_measure");
const reads = @import("text_engine_reads");
const database = elk.database;
const packages = elk.packages;
const testing = std.testing;

const Rect = measure.Rect;
const Label = reads.Label;
const width = reads.width;
const height = reads.height;

const scratch = "zig-out/local-test/text-engine-sidebyside";
/// The ring around a label's box in which ink counts as overflow.
const ring: usize = 6;
/// A pixel differs between the engines above this channel difference.
const engine_tolerance: u8 = 16;
/// Between the two frames of a composite.
const gap: usize = 8;

const Language = struct {
    code: []const u8,
    /// The mod of the bitmap run (texts and bitmap fonts); null is the base game.
    bitmap_mod: ?[]const u8,
    /// The mod of the ttf run, when the language tier already installs a texts-only one.
    ttf_mod: ?[]const u8 = null,
};

const languages = [_]Language{
    .{ .code = "en", .bitmap_mod = null },
    .{ .code = "de", .bitmap_mod = "lang-de" },
    .{ .code = "ru", .bitmap_mod = "lang-ru" },
    .{ .code = "hi", .bitmap_mod = "lang-hi", .ttf_mod = "lang-hi" },
};

const Screen = struct {
    tag: []const u8,
    /// The name of the Game log the capture routes write for it.
    log: []const u8,
    labels: []const Label,
    /// A wrapped text, measured the same way.
    body: ?Label = null,
};

/// The four screens, in the order they are captured.
const screens = [_]Screen{
    .{ .tag = "menu", .log = "capture", .labels = &reads.labels },
    .{ .tag = "briefing", .log = "briefing", .labels = &reads.briefing_labels, .body = reads.briefing_body },
    .{ .tag = "options", .log = "options", .labels = reads.routes[0].labels },
    .{ .tag = "encyclopedia", .log = "encyclopedia", .labels = reads.routes[1].labels, .body = reads.routes[1].body },
};

fn selected(allocator: std.mem.Allocator, code: []const u8) !bool {
    const list = (try reads.envValue(allocator, "BK_TEXT_ENGINE_LANGS")) orelse return true;
    var codes = std.mem.splitScalar(u8, list, ',');
    while (codes.next()) |one| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, one, " "), code)) return true;
    return false;
}

test "old and new side by side: the Game on bitmap and on bk_text, measured and kept for approval (M004 S05)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const game = (try reads.envValue(allocator, "BK_TEXT_ENGINE_GAME")) orelse {
        std.debug.print("BK_TEXT_ENGINE_GAME is not set: run zig build text-engine-sidebyside\n", .{});
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
        var report = reads.Report{ .allocator = allocator };
        defer failures += report.failures;
        cwd.deleteTree(io, try std.fs.path.join(allocator, &.{ scratch, language.code })) catch {};
        const out = try std.fs.path.join(allocator, &.{ root, language.code });

        // The background: the bitmap path on the blank mod, so a frame minus it is the ink of the labels.
        const blank_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", reads.blank_mod });
        defer cwd.deleteTree(io, blank_folder) catch {};
        try reads.installBlankMod(io, allocator, game_root, root);
        var blank_run = try reads.Run.init(io, allocator, game, game_root, root, try std.fs.path.join(allocator, &.{ language.code, "blank" }), reads.blank_mod, false, true);
        blank_run.bitmap = true;
        const blank = try captureAll(allocator, blank_run, false);

        // Does the language's bitmap mod exist? Without it the old side cannot be captured.
        const bitmap_present = if (language.bitmap_mod) |mod| present(io, allocator, game_root, mod) else true;
        const ttf_mod: ?[]const u8 = if (language.ttf_mod) |mod| mod else if (language.bitmap_mod != null) try std.fmt.allocPrint(allocator, "lang-{s}-sbs", .{language.code}) else null;
        const ttf_folder = if (language.ttf_mod == null and ttf_mod != null) try std.fs.path.join(allocator, &.{ game_root, "mods", ttf_mod.? }) else null;
        defer if (ttf_folder) |folder| cwd.deleteTree(io, folder) catch {};
        if (ttf_folder != null) try installTextsOnlyMod(io, allocator, game_root, language.code, ttf_mod.?);

        try report.line("== {s}: old = BK_TEXT_ENGINE=bitmap ({s}), new = BK_TEXT_ENGINE=ttf ({s}); awaiting maintainer approval", .{ language.code, language.bitmap_mod orelse "base game", ttf_mod orelse "base game" });
        var old: ?[4][]u8 = null;
        if (bitmap_present) {
            var run = try reads.Run.init(io, allocator, game, game_root, root, try std.fs.path.join(allocator, &.{ language.code, "bitmap" }), language.bitmap_mod, false, false);
            run.bitmap = true;
            old = try captureAll(allocator, run, language.bitmap_mod != null);
        } else {
            try report.line("note  {s}: the bitmap mod {s} is not installed; the old side is not captured", .{ language.code, language.bitmap_mod.? });
        }
        const run = try reads.Run.init(io, allocator, game, game_root, root, try std.fs.path.join(allocator, &.{ language.code, "ttf" }), ttf_mod, true, false);
        const new = try captureAll(allocator, run, ttf_mod != null);

        for (screens, 0..) |screen, index| {
            try report.line("-- {s} {s}", .{ language.code, screen.tag });
            if (old) |frames| {
                try writeRgbaPng(io, allocator, try std.fmt.allocPrint(allocator, "{s}/{s}-bitmap.png", .{ out, screen.tag }), frames[index]);
                try measureFrame(allocator, language.code, "bitmap", frames[index], blank[index], screen, &report);
                try logFacts(io, allocator, &report, language.code, "bitmap", try std.fmt.allocPrint(allocator, "{s}/bitmap/{s}.log", .{ out, screen.log }));
            }
            try writeRgbaPng(io, allocator, try std.fmt.allocPrint(allocator, "{s}/{s}-ttf.png", .{ out, screen.tag }), new[index]);
            try measureFrame(allocator, language.code, "ttf", new[index], blank[index], screen, &report);
            try logFacts(io, allocator, &report, language.code, "ttf", try std.fmt.allocPrint(allocator, "{s}/ttf/{s}.log", .{ out, screen.log }));
            if (old) |frames| {
                try writeComposite(io, allocator, try std.fmt.allocPrint(allocator, "{s}/{s}-sidebyside.png", .{ out, screen.tag }), frames[index], new[index]);
                const differ = differing(frames[index], new[index]);
                try report.line("      {s} {s}: {d} of {d} pixels differ between the engines (above {d} of 255)", .{ language.code, screen.tag, differ, width * height, engine_tolerance });
            }
        }
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ out, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("text-engine-sidebyside: {s}: {d} measures failed; the report is {s}/{s}/report.txt\n", .{ language.code, report.failures, scratch, language.code });
    }
    if (failures != 0) return error.MeasureFailed;
}

fn present(io: std.Io, allocator: std.mem.Allocator, game_root: []const u8, mod: []const u8) bool {
    const path = std.fs.path.join(allocator, &.{ game_root, "mods", mod, "data", "texts.pak" }) catch return false;
    std.Io.Dir.cwd().access(io, path, .{}) catch return false;
    return true;
}

/// The four screens of one run, `screens` order. `keys` is whether the language is a mod whose briefing
/// texts the Game must open from it.
fn captureAll(allocator: std.mem.Allocator, run: reads.Run, keys: bool) ![4][]u8 {
    var frames: [4][]u8 = undefined;
    frames[0] = try run.capture(allocator);
    frames[1] = try run.briefing(allocator, keys);
    for (reads.routes, 0..) |route, index| frames[2 + index] = try run.route(allocator, route);
    return frames;
}

/// A mod of the language's overlay with texts and no fonts, so bk_text draws every size.
fn installTextsOnlyMod(io: std.Io, allocator: std.mem.Allocator, game_root: []const u8, code: []const u8, mod: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const db_folder = try std.fs.path.join(allocator, &.{ scratch, code, "ttf-db" });
    cwd.deleteTree(io, db_folder) catch {};
    const elk_xml = try std.fs.path.join(allocator, &.{ db_folder, "elk.xml" });
    try database.copyTree(io, "Data/ELK/game_data_base", try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    try cwd.writeFile(io, .{ .sub_path = elk_xml, .data = try cwd.readFileAlloc(io, "Data/ELK/elk.xml", allocator, .limited(1 << 20)) });
    try database.copyTree(io, try std.fmt.allocPrint(allocator, "Localization/game/{s}/game_data_base", .{code}), try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    var db = try database.Database.open(testing.allocator, io, elk_xml);
    defer db.deinit();
    const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", mod });
    cwd.deleteTree(io, mod_folder) catch {};
    const written = try packages.exportTo(testing.allocator, io, &db, 0, game_root, .{ .mod = mod }, .{});
    defer testing.allocator.free(written.path);
    const mod_xml = try std.fmt.allocPrint(allocator, "<?xml version=\"1.0\"?>\r\n<base><MODName>{s}</MODName><MODVersion>1.0</MODVersion><MODType>language</MODType><MODDesc></MODDesc></base>\r\n", .{mod});
    try database.writeFileAtomic(io, testing.allocator, try std.fs.path.join(allocator, &.{ std.fs.path.dirname(written.path).?, "mod.xml" }), mod_xml);
}

/// Per label: ink, width, overflow; failing when a label has no ink or a solid missing-glyph box.
fn measureFrame(allocator: std.mem.Allocator, code: []const u8, engine: []const u8, frame: []const u8, background: []const u8, screen: Screen, report: *reads.Report) !void {
    for (screen.labels) |label| try measureLabel(allocator, code, engine, frame, background, label, report);
    if (screen.body) |label| try measureLabel(allocator, code, engine, frame, background, label, report);
}

fn measureLabel(allocator: std.mem.Allocator, code: []const u8, engine: []const u8, frame: []const u8, background: []const u8, label: Label, report: *reads.Report) !void {
    const full = try reads.inkImage(allocator, frame, background, label);
    var lit: usize = 0;
    for (full.pixels) |p| lit += @intFromBool(p > reads.ink_threshold);
    const box = measure.inkBox(full, reads.ink_threshold);
    const box_width = if (box) |b| b.width() else 0;
    const overflow = try overflowPixels(allocator, frame, background, label);
    const wanted = if (label.small) reads.min_route_ink_pixels else reads.min_ink_pixels;
    try report.check(lit >= wanted and box_width >= reads.min_ink_width, "{s} {s} {s}: {d} ink pixels, ink {d} wide, {d} overflow pixels in the {d}px ring around the box {d},{d} to {d},{d} (ink at least {d} and {d} wide)", .{ code, engine, label.key, lit, box_width, overflow, ring, label.rect.x1, label.rect.y1, label.rect.x2, label.rect.y2, wanted, reads.min_ink_width });
    try report.check(!measure.hasSolidBox(full, reads.ink_threshold, reads.solid_w, reads.solid_h, reads.solid_fill), "{s} {s} {s}: no missing-glyph box", .{ code, engine, label.key });
}

/// Ink pixels in the `ring` around the label's box (clamped to the frame), outside the box itself.
fn overflowPixels(allocator: std.mem.Allocator, frame: []const u8, background: []const u8, label: Label) !usize {
    const rect = label.rect;
    var wide = label;
    wide.boxed = false;
    wide.rect = .{ .x1 = rect.x1 -| ring, .y1 = rect.y1 -| ring, .x2 = @min(width, rect.x2 + ring), .y2 = @min(height, rect.y2 + ring) };
    const image = try reads.inkImage(allocator, frame, background, wide);
    var count: usize = 0;
    for (0..image.height) |y| for (0..image.width) |x| {
        const px = wide.rect.x1 + x;
        const py = wide.rect.y1 + y;
        if (px >= rect.x1 and px < rect.x2 and py >= rect.y1 and py < rect.y2) continue;
        count += @intFromBool(image.pixels[y * image.width + x] > reads.ink_threshold);
    };
    return count;
}

/// The engine line and the BK_GLYPH_TRACE count of a run's log (the run already failed on a trace
/// line, so a count here is the proof that there were none).
fn logFacts(io: std.Io, allocator: std.mem.Allocator, report: *reads.Report, code: []const u8, engine: []const u8, log_path: []const u8) !void {
    const log = std.Io.Dir.cwd().readFileAlloc(io, log_path, allocator, .limited(64 << 20)) catch {
        try report.line("      {s} {s}: log {s} not readable", .{ code, engine, log_path });
        return;
    };
    var traces: usize = 0;
    var lines = std.mem.splitScalar(u8, log, '\n');
    var engine_line: []const u8 = "(no 'bk_text: engine' line: the bitmap path logs none)";
    while (lines.next()) |text| {
        if (std.mem.indexOf(u8, text, "BK_GLYPH_TRACE") != null) traces += 1;
        if (std.mem.indexOf(u8, text, "bk_text: engine") != null) engine_line = std.mem.trim(u8, text, " \r");
    }
    try report.check(traces == 0, "{s} {s}: {d} BK_GLYPH_TRACE lines in {s}; {s}", .{ code, engine, traces, log_path, engine_line });
}

fn differing(a: []const u8, b: []const u8) usize {
    var count: usize = 0;
    for (0..width * height) |i| {
        var over = false;
        for (0..3) |c| over = over or @abs(@as(i32, a[i * 4 + c]) - b[i * 4 + c]) > engine_tolerance;
        count += @intFromBool(over);
    }
    return count;
}

fn writeRgbaPng(io: std.Io, allocator: std.mem.Allocator, path: []const u8, rgba: []const u8) !void {
    const rgb = try allocator.alloc(u8, width * height * 3);
    for (0..width * height) |i| @memcpy(rgb[i * 3 ..][0..3], rgba[i * 4 ..][0..3]);
    try writePng(io, allocator, path, rgb, width, height);
}

/// The old frame on the left, the new on the right, a red strip between.
fn writeComposite(io: std.Io, allocator: std.mem.Allocator, path: []const u8, old: []const u8, new: []const u8) !void {
    const total = width * 2 + gap;
    const rgb = try allocator.alloc(u8, total * height * 3);
    for (0..height) |y| {
        const row = rgb[y * total * 3 ..][0 .. total * 3];
        for (0..width) |x| {
            @memcpy(row[x * 3 ..][0..3], old[(y * width + x) * 4 ..][0..3]);
            @memcpy(row[(width + gap + x) * 3 ..][0..3], new[(y * width + x) * 4 ..][0..3]);
        }
        for (0..gap) |x| @memcpy(row[(width + x) * 3 ..][0..3], &[_]u8{ 200, 0, 0 });
    }
    try writePng(io, allocator, path, rgb, total, height);
}

/// An RGB PNG whose zlib stream is stored (uncompressed) blocks: the std library has no encoder, and
/// the frames only need to open in any viewer.
fn writePng(io: std.Io, allocator: std.mem.Allocator, path: []const u8, rgb: []const u8, w: usize, h: usize) !void {
    const stride = w * 3 + 1;
    const raw = try allocator.alloc(u8, stride * h);
    for (0..h) |y| {
        raw[y * stride] = 0;
        @memcpy(raw[y * stride + 1 ..][0 .. w * 3], rgb[y * w * 3 ..][0 .. w * 3]);
    }
    var z: std.ArrayList(u8) = .empty;
    try z.appendSlice(allocator, &.{ 0x78, 0x01 });
    var at: usize = 0;
    while (at < raw.len) {
        const n = @min(raw.len - at, 65535);
        var head: [5]u8 = undefined;
        head[0] = @intFromBool(at + n == raw.len);
        std.mem.writeInt(u16, head[1..3], @intCast(n), .little);
        std.mem.writeInt(u16, head[3..5], ~@as(u16, @intCast(n)), .little);
        try z.appendSlice(allocator, &head);
        try z.appendSlice(allocator, raw[at..][0..n]);
        at += n;
    }
    var adler: [4]u8 = undefined;
    std.mem.writeInt(u32, &adler, std.hash.Adler32.hash(raw), .big);
    try z.appendSlice(allocator, &adler);

    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(allocator, &.{ 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a });
    var ihdr: [13]u8 = undefined;
    std.mem.writeInt(u32, ihdr[0..4], @intCast(w), .big);
    std.mem.writeInt(u32, ihdr[4..8], @intCast(h), .big);
    ihdr[8] = 8; // bit depth
    ihdr[9] = 2; // truecolour
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    try chunk(allocator, &out, "IHDR", &ihdr);
    try chunk(allocator, &out, "IDAT", z.items);
    try chunk(allocator, &out, "IEND", &.{});
    try std.Io.Dir.cwd().createDirPath(io, std.fs.path.dirname(path).?);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = out.items });
}

fn chunk(allocator: std.mem.Allocator, out: *std.ArrayList(u8), kind: *const [4]u8, data: []const u8) !void {
    var len: [4]u8 = undefined;
    std.mem.writeInt(u32, &len, @intCast(data.len), .big);
    try out.appendSlice(allocator, &len);
    try out.appendSlice(allocator, kind);
    try out.appendSlice(allocator, data);
    var crc = std.hash.Crc32.init();
    crc.update(kind);
    crc.update(data);
    var sum: [4]u8 = undefined;
    std.mem.writeInt(u32, &sum, crc.final(), .big);
    try out.appendSlice(allocator, &sum);
}

test "writePng writes a PNG the l10n decoder reads back" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const rgb = [_]u8{ 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255 };
    const path = "zig-out/local-test/text-engine-sidebyside/png-roundtrip.png";
    try writePng(testing.io, allocator, path, &rgb, 2, 2);
    const bytes = try std.Io.Dir.cwd().readFileAlloc(testing.io, path, allocator, .limited(1 << 16));
    const image = try measure.decodePng(allocator, bytes);
    try testing.expectEqual(@as(usize, 2), image.width);
    try testing.expectEqual(@as(usize, 2), image.height);
    try testing.expect(image.at(0, 0) > 200 and image.at(1, 0) < 50 and image.at(0, 1) < 50 and image.at(1, 1) > 200);
}
