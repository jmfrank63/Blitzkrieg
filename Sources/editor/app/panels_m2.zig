//! The M2 panels (04-03 on): drawn from panels.zig's `draw` like the M1 ones,
//! each a thin ImGui layer over the named commands in commands.zig, so a
//! button and a BK_EDITOR_AUTO `do=` run the same code. This plan adds the
//! Camera anchors panel; later plans add theirs here.
const std = @import("std");
const imgui = @import("editor_imgui");
const sdl3 = @import("sdl3");
const core = @import("editor_core");
const panels = @import("panels.zig");
const commands = @import("commands.zig");
const marker_logic = @import("marker_logic.zig");

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

    // 04-13: in the width mode All the sliders re-width the selected line too
    // (the MFC editor's CW_ALL); one slider drag is one undo step, its gesture
    // begun when the slider is taken hold of.
    var width: c_int = @intFromFloat(@round(tool.width_tiles));
    const width_moved = ig.igSliderInt("width", &width, 1, 16);
    if (ig.igIsItemActivated()) state.vso_slider_gesture = state.editor.beginGesture();
    if (width_moved) _ = commands.setVsoWidthTiles(state, @floatFromInt(width), state.vso_slider_gesture);
    var percent: c_int = @intFromFloat(@round(tool.opacity * 100));
    const opacity_moved = ig.igSliderInt("opacity %", &percent, 0, 100);
    if (ig.igIsItemActivated()) state.vso_slider_gesture = state.editor.beginGesture();
    if (opacity_moved) _ = commands.setVsoOpacity(state, @as(f32, @floatFromInt(percent)) / 100.0, state.vso_slider_gesture);
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

/// D-20: the Script dialog (Map -> Script...), the MFC map options' script
/// field made a window. The current value shows verbatim (a shipped map may
/// name a folder), a warning line says when its file is not beside the map, a
/// list offers None and the .lua files beside the map, Choose other... copies a
/// picked file beside the map (asking before it replaces one) and Open script
/// folder shows the folder holding it in the system's file manager. Every control runs a named command or
/// a panels.zig function the commands call, so BK_EDITOR_AUTO reaches it too.
pub fn drawScriptDialog(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.script_open) {
        state.script_open_seen = false;
        return;
    }
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Script", &state.script_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    panels.refreshScriptNames(state);
    var value_buffer: [records.script_file_capacity]u8 = undefined;
    const value = commands.readScriptFile(state, &value_buffer);
    var line: [128:0]u8 = undefined;
    if (value) |text| {
        panels.text(std.fmt.bufPrintZ(&line, "Script: {s}", .{if (text.len == 0) "None" else text}) catch "Script");
        warnIfMissing(state, text);
    } else {
        panels.text("Script: too long for the editor to edit (kept as it is)");
    }
    ig.igSeparatorText("Beside the map");
    var chosen: ?[]const u8 = null;
    if (ig.igBeginChild("script-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        const none_selected = value != null and value.?.len == 0;
        if (ig.igSelectableEx("None", none_selected, 0, .{ .x = 0, .y = 0 })) chosen = "";
        for (state.script_names.items) |name| {
            var label: [80:0]u8 = undefined;
            const label_text = std.fmt.bufPrintZ(&label, "{s}", .{name}) catch continue;
            const selected = value != null and std.mem.eql(u8, value.?, name);
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) chosen = name;
        }
    }
    ig.igEndChild();
    if (ig.igButton("Choose other...")) panels.chooseOtherScript(state);
    ig.igSameLine();
    ig.igBeginDisabled(value == null or value.?.len == 0);
    if (ig.igButton("Open script folder")) _ = panels.openScript(state);
    ig.igEndDisabled();
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(script_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
    // After the list: a choice re-reads the record.
    if (chosen) |name| _ = commands.setScriptFile(state, name);
}

pub const script_help = "The game runs <name>.lua from the folder of the map. Choose other copies a file there; the test game gets a copy of it too.";

/// A warning line when the named file is not beside the map (D-20): the game
/// would run the map without it.
fn warnIfMissing(state: *State, value: []const u8) void {
    if (value.len == 0) return;
    const files = state.editor.files orelse return;
    const name = core.script_file.gameScriptName(value) orelse {
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, .{ .x = 1, .y = 0.7, .z = 0.2, .w = 1 });
        panels.text("warning: this name does not name a file the game can load");
        ig.igPopStyleColor();
        return;
    };
    var path_buffer: [core.files.max_path]u8 = undefined;
    const path = core.script_file.scriptPathBeside(&path_buffer, state.editor.document.path.items, name) orelse return;
    if (files.exists(path)) return;
    var line: [160:0]u8 = undefined;
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, .{ .x = 1, .y = 0.7, .z = 0.2, .w = 1 });
    panels.text(std.fmt.bufPrintZ(&line, "warning: {s}.lua is not beside the map", .{name}) catch "warning: the script is not beside the map");
    ig.igPopStyleColor();
}

/// The two questions the script asks: Save As of a map (bring the script
/// along?) and Choose other (replace the file that is there?). Buttons are the
/// commands `script_copy_along_yes`/`_no` and `script_overwrite_yes`/`_no`.
pub fn drawScriptModals(state: *State) void {
    const copy_id = "Save As##script-copy";
    if (state.script_copy.active) {
        if (!ig.igIsPopupOpen(copy_id, 0)) _ = ig.igOpenPopup(copy_id, 0);
    }
    if (ig.igBeginPopupModal(copy_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        var line: [200:0]u8 = undefined;
        const replace = state.script_copy.replace;
        if (replace) {
            panels.text(std.fmt.bufPrintZ(&line, "A different {s}.lua is already beside the new map. Replace it with the old map's?", .{state.script_copy.name()}) catch "A different script is already beside the new map. Replace it?");
        } else {
            panels.text(std.fmt.bufPrintZ(&line, "Copy {s}.lua beside the new map?", .{state.script_copy.name()}) catch "Copy the script beside the new map?");
        }
        if (ig.igButton(if (replace) "Replace" else "Yes")) {
            _ = commands.run(state, "script_copy_along_yes", "");
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton(if (replace) "Keep it" else "No")) {
            _ = commands.run(state, "script_copy_along_no", "");
            ig.igCloseCurrentPopup();
        }
        if (!state.script_copy.active) ig.igCloseCurrentPopup();
        ig.igEndPopup();
    }
    const overwrite_id = "Script##script-overwrite";
    if (state.script_pick_active) {
        if (!ig.igIsPopupOpen(overwrite_id, 0)) _ = ig.igOpenPopup(overwrite_id, 0);
    }
    if (ig.igBeginPopupModal(overwrite_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        var line: [200:0]u8 = undefined;
        const name = core.script_file.pickedName(state.script_pick.slice()) orelse "the script";
        panels.text(std.fmt.bufPrintZ(&line, "{s}.lua is already beside the map. Replace it?", .{name}) catch "Replace the script beside the map?");
        if (ig.igButton("Replace")) {
            _ = commands.run(state, "script_overwrite_yes", "");
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton("Cancel")) {
            _ = commands.run(state, "script_overwrite_no", "");
            ig.igCloseCurrentPopup();
        }
        if (!state.script_pick_active) ig.igCloseCurrentPopup();
        ig.igEndPopup();
    }
}

/// Hands `url` (built by `script_file.folderUrl` from the resolved folder of a
/// validated script, never from typed text) to the system, which shows the
/// folder. Null on success, else why not.
pub fn openUrlWithSystem(url: [:0]const u8) ?[]const u8 {
    if (sdl3.c.SDL_OpenURL(url.ptr)) return null;
    const reason = sdl3.c.SDL_GetError();
    return if (reason != null) std.mem.span(reason) else "the system would not open the script";
}

/// What one frame of the Script areas panel asked for, run after the list's
/// loop: a command re-reads the rows the loop holds.
const AreaAction = union(enum) {
    none,
    choose: usize,
    rename: usize,
    delete: usize,
};

/// What the Script areas panel says.
pub const script_areas_help = "Drag on the map to draw an area; click inside one to select it. On the selected area drag the square at its centre to move it and the circle at its corner or edge to resize it; Delete removes it. The game finds an area by its name.";

/// D-21: the MFC Areas tab as a panel - the Rectangle / Circle switch, the name of
/// the next area (empty: the first free area_<n>), the map's areas (a click selects
/// one and centres the view on it), and the selected area's Rename and Delete.
/// Every control runs a named command (commands.zig), so BK_EDITOR_AUTO reaches it
/// too.
pub fn drawScriptAreas(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Script areas", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshAreas();
    const tool = &state.view.areas_tool;
    var action: AreaAction = .none;

    ig.igSeparatorText("New area");
    if (ig.igRadioButton("Rectangle", tool.shape == .rectangle)) _ = commands.run(state, "area_shape", "rect");
    ig.igSameLine();
    if (ig.igRadioButton("Circle", tool.shape == .circle)) _ = commands.run(state, "area_shape", "circle");
    if (ig.igInputTextWithHint("name", "next free: area_<n>", &tool.name_buffer, tool.name_buffer.len, 0)) {
        tool.name_len = std.mem.sliceTo(&tool.name_buffer, 0).len;
    }

    ig.igSeparatorText("Areas");
    const rows = state.areas.items;
    if (rows.len == 0) {
        panels.text("the map has no script areas");
    } else if (ig.igBeginChild("areas-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows, 0..) |area, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var label: [96:0]u8 = undefined;
            const label_text = std.fmt.bufPrintZ(&label, "{s} ({s})", .{ area.nameSlice(), if (area.shape == .rectangle) "rectangle" else "circle" }) catch continue;
            const selected = tool.selected != null and tool.selected.? == index;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (rows.len != 0) ig.igEndChild();

    ig.igSeparatorText("Selected");
    if (tool.selected != null and tool.selected.? < rows.len) {
        const index = tool.selected.?;
        const area = rows[index];
        // The Rename field follows the selection, and is not overwritten under
        // the cursor while it is being typed in.
        if (state.area_rename_for == null or state.area_rename_for.? != index) {
            @memset(&state.area_rename_field, 0);
            @memcpy(state.area_rename_field[0..area.nameSlice().len], area.nameSlice());
            state.area_rename_for = index;
        }
        var line: [128:0]u8 = undefined;
        const detail = if (area.shape == .rectangle)
            std.fmt.bufPrintZ(&line, "area {d}: rectangle at {d:.0}, {d:.0}, half size {d:.0} x {d:.0}", .{ index, area.cx, area.cy, area.hx, area.hy })
        else
            std.fmt.bufPrintZ(&line, "area {d}: circle at {d:.0}, {d:.0}, radius {d:.0}", .{ index, area.cx, area.cy, area.r });
        panels.text(detail catch "selected");
        _ = ig.igInputText("rename", &state.area_rename_field, state.area_rename_field.len + 1, 0);
        if (ig.igButton("Rename")) action = .{ .rename = index };
        ig.igSameLine();
        if (ig.igButton("Delete")) action = .{ .delete = index };
    } else {
        state.area_rename_for = null;
        panels.text("none selected");
    }
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(script_areas_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    switch (action) {
        .none => {},
        .choose => |index| _ = commands.gotoArea(state, index),
        .rename => |index| {
            var buffer: [records.area_name_capacity]u8 = undefined;
            const text = std.mem.sliceTo(&state.area_rename_field, 0);
            @memcpy(buffer[0..text.len], text);
            _ = commands.renameArea(state, index, buffer[0..text.len]);
            state.area_rename_for = null;
        },
        .delete => |index| {
            _ = commands.deleteArea(state, index);
            state.area_rename_for = null;
        },
    }
}

/// What one frame of the Start Commands window asked for, run after the lists'
/// loops: a command re-reads the rows the loops hold.
const StartAction = union(enum) {
    none,
    choose: usize,
    set_type: i32,
    set_number: f32,
    add_unit,
    remove_unit: i32,
    set_target,
    delete,
};

/// What the Start Commands window says.
pub const start_commands_help = "A start command orders its units when the mission starts. Select an object on the map and use Unit > Add start command, then Add selected unit for more units, pick the type, and Set target and click the map or a unit (red lines run from the units to the target). Delete removes the selected command.";

/// The name of action type `id`, from the action list; "type <id>" when the list
/// does not have it (a file's own odd command).
fn actionName(state: *const State, id: i32, buffer: *[40:0]u8) [:0]const u8 {
    for (state.startcmd_actions) |*item| {
        if (item.id == id) return std.fmt.bufPrintZ(buffer, "{s}", .{item.nameSlice()}) catch "type";
    }
    return std.fmt.bufPrintZ(buffer, "type {d}", .{id}) catch "type";
}

/// The target of a command as words: its object, its point, or none.
fn targetText(command: records.StartCommand, buffer: *[64:0]u8) [:0]const u8 {
    if (command.link_id != 0) return std.fmt.bufPrintZ(buffer, "unit {d}", .{command.link_id}) catch "unit";
    if (command.x != 0 or command.y != 0) return std.fmt.bufPrintZ(buffer, "point {d:.0}, {d:.0}", .{ command.x, command.y }) catch "point";
    return "no target";
}

/// The object's name for a unit row, from the document.
fn unitName(state: *const State, link_id: i32) []const u8 {
    const object = state.editor.document.find(link_id) orelse return "not on the map";
    return object.nameSlice();
}

/// D-17: the MFC Start Commands dialog as a floating window (Unit -> Start
/// commands...) - the map's commands (type, unit count, target), and for the
/// selected one the type list (from Data/Editor/actions.ini), the number, its units
/// with Remove, Add selected unit, Set target and Delete; the explosion flag is
/// shown and never edited. The Delete key deletes the selected command while the
/// window is focused. Every control runs a named command (commands.zig), so
/// BK_EDITOR_AUTO reaches it too.
pub fn drawStartCommands(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.startcmds_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Start commands", &state.startcmds_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshStartActions();
    state.refreshStartCommands();
    var action: StartAction = .none;

    if (state.startcmd_actions.len == 0) {
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, .{ .x = 1, .y = 0.7, .z = 0.2, .w = 1 });
        panels.text("the action list Data\\Editor\\actions.ini is not in the data: no start command can be made");
        ig.igPopStyleColor();
    }

    ig.igSeparatorText("Commands");
    const rows = state.startcmds;
    if (rows.len == 0) {
        panels.text("the map has no start commands");
    } else if (ig.igBeginChild("startcmds-list", .{ .x = 0, .y = 130 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows, 0..) |command, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var type_buffer: [40:0]u8 = undefined;
            var target_buffer: [64:0]u8 = undefined;
            var label: [160:0]u8 = undefined;
            const label_text = std.fmt.bufPrintZ(&label, "{d}: {s}, {d} {s}, {s}", .{ index, actionName(state, command.cmd_type, &type_buffer), command.units.len, if (command.units.len == 1) "unit" else "units", targetText(command, &target_buffer) }) catch continue;
            const selected = state.startcmd_selected != null and state.startcmd_selected.? == index;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (rows.len != 0) ig.igEndChild();

    ig.igSeparatorText("Selected");
    if (state.startcmd_selected != null and state.startcmd_selected.? < rows.len) {
        const index = state.startcmd_selected.?;
        const command = rows[index];
        var title: [48:0]u8 = undefined;
        panels.text(std.fmt.bufPrintZ(&title, "start command {d}", .{index}) catch "start command");
        var type_buffer: [40:0]u8 = undefined;
        if (ig.igBeginCombo("type", actionName(state, command.cmd_type, &type_buffer).ptr, 0)) {
            for (state.startcmd_actions) |*item| {
                ig.igPushIDInt(item.id);
                defer ig.igPopID();
                var item_label: [80:0]u8 = undefined;
                const item_text = std.fmt.bufPrintZ(&item_label, "{s}", .{item.nameSlice()}) catch continue;
                if (ig.igSelectableEx(item_text.ptr, item.id == command.cmd_type, 0, .{ .x = 0, .y = 0 })) action = .{ .set_type = item.id };
            }
            ig.igEndCombo();
        }
        // The number follows the selection and is not overwritten under the cursor.
        if (state.startcmd_number_for == null or state.startcmd_number_for.? != index) {
            state.startcmd_number_field = command.number;
            state.startcmd_number_for = index;
        }
        ig.igPushItemWidth(120);
        _ = ig.igInputFloatEx("number", &state.startcmd_number_field, 0, 0, "%.3f", 0);
        ig.igPopItemWidth();
        if (ig.igIsItemDeactivatedAfterEdit()) action = .{ .set_number = state.startcmd_number_field };

        var target_buffer: [64:0]u8 = undefined;
        var target_line: [96:0]u8 = undefined;
        panels.text(std.fmt.bufPrintZ(&target_line, "target: {s}", .{targetText(command, &target_buffer)}) catch "target");
        ig.igSameLine();
        if (ig.igSmallButton("Set target")) action = .set_target;
        var flag_line: [96:0]u8 = undefined;
        panels.text(std.fmt.bufPrintZ(&flag_line, "from explosion: {s} (kept from the file)", .{if (command.from_explosion) "yes" else "no"}) catch "from explosion");

        ig.igSeparatorText("Units");
        if (ig.igBeginChild("startcmd-units", .{ .x = 0, .y = 90 }, ig.ImGuiChildFlags_Borders, 0)) {
            for (command.units) |unit| {
                ig.igPushIDInt(unit);
                defer ig.igPopID();
                var line: [128:0]u8 = undefined;
                panels.text(std.fmt.bufPrintZ(&line, "{d}: {s}", .{ unit, unitName(state, unit) }) catch "unit");
                ig.igSameLine();
                if (ig.igSmallButton("Remove")) action = .{ .remove_unit = unit };
            }
        }
        ig.igEndChild();
        ig.igBeginDisabled(state.editor.selection == null);
        if (ig.igButton("Add selected unit")) action = .add_unit;
        ig.igEndDisabled();
        ig.igSameLine();
        if (ig.igButton("Delete")) action = .delete;
    } else {
        state.startcmd_number_for = null;
        panels.text("none selected");
    }
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(start_commands_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    // The Delete key is this window's while it is focused: the claim keeps the map's
    // Select tool from deleting the selected object with it (the other keys - undo,
    // the camera - stay the view's).
    if (ig.igIsWindowFocused(ig.ImGuiFocusedFlags_RootAndChildWindows) and !ig.igGetIO().*.WantTextInput) {
        state.view.delete_claimed = true;
        if (state.startcmd_selected != null and ig.igIsKeyPressedEx(ig.ImGuiKey_Delete, false)) action = .delete;
    }

    const selected = state.startcmd_selected;
    switch (action) {
        .none => {},
        .choose => |index| state.startcmd_selected = index,
        .set_type => |id| if (selected) |index| {
            _ = commands.setStartCommandType(state, index, id);
        },
        .set_number => |number| if (selected) |index| {
            _ = commands.setStartCommandNumber(state, index, number);
            state.startcmd_number_for = null;
        },
        .add_unit => if (selected) |index| {
            _ = commands.addSelectedUnitTo(state, index);
        },
        .remove_unit => |unit| if (selected) |index| {
            _ = commands.removeUnitFrom(state, index, unit);
        },
        .set_target => if (selected) |index| {
            _ = commands.beginTarget(state, index);
        },
        .delete => if (selected) |index| {
            _ = commands.deleteStartCommandAt(state, index);
        },
    }
}

/// What one frame of the Reserve positions panel asked for, run after the list's loop.
const ReserveAction = union(enum) {
    none,
    choose: usize,
    commit,
    clear,
    delete,
};

/// What the Reserve positions panel says.
pub const reserve_help = "Click a gun - a self-propelled or a towed one - then, for a towed gun, its truck, then the ground; Enter (or Add) adds the position, Escape (or Clear) drops the choice. Select a position in the list to delete it (Delete). The game puts the gun at its place when the mission starts, the truck towing it.";

/// The name of the object `link_id` for a row, from the document.
fn reserveObjectName(state: *const State, link_id: i32) []const u8 {
    const object = state.editor.document.find(link_id) orelse return "not on the map";
    return object.nameSlice();
}

/// D-18: the MFC editor's artillery positions mode as a panel - the choice in hand (gun,
/// truck, place), Add and Clear, the map's positions (a click selects one and centres
/// the view on its place), Delete. Every control runs a named command or the tool's own
/// function (commands.zig), so BK_EDITOR_AUTO reaches it too.
pub fn drawReservePositions(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Reserve positions", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshReserve();
    const tool = &state.view.reserve_tool;
    var action: ReserveAction = .none;

    ig.igSeparatorText("In hand");
    var line: [160:0]u8 = undefined;
    if (tool.gun) |gun| {
        panels.text(std.fmt.bufPrintZ(&line, "gun: {s} ({d}, {s})", .{ reserveObjectName(state, gun), gun, if (tool.gun_role == .towed) "towed" else "self-propelled" }) catch "gun");
    } else panels.text("gun: click one");
    if (tool.truck) |truck| {
        panels.text(std.fmt.bufPrintZ(&line, "truck: {s} ({d})", .{ reserveObjectName(state, truck), truck }) catch "truck");
    } else panels.text(if (tool.gun != null and tool.gun_role == .towed) "truck: click one" else "truck: none");
    if (tool.has_place) {
        panels.text(std.fmt.bufPrintZ(&line, "place: {d:.0}, {d:.0}", .{ tool.x, tool.y }) catch "place");
    } else panels.text("place: click the ground");
    ig.igBeginDisabled(tool.pendingRecord() == null);
    if (ig.igButton("Add")) action = .commit;
    ig.igEndDisabled();
    ig.igSameLine();
    if (ig.igButton("Clear")) action = .clear;

    ig.igSeparatorText("Positions");
    const rows = state.reserve_list;
    if (rows.len == 0) {
        panels.text("the map has no reserve positions");
    } else if (ig.igBeginChild("reserve-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows, 0..) |position, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var label: [200:0]u8 = undefined;
            const gun_name = reserveObjectName(state, position.artillery);
            const label_text = if (position.truck != 0)
                std.fmt.bufPrintZ(&label, "{d}: {s} + {s}, at {d:.0}, {d:.0}", .{ index, gun_name, reserveObjectName(state, position.truck), position.x, position.y })
            else
                std.fmt.bufPrintZ(&label, "{d}: {s}, no truck, at {d:.0}, {d:.0}", .{ index, gun_name, position.x, position.y });
            const selected = tool.selected != null and tool.selected.? == index;
            if (ig.igSelectableEx((label_text catch continue).ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (rows.len != 0) ig.igEndChild();
    ig.igBeginDisabled(tool.selected == null);
    if (ig.igButton("Delete")) action = .delete;
    ig.igEndDisabled();

    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(reserve_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    switch (action) {
        .none => {},
        .choose => |index| {
            tool.selected = index;
            if (index < rows.len) {
                const world = marker_logic.aiToWorld(.{ .x = rows[index].x, .y = rows[index].y });
                state.view.centreOn(state.real, world.x, world.y);
            }
        },
        .commit => _ = commands.run(state, "reserve_commit", ""),
        .clear => tool.clearPending(),
        .delete => if (tool.selected) |index| {
            _ = commands.deleteReserveAt(state, index);
        },
    }
}

/// What one frame of the AI General panel asked for, run after the lists' loops: a
/// command re-reads the side the loops hold.
const AiAction = union(enum) {
    none,
    side: usize,
    add_mobile,
    remove_mobile: i32,
    choose: usize,
    switch_type,
    delete,
};

/// What the AI General panel says.
pub const ai_general_help = "Click open ground for a defence parcel (radius 4 map tiles, the default and the smallest); click inside a parcel for a reinforce point. Drag a parcel's square to move it and the circle at the end of its arrow to set its radius and direction; drag a point's square to move it and its circle to turn it. Enter, Insert or Space switch the selected parcel between defence and reinforce; Delete removes the selected point or parcel. The game gives a general to sides 0 and 1, never to the player's own side.";

/// One map tile in AI units, for the radius column.
const ai_tile: f32 = 64.0;

fn parcelKindName(kind: records.ParcelKind) []const u8 {
    return switch (kind) {
        .defence => "defence",
        .reinforce => "reinforce",
        else => "unknown type",
    };
}

/// D-19: the MFC AI General tab as a panel - the side (a radio for each side the map has
/// and one more, "new side", which the first edit creates with every side below it), the
/// side's mobile script IDs with Remove each, a field and Add (an ID already there is
/// skipped), and the side's parcels (type, centre, radius in map tiles, direction in
/// degrees, points) with Switch type and Delete for the selected one. Every control
/// runs a named command (commands.zig), so BK_EDITOR_AUTO's `do=` reaches it too.
pub fn drawAIGeneral(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("AI general", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    state.refreshAi();
    const tool = &state.view.ai_tool;
    const side = state.aiActive();
    var action: AiAction = .none;

    ig.igSeparatorText("Side");
    var shown: usize = 0;
    while (shown <= state.ai_side_count and shown < 12) : (shown += 1) {
        ig.igPushIDInt(@intCast(shown));
        defer ig.igPopID();
        var label: [32:0]u8 = undefined;
        const label_text = if (shown < state.ai_side_count)
            std.fmt.bufPrintZ(&label, "side {d}", .{shown})
        else
            std.fmt.bufPrintZ(&label, "new side ({d})", .{shown});
        if (shown != 0) ig.igSameLine();
        if (ig.igRadioButton((label_text catch continue).ptr, tool.side == shown)) action = .{ .side = shown };
    }

    ig.igSeparatorText("Mobile script IDs");
    if (side.mobile_ids.len == 0) {
        panels.text("no mobile script IDs");
    } else if (ig.igBeginChild("ai-mobile-ids", .{ .x = 0, .y = 70 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (side.mobile_ids) |script_id| {
            ig.igPushIDInt(script_id);
            defer ig.igPopID();
            var line: [24:0]u8 = undefined;
            panels.text(std.fmt.bufPrintZ(&line, "{d}", .{script_id}) catch "?");
            ig.igSameLine();
            if (ig.igSmallButton("Remove")) action = .{ .remove_mobile = script_id };
        }
    }
    if (side.mobile_ids.len != 0) ig.igEndChild();
    ig.igPushItemWidth(120);
    _ = ig.igInputIntEx("script ID", &state.ai_mobile_field, 1, 10, 0);
    ig.igPopItemWidth();
    ig.igSameLine();
    if (ig.igButton("Add")) action = .add_mobile;

    ig.igSeparatorText("Parcels");
    if (side.parcels.len == 0) {
        panels.text("this side has no parcels");
    } else if (ig.igBeginChild("ai-parcels", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (side.parcels, 0..) |parcel, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var label: [200:0]u8 = undefined;
            const degrees = @as(f32, @floatFromInt(parcel.defence_dir)) * 360.0 / 65535.0;
            const label_text = std.fmt.bufPrintZ(&label, "{d}: {s}, {d:.1} tiles, {d:.0} deg", .{ index, parcelKindName(parcel.kind), parcel.radius / ai_tile, degrees }) catch continue;
            const selected = tool.selected_parcel != null and tool.selected_parcel.? == index;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (side.parcels.len != 0) ig.igEndChild();
    ig.igBeginDisabled(tool.selected_parcel == null);
    if (ig.igButton("Switch type")) action = .switch_type;
    ig.igSameLine();
    if (ig.igButton("Delete")) action = .delete;
    ig.igEndDisabled();
    if (tool.selected_parcel) |index| {
        ig.igPushTextWrapPos(0);
        defer ig.igPopTextWrapPos();
        var line: [160:0]u8 = undefined;
        if (index < side.parcels.len) {
            const parcel = side.parcels[index];
            panels.text(std.fmt.bufPrintZ(&line, "selected: parcel {d} at {d:.0}, {d:.0}, {d} points", .{ index, parcel.cx, parcel.cy, parcel.points.len }) catch "selected");
        }
        if (tool.selected_point) |point| {
            panels.text(std.fmt.bufPrintZ(&line, "point {d} selected: Delete removes the point", .{point}) catch "point selected");
        } else {
            panels.text("Delete removes the parcel");
        }
    }

    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(ai_general_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    switch (action) {
        .none => {},
        .side => |chosen| _ = commands.setAiSide(state, chosen),
        .add_mobile => _ = commands.addAiMobile(state, state.ai_mobile_field),
        .remove_mobile => |script_id| _ = commands.removeAiMobile(state, script_id),
        .choose => |index| {
            tool.select(index, null);
            if (index < side.parcels.len) {
                const world = marker_logic.aiToWorld(.{ .x = side.parcels[index].cx, .y = side.parcels[index].cy });
                state.view.centreOn(state.real, world.x, world.y);
            }
        },
        .switch_type => if (tool.selected_parcel) |index| {
            _ = commands.run(state, "ai_toggle_type", std.fmt.bufPrint(&state.ai_arg_buffer, "{d}", .{index}) catch "0");
        },
        .delete => _ = commands.deleteAiSelected(state),
    }
}
