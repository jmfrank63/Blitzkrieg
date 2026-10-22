//! M004 S03: is bk_text not slower than the bitmap fonts on the screens a player looks at? The real
//! Game runs twice on each screen (bitmap, then BK_TEXT_ENGINE=ttf) with BK_PERF=1, holds the screen
//! and exits late through BK_AUTO_UI (not -reference-scene, which exits at its capture). Frames per
//! second on a vsync-capped present hide the text cost, so the comparison is the Game's own
//! `BK_PERF text:` line, once a second: steady-clock milliseconds spent measuring, wrapping and
//! drawing text, the same counters on both engines (GraphicsEngineGpu.cpp, GfxGpuObjectFactory.cpp).
//!
//! Screens: the options screen with a tooltip, the Churchill's encyclopedia page, the first mission's
//! briefing and the first mission running. Languages: en, hi (bitmap: the shaped `lang-hi` mod with
//! its Private Use Area clusters; ttf: `lang-hi-ttf`, plain Unicode) and ja (`lang-ja` on both). A
//! language whose mod is not in the game's mods folder is `not run` in the report, never measured:
//! `language-mods` fails on ja (TextDoesNotFit), so there is no ja mod to run.
//!
//! The samples are the text lines the Game prints after the screen's last scheduled action; the first
//! `settle_samples` of them (the screen loading, the tooltip's dwell) are dropped, and the medians of
//! text ms per frame, ms per frame and text calls per frame are the numbers. The tier fails a screen
//! whose ttf text time is above `bitmap * 1.10 + 0.3` ms per frame, or which left fewer than
//! `min_samples` samples. That is the rule for the release build a player runs. A debug build (its
//! path says so) keeps the ratio but allows `debug_extra_ms` more: its Zig and C++ are unoptimized, a
//! ttf line costs about a microsecond more per glyph there, and the options screen's 200 text calls a
//! frame turn that into a gap of up to a millisecond that a release build does not have (options, en:
//! bitmap 0.20, ttf 0.31 ms release; bitmap 1.2, ttf 1.7 to 1.9 ms debug). `report.txt` in `zig-out/local-test/text-engine-perf/` lists every median (`report-<codes>.txt` for a shard);
//! BK_TEXT_PERF_LANGS limits the languages (a comma list, default all), BK_TEXT_PERF_HOLD sets the
//! hold in frames. The user's data is never touched: XDG_DATA_HOME and BK_USER_ROOT are the scratch
//! folder. BK_TEXT_PERF_GAME names the installed Game executable; `zig build text-engine-perf` sets it
//! and builds the mods first. Runs on Linux and macOS (a window is needed); it compiles on every target.
const std = @import("std");
const measure = @import("l10n_measure");
const testing = std.testing;

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

const scratch = "zig-out/local-test/text-engine-perf";
const profile_arg = "-profile=TextPerf";

/// Samples dropped after the last action: a second each, the screen settling.
const settle_samples: usize = 2;
const min_samples: usize = 2;
/// ttf may spend this much more than bitmap, relative and absolute (ms per frame).
const allowed_ratio: f64 = 1.10;
const allowed_extra_ms: f64 = 0.3;
/// What an unoptimized (debug) Game is allowed on top of `allowed_extra_ms`.
const debug_extra_ms: f64 = 1.3;
/// Frames the screen is held after its last action, when BK_TEXT_PERF_HOLD does not say.
const default_hold: u32 = 360;

const Screen = struct {
    tag: []const u8,
    /// BK_AUTO_UI up to and including the last action, which is at frame `last`.
    schedule: []const u8,
    last: u32,
    /// The log line that shows the last action ran.
    action: []const u8,
    /// A line that shows the screen itself reached the display; empty when the action is enough.
    screen: []const u8 = "",
    /// Frames to hold; a mission frame costs far more than a menu frame, so it is held shorter.
    hold_scale: f64 = 1.0,
};

const briefing_schedule = "60:campaign=2=scenarios\\campaigns\\allies\\allies,120:chapter=scenarios\\chapters\\allies\\norway\\1," ++
    "240:var=Mission.Last.FinishStatus=-1,300:mission=scenarios\\scenariomissions\\allies\\norway\\1";

const screens = [_]Screen{
    .{
        .tag = "options",
        .schedule = "60:settings,100:msg=10003,110:var=X64.ReferenceScene=0,400:hover=100x710",
        .last = 400,
        .action = "BK_AUTO_UI: frame 400 action hover=100x710",
        .screen = "BK_UI_TRACE: reposition \"ui\\OptionsSettings\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    },
    .{
        .tag = "encyclopedia",
        .schedule = "60:cmdc=269353109=1;Infantry_MKIV_Churchill_GB",
        .last = 60,
        .action = "BK_AUTO_UI: frame 60 action cmdc=269353109=1;Infantry_MKIV_Churchill_GB",
        .screen = "BK_UI_TRACE: reposition \"ui\\encyclopedia\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    },
    .{
        .tag = "briefing",
        .schedule = briefing_schedule,
        .last = 300,
        .action = "BK_AUTO_UI: frame 300 action mission=scenarios\\scenariomissions\\allies\\norway\\1",
        .screen = "BK_UI_TRACE: reposition \"ui\\common\\mission\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000",
    },
    .{
        .tag = "mission",
        .schedule = briefing_schedule ++ ",420:ok",
        .last = 420,
        .action = "BK_AUTO_UI: frame 420 action ok",
        .hold_scale = 0.35,
    },
};

const Language = struct {
    code: []const u8,
    /// The mod folders (language option values) of the bitmap run and of the ttf run; null is the
    /// base game.
    bitmap_mod: ?[]const u8,
    ttf_mod: ?[]const u8,
};

const languages = [_]Language{
    .{ .code = "en", .bitmap_mod = null, .ttf_mod = null },
    .{ .code = "hi", .bitmap_mod = "lang-hi", .ttf_mod = "lang-hi-ttf" },
    .{ .code = "ja", .bitmap_mod = "lang-ja", .ttf_mod = "lang-ja" },
};

/// What one run of the Game came to: the medians over the samples it kept.
const Medians = struct {
    samples: usize,
    text_ms: f64,
    frame_ms: f64,
    calls: f64,
};

/// The number after `name` in a `BK_PERF text:` line, or null.
fn field(line: []const u8, name: []const u8) ?f64 {
    var words = std.mem.tokenizeScalar(u8, line, ' ');
    while (words.next()) |word| {
        if (!std.mem.eql(u8, word, name)) continue;
        return std.fmt.parseFloat(f64, words.next() orelse return null) catch null;
    }
    return null;
}

fn median(allocator: std.mem.Allocator, values: []const f64) !f64 {
    if (values.len == 0) return 0;
    const sorted = try allocator.dupe(f64, values);
    std.mem.sort(f64, sorted, {}, std.sort.asc(f64));
    const middle = sorted.len / 2;
    return if (sorted.len % 2 == 1) sorted[middle] else (sorted[middle - 1] + sorted[middle]) / 2.0;
}

/// Medians of the text lines after `marker` in `log`, the first `settle_samples` dropped.
fn medians(allocator: std.mem.Allocator, log: []const u8, marker: []const u8) !Medians {
    const start = std.mem.indexOf(u8, log, marker) orelse return error.ActionNotInLog;
    var text: std.ArrayList(f64) = .empty;
    var frame_ms: std.ArrayList(f64) = .empty;
    var calls: std.ArrayList(f64) = .empty;
    var seen: usize = 0;
    var lines = std.mem.splitScalar(u8, log[start..], '\n');
    while (lines.next()) |line| {
        if (std.mem.indexOf(u8, line, "BK_PERF text:") == null) continue;
        seen += 1;
        if (seen <= settle_samples) continue;
        const frames = field(line, "frames") orelse continue;
        if (frames < 1) continue;
        const total = (field(line, "measure") orelse continue) + (field(line, "draw") orelse continue) + (field(line, "wrap") orelse continue);
        try text.append(allocator, total / frames);
        try frame_ms.append(allocator, field(line, "ms/frame") orelse continue);
        try calls.append(allocator, (field(line, "calls") orelse continue) / frames);
    }
    return .{
        .samples = text.items.len,
        .text_ms = try median(allocator, text.items),
        .frame_ms = try median(allocator, frame_ms.items),
        .calls = try median(allocator, calls.items),
    };
}

/// One scratch profile of the Game whose language option is `mod` (Default for the base game).
const Run = struct {
    io: std.Io,
    game: []const u8,
    folder: []const u8,
    user: []const u8,
    ttf: bool,

    fn init(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, name: []const u8, mod: ?[]const u8, ttf: bool) !Run {
        const cwd = std.Io.Dir.cwd();
        const folder = try std.fs.path.join(allocator, &.{ root, name });
        const user = try std.fs.path.join(allocator, &.{ folder, "user" });
        // defconf.cfg, the options' declarations, is looked for under the working directory.
        const configs = try std.fs.path.join(allocator, &.{ user, "Data", "Configs" });
        try cwd.createDirPath(io, configs);
        const defconf = try std.fs.path.join(allocator, &.{ std.fs.path.dirname(game).?, "Data", "Configs", "defconf.cfg" });
        try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ configs, "defconf.cfg" }), .data = try cwd.readFileAlloc(io, defconf, allocator, .limited(1 << 20)) });
        const run = Run{ .io = io, .game = game, .folder = folder, .user = user, .ttf = ttf };
        const schedule = try std.fmt.allocPrint(allocator, "30:set=GamePlay.Language={s},60:exit", .{mod orelse "Default"});
        const log = try run.play(allocator, &.{ "-windowed", profile_arg }, schedule, "choose");
        if (mod) |one| try expectLine(log, run.folder, try std.fmt.allocPrint(allocator, "language: \"{s}\" mounted", .{one}));
        return run;
    }

    fn play(self: Run, allocator: std.mem.Allocator, args: []const []const u8, schedule: []const u8, tag: []const u8) ![]u8 {
        var environ = try testing.environ.createMap(allocator);
        try environ.put("XDG_DATA_HOME", self.user);
        try environ.put("BK_USER_ROOT", self.user);
        try environ.put("BK_MEM_REPORT", "count");
        try environ.put("BK_PERF", "1");
        try environ.put("BK_DEBUG_LOG", "1");
        try environ.put("BK_UI_TRACE", "1");
        try environ.put("BK_NO_HELP", "1");
        try environ.put("BK_AUTO_UI", schedule);
        if (self.ttf) try environ.put("BK_TEXT_ENGINE", "ttf");
        var argv: std.ArrayList([]const u8) = .empty;
        try argv.append(allocator, self.game);
        try argv.appendSlice(allocator, args);
        const result = try measure.timed_run.run(allocator, self.io, .{
            .argv = argv.items,
            .cwd = .{ .path = self.user },
            .environ_map = &environ,
            .timeout = .{ .duration = .{ .raw = .fromSeconds(240), .clock = .awake } },
        });
        const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
        const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ self.folder, tag });
        try std.Io.Dir.cwd().writeFile(self.io, .{ .sub_path = log_path, .data = log });
        if (!result.term.success()) {
            std.debug.print("Game ({s} {s}) ended {any}; its output is in {s}\n", .{ self.folder, tag, result.term, log_path });
            return error.GameFailed;
        }
        // A bridge that fell back to the bitmap fonts logs `engine bitmap` and would pass for ttf.
        if (self.ttf) try expectLine(log, self.folder, "bk_text: engine ttf");
        return log;
    }

    /// The screen held to its late exit; the medians of what the Game measured meanwhile.
    fn hold(self: Run, allocator: std.mem.Allocator, screen: Screen, frames: u32) !Medians {
        const exit_at: u32 = screen.last + @as(u32, @intFromFloat(@as(f64, @floatFromInt(frames)) * screen.hold_scale));
        const schedule = try std.fmt.allocPrint(allocator, "{s},{d}:exit", .{ screen.schedule, exit_at });
        const log = try self.play(allocator, &.{ "-windowed", "-mode=1024x768", profile_arg }, schedule, screen.tag);
        try expectLine(log, self.folder, screen.action);
        if (screen.screen.len != 0) try expectLine(log, self.folder, screen.screen);
        return medians(allocator, log, screen.action);
    }
};

fn expectLine(log: []const u8, folder: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its logs are in {s}\n", .{ line, folder });
    return error.GameLineMissing;
}

fn selected(allocator: std.mem.Allocator, code: []const u8) !bool {
    const list = (try envValue(allocator, "BK_TEXT_PERF_LANGS")) orelse return true;
    var codes = std.mem.splitScalar(u8, list, ',');
    while (codes.next()) |one| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, one, " "), code)) return true;
    return false;
}

test "bk_text spends no more time on text than the bitmap fonts on the screens a player sees (M004 S03)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const game = (try envValue(allocator, "BK_TEXT_PERF_GAME")) orelse {
        std.debug.print("BK_TEXT_PERF_GAME is not set: run zig build text-engine-perf\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const frames: u32 = if (try envValue(allocator, "BK_TEXT_PERF_HOLD")) |text| try std.fmt.parseInt(u32, text, 10) else default_hold;
    const cwd = std.Io.Dir.cwd();
    const cwd_path = try std.process.currentPathAlloc(io, allocator);
    const root = try std.fs.path.join(allocator, &.{ cwd_path, scratch });
    try cwd.createDirPath(io, scratch);

    const debug_build = std.mem.indexOf(u8, game, "/debug/") != null or std.mem.indexOf(u8, game, "\\debug\\") != null;
    const extra_ms = allowed_extra_ms + if (debug_build) debug_extra_ms else 0.0;
    var lines: std.ArrayList(u8) = .empty;
    try lines.print(allocator, "text-engine-perf: game {s}, hold {d} frames after each screen's last action, first {d} samples dropped\n" ++
        "verdict rule: ttf text ms/frame <= bitmap * {d:.2} + {d:.1} ms{s}\n", .{ game, frames, settle_samples, allowed_ratio, extra_ms, if (debug_build) " (debug build)" else "" });
    var failures: usize = 0;
    for (languages) |language| {
        if (!try selected(allocator, language.code)) continue;
        // A mod that is not installed is not run, and the report says so.
        var missing: ?[]const u8 = null;
        for ([_]?[]const u8{ language.bitmap_mod, language.ttf_mod }) |mod| {
            const name = mod orelse continue;
            const pak = try std.fs.path.join(allocator, &.{ game_root, "mods", name, "data", "texts.pak" });
            cwd.access(io, pak, .{}) catch {
                missing = name;
                break;
            };
        }
        if (missing) |name| {
            try lines.print(allocator, "== {s}: not run (mod {s} is not installed in {s}/mods)\n", .{ language.code, name, game_root });
            continue;
        }
        cwd.deleteTree(io, try std.fs.path.join(allocator, &.{ scratch, language.code })) catch {};
        const bitmap_run = try Run.init(io, allocator, game, root, try std.fs.path.join(allocator, &.{ language.code, "bitmap" }), language.bitmap_mod, false);
        const ttf_run = try Run.init(io, allocator, game, root, try std.fs.path.join(allocator, &.{ language.code, "ttf" }), language.ttf_mod, true);
        try lines.print(allocator, "== {s}\n", .{language.code});
        for (screens) |screen| {
            const bitmap = try bitmap_run.hold(allocator, screen, frames);
            const ttf = try ttf_run.hold(allocator, screen, frames);
            const limit = bitmap.text_ms * allowed_ratio + extra_ms;
            const enough = bitmap.samples >= min_samples and ttf.samples >= min_samples;
            const ok = enough and ttf.text_ms <= limit;
            if (!ok) failures += 1;
            const line = try std.fmt.allocPrint(allocator,
                "{s} {s} {s}: text ms/frame bitmap {d:.3} ttf {d:.3} (limit {d:.3}), ms/frame bitmap {d:.2} ttf {d:.2}, text calls/frame bitmap {d:.1} ttf {d:.1}, samples {d}/{d}",
                .{ if (ok) "ok  " else "FAIL", language.code, screen.tag, bitmap.text_ms, ttf.text_ms, limit, bitmap.frame_ms, ttf.frame_ms, bitmap.calls, ttf.calls, bitmap.samples, ttf.samples });
            if (!ok) std.debug.print("{s}{s}\n", .{ line, if (enough) "" else " (too few samples)" });
            try lines.print(allocator, "{s}\n", .{line});
        }
    }
    // A shard (BK_TEXT_PERF_LANGS) keeps its own report so the shards do not overwrite each other.
    var report_name: []const u8 = "report.txt";
    if (try envValue(allocator, "BK_TEXT_PERF_LANGS")) |list| {
        const joined = try allocator.dupe(u8, list);
        std.mem.replaceScalar(u8, joined, ',', '-');
        report_name = try std.fmt.allocPrint(allocator, "report-{s}.txt", .{joined});
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, report_name }), .data = lines.items });
    if (failures != 0) {
        std.debug.print("text-engine-perf: {d} screens failed; the report is {s}/{s}\n", .{ failures, scratch, report_name });
        return error.TextSlowerThanBitmap;
    }
}
