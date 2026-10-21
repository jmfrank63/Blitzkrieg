//! M002 S06 D066 on the real Resource Editor: the installed editor started hidden in each of the
//! ten languages (the scratch cfg's `ui_language`) runs the `schedule` below and captures four
//! frames: `main` (a new weapon project, the menu bar), `menu` (the View menu opened by
//! `do=open_menu`), `help` (the Help window, opened by `key=f1`) and `subeditor` (a new sprite
//! project with the Thumbnails dock). The editor's `--l10n-auto` mode prints
//! `l10n-auto: rect <key> <x> <y> <w> <h>` for every label it draws (every frame; the last rect of
//! a key before a shot is the captured layout, `RectsAre.before_shot`). This runner reads those
//! lines, decodes the TGAs, cuts each measured label's rect (`measuredIn`), takes the ink against
//! the rect's own background and compares it with hb-view's raster of the table's text at the UI's
//! 16 px in the language's Noto face (D069: the tier fails when hb-view is missing). The checks are
//! those of the Map Editor tier (l10n_auto_common.zig): the correlation with hb-view, a frame that
//! differs from English where the text differs, Hindi's headline and i matra, the regional Han
//! faces, and the fit: every measured label lies inside the frame, the Help rows and manual text
//! inside the Help window.
//!
//! The missing-key case: `tools/zig/fixtures/l10n/resource-editor-drop/ru.lang` is ru.lang without
//! `resource_app.menu.help`. The editor on ru with `BK_L10N_DIR` at a copy of it must log exactly
//! one `l10n: missing ru resource_app.menu.help` and draw that title in English: its ink
//! correlates with hb-view's English text and better than with the Russian one. The fixture is
//! kept as a file and must stay ru.lang minus that line, which the runner checks; regenerate it
//! when ru.lang changes.
//!
//! Frames, references, the editor's log and `report.txt` are kept in
//! `zig-out/local-test/resource-editor/l10n-auto/<code>/` (the drop case in `drop-ru/`). The
//! user's data is never touched: XDG_DATA_HOME, BK_USER_ROOT and the settings file are in the
//! scratch folder. BK_RESOURCE_EDITOR_EXE names the installed editor; `zig build
//! resource-editor-l10n-auto` sets it. BK_L10N_LANGS (a comma list of codes, default all) picks
//! the languages; the drop case runs with ru, or when the list is unset.
const std = @import("std");
const testing = std.testing;
const common = @import("l10n_auto_common");

const Report = common.Report;
const Run = common.Run;
const Item = common.Item;
const Frames = common.Frames;
const Drawn = common.Drawn;
const Shot = common.Shot;
const languages = common.languages;
const envValue = common.envValue;
const selected = common.selected;
const countOf = common.countOf;
const languageTable = common.languageTable;
const tableText = common.tableText;

const scratch = "zig-out/local-test/resource-editor/l10n-auto";
const drop_fixture = "tools/zig/fixtures/l10n/resource-editor-drop/ru.lang";
const drop_key = "resource_app.menu.help";
const drop_shot = "main";
const fixtures_argument = "tools/zig/fixtures/resource_editor";

/// The schedule (BK_EDITOR_AUTO frames): a new weapon project, a few frames to settle, then the
/// main window. The rects logged before the shot are the captured layout.
const schedule = "2:do=new:wpn,8:shot=main,9:do=open_menu:resource_app.menu.view,13:shot=menu,14:key=escape,17:key=f1,21:shot=help,22:do=help_close,23:do=new:spt,24:do=dock:thumbnails,30:shot=subeditor,32:exit";
const shot_names = [_][]const u8{ "main", "menu", "help", "subeditor" };

/// The menu bar the scenario draws: File, Edit, Tools, Editors, View, Preview and Help.
const expected_titles = 7;
/// The menu bar's Japanese is mostly kana, so the Han check takes one informative character here;
/// the later shots (docks, dialogs) hold more labels and raise it.
const least_han: usize = 1;
/// A title may equal the English word in a language: at most this many of the seven
/// (Editors, Tools and the like are the same word in several languages).
const max_unchanged: usize = 3;

/// The labels measured against hb-view in each shot: the menu titles of the main frame, the View
/// menu's dock items, the Help window's action rows, the Thumbnails dock's button of the second
/// sub-editor. Wrapped paragraphs (the manual text) and labels with arguments have no one-line
/// reference, so they are only held to the fit check.
fn measuredIn(shot: []const u8, key: []const u8) bool {
    if (std.mem.eql(u8, shot, "main")) return std.mem.startsWith(u8, key, "resource_app.menu.");
    if (std.mem.eql(u8, shot, "menu")) return std.mem.eql(u8, key, "resource_app.dock.thumbnails") or std.mem.eql(u8, key, "resource_app.dock.wireframe_view");
    if (std.mem.eql(u8, shot, "help")) return std.mem.startsWith(u8, key, "resource_app.help.act.");
    if (std.mem.eql(u8, shot, "subeditor")) return std.mem.eql(u8, key, "resource_app.thumbs.rescan");
    return false;
}

/// The labels of `shot` that must lie inside the Help window.
fn fitsHelpWindow(shot: []const u8, key: []const u8) bool {
    return std.mem.eql(u8, shot, "help") and (measuredIn(shot, key) or std.mem.eql(u8, key, "resource_app.help.manual_text"));
}

/// Labels per shot a whole run logs.
const expected_measured = 7 + 2 + 3 + 1;

// ------------------------------------------------------------------ running the Resource Editor

const Setup = struct {
    io: std.Io,
    exe: []const u8,
    /// Absolute.
    root: []const u8,
    /// Absolute.
    fixtures: []const u8,
};

/// The editor on `code` with the scratch settings of `name`'s folder, and `table_dir` as BK_L10N_DIR
/// when given; the log and the decoded frames.
fn runEditor(allocator: std.mem.Allocator, setup: Setup, name: []const u8, code: []const u8, table_dir: ?[]const u8) !Run {
    const cwd = std.Io.Dir.cwd();
    const folder = try std.fs.path.join(allocator, &.{ setup.root, name });
    cwd.deleteTree(setup.io, folder) catch {};
    try cwd.createDirPath(setup.io, folder);
    const config = try std.fs.path.join(allocator, &.{ folder, "resourceeditor.cfg" });
    try cwd.writeFile(setup.io, .{ .sub_path = config, .data = try std.fmt.allocPrint(allocator, "ui_language={s}\r\n", .{code}) });
    const user_root = try std.fs.path.join(allocator, &.{ folder, "user" });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user_root);
    try environ.put("BK_USER_ROOT", user_root);
    try environ.put("BK_RESOURCE_EDITOR_SETTINGS", config);
    try environ.put("BK_EDITOR_AUTO", schedule);
    if (table_dir) |dir| try environ.put("BK_L10N_DIR", dir);
    const result = try common.timed_run.run(allocator, setup.io, .{
        .argv = &.{ setup.exe, "--l10n-auto", setup.fixtures, folder },
        // The installation the editor runs in: Data, the engine and Fonts/ are beside the exe.
        .cwd = .{ .path = std.fs.path.dirname(setup.exe).? },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(setup.io, .{ .sub_path = try std.fs.path.join(allocator, &.{ folder, "resource-editor.log" }), .data = log });
    if (!result.term.success()) {
        std.debug.print("Resource Editor ({s}) ended {any}; its output is in {s}/resource-editor.log\n", .{ name, result.term, folder });
        return error.ResourceEditorFailed;
    }
    return .{
        .log = log,
        .shots = try common.parseShots(allocator, log, .before_shot),
        .folder = folder,
        .frames = try Frames.read(setup.io, allocator, folder, &shot_names),
    };
}

/// The labels of a run, each cut from its frame, and the count a whole run must log.
fn measureItems(allocator: std.mem.Allocator, run: Run, table: []const u8, report: *Report, code: []const u8) ![]Item {
    var items: std.ArrayList(Item) = .empty;
    var titles: usize = 0;
    var measured: usize = 0;
    for (run.shots) |shot| {
        const frame = run.frames.named(shot.name) orelse continue;
        for (shot.rects.items) |drawn| {
            if (!measuredIn(shot.name, drawn.key)) continue;
            if (std.mem.eql(u8, shot.name, "main")) titles += 1;
            measured += 1;
            const item = (try common.cutLabel(allocator, shot, frame, drawn, table, report, code)) orelse continue;
            try items.append(allocator, item);
        }
    }
    try report.check(titles == expected_titles, "{s}: the Resource Editor logged {d} menu bar titles on the main frame (expected {d})", .{ code, titles, expected_titles });
    try report.check(measured == expected_measured, "{s}: the Resource Editor logged {d} measured labels over the four shots (expected {d})", .{ code, measured, expected_measured });
    return items.items;
}

/// Every measured label inside the frame, and the Help window's rows and manual inside the Help
/// window: what a clipped translation breaks. ImGui lays a widget out past the window's edge when
/// it does not fit, so the logged rect ends beyond it.
fn checkFit(run: Run, report: *Report, code: []const u8) !void {
    for (run.shots) |shot| {
        var window: ?Drawn = null;
        for (shot.rects.items) |drawn| {
            if (std.mem.eql(u8, shot.name, "help") and std.mem.eql(u8, drawn.key, "resource_app.window.help")) window = drawn;
        }
        for (shot.rects.items) |drawn| {
            if (!measuredIn(shot.name, drawn.key) and !fitsHelpWindow(shot.name, drawn.key)) continue;
            try report.check(drawn.x + drawn.w <= shot.width and drawn.y + drawn.h <= shot.height, "{s} {s}: the label ends at {d:.0},{d:.0} inside the {d:.0}x{d:.0} px window (fit)", .{ code, drawn.key, drawn.x + drawn.w, drawn.y + drawn.h, shot.width, shot.height });
            if (!fitsHelpWindow(shot.name, drawn.key)) continue;
            const frame = window orelse {
                try report.check(false, "{s} {s}: the Help window logged no rect (fit)", .{ code, drawn.key });
                continue;
            };
            try report.check(drawn.x >= frame.x and drawn.x + drawn.w <= frame.x + frame.w and drawn.y + drawn.h <= frame.y + frame.h, "{s} {s}: the label {d:.0},{d:.0} {d:.0}x{d:.0} lies inside the Help window {d:.0},{d:.0} {d:.0}x{d:.0} (fit)", .{ code, drawn.key, drawn.x, drawn.y, drawn.w, drawn.h, frame.x, frame.y, frame.w, frame.h });
        }
    }
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
    const run = try runEditor(allocator, setup, "drop-ru", "ru", tables);
    const wanted = "l10n: missing ru " ++ drop_key;
    try report.check(countOf(run.log, wanted) == 1, "drop: the Resource Editor logged \"{s}\" {d} times (once)", .{ wanted, countOf(run.log, wanted) });
    try report.check(countOf(run.log, "l10n: missing ") == 1, "drop: the Resource Editor logged {d} missing-key lines in all (exactly the one)", .{countOf(run.log, "l10n: missing ")});

    const english = (try tableText(allocator, english_table, drop_key)).?;
    const russian = (try tableText(allocator, ru, drop_key)).?;
    try common.checkDroppedLabel(io, allocator, run, drop_shot, drop_key, english, russian, languages[1], report);
}

// ------------------------------------------------------------------ the test

test "the Resource Editor draws its main window in the ten languages (M002 S06, D066)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const exe = (try envValue(allocator, "BK_RESOURCE_EDITOR_EXE")) orelse {
        std.debug.print("BK_RESOURCE_EDITOR_EXE is not set: run zig build resource-editor-l10n-auto\n", .{});
        return error.MissingResourceEditor;
    };
    const cwd = std.Io.Dir.cwd();
    const cwd_path = try std.process.currentPathAlloc(io, allocator);
    const root = try std.fs.path.join(allocator, &.{ cwd_path, scratch });
    try cwd.createDirPath(io, root);
    const setup = Setup{ .io = io, .exe = exe, .root = root, .fixtures = try std.fs.path.join(allocator, &.{ cwd_path, fixtures_argument }) };
    const english_table = try languageTable(io, allocator, "en");

    // English first: the other languages' frames are held against it.
    var failures: usize = 0;
    var english_run: ?Run = null;
    for (languages) |language| {
        const is_english = std.mem.eql(u8, language.code, "en");
        if (!is_english and !try selected(allocator, language.code)) continue;
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        const run = try runEditor(allocator, setup, language.code, language.code, null);
        if (is_english) english_run = run;
        const table = try languageTable(io, allocator, language.code);
        try report.line("== {s}: {d} frames, {d} shots", .{ language.code, shot_names.len, run.shots.len });
        try report.check(countOf(run.log, "l10n: missing ") == 0, "{s}: the Resource Editor logged {d} missing-key lines (none: the table is whole)", .{ language.code, countOf(run.log, "l10n: missing ") });
        const items = try measureItems(allocator, run, table, &report, language.code);
        try checkFit(run, &report, language.code);
        try common.checkReferences(io, allocator, language, run, items, &report);
        if (language.differs) try common.checkDiffersFromEnglish(language, run, english_run.?.frames, items, max_unchanged, &report);
        if (std.mem.eql(u8, language.code, "hi")) try common.checkHindi(io, allocator, language, run, items, &report);
        if (common.isCjk(language.code)) try common.checkHanAtLeast(io, allocator, language, run, items, &report, least_han);
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ run.folder, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("resource-editor-l10n-auto: {s}: {d} measures failed; the report is {s}/{s}/report.txt\n", .{ language.code, report.failures, scratch, language.code });
    }

    if (try selected(allocator, "ru")) {
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        try report.line("== the drop case: ru without {s}", .{drop_key});
        try checkDrop(setup, allocator, english_table, &report);
        const folder = try std.fs.path.join(allocator, &.{ root, "drop-ru" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ folder, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("resource-editor-l10n-auto: drop: {d} measures failed; the report is {s}/drop-ru/report.txt\n", .{ report.failures, scratch });
    }
    if (failures != 0) return error.MeasureFailed;
}
