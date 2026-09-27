//! The engine tier of the core: the core's Editor driving the real engine
//! through RealBridge, and the engine asked after every step whether it still
//! agrees with the map (BkEditorTerrainMatchesEngine, BkEditorWorldMatchesMap).
//! Run from the installation (build.zig test-map-editor-engine), because the
//! engine finds its data from there.
const std = @import("std");
const core = @import("editor_core");
const crt = @import("crt.zig");
const host_mod = @import("host.zig");
const Host = host_mod.Host;
const c_bridge = @import("c_bridge.zig");
const RealBridge = c_bridge.RealBridge;
const c = c_bridge.c;
const Editor = core.editor.Editor;

// A test executable is a host too. On Windows it is entered like MapEditor,
// through mainCRTStartup (crt.zig minimalFromPeb says why), so the C main
// that calls is exported here and hands over to the test runner, which is
// the root of a test build. Its asserts and aborts go to stderr first, or a
// failed assert would hang the Windows job behind a message box.
comptime {
    if (crt.exports_c_main) @export(&crtMain, .{ .name = "main" });
}

fn crtMain(argc: c_int, argv: ?*anyopaque) callconv(.c) c_int {
    _ = argc;
    _ = argv;
    crt.routeCrtReportsToStderr();
    @import("root").main(crt.minimalFromPeb());
    return 0;
}

/// The engine agrees with the map, or the test fails naming the first
/// difference, which the two checks put in the bridge's last message.
fn expectEngineMatches(real: *RealBridge) !void {
    const result = real.engineMatches();
    if (result != .ok) {
        std.debug.print("map-editor-engine: the engine does not match the map ({t}): {s}\n", .{ result, std.mem.span(c.BkEditorLastMessage(real.session)) });
        return error.EngineDoesNotMatch;
    }
}

fn engineTile(real: *RealBridge, x: c_int, y: c_int) !u8 {
    var tile: u8 = 0;
    if (c.BkEditorEngineTile(real.session, x, y, &tile) != c.BK_EDITOR_OK) {
        std.debug.print("map-editor-engine: no tile at {d},{d}: {s}\n", .{ x, y, std.mem.span(c.BkEditorLastMessage(real.session)) });
        return error.NoTile;
    }
    return tile;
}

/// The document against the bridge's objects read straight through the C
/// ABI, not through RealBridge: a conversion bug in the adapter (x and y
/// swapped, say) reaches the document and RealBridge alike, but not the raw
/// records.
fn expectDocumentIsBridge(real: *RealBridge, editor: *Editor) !void {
    var count: c_int = 0;
    _ = c.BkEditorObjects(real.session, null, 0, &count);
    const records = try std.testing.allocator.alloc(c.BkEditorObjectRecord, @intCast(count));
    defer std.testing.allocator.free(records);
    try std.testing.expect(c.BkEditorObjects(real.session, records.ptr, count, &count) == c.BK_EDITOR_OK);
    const objects = editor.document.objects.items;
    try std.testing.expectEqual(records.len, objects.len);
    for (records, objects, 0..) |record, object, index| {
        errdefer std.debug.print("map-editor-engine: object {d} (link {d}) differs between the document and the bridge\n", .{ index, record.link_id });
        try std.testing.expectEqual(record.link_id, object.link_id);
        try std.testing.expectEqualStrings(std.mem.sliceTo(&record.name, 0), object.nameSlice());
        try std.testing.expectEqual(record.x, object.x);
        try std.testing.expectEqual(record.y, object.y);
        try std.testing.expectEqual(record.dir, object.dir);
        try std.testing.expectEqual(record.player, object.player);
        try std.testing.expectEqual(record.scenario != 0, object.scenario);
        try std.testing.expectEqual(record.known != 0, object.known);
    }
}

// First, while no module is loaded yet: a start refused for an empty
// installation fails before the engine loads anything (LoadAllModules finds
// nothing and keeps nothing), so the next test starts the real one as if
// this had not run. The start-up dialog shows failureReason(), so it has to
// be the bridge's reason, not only the error's name.
test "a failed start keeps the bridge's reason for the start-up dialog" {
    var empty = std.testing.tmpDir(.{});
    defer empty.cleanup();
    const root = try std.fmt.allocPrintSentinel(std.testing.allocator, ".zig-cache{c}tmp{c}{s}", .{ std.fs.path.sep, std.fs.path.sep, empty.sub_path }, 0);
    defer std.testing.allocator.free(root);
    if (Host.start(.{ .title = "map-editor-engine", .hidden = true, .data_root = root.ptr })) |started| {
        var host = started;
        host.stop();
        return error.StartedWithoutAnEngine;
    } else |err| {
        std.debug.print("map-editor-engine: an empty installation: {t}: {s}\n", .{ err, host_mod.failureReason() });
        if (err == error.SdlInitFailed or err == error.WindowFailed) return error.SkipZigTest;
        try std.testing.expectEqual(error.EngineFailed, err);
        try std.testing.expect(std.mem.indexOf(u8, host_mod.failureReason(), "no engine modules loaded from") != null);
    }
}

test "the core drives the real bridge: every command, undone and redone" {
    var host = Host.start(.{ .title = "map-editor-engine", .hidden = true }) catch |err| switch (err) {
        error.NoDevice => {
            std.debug.print("map-editor-engine: skipped: no GPU device\n", .{});
            return;
        },
        else => return err,
    };
    defer host.stop();
    var real = RealBridge.init(host.session);
    var editor = Editor.init(std.testing.allocator, real.bridge());
    defer editor.deinit();

    try editor.open("Data\\Maps\\Multiplayer\\coldwinter.bzm");
    const objects_at_open = editor.document.objects.items.len;
    try std.testing.expect(objects_at_open > 0);

    // A known object nothing refers to, placed in the engine: the first one
    // whose move the bridge accepts (a bridge span or an object the database
    // does not know is refused).
    const first = pick: for (editor.document.objects.items, 0..) |candidate, index| {
        if (!candidate.known) continue;
        const moved: core.editor.Pose = .{ .x = candidate.x + 64, .y = candidate.y, .dir = candidate.dir, .player = candidate.player };
        editor.place(candidate.link_id, moved, 0) catch |err| switch (err) {
            error.Refused => continue,
            else => return err,
        };
        std.debug.print("map-editor-engine: moving object {d}, link {d} ({s})\n", .{ index, candidate.link_id, candidate.nameSlice() });
        break :pick candidate;
    } else return error.NoObjectMoves;
    try expectEngineMatches(&real);
    _ = try editor.undo();
    try std.testing.expectEqual(first.x, editor.document.find(first.link_id).?.x);

    // A paint the engine must show: a tile neither cell holds. 0 and 14 are in
    // every shipped tileset (Data/Terrain/sets/*/tileset.xml), in different
    // terrain types.
    const cells = [2][2]c_int{ .{ 20, 20 }, .{ 21, 20 } };
    var before: [2]u8 = undefined;
    for (cells, &before) |cell, *tile| tile.* = try engineTile(&real, cell[0], cell[1]);
    const tile: u8 = if (before[0] != 0 and before[1] != 0) 0 else 14;
    try std.testing.expect(tile != before[0] and tile != before[1]);
    const gesture = editor.beginGesture();
    try editor.paint(&.{.{ .x = cells[0][0], .y = cells[0][1], .tile = tile }}, gesture);
    try editor.paint(&.{.{ .x = cells[1][0], .y = cells[1][1], .tile = tile }}, gesture);
    try expectEngineMatches(&real);
    var painted: [2]u8 = undefined;
    for (cells, &painted, before) |cell, *now, was| {
        now.* = try engineTile(&real, cell[0], cell[1]);
        try std.testing.expect(now.* != was);
    }
    std.debug.print("map-editor-engine: painted tile {d} over {d},{d}; the engine shows {d},{d}\n", .{ tile, before[0], before[1], painted[0], painted[1] });
    _ = try editor.undo();
    try expectEngineMatches(&real);
    for (cells, before) |cell, was| try std.testing.expectEqual(was, try engineTile(&real, cell[0], cell[1]));
    _ = try editor.redo();
    try expectEngineMatches(&real);
    for (cells, painted) |cell, now| try std.testing.expectEqual(now, try engineTile(&real, cell[0], cell[1]));

    const added = try editor.addObject(first.nameSlice(), first.x + 96, first.y + 96, 0, 0);
    try expectEngineMatches(&real);
    _ = try editor.undo();
    try std.testing.expect(editor.document.find(added) == null);
    _ = try editor.redo();
    try std.testing.expect(editor.document.find(added) != null);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);

    try editor.delete(added);
    _ = try editor.undo();
    try std.testing.expectEqual(added, editor.document.find(added).?.link_id);
    try expectEngineMatches(&real);

    const side = editor.document.diplomacy.items[1];
    try editor.setDiplomacy(1, if (side == 0) 1 else 0);
    _ = try editor.undo();
    try std.testing.expectEqual(side, editor.document.diplomacy.items[1]);
    var bridge_side: c_int = -1;
    try std.testing.expect(c.BkEditorDiplomacy(real.session, 1, &bridge_side) == c.BK_EDITOR_OK);
    try std.testing.expectEqual(side, bridge_side);

    // Everything undone: back to the map as opened, in the engine too.
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(objects_at_open, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    std.debug.print("map-editor-engine: PASS ({d} objects)\n", .{objects_at_open});
}
