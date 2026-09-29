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
