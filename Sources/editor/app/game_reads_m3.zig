//! `MapEditor --game-reads-it-m3 <short-railroad map> [<log>]`: the M3 game-reads-it
//! scenario (05-05, D-37's game tier). The game's map loader builds the AI railroad
//! graph at load (CAILogic::Init -> CRailroadGraphConstructor::Construct), and a
//! railroad with fewer than two control points used to crash it there
//! (CSplineEdge reads controlpoints[0] and edgeParts[-1]; 04-REVIEW-FIX.md:60). The
//! editor will not draw such a record - and Check Map flags one - but a file can
//! hold it, so the loader itself skips it now (RailroadGraph.cpp). This scenario
//! proves it on the real Game:
//!
//!  1. the map the build crafted (coldwinter plus a one-control-point and a
//!     no-control-point railroad, `--craft short-railroad`) opens in the editor
//!     through the core Editor on the real bridge, and its test copy - which keeps
//!     both records as read - is played by the real Game under BK_AUTO_UI: it must
//!     load, run and exit 0 (a crash is a nonzero exit or a signal, and `runGame`
//!     fails on both), and the game's own BK_MAP_TRACE must report the map's roads;
//!  2. the regression guard: an ordinary shipped map still loads and exits 0.
//!
//! The log path names a combined report (the traces of both runs); each run's own
//! full log is `<log>.short-railroad.log` and `<log>.regular.log` beside it.
const std = @import("std");
const core = @import("editor_core");
const common = @import("game_reads_common.zig");
const testlaunch = @import("testlaunch.zig");

const label = "game reads it M3";

/// The schedule of each play: BK_AUTO_UI's shot then exit, well after the mission is
/// up (the M1 and M2 baselines wait for frame 400 too).
const auto_ui = "400:shot,420:exit";

/// The ordinary map of the regression guard.
const regular_map = "Data\\Maps\\Multiplayer\\coldwinter.bzm";

/// One play of a test copy: the game's own log and what it reported.
const Play = struct {
    log: []u8,
    trace: testlaunch.MapTraceSummary,
};

/// Plays the test copy (already saved) with BK_MAP_TRACE and parses the report;
/// null after printing why (the game did not start, crashed, or did not exit 0).
fn play(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, paths: *const common.TestPaths, what: []const u8, log_path: []const u8) ?Play {
    common.deleteAutoshots(io, paths.game_path);
    const log = common.runGame(gpa, io, environ, label, what, paths.game_path, log_path, &.{
        .{ "BK_AUTO_UI", auto_ui },
        .{ "BK_NO_HELP", "1" },
        .{ "BK_AUDIO_NULL", "1" },
        .{ "BK_MAP_TRACE", "1" },
    }) orelse return null;
    return .{ .log = log, .trace = testlaunch.parseMapTrace(log) };
}

fn appendTraceLines(gpa: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), heading: []const u8, log: []const u8) !void {
    try out.appendSlice(gpa, heading);
    try out.append(gpa, '\n');
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        if (!std.mem.startsWith(u8, line, "BK_MAP_TRACE: ")) continue;
        try out.appendSlice(gpa, line);
        try out.append(gpa, '\n');
    }
}

fn pathWithSuffix(gpa: std.mem.Allocator, log_path: []const u8, suffix: []const u8) ![]u8 {
    return std.fmt.allocPrint(gpa, "{s}{s}", .{ log_path, suffix });
}

/// The scenario. True when every check held; every failure prints `map-editor:
/// game reads it M3 FAIL: <reason>` first.
pub fn run(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, log_path: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
    var rig: common.Rig = .{};
    defer rig.deinit();
    if (!rig.open(gpa, io, label, map, mod_folder, mod_requested)) return false;
    const editor = &rig.editor;
    var paths: common.TestPaths = .{};
    if (!paths.resolve(&rig, io, label)) return false;

    // What the editor sees in the crafted map: two railroads the loader must survive.
    const roads_in_map = editor.vsoCount(.road) catch {
        std.debug.print("map-editor: {s} FAIL: the roads would not count: {s}\n", .{ label, editor.status() });
        return false;
    };
    var short: usize = 0;
    var index: usize = 0;
    while (index < roads_in_map) : (index += 1) {
        var view = editor.readVso(.road, index) catch continue;
        defer view.deinit(editor.allocator);
        if (view.control_points.len < 2) short += 1;
    }
    if (short < 2) {
        std.debug.print("map-editor: {s} FAIL: the crafted map holds {d} roads with fewer than two control points, not the two that were crafted\n", .{ label, short });
        return false;
    }
    std.debug.print("map-editor: {s}: the map holds {d} roads, {d} with fewer than two control points\n", .{ label, roads_in_map, short });

    // 1. The crafted map's test copy keeps both records as read; the game must load it.
    if (!common.saveTestCopy(&rig, label, "short-railroad test copy", paths.test_path)) return false;
    const short_log_path = try pathWithSuffix(gpa, log_path, ".short-railroad.log");
    defer gpa.free(short_log_path);
    const crafted = play(gpa, io, environ, &paths, "game on the short-railroad map", short_log_path) orelse return false;
    defer gpa.free(crafted.log);
    const crafted_roads = crafted.trace.roads orelse {
        std.debug.print("map-editor: {s} FAIL: the game's BK_MAP_TRACE did not report the map's roads (it did not load the map?); see {s}\n", .{ label, short_log_path });
        return false;
    };
    if (crafted_roads != roads_in_map) {
        std.debug.print("map-editor: {s} FAIL: the game loaded {d} roads, the editor holds {d}; see {s}\n", .{ label, crafted_roads, roads_in_map, short_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: short railroad loaded clean - {d} roads loaded, {d} of them with fewer than two control points, exit 0\n", .{ label, crafted_roads, short });

    // 2. The regression guard: a shipped map, unedited, still loads.
    editor.open(regular_map) catch {
        std.debug.print("map-editor: {s} FAIL: {s} did not open: {s}\n", .{ label, regular_map, editor.status() });
        return false;
    };
    if (!rig.settle(label, 2)) return false;
    const regular_roads = editor.vsoCount(.road) catch 0;
    if (!common.saveTestCopy(&rig, label, "regular test copy", paths.test_path)) return false;
    const regular_log_path = try pathWithSuffix(gpa, log_path, ".regular.log");
    defer gpa.free(regular_log_path);
    const regular = play(gpa, io, environ, &paths, "game on the regular map", regular_log_path) orelse return false;
    defer gpa.free(regular.log);
    const regular_traced = regular.trace.roads orelse {
        std.debug.print("map-editor: {s} FAIL: the game's BK_MAP_TRACE did not report the regular map's roads; see {s}\n", .{ label, regular_log_path });
        return false;
    };
    if (regular_traced != regular_roads) {
        std.debug.print("map-editor: {s} FAIL: the game loaded {d} roads of {s}, the editor holds {d}; see {s}\n", .{ label, regular_traced, regular_map, regular_roads, regular_log_path });
        return false;
    }

    var report: std.ArrayListUnmanaged(u8) = .empty;
    defer report.deinit(gpa);
    try appendTraceLines(gpa, &report, "=== short railroad ===", crafted.log);
    try appendTraceLines(gpa, &report, "=== regular map ===", regular.log);
    std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = report.items }) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the report {s} would not write: {s}\n", .{ label, log_path, @errorName(err) });
        return false;
    };
    common.deleteAutoshots(io, paths.game_path);
    std.debug.print("map-editor: {s} PASS (short railroad loaded clean: {d} roads incl. {d} short, exit 0; the regular map still loads: {d} roads, exit 0)\n", .{ label, crafted_roads, short, regular_traced });
    return true;
}
