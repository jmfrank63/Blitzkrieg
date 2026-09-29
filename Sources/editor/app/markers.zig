//! The M2 marker layer (D-06): the app draws each kind of M2 record on
//! ImGui's background draw list, through BkEditorWorldToScreen, so panels draw
//! over the markers and a marker sits under the cursor the way a click
//! resolves (Pitfall 17: both use the z = 0 plane). View -> Markers switches
//! each kind on or off (`State.marker_set`); the active tool's own kinds are
//! always on (marker_logic.visible). This plan draws the camera anchors;
//! later plans add their kind's function here and one call in `drawM2Markers`.
//!
//! An item whose conversion fails (off the current view, no camera yet) is
//! skipped alone, never the whole kind; every kind is capped
//! (marker_logic.cap) so a malformed map cannot make a frame draw thousands of
//! shapes (T-04-03-02).
const std = @import("std");
const imgui = @import("editor_imgui");
const panels = @import("panels.zig");
const marker_logic = @import("marker_logic.zig");

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
    // The active tool's own kinds: none of the tools so far has markers.
    const active = marker_logic.MarkerSet.none();
    if (marker_logic.visible(state.marker_set, .camera_anchors, active)) drawCameraAnchors(state, real);
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
