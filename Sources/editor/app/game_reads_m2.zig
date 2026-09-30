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

/// The M2 test script (tools/zig/fixtures/m2_script.lua), embedded through the
/// build's anonymous import, and the bare name the map is given for it.
const script_source = @embedFile("m2_script_lua");
const script_name = "m2_script";
/// The script area the scenario draws; the Lua fixture finds it by this name.
const area_name = "m2_area";

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

/// Of the spread points `chooseAnchor` looks at, the one farthest from `from`.
fn farthestPoint(editor: *core.editor.Editor, from: [2]f32) [2]f32 {
    const width = @as(f32, @floatFromInt(editor.document.info.width_tiles)) * world_cell_size;
    const height = @as(f32, @floatFromInt(editor.document.info.height_tiles)) * world_cell_size;
    const fractions = [_][2]f32{ .{ 0.25, 0.25 }, .{ 0.75, 0.25 }, .{ 0.25, 0.75 }, .{ 0.75, 0.75 }, .{ 0.5, 0.5 } };
    var best: [2]f32 = .{ width / 2, height / 2 };
    var best_distance: f32 = -1;
    for (fractions) |fraction| {
        const x = width * fraction[0];
        const y = height * fraction[1];
        const distance = std.math.hypot(x - from[0], y - from[1]);
        if (distance > best_distance) {
            best_distance = distance;
            best = .{ x, y };
        }
    }
    return best;
}

/// A three-point line 500 world units long across (x, y), of the first type
/// of `kind`, width 3, full opacity, through the core Editor. False after
/// printing why.
fn addLine(gpa: std.mem.Allocator, editor: *core.editor.Editor, kind: core.bridge.VsoKind, x: f32, y: f32) bool {
    const types = editor.vsoDescriptors(kind, gpa) catch {
        std.debug.print("map-editor: {s} FAIL: the {s} types would not list: {s}\n", .{ label, kind.label(), editor.status() });
        return false;
    };
    defer gpa.free(types);
    if (types.len == 0) {
        std.debug.print("map-editor: {s} FAIL: the map's season has no {s} type\n", .{ label, kind.label() });
        return false;
    }
    const line = [_]core.records.Vec3{ .{ .x = x - 250, .y = y }, .{ .x = x, .y = y + 40 }, .{ .x = x + 250, .y = y } };
    _ = editor.addVso(kind, types[0].nameSlice(), &line, 3, 1) catch {
        std.debug.print("map-editor: {s} FAIL: the {s} at {d:.0},{d:.0} was not added: {s}\n", .{ label, kind.label(), x, y, editor.status() });
        return false;
    };
    std.debug.print("map-editor: {s}: added a {s} ({s}) across {d:.0},{d:.0}\n", .{ label, kind.label(), types[0].nameSlice(), x, y });
    return true;
}

/// A W_WoodenBig_Heavy_01 bridge 500 world units along x at (x, y), through
/// the core Editor; its bridges index, or null after printing why.
fn addBridge(editor: *core.editor.Editor, x: f32, y: f32) ?usize {
    return editor.drawBridge("W_WoodenBig_Heavy_01", x - 250, y, x + 250, y) catch {
        std.debug.print("map-editor: {s} FAIL: the bridge across {d:.0},{d:.0} was not drawn: {s}\n", .{ label, x, y, editor.status() });
        return null;
    };
}

/// 04-07 (D-14): a run of W_FactoryFence about 400 world units along x, in
/// view of the start camera (which stands on `anchor`), through the core
/// Editor. The engine refuses a fence on another object, so a few offsets
/// from the anchor are tried; how many fences the run made, or null after
/// printing why.
fn addFences(editor: *core.editor.Editor, anchor: [2]f32) ?usize {
    const offsets = [_][2]f32{ .{ 0, 130 }, .{ 0, -130 }, .{ 0, 220 }, .{ 0, -220 }, .{ 0, 310 }, .{ 0, -310 } };
    const before = editor.document.objects.items.len;
    for (offsets) |offset| {
        editor.drawFences("W_FactoryFence", anchor[0] - 200, anchor[1] + offset[1], anchor[0] + 200, anchor[1] + offset[1], false) catch continue;
        return editor.document.objects.items.len - before;
    }
    std.debug.print("map-editor: {s} FAIL: no fence run near the anchor {d:.0},{d:.0} was placed: {s}\n", .{ label, anchor[0], anchor[1], editor.status() });
    return null;
}

/// 04-08 (D-13): an L-shaped entrenchment for player 0 in view of the start
/// camera (which stands on `anchor`), through the core Editor: 300 world
/// units along x, then 200 along y, on the open snow in front of the fence
/// run. The trench builder refuses a piece off the map, so a few offsets
/// from the anchor are tried; the new entry's index, or null after printing
/// why.
fn addTrench(editor: *core.editor.Editor, anchor: [2]f32) ?usize {
    const offsets = [_][2]f32{ .{ -150, -150 }, .{ -150, 250 }, .{ -150, -450 }, .{ -150, 450 } };
    for (offsets) |offset| {
        const x = anchor[0] + offset[0];
        const y = anchor[1] + offset[1];
        const clicks = [_]core.records.Vec3{ .{ .x = x, .y = y }, .{ .x = x + 300, .y = y }, .{ .x = x + 300, .y = y + 200 } };
        return editor.drawEntrenchment(&clicks, 0) catch continue;
    }
    std.debug.print("map-editor: {s} FAIL: no entrenchment near the anchor {d:.0},{d:.0} was drawn: {s}\n", .{ label, anchor[0], anchor[1], editor.status() });
    return null;
}

/// The group and the script ID 04-09's held unit uses. 900 is the ID 04-10's
/// Lua fixture names, so the scenario fails if the first free ID at or above it
/// is not 900; 4245 is far above any script ID a shipped map carries.
const held_group: i32 = 900;
const held_script_id: i32 = 4245;

/// 04-09 (D-15, D-16): one unit of player 0 - the first placeable SGVOGT_UNIT
/// of the catalogue - beside the start camera (which stands on `anchor`), given
/// script ID 4245 and held back by a new group 900 that holds 4245. The engine
/// refuses a unit on another object, so a few offsets from the anchor are
/// tried. False after printing why.
fn addHeldUnit(gpa: std.mem.Allocator, rig: *common.Rig, anchor: [2]f32) bool {
    const editor = &rig.editor;
    const entries = rig.real.catalogue(gpa) catch {
        std.debug.print("map-editor: {s} FAIL: the object catalogue did not read\n", .{label});
        return false;
    };
    defer gpa.free(entries);
    const unit_name: []const u8 = for (entries) |entry| {
        if (entry.game_type == 1 and entry.placeable != 0) break std.mem.sliceTo(&entry.name, 0);
    } else {
        std.debug.print("map-editor: {s} FAIL: no placeable SGVOGT_UNIT in the catalogue\n", .{label});
        return false;
    };
    const offsets = [_][2]f32{ .{ 90, 0 }, .{ -90, 0 }, .{ 0, 90 }, .{ 0, -90 }, .{ 160, 60 }, .{ -160, -60 } };
    var link_id: i32 = -1;
    for (offsets) |offset| {
        var map_x: f32 = 0;
        var map_y: f32 = 0;
        if (editor.bridge.worldToMap(anchor[0] + offset[0], anchor[1] + offset[1], &map_x, &map_y) != .ok) continue;
        link_id = editor.addObject(unit_name, map_x, map_y, 0, 0) catch continue;
        break;
    }
    if (link_id < 0) {
        std.debug.print("map-editor: {s} FAIL: no unit {s} near the anchor {d:.0},{d:.0} was placed: {s}\n", .{ label, unit_name, anchor[0], anchor[1], editor.status() });
        return false;
    }
    editor.setScriptID(link_id, held_script_id, 0) catch {
        std.debug.print("map-editor: {s} FAIL: script ID {d} on object {d} was refused: {s}\n", .{ label, held_script_id, link_id, editor.status() });
        return false;
    };
    const group = editor.newGroup(held_group) catch {
        std.debug.print("map-editor: {s} FAIL: no group at or above {d}: {s}\n", .{ label, held_group, editor.status() });
        return false;
    };
    if (group != held_group) {
        std.debug.print("map-editor: {s} FAIL: the first free group ID at or above {d} is {d}; 04-10's Lua fixture names group {d}\n", .{ label, held_group, group, held_group });
        return false;
    }
    editor.addScriptIDToGroup(group, held_script_id) catch {
        std.debug.print("map-editor: {s} FAIL: script ID {d} into group {d}: {s}\n", .{ label, held_script_id, group, editor.status() });
        return false;
    };
    std.debug.print("map-editor: {s}: placed {s} (link {d}, script ID {d}) beside the start camera and put script ID {d} in group {d}\n", .{ label, unit_name, link_id, held_script_id, held_script_id, group });
    return true;
}

/// A placeable SGVOGT_UNIT of player 0 of the catalogue's first such type, put on
/// the first free spot of `offsets` from `anchor` (world units), through the core
/// Editor: the engine refuses a unit on another object. Its link ID, or null after
/// printing why; `what` says what it is for.
fn placeUnit(gpa: std.mem.Allocator, rig: *common.Rig, anchor: [2]f32, offsets: []const [2]f32, what: []const u8) ?i32 {
    const editor = &rig.editor;
    const entries = rig.real.catalogue(gpa) catch {
        std.debug.print("map-editor: {s} FAIL: the object catalogue did not read\n", .{label});
        return null;
    };
    defer gpa.free(entries);
    const unit_name: []const u8 = for (entries) |entry| {
        if (entry.game_type == 1 and entry.placeable != 0) break std.mem.sliceTo(&entry.name, 0);
    } else {
        std.debug.print("map-editor: {s} FAIL: no placeable SGVOGT_UNIT in the catalogue\n", .{label});
        return null;
    };
    for (offsets) |offset| {
        var map_x: f32 = 0;
        var map_y: f32 = 0;
        if (editor.bridge.worldToMap(anchor[0] + offset[0], anchor[1] + offset[1], &map_x, &map_y) != .ok) continue;
        const link_id = editor.addObject(unit_name, map_x, map_y, 0, 0) catch continue;
        std.debug.print("map-editor: {s}: placed {s} (link {d}) for {s} beside the start camera\n", .{ label, unit_name, link_id, what });
        return link_id;
    }
    std.debug.print("map-editor: {s} FAIL: no unit {s} near the anchor {d:.0},{d:.0} was placed for {s}: {s}\n", .{ label, unit_name, anchor[0], anchor[1], what, editor.status() });
    return null;
}

/// 04-11 (D-17): a second unit of player 0, no script ID, given a STOP command the
/// way Unit -> Add start command gives one. The game's InitStartCommands launches
/// every command that names a unit and reports the count, so `startcmd launched`
/// one above the baseline is the proof. False after printing why.
fn addStartCommandUnit(gpa: std.mem.Allocator, rig: *common.Rig, anchor: [2]f32) bool {
    const editor = &rig.editor;
    const offsets = [_][2]f32{ .{ 0, 170 }, .{ 0, -170 }, .{ -170, 0 }, .{ 170, 60 }, .{ -170, -60 }, .{ 0, 260 }, .{ 0, -260 } };
    const link_id = placeUnit(gpa, rig, anchor, &offsets, "a start command") orelse return false;
    const index = editor.addStartCommand(link_id) catch {
        std.debug.print("map-editor: {s} FAIL: the start command for unit {d} was not added: {s}\n", .{ label, link_id, editor.status() });
        return false;
    };
    std.debug.print("map-editor: {s}: start command {d} (STOP) for unit {d}\n", .{ label, index, link_id });
    return true;
}

/// 04-10 (D-20): the map names `m2_script` as its script, and the game finds the
/// file where it looks - beside the test copy. The fixture is written beside a
/// stand-in for the edited map (a folder of our own under zig-out/local-test:
/// the shipped map's folder is never written) and `script_file.copyForTest`, the
/// very call Test in game makes, copies it beside the test map. False after
/// printing why.
fn addScript(io: std.Io, editor: *core.editor.Editor, paths: *const common.TestPaths, log_path: []const u8) bool {
    editor.setScriptFile(script_name) catch {
        std.debug.print("map-editor: {s} FAIL: naming the script {s} was refused: {s}\n", .{ label, script_name, editor.status() });
        return false;
    };
    const parent = std.fs.path.dirname(log_path) orelse ".";
    var dir_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const scratch = std.fmt.bufPrint(&dir_buffer, "{s}{c}game-reads-it-m2-script", .{ parent, std.fs.path.sep }) catch {
        std.debug.print("map-editor: {s} FAIL: the script folder's path is too long\n", .{label});
        return false;
    };
    std.Io.Dir.cwd().createDirPath(io, scratch) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the folder {s} would not be made: {s}\n", .{ label, scratch, @errorName(err) });
        return false;
    };
    var source_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const source = std.fmt.bufPrint(&source_buffer, "{s}{c}{s}.lua", .{ scratch, std.fs.path.sep, script_name }) catch return false;
    std.Io.Dir.cwd().writeFile(io, .{ .sub_path = source, .data = script_source }) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the script {s} would not write: {s}\n", .{ label, source, @errorName(err) });
        return false;
    };
    var stand_in_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const stand_in = std.fmt.bufPrint(&stand_in_buffer, "{s}{c}m2_edited.bzm", .{ scratch, std.fs.path.sep }) catch return false;
    const files = editor.files orelse return false;
    const outcome = core.script_file.copyForTest(files, stand_in, core.script_file.directoryOf(paths.test_path), script_name);
    if (outcome != .copied) {
        std.debug.print("map-editor: {s} FAIL: the script was not copied beside the test map {s}: {s}\n", .{ label, paths.test_path, @tagName(outcome) });
        return false;
    }
    std.debug.print("map-editor: {s}: the map names script {s}; {s} copied beside {s}\n", .{ label, script_name, source, paths.test_path });
    return true;
}

/// 04-10 (D-21): a rectangle area named `m2_area` around the start camera (which
/// stands on `anchor`), made the way the Script Areas tool makes one: a drag in
/// world units through the bridge's conversion, then added. Its AI values, which
/// the game must report back, are what `addScriptArea` stored. Null after
/// printing why.
fn addArea(editor: *core.editor.Editor, anchor: [2]f32) ?core.records.ScriptArea {
    const area = editor.scriptAreaFromVis(.rectangle, anchor[0] - 120, anchor[1] - 80, anchor[0] + 120, anchor[1] + 80, area_name) catch {
        std.debug.print("map-editor: {s} FAIL: the area drag would not convert: {s}\n", .{ label, editor.status() });
        return null;
    };
    const index = editor.addScriptArea(area) catch {
        std.debug.print("map-editor: {s} FAIL: the area {s} was not added: {s}\n", .{ label, area_name, editor.status() });
        return null;
    };
    std.debug.print("map-editor: {s}: area {d} {s}, a rectangle at {d:.0},{d:.0} (map units), half size {d:.0} x {d:.0}\n", .{ label, index, area_name, area.cx, area.cy, area.hx, area.hy });
    return area;
}

/// The edited run's own screenshot dump (BK_AUTO_UI's shot, written in the
/// game's directory and swept afterwards), copied beside the report as
/// `<log>.edited.rgba`: 04-13 looks at it. Best effort: no shot is not a
/// failure of the scenario.
fn keepEditedShot(gpa: std.mem.Allocator, io: std.Io, game_path: []const u8, log_path: []const u8) void {
    const game_dir = std.fs.path.dirname(game_path) orelse return;
    var dir = std.Io.Dir.cwd().openDir(io, game_dir, .{ .iterate = true }) catch return;
    defer dir.close(io);
    var it = dir.iterate();
    while (it.next(io) catch null) |entry| {
        if (entry.kind != .file) continue;
        if (!std.mem.startsWith(u8, entry.name, "autoshot_") or !std.mem.endsWith(u8, entry.name, ".rgba")) continue;
        const bytes = dir.readFileAlloc(io, entry.name, gpa, .limited(64 << 20)) catch return;
        defer gpa.free(bytes);
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const kept = std.fmt.bufPrint(&path_buffer, "{s}.edited.rgba", .{log_path}) catch return;
        std.Io.Dir.cwd().writeFile(io, .{ .sub_path = kept, .data = bytes }) catch return;
        std.debug.print("map-editor: {s}: kept the edited game's shot {s} as {s}\n", .{ label, entry.name, kept });
        return;
    }
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
    // The counts later plans compare against ("baseline + my edit"); null is
    // "the game did not report it", which is not a zero.
    const base = baseline.trace;
    std.debug.print("map-editor: {s}: baseline roads={?d} rivers={?d} bridges={?d} entrenchments={?d} startcmd={?d} reserve={?d}\n", .{ label, base.roads, base.rivers, base.bridges, base.entrenchments, base.startcmd_launched, base.reserve_applied });
    std.debug.print("map-editor: {s}: baseline areas={d} groups={d} generals={d} parcels={d} lua={d} script={s}\n", .{
        label,
        base.areas.seen,
        base.groups.seen,
        base.generals.seen,
        base.parcels.seen,
        base.lua.seen,
        if (base.script) |*script| script.name.slice() else "not reported",
    });

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

    // 04-05 (D-07, D-09): a new road and a new river, drawn through the core
    // Editor with the first type of each kind, on the spread point farthest
    // from the anchor (the camera starts there, so neither is under it).
    const lines_at = farthestPoint(editor, anchor_at);
    if (!addLine(gpa, editor, .road, lines_at[0], lines_at[1] - 150)) return false;
    if (!addLine(gpa, editor, .river, lines_at[0], lines_at[1] + 150)) return false;

    // 04-06 (D-10..D-12): two W_WoodenBig_Heavy bridges on either side of the
    // road and the river, the first rotated to its _02 partner, the second
    // built during play (every span saved with HP -1).
    const rotated = addBridge(editor, lines_at[0], lines_at[1] - 450) orelse return false;
    editor.rotateBridge(rotated) catch {
        std.debug.print("map-editor: {s} FAIL: bridge {d} would not rotate: {s}\n", .{ label, rotated, editor.status() });
        return false;
    };
    const built = addBridge(editor, lines_at[0], lines_at[1] + 450) orelse return false;
    editor.toggleBridgeBuild(built) catch {
        std.debug.print("map-editor: {s} FAIL: bridge {d} would not toggle built during play: {s}\n", .{ label, built, editor.status() });
        return false;
    };
    std.debug.print("map-editor: {s}: drew bridge {d} (rotated to W_WoodenBig_Heavy_02) and bridge {d} (built during play)\n", .{ label, rotated, built });

    // 04-07 (D-14): a fence run in view of the start camera. The game reads
    // fences as ordinary objects, so the proof is that it loads the map and
    // exits cleanly; the edited shot is kept for 04-13 to look at.
    const fences = addFences(editor, anchor_at) orelse return false;
    std.debug.print("map-editor: {s}: placed a run of {d} W_FactoryFence fences beside the start camera\n", .{ label, fences });

    // 04-08 (D-13): an entrenchment for player 0 beside the start camera. The
    // game's LoadEntrenchments dereferences every link of every section, so a
    // clean run reporting one more entrenchment is the proof.
    const trench = addTrench(editor, anchor_at) orelse return false;
    const trench_pieces = pieces: {
        const infos = editor.entrenchments(gpa) catch break :pieces @as(i32, 0);
        defer gpa.free(infos);
        break :pieces if (trench < infos.len) infos[trench].piece_count else 0;
    };
    std.debug.print("map-editor: {s}: drew entrenchment {d} ({d} pieces) for player 0 beside the start camera\n", .{ label, trench, trench_pieces });

    // 04-09 (D-15, D-16): a unit the new group 900 holds back. The game's
    // LoadUnits holds back every map object whose script ID a group names and
    // reports how many it held per group, so `group id=900 held=1` is the proof.
    if (!addHeldUnit(gpa, &rig, anchor_at)) return false;

    // 04-11 (D-17): a unit given a STOP command. The game's InitStartCommands
    // launches every command that names a unit and reports how many.
    if (!addStartCommandUnit(gpa, &rig, anchor_at)) return false;

    // 04-10 (D-20): the map's script, which lands group 900 and traces what the
    // game holds.
    if (!addScript(io, editor, &paths, log_path)) return false;

    // 04-10 (D-21): a script area the script finds by name.
    const area = addArea(editor, anchor_at) orelse return false;

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
    // The game read one more road and one more river than the map shipped with.
    const edited_trace = edited.trace;
    if (base.roads == null or base.rivers == null or edited_trace.roads == null or edited_trace.rivers == null) {
        std.debug.print("map-editor: {s} FAIL: a run printed no BK_MAP_TRACE terrain line (roads {?d} -> {?d}, rivers {?d} -> {?d}); see {s}\n", .{ label, base.roads, edited_trace.roads, base.rivers, edited_trace.rivers, edited_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: roads={d} rivers={d} (baseline roads={d} rivers={d})\n", .{ label, edited_trace.roads.?, edited_trace.rivers.?, base.roads.?, base.rivers.? });
    if (edited_trace.roads.? != base.roads.? + 1 or edited_trace.rivers.? != base.rivers.? + 1) {
        std.debug.print("map-editor: {s} FAIL: the game read {d} roads and {d} rivers, not one more of each than the baseline's {d} and {d}; see {s}\n", .{ label, edited_trace.roads.?, edited_trace.rivers.?, base.roads.?, base.rivers.?, edited_log_path });
        return false;
    }

    // The game loaded both bridges (LoadBridges asserts every link, so a
    // clean run with two more is the proof the entries name real spans).
    if (base.bridges == null or edited_trace.bridges == null) {
        std.debug.print("map-editor: {s} FAIL: a run printed no BK_MAP_TRACE bridges line ({?d} -> {?d}); see {s}\n", .{ label, base.bridges, edited_trace.bridges, edited_log_path });
        return false;
    }
    if (edited_trace.bridges.? != base.bridges.? + 2) {
        std.debug.print("map-editor: {s} FAIL: the game read {d} bridges, not the baseline's {d} and two more; see {s}\n", .{ label, edited_trace.bridges.?, base.bridges.?, edited_log_path });
        return false;
    }

    // The game grouped the new entrenchment's pieces (LoadEntrenchments).
    if (base.entrenchments == null or edited_trace.entrenchments == null) {
        std.debug.print("map-editor: {s} FAIL: a run printed no BK_MAP_TRACE entrenchments line ({?d} -> {?d}); see {s}\n", .{ label, base.entrenchments, edited_trace.entrenchments, edited_log_path });
        return false;
    }
    if (edited_trace.entrenchments.? != base.entrenchments.? + 1) {
        std.debug.print("map-editor: {s} FAIL: the game read {d} entrenchments, not the baseline's {d} and one more; see {s}\n", .{ label, edited_trace.entrenchments.?, base.entrenchments.?, edited_log_path });
        return false;
    }

    // The game held the unit back for group 900 (LoadUnits' GetGroupById).
    var held_by_group: ?u32 = null;
    for (edited_trace.groups.slice()) |group| {
        if (group.id == held_group) held_by_group = group.held;
    }
    if (held_by_group == null or held_by_group.? != 1) {
        std.debug.print("map-editor: {s} FAIL: the game held {?d} objects back for group {d}, not 1 (groups seen {d}); see {s}\n", .{ label, held_by_group, held_group, edited_trace.groups.seen, edited_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: BK_MAP_TRACE group id={d} held={d}\n", .{ label, held_group, held_by_group.? });

    // The game launched the new start command (InitStartCommands).
    if (base.startcmd_launched == null or edited_trace.startcmd_launched == null) {
        std.debug.print("map-editor: {s} FAIL: a run printed no BK_MAP_TRACE startcmd line ({?d} -> {?d}); see {s}\n", .{ label, base.startcmd_launched, edited_trace.startcmd_launched, edited_log_path });
        return false;
    }
    if (edited_trace.startcmd_launched.? != base.startcmd_launched.? + 1) {
        std.debug.print("map-editor: {s} FAIL: the game launched {d} start commands, not the baseline's {d} and one more; see {s}\n", .{ label, edited_trace.startcmd_launched.?, base.startcmd_launched.?, edited_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: BK_MAP_TRACE startcmd launched={d} (baseline {d})\n", .{ label, edited_trace.startcmd_launched.?, base.startcmd_launched.? });

    // The game loaded and ran the script the map names (Scripts.cpp Load).
    const script = edited_trace.script orelse {
        std.debug.print("map-editor: {s} FAIL: the game printed no BK_MAP_TRACE script line; see {s}\n", .{ label, edited_log_path });
        return false;
    };
    if (!std.mem.eql(u8, script.name.slice(), script_name) or !script.loaded or !script.init) {
        std.debug.print("map-editor: {s} FAIL: the game's script is \"{s}\" loaded={} init={}, not {s} loaded and run; see {s}\n", .{ label, script.name.slice(), script.loaded, script.init, script_name, edited_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: BK_MAP_TRACE script name={s} loaded=1 init=1\n", .{ label, script.name.slice() });
    // The game read the area by its name (Scripts.cpp InitAreas), raw as stored.
    var reported_area: ?testlaunch.TraceArea = null;
    for (edited_trace.areas.slice()) |item| {
        if (std.mem.eql(u8, item.name.slice(), area_name)) reported_area = item;
    }
    const seen_area = reported_area orelse {
        std.debug.print("map-editor: {s} FAIL: the game reported no script area named {s} (areas seen {d}); see {s}\n", .{ label, area_name, edited_trace.areas.seen, edited_log_path });
        return false;
    };
    if (@abs(seen_area.cx - area.cx) > 1 or @abs(seen_area.cy - area.cy) > 1) {
        std.debug.print("map-editor: {s} FAIL: the game's area {s} is at {d:.0},{d:.0}, not the stored {d:.0},{d:.0}; see {s}\n", .{ label, area_name, seen_area.cx, seen_area.cy, area.cx, area.cy, edited_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: BK_MAP_TRACE area name={s} cx={d:.0} cy={d:.0} (stored {d:.0},{d:.0})\n", .{ label, area_name, seen_area.cx, seen_area.cy, area.cx, area.cy });

    // The script's traces: Init's 1 (it ran), then the two numbers GetScriptAreaParams
    // gave for the area - the stored centre - then Report's counts of script group
    // 4245, which LandReinforcement queued: 0 until the game lands the unit, then 1.
    const lua = edited_trace.lua.slice();
    if (lua.len < 4 or !std.mem.eql(u8, lua[0].slice(), "1")) {
        std.debug.print("map-editor: {s} FAIL: the script's first trace is not \"1\", or it traced fewer than four lines (lua lines seen {d}); see {s}\n", .{ label, edited_trace.lua.seen, edited_log_path });
        return false;
    }
    const lua_x = std.fmt.parseFloat(f32, lua[1].slice()) catch std.math.nan(f32);
    const lua_y = std.fmt.parseFloat(f32, lua[2].slice()) catch std.math.nan(f32);
    if (!(@abs(lua_x - area.cx) <= 1) or !(@abs(lua_y - area.cy) <= 1)) {
        std.debug.print("map-editor: {s} FAIL: GetScriptAreaParams(\"{s}\") gave {s},{s} to the script, not the stored centre {d:.0},{d:.0}; see {s}\n", .{ label, area_name, lua[1].slice(), lua[2].slice(), area.cx, area.cy, edited_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: BK_MAP_TRACE lua: the script found {s} at {s},{s}\n", .{ label, area_name, lua[1].slice(), lua[2].slice() });
    var landed_at: ?usize = null;
    for (lua[3..], 1..) |line, index| {
        const count = std.fmt.parseInt(i32, line.slice(), 10) catch -1;
        if (count > 1) {
            std.debug.print("map-editor: {s} FAIL: the script counted {s} units in script group {d}, not at most 1; see {s}\n", .{ label, line.slice(), held_script_id, edited_log_path });
            return false;
        }
        if (count == 1 and landed_at == null) landed_at = index;
    }
    const landed = landed_at orelse {
        std.debug.print("map-editor: {s} FAIL: script group {d} never held the landed unit in {d} counts (lua lines seen {d}); see {s}\n", .{ label, held_script_id, lua.len - 3, edited_trace.lua.seen, edited_log_path });
        return false;
    };
    std.debug.print("map-editor: {s}: BK_MAP_TRACE lua: script group {d} held one unit from count {d} of {d} after LandReinforcement({d})\n", .{ label, held_script_id, landed, lua.len - 3, held_group });

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
    keepEditedShot(gpa, io, paths.game_path, log_path);
    common.deleteAutoshots(io, paths.game_path);
    std.debug.print("map-editor: {s} PASS (camera at player 0's anchor {d:.0},{d:.0}; baseline {d:.0},{d:.0}; roads {d} -> {d}, rivers {d} -> {d}, bridges {d} -> {d}, fences +{d}, entrenchments {d} -> {d}, group {d} held {d}, startcmd launched {d} -> {d}, script {s} ran, area {s} found)\n", .{ label, camera.x, camera.y, baseline_camera.x, baseline_camera.y, base.roads.?, edited_trace.roads.?, base.rivers.?, edited_trace.rivers.?, base.bridges.?, edited_trace.bridges.?, fences, base.entrenchments.?, edited_trace.entrenchments.?, held_group, held_by_group.?, base.startcmd_launched.?, edited_trace.startcmd_launched.?, script_name, area_name });
    return true;
}
