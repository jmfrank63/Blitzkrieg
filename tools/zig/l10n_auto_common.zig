//! What the per-app language tiers share (M002 S04 D066, S05): `elk-l10n-auto` and
//! `map-editor-l10n-auto` start the installed editor hidden in each of the ten languages, read the
//! `l10n-auto: shot` and `l10n-auto: rect` lines it prints, decode the captured TGAs
//! (`l10n_measure.decodeTga`) and measure every logged label against hb-view's raster of the table's
//! text (D069: a tier fails when hb-view is missing). This file holds the log parser, the table
//! reader, the thresholds, the report and the checks that only need a label's ink and its text:
//! the reference correlation, the frame that differs from English, Hindi's headline and i matra and
//! the regional Han faces. What an app logs, where each label is measured and the missing-key case
//! stay in the app's own runner.
const std = @import("std");
const testing = std.testing;
const measure = @import("l10n_measure");

pub const Rect = measure.Rect;
pub const Gray = measure.Gray;
pub const timed_run = measure.timed_run;

pub const lang_folder = "Sources/editor/kit/lang";
pub const log_prefix = "l10n-auto: ";

// Thresholds, each set where the first passing run left room for the rasterisers' differences but
// not for a wrong glyph, wrong shaping or a wrong face (the values are in the report).
pub const ink_threshold: u8 = 24;
/// The least correlation of a label's ink with its hb-view raster. S01's 0.50 is for the Game's
/// larger text; the menu's 16 px labels, hinted, measured 0.457 at the least for a long French one.
pub const min_correlation: f64 = 0.40;
/// A label of a CJK language with Latin letters (PAK/UPD, MOD): the editor draws the letters in its
/// Latin face and hb-view in the CJK face's own, so only the Han and kana ink matches (0.320 measured).
pub const min_mixed_correlation: f64 = 0.30;
/// A label counts as drawn differently from English when this many pixels of its rect differ by
/// more than `frame_tolerance`; a translation may equal the English word, so a few may not.
pub const min_text_change: usize = 30;
pub const frame_tolerance: u8 = 16;
/// Hindi: the longest run of columns without ink on a word's headline, in frame pixels (D058).
/// The menu's 16 px text is rasterised cluster by cluster, so a join may lose a second column to
/// anti-aliasing where the Game's larger text loses one.
pub const max_headline_gap: usize = 2;
/// Hindi: the same for a word with a conjunct, whose half form is shaped alone and advances a
/// column more than hb-view's joined cluster: the Settings row "स्क्रॉल और स्वाइप की गति" measured 3
/// where every label without a conjunct stays within 2.
pub const max_conjunct_headline_gap: usize = 3;
/// Hindi: a label with a conjunct (a halant before a consonant) is drawn with the half form's
/// advance of the cluster shaped alone (the known limit of kit/imgui), so its words and its ink
/// drift from hb-view's: it must still resemble the reference, at this least correlation.
pub const min_conjunct_correlation: f64 = 0.15;
pub const headline_threshold: u8 = 8;
/// Hindi: the first third of the word that holds the short i matra against hb-view's first third,
/// and against its middle and last.
pub const min_matra_correlation: f64 = 0.4;
pub const min_matra_margin: f64 = 0.05;
/// Han: a character cell whose two faces' rasters correlate above this is not informative.
pub const han_informative_below: f64 = 0.90;
pub const min_face_margin: f64 = 0.01;
/// Han references for the face check are drawn large and scaled down, as in the Game tier.
pub const han_reference_size: u32 = 96;

pub const noto_sans = "vendor/fonts/NotoSans-Regular.ttf";
pub const noto_sc = "vendor/fonts/NotoSansSC-Regular.otf";
pub const noto_jp = "vendor/fonts/NotoSansJP-Regular.otf";
pub const noto_deva = "vendor/fonts/NotoSansDevanagari-Regular.ttf";

pub const Language = struct {
    code: []const u8,
    /// BCP 47 tag for hb-view.
    tag: []const u8,
    /// The face the language's text is shaped with for the reference.
    face: []const u8,
    script: ?[]const u8 = null,
    /// Whether the frame must differ from the English one (the languages whose labels are not English).
    differs: bool = true,
};

pub const languages = [_]Language{
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

pub fn isCjk(code: []const u8) bool {
    return std.mem.eql(u8, code, "zh-Hans") or std.mem.eql(u8, code, "ja");
}

pub fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

/// Whether BK_L10N_LANGS (a comma list of codes, default all) names `code`.
pub fn selected(allocator: std.mem.Allocator, code: []const u8) !bool {
    const list = (try envValue(allocator, "BK_L10N_LANGS")) orelse return true;
    var codes = std.mem.splitScalar(u8, list, ',');
    while (codes.next()) |one| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, one, " "), code)) return true;
    return false;
}

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

// ------------------------------------------------------------------ what the editor printed

pub const Drawn = struct { key: []const u8, x: f32, y: f32, w: f32, h: f32 };

pub const Shot = struct {
    name: []const u8,
    width: f32,
    height: f32,
    rects: std.ArrayList(Drawn) = .empty,
};

/// Which side of its `shot` line an app prints the rects a shot holds.
pub const RectsAre = enum {
    /// ELK logs a shot's rects in the frames after the shot line.
    after_shot,
    /// The Map Editor logs every frame: the shot holds the last rect of each key printed since the
    /// previous shot, which is the layout of the frame that was captured.
    before_shot,
};

/// The `l10n-auto: shot` and `l10n-auto: rect` lines of an editor's stderr, in order.
pub fn parseShots(allocator: std.mem.Allocator, log: []const u8, rects_are: RectsAre) ![]Shot {
    var shots: std.ArrayList(Shot) = .empty;
    var pending: std.ArrayList(Drawn) = .empty;
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
            if (rects_are == .before_shot) {
                shots.items[shots.items.len - 1].rects = pending;
                pending = .empty;
            }
        } else if (std.mem.eql(u8, kind, "rect")) {
            const key = words.next() orelse return error.BadLine;
            var numbers: [4]f32 = undefined;
            for (&numbers) |*number| number.* = try std.fmt.parseFloat(f32, words.next() orelse return error.BadLine);
            const drawn: Drawn = .{ .key = try allocator.dupe(u8, key), .x = numbers[0], .y = numbers[1], .w = numbers[2], .h = numbers[3] };
            switch (rects_are) {
                .after_shot => {
                    if (shots.items.len == 0) return error.BadLine;
                    try shots.items[shots.items.len - 1].rects.append(allocator, drawn);
                },
                .before_shot => {
                    for (pending.items) |*old| {
                        if (std.mem.eql(u8, old.key, key)) {
                            old.* = drawn;
                            break;
                        }
                    } else try pending.append(allocator, drawn);
                },
            }
        }
    }
    return shots.items;
}

pub fn countOf(haystack: []const u8, needle: []const u8) usize {
    return std.mem.count(u8, haystack, needle);
}

// ------------------------------------------------------------------ the tables

/// The value of `key` in a table, escapes undone, or null.
pub fn tableText(allocator: std.mem.Allocator, table: []const u8, key: []const u8) !?[]u8 {
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

pub fn languageTable(io: std.Io, allocator: std.mem.Allocator, code: []const u8) ![]u8 {
    const path = try std.fmt.allocPrint(allocator, "{s}/{s}.lang", .{ lang_folder, code });
    return std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(1 << 20));
}

/// `table` without the one line of `key`, joined with CRLF as the tables are: what a drop fixture
/// must equal.
pub fn withoutKey(allocator: std.mem.Allocator, table: []const u8, key: []const u8) ![]u8 {
    var expected: std.ArrayList(u8) = .empty;
    var lines = std.mem.splitSequence(u8, table, "\r\n");
    var first = true;
    while (lines.next()) |line| {
        if (std.mem.startsWith(u8, line, key) and line.len > key.len and line[key.len] == '=') continue;
        if (!first) try expected.appendSlice(allocator, "\r\n");
        try expected.appendSlice(allocator, line);
        first = false;
    }
    return expected.items;
}

pub fn hasAsciiLetter(text: []const u8) bool {
    for (text) |byte| if (std.ascii.isAlphabetic(byte)) return true;
    return false;
}

pub fn hasHan(text: []const u8) bool {
    var view = std.unicode.Utf8View.initUnchecked(text);
    var it = view.iterator();
    while (it.nextCodepoint()) |cp| if (cp >= 0x4e00 and cp <= 0x9fff) return true;
    return false;
}

pub fn hasCodepoint(text: []const u8, wanted: u21) bool {
    var view = std.unicode.Utf8View.initUnchecked(text);
    var it = view.iterator();
    while (it.nextCodepoint()) |cp| if (cp == wanted) return true;
    return false;
}

/// A halant (U+094D) before a Devanagari consonant, which makes a half form.
pub fn hasConjunct(text: []const u8) bool {
    var view = std.unicode.Utf8View.initUnchecked(text);
    var it = view.iterator();
    var halant = false;
    while (it.nextCodepoint()) |c| {
        if (halant and c >= 0x0915 and c <= 0x0939) return true;
        halant = c == 0x094d;
    }
    return false;
}

// ------------------------------------------------------------------ the frames

/// A language's captured frames by the shot's name, for the checks that compare them with another
/// language's.
pub const Frames = struct {
    names: []const []const u8,
    grays: []const Gray,

    pub fn named(self: Frames, name: []const u8) ?Gray {
        for (self.names, self.grays) |one, gray| if (std.mem.eql(u8, one, name)) return gray;
        return null;
    }

    /// Decodes `<folder>/<name>.tga` for each name.
    pub fn read(io: std.Io, allocator: std.mem.Allocator, folder: []const u8, names: []const []const u8) !Frames {
        const grays = try allocator.alloc(Gray, names.len);
        for (names, grays) |name, *gray| {
            const path = try std.fmt.allocPrint(allocator, "{s}/{s}.tga", .{ folder, name });
            const bytes = try std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(64 << 20));
            gray.* = try measure.decodeTga(allocator, bytes);
        }
        return .{ .names = names, .grays = grays };
    }
};

/// What an editor run gave: its log, the shots parsed from it, the decoded frames and its folder.
pub const Run = struct { log: []u8, shots: []Shot, frames: Frames, folder: []const u8 };

/// The pixel box of a drawn rect on a frame: the logical rect scaled to the frame's size, clamped.
pub fn pixelRect(frame: Gray, shot: Shot, drawn: Drawn) ?Rect {
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
pub fn inkImage(allocator: std.mem.Allocator, frame: Gray, rect: Rect) !Gray {
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
pub fn changedIn(a: Gray, b: Gray, rect: Rect) usize {
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
pub const Item = struct {
    shot: []const u8,
    key: []const u8,
    text: []const u8,
    rect: Rect,
    /// The ink cut to its box; null when the rect holds none.
    ink: ?Gray,
};

/// A label cut from its frame, or null (a failure in the report) when its rect is outside it. The
/// text is the table's, or English's for a key the table lacks.
pub fn cutLabel(allocator: std.mem.Allocator, shot: Shot, frame: Gray, drawn: Drawn, table: []const u8, report: *Report, code: []const u8) !?Item {
    const text = (try tableText(allocator, table, drawn.key)) orelse (try tableText(allocator, try languageTable(testing.io, allocator, "en"), drawn.key)) orelse return error.NoSuchKey;
    const rect = pixelRect(frame, shot, drawn) orelse {
        try report.check(false, "{s} {s}: the rect {d:.0},{d:.0} {d:.0}x{d:.0} is outside the {d}x{d} frame", .{ code, drawn.key, drawn.x, drawn.y, drawn.w, drawn.h, frame.width, frame.height });
        return null;
    };
    const full = try inkImage(allocator, frame, rect);
    const box = measure.inkBox(full, ink_threshold);
    return .{
        .shot = shot.name,
        .key = drawn.key,
        .text = text,
        .rect = rect,
        .ink = if (box) |found| try measure.crop(allocator, full, found) else null,
    };
}

pub fn hbReference(io: std.Io, allocator: std.mem.Allocator, folder: []const u8, stem: []const u8, text: []const u8, face: []const u8, tag: []const u8, script: ?[]const u8, size: u32) !Gray {
    const reference_folder = try std.fs.path.join(allocator, &.{ folder, "reference" });
    try std.Io.Dir.cwd().createDirPath(io, reference_folder);
    return measure.hbView(allocator, io, reference_folder, stem, text, .{ .font = face, .size = size, .language = tag, .script = script });
}

// ------------------------------------------------------------------ the checks

/// Every label's ink against hb-view's raster of its text: the first checks of all ten languages.
pub fn checkReferences(io: std.Io, allocator: std.mem.Allocator, language: Language, run: Run, items: []const Item, report: *Report) !void {
    for (items) |item| {
        const ink = item.ink orelse {
            try report.check(false, "{s} {s}: no ink in the label's rect {d},{d} {d}x{d} (the editor drew nothing there)", .{ language.code, item.key, item.rect.x1, item.rect.y1, item.rect.width(), item.rect.height() });
            continue;
        };
        // Noto Sans Devanagari has no Latin letters: a mixed label has no hb-view reference.
        if (std.mem.eql(u8, language.code, "hi") and hasAsciiLetter(item.text)) {
            try report.check(true, "{s} {s} \"{s}\": ink {d}x{d}, no hb-view reference (the Devanagari face has no Latin letters)", .{ language.code, item.key, item.text, ink.width, ink.height });
            continue;
        }
        const reference = try hbReference(io, allocator, run.folder, item.key, item.text, language.face, language.tag, language.script, 16);
        const match = try measure.matchReference(allocator, ink, reference, 64);
        const least = if (std.mem.eql(u8, language.code, "hi") and hasConjunct(item.text)) min_conjunct_correlation else if (isCjk(language.code) and hasAsciiLetter(item.text)) min_mixed_correlation else min_correlation;
        try report.check(match.correlation >= least, "{s} {s} \"{s}\": correlation {d:.3} with hb-view (minimum {d:.2}), ink {d}x{d}, width to height {d:.3} of the reference's", .{ language.code, item.key, item.text, match.correlation, least, ink.width, ink.height, match.aspect });
    }
}

/// The five Latin-script languages, ru and the CJK and Hindi ones all draw frames that differ from
/// English wherever the label's text differs; `max_unchanged` labels may equal the English word.
pub fn checkDiffersFromEnglish(language: Language, run: Run, english: Frames, items: []const Item, max_unchanged: usize, report: *Report) !void {
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

pub fn checkHindi(io: std.Io, allocator: std.mem.Allocator, language: Language, run: Run, items: []const Item, report: *Report) !void {
    var checked: usize = 0;
    for (items) |item| {
        const ink = item.ink orelse continue;
        if (hasAsciiLetter(item.text)) continue;
        checked += 1;
        const split = try measure.words(allocator, ink, ink_threshold, 3);
        for (split, 0..) |word, n| {
            const head = measure.headline(ink, word, headline_threshold);
            const most = if (hasConjunct(item.text)) max_conjunct_headline_gap else max_headline_gap;
            try report.check(head.longest_gap <= most, "hi {s} word {d} (columns {d}..{d}): headline row {d} has a gap of {d} columns (at most {d}, D058)", .{ item.key, n, word.x1, word.x2, head.row, head.longest_gap, most });
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

pub fn checkHan(io: std.Io, allocator: std.mem.Allocator, language: Language, run: Run, items: []const Item, report: *Report) !void {
    return checkHanAtLeast(io, allocator, language, run, items, report, 2);
}

/// `checkHan` for a frame whose labels hold few Han characters that the two faces draw differently (a
/// menu bar of mostly kana): `least` of them must be informative.
pub fn checkHanAtLeast(io: std.Io, allocator: std.mem.Allocator, language: Language, run: Run, items: []const Item, report: *Report, least: usize) !void {
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
    try report.check(informative >= least, "{s}: {d} Han characters are drawn differently by the two faces (at least {d}: the labels have few)", .{ language.code, informative, least });
    const mean = margin_sum / @as(f64, @floatFromInt(@max(informative, 1)));
    try report.check(mean >= min_face_margin, "{s}: over those characters the {s} face wins by {d:.3} on average (at least {d:.2}), in {d} of {d}", .{ language.code, own_name, mean, min_face_margin, won, informative });
    try report.check(won * 3 >= informative * 2, "{s}: the {s} face is nearer in {d} of {d} of those characters (at least two in three)", .{ language.code, own_name, won, informative });
}

/// The ink of a drop case's label against hb-view's English text and the language's own: the table
/// lacks the key, so the English text must be drawn. `shot` names the frame the label was drawn in.
pub fn checkDroppedLabel(io: std.Io, allocator: std.mem.Allocator, run: Run, shot_name: []const u8, key: []const u8, english: []const u8, own: []const u8, own_language: Language, report: *Report) !void {
    var found = false;
    for (run.shots) |shot| {
        if (!std.mem.eql(u8, shot.name, shot_name)) continue;
        for (shot.rects.items) |drawn| {
            if (!std.mem.eql(u8, drawn.key, key)) continue;
            found = true;
            const frame = run.frames.named(shot_name).?;
            const rect = pixelRect(frame, shot, drawn) orelse return error.OutsideFrame;
            const full = try inkImage(allocator, frame, rect);
            const box = measure.inkBox(full, ink_threshold) orelse {
                try report.check(false, "drop: no ink where the editor drew {s}", .{key});
                return;
            };
            const ink = try measure.crop(allocator, full, box);
            const in_english = try measure.matchReference(allocator, ink, try hbReference(io, allocator, run.folder, "english", english, noto_sans, "en", null, 16), 64);
            const in_own = try measure.matchReference(allocator, ink, try hbReference(io, allocator, run.folder, "translated", own, own_language.face, own_language.tag, own_language.script, 16), 64);
            try report.check(in_english.correlation >= min_correlation, "drop: {s} drawn as \"{s}\": correlation {d:.3} with hb-view's English (minimum {d:.2})", .{ key, english, in_english.correlation, min_correlation });
            try report.check(in_english.correlation > in_own.correlation, "drop: {s} is nearer the English text ({d:.3}) than the {s} \"{s}\" ({d:.3})", .{ key, in_english.correlation, own_language.code, own, in_own.correlation });
        }
    }
    try report.check(found, "drop: the editor logged a rect for {s} on the {s} frame", .{ key, shot_name });
}
