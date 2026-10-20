//! M002 S05 D066 on the real Map Editor: the installed editor started hidden on a shipped map in
//! each of the ten languages (the scratch cfg's `ui_language`) draws its main window with the menu
//! bar, its Tools palette and its Settings dialog, and the captured frames are measured by code.
//! The editor's `--l10n-auto` mode runs the fixed schedule below, captures `main`, `tools` and
//! `settings` and prints `l10n-auto: rect <key> <x> <y> <w> <h>` for every label it draws (every
//! frame; the last rect of a key before a shot is the captured layout, `RectsAre.before_shot`).
//! This runner reads those lines, decodes the TGAs, cuts each label's rect, takes the ink against
//! the rect's own background and compares it with hb-view's raster of the table's text at the UI's
//! 16 px in the language's Noto face (D069: the tier fails when hb-view is missing). The checks are
//! those of the ELK tier (l10n_auto_common.zig): the correlation with hb-view, a frame that differs
//! from English where the text differs, Hindi's headline and i matra, the regional Han faces.
//!
//! On top of that, per language (R053, the fit): every Tools palette button and its brush header lie
//! inside the palette's window, and every Settings row inside the Settings window, so a longer
//! translation that the layout constants clip fails here with the language and the key.
//!
//! The missing-key case: `tools/zig/fixtures/l10n/map-editor-drop/ru.lang` is ru.lang without
//! `app.settings.use_default`. The editor on ru with `BK_L10N_DIR` at a copy of it must log exactly
//! one `l10n: missing ru app.settings.use_default` and draw that button in English: its ink
//! correlates with hb-view's English text and better than with the Russian one. The fixture must
//! stay ru.lang minus that line, which the runner checks.
//!
//! Frames, references, the editor's log and `report.txt` are kept in
//! `zig-out/local-test/map-editor/l10n-auto/<code>/` (the drop case in `drop-ru/`). The user's data
//! is never touched: XDG_DATA_HOME, BK_USER_ROOT and the settings file are in the scratch folder.
//! BK_MAP_EDITOR_EXE names the installed editor; `zig build map-editor-l10n-auto` sets it.
//! BK_L10N_LANGS (a comma list of codes, default all) picks the languages; the drop case runs with
//! ru, or when the list is unset.
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

const scratch = "zig-out/local-test/map-editor/l10n-auto";
const drop_fixture = "tools/zig/fixtures/l10n/map-editor-drop/ru.lang";
const drop_key = "app.settings.use_default";
const drop_shot = "settings";
const map_argument = "Data/Maps/Multiplayer/coldwinter.bzm";

/// The schedule (BK_EDITOR_AUTO frames): the main window as it opens, the Tools palette with the
/// Brush tool chosen (its pressed colour and the brush header), the Settings dialog opened by the
/// named command. Each shot comes a few frames after the change it shows, so the rects logged
/// before it are the captured layout.
const schedule = "6:shot=main,8:tool=brush,12:shot=tools,14:do=settings_window:on,18:shot=settings,20:exit";
const shot_names = [_][]const u8{ "main", "tools", "settings" };

/// How many labels a language must log in each frame: the nine menu bar titles and the six Settings
/// rows. The Tools palette's buttons are as many as English logs (at least `least_tools`).
const expected_titles = 9;
const expected_settings = 6;
const least_tools = 8;
/// A label may equal the English word in a language: at most this many of the Map Editor's 29 do
/// (Test, Info and the like are the same word in several languages).
const max_unchanged: usize = 5;

/// Whether the shot is where the label is measured: the menu titles in the main frame, the tool
/// buttons in the tools frame, the Settings rows in the settings frame. The windows' own title
/// rects (whole windows) and the brush header (a rule with text) are checked for fit, not by ink.
fn measuredIn(shot: []const u8, key: []const u8) bool {
    if (std.mem.eql(u8, shot, "main")) return std.mem.startsWith(u8, key, "app.menu.");
    if (std.mem.eql(u8, shot, "tools")) return std.mem.startsWith(u8, key, "app.tool.") or std.mem.startsWith(u8, key, "app.window.");
    if (std.mem.eql(u8, shot, "settings")) return (std.mem.startsWith(u8, key, "app.settings.") or std.mem.eql(u8, key, "kit.language.label")) and !std.mem.eql(u8, key, "app.settings.title");
    return false;
}

// ------------------------------------------------------------------ running the Map Editor

const Setup = struct {
    io: std.Io,
    exe: []const u8,
    /// Absolute.
    root: []const u8,
};

/// The editor on `code` with the scratch settings of `name`'s folder, and `table_dir` as BK_L10N_DIR
/// when given; the log and the three decoded frames.
fn runEditor(allocator: std.mem.Allocator, setup: Setup, name: []const u8, code: []const u8, table_dir: ?[]const u8) !Run {
    const cwd = std.Io.Dir.cwd();
    const folder = try std.fs.path.join(allocator, &.{ setup.root, name });
    cwd.deleteTree(setup.io, folder) catch {};
    try cwd.createDirPath(setup.io, folder);
    const config = try std.fs.path.join(allocator, &.{ folder, "mapeditor.cfg" });
    try cwd.writeFile(setup.io, .{ .sub_path = config, .data = try std.fmt.allocPrint(allocator, "ui_language={s}\r\n", .{code}) });
    const user_root = try std.fs.path.join(allocator, &.{ folder, "user" });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user_root);
    try environ.put("BK_USER_ROOT", user_root);
    try environ.put("BK_EDITOR_SETTINGS", config);
    try environ.put("BK_EDITOR_AUTO", schedule);
    if (table_dir) |dir| try environ.put("BK_L10N_DIR", dir);
    const result = try std.process.run(allocator, setup.io, .{
        .argv = &.{ setup.exe, "--hidden", "--l10n-auto", map_argument, folder },
        // The installation the editor runs in: Data, the engine and Fonts/ are beside the exe.
        .cwd = .{ .path = std.fs.path.dirname(setup.exe).? },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(setup.io, .{ .sub_path = try std.fs.path.join(allocator, &.{ folder, "map-editor.log" }), .data = log });
    if (!result.term.success()) {
        std.debug.print("Map Editor ({s}) ended {any}; its output is in {s}/map-editor.log\n", .{ name, result.term, folder });
        return error.MapEditorFailed;
    }
    return .{
        .log = log,
        .shots = try common.parseShots(allocator, log, .before_shot),
        .folder = folder,
        .frames = try Frames.read(setup.io, allocator, folder, &shot_names),
    };
}

fn rectOf(shot: Shot, key: []const u8) ?Drawn {
    for (shot.rects.items) |drawn| if (std.mem.eql(u8, drawn.key, key)) return drawn;
    return null;
}

fn shotNamed(run: Run, name: []const u8) ?Shot {
    for (run.shots) |shot| if (std.mem.eql(u8, shot.name, name)) return shot;
    return null;
}

/// The labels of a run, each cut from its frame, and the counts a whole run must log.
fn measureItems(allocator: std.mem.Allocator, run: Run, table: []const u8, report: *Report, code: []const u8, english_tools: ?usize, tools_logged: *usize) ![]Item {
    var items: std.ArrayList(Item) = .empty;
    var titles: usize = 0;
    var tools: usize = 0;
    var settings: usize = 0;
    for (run.shots) |shot| {
        const frame = run.frames.named(shot.name) orelse continue;
        for (shot.rects.items) |drawn| {
            if (!measuredIn(shot.name, drawn.key)) continue;
            if (std.mem.eql(u8, shot.name, "main")) titles += 1;
            if (std.mem.eql(u8, shot.name, "tools")) tools += 1;
            if (std.mem.eql(u8, shot.name, "settings")) settings += 1;
            const item = (try common.cutLabel(allocator, shot, frame, drawn, table, report, code)) orelse continue;
            try items.append(allocator, item);
        }
    }
    tools_logged.* = tools;
    try report.check(titles == expected_titles, "{s}: the Map Editor logged {d} menu bar titles on the main frame (expected {d})", .{ code, titles, expected_titles });
    try report.check(settings == expected_settings, "{s}: the Map Editor logged {d} Settings rows on the settings frame (expected {d})", .{ code, settings, expected_settings });
    try report.check(tools >= least_tools, "{s}: the Map Editor logged {d} Tools palette buttons (at least {d})", .{ code, tools, least_tools });
    if (english_tools) |want| try report.check(tools == want, "{s}: the Tools palette has {d} buttons, as many as English's {d}", .{ code, tools, want });
    return items.items;
}

// ------------------------------------------------------------------ the fit (R053)

/// Whether `inner` lies inside `outer`, a pixel of slack for the rounding of the logged rects.
fn inside(inner: Drawn, outer: Drawn) bool {
    const slack = 1.0;
    return inner.x >= outer.x - slack and inner.y >= outer.y - slack and inner.x + inner.w <= outer.x + outer.w + slack and inner.y + inner.h <= outer.y + outer.h + slack;
}

/// ImGui's scrollbar, which a panel's widgets must stay clear of: the Players panel scrolls.
const scrollbar_width = 14;

/// The labels `keys` of a docked panel inside its window (less the scrollbar's width at the right),
/// the left panel's filter buttons and the right panel's combo labels.
fn checkPanel(shot: Shot, report: *Report, code: []const u8, window_key: []const u8, keys: []const []const u8) !void {
    const window = rectOf(shot, window_key) orelse return report.check(false, "{s}: no rect for the panel {s}", .{ code, window_key });
    const usable: Drawn = .{ .key = window.key, .x = window.x, .y = window.y, .w = window.w - scrollbar_width, .h = window.h };
    for (keys) |key| {
        const drawn = rectOf(shot, key) orelse {
            try report.check(false, "{s}: no rect for {s} in the panel {s}", .{ code, key, window_key });
            continue;
        };
        try report.check(inside(drawn, usable), "{s} {s}: the label's box {d:.0},{d:.0} {d:.0}x{d:.0} lies inside {s} {d:.0},{d:.0} {d:.0}x{d:.0} without its scrollbar (fit)", .{ code, key, drawn.x, drawn.y, drawn.w, drawn.h, window_key, window.x, window.y, window.w, window.h });
    }
}

/// Every button of the Tools palette and its brush header inside the palette's window, every
/// Settings row inside the Settings window, and every menu title inside the frame: what a clipped
/// translation breaks. ImGui lays a widget out past the window's edge when it does not fit, so the
/// logged rect is outside it.
fn checkFit(run: Run, report: *Report, code: []const u8) !void {
    if (shotNamed(run, "tools")) |shot| {
        if (rectOf(shot, "app.tools.title")) |window| {
            for (shot.rects.items) |drawn| {
                if (!measuredIn("tools", drawn.key) and !std.mem.eql(u8, drawn.key, "app.tools.brush_header")) continue;
                try report.check(inside(drawn, window), "{s} {s}: the label's box {d:.0},{d:.0} {d:.0}x{d:.0} lies inside the Tools palette {d:.0},{d:.0} {d:.0}x{d:.0} (fit)", .{ code, drawn.key, drawn.x, drawn.y, drawn.w, drawn.h, window.x, window.y, window.w, window.h });
            }
        } else try report.check(false, "{s}: no rect for the Tools palette", .{code});
    }
    if (shotNamed(run, "settings")) |shot| {
        if (rectOf(shot, "app.settings.title")) |window| {
            for (shot.rects.items) |drawn| {
                if (!measuredIn("settings", drawn.key)) continue;
                try report.check(inside(drawn, window), "{s} {s}: the label's box {d:.0},{d:.0} {d:.0}x{d:.0} lies inside the Settings window {d:.0},{d:.0} {d:.0}x{d:.0} (fit)", .{ code, drawn.key, drawn.x, drawn.y, drawn.w, drawn.h, window.x, window.y, window.w, window.h });
            }
        } else try report.check(false, "{s}: no rect for the Settings window", .{code});
    }
    if (shotNamed(run, "main")) |shot| {
        try checkPanel(shot, report, code, "app.objects.title", &.{ "app.filter.new", "app.filter.delete" });
        try checkPanel(shot, report, code, "app.players.title", &.{ "app.players.map_type", "app.players.attacking" });
        const frame_width = shot.width;
        for (shot.rects.items) |drawn| {
            if (!measuredIn("main", drawn.key)) continue;
            try report.check(drawn.x + drawn.w <= frame_width, "{s} {s}: the menu title ends at {d:.0} inside the {d:.0} px window (fit)", .{ code, drawn.key, drawn.x + drawn.w, frame_width });
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
    try report.check(countOf(run.log, wanted) == 1, "drop: the Map Editor logged \"{s}\" {d} times (once)", .{ wanted, countOf(run.log, wanted) });
    try report.check(countOf(run.log, "l10n: missing ") == 1, "drop: the Map Editor logged {d} missing-key lines in all (exactly the one)", .{countOf(run.log, "l10n: missing ")});

    const english = (try tableText(allocator, english_table, drop_key)).?;
    const russian = (try tableText(allocator, ru, drop_key)).?;
    try common.checkDroppedLabel(io, allocator, run, drop_shot, drop_key, english, russian, languages[1], report);
}

// ------------------------------------------------------------------ the test

test "the Map Editor draws its main window, Tools palette and Settings dialog in the ten languages (M002 S05, D066)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const exe = (try envValue(allocator, "BK_MAP_EDITOR_EXE")) orelse {
        std.debug.print("BK_MAP_EDITOR_EXE is not set: run zig build map-editor-l10n-auto\n", .{});
        return error.MissingMapEditor;
    };
    const cwd = std.Io.Dir.cwd();
    const cwd_path = try std.process.currentPathAlloc(io, allocator);
    const root = try std.fs.path.join(allocator, &.{ cwd_path, scratch });
    try cwd.createDirPath(io, root);
    const setup = Setup{ .io = io, .exe = exe, .root = root };
    const english_table = try languageTable(io, allocator, "en");

    // English first: the other languages' frames and button counts are held against it.
    var failures: usize = 0;
    var english_run: ?Run = null;
    var english_tools: ?usize = null;
    for (languages) |language| {
        const is_english = std.mem.eql(u8, language.code, "en");
        if (!is_english and !try selected(allocator, language.code)) continue;
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        const run = try runEditor(allocator, setup, language.code, language.code, null);
        if (is_english) english_run = run;
        const table = try languageTable(io, allocator, language.code);
        try report.line("== {s}: {d} frames, {d} shots", .{ language.code, shot_names.len, run.shots.len });
        try report.check(countOf(run.log, "l10n: missing ") == 0, "{s}: the Map Editor logged {d} missing-key lines (none: the table is whole)", .{ language.code, countOf(run.log, "l10n: missing ") });
        var tools_logged: usize = 0;
        const items = try measureItems(allocator, run, table, &report, language.code, english_tools, &tools_logged);
        if (is_english) english_tools = tools_logged;
        try checkFit(run, &report, language.code);
        try common.checkReferences(io, allocator, language, run, items, &report);
        if (language.differs) try common.checkDiffersFromEnglish(language, run, english_run.?.frames, items, max_unchanged, &report);
        if (std.mem.eql(u8, language.code, "hi")) try common.checkHindi(io, allocator, language, run, items, &report);
        if (common.isCjk(language.code)) try common.checkHan(io, allocator, language, run, items, &report);
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ run.folder, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("map-editor-l10n-auto: {s}: {d} measures failed; the report is {s}/{s}/report.txt\n", .{ language.code, report.failures, scratch, language.code });
    }

    if (try selected(allocator, "ru")) {
        var report = Report{ .allocator = allocator };
        defer failures += report.failures;
        try report.line("== the drop case: ru without {s}", .{drop_key});
        try checkDrop(setup, allocator, english_table, &report);
        const folder = try std.fs.path.join(allocator, &.{ root, "drop-ru" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ folder, "report.txt" }), .data = report.lines.items });
        if (report.failures != 0) std.debug.print("map-editor-l10n-auto: drop: {d} measures failed; the report is {s}/drop-ru/report.txt\n", .{ report.failures, scratch });
    }
    if (failures != 0) return error.MeasureFailed;
}
