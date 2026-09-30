//! The group tools (04-06): the Bridge tool (D-10..D-12), the MFC Bridges tab
//! (Sources/src/MapEditor/RoadDrawState.cpp). Entrenchments (04-08) and
//! fences join it here.
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
