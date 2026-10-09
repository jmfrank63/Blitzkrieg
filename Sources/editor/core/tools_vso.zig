//! The Roads & Rivers tool (D-08): one tool with a road / river switch, the
//! MFC editor's gestures (VectorStripeObjectsState.cpp, last in the tree at 1c82b6b87;
//! the MFC editor was deleted in 05-11).
//!
//! Adding: a press adds the pointer's world point, a right press takes the
//! last point back, a double click or Enter/Space finishes the line as one
//! undo step, Esc drops it (an addition: MFC has no cancel). A finish with
//! fewer than two points is a status note, not a command.
//!
//! Editing the selected line: a press on a control point drags it (and, in
//! the width modes, every later or every point), a press on a width handle
//! drags the width, a right press on a point drags the opacity (100 pixels to
//! 1.0), Insert adds a midpoint and Delete removes a point (never below 2) or,
//! with none in hand, the whole line; one undo step per drag and per key. A
//! press on another line selects it, on empty ground deselects; a right press
//! with nothing in hand walks through the lines under the pointer.
//!
//! Research Q7, a recorded parity deviation: the MFC editor applies Insert and
//! Delete only while the button is still held on a point; here they act on
//! the hovered control point, else the last one a press took hold of, which a
//! trackpad can reach.
//!
//! Points are world (Vis) units, the pointer's `world_x`/`world_y`: roads and
//! rivers are saved in the scene's units, not the map's (bridge.h).
const std = @import("std");
const editor_mod = @import("editor.zig");
const fake_mod = @import("fake_bridge.zig");
const bridge_mod = @import("bridge.zig");
const records = @import("records.zig");
const tools = @import("tools.zig");
const Editor = editor_mod.Editor;
const EditError = bridge_mod.EditError;
const VsoKind = bridge_mod.VsoKind;
const Event = tools.Event;
const Pointer = tools.Pointer;

/// The MFC width modes (CTabVOVSODialog::CW_SINGLE/MULTI/ALL): an edit of a
/// point changes that point only, that point and every later one, or every
/// point.
pub const WidthMode = bridge_mod.VsoWidthMode;

/// The most points one unfinished line holds; a click past it is a note.
pub const max_pending = 256;

/// The MFC tool's hit radii, world units (VectorStripeObjectsState.cpp:16-19):
/// a control point within fWorldCellSize / 5, a width handle within
/// fWorldCellSize / 3. fWorldCellSize is 32 * sqrt(2) (Formats/fmtTerrain.h).
pub const world_cell_size: f32 = 45.254833;
pub const control_point_radius: f32 = world_cell_size / 5.0;
pub const key_point_radius: f32 = world_cell_size / 3.0;
/// The MFC opacity drag: 100 screen pixels to an opacity of 1.
pub const opacity_pixels: f32 = 100.0;
/// A right press this close to the last one cycles on through what is under
/// the pointer, farther away starts again from the first.
const cycle_distance: f32 = 4.0;

pub const RoadsRivers = struct {
    /// What a finished line becomes.
    kind: VsoKind = .road,
    /// The type a new line is drawn with: a bare descriptor name
    /// (`Editor.vsoDescriptors`), set by the panel.
    desc_buffer: [bridge_mod.vso_name_capacity]u8 = @splat(0),
    desc_len: usize = 0,
    /// The MFC width spinner, 1..16 (fWidth = w * fWorldCellSize / 2).
    width_tiles: f32 = 3,
    /// 0..1, the panel's 0..100 %.
    opacity: f32 = 1.0,
    width_mode: WidthMode = .single,
    /// With `multi`, a control-point drag moves the grabbed point and every
    /// EARLIER one instead of every later one (MFC: Ctrl held; here a panel
    /// switch, because Ctrl+click is the right button in this tool).
    multi_backwards: bool = false,
    /// The unfinished line, world units.
    pending: [max_pending]records.Vec3 = undefined,
    pending_len: usize = 0,
    /// The road or river being edited: set by a finished line, a press on a
    /// line or a right-press cycle.
    selected: ?Selected = null,
    /// WR-B02: the selected line's saved nID just before an undo or redo, so
    /// `resolveSelection` finds the same line again whatever the replay did
    /// to the list's order.
    selected_key: ?i32 = null,
    /// Where the pointer is (world units), for the unfinished line's last
    /// leg; null off the terrain.
    cursor: ?[2]f32 = null,
    /// The selected line's control point under the pointer (research Q7:
    /// Insert and Delete act on it, or on the last grabbed one).
    hovered_control: ?usize = null,
    /// What the last press took hold of; stays after the release.
    last_grab: LastGrab = .none,
    /// What the current press or right press holds, null between gestures.
    grab: ?Grab = null,
    gesture: u32 = 0,
    /// The press point and, for a control-point drag, the control points as
    /// they were at the press.
    grab_at: [2]f32 = .{ 0, 0 },
    grab_screen_y: f32 = 0,
    grab_controls: std.ArrayListUnmanaged(records.Vec3) = .empty,
    /// The right-press cycle through overlapping lines.
    cycle: u32 = 0,
    cycle_at: ?[2]f32 = null,
    /// The selected line as last read from the bridge, and the generation it
    /// was read at: re-read after any road or river change.
    view: bridge_mod.VsoView = .{},
    view_of: ?Selected = null,
    view_generation: u32 = 0,

    pub const Selected = bridge_mod.VsoRef;
    pub const LastGrab = union(enum) { none, control: usize, key: usize };
    pub const Grab = union(enum) {
        control: usize,
        /// A width handle: the key point, the side (+1 or -1 along its
        /// normal) and where on the handle the press landed.
        width: struct { key: usize, side: f32, offset: [2]f32 },
        /// The opacity drag of a key point, from its opacity at the press.
        opacity: struct { key: usize, start: f32 },
    };

    pub fn deinit(self: *RoadsRivers, allocator: std.mem.Allocator) void {
        self.grab_controls.deinit(allocator);
        self.view.deinit(allocator);
        self.* = undefined;
    }

    pub fn setDesc(self: *RoadsRivers, name: []const u8) void {
        const len = @min(name.len, self.desc_buffer.len - 1);
        @memset(&self.desc_buffer, 0);
        @memcpy(self.desc_buffer[0..len], name[0..len]);
        self.desc_len = len;
    }

    pub fn desc(self: *const RoadsRivers) []const u8 {
        return self.desc_buffer[0..self.desc_len];
    }

    pub fn pendingPoints(self: *const RoadsRivers) []const records.Vec3 {
        return self.pending[0..self.pending_len];
    }

    pub fn adding(self: *const RoadsRivers) bool {
        return self.pending_len != 0;
    }

    /// Forgets the unfinished line, the selection and any gesture, as a map
    /// change must. The cached line's memory is kept for reuse.
    pub fn reset(self: *RoadsRivers) void {
        self.pending_len = 0;
        self.deselect();
        self.cursor = null;
        self.cycle_at = null;
    }

    fn savedIdAt(editor: *Editor, kind: VsoKind, index: usize) ?i32 {
        var view = editor.readVso(kind, index) catch return null;
        defer view.deinit(editor.allocator);
        return view.saved_id;
    }

    pub fn captureSelection(self: *RoadsRivers, editor: *Editor) void {
        self.selected_key = null;
        const which = self.selected orelse return;
        self.selected_key = savedIdAt(editor, which.kind, which.index);
    }

    /// The line found again keeps its selection, and the point last grabbed
    /// or hovered on it while that point is still there (Insert and Delete
    /// act on it); a gesture in hand is dropped. A line that is gone is
    /// deselected.
    pub fn resolveSelection(self: *RoadsRivers, editor: *Editor) void {
        const which = self.selected orelse return;
        const key = self.selected_key orelse {
            self.deselect();
            return;
        };
        self.selected_key = null;
        self.grab = null;
        self.gesture = 0;
        const count = editor.vsoCount(which.kind) catch {
            self.deselect();
            return;
        };
        var found: ?usize = null;
        if (which.index < count and savedIdAt(editor, which.kind, which.index) == key) {
            found = which.index;
        } else {
            var index: usize = 0;
            while (index < count and found == null) : (index += 1) {
                if (savedIdAt(editor, which.kind, index) == key) found = index;
            }
        }
        const index = found orelse {
            self.deselect();
            return;
        };
        self.selected = .{ .kind = which.kind, .index = index };
        var view = editor.readVso(which.kind, index) catch {
            self.hovered_control = null;
            self.last_grab = .none;
            return;
        };
        defer view.deinit(editor.allocator);
        if (self.hovered_control) |control| {
            if (control >= view.control_points.len) self.hovered_control = null;
        }
        switch (self.last_grab) {
            .none => {},
            .control => |control| if (control >= view.control_points.len) {
                self.last_grab = .none;
            },
            .key => |key_point| if (key_point >= view.key_points.len) {
                self.last_grab = .none;
            },
        }
    }

    fn deselect(self: *RoadsRivers) void {
        self.selected = null;
        self.hovered_control = null;
        self.last_grab = .none;
        self.grab = null;
        self.gesture = 0;
    }

    fn select(self: *RoadsRivers, which: Selected) void {
        if (self.selected) |current| {
            if (current.kind == which.kind and current.index == which.index) return;
        }
        self.deselect();
        self.selected = which;
    }

    /// The panel's width slider moved (04-13; the MFC editor's
    /// CVSOState::Update re-widthed the selected line in CW_ALL mode): with a
    /// line selected and the width mode All, every key point of it takes the
    /// panel's width, `width_tiles * fWorldCellSize / 2` world units. The moves
    /// of one slider drag pass one `gesture` and are one undo step. Nothing
    /// happens without a selection or in the other modes, which name a key
    /// point that only a drag on the line gives.
    pub fn applyPanelWidth(self: *RoadsRivers, editor: *Editor, gesture: u32) EditError!void {
        if (self.width_mode != .all) return;
        const selected = self.selected orelse return;
        try editor.setVsoWidth(selected.kind, selected.index, 0, self.width_tiles * world_cell_size / 2.0, .all, gesture);
    }

    /// The panel's opacity slider moved: as `applyPanelWidth`, every key point
    /// of the selected line takes the panel's opacity in the width mode All.
    pub fn applyPanelOpacity(self: *RoadsRivers, editor: *Editor, gesture: u32) EditError!void {
        if (self.width_mode != .all) return;
        const selected = self.selected orelse return;
        try editor.setVsoOpacity(selected.kind, selected.index, 0, self.opacity, .all, gesture);
    }

    /// The selected line as the bridge holds it now, read again when a road
    /// or river changed since the last read; null (and deselected) when the
    /// selection no longer names one - an undo took it away.
    pub fn selectedView(self: *RoadsRivers, editor: *Editor) ?*const bridge_mod.VsoView {
        const selected = self.selected orelse return null;
        const fresh = if (self.view_of) |of| of.kind == selected.kind and of.index == selected.index and self.view_generation == editor.vso_generation else false;
        if (!fresh) {
            self.view.deinit(editor.allocator);
            self.view_of = null;
            self.view = editor.readVso(selected.kind, selected.index) catch {
                self.deselect();
                return null;
            };
            self.view_of = selected;
            self.view_generation = editor.vso_generation;
        }
        return &self.view;
    }

    /// The pointer moved with no button held: the cursor for the unfinished
    /// line's last leg, and the control point under it for Insert and Delete.
    pub fn hover(self: *RoadsRivers, editor: *Editor, pointer: Pointer) void {
        self.cursor = .{ pointer.world_x, pointer.world_y };
        self.hovered_control = null;
        const view = self.selectedView(editor) orelse return;
        self.hovered_control = nearestControl(view, pointer.world_x, pointer.world_y);
    }

    /// The pointer left the terrain.
    pub fn hoverNone(self: *RoadsRivers) void {
        self.cursor = null;
        self.hovered_control = null;
    }

    pub fn handle(self: *RoadsRivers, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| try self.press(editor, pointer),
            .drag => |pointer| try self.drag(editor, pointer),
            .release, .right_release => {
                self.grab = null;
                self.gesture = 0;
            },
            .right_press => |pointer| try self.rightPress(editor, pointer),
            .right_drag => |pointer| try self.rightDrag(editor, pointer),
            // The MFC double click only finishes a line (WR-B07). With nothing
            // being drawn it is the second click of a pair whose first press
            // already selected the line or grabbed a point: that stays.
            .double_click => {
                if (self.adding()) try self.finish(editor);
            },
            .key => |key| switch (key) {
                .enter, .space => {
                    if (self.adding()) try self.finish(editor) else self.deselect();
                },
                .escape => {
                    if (self.adding()) self.pending_len = 0 else self.deselect();
                },
                .delete => try self.deleteKey(editor),
                .insert => try self.insertKey(editor),
                .rotate_left, .rotate_right => {},
            },
        }
    }

    fn nearestControl(view: *const bridge_mod.VsoView, x: f32, y: f32) ?usize {
        var best: ?usize = null;
        var best_distance: f32 = control_point_radius;
        for (view.control_points, 0..) |point, index| {
            const distance = std.math.hypot(point.x - x, point.y - y);
            if (distance <= best_distance) {
                best = index;
                best_distance = distance;
            }
        }
        return best;
    }

    /// A width handle of the selected line under (x, y): the key point, the
    /// side and the press's offset from the handle.
    fn widthHandleAt(view: *const bridge_mod.VsoView, x: f32, y: f32) ?Grab {
        for (view.key_points, 0..) |key, index| {
            for ([_]f32{ 1, -1 }) |side| {
                const hx = key.x + side * key.nx * key.width;
                const hy = key.y + side * key.ny * key.width;
                if (std.math.hypot(x - hx, y - hy) <= key_point_radius) {
                    return .{ .width = .{ .key = index, .side = side, .offset = .{ x - hx, y - hy } } };
                }
            }
        }
        return null;
    }

    fn press(self: *RoadsRivers, editor: *Editor, pointer: Pointer) EditError!void {
        const x = pointer.world_x;
        const y = pointer.world_y;
        self.cursor = .{ x, y };
        if (self.adding()) return self.addPoint(editor, pointer);
        if (self.selectedView(editor)) |view| {
            if (nearestControl(view, x, y)) |control| {
                try self.grab_controls.resize(editor.allocator, view.control_points.len);
                @memcpy(self.grab_controls.items, view.control_points);
                self.grab = .{ .control = control };
                self.last_grab = .{ .control = control };
                self.grab_at = .{ x, y };
                self.gesture = editor.beginGesture();
                return;
            }
            if (widthHandleAt(view, x, y)) |held| {
                self.grab = held;
                self.last_grab = .{ .key = held.width.key };
                self.grab_at = .{ x, y };
                self.gesture = editor.beginGesture();
                return;
            }
        }
        // Not on the selected line's handles: another line selects it, empty
        // ground deselects - or, with nothing selected, starts a new line.
        if (try editor.pickVso(x, y, 0)) |hit| {
            self.select(hit);
            return;
        }
        if (self.selected != null) {
            self.deselect();
            return;
        }
        self.addPoint(editor, pointer);
    }

    fn drag(self: *RoadsRivers, editor: *Editor, pointer: Pointer) EditError!void {
        const grab = self.grab orelse return;
        const selected = self.selected orelse return;
        switch (grab) {
            .control => |control| {
                const count = self.grab_controls.items.len;
                if (control >= count) return;
                const dx = pointer.world_x - self.grab_at[0];
                const dy = pointer.world_y - self.grab_at[1];
                const moved = try editor.allocator.dupe(records.Vec3, self.grab_controls.items);
                defer editor.allocator.free(moved);
                for (moved, 0..) |*point, index| {
                    const in = switch (self.width_mode) {
                        .single => index == control,
                        .multi => if (self.multi_backwards) index <= control else index >= control,
                        .all => true,
                    };
                    if (!in) continue;
                    point.x += dx;
                    point.y += dy;
                }
                // A position the bridge will not take is skipped; the drag
                // goes on from the last one it took.
                editor.moveVsoPoints(selected.kind, selected.index, moved, self.gesture) catch |err| if (err != error.Refused) return err;
            },
            .width => |held| {
                const view = self.selectedView(editor) orelse return;
                if (held.key >= view.key_points.len) return;
                const key = view.key_points[held.key];
                const sx = pointer.world_x - held.offset[0] - key.x;
                const sy = pointer.world_y - held.offset[1] - key.y;
                const width = @abs(sx * key.nx + sy * key.ny);
                if (!(width > 0)) return;
                editor.setVsoWidth(selected.kind, selected.index, held.key, width, self.width_mode, self.gesture) catch |err| if (err != error.Refused) return err;
            },
            .opacity => {},
        }
    }

    fn rightPress(self: *RoadsRivers, editor: *Editor, pointer: Pointer) EditError!void {
        const x = pointer.world_x;
        const y = pointer.world_y;
        if (self.adding()) {
            self.pending_len -= 1;
            return;
        }
        if (self.selectedView(editor)) |view| {
            // On a point of the selected line: the opacity drag of its key
            // point (a control point's own key point, or a width handle's).
            const key: ?usize = if (nearestControl(view, x, y)) |control|
                (if (control < view.key_points.len) control else null)
            else if (widthHandleAt(view, x, y)) |held| held.width.key else null;
            if (key) |k| {
                self.grab = .{ .opacity = .{ .key = k, .start = view.key_points[k].opacity } };
                self.grab_screen_y = pointer.screen_y;
                self.gesture = editor.beginGesture();
                return;
            }
        }
        // Nothing in hand: walk through the lines under the pointer.
        const again = if (self.cycle_at) |previous| std.math.hypot(previous[0] - x, previous[1] - y) <= cycle_distance else false;
        self.cycle = if (again) self.cycle +% 1 else 0;
        self.cycle_at = .{ x, y };
        if (try editor.pickVso(x, y, self.cycle)) |hit| {
            self.deselect();
            self.selected = hit;
        }
    }

    fn rightDrag(self: *RoadsRivers, editor: *Editor, pointer: Pointer) EditError!void {
        const grab = self.grab orelse return;
        const selected = self.selected orelse return;
        switch (grab) {
            .opacity => |held| {
                const shift = (self.grab_screen_y - pointer.screen_y) / opacity_pixels;
                const value = std.math.clamp(held.start + shift, 0, 1);
                editor.setVsoOpacity(selected.kind, selected.index, held.key, value, self.width_mode, self.gesture) catch |err| if (err != error.Refused) return err;
            },
            .control, .width => {},
        }
    }

    /// The control point Insert and Delete act on: the hovered one, else the
    /// last one a press took hold of.
    fn targetControl(self: *const RoadsRivers) ?usize {
        if (self.hovered_control) |control| return control;
        return switch (self.last_grab) {
            .control => |control| control,
            .none, .key => null,
        };
    }

    fn insertKey(self: *RoadsRivers, editor: *Editor) EditError!void {
        if (self.adding()) return;
        const selected = self.selected orelse return;
        const view = self.selectedView(editor) orelse return;
        const control = self.targetControl() orelse return;
        const count = view.control_points.len;
        if (control >= count) return;
        try editor.insertVsoPoint(selected.kind, selected.index, control);
        // The point the insert was made at keeps its place in hand: it moves
        // up by one when the midpoint went before it (it was the last).
        const kept = if (control + 1 < count) control else control + 1;
        self.last_grab = .{ .control = kept };
        self.hovered_control = null;
    }

    /// Delete: the target control point, never below 2 (refused); a width
    /// handle in hand deletes nothing (MFC); with nothing in hand the whole
    /// road or river.
    fn deleteKey(self: *RoadsRivers, editor: *Editor) EditError!void {
        if (self.adding()) return;
        const selected = self.selected orelse return;
        if (self.targetControl()) |control| {
            try editor.deleteVsoPoint(selected.kind, selected.index, control);
            self.last_grab = .none;
            self.hovered_control = null;
            return;
        }
        if (self.last_grab == .key) return;
        try editor.deleteVso(selected.kind, selected.index);
        self.deselect();
    }

    fn addPoint(self: *RoadsRivers, editor: *Editor, pointer: Pointer) void {
        if (self.pending_len == 0) self.deselect();
        if (self.pending_len == max_pending) {
            editor.note("a road or river takes at most 256 points at a time: finish it first");
            return;
        }
        self.pending[self.pending_len] = .{ .x = pointer.world_x, .y = pointer.world_y, .z = 0 };
        self.pending_len += 1;
    }

    /// The MFC finish: the line becomes a road or river when it has two
    /// points, and the unfinished line goes either way (a refused one too).
    fn finish(self: *RoadsRivers, editor: *Editor) EditError!void {
        if (self.pending_len == 0) return;
        defer self.pending_len = 0;
        if (self.pending_len < 2) {
            editor.note("a road or river needs at least two points");
            return;
        }
        const index = try editor.addVso(self.kind, self.desc(), self.pendingPoints(), self.width_tiles, self.opacity);
        self.deselect();
        self.selected = .{ .kind = self.kind, .index = index };
    }
};

const testing = std.testing;

fn opened(fake: *fake_mod.FakeBridge) !Editor {
    var editor = Editor.init(testing.allocator, fake.bridge());
    errdefer editor.deinit();
    try editor.open("fixture.bzm");
    return editor;
}

fn at(editor: *Editor, x: f32, y: f32) !Pointer {
    return editor.resolve(x, y);
}

fn roadTool() RoadsRivers {
    var tool: RoadsRivers = .{};
    tool.setDesc("road_track");
    return tool;
}

fn countCalls(fake: *const fake_mod.FakeBridge, kind: fake_mod.CallKind) usize {
    var count: usize = 0;
    for (fake.calls.items) |call| {
        if (call.kind == kind) count += 1;
    }
    return count;
}

test "click, click, double-click draws one road as one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    const generation = editor.vso_generation;
    // A double click arrives as the single click's press and release, then
    // double_click (SDL: clicks 1, then clicks 2).
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .release = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 120, 40) });
    try tool.handle(&editor, .{ .release = try at(&editor, 120, 40) });
    try tool.handle(&editor, .{ .press = try at(&editor, 200, 120) });
    try tool.handle(&editor, .{ .release = try at(&editor, 200, 120) });
    try tool.handle(&editor, .{ .double_click = try at(&editor, 200, 120) });
    try testing.expectEqual(@as(usize, 1), fake.vsoLen(.road));
    try testing.expectEqual(@as(usize, 3), fake.vso(.road, 0).count);
    try testing.expectEqual(@as(f32, 120), fake.vso(.road, 0).controls[1].x);
    try testing.expectEqual(@as(f32, 3 * fake_mod.tile_size / 2.0), fake.vso(.road, 0).keys[0].width);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(editor.vso_generation != generation);
    try testing.expect(!tool.adding());
    try testing.expectEqual(@as(?RoadsRivers.Selected, .{ .kind = .road, .index = 0 }), tool.selected);
    _ = try editor.undo();
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.road));
    _ = try editor.redo();
    try testing.expectEqual(@as(usize, 1), fake.vsoLen(.road));
    try testing.expectEqual(@as(usize, 1), countCalls(&fake, .vso_edit));
    try testing.expectEqual(@as(usize, 1), countCalls(&fake, .undo_edit));
    try testing.expectEqual(@as(usize, 1), countCalls(&fake, .redo_edit));
}

test "Enter and Space finish a line, a river when the switch says so" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 20) });
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(@as(usize, 1), fake.vsoLen(.road));
    tool.kind = .river;
    tool.setDesc("defaultriver");
    tool.opacity = 0.5;
    // The finished road is selected: a press on empty ground deselects it
    // first (MFC's edit state does nothing there), the next one starts a line.
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 200) });
    try testing.expect(!tool.adding() and tool.selected == null);
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 200) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 200) });
    try tool.handle(&editor, .{ .key = .space });
    try testing.expectEqual(@as(usize, 1), fake.vsoLen(.river));
    try testing.expectEqual(@as(f32, 0.5), fake.vso(.river, 0).keys[1].opacity);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
}

test "Esc drops the unfinished line and records nothing" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 20) });
    try tool.handle(&editor, .{ .key = .escape });
    try testing.expect(!tool.adding());
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.road));
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "a right click takes the last point back" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 150, 90) });
    try tool.handle(&editor, .{ .right_press = try at(&editor, 150, 90) });
    try tool.handle(&editor, .{ .right_release = try at(&editor, 150, 90) });
    try testing.expectEqual(@as(usize, 2), tool.pending_len);
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(@as(usize, 2), fake.vso(.road, 0).count);
    try testing.expectEqual(@as(f32, 100), fake.vso(.road, 0).controls[1].x);
}

test "a too-short road is refused and records nothing; one point is only a note" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    // Two clicks 1 unit apart: one point after the builder's 2-unit rule.
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 21, 20) });
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .key = .enter }));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "too short") != null);
    try testing.expect(!tool.adding());
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.road));
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .double_click = try at(&editor, 20, 20) });
    try testing.expect(std.mem.indexOf(u8, editor.status(), "two points") != null);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 0), countCalls(&fake, .vso_edit));
}

test "an unknown type or a point off the map is refused with the map unchanged" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const points = [_]records.Vec3{ .{ .x = 20, .y = 20 }, .{ .x = 100, .y = 20 } };
    try testing.expectError(error.Refused, editor.addVso(.road, "no_such_road", &points, 3, 1));
    const off = [_]records.Vec3{ .{ .x = 20, .y = 20 }, .{ .x = 9999, .y = 20 } };
    try testing.expectError(error.Refused, editor.addVso(.road, "road_track", &off, 3, 1));
    try testing.expectError(error.Failed, editor.addVso(.road, "road_track", &points, 17, 1));
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.road));
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    const descriptors = try editor.vsoDescriptors(.road, testing.allocator);
    defer testing.allocator.free(descriptors);
    try testing.expectEqual(@as(usize, 2), descriptors.len);
    try testing.expectEqualStrings("road_track", descriptors[1].nameSlice());
}

test "Delete with a road selected and no point in hand deletes the whole road, one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 20) });
    try tool.handle(&editor, .{ .key = .enter });
    tool.kind = .river;
    tool.setDesc("defaultriver");
    try tool.handle(&editor, .{ .key = .escape }); // deselects the road
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 200) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 200) });
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(@as(?RoadsRivers.Selected, .{ .kind = .river, .index = 0 }), tool.selected);
    const generation = editor.vso_generation;
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.river));
    try testing.expectEqual(@as(usize, 1), fake.vsoLen(.road));
    try testing.expectEqual(@as(?RoadsRivers.Selected, null), tool.selected);
    try testing.expect(editor.vso_generation != generation);
    try testing.expectEqual(@as(usize, 3), editor.history.undo_stack.items.len);
    // Nothing selected: Delete does nothing.
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 3), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectEqual(@as(usize, 1), fake.vsoLen(.river));
    try testing.expectEqual(@as(f32, 100), fake.vso(.river, 0).controls[1].x);
    _ = try editor.redo();
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.river));
    // Undo everything: no roads, no rivers.
    while (try editor.undo()) {}
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.river));
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.road));
    try testing.expect(!editor.dirty());
}

// ---------------------------------------------------------------------------
// Editing a selected road or river (04-05 Task 3, one test per behaviour).
// The fake's world is its screen, so a point's screen_y is its world y.
// ---------------------------------------------------------------------------

/// A road through (20,100), (100,100), (180,100), drawn and selected. Its key
/// normals point along +y, width 48 (3 tiles of the fake's 32).
fn drawnRoad(editor: *Editor, tool: *RoadsRivers) !void {
    try tool.handle(editor, .{ .press = try at(editor, 20, 100) });
    try tool.handle(editor, .{ .press = try at(editor, 100, 100) });
    try tool.handle(editor, .{ .press = try at(editor, 180, 100) });
    try tool.handle(editor, .{ .key = .enter });
}

fn dragFrom(tool: *RoadsRivers, editor: *Editor, from_x: f32, from_y: f32, to_x: f32, to_y: f32) !void {
    try tool.handle(editor, .{ .press = try at(editor, from_x, from_y) });
    try tool.handle(editor, .{ .drag = try at(editor, (from_x + to_x) / 2, (from_y + to_y) / 2) });
    try tool.handle(editor, .{ .drag = try at(editor, to_x, to_y) });
    try tool.handle(editor, .{ .release = try at(editor, to_x, to_y) });
}

fn expectControl(fake: *const fake_mod.FakeBridge, index: usize, x: f32, y: f32) !void {
    const point = fake.vso(.road, 0).controls[index];
    try testing.expectEqual(x, point.x);
    try testing.expectEqual(y, point.y);
}

test "a press near a selected road's control point grabs it and a single-mode drag moves only it, one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    // 3 units off the point: inside fWorldCellSize / 5 (9 units).
    try dragFrom(&tool, &editor, 103, 100, 110, 130);
    try expectControl(&fake, 0, 20, 100);
    try expectControl(&fake, 1, 107, 130);
    try expectControl(&fake, 2, 180, 100);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try expectControl(&fake, 1, 100, 100);
    _ = try editor.redo();
    try expectControl(&fake, 1, 107, 130);
}

test "a drag's undo or redo that fails part-way puts back what it replayed, so a retry works (WR-B01)" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    try dragFrom(&tool, &editor, 103, 100, 110, 130);
    const entry = editor.history.undo_stack.items[editor.history.undo_stack.items.len - 1];
    const tokens = entry.command.edit.tokens.items;
    try testing.expect(tokens.len >= 2);
    // The oldest token will not go back: the newer ones already undone are redone.
    fake.fail_undo_token = tokens[0];
    try testing.expectError(error.Failed, editor.undo());
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    try expectControl(&fake, 1, 107, 130);
    try testing.expect(!editor.replay_broken);
    fake.fail_undo_token = null;
    try testing.expect(try editor.undo());
    try expectControl(&fake, 1, 100, 100);
    // The newest token will not be redone: the older ones already redone are undone.
    fake.fail_redo_token = tokens[tokens.len - 1];
    try testing.expectError(error.Failed, editor.redo());
    try expectControl(&fake, 1, 100, 100);
    fake.fail_redo_token = null;
    try testing.expect(try editor.redo());
    try expectControl(&fake, 1, 107, 130);
}

test "a failed replay whose own unwinding fails refuses every further undo and redo until the map is reopened (WR-B01)" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    try dragFrom(&tool, &editor, 103, 100, 110, 130);
    const tokens = editor.history.undo_stack.items[editor.history.undo_stack.items.len - 1].command.edit.tokens.items;
    try testing.expect(tokens.len >= 2);
    fake.fail_undo_token = tokens[0];
    fake.fail_redo_token = tokens[1];
    try testing.expectError(error.Failed, editor.undo());
    try testing.expect(editor.replay_broken);
    fake.fail_undo_token = null;
    fake.fail_redo_token = null;
    try testing.expectError(error.Failed, editor.undo());
    try testing.expect(std.mem.indexOf(u8, editor.status(), "reopen the map") != null);
    try editor.open("fixture.bzm");
    try testing.expect(!editor.replay_broken);
}

test "the fake holds as many points as the tool draws, and names a line by its full saved name (WR-B06)" {
    try testing.expect(fake_mod.max_vso_points >= max_pending);
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    var view = try editor.readVso(.road, 0);
    defer view.deinit(testing.allocator);
    try testing.expect(std.mem.startsWith(u8, view.descSlice(), fake_mod.fake_season_folder ++ "Roads3D\\"));
    try testing.expect(std.mem.endsWith(u8, view.descSlice(), tool.desc()));
}

test "a double click on a selected road keeps it selected and what the first click grabbed (WR-B07)" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    // The pair's first click grabs control point 1; the second arrives as a double click.
    try tool.handle(&editor, .{ .press = try at(&editor, 101, 100) });
    try tool.handle(&editor, .{ .release = try at(&editor, 101, 100) });
    const grabbed = tool.last_grab;
    try tool.handle(&editor, .{ .double_click = try at(&editor, 101, 100) });
    try testing.expect(tool.selected != null);
    try testing.expectEqual(grabbed, tool.last_grab);
    // Enter still deselects.
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expect(tool.selected == null);
}

test "an undo that shifts the roads list keeps the tool on the road it had selected (WR-B02)" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool); // road 0
    tool.reset(); // nothing selected: the next press starts a line
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 200) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 200) });
    try tool.handle(&editor, .{ .key = .enter }); // road 1, selected
    const second = fake.vso(.road, 1).controls[0];
    try editor.deleteVso(.road, 0);
    tool.selected = .{ .kind = .road, .index = 0 }; // the second road, now first
    tool.captureSelection(&editor);
    try testing.expect(try editor.undo()); // road 0 back in front of it
    tool.resolveSelection(&editor);
    try testing.expectEqual(@as(usize, 1), tool.selected.?.index);
    try testing.expectEqual(second.y, fake.vso(.road, tool.selected.?.index).controls[0].y);
}

test "multi mode moves the point and every later one (earlier ones when asked); all moves every point" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    tool.width_mode = .multi;
    try dragFrom(&tool, &editor, 100, 100, 100, 120);
    try expectControl(&fake, 0, 20, 100);
    try expectControl(&fake, 1, 100, 120);
    try expectControl(&fake, 2, 180, 120);
    tool.multi_backwards = true;
    try dragFrom(&tool, &editor, 100, 120, 100, 140);
    try expectControl(&fake, 0, 20, 120);
    try expectControl(&fake, 1, 100, 140);
    try expectControl(&fake, 2, 180, 120);
    tool.width_mode = .all;
    try dragFrom(&tool, &editor, 180, 120, 170, 120);
    try expectControl(&fake, 0, 10, 120);
    try expectControl(&fake, 1, 90, 140);
    try expectControl(&fake, 2, 170, 120);
    // The add and three drags.
    try testing.expectEqual(@as(usize, 4), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try expectControl(&fake, 0, 20, 120);
    try expectControl(&fake, 2, 180, 120);
    _ = try editor.redo();
    try expectControl(&fake, 0, 10, 120);
}

test "a press near a width handle grabs it; the drag sets |shift . normal| per mode, one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    // Key point 1's handle is at (100, 100 + 48); 5 units off it is inside
    // fWorldCellSize / 3 (15 units).
    try dragFrom(&tool, &editor, 104, 151, 104, 173);
    try testing.expectEqual(@as(f32, 48), fake.vso(.road, 0).keys[0].width);
    try testing.expectEqual(@as(f32, 70), fake.vso(.road, 0).keys[1].width);
    try testing.expectEqual(@as(f32, 48), fake.vso(.road, 0).keys[2].width);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    // The other side's handle, all mode: every key point.
    tool.width_mode = .all;
    try dragFrom(&tool, &editor, 20, 52, 20, 40);
    for (fake.vso(.road, 0).keySlice()) |key| try testing.expectEqual(@as(f32, 60), key.width);
    tool.width_mode = .multi;
    try dragFrom(&tool, &editor, 100, 160, 100, 150);
    try testing.expectEqual(@as(f32, 60), fake.vso(.road, 0).keys[0].width);
    try testing.expectEqual(@as(f32, 50), fake.vso(.road, 0).keys[1].width);
    try testing.expectEqual(@as(f32, 50), fake.vso(.road, 0).keys[2].width);
    try testing.expectEqual(@as(usize, 4), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 60), fake.vso(.road, 0).keys[2].width);
    _ = try editor.redo();
    try testing.expectEqual(@as(f32, 50), fake.vso(.road, 0).keys[2].width);
}

test "the panel's width and opacity re-width the selected line in mode All, one undo step per slider drag, nothing otherwise" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    const drawn = fake.vso(.road, 0).keys[1].width;
    // Single mode: the slider is only what the next line takes.
    tool.width_tiles = 6;
    try tool.applyPanelWidth(&editor, editor.beginGesture());
    try testing.expectEqual(drawn, fake.vso(.road, 0).keys[1].width);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    // All mode: one slider drag through 5 and 6 tiles is one step, every key point.
    tool.width_mode = .all;
    const gesture = editor.beginGesture();
    tool.width_tiles = 5;
    try tool.applyPanelWidth(&editor, gesture);
    tool.width_tiles = 6;
    try tool.applyPanelWidth(&editor, gesture);
    for (fake.vso(.road, 0).keySlice()) |key| try testing.expectApproxEqAbs(6 * world_cell_size / 2.0, key.width, 0.001);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    tool.opacity = 0.25;
    try tool.applyPanelOpacity(&editor, editor.beginGesture());
    for (fake.vso(.road, 0).keySlice()) |key| try testing.expectApproxEqAbs(@as(f32, 0.25), key.opacity, 0.0001);
    try testing.expectEqual(@as(usize, 3), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 1), fake.vso(.road, 0).keys[0].opacity);
    _ = try editor.undo();
    try testing.expectEqual(drawn, fake.vso(.road, 0).keys[1].width);
    // No selection: nothing.
    tool.reset();
    try tool.applyPanelWidth(&editor, editor.beginGesture());
    try testing.expectEqual(drawn, fake.vso(.road, 0).keys[1].width);
}

test "a right-drag 50 pixels down on a point of the selected road takes 0.5 off its opacity, clamped, one step each" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    try tool.handle(&editor, .{ .right_press = try at(&editor, 100, 100) });
    try tool.handle(&editor, .{ .right_drag = try at(&editor, 100, 125) });
    try tool.handle(&editor, .{ .right_drag = try at(&editor, 100, 150) });
    try tool.handle(&editor, .{ .right_release = try at(&editor, 100, 150) });
    try testing.expectEqual(@as(f32, 1), fake.vso(.road, 0).keys[0].opacity);
    try testing.expectApproxEqAbs(@as(f32, 0.5), fake.vso(.road, 0).keys[1].opacity, 0.0001);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    // 150 pixels down from 0.5 stops at 0; all mode sets every point.
    tool.width_mode = .all;
    try tool.handle(&editor, .{ .right_press = try at(&editor, 100, 100) });
    try tool.handle(&editor, .{ .right_drag = try at(&editor, 100, 250) });
    try tool.handle(&editor, .{ .right_release = try at(&editor, 100, 250) });
    for (fake.vso(.road, 0).keySlice()) |key| try testing.expectEqual(@as(f32, 0), key.opacity);
    try testing.expectEqual(@as(usize, 3), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectApproxEqAbs(@as(f32, 0.5), fake.vso(.road, 0).keys[1].opacity, 0.0001);
    try testing.expectEqual(@as(f32, 1), fake.vso(.road, 0).keys[2].opacity);
}

test "Insert adds the midpoint after the last grabbed point, before it at the end; Delete removes it, never below 2, else the road" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    try drawnRoad(&editor, &tool);
    // A click on point 1 grabs it and changes nothing.
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 100) });
    try tool.handle(&editor, .{ .release = try at(&editor, 100, 100) });
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try tool.handle(&editor, .{ .key = .insert });
    try testing.expectEqual(@as(usize, 4), fake.vso(.road, 0).count);
    try expectControl(&fake, 2, 140, 100);
    // The last point: the midpoint goes before it.
    try tool.handle(&editor, .{ .press = try at(&editor, 180, 100) });
    try tool.handle(&editor, .{ .release = try at(&editor, 180, 100) });
    try tool.handle(&editor, .{ .key = .insert });
    try testing.expectEqual(@as(usize, 5), fake.vso(.road, 0).count);
    try expectControl(&fake, 3, 160, 100);
    try expectControl(&fake, 4, 180, 100);
    // Delete removes the grabbed point (still the last one).
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 4), fake.vso(.road, 0).count);
    try expectControl(&fake, 3, 160, 100);
    // A hovered point is the target too.
    tool.hover(&editor, try at(&editor, 21, 100));
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 3), fake.vso(.road, 0).count);
    try expectControl(&fake, 0, 100, 100);
    try testing.expectEqual(@as(usize, 5), editor.history.undo_stack.items.len);
    // Never below 2 points.
    tool.hover(&editor, try at(&editor, 100, 100));
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 2), fake.vso(.road, 0).count);
    tool.hover(&editor, try at(&editor, 160, 100));
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .key = .delete }));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "at least 2 points") != null);
    try testing.expectEqual(@as(usize, 2), fake.vso(.road, 0).count);
    // Nothing grabbed or hovered: the whole road.
    tool.hover(&editor, try at(&editor, 60, 200));
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 0), fake.vsoLen(.road));
    try testing.expectEqual(@as(usize, 7), editor.history.undo_stack.items.len);
    _ = try editor.undo();
    try testing.expectEqual(@as(usize, 2), fake.vso(.road, 0).count);
    _ = try editor.undo();
    try testing.expectEqual(@as(usize, 3), fake.vso(.road, 0).count);
}

test "a right press with nothing grabbed cycles through the roads and rivers under the pointer" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    const road = [_]records.Vec3{ .{ .x = 20, .y = 100 }, .{ .x = 200, .y = 100 } };
    _ = try editor.addVso(.road, "road_track", &road, 3, 1);
    const river = [_]records.Vec3{ .{ .x = 100, .y = 20 }, .{ .x = 100, .y = 200 } };
    _ = try editor.addVso(.river, "defaultriver", &river, 3, 1);
    const expected = [_]RoadsRivers.Selected{ .{ .kind = .road, .index = 0 }, .{ .kind = .river, .index = 0 }, .{ .kind = .road, .index = 0 } };
    for (expected) |want| {
        try tool.handle(&editor, .{ .right_press = try at(&editor, 101, 101) });
        try tool.handle(&editor, .{ .right_release = try at(&editor, 101, 101) });
        try testing.expectEqual(@as(?RoadsRivers.Selected, want), tool.selected);
    }
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
}

test "a press on another road selects it, on empty ground deselects, and with nothing selected starts a line" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = roadTool();
    defer tool.deinit(testing.allocator);
    const river = [_]records.Vec3{ .{ .x = 40, .y = 200 }, .{ .x = 220, .y = 200 } };
    _ = try editor.addVso(.river, "defaultriver", &river, 3, 1);
    try drawnRoad(&editor, &tool);
    try testing.expectEqual(@as(?RoadsRivers.Selected, .{ .kind = .road, .index = 0 }), tool.selected);
    try tool.handle(&editor, .{ .press = try at(&editor, 130, 205) });
    try tool.handle(&editor, .{ .release = try at(&editor, 130, 205) });
    try testing.expectEqual(@as(?RoadsRivers.Selected, .{ .kind = .river, .index = 0 }), tool.selected);
    try testing.expect(!tool.adding());
    try tool.handle(&editor, .{ .press = try at(&editor, 230, 20) });
    try tool.handle(&editor, .{ .release = try at(&editor, 230, 20) });
    try testing.expectEqual(@as(?RoadsRivers.Selected, null), tool.selected);
    try testing.expect(!tool.adding());
    try tool.handle(&editor, .{ .press = try at(&editor, 230, 20) });
    try testing.expect(tool.adding());
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
}
