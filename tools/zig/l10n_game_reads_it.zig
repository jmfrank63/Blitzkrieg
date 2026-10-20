//! M002 R056/D066 on the real Game: the language mods `language-mods` built
//! (ru, zh-Hans, ja, hi) draw a translated main menu with their generated
//! fonts, and what they draw is what an independent text stack draws.
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
//! Last, the Game starts on ru and switches to ja while it runs (the font
//! cache clear of ApplyLanguage, T03): its capture must show ja's frame.
//!
//! Frames, references and `report.txt` are kept in
//! `zig-out/local-test/l10n-game-reads-it/<code>/`. The user's data is never
//! touched: XDG_DATA_HOME and BK_USER_ROOT are the scratch folder and the Game
//! runs from it. BK_L10N_GAME names the installed Game executable;
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
/// Hindi: the shaped label's correlation must beat the halant spelling's by
/// this much.
const min_conjunct_margin: f64 = 0.02;
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

const Label = struct {
    /// The overlay key below `textes/strings/`.
    key: []const u8,
    rect: Rect,
    /// The header: FontSize 2 (the large font), grey text. The buttons use the
    /// medium font and gold text.
    large: bool = false,
};

/// The header (element 20001, FontSize 2) and the five buttons (FontSize 1) on
/// the 1024x768 canvas, as in bk_font_game_reads_it.zig: the buttons are 320x50
/// at x 650, y 267 + 65 * n; the header lies in the panel above them.
const labels = [_]Label{
    .{ .key = "imheader-mainmenu-header", .rect = .{ .x1 = 630, .y1 = 175, .x2 = 990, .y2 = 248 }, .large = true },
    .{ .key = "imheader-gbutton-newgame", .rect = .{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 } },
    .{ .key = "imheader-gbutton-multiplayer", .rect = .{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 } },
    .{ .key = "imheader-gbutton-loadgame", .rect = .{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 } },
    .{ .key = "imheader-gbutton-options", .rect = .{ .x1 = 650, .y1 = 462, .x2 = 970, .y2 = 512 } },
    .{ .key = "imheader-gbutton-exitgame", .rect = .{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 } },
};

const Language = struct {
    code: []const u8,
    mod: []const u8,
    /// BCP 47 tag for hb-view.
    tag: []const u8,
    face: []const u8,
    script: ?[]const u8 = null,
};

const languages = [_]Language{
    .{ .code = "ru", .mod = "lang-ru", .tag = "ru", .face = "vendor/fonts/NotoSans-Regular.ttf" },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .tag = "zh-Hans", .face = "vendor/fonts/NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .tag = "ja", .face = "vendor/fonts/NotoSansJP-Regular.otf" },
    .{ .code = "hi", .mod = "lang-hi", .tag = "hi", .face = "vendor/fonts/NotoSansDevanagari-Regular.ttf", .script = "Deva" },
};

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

test "the Game draws the language mods' main menus as an independent shaper does (R056)" {
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
    cwd.deleteTree(io, scratch) catch {};
    try cwd.createDirPath(io, scratch);

    // The background: the blank language, in a profile of its own.
    const blank_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", blank_mod });
    defer cwd.deleteTree(io, blank_folder) catch {};
    try installBlankMod(io, allocator, game_root, root);
    const blank_run = try Run.init(io, allocator, game, game_root, root, "blank", blank_mod);
    const blank = try blank_run.capture(allocator, null, null);
    try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "blank", "blank.tga" }), blank);

    var report = Report{ .allocator = allocator };
    var frames: [languages.len][]u8 = undefined;
    for (languages, 0..) |language, index| {
        const run = try Run.init(io, allocator, game, game_root, root, language.code, language.mod);
        const frame = try run.capture(allocator, null, null);
        frames[index] = frame;
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, language.code, "frame.tga" }), frame);
        try report.line("== {s} ({s})", .{ language.code, language.mod });
        try checkLanguage(io, allocator, language, run.folder, frame, blank, &report);
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, language.code, "report.txt" }), .data = report.lines.items });
    }

    // The font cache clear: ru running, ja chosen while it runs.
    {
        const ru = try Run.init(io, allocator, game, game_root, root, "switch", "lang-ru");
        const live = try ru.capture(allocator, "60:var=Text.Language=lang-ja", 160);
        try writeTga(io, allocator, try std.fs.path.join(allocator, &.{ root, "switch", "frame.tga" }), live);
        const ja_index = 2;
        // The five buttons re-read their text and take the new font at once. The header
        // (labels[0]) is set once when the menu is built and keeps its text on a
        // live switch, so it is reported and not measured.
        var worst: usize = 0;
        for (labels[1..]) |label| worst = @max(worst, changedIn(frames[ja_index], live, label.rect));
        try report.line("      the header differs from ja started cold in {d} pixels (not measured: its text is set when the menu is built)", .{changedIn(frames[ja_index], live, labels[0].rect)});
        try report.check(worst <= max_live_difference, "ru then ja live: at most {d} pixels differ from ja started cold in any button's box (limit {d})", .{ worst, max_live_difference });
    }

    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "report.txt" }), .data = report.lines.items });
    if (report.failures != 0) {
        std.debug.print("l10n-game-reads-it: {d} measures failed; the report is {s}/report.txt\n", .{ report.failures, scratch });
        return error.MeasureFailed;
    }
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
/// brightness would count both; the header's grey text over the dark panel
/// adds brightness.
fn inkImage(allocator: std.mem.Allocator, frame: []const u8, background: []const u8, label: Label) !Gray {
    const rect = label.rect;
    const pixels = try allocator.alloc(u8, rect.width() * rect.height());
    for (0..rect.height()) |y| for (0..rect.width()) |x| {
        const at = ((rect.y1 + y) * width + rect.x1 + x) * 4;
        const added = if (label.large) luma(frame[at..][0..3]) - luma(background[at..][0..3]) else warmth(frame[at..][0..3]) - warmth(background[at..][0..3]);
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
fn overlayText(io: std.Io, allocator: std.mem.Allocator, code: []const u8, key: []const u8) ![]u8 {
    const path = try std.fmt.allocPrint(allocator, "Localization/game/{s}/game_data_base/textes/strings/{s}.txt", .{ code, key });
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

fn checkLanguage(io: std.Io, allocator: std.mem.Allocator, language: Language, folder: []const u8, frame: []const u8, blank: []const u8, report: *Report) !void {
    const hb_folder = try std.fs.path.join(allocator, &.{ folder, "reference" });
    try std.Io.Dir.cwd().createDirPath(io, hb_folder);
    const texts = try allocator.alloc([]u8, labels.len);
    const refs = try allocator.alloc(Gray, labels.len);
    const inks = try allocator.alloc(Gray, labels.len);
    for (labels, 0..) |label, index| {
        texts[index] = try overlayText(io, allocator, language.code, label.key);
        refs[index] = try measure.hbView(allocator, io, hb_folder, label.key, texts[index], .{ .font = language.face, .language = language.tag, .script = language.script });
        const full = try inkImage(allocator, frame, blank, label);
        const box = measure.inkBox(full, ink_threshold) orelse {
            try report.check(false, "{s} {s}: no ink in the label's box (the Game drew nothing there)", .{ language.code, label.key });
            inks[index] = full;
            continue;
        };
        inks[index] = try measure.crop(allocator, full, box);
    }

    for (labels, 0..) |label, index| {
        const ink = inks[index];
        if (ink.width == full_width(label)) continue; // no ink: reported above
        const own = try measure.matchReference(allocator, ink, refs[index], 64);
        var best_other: f64 = -1;
        for (labels, 0..) |_, other| {
            if (other == index) continue;
            best_other = @max(best_other, (try measure.matchReference(allocator, ink, refs[other], 64)).correlation);
        }
        try report.check(own.correlation >= min_correlation, "{s} {s} \"{s}\": correlation {d:.3} with hb-view (minimum {d:.2}), ink {d}x{d}", .{ language.code, label.key, texts[index], own.correlation, min_correlation, ink.width, ink.height });
        try report.check(own.correlation - best_other >= min_margin, "{s} {s}: own reference beats the best other by {d:.3} (minimum {d:.2})", .{ language.code, label.key, own.correlation - best_other, min_margin });
        try report.check(@abs(own.aspect - 1) <= max_aspect_error, "{s} {s}: width to height is {d:.3} of the reference's (within {d:.2})", .{ language.code, label.key, own.aspect, max_aspect_error });
    }

    if (std.mem.eql(u8, language.code, "hi")) try checkHindi(io, allocator, language, hb_folder, texts, refs, inks, report);
    if (std.mem.eql(u8, language.code, "zh-Hans") or std.mem.eql(u8, language.code, "ja")) try checkHan(io, allocator, language, hb_folder, texts, inks, report);
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
        // Conjuncts: the label drawn as hb-view's conjunct, not the halant spelling.
        if (hasCodepoint(texts[index], 0x094d)) {
            const apart = try measure.hbView(allocator, io, hb_folder, try std.mem.concat(allocator, u8, &.{ label.key, "-halant" }), texts[index], .{ .font = language.face, .language = language.tag, .script = language.script, .features = measure.halant_spelling });
            const joined = try measure.matchReference(allocator, ink, refs[index], 64);
            const spelled = try measure.matchReference(allocator, ink, apart, 64);
            try report.check(joined.correlation - spelled.correlation >= min_conjunct_margin, "hi {s}: conjuncts: shaped reference {d:.3}, halant spelling {d:.3} (margin at least {d:.2})", .{ label.key, joined.correlation, spelled.correlation, min_conjunct_margin });
        }
        // The short i matra: its bar lies where hb-view puts it, in the first
        // word that has the matra.
        if (hasCodepoint(texts[index], 0x093f)) {
            const ref_box = measure.inkBox(refs[index], 64).?;
            const ref_cut = try measure.crop(allocator, refs[index], ref_box);
            const scaled = try measure.resample(allocator, ref_cut, ink.width, ink.height);
            const ref_words = try measure.words(allocator, scaled, 64, 3);
            const text_words = std.mem.count(u8, texts[index], " ") + 1;
            if (split.len != text_words or ref_words.len != text_words) {
                try report.check(false, "hi {s}: {d} words in the text, {d} in the frame and {d} in the reference", .{ label.key, text_words, split.len, ref_words.len });
                continue;
            }
            var at: usize = 0;
            var words_in = std.mem.splitScalar(u8, texts[index], ' ');
            const n = while (words_in.next()) |text_word| : (at += 1) {
                if (hasCodepoint(text_word, 0x093f)) break at;
            } else unreachable;
            // The word's first third (the matra and its consonant) is more like
            // the reference's first third than like its middle or its last.
            const mine = measure.window(split[n], 0, 0.3);
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
    mod: []const u8,
    name: []const u8,

    /// Makes the scratch folder and sets the profile's language option to
    /// `mod` with a short run of the Game on the options' own `set=`, which
    /// the profile keeps.
    fn init(io: std.Io, allocator: std.mem.Allocator, game: []const u8, game_root: []const u8, root: []const u8, name: []const u8, mod: []const u8) !Run {
        const cwd = std.Io.Dir.cwd();
        const folder = try std.fs.path.join(allocator, &.{ root, name });
        const user = try std.fs.path.join(allocator, &.{ folder, "user" });
        // defconf.cfg, the options' declarations, is looked for under the working directory.
        const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
        try cwd.createDirPath(io, configs);
        const defconf = try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }), .data = try cwd.readFileAlloc(io, defconf, allocator, .limited(1 << 20)) });
        const run = Run{ .io = io, .game = game, .folder = folder, .user = user, .mod = mod, .name = name };
        const schedule = try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{mod});
        const log = try run.play(allocator, &.{ "-windowed", "-profile=L10n" }, schedule, null, "choose");
        try expectLine(log, run.folder, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{mod}));
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
        const path = try std.fmt.allocPrint(allocator, "{s}/capture.rgba", .{self.folder});
        const log = try self.play(allocator, &.{ "-reference-scene", path, "-reference-resolution", "1024", "768", "-windowed", "-profile=L10n" }, schedule, delay, "capture");
        try expectLine(log, self.folder, "BK_REFERENCE_SCENE: capture complete");
        try expectLine(log, self.folder, "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
        // The fonts come from the language mod: both sizes the menu uses, tfd and atlas.
        for ([_][]const u8{ "Fonts\\medium\\1.tfd", "Fonts\\medium\\1_h.dds", "Fonts\\large\\1.tfd", "Fonts\\large\\1_h.dds" }) |file| {
            const line = try std.fmt.allocPrint(allocator, "BK_MOD_TRACE: open \"{s}\" from LANGUAGE", .{file});
            try expectLine(log, self.folder, line);
        }
        const frame = try std.Io.Dir.cwd().readFileAlloc(self.io, path, allocator, .limited(16 << 20));
        try testing.expectEqual(@as(usize, width * height * 4), frame.len);
        return frame;
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
    const mod_xml = "<?xml version=\"1.0\"?>\r\n<base><MODName>l10n blank</MODName><MODVersion>1.0</MODVersion><MODDesc></MODDesc></base>\r\n";
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
