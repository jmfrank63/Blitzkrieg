//! The Heights tool (M3, D-18): the MFC editor's terrain tab
//! (DrawShadeState.cpp) as one gesture tool. Left-drag raises, right-drag
//! lowers, middle-drag - or left and right held together, or Alt+drag, which
//! the view maps to the middle button - levels toward the level mode's
//! target. The buttons' combination decides per event, exactly the MFC's own
//! OnMouseMove precedence (DrawShadeState.cpp:217-302): middle or L+R first,
//! then left, then right.
//!
//! The brush, the speed, the ratio and the mode are the panel's fields on
//! this struct (panels_m3.zig edits them; the Heights panel is their UI).
//! One stroke is one undo step: the gesture's steps merge in the Editor.
const std = @import("std");
const editor_mod = @import("editor.zig");
const fake_mod = @import("fake_bridge.zig");
const bridge_mod = @import("bridge.zig");
const tools = @import("tools.zig");
const Editor = editor_mod.Editor;
const EditError = bridge_mod.EditError;
const Pointer = tools.Pointer;

pub const Heights = struct {
    /// The MFC slider's own range and default (TabTerrainAltitudesDialog.cpp:141-159):
    /// 2..16; the pattern the bridge scales spans brush*2 vertices per axis.
    brush: i32 = 3,
    /// The profile gradient's ceiling, world z units (the MFC's fParameters[0]).
    speed: f32 = 1.0,
    /// The level step, percent of the distance to the target (the MFC's
    /// fParameters[1], shown as 3.00).
    ratio_percent: f32 = 3.0,
    /// What a level stroke moves the terrain toward (the MFC's LEVEL_TO_0..3,
    /// default LEVEL_TO_2 - instant average).
    level_mode: bridge_mod.HeightsLevelMode = .instant_average,
    gesture: u32 = 0,
    /// The stroke's start in world units - the click modes' reference, kept
    /// from the press until every button has gone.
    click_x: f32 = 0,
    click_y: f32 = 0,
    /// Whether the next step is its stroke's first (where the bridge takes
    /// the click modes' frozen targets).
    stroke_start: bool = false,

    pub fn handle(self: *Heights, editor: *Editor, event: tools.Event) EditError!void {
        switch (event) {
            .press => |pointer| try self.step(editor, pointer, .raise),
            .drag => |pointer| try self.move(editor, pointer),
            .release => |pointer| self.endIfNoButtons(pointer),
            .right_press => |pointer| try self.step(editor, pointer, .lower),
            .right_drag => |pointer| try self.move(editor, pointer),
            .right_release => |pointer| self.endIfNoButtons(pointer),
            .key, .double_click => {},
        }
    }

    /// One press: a fresh stroke when none is open (the click reference and
    /// the stroke-start flag taken now), else the ongoing stroke continuing
    /// with the new button's action - the MFC applies per mouse move, so a
    /// button that joins a stroke changes what the next move does, not the
    /// stroke's identity.
    fn step(self: *Heights, editor: *Editor, pointer: Pointer, pressed: bridge_mod.HeightsAction) EditError!void {
        if (self.gesture == 0) {
            self.gesture = editor.beginGesture();
            self.click_x = pointer.world_x;
            self.click_y = pointer.world_y;
            self.stroke_start = true;
        }
        const action = actionFor(pointer, pressed);
        try self.apply(editor, pointer, action);
    }

    /// One drag: the stroke goes on with whatever the held buttons say. The
    /// initiating action is a tiebreak the mask cannot lose (a right-drag
    /// with no mask read as right, as the view sent it).
    fn move(self: *Heights, editor: *Editor, pointer: Pointer) EditError!void {
        if (self.gesture == 0) return;
        const held: bridge_mod.HeightsAction = if (pointer.buttons.middle or (pointer.buttons.left and pointer.buttons.right))
            .level
        else if (pointer.buttons.right and !pointer.buttons.left)
            .lower
        else if (pointer.buttons.left and !pointer.buttons.right)
            .raise
        else
            .level;
        try self.apply(editor, pointer, held);
    }

    /// The MFC's own precedence for a press whose mask is stamped: middle or
    /// L+R level, the pressed button's own action otherwise.
    fn actionFor(pointer: Pointer, pressed: bridge_mod.HeightsAction) bridge_mod.HeightsAction {
        if (pointer.buttons.middle or (pointer.buttons.left and pointer.buttons.right)) return .level;
        return pressed;
    }

    /// A release ends the stroke only when no button is held any more: the
    /// mask is stamped after the release, so left-up over a held right keeps
    /// lowering, exactly the MFC's per-move flags.
    fn endIfNoButtons(self: *Heights, pointer: Pointer) void {
        if (self.gesture == 0) return;
        if (pointer.buttons.left or pointer.buttons.right or pointer.buttons.middle) return;
        self.gesture = 0;
        self.stroke_start = false;
    }

    fn apply(self: *Heights, editor: *Editor, pointer: Pointer, action: bridge_mod.HeightsAction) EditError!void {
        const params: bridge_mod.HeightsStrokeParams = .{
            .action = @intFromEnum(action),
            .level_mode = @intFromEnum(self.level_mode),
            .brush = self.brush,
            .height_speed = self.speed,
            .level_ratio_percent = self.ratio_percent,
            .pos_x = pointer.world_x,
            .pos_y = pointer.world_y,
            .click_x = self.click_x,
            .click_y = self.click_y,
            .stroke_start = if (self.stroke_start) 1 else 0,
            .ctrl_held = if (pointer.ctrl) 1 else 0,
        };
        self.stroke_start = false;
        // A refused step is an ordinary answer: the status line says why and
        // the stroke goes on (the refused-stamp rule), so the next move can
        // try the same cell again.
        try editor.heightsStroke(params, self.gesture);
    }
};

// ---------------------------------------------------------------------------
// Tests (core tier, over the fake).
// ---------------------------------------------------------------------------

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

fn heightAt(fake: *fake_mod.FakeBridge, x: i32, y: i32) f32 {
    return fake.altitudes_grid[fake.altitudeIndex(x, y)];
}

test "a raise stroke lifts the terrain under the brush and is one undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: Heights = .{ .brush = 2, .speed = 1.0 };
    // Over cell 4,4 (world 4*32..): the pattern's corner is tile - 1, its
    // centre the cursor's own vertex neighbourhood.
    try tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .drag = try at(&editor, 5 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .release = try at(&editor, 5 * 32 + 16, 4 * 32 + 16) });
    // The dome's centre is the pattern's own centre: with brush 2 the
    // pattern spans 4 vertices, corner at tile-1 = 3, so vertices 3..7 on
    // each axis; the centre value (speed 1, distance 0) is at (5, 5).
    try testing.expect(heightAt(&fake, 5, 5) > 0);
    try testing.expectEqual(@as(u32, 0), tool.gesture);
    // Two steps, one history entry.
    var strokes: usize = 0;
    for (fake.calls.items) |call| {
        if (call.kind == .heights_stroke) strokes += 1;
    }
    try testing.expectEqual(@as(usize, 2), strokes);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, 5, 5));
    try testing.expect(!editor.dirty());
}

test "a lower stroke subtracts and the level modes move toward their targets" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    // Level to zero from a raised patch: raise once, then level (the middle
    // button's gesture, stamped on the pointer as the view does) in enough
    // steps that the dome rounds to 0.
    var tool: Heights = .{ .brush = 2, .speed = 4.0 };
    try tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .release = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    const raised = heightAt(&fake, 5, 5);
    try testing.expect(raised > 0);
    tool.level_mode = .zero;
    tool.ratio_percent = 100.0;
    var lifts: usize = 0;
    while (heightAt(&fake, 5, 5) != 0 and lifts < 8) : (lifts += 1) {
        try tool.handle(&editor, .{ .press = middleAt(4 * 32 + 16, 4 * 32 + 16) });
        try tool.handle(&editor, .{ .release = middleReleaseAt(4 * 32 + 16, 4 * 32 + 16) });
    }
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, 5, 5));
    // Lower under the zeroed ground.
    tool.ratio_percent = 3.0;
    try tool.handle(&editor, .{ .right_press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .right_release = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try testing.expect(heightAt(&fake, 5, 5) < 0);
}

/// A level press: the middle button's mask, as the view stamps it.
fn middleAt(x: f32, y: f32) Pointer {
    return .{ .world_x = x, .world_y = y, .map_x = x, .map_y = y, .buttons = .{ .middle = true } };
}

/// The release that ends a middle gesture: no button held any more.
fn middleReleaseAt(x: f32, y: f32) Pointer {
    return .{ .world_x = x, .world_y = y, .map_x = x, .map_y = y };
}

test "middle, or left and right together, level - the MFC's own precedence" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: Heights = .{ .brush = 2, .speed = 4.0, .ratio_percent = 100.0 };
    // Raise a patch, then level it flat with left+right held: the drag's
    // mask, not the button that began the stroke, decides.
    try tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .release = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try testing.expect(heightAt(&fake, 5, 5) > 0);
    tool.level_mode = .zero;
    try tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .drag = .{ .world_x = 4 * 32 + 16, .world_y = 4 * 32 + 16, .map_x = 4 * 32 + 16, .map_y = 4 * 32 + 16, .buttons = .{ .left = true, .right = true } } });
    try tool.handle(&editor, .{ .release = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, 5, 5));
    // And the middle button alone: the same level.
    try tool.handle(&editor, .{ .press = .{ .world_x = 4 * 32 + 16, .world_y = 4 * 32 + 16, .map_x = 4 * 32 + 16, .map_y = 4 * 32 + 16, .buttons = .{ .middle = true } } });
    try tool.handle(&editor, .{ .release = .{ .world_x = 4 * 32 + 16, .world_y = 4 * 32 + 16, .map_x = 4 * 32 + 16, .map_y = 4 * 32 + 16 } });
    try testing.expectEqual(@as(u32, 0), tool.gesture);
}

test "the click modes freeze their targets at the stroke's start" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: Heights = .{ .brush = 2, .speed = 4.0, .ratio_percent = 100.0, .level_mode = .click_tile };
    // Click Tile: the target is the average of the four vertices of the tile
    // under the PRESS - here a flat zero tile far from the raise, so a level
    // drag over the raised patch pulls it back to zero.
    try tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .release = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) });
    try testing.expect(heightAt(&fake, 5, 5) > 0);
    // A new level stroke: its press is over the flat ground beside the
    // patch (the fake's map is 8x8 tiles; tile 2 is well clear of the dome
    // at tile 4), its drag crosses the raised dome. The frozen target is
    // the flat tile's 0.
    try tool.handle(&editor, .{ .press = middleAt(2 * 32 + 16, 2 * 32 + 16) });
    try tool.handle(&editor, .{ .drag = middleAt(4 * 32 + 16, 4 * 32 + 16) });
    try tool.handle(&editor, .{ .release = middleReleaseAt(4 * 32 + 16, 4 * 32 + 16) });
    try testing.expect(heightAt(&fake, 5, 5) == 0);
}

test "an invalid-height stroke is refused and changes nothing unless Ctrl is held" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: Heights = .{ .brush = 2, .speed = fake_mod.fake_height_limit * 4.0 };
    // Past the fake's limit: the step is refused, the heights stay, the
    // status line words it, and the history gains nothing.
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) }));
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, 5, 5));
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expectEqualStrings("invalid height", editor.status());
    // A refused step is not the end of the gesture: the same stroke's next
    // step - the speed lowered mid-drag, exactly what the panel allows -
    // still lands, and both are one undo step. The drag carries the left
    // button's mask, as the view stamps it.
    tool.speed = 1.0;
    try tool.handle(&editor, .{ .drag = .{ .world_x = 5 * 32 + 16, .world_y = 4 * 32 + 16, .map_x = 5 * 32 + 16, .map_y = 4 * 32 + 16, .buttons = .{ .left = true } } });
    try tool.handle(&editor, .{ .release = try at(&editor, 5 * 32 + 16, 4 * 32 + 16) });
    try testing.expect(heightAt(&fake, 5, 5) > 0);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    // Ctrl holds the MFC's override: a cliff past the limit is kept.
    tool.speed = fake_mod.fake_height_limit * 4.0;
    try tool.handle(&editor, .{ .press = .{ .world_x = 4 * 32 + 16, .world_y = 4 * 32 + 16, .map_x = 4 * 32 + 16, .map_y = 4 * 32 + 16, .ctrl = true } });
    try tool.handle(&editor, .{ .release = .{ .world_x = 4 * 32 + 16, .world_y = 4 * 32 + 16, .map_x = 4 * 32 + 16, .map_y = 4 * 32 + 16 } });
    try testing.expect(heightAt(&fake, 5, 5) > fake_mod.fake_height_limit);
    // Undo takes both strokes away.
    _ = try editor.undo();
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, 5, 5));
    try testing.expect(!editor.dirty());
}

test "generate fills the sheet into the z range and set zero flattens it" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    try editor.generateHeights(.hills, 0.3, -1.0, 2.0);
    try testing.expectEqual(bridge_mod.HeightsGenerateType.hills, fake.last_generate_type);
    const low = heightAt(&fake, 0, 0);
    const high = heightAt(&fake, fake.info.width_tiles, fake.info.height_tiles);
    try testing.expectEqual(@as(f32, -1.0 * fake_mod.tile_size), low);
    try testing.expectEqual(@as(f32, 2.0 * fake_mod.tile_size), high);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, 0, 0));
    try editor.setZeroHeights();
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, 0, 0));
    try testing.expectEqual(@as(f32, 0), heightAt(&fake, fake.info.width_tiles, fake.info.height_tiles));
    try testing.expect(editor.dirty());
}

test "a brush outside 2..16 is a caller bug" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: Heights = .{ .brush = 1 };
    try testing.expectError(error.Failed, tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) }));
    tool.brush = 17;
    try testing.expectError(error.Failed, tool.handle(&editor, .{ .press = try at(&editor, 4 * 32 + 16, 4 * 32 + 16) }));
}

test "the heights tool ignores the keys and the double click" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: Heights = .{};
    const depth = editor.history.undo_stack.items.len;
    const pointer = try at(&editor, 40, 40);
    const events = [_]tools.Event{ .{ .key = .delete }, .{ .key = .enter }, .{ .key = .escape }, .{ .double_click = pointer } };
    for (events) |event| try tool.handle(&editor, event);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(u32, 0), tool.gesture);
}
