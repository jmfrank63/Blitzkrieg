//! The M2 marker layer (D-06): the app draws each kind of M2 record on
//! ImGui's background draw list, through BkEditorWorldToScreen, so panels draw
//! over the markers and a marker sits under the cursor the way a click
//! resolves (Pitfall 17: both use the z = 0 plane). View -> Markers switches
//! each kind on or off (`State.marker_set`); the active tool's own kinds are
//! always on (marker_logic.visible). 04-03 drew the camera anchors, 04-05 the
//! roads and rivers, 04-06 the bridge ghost and outlines; later plans add
//! their kind's function here and one call in `drawM2Markers`.
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
const sdl3 = @import("sdl3");

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
    // The Fence tool's ghost is its own and always on while it is active.
    if (state.view.tool == .fence) drawFenceGhost(state, real);
    // So are the Entrenchment tool's preview and outlines (04-08).
    if (state.view.tool == .entrenchment) drawTrenchMarkers(state, real);
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
    var points = mapBoxCorners(real, min_x, min_y, max_x, max_y, margin) orelse return;
    ig.ImDrawList_AddPolyline(draw_list, &points, 4, box_color, thickness, ig.ImDrawFlags_Closed);
}

fn refusedColor() ig.ImU32 {
    return color(1.0, 0.25, 0.2);
}

/// Not the engine mark's blue: the dashed outline has to read against it.
fn builtColor() ig.ImU32 {
    return color(1.0, 0.6, 0.1);
}

/// A dashed closed polyline: every other stretch of `dash` pixels drawn.
fn drawDashed(draw_list: *ig.ImDrawList, points: []const ig.ImVec2, dash_color: ig.ImU32, dash: f32, thickness: f32) void {
    for (points, 0..) |from, index| {
        const to = points[(index + 1) % points.len];
        const dx = to.x - from.x;
        const dy = to.y - from.y;
        const length = @sqrt(dx * dx + dy * dy);
        if (length <= 0) continue;
        var at: f32 = 0;
        while (at < length) : (at += 2 * dash) {
            const end = @min(at + dash, length);
            ig.ImDrawList_AddLineEx(draw_list, .{ .x = from.x + dx * at / length, .y = from.y + dy * at / length }, .{ .x = from.x + dx * end / length, .y = from.y + dy * end / length }, dash_color, thickness);
        }
    }
}

/// The four screen corners of a map-unit box grown by `margin` world units;
/// null when one does not convert.
fn mapBoxCorners(real: anytype, min_x: f32, min_y: f32, max_x: f32, max_y: f32, margin: f32) ?[4]ig.ImVec2 {
    const low = marker_logic.aiToWorld(.{ .x = min_x, .y = min_y });
    const high = marker_logic.aiToWorld(.{ .x = max_x, .y = max_y });
    const corners = [4][2]f32{
        .{ low.x - margin, low.y - margin },
        .{ high.x + margin, low.y - margin },
        .{ high.x + margin, high.y + margin },
        .{ low.x - margin, high.y + margin },
    };
    var points: [4]ig.ImVec2 = undefined;
    for (corners, &points) |corner, *point| point.* = screenOf(real, corner[0], corner[1]) orelse return null;
    return points;
}

/// D-10..D-12 while the Bridge tool is active: the drag's ghost (the planned
/// span centres and their outline in green, or the drag in red with the
/// refusal), the selected bridge's outline, and a dashed outline round every
/// bridge built during play, beside the engine's own mark (C1).
fn drawBridgeOutlines(state: *State, real: anytype) void {
    if (state.view.tool != .bridge) return;
    state.refreshBridges();
    const draw_list = ig.igGetBackgroundDrawList();
    for (state.bridge_infos) |info| {
        if (!info.built_during_play) continue;
        const points = mapBoxCorners(real, info.min_x, info.min_y, info.max_x, info.max_y, bridge_outline_margin * 1.4) orelse continue;
        drawDashed(draw_list, &points, builtColor(), 10, 2.5);
    }
    if (state.view.bridge_tool.selected) |selected| {
        if (selected < state.bridge_infos.len) {
            const info = state.bridge_infos[selected];
            drawMapBox(draw_list, real, info.min_x, info.min_y, info.max_x, info.max_y, bridge_outline_margin, outlineColor(), 3);
        }
    }
    drawBridgeGhost(state, real, draw_list);
}

/// The ghost of the drag in hand (research Q2: drawn by the app from the
/// plan, no engine objects): planned again only when the drag or the type
/// changed since the last frame.
fn drawBridgeGhost(state: *State, real: anytype, draw_list: *ig.ImDrawList) void {
    const tool = &state.view.bridge_tool;
    if (!tool.dragging or tool.desc().len == 0) return;
    const from = tool.start orelse return;
    const to = tool.current orelse return;
    const ghost = &state.bridge_ghost;
    const same_desc = std.mem.eql(u8, std.mem.sliceTo(&ghost.desc, 0), tool.desc());
    if (!ghost.valid or !same_desc or ghost.from[0] != from[0] or ghost.from[1] != from[1] or ghost.to[0] != to[0] or ghost.to[1] != to[1]) {
        ghost.valid = true;
        ghost.from = from;
        ghost.to = to;
        @memset(&ghost.desc, 0);
        @memcpy(ghost.desc[0..tool.desc().len], tool.desc());
        const planned = state.editor.planBridge(tool.desc(), from[0], from[1], to[0], to[1], &ghost.pieces) catch null;
        ghost.refused = planned == null;
        ghost.count = if (planned) |count| @min(count, ghost.pieces.len) else 0;
        const why = if (planned == null) state.editor.bridge.lastMessage() else "";
        ghost.why_len = @min(why.len, ghost.why.len);
        @memcpy(ghost.why[0..ghost.why_len], why[0..ghost.why_len]);
    }
    if (ghost.refused or ghost.count == 0) {
        const a = screenOf(real, from[0], from[1]) orelse return;
        const b = screenOf(real, to[0], to[1]) orelse return;
        ig.ImDrawList_AddLineEx(draw_list, a, b, refusedColor(), 3);
        const why = ghost.why[0..ghost.why_len];
        ig.ImDrawList_AddTextEx(draw_list, .{ .x = b.x + 8, .y = b.y - 8 }, refusedColor(), why.ptr, why.ptr + why.len);
        return;
    }
    const pieces = ghost.pieces[0..ghost.count];
    var min_x = pieces[0].x;
    var min_y = pieces[0].y;
    var max_x = pieces[0].x;
    var max_y = pieces[0].y;
    for (pieces) |piece| {
        min_x = @min(min_x, piece.x);
        min_y = @min(min_y, piece.y);
        max_x = @max(max_x, piece.x);
        max_y = @max(max_y, piece.y);
        const world = marker_logic.aiToWorld(.{ .x = piece.x, .y = piece.y });
        const at = screenOf(real, world.x, world.y) orelse continue;
        drawSquare(draw_list, at, 4, outlineColor(), piece.type != 2);
    }
    drawMapBox(draw_list, real, min_x, min_y, max_x, max_y, bridge_outline_margin, outlineColor(), 2);
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

/// The length (map units) of a fence's bar in the ghost: three AI tiles, the
/// footprint of a fence segment.
const fence_bar_length: f32 = 96.0;

/// D-14: the Fence tool's ghost (research Q2: drawn by the app from the plan,
/// no engine objects). While a drag is held it is the run from the press to
/// the pointer; between drags it is the one fence under the pointer, flipped
/// by Ctrl. Each planned fence is a short bar along its direction in green;
/// a refused run (an end off the map) is the drag in red with the reason.
/// Planned again only when the drag, the type or Ctrl changed.
fn drawFenceGhost(state: *State, real: anytype) void {
    const tool = &state.view.fence_tool;
    if (tool.desc().len == 0) return;
    var from: [2]f32 = undefined;
    var to: [2]f32 = undefined;
    var ctrl = sdl3.c.SDL_GetModState() & sdl3.c.SDL_KMOD_CTRL != 0;
    if (tool.dragging) {
        from = tool.start orelse return;
        to = tool.current orelse return;
        ctrl = tool.ctrl or ctrl;
    } else {
        const hover = state.view.hover orelse return;
        from = .{ hover.world_x, hover.world_y };
        to = from;
    }
    const draw_list = ig.igGetBackgroundDrawList();
    const ghost = &state.fence_ghost;
    const same_desc = std.mem.eql(u8, std.mem.sliceTo(&ghost.desc, 0), tool.desc());
    if (!ghost.valid or !same_desc or ghost.ctrl != ctrl or ghost.from[0] != from[0] or ghost.from[1] != from[1] or ghost.to[0] != to[0] or ghost.to[1] != to[1]) {
        ghost.valid = true;
        ghost.from = from;
        ghost.to = to;
        ghost.ctrl = ctrl;
        @memset(&ghost.desc, 0);
        @memcpy(ghost.desc[0..tool.desc().len], tool.desc());
        const planned = state.editor.planFences(tool.desc(), from[0], from[1], to[0], to[1], ctrl, &ghost.pieces) catch null;
        ghost.refused = planned == null;
        ghost.count = if (planned) |count| @min(count, ghost.pieces.len) else 0;
        const why = if (planned == null) state.editor.bridge.lastMessage() else "";
        ghost.why_len = @min(why.len, ghost.why.len);
        @memcpy(ghost.why[0..ghost.why_len], why[0..ghost.why_len]);
    }
    if (ghost.refused or ghost.count == 0) {
        // Only a drag in hand is worth a refusal on screen; the hover ghost
        // off the map just goes.
        if (!tool.dragging) return;
        const a = screenOf(real, from[0], from[1]) orelse return;
        const b = screenOf(real, to[0], to[1]) orelse return;
        ig.ImDrawList_AddLineEx(draw_list, a, b, refusedColor(), 3);
        const why = ghost.why[0..ghost.why_len];
        ig.ImDrawList_AddTextEx(draw_list, .{ .x = b.x + 8, .y = b.y - 8 }, refusedColor(), why.ptr, why.ptr + why.len);
        return;
    }
    for (ghost.pieces[0..ghost.count]) |piece| {
        // dir 1 and 3 (bit 1 and 3 of the packed type) run along x, 0 and 2
        // along y, in map units.
        const horizontal = piece.type & 0b1010 != 0;
        const half = fence_bar_length / 2;
        const a = marker_logic.aiToWorld(.{ .x = piece.x - (if (horizontal) half else 0), .y = piece.y - (if (horizontal) 0 else half) });
        const b = marker_logic.aiToWorld(.{ .x = piece.x + (if (horizontal) half else 0), .y = piece.y + (if (horizontal) 0 else half) });
        const sa = screenOf(real, a.x, a.y) orelse continue;
        const sb = screenOf(real, b.x, b.y) orelse continue;
        ig.ImDrawList_AddLineEx(draw_list, sa, sb, outlineColor(), 4);
    }
}

/// A trench piece's colour in the preview, by its packed type: fireplace
/// orange, line green, terminator cyan, arc yellow.
fn trenchPieceColor(piece_type: i32) ig.ImU32 {
    return switch (piece_type) {
        core.bridge.trench_fireplace => color(1.0, 0.55, 0.15),
        core.bridge.trench_line => color(0.2, 1.0, 0.2),
        core.bridge.trench_terminator => color(0.3, 0.9, 1.0),
        else => color(1.0, 1.0, 0.3),
    };
}

fn hoveredColor() ig.ImU32 {
    return color(1.0, 1.0, 0.3);
}

/// How long (world units) a planned piece's direction tick is: half a shipped
/// line piece.
const trench_tick_length: f32 = 36.0;

/// D-13 while the Entrenchment tool is active: the highlighted entrenchment's
/// outline (yellow, the MFC tool's hover highlight), the selected one's
/// (green), and - while a polyline is being clicked - the live preview.
fn drawTrenchMarkers(state: *State, real: anytype) void {
    state.refreshTrenches();
    const draw_list = ig.igGetBackgroundDrawList();
    const tool = &state.view.trench_tool;
    if (tool.hovered) |hovered| {
        if (hovered < state.trench_infos.len and tool.selected != hovered) {
            const info = state.trench_infos[hovered];
            if (mapBoxCorners(real, info.min_x, info.min_y, info.max_x, info.max_y, bridge_outline_margin)) |points| drawDashed(draw_list, &points, hoveredColor(), 8, 2.5);
        }
    }
    if (tool.selected) |selected| {
        if (selected < state.trench_infos.len) {
            const info = state.trench_infos[selected];
            drawMapBox(draw_list, real, info.min_x, info.min_y, info.max_x, info.max_y, bridge_outline_margin, outlineColor(), 3);
        }
    }
    drawTrenchPreview(state, real, draw_list);
}

/// The preview (research Q2: drawn by the app from the plan, no engine
/// objects): the clicked polyline to the pointer, and the pieces a click
/// there and a double click would commit - each a square at its centre in
/// its type's colour with a tick along its direction; a refusal is the
/// polyline in red with the reason. Planned again only when the clicks or the
/// pointer moved.
fn drawTrenchPreview(state: *State, real: anytype, draw_list: *ig.ImDrawList) void {
    const tool = &state.view.trench_tool;
    if (!tool.drawing()) return;
    var buffer: [panels.TrenchGhost.max_points]core.records.Vec3 = undefined;
    const points = tool.previewPoints(&buffer);
    const ghost = &state.trench_ghost;
    const same = ghost.valid and ghost.point_count == points.len and
        std.mem.eql(u8, std.mem.sliceAsBytes(ghost.points[0..ghost.point_count]), std.mem.sliceAsBytes(points));
    if (!same) {
        ghost.valid = true;
        ghost.point_count = points.len;
        @memcpy(ghost.points[0..points.len], points);
        const planned = state.editor.planEntrenchment(points, &ghost.pieces) catch null;
        ghost.refused = planned == null;
        ghost.count = if (planned) |count| @min(count, ghost.pieces.len) else 0;
        const why = if (planned == null) state.editor.bridge.lastMessage() else "";
        ghost.why_len = @min(why.len, ghost.why.len);
        @memcpy(ghost.why[0..ghost.why_len], why[0..ghost.why_len]);
    }
    const line_color = if (ghost.refused and points.len >= 2) refusedColor() else color(1.0, 0.3, 0.3);
    var previous: ?ig.ImVec2 = null;
    for (points) |point| {
        const at = screenOf(real, point.x, point.y) orelse {
            previous = null;
            continue;
        };
        if (previous) |from| ig.ImDrawList_AddLineEx(draw_list, from, at, line_color, 1.5);
        drawSquare(draw_list, at, 3, line_color, true);
        previous = at;
    }
    if (ghost.refused) {
        // One point alone is not yet a trench, not a refusal worth a word.
        if (points.len < 2) return;
        const last = screenOf(real, points[points.len - 1].x, points[points.len - 1].y) orelse return;
        const why = ghost.why[0..ghost.why_len];
        ig.ImDrawList_AddTextEx(draw_list, .{ .x = last.x + 8, .y = last.y - 8 }, refusedColor(), why.ptr, why.ptr + why.len);
        return;
    }
    for (ghost.pieces[0..ghost.count]) |piece| {
        const world = marker_logic.aiToWorld(.{ .x = piece.x, .y = piece.y });
        const at = screenOf(real, world.x, world.y) orelse continue;
        const piece_color = trenchPieceColor(piece.type);
        drawSquare(draw_list, at, 4, piece_color, piece.type != core.bridge.trench_arc);
        // The piece's direction: its angle in world units (the builder works
        // there), 65536 to a full turn.
        const angle = @as(f32, @floatFromInt(piece.dir)) / 65535.0 * 2.0 * std.math.pi;
        const tip = screenOf(real, world.x + @cos(angle) * trench_tick_length / 2, world.y + @sin(angle) * trench_tick_length / 2) orelse continue;
        ig.ImDrawList_AddLineEx(draw_list, at, tip, piece_color, 2);
    }
}
