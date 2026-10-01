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
            .press => |pointer| {
                // Fit Objects To Grid (M3, D-20): the MFC's placer asks the
                // fit before it edits (ObjectPlacerState.cpp:355-365), and so
                // does this - the map only ever holds a position someone
                // meant, and undo replays it raw.
                const where = editor.snapToGrid(self.name, pointer.map_x, pointer.map_y);
                editor.selection = try editor.addObject(self.name, where.x, where.y, self.dir, self.player);
                editor.selection_set.clearRetainingCapacity();
            },
            .drag, .release, .key, .right_press, .right_drag, .right_release, .double_click => {},
        }
    }
};

pub const Selector = struct {
    gesture: u32 = 0,
    grab_x: f32 = 0,
    grab_y: f32 = 0,
    /// The selection as it stood at the press (M3, D-25): each member's map
    /// position, so a drag frame's target is grab + (pointer - press) exactly
    /// and nothing drifts however many frames the drag takes. Squads keep
    /// their offsets by construction - every member moves by its own
    /// (snapped) delta.
    grabbed: std.ArrayListUnmanaged(Grabbed) = .empty,
    press_x: f32 = 0,
    press_y: f32 = 0,
    /// The rubber band (M3, D-25): set by a press on empty ground, drawn by
    /// the markers while it lives, finished by the release. `tile` is the
    /// last tile the drag saw - the Ctrl variant's rectangle is a tile
    /// rectangle, and a drag that ends off the map keeps the last tile it
    /// crossed, the MFC's own ValidatePoint clamp (ObjectPlacerState.cpp:479).
    band: ?Band = null,
    /// The objects under the press, in the pick's own order: the right click
    /// cycles them while the left button is held (the MFC's m_pickedObjects
    /// and m_curPickNum).
    cycle: std.ArrayListUnmanaged(i32) = .empty,
    cycle_index: usize = 0,

    pub const Grabbed = struct { link_id: i32, x: f32, y: f32 };

    /// A rubber band in progress: where it began, on screen for the plain
    /// band and the last tile for the Ctrl one, and which kind it is.
    pub const Band = struct { screen_x: f32, screen_y: f32, tile: ?[2]i32, ctrl: bool };

    /// A press's pick candidates (M3, D-25): a small screen rectangle around
    /// the point, through the band pick - every object whose visual meets it,
    /// in the pick's own order. Empty when nothing is there.
    fn candidatesUnder(editor: *Editor, pointer: Pointer) EditError![]i32 {
        const r: f32 = 6; // screen pixels half a box, a click's own reach
        return pickObjects(editor, pointer.screen_x - r, pointer.screen_y - r, pointer.screen_x + r, pointer.screen_y + r);
    }

    pub fn deinit(self: *Selector, allocator: std.mem.Allocator) void {
        self.grabbed.deinit(allocator);
        self.cycle.deinit(allocator);
        self.* = undefined;
    }

    fn beginMove(self: *Selector, editor: *Editor, pointer: Pointer) EditError!void {
        const members = try editor.selectionMembers(editor.allocator);
        defer editor.allocator.free(members);
        try self.grabbed.ensureUnusedCapacity(editor.allocator, members.len);
        self.grabbed.clearRetainingCapacity();
        for (members) |link_id| {
            const object = editor.document.find(link_id) orelse continue;
            self.grabbed.appendAssumeCapacity(.{ .link_id = link_id, .x = object.x, .y = object.y });
        }
        self.press_x = pointer.map_x;
        self.press_y = pointer.map_y;
        self.gesture = editor.beginGesture();
    }

    pub fn handle(self: *Selector, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                // The cycle candidates of this press, whatever the press then
                // does with them (the MFC keeps m_pickedObjects at the press
                // and the right click walks them).
                self.cycle.clearRetainingCapacity();
                if (candidatesUnder(editor, pointer)) |found| {
                    defer editor.allocator.free(found);
                    self.cycle.appendSlice(editor.allocator, found) catch {};
                    self.cycle_index = 0;
                } else |err| if (err != error.Refused) return err;
                const link_id = pointer.object orelse {
                    // Empty ground: the press clears and starts the band (the
                    // MFC's ClearAllSelection and Begin, ObjectPlacerState.cpp:510-520).
                    editor.selection = null;
                    editor.clearSelection();
                    self.gesture = 0;
                    self.band = .{ .screen_x = pointer.screen_x, .screen_y = pointer.screen_y, .tile = pointer.tile, .ctrl = pointer.ctrl };
                    return;
                };
                self.band = null;
                if (pointer.ctrl) {
                    // Ctrl+click toggles the clicked object (D-25; the MFC's
                    // Ctrl path only ever adds, the toggle takes it back out
                    // too) - even on a member of the selection, which is the
                    // only way out of one. A Ctrl press never moves.
                    editor.selectionToggle(link_id);
                    self.gesture = 0;
                    return;
                }
                if (editor.isSelected(link_id) and editor.selectionCount() > 1) {
                    // A plain press on the selection moves it, whole (the
                    // MFC's m_ifCanMovingMultiGroup branch, ObjectPlacerState.cpp:436-445).
                } else {
                    editor.selectOnly(link_id);
                }
                try self.beginMove(editor, pointer);
            },
            .drag => |pointer| {
                if (self.band) |*band| {
                    if (pointer.tile) |tile| band.tile = tile;
                    return;
                }
                if (self.gesture == 0 or self.grabbed.items.len == 0) return;
                // A refused frame (a member the engine will not move, most
                // often one that would leave the map) is skipped: the whole
                // selection stays at the last position it took and the drag
                // goes on - the batch move is all-or-nothing per frame, so
                // the frame is skipped whole.
                self.dragMove(editor, pointer) catch |err| if (err != error.Refused) return err;
            },
            .release => |pointer| {
                if (self.band) |band| {
                    try finishBand(editor, band, pointer);
                    self.band = null;
                }
                self.gesture = 0;
                self.grabbed.clearRetainingCapacity();
            },
            .right_press => |pointer| {
                // Right click alone deselects; with the left button held it
                // cycles the press's candidates (D-25, ObjectPlacerState.cpp:1050-1066).
                if (pointer.buttons.left) {
                    if (self.cycle.items.len == 0) return;
                    self.cycle_index = (self.cycle_index + 1) % self.cycle.items.len;
                    const next = self.cycle.items[self.cycle_index];
                    editor.selectOnly(next);
                    // The drag continues from the object just cycled to: its
                    // grab is where the pointer is now.
                    self.beginMove(editor, pointer) catch |err| switch (err) {
                        error.OutOfMemory => return err,
                        else => {},
                    };
                } else {
                    editor.selection = null;
                    editor.clearSelection();
                    self.gesture = 0;
                    self.grabbed.clearRetainingCapacity();
                }
            },
            .right_drag, .right_release, .double_click => {},
            .key => |key| {
                const link_id = editor.selection orelse return;
                switch (key) {
                    .enter, .insert, .escape, .space => {},
                    .delete => {
                        // Delete removes the whole selection, every member
                        // through the M2 cascade, as ONE undo step (D-25).
                        const members = try editor.selectionMembers(editor.allocator);
                        defer editor.allocator.free(members);
                        if (members.len > 1) {
                            try editor.deleteMany(members);
                        } else {
                            try editor.delete(link_id);
                        }
                    },
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

    /// One drag frame of a move: every member's target is its own grab plus
    /// the pointer's travel, non-units snapped by the same placement rule a
    /// single drag asks (the fit answers units and squads with the input),
    /// and members whose frame delta agrees move in one batch call each, so
    /// a frame is one or two bridge edits that the gesture merges into the
    /// one undo step. A member whose raw pose is where it already stands is
    /// this frame's click settling, not a move (the M2 rule), and a refused
    /// frame is skipped, not the end of the drag.
    fn dragMove(self: *Selector, editor: *Editor, pointer: Pointer) EditError!void {
        const Move = struct { link_id: i32, dx: f32, dy: f32 };
        var moves: std.ArrayListUnmanaged(Move) = .empty;
        defer moves.deinit(editor.allocator);
        for (self.grabbed.items) |*grab| {
            const object = editor.document.find(grab.link_id) orelse continue;
            const raw_x = grab.x + (pointer.map_x - self.press_x);
            const raw_y = grab.y + (pointer.map_y - self.press_y);
            if (raw_x == object.x and raw_y == object.y) continue;
            const fit = editor.snapToGrid(object.nameSlice(), raw_x, raw_y);
            const dx = fit.x - object.x;
            const dy = fit.y - object.y;
            if (dx == 0 and dy == 0) continue;
            moves.append(editor.allocator, .{ .link_id = grab.link_id, .dx = dx, .dy = dy }) catch return error.OutOfMemory;
        }
        var group: std.ArrayListUnmanaged(i32) = .empty;
        defer group.deinit(editor.allocator);
        var index: usize = 0;
        while (index < moves.items.len) {
            group.clearRetainingCapacity();
            group.append(editor.allocator, moves.items[index].link_id) catch return error.OutOfMemory;
            const dx = moves.items[index].dx;
            const dy = moves.items[index].dy;
            index += 1;
            while (index < moves.items.len and moves.items[index].dx == dx and moves.items[index].dy == dy) : (index += 1) {
                group.append(editor.allocator, moves.items[index].link_id) catch return error.OutOfMemory;
            }
            try editor.moveSelection(group.items, dx, dy, self.gesture);
        }
    }

    /// The band's release (M3, D-25): the plain band picks by screen
    /// rectangle, the Ctrl one by tile rectangle over map cells (bridges and
    /// entrenchments passed over, the pick filters them); either replaces the
    /// selection. Nothing picked clears it, as the MFC's band does.
    fn finishBand(editor: *Editor, band: Band, pointer: Pointer) EditError!void {
        if (band.ctrl) {
            const end_tile = pointer.tile orelse band.tile orelse return;
            const start_tile = band.tile orelse end_tile;
            const members = try pickObjectsInTiles(editor, start_tile, end_tile);
            defer editor.allocator.free(members);
            editor.selectionReplace(members);
        } else {
            const members = try pickObjects(editor, band.screen_x, band.screen_y, pointer.screen_x, pointer.screen_y);
            defer editor.allocator.free(members);
            editor.selectionReplace(members);
        }
    }
};

/// The band pick, two passes (M3, D-25): the sizing call gives the total, a
/// refusal for a short buffer is retried once with room. Empty answers an
/// empty slice; a refusal that is not the two-pass kind comes back as one.
fn pickObjects(editor: *Editor, sx0: f32, sy0: f32, sx1: f32, sy1: f32) EditError![]i32 {
    var total: usize = 0;
    var none: [0]i32 = .{};
    const sizing = editor.bridge.pickObjects(sx0, sy0, sx1, sy1, &none, &total);
    if (sizing != .ok and sizing != .refused) return error.Failed;
    const out = try editor.allocator.alloc(i32, total);
    errdefer editor.allocator.free(out);
    var got: usize = 0;
    try bridge_mod.check(editor.bridge.pickObjects(sx0, sy0, sx1, sy1, out, &got));
    if (got != total) return error.Failed;
    return out;
}

/// The Ctrl band's pick, the same two passes over a tile rectangle.
fn pickObjectsInTiles(editor: *Editor, start_tile: [2]i32, end_tile: [2]i32) EditError![]i32 {
    var total: usize = 0;
    var none: [0]i32 = .{};
    const sizing = editor.bridge.pickObjectsInTiles(start_tile[0], start_tile[1], end_tile[0], end_tile[1], &none, &total);
    if (sizing != .ok and sizing != .refused) return error.Failed;
    const out = try editor.allocator.alloc(i32, total);
    errdefer editor.allocator.free(out);
    var got: usize = 0;
    try bridge_mod.check(editor.bridge.pickObjectsInTiles(start_tile[0], start_tile[1], end_tile[0], end_tile[1], out, &got));
    if (got != total) return error.Failed;
    return out;
}

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
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
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
    defer selector.deinit(testing.allocator);
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
    defer selector.deinit(testing.allocator);
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
    defer selector.deinit(testing.allocator);
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
    defer selector.deinit(testing.allocator);
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
    defer selector.deinit(testing.allocator);
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
    defer selector.deinit(testing.allocator);
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .press = try at(&editor, 220, 220) });
    try testing.expectEqual(@as(?i32, null), editor.selection);
}

test "the brush and the placer ignore the right button, double click and the new keys" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var brush: Brush = .{ .tile = 7, .size = 1 };
    defer brush.deinit(testing.allocator);
    var placer: Placer = .{ .name = "T34" };
    try placer.handle(&editor, .{ .press = try at(&editor, 200, 200) });
    const depth = editor.history.undo_stack.items.len;
    const objects = editor.document.objects.items.len;
    const pointer = try at(&editor, 200, 200);
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
    }
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expectEqual(objects, editor.document.objects.items.len);
    try testing.expectEqual(@as(u32, 0), brush.gesture);
}

test "the selector's right click alone deselects, and a right click with the left held cycles" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    const depth = editor.history.undo_stack.items.len;
    // Right press alone: the selection goes, nothing else changes.
    try selector.handle(&editor, .{ .right_press = try at(&editor, 40, 40) });
    try testing.expect(editor.selection == null);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    // With the left held, the right press cycles the press's candidates -
    // the same object here, so the selection comes back to it.
    var held = try at(&editor, 40, 40);
    held.buttons.left = true;
    try selector.handle(&editor, .{ .press = held });
    try selector.handle(&editor, .{ .right_press = held });
    try testing.expectEqual(@as(?i32, 1), editor.selection);
    try selector.handle(&editor, .{ .release = held });
}

test "Ctrl+click toggles members, a plain click replaces, and a press on nothing clears the whole set" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const second = try editor.addObject("T34", 90, 90, 0, 0);
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
    // A plain click: the set is exactly the clicked object.
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try testing.expectEqual(@as(usize, 1), editor.selectionCount());
    // Ctrl+click adds the second.
    var ctrl = try at(&editor, 90, 90);
    ctrl.ctrl = true;
    try selector.handle(&editor, .{ .press = ctrl });
    try selector.handle(&editor, .{ .release = ctrl });
    try testing.expectEqual(@as(usize, 2), editor.selectionCount());
    try testing.expect(editor.isSelected(1) and editor.isSelected(second));
    // Ctrl+click again takes it back out; the anchor moves to the member
    // that stays.
    try selector.handle(&editor, .{ .press = ctrl });
    try selector.handle(&editor, .{ .release = ctrl });
    try testing.expectEqual(@as(usize, 1), editor.selectionCount());
    try testing.expectEqual(@as(?i32, 1), editor.selection);
    // A plain click replaces the whole set.
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    try selector.handle(&editor, .{ .release = try at(&editor, 40, 40) });
    try testing.expectEqual(@as(usize, 1), editor.selectionCount());
    // A press on nothing clears it.
    try selector.handle(&editor, .{ .press = try at(&editor, 5, 5) });
    try testing.expectEqual(@as(usize, 0), editor.selectionCount());
    try testing.expect(editor.selection == null);
}

test "the band on empty ground selects by screen rectangle" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    _ = try editor.addObject("T34", 90, 90, 0, 0); // both inside the band
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
    try selector.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try testing.expectEqual(@as(usize, 0), editor.selectionCount());
    try selector.handle(&editor, .{ .drag = try at(&editor, 60, 60) });
    try selector.handle(&editor, .{ .release = try at(&editor, 96, 96) });
    // The tank and the added one are in the (20..96) box; the referenced
    // span at 100,40 is not (100 > 96), the unknown is never drawn.
    try testing.expectEqual(@as(usize, 2), editor.selectionCount());
    try testing.expect(editor.isSelected(1));
}

test "the Ctrl band selects by tile rectangle, passing the span over" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    _ = try editor.addObject("T34", 90, 90, 0, 0); // tile 2,2 - in the band
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
    var press = try at(&editor, 20, 20); // tile 0,0
    press.ctrl = true;
    var release = try at(&editor, 100, 90); // tile 3,2 - the span's tile, too
    release.ctrl = true;
    try selector.handle(&editor, .{ .press = press });
    try selector.handle(&editor, .{ .drag = try at(&editor, 60, 60) });
    try selector.handle(&editor, .{ .release = release });
    // Tiles 0..3 x 0..2 hold the two tanks and - but for the rule - the
    // bridge span at tile 3,1. The span is passed over; the two remain.
    try testing.expectEqual(@as(usize, 2), editor.selectionCount());
    try testing.expect(!editor.isSelected(2));
}

test "a click on a squad member selects the whole squad" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const squad = try editor.addObject("Rifle_Squad", 60, 60, 0, 0);
    const soldier = try editor.addObject("US_sniper", 64, 64, 0, 0);
    try fake.addSquadMemberFixture(soldier, squad);
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
    // The click lands on the soldier; the pick answers the squad's link, as
    // the whole squad is what the map holds and what gets selected.
    try selector.handle(&editor, .{ .press = try at(&editor, 64, 64) });
    try selector.handle(&editor, .{ .release = try at(&editor, 64, 64) });
    try testing.expectEqual(@as(usize, 1), editor.selectionCount());
    try testing.expectEqual(@as(?i32, squad), editor.selection);
}

test "a group drag moves the whole selection as one undo step, and undo restores every member" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const second = try editor.addObject("T34", 90, 90, 0, 0);
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    var ctrl = try at(&editor, 90, 90);
    ctrl.ctrl = true;
    try selector.handle(&editor, .{ .press = ctrl });
    try selector.handle(&editor, .{ .release = ctrl });
    try testing.expectEqual(@as(usize, 2), editor.selectionCount());
    const depth_before_move = editor.history.undo_stack.items.len;
    // A press on the selection moves it whole; squads keep their offsets by
    // the same delta per member.
    try selector.handle(&editor, .{ .press = try at(&editor, 42, 42) });
    try selector.handle(&editor, .{ .drag = try at(&editor, 62, 62) });
    try selector.handle(&editor, .{ .release = try at(&editor, 62, 62) });
    try testing.expectEqual(@as(f32, 60), editor.document.find(1).?.x);
    try testing.expectEqual(@as(f32, 110), editor.document.find(second).?.x);
    try testing.expectEqual(depth_before_move + 1, editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
    try testing.expectEqual(@as(f32, 90), editor.document.find(second).?.x);
}

test "Delete takes the whole selection through the cascade as one undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const second = try editor.addObject("T34", 90, 90, 0, 0);
    try fake.addStartCommandFixture(&.{second}, 0); // a cascade reference on the second member
    var selector: Selector = .{};
    defer selector.deinit(testing.allocator);
    try selector.handle(&editor, .{ .press = try at(&editor, 40, 40) });
    var ctrl = try at(&editor, 90, 90);
    ctrl.ctrl = true;
    try selector.handle(&editor, .{ .press = ctrl });
    try selector.handle(&editor, .{ .release = ctrl });
    try selector.handle(&editor, .{ .release = try at(&editor, 90, 90) });
    const depth_before_delete = editor.history.undo_stack.items.len;
    try selector.handle(&editor, .{ .key = .delete });
    try testing.expect(editor.document.find(1) == null);
    try testing.expect(editor.document.find(second) == null);
    try testing.expectEqual(depth_before_delete + 1, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 0), fake.start_commands.items.len);
    _ = try editor.undo();
    try testing.expect(editor.document.find(1) != null);
    try testing.expect(editor.document.find(second) != null);
    try testing.expectEqual(@as(usize, 1), fake.start_commands.items.len);
}
