//! The M2 panels (04-03 on): drawn from panels.zig's `draw` like the M1 ones,
//! each a thin ImGui layer over the named commands in commands.zig, so a
//! button and a BK_EDITOR_AUTO `do=` run the same code. This plan adds the
//! Camera anchors panel; later plans add theirs here.
const std = @import("std");
const imgui = @import("editor_imgui");
const core = @import("editor_core");
const panels = @import("panels.zig");
const commands = @import("commands.zig");

const ig = imgui.c;
const State = panels.State;
const records = core.records;

/// What the game does with these (iMissionInternal.cpp's anchor order, D-22).
pub const camera_anchors_note = "The game starts at the player's anchor, then the neutral one, then the centre of the units.";

/// The rows: "Neutral", then "Player 0" up to the larger of the map's players
/// and the anchors the file holds, at most the record's 32.
fn playerRows(state: *const State) usize {
    const players: usize = @intCast(@max(state.editor.document.info.player_count, 0));
    return @min(@max(players, state.anchors.player_count), records.max_camera_players);
}

/// D-22: every anchor with its position, "Go to" and "Clear". The cache is
/// State's own (`refreshAnchors`, called from `draw` before anything else).
pub fn drawCameraAnchors(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Camera anchors", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    drawRow(state, "Neutral", commands.neutral_slot, state.anchors.neutral);
    var player: usize = 0;
    const rows = playerRows(state);
    while (player < rows) : (player += 1) {
        var label: [24:0]u8 = undefined;
        const label_text = std.fmt.bufPrintZ(&label, "Player {d}", .{player}) catch continue;
        drawRow(state, label_text, @intCast(player), state.anchors.slot(player));
    }
    ig.igSeparator();
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(camera_anchors_note);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
}

fn drawRow(state: *State, label: [:0]const u8, slot: i32, anchor: records.Vec3) void {
    ig.igPushIDInt(slot);
    defer ig.igPopID();
    const is_set = !anchor.isUnset();
    var line: [64:0]u8 = undefined;
    const line_text = if (is_set)
        std.fmt.bufPrintZ(&line, "{s}: {d:.0}, {d:.0}", .{ label, anchor.x, anchor.y }) catch label
    else
        std.fmt.bufPrintZ(&line, "{s}: unset", .{label}) catch label;
    panels.text(line_text);
    ig.igSameLine();
    ig.igBeginDisabled(!is_set);
    if (ig.igSmallButton("Go to")) _ = commands.gotoAnchor(state, slot);
    ig.igSameLine();
    if (ig.igSmallButton("Clear")) _ = commands.clearAnchor(state, slot);
    ig.igEndDisabled();
}

/// What the Roads & Rivers panel says when no line is selected.
pub const roads_rivers_help = "Click to draw; right-click takes the last point back; double-click or Enter finishes; Esc cancels. On a selected line drag a square to move a point, a circle to set the width; right-drag a point up or down for opacity; Insert adds a point, Delete removes one or the line.";

/// D-08: the MFC Roads and Rivers tabs (TabVOVSODialog) as one panel - the
/// Road / River switch, the season's types, width 1..16, opacity 0..100 %, the
/// width mode, and the selected line with a Delete button. Every control
/// runs a named command (commands.zig), so BK_EDITOR_AUTO reaches it too.
pub fn drawRoadsRivers(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Roads & Rivers", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    const tool = &state.view.roads_rivers;
    if (ig.igRadioButton("Road", tool.kind == .road)) _ = commands.setVsoKind(state, .road);
    ig.igSameLine();
    if (ig.igRadioButton("River", tool.kind == .river)) _ = commands.setVsoKind(state, .river);

    state.refreshVsoTypes();
    ig.igSeparatorText("Type");
    if (state.vso_types.len == 0) {
        panels.text("this season has no types of this kind");
    } else if (ig.igBeginChild("vso-types", .{ .x = 0, .y = 140 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (state.vso_types, 0..) |*item, index| {
            var name: [core.bridge.vso_name_capacity + 1:0]u8 = undefined;
            const label = std.fmt.bufPrintZ(&name, "{s}", .{item.nameSlice()}) catch continue;
            const selected = std.mem.eql(u8, item.nameSlice(), tool.desc());
            if (ig.igSelectableEx(label.ptr, selected, 0, .{ .x = 0, .y = 0 })) _ = commands.chooseVsoType(state, index);
        }
    }
    if (state.vso_types.len != 0) ig.igEndChild();

    var width: c_int = @intFromFloat(@round(tool.width_tiles));
    if (ig.igSliderInt("width", &width, 1, 16)) tool.width_tiles = @floatFromInt(width);
    var percent: c_int = @intFromFloat(@round(tool.opacity * 100));
    if (ig.igSliderInt("opacity %", &percent, 0, 100)) tool.opacity = @as(f32, @floatFromInt(percent)) / 100.0;
    ig.igSeparatorText("Width mode");
    if (ig.igRadioButton("Single point", tool.width_mode == .single)) tool.width_mode = .single;
    if (ig.igRadioButton("From this point on", tool.width_mode == .multi)) tool.width_mode = .multi;
    if (ig.igRadioButton("All", tool.width_mode == .all)) tool.width_mode = .all;
    ig.igBeginDisabled(tool.width_mode != .multi);
    _ = ig.igCheckbox("earlier points instead (MFC: Ctrl)", &tool.multi_backwards);
    ig.igEndDisabled();

    ig.igSeparatorText("Selected");
    if (tool.selectedView(state.editor)) |view| {
        const selected = tool.selected.?;
        var line: [256:0]u8 = undefined;
        const text = std.fmt.bufPrintZ(&line, "{s} {d}: ID {d}, {d} points", .{ selected.kind.label(), selected.index, view.saved_id, view.control_points.len }) catch "selected";
        panels.text(text);
        ig.igPushTextWrapPos(0);
        panels.text(view.descSlice());
        ig.igPopTextWrapPos();
        if (ig.igButton("Delete")) _ = commands.deleteSelectedVso(state);
    } else {
        ig.igPushTextWrapPos(0);
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
        panels.text(roads_rivers_help);
        ig.igPopStyleColor();
        ig.igPopTextWrapPos();
    }
}

/// What the Bridges panel says when no bridge is selected.
pub const bridges_help = "Drag along the type's direction to draw a bridge. Click a span to select its bridge; Q or E rotates it, Enter toggles built during play (WoodenBig_Heavy only), Delete removes it.";

/// D-10..D-12: the MFC Bridges tab (BridgeSetupDialog) - every bridge type
/// with its picture, its direction and whether it can be built during play,
/// and for the selected bridge its type, span count, Rotate, "Built during
/// play" and Delete. Every control runs a named command (commands.zig).
pub fn drawBridges(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Bridges", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshBridgeTypes();
    state.refreshBridges();
    const tool = &state.view.bridge_tool;
    ig.igSeparatorText("Type");
    if (state.bridge_types.len == 0) {
        panels.text("the object database has no bridge types");
    } else if (ig.igBeginChild("bridge-types", .{ .x = 0, .y = @max(size.y - 260, 120) }, ig.ImGuiChildFlags_Borders, 0)) {
        for (state.bridge_types, 0..) |*item, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            state.pictures.request(item.nameSlice());
            panels.drawPaletteRowPicture(state, item.nameSlice());
            ig.igSameLine();
            var line: [core.bridge.name_capacity + 48:0]u8 = undefined;
            const label = std.fmt.bufPrintZ(&line, "{s}\n{s}{s}", .{
                item.nameSlice(),
                if (item.direction == .horizontal) "horizontal" else "vertical",
                if (item.build_during_play_allowed) ", can be built during play" else "",
            }) catch continue;
            const selected = std.mem.eql(u8, item.nameSlice(), tool.desc());
            if (ig.igSelectableEx(label.ptr, selected, 0, .{ .x = 0, .y = 48 })) _ = commands.chooseBridgeType(state, item.nameSlice());
        }
    }
    if (state.bridge_types.len != 0) ig.igEndChild();
    if (state.real.gpuDevice()) |device| state.pictures.pump(state.real, device, panels.picture_pump_budget);

    ig.igSeparatorText("Selected");
    const selected = tool.selected;
    if (selected != null and selected.? < state.bridge_infos.len) {
        const info = state.bridge_infos[selected.?];
        var line: [core.bridge.name_capacity + 48:0]u8 = undefined;
        const text = std.fmt.bufPrintZ(&line, "bridge {d}: {s}, {d} spans", .{ selected.?, info.descSlice(), info.span_count }) catch "selected";
        panels.text(text);
        if (ig.igButton("Rotate")) _ = commands.rotateSelectedBridge(state);
        var built = info.built_during_play;
        const allowed = std.mem.indexOf(u8, info.descSlice(), "WoodenBig_Heavy_") != null;
        ig.igBeginDisabled(!allowed);
        if (ig.igCheckbox("Built during play", &built)) _ = commands.toggleSelectedBridgeBuild(state);
        ig.igEndDisabled();
        if (ig.igButton("Delete")) _ = commands.deleteSelectedBridge(state);
    } else {
        ig.igPushTextWrapPos(0);
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
        panels.text(bridges_help);
        ig.igPopStyleColor();
        ig.igPopTextWrapPos();
    }
}
