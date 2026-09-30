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

/// What the Fences panel says.
pub const fences_help = "Drag along one axis to place a run of fences, one every second AI tile; click to place one. Ctrl flips a single fence. Placed fences are ordinary objects: select them with the Select tool to move or delete them.";

/// D-14: the MFC Fences tab (FenceSetupWindow) - every fence type with its
/// picture. Choosing one is a named command (commands.zig).
pub fn drawFences(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Fences", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshFenceTypes();
    const tool = &state.view.fence_tool;
    ig.igSeparatorText("Type");
    if (state.fence_types.len == 0) {
        panels.text("the object database has no fence types");
    } else if (ig.igBeginChild("fence-types", .{ .x = 0, .y = @max(size.y - 190, 120) }, ig.ImGuiChildFlags_Borders, 0)) {
        for (state.fence_types, 0..) |*item, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            state.pictures.request(item.nameSlice());
            panels.drawPaletteRowPicture(state, item.nameSlice());
            ig.igSameLine();
            var line: [core.bridge.name_capacity + 8:0]u8 = undefined;
            const label = std.fmt.bufPrintZ(&line, "{s}", .{item.nameSlice()}) catch continue;
            const selected = std.mem.eql(u8, item.nameSlice(), tool.desc());
            if (ig.igSelectableEx(label.ptr, selected, 0, .{ .x = 0, .y = 48 })) _ = commands.chooseFenceType(state, item.nameSlice());
        }
    }
    if (state.fence_types.len != 0) ig.igEndChild();
    if (state.real.gpuDevice()) |device| state.pictures.pump(state.real, device, panels.picture_pump_budget);
    ig.igSeparatorText("Placing");
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(fences_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
}

/// What the Entrenchments panel says: the MFC gestures (RoadDrawState.cpp).
pub const entrenchments_help = "Click adds a point, right-click (or Ctrl+click) clears, double-click finishes; Esc clears too. Click a trench to select it, point at one to highlight it; Delete removes the selected or highlighted trench whole.";

/// D-13: the MFC Entrenchment tool's controls - the player the pieces will
/// belong to (the Objects panel's player until chosen here), the gestures,
/// and the selected entrenchment's piece and section counts with a Delete
/// button. Every control runs a named command (commands.zig).
pub fn drawEntrenchments(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Entrenchments", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshTrenches();
    const tool = &state.view.trench_tool;
    if (!state.trench_player_chosen) tool.player = @max(state.view.placer.player, 0);
    ig.igSeparatorText("Player");
    var player: c_int = tool.player;
    const players: c_int = @max(state.editor.document.info.player_count, 1);
    if (ig.igSliderInt("player", &player, 0, players - 1)) {
        var buffer: [12]u8 = undefined;
        const arg = std.fmt.bufPrint(&buffer, "{d}", .{player}) catch "0";
        _ = commands.run(state, "trench_player", arg);
    }
    ig.igSeparatorText("Drawing");
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(entrenchments_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
    if (tool.drawing()) {
        var line: [64:0]u8 = undefined;
        const text = std.fmt.bufPrintZ(&line, "{d} points clicked", .{tool.len}) catch "drawing";
        panels.text(text);
    }

    ig.igSeparatorText("Selected");
    const selected = tool.selected;
    if (selected != null and selected.? < state.trench_infos.len) {
        const info = state.trench_infos[selected.?];
        var line: [96:0]u8 = undefined;
        const text = std.fmt.bufPrintZ(&line, "entrenchment {d}, player {d}:\n{d} pieces in {d} sections", .{ selected.?, info.player, info.piece_count, info.section_count }) catch "selected";
        panels.text(text);
        if (ig.igButton("Delete")) _ = commands.run(state, "trench_delete", "");
    } else {
        var line: [64:0]u8 = undefined;
        const text = std.fmt.bufPrintZ(&line, "none ({d} entrenchments on the map)", .{state.trench_infos.len}) catch "none";
        panels.text(text);
    }
}

/// What one frame of the Groups window asked for, run after every loop over the
/// group rows: a command re-reads the rows and frees the slices a loop holds.
const GroupAction = union(enum) {
    none,
    new,
    delete,
    add_id,
    remove_id: i32,
    select_objects,
    choose: i32,
    hide: struct { id: i32, on: bool },
};

/// What the Groups window says.
pub const groups_help = "Hide checked holds a group's objects back in the view and from picking, as the game holds them for the group; Select objects outlines the objects that carry the group's script IDs and selects the first.";

/// D-16: the MFC Group Manager (GroupManagerDialog) as a window opened from
/// Map -> Reinforcement groups...: the groups by ID with a hide box each; New
/// with an ID field (bumped to the first unused one at or above it, C9) and
/// Delete; the selected group's script IDs with Remove each, a field and Add
/// (a script ID already there is skipped); and Select objects. Every control
/// runs a command (commands.zig), so BK_EDITOR_AUTO's `do=` reaches it too.
pub fn drawGroups(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.groups_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Reinforcement groups", &state.groups_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshGroups();
    var action: GroupAction = .none;

    ig.igPushItemWidth(120);
    _ = ig.igInputIntEx("ID", &state.group_new_from, 1, 10, 0);
    ig.igPopItemWidth();
    ig.igSameLine();
    if (ig.igButton("New")) action = .new;
    ig.igSameLine();
    ig.igBeginDisabled(state.group_selected == null);
    if (ig.igButton("Delete")) action = .delete;
    ig.igEndDisabled();

    ig.igSeparatorText("Groups");
    const rows = state.groups.items;
    if (rows.len == 0) {
        panels.text("the map has no reinforcement groups");
    } else if (ig.igBeginChild("groups-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows) |row| {
            ig.igPushIDInt(row.id);
            defer ig.igPopID();
            var checked = state.groupIsChecked(row.id);
            if (ig.igCheckbox("hide", &checked)) action = .{ .hide = .{ .id = row.id, .on = checked } };
            ig.igSameLine();
            var label: [64:0]u8 = undefined;
            const label_text = std.fmt.bufPrintZ(&label, "Group {d}: {d} script IDs", .{ row.id, row.ids.len }) catch continue;
            const selected = state.group_selected != null and state.group_selected.? == row.id;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = row.id };
        }
    }
    if (rows.len != 0) ig.igEndChild();

    ig.igSeparatorText("Script IDs");
    const selected_row = if (state.group_selected) |id| state.findGroup(id) else null;
    if (selected_row) |row| {
        var title: [48:0]u8 = undefined;
        panels.text(std.fmt.bufPrintZ(&title, "group {d}", .{row.id}) catch "group");
        if (row.ids.len == 0) {
            panels.text("no script IDs yet");
        } else if (ig.igBeginChild("group-ids", .{ .x = 0, .y = 100 }, ig.ImGuiChildFlags_Borders, 0)) {
            for (row.ids) |script_id| {
                ig.igPushIDInt(script_id);
                defer ig.igPopID();
                var line: [24:0]u8 = undefined;
                panels.text(std.fmt.bufPrintZ(&line, "{d}", .{script_id}) catch "?");
                ig.igSameLine();
                if (ig.igSmallButton("Remove")) action = .{ .remove_id = script_id };
            }
        }
        if (row.ids.len != 0) ig.igEndChild();
        ig.igPushItemWidth(120);
        _ = ig.igInputIntEx("script ID", &state.group_script_field, 1, 10, 0);
        ig.igPopItemWidth();
        ig.igSameLine();
        if (ig.igButton("Add")) action = .add_id;
        if (ig.igButton("Select objects")) action = .select_objects;
    } else {
        panels.text("select a group");
    }
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(groups_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    switch (action) {
        .none => {},
        .new => _ = commands.newGroup(state, state.group_new_from),
        .delete => if (state.group_selected) |id| {
            _ = commands.deleteGroup(state, id);
        },
        .add_id => if (state.group_selected) |id| {
            _ = commands.addGroupId(state, id, state.group_script_field);
        },
        .remove_id => |script_id| if (state.group_selected) |id| {
            _ = commands.removeGroupId(state, id, script_id);
        },
        .select_objects => if (state.group_selected) |id| {
            _ = commands.selectGroupObjects(state, id);
        },
        .choose => |id| state.group_selected = id,
        .hide => |hide| _ = commands.hideGroup(state, hide.id, hide.on),
    }
}
