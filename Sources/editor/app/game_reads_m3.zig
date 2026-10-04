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
//!  2. the regression guard: an ordinary shipped map still loads and exits 0 - and
//!     again with BK_PROGRESS_XML naming a file that does not exist, the stand-in
//!     for an installation without Data/movies/progress/progress.xml: the mission
//!     loads without a loading screen, the game says so in its log, and exits 0;
//!  3. the authored leg (05-10, D-40.5): a template, graph, container and field set
//!     authored through the core Editor's composer I/O (under the user RMG root,
//!     never a shipped file) generate a map from a fixed seed, the editor opens it,
//!     and the real Game loads its test copy and exits 0 with the roads the editor
//!     counted. The user root is the build step's scratch XDG_DATA_HOME where the
//!     platform honours it; on Windows it is the profile's, and the authored files
//!     (names under `user\authored_*`) are rewritten identically on every run.
//!  4. the script path ruling of 2026-10-03 (WINDOWS.md 5): that generated map names
//!     its script by the bare name (relative to the map's own folder), so the map
//!     and its .lua are MOVED to another folder (the stand-in for another computer;
//!     the originals are deleted), opened there, and Test in game - the very
//!     `copyScriptForTest` call the menu makes - copies the script beside the test
//!     map and the real Game loads and runs it (`BK_MAP_TRACE: script ... loaded=1
//!     init=1`). The same play runs once before the move, from the folder it was
//!     generated into.
//!
//! The log path names a combined report (the traces of both runs); each run's own
//! full log is `<log>.short-railroad.log` and `<log>.regular.log` beside it.
const std = @import("std");
const core = @import("editor_core");
const common = @import("game_reads_common.zig");
const testlaunch = @import("testlaunch.zig");
const panels_logic = @import("panels_logic.zig");

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
    return playWithEnv(gpa, io, environ, paths, what, log_path, &.{});
}

/// `play` with `extra_env` added to the game's environment.
fn playWithEnv(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, paths: *const common.TestPaths, what: []const u8, log_path: []const u8, extra_env: []const [2][]const u8) ?Play {
    common.deleteAutoshots(io, paths.game_path);
    const base_env = [_][2][]const u8{
        .{ "BK_AUTO_UI", auto_ui },
        .{ "BK_NO_HELP", "1" },
        .{ "BK_AUDIO_NULL", "1" },
        .{ "BK_MAP_TRACE", "1" },
    };
    var env: [base_env.len + 2][2][]const u8 = undefined;
    std.debug.assert(extra_env.len <= env.len - base_env.len);
    @memcpy(env[0..base_env.len], &base_env);
    @memcpy(env[base_env.len..][0..extra_env.len], extra_env);
    const log = common.runGame(gpa, io, environ, label, what, paths.game_path, log_path, env[0 .. base_env.len + extra_env.len]) orelse return null;
    return .{ .log = log, .trace = testlaunch.parseMapTrace(log) };
}

/// Where the no-progress-list leg points the game's progress screen
/// (BK_PROGRESS_XML): a name no installation holds, so the open fails exactly as
/// it does on an install without Data/movies/progress/progress.xml.
const missing_progress_xml = "movies\\progress\\no-such-progress-list.xml";

/// The line the game logs when the progress screen has no movie list.
const progress_warning = "progress screen: no loading movie";

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

const authored_template = "scenarios\\templates\\user\\authored_t";
const authored_graph = "scenarios\\graphs\\user\\authored_g";
const authored_field = "scenarios\\fieldsets\\user\\authored_f";

/// The names one RMG folder lists, owned by the caller (null after printing why).
fn listNames(gpa: std.mem.Allocator, editor: *core.editor.Editor, kind: core.bridge.RmgKind) ?[]core.bridge.RmgName {
    var total: usize = 0;
    _ = editor.bridge.listRmg(kind, &.{}, &total);
    if (total == 0) return null;
    const names = gpa.alloc(core.bridge.RmgName, total) catch return null;
    var got: usize = 0;
    _ = editor.bridge.listRmg(kind, names, &got);
    if (got != total) {
        gpa.free(names);
        return null;
    }
    return names;
}

fn replaceName(gpa: std.mem.Allocator, slot: *[]u8, text: []const u8) !void {
    const copy = try gpa.dupe(u8, text);
    gpa.free(slot.*);
    slot.* = copy;
}

/// Authors the set from the shipped template `base` through the Editor's composer
/// I/O: its first graph, its default field set, an authored copy of every container
/// the graph's nodes hold, and the template naming the authored graph and field set.
fn authorSet(gpa: std.mem.Allocator, editor: *core.editor.Editor, base: []const u8) !void {
    var tpl = try editor.readTemplate(base);
    defer tpl.deinit(gpa);
    if (tpl.graphs.items.len == 0 or tpl.fields.items.len == 0) return error.Refused;
    const default_index: usize = if (tpl.default_field >= 0 and @as(usize, @intCast(tpl.default_field)) < tpl.fields.items.len) @intCast(tpl.default_field) else 0;
    var graph = try editor.readGraph(tpl.graphs.items[0].name);
    defer graph.deinit(gpa);
    var field = try editor.readFieldSet(tpl.fields.items[default_index].name);
    defer field.deinit(gpa);
    var held: std.ArrayListUnmanaged([]u8) = .empty;
    defer {
        for (held.items) |name| gpa.free(name);
        held.deinit(gpa);
    }
    for (graph.nodes.items) |*node| {
        if (node.container.len == 0) continue;
        var at: usize = 0;
        while (at < held.items.len and !std.mem.eql(u8, held.items[at], node.container)) at += 1;
        if (at == held.items.len) try held.append(gpa, try gpa.dupe(u8, node.container));
        var container = try editor.readContainer(node.container);
        defer container.deinit(gpa);
        var name_buffer: [96]u8 = undefined;
        const name = try std.fmt.bufPrint(&name_buffer, "scenarios\\containers\\user\\authored_c{d}", .{at});
        try editor.writeContainer(name, &container);
        try replaceName(gpa, &node.container, name);
    }
    try editor.writeGraph(authored_graph, &graph);
    try editor.writeFieldSet(authored_field, &field);
    for (tpl.graphs.items[1..]) |*entry| entry.deinit(gpa);
    tpl.graphs.shrinkRetainingCapacity(1);
    try replaceName(gpa, &tpl.graphs.items[0].name, authored_graph);
    tpl.graphs.items[0].weight = 1;
    for (tpl.fields.items[1..]) |*entry| entry.deinit(gpa);
    tpl.fields.shrinkRetainingCapacity(1);
    try replaceName(gpa, &tpl.fields.items[0].name, authored_field);
    tpl.fields.items[0].weight = 1;
    tpl.default_field = 0;
    try editor.writeTemplate(authored_template, &tpl);
}

/// Test in game for the open map, as the menu does it: the map's script copied beside
/// the test map (`copyScriptForTest`), the test copy saved, the real Game played; the
/// game's BK_MAP_TRACE must report the map's `name` script loaded and run. Null after
/// printing why.
fn playWithScript(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, rig: *common.Rig, paths: *const common.TestPaths, log_path: []const u8, what: []const u8, name: []const u8) ?Play {
    const editor = &rig.editor;
    var note: [256]u8 = undefined;
    if (panels_logic.copyScriptForTest(editor, paths.test_path, &note)) |warning| {
        std.debug.print("map-editor: {s} FAIL: Test in game would not copy the script of {s}: {s}\n", .{ label, what, warning });
        return null;
    }
    if (!common.saveTestCopy(rig, label, what, paths.test_path)) return null;
    const played = play(gpa, io, environ, paths, what, log_path) orelse return null;
    const script = played.trace.script orelse {
        std.debug.print("map-editor: {s} FAIL: the game's BK_MAP_TRACE reported no script for {s}; see {s}\n", .{ label, what, log_path });
        gpa.free(played.log);
        return null;
    };
    if (!std.mem.eql(u8, script.name.slice(), name) or !script.loaded or !script.init) {
        std.debug.print("map-editor: {s} FAIL: the game's script for {s} is \"{s}\" loaded={} init={}, not {s} loaded and run; see {s}\n", .{ label, what, script.name.slice(), script.loaded, script.init, name, log_path });
        gpa.free(played.log);
        return null;
    }
    std.debug.print("map-editor: {s}: {s}: BK_MAP_TRACE script name={s} loaded=1 init=1\n", .{ label, what, script.name.slice() });
    return played;
}

/// The generated map's name, which is also its script's: the map is made as
/// `m3_authored`, so the stored script path has to be exactly that.
const authored_name = "m3_authored";

/// `path` with its last extension cut off (an OS path).
fn withoutExtension(path: []const u8) []const u8 {
    const dot = std.mem.lastIndexOfScalar(u8, path, '.') orelse return path;
    const cut = std.mem.lastIndexOfAny(u8, path, "/\\") orelse 0;
    return if (dot > cut) path[0..dot] else path;
}

/// Leg 4: the generated map (open now, beside its script) and its .lua are copied to a
/// folder of their own and the originals deleted - another computer's folder, as far as
/// the map file can tell - then the map is opened from there and played with its script.
/// Returns the play of the moved map, or null after printing why.
fn movedLeg(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, rig: *common.Rig, paths: *const common.TestPaths, log_path: []const u8, generated_os: []const u8) ?Play {
    const editor = &rig.editor;
    const files = editor.files orelse return null;
    const parent = std.fs.path.dirname(log_path) orelse ".";
    var dir_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const moved_dir = std.fmt.bufPrint(&dir_buffer, "{s}{c}game-reads-it-m3-moved", .{ parent, std.fs.path.sep }) catch {
        std.debug.print("map-editor: {s} FAIL: the moved folder's path is too long\n", .{label});
        return null;
    };
    std.Io.Dir.cwd().deleteTree(io, moved_dir) catch {};
    std.Io.Dir.cwd().createDirPath(io, moved_dir) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the folder {s} would not be made: {s}\n", .{ label, moved_dir, @errorName(err) });
        return null;
    };
    var from_lua_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const from_lua = std.fmt.bufPrint(&from_lua_buffer, "{s}.lua", .{withoutExtension(generated_os)}) catch return null;
    var to_map_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const to_map = std.fmt.bufPrint(&to_map_buffer, "{s}{c}{s}.bzm", .{ moved_dir, std.fs.path.sep, authored_name }) catch return null;
    var to_lua_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const to_lua = std.fmt.bufPrint(&to_lua_buffer, "{s}{c}{s}.lua", .{ moved_dir, std.fs.path.sep, authored_name }) catch return null;
    if (!files.exists(from_lua)) {
        std.debug.print("map-editor: {s} FAIL: the generation left no script beside its map at {s}\n", .{ label, from_lua });
        return null;
    }
    files.copy(generated_os, to_map) catch {
        std.debug.print("map-editor: {s} FAIL: the map would not copy to {s}: {s}\n", .{ label, to_map, files.lastError() });
        return null;
    };
    files.copy(from_lua, to_lua) catch {
        std.debug.print("map-editor: {s} FAIL: the script would not copy to {s}: {s}\n", .{ label, to_lua, files.lastError() });
        return null;
    };
    // The originals go, so nothing can be found where the map used to be.
    files.delete(generated_os);
    files.delete(from_lua);
    var open_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const moved = panels_logic.enginePath(&open_buffer, to_map, .open, .bzm) orelse {
        std.debug.print("map-editor: {s} FAIL: the moved map's path is too long\n", .{label});
        return null;
    };
    editor.open(moved) catch {
        std.debug.print("map-editor: {s} FAIL: the moved map {s} did not open: {s}\n", .{ label, to_map, editor.status() });
        return null;
    };
    if (!rig.settle(label, 2)) return null;
    var log_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const moved_log = std.fmt.bufPrint(&log_buffer, "{s}.moved.log", .{log_path}) catch return null;
    return playWithScript(gpa, io, environ, rig, paths, moved_log, "the moved authored map", authored_name);
}

/// The authored leg: author, generate from the fixed seed, open the map, play its
/// test copy (with its script), then move the map and its script and play that. True
/// when the game loaded it (its roads equal the editor's) and exited 0.
fn authoredLeg(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, rig: *common.Rig, paths: *const common.TestPaths, log_path: []const u8) !?usize {
    const editor = &rig.editor;
    const base = "scenarios\\templates\\summer\\template02";
    const contexts = listNames(gpa, editor, .chapters) orelse {
        std.debug.print("map-editor: {s} FAIL: no chapters are listed\n", .{label});
        return null;
    };
    defer gpa.free(contexts);
    var context: ?[]const u8 = null;
    for (contexts) |*entry| {
        if (std.mem.endsWith(u8, entry.nameSlice(), "\\context")) {
            context = entry.nameSlice();
            break;
        }
    }
    const chosen = context orelse {
        std.debug.print("map-editor: {s} FAIL: no chapter context is listed\n", .{label});
        return null;
    };
    authorSet(gpa, editor, base) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the authored set would not write ({s}): {s}\n", .{ label, @errorName(err), editor.status() });
        return null;
    };
    var params: core.bridge.RmgGenerateParams = .{};
    params.setTemplate(authored_template);
    params.setContext(chosen);
    params.setSetting("scenarios\\settings\\summer_france");
    params.setMapName("m3_authored");
    params.level = 0;
    params.graph = 0;
    params.angle = 0;
    params.save_as_bzm = 1;
    params.write_dds = 0;
    params.overwrite = 1;
    params.has_seed = 1;
    params.seed = 424242;
    var result: core.bridge.RmgGenerateResult = .{};
    editor.createRandomMap(params, &result) catch {
        std.debug.print("map-editor: {s} FAIL: the authored template would not generate: {s}\n", .{ label, editor.status() });
        return null;
    };
    // The engine's own spelling of the OS path, as the File menu's open does.
    var open_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
    const generated = panels_logic.enginePath(&open_buffer, result.mapPathSlice(), .open, .bzm) orelse {
        std.debug.print("map-editor: {s} FAIL: the generated map's path is too long\n", .{label});
        return null;
    };
    editor.open(generated) catch {
        std.debug.print("map-editor: {s} FAIL: the generated map {s} did not open: {s}\n", .{ label, result.mapPathSlice(), editor.status() });
        return null;
    };
    if (!rig.settle(label, 2)) return null;
    const roads = editor.vsoCount(.road) catch 0;
    // The generator names the script relative to the map's own folder (WINDOWS.md 5).
    var script_buffer: [core.records.script_file_capacity]u8 = undefined;
    const stored = editor.scriptFileName(&script_buffer) catch {
        std.debug.print("map-editor: {s} FAIL: the generated map's script name would not read: {s}\n", .{ label, editor.status() });
        return null;
    };
    if (!std.mem.eql(u8, stored, authored_name)) {
        std.debug.print("map-editor: {s} FAIL: the generated map names its script \"{s}\", not the bare {s}\n", .{ label, stored, authored_name });
        return null;
    }
    const authored_log_path = try pathWithSuffix(gpa, log_path, ".authored.log");
    defer gpa.free(authored_log_path);
    const played = playWithScript(gpa, io, environ, rig, paths, authored_log_path, "the authored map", authored_name) orelse return null;
    defer gpa.free(played.log);
    const traced = played.trace.roads orelse {
        std.debug.print("map-editor: {s} FAIL: the game's BK_MAP_TRACE did not report the authored map's roads (it did not load the map?); see {s}\n", .{ label, authored_log_path });
        return null;
    };
    if (traced != roads) {
        std.debug.print("map-editor: {s} FAIL: the game loaded {d} roads of the authored map, the editor holds {d}; see {s}\n", .{ label, traced, roads, authored_log_path });
        return null;
    }
    // 4. Moved to another folder with its script, the same map plays the same.
    const moved = movedLeg(gpa, io, environ, rig, paths, log_path, result.mapPathSlice()) orelse return null;
    defer gpa.free(moved.log);
    const moved_roads = moved.trace.roads orelse {
        std.debug.print("map-editor: {s} FAIL: the game's BK_MAP_TRACE did not report the moved map's roads\n", .{label});
        return null;
    };
    if (moved_roads != roads) {
        std.debug.print("map-editor: {s} FAIL: the game loaded {d} roads of the moved map, the editor held {d}\n", .{ label, moved_roads, roads });
        return null;
    }
    std.debug.print("map-editor: {s}: the map and its script moved to another folder and the game ran the script there\n", .{label});
    return traced;
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

    // 2b. An installation without the progress screen's movie list (a sparse
    // checkout without Data/movies/, an incomplete copy) used to abort the game at
    // the start of every mission (CProgressScreen::Init read the null stream). The
    // same test copy, the list pointed at a file that does not exist: the mission
    // must still load and the game exit 0, saying why there is no loading screen.
    const no_progress_log_path = try pathWithSuffix(gpa, log_path, ".no-progress-list.log");
    defer gpa.free(no_progress_log_path);
    const no_progress = playWithEnv(gpa, io, environ, &paths, "game without the progress screen's movie list", no_progress_log_path, &.{
        .{ "BK_PROGRESS_XML", missing_progress_xml },
    }) orelse return false;
    defer gpa.free(no_progress.log);
    if (std.mem.indexOf(u8, no_progress.log, progress_warning) == null) {
        std.debug.print("map-editor: {s} FAIL: the game without the progress list did not log \"{s}...\"; see {s}\n", .{ label, progress_warning, no_progress_log_path });
        return false;
    }
    const no_progress_roads = no_progress.trace.roads orelse {
        std.debug.print("map-editor: {s} FAIL: the game without the progress list did not report the map's roads (it did not load the mission?); see {s}\n", .{ label, no_progress_log_path });
        return false;
    };
    if (no_progress_roads != regular_roads) {
        std.debug.print("map-editor: {s} FAIL: the game without the progress list loaded {d} roads, the editor holds {d}; see {s}\n", .{ label, no_progress_roads, regular_roads, no_progress_log_path });
        return false;
    }
    std.debug.print("map-editor: {s}: without the progress list the mission still loads ({d} roads), the warning is logged, exit 0\n", .{ label, no_progress_roads });

    // 3. The authored set generates a map the game loads.
    const authored_roads = (try authoredLeg(gpa, io, environ, &rig, &paths, log_path)) orelse return false;

    var report: std.ArrayListUnmanaged(u8) = .empty;
    defer report.deinit(gpa);
    try appendTraceLines(gpa, &report, "=== short railroad ===", crafted.log);
    try appendTraceLines(gpa, &report, "=== regular map ===", regular.log);
    std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = report.items }) catch |err| {
        std.debug.print("map-editor: {s} FAIL: the report {s} would not write: {s}\n", .{ label, log_path, @errorName(err) });
        return false;
    };
    common.deleteAutoshots(io, paths.game_path);
    std.debug.print("map-editor: {s}: the authored set generated a map the game loaded clean: {d} roads, exit 0\n", .{ label, authored_roads });
    std.debug.print("map-editor: {s} PASS (short railroad loaded clean: {d} roads incl. {d} short, exit 0; the regular map still loads: {d} roads, exit 0, and without the progress list too; the authored template's map loads: {d} roads, exit 0)\n", .{ label, crafted_roads, short, regular_traced, authored_roads });
    return true;
}
