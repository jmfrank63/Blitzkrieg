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
/// atan2 with y up) through the `wheel_turn` command, which sets the
/// placer's placement angle and turns the selection with it. Q/E keep their
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
    // selection merge into ONE undo step (`wheel_turn` reads the gesture).
    if (ig.igIsItemActivated()) state.wheel_gesture = state.editor.beginGesture();
    if (active and ig.igIsMouseDown(0)) {
        const mouse = ig.igGetIO().*.MousePos;
        const angle = logic.wheelAngleDegrees(centre, .{ mouse.x, mouse.y });
        const old_angle: i32 = @intFromFloat(logic.directionToDegrees(state.view.placer.dir));
        if (angle != @mod(old_angle, 360)) {
            var buffer: [16:0]u8 = undefined;
            _ = commands.run(state, "wheel_turn", std.fmt.bufPrintZ(&buffer, "{d}", .{angle}) catch "");
        }
    }
    if (!active) state.wheel_gesture = 0;
    if (hovered) {
        ig.igSetTooltip("Direction: drag to set the placement angle; turns the selection too");
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
