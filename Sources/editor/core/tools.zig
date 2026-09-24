//! The tools turn plain input - a pointer already resolved to world, tile and
//! object, and a few keys - into Editor calls. They hold only what a gesture
//! needs between events; everything else is in the Editor.
const std = @import("std");
const editor_mod = @import("editor.zig");
const fake_mod = @import("fake_bridge.zig");
const bridge_mod = @import("bridge.zig");
const Editor = editor_mod.Editor;
const EditError = bridge_mod.EditError;
const PaintCell = bridge_mod.PaintCell;

pub const Pointer = struct { world_x: f32, world_y: f32, tile: ?[2]i32 = null, object: ?i32 = null };
pub const Key = enum { delete, rotate_left, rotate_right };
pub const Event = union(enum) { press: Pointer, drag: Pointer, release: Pointer, key: Key };

/// A sixteenth of a turn. Directions are the engine's: 65536 is a full turn
/// (SEngineObjectState::wDir is a WORD).
pub const rotate_step: i32 = 4096;
const full_turn: i32 = 65536;

pub const Brush = struct {
    tile: u8,
    /// 0 is one cell, 1 a 3x3 square, and so on.
    radius: i32 = 0,
    gesture: u32 = 0,
    painted: std.AutoHashMapUnmanaged([2]i32, void) = .empty,

    pub fn deinit(self: *Brush, allocator: std.mem.Allocator) void {
        self.painted.deinit(allocator);
        self.* = undefined;
    }

    pub fn handle(self: *Brush, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                self.gesture = editor.beginGesture();
                self.painted.clearRetainingCapacity();
                try self.stamp(editor, pointer);
            },
            .drag => |pointer| if (self.gesture != 0) try self.stamp(editor, pointer),
            .release => {
                self.gesture = 0;
                self.painted.clearRetainingCapacity();
            },
            .key => {},
        }
    }

    /// Paints the cells under the brush that this stroke has not painted yet,
    /// in one bridge call: the terrain function runs once per call, and each
    /// run is an undo record in the bridge. A cell counts as painted only once
    /// the bridge has taken it, so a refused stamp can be painted again by
    /// the same stroke.
    fn stamp(self: *Brush, editor: *Editor, pointer: Pointer) EditError!void {
        const centre = pointer.tile orelse return;
        const allocator = editor.allocator;
        var cells: std.ArrayListUnmanaged(PaintCell) = .empty;
        defer cells.deinit(allocator);
        const width = editor.document.info.width_tiles;
        const height = editor.document.info.height_tiles;
        var y = centre[1] - self.radius;
        while (y <= centre[1] + self.radius) : (y += 1) {
            var x = centre[0] - self.radius;
            while (x <= centre[0] + self.radius) : (x += 1) {
                if (x < 0 or y < 0 or x >= width or y >= height) continue;
                const cell = [2]i32{ x, y };
                if (self.painted.contains(cell)) continue;
                try cells.append(allocator, .{ .x = @intCast(x), .y = @intCast(y), .tile = self.tile });
            }
        }
        if (cells.items.len == 0) return;
        // Room to mark them first: once the bridge has painted, marking must
        // not be able to fail.
        try self.painted.ensureUnusedCapacity(allocator, @intCast(cells.items.len));
        try editor.paint(cells.items, self.gesture);
        for (cells.items) |cell| self.painted.putAssumeCapacity(.{ cell.x, cell.y }, {});
    }
};

pub const Placer = struct {
    name: []const u8,
    dir: i32 = 0,
    player: i32 = 0,

    pub fn handle(self: *Placer, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| editor.selection = try editor.addObject(self.name, pointer.world_x, pointer.world_y, self.dir, self.player),
            else => {},
        }
    }
};

pub const Selector = struct {
    gesture: u32 = 0,
    grab_x: f32 = 0,
    grab_y: f32 = 0,

    pub fn handle(self: *Selector, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                const link_id = pointer.object orelse {
                    editor.selection = null;
                    self.gesture = 0;
                    return;
                };
                const object = editor.document.find(link_id) orelse return;
                editor.selection = link_id;
                self.grab_x = object.x - pointer.world_x;
                self.grab_y = object.y - pointer.world_y;
                self.gesture = editor.beginGesture();
            },
            .drag => |pointer| {
                if (self.gesture == 0) return;
                const link_id = editor.selection orelse return;
                const object = editor.document.find(link_id) orelse return;
                const pose: editor_mod.Pose = .{
                    .x = pointer.world_x + self.grab_x,
                    .y = pointer.world_y + self.grab_y,
                    .dir = object.dir,
                    .player = object.player,
                };
                // A position the engine will not take is skipped: the object
                // stays at the last one it took and the drag goes on. The
                // status line says why.
                editor.place(link_id, pose, self.gesture) catch |err| if (err != error.Refused) return err;
            },
            .release => self.gesture = 0,
            .key => |key| {
                const link_id = editor.selection orelse return;
                switch (key) {
                    .delete => try editor.delete(link_id),
                    .rotate_left, .rotate_right => {
                        const object = editor.document.find(link_id) orelse return;
                        const step: i32 = if (key == .rotate_left) -rotate_step else rotate_step;
                        try editor.place(link_id, .{
                            .x = object.x,
                            .y = object.y,
                            .dir = @mod(object.dir + step, full_turn),
                            .player = object.player,
                        }, 0);
                    },
                }
            },
        }
    }
};

const testing = std.testing;
const testFixture = editor_mod.testFixture;

fn opened(fake: *fake_mod.FakeBridge) !Editor {
    var editor = Editor.init(testing.allocator, fake.bridge());
    errdefer editor.deinit();
    try editor.open("fixture.bzm");
    return editor;
}

fn at(editor: *Editor, x: f32, y: f32) !Pointer {
    return editor.resolve(x, y);
}

test "a brush drag paints the cells it crossed, once each, as one undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 7, .radius = 0 };
    defer brush.deinit(testing.allocator);
    try brush.handle(&editor, .{ .press = try at(&editor, 40, 40) }); // cell 1,1
    try brush.handle(&editor, .{ .drag = try at(&editor, 45, 40) }); // still 1,1: no paint
    try brush.handle(&editor, .{ .drag = try at(&editor, 70, 40) }); // cell 2,1
    try brush.handle(&editor, .{ .release = try at(&editor, 70, 40) });
    try testing.expectEqual(@as(u8, 7), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 7), fake.tile(2, 1));
    var paints: usize = 0;
    for (fake.calls.items) |call| {
        if (call.kind == .paint) paints += 1;
    }
    try testing.expectEqual(@as(usize, 2), paints);
    _ = try editor.undo();
    try testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), fake.tile(2, 1));
}

test "a refused stamp leaves its cells for the same stroke to paint again" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 7, .radius = 0 };
    defer brush.deinit(testing.allocator);
    fake.refuse_paints = true;
    try testing.expectError(error.Refused, brush.handle(&editor, .{ .press = try at(&editor, 40, 40) })); // cell 1,1
    try testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    fake.refuse_paints = false;
    try brush.handle(&editor, .{ .drag = try at(&editor, 45, 40) }); // still 1,1, which was never painted
    try brush.handle(&editor, .{ .release = try at(&editor, 45, 40) });
    try testing.expectEqual(@as(u8, 7), fake.tile(1, 1));
    _ = try editor.undo();
    try testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
}

test "a brush with a radius paints a square, clipped to the map" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 3, .radius = 1 };
    defer brush.deinit(testing.allocator);
    try brush.handle(&editor, .{ .press = try at(&editor, 5, 5) }); // cell 0,0 at the corner
    try brush.handle(&editor, .{ .release = try at(&editor, 5, 5) });
    try testing.expectEqual(@as(u8, 3), fake.tile(0, 0));
    try testing.expectEqual(@as(u8, 3), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), fake.tile(2, 2));
}

test "the placer adds and selects" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var placer: Placer = .{ .name = "T34", .dir = 0, .player = 1 };
    try placer.handle(&editor, .{ .press = try at(&editor, 200, 200) });
    const link = editor.selection.?;
    try testing.expectEqual(@as(i32, 1), editor.document.find(link).?.player);
}

test "drag-move moves, keeps the grab offset, and is one undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 45, 40) }); // grabs T34 at 40,40, 5 to its right
    try testing.expectEqual(@as(?i32, 1), editor.selection);
    try selector.handle(&editor, .{ .drag = try at(&editor, 65, 60) });
    try selector.handle(&editor, .{ .drag = try at(&editor, 85, 80) });
    try selector.handle(&editor, .{ .release = try at(&editor, 85, 80) });
    try testing.expectEqual(@as(f32, 80), editor.document.find(1).?.x);
    try testing.expectEqual(@as(f32, 80), editor.document.find(1).?.y);
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
}

test "a refused position mid-drag is skipped, not the end of the drag" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .drag = .{ .world_x = 2, .world_y = 40 } }); // 40 - 38 would be fine
    try selector.handle(&editor, .{ .drag = .{ .world_x = -30, .world_y = 40 } }); // off the map: refused
    try testing.expectEqual(@as(f32, 2), editor.document.find(1).?.x);
    try selector.handle(&editor, .{ .drag = .{ .world_x = 20, .world_y = 40 } });
    try testing.expectEqual(@as(f32, 20), editor.document.find(1).?.x);
}

test "delete then undo restores the same object; a refused delete keeps the selection" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 100, 40) }); // the referenced span
    try selector.handle(&editor, .{ .release = try at(&editor, 100, 40) });
    try testing.expectError(error.Refused, selector.handle(&editor, .{ .key = .delete }));
    try testing.expectEqual(@as(?i32, 2), editor.selection);
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(?i32, null), editor.selection);
    _ = try editor.undo();
    try testing.expectEqual(@as(i32, 0), editor.document.find(1).?.player);
}

test "rotate turns by a step and wraps" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .key = .rotate_left });
    try testing.expectEqual(@as(i32, 65536 - rotate_step), editor.document.find(1).?.dir);
    try selector.handle(&editor, .{ .key = .rotate_right });
    try testing.expectEqual(@as(i32, 0), editor.document.find(1).?.dir);
}

test "a press on nothing clears the selection" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .press = try at(&editor, 220, 220) });
    try testing.expectEqual(@as(?i32, null), editor.selection);
}
