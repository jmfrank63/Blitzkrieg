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
const panels_logic = @import("panels_logic.zig");
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
    if (marker_logic.visible(state.marker_set, .selection_outline, active)) {
        drawBridgeOutlines(state, real);
        // M3 (D-25/PARITY L13): the selector layer's double circles around
        // the whole selection, and the rubber band while a drag holds one.
        drawSelectionCircles(state, real);
        drawSelectionBand(state, real);
    }
    // M3 (D-26/D-27, PARITY S6/S7): the load-time answers as live layers -
    // scenario objects ringed blue, and one line per garrison, tow or
    // coupling from the passenger to its host.
    drawScenarioTint(state, real);
    drawLinkLines(state, real);
    drawDropTarget(state, real);
    // The Fence tool's ghost is its own and always on while it is active.
    if (state.view.tool == .fence) drawFenceGhost(state, real);
    // So are the Entrenchment tool's preview and outlines (04-08).
    if (state.view.tool == .entrenchment) drawTrenchMarkers(state, real);
    // D-21: the script areas, drawn where the map holds them; the active Script
    // Areas tool adds its selection's handles and the drag's ghost.
    if (marker_logic.visible(state.marker_set, .script_areas, active)) drawScriptAreaMarkers(state, real);
    // D-17: the red lines from each start command's units to its target. On
    // whenever the kind is on in View -> Markers, the Start Target tool is in hand,
    // or the Start Commands window has a command selected (the selected one is
    // drawn thicker).
    if (marker_logic.visible(state.marker_set, .start_commands, active) or (state.startcmds_open and state.startcmd_selected != null))
        drawStartCommandLines(state, real);
    // D-18: a line from each reserve position's gun through its truck to its place; the
    // Reserve Positions tool adds the choice in hand, dashed.
    if (marker_logic.visible(state.marker_set, .reserve_positions, active)) drawReserveLines(state, real);
    // D-19: the AI general's parcels as circles with a direction arrow and their reinforce
    // points as arrows, the tool's side bright and the others dimmed.
    if (marker_logic.visible(state.marker_set, .parcels, active)) drawParcelMarkers(state, real);
    // D-16: "Select objects" outlines the objects of a group's script IDs.
    if (state.group_marked != null and marker_logic.visible(state.marker_set, .groups, active)) drawGroupMarks(state, real);
}

fn areaColor() ig.ImU32 {
    return color(0.25, 0.85, 1.0);
}

fn areaSelectedColor() ig.ImU32 {
    return color(1.0, 0.95, 0.2);
}

/// The segments a circle is drawn with.
const area_circle_segments = 40;

/// One area's outline on the ground, from the record's AI units to world units
/// to the screen: a rectangle's four corners, a circle's ring of points. A point
/// that does not convert breaks the line there.
fn drawAreaOutline(draw_list: *ig.ImDrawList, real: anytype, area: core.records.ScriptArea, outline_color: ig.ImU32, thickness: f32) void {
    switch (area.shape) {
        .rectangle => {
            const corners = [4][2]f32{
                .{ area.cx - area.hx, area.cy - area.hy },
                .{ area.cx + area.hx, area.cy - area.hy },
                .{ area.cx + area.hx, area.cy + area.hy },
                .{ area.cx - area.hx, area.cy + area.hy },
            };
            var points: [4]ig.ImVec2 = undefined;
            for (corners, &points) |corner, *point| {
                const world = marker_logic.aiToWorld(.{ .x = corner[0], .y = corner[1] });
                point.* = screenOf(real, world.x, world.y) orelse return;
            }
            ig.ImDrawList_AddPolyline(draw_list, &points, 4, outline_color, thickness, ig.ImDrawFlags_Closed);
        },
        .circle => {
            var previous: ?ig.ImVec2 = null;
            var first: ?ig.ImVec2 = null;
            var step: usize = 0;
            while (step < area_circle_segments) : (step += 1) {
                const angle = @as(f32, @floatFromInt(step)) * 2.0 * std.math.pi / @as(f32, area_circle_segments);
                const world = marker_logic.aiToWorld(.{ .x = area.cx + area.r * @cos(angle), .y = area.cy + area.r * @sin(angle) });
                const here = screenOf(real, world.x, world.y);
                if (first == null) first = here;
                if (previous != null and here != null) ig.ImDrawList_AddLineEx(draw_list, previous.?, here.?, outline_color, thickness);
                previous = here;
            }
            if (previous != null and first != null) ig.ImDrawList_AddLineEx(draw_list, previous.?, first.?, outline_color, thickness);
        },
    }
}

/// D-21: every script area outlined with its name beside the centre; the
/// selected one in the selection colour with its handles while the Script Areas
/// tool is active (a square at the centre moves it, a circle at the corner or
/// edge resizes it, where `tools_ai.centreHandle` and `edgeHandle` say); and, while
/// a drag is held, the shape it would make. Capped like every kind.
fn drawScriptAreaMarkers(state: *State, real: anytype) void {
    state.refreshAreas();
    const draw_list = ig.igGetBackgroundDrawList();
    const tool = &state.view.areas_tool;
    const active = state.view.tool == .script_areas;
    const limit = marker_logic.cap(.script_areas);
    for (state.areas.items, 0..) |area, index| {
        if (index >= limit) break;
        const selected = active and tool.selected != null and tool.selected.? == index;
        drawAreaOutline(draw_list, real, area, if (selected) areaSelectedColor() else areaColor(), if (selected) 3 else 2);
        const world = marker_logic.aiToWorld(.{ .x = area.cx, .y = area.cy });
        if (screenOf(real, world.x, world.y)) |at| {
            var label: [core.records.area_name_capacity]u8 = undefined;
            const name = area.nameSlice();
            @memcpy(label[0..name.len], name);
            ig.ImDrawList_AddTextEx(draw_list, .{ .x = at.x + 8, .y = at.y - 8 }, labelColor(), &label, &label[name.len]);
        }
        if (selected) {
            const centre = core.tools_ai.centreHandle(area);
            const edge = core.tools_ai.edgeHandle(area);
            const centre_world = marker_logic.aiToWorld(.{ .x = centre[0], .y = centre[1] });
            const edge_world = marker_logic.aiToWorld(.{ .x = edge[0], .y = edge[1] });
            if (screenOf(real, centre_world.x, centre_world.y)) |at| drawSquare(draw_list, at, 6, areaSelectedColor(), true);
            if (screenOf(real, edge_world.x, edge_world.y)) |at| ig.ImDrawList_AddCircleEx(draw_list, at, 6, areaSelectedColor(), 16, 2.5);
        }
    }
    // The ghost of a drag in hand, in world units straight from the press and the pointer.
    if (active and tool.dragging) {
        const begin = tool.start orelse return;
        const end = tool.current orelse return;
        switch (tool.shape) {
            .rectangle => {
                const corners = [4][2]f32{ .{ begin[0], begin[1] }, .{ end[0], begin[1] }, .{ end[0], end[1] }, .{ begin[0], end[1] } };
                var points: [4]ig.ImVec2 = undefined;
                for (corners, &points) |corner, *point| point.* = screenOf(real, corner[0], corner[1]) orelse return;
                ig.ImDrawList_AddPolyline(draw_list, &points, 4, areaSelectedColor(), 2, ig.ImDrawFlags_Closed);
            },
            .circle => {
                const radius = std.math.hypot(end[0] - begin[0], end[1] - begin[1]);
                var previous: ?ig.ImVec2 = null;
                var first: ?ig.ImVec2 = null;
                var step: usize = 0;
                while (step < area_circle_segments) : (step += 1) {
                    const angle = @as(f32, @floatFromInt(step)) * 2.0 * std.math.pi / @as(f32, area_circle_segments);
                    const here = screenOf(real, begin[0] + radius * @cos(angle), begin[1] + radius * @sin(angle));
                    if (first == null) first = here;
                    if (previous != null and here != null) ig.ImDrawList_AddLineEx(draw_list, previous.?, here.?, areaSelectedColor(), 2);
                    previous = here;
                }
                if (previous != null and first != null) ig.ImDrawList_AddLineEx(draw_list, previous.?, first.?, areaSelectedColor(), 2);
            },
        }
    }
}

fn startLineColor() ig.ImU32 {
    return color(1.0, 0.15, 0.1);
}

/// Where a start command's target is on the ground, in world units: its unit's
/// position when it names one the document has, else its point (0,0 meaning none).
/// Null for a command with no target.
fn startTargetWorld(state: *State, command: core.records.StartCommand) ?marker_logic.Vec2 {
    if (command.link_id != 0) {
        const object = state.editor.document.find(command.link_id) orelse return null;
        return marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
    }
    if (command.x == 0 and command.y == 0) return null;
    return marker_logic.aiToWorld(.{ .x = command.x, .y = command.y });
}

/// D-17: a red line from every unit of every start command to its target, with a
/// small square at the target. A unit the document does not have, a command with no
/// target and a point that does not convert are skipped alone. Capped like every
/// kind (marker_logic.cap counts the lines).
fn drawStartCommandLines(state: *State, real: anytype) void {
    state.refreshStartCommands();
    const draw_list = ig.igGetBackgroundDrawList();
    const limit = marker_logic.cap(.start_commands);
    var drawn: usize = 0;
    for (state.startcmds, 0..) |command, index| {
        const target_world = startTargetWorld(state, command) orelse continue;
        const target = screenOf(real, target_world.x, target_world.y) orelse continue;
        const selected = state.startcmd_selected != null and state.startcmd_selected.? == index;
        const thickness: f32 = if (selected) 3 else 1.5;
        for (command.units) |unit| {
            if (drawn >= limit) return;
            const object = state.editor.document.find(unit) orelse continue;
            const world = marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
            const from = screenOf(real, world.x, world.y) orelse continue;
            ig.ImDrawList_AddLineEx(draw_list, from, target, startLineColor(), thickness);
            drawn += 1;
        }
        drawSquare(draw_list, target, if (selected) 5 else 3, startLineColor(), true);
    }
}

fn reserveColor() ig.ImU32 {
    return color(1.0, 0.75, 0.1);
}

/// An object's position as a screen point, from the document; null when it is not
/// on the map or does not convert.
fn objectScreen(state: *State, real: anytype, link_id: i32) ?ig.ImVec2 {
    if (link_id == 0) return null;
    const object = state.editor.document.find(link_id) orelse return null;
    const world = marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
    return screenOf(real, world.x, world.y);
}

/// A dashed line: every other stretch of `dash` pixels drawn.
fn drawDashedLine(draw_list: *ig.ImDrawList, from: ig.ImVec2, to: ig.ImVec2, line_color: ig.ImU32, dash: f32, thickness: f32) void {
    const dx = to.x - from.x;
    const dy = to.y - from.y;
    const length = @sqrt(dx * dx + dy * dy);
    if (length <= 0) return;
    var at: f32 = 0;
    while (at < length) : (at += 2 * dash) {
        const end = @min(at + dash, length);
        ig.ImDrawList_AddLineEx(draw_list, .{ .x = from.x + dx * at / length, .y = from.y + dy * at / length }, .{ .x = from.x + dx * end / length, .y = from.y + dy * end / length }, line_color, thickness);
    }
}

/// The chain of one reserve position: a square at the gun, a circle at the truck, a
/// diamond at the place, lines between them in that order. `dashed` for the choice
/// in hand. Parts that are missing or do not convert are skipped alone.
fn drawReserveChain(draw_list: *ig.ImDrawList, gun: ?ig.ImVec2, truck: ?ig.ImVec2, place: ?ig.ImVec2, chain_color: ig.ImU32, thickness: f32, dashed: bool) void {
    var previous: ?ig.ImVec2 = null;
    const parts = [3]?ig.ImVec2{ gun, truck, place };
    for (parts, 0..) |part, index| {
        const at = part orelse continue;
        if (previous) |from| {
            if (dashed) drawDashedLine(draw_list, from, at, chain_color, 7, thickness) else ig.ImDrawList_AddLineEx(draw_list, from, at, chain_color, thickness);
        }
        switch (index) {
            0 => drawSquare(draw_list, at, 5, chain_color, true),
            1 => ig.ImDrawList_AddCircleEx(draw_list, at, 5, chain_color, 16, 2.5),
            else => ig.ImDrawList_AddQuad(draw_list, .{ .x = at.x, .y = at.y - 6 }, .{ .x = at.x + 6, .y = at.y }, .{ .x = at.x, .y = at.y + 6 }, .{ .x = at.x - 6, .y = at.y }, chain_color),
        }
        previous = at;
    }
}

/// D-18: every reserve position of the map as gun -> truck -> place, the selected one
/// thicker while the Reserve Positions tool is in hand, and - with the tool in hand - the
/// choice it holds, dashed. Capped like every kind (marker_logic.cap).
fn drawReserveLines(state: *State, real: anytype) void {
    state.refreshReserve();
    const draw_list = ig.igGetBackgroundDrawList();
    const tool = &state.view.reserve_tool;
    const in_hand = state.view.tool == .reserve_positions;
    const limit = marker_logic.cap(.reserve_positions);
    for (state.reserve_list, 0..) |position, index| {
        if (index >= limit) break;
        const selected = in_hand and tool.selected != null and tool.selected.? == index;
        const world = marker_logic.aiToWorld(.{ .x = position.x, .y = position.y });
        drawReserveChain(draw_list, objectScreen(state, real, position.artillery), objectScreen(state, real, position.truck), screenOf(real, world.x, world.y), reserveColor(), if (selected) 3.5 else 2, false);
    }
    if (!in_hand) return;
    var place: ?ig.ImVec2 = null;
    if (tool.has_place) {
        const world = marker_logic.aiToWorld(.{ .x = tool.x, .y = tool.y });
        place = screenOf(real, world.x, world.y);
    }
    drawReserveChain(draw_list, if (tool.gun) |gun| objectScreen(state, real, gun) else null, if (tool.truck) |truck| objectScreen(state, real, truck) else null, place, color(1.0, 1.0, 0.2), 2, true);
}

/// The MFC editor's PARCEL_COLORS and POSITION_COLORS (StateAIGeneral.cpp), by the
/// parcel's type: a defence parcel green, a reinforce parcel yellow, anything else red.
const ParcelPalette = struct { parcel: [3]f32, point: [3]f32 };

fn parcelPalette(kind: core.records.ParcelKind) ParcelPalette {
    return switch (kind) {
        .defence => .{ .parcel = .{ 0.5, 1.0, 0.5 }, .point = .{ 0.125, 1.0, 0.125 } },
        .reinforce => .{ .parcel = .{ 1.0, 1.0, 0.5 }, .point = .{ 1.0, 1.0, 0.125 } },
        else => .{ .parcel = .{ 1.0, 0.5, 0.5 }, .point = .{ 1.0, 0.125, 0.125 } },
    };
}

fn withAlpha(rgb: [3]f32, alpha: f32) ig.ImU32 {
    return ig.igColorConvertFloat4ToU32(.{ .x = rgb[0], .y = rgb[1], .z = rgb[2], .w = alpha });
}

/// A map point (AI units) on the screen; null when it does not convert.
fn aiScreen(real: anytype, at: [2]f32) ?ig.ImVec2 {
    const world = marker_logic.aiToWorld(.{ .x = at[0], .y = at[1] });
    return screenOf(real, world.x, world.y);
}

/// The segments a parcel's ring is drawn with, and a handle's.
const parcel_ring_segments = 64;
const handle_ring_segments = 16;

/// A ring of `radius` (AI units) about a map point, each perimeter point converted on
/// its own, so on the iso ground it is the ellipse the terrain shows; a point that does
/// not convert breaks the ring there.
fn drawAiRing(draw_list: *ig.ImDrawList, real: anytype, centre: [2]f32, radius: f32, segments: usize, ring_color: ig.ImU32, thickness: f32) void {
    var previous: ?ig.ImVec2 = null;
    var first: ?ig.ImVec2 = null;
    var step: usize = 0;
    while (step < segments) : (step += 1) {
        const angle = @as(f32, @floatFromInt(step)) * 2.0 * std.math.pi / @as(f32, @floatFromInt(segments));
        const here = aiScreen(real, .{ centre[0] + radius * @cos(angle), centre[1] + radius * @sin(angle) });
        if (first == null) first = here;
        if (previous != null and here != null) ig.ImDrawList_AddLineEx(draw_list, previous.?, here.?, ring_color, thickness);
        previous = here;
    }
    if (previous != null and first != null) ig.ImDrawList_AddLineEx(draw_list, previous.?, first.?, ring_color, thickness);
}

fn drawAiLine(draw_list: *ig.ImDrawList, real: anytype, from: [2]f32, to: [2]f32, line_color: ig.ImU32, thickness: f32) void {
    const a = aiScreen(real, from) orelse return;
    const b = aiScreen(real, to) orelse return;
    ig.ImDrawList_AddLineEx(draw_list, a, b, line_color, thickness);
}

/// A filled disc at a map point: the active handle, as the MFC editor fills it with
/// concentric circles.
fn drawAiDisc(draw_list: *ig.ImDrawList, real: anytype, at: [2]f32, radius_pixels: f32, disc_color: ig.ImU32) void {
    const centre = aiScreen(real, at) orelse return;
    ig.ImDrawList_AddCircleFilled(draw_list, centre, radius_pixels, disc_color, 16);
}

/// One side's parcels (D-19, CAIGState::Draw): for each, its ring of `radius`, a line from
/// the centre out along its direction (plus a quarter turn) and past the ring, the ring of
/// the centre handle and of the arrow handle; for each point, its place's ring, a ring of
/// the arrow length round it, its own arrow and the ring of its arrow handle. The selected
/// parcel and point (while the AI General tool is in hand) are drawn thicker with their
/// handles filled. `alpha` dims a side the tool is not editing.
fn drawAiSide(draw_list: *ig.ImDrawList, real: anytype, side: core.records.AiSide, in_hand: bool, tool: *const core.tools_ai.AIGeneral, active_side: bool, alpha: f32, budget: *usize) void {
    const ai = core.tools_ai;
    for (side.parcels, 0..) |parcel, parcel_index| {
        if (budget.* == 0) return;
        budget.* -= 1;
        const palette = parcelPalette(parcel.kind);
        const parcel_color = withAlpha(palette.parcel, alpha);
        const point_color = withAlpha(palette.point, alpha);
        const selected = in_hand and active_side and tool.selected_parcel != null and tool.selected_parcel.? == parcel_index;
        const centre = ai.parcelCentreHandle(parcel);
        const arrow = ai.parcelArrowHandle(parcel);
        const angle = ai.directionAngle(parcel.defence_dir) + std.math.pi / 2.0;
        const tail: [2]f32 = .{ parcel.cx + (parcel.radius + ai.arrow_tail_length) * @cos(angle), parcel.cy + (parcel.radius + ai.arrow_tail_length) * @sin(angle) };
        const thickness: f32 = if (selected and tool.selected_point == null) 3.5 else 2;
        drawAiRing(draw_list, real, centre, parcel.radius, parcel_ring_segments, parcel_color, thickness);
        drawAiRing(draw_list, real, centre, ai.parcel_centre_grab, handle_ring_segments, parcel_color, thickness);
        drawAiRing(draw_list, real, arrow, ai.parcel_arrow_grab, handle_ring_segments, parcel_color, thickness);
        drawAiLine(draw_list, real, centre, arrow, parcel_color, thickness);
        drawAiLine(draw_list, real, arrow, tail, parcel_color, thickness);
        if (selected and tool.selected_point == null) {
            drawAiDisc(draw_list, real, centre, 7, parcel_color);
            drawAiDisc(draw_list, real, arrow, 5, parcel_color);
        }
        for (parcel.points, 0..) |point, point_index| {
            const point_selected = selected and tool.selected_point != null and tool.selected_point.? == point_index;
            const place = ai.pointCentreHandle(parcel, point);
            const point_arrow = ai.pointArrowHandle(parcel, point);
            const point_angle = ai.directionAngle(point.dir) + std.math.pi / 2.0;
            const point_tail = ai.pointToMap(.{ point.x + (ai.point_arrow_length + ai.arrow_tail_length) * @cos(point_angle), point.y + (ai.point_arrow_length + ai.arrow_tail_length) * @sin(point_angle) }, .{ parcel.cx, parcel.cy }, parcel.defence_dir);
            const point_thickness: f32 = if (point_selected) 3.5 else 2;
            drawAiLine(draw_list, real, place, point_arrow, point_color, point_thickness);
            drawAiLine(draw_list, real, point_arrow, point_tail, point_color, point_thickness);
            drawAiRing(draw_list, real, place, ai.point_arrow_length, handle_ring_segments, point_color, 1);
            drawAiRing(draw_list, real, place, ai.point_centre_grab, handle_ring_segments, point_color, point_thickness);
            drawAiRing(draw_list, real, point_arrow, ai.point_arrow_grab, handle_ring_segments, point_color, point_thickness);
            if (point_selected) {
                drawAiDisc(draw_list, real, place, 6, point_color);
                drawAiDisc(draw_list, real, point_arrow, 4, point_color);
            }
        }
    }
}

/// D-19: the parcels of every side the panel has read (marker_logic.cap bounds them), the
/// tool's side bright and the rest dimmed.
fn drawParcelMarkers(state: *State, real: anytype) void {
    state.refreshAi();
    const draw_list = ig.igGetBackgroundDrawList();
    const tool = &state.view.ai_tool;
    const in_hand = state.view.tool == .ai_general;
    var budget: usize = marker_logic.cap(.parcels);
    for (state.ai_sides.items, 0..) |side, index| {
        if (index == tool.side) continue;
        drawAiSide(draw_list, real, side, in_hand, tool, false, 0.4, &budget);
    }
    if (tool.side < state.ai_sides.items.len) drawAiSide(draw_list, real, state.ai_sides.items[tool.side], in_hand, tool, true, 1.0, &budget);
}

fn groupMarkColor() ig.ImU32 {
    return color(1.0, 0.2, 0.9);
}

/// D-16 "Select objects": a ring round every object of the map's objects list
/// that carries a script ID of the marked group, with the group's ID beside
/// it. Capped like every kind (marker_logic.cap).
fn drawGroupMarks(state: *State, real: anytype) void {
    const marked = state.group_marked orelse return;
    const row = state.findGroup(marked) orelse return;
    const draw_list = ig.igGetBackgroundDrawList();
    const limit = marker_logic.cap(.groups);
    var drawn: usize = 0;
    var label: [16]u8 = undefined;
    const label_text = std.fmt.bufPrint(&label, "G{d}", .{marked}) catch "G";
    for (state.editor.document.objects.items) |object| {
        if (drawn >= limit) break;
        if (object.scenario or !row.has(object.script_id)) continue;
        const world = marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
        const at = real.worldToScreen(world.x, world.y) orelse continue;
        const centre: ig.ImVec2 = .{ .x = at[0], .y = at[1] };
        ig.ImDrawList_AddCircleEx(draw_list, centre, 18, groupMarkColor(), 24, 2.5);
        ig.ImDrawList_AddTextEx(draw_list, .{ .x = centre.x + 20, .y = centre.y - 8 }, groupMarkColor(), label_text.ptr, label_text.ptr + label_text.len);
        drawn += 1;
    }
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

/// The selector layer's double circles (M3, D-25/PARITY L13): two concentric
/// rings around every selected object, anchored at its map position through
/// the world conversion - a squad shows on its record's own position, exactly
/// where its soldiers stand. Capped like every kind, so a malformed map
/// cannot draw thousands of shapes.
fn drawSelectionCircles(state: *State, real: anytype) void {
    const editor = state.editor;
    if (editor.selectionCount() == 0) return;
    const draw_list = ig.igGetBackgroundDrawList();
    const selected_color = color(0.15, 0.9, 0.35);
    const limit = marker_logic.cap(.selection_outline);
    var walked: usize = 0;
    for (editor.document.objects.items) |object| {
        if (!editor.isSelected(object.link_id)) continue;
        if (walked >= limit) return;
        walked += 1;
        const world = marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
        if (screenOf(real, world.x, world.y)) |at| {
            const radii = panels_logic.selectionCircles(.{ 16, 16 });
            ig.ImDrawList_AddCircleEx(draw_list, at, radii[0], selected_color, 24, 1.5);
            ig.ImDrawList_AddCircleEx(draw_list, at, radii[1], selected_color, 24, 1.0);
        }
    }
}

/// The rubber band (M3, D-25): a dashed rectangle from where the press
/// started to where the pointer is now, on screen for the plain band - the
/// view's own hover holds the moving end.
fn drawSelectionBand(state: *State, real: anytype) void {
    _ = real;
    const band = state.view.selector.band orelse return;
    const hover = state.view.hover orelse return;
    const draw_list = ig.igGetBackgroundDrawList();
    const left = @min(band.screen_x, hover.screen_x);
    const right = @max(band.screen_x, hover.screen_x);
    const top = @min(band.screen_y, hover.screen_y);
    const bottom = @max(band.screen_y, hover.screen_y);
    const band_color = if (band.ctrl) color(0.3, 0.7, 1.0) else color(0.95, 0.95, 0.4);
    var corners: [4]ig.ImVec2 = .{
        .{ .x = left, .y = top },
        .{ .x = right, .y = top },
        .{ .x = right, .y = bottom },
        .{ .x = left, .y = bottom },
    };
    drawDashed(draw_list, &corners, band_color, 10, 1.5);
}

/// S6 (D-26): scenario objects ringed blue - the MFC load pass draws its
/// scenario units with a blue specular (TemplateEditorFrame1.cpp:1357-2113),
/// and the portable editor rings each record of the scenarioObjects list.
/// Capped like every kind; a selection ring on top still reads.
fn drawScenarioTint(state: *State, real: anytype) void {
    const editor = state.editor;
    const draw_list = ig.igGetBackgroundDrawList();
    const scenario_color = color(0.35, 0.55, 1.0);
    const limit = marker_logic.cap(.selection_outline);
    var walked: usize = 0;
    for (editor.document.objects.items) |object| {
        if (!object.scenario) continue;
        if (walked >= limit) return;
        walked += 1;
        const world = marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
        if (screenOf(real, world.x, world.y)) |at| {
            ig.ImDrawList_AddCircleEx(draw_list, at, 9, scenario_color, 20, 1.2);
        }
    }
}

/// S7 (D-27): one line per link, from the passenger's record position to its
/// host's - the map's own nLinkWith answers, the same data the properties'
/// units list unlinks. Capped like every kind.
fn drawLinkLines(state: *State, real: anytype) void {
    const editor = state.editor;
    const draw_list = ig.igGetBackgroundDrawList();
    const link_color = color(1.0, 0.65, 0.2);
    const limit = marker_logic.cap(.selection_outline);
    var walked: usize = 0;
    for (editor.document.objects.items) |object| {
        if (object.link_with <= 0) continue;
        if (walked >= limit) return;
        const host = editor.document.find(object.link_with) orelse continue;
        walked += 1;
        const from = marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
        const to = marker_logic.aiToWorld(.{ .x = host.x, .y = host.y });
        const a = screenOf(real, from.x, from.y) orelse continue;
        const b = screenOf(real, to.x, to.y) orelse continue;
        ig.ImDrawList_AddLineEx(draw_list, a, b, link_color, 1.5);
    }
}

/// O13 (D-27): the drop's cursor feedback - the MFC shows IDC_UPARROW while
/// a dragged group hovers a valid host (ObjectPlacerState.cpp:129-134); the
/// portable editor rings the host, dashed for a tow or a coupling.
fn drawDropTarget(state: *State, real: anytype) void {
    const target = state.view.selector.drop_target orelse return;
    const editor = state.editor;
    const object = editor.document.find(target) orelse return;
    const tow = state.view.selector.drop_kind != 0;
    const world = marker_logic.aiToWorld(.{ .x = object.x, .y = object.y });
    const at = screenOf(real, world.x, world.y) orelse return;
    const draw_list = ig.igGetBackgroundDrawList();
    const drop_color = color(0.2, 1.0, 0.4);
    if (tow) {
        var corners: [4]ig.ImVec2 = .{
            .{ .x = at.x - 12, .y = at.y - 12 },
            .{ .x = at.x + 12, .y = at.y - 12 },
            .{ .x = at.x + 12, .y = at.y + 12 },
            .{ .x = at.x - 12, .y = at.y + 12 },
        };
        drawDashed(draw_list, &corners, drop_color, 12, 2.0);
    } else {
        ig.ImDrawList_AddCircleEx(draw_list, at, 13, drop_color, 24, 2.0);
    }
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
