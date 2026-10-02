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
pub fn drawRmgDialog(state: *State) void {
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
