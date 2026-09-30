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
    if (ig.igBeginCombo("##level_mode", modes[@intCast(@intFromEnum(tool.level_mode))], 0)) {
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
    var gen_type: usize = switch (state.heights_generate_type) {
        .hills => 0,
        .rocks => 1,
        .dunes => 2,
    };
    if (ig.igBeginCombo("##generate_type", gen_types[gen_type], 0)) {
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
