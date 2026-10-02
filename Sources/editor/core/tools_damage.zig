//! The Damage tool (M3, D-29/PARITY MT1): the MFC Map Tools tab's repair
//! mode as a one-press tool. A percentage (default 10, the MFC dialog's own
//! default, TabToolsDialog.cpp:95) is the hit's size; the left button
//! damages, the right button heals, the middle button - or Alt+click, the
//! trackpad's stand-in the view maps for the Heights tool - repairs to
//! full. The clamps and the missing-stats refusal live in the bridge (the
//! MFC's unguarded pTmp->pRPG dereference is not copied); the tool only
//! routes the click.
const std = @import("std");
const editor_mod = @import("editor.zig");
const bridge_mod = @import("bridge.zig");
const tools = @import("tools.zig");
const Editor = editor_mod.Editor;
const EditError = bridge_mod.EditError;

pub const Damage = struct {
    /// The hit's percentage (0..100), the MFC dialog's field.
    percent: f32 = 10,
    gesture: u32 = 0,

    pub fn handle(self: *Damage, editor: *Editor, event: tools.Event) EditError!void {
        switch (event) {
            .press, .right_press => |pointer| {
                const link_id = pointer.object orelse return;
                const mode: bridge_mod.DamageMode = if (event == .right_press)
                    .heal
                else if (pointer.buttons.middle)
                    .repair_full
                else
                    .damage;
                self.gesture = editor.beginGesture();
                editor.damageObject(link_id, mode, self.percent / 100.0, self.gesture) catch |err| {
                    self.gesture = 0;
                    return err;
                };
            },
            .release, .right_release => self.gesture = 0,
            .drag, .key, .right_drag, .double_click => {},
        }
    }
};

const testing = std.testing;
const fake_mod = @import("fake_bridge.zig");

fn opened(fake: *fake_mod.FakeBridge) !Editor {
    var editor = Editor.init(testing.allocator, fake.bridge());
    errdefer editor.deinit();
    try editor.open("fixture.bzm");
    return editor;
}

test "the damage tool damages, heals and repairs, one undo step per click" {
    var fake = try fake_mod.fixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var damage: Damage = .{};
    defer damage.gesture = 0;
    const pointer = try editor.resolve(40, 40); // the fixture tank, link 1
    try damage.handle(&editor, .{ .press = pointer });
    try testing.expectApproxEqAbs(@as(f32, 0.90), editor.document.find(1).?.hp, 0.0001);
    const depth = editor.history.undo_stack.items.len;
    try damage.handle(&editor, .{ .press = pointer });
    try testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try testing.expectApproxEqAbs(@as(f32, 0.80), editor.document.find(1).?.hp, 0.0001);
    // A heal gives the 10% back.
    try damage.handle(&editor, .{ .right_press = pointer });
    try testing.expectApproxEqAbs(@as(f32, 0.90), editor.document.find(1).?.hp, 0.0001);
    // The repair sets full.
    try damage.handle(&editor, .{ .press = .{ .world_x = pointer.world_x, .world_y = pointer.world_y, .map_x = pointer.map_x, .map_y = pointer.map_y, .tile = pointer.tile, .object = pointer.object, .screen_x = pointer.screen_x, .screen_y = pointer.screen_y, .buttons = .{ .middle = true } } });
    try testing.expectEqual(@as(f32, 1.0), editor.document.find(1).?.hp);
    // One click, one undo step: back to 0.90.
    _ = try editor.undo();
    try testing.expectEqual(@as(f32, 0.90), editor.document.find(1).?.hp);
}

test "the damage tool keeps a unit at 1% and refuses stats-less objects" {
    var fake = try fake_mod.fixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var damage: Damage = .{ .percent = 100 };
    // The 1% clamp: damaging the full tank by 100% leaves 1%.
    const pointer = try editor.resolve(40, 40);
    try damage.handle(&editor, .{ .press = pointer });
    try testing.expectEqual(@as(f32, 0.01), editor.document.find(1).?.hp);
    // A click on nothing is no edit at all.
    const depth = editor.history.undo_stack.items.len;
    const empty = try editor.resolve(220, 220);
    try damage.handle(&editor, .{ .press = empty });
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    // The missing-stats refusal (the MFC bug is not copied): the fake says
    // no stats and the tool refuses, changing nothing.
    fake.no_stats_fixture = true;
    try testing.expectError(error.Refused, damage.handle(&editor, .{ .press = pointer }));
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(f32, 0.01), editor.document.find(1).?.hp);
}
