//! The engine tier of the core: the core's Editor driving the real engine
//! through RealBridge, and the engine asked after every step whether it still
//! agrees with the map (BkEditorTerrainMatchesEngine, BkEditorWorldMatchesMap).
//! Run from the installation (build.zig test-map-editor-engine), because the
//! engine finds its data from there.
const std = @import("std");
const core = @import("editor_core");
const crt = @import("editor_kit").crt;
const host_mod = @import("editor_kit").host;
const Host = host_mod.Host;
const c_bridge = @import("c_bridge.zig");
const RealBridge = c_bridge.RealBridge;
const c = c_bridge.c;
const Editor = core.editor.Editor;
const logic = @import("panels_logic.zig");

/// The vertices of coldwinter (the map these tests open) that `CVertexAltitudeInfo::IsValidHeight`
/// refuses, counted by the bridge tier's `editor-bridge: M3 height rule` line.
const shipped_map_refused_vertices: usize = 0;

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
        try std.testing.expectEqual(record.script_id, object.script_id);
    }
}

/// The camera anchors read straight through the C ABI, not through
/// RealBridge, for the same reason `expectDocumentIsBridge` reads raw
/// records: an adapter conversion bug reaches the core and RealBridge alike.
/// How many start commands the editor lists now (freed again).
fn startCommandCount(editor: *Editor) !usize {
    const listed = try editor.startCommands(std.testing.allocator);
    defer Editor.freeStartCommands(std.testing.allocator, listed);
    return listed.len;
}

/// The counts of one side of the AI general as the bridge answers them (the sizing pass).
fn rawAiSideInfo(real: *RealBridge, side: usize) !c.BkEditorAISideInfo {
    var info: c.BkEditorAISideInfo = std.mem.zeroes(c.BkEditorAISideInfo);
    const status = c.BkEditorAIGeneralSide(real.session, @intCast(side), &info, null, 0, null, 0, null, 0);
    try std.testing.expect(status == c.BK_EDITOR_OK or status == c.BK_EDITOR_REFUSED);
    return info;
}

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
        // WR-C09: a skip, reported by the test runner as one ("1 skipped"),
        // never a silent pass of the whole engine tier.
        error.NoDevice => {
            std.debug.print("map-editor-engine: skipped: no GPU device\n", .{});
            return error.SkipZigTest;
        },
        else => return err,
    };
    defer host.stop();
    var real = RealBridge.init(host.session);
    var editor = Editor.init(std.testing.allocator, real.bridge());
    defer editor.deinit();

    // WR-C02: with no map open the two-pass reads answer REFUSED, never an OK
    // with a zeroed record, and a group insert is not "already there".
    {
        const bridge = real.bridge();
        var value: core.records.Value = undefined;
        try std.testing.expectEqual(core.bridge.Status.refused, bridge.readRecord(.start_command, 0, std.testing.allocator, &value));
        try std.testing.expectEqual(core.bridge.Status.refused, bridge.readRecord(.ai_side, 0, std.testing.allocator, &value));
        try std.testing.expectEqual(core.bridge.Status.refused, bridge.readRecord(.group, 0, std.testing.allocator, &value));
        const group: core.records.Value = .{ .group = .{ .id = 3 } };
        try std.testing.expectEqual(core.bridge.Status.refused, bridge.insertRecord(3, &group));
        try std.testing.expect(std.mem.indexOf(u8, bridge.lastMessage(), "already") == null);
    }

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

    // M2 (04-09): an object's script ID end to end. The first object that
    // takes one (link ID 0 and shared link IDs are refused) is set, the
    // document and the raw records agree with the bridge, the engine still
    // agrees with the map (the AI is left alone, C7), and undo and redo walk
    // it back and forth.
    const script_target = pick: for (editor.document.objects.items) |candidate| {
        if (!candidate.known or candidate.link_id == 0) continue;
        editor.setScriptID(candidate.link_id, 4242, 0) catch |err| switch (err) {
            error.Refused => continue,
            else => return err,
        };
        break :pick candidate.link_id;
    } else return error.NoObjectTakesAScriptID;
    try std.testing.expectEqual(@as(i32, 4242), editor.document.find(script_target).?.script_id);
    try expectDocumentIsBridge(&real, &editor);
    try expectEngineMatches(&real);
    try std.testing.expect(editor.dirty());
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(i32, -1), editor.document.find(script_target).?.script_id);
    try expectDocumentIsBridge(&real, &editor);
    _ = try editor.redo();
    try std.testing.expectEqual(@as(i32, 4242), editor.document.find(script_target).?.script_id);
    try expectDocumentIsBridge(&real, &editor);
    try std.testing.expectError(error.Refused, editor.setScriptID(script_target, 32001, 0));
    try std.testing.expectError(error.Refused, editor.setScriptID(script_target, -2, 0));
    try std.testing.expectEqual(@as(i32, 4242), editor.document.find(script_target).?.script_id);
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    std.debug.print("map-editor-engine: M2 script id round trip ok\n", .{});

    // M3 (D-25): the selection's group move end to end. Two movable objects
    // selected as a set move by one batch call, the engine agrees, undo and
    // redo walk both back and forth, and everything undone leaves the map as
    // it opened - not dirty, engine in step.
    const second = pick2: for (editor.document.objects.items) |candidate| {
        if (!candidate.known or candidate.link_id == first.link_id or candidate.link_id == 0) continue;
        editor.moveSelection(&.{candidate.link_id}, 64, 0, 0) catch |err| switch (err) {
            error.Refused => continue,
            else => return err,
        };
        // Undo the probe move: only the "does it move" question was asked.
        _ = try editor.undo();
        break :pick2 candidate;
    } else return error.NoSecondObjectMoves;
    const depth_at_multi = editor.history.undo_stack.items.len;
    editor.selectOnly(first.link_id);
    editor.selectionToggle(second.link_id);
    try std.testing.expectEqual(@as(usize, 2), editor.selectionCount());
    const members = try editor.selectionMembers(std.testing.allocator);
    defer std.testing.allocator.free(members);
    const before_a = editor.document.find(first.link_id).?.x;
    const before_b = editor.document.find(second.link_id).?.x;
    try editor.moveSelection(members, 128, 0, 0);
    try expectEngineMatches(&real);
    try std.testing.expectEqual(before_a + 128, editor.document.find(first.link_id).?.x);
    try std.testing.expectEqual(before_b + 128, editor.document.find(second.link_id).?.x);
    try std.testing.expectEqual(depth_at_multi + 1, editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try expectEngineMatches(&real);
    try std.testing.expectEqual(before_a, editor.document.find(first.link_id).?.x);
    try std.testing.expectEqual(before_b, editor.document.find(second.link_id).?.x);
    _ = try editor.redo();
    try expectEngineMatches(&real);
    try std.testing.expectEqual(before_a + 128, editor.document.find(first.link_id).?.x);
    std.debug.print("map-editor-engine: M3 multi-select group move ok (links {d} and {d})\n", .{ first.link_id, second.link_id });
    editor.clearSelection();

    // M3 (05-05, D-30): players and the unit creation end to end through the
    // core on the real engine. A player is added before the neutral and the
    // document follows the bridge (the table and every owner), a unit-creation
    // edit - a party and an aircraft taken from the bridge's own choices - is
    // one undo step that puts the exact old vector back, and a delete re-owns
    // the player's objects, with undo and redo keeping the document, the
    // engine and the map in step.
    {
        const entries_at_start = editor.document.diplomacy.items.len;
        const depth_at_players = editor.history.undo_stack.items.len;
        try editor.addPlayer(1);
        try std.testing.expectEqual(entries_at_start + 1, editor.document.diplomacy.items.len);
        try expectEngineMatches(&real);
        try expectDocumentIsBridge(&real, &editor);
        try std.testing.expectEqual(depth_at_players + 1, editor.history.undo_stack.items.len);
        _ = try editor.undo();
        try std.testing.expectEqual(entries_at_start, editor.document.diplomacy.items.len);
        try expectDocumentIsBridge(&real, &editor);
        _ = try editor.redo();
        try std.testing.expectEqual(entries_at_start + 1, editor.document.diplomacy.items.len);
        try expectEngineMatches(&real);

        // The choices the combos offer, and a unit-creation put from them.
        var party_names: [64]core.bridge.UcName = undefined;
        var aircraft_names: [512]core.bridge.UcName = undefined;
        var total: usize = 0;
        try core.bridge.check(real.bridge().unitCreationChoices(.parties, &party_names, &total));
        const party_total = total;
        try std.testing.expect(party_total > 0);
        try core.bridge.check(real.bridge().unitCreationChoices(.aircraft, &aircraft_names, &total));
        try std.testing.expect(total > 0);
        var unit = try editor.unitCreation(0);
        const old_relax = unit.relax_time;
        unit.relax_time = old_relax + 11;
        unit.setParty(party_names[party_total - 1].nameSlice());
        unit.aircraft[2].setName(aircraft_names[0].nameSlice());
        unit.aircraft[2].formation_size = 3;
        try editor.editUnitCreation(0, unit, 0);
        const edited = try editor.unitCreation(0);
        try std.testing.expectEqual(old_relax + 11, edited.relax_time);
        try std.testing.expectEqualStrings(party_names[party_total - 1].nameSlice(), edited.partySlice());
        // A party the data does not list is refused naming it, and nothing changes.
        var bad = edited;
        bad.setParty("Narnia");
        try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "partys.xml") != null);
        try std.testing.expect(edited.eql(try editor.unitCreation(0)));
        _ = try editor.undo();
        try std.testing.expectEqual(old_relax, (try editor.unitCreation(0)).relax_time);
        _ = try editor.redo();
        try std.testing.expectEqual(old_relax + 11, (try editor.unitCreation(0)).relax_time);
        _ = try editor.undo();

        // Delete player 0: its objects become the neutral's, the table shrinks.
        const entries_before_delete = editor.document.diplomacy.items.len;
        var owned_by_zero: usize = 0;
        for (editor.document.objects.items) |object| {
            if (object.player == 0) owned_by_zero += 1;
        }
        try editor.deletePlayer(0);
        try std.testing.expectEqual(entries_before_delete - 1, editor.document.diplomacy.items.len);
        const neutral: i32 = @intCast(editor.document.diplomacy.items.len - 1);
        var now_neutral: usize = 0;
        for (editor.document.objects.items) |object| {
            if (object.player == neutral) now_neutral += 1;
        }
        try std.testing.expect(now_neutral >= owned_by_zero);
        try expectEngineMatches(&real);
        try expectDocumentIsBridge(&real, &editor);
        _ = try editor.undo();
        try std.testing.expectEqual(entries_before_delete, editor.document.diplomacy.items.len);
        try expectDocumentIsBridge(&real, &editor);
        _ = try editor.undo(); // the add
        try std.testing.expectEqual(entries_at_start, editor.document.diplomacy.items.len);
        try std.testing.expectEqual(depth_at_players, editor.history.undo_stack.items.len);
        try expectEngineMatches(&real);
        try expectDocumentIsBridge(&real, &editor);
        std.debug.print("map-editor-engine: M3 players and unit creation round trip ok\n", .{});
    }

    // M2 (04-09): reinforcement groups through the generic record path on the
    // real engine (readGroup's two passes, insert refusing a taken ID, delete
    // and undo). Read back through the raw C ABI, not RealBridge.
    var groups_before: c_int = -1;
    const groups_sizing = c.BkEditorGroupIDs(real.session, null, 0, &groups_before);
    try std.testing.expect(groups_sizing == c.BK_EDITOR_OK or groups_sizing == c.BK_EDITOR_REFUSED);
    try std.testing.expect(groups_before >= 0);
    const depth_at_groups = editor.history.undo_stack.items.len;
    const created = try editor.newGroup(0);
    try editor.addScriptIDToGroup(created, 5000);
    try editor.addScriptIDToGroup(created, 5001);
    try editor.removeScriptIDFromGroup(created, 5000);
    try editor.addScriptIDToGroup(created, 5001); // a duplicate: a note, no step
    try std.testing.expectEqual(depth_at_groups + 4, editor.history.undo_stack.items.len);
    {
        var ids: [4]c_int = @splat(-77);
        var count: c_int = -1;
        try std.testing.expect(c.BkEditorGroup(real.session, created, &ids, 4, &count) == c.BK_EDITOR_OK);
        try std.testing.expectEqual(@as(c_int, 1), count);
        try std.testing.expectEqual(@as(c_int, 5001), ids[0]);
        try std.testing.expectEqual(@as(c_int, -77), ids[1]);
    }
    const listed = try editor.groupIDs(std.testing.allocator);
    defer std.testing.allocator.free(listed);
    try std.testing.expectEqual(@as(usize, @intCast(groups_before + 1)), listed.len);
    try std.testing.expect(std.mem.indexOfScalar(i32, listed, created) != null);
    const clash: core.records.Value = .{ .group = .{ .id = created } };
    try std.testing.expectError(error.Refused, editor.addRecord(.group, created, &clash));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "already") != null);
    try std.testing.expectError(error.Refused, editor.addScriptIDToGroup(created, -1));
    try editor.deleteGroup(created);
    try std.testing.expectError(error.Refused, editor.deleteGroup(created));
    _ = try editor.undo(); // the delete: the group and its script ID are back
    {
        const back = try editor.groupScriptIDs(std.testing.allocator, created);
        defer std.testing.allocator.free(back);
        try std.testing.expectEqualSlices(i32, &.{5001}, back);
    }
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    var groups_after: c_int = -1;
    _ = c.BkEditorGroupIDs(real.session, null, 0, &groups_after);
    try std.testing.expectEqual(groups_before, groups_after);
    try expectEngineMatches(&real);
    std.debug.print("map-editor-engine: M2 groups round trip ok\n", .{});

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

    // M2 (04-11): a start command end to end on the real engine. The first unit the
    // bridge accepts (a building or a tree is refused, changing nothing) gets one of
    // the default type; the raw read and the editor's list agree, undo, redo and undo
    // walk it back and forth, and deleting the unit takes the command with it (the
    // cascade moves the generation) while the undo of that delete brings it back.
    const commands_at_open = try startCommandCount(&editor);
    const commanded = pick: for (editor.document.objects.items) |candidate| {
        if (!candidate.known or candidate.link_id <= 0) continue;
        _ = editor.addStartCommand(candidate.link_id) catch |err| switch (err) {
            error.Refused => continue,
            else => return err,
        };
        break :pick candidate.link_id;
    } else return error.NoUnitTakesAStartCommand;
    {
        var count: c_int = -1;
        try std.testing.expect(c.BkEditorStartCommandCount(real.session, &count) == c.BK_EDITOR_OK);
        try std.testing.expectEqual(@as(c_int, @intCast(commands_at_open + 1)), count);
        var record: c.BkEditorStartCommandRecord = undefined;
        var units: [4]c_int = @splat(-77);
        try std.testing.expect(c.BkEditorStartCommand(real.session, @intCast(commands_at_open), &record, &units, 4) == c.BK_EDITOR_OK);
        try std.testing.expectEqual(@as(c_int, 9), record.cmd_type);
        try std.testing.expectEqual(@as(c_int, 1), record.unit_count);
        try std.testing.expectEqual(@as(c_int, commanded), units[0]);
        try std.testing.expectEqual(@as(c_int, 0), record.link_id);
    }
    try std.testing.expect(editor.dirty());
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    _ = try editor.redo();
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    _ = try editor.redo();
    const generation_before_delete = editor.record_generations.get(.start_command);
    // WR-C09: the delete must go through, so the cascade checks always run.
    try editor.delete(commanded);
    try std.testing.expect(editor.document.find(commanded) == null);
    try std.testing.expect(editor.record_generations.get(.start_command) != generation_before_delete);
    try std.testing.expectEqual(commands_at_open, try startCommandCount(&editor));
    _ = try editor.undo();
    try std.testing.expectEqual(commands_at_open + 1, try startCommandCount(&editor));
    _ = try editor.undo();
    try std.testing.expect(!editor.dirty());
    try expectEngineMatches(&real);
    try expectDocumentIsBridge(&real, &editor);
    std.debug.print("map-editor-engine: M2 start command round trip ok\n", .{});

    // M2 (04-12): an AI general parcel end to end on the real engine. A defence parcel goes
    // on side 1 (coldwinter has two sides) and another on a side two above the side count,
    // which creates the side between them empty; the raw read and the editor agree, undo,
    // redo and undo walk each back and forth, the side count coming back exactly, and a
    // mobile script ID goes in and out the same way.
    {
        var map_x: f32 = 0;
        var map_y: f32 = 0;
        try std.testing.expect(editor.bridge.worldToMap(middle_x, middle_y, &map_x, &map_y) == .ok);
        const sides_at_open = try editor.aiSideCount();
        const first_side: usize = 1;
        const parcels_before = (try rawAiSideInfo(&real, first_side)).parcel_count;
        const index = try editor.addDefenceParcel(first_side, map_x, map_y);
        try std.testing.expectEqual(@as(usize, @intCast(parcels_before)), index);
        {
            const info = try rawAiSideInfo(&real, first_side);
            try std.testing.expectEqual(parcels_before + 1, info.parcel_count);
            var parcels: [8]c.BkEditorAIParcel = undefined;
            var points: [1]c.BkEditorAIPoint = undefined;
            var mobile: [1]c_int = undefined;
            try std.testing.expect(info.parcel_count <= 8);
            var again: c.BkEditorAISideInfo = std.mem.zeroes(c.BkEditorAISideInfo);
            const read = c.BkEditorAIGeneralSide(real.session, @intCast(first_side), &again, &mobile, 0, &parcels, 8, &points, 0);
            // WR-C09: OK, or REFUSED only as the sizing answer for the mobile IDs
            // or points left out here - the parcels themselves were written.
            try std.testing.expect(read == c.BK_EDITOR_OK or (read == c.BK_EDITOR_REFUSED and (again.mobile_count > 0 or again.point_count > 0)));
            try std.testing.expectEqual(info.parcel_count, again.parcel_count);
            try std.testing.expect(index < @as(usize, @intCast(again.parcel_count)));
            const mine = parcels[index];
            try std.testing.expectEqual(@as(c_int, 1), mine.type);
            try std.testing.expectEqual(@as(f32, 256), mine.radius);
            try std.testing.expectEqual(@as(c_int, 0), mine.defence_dir);
            try std.testing.expectEqual(@as(c_int, 0), mine.point_count);
        }
        try std.testing.expect(editor.dirty());
        _ = try editor.undo();
        try std.testing.expectEqual(parcels_before, (try rawAiSideInfo(&real, first_side)).parcel_count);
        try std.testing.expect(!editor.dirty());
        _ = try editor.redo();
        try std.testing.expectEqual(parcels_before + 1, (try rawAiSideInfo(&real, first_side)).parcel_count);
        _ = try editor.undo();
        try std.testing.expect(!editor.dirty());
        // A side two above the count: the side between is created empty, and undo takes both away.
        const far_side = sides_at_open + 1;
        _ = try editor.addDefenceParcel(far_side, map_x, map_y);
        try std.testing.expectEqual(sides_at_open + 2, try editor.aiSideCount());
        try std.testing.expectEqual(@as(c_int, 0), (try rawAiSideInfo(&real, sides_at_open)).parcel_count);
        try std.testing.expectEqual(@as(c_int, 1), (try rawAiSideInfo(&real, far_side)).parcel_count);
        _ = try editor.undo();
        try std.testing.expectEqual(sides_at_open, try editor.aiSideCount());
        try std.testing.expect(!editor.dirty());
        _ = try editor.redo();
        try std.testing.expectEqual(sides_at_open + 2, try editor.aiSideCount());
        _ = try editor.undo();
        try std.testing.expectEqual(sides_at_open, try editor.aiSideCount());
        // A mobile script ID in and out, a duplicate a note.
        const depth = editor.history.undo_stack.items.len;
        try editor.addMobileScriptID(first_side, 4245);
        try std.testing.expectEqual(@as(c_int, 1), (try rawAiSideInfo(&real, first_side)).mobile_count);
        try editor.addMobileScriptID(first_side, 4245);
        try std.testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
        try editor.removeMobileScriptID(first_side, 4245);
        try std.testing.expectEqual(@as(c_int, 0), (try rawAiSideInfo(&real, first_side)).mobile_count);
        _ = try editor.undo();
        _ = try editor.undo();
        try std.testing.expect(!editor.dirty());
        try expectEngineMatches(&real);
        try expectDocumentIsBridge(&real, &editor);
        std.debug.print("map-editor-engine: M2 ai general round trip ok\n", .{});
    }

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

    // M3 (05-01, D-19): altitudes end to end on the real engine. A ramp goes
    // in through the core Editor's setAltitudes - the bridge sets the
    // heights, recomputes the shades over the shade-kernel-grown region and
    // pushes the covering patches - and the engine agrees with the map after
    // every step; the calls of one gesture are one undo step, undo puts the
    // recorded region back raw and redo reapplies it, and the undo-all at the
    // end leaves the map not dirty.
    {
        const region: core.bridge.AltitudeRegion = .{ .x0 = 8, .y0 = 8, .x1 = 16, .y1 = 16 };
        const width: usize = @intCast(region.x1 - region.x0);
        const area = width * @as(usize, @intCast(region.y1 - region.y0));
        var heights_before: [64]f32 = undefined;
        var total: usize = 0;
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.altitudes(region, &heights_before, &total));
        try std.testing.expectEqual(area, total);
        var ramp: [64]f32 = undefined;
        for (&ramp, 0..) |*height, i| height.* = @floatFromInt(i * 8);
        const altitude_gesture = editor.beginGesture();
        const altitudes_seen = editor.altitudes_generation;
        try editor.setAltitudes(region, &ramp, altitude_gesture);
        try editor.setAltitudes(region, &ramp, altitude_gesture); // the same gesture: one undo step
        try std.testing.expectEqual(altitudes_seen + 2, editor.altitudes_generation);
        try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
        try expectEngineMatches(&real);
        try std.testing.expect(editor.dirty());
        var after: [64]f32 = undefined;
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.altitudes(region, &after, &total));
        for (after, ramp) |got, want| try std.testing.expectEqual(want, got);
        // The shades moved with the heights: the engine agrees with the map
        // (expectEngineMatches above) and undo lands byte-raw.
        _ = try editor.undo();
        try expectEngineMatches(&real);
        try std.testing.expect(!editor.dirty());
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.altitudes(region, &after, &total));
        for (after, heights_before) |got, want| try std.testing.expectEqual(want, got);
        _ = try editor.redo();
        try expectEngineMatches(&real);
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.altitudes(region, &after, &total));
        for (after, ramp) |got, want| try std.testing.expectEqual(want, got);
        while (try editor.undo()) {}
        try std.testing.expect(!editor.dirty());
        try expectEngineMatches(&real);
        try expectDocumentIsBridge(&real, &editor);
        std.debug.print("map-editor-engine: M3 altitudes round trip ok\n", .{});
    }

    // M3 (05-07, D-14/D-15/D-16): the minimap on the real engine. The reads
    // come through the vtable the panel uses (two-pass, never past the count the
    // sizing call gave), and a click moves the real camera by the MFC's rule: the
    // picture's point to the world, plus the anchor's distance from what is under
    // the screen's centre, held to the map.
    {
        const info = editor.document.info;
        const width: usize = @intCast(info.width_tiles);
        const height: usize = @intCast(info.height_tiles);
        const bridge = editor.bridge;
        var total: usize = 0;
        const region: core.bridge.TileRegion = .{ .x0 = 0, .y0 = 0, .x1 = info.width_tiles, .y1 = info.height_tiles };
        var none_tiles: [0]u8 = .{};
        try std.testing.expectEqual(core.bridge.Status.refused, bridge.tiles(region, &none_tiles, &total));
        try std.testing.expectEqual(width * height, total);
        const tiles = try std.testing.allocator.alloc(u8, total);
        defer std.testing.allocator.free(tiles);
        try std.testing.expectEqual(core.bridge.Status.ok, bridge.tiles(region, tiles, &total));
        var cell_y: usize = 0;
        while (cell_y < height) : (cell_y += 41) {
            var cell_x: usize = 0;
            while (cell_x < width) : (cell_x += 37) {
                try std.testing.expectEqual(try engineTile(&real, @intCast(cell_x), @intCast(cell_y)), tiles[cell_y * width + cell_x]);
            }
        }
        var colors: [256]u32 = undefined;
        try std.testing.expectEqual(core.bridge.Status.ok, bridge.minimapTileColors(&colors, &total));
        try std.testing.expect(total > 0 and total <= 256);
        for (tiles) |tile_index| try std.testing.expect(tile_index < total);
        var none_units: [0]core.bridge.MinimapUnit = .{};
        try std.testing.expectEqual(core.bridge.Status.refused, bridge.minimapUnits(&none_units, &total));
        try std.testing.expect(total > 0);
        const units = try std.testing.allocator.alloc(core.bridge.MinimapUnit, total);
        defer std.testing.allocator.free(units);
        const want_units = total;
        try std.testing.expectEqual(core.bridge.Status.ok, bridge.minimapUnits(units, &total));
        try std.testing.expectEqual(want_units, total);
        var none_areas: [0]core.bridge.MinimapArea = .{};
        try std.testing.expectEqual(core.bridge.Status.ok, bridge.minimapAreas(&none_areas, &total));
        try std.testing.expectEqual(@as(usize, 0), total);

        // Game mode's picture: coldwinter ships its _h.dds.
        const buffer = try std.testing.allocator.alloc(u8, 1024 * 1024 * 4);
        defer std.testing.allocator.free(buffer);
        const picture = real.minimapImage(editor.document.path.items, buffer, 1024) orelse return error.NoMinimapPicture;
        try std.testing.expect(picture.width > 0 and picture.height > 0);
        try std.testing.expect(real.minimapImage("Data\\Maps\\Multiplayer\\no_such_map.bzm", buffer, 1024) == null);

        // The click: two points of a 256 x 256 picture, the camera where the rule says.
        const minimap_clicks = [2][2]f32{ .{ 60, 70 }, .{ 190, 40 } };
        var previous: [2]f32 = .{ -1, -1 };
        for (minimap_clicks) |click| {
            const world = logic.minimapToWorld(click[0], click[1], 256, 256, info.width_tiles, info.height_tiles);
            const screen = real.screenSize() orelse return error.NoScreen;
            const view_before = real.viewState() orelse return error.NoView;
            const centre = try editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0);
            const target = logic.minimapCameraTarget(world, .{ view_before.anchor_x, view_before.anchor_y }, .{ centre.world_x, centre.world_y }, .{ .width_tiles = info.width_tiles, .height_tiles = info.height_tiles });
            try std.testing.expectEqual(core.bridge.Status.ok, real.setCamera(target[0], target[1]));
            // The projection follows the camera on the next frame the engine draws.
            _ = c.BkEditorFrame(real.session);
            const view_after = real.viewState() orelse return error.NoView;
            // The point of the rule: the clicked point is now at the middle of the screen
            // (to the projection's own rounding - the engine's anchor comes back a couple
            // of world units off what was set), whatever the anchor and the centre were.
            const centre_after = try editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0);
            std.debug.print("map-editor-engine: minimap click {d},{d}: world {d:.2},{d:.2}; camera set {d:.2},{d:.2}, the middle of the screen is now {d:.2},{d:.2}\n", .{ click[0], click[1], world[0], world[1], target[0], target[1], centre_after.world_x, centre_after.world_y });
            try std.testing.expectApproxEqAbs(world[0], centre_after.world_x, 3.0);
            try std.testing.expectApproxEqAbs(world[1], centre_after.world_y, 3.0);
            try std.testing.expect(view_after.anchor_x != previous[0] or view_after.anchor_y != previous[1]);
            previous = .{ view_after.anchor_x, view_after.anchor_y };
        }
        // The Heights minimap's red: the app's port of the engine's validity rule over the
        // sheet the panel reads. The bridge tier counts the same map's refused vertices with
        // the engine's own function (`editor-bridge: M3 height rule: N of M vertices ...`);
        // the two counts are the same number.
        {
            const vertex_w: usize = width + 1;
            const vertex_h: usize = height + 1;
            const sheet = try std.testing.allocator.alloc(f32, vertex_w * vertex_h);
            defer std.testing.allocator.free(sheet);
            const vertices: core.bridge.AltitudeRegion = .{ .x0 = 0, .y0 = 0, .x1 = @intCast(vertex_w), .y1 = @intCast(vertex_h) };
            var got: usize = 0;
            try std.testing.expectEqual(core.bridge.Status.ok, bridge.altitudes(vertices, sheet, &got));
            try std.testing.expectEqual(sheet.len, got);
            var refused: usize = 0;
            for (0..vertex_h) |y| for (0..vertex_w) |x| {
                if (!logic.isValidHeight(sheet, vertex_w, vertex_h, x, y)) refused += 1;
            };
            std.debug.print("map-editor-engine: M3 height rule: {d} of {d} vertices of the shipped map refused\n", .{ refused, sheet.len });
            try std.testing.expectEqual(shipped_map_refused_vertices, refused);
        }
        try std.testing.expect(!editor.dirty());
        std.debug.print("map-editor-engine: M3 minimap reads and click round trip ok\n", .{});
    }

    // M3 (05-08, D-01/D-13): the Create Random Map and Export lists reads cross the
    // ABI. The storage listing keeps the extension and the folder's own names, the
    // template's graphs come with their weights, a refusal names its field and
    // writes nothing (a real generation writes into the user's maps folder, which
    // this process has no scratch root for - the bridge tier does it with one).
    {
        var total: usize = 0;
        _ = editor.bridge.listStorageFiles("scenarios\\chapters\\", "context.xml", &.{}, &total);
        try std.testing.expect(total > 0);
        const contexts = try std.testing.allocator.alloc(core.bridge.RmgName, total);
        defer std.testing.allocator.free(contexts);
        var got: usize = 0;
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.listStorageFiles("scenarios\\chapters\\", "context.xml", contexts, &got));
        try std.testing.expectEqual(total, got);
        for (contexts) |context| try std.testing.expect(std.mem.endsWith(u8, context.nameSlice(), "\\context.xml"));
        for (contexts[1..], 1..) |context, i| try std.testing.expect(std.mem.order(u8, contexts[i - 1].nameSlice(), context.nameSlice()) == .lt);
        try std.testing.expectEqual(core.bridge.Status.refused, editor.bridge.listStorageFiles("..\\", ".xml", &.{}, &total));
        try std.testing.expectEqual(@as(usize, 0), total);

        var templates: usize = 0;
        _ = editor.bridge.listRmg(.templates, &.{}, &templates);
        try std.testing.expect(templates > 0);
        var graphs_total: usize = 0;
        _ = editor.bridge.rmgTemplateGraphs("scenarios\\templates\\summer\\template02", &.{}, &graphs_total);
        try std.testing.expect(graphs_total > 0);
        const graphs = try std.testing.allocator.alloc(core.bridge.RmgGraph, graphs_total);
        defer std.testing.allocator.free(graphs);
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.rmgTemplateGraphs("scenarios\\templates\\summer\\template02", graphs, &got));
        try std.testing.expectEqual(graphs_total, got);
        for (graphs) |graph| {
            try std.testing.expect(graph.weight > 0);
            try std.testing.expect(std.mem.startsWith(u8, graph.nameSlice(), "scenarios\\graphs\\"));
        }
        try std.testing.expectEqual(core.bridge.Status.refused, editor.bridge.rmgTemplateGraphs("scenarios\\templates\\summer\\nope", &.{}, &graphs_total));

        var params: core.bridge.RmgGenerateParams = .{};
        params.setTemplate("scenarios\\templates\\summer\\nope");
        params.setContext(contexts[0].nameSlice()[0 .. contexts[0].nameSlice().len - ".xml".len]);
        params.setMapName("c_bridge_never_written");
        var result: core.bridge.RmgGenerateResult = .{};
        try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "template") != null);
        try std.testing.expect(!editor.dirty());
        std.debug.print("map-editor-engine: M3 random map reads round trip ok\n", .{});
    }

    // M3 (05-09, D-06/D-07): the composers' records cross the ABI. A shipped
    // container and graph read in two passes into the core's owned types, the
    // patch map they list reports its own facts, Check! on them is clean (the
    // shipped data is consistent), and a write to a shipped name is refused
    // with the Save-As message and writes nothing (a real write lands in the
    // user's own folder, which this process has no scratch root for - the bridge
    // tier does that with one).
    {
        var total: usize = 0;
        _ = editor.bridge.listRmg(.containers, &.{}, &total);
        try std.testing.expect(total > 100);
        const names = try std.testing.allocator.alloc(core.bridge.RmgName, total);
        defer std.testing.allocator.free(names);
        var got: usize = 0;
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.listRmg(.containers, names, &got));
        // The first container with patches.
        var container: ?core.rmg.Container = null;
        defer if (container) |*owned| owned.deinit(std.testing.allocator);
        var container_name: []const u8 = "";
        for (names) |listed_name| {
            var candidate = try editor.readContainer(listed_name.nameSlice());
            if (candidate.patchCount() >= 2) {
                container = candidate;
                container_name = listed_name.nameSlice();
                break;
            }
            candidate.deinit(std.testing.allocator);
        }
        const read = container orelse return error.NoContainerWithPatches;
        std.debug.print("map-editor-engine: composer container {s}: {d} patches, size {d}x{d}, {d} script IDs\n", .{ container_name, read.patchCount(), read.size_x, read.size_y, read.script_ids.items.len });
        try std.testing.expect(read.size_x > 0 and read.size_y > 0);
        for (read.indices) |direction_list| for (direction_list.items) |patch_index| try std.testing.expect(patch_index >= 0 and patch_index < read.patchCount());
        // A shipped container is consistent: Check! through the real storages is clean.
        var report = try core.rmg.checkContainer(std.testing.allocator, &read, editor.rmgSource());
        defer report.deinit(std.testing.allocator);
        for (report.findings.items) |finding| std.debug.print("map-editor-engine: composer container finding: {s}\n", .{finding.text});
        try std.testing.expectEqual(@as(usize, 0), report.errorCount());
        // Shipped is read-only.
        try std.testing.expectError(error.Refused, editor.writeContainer(container_name, &read));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "Save As") != null);
        try std.testing.expectError(error.Refused, editor.readContainer("scenarios\\containers\\nope\\nothing"));
        // The graphs: the scan finds them under scenarios\graphs now.
        var graph_total: usize = 0;
        _ = editor.bridge.listRmg(.graphs, &.{}, &graph_total);
        try std.testing.expect(graph_total >= 100);
        const graph_names = try std.testing.allocator.alloc(core.bridge.RmgName, graph_total);
        defer std.testing.allocator.free(graph_names);
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.listRmg(.graphs, graph_names, &got));
        var graph = try editor.readGraph(graph_names[0].nameSlice());
        defer graph.deinit(std.testing.allocator);
        try std.testing.expect(graph.nodes.items.len > 0 and graph.size_x > 0);
        var graph_report = try core.rmg.checkGraph(std.testing.allocator, &graph, editor.rmgSource());
        defer graph_report.deinit(std.testing.allocator);
        for (graph_report.findings.items) |finding| std.debug.print("map-editor-engine: composer graph finding: {s}\n", .{finding.text});
        try std.testing.expectError(error.Refused, editor.writeGraph(graph_names[0].nameSlice(), &graph));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "Save As") != null);
        var buffer: [1024]u8 = undefined;
        const root = try editor.rmgRoot(&buffer);
        try std.testing.expect(std.mem.endsWith(u8, root, "rmg"));
        try std.testing.expect(!editor.dirty());
        std.debug.print("map-editor-engine: M3 rmg composer reads round trip ok\n", .{});
    }

    // M3 (05-10, D-06/D-07/D-12): the Fields Composer's records on the real
    // engine. Every shipped field set reads in two passes into the core's owned
    // type, equals itself after a clone, and the Check! rules - the tileset's
    // terrain types, the profile in the storages, the objects of the catalogue -
    // run through the real bridge. A crafted set with a tile past the tileset
    // and an object nobody knows is found, and Fix all removes exactly those.
    {
        var total: usize = 0;
        _ = editor.bridge.listRmg(.field_sets, &.{}, &total);
        try std.testing.expect(total >= 20);
        const names = try std.testing.allocator.alloc(core.bridge.RmgName, total);
        defer std.testing.allocator.free(names);
        var got: usize = 0;
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.listRmg(.field_sets, names, &got));
        // The object catalogue the app's State holds, as the Check! asks it.
        const known = struct {
            fn has(ctx: *anyopaque, name: []const u8) bool {
                const entries: *const []c.BkEditorCatalogueEntry = @ptrCast(@alignCast(ctx));
                for (entries.*) |*cat_item| if (std.mem.eql(u8, std.mem.sliceTo(&cat_item.name, 0), name)) return true;
                return false;
            }
        };
        var catalogue_view: []c.BkEditorCatalogueEntry = catalogue;
        var composers = core.composers.Composers.init(std.testing.allocator);
        defer composers.deinit();
        composers.object_lookup = .{ .ctx = @ptrCast(&catalogue_view), .has_fn = known.has };
        var shells: usize = 0;
        var errors: usize = 0;
        for (names[0..got]) |listed_name| {
            try composers.openField(&editor, listed_name.nameSlice());
            shells += composers.fdoc.current.tile_shells.items.len + composers.fdoc.current.object_shells.items.len;
            var copy = try composers.fdoc.current.clone(std.testing.allocator);
            defer copy.deinit(std.testing.allocator);
            try std.testing.expect(copy.eql(&composers.fdoc.current));
            _ = try composers.checkField(&editor);
            const report = &composers.field_report.?;
            errors += report.errorCount();
            for (report.findings.items) |finding| std.debug.print("map-editor-engine: field set {s}: {s}\n", .{ listed_name.nameSlice(), finding.text });
        }
        std.debug.print("map-editor-engine: {d} shipped field sets read ({d} shells), Check! found {d} errors\n", .{ got, shells, errors });
        try std.testing.expect(shells > got);
        try std.testing.expectEqual(@as(usize, 0), errors);
        // The terrain types of the four seasons come through the ABI, and the profile probe agrees.
        for (0..4) |season| {
            const types = try editor.tilesetTypes(std.testing.allocator, season);
            defer std.testing.allocator.free(types);
            try std.testing.expect(types.len > 5);
        }
        try std.testing.expect(editor.rmgFileExists("scenarios\\profiles\\profile", ".tga"));
        try std.testing.expect(editor.rmgFileExists("\\Scenarios\\Profiles\\Profile", ".tga"));
        try std.testing.expect(!editor.rmgFileExists("scenarios\\profiles\\nothing_here", ".tga"));
        // A crafted set: one tile past the tileset, one object nobody knows.
        try composers.openField(&editor, names[0].nameSlice());
        const field = try composers.fdoc.begin();
        const shell = try field.addTileShell(std.testing.allocator);
        try std.testing.expect(try field.addTile(std.testing.allocator, shell, 3));
        try std.testing.expect(try field.addTile(std.testing.allocator, shell, 4000));
        const oshell = try field.addObjectShell(std.testing.allocator);
        try std.testing.expect(try field.addObject(std.testing.allocator, oshell, "NoSuchObjectAnywhere"));
        _ = try composers.checkField(&editor);
        try std.testing.expectEqual(@as(usize, 2), composers.field_report.?.errorCount());
        try std.testing.expectEqual(@as(usize, 2), try composers.fixFieldAll(&editor));
        try std.testing.expectEqual(@as(usize, 0), composers.field_report.?.errorCount());
        try std.testing.expectError(error.Refused, editor.writeFieldSet(names[0].nameSlice(), &composers.fdoc.current));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "Save As") != null);
        try std.testing.expect(!editor.dirty());
        std.debug.print("map-editor-engine: M3 rmg field sets round trip ok\n", .{});
    }

    // M3 (05-10, D-06/D-07/D-12): the Templates Composer's records on the real
    // engine. Every shipped template reads in two passes into the core's owned
    // type, clones equal, and the template Check! - the MFC's empty button, here the
    // graph, field set and vso rules over everything it lists - runs through the
    // real bridge. A crafted template listing a graph and a field set that are not
    // there is found, and Fix all removes exactly those.
    {
        var total: usize = 0;
        _ = editor.bridge.listRmg(.templates, &.{}, &total);
        try std.testing.expect(total >= 40);
        const names = try std.testing.allocator.alloc(core.bridge.RmgName, total);
        defer std.testing.allocator.free(names);
        var got: usize = 0;
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.listRmg(.templates, names, &got));
        const known = struct {
            fn has(ctx: *anyopaque, name: []const u8) bool {
                const entries: *const []c.BkEditorCatalogueEntry = @ptrCast(@alignCast(ctx));
                for (entries.*) |*cat_item| if (std.mem.eql(u8, std.mem.sliceTo(&cat_item.name, 0), name)) return true;
                return false;
            }
        };
        var catalogue_view: []c.BkEditorCatalogueEntry = catalogue;
        var composers = core.composers.Composers.init(std.testing.allocator);
        defer composers.deinit();
        composers.object_lookup = .{ .ctx = @ptrCast(&catalogue_view), .has_fn = known.has };
        var graphs: usize = 0;
        var errors: usize = 0;
        var nested_findings: usize = 0;
        for (names[0..got]) |listed_name| {
            try composers.openTemplate(&editor, listed_name.nameSlice());
            const t = &composers.tdoc.current;
            graphs += t.graphs.items.len;
            try std.testing.expect(t.diplomacies.items.len >= 3 and t.units.items.len == t.playerCount());
            var copy = try t.clone(std.testing.allocator);
            defer copy.deinit(std.testing.allocator);
            try std.testing.expect(copy.eql(t));
            _ = try composers.checkTemplate(&editor);
            const report = &composers.template_report.?;
            // The template's own rules (lists, weights, default field, vso, players,
            // header) hold for every shipped template. What the graph and field set rules
            // say about the graphs and field sets it lists is those composers' own
            // finding (shipped graphs hold links of fewer than eight parts, which the
            // Graphs Composer's Check! reports too): counted, not asserted.
            for (report.findings.items) |finding| {
                const nested = std.mem.startsWith(u8, finding.text, "graph ") and std.mem.indexOf(u8, finding.text, "\": ") != null or std.mem.startsWith(u8, finding.text, "field set ") and std.mem.indexOf(u8, finding.text, "\": ") != null;
                if (nested) {
                    nested_findings += 1;
                } else if (finding.severity == .@"error") {
                    errors += 1;
                    std.debug.print("map-editor-engine: template {s}: {s}\n", .{ listed_name.nameSlice(), finding.text });
                }
            }
        }
        std.debug.print("map-editor-engine: {d} shipped templates read ({d} graphs), the template's own Check! found {d} errors ({d} findings of its graphs' and field sets' rules)\n", .{ got, graphs, errors, nested_findings });
        try std.testing.expect(graphs > got);
        try std.testing.expectEqual(@as(usize, 0), errors);
        // A crafted one: a graph and a field set that are not in the data.
        try composers.openTemplate(&editor, names[0].nameSlice());
        const template = try composers.tdoc.begin();
        try template.graphs.append(std.testing.allocator, .{ .name = try std.testing.allocator.dupe(u8, "scenarios\\graphs\\summer\\no_such_graph"), .weight = 1 });
        try template.fields.append(std.testing.allocator, .{ .name = try std.testing.allocator.dupe(u8, "scenarios\\fieldsets\\summer\\no_such_field"), .weight = 1 });
        _ = try composers.checkTemplate(&editor);
        const ghosts = struct {
            fn count(report: *const core.rmg.Report) usize {
                var n: usize = 0;
                for (report.findings.items) |finding| {
                    if (std.mem.indexOf(u8, finding.text, "no_such_graph") != null or std.mem.indexOf(u8, finding.text, "no_such_field") != null) n += 1;
                }
                return n;
            }
        };
        try std.testing.expectEqual(@as(usize, 2), ghosts.count(&composers.template_report.?));
        try std.testing.expect(try composers.fixTemplateAll(&editor) >= 2);
        try std.testing.expectEqual(@as(usize, 0), ghosts.count(&composers.template_report.?));
        try std.testing.expectError(error.Refused, editor.writeTemplate(names[0].nameSlice(), &composers.tdoc.current));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "Save As") != null);
        try std.testing.expect(!editor.dirty());
        std.debug.print("map-editor-engine: M3 rmg templates round trip ok\n", .{});
    }

    // M3 (05-06, D-32): the Layers menu on the real engine, through the core
    // that remembers it. Three layers are toggled, the map is opened again, and
    // the renderer - read back from the bridge, not the editor's memory - is
    // what was asked: the MFC editor's check marks and the scene's own flags
    // drifted apart across an open. A layer the GPU renderer cannot draw is
    // refused with the state unchanged, and none of it is a map edit.
    {
        const layers = core.layers;
        try std.testing.expect(!editor.dirty());
        try std.testing.expect(editor.layerAvailable(.grid));
        try std.testing.expect(!editor.layerAvailable(.depth_complexity));
        try std.testing.expect(editor.layerAvailable(.wireframe));
        try editor.toggleLayer(.grid);
        try editor.toggleLayer(.bounding_boxes);
        try editor.toggleLayer(.terrain_noise);
        try editor.toggleLayer(.war_fog);
        try std.testing.expectError(error.Refused, editor.toggleLayer(.depth_complexity));
        var bits: u32 = 0;
        var mask: u32 = 0;
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.layers(&bits, &mask));
        try std.testing.expect(bits & layers.bit(.grid) != 0);
        try std.testing.expect(bits & layers.bit(.bounding_boxes) != 0);
        try std.testing.expect(bits & layers.bit(.terrain_noise) == 0);
        try std.testing.expect(bits & layers.bit(.war_fog) != 0);
        try std.testing.expect(bits & layers.bit(.depth_complexity) == 0);
        const chosen = bits;
        try expectEngineMatches(&real);
        // Close and open the same map again: the layers come back as chosen.
        try editor.open("Data\\Maps\\Multiplayer\\coldwinter.bzm");
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.layers(&bits, &mask));
        try std.testing.expectEqual(chosen, bits);
        try std.testing.expect(editor.layers.shown(.grid) and editor.layers.shown(.war_fog));
        try std.testing.expect(!editor.dirty());
        try expectEngineMatches(&real);
        // A new map too.
        try editor.newMap(.{ .size_x = 2, .size_y = 2, .season = 0 });
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.layers(&bits, &mask));
        try std.testing.expectEqual(chosen, bits);
        try editor.open("Data\\Maps\\Multiplayer\\coldwinter.bzm");
        // The editor's own memory is the truth the bridge is brought to: a state
        // remembered from settings before the open is what the open applies.
        var remembered: layers.State = .{};
        remembered.set(.shadows, false);
        remembered.set(.haze, false);
        editor.layers = remembered;
        try editor.open("Data\\Maps\\Multiplayer\\coldwinter.bzm");
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.layers(&bits, &mask));
        try std.testing.expectEqual(remembered.bitsFor(mask), bits & ~layers.bit(.fire_ranges));
        // Fire ranges: the selected units' ranges come and go with the selection, and
        // the mode is asked again after an open (the AI forgot its groups).
        try editor.loadFilters();
        try editor.setFireRange(.filter, "Buildings");
        try std.testing.expect(editor.layers.shown(.fire_ranges));
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.layers(&bits, &mask));
        try std.testing.expect(bits & layers.bit(.fire_ranges) != 0);
        try std.testing.expectError(error.Refused, editor.setFireRange(.filter, "No Such Filter"));
        try std.testing.expectEqualStrings("Buildings", editor.layers.fireFilter());
        try editor.open("Data\\Maps\\Multiplayer\\coldwinter.bzm");
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.layers(&bits, &mask));
        try std.testing.expect(bits & layers.bit(.fire_ranges) != 0);
        try editor.setFireRange(.off, "");
        try std.testing.expectEqual(core.bridge.Status.ok, editor.bridge.layers(&bits, &mask));
        try std.testing.expect(bits & layers.bit(.fire_ranges) == 0);
        // Put the renderer back for whatever runs after.
        editor.layers = .{};
        try editor.open("Data\\Maps\\Multiplayer\\coldwinter.bzm");
        try std.testing.expect(!editor.dirty());
        std.debug.print("map-editor-engine: M3 layers re-applied after open and new ok\n", .{});
    }

    std.debug.print("map-editor-engine: PASS ({d} objects)\n", .{objects_at_open});
}
