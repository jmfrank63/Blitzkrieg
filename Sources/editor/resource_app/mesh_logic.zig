//! The Unit (mesh, .msh) editor's preview tools, with no window and no
//! ImGui: which locator a right-click in the preview picks, which tree node
//! that locator is, the model variant and visibility toolbar, and the
//! direction dock's turn of the previewed unit. Picking only moves the tree
//! selection, as MFC's locator pick did; it is no edit and never reaches the
//! history. Every bridge call here is refused by the bridge while the
//! preview shows no unit, and the refusal's message is what the app shows.
//! Runs under `zig build test-resource-app-logic` against the fake bridge.
const std = @import("std");
const core = @import("resource_core");
const edit = @import("edit_logic.zig");
const dl = @import("docks_logic.zig");

const tools = core.sub_editor_tools;
const bridge_mod = core.bridge;
const ResBridge = bridge_mod.ResBridge;
const MeshLocator = bridge_mod.MeshLocator;
const Point2 = bridge_mod.Point2;
const Document = core.document.Document;
const FakeResBridge = core.fake_bridge.FakeResBridge;
const testing = std.testing;

/// How far from a locator's marker, in screen pixels, a right-click still
/// picks it.
pub const pick_radius_px: f32 = 12;

/// The most skeleton nodes a unit model is read with (the biggest shipped
/// skeleton has far fewer).
pub const locator_capacity: usize = 256;

/// The three models of Graphics Info, in the order BkResPreviewMeshVariant
/// numbers them: MFC's SetCombatMesh, SetInstallMesh, SetTransportableMesh.
pub const Variant = enum(u8) {
    combat = 0,
    install = 1,
    transportable = 2,

    pub fn label(self: Variant) [:0]const u8 {
        return switch (self) {
            .combat => "Combat",
            .install => "Install",
            .transportable => "Transportable",
        };
    }
};

/// The nearest locator marker to a screen point, by index into the list and
/// by distance in pixels; null for an empty list.
pub const Nearest = struct { index: usize, distance: f32 };

pub fn nearestLocator(locators: []const MeshLocator, at: Point2) ?Nearest {
    var best: ?Nearest = null;
    for (locators, 0..) |locator, index| {
        const dx = locator.sx - at.x;
        const dy = locator.sy - at.y;
        const distance = @sqrt(dx * dx + dy * dy);
        if (best == null or distance < best.?.distance) best = .{ .index = index, .distance = distance };
    }
    return best;
}

/// The locator nearest the click and within `radius` pixels of it, or null.
pub fn pickLocator(locators: []const MeshLocator, at: Point2, radius: f32) ?usize {
    const nearest = nearestLocator(locators, at) orelse return null;
    return if (nearest.distance <= radius) nearest.index else null;
}

/// The tree node a locator is: the Locators item's child at the position the
/// locator's skeleton index names (the bridge builds the children in
/// skeleton order). Null when the item has fewer children, which is what a
/// model that changed under the preview looks like.
pub fn nodeForLocator(doc: *const Document, locator: MeshLocator) ?i32 {
    const parent = tools.firstOfClass(doc, tools.item_type.mesh_locators) orelse return null;
    if (locator.node_id < 0) return null;
    return tools.childOfClass(doc, parent, tools.item_type.mesh_locator_props, @intCast(locator.node_id));
}

/// What a right-click in the preview did.
pub const Pick = union(enum) {
    /// The locator's node is selected.
    node: i32,
    /// No locator within the radius; `nearest` is the closest, if any.
    miss: ?Nearest,
};

/// A right-click at `at`: selects the picked locator's node in the tree and
/// does nothing else (no command, no history entry).
pub fn pickAndSelect(gpa: std.mem.Allocator, doc: *const Document, selection: *edit.Selection, locators: []const MeshLocator, at: Point2) !Pick {
    const index = pickLocator(locators, at, pick_radius_px) orelse return .{ .miss = nearestLocator(locators, at) };
    const node = nodeForLocator(doc, locators[index]) orelse return .{ .miss = nearestLocator(locators, at) };
    try selection.only(gpa, node);
    return .{ .node = node };
}

/// The line a pick miss logs: the click and the distance to the nearest
/// locator marker.
pub fn missText(buffer: []u8, at: Point2, nearest: ?Nearest) []const u8 {
    if (nearest) |n| return std.fmt.bufPrint(buffer, "no locator at ({d:.0}, {d:.0}): the nearest is {d:.1} px away (radius {d:.0})", .{ at.x, at.y, n.distance, pick_radius_px }) catch buffer[0..0];
    return std.fmt.bufPrint(buffer, "no locator at ({d:.0}, {d:.0}): the preview has none", .{ at.x, at.y }) catch buffer[0..0];
}

/// The preview toolbar's state: the variant shown and the two display
/// toggles. They are the app's, not the project's: no command, no history.
pub const Toolbar = struct {
    variant: Variant = .combat,
    show_locators: bool = false,
    show_bounding_boxes: bool = false,

    pub fn setVariant(self: *Toolbar, b: ResBridge, variant: Variant) bridge_mod.EditError!void {
        try bridge_mod.check(b.previewMeshVariant(@intFromEnum(variant)));
        self.variant = variant;
    }

    pub fn setShow(self: *Toolbar, b: ResBridge, locators: bool, bounding_boxes: bool) bridge_mod.EditError!void {
        try bridge_mod.check(b.previewShowLocators(locators, bounding_boxes));
        self.show_locators = locators;
        self.show_bounding_boxes = bounding_boxes;
    }

    /// A fresh Run builds the unit again with the combat model and no
    /// locators; the toolbar goes back to match.
    pub fn reset(self: *Toolbar) void {
        self.* = .{};
    }
};

/// The direction dock's angle (radians, -pi..pi, 0 pointing right) as the
/// whole degrees BkResPreviewDirection takes: the dock's own degrees text,
/// rounded and taken into 0..359.
pub fn directionDegreesOf(angle: f32) i32 {
    const degrees: i32 = @intFromFloat(@round(dl.directionDegrees(angle)));
    return @mod(degrees, 360);
}

/// The dock turned the needle: the previewed unit follows.
pub fn turnPreview(b: ResBridge, angle: f32) bridge_mod.EditError!void {
    try bridge_mod.check(b.previewDirection(directionDegreesOf(angle)));
}

/// The skeleton nodes of the shown model, for the overlay's markers.
pub fn readLocators(b: ResBridge, buffer: []MeshLocator) bridge_mod.EditError![]const MeshLocator {
    var total: usize = 0;
    try bridge_mod.check(b.meshLocators(buffer, &total));
    return buffer[0..@min(total, buffer.len)];
}

// --- Tests ---------------------------------------------------------------------

/// A unit project on the fake: the Graphics item holding the combat model's
/// name, a Locators item, and a platform with a gun, with the model's
/// skeleton declared so a model switch rebuilds the Locators children.
pub const Rig = struct {
    fake: FakeResBridge,
    doc: Document = .{},
    history: core.history.History = .{},
    graphics: i32 = 0,
    locators: i32 = 0,
    platforms: i32 = 0,
    platform: i32 = 0,
    gun: i32 = 0,

    pub const skeleton = [_]FakeResBridge.MeshNodeDef{
        .{ .name = "Base" },
        .{ .name = "Turret" },
        .{ .name = "GunCarriage1" },
        .{ .name = "LMainGun1", .locator = true },
        .{ .name = "LExhaust1", .locator = true },
    };
    pub const other_skeleton = [_]FakeResBridge.MeshNodeDef{
        .{ .name = "Hull" },
        .{ .name = "LMachineGun1", .locator = true },
    };

    pub fn init(allocator: std.mem.Allocator) !Rig {
        var rig: Rig = .{ .fake = FakeResBridge.init(allocator) };
        errdefer rig.deinit(allocator);
        try rig.fake.addMeshModel("units/a.mod", &skeleton);
        try rig.fake.addMeshModel("units/b.mod", &other_skeleton);
        const b = rig.fake.bridge();
        try bridge_mod.check(b.new(.mesh_unit));
        const root = rig.fake.nodes.items[0].id;
        rig.graphics = try rig.add(allocator, root, tools.item_type.mesh_graphics);
        try rig.addProp(allocator, rig.graphics, 1, "Model", "units/a.mod");
        rig.platforms = try rig.add(allocator, root, tools.item_type.mesh_platforms);
        rig.locators = try rig.add(allocator, root, tools.item_type.mesh_locators);
        rig.platform = try rig.add(allocator, rig.platforms, tools.item_type.mesh_platform_props);
        try rig.addProp(allocator, rig.platform, 1, "Part", "NA");
        try rig.addProp(allocator, rig.platform, 2, "Gun carriage 1", "NA");
        try rig.addProp(allocator, rig.platform, 3, "Gun carriage 2", "NA");
        const guns = try rig.add(allocator, rig.platform, tools.item_type.mesh_guns);
        rig.gun = try rig.add(allocator, guns, tools.item_type.mesh_gun_props);
        try rig.addProp(allocator, rig.gun, 1, "Shoot point", "NA");
        try rig.addProp(allocator, rig.gun, 2, "Shoot part", "NA");
        // The model name goes in again through the bridge, the way an edit does,
        // so the Locators children exist.
        try bridge_mod.check(b.setProp(rig.graphics, 1, "units/a.mod"));
        try rig.doc.reload(allocator, b);
        try rig.doc.refreshKind(b);
        return rig;
    }

    pub fn deinit(self: *Rig, allocator: std.mem.Allocator) void {
        self.history.deinit(allocator);
        self.doc.deinit(allocator);
        self.fake.deinit();
    }

    fn add(self: *Rig, allocator: std.mem.Allocator, parent: i32, class_type: i32) !i32 {
        _ = allocator;
        var buf: [16]u8 = undefined;
        var id: i32 = 0;
        var count: i32 = 0;
        for (self.fake.nodes.items) |n| if (n.parent == parent) {
            count += 1;
        };
        try bridge_mod.check(self.fake.bridge().insertNode(parent, try std.fmt.bufPrint(&buf, "{d}", .{class_type}), count, &id));
        return id;
    }

    fn addProp(self: *Rig, allocator: std.mem.Allocator, node: i32, id: i32, name: []const u8, value: []const u8) !void {
        var prop: bridge_mod.PropRecord = .{ .id = id };
        _ = prop.setDefault(name);
        _ = prop.setDisplay(name);
        _ = prop.setValue(value);
        for (self.fake.nodes.items) |*n| if (n.id == node) {
            try n.props.append(allocator, prop);
            return;
        };
        return error.NoSuchNode;
    }

    /// Writes `text` to a property as one recorded, undoable edit.
    pub fn set(self: *Rig, allocator: std.mem.Allocator, node: i32, prop_id: i32, text: []const u8) !void {
        const command = try tools.setProp(allocator, &self.doc, node, prop_id, text);
        try tools.commit(allocator, self.fake.bridge(), &self.doc, &self.history, command, 0);
    }

    fn target(self: *Rig, allocator: std.mem.Allocator) edit.Target {
        return .{ .allocator = allocator, .bridge = self.fake.bridge(), .doc = &self.doc, .history = &self.history };
    }

    pub fn undo(self: *Rig, allocator: std.mem.Allocator) !void {
        if (!try edit.undo(self.target(allocator))) return error.NothingToUndo;
    }

    pub fn redo(self: *Rig, allocator: std.mem.Allocator) !void {
        if (!try edit.redo(self.target(allocator))) return error.NothingToRedo;
    }

    pub fn show(self: *Rig) !void {
        const b = self.fake.bridge();
        try bridge_mod.check(b.previewBegin(.mesh_unit));
        try bridge_mod.check(b.previewShow());
    }
};

test "pickLocator takes the nearest marker within the radius and nothing past it" {
    const locators = [_]MeshLocator{
        .{ .node_id = 0, .sx = 100, .sy = 100 },
        .{ .node_id = 1, .sx = 108, .sy = 100 },
    };
    try testing.expectEqual(@as(?usize, 1), pickLocator(&locators, .{ .x = 106, .y = 100 }, pick_radius_px));
    try testing.expectEqual(@as(?usize, 0), pickLocator(&locators, .{ .x = 98, .y = 101 }, pick_radius_px));
    try testing.expectEqual(@as(?usize, null), pickLocator(&locators, .{ .x = 140, .y = 100 }, pick_radius_px));
    try testing.expectEqual(@as(?usize, null), pickLocator(&.{}, .{ .x = 1, .y = 1 }, pick_radius_px));
    // The radius is the caller's: a wider one reaches the far marker.
    try testing.expectEqual(@as(?usize, 1), pickLocator(&locators, .{ .x = 140, .y = 100 }, 40));
}

test "a pick selects the locator's tree node and records nothing" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    try rig.show();
    var buffer: [locator_capacity]MeshLocator = undefined;
    const locators = try readLocators(rig.fake.bridge(), &buffer);
    try testing.expectEqual(@as(usize, Rig.skeleton.len), locators.len);

    var selection: edit.Selection = .{};
    defer selection.deinit(testing.allocator);
    const history_before = rig.history.undo_stack.items.len;
    // The fake puts locator i at screen (100 + 20 i, 200): click on the fourth.
    const result = try pickAndSelect(testing.allocator, &rig.doc, &selection, locators, .{ .x = 160, .y = 203 });
    const node = result.node;
    try testing.expectEqual(@as(?i32, node), selection.primary);
    const record = tools.findNode(&rig.doc, node).?;
    try testing.expectEqualStrings("LMainGun1", std.mem.sliceTo(&record.display_name, 0));
    try testing.expectEqual(rig.locators, record.parent);
    try testing.expectEqual(history_before, rig.history.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 0), rig.history.redo_stack.items.len);
}

test "a pick miss leaves the selection alone and says how far the nearest marker is" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    try rig.show();
    var buffer: [locator_capacity]MeshLocator = undefined;
    const locators = try readLocators(rig.fake.bridge(), &buffer);
    var selection: edit.Selection = .{};
    defer selection.deinit(testing.allocator);
    try selection.only(testing.allocator, rig.graphics);
    const result = try pickAndSelect(testing.allocator, &rig.doc, &selection, locators, .{ .x = 100, .y = 260 });
    const nearest = result.miss.?;
    try testing.expectEqual(@as(usize, 0), nearest.index);
    try testing.expectApproxEqAbs(@as(f32, 60), nearest.distance, 1e-3);
    try testing.expectEqual(@as(?i32, rig.graphics), selection.primary);
    var text: [128]u8 = undefined;
    const line = missText(&text, .{ .x = 100, .y = 260 }, nearest);
    try testing.expect(std.mem.indexOf(u8, line, "60.0 px") != null);
    try testing.expect(std.mem.indexOf(u8, line, "(100, 260)") != null);
}

test "nodeForLocator is null for a locator past the children the tree has" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    try testing.expect(nodeForLocator(&rig.doc, .{ .node_id = 4 }) != null);
    try testing.expectEqual(@as(?i32, null), nodeForLocator(&rig.doc, .{ .node_id = 5 }));
    try testing.expectEqual(@as(?i32, null), nodeForLocator(&rig.doc, .{ .node_id = -1 }));
}

test "the toolbar sets the variant and the toggles through the bridge, and the bridge's refusals pass" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const b = rig.fake.bridge();
    var toolbar: Toolbar = .{};
    // No unit shown yet: refused, state unchanged.
    try testing.expectError(error.Refused, toolbar.setVariant(b, .install));
    try testing.expectEqual(Variant.combat, toolbar.variant);
    try testing.expect(std.mem.indexOf(u8, b.lastMessage(), "no unit") != null);
    try testing.expectError(error.Refused, toolbar.setShow(b, true, false));
    try testing.expect(!toolbar.show_locators);

    try rig.show();
    try toolbar.setVariant(b, .transportable);
    try testing.expectEqual(@as(u8, 2), rig.fake.mesh_variant);
    try toolbar.setShow(b, true, false);
    try testing.expect(rig.fake.show_locators and !rig.fake.show_bounding_boxes);
    try toolbar.setShow(b, true, true);
    try testing.expect(rig.fake.show_locators and rig.fake.show_bounding_boxes);
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
    toolbar.reset();
    try testing.expectEqual(Variant.combat, toolbar.variant);
}

test "the direction dock's angle turns the previewed unit by the degrees it shows" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const b = rig.fake.bridge();
    // The needle straight up-right (pi/4) reads 0 degrees, as in MFC.
    try testing.expectEqual(@as(i32, 0), directionDegreesOf(std.math.pi / 4.0));
    try testing.expectEqual(@as(i32, 90), directionDegreesOf(std.math.pi / 4.0 + std.math.pi / 2.0));
    try testing.expectEqual(@as(i32, 270), directionDegreesOf(std.math.pi / 4.0 - std.math.pi / 2.0));
    try testing.expectError(error.Refused, turnPreview(b, 1.0));
    try rig.show();
    try turnPreview(b, std.math.pi / 4.0 + std.math.pi / 2.0);
    try testing.expectEqual(@as(i32, 90), rig.fake.mesh_direction);
}

test "the locator combos follow the combat model's skeleton, and NA ends each list" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const b = rig.fake.bridge();
    var entries: [16]bridge_mod.ReferenceEntry = undefined;
    var total: usize = 0;

    try bridge_mod.check(b.propStrings(rig.platform, 1, &entries, &total));
    try expectNames(&entries, total, &.{ "Base", "Turret", "GunCarriage1", "NA" });
    try bridge_mod.check(b.propStrings(rig.platform, 2, &entries, &total));
    try expectNames(&entries, total, &.{ "GunCarriage1", "NA" });
    try bridge_mod.check(b.propStrings(rig.platform, 3, &entries, &total));
    try expectNames(&entries, total, &.{ "GunCarriage1", "NA" });
    try bridge_mod.check(b.propStrings(rig.gun, 1, &entries, &total));
    try expectNames(&entries, total, &.{ "LMainGun1", "NA" });
    try bridge_mod.check(b.propStrings(rig.gun, 2, &entries, &total));
    try expectNames(&entries, total, &.{ "Base", "Turret", "NA" });

    // A model switch changes every list.
    try rig.set(testing.allocator, rig.graphics, 1, "units/b.mod");
    try bridge_mod.check(b.propStrings(rig.gun, 1, &entries, &total));
    try expectNames(&entries, total, &.{ "LMachineGun1", "NA" });
    try bridge_mod.check(b.propStrings(rig.platform, 2, &entries, &total));
    try expectNames(&entries, total, &.{"NA"});
    // A model nobody declared: no nodes, only NA.
    try rig.set(testing.allocator, rig.graphics, 1, "units/missing.mod");
    try bridge_mod.check(b.propStrings(rig.platform, 1, &entries, &total));
    try expectNames(&entries, total, &.{"NA"});
}

fn expectNames(entries: []const bridge_mod.ReferenceEntry, total: usize, want: []const []const u8) !void {
    try testing.expectEqual(want.len, total);
    for (want, entries[0..total]) |name, *entry| try testing.expectEqualStrings(name, entry.nameSlice());
}

/// The names of the Locators children, in order.
fn locatorNames(rig: *Rig, out: *[16][]const u8) usize {
    var n: usize = 0;
    for (rig.doc.tree.nodes.items) |*node| {
        if (node.parent != rig.locators) continue;
        out[n] = std.mem.sliceTo(&node.display_name, 0);
        n += 1;
    }
    return n;
}

fn propText(rig: *Rig, node: i32, prop_id: i32) []const u8 {
    return tools.propValue(&rig.doc, node, prop_id).?;
}

test "undo and redo: a gun point, part and carriage locator reference" {
    const gpa = testing.allocator;
    var rig = try Rig.init(gpa);
    defer rig.deinit(gpa);
    const Case = struct { node: i32, prop: i32, value: []const u8 };
    const cases = [_]Case{
        .{ .node = rig.gun, .prop = 1, .value = "LMainGun1" },
        .{ .node = rig.gun, .prop = 2, .value = "Turret" },
        .{ .node = rig.platform, .prop = 2, .value = "GunCarriage1" },
        .{ .node = rig.platform, .prop = 3, .value = "GunCarriage1" },
    };
    for (cases) |case| {
        const before = try gpa.dupe(u8, propText(&rig, case.node, case.prop));
        defer gpa.free(before);
        try rig.set(gpa, case.node, case.prop, case.value);
        expectText(case.value, propText(&rig, case.node, case.prop), "after set", case.node, case.prop) catch |err| return err;
        try rig.undo(gpa);
        try expectText(before, propText(&rig, case.node, case.prop), "after undo", case.node, case.prop);
        try rig.redo(gpa);
        try expectText(case.value, propText(&rig, case.node, case.prop), "after redo", case.node, case.prop);
    }
}

fn expectText(want: []const u8, got: []const u8, when: []const u8, node: i32, prop: i32) !void {
    if (!std.mem.eql(u8, want, got)) {
        std.debug.print("property {d} of node {d} {s}: expected \"{s}\", got \"{s}\"\n", .{ prop, node, when, want, got });
        return error.TestExpectedEqual;
    }
}

test "undo and redo: a platform's locator choice" {
    const gpa = testing.allocator;
    var rig = try Rig.init(gpa);
    defer rig.deinit(gpa);
    try rig.set(gpa, rig.platform, 1, "Turret");
    try expectText("Turret", propText(&rig, rig.platform, 1), "after set", rig.platform, 1);
    try rig.set(gpa, rig.platform, 1, "Base");
    try rig.undo(gpa);
    try expectText("Turret", propText(&rig, rig.platform, 1), "after undo", rig.platform, 1);
    try rig.undo(gpa);
    try expectText("NA", propText(&rig, rig.platform, 1), "after second undo", rig.platform, 1);
    try rig.redo(gpa);
    try rig.redo(gpa);
    try expectText("Base", propText(&rig, rig.platform, 1), "after redo", rig.platform, 1);
}

test "undo and redo: a model switch rebuilds the locator children both ways" {
    const gpa = testing.allocator;
    var rig = try Rig.init(gpa);
    defer rig.deinit(gpa);
    var names: [16][]const u8 = undefined;
    try testing.expectEqual(@as(usize, 5), locatorNames(&rig, &names));
    try testing.expectEqualStrings("Base", names[0]);

    try rig.set(gpa, rig.graphics, 1, "units/b.mod");
    try expectText("units/b.mod", propText(&rig, rig.graphics, 1), "after set", rig.graphics, 1);
    try testing.expectEqual(@as(usize, 2), locatorNames(&rig, &names));
    try testing.expectEqualStrings("Hull", names[0]);
    try testing.expectEqualStrings("LMachineGun1", names[1]);

    try rig.undo(gpa);
    try expectText("units/a.mod", propText(&rig, rig.graphics, 1), "after undo", rig.graphics, 1);
    try testing.expectEqual(@as(usize, 5), locatorNames(&rig, &names));
    try testing.expectEqualStrings("LExhaust1", names[4]);

    try rig.redo(gpa);
    try testing.expectEqual(@as(usize, 2), locatorNames(&rig, &names));
    try testing.expectEqualStrings("Hull", names[0]);
}

test "undo and redo: a platform and a gun inserted and deleted" {
    const gpa = testing.allocator;
    var rig = try Rig.init(gpa);
    defer rig.deinit(gpa);
    const b = rig.fake.bridge();
    const nodes_before = rig.doc.tree.nodes.items.len;

    // Insert: a platform under Platforms (MFC's Insert key on the platforms item).
    const insert_platform = try tools.appendChild(gpa, &rig.doc, rig.platforms, tools.item_type.mesh_platform_props);
    try tools.commit(gpa, b, &rig.doc, &rig.history, insert_platform, 0);
    try testing.expectEqual(nodes_before + 1, rig.doc.tree.nodes.items.len);
    try testing.expectEqual(@as(i32, 2), tools.childCount(&rig.doc, rig.platforms));
    try rig.undo(gpa);
    try testing.expectEqual(nodes_before, rig.doc.tree.nodes.items.len);
    try rig.redo(gpa);
    try testing.expectEqual(nodes_before + 1, rig.doc.tree.nodes.items.len);

    // Insert: a gun under the platform's Guns item.
    const guns = tools.childOfClass(&rig.doc, rig.platform, tools.item_type.mesh_guns, 0).?;
    const insert_gun = try tools.appendChild(gpa, &rig.doc, guns, tools.item_type.mesh_gun_props);
    try tools.commit(gpa, b, &rig.doc, &rig.history, insert_gun, 0);
    try testing.expectEqual(@as(i32, 2), tools.childCount(&rig.doc, guns));
    try rig.undo(gpa);
    try testing.expectEqual(@as(i32, 1), tools.childCount(&rig.doc, guns));
    try rig.redo(gpa);
    try testing.expectEqual(@as(i32, 2), tools.childCount(&rig.doc, guns));

    // Delete: the original gun (MFC's Delete key), then undo brings it back with its values.
    try rig.set(gpa, rig.gun, 1, "LMainGun1");
    const count = rig.doc.tree.nodes.items.len;
    const delete = try tools.deleteNode(&rig.doc, rig.gun);
    try tools.commit(gpa, b, &rig.doc, &rig.history, delete, 0);
    try testing.expectEqual(count - 1, rig.doc.tree.nodes.items.len);
    try rig.undo(gpa);
    try testing.expectEqual(count, rig.doc.tree.nodes.items.len);
    const restored = tools.childOfClass(&rig.doc, guns, tools.item_type.mesh_gun_props, 0).?;
    try expectText("LMainGun1", propText(&rig, restored, 1), "after undo of the delete", restored, 1);
    try rig.redo(gpa);
    try testing.expectEqual(count - 1, rig.doc.tree.nodes.items.len);
}
