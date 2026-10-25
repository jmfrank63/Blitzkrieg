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
//! The shared parts (log parser, thresholds, the per-language checks) are in l10n_auto_common.zig.
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
const common = @import("l10n_auto_common");

const Report = common.Report;
const Run = common.Run;
const Item = common.Item;
const Frames = common.Frames;
const Language = common.Language;

const scratch = "zig-out/local-test/elk/l10n-auto";
const drop_fixture = "tools/zig/fixtures/l10n/elk-drop/ru.lang";
const drop_key = "elk.file.import_pak";

/// How many rects a run must log: the titles, the File menu's items and the help window's title.
const expected_titles = 6;
const expected_items = 9;
/// A label may equal the English word in a language: at most this many of ELK's do.
const max_unchanged: usize = 3;

const languages = common.languages;
const envValue = common.envValue;
const selected = common.selected;
const countOf = common.countOf;
const tableText = common.tableText;
const languageTable = common.languageTable;

// ------------------------------------------------------------------ what ELK printed

/// Whether the shot is where the label is measured: titles in the main frame (in the menu frame
/// the open menu's title is highlighted), the items in the menu frame, the help title in the help
/// frame.
fn measuredIn(shot: []const u8, key: []const u8) bool {
    if (std.mem.eql(u8, shot, "main")) return std.mem.startsWith(u8, key, "elk.menu.");
    if (std.mem.eql(u8, shot, "menu")) return std.mem.startsWith(u8, key, "elk.file.");
    if (std.mem.eql(u8, shot, "help")) return std.mem.startsWith(u8, key, "elk.helpwin.");
    return false;
}

// ------------------------------------------------------------------ running ELK

const Setup = struct {
    io: std.Io,
    exe: []const u8,
    /// Absolute paths.
    database: []const u8,
    root: []const u8,
};

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
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ folder, "xdg", "profiles" }));
    try environ.put("BK_ELK_SETTINGS", config);
    if (table_dir) |dir| try environ.put("BK_L10N_DIR", dir);
    const result = try common.timed_run.run(allocator, setup.io, .{
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
    return .{
        .log = log,
        .shots = try common.parseShots(allocator, log, .after_shot),
        .folder = folder,
        .frames = try Frames.read(setup.io, allocator, folder, &.{ "main", "menu", "help" }),
    };
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
            const item = (try common.cutLabel(allocator, shot, frame, drawn, table, report, code)) orelse continue;
            try items.append(allocator, item);
        }
    }
    try report.check(titles == expected_titles, "{s}: ELK logged {d} menu bar titles on the main frame (expected {d})", .{ code, titles, expected_titles });
    try report.check(menu_items == expected_items, "{s}: ELK logged {d} File menu items on the menu frame (expected {d})", .{ code, menu_items, expected_items });
    return items.items;
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
    const expected = try common.withoutKey(allocator, ru, drop_key);
    try report.check(std.mem.eql(u8, expected, fixture), "drop: {s} is ru.lang without {s} (regenerate it from {s}/ru.lang when ru.lang changes)", .{ drop_fixture, drop_key, common.lang_folder });

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
    try common.checkDroppedLabel(io, allocator, run, "menu", drop_key, english, russian, languages[1], report);
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
        try common.checkReferences(io, allocator, language, run, items, &report);
        if (language.differs) try common.checkDiffersFromEnglish(language, run, english_run.?.frames, items, max_unchanged, &report);
        if (std.mem.eql(u8, language.code, "hi")) try common.checkHindi(io, allocator, language, run, items, &report);
        if (std.mem.eql(u8, language.code, "zh-Hans") or std.mem.eql(u8, language.code, "ja")) try common.checkHan(io, allocator, language, run, items, &report);
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
