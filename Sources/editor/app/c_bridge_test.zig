//! The engine tier of the core: the core's Editor driving the real engine
//! through RealBridge, and the engine asked after every step whether it still
//! agrees with the map (BkEditorTerrainMatchesEngine, BkEditorWorldMatchesMap).
//! Run from the installation (build.zig test-map-editor-engine), because the
//! engine finds its data from there.
const std = @import("std");
const core = @import("editor_core");
const crt = @import("crt.zig");
const Host = @import("host.zig").Host;
const RealBridge = @import("c_bridge.zig").RealBridge;
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
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.undo();
    try std.testing.expectEqual(first.x, editor.document.find(first.link_id).?.x);

    // Tile 0, the first tile of every shipped tileset (Data/Terrain/sets/*/
    // tileset.xml); odd tiles below 14 are in none of them, and the engine's
    // CTerrain::SetTile indexes its terrain types with -1 for a tile the
    // tileset lacks.
    const tile = 0;
    const gesture = editor.beginGesture();
    try editor.paint(&.{.{ .x = 20, .y = 20, .tile = tile }}, gesture);
    try editor.paint(&.{.{ .x = 21, .y = 20, .tile = tile }}, gesture);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.undo();
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.redo();
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());

    const added = try editor.addObject(first.nameSlice(), first.x + 96, first.y + 96, 0, 0);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    _ = try editor.undo();
    try std.testing.expect(editor.document.find(added) == null);
    _ = try editor.redo();
    try std.testing.expect(editor.document.find(added) != null);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());

    try editor.delete(added);
    _ = try editor.undo();
    try std.testing.expectEqual(added, editor.document.find(added).?.link_id);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());

    try editor.setDiplomacy(1, if (editor.document.diplomacy.items[1] == 0) 1 else 0);
    _ = try editor.undo();

    // Everything undone: back to the map as opened, in the engine too.
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(objects_at_open, editor.document.objects.items.len);
    try std.testing.expectEqual(core.bridge.Status.ok, real.engineMatches());
    std.debug.print("map-editor-engine: PASS ({d} objects)\n", .{objects_at_open});
}
