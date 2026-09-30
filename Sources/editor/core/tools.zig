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

/// The mouse buttons held as an event arrives, as the view reads them (its
/// own down-tracking plus the motion's mask) - what a tool that changes
/// behaviour mid-gesture when buttons combine needs. The Heights tool is the
/// first: middle, or left and right together, level (DrawShadeState.cpp:217).
pub const Buttons = struct { left: bool = false, right: bool = false, middle: bool = false };

/// world_x/world_y are the scene's units, for the camera; map_x/map_y the
/// same point in the map's, which is what an object's position is in. The
/// two differ by sqrt 2 (bridge.h, BkEditorScreenToWorld): a placer handed
/// the world point put its object off the screen. screen_x/screen_y are the
/// window pixels the point was resolved from (Editor.resolve), for gestures
/// the MFC editor measures on screen: the Roads & Rivers opacity drag,
/// 100 pixels to 1.0.
/// `ctrl` is the modifier as the view read it with the event (04-07): the
/// Fence tool's flip of a single fence. It is a modifier, never a right click.
pub const Pointer = struct { world_x: f32, world_y: f32, map_x: f32, map_y: f32, tile: ?[2]i32 = null, object: ?i32 = null, screen_x: f32 = 0, screen_y: f32 = 0, ctrl: bool = false, buttons: Buttons = .{} };
/// `enter`, `insert`, `escape` and `space` are the MFC editor's keys for
/// finishing, toggling and cancelling a gesture (04-03, C13); the M1 tools
/// ignore them.
pub const Key = enum { delete, rotate_left, rotate_right, enter, insert, escape, space };
/// `right_*` are the right button's gesture (or Ctrl+left in a tool whose
/// registry entry asks for it); `double_click` arrives after the single
/// click's own press and release (SDL sends clicks 1, then clicks 2). The M1
/// tools ignore all four.
pub const Event = union(enum) {
    press: Pointer,
    drag: Pointer,
    release: Pointer,
    key: Key,
    right_press: Pointer,
    right_drag: Pointer,
    right_release: Pointer,
    double_click: Pointer,
};

/// WR-B02: where the selected item is in `items` after an undo or redo.
/// A replay that kept the list's length (`captured_len`, the length at the
/// capture) added or took away nothing, so no index moved: `index` stands,
/// even when the replay changed the item itself (a rotate, a move). One that
/// changed the length shifted the list: the item is found again by `key`, what
/// it was at the capture - `index` itself when it still holds it, else the
/// first equal one, else null (the item is gone).
pub fn refind(comptime T: type, items: []const T, index: ?usize, key: T, captured_len: usize, comptime eql: fn (T, T) bool) ?usize {
    if (index) |position| {
        if (items.len == captured_len and position < items.len) return position;
        if (position < items.len and eql(items[position], key)) return position;
    }
    for (items, 0..) |item, position| {
        if (eql(item, key)) return position;
    }
    return null;
}

/// `std.meta.eql` in the shape `refind` takes.
pub fn metaEql(comptime T: type) fn (T, T) bool {
    return struct {
        fn eql(a: T, b: T) bool {
            return std.meta.eql(a, b);
        }
    }.eql;
}

/// A sixteenth of a turn. Directions are the engine's: 65536 is a full turn
/// (SEngineObjectState::wDir is a WORD).
pub const rotate_step: i32 = 4096;
const full_turn: i32 = 65536;

pub const Brush = struct {
    tile: u8,
    /// Cells per axis, 1..16, even sizes included (M3, D-22/PARITY V4): the
    /// MFC toolbar combo's own range and its 2x2 default. M1's radius
    /// (whose ceiling was 9x9) could not name an even square at all. An
    /// even stamp hangs to the right and below the cell under the cursor -
    /// the cursor names the stamp's top-left - which is the only convention
    /// a size carries; `topLeft` is the one place that knows it.
    size: i32 = 2,
    gesture: u32 = 0,
    painted: std.AutoHashMapUnmanaged([2]i32, void) = .empty,

    /// The stamp's top-left cell for a brush of `size` over `centre`: the
    /// centre's own cell for odd sizes, the one whose right and below
    /// neighbours fill the even square out.
    pub fn topLeft(size: i32, centre: [2]i32) [2]i32 {
        const half = @divFloor(size - 1, 2);
        return .{ centre[0] - half, centre[1] - half };
    }

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
            .key, .right_press, .right_drag, .right_release, .double_click => {},
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
        const origin = topLeft(self.size, centre);
        var y = origin[1];
        while (y < origin[1] + self.size) : (y += 1) {
            var x = origin[0];
            while (x < origin[0] + self.size) : (x += 1) {
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
            .press => |pointer| editor.selection = try editor.addObject(self.name, pointer.map_x, pointer.map_y, self.dir, self.player),
            .drag, .release, .key, .right_press, .right_drag, .right_release, .double_click => {},
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
                self.grab_x = object.x - pointer.map_x;
                self.grab_y = object.y - pointer.map_y;
                self.gesture = editor.beginGesture();
            },
            .drag => |pointer| {
                if (self.gesture == 0) return;
                const link_id = editor.selection orelse return;
                const object = editor.document.find(link_id) orelse return;
                const pose: editor_mod.Pose = .{
                    .x = pointer.map_x + self.grab_x,
                    .y = pointer.map_y + self.grab_y,
                    .dir = object.dir,
                    .player = object.player,
                };
                // A position the engine will not take is skipped: the object
                // stays at the last one it took and the drag goes on. The
                // status line says why.
                editor.place(link_id, pose, self.gesture) catch |err| if (err != error.Refused) return err;
            },
            .release => self.gesture = 0,
            .right_press, .right_drag, .right_release, .double_click => {},
            .key => |key| {
                const link_id = editor.selection orelse return;
                switch (key) {
                    .enter, .insert, .escape, .space => {},
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
    var brush: Brush = .{ .tile = 7, .size = 1 };
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
    var brush: Brush = .{ .tile = 7, .size = 1 };
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

test "a brush with a size paints a square, clipped to the map" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 3, .size = 3 };
    defer brush.deinit(testing.allocator);
    try brush.handle(&editor, .{ .press = try at(&editor, 5, 5) }); // cell 0,0 at the corner
    try brush.handle(&editor, .{ .release = try at(&editor, 5, 5) });
    try testing.expectEqual(@as(u8, 3), fake.tile(0, 0));
    try testing.expectEqual(@as(u8, 3), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 0), fake.tile(2, 2));
}

test "the brush takes sizes 1..16, even sizes hanging right and below (D-22)" {
    // The MFC toolbar combo's own range (MainFrm.cpp:487-503): 1x1 and 16x16
    // are both valid, and an even square's cursor cell names its top-left.
    try testing.expectEqual([2]i32{ 0, 0 }, Brush.topLeft(1, .{ 0, 0 }));
    try testing.expectEqual([2]i32{ -1, -1 }, Brush.topLeft(3, .{ 0, 0 }));
    try testing.expectEqual([2]i32{ 0, 0 }, Brush.topLeft(2, .{ 0, 0 }));
    try testing.expectEqual([2]i32{ 4, 4 }, Brush.topLeft(2, .{ 4, 4 }));
    try testing.expectEqual([2]i32{ 4, 4 }, Brush.topLeft(4, .{ 5, 5 }));
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    // An even square hangs right and below: size 2 at cell 1,1 paints
    // 1..2 on both axes, never 0 or 3.
    var brush: Brush = .{ .tile = 3, .size = 2 };
    defer brush.deinit(testing.allocator);
    try brush.handle(&editor, .{ .press = try at(&editor, 40, 40) }); // cell 1,1
    try brush.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try testing.expectEqual(@as(u8, 3), fake.tile(1, 1));
    try testing.expectEqual(@as(u8, 3), fake.tile(2, 2));
    try testing.expectEqual(@as(u8, 0), fake.tile(0, 0));
    try testing.expectEqual(@as(u8, 0), fake.tile(3, 3));
    // And 16 - the combo's ceiling - covers the fake's whole 8x8 map from
    // anywhere: topLeft(16, 1,1) = -6,-6 .. 9,9 clipped to 0..7.
    brush.size = 16;
    try brush.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try brush.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try testing.expectEqual(@as(u8, 3), fake.tile(0, 0));
    try testing.expectEqual(@as(u8, 3), fake.tile(7, 7));
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

test "the placer and a drag take the map position under the pointer, not the world point" {
    // The real engine's map units are not its world units; a world point
    // used as an object position puts the object somewhere else than the
    // click (plan 5, Task 7.1).
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 2;
    var editor = try opened(&fake);
    defer editor.deinit();
    var placer: Placer = .{ .name = "T34" };
    try placer.handle(&editor, .{ .press = try at(&editor, 100, 60) });
    const link = editor.selection.?;
    try testing.expectEqual(@as(f32, 200), editor.document.find(link).?.x);
    try testing.expectEqual(@as(f32, 120), editor.document.find(link).?.y);
    // And a click where it was placed finds it.
    try testing.expectEqual(@as(?i32, link), (try at(&editor, 100, 60)).object);
    var selector: Selector = .{};
    try selector.handle(&editor, .{ .press = try at(&editor, 105, 60) }); // 10 map units to its right
    try selector.handle(&editor, .{ .drag = try at(&editor, 125, 70) });
    try selector.handle(&editor, .{ .release = try at(&editor, 125, 70) });
    try testing.expectEqual(@as(f32, 240), editor.document.find(link).?.x);
    try testing.expectEqual(@as(f32, 140), editor.document.find(link).?.y);
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
    try selector.handle(&editor, .{ .drag = .{ .world_x = 2, .world_y = 40, .map_x = 2, .map_y = 40 } }); // 40 - 38 would be fine
    try selector.handle(&editor, .{ .drag = .{ .world_x = -30, .world_y = 40, .map_x = -30, .map_y = 40 } }); // off the map: refused
    try testing.expectEqual(@as(f32, 2), editor.document.find(1).?.x);
    try selector.handle(&editor, .{ .drag = .{ .world_x = 20, .world_y = 40, .map_x = 20, .map_y = 40 } });
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

test "the M1 tools ignore the right button, double click and the new keys" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 7, .size = 1 };
    defer brush.deinit(testing.allocator);
    var placer: Placer = .{ .name = "T34" };
    var selector: Selector = .{};
    // The selector holds the tank, so a stray key would have something to hit.
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    const depth = editor.history.undo_stack.items.len;
    const objects = editor.document.objects.items.len;
    const pointer = try at(&editor, 40, 40);
    const events = [_]Event{
        .{ .right_press = pointer },
        .{ .right_drag = pointer },
        .{ .right_release = pointer },
        .{ .double_click = pointer },
        .{ .key = .enter },
        .{ .key = .insert },
        .{ .key = .escape },
        .{ .key = .space },
    };
    for (events) |event| {
        try brush.handle(&editor, event);
        try placer.handle(&editor, event);
        try selector.handle(&editor, event);
    }
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expectEqual(objects, editor.document.objects.items.len);
    try testing.expectEqual(@as(?i32, 1), editor.selection);
    try testing.expectEqual(@as(u32, 0), brush.gesture);
    try testing.expectEqual(@as(u32, 0), selector.gesture);
}
