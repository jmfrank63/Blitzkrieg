//! The group tools (04-06): the Bridge tool (D-10..D-12), the MFC Bridges tab
//! (Sources/src/MapEditor/RoadDrawState.cpp). The Fence tool (04-07, D-14) and
//! the Entrenchment tool (04-08, D-13) are here too.
//!
//! Bridge: a press starts a drag at the pointer's world point, the drag moves
//! `current` (the app draws the ghost from BkEditorPlanBridge between the
//! two), and the release draws the bridge along the drag as one undo step.
//! The bridge snaps the drag to the chosen type's axis; a drag along the other
//! axis, or one that would put a span off the map, is refused with a status
//! note and records nothing.
//!
//! A click - a press released within `click_pixels` of where it began - draws
//! nothing: on a span it selects that span's whole bridge (D-11), anywhere
//! else it drops the selection. (The MFC tool drew a two-span bridge on any
//! release; here a bridge is a drag, so a click can select.) Delete removes
//! the selected bridge whole, Q or E rotates it to its `_01`/`_02` partner
//! (D-11) and Enter toggles a WoodenBig_Heavy bridge between intact and built
//! during play (D-12), each one undo step.
//!
//! Drags are world (Vis) units, the pointer's `world_x`/`world_y`: the bridge
//! plans in the scene's units and converts each span to map units itself.
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

pub const BridgeTool = struct {
    /// The type a new bridge is drawn with: a name from
    /// `Editor.bridgeDescriptors`, set by the Bridges panel.
    desc_buffer: [bridge_mod.name_capacity]u8 = [_]u8{0} ** bridge_mod.name_capacity,
    desc_len: usize = 0,
    /// Where the drag began and where it is now, world units; null between
    /// drags. The ghost is planned from these two.
    start: ?[2]f32 = null,
    current: ?[2]f32 = null,
    dragging: bool = false,
    /// Where the press was on screen, to tell a click from a drag.
    press_screen: [2]f32 = .{ 0, 0 },
    /// The bridges entry the tool works on: the last one drawn, or the one a
    /// click picked.
    selected: ?usize = null,

    pub fn setDesc(self: *BridgeTool, name: []const u8) void {
        const len = @min(name.len, self.desc_buffer.len - 1);
        @memset(&self.desc_buffer, 0);
        @memcpy(self.desc_buffer[0..len], name[0..len]);
        self.desc_len = len;
    }

    pub fn desc(self: *const BridgeTool) []const u8 {
        return self.desc_buffer[0..self.desc_len];
    }

    /// Forgets the drag and the selection, as a map change must; the type
    /// stays.
    pub fn reset(self: *BridgeTool) void {
        self.start = null;
        self.current = null;
        self.dragging = false;
        self.selected = null;
    }

    pub fn handle(self: *BridgeTool, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                self.start = .{ pointer.world_x, pointer.world_y };
                self.current = self.start;
                self.dragging = true;
                self.press_screen = .{ pointer.screen_x, pointer.screen_y };
            },
            .drag => |pointer| {
                if (self.dragging) self.current = .{ pointer.world_x, pointer.world_y };
            },
            .release => |pointer| {
                if (!self.dragging) return;
                const begin = self.start.?;
                const end: [2]f32 = .{ pointer.world_x, pointer.world_y };
                self.start = null;
                self.current = null;
                self.dragging = false;
                const dx = pointer.screen_x - self.press_screen[0];
                const dy = pointer.screen_y - self.press_screen[1];
                if (dx * dx + dy * dy <= click_pixels * click_pixels) {
                    // A click: the bridge under it, or none.
                    const picked = try editor.pickGroup(pointer.screen_x, pointer.screen_y);
                    self.selected = if (picked) |group| (if (group.kind == .bridge) group.index else null) else null;
                    return;
                }
                if (self.desc_len == 0) {
                    editor.note("choose a bridge type in the Bridges panel first");
                    return;
                }
                self.selected = try editor.drawBridge(self.desc(), begin[0], begin[1], end[0], end[1]);
            },
            .key => |key| switch (key) {
                // Q/E (D-11): rotate the selected bridge to its partner. The
                // index stays; the MFC tool had no rotate, it redrew.
                .rotate_left, .rotate_right => {
                    const index = self.selected orelse {
                        editor.note("click a bridge to select it first");
                        return;
                    };
                    try editor.rotateBridge(index);
                },
                // Enter (D-12): built during play, WoodenBig_Heavy only.
                .enter => {
                    const index = self.selected orelse {
                        editor.note("click a bridge to select it first");
                        return;
                    };
                    try editor.toggleBridgeBuild(index);
                },
                .delete => {
                    const index = self.selected orelse {
                        editor.note("click a bridge to select it first");
                        return;
                    };
                    self.selected = null;
                    try editor.deleteBridge(index);
                },
                else => {},
            },
            else => {},
        }
    }
};

/// The Fence tool (04-07, D-14): the MFC Fences tab. A press starts a drag at
/// the pointer's world point, the drag moves `current` (the app draws the
/// ghost from BkEditorPlanFences between the two, or one fence under the
/// pointer between drags), and the release places the run as one undo step.
/// The bridge locks the drag to one axis and puts a fence every second AI
/// tile; a press released within `click_pixels` of where it began is a click
/// and places one fence (direction 0, flipped to 1 with Ctrl). A run with an
/// end off the map is refused whole with a status note and records nothing.
/// Placed fences are ordinary objects: the Select tool moves and deletes them.
pub const FenceTool = struct {
    /// The type a run is placed with: a name from `Editor.fenceDescriptors`,
    /// set by the Fences panel.
    desc_buffer: [bridge_mod.name_capacity]u8 = [_]u8{0} ** bridge_mod.name_capacity,
    desc_len: usize = 0,
    /// Where the drag began and where it is now, world units; null between
    /// drags.
    start: ?[2]f32 = null,
    current: ?[2]f32 = null,
    dragging: bool = false,
    /// Ctrl as of the last pointer event (the ghost of a single fence flips
    /// with it).
    ctrl: bool = false,
    press_screen: [2]f32 = .{ 0, 0 },

    pub fn setDesc(self: *FenceTool, name: []const u8) void {
        const len = @min(name.len, self.desc_buffer.len - 1);
        @memset(&self.desc_buffer, 0);
        @memcpy(self.desc_buffer[0..len], name[0..len]);
        self.desc_len = len;
    }

    pub fn desc(self: *const FenceTool) []const u8 {
        return self.desc_buffer[0..self.desc_len];
    }

    /// Forgets the drag, as a map change must; the type stays.
    pub fn reset(self: *FenceTool) void {
        self.start = null;
        self.current = null;
        self.dragging = false;
        self.ctrl = false;
    }

    pub fn handle(self: *FenceTool, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                self.start = .{ pointer.world_x, pointer.world_y };
                self.current = self.start;
                self.dragging = true;
                self.ctrl = pointer.ctrl;
                self.press_screen = .{ pointer.screen_x, pointer.screen_y };
            },
            .drag => |pointer| {
                self.ctrl = pointer.ctrl;
                if (self.dragging) self.current = .{ pointer.world_x, pointer.world_y };
            },
            .release => |pointer| {
                if (!self.dragging) return;
                const begin = self.start.?;
                var end: [2]f32 = .{ pointer.world_x, pointer.world_y };
                self.start = null;
                self.current = null;
                self.dragging = false;
                self.ctrl = pointer.ctrl;
                const dx = pointer.screen_x - self.press_screen[0];
                const dy = pointer.screen_y - self.press_screen[1];
                // A click stays on the tile it began on, whatever the pixel
                // jitter of the release.
                if (dx * dx + dy * dy <= click_pixels * click_pixels) end = begin;
                if (self.desc_len == 0) {
                    editor.note("choose a fence type in the Fences panel first");
                    return;
                }
                try editor.drawFences(self.desc(), begin[0], begin[1], end[0], end[1], pointer.ctrl);
            },
            .key => |key| switch (key) {
                // Escape gives the drag up.
                .escape => {
                    self.start = null;
                    self.current = null;
                    self.dragging = false;
                },
                else => {},
            },
            else => {},
        }
    }
};

/// The most clicks one unfinished trench holds (the bridge takes at most
/// 256); a click past it is a note.
pub const max_trench_points = 256;
/// A click this close (world units) to the one before it is the same point
/// (the MFC editor's UniquePolygon test: squared distance at most 4).
pub const trench_same_point: f32 = 2.0;

/// The Entrenchment tool (04-08, D-13): the MFC trench builder's gestures
/// (RoadDrawState.cpp). A press adds the pointer's world point to the
/// polyline, a right press (or Ctrl+click) clears it, a double click commits
/// it as one undo step - the double click's own first press already added its
/// point - and Escape clears it too (an addition: MFC has none). The bridge
/// builds the trench from the clicks: straight runs of fireplaces and lines,
/// arcs at turns, a terminator at each end, for `player`. The app draws the
/// live preview from BkEditorPlanEntrenchment of the clicks and `current`.
///
/// With no polyline started, a press on a piece selects its whole
/// entrenchment instead of starting one (D-11's rule for groups; the MFC tool
/// had no selection), the entrenchment under the pointer is `hovered` (the
/// MFC tool highlighted it, RoadDrawState.cpp:796-839), and Delete removes the
/// selected one, else the hovered one, whole (RoadDrawState.cpp:1312-1358), one
/// undo step. A single piece is never deleted alone (D-04).
pub const EntrenchmentTool = struct {
    points: [max_trench_points][2]f32 = undefined,
    len: usize = 0,
    /// The player the pieces will belong to, set by the Entrenchments panel.
    player: i32 = 0,
    /// Where the pointer is, world units, for the preview's last leg; null
    /// when it is off the terrain.
    current: ?[2]f32 = null,
    /// The entrenchments entry the tool works on: the last one drawn, or the
    /// one a press picked.
    selected: ?usize = null,
    /// The entrenchment under the pointer while no polyline is started.
    hovered: ?usize = null,

    pub fn pointSlice(self: *const EntrenchmentTool) []const [2]f32 {
        return self.points[0..self.len];
    }

    /// True while a polyline is being clicked.
    pub fn drawing(self: *const EntrenchmentTool) bool {
        return self.len > 0;
    }

    /// Forgets the polyline and the selection, as a map change must; the
    /// player stays.
    pub fn reset(self: *EntrenchmentTool) void {
        self.len = 0;
        self.current = null;
        self.selected = null;
        self.hovered = null;
    }

    /// The pointer moved with no button held: the preview's last leg, and -
    /// with no polyline started - the entrenchment under the pointer.
    pub fn hover(self: *EntrenchmentTool, editor: *Editor, pointer: Pointer) void {
        self.current = .{ pointer.world_x, pointer.world_y };
        self.hovered = null;
        if (self.drawing()) return;
        const picked = editor.pickGroup(pointer.screen_x, pointer.screen_y) catch return;
        if (picked) |group| {
            if (group.kind == .entrenchment) self.hovered = group.index;
        }
    }

    /// The pointer left the terrain.
    pub fn hoverNone(self: *EntrenchmentTool) void {
        self.current = null;
        self.hovered = null;
    }

    pub fn handle(self: *EntrenchmentTool, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| {
                self.current = .{ pointer.world_x, pointer.world_y };
                if (!self.drawing()) {
                    // On a piece: its entrenchment is selected, nothing drawn.
                    if (try editor.pickGroup(pointer.screen_x, pointer.screen_y)) |group| {
                        if (group.kind == .entrenchment) {
                            self.selected = group.index;
                            self.hovered = null;
                            return;
                        }
                    }
                }
                self.hovered = null;
                self.addPoint(editor, pointer.world_x, pointer.world_y);
            },
            .drag => |pointer| self.current = .{ pointer.world_x, pointer.world_y },
            // MFC: the right button's release clears the path; here its press
            // does, so a right drag is nothing more.
            .right_press => self.len = 0,
            .double_click => try self.commit(editor),
            .key => |key| switch (key) {
                .escape => self.len = 0,
                .delete => {
                    // MFC: Delete acts only while no polyline is started.
                    if (self.drawing()) return;
                    const index = self.selected orelse self.hovered orelse {
                        editor.note("click an entrenchment, or point at one, to delete it");
                        return;
                    };
                    self.selected = null;
                    self.hovered = null;
                    try editor.deleteEntrenchment(index);
                },
                else => {},
            },
            else => {},
        }
    }

    fn addPoint(self: *EntrenchmentTool, editor: *Editor, x: f32, y: f32) void {
        if (self.len > 0) {
            const last = self.points[self.len - 1];
            if (std.math.hypot(x - last[0], y - last[1]) <= trench_same_point) return;
        }
        if (self.len == max_trench_points) {
            editor.note("a trench takes at most 256 points: double-click to finish it");
            return;
        }
        if (self.len == 0) self.selected = null;
        self.points[self.len] = .{ x, y };
        self.len += 1;
    }

    /// The double click: the clicks become one entrenchment, one undo step.
    /// Fewer than two is a note, not a command, and keeps the point.
    fn commit(self: *EntrenchmentTool, editor: *Editor) EditError!void {
        if (self.len < 2) {
            editor.note("a trench needs two points: click where it runs, then double-click");
            return;
        }
        var points: [max_trench_points]records.Vec3 = undefined;
        for (self.pointSlice(), points[0..self.len]) |point, *out| out.* = .{ .x = point[0], .y = point[1], .z = 0 };
        self.selected = try editor.drawEntrenchment(points[0..self.len], self.player);
        self.len = 0;
    }

    /// The clicks and the pointer as the preview plans them: the polyline
    /// with the pointer as one more click (what a click there and a double
    /// click would commit). Returns the slice of `out` used.
    pub fn previewPoints(self: *const EntrenchmentTool, out: *[max_trench_points + 1]records.Vec3) []const records.Vec3 {
        for (self.pointSlice(), out[0..self.len]) |point, *slot| slot.* = .{ .x = point[0], .y = point[1], .z = 0 };
        var count = self.len;
        if (self.current) |cursor| {
            const same = count > 0 and std.math.hypot(cursor[0] - self.points[count - 1][0], cursor[1] - self.points[count - 1][1]) <= trench_same_point;
            if (!same) {
                out[count] = .{ .x = cursor[0], .y = cursor[1], .z = 0 };
                count += 1;
            }
        }
        return out[0..count];
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

fn bridgeTool(name: []const u8) BridgeTool {
    var tool: BridgeTool = .{};
    tool.setDesc(name);
    return tool;
}

/// A press at (x0, y0), a drag and a release at (x1, y1).
fn dragAcross(tool: *BridgeTool, editor: *Editor, x0: f32, y0: f32, x1: f32, y1: f32) EditError!void {
    try tool.handle(editor, .{ .press = try at(editor, x0, y0) });
    try tool.handle(editor, .{ .drag = try at(editor, (x0 + x1) / 2, (y0 + y1) / 2) });
    try tool.handle(editor, .{ .release = try at(editor, x1, y1) });
}

test "a horizontal drag draws one bridges entry and its spans as one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_Fake_Bridge_01");
    const bridges_before = fake.bridgeCount();
    const objects_before = editor.document.objects.items.len;
    const generation = editor.bridges_generation;

    try tool.handle(&editor, .{ .press = try at(&editor, 20, 150) });
    try tool.handle(&editor, .{ .drag = try at(&editor, 90, 152) });
    try testing.expect(tool.dragging);
    try testing.expectEqual(@as(f32, 90), tool.current.?[0]);
    try tool.handle(&editor, .{ .release = try at(&editor, 150, 155) });

    // 130 world units at one 32-unit span each: 4 middle spans and the ends.
    try testing.expectEqual(bridges_before + 1, fake.bridgeCount());
    const entry = fake.bridgeEntry(bridges_before);
    try testing.expectEqual(@as(usize, 6), entry.count);
    try testing.expectEqual(objects_before + 6, editor.document.objects.items.len);
    for (entry.linkSlice()) |link| try testing.expect(editor.document.find(link) != null);
    try testing.expectEqual(@as(?usize, bridges_before), tool.selected);
    try testing.expect(!tool.dragging and tool.start == null);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(editor.bridges_generation != generation);

    // Every span on the drag's first y, the ends at 20 and 20 + 4 * 32.
    const infos = try editor.bridges(testing.allocator);
    defer testing.allocator.free(infos);
    const info = infos[bridges_before];
    try testing.expectEqualStrings("W_Fake_Bridge_01", info.descSlice());
    try testing.expectEqual(@as(i32, 6), info.span_count);
    try testing.expectEqual(@as(f32, 20), info.min_x);
    try testing.expectEqual(@as(f32, 20 + 4 * 32), info.max_x);
    try testing.expectEqual(@as(f32, 150), info.min_y);
    try testing.expectEqual(@as(f32, 150), info.max_y);
}

test "a vertical drag with a horizontal type is refused and records nothing" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_Fake_Bridge_01");
    const bridges_before = fake.bridgeCount();
    const objects_before = editor.document.objects.items.len;
    try testing.expectError(error.Refused, dragAcross(&tool, &editor, 60, 20, 64, 200));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "horizontally") != null);
    try testing.expectEqual(bridges_before, fake.bridgeCount());
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expect(!tool.dragging);
    // The vertical partner takes the same drag.
    tool.setDesc("W_Fake_Bridge_02");
    try dragAcross(&tool, &editor, 60, 20, 64, 200);
    try testing.expectEqual(bridges_before + 1, fake.bridgeCount());
}

test "a drag whose spans would leave the map is refused and records nothing" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    // The map is 8 tiles of 32 units: a drag from 200 to 300 puts its end
    // span past 256. (The pointer never resolves off the map, so the editor
    // is asked directly, as a panel or a script could.)
    const bridges_before = fake.bridgeCount();
    const objects_before = editor.document.objects.items.len;
    try testing.expectError(error.Refused, editor.drawBridge("W_Fake_Bridge_01", 200, 60, 300, 60));
    try testing.expect(editor.status().len != 0);
    try testing.expectEqual(bridges_before, fake.bridgeCount());
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "undo removes the spans and the entry, redo puts both back" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_Fake_Bridge_01");
    const bridges_before = fake.bridgeCount();
    const objects_before = editor.document.objects.items.len;
    try dragAcross(&tool, &editor, 20, 150, 120, 150);
    const drawn = fake.bridgeEntry(bridges_before).*;
    const generation = editor.bridges_generation;

    try testing.expect(try editor.undo());
    try testing.expectEqual(bridges_before, fake.bridgeCount());
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    for (drawn.linkSlice()) |link| try testing.expect(editor.document.find(link) == null);
    try testing.expect(editor.bridges_generation != generation);
    try testing.expect(!editor.dirty());

    try testing.expect(try editor.redo());
    try testing.expectEqual(bridges_before + 1, fake.bridgeCount());
    try testing.expectEqualSlices(i32, drawn.linkSlice(), fake.bridgeEntry(bridges_before).linkSlice());
    for (drawn.linkSlice()) |link| try testing.expect(editor.document.find(link) != null);
    try testing.expectEqual(objects_before + drawn.count, editor.document.objects.items.len);
}

test "a re-read that fails after an undo leaves the step on the redo stack, where the bridge is (WR-B01)" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_Fake_Bridge_01");
    const bridges_before = fake.bridgeCount();
    try dragAcross(&tool, &editor, 20, 150, 120, 150);
    const undo_depth = editor.history.undo_stack.items.len;
    fake.fail_objects = true;
    try testing.expectError(error.Failed, editor.undo());
    fake.fail_objects = false;
    // The bridge took the bridge out, and the history followed it.
    try testing.expectEqual(bridges_before, fake.bridgeCount());
    try testing.expectEqual(undo_depth - 1, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 1), editor.history.redo_stack.items.len);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "reopen the map") != null);
    // So the redo is the next step, and it works.
    try testing.expect(try editor.redo());
    try testing.expectEqual(bridges_before + 1, fake.bridgeCount());
    try testing.expect(try editor.undo());
    try testing.expectEqual(bridges_before, fake.bridgeCount());
}

test "no type chosen is a note, not a command; the ghost follows the drag" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: BridgeTool = .{};
    try dragAcross(&tool, &editor, 20, 150, 120, 150);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "bridge type") != null);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    // The plan the app draws the ghost from, changing nothing.
    var pieces: [16]bridge_mod.PlannedPiece = undefined;
    const planned = (try editor.planBridge("W_Fake_Bridge_01", 20, 150, 120, 150, &pieces)).?;
    try testing.expectEqual(@as(usize, 5), planned);
    try testing.expectEqual(@as(i32, 1), pieces[0].type);
    try testing.expectEqual(@as(i32, 4), pieces[planned - 1].type);
    try testing.expectEqual(@as(?usize, null), try editor.planBridge("W_Fake_Bridge_01", 20, 20, 20, 150, &pieces));
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "a click on a span selects its whole bridge; a click on empty ground drops it" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_Fake_Bridge_01");
    try dragAcross(&tool, &editor, 20, 150, 120, 150);
    const drawn = tool.selected.?;
    // The fixture's one-span bridge 0 at (100, 40).
    try tool.handle(&editor, .{ .press = try at(&editor, 101, 41) });
    try tool.handle(&editor, .{ .release = try at(&editor, 102, 41) });
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    // A span in the middle of the drawn bridge selects it.
    try tool.handle(&editor, .{ .press = try at(&editor, 68, 150) });
    try tool.handle(&editor, .{ .release = try at(&editor, 68, 150) });
    try testing.expectEqual(@as(?usize, drawn), tool.selected);
    // Empty ground: nothing selected, and a click never draws.
    const bridges = fake.bridgeCount();
    try tool.handle(&editor, .{ .press = try at(&editor, 200, 220) });
    try tool.handle(&editor, .{ .release = try at(&editor, 201, 220) });
    try testing.expectEqual(@as(?usize, null), tool.selected);
    try testing.expectEqual(bridges, fake.bridgeCount());
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
}

test "Delete removes the selected bridge whole, and undo puts the spans and the entry back at the same index" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_Fake_Bridge_01");
    try dragAcross(&tool, &editor, 20, 150, 120, 150);
    try dragAcross(&tool, &editor, 20, 200, 120, 200);
    // Select the first drawn bridge (entry 1; the fixture's is 0) and delete it.
    try tool.handle(&editor, .{ .press = try at(&editor, 36, 150) });
    try tool.handle(&editor, .{ .release = try at(&editor, 36, 150) });
    try testing.expectEqual(@as(?usize, 1), tool.selected);
    const entry = fake.bridgeEntry(1).*;
    const last = fake.bridgeEntry(2).*;
    const objects = editor.document.objects.items.len;
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(?usize, null), tool.selected);
    try testing.expectEqual(@as(usize, 2), fake.bridgeCount());
    try testing.expectEqualSlices(i32, last.linkSlice(), fake.bridgeEntry(1).linkSlice());
    try testing.expectEqual(objects - entry.count, editor.document.objects.items.len);
    for (entry.linkSlice()) |link| try testing.expect(editor.document.find(link) == null);
    try testing.expectEqual(@as(usize, 3), editor.history.undo_stack.items.len);

    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 3), fake.bridgeCount());
    try testing.expectEqualSlices(i32, entry.linkSlice(), fake.bridgeEntry(1).linkSlice());
    try testing.expectEqualSlices(i32, last.linkSlice(), fake.bridgeEntry(2).linkSlice());
    for (entry.linkSlice()) |link| try testing.expect(editor.document.find(link) != null);
    try testing.expectEqual(objects, editor.document.objects.items.len);
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(usize, 2), fake.bridgeCount());

    // Delete with nothing selected is a note, not a command.
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expect(std.mem.indexOf(u8, editor.status(), "select") != null);
    try testing.expectEqual(@as(usize, 3), editor.history.undo_stack.items.len);
}

test "a span alone is still refused to the object delete, and the pick finds its bridge" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    try testing.expectError(error.Refused, editor.delete(2));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "bridge 0") != null);
    const group = (try editor.pickGroup(100, 40)).?;
    try testing.expectEqual(bridge_mod.GroupKind.bridge, group.kind);
    try testing.expectEqual(@as(usize, 0), group.index);
    try testing.expectEqual(@as(?bridge_mod.GroupRef, null), try editor.pickGroup(200, 200));
}

fn fakeInfo(editor: *Editor, index: usize) !bridge_mod.BridgeInfo {
    const infos = try editor.bridges(testing.allocator);
    defer testing.allocator.free(infos);
    return infos[index];
}

test "Q rotates the selected bridge to its partner: same span count, the other axis, one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_Fake_Bridge_01");
    try dragAcross(&tool, &editor, 40, 150, 140, 150);
    const index = tool.selected.?;
    const before = try fakeInfo(&editor, index);
    const old_links = fake.bridgeEntry(index).*;
    try testing.expectEqual(@as(f32, before.min_y), before.max_y);
    try tool.handle(&editor, .{ .key = .rotate_left });
    const after = try fakeInfo(&editor, index);
    try testing.expectEqualStrings("W_Fake_Bridge_02", after.descSlice());
    try testing.expectEqual(before.span_count, after.span_count);
    // Vertical now: one x, a run along y, about the same centre.
    try testing.expectEqual(after.min_x, after.max_x);
    try testing.expect(after.max_y > after.min_y);
    try testing.expectApproxEqAbs((before.min_x + before.max_x) / 2, after.min_x, 0.01);
    try testing.expectApproxEqAbs((before.min_y + before.max_y) / 2, (after.min_y + after.max_y) / 2, 0.01);
    for (old_links.linkSlice()) |link| try testing.expect(editor.document.find(link) == null);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    // E rotates it back to the _01 type; undo walks both back.
    try tool.handle(&editor, .{ .key = .rotate_right });
    try testing.expectEqualStrings("W_Fake_Bridge_01", (try fakeInfo(&editor, index)).descSlice());
    try testing.expect(try editor.undo());
    try testing.expect(try editor.undo());
    try testing.expectEqualSlices(i32, old_links.linkSlice(), fake.bridgeEntry(index).linkSlice());
    try testing.expectEqualStrings("W_Fake_Bridge_01", (try fakeInfo(&editor, index)).descSlice());
}

test "a bridge with no rotated variant is refused and the history stays as it was" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("Lonely_Bridge");
    try dragAcross(&tool, &editor, 40, 150, 140, 150);
    const links = fake.bridgeEntry(tool.selected.?).*;
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .key = .rotate_left }));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "no rotated variant of Lonely_Bridge") != null);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expectEqualSlices(i32, links.linkSlice(), fake.bridgeEntry(tool.selected.?).linkSlice());
    // A rotation whose spans would leave the map is refused too: a long
    // horizontal bridge near the bottom edge turned upright.
    var long = bridgeTool("W_Fake_Bridge_01");
    try dragAcross(&long, &editor, 10, 240, 250, 240);
    try testing.expectError(error.Refused, long.handle(&editor, .{ .key = .rotate_right }));
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
}

test "Enter toggles a WoodenBig_Heavy bridge built during play, one step each way; other types are refused" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = bridgeTool("W_WoodenBig_Heavy_01");
    try dragAcross(&tool, &editor, 40, 150, 140, 150);
    const index = tool.selected.?;
    try testing.expect(!(try fakeInfo(&editor, index)).built_during_play);
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expect((try fakeInfo(&editor, index)).built_during_play);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    // Rotating keeps it built during play.
    try tool.handle(&editor, .{ .key = .rotate_left });
    try testing.expect((try fakeInfo(&editor, index)).built_during_play);
    try testing.expect(try editor.undo());
    try testing.expect(try editor.undo());
    try testing.expect(!(try fakeInfo(&editor, index)).built_during_play);
    try testing.expect(try editor.redo());
    try testing.expect((try fakeInfo(&editor, index)).built_during_play);

    var other = bridgeTool("W_Fake_Bridge_01");
    try dragAcross(&other, &editor, 40, 200, 140, 200);
    const depth = editor.history.undo_stack.items.len;
    try testing.expectError(error.Refused, other.handle(&editor, .{ .key = .enter }));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "WoodenBig_Heavy") != null);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
}

fn fenceTool(name: []const u8) FenceTool {
    var tool: FenceTool = .{};
    tool.setDesc(name);
    return tool;
}

/// A press at (x0, y0), a drag and a release at (x1, y1); the screen is the
/// world in the fake, so the drag is over its own pixels.
fn fenceDrag(tool: *FenceTool, editor: *Editor, x0: f32, y0: f32, x1: f32, y1: f32) EditError!void {
    try tool.handle(editor, .{ .press = try at(editor, x0, y0) });
    try tool.handle(editor, .{ .drag = try at(editor, (x0 + x1) / 2, (y0 + y1) / 2) });
    try tool.handle(editor, .{ .release = try at(editor, x1, y1) });
}

test "a horizontal fence drag over 10 tiles places 5 fences as one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = fenceTool("W_Fake_Fence");
    const objects_before = editor.document.objects.items.len;
    const generation = editor.bridges_generation;

    // The fake's AI tile is 16 world units: 24 is tile 1, 168 is tile 10.
    try tool.handle(&editor, .{ .press = try at(&editor, 24, 100) });
    try tool.handle(&editor, .{ .drag = try at(&editor, 100, 102) });
    try testing.expect(tool.dragging);
    try testing.expectEqual(@as(f32, 100), tool.current.?[0]);
    try tool.handle(&editor, .{ .release = try at(&editor, 168, 104) });

    // 10 tiles inclusive, every second one: 5 fences, one entry of the history.
    try testing.expectEqual(objects_before + 5, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(editor.bridges_generation != generation);
    try testing.expect(!tool.dragging and tool.start == null);
    // No bridges entry is made for a fence.
    try testing.expectEqual(@as(usize, 1), fake.bridgeCount());

    // The ghost (the same plan): dir 3 going right, types 8 | 0x10000, two
    // tiles apart along x, the last one two tiles on.
    var pieces: [16]bridge_mod.PlannedPiece = undefined;
    const planned = (try editor.planFences("W_Fake_Fence", 24, 100, 168, 104, false, &pieces)).?;
    try testing.expectEqual(@as(usize, 5), planned);
    for (pieces[0..planned], 0..) |piece, index| {
        try testing.expectEqual(@as(i32, (1 << 3) | 0x00010000), piece.type);
        try testing.expectEqual(@as(f32, @floatFromInt(1 + 2 + index * 2)) * 16, piece.x);
        try testing.expectEqual(@as(f32, 96), piece.y);
    }
}

test "the four drag directions give the directions 3, 1, 0 and 2" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var pieces: [16]bridge_mod.PlannedPiece = undefined;
    // Right, left, up (smaller y), down; the run is axis-locked, so the small
    // cross-axis wobble changes nothing.
    const cases = [_]struct { x0: f32, y0: f32, x1: f32, y1: f32, dir: u5 }{
        .{ .x0 = 40, .y0 = 100, .x1 = 168, .y1 = 108, .dir = 3 },
        .{ .x0 = 168, .y0 = 100, .x1 = 40, .y1 = 108, .dir = 1 },
        .{ .x0 = 100, .y0 = 168, .x1 = 108, .y1 = 40, .dir = 0 },
        .{ .x0 = 100, .y0 = 40, .x1 = 108, .y1 = 168, .dir = 2 },
    };
    for (cases) |case| {
        const count = (try editor.planFences("W_Fake_Fence", case.x0, case.y0, case.x1, case.y1, false, &pieces)).?;
        try testing.expect(count >= 2);
        for (pieces[0..count]) |piece| try testing.expectEqual(@as(i32, (@as(i32, 1) << case.dir) | 0x00010000), piece.type);
    }
}

test "a click places one fence, direction 0, and direction 1 with Ctrl" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = fenceTool("W_Fake_Fence");
    const objects_before = editor.document.objects.items.len;

    // A press and release on the same pixel: one fence, one undo step.
    try tool.handle(&editor, .{ .press = try at(&editor, 120, 120) });
    try tool.handle(&editor, .{ .release = try at(&editor, 121, 121) });
    try testing.expectEqual(objects_before + 1, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);

    var pieces: [4]bridge_mod.PlannedPiece = undefined;
    try testing.expectEqual(@as(?usize, 1), try editor.planFences("W_Fake_Fence", 120, 120, 120, 120, false, &pieces));
    try testing.expectEqual(@as(i32, 1 | 0x00010000), pieces[0].type);
    try testing.expectEqual(@as(?usize, 1), try editor.planFences("W_Fake_Fence", 120, 120, 120, 120, true, &pieces));
    try testing.expectEqual(@as(i32, 2 | 0x00010000), pieces[0].type);

    // Ctrl is a modifier of the click, not a right click: the tool has no
    // right button, and the press still places its fence.
    var press = try at(&editor, 60, 60);
    press.ctrl = true;
    var release = try at(&editor, 60, 60);
    release.ctrl = true;
    try tool.handle(&editor, .{ .press = press });
    try tool.handle(&editor, .{ .release = release });
    try testing.expectEqual(objects_before + 2, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    try testing.expect(!tool.dragging);
}

test "a run with an end off the map is refused whole and records nothing" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const objects_before = editor.document.objects.items.len;
    // The map is 8 tiles of 32 units: 256 world units; an end at 300 is off.
    // (The pointer never resolves off the map, so the editor is asked
    // directly, as a panel or a script could.)
    try testing.expectError(error.Refused, editor.drawFences("W_Fake_Fence", 40, 100, 300, 100, false));
    try testing.expectEqualStrings("the fence run leaves the map", editor.status());
    try testing.expectError(error.Refused, editor.drawFences("W_Fake_Fence", -10, 100, 100, 100, false));
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    var pieces: [4]bridge_mod.PlannedPiece = undefined;
    try testing.expectEqual(@as(?usize, null), try editor.planFences("W_Fake_Fence", 40, 100, 300, 100, false, &pieces));
    // An unknown type is refused too, changing nothing.
    try testing.expectError(error.Refused, editor.drawFences("No_Such_Fence", 40, 100, 100, 100, false));
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
}

test "undo takes the run out, redo puts it back, and a fence is an ordinary object" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool = fenceTool("W_Fake_Fence");
    const objects_before = editor.document.objects.items.len;
    try fenceDrag(&tool, &editor, 24, 100, 168, 100);
    try testing.expectEqual(objects_before + 5, editor.document.objects.items.len);
    const first_fence = editor.document.objects.items[objects_before].link_id;

    // Ordinary: the object commands move and delete one fence.
    try editor.delete(first_fence);
    try testing.expectEqual(objects_before + 4, editor.document.objects.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(objects_before + 5, editor.document.objects.items.len);

    // Undo the run whole, then redo it.
    try testing.expect(try editor.undo());
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    try testing.expect(editor.document.find(first_fence) == null);
    try testing.expect(try editor.redo());
    try testing.expectEqual(objects_before + 5, editor.document.objects.items.len);
    try testing.expect(editor.document.find(first_fence) != null);
}

test "no type chosen is a note; Escape gives the drag up" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: FenceTool = .{};
    const objects_before = editor.document.objects.items.len;
    try fenceDrag(&tool, &editor, 24, 100, 168, 100);
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expect(editor.status().len != 0);
    tool.setDesc("W_Fake_Fence");
    try tool.handle(&editor, .{ .press = try at(&editor, 24, 100) });
    try testing.expect(tool.dragging);
    try tool.handle(&editor, .{ .key = .escape });
    try testing.expect(!tool.dragging and tool.start == null);
    try tool.handle(&editor, .{ .release = try at(&editor, 168, 100) });
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
}

fn trenchClick(tool: *EntrenchmentTool, editor: *Editor, x: f32, y: f32) EditError!void {
    try tool.handle(editor, .{ .press = try at(editor, x, y) });
    try tool.handle(editor, .{ .release = try at(editor, x, y) });
}

test "three clicks and a double click draw one entrenchment as one undo step" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: EntrenchmentTool = .{ .player = 1 };
    const objects_before = editor.document.objects.items.len;
    const generation = editor.entrenchments_generation;
    // A double click arrives as the single click's press and release, then
    // double_click (SDL: clicks 1, then clicks 2).
    try trenchClick(&tool, &editor, 20, 200);
    try trenchClick(&tool, &editor, 120, 200);
    try trenchClick(&tool, &editor, 120, 100);
    try testing.expectEqual(@as(usize, 3), tool.len);
    try tool.handle(&editor, .{ .double_click = try at(&editor, 120, 100) });

    // Two terminators and one piece per step: 4 pieces, one entry.
    try testing.expectEqual(@as(usize, 1), fake.trenchCount());
    try testing.expectEqual(@as(usize, 4), fake.trenchEntry(0).count);
    try testing.expectEqual(objects_before + 4, editor.document.objects.items.len);
    for (fake.trenchEntry(0).linkSlice()) |link| try testing.expectEqual(@as(i32, 1), editor.document.find(link).?.player);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(editor.entrenchments_generation != generation);
    try testing.expect(!tool.drawing());
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    const infos = try editor.entrenchments(testing.allocator);
    defer testing.allocator.free(infos);
    try testing.expectEqual(@as(usize, 1), infos.len);
    try testing.expectEqual(@as(i32, 4), infos[0].piece_count);
    try testing.expectEqual(@as(i32, 1), infos[0].player);

    // Undo takes the pieces and the entry out, redo puts them back.
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 0), fake.trenchCount());
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    try testing.expect(!editor.dirty());
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(usize, 1), fake.trenchCount());
    try testing.expectEqual(objects_before + 4, editor.document.objects.items.len);
}

test "a right click clears the polyline with no history; a click on the same point twice is one point" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: EntrenchmentTool = .{};
    try trenchClick(&tool, &editor, 20, 200);
    try trenchClick(&tool, &editor, 21, 201);
    try testing.expectEqual(@as(usize, 1), tool.len);
    try trenchClick(&tool, &editor, 120, 200);
    try testing.expectEqual(@as(usize, 2), tool.len);
    try tool.handle(&editor, .{ .right_press = try at(&editor, 100, 100) });
    try tool.handle(&editor, .{ .right_release = try at(&editor, 100, 100) });
    try testing.expect(!tool.drawing());
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    // Escape clears too.
    try trenchClick(&tool, &editor, 20, 200);
    try tool.handle(&editor, .{ .key = .escape });
    try testing.expect(!tool.drawing());
    // A double click with nothing left commits nothing.
    try tool.handle(&editor, .{ .double_click = try at(&editor, 20, 200) });
    try testing.expectEqual(@as(usize, 0), fake.trenchCount());
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "a one-point commit is a note, not a command" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: EntrenchmentTool = .{};
    try trenchClick(&tool, &editor, 60, 60);
    try tool.handle(&editor, .{ .double_click = try at(&editor, 60, 60) });
    try testing.expect(std.mem.indexOf(u8, editor.status(), "two points") != null);
    try testing.expectEqual(@as(usize, 0), fake.trenchCount());
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    // The point is kept: a second click and a double click finish the trench.
    try testing.expectEqual(@as(usize, 1), tool.len);
    try trenchClick(&tool, &editor, 160, 60);
    try tool.handle(&editor, .{ .double_click = try at(&editor, 160, 60) });
    try testing.expectEqual(@as(usize, 1), fake.trenchCount());
}

test "a trench off the map is refused whole and records nothing; the preview plans the clicks and the pointer" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const objects_before = editor.document.objects.items.len;
    // The map is 256 world units a side. (The pointer never resolves off the
    // map, so the editor is asked directly, as a panel or a script could.)
    const off = [_]records.Vec3{ .{ .x = 200, .y = 100 }, .{ .x = 300, .y = 100 } };
    try testing.expectError(error.Refused, editor.drawEntrenchment(&off, 0));
    try testing.expectEqualStrings("the trench leaves the map", editor.status());
    try testing.expectEqual(objects_before, editor.document.objects.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    // A player the map lacks is refused too.
    const inside = [_]records.Vec3{ .{ .x = 20, .y = 100 }, .{ .x = 120, .y = 100 } };
    try testing.expectError(error.Failed, editor.drawEntrenchment(&inside, 9));
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);

    var tool: EntrenchmentTool = .{};
    try trenchClick(&tool, &editor, 20, 100);
    tool.hover(&editor, try at(&editor, 120, 100));
    var buffer: [max_trench_points + 1]records.Vec3 = undefined;
    const preview = tool.previewPoints(&buffer);
    try testing.expectEqual(@as(usize, 2), preview.len);
    var pieces: [8]bridge_mod.PlannedPiece = undefined;
    const planned = (try editor.planEntrenchment(preview, &pieces)).?;
    try testing.expectEqual(@as(usize, 3), planned);
    try testing.expectEqual(bridge_mod.trench_terminator, pieces[0].type);
    try testing.expectEqual(bridge_mod.trench_fireplace, pieces[2].type);
    try testing.expectEqual(@as(?usize, null), try editor.planEntrenchment(preview[0..1], &pieces));
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "a press on a piece selects its entrenchment; Delete removes it whole and undo puts it back at the same index" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: EntrenchmentTool = .{};
    // Two trenches: entry 0 along y = 200, entry 1 along y = 60.
    try trenchClick(&tool, &editor, 20, 200);
    try trenchClick(&tool, &editor, 120, 200);
    try tool.handle(&editor, .{ .double_click = try at(&editor, 120, 200) });
    try trenchClick(&tool, &editor, 20, 60);
    try trenchClick(&tool, &editor, 120, 60);
    try tool.handle(&editor, .{ .double_click = try at(&editor, 120, 60) });
    try testing.expectEqual(@as(usize, 2), fake.trenchCount());
    const first = fake.trenchEntry(0).*;
    const second = fake.trenchEntry(1).*;
    const objects = editor.document.objects.items.len;

    // A press on entry 0's middle piece (70, 200) selects it and draws nothing.
    try tool.handle(&editor, .{ .press = try at(&editor, 70, 200) });
    try tool.handle(&editor, .{ .release = try at(&editor, 70, 200) });
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    try testing.expect(!tool.drawing());
    // Hovering over entry 1 marks it, over bare ground nothing.
    tool.hover(&editor, try at(&editor, 120, 60));
    try testing.expectEqual(@as(?usize, 1), tool.hovered);
    tool.hover(&editor, try at(&editor, 200, 240));
    try testing.expectEqual(@as(?usize, null), tool.hovered);

    // Delete: the selected entrenchment goes whole, one undo step.
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 1), fake.trenchCount());
    try testing.expectEqualSlices(i32, second.linkSlice(), fake.trenchEntry(0).linkSlice());
    try testing.expectEqual(objects - first.count, editor.document.objects.items.len);
    for (first.linkSlice()) |link| try testing.expect(editor.document.find(link) == null);
    try testing.expectEqual(@as(usize, 3), editor.history.undo_stack.items.len);

    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 2), fake.trenchCount());
    try testing.expectEqualSlices(i32, first.linkSlice(), fake.trenchEntry(0).linkSlice());
    try testing.expectEqualSlices(i32, second.linkSlice(), fake.trenchEntry(1).linkSlice());
    for (first.linkSlice()) |link| try testing.expect(editor.document.find(link) != null);
    try testing.expectEqual(objects, editor.document.objects.items.len);
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(usize, 1), fake.trenchCount());

    // With nothing selected, Delete takes the hovered one.
    tool.hover(&editor, try at(&editor, 70, 60));
    try testing.expectEqual(@as(?usize, 0), tool.hovered);
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 0), fake.trenchCount());
    // And with neither, it is a note, not a command.
    const depth = editor.history.undo_stack.items.len;
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expect(std.mem.indexOf(u8, editor.status(), "delete") != null);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
}

test "a trench piece alone is still refused to the object delete (D-04)" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: EntrenchmentTool = .{};
    try trenchClick(&tool, &editor, 20, 200);
    try trenchClick(&tool, &editor, 120, 200);
    try tool.handle(&editor, .{ .double_click = try at(&editor, 120, 200) });
    const piece = fake.trenchEntry(0).links[2];
    try testing.expectError(error.Refused, editor.delete(piece));
    try testing.expectEqualStrings("still part of entrenchment 0", editor.status());
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(editor.document.find(piece) != null);
    // The pick finds the piece's entrenchment.
    const group = (try editor.pickGroup(70, 200)).?;
    try testing.expectEqual(bridge_mod.GroupKind.entrenchment, group.kind);
    try testing.expectEqual(@as(usize, 0), group.index);
}
