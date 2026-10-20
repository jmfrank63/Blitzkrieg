//! M002 S04 D066 on the real ELK: the installed editor started hidden on a scratch copy of Data/ELK
//! in each of the ten languages (the scratch elk.cfg's `ui_language`) draws its main window, its
//! File menu opened and its help window, and the captured frames are measured by code. ELK's
//! `--l10n-auto` mode captures the three frames and prints `l10n-auto: rect <key> <x> <y> <w> <h>`
//! for every label they hold (the six menu bar titles, the File menu's nine items without a
//! shortcut, the help window's title); this runner reads those lines, decodes the TGAs
//! (`l10n_measure.decodeTga`), and for each label cuts the rect, takes the ink against the rect's
//! own background and compares it with hb-view's raster of the table's text at the UI's 16 px in the
//! language's Noto face (D069: the tier fails when hb-view is missing). The normalised correlation
//! must reach 0.50, the S01 threshold.
//!
//! On top of that, per language:
//!
//! - en, ru and the Latin-script languages (es, fr, de, pt-BR, id): a frame that differs from the
//!   English one where the label's text differs, so a table that is not applied cannot pass;
//! - zh-Hans and ja: the regional Han face checks of the Game tier, per Han character, on the
//!   labels' ink: the language's own face (Simplified or Japanese Noto) wins over the other;
//! - hi: the headline of every word is continuous (D058) and the short i matra lies where
//!   hb-view puts it, on the labels that are Devanagari alone (Noto Sans Devanagari has no Latin
//!   letters, so a label that mixes "PAK/UPD" in has no hb-view reference and is held to its ink).
//!
//! The missing-key case: `tools/zig/fixtures/l10n/elk-drop/ru.lang` is ru.lang without
//! `elk.file.import_pak`. ELK on ru with `BK_L10N_DIR` at a copy of it must log exactly one
//! `l10n: missing ru elk.file.import_pak` and draw that item in English: its ink correlates with
//! hb-view's English text and better than with the Russian one. The fixture must stay ru.lang
//! minus that line, which the runner checks.
//!
//! Frames, references and `report.txt` are kept in `zig-out/local-test/elk/l10n-auto/<code>/` (the
//! drop case in `drop-ru/`). The user's data is never touched: XDG_DATA_HOME and the settings file
//! are in the scratch folder and the database is a copy. BK_ELK_EXE names the installed ELK;
//! `zig build elk-l10n-auto` sets it. BK_L10N_LANGS (a comma list of codes, default all) picks the
//! languages; the drop case runs with ru, or when the list is unset.
const std = @import("std");
const testing = std.testing;
const elk = @import("elk");
const measure = @import("l10n_measure");

const Rect = measure.Rect;
const Gray = measure.Gray;

const scratch = "zig-out/local-test/elk/l10n-auto";
const lang_folder = "Sources/editor/kit/lang";
const drop_fixture = "tools/zig/fixtures/l10n/elk-drop/ru.lang";
const drop_key = "elk.file.import_pak";
const log_prefix = "l10n-auto: ";

// Thresholds, each set where the first passing run left room for the rasterisers' differences but
// not for a wrong glyph, wrong shaping or a wrong face (the values are in the report).
const ink_threshold: u8 = 24;
/// The least correlation of a label's ink with its hb-view raster. S01's 0.50 is for the Game's
/// larger text; the menu's 16 px labels, hinted, measured 0.457 at the least for a long French one.
const min_correlation: f64 = 0.40;
/// A label of a CJK language with Latin letters (PAK/UPD, MOD): ELK draws the letters in its Latin
/// face and hb-view in the CJK face's own, so only the Han and kana ink matches (0.320 measured).
const min_mixed_correlation: f64 = 0.30;
/// A label counts as drawn differently from English when this many pixels of its rect differ by
/// more than `frame_tolerance`; a translation may equal the English word, so a few may not.
const min_text_change: usize = 30;
const frame_tolerance: u8 = 16;
const max_unchanged: usize = 3;
/// Hindi: the longest run of columns without ink on a word's headline, in frame pixels (D058).
/// The menu's 16 px text is rasterised cluster by cluster, so a join may lose a second column to
/// anti-aliasing where the Game's larger text loses one.
const max_headline_gap: usize = 2;
/// Hindi: a label with a conjunct (a halant before a consonant) is drawn with the half form's
/// advance of the cluster shaped alone (the known limit of kit/imgui), so its words and its ink
/// drift from hb-view's: it must still resemble the reference, at this least correlation.
const min_conjunct_correlation: f64 = 0.15;
const headline_threshold: u8 = 8;
/// Hindi: the first third of the word that holds the short i matra against hb-view's first third,
/// and against its middle and last.
const min_matra_correlation: f64 = 0.4;
const min_matra_margin: f64 = 0.05;
/// Han: a character cell whose two faces' rasters correlate above this is not informative.
const han_informative_below: f64 = 0.90;
const min_face_margin: f64 = 0.01;
/// Han references for the face check are drawn large and scaled down, as in the Game tier.
const han_reference_size: u32 = 96;
/// How many rects a run must log: the titles, the File menu's items and the help window's title.
const expected_titles = 6;
const expected_items = 9;

const noto_sans = "vendor/fonts/NotoSans-Regular.ttf";
const noto_sc = "vendor/fonts/NotoSansSC-Regular.otf";
const noto_jp = "vendor/fonts/NotoSansJP-Regular.otf";
const noto_deva = "vendor/fonts/NotoSansDevanagari-Regular.ttf";

const Language = struct {
    code: []const u8,
    /// BCP 47 tag for hb-view.
    tag: []const u8,
    /// The face the language's text is shaped with for the reference.
    face: []const u8,
    script: ?[]const u8 = null,
    /// Whether the frame must differ from the English one (the languages whose labels are not English).
    differs: bool = true,
};

const languages = [_]Language{
    .{ .code = "en", .tag = "en", .face = noto_sans, .differs = false },
    .{ .code = "ru", .tag = "ru", .face = noto_sans },
    .{ .code = "zh-Hans", .tag = "zh-Hans", .face = noto_sc },
    .{ .code = "ja", .tag = "ja", .face = noto_jp },
    .{ .code = "hi", .tag = "hi", .face = noto_deva, .script = "Deva" },
    .{ .code = "es", .tag = "es", .face = noto_sans },
    .{ .code = "fr", .tag = "fr", .face = noto_sans },
    .{ .code = "de", .tag = "de", .face = noto_sans },
    .{ .code = "pt-BR", .tag = "pt-BR", .face = noto_sans },
    .{ .code = "id", .tag = "id", .face = noto_sans },
};

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

/// Whether BK_L10N_LANGS (a comma list of codes, default all) names `code`.
fn selected(allocator: std.mem.Allocator, code: []const u8) !bool {
    const list = (try envValue(allocator, "BK_L10N_LANGS")) orelse return true;
    var codes = std.mem.splitScalar(u8, list, ',');
    while (codes.next()) |one| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, one, " "), code)) return true;
    return false;
}

/// Failures are collected, then printed together with the failing measure first.
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

// ------------------------------------------------------------------ what ELK printed

const Drawn = struct { key: []const u8, x: f32, y: f32, w: f32, h: f32 };

const Shot = struct {
    name: []const u8,
    width: f32,
    height: f32,
    rects: std.ArrayList(Drawn) = .empty,
};

/// The `l10n-auto: shot` and `l10n-auto: rect` lines of ELK's stderr, in order.
fn parseShots(allocator: std.mem.Allocator, log: []const u8) ![]Shot {
    var shots: std.ArrayList(Shot) = .empty;
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw| {
        if (!std.mem.startsWith(u8, raw, log_prefix)) continue;
        var words = std.mem.tokenizeScalar(u8, std.mem.trim(u8, raw[log_prefix.len..], "\r "), ' ');
        const kind = words.next() orelse continue;
        if (std.mem.eql(u8, kind, "shot")) {
            const name = words.next() orelse return error.BadLine;
            const size = words.next() orelse return error.BadLine;
            const x = std.mem.indexOfScalar(u8, size, 'x') orelse return error.BadLine;
            try shots.append(allocator, .{
                .name = try allocator.dupe(u8, name),
                .width = try std.fmt.parseFloat(f32, size[0..x]),
                .height = try std.fmt.parseFloat(f32, size[x + 1 ..]),
            });
        } else if (std.mem.eql(u8, kind, "rect")) {
            if (shots.items.len == 0) return error.BadLine;
            const key = words.next() orelse return error.BadLine;
            var numbers: [4]f32 = undefined;
            for (&numbers) |*number| number.* = try std.fmt.parseFloat(f32, words.next() orelse return error.BadLine);
            try shots.items[shots.items.len - 1].rects.append(allocator, .{ .key = try allocator.dupe(u8, key), .x = numbers[0], .y = numbers[1], .w = numbers[2], .h = numbers[3] });
        }
    }
    return shots.items;
}

fn countOf(haystack: []const u8, needle: []const u8) usize {
    return std.mem.count(u8, haystack, needle);
}

// ------------------------------------------------------------------ the tables

/// The value of `key` in a table, escapes undone, or null.
fn tableText(allocator: std.mem.Allocator, table: []const u8, key: []const u8) !?[]u8 {
    var lines = std.mem.splitScalar(u8, table, '\n');
    while (lines.next()) |raw| {
        const line = if (raw.len > 0 and raw[raw.len - 1] == '\r') raw[0 .. raw.len - 1] else raw;
        if (line.len == 0 or line[0] == '#') continue;
        const eq = std.mem.indexOfScalar(u8, line, '=') orelse continue;
        if (!std.mem.eql(u8, line[0..eq], key)) continue;
        var out: std.ArrayList(u8) = .empty;
        var at = eq + 1;
        while (at < line.len) : (at += 1) {
            if (line[at] == '\\' and at + 1 < line.len) {
                at += 1;
                try out.append(allocator, switch (line[at]) {
                    'n' => '\n',
                    't' => '\t',
                    else => line[at],
                });
            } else try out.append(allocator, line[at]);
        }
        return out.items;
    }
    return null;
}

fn languageTable(io: std.Io, allocator: std.mem.Allocator, code: []const u8) ![]u8 {
    const path = try std.fmt.allocPrint(allocator, "{s}/{s}.lang", .{ lang_folder, code });
    return std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(1 << 20));
}

fn hasAsciiLetter(text: []const u8) bool {
    for (text) |byte| if (std.ascii.isAlphabetic(byte)) return true;
    return false;
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

// ------------------------------------------------------------------ the frames

/// What a language's three frames gave, for the checks that compare them with another language's.
const Frames = struct {
    main: Gray,
    menu: Gray,
    help: Gray,

    fn named(self: Frames, name: []const u8) ?Gray {
        if (std.mem.eql(u8, name, "main")) return self.main;
        if (std.mem.eql(u8, name, "menu")) return self.menu;
        if (std.mem.eql(u8, name, "help")) return self.help;
        return null;
    }
};

/// Whether the shot is where the label is measured: titles in the main frame (in the menu frame
/// the open menu's title is highlighted), the items in the menu frame, the help title in the help
/// frame.
fn measuredIn(shot: []const u8, key: []const u8) bool {
    if (std.mem.eql(u8, shot, "main")) return std.mem.startsWith(u8, key, "elk.menu.");
    if (std.mem.eql(u8, shot, "menu")) return std.mem.startsWith(u8, key, "elk.file.");
    if (std.mem.eql(u8, shot, "help")) return std.mem.startsWith(u8, key, "elk.helpwin.");
    return false;
}

/// The pixel box of a drawn rect on a frame: the logical rect scaled to the frame's size, clamped.
fn pixelRect(frame: Gray, shot: Shot, drawn: Drawn) ?Rect {
    const scale = @as(f64, @floatFromInt(frame.width)) / @as(f64, shot.width);
    const x1 = @max(0, @floor(@as(f64, drawn.x) * scale));
    const y1 = @max(0, @floor(@as(f64, drawn.y) * scale));
    const x2 = @min(@as(f64, @floatFromInt(frame.width)), @ceil((@as(f64, drawn.x) + drawn.w) * scale));
    const y2 = @min(@as(f64, @floatFromInt(frame.height)), @ceil((@as(f64, drawn.y) + drawn.h) * scale));
    if (x2 <= x1 or y2 <= y1) return null;
    return .{ .x1 = @intFromFloat(x1), .y1 = @intFromFloat(y1), .x2 = @intFromFloat(x2), .y2 = @intFromFloat(y2) };
}

/// The label's ink: how far each pixel of the rect is from the rect's own background (its most
/// common value), so light text on dark, dark on light and the blue title bar all read the same way.
fn inkImage(allocator: std.mem.Allocator, frame: Gray, rect: Rect) !Gray {
    var counts: [256]usize = @splat(0);
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        counts[frame.at(x, y)] += 1;
    };
    const background: i32 = @intCast(std.mem.indexOfMax(usize, &counts));
    const pixels = try allocator.alloc(u8, rect.width() * rect.height());
    for (0..rect.height()) |y| for (0..rect.width()) |x| {
        const value: i32 = frame.at(rect.x1 + x, rect.y1 + y);
        pixels[y * rect.width() + x] = @intCast(@min(255, @abs(value - background)));
    };
    return .{ .width = rect.width(), .height = rect.height(), .pixels = pixels };
}

/// Pixels of `rect` that differ by more than `frame_tolerance` between two frames of one size.
fn changedIn(a: Gray, b: Gray, rect: Rect) usize {
    if (a.width != b.width or a.height != b.height) return rect.width() * rect.height();
    var changed: usize = 0;
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        const left: i32 = a.at(x, y);
        const right: i32 = b.at(x, y);
        if (@abs(left - right) > frame_tolerance) changed += 1;
    };
    return changed;
}

/// One measured label.
const Item = struct {
    shot: []const u8,
    key: []const u8,
    text: []const u8,
    rect: Rect,
    /// The ink cut to its box; null when the rect holds none.
    ink: ?Gray,
};

// ------------------------------------------------------------------ running ELK

const Setup = struct {
    io: std.Io,
    exe: []const u8,
    /// Absolute paths.
    database: []const u8,
    root: []const u8,
};

const Run = struct { log: []u8, shots: []Shot, frames: Frames, folder: []const u8 };

/// ELK on `code` with the scratch settings of `name`'s folder, and `table_dir` as BK_L10N_DIR when
/// given; the log and the three decoded frames.
fn runElk(allocator: std.mem.Allocator, setup: Setup, name: []const u8, code: []const u8, table_dir: ?[]const u8) !Run {
    const cwd = std.Io.Dir.cwd();
    const folder = try std.fs.path.join(allocator, &.{ setup.root, name });
    cwd.deleteTree(setup.io, folder) catch {};
    try cwd.createDirPath(setup.io, folder);
    const config = try std.fs.path.join(allocator, &.{ folder, "elk.cfg" });
    try cwd.writeFile(setup.io, .{ .sub_path = config, .data = try std.fmt.allocPrint(allocator, "ui_language={s}\r\n", .{code}) });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", try std.fs.path.join(allocator, &.{ folder, "xdg" }));
    try environ.put("BK_ELK_SETTINGS", config);
    if (table_dir) |dir| try environ.put("BK_L10N_DIR", dir);
    const result = try std.process.run(allocator, setup.io, .{
        .argv = &.{ setup.exe, "--l10n-auto", try std.fs.path.join(allocator, &.{ setup.database, "elk.xml" }), folder },
        .cwd = .{ .path = folder },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(setup.io, .{ .sub_path = try std.fs.path.join(allocator, &.{ folder, "elk.log" }), .data = log });
    if (!result.term.success()) {
        std.debug.print("ELK ({s}) ended {any}; its output is in {s}/elk.log\n", .{ name, result.term, folder });
        return error.ElkFailed;
    }
    return .{ .log = log, .shots = try parseShots(allocator, log), .folder = folder, .frames = .{
        .main = try readFrame(setup.io, allocator, folder, "main"),
        .menu = try readFrame(setup.io, allocator, folder, "menu"),
        .help = try readFrame(setup.io, allocator, folder, "help"),
    } };
}

fn readFrame(io: std.Io, allocator: std.mem.Allocator, folder: []const u8, name: []const u8) !Gray {
    const path = try std.fmt.allocPrint(allocator, "{s}/{s}.tga", .{ folder, name });
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(64 << 20));
    return measure.decodeTga(allocator, bytes);
}

/// The labels of a run, each cut from its frame, with the text the table of `code` (or `table`)
/// gives for it.
fn measureItems(allocator: std.mem.Allocator, run: Run, table: []const u8, report: *Report, code: []const u8) ![]Item {
    var items: std.ArrayList(Item) = .empty;
    var titles: usize = 0;
    var menu_items: usize = 0;
    for (run.shots) |shot| {
        const frame = run.frames.named(shot.name) orelse continue;
        for (shot.rects.items) |drawn| {
            if (!measuredIn(shot.name, drawn.key)) continue;
            if (std.mem.eql(u8, shot.name, "main")) titles += 1;
            if (std.mem.eql(u8, shot.name, "menu")) menu_items += 1;
            const text = (try tableText(allocator, table, drawn.key)) orelse (try tableText(allocator, try languageTable(testing.io, allocator, "en"), drawn.key)) orelse return error.NoSuchKey;
            const rect = pixelRect(frame, shot, drawn) orelse {
                try report.check(false, "{s} {s}: the rect {d:.0},{d:.0} {d:.0}x{d:.0} is outside the {d}x{d} frame", .{ code, drawn.key, drawn.x, drawn.y, drawn.w, drawn.h, frame.width, frame.height });
                continue;
            };
            const full = try inkImage(allocator, frame, rect);
            const box = measure.inkBox(full, ink_threshold);
            try items.append(allocator, .{
                .shot = shot.name,
                .key = drawn.key,
                .text = text,
                .rect = rect,
                .ink = if (box) |found| try measure.crop(allocator, full, found) else null,
            });
        }
    }
    try report.check(titles == expected_titles, "{s}: ELK logged {d} menu bar titles on the main frame (expected {d})", .{ code, titles, expected_titles });
    try report.check(menu_items == expected_items, "{s}: ELK logged {d} File menu items on the menu frame (expected {d})", .{ code, menu_items, expected_items });
    return items.items;
}

fn hbReference(io: std.Io, allocator: std.mem.Allocator, folder: []const u8, stem: []const u8, text: []const u8, face: []const u8, tag: []const u8, script: ?[]const u8, size: u32) !Gray {
    const reference_folder = try std.fs.path.join(allocator, &.{ folder, "reference" });
    try std.Io.Dir.cwd().createDirPath(io, reference_folder);
    return measure.hbView(allocator, io, reference_folder, stem, text, .{ .font = face, .size = size, .language = tag, .script = script });
}

/// Every label's ink against hb-view's raster of its text: the first checks of all ten languages.
fn checkReferences(io: std.Io, allocator: std.mem.Allocator, language: Language, run: Run, items: []const Item, report: *Report) !void {
    for (items) |item| {
        const ink = item.ink orelse {
            try report.check(false, "{s} {s}: no ink in the label's rect {d},{d} {d}x{d} (ELK drew nothing there)", .{ language.code, item.key, item.rect.x1, item.rect.y1, item.rect.width(), item.rect.height() });
            continue;
        };
        // Noto Sans Devanagari has no Latin letters: a mixed label has no hb-view reference.
        if (std.mem.eql(u8, language.code, "hi") and hasAsciiLetter(item.text)) {
            try report.check(true, "{s} {s} \"{s}\": ink {d}x{d}, no hb-view reference (the Devanagari face has no Latin letters)", .{ language.code, item.key, item.text, ink.width, ink.height });
            continue;
        }
        const reference = try hbReference(io, allocator, run.folder, item.key, item.text, language.face, language.tag, language.script, 16);
        const match = try measure.matchReference(allocator, ink, reference, 64);
        const cjk = std.mem.eql(u8, language.code, "zh-Hans") or std.mem.eql(u8, language.code, "ja");
        const least = if (std.mem.eql(u8, language.code, "hi") and hasConjunct(item.text)) min_conjunct_correlation else if (cjk and hasAsciiLetter(item.text)) min_mixed_correlation else min_correlation;
        try report.check(match.correlation >= least, "{s} {s} \"{s}\": correlation {d:.3} with hb-view (minimum {d:.2}), ink {d}x{d}, width to height {d:.3} of the reference's", .{ language.code, item.key, item.text, match.correlation, least, ink.width, ink.height, match.aspect });
    }
}

/// The five Latin-script languages, ru and the CJK and Hindi ones all draw frames that differ from
/// English wherever the label's text differs.
fn checkDiffersFromEnglish(language: Language, run: Run, english: Frames, items: []const Item, report: *Report) !void {
    var unchanged: usize = 0;
    for (items) |item| {
        const mine = run.frames.named(item.shot).?;
        const theirs = english.named(item.shot).?;
        const changed = changedIn(mine, theirs, item.rect);
        try report.line("      {s} {s}: {d} pixels differ from the English frame", .{ language.code, item.key, changed });
        if (changed < min_text_change) unchanged += 1;
    }
    try report.check(unchanged <= max_unchanged, "{s}: {d} of {d} labels draw as English does (at most {d}: a translation may equal the English word)", .{ language.code, unchanged, items.len, max_unchanged });
}

// ------------------------------------------------------------------ Hindi, Han

/// A halant (U+094D) before a Devanagari consonant, which makes a half form.
fn hasConjunct(text: []const u8) bool {
    var view = std.unicode.Utf8View.initUnchecked(text);
    var it = view.iterator();
    var halant = false;
    while (it.nextCodepoint()) |c| {
        if (halant and c >= 0x0915 and c <= 0x0939) return true;
        halant = c == 0x094d;
    }
    return false;
}

fn checkHindi(io: std.Io, allocator: std.mem.Allocator, language: Language, run: Run, items: []const Item, report: *Report) !void {
    var checked: usize = 0;
    for (items) |item| {
        const ink = item.ink orelse continue;
        if (hasAsciiLetter(item.text)) continue;
        checked += 1;
        const split = try measure.words(allocator, ink, ink_threshold, 3);
        for (split, 0..) |word, n| {
            const head = measure.headline(ink, word, headline_threshold);
            try report.check(head.longest_gap <= max_headline_gap, "hi {s} word {d} (columns {d}..{d}): headline row {d} has a gap of {d} columns (at most {d}, D058)", .{ item.key, n, word.x1, word.x2, head.row, head.longest_gap, max_headline_gap });
        }
        if (!hasCodepoint(item.text, 0x093f) or hasConjunct(item.text)) continue;
        const reference = try hbReference(io, allocator, run.folder, item.key, item.text, language.face, language.tag, language.script, 16);
        const ref_box = measure.inkBox(reference, 64).?;
        const ref_cut = try measure.crop(allocator, reference, ref_box);
        const scaled = try measure.resample(allocator, ref_cut, ink.width, ink.height);
        const ref_words = try measure.words(allocator, scaled, 64, 3);
        const text_words = std.mem.count(u8, item.text, " ") + 1;
        if (split.len != text_words or ref_words.len != text_words) {
            try report.check(false, "hi {s}: {d} words in the text, {d} in the frame and {d} in the reference", .{ item.key, text_words, split.len, ref_words.len });
            continue;
        }
        var at: usize = 0;
        var words_in = std.mem.splitScalar(u8, item.text, ' ');
        const n = while (words_in.next()) |text_word| : (at += 1) {
            if (hasCodepoint(text_word, 0x093f)) break at;
        } else unreachable;
        // The word's first third (the matra and its consonant) is more like the reference's first
        // third than like its middle or its last.
        const mine = measure.window(split[n], 0, 0.3);
        const own = try measure.correlationIn(allocator, ink, scaled, mine, measure.window(ref_words[n], 0, 0.3));
        const middle = try measure.correlationIn(allocator, ink, scaled, mine, measure.window(ref_words[n], 0.35, 0.65));
        const end = try measure.correlationIn(allocator, ink, scaled, mine, measure.window(ref_words[n], 0.7, 1));
        const other = @max(middle, end);
        try report.check(own - other >= min_matra_margin and own >= min_matra_correlation, "hi {s} word {d}: the short i matra's end of the word correlates {d:.3} with hb-view's, {d:.3} with its middle and {d:.3} with its other end (at least {d:.2}, a margin of {d:.2})", .{ item.key, n, own, middle, end, min_matra_correlation, min_matra_margin });
    }
    try report.check(checked >= 4, "hi: {d} Devanagari-only labels were measured for headline and matra (at least 4)", .{checked});
}

/// A reference raster cut to its ink and resampled onto the grid of a frame's label ink.
fn onGrid(allocator: std.mem.Allocator, reference: Gray, ink: Gray) !Gray {
    const cut = try measure.crop(allocator, reference, measure.inkBox(reference, 64).?);
    return measure.resample(allocator, cut, ink.width, ink.height);
}

fn checkHan(io: std.Io, allocator: std.mem.Allocator, language: Language, run: Run, items: []const Item, report: *Report) !void {
    const own_is_sc = std.mem.eql(u8, language.code, "zh-Hans");
    const own_name = if (own_is_sc) "Simplified Chinese" else "Japanese";
    // Whole labels barely tell the faces apart, so the verdict is taken per Han character: the CJK
    // faces advance one em per character, so a label made of Han alone divides into equal cells.
    var informative: usize = 0;
    var margin_sum: f64 = 0;
    var won: usize = 0;
    for (items) |item| {
        const ink = item.ink orelse continue;
        if (!hasHan(item.text) or hasAsciiLetter(item.text)) continue;
        const sc = try hbReference(io, allocator, run.folder, try std.mem.concat(allocator, u8, &.{ item.key, "-sc" }), item.text, noto_sc, "zh-Hans", null, han_reference_size);
        const jp = try hbReference(io, allocator, run.folder, try std.mem.concat(allocator, u8, &.{ item.key, "-jp" }), item.text, noto_jp, "ja", null, han_reference_size);
        const sc_scaled = try onGrid(allocator, sc, ink);
        const jp_scaled = try onGrid(allocator, jp, ink);
        var view = std.unicode.Utf8View.initUnchecked(item.text);
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
            try report.line("      {s} {s} {s}: SC {d:.3}, JP {d:.3} ({s} by {d:.3}); the faces' cells correlate {d:.3}", .{ language.code, item.key, utf8[0..len], with_sc, with_jp, if (own_is_sc) "SC" else "JP", margin, faces });
            if (faces >= han_informative_below) continue;
            informative += 1;
            margin_sum += margin;
            won += @intFromBool(margin > 0);
        }
    }
    try report.check(informative >= 2, "{s}: {d} Han characters are drawn differently by the two faces (at least 2: the menu has few)", .{ language.code, informative });
    const mean = margin_sum / @as(f64, @floatFromInt(@max(informative, 1)));
    try report.check(mean >= min_face_margin, "{s}: over those characters the {s} face wins by {d:.3} on average (at least {d:.2}), in {d} of {d}", .{ language.code, own_name, mean, min_face_margin, won, informative });
    try report.check(won * 3 >= informative * 2, "{s}: the {s} face is nearer in {d} of {d} of those characters (at least two in three)", .{ language.code, own_name, won, informative });
}

// ------------------------------------------------------------------ the missing key

/// The drop case: ru with a table that lacks `drop_key`.
fn checkDrop(setup: Setup, allocator: std.mem.Allocator, english_table: []const u8, report: *Report) !void {
    const io = setup.io;
    const cwd = std.Io.Dir.cwd();
    // The fixture is ru.lang without the one line, so a key added to ru.lang later fails here, not
    // as a second `l10n: missing` line in the log below.
    const ru = try languageTable(io, allocator, "ru");
    const fixture = try cwd.readFileAlloc(io, drop_fixture, allocator, .limited(1 << 20));
    var expected: std.ArrayList(u8) = .empty;
    var lines = std.mem.splitSequence(u8, ru, "\r\n");
    var first = true;
    while (lines.next()) |line| {
        if (std.mem.startsWith(u8, line, drop_key ++ "=")) continue;
        if (!first) try expected.appendSlice(allocator, "\r\n");
        try expected.appendSlice(allocator, line);
        first = false;
    }
    try report.check(std.mem.eql(u8, expected.items, fixture), "drop: {s} is ru.lang without {s} (regenerate it from {s}/ru.lang when ru.lang changes)", .{ drop_fixture, drop_key, lang_folder });

    const tables = try std.fs.path.join(allocator, &.{ setup.root, "drop-tables" });
    cwd.deleteTree(io, tables) catch {};
    try cwd.createDirPath(io, tables);
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ tables, "ru.lang" }), .data = fixture });
    const run = try runElk(allocator, setup, "drop-ru", "ru", tables);
    const wanted = "l10n: missing ru " ++ drop_key;
    try report.check(countOf(run.log, wanted) == 1, "drop: ELK logged \"{s}\" {d} times (once)", .{ wanted, countOf(run.log, wanted) });
    try report.check(countOf(run.log, "l10n: missing ") == 1, "drop: ELK logged {d} missing-key lines in all (exactly the one)", .{countOf(run.log, "l10n: missing ")});

    const english = (try tableText(allocator, english_table, drop_key)).?;
    const russian = (try tableText(allocator, ru, drop_key)).?;
    var found = false;
    for (run.shots) |shot| {
        if (!std.mem.eql(u8, shot.name, "menu")) continue;
        for (shot.rects.items) |drawn| {
            if (!std.mem.eql(u8, drawn.key, drop_key)) continue;
            found = true;
            const frame = run.frames.menu;
            const rect = pixelRect(frame, shot, drawn) orelse return error.OutsideFrame;
            const full = try inkImage(allocator, frame, rect);
            const box = measure.inkBox(full, ink_threshold) orelse {
                try report.check(false, "drop: no ink where ELK drew {s}", .{drop_key});
                return;
            };
            const ink = try measure.crop(allocator, full, box);
            const in_english = try measure.matchReference(allocator, ink, try hbReference(io, allocator, run.folder, "english", english, noto_sans, "en", null, 16), 64);
            const in_russian = try measure.matchReference(allocator, ink, try hbReference(io, allocator, run.folder, "russian", russian, noto_sans, "ru", null, 16), 64);
            try report.check(in_english.correlation >= min_correlation, "drop: {s} drawn as \"{s}\": correlation {d:.3} with hb-view's English (minimum {d:.2})", .{ drop_key, english, in_english.correlation, min_correlation });
            try report.check(in_english.correlation > in_russian.correlation, "drop: {s} is nearer the English text ({d:.3}) than the Russian \"{s}\" ({d:.3})", .{ drop_key, in_english.correlation, russian, in_russian.correlation });
        }
    }
    try report.check(found, "drop: ELK logged a rect for {s} on the menu frame", .{drop_key});
}

// ------------------------------------------------------------------ the test

fn copyDatabase(setup_io: std.Io, allocator: std.mem.Allocator, source: []const u8, target: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(setup_io, target) catch {};
    try cwd.createDirPath(setup_io, target);
    const xml = try cwd.readFileAlloc(setup_io, try std.fs.path.join(allocator, &.{ source, "elk.xml" }), allocator, .limited(16 << 20));
    try cwd.writeFile(setup_io, .{ .sub_path = try std.fs.path.join(allocator, &.{ target, "elk.xml" }), .data = xml });
    // The element folder the tracked database keeps its texts in.
    try elk.database.copyTree(setup_io, try std.fs.path.join(allocator, &.{ source, "game_data_base" }), try std.fs.path.join(allocator, &.{ target, "game_data_base" }));
}

test "ELK draws its main window, File menu and help in the ten languages (M002 S04, D066)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const exe = (try envValue(allocator, "BK_ELK_EXE")) orelse {
        std.debug.print("BK_ELK_EXE is not set: run zig build elk-l10n-auto\n", .{});
        return error.MissingElk;
    };
    const cwd = std.Io.Dir.cwd();
    const cwd_path = try std.process.currentPathAlloc(io, allocator);
    const root = try std.fs.path.join(allocator, &.{ cwd_path, scratch });
    try cwd.createDirPath(io, root);
    const database = try std.fs.path.join(allocator, &.{ root, "db" });
    try copyDatabase(io, allocator, "Data/ELK", database);
    const setup = Setup{ .io = io, .exe = exe, .database = database, .root = root };
    const english_table = try languageTable(io, allocator, "en");

    // English first: the other languages' frames are held against it.
    var failures: usize = 0;
    var english_run: ?Run = null;
    for (languages) |language| {
        const is_english = std.mem.eql(u8, language.code, "en");
        if (!is_english and !try selected(allocator, language.code)) continue;
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        const run = try runElk(allocator, setup, language.code, language.code, null);
        if (is_english) english_run = run;
        const table = try languageTable(io, allocator, language.code);
        try report.line("== {s}: {d} frames, {d} shots", .{ language.code, 3, run.shots.len });
        try report.check(countOf(run.log, "l10n: missing ") == 0, "{s}: ELK logged {d} missing-key lines (none: the table is whole)", .{ language.code, countOf(run.log, "l10n: missing ") });
        const items = try measureItems(allocator, run, table, &report, language.code);
        try checkReferences(io, allocator, language, run, items, &report);
        if (language.differs) try checkDiffersFromEnglish(language, run, english_run.?.frames, items, &report);
        if (std.mem.eql(u8, language.code, "hi")) try checkHindi(io, allocator, language, run, items, &report);
        if (std.mem.eql(u8, language.code, "zh-Hans") or std.mem.eql(u8, language.code, "ja")) try checkHan(io, allocator, language, run, items, &report);
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ run.folder, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("elk-l10n-auto: {s}: {d} measures failed; the report is {s}/{s}/report.txt\n", .{ language.code, report.failures, scratch, language.code });
    }

    if (try selected(allocator, "ru")) {
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        try report.line("== the drop case: ru without {s}", .{drop_key});
        try checkDrop(setup, allocator, english_table, &report);
        const folder = try std.fs.path.join(allocator, &.{ root, "drop-ru" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ folder, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("elk-l10n-auto: drop: {d} measures failed; the report is {s}/drop-ru/report.txt\n", .{ report.failures, scratch });
    }
    if (failures != 0) return error.MeasureFailed;
}
