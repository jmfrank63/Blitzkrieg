//! The M3 panels (05-02 on): drawn from panels.zig's `draw` like the M1 and
//! M2 ones, each a thin ImGui layer over the named commands in commands.zig,
//! so a button and a BK_EDITOR_AUTO `do=` run the same code. This plan adds
//! the Heights panel (D-18); the later M3 plans add theirs here.
const std = @import("std");
const imgui = @import("editor_imgui");
const sdl3 = @import("sdl3");
const core = @import("editor_core");
const panels = @import("panels.zig");
const commands = @import("commands.zig");
const logic = @import("panels_logic.zig");
const view_math = @import("view_math.zig");
const tool_registry = @import("tool_registry.zig");

const ig = imgui.c;
const State = panels.State;

/// What the Heights panel says about its own gestures.
pub const heights_help = "Left-drag raises, right-drag lowers; middle-drag, Alt+drag or left+right held together levels toward the level mode's target. Ctrl keeps a height the terrain's validity rule would refuse.";

/// D-18: the MFC terrain tab (TabTerrainAltitudesDialog) as one panel - the
/// brush slider (2..16, the MFC's own range and default 3), Height Speed,
/// Level ratio %, the level-mode combo, Generate (Hills/Rocks/Dunes with
/// granularity and min/max Z, behind a confirmation popup exactly the MFC's
/// own "Do you really want to generate heights?") and Set Zero (behind its
/// own). Every control runs a named command (commands.zig), so
/// BK_EDITOR_AUTO reaches it too; the fields commit on deactivate.
pub fn drawHeightsPanel(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.heights_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Heights", &state.heights_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    const tool = &state.view.heights_tool;

    // The MFC slider's own control (SetRange(2, 16), TabTerrainAltitudesDialog.cpp:159).
    var brush_buffer: [8:0]u8 = undefined;
    var brush: c_int = @intCast(tool.brush);
    if (ig.igSliderInt("brush", &brush, 2, 16)) {
        _ = commands.run(state, "heights_brush", std.fmt.bufPrintZ(&brush_buffer, "{d}", .{brush}) catch "");
    }

    // Commit-on-deactivate fields (the Sounds panel's pattern): the edit
    // buffer is the tool's own value, so a cancelled edit changes nothing.
    var speed = tool.speed;
    _ = ig.igInputFloatEx("height speed", &speed, 0, 0, "%.2f", 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        var buffer: [16:0]u8 = undefined;
        _ = commands.run(state, "heights_speed", std.fmt.bufPrintZ(&buffer, "{d:.2}", .{speed}) catch "");
    }
    var ratio = tool.ratio_percent;
    _ = ig.igInputFloatEx("level ratio %", &ratio, 0, 0, "%.2f", 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        var buffer: [16:0]u8 = undefined;
        _ = commands.run(state, "heights_ratio", std.fmt.bufPrintZ(&buffer, "{d:.2}", .{ratio}) catch "");
    }

    ig.igSeparatorText("Level to");
    const modes = [_][]const u8{ "Zero", "Click Tile", "Instant Average", "Click Average" };
    if (ig.igBeginCombo("##level_mode", modes[@intCast(@intFromEnum(tool.level_mode))].ptr, 0)) {
        for (modes, 0..) |label, index| {
            if (ig.igSelectableEx(label.ptr, index == @intFromEnum(tool.level_mode), 0, .{ .x = 0, .y = 0 })) {
                _ = commands.run(state, "heights_mode", logic.heights_mode_names[index]);
            }
        }
        ig.igEndCombo();
    }

    ig.igSeparatorText("Generate");
    // The combo's three names in the MFC dialog's own order; the engine's own
    // type values are not 0..2 (TG_HYBRID is 3, TG_RIDGED 4), so the mapping
    // runs through the parallel arrays.
    const gen_types = [_][]const u8{ "Hills", "Rocks", "Dunes" };
    const gen_names = [_][]const u8{ "hills", "rocks", "dunes" };
    const gen_values = [_]core.bridge.HeightsGenerateType{ .hills, .rocks, .dunes };
    const gen_type: usize = switch (state.heights_generate_type) {
        .hills => 0,
        .rocks => 1,
        .dunes => 2,
    };
    if (ig.igBeginCombo("##generate_type", gen_types[gen_type].ptr, 0)) {
        for (gen_types, 0..) |label, index| {
            if (ig.igSelectableEx(label.ptr, index == gen_type, 0, .{ .x = 0, .y = 0 })) {
                state.heights_generate_type = gen_values[index];
            }
        }
        ig.igEndCombo();
    }
    _ = ig.igInputFloatEx("granularity", &state.heights_granularity, 0, 0, "%.2f", 0);
    _ = ig.igInputFloatEx("min Z", &state.heights_min_z, 0, 0, "%.2f", 0);
    _ = ig.igInputFloatEx("max Z", &state.heights_max_z, 0, 0, "%.2f", 0);
    if (ig.igButton("Generate...")) _ = ig.igOpenPopup("heights_generate_confirm", 0);
    ig.igSameLine();
    if (ig.igButton("Set Zero...")) _ = ig.igOpenPopup("heights_zero_confirm", 0);

    ig.igSeparator();
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(heights_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    drawConfirmPopups(state, &gen_names);
}

/// The MFC's own Yes/No questions (TabTerrainAltitudesDialog.cpp:325 and :372):
/// Generate and Set Zero ask before they overwrite every height on the map.
/// The named commands themselves do not ask - a script's `do=` is the answer
/// (the same split the Save As dialog and its command have).
fn drawConfirmPopups(state: *State, gen_names: []const []const u8) void {
    if (ig.igBeginPopupModal("heights_generate_confirm", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        panels.text("Do you really want to generate heights?");
        if (ig.igButton("Yes")) {
            ig.igCloseCurrentPopup();
            var buffer: [64:0]u8 = undefined;
            const text = std.fmt.bufPrintZ(&buffer, "{s}:{d:.2}:{d:.2}:{d:.2}", .{
                gen_names[heightsGenerateIndex(state.heights_generate_type)],
                state.heights_granularity,
                state.heights_min_z,
                state.heights_max_z,
            }) catch "";
            _ = commands.run(state, "heights_generate", text);
        }
        ig.igSameLine();
        if (ig.igButton("No")) ig.igCloseCurrentPopup();
        ig.igEndPopup();
    }
    if (ig.igBeginPopupModal("heights_zero_confirm", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        panels.text("Do you really want to zero all heights?");
        if (ig.igButton("Yes")) {
            ig.igCloseCurrentPopup();
            _ = commands.run(state, "heights_set_zero", "");
        }
        ig.igSameLine();
        if (ig.igButton("No")) ig.igCloseCurrentPopup();
        ig.igEndPopup();
    }
}

fn heightsGenerateIndex(which: core.bridge.HeightsGenerateType) usize {
    return switch (which) {
        .hills => 0,
        .rocks => 1,
        .dunes => 2,
    };
}

/// D-31 (PARITY O4): the Filters Composer (the MFC's CreateFilterDialog) as a
/// dockable Tools window: the filters on the left (a `*` marks the ones a
/// save would write), the selected filter's conditions on the right - each
/// line one word list, words separated by spaces, committed on deactivate
/// through `filter_words`. Add/Delete/Rename and Save are named commands,
/// so BK_EDITOR_AUTO drives the composer without ImGui text entry.
pub fn drawFiltersComposer(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.filters_composer_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Filters Composer", &state.filters_composer_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;

    // The palette's cache reads the same list; keep its generation stamp in
    // step even when the palette itself is closed this frame.
    if (state.filters_generation_seen != state.editor.filters_generation)
        state.filters_generation_seen = state.editor.filters_generation;

    const selected = std.mem.sliceTo(&state.filters_composer_selected, 0);

    _ = ig.igBeginChild("filterslist", .{ .x = 170, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    for (state.editor.filtersSlice()) |*entry| {
        const name = entry.nameSlice();
        var row: [80:0]u8 = undefined;
        const row_text = std.fmt.bufPrintZ(&row, "{s}{s}", .{ name, if (entry.user == 1) " *" else "" }) catch continue;
        if (ig.igSelectableEx(row_text.ptr, std.mem.eql(u8, name, selected), 0, .{ .x = 0, .y = 0 })) {
            selectComposerFilter(state, name);
        }
    }
    ig.igEndChild();
    ig.igSameLine();
    _ = ig.igBeginGroup();

    // The composer's controls (the MFC dialog's own buttons).
    _ = ig.igInputTextWithHint("##newfilter", "new filter's name", &state.filter_new_edit, state.filter_new_edit.len + 1, 0);
    ig.igSameLine();
    if (ig.igSmallButton("Add")) {
        _ = commands.run(state, "filter_new", std.mem.sliceTo(&state.filter_new_edit, 0));
        state.filter_new_edit = [_:0]u8{0} ** 64;
    }
    if (ig.igSmallButton("Delete")) {
        if (selected.len != 0) _ = commands.run(state, "filter_delete", selected);
    }
    ig.igSameLine();
    if (ig.igSmallButton("Save")) _ = commands.run(state, "filters_save", "");

    if (selected.len == 0) {
        ig.igEndGroup();
        return;
    }
    const filter_ptr = for (state.editor.filtersSlice()) |*entry| {
        if (std.mem.eql(u8, entry.nameSlice(), selected)) break entry;
    } else null;
    if (filter_ptr == null) {
        ig.igEndGroup();
        return;
    }
    const filter = filter_ptr.?;

    // Rename: the field starts empty; a committed edit carries both names.
    var rename_buffer: [144:0]u8 = undefined;
    _ = ig.igInputTextWithHint("##renamefilter", "rename to", &state.filter_rename_edit, state.filter_rename_edit.len + 1, 0);
    ig.igSameLine();
    if (ig.igSmallButton("Rename")) {
        const arg = std.fmt.bufPrintZ(&rename_buffer, "{s}|{s}", .{ selected, std.mem.sliceTo(&state.filter_rename_edit, 0) }) catch "";
        _ = commands.run(state, "filter_rename", arg);
        state.filter_rename_edit = [_:0]u8{0} ** 64;
    }

    ig.igSeparatorText("Conditions");
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text("One line per condition: every word must appear in the object's folder path; any one matching line passes the object.");
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();

    // The conditions' edit buffers reload when the selection or the
    // generation moves; a deactivated edit commits through filter_words.
    if (state.filters_composer_words_seen != state.editor.filters_generation) {
        reloadComposerWords(state, filter);
        state.filters_composer_words_seen = state.editor.filters_generation;
    }
    const list_count: usize = @intCast(@max(filter.list_count, 0));
    var li: usize = 0;
    while (li < list_count and li < core.bridge.filter_max_lists) : (li += 1) {
        var label: [16:0]u8 = undefined;
        const label_z = std.fmt.bufPrintZ(&label, "##cond{d}", .{li}) catch continue;
        _ = ig.igInputTextWithHint(label_z.ptr, null, &state.filter_words_edit[li], state.filter_words_edit[li].len + 1, 0);
        if (ig.igIsItemDeactivatedAfterEdit()) {
            var arg: [300]u8 = undefined;
            const arg_text = std.fmt.bufPrintZ(&arg, "{s}|{d}|{s}", .{ selected, li, std.mem.sliceTo(&state.filter_words_edit[li], 0) }) catch continue;
            _ = commands.run(state, "filter_words", arg_text);
        }
    }
    ig.igEndGroup();
}

/// Remembers the composer's selected filter and reloads its condition
/// buffers.
fn selectComposerFilter(state: *State, name: []const u8) void {
    const len = @min(name.len, state.filters_composer_selected.len - 1);
    @memcpy(state.filters_composer_selected[0..len], name[0..len]);
    state.filters_composer_selected[len] = 0;
    if (for (state.editor.filtersSlice()) |*entry| {
        if (std.mem.eql(u8, entry.nameSlice(), name)) break entry;
    } else null) |filter| {
        reloadComposerWords(state, filter);
        state.filters_composer_words_seen = state.editor.filters_generation;
    }
}

/// Fills the composer's condition buffers from the filter: the words joined
/// by single spaces (the words themselves never hold spaces - they are
/// folder-name substrings).
fn reloadComposerWords(state: *State, filter: *core.bridge.ObjectFilter) void {
    const list_count: usize = @intCast(std.math.clamp(filter.list_count, 0, core.bridge.filter_max_lists));
    for (0..core.bridge.filter_max_lists) |li| {
        state.filter_words_edit[li] = [_:0]u8{0} ** 256;
        if (li >= list_count) continue;
        var len: usize = 0;
        const buffer = &state.filter_words_edit[li];
        for (filter.lists[li].words[0..@intCast(@max(filter.lists[li].word_count, 0))]) |word| {
            const word_text = std.mem.sliceTo(&word, 0);
            if (word_text.len == 0) continue;
            if (len != 0 and len + 1 < buffer.len - 1) {
                buffer[len] = ' ';
                len += 1;
            }
            const room = @min(word_text.len, buffer.len - 1 - len);
            @memcpy(buffer[len..][0..room], word_text[0..room]);
            len += room;
        }
    }
}


/// D-21 (PARITY TR14-TR17): the MFC terrain tab (TabTerrainFieldsDialog) as
/// one panel - the field-set combo (from the storage scan, D-08, plus
/// Browse's file dialog shaped like every other), the dialog's checkboxes,
/// the Randomize fields, Apply behind the season confirmation, and Check
/// Passability as the report-only run. Every control a named command.
pub fn drawFieldsPanel(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.fields_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Fields", &state.fields_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }

    // The field-set combo: the storage scan (BkEditorListRmg), read the
    // frame the popup opens like the mods list.
    const current = std.mem.sliceTo(&state.fields_set_name, 0);
    var preview: [core.bridge.field_set_name_capacity:0]u8 = undefined;
    const preview_z = std.fmt.bufPrintZ(&preview, "{s}", .{if (current.len != 0) current else "(no field set)"}) catch "(no field set)";
    if (ig.igBeginCombo("##fieldset", preview_z.ptr, 0)) {
        var names: [64]core.bridge.RmgName = undefined;
        var total: usize = 0;
        if (state.editor.bridge.listRmg(.field_sets, &names, &total) == .ok) {
            for (names[0..@min(total, names.len)]) |*entry| {
                const name = entry.nameSlice();
                var name_buffer: [core.bridge.field_set_name_capacity:0]u8 = undefined;
                const name_z = std.fmt.bufPrintZ(&name_buffer, "{s}", .{name}) catch continue;
                if (ig.igSelectableEx(name_z.ptr, std.mem.eql(u8, name, current), 0, .{ .x = 0, .y = 0 })) {
                    var arg: [core.bridge.field_set_name_capacity:0]u8 = undefined;
                    _ = commands.run(state, "fields_set", std.fmt.bufPrintZ(&arg, "{s}", .{name}) catch "");
                }
            }
        }
        ig.igEndCombo();
    }
    // D-08: the scan replaces the MFC's file dialog - the combo IS the
    // browse, user field sets included once 05-09's user root mounts them.

    ig.igSeparatorText("Apply");
    checkboxCommand(state, "Fill terrain", "terrain", &state.fields_fill_terrain);
    checkboxCommand(state, "Place objects", "objects", &state.fields_place_objects);
    checkboxCommand(state, "Modify heights", "heights", &state.fields_modify_heights);
    checkboxCommand(state, "Update map afterwards", "update", &state.fields_update_after);
    checkboxCommand(state, "Check passability", "passability", &state.fields_check_passability);
    checkboxCommand(state, "Use the object filter", "filter", &state.fields_filter_objects);

    ig.igSeparatorText("Randomize polygon");
    checkboxCommand(state, "Randomize", "randomize", &state.fields_randomize);
    var min_length = state.fields_min_length;
    _ = ig.igInputFloatEx("min length (cells)", &min_length, 0, 0, "%.1f", 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        var buffer: [32:0]u8 = undefined;
        _ = commands.run(state, "fields_randomize", std.fmt.bufPrintZ(&buffer, "{d:.1}:{d:.2}:{d:.2}", .{ min_length, state.fields_width, state.fields_disturbance }) catch "");
    }
    var width = state.fields_width;
    _ = ig.igInputFloatEx("width (0..0.5)", &width, 0, 0, "%.2f", 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        var buffer: [32:0]u8 = undefined;
        _ = commands.run(state, "fields_randomize", std.fmt.bufPrintZ(&buffer, "{d:.1}:{d:.2}:{d:.2}", .{ state.fields_min_length, width, state.fields_disturbance }) catch "");
    }
    var disturbance = state.fields_disturbance;
    _ = ig.igInputFloatEx("disturbance (0..1)", &disturbance, 0, 0, "%.2f", 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        var buffer: [32:0]u8 = undefined;
        _ = commands.run(state, "fields_randomize", std.fmt.bufPrintZ(&buffer, "{d:.1}:{d:.2}:{d:.2}", .{ state.fields_min_length, state.fields_width, disturbance }) catch "");
    }

    ig.igSeparator();
    if (ig.igButton("Apply...")) _ = ig.igOpenPopup("fields_season_confirm", 0);
    ig.igSameLine();
    if (ig.igButton("Check Passability")) _ = commands.run(state, "fields_apply", "passability");

    // The season confirmation: the MFC's IDS_INVALID_FIELD_SEASON
    // YES/NO question. The named command refuses naming the mismatch; the
    // popup's Yes is the `fields_apply:yes` answer.
    if (ig.igBeginPopupModal("fields_season_confirm", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        panels.text("Apply the field set?");
        if (ig.igButton("Yes")) {
            ig.igCloseCurrentPopup();
            _ = commands.run(state, "fields_apply", "yes");
        }
        ig.igSameLine();
        if (ig.igButton("No")) ig.igCloseCurrentPopup();
        ig.igEndPopup();
    }

    ig.igSeparator();
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text("Draw the polygon with the Fields tool: click adds a vertex, right-click takes the last back, double-click or Enter closes, Esc keeps the last point; Insert and Delete work on a picked vertex.");
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
}

fn checkboxCommand(state: *State, label: [*:0]const u8, what: []const u8, value: *bool) void {
    var checked = value.*;
    if (ig.igCheckbox(label, &checked)) {
        var buffer: [16:0]u8 = undefined;
        _ = commands.run(state, "fields_toggle", std.fmt.bufPrintZ(&buffer, "{s}", .{what}) catch "");
        checked = value.*;
    }
}

/// What the Damage tool's panel says about its own clicks.
pub const damage_help = "Left click damages the object under the pointer by the percentage, right click heals it, middle click or Alt+click repairs it to full. A unit keeps at least 1%.";

/// The Damage tool's panel (M3, D-29/PARITY MT1): the MFC Map Tools tab's
/// damage row (IDD_TAB_TOOLS: "Damage To Add:" [edit] "%",
/// TabToolsDialog.cpp) in the left panel while the tool is in hand, as the
/// MFC tab replaced the palette. The field is a whole percentage, default
/// 10 (the dialog's nParameters[0]); it applies on every change, as the
/// MFC's ON_EN_CHANGE did, through the `damage_percent` command, held to
/// 0..100 (the bridge refuses a hit beyond the whole object).
pub fn drawDamageTool(state: *State, pos: ig.ImVec2, size: ig.ImVec2, cond: ig.ImGuiCond) void {
    const open = panels.beginPanel("Map Tools", pos, size, cond, null);
    defer panels.endPanel(open);
    if (!open) return;
    ig.igSeparatorText("Damage Tool");
    var percent: c_int = @intFromFloat(@round(state.view.damage_tool.percent));
    panels.text("Damage To Add:");
    ig.igSameLine();
    ig.igPushItemWidth(@max(ig.igGetContentRegionAvail().x - 24, 40));
    const changed = ig.igInputIntEx("##damage_percent", &percent, 1, 10, 0);
    ig.igPopItemWidth();
    ig.igSameLine();
    panels.text("%");
    if (changed) {
        const clamped = std.math.clamp(percent, 0, 100);
        var buffer: [8:0]u8 = undefined;
        _ = commands.run(state, "damage_percent", std.fmt.bufPrintZ(&buffer, "{d}", .{clamped}) catch "");
    }
    ig.igPushTextWrapPos(0);
    panels.text(damage_help);
    ig.igPopTextWrapPos();
}

/// The direction wheel (M3, D-28/PARITY O6): the MFC CDirectionButton as a
/// drawn dial in the palette - a drag over it reads the angle (the MFC's
/// atan2 with y up) through the `wheel_turn` command. The placer's placement
/// angle follows the wheel (the needle goes to the pointer, the ghost with it)
/// and the selection turns BY THE DELTA of the drag, each object keeping its
/// own angle offset (the user's ruling of 2026-10-03; the MFC set every object
/// TO the wheel's angle). Pressing on the dial only GRABS it: the needle goes
/// to the pointer and nothing turns until the pointer moves. Q/E keep their
/// own M1 behaviour beside it. `input_width` is the width the filter input
/// gave up on the shared row: the wheel hangs beside it, so the rows below
/// keep the heights the M1 reference frames were captured with.
pub fn drawDirectionWheel(state: *State, input_width: f32) void {
    const size: f32 = 40;
    const origin = ig.igGetCursorScreenPos();
    const centre = [2]f32{ origin.x + size / 2, origin.y + size / 2 };
    const draw_list = ig.igGetWindowDrawList();
    const button_w = @max(input_width, size);
    const button_h = ig.igGetFrameHeight();
    _ = ig.igInvisibleButton("##direction_wheel", .{ .x = button_w, .y = button_h }, 0);
    const active = ig.igIsItemActive();
    const hovered = ig.igIsItemHovered(ig.ImGuiHoveredFlags_None);
    ig.ImDrawList_AddCircleEx(draw_list, .{ .x = centre[0], .y = centre[1] }, size / 2, ig.igColorConvertFloat4ToU32(.{ .x = 0.45, .y = 0.45, .z = 0.45, .w = 1 }), 24, 1.5);

    // The needle: from the centre to the rim at the placer's angle, the
    // MFC's own arrow direction.
    const degrees: f32 = logic.directionToDegrees(state.view.placer.dir);
    const radians = std.math.degreesToRadians(degrees);
    const tip = [2]f32{ centre[0] + size / 2 * 0.9 * @cos(radians), centre[1] - size / 2 * 0.9 * @sin(radians) };
    ig.ImDrawList_AddLineEx(draw_list, .{ .x = centre[0], .y = centre[1] }, .{ .x = tip[0], .y = tip[1] }, ig.igColorConvertFloat4ToU32(.{ .x = 1, .y = 0.9, .z = 0.2, .w = 1 }), 2.5);

    // One drag over the dial is one gesture: its frames' turns of the
    // selection merge into ONE undo step (`wheel_turn` reads the gesture). The
    // press grabs the dial where the pointer is - the needle (the placer's
    // angle) goes there, the selection stays - and the turn counted from then.
    if (ig.igIsItemActivated()) {
        state.wheel_gesture = state.editor.beginGesture();
        state.wheel_turned = 0;
        const mouse = ig.igGetIO().*.MousePos;
        state.view.placer.dir = logic.degreesToDirection(@floatFromInt(logic.wheelAngleDegrees(centre, .{ mouse.x, mouse.y })));
    }
    if (active and ig.igIsMouseDown(0)) {
        const mouse = ig.igGetIO().*.MousePos;
        const angle = logic.wheelAngleDegrees(centre, .{ mouse.x, mouse.y });
        if (angle != logic.wheelDegreesOfDirection(state.view.placer.dir)) {
            var buffer: [16:0]u8 = undefined;
            _ = commands.run(state, "wheel_turn", std.fmt.bufPrintZ(&buffer, "{d}", .{angle}) catch "");
        }
    }
    if (!active) state.wheel_gesture = 0;
    if (hovered) {
        ig.igSetTooltip("Direction: drag to set the placement angle; turns the selection by the drag");
    }
}

/// The catalogue's game type for an object name, or null when the database
/// does not list it. The palette's own cache answers; the panel never reads
/// the bridge for it.
fn gameTypeOf(state: *State, name: []const u8) ?i32 {
    for (state.catalogue) |*entry| {
        if (std.mem.eql(u8, std.mem.sliceTo(&entry.name, 0), name)) return entry.game_type;
    }
    return null;
}

/// The properties' reload: the buffers start from the object as it is now.
fn reloadProps(state: *State) void {
    const editor = state.editor;
    const object = if (editor.selection) |link_id| editor.document.find(link_id) else null;
    state.props_link_id = if (object) |o| o.link_id else -1;
    state.props_script_edit = [_:0]u8{0} ** 16;
    if (object) |o| {
        var buffer: [16]u8 = undefined;
        const text = std.fmt.bufPrint(&buffer, "{d}", .{o.script_id}) catch "";
        const len = @min(text.len, state.props_script_edit.len - 1);
        @memcpy(state.props_script_edit[0..len], text[0..len]);
        state.props_health = o.hp * 100.0;
        state.props_angle = logic.directionToDegrees(o.dir);
        state.props_formation = @intCast(@max(o.frame_index, 0));
    } else {
        state.props_health = 100;
        state.props_angle = 0;
        state.props_formation = 0;
    }
    state.props_reload = false;
}

/// The Properties window (M3, D-26/PARITY O15..O21): the MFC
/// CPropertieDialog as one dockable window. Per kind the SEditorMApObject
/// fields - a building garrisons (units, Script ID, player, health), a
/// trench piece takes units and a Script ID, a unit or a squad the full set
/// (units, angle, Script ID, scenario unit, player with the flag swap,
/// health, the squad its formation) - and for a multi-selection the fields
/// every member takes (angle, Script ID, player), the MFC's own
/// multi-unit set minus its dead Behaviour combo (the manipulator is
/// commented out there, SEditorMApObject.cpp:655-860). Every field commits
/// on deactivation as one undo step, through the `props_set` command.
pub fn drawPropertiesPanel(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.properties_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    // NOTE (05-11): the docked Properties panel (panels.zig) is also called
    // "Properties", and two windows of one name are one window to ImGui - this
    // window's fields are appended to the docked panel. Renaming it moves them
    // into a floating window over the map, which the M2 and M3 scenarios' map
    // clicks then hit; recorded in deferred-items.md, not changed here.
    const open = ig.igBegin("Properties", &state.properties_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    const editor = state.editor;
    if (!panels.mapIsOpen(editor)) {
        panels.text("no map open");
        return;
    }
    // The buffers reload when the selection moved or an edit re-read the
    // objects (a flag's swap renames the record).
    const anchor_changed = state.props_link_id != (editor.selection orelse -1);
    if (state.props_reload or anchor_changed) reloadProps(state);

    const count = editor.selectionCount();
    if (count == 0) {
        panels.text("Name: no selected");
        return;
    }

    if (count > 1) {
        var header: [48:0]u8 = undefined;
        if (std.fmt.bufPrintZ(&header, "{d} objects selected", .{count})) |text| {
            ig.igSeparatorText(text.ptr);
        } else |_| {}
        drawMultiFields(state);
        return;
    }

    const link_id = editor.selection.?;
    const object = editor.document.find(link_id) orelse {
        panels.text("Name: no selected");
        return;
    };
    const kind: logic.PropKind = if (gameTypeOf(state, object.nameSlice())) |game_type| logic.propertyKind(game_type) else .other;
    ig.igSeparatorText(object.nameSlice().ptr);
    if (kind == .other) {
        panels.text("this kind has no properties");
        return;
    }

    if (kind == .building or kind == .trench or kind == .unit or kind == .squad)
        drawUnitsList(state, link_id);

    // Script ID: the M2 field's own merge-per-typed-value path.
    _ = ig.igInputText("Script ID", &state.props_script_edit, state.props_script_edit.len, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        const text = std.mem.sliceTo(&state.props_script_edit, 0);
        _ = commands.run(state, "script_id", text);
        state.props_reload = true;
    }

    if (kind == .building or kind == .unit or kind == .squad)
        drawPlayerCombo(state, link_id, object.player);

    if (kind == .building or kind == .unit or kind == .squad) {
        var health = state.props_health;
        _ = ig.igInputFloatEx("Health %", &health, 0, 0, "%.1f", 0);
        if (ig.igIsItemDeactivatedAfterEdit()) {
            var buffer: [24:0]u8 = undefined;
            _ = commands.run(state, "props_set", std.fmt.bufPrintZ(&buffer, "health={d:.2}", .{logic.clampHealthPercent(health)}) catch "");
            state.props_reload = true;
        }
    }

    if (kind == .unit or kind == .squad) {
        var angle = state.props_angle;
        _ = ig.igInputFloatEx("Angle (degrees)", &angle, 0, 0, "%.1f", 0);
        if (ig.igIsItemDeactivatedAfterEdit()) {
            var buffer: [24:0]u8 = undefined;
            _ = commands.run(state, "props_set", std.fmt.bufPrintZ(&buffer, "angle={d:.1}", .{angle}) catch "");
            state.props_reload = true;
        }
        // O18: the scenario unit, drawn blue. The MFC's combo is editor
        // session state (a specular on the visual, never saved); the map's
        // own answer is what the record is - scenarioObjects or objects.
        panels.text(if (object.scenario) "Scenario unit: TRUE" else "Scenario unit: FALSE");
    }

    if (kind == .squad) {
        const formation: usize = @intCast(@max(object.frame_index, 0));
        const label = if (formation < logic.formation_labels.len) logic.formation_labels[formation] else "FORMATION?";
        if (ig.igBeginCombo("##formation", label.ptr, 0)) {
            for (logic.formation_labels, 0..) |name, index| {
                if (ig.igSelectableEx(name.ptr, index == formation, 0, .{ .x = 0, .y = 0 })) {
                    var buffer: [24:0]u8 = undefined;
                    _ = commands.run(state, "props_set", std.fmt.bufPrintZ(&buffer, "formation={d}", .{index}) catch "");
                    state.props_reload = true;
                }
            }
            ig.igEndCombo();
        }
    }
}

/// The units list (PARITY O17..O19): the passengers - the records whose
/// nLinkWith names this object - read-only, one per row, and double-click
/// unlinks the row (the MFC's own DblClick-for-unlink,
/// TemplateEditorFrame1.cpp:3678).
fn drawUnitsList(state: *State, link_id: i32) void {
    const editor = state.editor;
    ig.igSeparatorText("Units");
    var shown: usize = 0;
    for (editor.document.objects.items) |*passenger| {
        if (passenger.link_with != link_id or passenger.link_id == link_id) continue;
        shown += 1;
        var row: [96:0]u8 = undefined;
        const row_text = std.fmt.bufPrintZ(&row, "{s}##u{d}", .{ passenger.nameSlice(), passenger.link_id }) catch continue;
        _ = ig.igSelectableEx(row_text.ptr, false, 0, .{ .x = 0, .y = 0 });
        if (ig.igIsItemHovered(ig.ImGuiHoveredFlags_None) and ig.igIsMouseDoubleClicked(0)) {
            var buffer: [24:0]u8 = undefined;
            _ = commands.run(state, "link_unlink", std.fmt.bufPrintZ(&buffer, "{d}", .{passenger.link_id}) catch "");
            state.props_reload = true;
        }
    }
    if (shown == 0) panels.text("(none)");
}

/// The Player combo over the map's diplomacies (the MFC's own combo range),
/// committing through the flag-swap rule on a flag.
fn drawPlayerCombo(state: *State, link_id: i32, current_player: i32) void {
    _ = link_id;
    var buffer: [16:0]u8 = undefined;
    const current_z = std.fmt.bufPrintZ(&buffer, "{d}", .{current_player}) catch "0";
    if (ig.igBeginCombo("Player", current_z.ptr, 0)) {
        var player: usize = 0;
        while (player < state.editor.document.diplomacy.items.len) : (player += 1) {
            var label: [16:0]u8 = undefined;
            const label_z = std.fmt.bufPrintZ(&label, "{d}", .{player}) catch continue;
            if (ig.igSelectableEx(label_z.ptr, @as(i32, @intCast(player)) == current_player, 0, .{ .x = 0, .y = 0 })) {
                var arg: [24:0]u8 = undefined;
                _ = commands.run(state, "props_set", std.fmt.bufPrintZ(&arg, "player={d}", .{player}) catch "");
                state.props_reload = true;
            }
        }
        ig.igEndCombo();
    }
}

/// The multi-selection's fields (PARITY O20): angle, Script ID and player -
/// the fields every member's record carries - committed to every member as
/// ONE undo step. The MFC's Behaviour combo is its commented-out
/// multi-unit manipulator (SEditorMApObject.cpp:655-860); the reinforcement
/// group it displayed is the Group Manager's own data (M2).
fn drawMultiFields(state: *State) void {
    const editor = state.editor;
    var angle = state.props_angle;
    _ = ig.igInputFloatEx("Angle (degrees)", &angle, 0, 0, "%.1f", 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        var buffer: [24:0]u8 = undefined;
        _ = commands.run(state, "props_set", std.fmt.bufPrintZ(&buffer, "angle={d:.1}", .{angle}) catch "");
        state.props_reload = true;
    }
    _ = ig.igInputText("Script ID", &state.props_script_edit, state.props_script_edit.len, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        const text = std.mem.sliceTo(&state.props_script_edit, 0);
        _ = commands.run(state, "script_id", text);
        state.props_reload = true;
    }
    // The player of the multi-selection: the anchor's combo, applied to all.
    const anchor = editor.selection.?;
    const current = editor.document.find(anchor).?.player;
    var buffer: [16:0]u8 = undefined;
    const current_z = std.fmt.bufPrintZ(&buffer, "{d}", .{current}) catch "0";
    if (ig.igBeginCombo("Player", current_z.ptr, 0)) {
        var player: usize = 0;
        while (player < editor.document.diplomacy.items.len) : (player += 1) {
            var label: [16:0]u8 = undefined;
            const label_z = std.fmt.bufPrintZ(&label, "{d}", .{player}) catch continue;
            if (ig.igSelectableEx(label_z.ptr, @as(i32, @intCast(player)) == current, 0, .{ .x = 0, .y = 0 })) {
                var arg: [24:0]u8 = undefined;
                _ = commands.run(state, "props_set", std.fmt.bufPrintZ(&arg, "player={d}", .{player}) catch "");
                state.props_reload = true;
            }
        }
        ig.igEndCombo();
    }
}

// ---------------------------------------------------------------------------
// The Unit Creation Info window (05-05, D-30)
// ---------------------------------------------------------------------------

/// What the points list says about its units: the MFC's Appear Points dialog shows
/// a point's place in tiles (the file holds map units, 64 to a tile).
pub const appear_points_help = "Appear points are the places the game's aircraft and paratroopers come in from. They are listed in tiles; Add at view centre puts one where the view looks.";

/// The map units to a tile in the points list (PEPointsListDialog.cpp:180).
const appear_units_per_tile: f32 = 64.0;

fn ucChoiceKind(index: usize) core.bridge.UcChoice {
    return switch (index) {
        0 => .parties,
        1 => .aircraft,
        else => .squads,
    };
}

/// Reads the three combo lists through the bridge, two passes each; a list that
/// will not read stays empty (the combos then show only the current value).
fn loadUcLists(state: *State) void {
    for (&state.uc_lists, 0..) |*list, index| {
        state.allocator.free(list.*);
        list.* = &.{};
        var none: [0]core.bridge.UcName = .{};
        var total: usize = 0;
        const sizing = state.editor.bridge.unitCreationChoices(ucChoiceKind(index), &none, &total);
        if ((sizing != .ok and sizing != .refused) or total == 0) continue;
        const names = state.allocator.alloc(core.bridge.UcName, total) catch continue;
        var got: usize = 0;
        if (state.editor.bridge.unitCreationChoices(ucChoiceKind(index), names, &got) != .ok or got != total) {
            state.allocator.free(names);
            continue;
        }
        list.* = names;
    }
}

/// A combo over `list` showing `current`; true with the chosen name written to `chosen`.
fn drawNameCombo(label: [*:0]const u8, current: []const u8, list: []const core.bridge.UcName, chosen: *[core.records.uc_name_capacity]u8) bool {
    var preview: [core.records.uc_name_capacity + 1:0]u8 = undefined;
    const shown = std.fmt.bufPrintZ(&preview, "{s}", .{current}) catch "";
    var picked = false;
    if (ig.igBeginCombo(label, shown.ptr, 0)) {
        for (list) |*item| {
            const name = item.nameSlice();
            if (ig.igSelectableEx(@ptrCast(&item.name), std.mem.eql(u8, name, current), 0, .{ .x = 0, .y = 0 })) {
                @memset(chosen, 0);
                @memcpy(chosen[0..name.len], name);
                picked = true;
            }
        }
        ig.igEndCombo();
    }
    return picked;
}

/// Runs `unit_creation_set` with `<field>=<value>`.
fn setField(state: *State, field: []const u8, value: []const u8) void {
    var buffer: [128:0]u8 = undefined;
    _ = commands.run(state, "unit_creation_set", std.fmt.bufPrintZ(&buffer, "{s}={s}", .{ field, value }) catch return);
}

/// An integer field committed on deactivation (the Sounds panel's pattern):
/// the buffer is the cached value, so a cancelled edit changes nothing.
fn drawIntField(state: *State, label: [*:0]const u8, field: []const u8, current: i32) void {
    var value: c_int = current;
    _ = ig.igInputIntEx(label, &value, 0, 0, 0);
    if (ig.igIsItemDeactivatedAfterEdit() and value != current) {
        var buffer: [16:0]u8 = undefined;
        setField(state, field, std.fmt.bufPrintZ(&buffer, "{d}", .{value}) catch return);
    }
}

/// The MFC's Map Unit Creation Property as one window: the player, the party,
/// the five aviation slots (aircraft name, formation size, count), the paratroop
/// squad with its count, the relax time and the appear points. Every control
/// runs a named command (`unit_creation_set`, `appear_point_*`), so a button
/// and a BK_EDITOR_AUTO `do=` share one path and one undo step; the bridge's
/// MutableValidate-style rules refuse a bad value naming the field.
pub fn drawUnitCreationPanel(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.uc_open) {
        state.uc_was_open = false;
        return;
    }
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Unit Creation Info", &state.uc_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    const editor = state.editor;
    if (!panels.mapIsOpen(editor)) {
        panels.text("no map open");
        return;
    }
    if (!state.uc_was_open) {
        loadUcLists(state);
        state.uc_was_open = true;
        state.uc_cache_player = null;
    }
    const entries = editor.document.diplomacy.items.len;
    const players = if (entries > 0) entries - 1 else 0;
    if (players == 0) {
        panels.text("the map has no players");
        return;
    }
    if (state.uc_player >= players) state.uc_player = players - 1;

    var player_label: [16:0]u8 = undefined;
    const player_text = std.fmt.bufPrintZ(&player_label, "player {d}", .{state.uc_player}) catch "player";
    if (ig.igBeginCombo("Player", player_text.ptr, 0)) {
        var index: usize = 0;
        while (index < players) : (index += 1) {
            var label: [16:0]u8 = undefined;
            const label_text = std.fmt.bufPrintZ(&label, "player {d}", .{index}) catch continue;
            if (ig.igSelectableEx(label_text.ptr, index == state.uc_player, 0, .{ .x = 0, .y = 0 })) state.uc_player = index;
        }
        ig.igEndCombo();
    }

    // The player's record, read again when the player changes or an edit, undo or
    // redo moved the editor's unit-creation generation.
    const generation = editor.record_generations.get(.unit_creation);
    if (state.uc_cache_player == null or state.uc_cache_player.? != state.uc_player or state.uc_cache_generation != generation) {
        state.uc_cache_player = state.uc_player;
        state.uc_cache_generation = generation;
        if (editor.unitCreation(state.uc_player)) |unit| {
            state.uc_cache = unit;
            state.uc_cache_ok = true;
        } else |_| {
            state.uc_cache_ok = false;
        }
    }
    if (!state.uc_cache_ok) {
        ig.igPushTextWrapPos(0);
        panels.text(if (editor.status().len > 0) editor.status() else "this player's unit creation cannot be shown");
        ig.igPopTextWrapPos();
        return;
    }
    const unit = state.uc_cache;
    var chosen: [core.records.uc_name_capacity]u8 = undefined;

    if (drawNameCombo("Party", unit.partySlice(), state.uc_lists[0], &chosen)) setField(state, "party", std.mem.sliceTo(&chosen, 0));

    ig.igSeparatorText("Aviation");
    for (unit.aircraft, 0..) |slot, index| {
        ig.igPushIDInt(@intCast(index));
        defer ig.igPopID();
        panels.text(core.records.uc_aircraft_labels[index]);
        var name_field: [24]u8 = undefined;
        ig.igSetNextItemWidth(200);
        if (drawNameCombo("##aircraft", slot.nameSlice(), state.uc_lists[1], &chosen))
            setField(state, std.fmt.bufPrint(&name_field, "aircraft{d}_name", .{index}) catch "", std.mem.sliceTo(&chosen, 0));
        ig.igSameLine();
        ig.igSetNextItemWidth(80);
        var formation_field: [28:0]u8 = undefined;
        drawIntField(state, "formation##f", std.fmt.bufPrintZ(&formation_field, "aircraft{d}_formation", .{index}) catch "", slot.formation_size);
        ig.igSameLine();
        ig.igSetNextItemWidth(80);
        var count_field: [24:0]u8 = undefined;
        drawIntField(state, "count##c", std.fmt.bufPrintZ(&count_field, "aircraft{d}_count", .{index}) catch "", slot.count);
    }

    ig.igSeparatorText("Paratroopers");
    if (drawNameCombo("Squad", unit.paratroopSlice(), state.uc_lists[2], &chosen)) setField(state, "paratroop_name", std.mem.sliceTo(&chosen, 0));
    drawIntField(state, "Squads count", "paratroop_count", unit.paratroop_count);
    drawIntField(state, "Relax time (s)", "relax", unit.relax_time);

    ig.igSeparatorText("Appear points");
    var remove: ?usize = null;
    for (unit.appearSlice(), 0..) |point, index| {
        ig.igPushIDInt(@intCast(index));
        defer ig.igPopID();
        var tile_x = point.x / appear_units_per_tile;
        var tile_y = point.y / appear_units_per_tile;
        ig.igSetNextItemWidth(90);
        _ = ig.igInputFloatEx("x##p", &tile_x, 0, 0, "%.2f", 0);
        const x_done = ig.igIsItemDeactivatedAfterEdit();
        ig.igSameLine();
        ig.igSetNextItemWidth(90);
        _ = ig.igInputFloatEx("y##p", &tile_y, 0, 0, "%.2f", 0);
        const y_done = ig.igIsItemDeactivatedAfterEdit();
        ig.igSameLine();
        if (ig.igButton("Remove")) remove = index;
        if (x_done or y_done) {
            var arg: [64:0]u8 = undefined;
            _ = commands.run(state, "appear_point_set", std.fmt.bufPrintZ(&arg, "{d}/{d:.1}/{d:.1}", .{ index, tile_x * appear_units_per_tile, tile_y * appear_units_per_tile }) catch "");
        }
    }
    if (unit.appear_count == 0) panels.text("no appear points");
    if (ig.igButton("Add at view centre")) _ = commands.run(state, "appear_point_here", "");
    if (remove) |index| {
        var arg: [16:0]u8 = undefined;
        _ = commands.run(state, "appear_point_remove", std.fmt.bufPrintZ(&arg, "{d}", .{index}) catch "");
    }
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(appear_points_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
}

// ---------------------------------------------------------------------------
// The Check Map window (05-05, D-33)
// ---------------------------------------------------------------------------

/// What the window says about Fix all.
pub const check_help = "Check Map reports what is wrong; it changes nothing. Click a finding to go to it. Fix all makes every fix as one undo step; removing an object the database does not know, or a road with fewer than two control points, asks first. Save never fixes anything: it only says here when the checks find something.";

/// The MFC's own wording where it had one.
fn kindLabel(kind: core.checks.Kind) [:0]const u8 {
    return kind.heading();
}

/// The window: Run check, Fix all (with the confirmation when a fix would remove
/// something), the findings grouped by kind - a click goes to the finding. Every
/// button runs a named command (check_map, check_map_fix_all, check_jump).
pub fn drawCheckMapPanel(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.check_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Check Map", &state.check_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.mapIsOpen(state.editor)) {
        panels.text("no map open");
        return;
    }
    if (ig.igButton("Run check")) _ = commands.run(state, "check_map", "");
    ig.igSameLine();
    const findings = state.check_findings;
    var destructive: usize = 0;
    for (findings) |finding| {
        if (finding.needsConfirmation()) destructive += 1;
    }
    ig.igBeginDisabled(findings.len == 0);
    if (ig.igButton("Fix all")) {
        if (destructive == 0) {
            _ = commands.run(state, "check_map_fix_all", "");
        } else {
            state.check_confirm_pending = true;
            _ = ig.igOpenPopup("Fix all##check", 0);
        }
    }
    ig.igEndDisabled();

    // The confirmation: what removes something is listed and asked for.
    if (ig.igBeginPopupModal("Fix all##check", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        const unknown = core.checks.count(findings, .unknown_object_type);
        const short = core.checks.count(findings, .short_vso);
        var line: [160:0]u8 = undefined;
        ig.igText("Fix all will also remove:");
        if (unknown > 0) ig.igText(std.fmt.bufPrintZ(&line, "  {d} object(s) whose type the object database does not know", .{unknown}) catch "  unknown objects");
        if (short > 0) ig.igText(std.fmt.bufPrintZ(&line, "  {d} road(s) or river(s) with fewer than two control points", .{short}) catch "  short roads");
        ig.igText("It is one undo step.");
        if (ig.igButton("Fix all and remove them")) {
            _ = commands.run(state, "check_map_fix_all", "remove");
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton("Fix all but leave them")) {
            _ = commands.run(state, "check_map_fix_all", "");
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton("Cancel")) {
            state.check_confirm_pending = false;
            ig.igCloseCurrentPopup();
        }
        ig.igEndPopup();
    }

    if (state.check_fix_report) |report| {
        var line: [128:0]u8 = undefined;
        panels.text(std.fmt.bufPrintZ(&line, "last Fix all: fixed {d}, left {d}, refused {d}", .{ report.fixed, report.left, report.refused }) catch "last Fix all ran");
    }
    ig.igSeparator();
    if (findings.len == 0) {
        panels.text("no problems found");
    } else {
        var current: ?core.checks.Kind = null;
        for (findings, 0..) |*finding, index| {
            if (current == null or current.? != finding.kind) {
                ig.igSeparatorText(kindLabel(finding.kind).ptr);
                current = finding.kind;
            }
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            var label: [core.checks.detail_capacity + 1:0]u8 = undefined;
            const label_text = std.fmt.bufPrintZ(&label, "{s}", .{finding.text()}) catch continue;
            if (ig.igSelectableEx(label_text.ptr, false, 0, .{ .x = 0, .y = 0 })) {
                var arg: [16:0]u8 = undefined;
                _ = commands.run(state, "check_jump", std.fmt.bufPrintZ(&arg, "{d}", .{index}) catch "");
            }
        }
    }
    ig.igSeparator();
    ig.igPushTextWrapPos(0);
    ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, ig.igGetStyleColorVec4(ig.ImGuiCol_TextDisabled).*);
    panels.text(check_help);
    ig.igPopStyleColor();
    ig.igPopTextWrapPos();
}

/// The Layers menu (D-32, 05-06): one check per MFC toggle, in the MFC's own
/// order, greyed with no map open or - with the renderer's own finding as its
/// tip - for a layer the renderer cannot draw; then the Unit Fire Ranges
/// submenu (Off / Selected units / one entry per object filter, the D-31
/// filters the palette's combo lists). Every entry runs a named command, so a
/// click and a BK_EDITOR_AUTO `do=` are the same code.
pub fn drawLayersMenu(state: *State, map_open: bool) void {
    const items = logic.layerMenuItems(&state.editor.layers, state.editor.layers_mask, map_open);
    for (items) |item| {
        var on = item.checked;
        if (ig.igMenuItemBoolPtr(item.label.ptr, null, &on, item.enabled)) {
            _ = commands.layerToggle(state, core.layers.commandName(item.layer));
        }
        if (ig.igIsItemHovered(ig.ImGuiHoveredFlags_AllowWhenDisabled) and ig.igBeginTooltip()) {
            panels.text(item.tooltip);
            ig.igEndTooltip();
        }
        // The MFC's menu grouped the scene toggles apart from the wire frame and the passability.
        if (item.layer == .depth_complexity or item.layer == .war_fog) ig.igSeparator();
    }
    ig.igSeparator();
    var title_buffer: [128:0]u8 = undefined;
    const title = logic.fireRangeTitle(&title_buffer, &state.editor.layers);
    var title_z: [160:0]u8 = undefined;
    const title_text = std.fmt.bufPrintZ(&title_z, "{s}###fire_ranges", .{title}) catch "Unit Fire Ranges###fire_ranges";
    if (ig.igBeginMenu(title_text.ptr)) {
        defer ig.igEndMenu();
        const fire_mode = state.editor.layers.fire_mode;
        if (ig.igMenuItemEx(logic.fireModeLabel(.off).ptr, null, fire_mode == .off, map_open)) _ = commands.fireRange(state, "off");
        if (ig.igMenuItemEx(logic.fireModeLabel(.selected).ptr, null, fire_mode == .selected, map_open)) _ = commands.fireRange(state, "selected");
        if (ig.igIsItemHovered(ig.ImGuiHoveredFlags_AllowWhenDisabled) and ig.igBeginTooltip()) {
            panels.text("The ranges of the units selected now; they follow the selection.");
            ig.igEndTooltip();
        }
        ig.igSeparator();
        ig.igTextDisabled("Filter:");
        for (state.editor.filtersSlice()) |*filter| {
            const name = filter.nameSlice();
            var name_buffer: [96:0]u8 = undefined;
            const name_z = std.fmt.bufPrintZ(&name_buffer, "{s}", .{name}) catch continue;
            const ticked = fire_mode == .filter and std.mem.eql(u8, state.editor.layers.fireFilter(), name);
            if (ig.igMenuItemEx(name_z.ptr, null, ticked, map_open)) _ = commands.fireRangeFilter(state, name);
        }
    }
}


// ---------------------------------------------------------------------------
// Create Random Map (05-08, D-01..D-05)
// ---------------------------------------------------------------------------

/// Which edit text a Create Random Map combo fills.
const RmgField = enum { template, context, setting };

/// A combo of the dialog's names: `current` as the preview (`blank_label`
/// when empty), one selectable per name, and - when `any_label` is given - an
/// extra first entry that clears the field (the setting's `<any setting>`).
fn rmgCombo(
    state: *State,
    label: [*:0]const u8,
    names: []const core.bridge.RmgName,
    field: *logic.NameText,
    which: RmgField,
    blank_label: [:0]const u8,
) void {
    var preview: [core.bridge.field_set_name_capacity + 16:0]u8 = undefined;
    const shown = if (field.len == 0) blank_label else std.fmt.bufPrintZ(&preview, "{s}", .{field.slice()}) catch blank_label;
    if (ig.igBeginCombo(label, shown.ptr, 0)) {
        if (which == .setting and ig.igSelectableEx("<any setting>", field.len == 0, 0, .{ .x = 0, .y = 0 })) field.set("");
        for (names) |*entry| {
            const name = entry.nameSlice();
            var name_buffer: [core.bridge.field_set_name_capacity:0]u8 = undefined;
            const name_z = std.fmt.bufPrintZ(&name_buffer, "{s}", .{name}) catch continue;
            if (ig.igSelectableEx(name_z.ptr, std.mem.eql(u8, name, field.slice()), 0, .{ .x = 0, .y = 0 })) {
                field.set(name);
                if (which == .template) panels.refreshRmgGraphCount(state);
            }
        }
        ig.igEndCombo();
    }
}

/// File > Create Random Map (D-01): the MFC dialog's fields - the template
/// and context combos with Browse, the graph index, the setting with
/// `<any setting>`, the direction N/E/S/W, the difficulty level, Save as BZM,
/// Write DDS, the map name - plus the seed this port adds (blank draws one).
/// OK waits for a template, a context and a name (the MFC's own rule), then
/// hands the fields to the progress modal.
pub fn drawRandomMapDialog(state: *State) void {
    if (!state.rmg_open) return;
    if (!ig.igBegin("Create Random Map", &state.rmg_open, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        ig.igEnd();
        return;
    }
    defer ig.igEnd();
    const fields = &state.rmg_fields;

    rmgCombo(state, "##rmg_template", state.rmg_templates.items, &fields.template, .template, "(choose a template)");
    ig.igSameLine();
    if (ig.igSmallButton("Browse##template")) panels.browseRmg(state, 1);
    ig.igSameLine();
    panels.text("Template");

    rmgCombo(state, "##rmg_context", state.rmg_contexts.items, &fields.context, .context, "(choose a context)");
    ig.igSameLine();
    if (ig.igSmallButton("Browse##context")) panels.browseRmg(state, 2);
    ig.igSameLine();
    panels.text("Context");

    var graph: c_int = fields.graph;
    if (ig.igInputIntEx("Graph", &graph, 1, 1, 0)) fields.graph = if (graph < -1) -1 else graph;
    if (state.rmg_graph_count > 0) {
        var hint: [96]u8 = undefined;
        panels.text(std.fmt.bufPrint(&hint, "-1 lets the template's weights pick; 0 to {d} for this template", .{state.rmg_graph_count - 1}) catch "-1 lets the template's weights pick");
    } else {
        panels.text("-1 lets the template's weights pick");
    }

    rmgCombo(state, "Setting", state.rmg_settings.items, &fields.setting, .setting, "<any setting>");

    ig.igText("Direction");
    var angle: c_int = fields.angle;
    inline for (logic.RmgFields.direction_names, 0..) |name, index| {
        ig.igSameLine();
        _ = ig.igRadioButtonIntPtr(name ++ "##dir", &angle, index);
    }
    fields.angle = angle;
    ig.igText("Level");
    var level: c_int = fields.level;
    inline for ([_][:0]const u8{ "1", "2", "3" }, 0..) |name, index| {
        ig.igSameLine();
        _ = ig.igRadioButtonIntPtr(name ++ "##level", &level, index);
    }
    fields.level = level;

    _ = ig.igCheckbox("Save as BZM", &fields.save_as_bzm);
    ig.igSameLine();
    _ = ig.igCheckbox("Write DDS", &fields.write_dds);

    _ = ig.igInputTextWithHint("Map name", "name", &state.rmg_map_edit, state.rmg_map_edit.len + 1, 0);
    fields.map_name.set(std.mem.sliceTo(&state.rmg_map_edit, 0));
    _ = ig.igInputTextWithHint("Seed", "blank = random", &state.rmg_seed_edit, state.rmg_seed_edit.len + 1, ig.ImGuiInputTextFlags_CharsDecimal);
    fields.seed.set(std.mem.sliceTo(&state.rmg_seed_edit, 0));
    _ = ig.igCheckbox("Replace a map of that name", &fields.overwrite);
    ig.igTextDisabled("The map goes to your maps folder (or the active mod's); the seed used is shown afterwards.");

    if (state.rmg_message_len != 0) {
        ig.igSpacing();
        panels.text(state.rmg_message[0..state.rmg_message_len]);
    }

    ig.igSpacing();
    ig.igBeginDisabled(!fields.okEnabled());
    const ok = ig.igButton("OK");
    ig.igEndDisabled();
    ig.igSameLine();
    const cancel = ig.igButton("Cancel");
    if (cancel) state.rmg_open = false;
    if (ok) _ = panels.startRmgGeneration(state);
}

/// The generation's modal (D-03): announced at 0 of the generator's 19 steps
/// for two frames (a new auto-sized window is hidden for its first) so the window shows it, then the generator runs on the main
/// thread with the window frozen - the callback only counts, and there is
/// no cancel (the MFC editor has none) - then the result: the seed to ask
/// for the same map again, with Open map (the generated file as a normal
/// document, D-02) and Close. A refusal reopens the dialog with its reason.
pub fn drawRmgProgress(state: *State) void {
    if (state.rmg_phase == .idle) return;
    // The frame before showed the modal at 0 of 19; this call blocks. A
    // refusal closes the modal below and reopens the dialog with the reason.
    var refused = false;
    if (state.rmg_phase == .announce and state.rmg_announce_left == 0) state.rmg_phase = .run;
    if (state.rmg_phase == .run) {
        if (commands.rmgRun(state, state.rmg_params) == .ok) {
            state.rmg_phase = .done;
        } else {
            const status = state.editor.status();
            const len = @min(status.len, state.rmg_message.len);
            @memcpy(state.rmg_message[0..len], status[0..len]);
            state.rmg_message_len = len;
            refused = true;
        }
    }
    if (!state.rmg_popup_opened) {
        _ = ig.igOpenPopup("Creating random map", 0);
        state.rmg_popup_opened = true;
    }
    if (!ig.igBeginPopupModal("Creating random map", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
    defer ig.igEndPopup();
    if (refused) {
        state.rmg_phase = .idle;
        state.rmg_popup_opened = false;
        state.rmg_open = true;
        ig.igCloseCurrentPopup();
        return;
    }
    const total: i32 = if (state.rmg_progress.total > 0) state.rmg_progress.total else 19;
    var overlay: [32:0]u8 = undefined;
    const overlay_z = std.fmt.bufPrintZ(&overlay, "{d} of {d}", .{ state.rmg_progress.steps, total }) catch "";
    ig.igProgressBar(@as(f32, @floatFromInt(state.rmg_progress.steps)) / @as(f32, @floatFromInt(total)), .{ .x = 320, .y = 0 }, overlay_z.ptr);
    switch (state.rmg_phase) {
        .announce => {
            panels.text("Creating the random map - the window waits until it is done.");
            if (state.rmg_announce_left > 0) state.rmg_announce_left -= 1;
        },
        .run, .idle => {},
        .done => {
            // Wrapped: a map's path can be longer than the window is wide.
            ig.igPushTextWrapPos(520);
            var raw: [256]u8 = undefined;
            var line: [256:0]u8 = undefined;
            const said = std.fmt.bufPrintZ(&line, "{s}", .{logic.rmgResultLine(&raw, state.rmg_made_name.slice(), &state.rmg_result)}) catch "";
            ig.igTextWrapped("%s", said.ptr);
            ig.igText("Seed used: %u", @as(c_uint, state.rmg_result.seed));
            var path_z: [1100:0]u8 = undefined;
            const shown_path = std.fmt.bufPrintZ(&path_z, "{s}", .{state.rmg_result.mapPathSlice()}) catch "";
            ig.igTextWrapped("%s", shown_path.ptr);
            ig.igPopTextWrapPos();
            ig.igSpacing();
            if (ig.igButton("Open map")) {
                panels.openGeneratedRmgMap(state);
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("Close")) {
                state.rmg_phase = .idle;
                state.rmg_popup_opened = false;
                ig.igCloseCurrentPopup();
            }
        },
    }
}

// ---------------------------------------------------------------------------
// The RMG composers (05-09, D-06..D-12): the Containers Composer and the
// Graphs Composer. Each is a dockable Tools window over core.composers; every
// button runs a named command (commands.zig rmgc_* / rmgg_*) - except the
// batch adds, which pass several names at once - so BK_EDITOR_AUTO reaches all
// of it. Decision for 05-10 (RESEARCH Open Question 4): containers and graphs
// share the file row, the Open combo with its filter, the Check! findings list
// and the confirm popups below; the tables and the canvas are their own.
// ---------------------------------------------------------------------------

fn caseInsensitiveContains(haystack: []const u8, needle: []const u8) bool {
    if (needle.len == 0) return true;
    if (needle.len > haystack.len) return false;
    var at: usize = 0;
    while (at + needle.len <= haystack.len) : (at += 1) {
        if (std.ascii.eqlIgnoreCase(haystack[at .. at + needle.len], needle)) return true;
    }
    return false;
}

/// A command the discard prompt will run on YES: `name arg`.
fn setPending(buffer: *[220:0]u8, name: []const u8, arg: []const u8) void {
    const text = std.fmt.bufPrintZ(buffer, "{s} {s}", .{ name, arg }) catch {
        buffer[0] = 0;
        return;
    };
    _ = text;
}

fn runPending(state: *State, buffer: *const [220:0]u8) void {
    const text = std.mem.sliceTo(buffer, 0);
    const split = std.mem.indexOfScalar(u8, text, ' ') orelse text.len;
    _ = commands.run(state, text[0..split], if (split < text.len) text[split + 1 ..] else "");
}

/// Opens the named modal once when `want` asks for it, and reports whether it
/// is showing this frame. The modal's own buttons set `want` back to none.
fn modalIsShowing(want: panels.ComposerPopup, which: panels.ComposerPopup, name: [:0]const u8) bool {
    if (want != which) return false;
    if (!ig.igIsPopupOpen(name.ptr, 0)) _ = ig.igOpenPopup(name.ptr, 0);
    return true;
}

fn drawDiscardModal(state: *State, popup: *panels.ComposerPopup, pending: *const [220:0]u8) void {
    if (!modalIsShowing(popup.*, .discard, "Discard changes?")) return;
    if (ig.igBeginPopupModal("Discard changes?", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        panels.text("This file has changes that are not saved. Discard them?");
        if (ig.igButton("Discard")) {
            popup.* = .none;
            ig.igCloseCurrentPopup();
            runPending(state, pending);
        }
        ig.igSameLine();
        if (ig.igButton("Keep editing")) {
            popup.* = .none;
            ig.igCloseCurrentPopup();
        }
        ig.igEndPopup();
    }
}

/// New and Open ask first when the file has unsaved changes (the MFC saved
/// silently on New/Open; here the choice is the person's).
fn requestWithDiscard(state: *State, dirty: bool, popup: *panels.ComposerPopup, pending: *[220:0]u8, command: []const u8, arg: []const u8) void {
    if (!dirty) {
        _ = commands.run(state, command, arg);
        return;
    }
    setPending(pending, command, arg);
    popup.* = .discard;
}

fn drawFindings(state: *State, report: ?*const core.rmg.Report, fix_command: []const u8, fix_all_command: []const u8, clean_text: [:0]const u8) void {
    const found = report orelse return;
    ig.igSeparatorText("Check! findings");
    if (found.findings.items.len == 0) {
        panels.text(clean_text);
        return;
    }
    if (ig.igSmallButton("Fix all")) _ = commands.run(state, fix_all_command, "");
    ig.igSameLine();
    panels.text("Fixes that remove an entry are explicit: Fix removes just that one, and every fix is one undo step.");
    _ = ig.igBeginChild("findings", .{ .x = 0, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    for (found.findings.items, 0..) |finding, i| {
        ig.igPushIDInt(@intCast(i));
        if (finding.fix != .none) {
            if (ig.igSmallButton("Fix")) {
                var buffer: [16:0]u8 = undefined;
                _ = commands.run(state, fix_command, std.fmt.bufPrintZ(&buffer, "{d}", .{i}) catch "");
            }
            ig.igSameLine();
        }
        const color = if (finding.severity == .@"error") ig.ImVec4{ .x = 1.0, .y = 0.45, .z = 0.4, .w = 1 } else ig.ImVec4{ .x = 1.0, .y = 0.85, .z = 0.4, .w = 1 };
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, color);
        var line: [800:0]u8 = undefined;
        const text = std.fmt.bufPrintZ(&line, "{s}: {s}", .{ if (finding.severity == .@"error") "error" else "warning", finding.text }) catch "?";
        ig.igTextWrapped("%s", text.ptr);
        ig.igPopStyleColor();
        ig.igPopID();
    }
    ig.igEndChild();
}

/// The shared File row: New, an Open combo over the scanned names with its
/// filter, Save, Save As with its name field, Undo, Redo, Check!. `prefix` is
/// `rmgc` or `rmgg`.
fn drawFileRow(
    state: *State,
    comptime prefix: []const u8,
    folder: []const u8,
    names: []const []u8,
    doc_name: []const u8,
    dirty: bool,
    shipped: bool,
    can_undo: bool,
    can_redo: bool,
    open_filter: *[96:0]u8,
    save_as_edit: *[128:0]u8,
    popup: *panels.ComposerPopup,
    pending: *[220:0]u8,
) void {
    if (ig.igButton("New")) requestWithDiscard(state, dirty, popup, pending, prefix ++ "_new", "");
    ig.igSameLine();
    ig.igSetNextItemWidth(230);
    if (ig.igBeginCombo(std.fmt.comptimePrint("##open_{s}", .{prefix}), "Open...", 0)) {
        _ = ig.igInputTextWithHint(std.fmt.comptimePrint("##filter_{s}", .{prefix}), "filter", open_filter, open_filter.len + 1, 0);
        const filter = std.mem.sliceTo(open_filter, 0);
        _ = ig.igBeginChild(std.fmt.comptimePrint("##names_{s}", .{prefix}), .{ .x = 440, .y = 260 }, 0, 0);
        for (names) |full| {
            const relative = core.composers.relativeName(folder, full);
            if (!caseInsensitiveContains(relative, filter)) continue;
            var label: [200:0]u8 = undefined;
            const label_z = std.fmt.bufPrintZ(&label, "{s}", .{relative}) catch continue;
            if (ig.igSelectableEx(label_z.ptr, std.ascii.eqlIgnoreCase(full, doc_name), 0, .{ .x = 0, .y = 0 })) {
                var arg_buffer: [200:0]u8 = undefined;
                const arg = std.fmt.bufPrintZ(&arg_buffer, "{s}", .{relative}) catch "";
                requestWithDiscard(state, dirty, popup, pending, prefix ++ "_open", arg);
                ig.igCloseCurrentPopup();
            }
        }
        ig.igEndChild();
        ig.igEndCombo();
    }
    ig.igSameLine();
    if (ig.igButton(if (shipped) "Save (read-only: Save As)" else "Save")) _ = commands.run(state, prefix ++ "_save", "");
    ig.igSameLine();
    ig.igSetNextItemWidth(170);
    _ = ig.igInputTextWithHint(std.fmt.comptimePrint("##saveas_{s}", .{prefix}), "user\\name", save_as_edit, save_as_edit.len + 1, 0);
    ig.igSameLine();
    if (ig.igButton("Save As")) {
        const typed = std.mem.sliceTo(save_as_edit, 0);
        if (typed.len != 0) _ = commands.run(state, prefix ++ "_saveas", typed);
    }
    ig.igSameLine();
    ig.igBeginDisabled(!can_undo);
    if (ig.igButton("Undo")) _ = commands.run(state, prefix ++ "_undo", "");
    ig.igEndDisabled();
    ig.igSameLine();
    ig.igBeginDisabled(!can_redo);
    if (ig.igButton("Redo")) _ = commands.run(state, prefix ++ "_redo", "");
    ig.igEndDisabled();
    ig.igSameLine();
    if (ig.igButton("Check!")) _ = commands.run(state, prefix ++ "_check", "");
}

fn composerTitle(buffer: []u8, title: []const u8, doc_name: []const u8, dirty: bool, shipped: bool) [:0]const u8 {
    return std.fmt.bufPrintZ(buffer, "{s} - [{s}]{s}{s}###{s}", .{
        title,
        if (doc_name.len == 0) "new" else doc_name,
        if (dirty) " *" else "",
        if (shipped) " (shipped, read-only)" else "",
        title,
    }) catch "composer";
}

/// How many bytes a bufPrint wrote (none when it did not fit).
fn printedLen(printed: anyerror![]u8) usize {
    const text = printed catch return 0;
    return text.len;
}

fn fmtZ(buffer: []u8, comptime format: []const u8, args: anytype) [:0]const u8 {
    return std.fmt.bufPrintZ(buffer, format, args) catch "";
}

fn textCell(comptime format: []const u8, args: anytype) void {
    var buffer: [512:0]u8 = undefined;
    const text = std.fmt.bufPrintZ(&buffer, format, args) catch "?";
    ig.igTextUnformatted(text.ptr);
}

/// D-06/D-07/D-10/D-12: the Containers Composer.
pub fn drawContainersComposer(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.containers_open) return;
    const composers = &state.composers;
    composers.ensureScanned(state.editor);
    var title_buffer: [256]u8 = undefined;
    const doc = &composers.cdoc;
    const title = composerTitle(&title_buffer, "Containers Composer", doc.name, doc.dirty, doc.shipped);
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin(title.ptr, &state.containers_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;

    drawFileRow(state, "rmgc", core.composers.container_folder, composers.container_names.items, doc.name, doc.dirty, doc.shipped, doc.canUndo(), doc.canRedo(), &state.cc_open_filter, &state.cc_save_as_edit, &state.cc_popup, &state.cc_pending_cmd);
    if (composers.message().len != 0) {
        ig.igPushTextWrapPos(0);
        textCell("{s}", .{composers.message()});
        ig.igPopTextWrapPos();
    }
    drawContainerHeader(state);
    drawPatchTable(state);
    drawFindings(state, if (composers.container_report) |*report| report else null, "rmgc_fix", "rmgc_fix_all", "Nothing found: every patch, size and list checks out.");
    drawContainerModals(state);
    drawDiscardModal(state, &state.cc_popup, &state.cc_pending_cmd);
}

/// The container's own row, the MFC list's twelve columns.
fn drawContainerHeader(state: *State) void {
    const c = &state.composers.cdoc.current;
    const columns = [_][:0]const u8{ "Path", "Size", "Count", "NORTH (0)", "EAST (90)", "SOUTH (180)", "WEST (270)", "Season", "Season Folder", "Supported Settings", "Used Script IDs", "Used Script Areas" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollX | ig.ImGuiTableFlags_SizingFixedFit;
    if (!ig.igBeginTableEx("##container_header", columns.len, flags, .{ .x = 0, .y = ig.igGetFrameHeight() * 2.6 }, 0)) return;
    defer ig.igEndTable();
    ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 230, 0);
    for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
    ig.igTableHeadersRow();
    ig.igTableNextRow();
    var scratch: [512]u8 = undefined;
    _ = ig.igTableSetColumnIndex(0);
    textCell("{s}", .{if (state.composers.cdoc.name.len == 0) "(new)" else core.composers.relativeName(core.composers.container_folder, state.composers.cdoc.name)});
    _ = ig.igTableSetColumnIndex(1);
    textCell("{s}", .{logic.patchSizeText(&scratch, c.size_x, c.size_y)});
    _ = ig.igTableSetColumnIndex(2);
    textCell("{d}", .{c.patchCount()});
    for (0..4) |d| {
        _ = ig.igTableSetColumnIndex(@intCast(3 + d));
        textCell("{d}", .{c.indices[d].items.len});
    }
    _ = ig.igTableSetColumnIndex(7);
    textCell("{s}", .{core.rmg.seasonName(c.season, c.season_folder)});
    _ = ig.igTableSetColumnIndex(8);
    textCell("{s}", .{c.season_folder});
    _ = ig.igTableSetColumnIndex(9);
    if (c.supportedSettings(state.allocator)) |settings| {
        defer core.rmg.freeNames(state.allocator, settings);
        textCell("{s}", .{logic.namesText(&scratch, settings)});
    } else |_| {}
    _ = ig.igTableSetColumnIndex(10);
    textCell("{s}", .{logic.idsText(&scratch, c.script_ids.items)});
    _ = ig.igTableSetColumnIndex(11);
    textCell("{s}", .{logic.areasText(&scratch, c.script_areas.items)});
}

fn selectedPatches(state: *State, out: []usize) usize {
    var n: usize = 0;
    const count = @min(state.composers.cdoc.current.patchCount(), state.cc_selected.len);
    for (0..count) |i| {
        if (state.cc_selected[i] and n < out.len) {
            out[n] = i;
            n += 1;
        }
    }
    return n;
}

fn openPatchProperties(state: *State) void {
    var picked: [core.bridge.rmg_max_patches]usize = undefined;
    const n = selectedPatches(state, &picked);
    if (n == 0) return;
    const c = &state.composers.cdoc.current;
    // The setting shown is the common one; the cells are tri-state over the
    // selection (RMG_CreateContainerDialog.cpp OnPropertiesButton: all, none or
    // mixed - mixed is "keep").
    const first_place = c.patches.items[picked[0]].place;
    var same = true;
    for (picked[1..n]) |i| {
        if (!std.mem.eql(u8, c.patches.items[i].place, first_place)) same = false;
    }
    const shown = if (!same) "" else if (first_place.len == 0) "<any setting>" else first_place;
    @memset(&state.cc_place_edit, 0);
    const len = @min(shown.len, state.cc_place_edit.len - 1);
    @memcpy(state.cc_place_edit[0..len], shown[0..len]);
    for (0..4) |d| {
        var have: usize = 0;
        for (picked[0..n]) |i| {
            if (c.hasDirection(i, @enumFromInt(d))) have += 1;
        }
        state.cc_flags_edit[d] = if (have == 0) .off else if (have == n) .on else .keep;
    }
    state.cc_popup = .properties;
}

fn drawPatchTable(state: *State) void {
    const c = &state.composers.cdoc.current;
    // The buttons above the table (the MFC's Add / Delete / Properties).
    var picked: [core.bridge.rmg_max_patches]usize = undefined;
    const picked_count = selectedPatches(state, &picked);
    if (ig.igButton("Add patches...")) {
        state.composers.refreshPatches(state.editor) catch {};
        state.cc_picker_selected.clearRetainingCapacity();
        state.cc_picker_selected.appendNTimes(state.allocator, false, state.composers.patch_names.items.len) catch {};
        state.cc_popup = .picker;
    }
    ig.igSameLine();
    ig.igBeginDisabled(picked_count == 0);
    if (ig.igButton("Delete")) state.cc_popup = .delete_patches;
    ig.igSameLine();
    if (ig.igButton("Properties...")) openPatchProperties(state);
    ig.igEndDisabled();
    ig.igSameLine();
    var counter: [160]u8 = undefined;
    ig.igTextDisabled("%s", fmtZ(&counter, "{d} patches, {d} selected (Insert adds, Delete removes, Space or a double-click edits)", .{ c.patchCount(), picked_count }).ptr);

    const columns = [_][:0]const u8{ "Path", "Size", "Setting", "NORTH (0)", "EAST (90)", "SOUTH (180)", "WEST (270)" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollY | ig.ImGuiTableFlags_ScrollX | ig.ImGuiTableFlags_SizingFixedFit;
    const avail = ig.igGetContentRegionAvail();
    const height = if (state.composers.container_report != null) @max(avail.y * 0.5, 120) else @max(avail.y, 120);
    if (ig.igBeginTableEx("##patches", columns.len, flags, .{ .x = 0, .y = height }, 0)) {
        ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 470, 0);
        for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
        ig.igTableHeadersRow();
        const shift = ig.igGetIO().*.KeyShift;
        const ctrl = ig.igGetIO().*.KeyCtrl or ig.igGetIO().*.KeySuper;
        for (c.patches.items, 0..) |patch, i| {
            if (i >= state.cc_selected.len) break;
            ig.igTableNextRow();
            _ = ig.igTableSetColumnIndex(0);
            ig.igPushIDInt(@intCast(i));
            var label: [260:0]u8 = undefined;
            const label_z = std.fmt.bufPrintZ(&label, "{s}", .{patch.name}) catch "?";
            if (ig.igSelectableEx(label_z.ptr, state.cc_selected[i], ig.ImGuiSelectableFlags_SpanAllColumns | ig.ImGuiSelectableFlags_AllowDoubleClick, .{ .x = 0, .y = 0 })) {
                if (shift) {
                    const lo = @min(state.cc_anchor, i);
                    const hi = @max(state.cc_anchor, i);
                    if (!ctrl) @memset(&state.cc_selected, false);
                    for (lo..hi + 1) |k| state.cc_selected[k] = true;
                } else if (ctrl) {
                    state.cc_selected[i] = !state.cc_selected[i];
                    state.cc_anchor = i;
                } else {
                    @memset(&state.cc_selected, false);
                    state.cc_selected[i] = true;
                    state.cc_anchor = i;
                }
                if (ig.igIsMouseDoubleClicked(0)) openPatchProperties(state);
            }
            // A right-click selects the row under it when it is not selected.
            if (ig.igIsItemClickedEx(ig.ImGuiMouseButton_Right) and !state.cc_selected[i]) {
                @memset(&state.cc_selected, false);
                state.cc_selected[i] = true;
                state.cc_anchor = i;
            }
            ig.igPopID();
            var scratch: [32]u8 = undefined;
            _ = ig.igTableSetColumnIndex(1);
            textCell("{s}", .{logic.patchSizeText(&scratch, patch.size_x, patch.size_y)});
            _ = ig.igTableSetColumnIndex(2);
            textCell("{s}", .{patch.place});
            for (0..4) |d| {
                _ = ig.igTableSetColumnIndex(@intCast(3 + d));
                textCell("{s}", .{logic.directionCell(c.hasDirection(i, @enumFromInt(d)))});
            }
        }
        ig.igEndTable();
    }
    // The popup menu on the patches (the MFC's right-click menu).
    if (ig.igBeginPopupContextWindowEx("patches_context", ig.ImGuiPopupFlags_MouseButtonRight)) {
        if (ig.igMenuItemEx("Add patches...", "Insert", false, true)) {
            state.composers.refreshPatches(state.editor) catch {};
            state.cc_picker_selected.clearRetainingCapacity();
            state.cc_picker_selected.appendNTimes(state.allocator, false, state.composers.patch_names.items.len) catch {};
            state.cc_popup = .picker;
        }
        if (ig.igMenuItemEx("Delete patch", "Delete", false, picked_count > 0)) state.cc_popup = .delete_patches;
        if (ig.igMenuItemEx("Properties...", "Space", false, picked_count > 0)) openPatchProperties(state);
        ig.igSeparator();
        if (ig.igMenuItemEx("Check!", null, false, true)) _ = commands.run(state, "rmgc_check", "");
        ig.igEndPopup();
    }
    // The MFC list's keys, when this window has the focus and no text is being typed.
    if (ig.igIsWindowFocused(ig.ImGuiFocusedFlags_RootAndChildWindows) and !ig.igGetIO().*.WantTextInput and state.cc_popup == .none) {
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_Insert, false)) {
            state.composers.refreshPatches(state.editor) catch {};
            state.cc_picker_selected.clearRetainingCapacity();
            state.cc_picker_selected.appendNTimes(state.allocator, false, state.composers.patch_names.items.len) catch {};
            state.cc_popup = .picker;
        } else if (ig.igIsKeyPressedEx(ig.ImGuiKey_Delete, false) and picked_count > 0) {
            state.cc_popup = .delete_patches;
        } else if (ig.igIsKeyPressedEx(ig.ImGuiKey_Space, false) and picked_count > 0) {
            openPatchProperties(state);
        }
    }
}

fn drawContainerModals(state: *State) void {
    const composers = &state.composers;
    // The patch picker: every patch map the storages hold under
    // Scenarios\Patches, multi-select, with Browse for a map outside them.
    if (modalIsShowing(state.cc_popup, .picker, "Add patches")) {
        ig.igSetNextWindowSize(.{ .x = 560, .y = 440 }, ig.ImGuiCond_Appearing);
        if (ig.igBeginPopupModal("Add patches", null, 0)) {
            panels.text("Patches of the storages (Scenarios\\Patches). Click to pick several.");
            _ = ig.igInputTextWithHint("##picker_filter", "filter", &state.cc_picker_filter, state.cc_picker_filter.len + 1, 0);
            const filter = std.mem.sliceTo(&state.cc_picker_filter, 0);
            _ = ig.igBeginChild("##picker_list", .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() * 2 }, ig.ImGuiChildFlags_Borders, 0);
            var picked_count: usize = 0;
            for (composers.patch_names.items, 0..) |name, i| {
                if (i >= state.cc_picker_selected.items.len) break;
                if (state.cc_picker_selected.items[i]) picked_count += 1;
                const relative = core.composers.relativeName(core.composers.patch_folder, name);
                if (!caseInsensitiveContains(relative, filter)) continue;
                var label: [260:0]u8 = undefined;
                const label_z = std.fmt.bufPrintZ(&label, "{s}", .{relative}) catch continue;
                ig.igPushIDInt(@intCast(i));
                if (ig.igSelectableEx(label_z.ptr, state.cc_picker_selected.items[i], 0, .{ .x = 0, .y = 0 })) state.cc_picker_selected.items[i] = !state.cc_picker_selected.items[i];
                ig.igPopID();
            }
            ig.igEndChild();
            var button: [48]u8 = undefined;
            if (ig.igButton(fmtZ(&button, "Add {d} selected", .{picked_count}).ptr)) {
                var names_buffer = std.ArrayListUnmanaged([]const u8).empty;
                defer names_buffer.deinit(state.allocator);
                for (composers.patch_names.items, 0..) |name, i| {
                    if (i < state.cc_picker_selected.items.len and state.cc_picker_selected.items[i]) names_buffer.append(state.allocator, name) catch break;
                }
                _ = composers.addPatches(state.editor, names_buffer.items) catch 0;
                state.view.setStatus("containers: ", composers.message());
                state.cc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("Browse for a map file...")) panels.browsePatch(state);
            ig.igSameLine();
            if (ig.igButton("Close")) {
                state.cc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    // D-10: a map outside the storages is copied in, not refused.
    if (modalIsShowing(state.cc_popup, .import_copy, "Copy the patch into the RMG folder?")) {
        if (ig.igBeginPopupModal("Copy the patch into the RMG folder?", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            var root_buffer: [1024]u8 = undefined;
            var dest_buffer: [1200]u8 = undefined;
            const root = state.editor.rmgRoot(&root_buffer) catch "";
            const dest = logic.copyInDestination(&dest_buffer, root, composers.pending_import.dest.nameSlice(), composers.pending_import.pathSlice()) orelse "";
            ig.igPushTextWrapPos(520);
            textCell("{s}", .{composers.pending_import.pathSlice()});
            panels.text("is outside the game's data, so a container could not find it. Copy it to");
            textCell("{s}", .{dest});
            panels.text("and add the copy?");
            ig.igPopTextWrapPos();
            if (ig.igButton("Yes")) _ = commands.run(state, "rmgc_import_yes", "");
            ig.igSameLine();
            if (ig.igButton("No")) _ = commands.run(state, "rmgc_import_no", "");
            if (state.cc_popup != .import_copy) ig.igCloseCurrentPopup();
            ig.igEndPopup();
        }
    }
    if (modalIsShowing(state.cc_popup, .delete_patches, "Delete patches?")) {
        if (ig.igBeginPopupModal("Delete patches?", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            panels.text("Do you really want to DELETE the selected patches?");
            if (ig.igButton("Yes")) {
                var picked: [core.bridge.rmg_max_patches]usize = undefined;
                const n = selectedPatches(state, &picked);
                composers.deletePatches(picked[0..n]) catch {};
                @memset(&state.cc_selected, false);
                state.cc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("No")) {
                state.cc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    // Patch properties: the setting and the four direction cells (tri-state
    // over a multi-selection: unchanged cells stay as each patch has them).
    if (modalIsShowing(state.cc_popup, .properties, "Patch properties")) {
        if (ig.igBeginPopupModal("Patch properties", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            var picked: [core.bridge.rmg_max_patches]usize = undefined;
            const n = selectedPatches(state, &picked);
            if (n == 1) {
                const patch = composers.cdoc.current.patches.items[picked[0]];
                textCell("Path: {s}", .{patch.name});
                textCell("Size: {d}x{d} patches", .{ patch.size_x, patch.size_y });
            } else {
                panels.text("Path: Multiple selection...");
                panels.text("Size: ...");
            }
            _ = ig.igInputTextWithHint("setting", "<any setting> or a settings name", &state.cc_place_edit, state.cc_place_edit.len + 1, 0);
            ig.igSameLine();
            if (ig.igSmallButton("any")) {
                @memset(&state.cc_place_edit, 0);
                @memcpy(state.cc_place_edit[0.."<any setting>".len], "<any setting>");
            }
            if (ig.igBeginCombo("##settings_scan", "choose a setting", 0)) {
                var total: usize = 0;
                _ = state.editor.bridge.listRmg(.settings, &.{}, &total);
                if (total != 0) {
                    if (state.allocator.alloc(core.bridge.RmgName, total)) |names| {
                        defer state.allocator.free(names);
                        var got: usize = 0;
                        if (state.editor.bridge.listRmg(.settings, names, &got) == .ok) {
                            for (names[0..@min(got, names.len)]) |entry| {
                                var label: [200:0]u8 = undefined;
                                const stem = core.composers.relativeName("scenarios\\settings\\", entry.nameSlice());
                                const label_z = std.fmt.bufPrintZ(&label, "{s}", .{stem}) catch continue;
                                if (ig.igSelectableEx(label_z.ptr, false, 0, .{ .x = 0, .y = 0 })) {
                                    @memset(&state.cc_place_edit, 0);
                                    const len = @min(stem.len, state.cc_place_edit.len - 1);
                                    @memcpy(state.cc_place_edit[0..len], stem[0..len]);
                                }
                            }
                        }
                    } else |_| {}
                }
                ig.igEndCombo();
            }
            for (0..4) |d| {
                var label: [32:0]u8 = undefined;
                const text = std.fmt.bufPrintZ(&label, "{s}", .{core.rmg.direction_names[d]}) catch "?";
                var value: bool = state.cc_flags_edit[d] == .on;
                // Mixed over a selection (keep) shows unticked and stays unless clicked.
                if (ig.igCheckbox(text.ptr, &value)) state.cc_flags_edit[d] = if (value) .on else .off;
                if (state.cc_flags_edit[d] == .keep) {
                    ig.igSameLine();
                    ig.igTextDisabled("(mixed: unchanged)");
                }
            }
            if (ig.igButton("OK")) {
                const place = std.mem.sliceTo(&state.cc_place_edit, 0);
                // A blank setting over a mixed selection is "leave the setting".
                composers.setPatchProperties(picked[0..n], if (place.len == 0 and n > 1) null else place, state.cc_flags_edit) catch {};
                state.cc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("Cancel")) {
                state.cc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
}

// --- The Graphs Composer ------------------------------------------------

const link_half_width_pixels: f32 = 2;

fn graphGeometry(origin: ig.ImVec2, side: f32, canvas: *const core.rmg.Canvas) logic.CanvasGeometry {
    return .{ .origin_x = origin.x, .origin_y = origin.y, .side = side, .limit = canvas.limit() };
}

fn abgr(r: u8, g: u8, b: u8, a: u8) u32 {
    return (@as(u32, a) << 24) | (@as(u32, b) << 16) | (@as(u32, g) << 8) | r;
}

/// What is under `tile`, remembered for the right-click menu and the
/// properties dialogs: the node and every link (up to eight).
fn rememberHit(state: *State, geometry: logic.CanvasGeometry, tile: core.rmg.Tile) core.rmg.Hit {
    const graph = &state.composers.gdoc.current;
    const tolerance = geometry.toleranceTiles(link_half_width_pixels);
    const hit = core.rmg.hitTest(graph, tile, tolerance);
    state.cg_hit_node = hit.node;
    var listed: [8]usize = undefined;
    state.cg_hit_link_count = core.rmg.hitLinks(graph, tile, tolerance, &listed);
    for (listed[0..state.cg_hit_link_count], 0..) |index, slot| state.cg_hit_links[slot] = @intCast(index);
    return hit;
}

/// Opens the properties dialog for what is under `tile`: the links first (the
/// MFC's own order - a link's line crosses its nodes), else the node.
fn openGraphProperties(state: *State, geometry: logic.CanvasGeometry, tile: core.rmg.Tile) bool {
    const hit = rememberHit(state, geometry, tile);
    if (!hit.any()) return false;
    openRememberedProperties(state);
    return true;
}

fn openRememberedProperties(state: *State) void {
    if (state.cg_hit_link_count > 0) {
        state.cg_link_index = 0;
        loadLinkEdits(state);
        state.cg_popup = .link_properties;
    } else if (state.cg_hit_node >= 0) {
        const node = state.composers.gdoc.current.nodes.items[@intCast(state.cg_hit_node)];
        @memset(&state.cg_container_edit, 0);
        const shown = core.composers.relativeName(core.composers.container_folder, node.container);
        const len = @min(shown.len, state.cg_container_edit.len - 1);
        @memcpy(state.cg_container_edit[0..len], shown[0..len]);
        state.cg_popup = .node_properties;
    }
}

fn currentLink(state: *State) ?usize {
    if (state.cg_link_index >= state.cg_hit_link_count) return null;
    const index = state.cg_hit_links[state.cg_link_index];
    return if (index < state.composers.gdoc.current.links.items.len) index else null;
}

/// The link dialog's edit buffers from the link now shown (radius and min
/// length in cells, as the MFC dialog has them).
fn loadLinkEdits(state: *State) void {
    const index = currentLink(state) orelse return;
    const link = state.composers.gdoc.current.links.items[index];
    var scratch: [64]u8 = undefined;
    const values = [6][]const u8{
        link.desc,
        logic.cellsText(&scratch, link.radius),
        "",
        "",
        "",
        "",
    };
    for (&state.cg_link_edit) |*buffer| buffer.* = [_:0]u8{0} ** 96;
    const put = struct {
        fn into(buffer: *[96:0]u8, text: []const u8) void {
            const len = @min(text.len, buffer.len - 1);
            @memcpy(buffer[0..len], text[0..len]);
            buffer[len] = 0;
        }
    }.into;
    put(&state.cg_link_edit[0], values[0]);
    put(&state.cg_link_edit[1], values[1]);
    var parts: [16]u8 = undefined;
    put(&state.cg_link_edit[2], std.fmt.bufPrint(&parts, "{d}", .{link.parts}) catch "");
    var cells: [32]u8 = undefined;
    put(&state.cg_link_edit[3], logic.cellsText(&cells, link.min_length));
    var width: [32]u8 = undefined;
    put(&state.cg_link_edit[4], std.fmt.bufPrint(&width, "{d:.2}", .{link.distance}) catch "");
    var disturbance: [32]u8 = undefined;
    put(&state.cg_link_edit[5], std.fmt.bufPrint(&disturbance, "{d:.2}", .{link.disturbance}) catch "");
}

fn drawGraphHeader(state: *State) void {
    const g = &state.composers.gdoc.current;
    const columns = [_][:0]const u8{ "Path", "Max Size", "Nodes", "Links", "Empty Nodes", "Empty Links", "Season", "Season Folder", "Supported Settings", "Used ScriptIDs", "Used ScriptAreas" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollX | ig.ImGuiTableFlags_SizingFixedFit;
    if (!ig.igBeginTableEx("##graph_header", columns.len, flags, .{ .x = 0, .y = ig.igGetFrameHeight() * 2.6 }, 0)) return;
    defer ig.igEndTable();
    ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 230, 0);
    for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
    ig.igTableHeadersRow();
    ig.igTableNextRow();
    var scratch: [512]u8 = undefined;
    _ = ig.igTableSetColumnIndex(0);
    textCell("{s}", .{if (state.composers.gdoc.name.len == 0) "(new)" else core.composers.relativeName(core.composers.graph_folder, state.composers.gdoc.name)});
    _ = ig.igTableSetColumnIndex(1);
    textCell("{s}", .{logic.patchSizeText(&scratch, g.size_x, g.size_y)});
    _ = ig.igTableSetColumnIndex(2);
    textCell("{d}", .{g.nodes.items.len});
    _ = ig.igTableSetColumnIndex(3);
    textCell("{d}", .{g.links.items.len});
    _ = ig.igTableSetColumnIndex(4);
    textCell("{d}", .{g.emptyNodeCount()});
    _ = ig.igTableSetColumnIndex(5);
    textCell("{d}", .{g.emptyLinkCount()});
    _ = ig.igTableSetColumnIndex(6);
    textCell("{s}", .{core.rmg.seasonName(g.season, g.season_folder)});
    _ = ig.igTableSetColumnIndex(7);
    textCell("{s}", .{g.season_folder});
    _ = ig.igTableSetColumnIndex(8);
    // Reading every node's container is not free: only when the graph moved.
    if (state.cg_settings_seen != state.composers.generation or state.cg_settings_text.items.len == 0 and state.cg_settings_seen == 0) {
        state.composers.graphSettingsText(state.editor, &state.cg_settings_text) catch {};
        state.cg_settings_seen = state.composers.generation;
    }
    textCell("{s}", .{state.cg_settings_text.items});
    _ = ig.igTableSetColumnIndex(9);
    textCell("{s}", .{logic.idsText(&scratch, g.script_ids.items)});
    _ = ig.igTableSetColumnIndex(10);
    textCell("{s}", .{logic.areasText(&scratch, g.script_areas.items)});
}

/// D-11: the canvas - left-drag on empty ground adds a node, a drag from a node
/// moves it, a drag from its edge resizes it, Ctrl+drag from one node to
/// another links them, right-click is Delete / Properties, a double-click is
/// Properties, and the slider shows 1 to 32 patches across.
fn drawGraphCanvas(state: *State, reserve_below: f32) void {
    const composers = &state.composers;
    const canvas = &composers.canvas;
    const doc = &composers.gdoc;
    var patches: c_int = canvas.patches;
    if (ig.igSliderInt("patches across", &patches, core.rmg.min_zoom, core.rmg.max_zoom)) {
        var buffer: [8:0]u8 = undefined;
        _ = commands.run(state, "rmgg_zoom", std.fmt.bufPrintZ(&buffer, "{d}", .{patches}) catch "");
    }
    const avail = ig.igGetContentRegionAvail();
    const side = @max(@min(avail.x, avail.y - reserve_below - ig.igGetFrameHeightWithSpacing() * 2), 160);
    const hover_tile = state.cg_pointer;
    // The line above the canvas: the map size, the position and the rect.
    {
        var rect = core.rmg.Rect{ .x1 = hover_tile.x, .y1 = hover_tile.y, .x2 = hover_tile.x + 1, .y2 = hover_tile.y + 1 };
        if (canvas.state == .add) rect = canvas.dragRect();
        textCell("Map: [{d}x{d}], position: ({d}, {d}), rect: ({d}, {d}, {d}, {d}) [{d}x{d}]", .{ canvas.patches, canvas.patches, hover_tile.x, hover_tile.y, rect.x1, rect.y1, rect.x2, rect.y2, rect.width(), rect.height() });
    }
    const origin = ig.igGetCursorScreenPos();
    _ = ig.igInvisibleButton("##graph_canvas", .{ .x = side, .y = side }, ig.ImGuiButtonFlags_MouseButtonLeft | ig.ImGuiButtonFlags_MouseButtonRight);
    const hovered = ig.igIsItemHovered(0);
    const geometry = graphGeometry(origin, side, canvas);
    const io = ig.igGetIO();
    const mouse = io.*.MousePos;
    const tile = geometry.tileAt(mouse.x, mouse.y);
    if (hovered or state.cg_dragging) state.cg_pointer = tile;
    const tolerance = geometry.toleranceTiles(link_half_width_pixels);
    const ctrl = io.*.KeyCtrl or io.*.KeySuper;

    // The gestures: press, drag, release - core.rmg.Canvas decides.
    if (state.cg_popup == .none and hovered and ig.igIsMouseClicked(0) and !state.cg_dragging) {
        canvas.press(state.allocator, doc, tile, ctrl, tolerance) catch {};
        state.cg_dragging = true;
    }
    if (state.cg_dragging) {
        if (ig.igIsMouseDown(0)) {
            canvas.drag(doc, tile);
        } else {
            _ = composers.finishGesture(tile) catch {};
            state.cg_dragging = false;
            if (composers.message().len != 0) state.view.setStatus("graphs: ", composers.message());
        }
    }
    if (hovered and !state.cg_dragging and state.cg_popup == .none) {
        if (ig.igIsMouseDoubleClicked(0)) _ = openGraphProperties(state, geometry, tile);
        if (ig.igIsMouseReleased(1)) {
            if (rememberHit(state, geometry, tile).any()) _ = ig.igOpenPopup("graph_context", 0);
        }
    }

    // Drawing.
    const draw_list = ig.igGetWindowDrawList();
    const corner = ig.ImVec2{ .x = origin.x + side, .y = origin.y + side };
    ig.ImDrawList_AddRectFilled(draw_list, origin, corner, abgr(0x20, 0x20, 0x20, 0xFF));
    if (canvas.patches > 1) {
        var k: i32 = 1;
        while (k < canvas.patches) : (k += 1) {
            const offset = @as(f32, @floatFromInt(k)) * side / @as(f32, @floatFromInt(canvas.patches));
            ig.ImDrawList_AddLine(draw_list, .{ .x = origin.x + offset, .y = origin.y }, .{ .x = origin.x + offset, .y = corner.y }, abgr(0x60, 0x60, 0x60, 0xFF));
            ig.ImDrawList_AddLine(draw_list, .{ .x = origin.x, .y = origin.y + offset }, .{ .x = corner.x, .y = origin.y + offset }, abgr(0x60, 0x60, 0x60, 0xFF));
        }
    }
    ig.ImDrawList_AddRect(draw_list, origin, corner, abgr(0xA0, 0xA0, 0xA0, 0xFF));
    const graph = &doc.current;
    for (graph.nodes.items, 0..) |node, i| {
        const outer = geometry.rectOnScreen(node.rect);
        const inner = geometry.rectOnScreen(.{ .x1 = node.rect.x1 + 1, .y1 = node.rect.y1 + 1, .x2 = node.rect.x2 - 1, .y2 = node.rect.y2 - 1 });
        const empty = node.container.len == 0;
        const fill: u32 = if (empty) abgr(0x55, 0x55, 0x55, 0xFF) else abgr(0x80, 0x80, 0x80, 0xFF);
        ig.ImDrawList_AddRectFilled(draw_list, .{ .x = outer[0], .y = outer[1] }, .{ .x = outer[2], .y = outer[3] }, fill);
        ig.ImDrawList_AddRect(draw_list, .{ .x = outer[0], .y = outer[1] }, .{ .x = outer[2], .y = outer[3] }, abgr(0xFF, 0xFF, 0xFF, 0xFF));
        ig.ImDrawList_AddRect(draw_list, .{ .x = inner[0], .y = inner[1] }, .{ .x = inner[2], .y = inner[3] }, abgr(0xFF, 0xFF, 0xFF, 0xFF));
        var label: [16:0]u8 = undefined;
        const label_z = std.fmt.bufPrintZ(&label, "{d}", .{i}) catch "";
        ig.ImDrawList_AddText(draw_list, .{ .x = inner[0] + 2, .y = inner[1] + 2 }, if (empty) abgr(0xFF, 0xA0, 0xA0, 0xFF) else abgr(0xA0, 0xFF, 0xA0, 0xFF), label_z.ptr);
    }
    const node_count: i32 = @intCast(graph.nodes.items.len);
    for (graph.links.items) |link| {
        if (link.a < 0 or link.a >= node_count or link.b < 0 or link.b >= node_count) continue;
        const ra = graph.nodes.items[@intCast(link.a)].rect;
        const rb = graph.nodes.items[@intCast(link.b)].rect;
        const a = geometry.pointOnScreen(@as(f32, @floatFromInt(ra.x1 + ra.x2)) * 0.5, @as(f32, @floatFromInt(ra.y1 + ra.y2)) * 0.5);
        const b = geometry.pointOnScreen(@as(f32, @floatFromInt(rb.x1 + rb.x2)) * 0.5, @as(f32, @floatFromInt(rb.y1 + rb.y2)) * 0.5);
        const color = if (link.desc.len == 0) abgr(0xFF, 0xA0, 0xA0, 0xFF) else abgr(0xA0, 0xFF, 0xA0, 0xFF);
        ig.ImDrawList_AddLineEx(draw_list, .{ .x = a[0], .y = a[1] }, .{ .x = b[0], .y = b[1] }, color, link_half_width_pixels * 2);
    }
    if (state.cg_dragging and canvas.state == .add) {
        const r = geometry.rectOnScreen(canvas.dragRect());
        ig.ImDrawList_AddRect(draw_list, .{ .x = r[0], .y = r[1] }, .{ .x = r[2], .y = r[3] }, abgr(0xFF, 0xFF, 0xFF, 0xFF));
    } else if (state.cg_dragging and canvas.state == .link) {
        const a = geometry.pointOnScreen(@as(f32, @floatFromInt(canvas.start.x)) + 0.5, @as(f32, @floatFromInt(canvas.start.y)) + 0.5);
        ig.ImDrawList_AddLineEx(draw_list, .{ .x = a[0], .y = a[1] }, mouse, abgr(0xFF, 0xA0, 0xA0, 0xFF), link_half_width_pixels * 2);
    }

    // The line below: what is under the pointer, and the cursor to match.
    var below: [512]u8 = undefined;
    var shown: []const u8 = "";
    if (hovered and !state.cg_dragging) {
        const hit = core.rmg.hitTest(graph, tile, tolerance);
        var used: usize = 0;
        if (hit.links > 1) {
            const links_text = std.fmt.bufPrint(&below, "{d} Links", .{hit.links});
            used = printedLen(links_text);
        } else if (hit.link >= 0) {
            const link = graph.links.items[@intCast(hit.link)];
            const link_text = if (link.desc.len == 0)
                std.fmt.bufPrint(&below, "Empty Link {d}", .{hit.link})
            else
                std.fmt.bufPrint(&below, "Link {d}: {s}", .{ hit.link, link.desc });
            used = printedLen(link_text);
        }
        if (hit.node >= 0) {
            const node = graph.nodes.items[@intCast(hit.node)];
            const sep = if (used != 0) ", " else "";
            const text = if (node.container.len == 0)
                std.fmt.bufPrint(below[used..], "{s}Empty Node {d}", .{ sep, hit.node })
            else
                std.fmt.bufPrint(below[used..], "{s}Node {d}: {s}", .{ sep, hit.node, node.container });
            used += printedLen(text);
            if (hit.link < 0) setResizeCursor(hit.sides);
        }
        shown = below[0..used];
    }
    textCell("{s}", .{shown});
}

fn setResizeCursor(sides: u8) void {
    const rmg = core.rmg;
    const min_x = sides & rmg.side_min_x != 0;
    const max_x = sides & rmg.side_max_x != 0;
    const min_y = sides & rmg.side_min_y != 0;
    const max_y = sides & rmg.side_max_y != 0;
    const cursor: c_int = if ((min_x and min_y) or (max_x and max_y)) ig.ImGuiMouseCursor_ResizeNESW else if ((min_x and max_y) or (max_x and min_y)) ig.ImGuiMouseCursor_ResizeNWSE else if (min_x or max_x) ig.ImGuiMouseCursor_ResizeEW else if (min_y or max_y) ig.ImGuiMouseCursor_ResizeNS else ig.ImGuiMouseCursor_ResizeAll;
    ig.igSetMouseCursor(cursor);
}

/// D-11/D-12: the Graphs Composer.
pub fn drawGraphsComposer(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.graphs_open) return;
    const composers = &state.composers;
    composers.ensureScanned(state.editor);
    var title_buffer: [256]u8 = undefined;
    const doc = &composers.gdoc;
    const title = composerTitle(&title_buffer, "Graphs Composer", doc.name, doc.dirty, doc.shipped);
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin(title.ptr, &state.graphs_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;

    drawFileRow(state, "rmgg", core.composers.graph_folder, composers.graph_names.items, doc.name, doc.dirty, doc.shipped, doc.canUndo(), doc.canRedo(), &state.cg_open_filter, &state.cg_save_as_edit, &state.cg_popup, &state.cg_pending_cmd);
    if (composers.message().len != 0) {
        ig.igPushTextWrapPos(0);
        textCell("{s}", .{composers.message()});
        ig.igPopTextWrapPos();
    }
    drawGraphHeader(state);
    const reserve: f32 = if (composers.graph_report != null) 150 else 0;
    drawGraphCanvas(state, reserve);
    drawFindings(state, if (composers.graph_report) |*report| report else null, "rmgg_fix", "rmgg_fix_all", "Nothing found: every node, link and list checks out.");
    drawGraphModals(state);
    drawDiscardModal(state, &state.cg_popup, &state.cg_pending_cmd);
}

fn drawGraphModals(state: *State) void {
    if (ig.igBeginPopup("graph_context", 0)) {
        const has_target = state.cg_hit_link_count > 0 or state.cg_hit_node >= 0;
        if (ig.igMenuItemEx("Delete", null, false, has_target)) {
            state.cg_popup = if (state.cg_hit_link_count > 0) .delete_link else .delete_node;
        }
        if (ig.igMenuItemEx("Properties...", null, false, has_target)) openRememberedProperties(state);
        ig.igEndPopup();
    }
    // Delete asks first, as the MFC does ("Do you really want to DELETE selected
    // Links/Nodes?"); a script's rmgg_node_del / rmgg_link_del is the answer.
    if (modalIsShowing(state.cg_popup, .delete_link, "Delete link?")) {
        if (ig.igBeginPopupModal("Delete link?", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            panels.text("Do you really want to DELETE the selected link?");
            if (ig.igButton("Yes")) {
                if (state.cg_hit_link_count > 0) {
                    var buffer: [16:0]u8 = undefined;
                    _ = commands.run(state, "rmgg_link_del", std.fmt.bufPrintZ(&buffer, "{d}", .{state.cg_hit_links[0]}) catch "");
                }
                state.cg_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("No")) {
                state.cg_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    if (modalIsShowing(state.cg_popup, .delete_node, "Delete node?")) {
        if (ig.igBeginPopupModal("Delete node?", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            panels.text("Do you really want to DELETE the selected node (and its links)?");
            if (ig.igButton("Yes")) {
                if (state.cg_hit_node >= 0) {
                    var buffer: [16:0]u8 = undefined;
                    _ = commands.run(state, "rmgg_node_del", std.fmt.bufPrintZ(&buffer, "{d}", .{state.cg_hit_node}) catch "");
                }
                state.cg_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("No")) {
                state.cg_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    drawNodeProperties(state);
    drawLinkProperties(state);
}

/// Node properties: the node's rectangle and link count, the container's path
/// with Browse over the storages (and the size and season checks run on OK,
/// RMG_CreateGraphDialog.cpp OnPropertiesMenu).
fn drawNodeProperties(state: *State) void {
    const composers = &state.composers;
    if (!modalIsShowing(state.cg_popup, .node_properties, "Node properties")) return;
    if (!ig.igBeginPopupModal("Node properties", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
    defer ig.igEndPopup();
    const graph = &composers.gdoc.current;
    if (state.cg_hit_node < 0 or state.cg_hit_node >= graph.nodes.items.len) {
        state.cg_popup = .none;
        ig.igCloseCurrentPopup();
        return;
    }
    const index: usize = @intCast(state.cg_hit_node);
    const node = graph.nodes.items[index];
    textCell("Node {d}: ({d}, {d}, {d}, {d}), [{d}x{d}], Links: {d}", .{ index, node.rect.x1, node.rect.y1, node.rect.x2, node.rect.y2, node.rect.width(), node.rect.height(), graph.linksOfNode(index) });
    _ = ig.igInputTextWithHint("container", "e.g. winter\\army_s (under scenarios\\containers)", &state.cg_container_edit, state.cg_container_edit.len + 1, 0);
    if (ig.igBeginCombo("##browse_container", "Browse the storages...", 0)) {
        _ = ig.igInputTextWithHint("##container_filter", "filter", &state.cg_open_filter, state.cg_open_filter.len + 1, 0);
        const filter = std.mem.sliceTo(&state.cg_open_filter, 0);
        _ = ig.igBeginChild("##container_list", .{ .x = 440, .y = 240 }, 0, 0);
        for (composers.container_names.items) |full| {
            const relative = core.composers.relativeName(core.composers.container_folder, full);
            if (!caseInsensitiveContains(relative, filter)) continue;
            var label: [200:0]u8 = undefined;
            const label_z = std.fmt.bufPrintZ(&label, "{s}", .{relative}) catch continue;
            if (ig.igSelectableEx(label_z.ptr, false, 0, .{ .x = 0, .y = 0 })) {
                @memset(&state.cg_container_edit, 0);
                const len = @min(relative.len, state.cg_container_edit.len - 1);
                @memcpy(state.cg_container_edit[0..len], relative[0..len]);
            }
        }
        ig.igEndChild();
        ig.igEndCombo();
    }
    if (ig.igButton("OK")) {
        const typed = std.mem.sliceTo(&state.cg_container_edit, 0);
        composers.setNodeContainer(state.editor, index, typed) catch {};
        state.view.setStatus("graphs: ", composers.message());
        state.cg_popup = .none;
        ig.igCloseCurrentPopup();
    }
    ig.igSameLine();
    if (ig.igButton("Clear")) {
        @memset(&state.cg_container_edit, 0);
    }
    ig.igSameLine();
    if (ig.igButton("Cancel")) {
        state.cg_popup = .none;
        ig.igCloseCurrentPopup();
    }
}

/// Link properties: the links under the point (a list when there are several),
/// the type, the descriptor with Browse, the length, radius, width,
/// disturbance and parts (RMG_GraphLinkPropertiesDialog); every edit commits
/// on deactivate as one undo step.
fn drawLinkProperties(state: *State) void {
    const composers = &state.composers;
    if (!modalIsShowing(state.cg_popup, .link_properties, "Link properties")) return;
    if (!ig.igBeginPopupModal("Link properties", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
    defer ig.igEndPopup();
    const graph = &composers.gdoc.current;
    if (state.cg_hit_link_count == 0 or currentLink(state) == null) {
        state.cg_popup = .none;
        ig.igCloseCurrentPopup();
        return;
    }
    if (state.cg_hit_link_count > 1) {
        for (0..state.cg_hit_link_count) |slot| {
            const link_index = state.cg_hit_links[slot];
            if (link_index >= graph.links.items.len) continue;
            const link = graph.links.items[link_index];
            var label: [160:0]u8 = undefined;
            const label_z = std.fmt.bufPrintZ(&label, "link {d}: node {d} to node {d}  {s}", .{ link_index, link.a, link.b, link.desc }) catch continue;
            if (ig.igSelectableEx(label_z.ptr, slot == state.cg_link_index, 0, .{ .x = 0, .y = 0 })) {
                state.cg_link_index = slot;
                loadLinkEdits(state);
            }
        }
        ig.igSeparator();
    }
    const index = currentLink(state).?;
    const link = graph.links.items[index];
    textCell("Link {d}: node {d} to node {d}", .{ index, link.a, link.b });
    var arg_buffer: [200:0]u8 = undefined;
    var road = link.kind == core.rmg.link_road;
    if (ig.igRadioButton("Road", road)) {
        _ = commands.run(state, "rmgg_link", std.fmt.bufPrintZ(&arg_buffer, "{d}:kind:0", .{index}) catch "");
        road = true;
    }
    ig.igSameLine();
    if (ig.igRadioButton("River", !road)) {
        _ = commands.run(state, "rmgg_link", std.fmt.bufPrintZ(&arg_buffer, "{d}:kind:1", .{index}) catch "");
    }
    // The descriptor: typed, or picked from the storage's own (terrain\sets\...\roads3d or rivers3d).
    _ = ig.igInputTextWithHint("VSO desc", "terrain\\sets\\2\\roads3d\\road_grunt", &state.cg_link_edit[0], state.cg_link_edit[0].len + 1, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) {
        const typed = std.mem.sliceTo(&state.cg_link_edit[0], 0);
        _ = commands.run(state, "rmgg_link", std.fmt.bufPrintZ(&arg_buffer, "{d}:desc:{s}", .{ index, typed }) catch "");
    }
    if (ig.igBeginCombo("##browse_vso", "Browse the storages...", 0)) {
        const folder_word: []const u8 = if (link.kind == core.rmg.link_road) "\\roads3d\\" else "\\rivers3d\\";
        var total: usize = 0;
        _ = state.editor.bridge.listStorageFiles("terrain\\sets\\", ".xml", &.{}, &total);
        if (total != 0) {
            if (state.allocator.alloc(core.bridge.RmgName, total)) |names| {
                defer state.allocator.free(names);
                var got: usize = 0;
                if (state.editor.bridge.listStorageFiles("terrain\\sets\\", ".xml", names, &got) == .ok) {
                    for (names[0..@min(got, names.len)]) |entry| {
                        const full = entry.nameSlice();
                        if (std.mem.indexOf(u8, full, folder_word) == null) continue;
                        const bare = full[0 .. full.len - ".xml".len];
                        var label: [200:0]u8 = undefined;
                        const label_z = std.fmt.bufPrintZ(&label, "{s}", .{bare}) catch continue;
                        if (ig.igSelectableEx(label_z.ptr, false, 0, .{ .x = 0, .y = 0 })) {
                            _ = commands.run(state, "rmgg_link", std.fmt.bufPrintZ(&arg_buffer, "{d}:desc:{s}", .{ index, bare }) catch "");
                            loadLinkEdits(state);
                        }
                    }
                }
            } else |_| {}
        }
        ig.igEndCombo();
    }
    // Numbers: committed on deactivate, as the MFC's edits commit on change.
    const fields = [_]struct { label: [:0]const u8, field: []const u8, slot: usize }{
        .{ .label = "min length (cells)", .field = "min_length", .slot = 3 },
        .{ .label = "radius (cells)", .field = "radius", .slot = 1 },
        .{ .label = "width (0..1)", .field = "distance", .slot = 4 },
        .{ .label = "disturbance (0..1)", .field = "disturbance", .slot = 5 },
        .{ .label = "parts", .field = "parts", .slot = 2 },
    };
    for (fields) |entry| {
        _ = ig.igInputTextWithHint(entry.label.ptr, null, &state.cg_link_edit[entry.slot], state.cg_link_edit[entry.slot].len + 1, 0);
        if (ig.igIsItemDeactivatedAfterEdit()) {
            const typed = std.mem.sliceTo(&state.cg_link_edit[entry.slot], 0);
            if (typed.len != 0) _ = commands.run(state, "rmgg_link", std.fmt.bufPrintZ(&arg_buffer, "{d}:{s}:{s}", .{ index, entry.field, typed }) catch "");
            loadLinkEdits(state);
        }
    }
    if (link.parts < core.rmg.min_parts) {
        ig.igPushStyleColorImVec4(ig.ImGuiCol_Text, .{ .x = 1.0, .y = 0.85, .z = 0.4, .w = 1 });
        textCell("The engine raises {d} parts to {d}.", .{ link.parts, core.rmg.min_parts });
        ig.igPopStyleColor();
    }
    if (ig.igButton("Delete link...")) state.cg_popup = .delete_link;
    ig.igSameLine();
    if (ig.igButton("Close")) {
        state.cg_popup = .none;
        ig.igCloseCurrentPopup();
    }
}

// ---------------------------------------------------------------------------
// The Fields Composer (M3 05-10, D-06/D-07/D-12): the MFC's RMG_CreateFieldDialog
// and its three tabs (terrain, objects, heights) as one dockable Tools window
// over core.composers' field set. A file, not the MFC's list of files: Open
// lists the storages' own scan (D-08), Save writes the open one (the MFC's
// "Save All" saved every field set of its list file), Save As names a user file,
// and a shipped set is read-only. Every edit is a named command (rmgf_*) or one
// composer call, and one step of the file's own undo.
// ---------------------------------------------------------------------------

fn ensureSelection(state: *State, list: *std.ArrayListUnmanaged(bool), count: usize) void {
    const had = list.items.len;
    if (had == count) return;
    list.resize(state.allocator, count) catch return;
    if (count > had) @memset(list.items[had..], false);
}

fn selectedFrom(list: []const bool, out: []usize) usize {
    var n: usize = 0;
    for (list, 0..) |on, i| {
        if (on and n < out.len) {
            out[n] = i;
            n += 1;
        }
    }
    return n;
}

/// A click on a row of a multi-select list: Ctrl toggles, Shift extends from
/// the anchor-less first selected, a plain click picks just that row.
fn clickRow(list: []bool, index: usize) void {
    const io = ig.igGetIO();
    const ctrl = io.*.KeyCtrl or io.*.KeySuper;
    if (ctrl) {
        list[index] = !list[index];
        return;
    }
    if (io.*.KeyShift) {
        var first: ?usize = null;
        for (list, 0..) |on, i| {
            if (on) {
                first = i;
                break;
            }
        }
        if (first) |from| {
            const lo = @min(from, index);
            const hi = @max(from, index);
            @memset(list, false);
            for (lo..hi + 1) |k| list[k] = true;
            return;
        }
    }
    @memset(list, false);
    list[index] = true;
}

fn terrainTypeLabel(state: *State, buffer: []u8, tile: i32, weight: ?i32) [:0]const u8 {
    const name: []const u8 = if (tile >= 0 and @as(usize, @intCast(tile)) < state.fc_types.items.len) state.fc_types.items[@intCast(tile)].nameSlice() else "Unknown";
    if (weight) |w| return fmtZ(buffer, "({d}) {s}", .{ w, name });
    return fmtZ(buffer, "{s}", .{name});
}

fn reloadFieldTypes(state: *State) void {
    const slot = state.composers.fdoc.current.seasonSlot();
    if (slot == state.fc_types_slot) return;
    state.fc_types_slot = slot;
    state.fc_types.clearRetainingCapacity();
    state.fc_type_selected.clearRetainingCapacity();
    const types = state.editor.tilesetTypes(state.allocator, slot) catch return;
    defer state.allocator.free(types);
    state.fc_types.appendSlice(state.allocator, types) catch {};
}

fn loadHeightEdits(state: *State) void {
    const field = &state.composers.fdoc.current;
    var scratch: [5][96]u8 = undefined;
    const filled = [5][:0]const u8{
        fmtZ(&scratch[0], "{d:.2}", .{field.height}),
        fmtZ(&scratch[1], "{d}", .{field.pattern_min}),
        fmtZ(&scratch[2], "{d}", .{field.pattern_max}),
        fmtZ(&scratch[3], "{d:.2}", .{field.positive_ratio * 100.0}),
        fmtZ(&scratch[4], "{s}", .{field.profile}),
    };
    for (filled, 0..) |text, i| {
        @memset(&state.fc_edit[i], 0);
        const len = @min(text.len, state.fc_edit[i].len - 1);
        @memcpy(state.fc_edit[i][0..len], text[0..len]);
    }
}

fn readProfiles(state: *State) void {
    if (state.fc_profiles_read) return;
    state.fc_profiles_read = true;
    var total: usize = 0;
    _ = state.editor.bridge.listStorageFiles("scenarios\\profiles\\", ".tga", &.{}, &total);
    if (total == 0) return;
    const names = state.allocator.alloc(core.bridge.RmgName, total) catch return;
    defer state.allocator.free(names);
    var got: usize = 0;
    if (state.editor.bridge.listStorageFiles("scenarios\\profiles\\", ".tga", names, &got) != .ok) return;
    for (names[0..@min(got, names.len)]) |entry| {
        const full = entry.nameSlice();
        const bare = full[0 .. full.len - ".tga".len];
        const copy = state.allocator.dupe(u8, bare) catch return;
        state.fc_profiles.append(state.allocator, copy) catch {
            state.allocator.free(copy);
            return;
        };
    }
}

pub fn drawFieldsComposer(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.fields_composer_open) return;
    const composers = &state.composers;
    composers.ensureScanned(state.editor);
    commands.bindObjectLookup(state);
    var title_buffer: [256]u8 = undefined;
    const doc = &composers.fdoc;
    const title = composerTitle(&title_buffer, "Fields Composer", doc.name, doc.dirty, doc.shipped);
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin(title.ptr, &state.fields_composer_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;

    drawFileRow(state, "rmgf", core.composers.field_folder, composers.field_names.items, doc.name, doc.dirty, doc.shipped, doc.canUndo(), doc.canRedo(), &state.fc_open_filter, &state.fc_save_as_edit, &state.fc_popup, &state.fc_pending_cmd);
    if (composers.message().len != 0) {
        ig.igPushTextWrapPos(0);
        textCell("{s}", .{composers.message()});
        ig.igPopTextWrapPos();
    }
    if (doc.name.len == 0 and doc.current.season_folder.len == 0) {
        panels.text("Open a field set (the list scans the storages), or New.");
        if (ig.igButton("New field set")) _ = commands.run(state, "rmgf_new", "");
        drawFieldModals(state);
        drawDiscardModal(state, &state.fc_popup, &state.fc_pending_cmd);
        return;
    }
    drawFieldHeader(state);
    reloadFieldTypes(state);
    drawFieldTabs(state);
    drawFindings(state, if (composers.field_report) |*report| report else null, "rmgf_fix", "rmgf_fix_all", "Nothing found: every range, tile, object and the profile check out.");
    drawFieldModals(state);
    drawDiscardModal(state, &state.fc_popup, &state.fc_pending_cmd);
}

/// The open field set's own row: the MFC list's eight columns.
fn drawFieldHeader(state: *State) void {
    const f = &state.composers.fdoc.current;
    const columns = [_][:0]const u8{ "Path", "Season", "Terrain Shells", "Objects Shells", "Profile", "Height", "Pattern Size", "Positive Ratio %" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollX | ig.ImGuiTableFlags_SizingFixedFit;
    if (!ig.igBeginTableEx("##field_header", columns.len, flags, .{ .x = 0, .y = ig.igGetFrameHeight() * 2.6 }, 0)) return;
    defer ig.igEndTable();
    ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 230, 0);
    for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
    ig.igTableHeadersRow();
    ig.igTableNextRow();
    _ = ig.igTableSetColumnIndex(0);
    textCell("{s}", .{if (state.composers.fdoc.name.len == 0) "(new)" else core.composers.relativeName(core.composers.field_folder, state.composers.fdoc.name)});
    _ = ig.igTableSetColumnIndex(1);
    textCell("{s}", .{core.rmg.seasonName(f.season, f.season_folder)});
    _ = ig.igTableSetColumnIndex(2);
    textCell("{d}", .{f.tile_shells.items.len});
    _ = ig.igTableSetColumnIndex(3);
    textCell("{d}", .{f.object_shells.items.len});
    _ = ig.igTableSetColumnIndex(4);
    textCell("{s}", .{f.profile});
    _ = ig.igTableSetColumnIndex(5);
    textCell("{d:.2}", .{f.height});
    _ = ig.igTableSetColumnIndex(6);
    textCell("{d} - {d}", .{ f.pattern_min, f.pattern_max });
    _ = ig.igTableSetColumnIndex(7);
    textCell("{d:.2}", .{f.positive_ratio * 100.0});
}

fn drawFieldTabs(state: *State) void {
    const reserve: f32 = if (state.composers.field_report != null) 160 else 0;
    const avail = ig.igGetContentRegionAvail();
    _ = ig.igBeginChild("##field_tabs_area", .{ .x = 0, .y = @max(avail.y - reserve, 200) }, 0, 0);
    defer ig.igEndChild();
    if (!ig.igBeginTabBar("##field_tabs", 0)) return;
    defer ig.igEndTabBar();
    const tabs = [_]struct { tab: panels.FieldTab, label: [:0]const u8 }{
        .{ .tab = .terrain, .label = "Terrain" },
        .{ .tab = .objects, .label = "Objects" },
        .{ .tab = .heights, .label = "Heights" },
    };
    for (tabs) |entry| {
        const flags: c_int = if (state.fc_tab_request and state.fc_tab == entry.tab) ig.ImGuiTabItemFlags_SetSelected else 0;
        if (ig.igBeginTabItem(entry.label.ptr, null, flags)) {
            if (!state.fc_tab_request) state.fc_tab = entry.tab;
            switch (entry.tab) {
                .terrain => drawTerrainTab(state),
                .objects => drawObjectsTab(state),
                .heights => drawHeightsTab(state),
            }
            ig.igEndTabItem();
        }
    }
    state.fc_tab_request = false;
}

/// The shell list of one kind (the MFC's N / count / size columns, and the
/// step and probability for objects): a click chooses the shell, Ctrl+click
/// picks several. Add / Delete / Properties are under it.
fn drawShellList(state: *State, kind: usize, height: f32) void {
    const f = &state.composers.fdoc.current;
    const objects = kind == 1;
    const count = if (objects) f.object_shells.items.len else f.tile_shells.items.len;
    ensureSelection(state, &state.fc_shell_selected[kind], count);
    const selected = state.fc_shell_selected[kind].items;
    var picked_buffer: [core.bridge.rmg_max_shells]usize = undefined;
    const picked_count = selectedFrom(selected, &picked_buffer);
    if (ig.igButton(if (objects) "Add objects shell" else "Add shell")) _ = commands.run(state, "rmgf_shell_add", if (objects) "objects" else "terrain");
    ig.igSameLine();
    ig.igBeginDisabled(picked_count == 0);
    if (ig.igButton("Delete shell")) state.fc_popup = .delete_shells;
    ig.igSameLine();
    if (ig.igButton("Shell properties...")) openShellProperties(state, kind);
    ig.igEndDisabled();
    const columns: usize = if (objects) 5 else 3;
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_ScrollY | ig.ImGuiTableFlags_SizingFixedFit;
    if (ig.igBeginTableEx(if (objects) "##object_shells" else "##tile_shells", @intCast(columns), flags, .{ .x = 0, .y = height }, 0)) {
        const names = if (objects) [_][:0]const u8{ "N", "Objects Count", "Size", "Step", "Probability %" } else [_][:0]const u8{ "N", "Tiles Count", "Size", "", "" };
        for (names[0..columns]) |label| ig.igTableSetupColumn(label.ptr, 0);
        ig.igTableHeadersRow();
        for (0..count) |i| {
            ig.igTableNextRow();
            _ = ig.igTableSetColumnIndex(0);
            ig.igPushIDInt(@intCast(i));
            var label: [16:0]u8 = undefined;
            const label_z = fmtZ(&label, "{d}", .{i});
            if (ig.igSelectableEx(label_z.ptr, selected[i], ig.ImGuiSelectableFlags_SpanAllColumns | ig.ImGuiSelectableFlags_AllowDoubleClick, .{ .x = 0, .y = 0 })) {
                clickRow(state.fc_shell_selected[kind].items, i);
                // The chosen shell is the first of the selection (the MFC's focused one).
                state.fc_shell_chosen[kind] = false;
                for (state.fc_shell_selected[kind].items, 0..) |on, pick| {
                    if (on) {
                        state.fc_shell[kind] = pick;
                        state.fc_shell_chosen[kind] = true;
                        break;
                    }
                }
                state.fc_entry_selected[kind].clearRetainingCapacity();
                if (ig.igIsMouseDoubleClicked(0)) openShellProperties(state, kind);
            }
            ig.igPopID();
            if (objects) {
                const shell = f.object_shells.items[i];
                _ = ig.igTableSetColumnIndex(1);
                textCell("{d}", .{shell.objects.items.len});
                _ = ig.igTableSetColumnIndex(2);
                textCell("{d:.2}", .{shell.width});
                _ = ig.igTableSetColumnIndex(3);
                textCell("{d}", .{shell.step});
                _ = ig.igTableSetColumnIndex(4);
                textCell("{d:.2}", .{shell.ratio * 100.0});
            } else {
                const shell = f.tile_shells.items[i];
                _ = ig.igTableSetColumnIndex(1);
                textCell("{d}", .{shell.tiles.items.len});
                _ = ig.igTableSetColumnIndex(2);
                textCell("{d:.2}", .{shell.width});
            }
        }
        ig.igEndTable();
    }
}

fn openShellProperties(state: *State, kind: usize) void {
    const f = &state.composers.fdoc.current;
    var picked: [core.bridge.rmg_max_shells]usize = undefined;
    const n = selectedFrom(state.fc_shell_selected[kind].items, &picked);
    if (n == 0) return;
    // The common value of each field over the selection, "..." when they differ
    // (the MFC's CValuesCollector).
    var width_text: [32]u8 = undefined;
    var step_text: [32]u8 = undefined;
    var ratio_text: [32]u8 = undefined;
    var same_width = true;
    var same_step = true;
    var same_ratio = true;
    const first = picked[0];
    for (picked[0..n]) |i| {
        if (kind == 1) {
            const shell = f.object_shells.items[i];
            const base = f.object_shells.items[first];
            if (shell.width != base.width) same_width = false;
            if (shell.step != base.step) same_step = false;
            if (shell.ratio != base.ratio) same_ratio = false;
        } else if (f.tile_shells.items[i].width != f.tile_shells.items[first].width) same_width = false;
    }
    const base_width: f32 = if (kind == 1) f.object_shells.items[first].width else f.tile_shells.items[first].width;
    const widths = [3][:0]const u8{
        if (same_width) fmtZ(&width_text, "{d:.2}", .{base_width}) else "...",
        if (kind == 1 and same_step) fmtZ(&step_text, "{d}", .{f.object_shells.items[first].step}) else "...",
        if (kind == 1 and same_ratio) fmtZ(&ratio_text, "{d:.2}", .{f.object_shells.items[first].ratio * 100.0}) else "...",
    };
    for (widths, 0..) |text, i| {
        @memset(&state.fc_shell_edit[i], 0);
        @memcpy(state.fc_shell_edit[i][0..text.len], text);
    }
    state.fc_popup = .shell_properties;
}

fn drawTerrainTab(state: *State) void {
    const composers = &state.composers;
    const f = &composers.fdoc.current;
    // Season: the combo sets the season and its tileset folder.
    var current: [24:0]u8 = undefined;
    ig.igSetNextItemWidth(160);
    if (ig.igBeginCombo("Season", fmtZ(&current, "{s}", .{core.rmg.seasonName(f.season, f.season_folder)}).ptr, 0)) {
        for (core.rmg.season_names, 0..) |name, i| {
            var label: [24:0]u8 = undefined;
            if (ig.igSelectableEx(fmtZ(&label, "{s}", .{name}).ptr, f.seasonSlot() == i, 0, .{ .x = 0, .y = 0 })) {
                var arg: [8:0]u8 = undefined;
                _ = commands.run(state, "rmgf_season", fmtZ(&arg, "{d}", .{i}));
            }
        }
        ig.igEndCombo();
    }
    if (state.fc_types.items.len == 0) {
        ig.igSameLine();
        ig.igTextDisabled("(the tileset did not load: tile names are unknown)");
    }
    const avail = ig.igGetContentRegionAvail();
    const third = @max((avail.x - 24) / 3.0, 180);
    // Left: the tileset's terrain types, a click picks several.
    _ = ig.igBeginChild("##field_types", .{ .x = third, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    ensureSelection(state, &state.fc_type_selected, state.fc_types.items.len);
    var picked_types: [512]usize = undefined;
    const type_count = selectedFrom(state.fc_type_selected.items, &picked_types);
    var heading: [64]u8 = undefined;
    ig.igSeparatorText(fmtZ(&heading, "Available tiles ({d})", .{state.fc_types.items.len}).ptr);
    const chosen = state.fc_shell_chosen[0] and state.fc_shell[0] < f.tile_shells.items.len;
    ig.igBeginDisabled(!chosen or type_count == 0);
    if (ig.igButton("Add to shell ->")) {
        var tiles: [512]i32 = undefined;
        for (picked_types[0..type_count], 0..) |t, i| tiles[i] = @intCast(t);
        _ = composers.addShellTiles(state.fc_shell[0], tiles[0..type_count]) catch 0;
        state.view.setStatus("fields: ", composers.message());
    }
    ig.igEndDisabled();
    for (state.fc_types.items, 0..) |entry, i| {
        ig.igPushIDInt(@intCast(i));
        var label: [96:0]u8 = undefined;
        if (ig.igSelectableEx(fmtZ(&label, "{d}: {s}", .{ i, entry.nameSlice() }).ptr, state.fc_type_selected.items[i], ig.ImGuiSelectableFlags_AllowDoubleClick, .{ .x = 0, .y = 0 })) {
            clickRow(state.fc_type_selected.items, i);
            if (ig.igIsMouseDoubleClicked(0) and chosen) {
                var arg: [32:0]u8 = undefined;
                _ = commands.run(state, "rmgf_tile_add", fmtZ(&arg, "{d}:{d}", .{ state.fc_shell[0], i }));
            }
        }
        if (ig.igIsItemHovered(0)) {
            var tip: [96:0]u8 = undefined;
            ig.igSetTooltip("%s", fmtZ(&tip, "{s}: {d} tile variants", .{ entry.nameSlice(), entry.variant_count }).ptr);
        }
        ig.igPopID();
    }
    ig.igEndChild();
    ig.igSameLine();
    // Middle: the shells.
    _ = ig.igBeginChild("##field_shells", .{ .x = third, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    ig.igSeparatorText("Shells");
    drawShellList(state, 0, ig.igGetContentRegionAvail().y - 4);
    ig.igEndChild();
    ig.igSameLine();
    // Right: the tiles of the chosen shell with their weights.
    _ = ig.igBeginChild("##field_shell_tiles", .{ .x = 0, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    ig.igSeparatorText("Tiles of the shell");
    if (!chosen) {
        panels.text("Choose a shell.");
    } else {
        drawEntryList(state, 0);
    }
    ig.igEndChild();
}

/// The chosen shell's entries with their weights (terrain: tiles, objects:
/// objects): Remove and Properties (the weight) act on the selected rows.
fn drawEntryList(state: *State, kind: usize) void {
    const composers = &state.composers;
    const f = &composers.fdoc.current;
    const shell = state.fc_shell[kind];
    const objects = kind == 1;
    const count = if (objects) f.object_shells.items[shell].objects.items.len else f.tile_shells.items[shell].tiles.items.len;
    ensureSelection(state, &state.fc_entry_selected[kind], count);
    var picked: [core.bridge.rmg_max_shell_entries]usize = undefined;
    const picked_count = selectedFrom(state.fc_entry_selected[kind].items, picked[0..@min(picked.len, 2048)]);
    ig.igBeginDisabled(picked_count == 0);
    if (ig.igButton(if (objects) "Remove object" else "Remove tile")) {
        if (objects) {
            _ = composers.removeShellObjects(shell, picked[0..picked_count]) catch false;
        } else {
            _ = composers.removeShellTiles(shell, picked[0..picked_count]) catch false;
        }
        state.fc_entry_selected[kind].clearRetainingCapacity();
    }
    ig.igSameLine();
    if (ig.igButton("Properties...")) {
        @memset(&state.fc_weight_edit, 0);
        const first = picked[0];
        const weight: i32 = if (objects) f.object_shells.items[shell].objects.items[first].weight else f.tile_shells.items[shell].tiles.items[first].weight;
        var same = true;
        for (picked[1..picked_count]) |i| {
            const other: i32 = if (objects) f.object_shells.items[shell].objects.items[i].weight else f.tile_shells.items[shell].tiles.items[i].weight;
            if (other != weight) same = false;
        }
        var text: [32]u8 = undefined;
        const shown = if (same) fmtZ(&text, "{d}", .{weight}) else "...";
        @memcpy(state.fc_weight_edit[0..shown.len], shown);
        state.fc_popup = .entry_properties;
    }
    ig.igEndDisabled();
    ig.igSameLine();
    var counter: [64]u8 = undefined;
    ig.igTextDisabled("%s", fmtZ(&counter, "{d} entries, overall weight {d}", .{ count, entryWeight(f, kind, shell) }).ptr);
    _ = ig.igBeginChild("##entries", .{ .x = 0, .y = 0 }, 0, 0);
    for (0..count) |i| {
        ig.igPushIDInt(@intCast(i));
        var label: [160:0]u8 = undefined;
        const label_z = if (objects)
            fmtZ(&label, "({d}) {s}", .{ f.object_shells.items[shell].objects.items[i].weight, f.object_shells.items[shell].objects.items[i].name })
        else
            terrainTypeLabel(state, &label, f.tile_shells.items[shell].tiles.items[i].tile, f.tile_shells.items[shell].tiles.items[i].weight);
        if (ig.igSelectableEx(label_z.ptr, state.fc_entry_selected[kind].items[i], 0, .{ .x = 0, .y = 0 })) clickRow(state.fc_entry_selected[kind].items, i);
        ig.igPopID();
    }
    ig.igEndChild();
}

fn entryWeight(f: *const core.rmg.FieldSet, kind: usize, shell: usize) i64 {
    var sum: i64 = 0;
    if (kind == 1) {
        for (f.object_shells.items[shell].objects.items) |entry| sum += entry.weight;
    } else {
        for (f.tile_shells.items[shell].tiles.items) |entry| sum += entry.weight;
    }
    return sum;
}

fn drawObjectsTab(state: *State) void {
    const composers = &state.composers;
    const f = &composers.fdoc.current;
    // The filter combo over the D-31 filters, and a name filter (the MFC's own list had the combo).
    ig.igSetNextItemWidth(200);
    const filter_name = std.mem.sliceTo(&state.fc_filter, 0);
    var shown: [80:0]u8 = undefined;
    if (ig.igBeginCombo("Filter", fmtZ(&shown, "{s}", .{if (filter_name.len == 0) "(choose a filter)" else filter_name}).ptr, 0)) {
        for (state.editor.filtersSlice()) |*candidate| {
            var label: [80:0]u8 = undefined;
            const name = candidate.nameSlice();
            if (ig.igSelectableEx(fmtZ(&label, "{s}", .{name}).ptr, std.mem.eql(u8, name, filter_name), 0, .{ .x = 0, .y = 0 })) {
                var arg: [80:0]u8 = undefined;
                _ = commands.run(state, "rmgf_filter", fmtZ(&arg, "{s}", .{name}));
            }
        }
        ig.igEndCombo();
    }
    ig.igSameLine();
    ig.igSetNextItemWidth(160);
    _ = ig.igInputTextWithHint("##field_text_filter", "name contains", &state.fc_text_filter, state.fc_text_filter.len + 1, 0);
    panels.refreshAvailableObjects(state);
    ensureSelection(state, &state.fc_avail_selected, state.fc_avail.items.len);
    const avail = ig.igGetContentRegionAvail();
    const third = @max((avail.x - 24) / 3.0, 180);
    _ = ig.igBeginChild("##field_available", .{ .x = third, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    var heading: [64]u8 = undefined;
    ig.igSeparatorText(fmtZ(&heading, "Available objects ({d})", .{state.fc_avail.items.len}).ptr);
    var picked: [2048]usize = undefined;
    const picked_count = selectedFrom(state.fc_avail_selected.items, &picked);
    const chosen = state.fc_shell_chosen[1] and state.fc_shell[1] < f.object_shells.items.len;
    ig.igBeginDisabled(!chosen or picked_count == 0);
    if (ig.igButton("Add to shell ->")) {
        var names: [2048][]const u8 = undefined;
        for (picked[0..picked_count], 0..) |slot, i| names[i] = std.mem.sliceTo(&state.catalogue[state.fc_avail.items[slot]].name, 0);
        _ = composers.addShellObjects(state.fc_shell[1], names[0..picked_count]) catch 0;
        state.view.setStatus("fields: ", composers.message());
    }
    ig.igEndDisabled();
    if (filter_name.len == 0) panels.text("Choose a filter to list objects.");
    for (state.fc_avail.items, 0..) |catalogue_index, slot| {
        ig.igPushIDInt(@intCast(slot));
        const name = std.mem.sliceTo(&state.catalogue[catalogue_index].name, 0);
        var label: [96:0]u8 = undefined;
        if (ig.igSelectableEx(fmtZ(&label, "{s}", .{name}).ptr, state.fc_avail_selected.items[slot], ig.ImGuiSelectableFlags_AllowDoubleClick, .{ .x = 0, .y = 0 })) {
            clickRow(state.fc_avail_selected.items, slot);
            if (ig.igIsMouseDoubleClicked(0) and chosen) {
                var arg: [128:0]u8 = undefined;
                _ = commands.run(state, "rmgf_object_add", fmtZ(&arg, "{d}:{s}", .{ state.fc_shell[1], name }));
            }
        }
        if (ig.igIsItemHovered(0)) {
            var tip: [160:0]u8 = undefined;
            ig.igSetTooltip("%s", fmtZ(&tip, "{s}", .{std.mem.sliceTo(&state.catalogue[catalogue_index].path, 0)}).ptr);
        }
        ig.igPopID();
    }
    ig.igEndChild();
    ig.igSameLine();
    _ = ig.igBeginChild("##field_oshells", .{ .x = third, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    ig.igSeparatorText("Shells");
    drawShellList(state, 1, ig.igGetContentRegionAvail().y - 4);
    ig.igEndChild();
    ig.igSameLine();
    _ = ig.igBeginChild("##field_shell_objects", .{ .x = 0, .y = 0 }, ig.ImGuiChildFlags_Borders, 0);
    ig.igSeparatorText("Objects of the shell");
    if (!chosen) panels.text("Choose a shell.") else drawEntryList(state, 1);
    ig.igEndChild();
}

fn commitHeightField(state: *State, name: []const u8, slot: usize) void {
    var arg: [128:0]u8 = undefined;
    const typed = std.mem.sliceTo(&state.fc_edit[slot], 0);
    if (typed.len == 0) return;
    _ = commands.run(state, "rmgf_set", fmtZ(&arg, "{s}:{s}", .{ name, typed }));
    loadHeightEdits(state);
}

fn drawHeightsTab(state: *State) void {
    const composers = &state.composers;
    const f = &composers.fdoc.current;
    if (state.fc_edit_seen != composers.generation or state.fc_edit[0][0] == 0) {
        loadHeightEdits(state);
        state.fc_edit_seen = composers.generation;
    }
    readProfiles(state);
    ig.igSetNextItemWidth(320);
    _ = ig.igInputTextWithHint("Profile (.tga in the storages)", "scenarios\\profiles\\profile", &state.fc_edit[4], state.fc_edit[4].len + 1, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) commitHeightField(state, "profile", 4);
    ig.igSetNextItemWidth(320);
    if (ig.igBeginCombo("##profiles", "Browse the storages...", 0)) {
        for (state.fc_profiles.items) |name| {
            var label: [200:0]u8 = undefined;
            if (ig.igSelectableEx(fmtZ(&label, "{s}", .{name}).ptr, std.ascii.eqlIgnoreCase(name, f.profile), 0, .{ .x = 0, .y = 0 })) {
                var arg: [220:0]u8 = undefined;
                _ = commands.run(state, "rmgf_set", fmtZ(&arg, "profile:{s}", .{name}));
                loadHeightEdits(state);
            }
        }
        ig.igEndCombo();
    }
    ig.igSetNextItemWidth(100);
    _ = ig.igInputTextWithHint("Height (0 - 5)", null, &state.fc_edit[0], state.fc_edit[0].len + 1, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) commitHeightField(state, "height", 0);
    ig.igSetNextItemWidth(100);
    _ = ig.igInputTextWithHint("Pattern size min (1 - 16)", null, &state.fc_edit[1], state.fc_edit[1].len + 1, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) commitHeightField(state, "pattern_min", 1);
    ig.igSetNextItemWidth(100);
    _ = ig.igInputTextWithHint("Pattern size max (1 - 16)", null, &state.fc_edit[2], state.fc_edit[2].len + 1, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) commitHeightField(state, "pattern_max", 2);
    ig.igSetNextItemWidth(100);
    _ = ig.igInputTextWithHint("Positive ratio %", null, &state.fc_edit[3], state.fc_edit[3].len + 1, 0);
    if (ig.igIsItemDeactivatedAfterEdit()) commitHeightField(state, "positive", 3);
    var percent: f32 = f.positive_ratio * 100.0;
    ig.igSetNextItemWidth(320);
    if (ig.igSliderFloat("##positive_slider", &percent, 0, 100)) {}
    if (ig.igIsItemDeactivatedAfterEdit()) {
        var arg: [32:0]u8 = undefined;
        _ = commands.run(state, "rmgf_set", fmtZ(&arg, "positive:{d:.0}", .{percent}));
        loadHeightEdits(state);
    }
    ig.igTextDisabled("1 - only positive heights, 0 - only negative ones (the field's pattern ratio).");
}

fn drawFieldModals(state: *State) void {
    const composers = &state.composers;
    // Delete shells asks first ("Do you really want to DELETE selected ... shells?").
    if (modalIsShowing(state.fc_popup, .delete_shells, "Delete shells?")) {
        if (ig.igBeginPopupModal("Delete shells?", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            const kind: usize = if (state.fc_tab == .objects) 1 else 0;
            panels.text(if (kind == 1) "Do you really want to DELETE the selected objects shells?" else "Do you really want to DELETE the selected terrain shells?");
            if (ig.igButton("Yes")) {
                var picked: [core.bridge.rmg_max_shells]usize = undefined;
                const n = selectedFrom(state.fc_shell_selected[kind].items, &picked);
                _ = composers.removeFieldShells(kind == 1, picked[0..n]) catch false;
                state.fc_shell_selected[kind].clearRetainingCapacity();
                state.fc_entry_selected[kind].clearRetainingCapacity();
                state.fc_shell_chosen[kind] = false;
                state.fc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("No")) {
                state.fc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    // Shell properties: the width (and, for objects, the step and the percent) for every selected shell.
    if (modalIsShowing(state.fc_popup, .shell_properties, "Shell properties")) {
        if (ig.igBeginPopupModal("Shell properties", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            const kind: usize = if (state.fc_tab == .objects) 1 else 0;
            _ = ig.igInputTextWithHint("Width (VIS tiles)", null, &state.fc_shell_edit[0], state.fc_shell_edit[0].len + 1, 0);
            if (kind == 1) {
                _ = ig.igInputTextWithHint("Step (VIS tiles)", null, &state.fc_shell_edit[1], state.fc_shell_edit[1].len + 1, 0);
                _ = ig.igInputTextWithHint("Probability %", null, &state.fc_shell_edit[2], state.fc_shell_edit[2].len + 1, 0);
            }
            if (ig.igButton("OK")) {
                var picked: [core.bridge.rmg_max_shells]usize = undefined;
                const n = selectedFrom(state.fc_shell_selected[kind].items, &picked);
                const names = [3][]const u8{ "width", "step", "ratio" };
                for (picked[0..n]) |index| {
                    for (names, 0..) |field_name, slot| {
                        if (kind == 0 and slot != 0) continue;
                        const typed = std.mem.sliceTo(&state.fc_shell_edit[slot], 0);
                        if (typed.len == 0 or std.mem.eql(u8, typed, "...")) continue;
                        var arg: [80:0]u8 = undefined;
                        _ = commands.run(state, "rmgf_shell_set", fmtZ(&arg, "{s}:{d}:{s}:{s}", .{ if (kind == 1) "objects" else "terrain", index, field_name, typed }));
                    }
                }
                state.fc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("Cancel")) {
                state.fc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    // Entry properties: the weight of the selected tiles or objects.
    if (modalIsShowing(state.fc_popup, .entry_properties, "Weight")) {
        if (ig.igBeginPopupModal("Weight", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            const kind: usize = if (state.fc_tab == .objects) 1 else 0;
            const shell = state.fc_shell[kind];
            const f = &composers.fdoc.current;
            if (shell < (if (kind == 1) f.object_shells.items.len else f.tile_shells.items.len)) {
                var picked: [2048]usize = undefined;
                const n = selectedFrom(state.fc_entry_selected[kind].items, &picked);
                if (n == 1 and kind == 0) {
                    const tile = f.tile_shells.items[shell].tiles.items[picked[0]].tile;
                    var label: [96:0]u8 = undefined;
                    textCell("Name: {s}", .{terrainTypeLabel(state, &label, tile, null)});
                    if (tile >= 0 and @as(usize, @intCast(tile)) < state.fc_types.items.len) textCell("Variants: {d}", .{state.fc_types.items[@intCast(tile)].variant_count});
                } else if (n == 1) {
                    textCell("Object: {s}", .{f.object_shells.items[shell].objects.items[picked[0]].name});
                } else {
                    panels.text("Multiple selection...");
                }
                _ = ig.igInputTextWithHint("Weight", null, &state.fc_weight_edit, state.fc_weight_edit.len + 1, 0);
                if (ig.igButton("OK")) {
                    const typed = std.mem.sliceTo(&state.fc_weight_edit, 0);
                    if (typed.len != 0 and !std.mem.eql(u8, typed, "...")) {
                        for (picked[0..n]) |entry| {
                            var arg: [64:0]u8 = undefined;
                            _ = commands.run(state, if (kind == 1) "rmgf_object_weight" else "rmgf_tile_weight", fmtZ(&arg, "{d}:{d}:{s}", .{ shell, entry, typed }));
                        }
                    }
                    state.fc_popup = .none;
                    ig.igCloseCurrentPopup();
                }
                ig.igSameLine();
            }
            if (ig.igButton("Cancel")) {
                state.fc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
}

// ---------------------------------------------------------------------------
// The Templates Composer (M3 05-10, D-06/D-07/D-12): the MFC's
// RMG_CreateTemplateDialog as one dockable Tools window over core.composers'
// template: the header row's thirteen columns, the graphs, VSO and field set lists
// with their weights, Diplomacy... and Units... (the template's own unit creation,
// one entry per player - the map's Unit Creation Info is a different window), the
// script file and the MOD combo, and Check!, which the MFC left empty
// (RMG_CreateTemplateDialog.cpp:1269 OnCheckTemplatesButton) and this implements.
// Save writes the Template and, beside it, its QuickLoadMapInfo.
// ---------------------------------------------------------------------------

fn freeCells(state: *State) void {
    for (state.tc_cells.items) |text| state.allocator.free(text);
    state.tc_cells.clearRetainingCapacity();
}

fn pushCell(state: *State, text: []const u8) void {
    const copy = state.allocator.dupe(u8, text) catch return;
    state.tc_cells.append(state.allocator, copy) catch state.allocator.free(copy);
}

fn cell(state: *State, index: usize) []const u8 {
    return if (index < state.tc_cells.items.len) state.tc_cells.items[index] else "";
}

/// The cells of the lists that need a read of each graph and field set (their
/// counts, season, settings, script lists), rebuilt when the template or its file
/// moved: graphs first (seven cells each), then fields (two each).
fn rebuildTemplateCells(state: *State) void {
    const composers = &state.composers;
    freeCells(state);
    const t = &composers.tdoc.current;
    var scratch: [512]u8 = undefined;
    for (t.graphs.items) |entry| {
        var graph = state.editor.readGraphQuiet(entry.name) orelse {
            for (0..7) |_| pushCell(state, "?");
            continue;
        };
        defer graph.deinit(state.allocator);
        var number: [16]u8 = undefined;
        pushCell(state, fmtZ(&number, "{d}", .{graph.nodes.items.len}));
        pushCell(state, fmtZ(&number, "{d}", .{graph.links.items.len}));
        pushCell(state, core.rmg.seasonName(graph.season, graph.season_folder));
        pushCell(state, graph.season_folder);
        if (composers.graphSettingList(state.editor, &graph)) |names| {
            defer core.rmg.freeNames(state.allocator, names);
            pushCell(state, logic.namesText(&scratch, names));
        } else |_| pushCell(state, "");
        pushCell(state, logic.idsText(&scratch, graph.script_ids.items));
        pushCell(state, logic.areasText(&scratch, graph.script_areas.items));
    }
    for (t.fields.items) |entry| {
        var field = state.editor.readFieldSetQuiet(entry.name) orelse {
            pushCell(state, "?");
            pushCell(state, "?");
            continue;
        };
        defer field.deinit(state.allocator);
        var number: [16]u8 = undefined;
        pushCell(state, fmtZ(&number, "{d}", .{field.tile_shells.items.len}));
        pushCell(state, fmtZ(&number, "{d}", .{field.object_shells.items.len}));
    }
    composers.templateSettingsText(state.editor, &state.tc_settings_text) catch {};
    state.tc_cells_seen = composers.generation;
    state.tc_cells_valid = true;
}

/// "N / a / b": the Players cell - entries (players and the neutral), then the
/// players on side 0 and side 1 (SetTemplateItem).
fn playersText(buffer: []u8, t: *const core.rmg.Template) [:0]const u8 {
    const counts = t.sideCounts();
    return fmtZ(buffer, "{d} / {d} / {d}", .{ t.diplomacies.items.len, counts[0], counts[1] });
}

fn modKey(buffer: []u8, name: []const u8, version: []const u8) []const u8 {
    if (name.len == 0) return "";
    if (version.len == 0) return std.fmt.bufPrint(buffer, "{s}", .{name}) catch "";
    return std.fmt.bufPrint(buffer, "{s} {s}", .{ name, version }) catch "";
}

fn readScripts(state: *State) void {
    if (state.tc_scripts_read) return;
    state.tc_scripts_read = true;
    var total: usize = 0;
    _ = state.editor.bridge.listStorageFiles("scenarios\\scripts\\", ".lua", &.{}, &total);
    if (total == 0) return;
    const names = state.allocator.alloc(core.bridge.RmgName, total) catch return;
    defer state.allocator.free(names);
    var got: usize = 0;
    if (state.editor.bridge.listStorageFiles("scenarios\\scripts\\", ".lua", names, &got) != .ok) return;
    for (names[0..@min(got, names.len)]) |entry| {
        const full = entry.nameSlice();
        const copy = state.allocator.dupe(u8, full[0 .. full.len - ".lua".len]) catch return;
        state.tc_scripts.append(state.allocator, copy) catch {
            state.allocator.free(copy);
            return;
        };
    }
}

pub fn drawTemplatesComposer(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.templates_composer_open) return;
    const composers = &state.composers;
    composers.ensureScanned(state.editor);
    commands.bindObjectLookup(state);
    var title_buffer: [256]u8 = undefined;
    const doc = &composers.tdoc;
    const title = composerTitle(&title_buffer, "Templates Composer", doc.name, doc.dirty, doc.shipped);
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin(title.ptr, &state.templates_composer_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;

    drawFileRow(state, "rmgt", core.composers.template_folder, composers.template_names.items, doc.name, doc.dirty, doc.shipped, doc.canUndo(), doc.canRedo(), &state.tc_open_filter, &state.tc_save_as_edit, &state.tc_popup, &state.tc_pending_cmd);
    if (composers.message().len != 0) {
        ig.igPushTextWrapPos(0);
        textCell("{s}", .{composers.message()});
        ig.igPopTextWrapPos();
    }
    if (doc.name.len == 0 and doc.current.diplomacies.items.len == 0) {
        panels.text("Open a template (the list scans the storages), or New.");
        if (ig.igButton("New template")) _ = commands.run(state, "rmgt_new", "");
        drawTemplateModals(state);
        drawDiscardModal(state, &state.tc_popup, &state.tc_pending_cmd);
        return;
    }
    if (!state.tc_cells_valid or state.tc_cells_seen != composers.generation) rebuildTemplateCells(state);
    drawTemplateHeader(state);
    drawTemplateControls(state);
    const reserve: f32 = if (composers.template_report != null) 150 else 0;
    const avail = ig.igGetContentRegionAvail();
    _ = ig.igBeginChild("##template_lists", .{ .x = 0, .y = @max(avail.y - reserve, 220) }, 0, 0);
    drawTemplateGraphs(state);
    drawTemplateVsos(state);
    drawTemplateFields(state);
    ig.igEndChild();
    drawFindings(state, if (composers.template_report) |*report| report else null, "rmgt_fix", "rmgt_fix_all", "Nothing found: every graph, field set, descriptor and player checks out.");
    // A popup a command opened (rmgt_popup) loads what the button would have.
    if (state.tc_popup == .unit_grid and state.tc_popup_seen != .unit_grid) loadUcLists(state);
    if (state.tc_popup == .diplomacy and state.tc_popup_seen != .diplomacy) openDiplomacy(state);
    state.tc_popup_seen = state.tc_popup;
    drawTemplateModals(state);
    drawDiscardModal(state, &state.tc_popup, &state.tc_pending_cmd);
}

/// The open template's own row: the MFC list's thirteen columns.
fn drawTemplateHeader(state: *State) void {
    const t = &state.composers.tdoc.current;
    const columns = [_][:0]const u8{ "Path", "Size", "Season", "Players", "Graphs", "Fields", "VSO", "Season Folder", "Supported Settings", "Used Script IDs", "Used Script Areas", "Script Name", "MOD" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollX | ig.ImGuiTableFlags_SizingFixedFit;
    if (!ig.igBeginTableEx("##template_header", columns.len, flags, .{ .x = 0, .y = ig.igGetFrameHeight() * 2.6 }, 0)) return;
    defer ig.igEndTable();
    ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 200, 0);
    for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
    ig.igTableHeadersRow();
    ig.igTableNextRow();
    var scratch: [512]u8 = undefined;
    _ = ig.igTableSetColumnIndex(0);
    textCell("{s}", .{if (state.composers.tdoc.name.len == 0) "(new)" else core.composers.relativeName(core.composers.template_folder, state.composers.tdoc.name)});
    _ = ig.igTableSetColumnIndex(1);
    textCell("{d}x{d}", .{ t.size_x, t.size_y });
    _ = ig.igTableSetColumnIndex(2);
    textCell("{s}", .{core.rmg.seasonName(t.season, t.season_folder)});
    _ = ig.igTableSetColumnIndex(3);
    textCell("{s}", .{playersText(&scratch, t)});
    _ = ig.igTableSetColumnIndex(4);
    textCell("{d}", .{t.graphs.items.len});
    _ = ig.igTableSetColumnIndex(5);
    textCell("{d}", .{t.fields.items.len});
    _ = ig.igTableSetColumnIndex(6);
    textCell("{d}", .{t.vso.items.len});
    _ = ig.igTableSetColumnIndex(7);
    textCell("{s}", .{t.season_folder});
    _ = ig.igTableSetColumnIndex(8);
    textCell("{s}", .{state.tc_settings_text.items});
    _ = ig.igTableSetColumnIndex(9);
    textCell("{s}", .{logic.idsText(&scratch, t.script_ids.items)});
    _ = ig.igTableSetColumnIndex(10);
    textCell("{s}", .{logic.areasText(&scratch, t.script_areas.items)});
    _ = ig.igTableSetColumnIndex(11);
    textCell("{s}", .{t.script_file});
    _ = ig.igTableSetColumnIndex(12);
    var key: [160]u8 = undefined;
    textCell("{s}", .{modKey(&key, t.mod_name, t.mod_version)});
}

/// Diplomacy..., Units..., the script file (typed, or picked from the storages) and
/// the MOD combo.
fn drawTemplateControls(state: *State) void {
    const composers = &state.composers;
    const t = &composers.tdoc.current;
    if (ig.igButton("Diplomacy...")) state.tc_popup = .diplomacy;
    ig.igSameLine();
    if (ig.igButton("Units...")) state.tc_popup = .unit_grid;
    ig.igSameLine();
    ig.igTextDisabled("(the template's own unit creation: one entry per player)");
    // The script file: a storage name without ".lua".
    var script_buffer: [192:0]u8 = [_:0]u8{0} ** 192;
    const current_len = @min(t.script_file.len, script_buffer.len - 1);
    @memcpy(script_buffer[0..current_len], t.script_file[0..current_len]);
    ig.igSetNextItemWidth(360);
    if (ig.igInputTextWithHint("Script file", "scenarios\\scripts\\sa_13_14_15\\secure_area", &script_buffer, script_buffer.len + 1, 0)) {}
    if (ig.igIsItemDeactivatedAfterEdit()) {
        const typed = std.mem.sliceTo(&script_buffer, 0);
        _ = commands.run(state, "rmgt_script", if (typed.len == 0) "none" else typed);
    }
    ig.igSameLine();
    readScripts(state);
    ig.igSetNextItemWidth(170);
    if (ig.igBeginCombo("##script_browse", "Browse the storages...", 0)) {
        if (ig.igSelectableEx("(none)", t.script_file.len == 0, 0, .{ .x = 0, .y = 0 })) _ = commands.run(state, "rmgt_script", "none");
        for (state.tc_scripts.items) |name| {
            var label: [200:0]u8 = undefined;
            if (ig.igSelectableEx(fmtZ(&label, "{s}", .{name}).ptr, std.ascii.eqlIgnoreCase(name, t.script_file), 0, .{ .x = 0, .y = 0 })) {
                _ = commands.run(state, "rmgt_script", name);
            }
        }
        ig.igEndCombo();
    }
    // The MOD combo: none, or an installed mod ("name version"); a mod the template names that is not
    // installed stays as it is.
    var key: [160]u8 = undefined;
    var shown: [170:0]u8 = undefined;
    ig.igSetNextItemWidth(260);
    const label = modKey(&key, t.mod_name, t.mod_version);
    if (ig.igBeginCombo("MOD", fmtZ(&shown, "{s}", .{if (label.len == 0) "(none)" else label}).ptr, 0)) {
        panels.refreshModList(state);
        if (ig.igSelectableEx("(none)", label.len == 0, 0, .{ .x = 0, .y = 0 })) _ = commands.run(state, "rmgt_mod", "none");
        for (state.mod_list_buffer[0..state.mod_list_count]) |*mod| {
            var mod_key: [160]u8 = undefined;
            const text = modKey(&mod_key, std.mem.sliceTo(&mod.name, 0), std.mem.sliceTo(&mod.version, 0));
            var item: [170:0]u8 = undefined;
            if (ig.igSelectableEx(fmtZ(&item, "{s}", .{text}).ptr, std.mem.eql(u8, text, label), 0, .{ .x = 0, .y = 0 })) {
                _ = commands.run(state, "rmgt_mod", std.mem.sliceTo(&mod.folder, 0));
            }
        }
        ig.igEndCombo();
    }
}

fn openDiplomacy(state: *State) void {
    const t = &state.composers.tdoc.current;
    state.tc_dipl_count = @min(t.diplomacies.items.len, state.tc_dipl_sides.len);
    @memcpy(state.tc_dipl_sides[0..state.tc_dipl_count], t.diplomacies.items[0..state.tc_dipl_count]);
    state.tc_dipl_type = t.game_type;
    state.tc_dipl_attacking = t.attacking_side;
    state.tc_popup = .diplomacy;
}

/// A list's header buttons (Add, Delete, Properties) and the row selection of
/// list `list` (0 fields, 1 graphs, 2 vso) - the MFC's three identical button rows.
fn drawListButtons(state: *State, list: usize, noun: []const u8, count: usize) void {
    ensureSelection(state, &state.tc_selected[list], count);
    var picked: [core.bridge.rmg_max_weighted]usize = undefined;
    const picked_count = selectedFrom(state.tc_selected[list].items, &picked);
    var label: [48:0]u8 = undefined;
    if (ig.igButton(fmtZ(&label, "Add {s}...", .{noun}).ptr)) openPicker(state, list);
    ig.igSameLine();
    ig.igBeginDisabled(picked_count == 0);
    if (ig.igButton(fmtZ(&label, "Delete {s}", .{noun}).ptr)) {
        state.tc_list = list;
        state.tc_popup = .delete_entries;
    }
    ig.igSameLine();
    if (ig.igButton(fmtZ(&label, "{s} properties...", .{noun}).ptr)) openListProperties(state, list);
    ig.igEndDisabled();
}

fn openPicker(state: *State, list: usize) void {
    for (state.tc_picker_names.items) |name| state.allocator.free(name);
    state.tc_picker_names.clearRetainingCapacity();
    state.tc_picker_selected.clearRetainingCapacity();
    const composers = &state.composers;
    switch (list) {
        0 => for (composers.field_names.items) |name| {
            state.tc_picker_names.append(state.allocator, state.allocator.dupe(u8, name) catch continue) catch {};
        },
        1 => for (composers.graph_names.items) |name| {
            state.tc_picker_names.append(state.allocator, state.allocator.dupe(u8, name) catch continue) catch {};
        },
        else => {
            var total: usize = 0;
            _ = state.editor.bridge.listStorageFiles("terrain\\sets\\", ".xml", &.{}, &total);
            if (total != 0) {
                if (state.allocator.alloc(core.bridge.RmgName, total)) |names| {
                    defer state.allocator.free(names);
                    var got: usize = 0;
                    if (state.editor.bridge.listStorageFiles("terrain\\sets\\", ".xml", names, &got) == .ok) {
                        for (names[0..@min(got, names.len)]) |entry| {
                            const full = entry.nameSlice();
                            if (std.mem.indexOf(u8, full, "\\roads3d\\") == null and std.mem.indexOf(u8, full, "\\rivers3d\\") == null) continue;
                            state.tc_picker_names.append(state.allocator, state.allocator.dupe(u8, full[0 .. full.len - ".xml".len]) catch continue) catch {};
                        }
                    }
                } else |_| {}
            }
        },
    }
    state.tc_picker_selected.appendNTimes(state.allocator, false, state.tc_picker_names.items.len) catch {};
    @memset(&state.tc_picker_filter, 0);
    state.tc_list = list;
    state.tc_popup = .picker;
}

fn openListProperties(state: *State, list: usize) void {
    const t = &state.composers.tdoc.current;
    var picked: [core.bridge.rmg_max_weighted]usize = undefined;
    const n = selectedFrom(state.tc_selected[list].items, &picked);
    if (n == 0) return;
    const first = picked[0];
    var text: [32]u8 = undefined;
    var same = true;
    const weight_of = struct {
        fn at(tt: *const core.rmg.Template, which: usize, i: usize) i32 {
            return switch (which) {
                0 => tt.fields.items[i].weight,
                1 => tt.graphs.items[i].weight,
                else => tt.vso.items[i].weight,
            };
        }
    }.at;
    for (picked[1..n]) |i| {
        if (weight_of(t, list, i) != weight_of(t, list, first)) same = false;
    }
    const weight_text = if (same) fmtZ(&text, "{d}", .{weight_of(t, list, first)}) else "...";
    @memset(&state.tc_edit[0], 0);
    @memcpy(state.tc_edit[0][0..weight_text.len], weight_text);
    if (list == 2) {
        const entry = t.vso.items[first];
        @memset(&state.tc_edit[1], 0);
        @memset(&state.tc_edit[2], 0);
        const width_text = fmtZ(&text, "{d:.2}", .{entry.width / core.rmg.world_cell});
        @memcpy(state.tc_edit[1][0..width_text.len], width_text);
        var opacity: [32]u8 = undefined;
        const opacity_text = fmtZ(&opacity, "{d:.2}", .{entry.opacity * 100.0});
        @memcpy(state.tc_edit[2][0..opacity_text.len], opacity_text);
    }
    state.tc_default_edit = list == 0 and t.default_field == @as(i32, @intCast(first));
    state.tc_list = list;
    state.tc_popup = .template_list_properties;
}

fn listRowClick(state: *State, list: usize, index: usize) void {
    clickRow(state.tc_selected[list].items, index);
}

fn drawTemplateGraphs(state: *State) void {
    const t = &state.composers.tdoc.current;
    ig.igSeparatorText("Graphs");
    drawListButtons(state, 1, "graph", t.graphs.items.len);
    const columns = [_][:0]const u8{ "Path", "Weight", "Nodes", "Links", "Season", "Season Folder", "Supported Settings", "Used Script IDs", "Used Script Areas" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollY | ig.ImGuiTableFlags_ScrollX | ig.ImGuiTableFlags_SizingFixedFit;
    if (!ig.igBeginTableEx("##template_graphs", columns.len, flags, .{ .x = 0, .y = ig.igGetFrameHeight() * 5.5 }, 0)) return;
    defer ig.igEndTable();
    ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 290, 0);
    for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
    ig.igTableHeadersRow();
    for (t.graphs.items, 0..) |entry, i| {
        ig.igTableNextRow();
        _ = ig.igTableSetColumnIndex(0);
        ig.igPushIDInt(@intCast(i));
        var label: [260:0]u8 = undefined;
        if (ig.igSelectableEx(fmtZ(&label, "{s}", .{entry.name}).ptr, state.tc_selected[1].items[i], ig.ImGuiSelectableFlags_SpanAllColumns | ig.ImGuiSelectableFlags_AllowDoubleClick, .{ .x = 0, .y = 0 })) {
            listRowClick(state, 1, i);
            if (ig.igIsMouseDoubleClicked(0)) openListProperties(state, 1);
        }
        ig.igPopID();
        _ = ig.igTableSetColumnIndex(1);
        textCell("{d}", .{entry.weight});
        for (0..7) |c| {
            _ = ig.igTableSetColumnIndex(@intCast(2 + c));
            textCell("{s}", .{cell(state, i * 7 + c)});
        }
    }
}

fn drawTemplateVsos(state: *State) void {
    const t = &state.composers.tdoc.current;
    ig.igSeparatorText("VSO (roads and rivers)");
    drawListButtons(state, 2, "VSO", t.vso.items.len);
    const columns = [_][:0]const u8{ "Path", "Weight", "Width", "Opacity" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollY | ig.ImGuiTableFlags_SizingFixedFit;
    if (!ig.igBeginTableEx("##template_vsos", columns.len, flags, .{ .x = 0, .y = ig.igGetFrameHeight() * 3.5 }, 0)) return;
    defer ig.igEndTable();
    ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 330, 0);
    for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
    ig.igTableHeadersRow();
    for (t.vso.items, 0..) |entry, i| {
        ig.igTableNextRow();
        _ = ig.igTableSetColumnIndex(0);
        ig.igPushIDInt(@intCast(i));
        var label: [260:0]u8 = undefined;
        if (ig.igSelectableEx(fmtZ(&label, "{s}", .{entry.name}).ptr, state.tc_selected[2].items[i], ig.ImGuiSelectableFlags_SpanAllColumns | ig.ImGuiSelectableFlags_AllowDoubleClick, .{ .x = 0, .y = 0 })) {
            listRowClick(state, 2, i);
            if (ig.igIsMouseDoubleClicked(0)) openListProperties(state, 2);
        }
        ig.igPopID();
        _ = ig.igTableSetColumnIndex(1);
        textCell("{d}", .{entry.weight});
        _ = ig.igTableSetColumnIndex(2);
        textCell("{d:.2}", .{entry.width / core.rmg.world_cell});
        _ = ig.igTableSetColumnIndex(3);
        textCell("{d:.2}", .{entry.opacity * 100.0});
    }
}

fn drawTemplateFields(state: *State) void {
    const t = &state.composers.tdoc.current;
    ig.igSeparatorText("Field sets");
    drawListButtons(state, 0, "field set", t.fields.items.len);
    const columns = [_][:0]const u8{ "Path", "Weight", "Default", "Terrain Shells", "Objects Shells" };
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollY | ig.ImGuiTableFlags_SizingFixedFit;
    if (!ig.igBeginTableEx("##template_fields", columns.len, flags, .{ .x = 0, .y = ig.igGetFrameHeight() * 4.5 }, 0)) return;
    defer ig.igEndTable();
    ig.igTableSetupColumnEx(columns[0].ptr, ig.ImGuiTableColumnFlags_WidthFixed, 330, 0);
    for (columns[1..]) |label| ig.igTableSetupColumn(label.ptr, 0);
    ig.igTableHeadersRow();
    const graph_cells = t.graphs.items.len * 7;
    for (t.fields.items, 0..) |entry, i| {
        ig.igTableNextRow();
        _ = ig.igTableSetColumnIndex(0);
        ig.igPushIDInt(@intCast(i));
        var label: [260:0]u8 = undefined;
        if (ig.igSelectableEx(fmtZ(&label, "{s}", .{entry.name}).ptr, state.tc_selected[0].items[i], ig.ImGuiSelectableFlags_SpanAllColumns | ig.ImGuiSelectableFlags_AllowDoubleClick, .{ .x = 0, .y = 0 })) {
            listRowClick(state, 0, i);
            if (ig.igIsMouseDoubleClicked(0)) openListProperties(state, 0);
        }
        ig.igPopID();
        _ = ig.igTableSetColumnIndex(1);
        textCell("{d}", .{entry.weight});
        _ = ig.igTableSetColumnIndex(2);
        textCell("{s}", .{if (t.default_field == @as(i32, @intCast(i))) "Yes" else ""});
        _ = ig.igTableSetColumnIndex(3);
        textCell("{s}", .{cell(state, graph_cells + i * 2)});
        _ = ig.igTableSetColumnIndex(4);
        textCell("{s}", .{cell(state, graph_cells + i * 2 + 1)});
    }
}

fn drawTemplateModals(state: *State) void {
    const composers = &state.composers;
    const t = &composers.tdoc.current;
    const list_names = [3][:0]const u8{ "field sets", "graphs", "VSO" };
    // The picker: names of the storages, several at once.
    if (modalIsShowing(state.tc_popup, .picker, "Add to template")) {
        ig.igSetNextWindowSize(.{ .x = 560, .y = 440 }, ig.ImGuiCond_Appearing);
        if (ig.igBeginPopupModal("Add to template", null, 0)) {
            var heading: [96:0]u8 = undefined;
            panels.text(fmtZ(&heading, "Add {s} of the storages. Click to pick several.", .{list_names[state.tc_list]}));
            _ = ig.igInputTextWithHint("##tc_picker_filter", "filter", &state.tc_picker_filter, state.tc_picker_filter.len + 1, 0);
            const filter = std.mem.sliceTo(&state.tc_picker_filter, 0);
            _ = ig.igBeginChild("##tc_picker_list", .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() * 2 }, ig.ImGuiChildFlags_Borders, 0);
            var picked_count: usize = 0;
            for (state.tc_picker_names.items, 0..) |name, i| {
                if (i >= state.tc_picker_selected.items.len) break;
                if (state.tc_picker_selected.items[i]) picked_count += 1;
                if (!caseInsensitiveContains(name, filter)) continue;
                var label: [260:0]u8 = undefined;
                ig.igPushIDInt(@intCast(i));
                if (ig.igSelectableEx(fmtZ(&label, "{s}", .{name}).ptr, state.tc_picker_selected.items[i], 0, .{ .x = 0, .y = 0 })) state.tc_picker_selected.items[i] = !state.tc_picker_selected.items[i];
                ig.igPopID();
            }
            ig.igEndChild();
            var button: [48]u8 = undefined;
            if (ig.igButton(fmtZ(&button, "Add {d} selected", .{picked_count}).ptr)) {
                var chosen = std.ArrayListUnmanaged([]const u8).empty;
                defer chosen.deinit(state.allocator);
                for (state.tc_picker_names.items, 0..) |name, i| {
                    if (i < state.tc_picker_selected.items.len and state.tc_picker_selected.items[i]) chosen.append(state.allocator, name) catch break;
                }
                switch (state.tc_list) {
                    0 => _ = composers.addTemplateFields(state.editor, chosen.items) catch 0,
                    1 => _ = composers.addTemplateGraphs(state.editor, chosen.items) catch 0,
                    else => _ = composers.addTemplateVsos(state.editor, chosen.items) catch 0,
                }
                state.view.setStatus("templates: ", composers.message());
                state.tc_selected[state.tc_list].clearRetainingCapacity();
                state.tc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("Close")) {
                state.tc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    if (modalIsShowing(state.tc_popup, .delete_entries, "Delete from template?")) {
        if (ig.igBeginPopupModal("Delete from template?", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            var question: [96:0]u8 = undefined;
            panels.text(fmtZ(&question, "Do you really want to DELETE the selected {s}?", .{list_names[state.tc_list]}));
            if (ig.igButton("Yes")) {
                var picked: [core.bridge.rmg_max_weighted]usize = undefined;
                const n = selectedFrom(state.tc_selected[state.tc_list].items, &picked);
                _ = composers.removeTemplateEntries(@enumFromInt(state.tc_list), picked[0..n]) catch false;
                state.tc_selected[state.tc_list].clearRetainingCapacity();
                state.tc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("No")) {
                state.tc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    // Properties: the weight of every selected entry; the field set's Default tick;
    // the vso's width in cells and opacity in percent.
    if (modalIsShowing(state.tc_popup, .template_list_properties, "Properties")) {
        if (ig.igBeginPopupModal("Properties", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            var picked: [core.bridge.rmg_max_weighted]usize = undefined;
            const n = selectedFrom(state.tc_selected[state.tc_list].items, &picked);
            if (n == 1) {
                const name = switch (state.tc_list) {
                    0 => t.fields.items[picked[0]].name,
                    1 => t.graphs.items[picked[0]].name,
                    else => t.vso.items[picked[0]].name,
                };
                textCell("Path: {s}", .{name});
            } else panels.text("Path: Multiple selection...");
            var stats: [96]u8 = undefined;
            var overall: i64 = 0;
            const entries: usize = switch (state.tc_list) {
                0 => t.fields.items.len,
                1 => t.graphs.items.len,
                else => t.vso.items.len,
            };
            for (0..entries) |i| overall += switch (state.tc_list) {
                0 => t.fields.items[i].weight,
                1 => t.graphs.items[i].weight,
                else => t.vso.items[i].weight,
            };
            ig.igTextDisabled("%s", fmtZ(&stats, "Overall weight: {d}, average weight: {d:.2}", .{ overall, if (entries == 0) 0.0 else @as(f64, @floatFromInt(overall)) / @as(f64, @floatFromInt(entries)) }).ptr);
            _ = ig.igInputTextWithHint("Weight", null, &state.tc_edit[0], state.tc_edit[0].len + 1, 0);
            if (state.tc_list == 0) _ = ig.igCheckbox("Default field", &state.tc_default_edit);
            if (state.tc_list == 2) {
                _ = ig.igInputTextWithHint("Width (cells)", null, &state.tc_edit[1], state.tc_edit[1].len + 1, 0);
                _ = ig.igInputTextWithHint("Opacity (%)", null, &state.tc_edit[2], state.tc_edit[2].len + 1, 0);
            }
            if (ig.igButton("OK")) {
                const weight_text = std.mem.sliceTo(&state.tc_edit[0], 0);
                const list_name: []const u8 = switch (state.tc_list) {
                    0 => "fields",
                    1 => "graphs",
                    else => "vso",
                };
                for (picked[0..n]) |index| {
                    var arg: [96:0]u8 = undefined;
                    if (weight_text.len != 0 and !std.mem.eql(u8, weight_text, "...")) _ = commands.run(state, "rmgt_weight_set", fmtZ(&arg, "{s}:{d}:{s}", .{ list_name, index, weight_text }));
                    if (state.tc_list == 2) {
                        const width_text = std.mem.sliceTo(&state.tc_edit[1], 0);
                        const opacity_text = std.mem.sliceTo(&state.tc_edit[2], 0);
                        if (width_text.len != 0 and opacity_text.len != 0) _ = commands.run(state, "rmgt_vso_set", fmtZ(&arg, "{d}:{s}:{s}", .{ index, width_text, opacity_text }));
                    }
                }
                if (state.tc_list == 0 and n == 1) {
                    var arg: [16:0]u8 = undefined;
                    const is_default = t.default_field == @as(i32, @intCast(picked[0]));
                    if (state.tc_default_edit and !is_default) _ = commands.run(state, "rmgt_default_field", fmtZ(&arg, "{d}", .{picked[0]}));
                    if (!state.tc_default_edit and is_default) _ = commands.run(state, "rmgt_default_field", "-1");
                }
                state.tc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igSameLine();
            if (ig.igButton("Cancel")) {
                state.tc_popup = .none;
                ig.igCloseCurrentPopup();
            }
            ig.igEndPopup();
        }
    }
    drawDiplomacyPopup(state);
    drawUnitGrid(state);
}

/// Diplomacy...: the table of sides (players, then the neutral), the game type and
/// the attacking side; OK applies it as one step and the unit creation follows the
/// player count (CTabSimpleObjectsDiplomacyDialog / OnTemplateDiplomacyButton).
fn drawDiplomacyPopup(state: *State) void {
    if (!modalIsShowing(state.tc_popup, .diplomacy, "Diplomacy")) return;
    if (!ig.igBeginPopupModal("Diplomacy", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
    defer ig.igEndPopup();
    const players = if (state.tc_dipl_count == 0) 0 else state.tc_dipl_count - 1;
    var remove: ?usize = null;
    for (0..players) |i| {
        ig.igPushIDInt(@intCast(i));
        var label: [24:0]u8 = undefined;
        ig.igTextUnformatted(fmtZ(&label, "Player {d}", .{i}).ptr);
        ig.igSameLine();
        if (ig.igRadioButton("Side 0", state.tc_dipl_sides[i] == 0)) state.tc_dipl_sides[i] = 0;
        ig.igSameLine();
        if (ig.igRadioButton("Side 1", state.tc_dipl_sides[i] == 1)) state.tc_dipl_sides[i] = 1;
        ig.igSameLine();
        ig.igBeginDisabled(players <= 2);
        if (ig.igSmallButton("Delete")) remove = i;
        ig.igEndDisabled();
        ig.igPopID();
    }
    if (remove) |index| {
        var at = index;
        while (at + 1 < state.tc_dipl_count) : (at += 1) state.tc_dipl_sides[at] = state.tc_dipl_sides[at + 1];
        state.tc_dipl_count -= 1;
    }
    panels.text("Neutral");
    ig.igBeginDisabled(state.tc_dipl_count >= core.rmg.max_diplomacies);
    if (ig.igButton("Add a player on side 0")) {
        state.tc_dipl_sides[state.tc_dipl_count] = 2;
        state.tc_dipl_sides[state.tc_dipl_count - 1] = 0;
        state.tc_dipl_count += 1;
    }
    ig.igSameLine();
    if (ig.igButton("Add a player on side 1")) {
        state.tc_dipl_sides[state.tc_dipl_count] = 2;
        state.tc_dipl_sides[state.tc_dipl_count - 1] = 1;
        state.tc_dipl_count += 1;
    }
    ig.igEndDisabled();
    ig.igSeparator();
    var shown: [32:0]u8 = undefined;
    const type_index: usize = @intCast(std.math.clamp(state.tc_dipl_type, 0, @as(i32, core.rmg.game_type_names.len) - 1));
    if (ig.igBeginCombo("Type", fmtZ(&shown, "{s}", .{core.rmg.game_type_names[type_index]}).ptr, 0)) {
        for (core.rmg.game_type_names, 0..) |name, i| {
            var item: [32:0]u8 = undefined;
            if (ig.igSelectableEx(fmtZ(&item, "{s}", .{name}).ptr, i == type_index, 0, .{ .x = 0, .y = 0 })) state.tc_dipl_type = @intCast(i);
        }
        ig.igEndCombo();
    }
    panels.text("Attacking side");
    ig.igSameLine();
    if (ig.igRadioButton("0##attacking", state.tc_dipl_attacking == 0)) state.tc_dipl_attacking = 0;
    ig.igSameLine();
    if (ig.igRadioButton("1##attacking", state.tc_dipl_attacking == 1)) state.tc_dipl_attacking = 1;
    if (ig.igButton("OK")) {
        _ = state.composers.setTemplateDiplomacy(state.tc_dipl_sides[0..state.tc_dipl_count], state.tc_dipl_type, state.tc_dipl_attacking) catch false;
        state.view.setStatus("templates: ", state.composers.message());
        state.tc_popup = .none;
        ig.igCloseCurrentPopup();
    }
    ig.igSameLine();
    if (ig.igButton("Cancel")) {
        state.tc_popup = .none;
        ig.igCloseCurrentPopup();
    }
}

fn unitInt(state: *State, player: usize, label: [*:0]const u8, field_name: []const u8, current: i32) void {
    var value: c_int = current;
    _ = ig.igInputIntEx(label, &value, 0, 0, 0);
    if (ig.igIsItemDeactivatedAfterEdit() and value != current) {
        var arg: [96:0]u8 = undefined;
        _ = commands.run(state, "rmgt_units_set", fmtZ(&arg, "{d}:{s}={d}", .{ player, field_name, value }));
    }
}

fn unitName(state: *State, player: usize, label: [*:0]const u8, field_name: []const u8, current: []const u8, list_index: usize) void {
    var chosen: [core.records.uc_name_capacity]u8 = undefined;
    if (drawNameCombo(label, current, state.uc_lists[list_index], &chosen)) {
        var arg: [160:0]u8 = undefined;
        _ = commands.run(state, "rmgt_units_set", fmtZ(&arg, "{d}:{s}={s}", .{ player, field_name, std.mem.sliceTo(&chosen, 0) }));
    }
}

/// Units...: the template's own unit creation (CTemplateUnitsDialog over
/// rTemplate.unitCreation), one entry per player - party, the five aviation slots,
/// the paratroop squad, the relax time and the appear points (listed in tiles).
/// Every control is a command (rmgt_units_set, rmgt_appear_*).
fn drawUnitGrid(state: *State) void {
    if (!modalIsShowing(state.tc_popup, .unit_grid, "Template units")) return;
    ig.igSetNextWindowSize(.{ .x = 640, .y = 560 }, ig.ImGuiCond_Appearing);
    if (!ig.igBeginPopupModal("Template units", null, 0)) return;
    defer ig.igEndPopup();
    const t = &state.composers.tdoc.current;
    ig.igPushTextWrapPos(0);
    panels.text("This is the template's own unit creation: what every map generated from it starts with, one entry per player. The map's Unit Creation Info window edits an open map's, not this.");
    ig.igPopTextWrapPos();
    _ = ig.igBeginChild("##unit_grid", .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() }, ig.ImGuiChildFlags_Borders, 0);
    for (t.units.items, 0..) |unit, player| {
        ig.igPushIDInt(@intCast(player));
        defer ig.igPopID();
        var header: [48:0]u8 = undefined;
        if (!ig.igCollapsingHeader(fmtZ(&header, "Player {d} - {s}", .{ player, unit.partySlice() }).ptr, if (player == 0) ig.ImGuiTreeNodeFlags_DefaultOpen else 0)) continue;
        unitName(state, player, "Party", "party", unit.partySlice(), 0);
        for (unit.aircraft, 0..) |slot, index| {
            ig.igPushIDInt(@intCast(index));
            defer ig.igPopID();
            panels.text(core.records.uc_aircraft_labels[index]);
            var name_field: [24]u8 = undefined;
            ig.igSetNextItemWidth(200);
            unitName(state, player, "##aircraft", std.fmt.bufPrint(&name_field, "aircraft{d}_name", .{index}) catch "", slot.nameSlice(), 1);
            ig.igSameLine();
            ig.igSetNextItemWidth(80);
            var field: [28]u8 = undefined;
            unitInt(state, player, "formation##f", std.fmt.bufPrint(&field, "aircraft{d}_formation", .{index}) catch "", slot.formation_size);
            ig.igSameLine();
            ig.igSetNextItemWidth(80);
            unitInt(state, player, "count##c", std.fmt.bufPrint(&field, "aircraft{d}_count", .{index}) catch "", slot.count);
        }
        unitName(state, player, "Paratroop squad", "paratroop_name", unit.paratroopSlice(), 2);
        unitInt(state, player, "Squads count", "paratroop_count", unit.paratroop_count);
        unitInt(state, player, "Relax time (s)", "relax", unit.relax_time);
        panels.text("Appear points (tiles)");
        var remove: ?usize = null;
        for (unit.appearSlice(), 0..) |point, index| {
            ig.igPushIDInt(@intCast(1000 + index));
            defer ig.igPopID();
            var tile_x = point.x / core.rmg.appear_units_per_tile;
            var tile_y = point.y / core.rmg.appear_units_per_tile;
            ig.igSetNextItemWidth(90);
            _ = ig.igInputFloatEx("x##p", &tile_x, 0, 0, "%.2f", 0);
            const x_done = ig.igIsItemDeactivatedAfterEdit();
            ig.igSameLine();
            ig.igSetNextItemWidth(90);
            _ = ig.igInputFloatEx("y##p", &tile_y, 0, 0, "%.2f", 0);
            const y_done = ig.igIsItemDeactivatedAfterEdit();
            ig.igSameLine();
            if (ig.igButton("Remove")) remove = index;
            if (x_done or y_done) {
                var arg: [96:0]u8 = undefined;
                _ = commands.run(state, "rmgt_appear_set", fmtZ(&arg, "{d}:{d}:{d:.1}:{d:.1}", .{ player, index, tile_x * core.rmg.appear_units_per_tile, tile_y * core.rmg.appear_units_per_tile }));
            }
        }
        if (unit.appear_count == 0) panels.text("no appear points");
        if (ig.igButton("Add appear point")) {
            // At the middle of the template's map (or the origin while it has no size).
            var arg: [96:0]u8 = undefined;
            const half = core.rmg.appear_units_per_patch * 0.5;
            _ = commands.run(state, "rmgt_appear_add", fmtZ(&arg, "{d}:{d:.0}:{d:.0}", .{ player, @as(f32, @floatFromInt(t.size_x)) * half, @as(f32, @floatFromInt(t.size_y)) * half }));
        }
        if (remove) |index| {
            var arg: [32:0]u8 = undefined;
            _ = commands.run(state, "rmgt_appear_del", fmtZ(&arg, "{d}:{d}", .{ player, index }));
        }
    }
    ig.igEndChild();
    if (ig.igButton("Close")) {
        state.tc_popup = .none;
        ig.igCloseCurrentPopup();
    }
}

// ---------------------------------------------------------------------------
// 05-11 (D-34): Tools > Options, Help > Keys and tools, Help > About
// ---------------------------------------------------------------------------

/// Tools > Options (PARITY T2, the MFC's CMapEditorOptionsDialog): loads the
/// window's two fields fresh from the settings, so a stale edit is never shown.
pub fn openOptionsWindow(state: *State) void {
    state.options_open = true;
    @memset(&state.options_params_edit, 0);
    const text = state.settings.gameParameters();
    @memcpy(state.options_params_edit[0..text.len], text);
    state.options_format_edit = state.settings.default_format;
}

/// OK: both fields in one settings write (the file is written once, by the
/// run loop, after this frame; nothing is written when nothing changed).
pub fn commitOptions(state: *State) void {
    if (logic.applyOptions(&state.settings, std.mem.sliceTo(&state.options_params_edit, 0), state.options_format_edit)) state.settings_changed = true;
}

/// The MFC's two Options controls: the game's extra command line (Test in game
/// puts it before the map name, exactly as the MFC's `<parameters> -<map>`) and
/// the default save format, here the format a Save As of a name with no .bzm or
/// .xml extension gets (the format of an open file follows its own extension).
pub fn drawOptionsWindow(state: *State) void {
    if (!state.options_open) return;
    ig.igSetNextWindowPos(.{ .x = 420, .y = 160 }, ig.ImGuiCond_FirstUseEver);
    if (!ig.igBegin("Options", &state.options_open, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        ig.igEnd();
        return;
    }
    defer ig.igEnd();
    ig.igTextDisabled("Test in game starts the game with these extra parameters, before the map name.");
    _ = ig.igInputTextWithHint("Game parameters", "e.g. -nosound", &state.options_params_edit, state.options_params_edit.len + 1, 0);
    ig.igSeparatorText("Default save format");
    if (ig.igRadioButton("BZM (binary)", state.options_format_edit == .bzm)) state.options_format_edit = .bzm;
    ig.igSameLine();
    if (ig.igRadioButton("XML", state.options_format_edit == .xml)) state.options_format_edit = .xml;
    ig.igTextDisabled("Used by Save As when the name has no .bzm or .xml extension.");
    if (ig.igButton("OK")) {
        commitOptions(state);
        state.options_open = false;
    }
    ig.igSameLine();
    if (ig.igButton("Cancel")) state.options_open = false;
}

/// Help > Keys and tools (PARITY H1; the MFC's .chm is not shipped): the tools
/// from tool_registry's own entries - label, key and the buttons each takes -
/// and the view's key and pointer map (view_math.key_help), so neither list is
/// typed twice.
pub fn drawHelpKeysWindow(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    if (!state.help_keys_open) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Keys and tools", &state.help_keys_open, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    ig.igSeparatorText("Tools");
    const flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_SizingFixedFit;
    if (ig.igBeginTableEx("##help_tools", 3, flags, .{ .x = 0, .y = 0 }, 0)) {
        ig.igTableSetupColumn("Tool", 0);
        ig.igTableSetupColumn("Key", 0);
        ig.igTableSetupColumn("Also takes", 0);
        ig.igTableHeadersRow();
        for (&tool_registry.entries) |*item| {
            if (item.hidden) continue;
            ig.igTableNextRow();
            _ = ig.igTableSetColumnIndex(0);
            ig.igTextUnformatted(item.label.ptr);
            _ = ig.igTableSetColumnIndex(1);
            var key_buffer: [2:0]u8 = undefined;
            const key = tool_registry.shortcutText(item, &key_buffer);
            ig.igTextUnformatted(if (key.len == 0) "-" else key.ptr);
            _ = ig.igTableSetColumnIndex(2);
            var extra_buffer: [96:0]u8 = undefined;
            ig.igTextUnformatted(helpToolExtras(&extra_buffer, item).ptr);
        }
        ig.igEndTable();
    }
    ig.igSeparatorText("Keys and pointer");
    if (ig.igBeginTableEx("##help_keys", 2, flags | ig.ImGuiTableFlags_ScrollX, .{ .x = 0, .y = 0 }, 0)) {
        for (view_math.key_help) |line| {
            ig.igTableNextRow();
            _ = ig.igTableSetColumnIndex(0);
            ig.igTextUnformatted(line.keys.ptr);
            _ = ig.igTableSetColumnIndex(1);
            ig.igTextUnformatted(line.what.ptr);
        }
        ig.igEndTable();
    }
    if (ig.igButton("Close")) state.help_keys_open = false;
}

/// What a tool takes beyond the left button, from its registry flags.
pub fn helpToolExtras(buffer: *[96:0]u8, item: *const tool_registry.Entry) [:0]const u8 {
    var len: usize = 0;
    const parts = [_]struct { on: bool, text: []const u8 }{
        .{ .on = item.needs_right_button, .text = "right button" },
        .{ .on = item.ctrl_click_is_right, .text = "Ctrl+click as right" },
        .{ .on = item.needs_double_click, .text = "double click" },
    };
    for (parts) |part| {
        if (!part.on) continue;
        const written = std.fmt.bufPrint(buffer[len..95], "{s}{s}", .{ if (len == 0) "" else ", ", part.text }) catch break;
        len += written.len;
    }
    buffer[len] = 0;
    return buffer[0..len :0];
}

/// Help > About (PARITY H2): the product line, the milestone, where the design
/// is written, the source and the licence.
pub fn drawAboutWindow(state: *State) void {
    if (!state.about_open) return;
    ig.igSetNextWindowPos(.{ .x = 440, .y = 180 }, ig.ImGuiCond_FirstUseEver);
    if (!ig.igBegin("About", &state.about_open, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
        ig.igEnd();
        return;
    }
    defer ig.igEnd();
    ig.igTextUnformatted(logic.about_title);
    ig.igTextDisabled(logic.about_version);
    ig.igSeparator();
    ig.igTextUnformatted(logic.about_spec);
    ig.igTextUnformatted(logic.about_source);
    ig.igTextWrapped("%s", logic.about_license);
    if (ig.igButton("Close")) state.about_open = false;
}
