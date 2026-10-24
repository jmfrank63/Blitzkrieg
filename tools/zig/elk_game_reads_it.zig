//! E-06 on the real Game: a text the ELK database exported to a mod is the
//! text the game shows. The tracked Data/ELK is copied, exported unchanged to
//! `mods/ELKTest/data/texts.pak` of the installed game (D-09) and the Game
//! started on it with its own `-reference-scene` capture of the main menu; then
//! the main menu's Exit Game label gets a marker translation, the database is
//! exported again and the Game captured the same way. The two frames must
//! differ, and only inside the Exit Game button: the game drew the marker
//! where it draws that label, and nothing else changed. The Game's own traces
//! back it up: BK_MOD_TRACE says the label's text was opened from the mod and
//! BK_UI_TRACE that the main menu is laid out 1:1 on the 1024x768 canvas the
//! button's rect below is in.
//!
//! The user's data is never touched: the Game runs from a scratch folder (its
//! `profiles/` are relative to the working directory) in the dedicated ELKTest
//! profile, with XDG_DATA_HOME and BK_USER_ROOT in that folder too. The mod is
//! removed from the installation afterwards. BK_ELK_GAME names the installed
//! Game executable; `zig build elk-game-reads-it` sets it.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const measure = @import("l10n_measure.zig");
const elk = @import("elk");
const database = elk.database;
const packages = elk.packages;
const testing = std.testing;

/// The value of an environment variable the build step sets, or null.
/// testing.environ.getPosix compiles only for POSIX targets (on Windows its
/// block has no view(), run 39086936588); the map works everywhere.
fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

const scratch = "zig-out/local-test/elk/game-reads-it";
const mod_name = "ELKTest";
/// `imheader-gbutton-exitgame` in Data/UI/MainMenu.xml: the button in the
/// main menu the game opens on.
const label_key = "textes/strings/imheader-gbutton-exitgame";
const marker = "ELK E-06 MARKER";
const width = 1024;
const height = 768;
/// The Exit Game button on the 1024x768 canvas, as CSimpleWindow::Reposition
/// places it: its panel (element 2000, LEFT|TOP) at (610,267) sized 400x328,
/// the button (element 10006, HMID|TOP) at (0,260) sized 320x50, so x from
/// 610 + 200 - 160 = 650 to 970 and y from 267 + 260 = 527 to 577. Exclusive
/// right and bottom.
const label_rect = Rect{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 };
/// The settings screen's Gameplay tab: the division's name, which the screen
/// sets on its tab button (element 10008 in Data/UI/OptionsSettings.xml, at
/// (45,223) sized 433x66 on the 1024x768 canvas).
const tab_key = "textes/options/gameplay.name";
const tab_marker = "ELK TAB MARKER";
const gameplay_tab = Rect{ .x1 = 45, .y1 = 223, .x2 = 478, .y2 = 289 };

const Rect = struct {
    x1: usize,
    y1: usize,
    x2: usize,
    y2: usize,

    fn contains(self: Rect, other: Rect) bool {
        return other.x1 >= self.x1 and other.y1 >= self.y1 and other.x2 <= self.x2 and other.y2 <= self.y2;
    }
};

test "the Game shows a main-menu label the ELK exported to a mod (E-06)" {
    const io = testing.io;
    const gpa = testing.allocator;
    var arena = std.heap.ArenaAllocator.init(gpa);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_ELK_GAME")) orelse {
        std.debug.print("BK_ELK_GAME is not set: run zig build elk-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    try cwd.createDirPath(io, user);

    // A copy of the tracked database: the edit below must not reach Data/ELK.
    const db_folder = try std.fs.path.join(allocator, &.{ root, "db" });
    try database.copyTree(io, "Data/ELK/game_data_base", try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    const elk_xml = try std.fs.path.join(allocator, &.{ db_folder, "elk.xml" });
    try cwd.writeFile(io, .{ .sub_path = elk_xml, .data = try cwd.readFileAlloc(io, "Data/ELK/elk.xml", allocator, .limited(1 << 20)) });
    var db = try database.Database.open(gpa, io, elk_xml);
    defer db.deinit();

    const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", mod_name });
    defer cwd.deleteTree(io, mod_folder) catch {};

    try exportMod(io, allocator, &db, game_root, null);
    const control = try runGame(io, allocator, game, user, try std.fs.path.join(allocator, &.{ root, "control.rgba" }), "control");

    // The translator's edit: the label is Approved and has no translation,
    // so the export carries its original until a translation is written.
    try testing.expect((try db.readState(0, label_key)).state != .not_translated);
    try testing.expect(try db.writeTranslation(0, label_key, marker));
    try exportMod(io, allocator, &db, game_root, marker);
    const marked = try runGame(io, allocator, game, user, try std.fs.path.join(allocator, &.{ root, "marker.rgba" }), "marker");

    // The measurement: every pixel that differs lies in the label's button,
    // and enough of them differ to be glyphs rather than noise.
    try testing.expectEqual(@as(usize, width * height * 4), control.len);
    try testing.expectEqual(control.len, marked.len);
    var changed: usize = 0;
    var box = Rect{ .x1 = width, .y1 = height, .x2 = 0, .y2 = 0 };
    for (0..height) |y| for (0..width) |x| {
        const at = (y * width + x) * 4;
        if (std.mem.eql(u8, control[at..][0..3], marked[at..][0..3])) continue;
        changed += 1;
        box = .{ .x1 = @min(box.x1, x), .y1 = @min(box.y1, y), .x2 = @max(box.x2, x + 1), .y2 = @max(box.y2, y + 1) };
    };
    errdefer std.debug.print("E-06: {d} pixels differ, in ({d},{d})-({d},{d}); the Exit Game button is ({d},{d})-({d},{d})\n", .{ changed, box.x1, box.y1, box.x2, box.y2, label_rect.x1, label_rect.y1, label_rect.x2, label_rect.y2 });
    try testing.expect(changed >= 100);
    try testing.expect(label_rect.contains(box));
}

/// The Exit Game button in MainMenu.xml coordinates, before the menu is
/// scaled onto the window (label_rect above is the same button at scale 1).
const exit_button = label_rect;

test "the Options' Language row switches the Game's texts at once and keeps the choice" {
    // The language option on the real Game, the E-06 mod as the second text
    // source. The Game runs windowed at the desktop's size with BK_AUTO_UI
    // driving the real settings screen: Options > Settings, the Gameplay tab
    // (10008), four rows down to Language, then RIGHT (the mod) or LEFT
    // (Default), V, and back to the main panel. `shot` writes the last frame
    // drawn; every claim is measured inside the Exit Game button, where the
    // label is, because the menu's backdrop is animated.
    const io = testing.io;
    const gpa = testing.allocator;
    var arena = std.heap.ArenaAllocator.init(gpa);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_ELK_GAME")) orelse {
        std.debug.print("BK_ELK_GAME is not set: run zig build elk-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch ++ "-language" });
    cwd.deleteTree(io, scratch ++ "-language") catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    // The Game looks for defconf.cfg, the options' declarations, under its
    // working directory (.\Data\Configs and up to four folders above); run
    // from the scratch folder it needs the installed copy beside it, or it
    // has no options at all.
    const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
    try cwd.createDirPath(io, configs);
    const defconf = try std.fs.path.join(allocator, &.{ game_root, "Data", "Configs", "defconf.cfg" });
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }), .data = try cwd.readFileAlloc(io, defconf, allocator, .limited(1 << 20)) });

    const db_folder = try std.fs.path.join(allocator, &.{ root, "db" });
    try database.copyTree(io, "Data/ELK/game_data_base", try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    const elk_xml = try std.fs.path.join(allocator, &.{ db_folder, "elk.xml" });
    try cwd.writeFile(io, .{ .sub_path = elk_xml, .data = try cwd.readFileAlloc(io, "Data/ELK/elk.xml", allocator, .limited(1 << 20)) });
    var db = try database.Database.open(gpa, io, elk_xml);
    defer db.deinit();
    const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", mod_name });
    defer cwd.deleteTree(io, mod_folder) catch {};
    try testing.expect(try db.writeTranslation(0, label_key, marker));
    try testing.expect(try db.writeTranslation(0, tab_key, tab_marker));
    try exportMod(io, allocator, &db, game_root, marker);

    // Opens the settings screen from the main panel at `frame`, picks the
    // Language row's next (RIGHT) or previous (LEFT) entry, confirms and goes
    // back from the Options panel to the main panel, there by `frame` + 400.
    // The waits are the screens' own open and close transitions.
    const switch_to = struct {
        fn at(a: std.mem.Allocator, frame: u32, key: []const u8) ![]const u8 {
            return std.fmt.allocPrint(a, "{d}:settings,{d}:msg=10003,{d}:msg=10008,{d}:key=DOWN,{d}:key=DOWN,{d}:key=DOWN,{d}:key=DOWN,{d}:key={s},{d}:ok,{d}:cancel", .{ frame, frame + 30, frame + 130, frame + 150, frame + 160, frame + 170, frame + 180, frame + 210, key, frame + 240, frame + 320 });
        }
    }.at;
    // Run 1: the shipped label (60), the mod chosen in the options (480),
    // Default chosen again (900), the mod chosen once more and the Game left.
    // The settings screen itself is shot on the Gameplay tab just before the
    // first switch (265) and just after it (300), still open.
    const first = try std.mem.join(allocator, ",", &.{ "60:shot", try switch_to(allocator, 70, "RIGHT"), "265:shot", "300:shot", "480:shot", try switch_to(allocator, 490, "LEFT"), "900:shot", try switch_to(allocator, 910, "RIGHT"), "1320:exit" });
    const log1 = try runAuto(io, allocator, game, user, first, "language-1");
    const layout = try menuLayout(log1);
    const button = layout.scaled(exit_button);
    const original = try shot(io, allocator, user, 60, layout);
    const chosen = try shot(io, allocator, user, 480, layout);
    const back = try shot(io, allocator, user, 900, layout);
    const settings_layout = try screenLayout(log1, "ui\\OptionsSettings");
    const tab = settings_layout.scaled(gameplay_tab);
    const tab_before = try shot(io, allocator, user, 265, settings_layout);
    const tab_switched = try shot(io, allocator, user, 300, settings_layout);
    // Run 3, the same profile, which kept the mod: the settings screen opened
    // on the language it started in, the reference the live switch must match.
    _ = try runAuto(io, allocator, game, user, "60:settings,90:msg=10003,190:msg=10008,265:shot,280:exit", "language-3");
    const tab_cold = try shot(io, allocator, user, 265, settings_layout);
    // Run 2, the same profile: the menu comes up in the mod's language; then
    // Default is set on the open main menu at frame 80, the way the row
    // commits it, and the frames on either side of the switch are kept.
    const log2 = try runAuto(io, allocator, game, user, "60:shot,80:set=GamePlay.Language=Default,80:shot,81:shot,100:exit", "language-2");

    try expectLine(log1, "language-1", "language: \"" ++ mod_name ++ "\" mounted, texts reloaded");
    try expectLine(log1, "language-1", "language: \"Default\" mounted, texts reloaded");
    // The second run mounts the mod before its first scripted frame.
    const restart = std.mem.indexOf(u8, log2, "language: \"" ++ mod_name ++ "\" mounted, texts reloaded") orelse return error.LanguageNotRestored;
    try testing.expect(restart < (std.mem.indexOf(u8, log2, "BK_AUTO_UI: frame 60 action shot") orelse return error.GameLineMissing));
    if (!opensFromOverlay(log2, label_key ++ ".txt", "LANGUAGE")) return error.LabelNotReadFromLanguage;

    const restarted = try shot(io, allocator, user, 60, layout);
    const before = try shot(io, allocator, user, 80, layout);
    const after = try shot(io, allocator, user, 81, layout);
    const marked = changedIn(original, chosen, layout.width, button);
    errdefer std.debug.print("language: the Exit Game button ({d},{d})-({d},{d}) on a {d}x{d} window; {d} of its pixels change with the mod chosen, {d} differ from the original with Default chosen again, {d} from the chosen frame after a restart, {d} between the frames drawn just before and just after a switch on the open menu, {d} from the original in the frame just after\n", .{
        button.x1, button.y1, button.x2, button.y2, layout.width, layout.height, marked, changedIn(original, back, layout.width, button), changedIn(chosen, restarted, layout.width, button), changedIn(before, after, layout.width, button), changedIn(original, after, layout.width, button),
    });
    try testing.expect(marked >= 100);
    try testing.expectEqual(@as(usize, 0), changedIn(original, back, layout.width, button));
    try testing.expectEqual(@as(usize, 0), changedIn(chosen, restarted, layout.width, button));
    try testing.expectEqual(@as(usize, 0), changedIn(chosen, before, layout.width, button));
    try testing.expectEqual(@as(usize, 0), changedIn(original, after, layout.width, button));
    // The tab buttons are texts the settings screen copies out of the text
    // manager. Refreshed on the choice instead of on the reload, they showed
    // the previous language until the next switch.
    try testing.expect(changedIn(tab_before, tab_switched, settings_layout.width, tab) >= 100);
    try testing.expectEqual(@as(usize, 0), changedIn(tab_cold, tab_switched, settings_layout.width, tab));
}

/// How the main menu sits in the window, from BK_UI_TRACE's line for it.
const Layout = struct {
    width: usize,
    height: usize,
    x: f32,
    y: f32,
    scale_x: f32,
    scale_y: f32,

    /// A rect of the 1024x768 layout on the window, one pixel larger on
    /// every side for the glyphs' filtered edges.
    fn scaled(self: Layout, rect: Rect) Rect {
        const fx = struct {
            fn at(origin: f32, scale: f32, value: usize) f32 {
                return origin + scale * @as(f32, @floatFromInt(value));
            }
        }.at;
        return .{
            .x1 = @intFromFloat(@max(0, @floor(fx(self.x, self.scale_x, rect.x1)) - 1)),
            .y1 = @intFromFloat(@max(0, @floor(fx(self.y, self.scale_y, rect.y1)) - 1)),
            .x2 = @min(self.width, @as(usize, @intFromFloat(@ceil(fx(self.x, self.scale_x, rect.x2)))) + 1),
            .y2 = @min(self.height, @as(usize, @intFromFloat(@ceil(fx(self.y, self.scale_y, rect.y2)))) + 1),
        };
    }
};

/// The main menu's layout when the run's first shot was taken: the last
/// reposition before it, as the window may still settle after the first one
/// (on macbook-air-frank's 1440x900 screen it shrank from 799 to 797 rows).
fn menuLayout(log: []const u8) !Layout {
    return screenLayout(log, "ui\\mainmenu");
}

/// The layout of `screen` when it was first shot: the last reposition of it
/// before the first shot taken after it was laid out.
fn screenLayout(log: []const u8, comptime screen: []const u8) !Layout {
    const prefix = "BK_UI_TRACE: reposition \"" ++ screen ++ "\" screen ";
    const laid_out = std.mem.indexOf(u8, log, prefix) orelse return error.GameLineMissing;
    const first_shot = (std.mem.indexOfPos(u8, log, laid_out, " action shot") orelse return error.GameLineMissing);
    const start = (std.mem.lastIndexOf(u8, log[0..first_shot], prefix) orelse return error.GameLineMissing) + prefix.len;
    const line = log[start .. std.mem.indexOfScalarPos(u8, log, start, '\n') orelse log.len];
    // "<w>x<h> -> parent <pw>x<ph> at (<x>,<y>) scale <sx>,<sy> ..."
    var fields = std.mem.tokenizeAny(u8, line, " x(),\r");
    const window_width = try std.fmt.parseInt(usize, fields.next() orelse return error.BadTrace, 10);
    const window_height = try std.fmt.parseInt(usize, fields.next() orelse return error.BadTrace, 10);
    if (!std.mem.eql(u8, fields.next() orelse "", "->") or !std.mem.eql(u8, fields.next() orelse "", "parent")) return error.BadTrace;
    _ = fields.next();
    _ = fields.next();
    if (!std.mem.eql(u8, fields.next() orelse "", "at")) return error.BadTrace;
    const x = try std.fmt.parseFloat(f32, fields.next() orelse return error.BadTrace);
    const y = try std.fmt.parseFloat(f32, fields.next() orelse return error.BadTrace);
    if (!std.mem.eql(u8, fields.next() orelse "", "scale")) return error.BadTrace;
    const scale_x = try std.fmt.parseFloat(f32, fields.next() orelse return error.BadTrace);
    const scale_y = try std.fmt.parseFloat(f32, fields.next() orelse return error.BadTrace);
    return .{ .width = window_width, .height = window_height, .x = x, .y = y, .scale_x = scale_x, .scale_y = scale_y };
}

/// The frame BK_AUTO_UI's `shot` at `frame` wrote into the working directory.
/// Every run counts frames from 1 and overwrites the last run's shot of the
/// same frame, so read a run's shots before starting the next.
fn shot(io: std.Io, allocator: std.mem.Allocator, user: []const u8, frame: u32, layout: Layout) ![]u8 {
    const name = try std.fmt.allocPrint(allocator, "autoshot_{d}_{d}x{d}.rgba", .{ frame, layout.width, layout.height });
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, try std.fs.path.join(allocator, &.{ user, name }), allocator, .limited(64 << 20));
    try testing.expectEqual(layout.width * layout.height * 4, bytes.len);
    return bytes;
}

fn changedIn(a: []const u8, b: []const u8, row: usize, rect: Rect) usize {
    var changed: usize = 0;
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        const at = (y * row + x) * 4;
        if (!std.mem.eql(u8, a[at..][0..3], b[at..][0..3])) changed += 1;
    };
    return changed;
}

/// Runs the Game in the scratch profile under a BK_AUTO_UI schedule; it must
/// exit 0. Returns its output, kept beside the shots as `<name>.log`.
fn runAuto(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, schedule: []const u8, name: []const u8) ![]u8 {
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_MOD_TRACE", "1");
    try environ.put("BK_UI_TRACE", "1");
    try environ.put("BK_GLYPH_TRACE", "1");
    try environ.put("BK_NO_HELP", "1");
    try environ.put("BK_AUTO_UI", schedule);
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", "-profile=ELKLanguage" },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(240), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    const log_path = try std.fs.path.join(allocator, &.{ user, try std.fmt.allocPrint(allocator, "{s}.log", .{name}) });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try measure.expectNoMissingGlyph(log, log_path);
    try measure.expectNoLeak(log, log_path);
    return log;
}

/// Exports the database to the mod and checks, through the engine's zip
/// reader, that the label's entry carries `expected` (UTF-16 LE with a BOM,
/// as the game reads texts) or, with null, the original.
fn exportMod(io: std.Io, allocator: std.mem.Allocator, db: *database.Database, game_root: []const u8, expected: ?[]const u8) !void {
    const path = (try packages.exportTo(testing.allocator, io, db, 0, game_root, .{ .mod = mod_name }, .{})).path;
    defer testing.allocator.free(path);
    var package = try packages.readArchive(testing.allocator, io, path);
    defer package.deinit();
    const name = label_key ++ ".txt";
    const bytes = for (package.entries) |entry| {
        if (std.mem.eql(u8, entry.name, name)) break entry.bytes;
    } else return error.LabelNotExported;
    const wanted = if (expected) |text| try utf16(allocator, text) else (try db.readBytes(allocator, 0, label_key, .original)).?;
    try testing.expectEqualSlices(u8, wanted, bytes);
}

fn utf16(allocator: std.mem.Allocator, text: []const u8) ![]u8 {
    const out = try allocator.alloc(u8, 2 + text.len * 2);
    out[0] = 0xff;
    out[1] = 0xfe;
    for (text, 0..) |byte, at| {
        out[2 + at * 2] = byte;
        out[3 + at * 2] = 0;
    }
    return out;
}

/// Runs the Game on the mod to its main-menu capture and returns the frame
/// (RGBA, alpha zero). It must exit 0, report the capture, open the label's
/// text from the mod and lay the main menu out at scale 1. Its output is kept
/// beside the capture as `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, capture: []const u8, name: []const u8) ![]u8 {
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_MOD_TRACE", "1");
    try environ.put("BK_UI_TRACE", "1");
    try environ.put("BK_GLYPH_TRACE", "1");
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-reference-scene", capture, "-reference-resolution", "1024", "768", "-windowed", "-mod=" ++ mod_name, "-profile=ELKTest" },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    const log_path = try std.mem.concat(allocator, u8, &.{ capture[0 .. capture.len - ".rgba".len], ".log" });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try measure.expectNoMissingGlyph(log, log_path);
    try measure.expectNoLeak(log, log_path);
    try expectLine(log, log_path, "BK_REFERENCE_SCENE: capture complete");
    try expectLine(log, log_path, "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
    if (!opensFromMod(log, label_key ++ ".txt")) {
        std.debug.print("Game ({s}) did not open {s}.txt from the mod; its output is in {s}\n", .{ name, label_key, log_path });
        return error.LabelNotReadFromMod;
    }
    return std.Io.Dir.cwd().readFileAlloc(io, capture, allocator, .limited(16 << 20));
}

fn expectLine(log: []const u8, log_path: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}\n", .{ line, log_path });
    return error.GameLineMissing;
}

/// Whether a BK_MOD_TRACE line says the MOD overlay answered `name`, the
/// game's spelling (backslashes, any case) compared without regard to either.
fn opensFromMod(log: []const u8, name: []const u8) bool {
    return opensFromOverlay(log, name, "MOD");
}

/// The same for any overlay: MOD, or LANGUAGE, the language option's pak.
fn opensFromOverlay(log: []const u8, name: []const u8, overlay: []const u8) bool {
    const prefix = "BK_MOD_TRACE: open \"";
    var buffer: [32]u8 = undefined;
    const suffix = std.fmt.bufPrint(&buffer, "\" from {s}", .{overlay}) catch return false;
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        if (!std.mem.startsWith(u8, line, prefix) or !std.mem.endsWith(u8, line, suffix)) continue;
        const opened = line[prefix.len .. line.len - suffix.len];
        if (opened.len != name.len) continue;
        for (opened, name) |a, b| {
            const left = if (a == '\\') '/' else std.ascii.toLower(a);
            if (left != std.ascii.toLower(b)) break;
        } else return true;
    }
    return false;
}

test "a BK_MOD_TRACE line names the label whatever its slashes and case" {
    const log = "BK_MOD_TRACE: open \"Textes\\Strings\\imheader-gbutton-exitgame.txt\" from MOD\r\nother\n";
    try testing.expect(opensFromMod(log, label_key ++ ".txt"));
    try testing.expect(!opensFromMod("BK_MOD_TRACE: open \"textes\\strings\\imheader-gbutton-exitgame.txt\" from ELK\n", label_key ++ ".txt"));
}
