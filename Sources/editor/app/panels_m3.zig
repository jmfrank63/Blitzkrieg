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
