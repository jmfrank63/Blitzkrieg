//! The M2 marker layer (D-06): the app draws each kind of M2 record on
//! ImGui's background draw list, through BkEditorWorldToScreen, so panels draw
//! over the markers and a marker sits under the cursor the way a click
//! resolves (Pitfall 17: both use the z = 0 plane). View -> Markers switches
//! each kind on or off (`State.marker_set`); the active tool's own kinds are
//! always on (marker_logic.visible). 04-03 drew the camera anchors, 04-05 the
//! roads and rivers; later plans add their kind's function here and one call
//! in `drawM2Markers`.
//!
//! An item whose conversion fails (off the current view, no camera yet) is
//! skipped alone, never the whole kind; every kind is capped
//! (marker_logic.cap) so a malformed map cannot make a frame draw thousands of
//! shapes (T-04-03-02).
const std = @import("std");
const imgui = @import("editor_imgui");
const panels = @import("panels.zig");
const marker_logic = @import("marker_logic.zig");
const tool_registry = @import("tool_registry.zig");
const core = @import("editor_core");

const ig = imgui.c;
const State = panels.State;

fn color(r: f32, g: f32, b: f32) ig.ImU32 {
    return ig.igColorConvertFloat4ToU32(.{ .x = r, .y = g, .z = b, .w = 1 });
}

fn anchorColor() ig.ImU32 {
    return color(1.0, 0.55, 0.15);
}

fn labelColor() ig.ImU32 {
    return color(1.0, 1.0, 0.0);
}

/// Called once a frame from `panels.draw`, next to View.drawOverlay.
pub fn drawM2Markers(state: *State, real: anytype) void {
    if (!panels.mapIsOpen(state.editor)) return;
    const active = tool_registry.entry(state.view.tool).marker_kinds;
    if (marker_logic.visible(state.marker_set, .camera_anchors, active)) drawCameraAnchors(state, real);
    if (marker_logic.visible(state.marker_set, .roads_rivers, active)) drawRoadsRivers(state, real);
    if (marker_logic.visible(state.marker_set, .selection_outline, active)) drawBridgeOutlines(state, real);
}

fn outlineColor() ig.ImU32 {
    return color(0.2, 1.0, 0.2);
}

/// How far (world units) a bridge outline stands off its spans' centres: half
/// a tile, so the outline clears the span it goes round.
const bridge_outline_margin: f32 = 32.0 * std.math.sqrt2 / 2.0;

/// The outline of a box given in MAP units, grown by `margin` world units,
/// through four world corners on the z = 0 plane (Pitfall 17).
fn drawMapBox(draw_list: *ig.ImDrawList, real: anytype, min_x: f32, min_y: f32, max_x: f32, max_y: f32, margin: f32, box_color: ig.ImU32, thickness: f32) void {
    const low = marker_logic.aiToWorld(.{ .x = min_x, .y = min_y });
    const high = marker_logic.aiToWorld(.{ .x = max_x, .y = max_y });
    const corners = [4][2]f32{
        .{ low.x - margin, low.y - margin },
        .{ high.x + margin, low.y - margin },
        .{ high.x + margin, high.y + margin },
        .{ low.x - margin, high.y + margin },
    };
    var points: [4]ig.ImVec2 = undefined;
    for (corners, &points) |corner, *point| point.* = screenOf(real, corner[0], corner[1]) orelse return;
    ig.ImDrawList_AddPolyline(draw_list, &points, 4, box_color, thickness, ig.ImDrawFlags_Closed);
}

/// D-11: the Bridge tool's selected bridge, outlined round all its spans.
fn drawBridgeOutlines(state: *State, real: anytype) void {
    if (state.view.tool != .bridge) return;
    state.refreshBridges();
    const selected = state.view.bridge_tool.selected orelse return;
    if (selected >= state.bridge_infos.len) return;
    const info = state.bridge_infos[selected];
    const draw_list = ig.igGetBackgroundDrawList();
    drawMapBox(draw_list, real, info.min_x, info.min_y, info.max_x, info.max_y, bridge_outline_margin, outlineColor(), 3);
}

fn roadColor() ig.ImU32 {
    return color(0.95, 0.75, 0.35);
}

fn riverColor() ig.ImU32 {
    return color(0.35, 0.65, 1.0);
}

fn selectedColor() ig.ImU32 {
    return color(0.2, 1.0, 0.2);
}

/// The MFC tool's control-point colour (CONTROL_POINT_COLOR, pinkish).
fn controlColor() ig.ImU32 {
    return color(1.0, 0.5, 0.5);
}

/// The MFC tool's key-point colour (KEY_POINT_COLOR, pale yellow).
fn keyColor() ig.ImU32 {
    return color(1.0, 1.0, 0.5);
}

fn screenOf(real: anytype, x: f32, y: f32) ?ig.ImVec2 {
    const at = real.worldToScreen(x, y) orelse return null;
    return .{ .x = at[0], .y = at[1] };
}

/// A polyline through world points, one segment at a time so a point that
/// does not convert breaks the line there rather than dropping it.
fn drawLine(draw_list: *ig.ImDrawList, real: anytype, points: []const core.records.Vec3, line_color: ig.ImU32, thickness: f32) void {
    var previous: ?ig.ImVec2 = null;
    for (points) |point| {
        const here = screenOf(real, point.x, point.y);
        if (previous != null and here != null) ig.ImDrawList_AddLineEx(draw_list, previous.?, here.?, line_color, thickness);
        previous = here;
    }
}

fn drawSquare(draw_list: *ig.ImDrawList, at: ig.ImVec2, half: f32, square_color: ig.ImU32, filled: bool) void {
    const min: ig.ImVec2 = .{ .x = at.x - half, .y = at.y - half };
    const max: ig.ImVec2 = .{ .x = at.x + half, .y = at.y + half };
    if (filled) ig.ImDrawList_AddRectFilled(draw_list, min, max, square_color) else ig.ImDrawList_AddRectEx(draw_list, min, max, square_color, 0, 2, 0);
}

/// D-06 for roads and rivers (MFC CVSOState::Draw): every road's and river's
/// centre line through its key points; for the selected one, the centre line
/// in the selection colour, the control polyline with a square per control
/// point (the one in hand filled) and a circle at each width handle, joined
/// across the stripe; while drawing, the unfinished line and its last leg to
/// the pointer.
fn drawRoadsRivers(state: *State, real: anytype) void {
    const draw_list = ig.igGetBackgroundDrawList();
    state.refreshVsoLines();
    const tool = &state.view.roads_rivers;
    var start: usize = 0;
    for (state.vso_line_ends.items, state.vso_line_kinds.items) |end, kind| {
        drawLine(draw_list, real, state.vso_line_points.items[start..end], if (kind == .road) roadColor() else riverColor(), 1.5);
        start = end;
    }

    if (tool.selectedView(state.editor)) |view| {
        var centre: [256]core.records.Vec3 = undefined;
        const key_count = @min(view.key_points.len, centre.len);
        for (view.key_points[0..key_count], centre[0..key_count]) |key, *point| point.* = .{ .x = key.x, .y = key.y };
        drawLine(draw_list, real, centre[0..key_count], selectedColor(), 3);
        drawLine(draw_list, real, view.control_points, controlColor(), 1);
        for (view.key_points[0..key_count], 0..) |key, index| {
            const plus = screenOf(real, key.x + key.nx * key.width, key.y + key.ny * key.width) orelse continue;
            const minus = screenOf(real, key.x - key.nx * key.width, key.y - key.ny * key.width) orelse continue;
            ig.ImDrawList_AddLine(draw_list, plus, minus, keyColor());
            const held = if (tool.grab) |grab| switch (grab) {
                .width => |w| w.key == index,
                .opacity => |o| o.key == index,
                .control => false,
            } else false;
            if (held) {
                ig.ImDrawList_AddCircleFilled(draw_list, plus, 5, keyColor(), 12);
                ig.ImDrawList_AddCircleFilled(draw_list, minus, 5, keyColor(), 12);
            } else {
                ig.ImDrawList_AddCircle(draw_list, plus, 5, keyColor());
                ig.ImDrawList_AddCircle(draw_list, minus, 5, keyColor());
            }
        }
        const in_hand: ?usize = switch (tool.last_grab) {
            .control => |control| control,
            .none, .key => null,
        };
        for (view.control_points, 0..) |point, index| {
            const at = screenOf(real, point.x, point.y) orelse continue;
            const hot = (in_hand != null and in_hand.? == index) or (tool.hovered_control != null and tool.hovered_control.? == index);
            drawSquare(draw_list, at, 5, controlColor(), hot);
        }
    }

    if (tool.adding()) {
        const pending = tool.pendingPoints();
        drawLine(draw_list, real, pending, controlColor(), 2);
        for (pending) |point| {
            const at = screenOf(real, point.x, point.y) orelse continue;
            drawSquare(draw_list, at, 5, controlColor(), true);
        }
        if (tool.cursor) |cursor| {
            const last = pending[pending.len - 1];
            const from = screenOf(real, last.x, last.y);
            const to = screenOf(real, cursor[0], cursor[1]);
            if (from != null and to != null) ig.ImDrawList_AddLine(draw_list, from.?, to.?, controlColor());
        }
    }
}

/// A small filled triangle at each set anchor with "N" (neutral) or "P<n>".
fn drawCameraAnchors(state: *State, real: anytype) void {
    const draw_list = ig.igGetBackgroundDrawList();
    const anchors = state.anchors;
    const limit = marker_logic.cap(.camera_anchors);
    var drawn: usize = 0;
    if (!anchors.neutral.isUnset()) {
        drawAnchor(draw_list, real, anchors.neutral.x, anchors.neutral.y, "N");
        drawn += 1;
    }
    var player: usize = 0;
    while (player < anchors.player_count and drawn < limit) : (player += 1) {
        const anchor = anchors.players[player];
        if (anchor.isUnset()) continue;
        var label: [8]u8 = undefined;
        const text = std.fmt.bufPrint(&label, "P{d}", .{player}) catch continue;
        drawAnchor(draw_list, real, anchor.x, anchor.y, text);
        drawn += 1;
    }
}

fn drawAnchor(draw_list: *ig.ImDrawList, real: anytype, wx: f32, wy: f32, label: []const u8) void {
    const screen = real.worldToScreen(wx, wy) orelse return;
    const half: f32 = 7.0;
    const top: ig.ImVec2 = .{ .x = screen[0], .y = screen[1] - half };
    const left: ig.ImVec2 = .{ .x = screen[0] - half, .y = screen[1] + half };
    const right: ig.ImVec2 = .{ .x = screen[0] + half, .y = screen[1] + half };
    ig.ImDrawList_AddTriangleFilled(draw_list, top, left, right, anchorColor());
    ig.ImDrawList_AddTriangle(draw_list, top, left, right, labelColor());
    ig.ImDrawList_AddTextEx(draw_list, .{ .x = screen[0] + half + 3, .y = screen[1] - half }, labelColor(), label.ptr, label.ptr + label.len);
}
