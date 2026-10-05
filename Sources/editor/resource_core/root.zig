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
pub const sub_editor_tools = @import("sub_editor_tools.zig");
pub const grid_tools = @import("grid_tools.zig");
pub const point_tools = @import("point_tools.zig");

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

test "geometry passability_cells undo/redo round-trips the grid" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    const before_bytes = try allocator.alloc(u8, 0);
    const after_bytes = try allocator.alloc(u8, 6);
    for (after_bytes, 0..) |*b, i| b.* = @intCast(i + 1);

    var hist: History = .{};
    defer hist.deinit(allocator);
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .geometry = .{
        .node = root_id,
        .channel = .passability_cells,
        .before = .{ .bytes_grid = .{ .bytes = before_bytes, .width = 0, .height = 0 } },
        .after = .{ .bytes_grid = .{ .bytes = after_bytes, .width = 3, .height = 2 } },
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    try std.testing.expect(doc.isDirty(&hist));
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .passability_cells, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(i32, 3), read.bytes_grid.width);
        try std.testing.expectEqual(@as(i32, 2), read.bytes_grid.height);
        try std.testing.expectEqualSlices(u8, &.{ 1, 2, 3, 4, 5, 6 }, read.bytes_grid.bytes);
    }

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .passability_cells, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(i32, 0), read.bytes_grid.width);
        try std.testing.expectEqual(@as(i32, 0), read.bytes_grid.height);
        try std.testing.expectEqual(@as(usize, 0), read.bytes_grid.bytes.len);
    }

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .passability_cells, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqualSlices(u8, &.{ 1, 2, 3, 4, 5, 6 }, read.bytes_grid.bytes);
    }

    hist.markClean();
    try std.testing.expect(!doc.isDirty(&hist));
}

test "geometry locked_tiles undo/redo round-trips the grid" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    const before_bytes = try allocator.alloc(u8, 0);
    const after_bytes = try allocator.alloc(u8, 4);
    after_bytes[0] = 0;
    after_bytes[1] = 1;
    after_bytes[2] = 1;
    after_bytes[3] = 0;

    var hist: History = .{};
    defer hist.deinit(allocator);
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .geometry = .{
        .node = root_id,
        .channel = .locked_tiles,
        .before = .{ .bytes_grid = .{ .bytes = before_bytes, .width = 0, .height = 0 } },
        .after = .{ .bytes_grid = .{ .bytes = after_bytes, .width = 2, .height = 2 } },
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .locked_tiles, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqualSlices(u8, &.{ 0, 1, 1, 0 }, read.bytes_grid.bytes);
    }

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .locked_tiles, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(usize, 0), read.bytes_grid.bytes.len);
    }

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .locked_tiles, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqualSlices(u8, &.{ 0, 1, 1, 0 }, read.bytes_grid.bytes);
    }
}

test "geometry transparency_lines undo/redo round-trips the point list" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());

    const before_points = try allocator.alloc(Point2, 0);
    const after_points = try allocator.alloc(Point2, 3);
    after_points[0] = .{ .x = 0.5, .y = 1.5 };
    after_points[1] = .{ .x = 2.0, .y = 3.0 };
    after_points[2] = .{ .x = 4.25, .y = 5.75 };

    var hist: History = .{};
    defer hist.deinit(allocator);
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .geometry = .{
        .node = root_id,
        .channel = .transparency_lines,
        .before = .{ .points2 = before_points },
        .after = .{ .points2 = after_points },
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    try std.testing.expect(doc.isDirty(&hist));
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .transparency_lines, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(usize, 3), read.points2.len);
        try std.testing.expectEqual(@as(f32, 4.25), read.points2[2].x);
    }

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .transparency_lines, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(usize, 0), read.points2.len);
    }

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .transparency_lines, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(usize, 3), read.points2.len);
    }
}

test "geometry zero_point undo/redo rewrites the single Point2" {
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
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .geometry = .{
        .node = root_id,
        .channel = .zero_point,
        .before = .{ .point2 = .{ .x = 0, .y = 0 } },
        .after = .{ .point2 = .{ .x = 1.25, .y = -2.5 } },
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .zero_point, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(f32, 1.25), read.point2.x);
        try std.testing.expectEqual(@as(f32, -2.5), read.point2.y);
    }

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .zero_point, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(f32, 0), read.point2.x);
        try std.testing.expectEqual(@as(f32, 0), read.point2.y);
    }

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .zero_point, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(f32, 1.25), read.point2.x);
    }
}

test "geometry entrance undo/redo rewrites the single Point2" {
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
    try hist.reserve(allocator);
    var cmd: ResourceCommand = .{ .geometry = .{
        .node = root_id,
        .channel = .entrance,
        .before = .{ .point2 = .{ .x = 0, .y = 0 } },
        .after = .{ .point2 = .{ .x = 7, .y = 11 } },
    } };
    try doc.apply(allocator, fake.bridge(), &cmd);
    hist.recordAssumeCapacity(allocator, cmd, 0);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .entrance, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(f32, 7), read.point2.x);
        try std.testing.expectEqual(@as(f32, 11), read.point2.y);
    }

    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .entrance, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(f32, 0), read.point2.y);
    }

    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    {
        var read: GeometryValue = undefined;
        try bridge.check(fake.bridge().geometryRead(root_id, .entrance, &read));
        defer read.deinit(allocator);
        try std.testing.expectEqual(@as(f32, 11), read.point2.y);
    }
}

test "geometry aimed-point channels undo/redo rewrite the list" {
    // One test loop that exercises each of the four aimed channels in turn.
    // The angle/cone pair carries the MFC-era degrees semantic the ABI pins;
    // the fake stores them verbatim.
    const allocator = std.testing.allocator;
    const channels = [_]GeometryChannel{
        .shoot_points, .fire_points, .smoke_points, .directed_explosion_points,
    };
    for (channels) |channel| {
        var fake = FakeResBridge.init(allocator);
        defer fake.deinit();
        try setupProject(allocator, &fake);
        const root_id = fake.nodes.items[0].id;

        var doc: Document = .{};
        defer doc.deinit(allocator);
        try doc.reload(allocator, fake.bridge());

        const before_aimed = try allocator.alloc(AimedPoint, 0);
        const after_aimed = try allocator.alloc(AimedPoint, 2);
        after_aimed[0] = .{ .at = .{ .x = 0.5, .y = 1.5 }, .angle = 90, .cone = 15 };
        after_aimed[1] = .{ .at = .{ .x = 3.0, .y = 4.0 }, .angle = 180, .cone = 30 };

        var hist: History = .{};
        defer hist.deinit(allocator);
        try hist.reserve(allocator);
        var cmd: ResourceCommand = .{ .geometry = .{
            .node = root_id,
            .channel = channel,
            .before = .{ .aimed = before_aimed },
            .after = .{ .aimed = after_aimed },
        } };
        try doc.apply(allocator, fake.bridge(), &cmd);
        hist.recordAssumeCapacity(allocator, cmd, 0);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(root_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqual(@as(usize, 2), read.aimed.len);
            try std.testing.expectEqual(@as(i32, 90), read.aimed[0].angle);
            try std.testing.expectEqual(@as(i32, 15), read.aimed[0].cone);
            try std.testing.expectEqual(@as(f32, 3.0), read.aimed[1].at.x);
        }

        try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(root_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqual(@as(usize, 0), read.aimed.len);
        }

        try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(root_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqual(@as(usize, 2), read.aimed.len);
            try std.testing.expectEqual(@as(i32, 180), read.aimed[1].angle);
        }
    }
}

test "geometry points2 owner-node channels undo/redo on a child node" {
    // The points2 channels of the squad, bridge, mission, chapter and
    // campaign editors. MFC keeps each list on a node below the root (a
    // formation props node, a spans node, an objectives or chapters list),
    // so the edit goes to an inserted child, and `before` is a real list so
    // undo has to put the old points back, not just clear.
    const allocator = std.testing.allocator;
    const channels = [_]GeometryChannel{ .formation_positions, .bridge_span_marks, .mission_objectives, .chapter_crosses, .campaign_crosses };
    for (channels) |channel| {
        var fake = FakeResBridge.init(allocator);
        defer fake.deinit();
        try setupProject(allocator, &fake);
        const root_id = fake.nodes.items[0].id;
        var child_id: i32 = 0;
        try bridge.check(fake.bridge().insertNode(root_id, "Formation", 0, &child_id));
        try std.testing.expect(child_id != root_id);

        const old_points = [_]Point2{.{ .x = 512, .y = 256 }};
        const seed: GeometryValue = .{ .points2 = @constCast(&old_points) };
        try bridge.check(fake.bridge().geometryWrite(child_id, channel, &seed));

        var doc: Document = .{};
        defer doc.deinit(allocator);
        try doc.reload(allocator, fake.bridge());

        const before_points = try allocator.dupe(Point2, &old_points);
        const after_points = try allocator.alloc(Point2, 3);
        after_points[0] = .{ .x = 496, .y = 240 };
        after_points[1] = .{ .x = 528, .y = 240 };
        after_points[2] = .{ .x = 512.5, .y = 272.25 };

        var hist: History = .{};
        defer hist.deinit(allocator);
        try hist.reserve(allocator);
        var cmd: ResourceCommand = .{ .geometry = .{
            .node = child_id,
            .channel = channel,
            .before = .{ .points2 = before_points },
            .after = .{ .points2 = after_points },
        } };
        try doc.apply(allocator, fake.bridge(), &cmd);
        hist.recordAssumeCapacity(allocator, cmd, 0);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(child_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqualSlices(Point2, &.{
                .{ .x = 496, .y = 240 }, .{ .x = 528, .y = 240 }, .{ .x = 512.5, .y = 272.25 },
            }, read.points2);
        }
        {
            // The root's list on the same channel is untouched.
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(root_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqual(@as(usize, 0), read.points2.len);
        }

        try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(child_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqualSlices(Point2, &old_points, read.points2);
        }

        try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(child_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqual(@as(usize, 3), read.points2.len);
            try std.testing.expectEqual(@as(f32, 272.25), read.points2[2].y);
        }
    }
}

test "geometry particle and effect keyframes undo/redo keep z" {
    // The vec3 family: a keyframe list on a track node below the root, with
    // a z that must survive undo and redo rather than be dropped to a Point2.
    const allocator = std.testing.allocator;
    const channels = [_]GeometryChannel{ .particle_keyframes, .effect_keyframes };
    for (channels) |channel| {
        var fake = FakeResBridge.init(allocator);
        defer fake.deinit();
        try setupProject(allocator, &fake);
        const root_id = fake.nodes.items[0].id;
        var child_id: i32 = 0;
        try bridge.check(fake.bridge().insertNode(root_id, "Track", 0, &child_id));

        const old_keys = [_]Vec3{ .{ .x = 0, .y = 1, .z = 0.5 }, .{ .x = 1, .y = 0, .z = -0.25 } };
        const seed: GeometryValue = .{ .vec3 = @constCast(&old_keys) };
        try bridge.check(fake.bridge().geometryWrite(child_id, channel, &seed));

        var doc: Document = .{};
        defer doc.deinit(allocator);
        try doc.reload(allocator, fake.bridge());

        const before_keys = try allocator.dupe(Vec3, &old_keys);
        const after_keys = try allocator.alloc(Vec3, 3);
        after_keys[0] = .{ .x = 0, .y = 0.75, .z = 0 };
        after_keys[1] = .{ .x = 0.5, .y = 0.125, .z = 2 };
        after_keys[2] = .{ .x = 1, .y = 0, .z = 3.375 };
        const new_keys = [_]Vec3{ after_keys[0], after_keys[1], after_keys[2] };

        var hist: History = .{};
        defer hist.deinit(allocator);
        try hist.reserve(allocator);
        var cmd: ResourceCommand = .{ .geometry = .{
            .node = child_id,
            .channel = channel,
            .before = .{ .vec3 = before_keys },
            .after = .{ .vec3 = after_keys },
        } };
        try doc.apply(allocator, fake.bridge(), &cmd);
        hist.recordAssumeCapacity(allocator, cmd, 0);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(child_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqualSlices(Vec3, &new_keys, read.vec3);
        }

        try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(child_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqualSlices(Vec3, &old_keys, read.vec3);
        }

        try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
        {
            var read: GeometryValue = undefined;
            try bridge.check(fake.bridge().geometryRead(child_id, channel, &read));
            defer read.deinit(allocator);
            try std.testing.expectEqualSlices(Vec3, &new_keys, read.vec3);
        }

        // A Point2 list on a keyframe channel is the wrong family.
        const flat = [_]Point2{.{ .x = 1, .y = 2 }};
        const wrong: GeometryValue = .{ .points2 = @constCast(&flat) };
        try std.testing.expectEqual(bridge.Status.bad_argument, fake.bridge().geometryWrite(child_id, channel, &wrong));
    }
}

test "geometry write refuses a payload of the wrong family" {
    // Bridge span marks were once declared as Vec3; a Vec3 list on that
    // channel must now be refused rather than silently stored.
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try setupProject(allocator, &fake);
    const root_id = fake.nodes.items[0].id;
    const marks = [_]Vec3{.{ .x = 1, .y = 2, .z = 3 }};
    const wrong: GeometryValue = .{ .vec3 = @constCast(&marks) };
    try std.testing.expectEqual(bridge.Status.bad_argument, fake.bridge().geometryWrite(root_id, .bridge_span_marks, &wrong));
    var read: GeometryValue = undefined;
    try bridge.check(fake.bridge().geometryRead(root_id, .bridge_span_marks, &read));
    defer read.deinit(allocator);
    try std.testing.expectEqual(std.meta.Tag(GeometryValue).points2, std.meta.activeTag(read));
    try std.testing.expectEqual(@as(usize, 0), read.points2.len);
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

test "export is refused until the kind's exporter is ported, then reaches it" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    const res = fake.bridge();
    var report: bridge.ExportReport = .{};
    var warnings: [2]bridge.Warning = .{ .{}, .{} };

    try std.testing.expectEqual(bridge.Status.refused, res.exportProject(.{}, false, &report, &warnings));
    try bridge.check(res.new(.medal));
    try std.testing.expectEqual(bridge.Status.refused, res.exportProject(.{}, false, &report, &warnings));
    try std.testing.expect(std.mem.indexOf(u8, res.lastMessage(), "save the project first") != null);
    try bridge.check(res.save("medal.mdc"));
    try std.testing.expectEqual(bridge.Status.refused, res.exportProject(.{}, false, &report, &warnings));
    try std.testing.expect(std.mem.indexOf(u8, res.lastMessage(), ".mdc projects is not ported yet") != null);
    try std.testing.expectEqual(@as(i32, 0), report.written);

    fake.setExportable(.medal, true);
    try bridge.check(res.exportProject(.{ .force = true }, false, &report, &warnings));
    try std.testing.expectEqual(@as(i32, 1), report.written);
    try std.testing.expect(fake.last_flags.force and !fake.last_stats_only);
    try bridge.check(res.exportProject(.{}, true, &report, &warnings));
    try std.testing.expect(fake.last_stats_only and !fake.last_flags.force);
    try std.testing.expectEqual(@as(u32, 2), fake.exports);
}

test "batch exports the ported kinds and warns for the rest" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    const res = fake.bridge();
    try fake.addBatchProject("src/nested/medal.mdc", .medal);
    try fake.addBatchProject("src/weapon.wpn", .weapon);
    try fake.addBatchProject("src/unit.unt", .animation_infantry);
    try fake.addBatchProject("elsewhere/other.mdc", .medal);
    fake.setExportable(.medal, true);

    var report: bridge.ExportReport = .{};
    var warnings: [1]bridge.Warning = .{.{}};
    try bridge.check(res.batch(null, "src/", "out", .{ .force = true }, &report, &warnings));
    try std.testing.expectEqual(@as(i32, 1), report.written);
    try std.testing.expectEqual(@as(i32, 2), report.skipped);
    // Two warnings, one slot: the total still counts both.
    try std.testing.expectEqual(@as(usize, 2), report.warning_total);
    try std.testing.expect(std.mem.indexOf(u8, warnings[0].textSlice(), "not ported yet") != null);

    try bridge.check(res.batch(.weapon, "src/", "out", .{ .open_save = true }, &report, &warnings));
    try std.testing.expectEqual(@as(i32, 1), report.written);
    try std.testing.expectEqual(@as(i32, 0), report.skipped);
    try std.testing.expectEqual(bridge.Status.data_missing, res.batch(null, "nowhere/", "out", .{}, &report, &warnings));
    try std.testing.expectEqual(bridge.Status.refused, res.batch(null, "src/", "game/Data", .{}, &report, &warnings));
    try std.testing.expectEqual(bridge.Status.bad_argument, res.batch(null, "", "out", .{}, &report, &warnings));
}

test "mod settings round-trip, refuse the shipped Data, and gate the pack" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    const res = fake.bridge();

    var current: bridge.ModSettings = .{};
    try bridge.check(res.modSettingsGet(&current));
    try std.testing.expectEqualStrings("mods/mymod", current.exportDirSlice());
    try std.testing.expectEqual(bridge.Status.refused, res.packMod("out.pak"));

    var shipped: bridge.ModSettings = .{};
    try std.testing.expect(shipped.setExportDir("game"));
    try std.testing.expectEqual(bridge.Status.refused, res.modSettingsSet(&shipped));
    try std.testing.expect(shipped.setExportDir("Game/data/"));
    try std.testing.expectEqual(bridge.Status.refused, res.modSettingsSet(&shipped));
    const empty: bridge.ModSettings = .{};
    try std.testing.expectEqual(bridge.Status.bad_argument, res.modSettingsSet(&empty));

    var mine: bridge.ModSettings = .{};
    try std.testing.expect(mine.setExportDir("mods/t10"));
    try std.testing.expect(mine.setName("T10 Mod"));
    try std.testing.expect(mine.setVersion("1.2"));
    try std.testing.expect(mine.setDesc("a test mod"));
    var too_long: [64]u8 = undefined;
    @memset(&too_long, 'n');
    try std.testing.expect(!mine.setName(&too_long));
    try bridge.check(res.modSettingsSet(&mine));
    var back: bridge.ModSettings = .{};
    try bridge.check(res.modSettingsGet(&back));
    try std.testing.expectEqualStrings("T10 Mod", back.nameSlice());
    try std.testing.expectEqualStrings("1.2", back.versionSlice());
    try std.testing.expectEqualStrings("a test mod", back.descSlice());

    try bridge.check(res.packMod("mods/t10.pak"));
    try std.testing.expect(fake.files.get("mods/t10.pak") != null);
    try std.testing.expectEqual(bridge.Status.refused, res.packMod("mods/t10/data/inside.pak"));
    try std.testing.expectEqual(bridge.Status.bad_argument, res.packMod(""));
}

test "import builds an infantry project the document mirrors, and refuses the other kinds" {
    const allocator = std.testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    const res = fake.bridge();
    try fake.addGameFolder("Data/Units/Humans/German/Gunner", "German_Gunner");

    try std.testing.expectEqual(bridge.Status.refused, res.importFromGame(.sprite, "Data/Units/Humans/German/Gunner"));
    try std.testing.expect(std.mem.indexOf(u8, res.lastMessage(), ".san") != null);
    try std.testing.expectEqual(bridge.Status.refused, res.importFromGame(.weapon, "Data/Units/Humans/German/Gunner"));
    try std.testing.expectEqual(bridge.Status.data_missing, res.importFromGame(.animation_infantry, "Data/nowhere"));
    try std.testing.expectEqual(bridge.Status.bad_argument, res.importFromGame(.animation_infantry, ""));

    try bridge.check(res.importFromGame(.animation_infantry, "Data/Units/Humans/German/Gunner"));
    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());
    try doc.refreshKind(fake.bridge());
    try std.testing.expectEqual(Kind.animation_infantry, doc.kind);
    var props: [4]PropRecord = undefined;
    var total: usize = 0;
    try bridge.check(res.props(fake.nodes.items[1].id, &props, &total));
    try std.testing.expectEqualStrings("German_Gunner", props[0].valueSlice());
    // An imported project has no file yet: export asks for a save first.
    var report: bridge.ExportReport = .{};
    fake.setExportable(.animation_infantry, true);
    try std.testing.expectEqual(bridge.Status.refused, res.exportProject(.{}, false, &report, &.{}));
}

fn meshItem(fake: *FakeResBridge, parent: i32, class_type: i32, props: []const struct { i32, []const u8 }) !i32 {
    var class: [16]u8 = undefined;
    var id: i32 = 0;
    var siblings: i32 = 0;
    for (fake.nodes.items) |n| {
        if (n.parent == parent) siblings += 1;
    }
    try bridge.check(fake.bridge().insertNode(parent, try std.fmt.bufPrint(&class, "{d}", .{class_type}), siblings, &id));
    for (fake.nodes.items) |*n| {
        if (n.id != id) continue;
        for (props) |entry| {
            var prop: PropRecord = .{ .id = entry[0] };
            _ = prop.setValue(entry[1]);
            try n.props.append(fake.allocator, prop);
        }
    }
    return id;
}

fn expectValue(doc: *Document, node: i32, prop: i32, want: []const u8, when: []const u8) !void {
    const got = sub_editor_tools.propValue(doc, node, prop).?;
    if (!std.mem.eql(u8, want, got)) {
        std.debug.print("unit property {d} of node {d} {s}: expected \"{s}\", got \"{s}\"\n", .{ prop, node, when, want, got });
        return error.TestExpectedEqual;
    }
}

test "unit locator references and a model switch undo and redo, the Locators children following the model" {
    const allocator = std.testing.allocator;
    const item = sub_editor_tools.item_type;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    try fake.addMeshModel("a.mod", &.{ .{ .name = "Base" }, .{ .name = "LMainGun1", .locator = true } });
    try fake.addMeshModel("b.mod", &.{.{ .name = "Hull" }});
    try bridge.check(fake.bridge().new(.mesh_unit));
    const root = fake.nodes.items[0].id;
    const graphics = try meshItem(&fake, root, item.mesh_graphics, &.{.{ 1, "a.mod" }});
    const locators = try meshItem(&fake, root, item.mesh_locators, &.{});
    const platforms = try meshItem(&fake, root, item.mesh_platforms, &.{});
    const platform = try meshItem(&fake, platforms, item.mesh_platform_props, &.{ .{ 1, "NA" }, .{ 2, "NA" } });
    const guns = try meshItem(&fake, platform, item.mesh_guns, &.{});
    const gun = try meshItem(&fake, guns, item.mesh_gun_props, &.{ .{ 1, "NA" }, .{ 2, "NA" } });
    try bridge.check(fake.bridge().setProp(graphics, 1, "a.mod"));

    var doc: Document = .{};
    defer doc.deinit(allocator);
    try doc.reload(allocator, fake.bridge());
    var hist: History = .{};
    defer hist.deinit(allocator);
    try std.testing.expectEqual(@as(i32, 2), sub_editor_tools.childCount(&doc, locators));

    // Locator references: set, undo, redo, each read back.
    const refs = [_]struct { node: i32, prop: i32, value: []const u8 }{
        .{ .node = gun, .prop = 1, .value = "LMainGun1" },
        .{ .node = platform, .prop = 1, .value = "Base" },
    };
    for (refs) |ref| {
        try sub_editor_tools.commit(allocator, fake.bridge(), &doc, &hist, try sub_editor_tools.setProp(allocator, &doc, ref.node, ref.prop, ref.value), 0);
        try expectValue(&doc, ref.node, ref.prop, ref.value, "after set");
        try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
        try expectValue(&doc, ref.node, ref.prop, "NA", "after undo");
        try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
        try expectValue(&doc, ref.node, ref.prop, ref.value, "after redo");
    }

    // The model switch: the children are rebuilt from b.mod, and undo brings a.mod's back.
    try sub_editor_tools.commit(allocator, fake.bridge(), &doc, &hist, try sub_editor_tools.setProp(allocator, &doc, graphics, 1, "b.mod"), 0);
    try std.testing.expectEqual(@as(i32, 1), sub_editor_tools.childCount(&doc, locators));
    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try expectValue(&doc, graphics, 1, "a.mod", "after undo of the switch");
    try std.testing.expectEqual(@as(i32, 2), sub_editor_tools.childCount(&doc, locators));
    try doc.redoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(@as(i32, 1), sub_editor_tools.childCount(&doc, locators));

    // A platform and a gun inserted and deleted.
    const platforms_before = sub_editor_tools.childCount(&doc, platforms);
    try sub_editor_tools.commit(allocator, fake.bridge(), &doc, &hist, try sub_editor_tools.appendChild(allocator, &doc, platforms, item.mesh_platform_props), 0);
    try std.testing.expectEqual(platforms_before + 1, sub_editor_tools.childCount(&doc, platforms));
    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(platforms_before, sub_editor_tools.childCount(&doc, platforms));
    try sub_editor_tools.commit(allocator, fake.bridge(), &doc, &hist, try sub_editor_tools.deleteNode(&doc, gun), 0);
    try std.testing.expectEqual(@as(i32, 0), sub_editor_tools.childCount(&doc, guns));
    try doc.undoOne(allocator, fake.bridge(), &hist.top().?.command);
    try std.testing.expectEqual(@as(i32, 1), sub_editor_tools.childCount(&doc, guns));
}
