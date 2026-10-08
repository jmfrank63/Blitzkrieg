//! What the Terrain (.til) editor's thumbnail lists decide (06-PARITY B-18.1
//! to B-18.4, B-19.2, B-20.2), with no window and no ImGui: which list the
//! tree selection shows (TileTreeItem.cpp's SwitchToEditCrossetsMode), where
//! the lists read their pictures, which item a double-click adds a tile to,
//! and the two Import buttons. MFC's tileset frame hid its game window and
//! showed these lists instead of a scene, so the app does the same.
//! Runs under `zig build test-resource-app-logic` against the fake bridge.
const std = @import("std");
const core = @import("resource_core");
const logic = @import("panels_logic.zig");

const tools = core.sub_editor_tools;
const bridge = core.bridge;
const ResBridge = bridge.ResBridge;
const History = core.history.History;
const ResourceCommand = core.history.ResourceCommand;
const item = tools.item_type;
const testing = std.testing;

/// CTileSetFrame::SwitchToEditCrossetsMode's two states.
pub const Mode = enum { terrains, crossets };

/// The mode a selected tree item switches to, or null when the item leaves
/// the mode alone (the root and the common properties). Terrains, a
/// terrain's properties, its Tiles and a tile are the terrain side; Crossets,
/// a crosset's properties, its tiles and a crosset tile are the crosset side
/// (MyLButtonClick of CTileSetTerrainsItem, CCrossetsItem, CCrossetPropsItem
/// and the tiles items).
pub fn modeFor(doc: *const core.document.Document, selected: ?i32) ?Mode {
    const node = tools.findNode(doc, selected orelse return null) orelse return null;
    const class = std.fmt.parseInt(i32, node.classSlice(), 10) catch return null;
    return switch (class) {
        item.tileset_terrains, item.tileset_terrain_props, item.tileset_tile_props, item.tileset_tiles => .terrains,
        item.crossets, item.crosset_props, item.crosset_tiles, item.crosset_tile_props => .crossets,
        else => null,
    };
}

/// The folder the all-tiles list reads for `mode`: the project's own
/// `terrains` or `crossets` folder, where the importer and the exporter
/// keep the cut tiles. Null when the project has no folder yet.
pub fn listFolder(buffer: []u8, project_folder: ?[]const u8, mode: Mode) ?[]const u8 {
    const folder = project_folder orelse return null;
    if (folder.len == 0) return null;
    const sub = if (mode == .crossets) "crossets" else "terrains";
    return std.fmt.bufPrint(buffer, "{s}/{s}", .{ std.mem.trimEnd(u8, folder, "/\\"), sub }) catch null;
}

/// The lists' state: the mode the tree selection chose. One list widget
/// serves both modes, so it reads the other folder when the mode flips.
pub const Lists = struct {
    mode: Mode = .terrains,

    /// Follows the tree selection; true when the mode changed.
    pub fn follow(self: *Lists, doc: *const core.document.Document, selected: ?i32) bool {
        const next = modeFor(doc, selected) orelse return false;
        if (next == self.mode) return false;
        self.mode = next;
        return true;
    }
};

/// The item a double-click adds a tile to: the selected Tiles item or
/// crosset tiles item, a terrain's or crosset's properties item standing for
/// its Tiles child, a tile standing for its parent. Null when the selection
/// is none of these.
pub fn addTarget(doc: *const core.document.Document, selected: ?i32) ?struct { id: i32, mode: Mode } {
    const id = selected orelse return null;
    const node = tools.findNode(doc, id) orelse return null;
    const class = std.fmt.parseInt(i32, node.classSlice(), 10) catch return null;
    return switch (class) {
        item.tileset_tiles => .{ .id = id, .mode = .terrains },
        item.crosset_tiles => .{ .id = id, .mode = .crossets },
        item.tileset_terrain_props => .{ .id = tools.childOfClass(doc, id, item.tileset_tiles, 0) orelse return null, .mode = .terrains },
        item.crosset_props => .{ .id = tools.childOfClass(doc, id, item.crosset_tiles, 0) orelse return null, .mode = .crossets },
        item.tileset_tile_props => .{ .id = node.parent, .mode = .terrains },
        item.crosset_tile_props => .{ .id = node.parent, .mode = .crossets },
        else => null,
    };
}

/// The thumbnail double-click (WM_THUMB_LIST_DBLCLK): BkResTileSetAddTile on
/// the active item, recorded as one undo step. `list` is the list the
/// picture came from; a terrain picture does not go into a crosset. The
/// bridge's reason is the message of the Refused and BadArgument errors.
pub fn addTile(gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, selected: ?i32, list: Mode, picture: []const u8) bridge.EditError!void {
    if (!life.is_open or life.read_only) return error.Refused;
    const target = addTarget(&life.doc, selected) orelse return error.Refused;
    if (target.mode != list) return error.Refused;
    try life.history.reserve(gpa);
    var new_id: i32 = 0;
    try bridge.check(b.tileSetAddTile(target.id, picture, &new_id));
    try life.doc.reload(gpa, b);
    const node = tools.findNode(&life.doc, new_id) orelse return error.Failed;
    var index: i32 = 0;
    for (life.doc.tree.nodes.items) |sibling| {
        if (sibling.id == node.id) break;
        if (sibling.parent == node.parent) index += 1;
    }
    var command: ResourceCommand = .{ .insert_node = .{
        .parent = node.parent,
        .class_name = try gpa.dupe(u8, node.classSlice()),
        .index = index,
        .new_id = node.id,
        .name = try core.history.OwnedBytes.fromSlice(gpa, node.displaySlice()),
    } };
    errdefer command.deinit(gpa);
    life.history.recordAssumeCapacity(gpa, command, 0);
}

/// Import terrains / Import crossets (OnImportTerrains, OnImportCrossets):
/// the tile count the file cut. The whole terrain or crosset tree is
/// replaced, which no single command undoes, so the undo history ends here
/// and the project counts as unsaved until it is saved.
pub fn importFile(gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, path: []const u8, mode: Mode) bridge.EditError!i32 {
    if (!life.is_open or life.read_only) return error.Refused;
    var count: i32 = 0;
    try bridge.check(b.tileSetImport(path, mode == .crossets, &count));
    life.history.clear(gpa);
    try life.doc.reload(gpa, b);
    life.unsaved_import = true;
    return count;
}

// --- Tests ---------------------------------------------------------------------

const FakeResBridge = core.fake_bridge.FakeResBridge;

/// A fake .til with the tree MFC builds: Terrains > terrain props > Tiles,
/// and Crossets > crosset props > crosset tiles; the ids come back in the
/// order terrains, terrain, tiles, crossets, crosset, crosset tiles.
fn tileRig(fake: *FakeResBridge, life: *logic.Lifecycle) ![6]i32 {
    const b = fake.bridge();
    try life.newProject(testing.allocator, b, .tile_set);
    const root = fake.nodes.items[0].id;
    var ids: [6]i32 = undefined;
    const plan = [_]struct { parent: usize, class: i32 }{
        .{ .parent = 6, .class = item.tileset_terrains },
        .{ .parent = 0, .class = item.tileset_terrain_props },
        .{ .parent = 1, .class = item.tileset_tiles },
        .{ .parent = 6, .class = item.crossets },
        .{ .parent = 3, .class = item.crosset_props },
        .{ .parent = 4, .class = item.crosset_tiles },
    };
    for (plan, 0..) |step, i| {
        var name: [16]u8 = undefined;
        const parent = if (step.parent == 6) root else ids[step.parent];
        try bridge.check(b.insertNode(parent, try std.fmt.bufPrint(&name, "{d}", .{step.class}), 0, &ids[i]));
    }
    try life.doc.reload(testing.allocator, b);
    return ids;
}

test "crosset mode follows the selected item as SwitchToEditCrossetsMode does" {
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    var life: logic.Lifecycle = .{};
    defer life.deinit(testing.allocator);
    const ids = try tileRig(&fake, &life);
    const root = fake.nodes.items[0].id;

    // The terrain side: Terrains, a terrain, its Tiles.
    for ([_]i32{ ids[0], ids[1], ids[2] }) |id| try testing.expectEqual(Mode.terrains, modeFor(&life.doc, id).?);
    // The crosset side: Crossets, a crosset, its tiles.
    for ([_]i32{ ids[3], ids[4], ids[5] }) |id| try testing.expectEqual(Mode.crossets, modeFor(&life.doc, id).?);
    // The root and no selection leave the mode as it was.
    try testing.expect(modeFor(&life.doc, root) == null);
    try testing.expect(modeFor(&life.doc, null) == null);
    try testing.expect(modeFor(&life.doc, 9999) == null);

    var lists: Lists = .{};
    try testing.expect(!lists.follow(&life.doc, ids[0]));
    try testing.expect(lists.follow(&life.doc, ids[4]));
    try testing.expectEqual(Mode.crossets, lists.mode);
    try testing.expect(!lists.follow(&life.doc, root));
    try testing.expectEqual(Mode.crossets, lists.mode);
    try testing.expect(lists.follow(&life.doc, ids[2]));
    try testing.expectEqual(Mode.terrains, lists.mode);
}

test "the all-tiles list reads the project's terrains or crossets folder" {
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("/p/til/terrains", listFolder(&buffer, "/p/til", .terrains).?);
    try testing.expectEqualStrings("/p/til/crossets", listFolder(&buffer, "/p/til/", .crossets).?);
    try testing.expect(listFolder(&buffer, null, .terrains) == null);
    try testing.expect(listFolder(&buffer, "", .terrains) == null);
    var tiny: [4]u8 = undefined;
    try testing.expect(listFolder(&tiny, "/p/til", .terrains) == null);
}

test "a double-click adds the tile to the active item, undoably, and names why it cannot" {
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    var life: logic.Lifecycle = .{};
    defer life.deinit(testing.allocator);
    const ids = try tileRig(&fake, &life);
    const root = fake.nodes.items[0].id;
    const b = fake.bridge();

    // The target for each selection.
    try testing.expectEqual(ids[2], addTarget(&life.doc, ids[2]).?.id);
    try testing.expectEqual(ids[2], addTarget(&life.doc, ids[1]).?.id);
    try testing.expectEqual(ids[5], addTarget(&life.doc, ids[4]).?.id);
    try testing.expectEqual(Mode.crossets, addTarget(&life.doc, ids[5]).?.mode);
    try testing.expect(addTarget(&life.doc, ids[0]) == null);
    try testing.expect(addTarget(&life.doc, root) == null);
    try testing.expect(addTarget(&life.doc, null) == null);

    // A terrain picture goes under the selected terrain's Tiles.
    try addTile(testing.allocator, b, &life, ids[1], .terrains, "grass.tga");
    const tile = tools.childOfClass(&life.doc, ids[2], item.tileset_tile_props, 0).?;
    try testing.expectEqualStrings("grass", tools.findNode(&life.doc, tile).?.displaySlice());
    try testing.expect(life.dirty());
    try testing.expectEqual(@as(usize, 1), life.history.undo_stack.items.len);
    // The recorded step undoes the add.
    try life.doc.undoOne(testing.allocator, b, &life.history.undo_stack.items[0].command);
    try testing.expectEqual(@as(i32, 0), tools.childCount(&life.doc, ids[2]));

    // The same picture twice, a picture of the other list, no target, read-only.
    try addTile(testing.allocator, b, &life, ids[2], .terrains, "grass.tga");
    try testing.expectError(error.Refused, addTile(testing.allocator, b, &life, ids[2], .terrains, "grass.tga"));
    try testing.expectError(error.Refused, addTile(testing.allocator, b, &life, ids[2], .crossets, "grass.tga"));
    try testing.expectError(error.Refused, addTile(testing.allocator, b, &life, ids[0], .terrains, "x.tga"));
    life.read_only = true;
    try testing.expectError(error.Refused, addTile(testing.allocator, b, &life, ids[2], .terrains, "y.tga"));
}

test "import replaces the tree, ends the undo history and marks the project unsaved" {
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    var life: logic.Lifecycle = .{};
    defer life.deinit(testing.allocator);
    _ = try tileRig(&fake, &life);
    const b = fake.bridge();
    try fake.addGameFolder("tiles/terrains.xml", "4");

    try testing.expectEqual(@as(i32, 4), try importFile(testing.allocator, b, &life, "tiles/terrains.xml", .terrains));
    try testing.expect(!life.history.canUndo());
    try testing.expect(life.unsaved_import);
    try testing.expect(life.dirty());
    // A missing file is the bridge's refusal, naming it.
    try testing.expectError(error.Failed, importFile(testing.allocator, b, &life, "tiles/none.xml", .terrains));
    try testing.expect(std.mem.indexOf(u8, b.lastMessage(), "none.xml") != null);
    life.read_only = true;
    try testing.expectError(error.Refused, importFile(testing.allocator, b, &life, "tiles/terrains.xml", .terrains));
}
