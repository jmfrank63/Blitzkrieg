//! The M2 panels (04-03 on): drawn from panels.zig's `draw` like the M1 ones,
//! each a thin ImGui layer over the named commands in commands.zig, so a
//! button and a BK_EDITOR_AUTO `do=` run the same code. This plan adds the
//! Camera anchors panel; later plans add theirs here.
const std = @import("std");
const imgui = @import("editor_imgui");
const sdl3 = @import("sdl3");
const core = @import("editor_core");
const kit = @import("editor_kit");
const panels = @import("panels.zig");
const commands = @import("commands.zig");
const marker_logic = @import("marker_logic.zig");

const l10n = kit.l10n;

const ig = imgui.c;
const State = panels.State;
const records = core.records;

/// The text of `key` as a slice, for the calls that take one.
fn tr(comptime key: []const u8) []const u8 {
    return std.mem.span(l10n.t(key));
}

/// `tr` with a terminating NUL, for the ImGui calls that take a C string.
fn trZ(comptime key: []const u8) [:0]const u8 {
    return std.mem.span(l10n.t(key));
}

/// `key` with its `{0}`.. slots filled, into `buffer`; the key's unfilled text when it is too small.
fn say(buffer: []u8, comptime key: []const u8, args: anytype) []const u8 {
    return l10n.format(buffer, key, args) catch tr(key);
}

/// `say` with a terminating NUL, for the ImGui calls that take a C string.
fn sayZ(buffer: []u8, comptime key: []const u8, args: anytype) [:0]const u8 {
    return l10n.formatZ(buffer, key, args) orelse std.mem.span(l10n.t(key));
}

/// A position or size as a whole number, as the panels always showed them.
fn whole(value: f32) i64 {
    return @intFromFloat(@round(value));
}

/// `value` with one decimal, for the places a whole number would hide a radius.
fn tenths(buffer: []u8, value: f32) []const u8 {
    return std.fmt.bufPrint(buffer, "{d:.1}", .{value}) catch "?";
}

/// The rows: "Neutral", then "Player 0" up to the larger of the map's players
/// and the anchors the file holds, at most the record's 32.
fn playerRows(state: *const State) usize {
    const players: usize = @intCast(@max(state.editor.document.info.player_count, 0));
    return @min(@max(players, state.anchors.player_count), records.max_camera_players);
}

/// D-22: every anchor with its position, "Go to" and "Clear". The cache is
/// State's own (`refreshAnchors`, called from `draw` before anything else).
pub fn drawCameraAnchors(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.panel.camera_anchors"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    drawRow(state, trZ("app.m2.anchors.neutral"), commands.neutral_slot, state.anchors.neutral);
    var player: usize = 0;
    const rows = playerRows(state);
    while (player < rows) : (player += 1) {
        var label: [64:0]u8 = undefined;
        drawRow(state, sayZ(&label, "app.m2.anchors.player", .{player}), @intCast(player), state.anchors.slot(player));
    }
    ig.igSeparator();
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.anchors.note"));
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
}

fn drawRow(state: *State, label: [:0]const u8, slot: i32, anchor: records.Vec3) void {
    ig.igPushIDInt(slot);
    defer ig.igPopID();
    const is_set = !anchor.isUnset();
    var line: [128:0]u8 = undefined;
    panels.text(if (is_set)
        say(&line, "app.m2.anchors.pos", .{ label, whole(anchor.x), whole(anchor.y) })
    else
        say(&line, "app.m2.anchors.unset", .{label}));
    ig.igSameLine();
    ig.igBeginDisabled(!is_set);
    if (ig.igSmallButton(l10n.id("app.m2.goto"))) _ = commands.gotoAnchor(state, slot);
    ig.igSameLine();
    if (ig.igSmallButton(l10n.id("app.m2.clear"))) _ = commands.clearAnchor(state, slot);
    ig.igEndDisabled();
}

/// D-08: the MFC Roads and Rivers tabs (TabVOVSODialog) as one panel - the
/// Road / River switch, the season's types, width 1..16, opacity 0..100 %, the
/// width mode, and the selected line with a Delete button. Every control
/// runs a named command (commands.zig), so BK_EDITOR_AUTO reaches it too.
pub fn drawRoadsRivers(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.tool.roads_rivers"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    const tool = &state.view.roads_rivers;
    if (ig.igRadioButton(l10n.id("app.m2.road"), tool.kind == .road)) _ = commands.setVsoKind(state, .road);
    ig.igSameLine();
    if (ig.igRadioButton(l10n.id("app.m2.river"), tool.kind == .river)) _ = commands.setVsoKind(state, .river);

    state.refreshVsoTypes();
    ig.igSeparatorText(l10n.t("app.m2.type"));
    if (state.vso_types.len == 0) {
        panels.text(tr("app.m2.roads.no_types"));
    } else if (ig.igBeginChild("##vso-types", .{ .x = 0, .y = 140 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (state.vso_types, 0..) |*item, index| {
            var name: [core.bridge.vso_name_capacity + 1:0]u8 = undefined;
            const label = std.mem.printSentinel(&name, "{s}", .{item.nameSlice()}, 0) catch continue;
            const selected = std.mem.eql(u8, item.nameSlice(), tool.desc());
            if (ig.igSelectableEx(label.ptr, selected, 0, .{ .x = 0, .y = 0 })) _ = commands.chooseVsoType(state, index);
        }
    }
    if (state.vso_types.len != 0) ig.igEndChild();

    // 04-13: in the width mode All the sliders re-width the selected line too
    // (the MFC editor's CW_ALL); one slider drag is one undo step, its gesture
    // begun when the slider is taken hold of.
    var width: c_int = @intFromFloat(@round(tool.width_tiles));
    const width_moved = ig.igSliderInt(l10n.id("app.m2.roads.width"), &width, 1, 16);
    if (ig.igIsItemActivated()) state.vso_slider_gesture = state.editor.beginGesture();
    if (width_moved) _ = commands.setVsoWidthTiles(state, @floatFromInt(width), state.vso_slider_gesture);
    var percent: c_int = @intFromFloat(@round(tool.opacity * 100));
    const opacity_moved = ig.igSliderInt(l10n.id("app.m2.roads.opacity"), &percent, 0, 100);
    if (ig.igIsItemActivated()) state.vso_slider_gesture = state.editor.beginGesture();
    if (opacity_moved) _ = commands.setVsoOpacity(state, @as(f32, @floatFromInt(percent)) / 100.0, state.vso_slider_gesture);
    ig.igSeparatorText(l10n.t("app.m2.roads.width_mode"));
    if (ig.igRadioButton(l10n.id("app.m2.roads.single"), tool.width_mode == .single)) tool.width_mode = .single;
    if (ig.igRadioButton(l10n.id("app.m2.roads.from_here"), tool.width_mode == .multi)) tool.width_mode = .multi;
    if (ig.igRadioButton(l10n.id("app.m2.roads.all"), tool.width_mode == .all)) tool.width_mode = .all;
    ig.igBeginDisabled(tool.width_mode != .multi);
    _ = ig.igCheckbox(l10n.id("app.m2.roads.backwards"), &tool.multi_backwards);
    ig.igEndDisabled();

    ig.igSeparatorText(l10n.t("app.m2.selected"));
    if (tool.selectedView(state.editor)) |view| {
        const selected = tool.selected.?;
        var line: [256:0]u8 = undefined;
        const kind = if (selected.kind == .road) tr("app.m2.road") else tr("app.m2.river");
        panels.text(say(&line, "app.m2.roads.selected", .{ kind, selected.index, view.saved_id, view.control_points.len }));
        ig.igPushTextWrapPos(0);
        panels.text(view.descSlice());
        ig.igPopTextWrapPos();
        if (ig.igButton(l10n.id("kit.common.delete"))) _ = commands.deleteSelectedVso(state);
    } else {
        ig.igPushTextWrapPos(0);
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
        panels.text(tr("app.m2.roads.help"));
        ig.igPopStyleColor();
        ig.igPopTextWrapPos();
    }
}

/// D-10..D-12: the MFC Bridges tab (BridgeSetupDialog) - every bridge type
/// with its picture, its direction and whether it can be built during play,
/// and for the selected bridge its type, span count, Rotate, "Built during
/// play" and Delete. Every control runs a named command (commands.zig).
pub fn drawBridges(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.m2.bridges.title"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshBridgeTypes();
    state.refreshBridges();
    const tool = &state.view.bridge_tool;
    ig.igSeparatorText(l10n.t("app.m2.type"));
    if (state.bridge_types.len == 0) {
        panels.text(tr("app.m2.bridges.no_types"));
    } else if (ig.igBeginChild("##bridge-types", .{ .x = 0, .y = @max(size.y - 260, 120) }, ig.ImGuiChildFlags_Borders, 0)) {
        for (state.bridge_types, 0..) |*item, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            state.pictures.request(item.nameSlice());
            panels.drawPaletteRowPicture(state, item.nameSlice());
            ig.igSameLine();
            var line: [core.bridge.name_capacity + 160:0]u8 = undefined;
            const direction = if (item.direction == .horizontal) tr("app.m2.bridges.horizontal") else tr("app.m2.bridges.vertical");
            const label = if (item.build_during_play_allowed)
                sayZ(&line, "app.m2.bridges.row_play", .{ item.nameSlice(), direction })
            else
                sayZ(&line, "app.m2.bridges.row", .{ item.nameSlice(), direction });
            const selected = std.mem.eql(u8, item.nameSlice(), tool.desc());
            if (ig.igSelectableEx(label.ptr, selected, 0, .{ .x = 0, .y = 48 })) _ = commands.chooseBridgeType(state, item.nameSlice());
        }
    }
    if (state.bridge_types.len != 0) ig.igEndChild();
    if (state.real.gpuDevice()) |device| state.pictures.pump(state.real, device, panels.picture_pump_budget);

    ig.igSeparatorText(l10n.t("app.m2.selected"));
    const selected = tool.selected;
    if (selected != null and selected.? < state.bridge_infos.len) {
        const info = state.bridge_infos[selected.?];
        var line: [core.bridge.name_capacity + 96:0]u8 = undefined;
        panels.text(say(&line, "app.m2.bridges.selected", .{ selected.?, info.descSlice(), info.span_count }));
        if (ig.igButton(l10n.id("app.m2.bridges.rotate"))) _ = commands.rotateSelectedBridge(state);
        var built = info.built_during_play;
        const allowed = std.mem.indexOf(u8, info.descSlice(), "WoodenBig_Heavy_") != null;
        ig.igBeginDisabled(!allowed);
        if (ig.igCheckbox(l10n.id("app.m2.bridges.built"), &built)) _ = commands.toggleSelectedBridgeBuild(state);
        ig.igEndDisabled();
        if (ig.igButton(l10n.id("kit.common.delete"))) _ = commands.deleteSelectedBridge(state);
    } else {
        ig.igPushTextWrapPos(0);
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
        panels.text(tr("app.m2.bridges.help"));
        ig.igPopStyleColor();
        ig.igPopTextWrapPos();
    }
}

/// D-14: the MFC Fences tab (FenceSetupWindow) - every fence type with its
/// picture. Choosing one is a named command (commands.zig).
pub fn drawFences(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.m2.fences.title"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshFenceTypes();
    const tool = &state.view.fence_tool;
    ig.igSeparatorText(l10n.t("app.m2.type"));
    if (state.fence_types.len == 0) {
        panels.text(tr("app.m2.fences.no_types"));
    } else if (ig.igBeginChild("##fence-types", .{ .x = 0, .y = @max(size.y - 190, 120) }, ig.ImGuiChildFlags_Borders, 0)) {
        for (state.fence_types, 0..) |*item, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            state.pictures.request(item.nameSlice());
            panels.drawPaletteRowPicture(state, item.nameSlice());
            ig.igSameLine();
            var line: [core.bridge.name_capacity + 8:0]u8 = undefined;
            const label = std.mem.printSentinel(&line, "{s}", .{item.nameSlice()}, 0) catch continue;
            const selected = std.mem.eql(u8, item.nameSlice(), tool.desc());
            if (ig.igSelectableEx(label.ptr, selected, 0, .{ .x = 0, .y = 48 })) _ = commands.chooseFenceType(state, item.nameSlice());
        }
    }
    if (state.fence_types.len != 0) ig.igEndChild();
    if (state.real.gpuDevice()) |device| state.pictures.pump(state.real, device, panels.picture_pump_budget);
    ig.igSeparatorText(l10n.t("app.m2.fences.placing"));
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.fences.help"));
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
}

/// D-13: the MFC Entrenchment tool's controls - the player the pieces will
/// belong to (the Objects panel's player until chosen here), the gestures,
/// and the selected entrenchment's piece and section counts with a Delete
/// button. Every control runs a named command (commands.zig).
pub fn drawEntrenchments(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.m2.trench.title"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshTrenches();
    const tool = &state.view.trench_tool;
    if (!state.trench_player_chosen) tool.player = @max(state.view.placer.player, 0);
    ig.igSeparatorText(l10n.t("app.m2.player"));
    var player: c_int = tool.player;
    const players: c_int = @max(state.editor.document.info.player_count, 1);
    if (ig.igSliderInt(l10n.id("app.props.player"), &player, 0, players - 1)) {
        var buffer: [12]u8 = undefined;
        const arg = std.fmt.bufPrint(&buffer, "{d}", .{player}) catch "0";
        _ = commands.run(state, "trench_player", arg);
    }
    ig.igSeparatorText(l10n.t("app.m2.trench.drawing"));
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.trench.help"));
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
    if (tool.drawing()) {
        var line: [96:0]u8 = undefined;
        panels.text(say(&line, "app.m2.trench.points", .{tool.len}));
    }

    ig.igSeparatorText(l10n.t("app.m2.selected"));
    const selected = tool.selected;
    if (selected != null and selected.? < state.trench_infos.len) {
        const info = state.trench_infos[selected.?];
        var line: [192:0]u8 = undefined;
        panels.text(say(&line, "app.m2.trench.selected", .{ selected.?, info.player, info.piece_count, info.section_count }));
        if (ig.igButton(l10n.id("kit.common.delete"))) _ = commands.run(state, "trench_delete", "");
    } else {
        var line: [128:0]u8 = undefined;
        panels.text(say(&line, "app.m2.trench.none", .{state.trench_infos.len}));
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
    const open = ig.igBegin(l10n.id("app.window.groups"), &state.groups_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshGroups();
    var action: GroupAction = .none;

    ig.igPushItemWidth(120);
    _ = ig.igInputIntEx(l10n.id("app.m2.groups.id"), &state.group_new_from, 1, 10, 0);
    ig.igPopItemWidth();
    ig.igSameLine();
    if (ig.igButton(l10n.id("app.m2.groups.new"))) action = .new;
    ig.igSameLine();
    ig.igBeginDisabled(state.group_selected == null);
    if (ig.igButton(l10n.id("kit.common.delete"))) action = .delete;
    ig.igEndDisabled();

    ig.igSeparatorText(l10n.t("app.m2.groups.groups"));
    const rows = state.groups.items;
    if (rows.len == 0) {
        panels.text(tr("app.m2.groups.none"));
    } else if (ig.igBeginChild("##groups-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows) |row| {
            ig.igPushIDInt(row.id);
            defer ig.igPopID();
            var checked = state.groupIsChecked(row.id);
            if (ig.igCheckbox(l10n.id("app.m2.groups.hide"), &checked)) action = .{ .hide = .{ .id = row.id, .on = checked } };
            ig.igSameLine();
            var label: [160:0]u8 = undefined;
            const label_text = sayZ(&label, "app.m2.groups.row", .{ row.id, row.ids.len });
            const selected = state.group_selected != null and state.group_selected.? == row.id;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = row.id };
        }
    }
    if (rows.len != 0) ig.igEndChild();

    ig.igSeparatorText(l10n.t("app.m2.groups.script_ids"));
    const selected_row = if (state.group_selected) |id| state.findGroup(id) else null;
    if (selected_row) |row| {
        var title: [96:0]u8 = undefined;
        panels.text(say(&title, "app.m2.groups.group", .{row.id}));
        if (row.ids.len == 0) {
            panels.text(tr("app.m2.groups.no_ids"));
        } else if (ig.igBeginChild("##group-ids", .{ .x = 0, .y = 100 }, ig.ImGuiChildFlags_Borders, 0)) {
            for (row.ids) |script_id| {
                ig.igPushIDInt(script_id);
                defer ig.igPopID();
                var line: [24:0]u8 = undefined;
                panels.text(std.mem.printSentinel(&line, "{d}", .{script_id}, 0) catch "?");
                ig.igSameLine();
                if (ig.igSmallButton(l10n.id("app.m2.remove"))) action = .{ .remove_id = script_id };
            }
        }
        if (row.ids.len != 0) ig.igEndChild();
        ig.igPushItemWidth(120);
        _ = ig.igInputIntEx(l10n.id("app.props.script_id"), &state.group_script_field, 1, 10, 0);
        ig.igPopItemWidth();
        ig.igSameLine();
        if (ig.igButton(l10n.id("kit.common.add"))) action = .add_id;
        if (ig.igButton(l10n.id("app.m2.groups.select_objects"))) action = .select_objects;
    } else {
        panels.text(tr("app.m2.groups.select_one"));
    }
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.groups.help"));
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
    const open = ig.igBegin(l10n.id("app.window.script"), &state.script_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    panels.refreshScriptNames(state);
    var value_buffer: [records.script_file_capacity]u8 = undefined;
    const value = commands.readScriptFile(state, &value_buffer);
    var line: [128:0]u8 = undefined;
    if (value) |text| {
        panels.text(say(&line, "app.m2.script.current", .{if (text.len == 0) tr("app.m2.none") else text}));
        warnIfMissing(state, text);
    } else {
        panels.text(tr("app.m2.script.too_long"));
    }
    ig.igSeparatorText(l10n.t("app.m2.script.beside"));
    var chosen: ?[]const u8 = null;
    if (ig.igBeginChild("##script-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        const none_selected = value != null and value.?.len == 0;
        if (ig.igSelectableEx(l10n.id("app.m2.none"), none_selected, 0, .{ .x = 0, .y = 0 })) chosen = "";
        for (state.script_names.items) |name| {
            var label: [80:0]u8 = undefined;
            const label_text = std.mem.printSentinel(&label, "{s}", .{name}, 0) catch continue;
            const selected = value != null and std.mem.eql(u8, value.?, name);
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) chosen = name;
        }
    }
    ig.igEndChild();
    if (ig.igButton(l10n.id("app.m2.script.choose_other"))) panels.chooseOtherScript(state);
    ig.igSameLine();
    ig.igBeginDisabled(value == null or value.?.len == 0);
    if (ig.igButton(l10n.id("app.m2.script.open_folder"))) _ = panels.openScript(state);
    ig.igEndDisabled();
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.script.help"));
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
    // After the list: a choice re-reads the record.
    if (chosen) |name| _ = commands.setScriptFile(state, name);
}

/// A warning line when the named file is not beside the map (D-20): the game
/// would run the map without it.
fn warnIfMissing(state: *State, value: []const u8) void {
    if (value.len == 0) return;
    const files = state.editor.files orelse return;
    const name = kit.script_file.gameScriptName(value) orelse {
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, .{ .x = 1, .y = 0.7, .z = 0.2, .w = 1 });
        panels.text(tr("app.m2.script.warn_bad_name"));
        ig.igPopStyleColor();
        return;
    };
    var path_buffer: [kit.files.max_path]u8 = undefined;
    const path = kit.script_file.scriptPathBeside(&path_buffer, state.editor.document.path.items, name) orelse return;
    if (files.exists(path)) return;
    var line: [256:0]u8 = undefined;
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, .{ .x = 1, .y = 0.7, .z = 0.2, .w = 1 });
    panels.text(say(&line, "app.m2.script.warn_missing", .{name}));
    ig.igPopStyleColor();
}

/// The two questions the script asks: Save As of a map (bring the script
/// along?) and Choose other (replace the file that is there?). Buttons are the
/// commands `script_copy_along_yes`/`_no` and `script_overwrite_yes`/`_no`.
pub fn drawScriptModals(state: *State) void {
    const copy_id = l10n.id("app.m2.script.copy_title");
    if (state.script_copy.active) {
        if (!ig.igIsPopupOpen(copy_id, 0)) _ = ig.igOpenPopup(copy_id, 0);
    }
    if (ig.igBeginPopupModal(copy_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        var line: [320:0]u8 = undefined;
        const replace = state.script_copy.replace;
        if (replace) {
            panels.text(say(&line, "app.m2.script.copy_replace", .{state.script_copy.name()}));
        } else {
            panels.text(say(&line, "app.m2.script.copy_ask", .{state.script_copy.name()}));
        }
        if (ig.igButton(if (replace) l10n.id("app.m2.replace") else l10n.id("kit.common.yes"))) {
            _ = commands.run(state, "script_copy_along_yes", "");
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton(if (replace) l10n.id("app.m2.script.keep") else l10n.id("kit.common.no"))) {
            _ = commands.run(state, "script_copy_along_no", "");
            ig.igCloseCurrentPopup();
        }
        if (!state.script_copy.active) ig.igCloseCurrentPopup();
        ig.igEndPopup();
    }
    const overwrite_id = l10n.id("app.window.script");
    if (state.script_pick_active) {
        if (!ig.igIsPopupOpen(overwrite_id, 0)) _ = ig.igOpenPopup(overwrite_id, 0);
    }
    if (ig.igBeginPopupModal(overwrite_id, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        var line: [320:0]u8 = undefined;
        const name = kit.script_file.pickedName(state.script_pick.slice()) orelse tr("app.m2.script.the_script");
        panels.text(say(&line, "app.m2.script.overwrite_ask", .{name}));
        if (ig.igButton(l10n.id("app.m2.replace"))) {
            _ = commands.run(state, "script_overwrite_yes", "");
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton(l10n.id("kit.common.cancel"))) {
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
    return if (reason != null) std.mem.span(reason) else tr("app.m2.script.open_failed");
}

/// What one frame of the Script areas panel asked for, run after the list's
/// loop: a command re-reads the rows the loop holds.
const AreaAction = union(enum) {
    none,
    choose: usize,
    rename: usize,
    delete: usize,
};

/// D-21: the MFC Areas tab as a panel - the Rectangle / Circle switch, the name of
/// the next area (empty: the first free area_<n>), the map's areas (a click selects
/// one and centres the view on it), and the selected area's Rename and Delete.
/// Every control runs a named command (commands.zig), so BK_EDITOR_AUTO reaches it
/// too.
pub fn drawScriptAreas(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.marker.script_areas"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshAreas();
    const tool = &state.view.areas_tool;
    var action: AreaAction = .none;

    ig.igSeparatorText(l10n.t("app.m2.areas.new_area"));
    if (ig.igRadioButton(l10n.id("app.m2.areas.rectangle"), tool.shape == .rectangle)) _ = commands.run(state, "area_shape", "rect");
    ig.igSameLine();
    if (ig.igRadioButton(l10n.id("app.m2.areas.circle"), tool.shape == .circle)) _ = commands.run(state, "area_shape", "circle");
    if (ig.igInputTextWithHint(l10n.id("app.m2.areas.name"), l10n.t("app.m2.areas.name_hint"), &tool.name_buffer, tool.name_buffer.len, 0)) {
        tool.name_len = std.mem.sliceTo(&tool.name_buffer, 0).len;
    }

    ig.igSeparatorText(l10n.t("app.m2.areas.areas"));
    const rows = state.areas.items;
    if (rows.len == 0) {
        panels.text(tr("app.m2.areas.none"));
    } else if (ig.igBeginChild("##areas-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows, 0..) |area, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var label: [160:0]u8 = undefined;
            const label_text = sayZ(&label, "app.m2.areas.row", .{ area.nameSlice(), if (area.shape == .rectangle) tr("app.m2.areas.rectangle_lc") else tr("app.m2.areas.circle_lc") });
            const selected = tool.selected != null and tool.selected.? == index;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (rows.len != 0) ig.igEndChild();

    ig.igSeparatorText(l10n.t("app.m2.selected"));
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
        var line: [192:0]u8 = undefined;
        panels.text(if (area.shape == .rectangle)
            say(&line, "app.m2.areas.detail_rect", .{ index, whole(area.cx), whole(area.cy), whole(area.hx), whole(area.hy) })
        else
            say(&line, "app.m2.areas.detail_circle", .{ index, whole(area.cx), whole(area.cy), whole(area.r) }));
        _ = ig.igInputText(l10n.id("app.m2.areas.rename_field"), &state.area_rename_field, state.area_rename_field.len + 1, 0);
        if (ig.igButton(l10n.id("app.m2.areas.rename"))) action = .{ .rename = index };
        ig.igSameLine();
        if (ig.igButton(l10n.id("kit.common.delete"))) action = .{ .delete = index };
    } else {
        state.area_rename_for = null;
        panels.text(tr("app.m2.none_selected"));
    }
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.areas.help"));
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

/// The name of action type `id`, from the action list; "type <id>" when the list
/// does not have it (a file's own odd command).
fn actionName(state: *const State, id: i32, buffer: *[64:0]u8) [:0]const u8 {
    for (state.startcmd_actions) |*item| {
        if (item.id == id) return std.mem.printSentinel(buffer, "{s}", .{item.nameSlice()}, 0) catch trZ("app.m2.type_field");
    }
    return sayZ(buffer, "app.m2.start.type_unknown", .{id});
}

/// The target of a command as words: its object, its point, or none.
fn targetText(command: records.StartCommand, buffer: *[96:0]u8) [:0]const u8 {
    if (command.link_id != 0) return sayZ(buffer, "app.m2.start.target_unit", .{command.link_id});
    if (command.x != 0 or command.y != 0) return sayZ(buffer, "app.m2.start.target_point", .{ whole(command.x), whole(command.y) });
    return sayZ(buffer, "app.m2.start.no_target", .{});
}

/// The object's name for a unit row, from the document.
fn unitName(state: *const State, link_id: i32) []const u8 {
    const object = state.editor.document.find(link_id) orelse return tr("app.m2.start.not_on_map");
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
    const open = ig.igBegin(l10n.id("app.window.start_commands"), &state.startcmds_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshStartActions();
    state.refreshStartCommands();
    var action: StartAction = .none;

    if (state.startcmd_actions.len == 0) {
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, .{ .x = 1, .y = 0.7, .z = 0.2, .w = 1 });
        var line: [256:0]u8 = undefined;
        panels.text(say(&line, "app.m2.start.no_actions", .{"Data\\Editor\\actions.ini"}));
        ig.igPopStyleColor();
    }

    ig.igSeparatorText(l10n.t("app.m2.start.commands"));
    const rows = state.startcmds;
    if (rows.len == 0) {
        panels.text(tr("app.m2.start.none"));
    } else if (ig.igBeginChild("##startcmds-list", .{ .x = 0, .y = 130 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows, 0..) |command, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var type_buffer: [64:0]u8 = undefined;
            var target_buffer: [96:0]u8 = undefined;
            var label: [320:0]u8 = undefined;
            const label_text = if (command.units.len == 1)
                sayZ(&label, "app.m2.start.row_one", .{ index, actionName(state, command.cmd_type, &type_buffer), targetText(command, &target_buffer) })
            else
                sayZ(&label, "app.m2.start.row_many", .{ index, actionName(state, command.cmd_type, &type_buffer), command.units.len, targetText(command, &target_buffer) });
            const selected = state.startcmd_selected != null and state.startcmd_selected.? == index;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (rows.len != 0) ig.igEndChild();

    ig.igSeparatorText(l10n.t("app.m2.selected"));
    if (state.startcmd_selected != null and state.startcmd_selected.? < rows.len) {
        const index = state.startcmd_selected.?;
        const command = rows[index];
        var title: [96:0]u8 = undefined;
        panels.text(say(&title, "app.m2.start.selected", .{index}));
        var type_buffer: [64:0]u8 = undefined;
        if (ig.igBeginCombo(l10n.id("app.m2.type_field"), actionName(state, command.cmd_type, &type_buffer).ptr, 0)) {
            for (state.startcmd_actions) |*item| {
                ig.igPushIDInt(item.id);
                defer ig.igPopID();
                var item_label: [80:0]u8 = undefined;
                const item_text = std.mem.printSentinel(&item_label, "{s}", .{item.nameSlice()}, 0) catch continue;
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
        _ = ig.igInputFloatEx(l10n.id("app.m2.start.number"), &state.startcmd_number_field, 0, 0, "%.3f", 0);
        ig.igPopItemWidth();
        if (ig.igIsItemDeactivatedAfterEdit()) action = .{ .set_number = state.startcmd_number_field };

        var target_buffer: [96:0]u8 = undefined;
        var target_line: [192:0]u8 = undefined;
        panels.text(say(&target_line, "app.m2.start.target", .{targetText(command, &target_buffer)}));
        ig.igSameLine();
        if (ig.igSmallButton(l10n.id("app.m2.start.set_target"))) action = .set_target;
        panels.text(if (command.from_explosion) tr("app.m2.start.explosion_yes") else tr("app.m2.start.explosion_no"));

        ig.igSeparatorText(l10n.t("app.m2.start.units"));
        if (ig.igBeginChild("##startcmd-units", .{ .x = 0, .y = 90 }, ig.ImGuiChildFlags_Borders, 0)) {
            for (command.units) |unit| {
                ig.igPushIDInt(unit);
                defer ig.igPopID();
                var line: [192:0]u8 = undefined;
                panels.text(say(&line, "app.m2.start.unit_row", .{ unit, unitName(state, unit) }));
                ig.igSameLine();
                if (ig.igSmallButton(l10n.id("app.m2.remove"))) action = .{ .remove_unit = unit };
            }
        }
        ig.igEndChild();
        ig.igBeginDisabled(state.editor.selection == null);
        if (ig.igButton(l10n.id("app.m2.start.add_unit"))) action = .add_unit;
        ig.igEndDisabled();
        ig.igSameLine();
        if (ig.igButton(l10n.id("kit.common.delete"))) action = .delete;
    } else {
        state.startcmd_number_for = null;
        panels.text(tr("app.m2.none_selected"));
    }
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.start.help"));
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

/// The name of the object `link_id` for a row, from the document.
fn reserveObjectName(state: *const State, link_id: i32) []const u8 {
    const object = state.editor.document.find(link_id) orelse return tr("app.m2.start.not_on_map");
    return object.nameSlice();
}

/// D-18: the MFC editor's artillery positions mode as a panel - the choice in hand (gun,
/// truck, place), Add and Clear, the map's positions (a click selects one and centres
/// the view on its place), Delete. Every control runs a named command or the tool's own
/// function (commands.zig), so BK_EDITOR_AUTO reaches it too.
pub fn drawReservePositions(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.marker.reserve_positions"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshReserve();
    const tool = &state.view.reserve_tool;
    var action: ReserveAction = .none;

    ig.igSeparatorText(l10n.t("app.m2.reserve.in_hand"));
    var line: [256:0]u8 = undefined;
    if (tool.gun) |gun| {
        panels.text(say(&line, "app.m2.reserve.gun", .{ reserveObjectName(state, gun), gun, if (tool.gun_role == .towed) tr("app.m2.reserve.towed") else tr("app.m2.reserve.self_propelled") }));
    } else panels.text(tr("app.m2.reserve.gun_click"));
    if (tool.truck) |truck| {
        panels.text(say(&line, "app.m2.reserve.truck", .{ reserveObjectName(state, truck), truck }));
    } else panels.text(if (tool.gun != null and tool.gun_role == .towed) tr("app.m2.reserve.truck_click") else tr("app.m2.reserve.truck_none"));
    if (tool.has_place) {
        panels.text(say(&line, "app.m2.reserve.place", .{ whole(tool.x), whole(tool.y) }));
    } else panels.text(tr("app.m2.reserve.place_click"));
    ig.igBeginDisabled(tool.pendingRecord() == null);
    if (ig.igButton(l10n.id("kit.common.add"))) action = .commit;
    ig.igEndDisabled();
    ig.igSameLine();
    if (ig.igButton(l10n.id("app.m2.clear"))) action = .clear;

    ig.igSeparatorText(l10n.t("app.m2.reserve.positions"));
    const rows = state.reserve_list;
    if (rows.len == 0) {
        panels.text(tr("app.m2.reserve.none"));
    } else if (ig.igBeginChild("##reserve-list", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (rows, 0..) |position, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var label: [320:0]u8 = undefined;
            const gun_name = reserveObjectName(state, position.artillery);
            const label_text = if (position.truck != 0)
                sayZ(&label, "app.m2.reserve.row_truck", .{ index, gun_name, reserveObjectName(state, position.truck), whole(position.x), whole(position.y) })
            else
                sayZ(&label, "app.m2.reserve.row_no_truck", .{ index, gun_name, whole(position.x), whole(position.y) });
            const selected = tool.selected != null and tool.selected.? == index;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (rows.len != 0) ig.igEndChild();
    ig.igBeginDisabled(tool.selected == null);
    if (ig.igButton(l10n.id("kit.common.delete"))) action = .delete;
    ig.igEndDisabled();

    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.reserve.help"));
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

/// One map tile in AI units, for the radius column.
const ai_tile: f32 = 64.0;

fn parcelKindName(kind: records.ParcelKind) []const u8 {
    return switch (kind) {
        .defence => tr("app.m2.ai.defence"),
        .reinforce => tr("app.m2.ai.reinforce"),
        else => tr("app.m2.ai.unknown_type"),
    };
}

/// D-19: the MFC AI General tab as a panel - the side (a radio for each side the map has
/// and one more, "new side", which the first edit creates with every side below it), the
/// side's mobile script IDs with Remove each, a field and Add (an ID already there is
/// skipped), and the side's parcels (type, centre, radius in map tiles, direction in
/// degrees, points) with Switch type and Delete for the selected one. Every control
/// runs a named command (commands.zig), so BK_EDITOR_AUTO's `do=` reaches it too.
pub fn drawAIGeneral(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel(l10n.id("app.m2.ai.title"), pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text(tr("app.common.no_map"));
        return;
    }
    state.refreshAi();
    const tool = &state.view.ai_tool;
    const side = state.aiActive();
    var action: AiAction = .none;

    ig.igSeparatorText(l10n.t("app.m2.ai.side"));
    var shown: usize = 0;
    while (shown <= state.ai_side_count and shown < 12) : (shown += 1) {
        ig.igPushIDInt(@intCast(shown));
        defer ig.igPopID();
        var label: [96:0]u8 = undefined;
        const label_text = if (shown < state.ai_side_count)
            sayZ(&label, "app.players.side", .{shown})
        else
            sayZ(&label, "app.m2.ai.new_side", .{shown});
        if (shown != 0) ig.igSameLine();
        if (ig.igRadioButton(label_text.ptr, tool.side == shown)) action = .{ .side = shown };
    }

    ig.igSeparatorText(l10n.t("app.m2.ai.mobile"));
    if (side.mobile_ids.len == 0) {
        panels.text(tr("app.m2.ai.no_mobile"));
    } else if (ig.igBeginChild("##ai-mobile-ids", .{ .x = 0, .y = 70 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (side.mobile_ids) |script_id| {
            ig.igPushIDInt(script_id);
            defer ig.igPopID();
            var line: [24:0]u8 = undefined;
            panels.text(std.mem.printSentinel(&line, "{d}", .{script_id}, 0) catch "?");
            ig.igSameLine();
            if (ig.igSmallButton(l10n.id("app.m2.remove"))) action = .{ .remove_mobile = script_id };
        }
    }
    if (side.mobile_ids.len != 0) ig.igEndChild();
    ig.igPushItemWidth(120);
    _ = ig.igInputIntEx(l10n.id("app.props.script_id"), &state.ai_mobile_field, 1, 10, 0);
    ig.igPopItemWidth();
    ig.igSameLine();
    if (ig.igButton(l10n.id("kit.common.add"))) action = .add_mobile;

    ig.igSeparatorText(l10n.t("app.m2.ai.parcels"));
    if (side.parcels.len == 0) {
        panels.text(tr("app.m2.ai.no_parcels"));
    } else if (ig.igBeginChild("##ai-parcels", .{ .x = 0, .y = 150 }, ig.ImGuiChildFlags_Borders, 0)) {
        for (side.parcels, 0..) |parcel, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var label: [256:0]u8 = undefined;
            var tiles: [24]u8 = undefined;
            const degrees = @as(f32, @floatFromInt(parcel.defence_dir)) * 360.0 / 65535.0;
            const label_text = sayZ(&label, "app.m2.ai.row", .{ index, parcelKindName(parcel.kind), tenths(&tiles, parcel.radius / ai_tile), whole(degrees) });
            const selected = tool.selected_parcel != null and tool.selected_parcel.? == index;
            if (ig.igSelectableEx(label_text.ptr, selected, 0, .{ .x = 0, .y = 0 })) action = .{ .choose = index };
        }
    }
    if (side.parcels.len != 0) ig.igEndChild();
    ig.igBeginDisabled(tool.selected_parcel == null);
    if (ig.igButton(l10n.id("app.m2.ai.switch"))) action = .switch_type;
    ig.igSameLine();
    if (ig.igButton(l10n.id("kit.common.delete"))) action = .delete;
    ig.igEndDisabled();
    if (tool.selected_parcel) |index| {
        ig.igPushTextWrapPos(0);
        defer ig.igPopTextWrapPos();
        var line: [256:0]u8 = undefined;
        if (index < side.parcels.len) {
            const parcel = side.parcels[index];
            panels.text(say(&line, "app.m2.ai.selected", .{ index, whole(parcel.cx), whole(parcel.cy), parcel.points.len }));
        }
        if (tool.selected_point) |point| {
            panels.text(say(&line, "app.m2.ai.point_selected", .{point}));
        } else {
            panels.text(tr("app.m2.ai.delete_parcel"));
        }
    }

    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(tr("app.m2.ai.help"));
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
