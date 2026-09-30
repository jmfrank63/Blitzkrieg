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

/// The engine's roads and rivers agree with the map (BkEditorVsoMatchesEngine),
/// or the test fails naming the first difference.
fn expectVsoMatches(real: *RealBridge) !void {
    const result = real.vsoMatchesEngine();
    if (result != .ok) {
        std.debug.print("map-editor-engine: the engine's roads and rivers do not match the map ({t}): {s}\n", .{ result, std.mem.span(c.BkEditorLastMessage(real.session)) });
        return error.VsoDoesNotMatch;
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

/// The camera anchors read straight through the C ABI, not through
/// RealBridge, for the same reason `expectDocumentIsBridge` reads raw
/// records: an adapter conversion bug reaches the core and RealBridge alike.
fn rawAnchors(real: *RealBridge) !c.BkEditorCameraAnchorRecord {
    var record: c.BkEditorCameraAnchorRecord = std.mem.zeroes(c.BkEditorCameraAnchorRecord);
    try std.testing.expect(c.BkEditorCameraAnchors(real.session, &record) == c.BK_EDITOR_OK);
    return record;
}

/// A single soldier (an infantry SGVOGT_UNIT) is never put on a map on its
/// own: the game plays soldiers only inside a squad, and one saved as a unit
/// crashed Test in game in CSoldierRestState::Segment (03-15 gap fix). The
/// catalogue marks it not placeable, an add is refused naming the squad to
/// place instead, and neither the map nor the engine changes.
fn expectLoneSoldierRefused(real: *RealBridge, editor: *Editor, catalogue: []const c.BkEditorCatalogueEntry, soldier: []const u8, squad: []const u8, x: f32, y: f32) !void {
    const entry = for (catalogue) |*candidate| {
        if (std.mem.eql(u8, std.mem.sliceTo(&candidate.name, 0), soldier)) break candidate;
    } else return error.SoldierNotInCatalogue;
    try std.testing.expectEqual(@as(c_int, 1), entry.game_type);
    try std.testing.expectEqual(@as(c_int, 0), entry.placeable);
    const objects_before = editor.document.objects.items.len;
    try std.testing.expectError(error.Refused, editor.addObject(soldier, x, y, 0, 0));
    std.debug.print("map-editor-engine: {s} refused: {s}\n", .{ soldier, editor.status() });
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), squad) != null);
    try std.testing.expectEqual(objects_before, editor.document.objects.items.len);
    try expectEngineMatches(real);
    try expectDocumentIsBridge(real, editor);
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

    // A single soldier is refused - the sniper Johannes placed and a Bren
    // gunner, whose squads differ in kind (a one-man squad, and a squad the
    // soldier is one of nine in) - and the squad it names places instead.
    const catalogue = try real.catalogue(std.testing.allocator);
    defer std.testing.allocator.free(catalogue);
    for (catalogue) |*entry| {
        if (std.mem.eql(u8, std.mem.sliceTo(&entry.name, 0), "US_sniper")) try std.testing.expectEqual(@as(c_int, 1), entry.placeable);
        if (std.mem.eql(u8, std.mem.sliceTo(&entry.name, 0), "10.5-cm_Flak38")) try std.testing.expectEqual(@as(c_int, 1), entry.placeable);
    }
    try expectLoneSoldierRefused(&real, &editor, catalogue, "Us_Sniper", "\"US_sniper\"", first.x + 160, first.y);
    try expectLoneSoldierRefused(&real, &editor, catalogue, "Allies_Bren", "\"GB_bren_43\"", first.x + 160, first.y);
    const squad = try editor.addObject("US_sniper", first.x + 160, first.y, 0, 0);
    try std.testing.expect(editor.document.find(squad) != null);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);

    // Everything undone: back to the map as opened, in the engine too.
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(objects_at_open, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);

    // M2 (04-01): a camera anchor end to end on the real engine, the smallest
    // real record edit. Player 0's anchor goes to the map's middle (world
    // units), the engine still agrees with the map, and each undo puts the
    // anchors back exactly as the bridge held them, vector size included.
    const world_cell: f32 = 32.0 * @sqrt(2.0);
    const middle_x = @as(f32, @floatFromInt(editor.document.info.width_tiles)) * world_cell / 2.0;
    const middle_y = @as(f32, @floatFromInt(editor.document.info.height_tiles)) * world_cell / 2.0;
    const anchors_before = try rawAnchors(&real);
    try editor.setCameraAnchor(0, middle_x, middle_y);
    try expectEngineMatches(&real);
    const anchors_set = try rawAnchors(&real);
    try std.testing.expect(anchors_set.player_count >= 1);
    try std.testing.expectEqual(middle_x, anchors_set.players[0].x);
    try std.testing.expectEqual(middle_y, anchors_set.players[0].y);
    try std.testing.expect(editor.dirty());
    _ = try editor.undo();
    try expectEngineMatches(&real);
    try std.testing.expectEqualSlices(u8, std.mem.asBytes(&anchors_before), std.mem.asBytes(&try rawAnchors(&real)));
    _ = try editor.redo();
    try std.testing.expectEqualSlices(u8, std.mem.asBytes(&anchors_set), std.mem.asBytes(&try rawAnchors(&real)));
    _ = try editor.undo();
    try std.testing.expectEqualSlices(u8, std.mem.asBytes(&anchors_before), std.mem.asBytes(&try rawAnchors(&real)));
    try std.testing.expectError(error.Refused, editor.setCameraAnchor(0, -5000, -5000));
    try std.testing.expect(!editor.dirty());
    try expectEngineMatches(&real);
    std.debug.print("map-editor-engine: M2 camera anchors round trip ok\n", .{});

    // M2 (04-02): a delete of a real unit of coldwinter, through the cascade
    // the bridge now runs (nothing in coldwinter names it, so the cascade is
    // empty here; the engine tier carries the records that do). Undo, redo and
    // undo again: the document is the bridge's and the engine agrees each time.
    const victim = pick: for (editor.document.objects.items) |candidate| {
        if (!candidate.known) continue;
        editor.delete(candidate.link_id) catch |err| switch (err) {
            error.Refused => continue,
            else => return err,
        };
        break :pick candidate;
    } else return error.NoObjectDeletes;
    try std.testing.expect(editor.document.find(victim.link_id) == null);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.undo();
    try std.testing.expect(editor.document.find(victim.link_id) != null);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.redo();
    try std.testing.expect(editor.document.find(victim.link_id) == null);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.undo();
    try std.testing.expect(editor.document.find(victim.link_id) != null);
    try std.testing.expect(!editor.dirty());
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    std.debug.print("map-editor-engine: M2 delete round trip ok\n", .{});

    // M2 (04-05): a road drawn through the core Editor on the real engine -
    // the bridge derives it, the engine draws it, undo and redo put the stored
    // records back - and the engine's roads and rivers agree with the map
    // after every step (BkEditorVsoMatchesEngine), as do its terrain and world.
    const road_types = try editor.vsoDescriptors(.road, std.testing.allocator);
    defer std.testing.allocator.free(road_types);
    try std.testing.expect(road_types.len > 0);
    const roads_at_open = try editor.vsoCount(.road);
    const line = [_]core.records.Vec3{
        .{ .x = middle_x - 300, .y = middle_y - 100 },
        .{ .x = middle_x, .y = middle_y + 60 },
        .{ .x = middle_x + 300, .y = middle_y - 40 },
    };
    try expectVsoMatches(&real);
    const road = try editor.addVso(.road, road_types[0].nameSlice(), &line, 3, 1);
    try std.testing.expectEqual(roads_at_open, road);
    try std.testing.expectEqual(roads_at_open + 1, try editor.vsoCount(.road));
    try expectEngineMatches(&real);
    try expectVsoMatches(&real);
    var view = try editor.readVso(.road, road);
    defer view.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 3), view.control_points.len);
    try std.testing.expectEqual(@as(usize, 3), view.key_points.len);
    try std.testing.expectEqual(line[1].x, view.control_points[1].x);
    _ = try editor.undo();
    try std.testing.expectEqual(roads_at_open, try editor.vsoCount(.road));
    try expectEngineMatches(&real);
    try expectVsoMatches(&real);
    _ = try editor.redo();
    try std.testing.expectEqual(roads_at_open + 1, try editor.vsoCount(.road));
    try expectEngineMatches(&real);
    try expectVsoMatches(&real);
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    try expectEngineMatches(&real);
    try expectVsoMatches(&real);
    std.debug.print("map-editor-engine: M2 road round trip ok\n", .{});

    // M2 (04-06): a W_WoodenBig_Heavy_01 bridge drawn through the core Editor
    // on the real engine - the bridge plans its spans, adds them and the
    // bridges entry as one step - then undone, redone and undone, with the
    // engine, the world and the document agreeing with the map each time.
    const bridge_types = try editor.bridgeDescriptors(std.testing.allocator);
    defer std.testing.allocator.free(bridge_types);
    const wooden = for (bridge_types) |*item| {
        if (std.mem.eql(u8, item.nameSlice(), "W_WoodenBig_Heavy_01")) break item;
    } else return error.NoWoodenBigHeavy;
    try std.testing.expectEqual(core.bridge.BridgeDirection.horizontal, wooden.direction);
    try std.testing.expect(wooden.has_partner and wooden.build_during_play_allowed);
    const bridges_at_open = count: {
        const at_open = try editor.bridges(std.testing.allocator);
        defer std.testing.allocator.free(at_open);
        break :count at_open.len;
    };
    const objects_before_bridge = editor.document.objects.items.len;
    const entry = try editor.drawBridge("W_WoodenBig_Heavy_01", middle_x - 250, middle_y + 250, middle_x + 250, middle_y + 250);
    try std.testing.expectEqual(bridges_at_open, entry);
    const drawn = try editor.bridges(std.testing.allocator);
    defer std.testing.allocator.free(drawn);
    try std.testing.expectEqual(bridges_at_open + 1, drawn.len);
    try std.testing.expectEqualStrings("W_WoodenBig_Heavy_01", drawn[entry].descSlice());
    const spans: usize = @intCast(drawn[entry].span_count);
    try std.testing.expect(spans >= 3);
    try std.testing.expectEqual(objects_before_bridge + spans, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.undo();
    try std.testing.expectEqual(objects_before_bridge, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.redo();
    try std.testing.expectEqual(objects_before_bridge + spans, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    try std.testing.expectError(error.Refused, editor.drawBridge("W_WoodenBig_Heavy_01", middle_x, middle_y - 250, middle_x, middle_y + 250));
    try std.testing.expect(!editor.dirty());
    std.debug.print("map-editor-engine: M2 bridge round trip ok ({d} spans)\n", .{spans});

    // M2 (04-08): an L-shaped entrenchment drawn through the core Editor on
    // the real engine - the bridge runs the MFC builder, adds the pieces and
    // the entrenchments entry as one step - then undone, redone and undone,
    // with the engine, the world and the document agreeing with the map each
    // time.
    const trenches_at_open = count: {
        const at_open = try editor.entrenchments(std.testing.allocator);
        defer std.testing.allocator.free(at_open);
        break :count at_open.len;
    };
    const objects_before_trench = editor.document.objects.items.len;
    const clicks = [_]core.records.Vec3{
        .{ .x = middle_x - 300, .y = middle_y - 250 },
        .{ .x = middle_x + 300, .y = middle_y - 250 },
        .{ .x = middle_x + 300, .y = middle_y + 250 },
    };
    const trench = try editor.drawEntrenchment(&clicks, 1);
    try std.testing.expectEqual(trenches_at_open, trench);
    const drawn_trenches = try editor.entrenchments(std.testing.allocator);
    defer std.testing.allocator.free(drawn_trenches);
    try std.testing.expectEqual(trenches_at_open + 1, drawn_trenches.len);
    const pieces: usize = @intCast(drawn_trenches[trench].piece_count);
    try std.testing.expect(pieces >= 6);
    try std.testing.expect(drawn_trenches[trench].section_count >= 2);
    try std.testing.expectEqual(@as(i32, 1), drawn_trenches[trench].player);
    try std.testing.expectEqual(objects_before_trench + pieces, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.undo();
    try std.testing.expectEqual(objects_before_trench, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.redo();
    try std.testing.expectEqual(objects_before_trench + pieces, editor.document.objects.items.len);
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    std.debug.print("map-editor-engine: M2 entrenchment round trip ok ({d} pieces, {d} sections)\n", .{ pieces, drawn_trenches[trench].section_count });

    std.debug.print("map-editor-engine: PASS ({d} objects)\n", .{objects_at_open});
}
