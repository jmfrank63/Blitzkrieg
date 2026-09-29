//! `MapEditor --game-reads-it-m2 <map> [<log>]`: the M2 game-reads-it
//! scenario (04-04). The editor edits a shipped map through the core Editor
//! on the real bridge, saves the test copy, the real Game plays it with
//! BK_MAP_TRACE set, and the scenario asserts on the game's own report of
//! what it consumed (testlaunch.parseMapTrace) - not on anything the editor
//! believes it wrote.
//!
//! Every plan of the phase that adds an M2 feature (roads and rivers,
//! bridges, areas, groups, start commands, reserve positions, generals, the
//! script) adds its edit and its assertion here, after the baseline and the
//! camera anchor: the shape is always "baseline run, edit, save the test
//! copy, play again, compare the two reports". `Play` is that second half.
//!
//! The first edit is D-22 ("Test in game therefore starts at player 0's
//! anchor"): the game starts its camera at playersCameraAnchors[user], so
//! an anchor set by the editor on a flat point far from where the map's own
//! anchor put the camera must come back in the camera line, source=player.
//!
//! The log path names a combined report - the baseline's trace lines, then
//! the edited run's - so one file shows both; each run's own full log is
//! `<log>.baseline.log` and `<log>.edited.log` beside it.
const std = @import("std");
const core = @import("editor_core");
const common = @import("game_reads_common.zig");
const testlaunch = @import("testlaunch.zig");

const label = "game reads it M2";

/// One world tile in world (Vis) units: fWorldCellSize, 32 * sqrt(2)
/// (Formats/fmtTerrain.h). The anchor must be at least this many tiles from
/// where the baseline camera stood, so an anchor that was never read cannot
/// pass for one that was.
const world_cell_size: f32 = 45.254833;
const min_anchor_distance_tiles: f32 = 20;

/// Where the game's camera must be, and how far off it may be: one world
/// unit, the trace prints the position to the unit.
const camera_tolerance: f32 = 1;

/// The schedule of each play: BK_AUTO_UI's shot then exit, well after the
/// mission is up (the M1 baseline waits for frame 400 too).
const auto_ui = "400:shot,420:exit";

/// One play of the test copy: the game's own log and what it reported.
pub const Play = struct {
    log: []u8,
    trace: testlaunch.MapTraceSummary,

    pub fn deinit(self: *Play, gpa: std.mem.Allocator) void {
        gpa.free(self.log);
    }
};

/// Plays the test copy (already saved) with BK_MAP_TRACE and parses the
/// report. `what` names the run in a failure line; the game's whole log
/// goes to `log_path`. Null after printing why.
fn play(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, paths: *const common.TestPaths, what: []const u8, log_path: []const u8) ?Play {
    const log = common.runGame(gpa, io, environ, label, what, paths.game_path, log_path, &.{
        .{ "BK_AUTO_UI", auto_ui },
        .{ "BK_NO_HELP", "1" },
        .{ "BK_AUDIO_NULL", "1" },
        .{ "BK_MAP_TRACE", "1" },
    }) orelse return null;
    return .{ .log = log, .trace = testlaunch.parseMapTrace(log) };
}

/// The trace lines of a log, for the combined report.
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

/// How level the ground is around a point: the largest height difference to
/// the four neighbours one tile away. Null when any of them is off the map.
fn roughness(editor: *core.editor.Editor, x: f32, y: f32) ?f32 {
    var centre: f32 = 0;
    if (editor.bridge.groundHeight(x, y, &centre) != .ok) return null;
    var worst: f32 = 0;
    const around = [_][2]f32{ .{ world_cell_size, 0 }, .{ -world_cell_size, 0 }, .{ 0, world_cell_size }, .{ 0, -world_cell_size } };
    for (around) |offset| {
        var z: f32 = 0;
        if (editor.bridge.groundHeight(x + offset[0], y + offset[1], &z) != .ok) return null;
        worst = @max(worst, @abs(z - centre));
    }
    return worst;
}

/// The point for player 0's anchor: of a few spread over the map, the level
/// one that is at least `min_anchor_distance_tiles` from `from`.
fn chooseAnchor(editor: *core.editor.Editor, from: [2]f32) ?[2]f32 {
    const width = @as(f32, @floatFromInt(editor.document.info.width_tiles)) * world_cell_size;
    const height = @as(f32, @floatFromInt(editor.document.info.height_tiles)) * world_cell_size;
    const fractions = [_][2]f32{ .{ 0.25, 0.25 }, .{ 0.75, 0.25 }, .{ 0.25, 0.75 }, .{ 0.75, 0.75 }, .{ 0.5, 0.5 } };
    var best: ?[2]f32 = null;
    var best_roughness: f32 = std.math.inf(f32);
    for (fractions) |fraction| {
        const x = width * fraction[0];
        const y = height * fraction[1];
        if (std.math.hypot(x - from[0], y - from[1]) < min_anchor_distance_tiles * world_cell_size) continue;
        const rough = roughness(editor, x, y) orelse continue;
        if (rough < best_roughness) {
            best_roughness = rough;
            best = .{ x, y };
        }
    }
    return best;
}

pub fn run(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, map: []const u8, log_path: []const u8, mod_folder: ?[]const u8, mod_requested: bool) !bool {
    var rig: common.Rig = .{};
    defer rig.deinit();
    if (!rig.open(gpa, io, label, map, mod_folder, mod_requested)) return false;
    const editor = &rig.editor;
    // D-01: a test copy goes through saveCopy, never editor.save: the
    // document must still be this map, edited and unsaved, at the end.
    const original_path = try gpa.dupe(u8, editor.document.path.items);
    defer gpa.free(original_path);

    var paths: common.TestPaths = .{};
    if (!paths.resolve(&rig, io, label)) return false;

    var baseline_log_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const baseline_log_path = std.fmt.bufPrint(&baseline_log_buffer, "{s}.baseline.log", .{log_path}) catch {
        std.debug.print("map-editor: {s} FAIL: the path {s} is too long\n", .{ label, log_path });
        return false;
    };
    var edited_log_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const edited_log_path = std.fmt.bufPrint(&edited_log_buffer, "{s}.edited.log", .{log_path}) catch {
        std.debug.print("map-editor: {s} FAIL: the path {s} is too long\n", .{ label, log_path });
        return false;
    };

    // The baseline: the map as shipped, unedited.
    if (!common.saveTestCopy(&rig, label, "unedited test copy", paths.test_path)) return false;
    var baseline = play(gpa, io, environ, &paths, "baseline game", baseline_log_path) orelse return false;
    defer baseline.deinit(gpa);
    const baseline_camera = baseline.trace.camera orelse {
        std.debug.print("map-editor: {s} FAIL: the baseline game printed no BK_MAP_TRACE camera line (is the Game current?); see {s}\n", .{ label, baseline_log_path });
        return false;
    };
    std.debug.print("map-editor: {s}: baseline camera {d:.0},{d:.0},{d:.0} source={s}\n", .{ label, baseline_camera.x, baseline_camera.y, baseline_camera.z, @tagName(baseline_camera.source) });
    std.debug.print("map-editor: {s}: baseline roads={?d} rivers={?d}\n", .{ label, baseline.trace.roads, baseline.trace.rivers });

    // D-22: player 0's anchor, far from where that camera stood.
    const anchor_at = chooseAnchor(editor, .{ baseline_camera.x, baseline_camera.y }) orelse {
        std.debug.print("map-editor: {s} FAIL: no point at least {d:.0} tiles from the baseline camera to put the anchor on\n", .{ label, min_anchor_distance_tiles });
        return false;
    };
    editor.setCameraAnchor(0, anchor_at[0], anchor_at[1]) catch {
        std.debug.print("map-editor: {s} FAIL: setting player 0's anchor at {d:.0},{d:.0} failed: {s}\n", .{ label, anchor_at[0], anchor_at[1], editor.status() });
        return false;
    };
    var anchor_z: f32 = 0;
    _ = editor.bridge.groundHeight(anchor_at[0], anchor_at[1], &anchor_z);

    if (!common.saveTestCopy(&rig, label, "edited test copy", paths.test_path)) return false;
    var edited = play(gpa, io, environ, &paths, "edited game", edited_log_path) orelse return false;
    defer edited.deinit(gpa);
    const camera = edited.trace.camera orelse {
        std.debug.print("map-editor: {s} FAIL: the edited game printed no BK_MAP_TRACE camera line; see {s}\n", .{ label, edited_log_path });
        return false;
    };
    if (camera.source != .player) {
        std.debug.print("map-editor: {s} FAIL: the game started its camera from source={s}, not from player 0's anchor (at {d:.0},{d:.0}); see {s}\n", .{ label, @tagName(camera.source), anchor_at[0], anchor_at[1], edited_log_path });
        return false;
    }
    if (@abs(camera.x - anchor_at[0]) > camera_tolerance or @abs(camera.y - anchor_at[1]) > camera_tolerance) {
        std.debug.print("map-editor: {s} FAIL: the game's camera is at {d:.0},{d:.0}, not at player 0's anchor {d:.0},{d:.0}; see {s}\n", .{ label, camera.x, camera.y, anchor_at[0], anchor_at[1], edited_log_path });
        return false;
    }
    // Assumption A2, measured and printed, not asserted: the anchor's z as
    // the editor took it from the terrain against the z the game reports.
    std.debug.print("map-editor: {s}: anchor z {d:.1}, game camera z {d:.1}\n", .{ label, anchor_z, camera.z });

    if (!editor.dirty() or !std.mem.eql(u8, editor.document.path.items, original_path)) {
        std.debug.print("map-editor: {s} FAIL: the document changed - dirty {}, path {s} (was {s})\n", .{ label, editor.dirty(), editor.document.path.items, original_path });
        return false;
    }

    // The combined report, then the sweep of the game's screenshot dumps.
    var report: std.ArrayListUnmanaged(u8) = .empty;
    defer report.deinit(gpa);
    try appendTraceLines(gpa, &report, "=== baseline ===", baseline.log);
    try appendTraceLines(gpa, &report, "=== edited ===", edited.log);
    std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = report.items }) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the report {s} would not write: {s}\n", .{ label, log_path, @errorName(err) });
        return false;
    };
    common.deleteAutoshots(io, paths.game_path);
    std.debug.print("map-editor: {s} PASS (camera at player 0's anchor {d:.0},{d:.0}; baseline {d:.0},{d:.0})\n", .{ label, camera.x, camera.y, baseline_camera.x, baseline_camera.y });
    return true;
}
