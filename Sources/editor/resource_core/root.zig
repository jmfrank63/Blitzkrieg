//! The resource editor core: no UI, no engine, no C. Mirrors
//! `Sources/editor/core/root.zig` for the resource editor's own command
//! vocabulary (`history.ResourceCommand`), its own bridge interface
//! (`bridge.ResBridge`), its own document mirror (`document.Document`) and
//! its own fake (`fake_bridge.FakeResBridge`). The kit (editor_kit) is
//! importable here and the generic stack primitive (editor_kit.history) is
//! used verbatim.
const std = @import("std");

pub const bridge = @import("bridge.zig");
pub const document = @import("document.zig");
pub const history = @import("history.zig");
pub const fake_bridge = @import("fake_bridge.zig");

const ResBridge = bridge.ResBridge;
const Kind = bridge.Kind;
const NodeRecord = bridge.NodeRecord;
const PropRecord = bridge.PropRecord;
const Point2 = bridge.Point2;
const AimedPoint = bridge.AimedPoint;
const Vec3 = bridge.Vec3;
const GeometryChannel = bridge.GeometryChannel;
const GeometryValue = bridge.GeometryValue;
const Document = document.Document;
const History = history.History;
const ResourceCommand = history.ResourceCommand;
const OwnedBytes = history.OwnedBytes;
const FakeResBridge = fake_bridge.FakeResBridge;

test {
    std.testing.refAllDecls(@This());
}

/// Builds a bare project with a root + one property so the tests have a
/// known starting shape to mutate.
fn setupProject(allocator: std.mem.Allocator, fake: *FakeResBridge) !void {
    try bridge.check(fake.bridge().new(.weapon));
    const root_id = fake.nodes.items[0].id;
    var prop: PropRecord = .{ .id = 1, .domain_type = 0, .value_kind = 0 };
    _ = prop.setDefault("damage");
    _ = prop.setDisplay("Damage");
    _ = prop.setValue("10");
    try fake.nodes.items[0].props.append(allocator, prop);
    _ = root_id;
}

test "set_prop records -> undo -> redo keeps the document in sync" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());
    try doc.refreshKind(fake.bridge());

    var hist: History = .{};
    defer hist.deinit(allocator);

    const root_id = fake.nodes.items[0].id;
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .set_prop = .{
        .node = root_id,
        .prop_id = 1,
        .before = try OwnedBytes.fromSlice(allocator, "10"),
        .after = try OwnedBytes.fromSlice(allocator, "42"),
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);

    try std.testing.expectEqualStrings("42", std.mem.sliceTo(&doc.tree.findProp(root_id, 1).?.record.value_text, 0));
    try std.testing.expect(hist.canUndo());

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqualStrings("10", std.mem.sliceTo(&doc.tree.findProp(root_id, 1).?.record.value_text, 0));

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqualStrings("42", std.mem.sliceTo(&doc.tree.findProp(root_id, 1).?.record.value_text, 0));
}

test "insert_node records -> undo -> redo recreates the node" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    var hist: History = .{};
    defer hist.deinit(allocator);

    const root_id = fake.nodes.items[0].id;
    const class = try allocator.dupe(u8, "Child");
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .insert_node = .{
        .parent = root_id,
        .class_name = class,
        .index = 0,
        .new_id = -1,
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    try std.testing.expectEqual(@as(usize, 2), doc.tree.nodes.items.len);

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(@as(usize, 1), doc.tree.nodes.items.len);

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(@as(usize, 2), doc.tree.nodes.items.len);
}

test "delete_node records -> undo -> redo restores the subtree" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;
    var child_id: i32 = -1;
    try bridge.check(fake.bridge().insertNode(root_id, "Child", 0, &child_id));

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    var hist: History = .{};
    defer hist.deinit(allocator);
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .delete_node = .{
        .parent = root_id,
        .index = 0,
        .node = child_id,
        .blob = .{},
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    try std.testing.expectEqual(@as(usize, 1), doc.tree.nodes.items.len);
    try std.testing.expect(hist.top().?.command.delete_node.blob.bytes.len > 0);

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(@as(usize, 2), doc.tree.nodes.items.len);

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(@as(usize, 1), doc.tree.nodes.items.len);
}

test "move_node records -> undo -> redo reparents" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;
    var child_a: i32 = -1;
    var child_b: i32 = -1;
    try bridge.check(fake.bridge().insertNode(root_id, "A", 0, &child_a));
    try bridge.check(fake.bridge().insertNode(root_id, "B", 1, &child_b));

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    var hist: History = .{};
    defer hist.deinit(allocator);
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .move_node = .{
        .node = child_b,
        .before_parent = root_id,
        .before_index = 1,
        .after_parent = child_a,
        .after_index = 0,
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    try std.testing.expectEqual(child_a, doc.tree.findNode(child_b).?.parent);

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(root_id, doc.tree.findNode(child_b).?.parent);

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(child_a, doc.tree.findNode(child_b).?.parent);
}

test "geometry records -> undo -> redo rewrites the channel" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    // Prime the channel with an empty payload the bridge treats as "clear".
    const before_points = try allocator.alloc(Point2, 0);
    const after_points = try allocator.alloc(Point2, 2);
    after_points[0] = .{ .x = 1, .y = 2 };
    after_points[1] = .{ .x = 3, .y = 4 };

    var hist: History = .{};
    defer hist.deinit(allocator);
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .geometry = .{
        .node = root_id,
        .channel = .formation_positions,
        .before = .{ .points2 = before_points },
        .after = .{ .points2 = after_points },
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .formation_positions, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(usize, 2), read.points2.len);
    }

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .formation_positions, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(usize, 0), read.points2.len);
    }

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .formation_positions, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(usize, 2), read.points2.len);
    }
}

test "composite collapses two commands into one undo step" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    var hist: History = .{};
    defer hist.deinit(allocator);

    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    try steps.append(allocator, .{ .set_prop = .{
        .node = root_id,
        .prop_id = 1,
        .before = try OwnedBytes.fromSlice(allocator, "10"),
        .after = try OwnedBytes.fromSlice(allocator, "20"),
    } });
    const class = try allocator.dupe(u8, "Attachment");
    try steps.append(allocator, .{ .insert_node = .{
        .parent = root_id,
        .class_name = class,
        .index = 0,
        .new_id = -1,
    } });
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .composite = .{ .steps = steps } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);

    try std.testing.expectEqual(@as(usize, 1), hist.undo_stack.items.len);
    try std.testing.expectEqualStrings("20", std.mem.sliceTo(&doc.tree.findProp(root_id, 1).?.record.value_text, 0));
    try std.testing.expectEqual(@as(usize, 2), doc.tree.nodes.items.len);

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqualStrings("10", std.mem.sliceTo(&doc.tree.findProp(root_id, 1).?.record.value_text, 0));
    try std.testing.expectEqual(@as(usize, 1), doc.tree.nodes.items.len);

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqualStrings("20", std.mem.sliceTo(&doc.tree.findProp(root_id, 1).?.record.value_text, 0));
    try std.testing.expectEqual(@as(usize, 2), doc.tree.nodes.items.len);
}

test "the dirty flag follows History.dirty()" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    var hist: History = .{};
    defer hist.deinit(allocator);
    try std.testing.expect(!doc.isDirty(&hist));

    const root_id = fake.nodes.items[0].id;
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .set_prop = .{
        .node = root_id,
        .prop_id = 1,
        .before = try OwnedBytes.fromSlice(allocator, "10"),
        .after = try OwnedBytes.fromSlice(allocator, "11"),
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    try std.testing.expect(doc.isDirty(&hist));

    hist.markClean();
    try std.testing.expect(!doc.isDirty(&hist));
}

test "markClean then undo past it drops the clean_depth, matching the kit" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    var hist: History = .{};
    defer hist.deinit(allocator);

    const root_id = fake.nodes.items[0].id;
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .set_prop = .{
        .node = root_id,
        .prop_id = 1,
        .before = try OwnedBytes.fromSlice(allocator, "10"),
        .after = try OwnedBytes.fromSlice(allocator, "11"),
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    hist.markClean();
    try std.testing.expect(!hist.dirty());

    // Undo past the clean mark.
    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    // Simulate the editor's move of the entry from undo to redo so the kit
    // can see the depth change.
    const entry = hist.undo_stack.pop().?;
    try hist.redo_stack.append(allocator, entry);
    hist.revision +%= 1;
    try std.testing.expect(hist.dirty());

    // Recording a fresh command past clean_depth clears clean_depth.
    try hist.reserve(allocator);
    var cmd2: ResourceCommand = .{ .set_prop = .{
        .node = root_id,
        .prop_id = 1,
        .before = try OwnedBytes.fromSlice(allocator, "10"),
        .after = try OwnedBytes.fromSlice(allocator, "12"),
    } };
    try doc.apply(allocator, fake.bridge(), &cmd2);
    hist.recordAssumeCapacity(allocator, cmd2, 0);
    try std.testing.expect(hist.dirty());
}
