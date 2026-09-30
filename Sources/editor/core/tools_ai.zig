//! The AI-side tools (04-10 on). The Script Areas tool (D-21), the MFC editor's
//! area tab (Sources/src/MapEditor/MapToolState.cpp, TabToolsDialog.cpp): a drag
//! draws a rectangle (the drag's two corners) or a circle (its centre and a point on
//! its edge) and names it; the map's areas are listed in a panel, which also renames
//! and deletes; and - beyond the MFC editor, which only drew, added and deleted - the
//! selected area moves by its centre handle and resizes by its corner or edge handle.
//!
//! Drags are world (Vis) units, the pointer's `world_x`/`world_y`; an area is stored
//! in map (AI) units. The bridge converts, once, with the MFC truncation
//! (`Editor.scriptAreaFromVis`, `scriptAreaMoved`, `scriptAreaResized`); the tool
//! never converts a length itself. A handle's place is compared with the pointer in
//! map units (`Pointer.map_x/y`), the units the area is stored in.
//!
//! One undo step each: an area added, an area deleted, a rename, and a whole move or
//! resize drag (its edits merge by gesture; a position the bridge refuses mid-drag is
//! skipped and the drag goes on, as the Selector does).
const std = @import("std");
const editor_mod = @import("editor.zig");
const fake_mod = @import("fake_bridge.zig");
const bridge_mod = @import("bridge.zig");
const records = @import("records.zig");
const tools = @import("tools.zig");
const Editor = editor_mod.Editor;
const EditError = bridge_mod.EditError;
const Event = tools.Event;
const Pointer = tools.Pointer;

/// How far (screen pixels) a press may move and still be a click.
pub const click_pixels: f32 = 4.0;

/// How near (map units) a press must be to a handle to grab it: about a third of a
/// tile, which at the game's default zoom is a few pixels beyond the handle's drawn
/// square.
pub const handle_radius: f32 = 24.0;

/// The selected area's centre handle, map units: its centre.
pub fn centreHandle(area: records.ScriptArea) [2]f32 {
    return .{ area.cx, area.cy };
}

/// The edge handle, map units: a rectangle's (+, +) corner, a circle's rightmost
/// point. Dragging it anywhere sets the size from its distance to the centre
/// (`Editor.scriptAreaResized`).
pub fn edgeHandle(area: records.ScriptArea) [2]f32 {
    return switch (area.shape) {
        .rectangle => .{ area.cx + area.hx, area.cy + area.hy },
        .circle => .{ area.cx + area.r, area.cy },
    };
}

fn near(handle: [2]f32, pointer: Pointer) bool {
    return std.math.hypot(handle[0] - pointer.map_x, handle[1] - pointer.map_y) <= handle_radius;
}

/// Whether the map point is inside the area.
pub fn contains(area: records.ScriptArea, map_x: f32, map_y: f32) bool {
    return switch (area.shape) {
        .rectangle => @abs(map_x - area.cx) <= area.hx and @abs(map_y - area.cy) <= area.hy,
        .circle => std.math.hypot(map_x - area.cx, map_y - area.cy) <= area.r,
    };
}

pub const ScriptAreas = struct {
    /// The shape the next drag draws; set by the panel.
    shape: records.AreaShape = .rectangle,
    /// The name for the next area, from the panel's field. Empty: the first free
    /// "area_<n>". Cleared once an area takes it.
    name_buffer: [records.area_name_capacity]u8 = [_]u8{0} ** records.area_name_capacity,
    name_len: usize = 0,
    /// The area the panel's list and the handles work on: an index into the map's
    /// list, the last area drawn or clicked. Follows an undo that removes it.
    selected: ?usize = null,
    /// A drawing drag: where it began and where it is now, world units; null
    /// between drags. The app draws the ghost from these two.
    start: ?[2]f32 = null,
    current: ?[2]f32 = null,
    dragging: bool = false,
    press_screen: [2]f32 = .{ 0, 0 },
    /// A handle drag: which, the gesture its edits merge under, the area as it was
    /// when the drag began (size and name kept from it) and, for a move, the pointer's
    /// offset from the centre in world units.
    handle_drag: HandleDrag = .none,
    gesture: u32 = 0,
    grab_area: records.ScriptArea = .{},
    grab_world: [2]f32 = .{ 0, 0 },

    pub const HandleDrag = enum { none, move, resize };

    pub fn setName(self: *ScriptAreas, text: []const u8) void {
        const len = @min(text.len, self.name_buffer.len - 1);
        @memset(&self.name_buffer, 0);
        @memcpy(self.name_buffer[0..len], text[0..len]);
        self.name_len = len;
    }

    pub fn name(self: *const ScriptAreas) []const u8 {
        return self.name_buffer[0..self.name_len];
    }

    /// Forgets the drag and the selection, as a map change must; shape and name stay.
    pub fn reset(self: *ScriptAreas) void {
        self.start = null;
        self.current = null;
        self.dragging = false;
        self.handle_drag = .none;
        self.gesture = 0;
        self.selected = null;
    }

    /// The name the next area takes: the panel's field, or the first "area_<n>"
    /// (n from 1) no area of the map has, in `buffer`.
    pub fn nameFor(self: *const ScriptAreas, editor: *Editor, buffer: *[records.area_name_capacity]u8) EditError![]const u8 {
        if (self.name_len != 0) return self.name();
        const areas = try editor.scriptAreas(editor.allocator);
        defer editor.allocator.free(areas);
        var number: usize = 1;
        while (true) : (number += 1) {
            const candidate = std.fmt.bufPrint(buffer, "area_{d}", .{number}) catch return error.Failed;
            var taken = false;
            for (areas) |*area| {
                if (std.mem.eql(u8, area.nameSlice(), candidate)) taken = true;
            }
            if (!taken) return candidate;
        }
    }

    /// The selected area, or null when none is selected or the index no longer names
    /// one (an undo took it away).
    pub fn selectedArea(self: *ScriptAreas, editor: *Editor) ?records.ScriptArea {
        const index = self.selected orelse return null;
        var value: records.Value = undefined;
        if (editor.bridge.readRecord(.script_area, @intCast(index), editor.allocator, &value) != .ok) {
            self.selected = null;
            return null;
        }
        return value.script_area;
    }

    /// AI units per world unit as the bridge answers them, for the grab offset of a move.
    fn mapPerWorld(editor: *Editor) f32 {
        var map_x: f32 = 0;
        var map_y: f32 = 0;
        if (editor.bridge.worldToMap(1000, 0, &map_x, &map_y) != .ok or map_x <= 0) return 1.4142135;
        return map_x / 1000;
    }

    pub fn handle(self: *ScriptAreas, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| try self.press(editor, pointer),
            .drag => |pointer| try self.drag(editor, pointer),
            .release => |pointer| try self.release(editor, pointer),
            .key => |key| switch (key) {
                .delete => {
                    const index = self.selected orelse {
                        editor.note("select a script area first");
                        return;
                    };
                    self.selected = null;
                    try editor.deleteScriptArea(index);
                },
                .escape => {
                    self.start = null;
                    self.current = null;
                    self.dragging = false;
                    self.handle_drag = .none;
                    self.gesture = 0;
                },
                else => {},
            },
            else => {},
        }
    }

    fn press(self: *ScriptAreas, editor: *Editor, pointer: Pointer) EditError!void {
        self.press_screen = .{ pointer.screen_x, pointer.screen_y };
        if (self.selectedArea(editor)) |area| {
            // The centre handle wins over the edge handle of a small area.
            const grabbed: HandleDrag = if (near(centreHandle(area), pointer)) .move else if (near(edgeHandle(area), pointer)) .resize else .none;
            if (grabbed != .none) {
                self.handle_drag = grabbed;
                self.gesture = editor.beginGesture();
                self.grab_area = area;
                const ratio = mapPerWorld(editor);
                self.grab_world = .{ area.cx / ratio - pointer.world_x, area.cy / ratio - pointer.world_y };
                return;
            }
        }
        self.start = .{ pointer.world_x, pointer.world_y };
        self.current = self.start;
        self.dragging = true;
    }

    fn drag(self: *ScriptAreas, editor: *Editor, pointer: Pointer) EditError!void {
        if (self.handle_drag != .none) {
            const index = self.selected orelse return;
            const changed = switch (self.handle_drag) {
                .move => editor.scriptAreaMoved(self.grab_area, pointer.world_x + self.grab_world[0], pointer.world_y + self.grab_world[1]),
                .resize => editor.scriptAreaResized(self.grab_area, pointer.world_x, pointer.world_y),
                .none => unreachable,
            } catch |err| {
                if (err == error.Refused) return;
                return err;
            };
            // A position the bridge will not take is skipped: the area stays at the
            // last one it took and the drag goes on; the status line says why.
            editor.editScriptArea(index, changed, self.gesture) catch |err| if (err != error.Refused) return err;
            return;
        }
        if (self.dragging) self.current = .{ pointer.world_x, pointer.world_y };
    }

    fn release(self: *ScriptAreas, editor: *Editor, pointer: Pointer) EditError!void {
        if (self.handle_drag != .none) {
            self.handle_drag = .none;
            self.gesture = 0;
            return;
        }
        if (!self.dragging) return;
        const begin = self.start.?;
        const end: [2]f32 = .{ pointer.world_x, pointer.world_y };
        self.start = null;
        self.current = null;
        self.dragging = false;
        const dx = pointer.screen_x - self.press_screen[0];
        const dy = pointer.screen_y - self.press_screen[1];
        if (dx * dx + dy * dy <= click_pixels * click_pixels) {
            // A click: the last area under it (the one drawn on top), or none.
            const areas = try editor.scriptAreas(editor.allocator);
            defer editor.allocator.free(areas);
            var picked: ?usize = null;
            for (areas, 0..) |area, index| {
                if (contains(area, pointer.map_x, pointer.map_y)) picked = index;
            }
            self.selected = picked;
            if (picked == null) editor.note("drag to draw a script area");
            return;
        }
        var name_buffer: [records.area_name_capacity]u8 = undefined;
        const area_name = try self.nameFor(editor, &name_buffer);
        const area = try editor.scriptAreaFromVis(self.shape, begin[0], begin[1], end[0], end[1], area_name);
        // A drag that converts to nothing - both half sizes or the radius 0 - is not
        // an area: a note, not a command.
        const empty = switch (area.shape) {
            .rectangle => area.hx == 0 and area.hy == 0,
            .circle => area.r == 0,
        };
        if (empty) {
            editor.note("that area is too small; drag further");
            return;
        }
        self.selected = try editor.addScriptArea(area);
        self.setName("");
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

/// A pointer at a world point, with the map point the fake's ratio gives and a
/// screen point that is the world one (so a drag longer than a click is one).
fn pointerAt(fake: *const fake_mod.FakeBridge, x: f32, y: f32) Pointer {
    return .{ .world_x = x, .world_y = y, .map_x = x * fake.map_per_world, .map_y = y * fake.map_per_world, .screen_x = x, .screen_y = y };
}

fn drag(tool: *ScriptAreas, editor: *Editor, fake: *const fake_mod.FakeBridge, from: [2]f32, to: [2]f32) !void {
    try tool.handle(editor, .{ .press = pointerAt(fake, from[0], from[1]) });
    try tool.handle(editor, .{ .drag = pointerAt(fake, (from[0] + to[0]) / 2, (from[1] + to[1]) / 2) });
    try tool.handle(editor, .{ .drag = pointerAt(fake, to[0], to[1]) });
    try tool.handle(editor, .{ .release = pointerAt(fake, to[0], to[1]) });
}

test "a rectangle drag adds a named area in AI units as one undo step, and is selected" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    tool.setName("m2_area");
    try drag(&tool, &editor, &fake, .{ 100, 200 }, .{ 300, 260 }); // 8x8 tiles of 40: inside the fixture map? tile_size decides
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    const area = fake.script_areas.items[0];
    try testing.expectEqualStrings("m2_area", area.nameSlice());
    try testing.expectEqual(records.AreaShape.rectangle, area.shape);
    try testing.expectEqual(@as(f32, 283), area.cx);
    try testing.expectEqual(@as(f32, 325), area.cy);
    try testing.expectEqual(@as(f32, 141), area.hx);
    try testing.expectEqual(@as(f32, 42), area.hy);
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    try testing.expectEqual(@as(usize, 0), tool.name().len); // the name was taken
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 0), fake.script_areas.items.len);
}

test "a circle drag adds a circle, and a click or a zero-size drag is a note, not a command" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{ .shape = .circle };
    try drag(&tool, &editor, &fake, .{ 100, 100 }, .{ 130, 140 });
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    const ring = fake.script_areas.items[0];
    try testing.expectEqual(records.AreaShape.circle, ring.shape);
    try testing.expectEqual(@as(f32, 141), ring.cx);
    try testing.expectEqual(@as(f32, 71), ring.r); // distance 50 -> 70.7 + 0.3
    try testing.expectEqualStrings("area_1", ring.nameSlice()); // the first free default
    // A press and release in one place (a click on nothing): a note.
    const depth = editor.history.undo_stack.items.len;
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 200, 40) });
    try tool.handle(&editor, .{ .release = pointerAt(&fake, 200, 40) });
    try testing.expectEqual(@as(?usize, null), tool.selected);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    // A drag far enough to leave a click but whose circle converts to radius 0 is refused by a note.
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 200, 40) });
    var tiny = pointerAt(&fake, 200, 40);
    tiny.screen_x = 260; // the screen says a drag, the world says nothing moved
    try tool.handle(&editor, .{ .release = tiny });
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "too small") != null);
    // The next default name skips the taken one.
    try drag(&tool, &editor, &fake, .{ 50, 50 }, .{ 80, 50 });
    try testing.expectEqualStrings("area_2", fake.script_areas.items[1].nameSlice());
}

test "a taken name is Refused with the history unchanged, and the name stays for another try" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var existing: records.ScriptArea = .{ .shape = .circle, .cx = 40, .cy = 40, .r = 8 };
    existing.setName("zone");
    try fake.addScriptAreaFixture(existing);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    tool.setName("zone");
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 100, 100) });
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .release = pointerAt(&fake, 160, 160) }));
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expectEqualStrings("zone", tool.name());
    tool.setName("zone_b");
    try drag(&tool, &editor, &fake, .{ 100, 100 }, .{ 160, 160 });
    try testing.expectEqual(@as(usize, 2), fake.script_areas.items.len);
}

test "the centre handle moves the selected area and the edge handle resizes it, one undo step per drag" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var box: records.ScriptArea = .{ .shape = .rectangle, .cx = 100, .cy = 100, .hx = 30, .hy = 20 };
    box.setName("box");
    try fake.addScriptAreaFixture(box);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{ .selected = 0 };
    // The centre (100, 100) map units is world (70.7, 70.7). Grab it a little off-centre
    // and drag by 20 world units each way: the centre moves by the same, size kept.
    const grab_x = 100 / fake.map_per_world + 3;
    const grab_y = 100 / fake.map_per_world + 3;
    try tool.handle(&editor, .{ .press = pointerAt(&fake, grab_x, grab_y) });
    try testing.expectEqual(ScriptAreas.HandleDrag.move, tool.handle_drag);
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, grab_x + 10, grab_y + 10) });
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, grab_x + 20, grab_y + 20) });
    try tool.handle(&editor, .{ .release = pointerAt(&fake, grab_x + 20, grab_y + 20) });
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(f32, 128), fake.script_areas.items[0].cx); // (70.7 + 20) * sqrt 2 + 0.3, cut
    try testing.expectEqual(@as(f32, 30), fake.script_areas.items[0].hx);
    try testing.expectEqualStrings("box", fake.script_areas.items[0].nameSlice());
    // A press nowhere near a handle does not grab one; it starts a new area's drag and, released
    // as a click inside the area, selects it.
    try testing.expect(try editor.undo());
    try testing.expect(fake.script_areas.items[0].eql(box));
    // The resize: the (+, +) corner is (130, 120) map units.
    const corner = edgeHandle(box);
    try tool.handle(&editor, .{ .press = pointerAt(&fake, corner[0] / fake.map_per_world, corner[1] / fake.map_per_world) });
    try testing.expectEqual(ScriptAreas.HandleDrag.resize, tool.handle_drag);
    const centre_world = 100 / fake.map_per_world;
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, centre_world + 40, centre_world + 12) });
    try tool.handle(&editor, .{ .release = pointerAt(&fake, centre_world + 40, centre_world + 12) });
    try testing.expectEqual(@as(f32, 56), fake.script_areas.items[0].hx);
    try testing.expectEqual(@as(f32, 17), fake.script_areas.items[0].hy);
    try testing.expectEqual(@as(f32, 100), fake.script_areas.items[0].cx);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expect(fake.script_areas.items[0].eql(box));
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(f32, 56), fake.script_areas.items[0].hx);
}

test "a position the bridge refuses mid-drag is skipped and the drag goes on" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var ring: records.ScriptArea = .{ .shape = .circle, .cx = 100, .cy = 100, .r = 10 };
    ring.setName("ring");
    try fake.addScriptAreaFixture(ring);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{ .selected = 0 };
    const centre_world = 100 / fake.map_per_world;
    try tool.handle(&editor, .{ .press = pointerAt(&fake, centre_world, centre_world) });
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, centre_world + 10, centre_world) });
    const at_ten = fake.script_areas.items[0].cx;
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, -500, centre_world) }); // off the map: refused, skipped
    try testing.expectEqual(at_ten, fake.script_areas.items[0].cx);
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, centre_world + 20, centre_world) });
    try testing.expect(fake.script_areas.items[0].cx > at_ten);
    try tool.handle(&editor, .{ .release = pointerAt(&fake, centre_world + 20, centre_world) });
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
}

test "a click inside an area selects it, the last one drawn when they overlap, and Delete removes the selected one" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var first: records.ScriptArea = .{ .shape = .circle, .cx = 100, .cy = 100, .r = 60 };
    first.setName("big");
    var second: records.ScriptArea = .{ .shape = .rectangle, .cx = 110, .cy = 100, .hx = 10, .hy = 10 };
    second.setName("small");
    try fake.addScriptAreaFixture(first);
    try fake.addScriptAreaFixture(second);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    const click = pointerAt(&fake, 105 / fake.map_per_world, 100 / fake.map_per_world);
    try tool.handle(&editor, .{ .press = click });
    try tool.handle(&editor, .{ .release = click });
    try testing.expectEqual(@as(?usize, 1), tool.selected);
    const outer = pointerAt(&fake, 60 / fake.map_per_world, 100 / fake.map_per_world);
    try tool.handle(&editor, .{ .press = outer });
    try tool.handle(&editor, .{ .release = outer });
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expectEqualStrings("small", fake.script_areas.items[0].nameSlice());
    try testing.expectEqual(@as(?usize, null), tool.selected);
    try tool.handle(&editor, .{ .key = .delete }); // nothing selected: a note
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqualStrings("big", fake.script_areas.items[0].nameSlice());
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
}

test "Escape abandons a drag and the tool ignores the events it has no use for" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 100, 100) });
    try tool.handle(&editor, .{ .key = .escape });
    try testing.expect(tool.start == null and !tool.dragging);
    try tool.handle(&editor, .{ .release = pointerAt(&fake, 200, 200) });
    try testing.expectEqual(@as(usize, 0), fake.script_areas.items.len);
    const pointer = pointerAt(&fake, 100, 100);
    for ([_]Event{ .{ .right_press = pointer }, .{ .right_drag = pointer }, .{ .right_release = pointer }, .{ .double_click = pointer }, .{ .key = .enter }, .{ .key = .insert }, .{ .key = .space }, .{ .key = .rotate_left } }) |event| {
        try tool.handle(&editor, event);
    }
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}
