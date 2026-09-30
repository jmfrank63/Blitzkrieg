//! The Roads & Rivers tool (D-08): one tool with a road / river switch, the
//! MFC editor's gestures (Sources/src/MapEditor/VectorStripeObjectsState.cpp).
//!
//! Adding: a press adds the pointer's world point, a right press takes the
//! last point back, a double click or Enter/Space finishes the line as one
//! undo step, Esc drops it (an addition: MFC has no cancel). A finish with
//! fewer than two points is a status note, not a command.
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
pub const WidthMode = enum(u8) { single = 0, multi = 1, all = 2 };

/// The most points one unfinished line holds; a click past it is a note.
pub const max_pending = 256;

pub const RoadsRivers = struct {
    /// What a finished line becomes.
    kind: VsoKind = .road,
    /// The type a new line is drawn with: a bare descriptor name
    /// (`Editor.vsoDescriptors`), set by the panel.
    desc_buffer: [bridge_mod.vso_name_capacity]u8 = [_]u8{0} ** bridge_mod.vso_name_capacity,
    desc_len: usize = 0,
    /// The MFC width spinner, 1..16 (fWidth = w * fWorldCellSize / 2).
    width_tiles: f32 = 3,
    /// 0..1, the panel's 0..100 %.
    opacity: f32 = 1.0,
    width_mode: WidthMode = .single,
    /// The unfinished line, world units.
    pending: [max_pending]records.Vec3 = undefined,
    pending_len: usize = 0,
    /// The road or river being edited: set by a finished line.
    selected: ?Selected = null,

    pub const Selected = struct { kind: VsoKind, index: usize };

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

    /// Forgets the unfinished line and the selection, as a map change must.
    pub fn reset(self: *RoadsRivers) void {
        self.pending_len = 0;
        self.selected = null;
    }

    pub fn handle(self: *RoadsRivers, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| self.addPoint(editor, pointer),
            .right_press => {
                if (self.pending_len != 0) self.pending_len -= 1;
            },
            .double_click => try self.finish(editor),
            .key => |key| switch (key) {
                .enter, .space => try self.finish(editor),
                .escape => self.pending_len = 0,
                .delete => try self.deleteSelected(editor),
                .insert, .rotate_left, .rotate_right => {},
            },
            .drag, .release, .right_drag, .right_release => {},
        }
    }

    fn addPoint(self: *RoadsRivers, editor: *Editor, pointer: Pointer) void {
        if (self.pending_len == 0) self.selected = null;
        if (self.pending_len == max_pending) {
            editor.note("a road or river takes at most 256 points at a time: finish it first");
            return;
        }
        self.pending[self.pending_len] = .{ .x = pointer.world_x, .y = pointer.world_y, .z = 0 };
        self.pending_len += 1;
    }

    /// Delete with a road or river selected and no point in hand: the whole
    /// record goes (MFC: VK_DELETE with no active point), one undo step.
    fn deleteSelected(self: *RoadsRivers, editor: *Editor) EditError!void {
        if (self.pending_len != 0) return;
        const selected = self.selected orelse return;
        try editor.deleteVso(selected.kind, selected.index);
        self.selected = null;
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
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 20) });
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(@as(usize, 1), fake.vsoLen(.road));
    tool.kind = .river;
    tool.setDesc("defaultriver");
    tool.opacity = 0.5;
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
    try tool.handle(&editor, .{ .press = try at(&editor, 20, 20) });
    try tool.handle(&editor, .{ .press = try at(&editor, 100, 20) });
    try tool.handle(&editor, .{ .key = .enter });
    tool.kind = .river;
    tool.setDesc("defaultriver");
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
