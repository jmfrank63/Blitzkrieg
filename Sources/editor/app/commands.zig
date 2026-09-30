//! Named commands and predicates (04-03, D-24's "verbs" item). One table that
//! the menus, the panels' buttons and BK_EDITOR_AUTO's `do=<name>[:<arg>]` and
//! `expect=<name>[:<arg>]` all call, so a button never carries logic a script
//! cannot reach and a scenario never needs ImGui text entry. A later plan adds
//! its commands and predicates to the two tables below and nothing else.
//!
//! A command changes the document through the editor (one undo step each) and
//! answers `Outcome`; a predicate only reads. Names are `[A-Za-z0-9_]{1,32}`
//! and arguments printable ASCII without comma or space (auto.zig's grammar).
const std = @import("std");
const core = @import("editor_core");
const panels = @import("panels.zig");
const logic = @import("panels_logic.zig");
const marker_logic = @import("marker_logic.zig");
const testlaunch = @import("testlaunch.zig");

const State = panels.State;
const records = core.records;

/// What a command or predicate said. `refused`: the editor said no and its
/// status line holds why (or a predicate is false; for `check`, a false
/// predicate is `refused` too). `unknown_name` and `bad_arg` are the caller's
/// mistake.
pub const Outcome = enum { ok, refused, unknown_name, bad_arg };

pub const Handler = *const fn (state: *State, arg: []const u8) Outcome;

pub const Entry = struct { name: []const u8, handler: Handler };

pub const command_table = [_]Entry{
    .{ .name = "camera_player", .handler = cameraPlayer },
    .{ .name = "camera_neutral", .handler = cameraNeutral },
    .{ .name = "camera_clear", .handler = cameraClear },
    .{ .name = "camera_goto", .handler = cameraGoto },
    .{ .name = "vso_kind", .handler = vsoKind },
    .{ .name = "vso_desc", .handler = vsoDesc },
    .{ .name = "vso_width", .handler = vsoWidth },
    .{ .name = "vso_opacity", .handler = vsoOpacity },
    .{ .name = "vso_width_mode", .handler = vsoWidthMode },
    .{ .name = "bridge_desc", .handler = bridgeDesc },
    .{ .name = "bridge_rotate", .handler = bridgeRotate },
    .{ .name = "bridge_toggle_build", .handler = bridgeToggleBuild },
    .{ .name = "bridge_delete", .handler = bridgeDelete },
    .{ .name = "fence_desc", .handler = fenceDesc },
    .{ .name = "trench_player", .handler = trenchPlayer },
    .{ .name = "trench_delete", .handler = trenchDelete },
    .{ .name = "script_id", .handler = scriptId },
    .{ .name = "group_new", .handler = groupNew },
    .{ .name = "group_add_id", .handler = groupAddId },
    .{ .name = "group_remove_id", .handler = groupRemoveId },
    .{ .name = "group_delete", .handler = groupDelete },
    .{ .name = "group_hide", .handler = groupHide },
    .{ .name = "group_select", .handler = groupSelect },
    .{ .name = "groups_window", .handler = groupsWindow },
    .{ .name = "script_file", .handler = scriptFile },
    .{ .name = "area_shape", .handler = areaShape },
    .{ .name = "area_name", .handler = areaName },
    .{ .name = "area_rename", .handler = areaRename },
    .{ .name = "area_delete", .handler = areaDelete },
    .{ .name = "script_dialog", .handler = scriptDialog },
    .{ .name = "script_choose", .handler = scriptChoose },
    .{ .name = "script_open", .handler = scriptOpen },
    .{ .name = "script_copy_along_yes", .handler = scriptCopyAlongYes },
    .{ .name = "script_copy_along_no", .handler = scriptCopyAlongNo },
    .{ .name = "script_overwrite_yes", .handler = scriptOverwriteYes },
    .{ .name = "script_overwrite_no", .handler = scriptOverwriteNo },
    .{ .name = "startcmd_add", .handler = startcmdAdd },
    .{ .name = "startcmd_type", .handler = startcmdType },
    .{ .name = "startcmd_number", .handler = startcmdNumber },
    .{ .name = "startcmd_add_unit", .handler = startcmdAddUnit },
    .{ .name = "startcmd_target_here", .handler = startcmdTargetHere },
    .{ .name = "startcmd_target_begin", .handler = startcmdTargetBegin },
    .{ .name = "startcmd_delete", .handler = startcmdDelete },
    .{ .name = "startcmd_select", .handler = startcmdSelect },
    .{ .name = "startcmds_window", .handler = startcmdsWindow },
    .{ .name = "reserve_mode", .handler = reserveMode },
    .{ .name = "reserve_pick_here", .handler = reservePickHere },
    .{ .name = "reserve_commit", .handler = reserveCommit },
    .{ .name = "reserve_delete", .handler = reserveDelete },
    .{ .name = "reserve_select", .handler = reserveSelect },
    .{ .name = "placer_role", .handler = placerRole },
    .{ .name = "placer_name", .handler = placerName },
    .{ .name = "ai_side", .handler = aiSide },
    .{ .name = "ai_parcel_here", .handler = aiParcelHere },
    .{ .name = "ai_point_here", .handler = aiPointHere },
    .{ .name = "ai_toggle_type", .handler = aiToggleType },
    .{ .name = "ai_delete", .handler = aiDelete },
    .{ .name = "ai_select", .handler = aiSelect },
    .{ .name = "ai_mobile_add", .handler = aiMobileAdd },
    .{ .name = "ai_mobile_remove", .handler = aiMobileRemove },
};

pub const predicate_table = [_]Entry{
    .{ .name = "anchor_set", .handler = anchorSet },
    .{ .name = "anchor_unset", .handler = anchorUnset },
    .{ .name = "undo_depth", .handler = undoDepth },
    .{ .name = "vso_delta", .handler = vsoDelta },
    .{ .name = "vso_points", .handler = vsoPoints },
    .{ .name = "bridge_delta", .handler = bridgeDelta },
    .{ .name = "bridge_built", .handler = bridgeBuilt },
    .{ .name = "fence_delta", .handler = fenceDelta },
    .{ .name = "trench_delta", .handler = trenchDelta },
    .{ .name = "script_id", .handler = scriptIdIs },
    .{ .name = "group_has", .handler = groupHas },
    .{ .name = "groups_delta", .handler = groupsDelta },
    .{ .name = "hidden_count", .handler = hiddenCount },
    .{ .name = "script_file", .handler = scriptFileIs },
    .{ .name = "script_beside", .handler = scriptBeside },
    .{ .name = "test_game_script", .handler = testGameScript },
    .{ .name = "areas_delta", .handler = areasDelta },
    .{ .name = "area_named", .handler = areaNamed },
    .{ .name = "startcmds_delta", .handler = startcmdsDelta },
    .{ .name = "startcmd_units", .handler = startcmdUnits },
    .{ .name = "startcmd_target", .handler = startcmdTargetIs },
    .{ .name = "startcmd_is", .handler = startcmdIs },
    .{ .name = "reserve_delta", .handler = reserveDelta },
    .{ .name = "reserve_pending", .handler = reservePending },
    .{ .name = "parcels", .handler = parcelsDelta },
    .{ .name = "mobile_has", .handler = mobileHas },
};

fn find(table: []const Entry, name: []const u8) ?Handler {
    for (table) |entry| {
        if (std.mem.eql(u8, entry.name, name)) return entry.handler;
    }
    return null;
}

/// Runs the named command. The status line already says why a `refused` was.
pub fn run(state: *State, name: []const u8, arg: []const u8) Outcome {
    const handler = find(&command_table, name) orelse return .unknown_name;
    return handler(state, arg);
}

/// Asks the named predicate: `ok` when it holds, `refused` when it does not.
pub fn check(state: *State, name: []const u8, arg: []const u8) Outcome {
    const handler = find(&predicate_table, name) orelse return .unknown_name;
    return handler(state, arg);
}

pub const neutral_slot: i32 = logic.neutral_anchor_slot;

fn parseSlot(arg: []const u8) ?i32 {
    return logic.parseAnchorSlot(arg);
}

fn resultOutcome(state: *State, result: core.bridge.EditError!void) Outcome {
    state.view.noteEditResult(state.editor, result);
    result catch return .refused;
    return .ok;
}

/// The anchors as the map holds them now (a fresh read, never the panel's
/// cache: a predicate the frame after a command must see the command).
pub fn readAnchors(state: *State) ?records.CameraAnchors {
    if (!panels.mapIsOpen(state.editor)) return null;
    var value: records.Value = undefined;
    if (state.editor.bridge.readRecord(.camera_anchors, 0, state.allocator, &value) != .ok) return null;
    defer value.deinit(state.allocator);
    return value.camera_anchors;
}

/// "Set camera for player N" / "Set neutral camera": the ground point under
/// the screen's centre becomes the anchor (D-22), one undo step. Public: the
/// Map menu and the anchors panel call it too.
pub fn setAnchorAtViewCentre(state: *State, slot: i32) Outcome {
    const editor = state.editor;
    if (!panels.mapIsOpen(editor)) return .refused;
    const screen = state.real.screenSize() orelse return .refused;
    const centre = editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0) catch return .refused;
    return resultOutcome(state, editor.setCameraAnchor(slot, centre.world_x, centre.world_y));
}

pub fn clearAnchor(state: *State, slot: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.clearCameraAnchor(slot));
}

/// Moves the view to the anchor (BkEditorSetCamera through the view). An
/// unset anchor is refused with a status line saying so.
pub fn gotoAnchor(state: *State, slot: i32) Outcome {
    const anchors = readAnchors(state) orelse return .refused;
    const anchor = if (slot == neutral_slot) anchors.neutral else anchors.slot(@intCast(slot));
    if (anchor.isUnset()) {
        state.view.setStatus("camera anchor: ", "that anchor is not set");
        return .refused;
    }
    state.view.centreOn(state.real, anchor.x, anchor.y);
    state.view.clearStatus();
    return .ok;
}

fn cameraPlayer(state: *State, arg: []const u8) Outcome {
    const slot = parseSlot(arg) orelse return .bad_arg;
    if (slot == neutral_slot) return .bad_arg;
    return setAnchorAtViewCentre(state, slot);
}

fn cameraNeutral(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return setAnchorAtViewCentre(state, neutral_slot);
}

fn cameraClear(state: *State, arg: []const u8) Outcome {
    const slot = parseSlot(arg) orelse return .bad_arg;
    return clearAnchor(state, slot);
}

fn cameraGoto(state: *State, arg: []const u8) Outcome {
    const slot = parseSlot(arg) orelse return .bad_arg;
    return gotoAnchor(state, slot);
}

fn anchorIsSet(state: *State, arg: []const u8) ?bool {
    const slot = parseSlot(arg) orelse return null;
    const anchors = readAnchors(state) orelse return false;
    const anchor = if (slot == neutral_slot) anchors.neutral else anchors.slot(@intCast(slot));
    return !anchor.isUnset();
}

fn anchorSet(state: *State, arg: []const u8) Outcome {
    const is_set = anchorIsSet(state, arg) orelse return .bad_arg;
    return if (is_set) .ok else .refused;
}

fn anchorUnset(state: *State, arg: []const u8) Outcome {
    const is_set = anchorIsSet(state, arg) orelse return .bad_arg;
    return if (is_set) .refused else .ok;
}

/// The number of undo entries: `undo_depth:1` after one anchor was set.
fn undoDepth(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return if (state.editor.history.undo_stack.items.len == want) .ok else .refused;
}

// ---------------------------------------------------------------------------
// Roads & Rivers (04-05, D-08): the panel's controls, as commands.
// ---------------------------------------------------------------------------

fn parseKind(text: []const u8) ?core.bridge.VsoKind {
    if (std.mem.eql(u8, text, "road")) return .road;
    if (std.mem.eql(u8, text, "river")) return .river;
    return null;
}

/// The Road / River switch: what a new line becomes. Switching drops the
/// unfinished line and the selection, as the MFC editor's two tools did, and
/// the type list follows the kind.
pub fn setVsoKind(state: *State, kind: core.bridge.VsoKind) Outcome {
    const tool = &state.view.roads_rivers;
    if (tool.kind == kind) return .ok;
    tool.kind = kind;
    tool.reset();
    state.refreshVsoTypes();
    return .ok;
}

/// A type from the panel's list, by its index there.
pub fn chooseVsoType(state: *State, index: usize) Outcome {
    state.refreshVsoTypes();
    if (index >= state.vso_types.len) return .bad_arg;
    state.view.roads_rivers.setDesc(state.vso_types[index].nameSlice());
    return .ok;
}

/// The panel's Delete: the whole selected road or river, one undo step.
pub fn deleteSelectedVso(state: *State) Outcome {
    const tool = &state.view.roads_rivers;
    const selected = tool.selected orelse return .refused;
    const result = state.editor.deleteVso(selected.kind, selected.index);
    if (result) |_| tool.reset() else |_| {}
    return resultOutcome(state, result);
}

fn vsoKind(state: *State, arg: []const u8) Outcome {
    const kind = parseKind(arg) orelse return .bad_arg;
    return setVsoKind(state, kind);
}

fn vsoDesc(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return chooseVsoType(state, index);
}

/// The panel's width (1..16 tiles): what the next line takes and, in the width
/// mode All with a line selected, the selected line's width too (04-13, the
/// MFC editor's CW_ALL); the calls that pass one `gesture` are one undo step.
/// Public: the panel's slider calls it.
pub fn setVsoWidthTiles(state: *State, width_tiles: f32, gesture: u32) Outcome {
    const tool = &state.view.roads_rivers;
    tool.width_tiles = width_tiles;
    if (!panels.mapIsOpen(state.editor)) return .ok;
    return resultOutcome(state, tool.applyPanelWidth(state.editor, gesture));
}

/// The panel's opacity (0..1), as `setVsoWidthTiles`.
pub fn setVsoOpacity(state: *State, opacity: f32, gesture: u32) Outcome {
    const tool = &state.view.roads_rivers;
    tool.opacity = opacity;
    if (!panels.mapIsOpen(state.editor)) return .ok;
    return resultOutcome(state, tool.applyPanelOpacity(state.editor, gesture));
}

/// The width spinner, 1..16 (the MFC tool's; w * fWorldCellSize / 2 world
/// units); one undo step when it re-widths the selected line.
fn vsoWidth(state: *State, arg: []const u8) Outcome {
    const width = std.fmt.parseInt(u8, arg, 10) catch return .bad_arg;
    if (width < 1 or width > 16) return .bad_arg;
    return setVsoWidthTiles(state, @floatFromInt(width), state.editor.beginGesture());
}

/// The opacity slider, 0..100 %; one undo step when it changes the selected line.
fn vsoOpacity(state: *State, arg: []const u8) Outcome {
    const percent = std.fmt.parseInt(u8, arg, 10) catch return .bad_arg;
    if (percent > 100) return .bad_arg;
    return setVsoOpacity(state, @as(f32, @floatFromInt(percent)) / 100.0, state.editor.beginGesture());
}

/// `do=vso_width_mode:single|multi|all`: the panel's width mode radio.
fn vsoWidthMode(state: *State, arg: []const u8) Outcome {
    const tool = &state.view.roads_rivers;
    if (std.mem.eql(u8, arg, "single")) {
        tool.width_mode = .single;
    } else if (std.mem.eql(u8, arg, "multi")) {
        tool.width_mode = .multi;
    } else if (std.mem.eql(u8, arg, "all")) {
        tool.width_mode = .all;
    } else return .bad_arg;
    return .ok;
}

/// `road:N` / `river:N`: the kind and a whole number (N may be negative).
fn parseKindCount(arg: []const u8) ?struct { kind: core.bridge.VsoKind, count: i64 } {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return null;
    const kind = parseKind(arg[0..colon]) orelse return null;
    const count = std.fmt.parseInt(i64, arg[colon + 1 ..], 10) catch return null;
    return .{ .kind = kind, .count = count };
}

/// `vso_delta:road:1`: the map holds one road more than when it opened.
fn vsoDelta(state: *State, arg: []const u8) Outcome {
    const want = parseKindCount(arg) orelse return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.vsoCount(want.kind) catch return .refused;
    const at_open = state.vso_count_at_open[@intFromEnum(want.kind)];
    const delta = @as(i64, @intCast(now)) - @as(i64, @intCast(at_open));
    return if (delta == want.count) .ok else .refused;
}

/// `vso_points:road:4`: the selected line is a road with 4 control points.
fn vsoPoints(state: *State, arg: []const u8) Outcome {
    const want = parseKindCount(arg) orelse return .bad_arg;
    const tool = &state.view.roads_rivers;
    const selected = tool.selected orelse return .refused;
    if (selected.kind != want.kind) return .refused;
    const view = tool.selectedView(state.editor) orelse return .refused;
    return if (@as(i64, @intCast(view.control_points.len)) == want.count) .ok else .refused;
}

// ---------------------------------------------------------------------------
// Bridges (04-06, D-10..D-12): the Bridges panel's controls, as commands.
// ---------------------------------------------------------------------------

/// A type from the panel's list, by name: what the next drag draws.
pub fn chooseBridgeType(state: *State, name: []const u8) Outcome {
    state.refreshBridgeTypes();
    for (state.bridge_types) |*item| {
        if (std.mem.eql(u8, item.nameSlice(), name)) {
            state.view.bridge_tool.setDesc(name);
            return .ok;
        }
    }
    return .bad_arg;
}

/// The selected bridge (the Bridge tool's), or a note saying none is.
fn selectedBridge(state: *State) ?usize {
    if (!panels.mapIsOpen(state.editor)) return null;
    const index = state.view.bridge_tool.selected orelse {
        state.editor.note("click a bridge to select it first");
        return null;
    };
    return index;
}

/// The panel's Rotate button and Q/E: the selected bridge to its partner.
pub fn rotateSelectedBridge(state: *State) Outcome {
    const index = selectedBridge(state) orelse return .refused;
    return resultOutcome(state, state.editor.rotateBridge(index));
}

/// The panel's "Built during play" checkbox and Enter.
pub fn toggleSelectedBridgeBuild(state: *State) Outcome {
    const index = selectedBridge(state) orelse return .refused;
    return resultOutcome(state, state.editor.toggleBridgeBuild(index));
}

/// The panel's Delete: the whole selected bridge.
pub fn deleteSelectedBridge(state: *State) Outcome {
    const index = selectedBridge(state) orelse return .refused;
    const result = state.editor.deleteBridge(index);
    if (result) |_| state.view.bridge_tool.selected = null else |_| {}
    return resultOutcome(state, result);
}

fn bridgeDesc(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or arg.len >= core.bridge.name_capacity) return .bad_arg;
    return chooseBridgeType(state, arg);
}

fn bridgeRotate(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return rotateSelectedBridge(state);
}

fn bridgeToggleBuild(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return toggleSelectedBridgeBuild(state);
}

fn bridgeDelete(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return deleteSelectedBridge(state);
}

/// `bridge_delta:1`: the map holds one bridges entry more than at open.
fn bridgeDelta(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i64, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.bridges(state.allocator) catch return .refused;
    defer state.allocator.free(now);
    const delta = @as(i64, @intCast(now.len)) - @as(i64, @intCast(state.bridge_count_at_open));
    return if (delta == want) .ok else .refused;
}

/// `bridge_built`: the selected bridge is built during play.
fn bridgeBuilt(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const index = state.view.bridge_tool.selected orelse return .refused;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.bridges(state.allocator) catch return .refused;
    defer state.allocator.free(now);
    if (index >= now.len) return .refused;
    return if (now[index].built_during_play) .ok else .refused;
}

// ---------------------------------------------------------------------------
// Fences (04-07, D-14).
// ---------------------------------------------------------------------------

/// A type from the Fences panel's list, by name: what the next run places.
pub fn chooseFenceType(state: *State, name: []const u8) Outcome {
    state.refreshFenceTypes();
    for (state.fence_types) |*item| {
        if (std.mem.eql(u8, item.nameSlice(), name)) {
            state.view.fence_tool.setDesc(name);
            return .ok;
        }
    }
    return .bad_arg;
}

fn fenceDesc(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or arg.len >= core.bridge.name_capacity) return .bad_arg;
    return chooseFenceType(state, arg);
}

/// `fence_delta:5`: the map holds five fences more than at open.
fn fenceDelta(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i64, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const delta = @as(i64, @intCast(state.fenceCount())) - @as(i64, @intCast(state.fence_count_at_open));
    if (delta == want) return .ok;
    // The scenario runner prints the status line with a false predicate: say
    // what the count is, so a wrong expectation is one run, not a search.
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "fence_delta is {d}, not {d}", .{ delta, want }) catch "fence_delta differs");
    return .refused;
}

// ---------------------------------------------------------------------------
// Entrenchments (04-08, D-13).
// ---------------------------------------------------------------------------

/// `trench_player:N`: the player the next entrenchment's pieces belong to,
/// 0..the map's players - 1. Chosen, it stops following the Objects panel's.
fn trenchPlayer(state: *State, arg: []const u8) Outcome {
    const player = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (player < 0 or player >= state.editor.document.info.player_count) return .bad_arg;
    state.view.trench_tool.player = player;
    state.trench_player_chosen = true;
    return .ok;
}

/// The panel's Delete and `trench_delete`: the selected entrenchment, else
/// the highlighted one, whole - what the tool's Delete key does.
fn trenchDelete(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.view.trench_tool.handle(state.editor, .{ .key = .delete }));
}

/// `trench_delta:1`: the map holds one entrenchment more than at open.
fn trenchDelta(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i64, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.entrenchments(state.allocator) catch return .refused;
    defer state.allocator.free(now);
    const delta = @as(i64, @intCast(now.len)) - @as(i64, @intCast(state.trench_count_at_open));
    if (delta == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "trench_delta is {d}, not {d}", .{ delta, want }) catch "trench_delta differs");
    return .refused;
}

// ---------------------------------------------------------------------------
// Script IDs (04-09, D-15) and reinforcement groups (D-16).
// ---------------------------------------------------------------------------

/// `script_id:4244`: the selected object's script ID, -1 (none) or 0..32000;
/// one undo step. What the Properties panel's Script ID field commits.
fn scriptId(state: *State, arg: []const u8) Outcome {
    const value = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const link_id = state.editor.selection orelse {
        state.editor.note("select an object first");
        return .refused;
    };
    return resultOutcome(state, state.editor.setScriptID(link_id, value, 0));
}

/// `expect=script_id:4244`: the selected object's script ID is that.
fn scriptIdIs(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const link_id = state.editor.selection orelse return .refused;
    const object = state.editor.document.find(link_id) orelse return .refused;
    if (object.script_id == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "script_id is {d}, not {d}", .{ object.script_id, want }) catch "script_id differs");
    return .refused;
}

/// The script file's name as the map holds it now (a fresh read), or null
/// with no map open or for a value the editor cannot read. `buffer` holds it.
pub fn readScriptFile(state: *State, buffer: *[records.script_file_capacity]u8) ?[]const u8 {
    if (!panels.mapIsOpen(state.editor)) return null;
    var value: records.Value = undefined;
    if (state.editor.bridge.readRecord(.script_file, 0, state.allocator, &value) != .ok) return null;
    defer value.deinit(state.allocator);
    buffer.* = value.script_file.name;
    return std.mem.sliceTo(buffer, 0);
}

/// Map -> Script: the map's script file becomes `name` (empty for None), one
/// undo step (D-20). Public: the Script dialog calls it too.
pub fn setScriptFile(state: *State, name: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.setScriptFile(name));
}

/// `do=script_file:<name>` (`none` for no script): the map's script file.
fn scriptFile(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    return setScriptFile(state, if (std.mem.eql(u8, arg, "none")) "" else arg);
}

// ---------------------------------------------------------------------------
// Script areas (04-10, D-21).
// ---------------------------------------------------------------------------

/// `area_shape:rect` or `:circle`: the shape the next drag of the Script Areas
/// tool draws.
fn areaShape(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "rect")) {
        state.view.areas_tool.shape = .rectangle;
    } else if (std.mem.eql(u8, arg, "circle")) {
        state.view.areas_tool.shape = .circle;
    } else return .bad_arg;
    return .ok;
}

/// `area_name:m2_area`: the name the next area takes (the panel's name field).
fn areaName(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or arg.len >= records.area_name_capacity) return .bad_arg;
    state.view.areas_tool.setName(arg);
    return .ok;
}

/// `index:name` - an area's index in the map's list, and a name.
fn parseIndexedName(arg: []const u8) ?struct { index: usize, name: []const u8 } {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return null;
    const index = std.fmt.parseInt(usize, arg[0..colon], 10) catch return null;
    return .{ .index = index, .name = arg[colon + 1 ..] };
}

/// Renames area `index` to `name`, one undo step; the status line says why when
/// the name is empty or taken. Public: the Script Areas panel's Rename calls it too.
pub fn renameArea(state: *State, index: usize, name: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.renameScriptArea(index, name));
}

/// `area_rename:0:m2_zone`.
fn areaRename(state: *State, arg: []const u8) Outcome {
    const parsed = parseIndexedName(arg) orelse return .bad_arg;
    return renameArea(state, parsed.index, parsed.name);
}

/// Deletes area `index`, one undo step. Public for the panel.
pub fn deleteArea(state: *State, index: usize) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const tool = &state.view.areas_tool;
    if (tool.selected != null and tool.selected.? == index) tool.selected = null;
    return resultOutcome(state, state.editor.deleteScriptArea(index));
}

/// `area_delete:0`.
fn areaDelete(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return deleteArea(state, index);
}

/// A click on the Script Areas list: the area is selected and the camera
/// centres on it (AI units to world units, the camera's own). Public for the panel.
pub fn gotoArea(state: *State, index: usize) Outcome {
    state.refreshAreas();
    if (index >= state.areas.items.len) return .refused;
    const area = state.areas.items[index];
    state.view.areas_tool.selected = index;
    const world = marker_logic.aiToWorld(.{ .x = area.cx, .y = area.cy });
    state.view.centreOn(state.real, world.x, world.y);
    return .ok;
}

/// `expect=areas_delta:2`: the map holds that many areas more than at open.
fn areasDelta(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i64, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.scriptAreas(state.allocator) catch return .refused;
    defer state.allocator.free(now);
    const delta = @as(i64, @intCast(now.len)) - @as(i64, @intCast(state.areas_at_open));
    if (delta == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "areas_delta is {d}, not {d}", .{ delta, want }) catch "areas_delta differs");
    return .refused;
}

/// `expect=area_named:m2_area`: an area of the map has that name.
fn areaNamed(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.scriptAreas(state.allocator) catch return .refused;
    defer state.allocator.free(now);
    for (now) |*area| {
        if (std.mem.eql(u8, area.nameSlice(), arg)) return .ok;
    }
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "no area is named {s}", .{arg}) catch "no area has that name");
    return .refused;
}

/// `script_dialog:1` opens the Script dialog, `:0` closes it.
fn scriptDialog(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) {
        state.script_open = true;
    } else if (std.mem.eql(u8, arg, "0")) {
        state.script_open = false;
    } else return .bad_arg;
    return .ok;
}

/// "Open script folder" (D-20, WR-B04): the folder holding the map's script in
/// the system's file manager; refused, with a status line, when the map names
/// none or it is not there.
fn scriptOpen(state: *State, _: []const u8) Outcome {
    return if (panels.openScript(state)) .ok else .refused;
}

/// `do=script_choose:<path>` (04-13): the Script dialog's "Choose other..."
/// without the file picker - the Lua file at the OS path `path` (absolute, or
/// relative to the working directory) is copied beside the map under its own
/// name and becomes the map's script, one undo step, exactly as a file the
/// dialog answered would be (`panels.pickScript`). Refused, the status line
/// saying why, for a shipped map, a failed copy, and when a different file of
/// that name is already beside the map: the "Replace it?" question is then up
/// (`script_overwrite_yes` / `_no`). A name that is not a bare name is bad.
fn scriptChoose(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    const name = core.script_file.pickedName(arg) orelse return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    panels.pickScript(state, arg, false);
    if (state.script_pick_active) {
        state.editor.note("a different file of that name is beside the map; the Replace it? question is up");
        return .refused;
    }
    var buffer: [records.script_file_capacity]u8 = undefined;
    const have = readScriptFile(state, &buffer) orelse return .refused;
    return if (std.mem.eql(u8, have, name)) .ok else .refused;
}

/// `expect=script_beside:<name>` (04-13): `<name>.lua` is a file beside the
/// open map - what Choose other and Save As's copy-along put there.
fn scriptBeside(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or !core.script_file.isBareName(arg)) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const files = state.editor.files orelse return .refused;
    var path_buffer: [core.files.max_path]u8 = undefined;
    const path = core.script_file.scriptPathBeside(&path_buffer, state.editor.document.path.items, arg) orelse return .bad_arg;
    if (files.exists(path)) return .ok;
    var note: [160]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&note, "{s}.lua is not beside the map", .{arg}) catch "the script is not beside the map");
    return .refused;
}

/// `expect=test_game_script:<name>` (04-13): the last test game's log (Test in
/// game's test-game.log) reports, through BK_MAP_TRACE, that it loaded the
/// script `<name>` and ran its Init - the script Test in game copied beside
/// the test map. The game must have been started with BK_MAP_TRACE
/// (BK_EDITOR_AUTO_GAME_TRACE) and have exited (`waitgame`).
fn testGameScript(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or !core.script_file.isBareName(arg)) return .bad_arg;
    if (state.test_game_log_len == 0) {
        state.editor.note("no test game has been started");
        return .refused;
    }
    const log_path = state.test_game_log_buffer[0..state.test_game_log_len];
    const log = std.Io.Dir.cwd().readFileAlloc(state.io, log_path, state.allocator, .limited(16 << 20)) catch {
        state.editor.note("the test game's log did not read");
        return .refused;
    };
    defer state.allocator.free(log);
    const trace = testlaunch.parseMapTrace(log);
    var note: [200]u8 = undefined;
    const script = trace.script orelse {
        state.editor.note(std.fmt.bufPrint(&note, "the test game's log {s} has no BK_MAP_TRACE script line", .{log_path}) catch "no script line");
        return .refused;
    };
    if (std.mem.eql(u8, script.name.slice(), arg) and script.loaded and script.init) return .ok;
    state.editor.note(std.fmt.bufPrint(&note, "the test game's script is \"{s}\" loaded={} init={}, not {s} run", .{ script.name.slice(), script.loaded, script.init, arg }) catch "the test game's script differs");
    return .refused;
}

/// The two buttons of Save As's "Copy <name>.lua beside the new map?".
fn scriptCopyAlongYes(state: *State, _: []const u8) Outcome {
    return if (panels.answerScriptCopyAlong(state, true)) .ok else .refused;
}

fn scriptCopyAlongNo(state: *State, _: []const u8) Outcome {
    return if (panels.answerScriptCopyAlong(state, false)) .ok else .refused;
}

/// The two buttons of Choose other's "Replace it?".
fn scriptOverwriteYes(state: *State, _: []const u8) Outcome {
    if (!state.script_pick_active) return .refused;
    state.script_pick_active = false;
    var picked: logic.PathText = .{};
    picked.set(state.script_pick.slice());
    panels.pickScript(state, picked.slice(), true);
    return .ok;
}

fn scriptOverwriteNo(state: *State, _: []const u8) Outcome {
    if (!state.script_pick_active) return .refused;
    state.script_pick_active = false;
    return .ok;
}

/// `expect=script_file:<name>` (`none` for no script): the map names it.
fn scriptFileIs(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    const want = if (std.mem.eql(u8, arg, "none")) "" else arg;
    var buffer: [records.script_file_capacity]u8 = undefined;
    const have = readScriptFile(state, &buffer) orelse return .refused;
    if (std.mem.eql(u8, have, want)) return .ok;
    var note: [128]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&note, "the script file is \"{s}\", not \"{s}\"", .{ have, want }) catch "the script file differs");
    return .refused;
}

/// `G:S`: a group ID and a script ID (or the 1/0 of `group_hide`), both whole
/// numbers.
fn parsePair(arg: []const u8) ?struct { first: i32, second: i32 } {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return null;
    const first = std.fmt.parseInt(i32, arg[0..colon], 10) catch return null;
    const second = std.fmt.parseInt(i32, arg[colon + 1 ..], 10) catch return null;
    return .{ .first = first, .second = second };
}

/// The Groups window's New: an empty group under the first unused ID at or
/// above `from_id` (C9), selected; one undo step.
pub fn newGroup(state: *State, from_id: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const id = state.editor.newGroup(from_id) catch |err| {
        state.view.noteEditResult(state.editor, err);
        return .refused;
    };
    state.view.clearStatus();
    state.group_selected = id;
    return .ok;
}

/// Add: a script ID (0..32000) to a group; one already there is a status note
/// and no step.
pub fn addGroupId(state: *State, group: i32, script_id: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.addScriptIDToGroup(group, script_id));
}

/// Remove: a script ID from a group.
pub fn removeGroupId(state: *State, group: i32, script_id: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.removeScriptIDFromGroup(group, script_id));
}

/// Delete: the group and its script IDs; its check and selection go too.
pub fn deleteGroup(state: *State, group: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const result = state.editor.deleteGroup(group);
    if (result) |_| {
        state.setGroupChecked(group, false);
        if (state.group_selected != null and state.group_selected.? == group) state.group_selected = null;
        if (state.group_marked != null and state.group_marked.? == group) state.group_marked = null;
    } else |_| {}
    return resultOutcome(state, result);
}

/// Hide checked, one group's box: its script IDs are held back in the view and
/// from picking, or shown again. A view setting: no undo step.
pub fn hideGroup(state: *State, group: i32, hide: bool) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (state.findGroup(group) == null) {
        var buffer: [64]u8 = undefined;
        state.editor.note(std.fmt.bufPrint(&buffer, "there is no reinforcement group {d}", .{group}) catch "no such group");
        return .refused;
    }
    state.setGroupChecked(group, hide);
    return .ok;
}

/// Select objects: the document's objects that carry a script ID of the group
/// are marked (View -> Markers -> Reinforcement groups) and the first is
/// selected in the Select tool; the status bar says how many.
pub fn selectGroupObjects(state: *State, group: i32) Outcome {
    const editor = state.editor;
    if (!panels.mapIsOpen(editor)) return .refused;
    const row = state.findGroup(group) orelse {
        var buffer: [64]u8 = undefined;
        editor.note(std.fmt.bufPrint(&buffer, "there is no reinforcement group {d}", .{group}) catch "no such group");
        return .refused;
    };
    state.group_selected = group;
    state.group_marked = group;
    var count: usize = 0;
    var first: ?i32 = null;
    for (editor.document.objects.items) |object| {
        if (object.scenario or !row.has(object.script_id)) continue;
        count += 1;
        if (first == null) first = object.link_id;
    }
    var buffer: [128]u8 = undefined;
    if (first) |link_id| {
        state.view.selectTool(editor, .select);
        editor.selection = link_id;
        editor.note(std.fmt.bufPrint(&buffer, "{d} {s} of group {d}'s script IDs; the first is selected", .{ count, if (count == 1) "object carries one" else "objects carry them", group }) catch "objects selected");
    } else {
        editor.note(std.fmt.bufPrint(&buffer, "no object carries a script ID of group {d}", .{group}) catch "no objects");
    }
    return .ok;
}

/// `group_new:0`: New with that ID in the field.
fn groupNew(state: *State, arg: []const u8) Outcome {
    const from_id = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    return newGroup(state, from_id);
}

/// `group_add_id:900:4245`: script ID 4245 into group 900.
fn groupAddId(state: *State, arg: []const u8) Outcome {
    const pair = parsePair(arg) orelse return .bad_arg;
    return addGroupId(state, pair.first, pair.second);
}

fn groupRemoveId(state: *State, arg: []const u8) Outcome {
    const pair = parsePair(arg) orelse return .bad_arg;
    return removeGroupId(state, pair.first, pair.second);
}

fn groupDelete(state: *State, arg: []const u8) Outcome {
    const group = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    return deleteGroup(state, group);
}

/// `group_hide:900:1` hides group 900's objects, `group_hide:900:0` shows them.
fn groupHide(state: *State, arg: []const u8) Outcome {
    const pair = parsePair(arg) orelse return .bad_arg;
    if (pair.second != 0 and pair.second != 1) return .bad_arg;
    return hideGroup(state, pair.first, pair.second == 1);
}

fn groupSelect(state: *State, arg: []const u8) Outcome {
    const group = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    return selectGroupObjects(state, group);
}

/// `groups_window:1` opens the Groups window, `:0` closes it.
fn groupsWindow(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) {
        state.groups_open = true;
    } else if (std.mem.eql(u8, arg, "0")) {
        state.groups_open = false;
    } else return .bad_arg;
    return .ok;
}

/// `expect=group_has:900:4245`: group 900 holds script ID 4245.
fn groupHas(state: *State, arg: []const u8) Outcome {
    const pair = parsePair(arg) orelse return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const row = state.findGroup(pair.first) orelse return .refused;
    return if (row.has(pair.second)) .ok else .refused;
}

/// `expect=groups_delta:1`: the map holds one group more than when it opened.
fn groupsDelta(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i64, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    state.refreshGroups();
    const delta = @as(i64, @intCast(state.groups.items.len)) - @as(i64, @intCast(state.groups_at_open));
    if (delta == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "groups_delta is {d}, not {d}", .{ delta, want }) catch "groups_delta differs");
    return .refused;
}

/// `expect=hidden_count:1`: the checked groups hide that many of the
/// document's objects now.
fn hiddenCount(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    state.syncHiddenGroups();
    if (state.hidden_object_count == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "hidden_count is {d}, not {d}", .{ state.hidden_object_count, want }) catch "hidden_count differs");
    return .refused;
}

// ---------------------------------------------------------------------------
// Start commands (04-11, D-17).
// ---------------------------------------------------------------------------

/// Unit -> Add start command: a command of the default type (STOP) for the
/// selected unit, selected in the Start Commands window; one undo step. The status
/// line says why when the selection is no unit, or warns of a held-back one.
/// Public: the menu goes through the named command, which calls this.
pub fn addStartCommandForSelection(state: *State) Outcome {
    const editor = state.editor;
    if (!panels.mapIsOpen(editor)) return .refused;
    const link_id = editor.selection orelse {
        editor.note("select a unit first");
        return .refused;
    };
    const index = editor.addStartCommand(link_id) catch |err| {
        state.view.noteEditResult(editor, err);
        return .refused;
    };
    state.view.clearStatus();
    state.startcmd_selected = index;
    return .ok;
}

/// `do=startcmd_add`: Unit -> Add start command for the selected unit.
fn startcmdAdd(state: *State, _: []const u8) Outcome {
    return addStartCommandForSelection(state);
}

/// `expect=startcmds_delta:1`: the map holds that many start commands more than
/// when it opened.
fn startcmdsDelta(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i64, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.startCommands(state.allocator) catch return .refused;
    defer core.editor.Editor.freeStartCommands(state.allocator, now);
    const delta = @as(i64, @intCast(now.len)) - @as(i64, @intCast(state.startcmds_at_open));
    if (delta == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "startcmds_delta is {d}, not {d}", .{ delta, want }) catch "startcmds_delta differs");
    return .refused;
}

/// The command `index` as it is now (a fresh read, never the window's cache: a
/// predicate the frame after a command must see the command), its units owned by
/// `state.allocator`; null when there is none.
fn readStartCommand(state: *State, index: usize) ?records.StartCommand {
    if (!panels.mapIsOpen(state.editor)) return null;
    var value: records.Value = undefined;
    if (state.editor.bridge.readRecord(.start_command, @intCast(index), state.allocator, &value) != .ok) return null;
    return value.start_command;
}

/// The selected command of the window, or a note and null.
fn selectedStartCommand(state: *State) ?usize {
    const index = state.startcmd_selected orelse {
        state.editor.note("select a start command first");
        return null;
    };
    return index;
}

/// Sets the type of command `index` to the action type `id`; one undo step.
/// Public: the window's type list calls it.
pub fn setStartCommandType(state: *State, index: usize, id: i32) Outcome {
    var command = readStartCommand(state, index) orelse return .refused;
    defer state.allocator.free(command.units);
    command.cmd_type = id;
    return resultOutcome(state, state.editor.editStartCommand(index, command, 0));
}

/// Sets the number of command `index`; one undo step. Public for the window's field.
pub fn setStartCommandNumber(state: *State, index: usize, number: f32) Outcome {
    if (!std.math.isFinite(number)) return .bad_arg;
    var command = readStartCommand(state, index) orelse return .refused;
    defer state.allocator.free(command.units);
    command.number = number;
    return resultOutcome(state, state.editor.editStartCommand(index, command, 0));
}

/// Deletes command `index`; one undo step. Public for the window.
pub fn deleteStartCommandAt(state: *State, index: usize) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (state.startcmd_selected != null and state.startcmd_selected.? == index) state.startcmd_selected = null;
    return resultOutcome(state, state.editor.deleteStartCommand(index));
}

/// "Add selected unit": the object selected on the map joins command `index`.
/// Public for the window.
pub fn addSelectedUnitTo(state: *State, index: usize) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const link_id = state.editor.selection orelse {
        state.editor.note("select a unit on the map first");
        return .refused;
    };
    return resultOutcome(state, state.editor.addUnitToStartCommand(index, link_id));
}

/// "Remove" beside a unit of command `index` (the last one takes the command with
/// it). Public for the window.
pub fn removeUnitFrom(state: *State, index: usize, link_id: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (state.startcmd_selected != null and state.startcmd_selected.? == index) {
        // The command goes with its last unit; its selection goes too.
        const command = readStartCommand(state, index);
        if (command) |held| {
            defer state.allocator.free(held.units);
            if (held.units.len == 1 and held.units[0] == link_id) state.startcmd_selected = null;
        }
    }
    return resultOutcome(state, state.editor.removeUnitFromStartCommand(index, link_id));
}

/// "Set target": the Start Target tool takes one click for command `index` and the
/// tool in hand comes back after it. Public for the window.
pub fn beginTarget(state: *State, index: usize) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (readStartCommand(state, index)) |command| {
        state.allocator.free(command.units);
    } else {
        state.editor.note("that start command is gone");
        return .refused;
    }
    state.view.beginStartTarget(state.editor, index);
    return .ok;
}

/// `do=startcmd_type:MOVE_TO`: the selected command's type, by a name the action
/// list (Data/Editor/actions.ini) has.
fn startcmdType(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    const index = selectedStartCommand(state) orelse return .refused;
    state.refreshStartActions();
    for (state.startcmd_actions) |*item| {
        if (std.mem.eql(u8, item.nameSlice(), arg)) return setStartCommandType(state, index, item.id);
    }
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "no action type is named {s}", .{arg}) catch "no such action type");
    return .refused;
}

/// `do=startcmd_number:2.5`: the selected command's number.
fn startcmdNumber(state: *State, arg: []const u8) Outcome {
    const number = std.fmt.parseFloat(f32, arg) catch return .bad_arg;
    const index = selectedStartCommand(state) orelse return .refused;
    return setStartCommandNumber(state, index, number);
}

/// `do=startcmd_add_unit`: the object selected on the map joins the selected command.
fn startcmdAddUnit(state: *State, _: []const u8) Outcome {
    const index = selectedStartCommand(state) orelse return .refused;
    return addSelectedUnitTo(state, index);
}

/// `do=startcmd_target_here`: the ground point at the centre of the view becomes the
/// selected command's target, as a click there with the Start Target tool would
/// (the ground, never an object, so a scenario needs no object under the centre).
fn startcmdTargetHere(state: *State, _: []const u8) Outcome {
    const editor = state.editor;
    const index = selectedStartCommand(state) orelse return .refused;
    const screen = state.real.screenSize() orelse return .refused;
    var centre = editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0) catch return .refused;
    centre.object = null;
    return resultOutcome(state, core.tools_ai.StartTarget.setTarget(editor, index, centre));
}

/// `do=startcmd_target_begin`: "Set target" for the selected command.
fn startcmdTargetBegin(state: *State, _: []const u8) Outcome {
    const index = selectedStartCommand(state) orelse return .refused;
    return beginTarget(state, index);
}

/// `do=startcmd_delete:0`.
fn startcmdDelete(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return deleteStartCommandAt(state, index);
}

/// `do=startcmd_select:0`: the window's selection (and the lines it draws).
fn startcmdSelect(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.refreshStartCommands();
    if (index >= state.startcmds.len) return .refused;
    state.startcmd_selected = index;
    return .ok;
}

/// `do=startcmds_window:1` opens the Start Commands window, `:0` closes it.
fn startcmdsWindow(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) {
        state.startcmds_open = true;
    } else if (std.mem.eql(u8, arg, "0")) {
        state.startcmds_open = false;
    } else return .bad_arg;
    return .ok;
}

/// `index:rest` - a command's index and what follows the first colon.
fn parseIndexed(arg: []const u8) ?struct { index: usize, rest: []const u8 } {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return null;
    const index = std.fmt.parseInt(usize, arg[0..colon], 10) catch return null;
    return .{ .index = index, .rest = arg[colon + 1 ..] };
}

/// `expect=startcmd_units:0:2`: command 0 names two units.
fn startcmdUnits(state: *State, arg: []const u8) Outcome {
    const parsed = parseIndexed(arg) orelse return .bad_arg;
    const want = std.fmt.parseInt(usize, parsed.rest, 10) catch return .bad_arg;
    const command = readStartCommand(state, parsed.index) orelse return .refused;
    defer state.allocator.free(command.units);
    if (command.units.len == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "start command {d} has {d} units, not {d}", .{ parsed.index, command.units.len, want }) catch "unit count differs");
    return .refused;
}

/// `expect=startcmd_target:0:link` (the target is an object) or `:pos` (a point and
/// no object): what command 0's target is.
fn startcmdTargetIs(state: *State, arg: []const u8) Outcome {
    const parsed = parseIndexed(arg) orelse return .bad_arg;
    const want_link = if (std.mem.eql(u8, parsed.rest, "link")) true else if (std.mem.eql(u8, parsed.rest, "pos")) false else return .bad_arg;
    const command = readStartCommand(state, parsed.index) orelse return .refused;
    defer state.allocator.free(command.units);
    const is_link = command.link_id != 0;
    const is_pos = !is_link and (command.x != 0 or command.y != 0);
    if ((want_link and is_link) or (!want_link and is_pos)) return .ok;
    var buffer: [128]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "start command {d}: link {d}, point {d:.0},{d:.0}", .{ parsed.index, command.link_id, command.x, command.y }) catch "target differs");
    return .refused;
}

/// `expect=startcmd_is:0:MOVE_TO`: command 0's type is named so by the action list.
fn startcmdIs(state: *State, arg: []const u8) Outcome {
    const parsed = parseIndexed(arg) orelse return .bad_arg;
    if (parsed.rest.len == 0) return .bad_arg;
    const command = readStartCommand(state, parsed.index) orelse return .refused;
    defer state.allocator.free(command.units);
    state.refreshStartActions();
    for (state.startcmd_actions) |*item| {
        if (item.id == command.cmd_type and std.mem.eql(u8, item.nameSlice(), parsed.rest)) return .ok;
    }
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "start command {d} has type {d}, not {s}", .{ parsed.index, command.cmd_type, parsed.rest }) catch "type differs");
    return .refused;
}

// ---------------------------------------------------------------------------
// Reserve positions (04-11, D-18).
// ---------------------------------------------------------------------------

/// Unit -> Artillery positions mode: the Reserve Positions tool in hand, or - when it
/// is - the Select tool back. Public: the menu goes through the named command.
pub fn toggleReserveMode(state: *State) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (state.view.tool == .reserve_positions) {
        state.view.reserve_tool.clearPending();
        state.view.selectTool(state.editor, .select);
    } else {
        state.view.selectTool(state.editor, .reserve_positions);
    }
    return .ok;
}

/// `do=reserve_mode`: Unit -> Artillery positions mode.
fn reserveMode(state: *State, _: []const u8) Outcome {
    return toggleReserveMode(state);
}

/// The Reserve Positions tool, or a note that it is not in hand.
fn reserveTool(state: *State) ?*core.tools_ai.ReservePositions {
    if (state.view.tool != .reserve_positions) {
        state.editor.note("Unit > Artillery positions mode first");
        return null;
    }
    return &state.view.reserve_tool;
}

/// `do=reserve_pick_here`: the object at the view centre - else the ground there - is
/// the next step of the Reserve Positions tool, as a click at the centre would be.
fn reservePickHere(state: *State, _: []const u8) Outcome {
    const tool = reserveTool(state) orelse return .refused;
    const screen = state.real.screenSize() orelse return .refused;
    const centre = state.editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0) catch return .refused;
    return resultOutcome(state, tool.pick(state.editor, centre));
}

/// `do=reserve_commit`: Enter of the Reserve Positions tool - the pending gun, truck
/// and place are added as one position.
fn reserveCommit(state: *State, _: []const u8) Outcome {
    const tool = reserveTool(state) orelse return .refused;
    return resultOutcome(state, tool.commit(state.editor));
}

/// Deletes reserve position `index`; one undo step. Public for the panel.
pub fn deleteReserveAt(state: *State, index: usize) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const tool = &state.view.reserve_tool;
    if (tool.selected != null and tool.selected.? == index) tool.selected = null;
    return resultOutcome(state, state.editor.deleteReservePosition(index));
}

/// `do=reserve_delete:0`.
fn reserveDelete(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return deleteReserveAt(state, index);
}

/// `do=reserve_select:0`: the list's selection (Delete acts on it).
fn reserveSelect(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.refreshReserve();
    if (index >= state.reserve_list.len) return .refused;
    state.view.reserve_tool.selected = index;
    return .ok;
}

/// `do=placer_role:towed` (or `truck`, `sp`): the Place tool's object becomes the first
/// placeable unit of the catalogue with that reserve role, read through the bridge. For
/// a scenario that needs a gun or a truck without naming one.
fn placerRole(state: *State, arg: []const u8) Outcome {
    const want: core.bridge.ReserveRole = if (std.mem.eql(u8, arg, "towed")) .towed else if (std.mem.eql(u8, arg, "truck")) .truck else if (std.mem.eql(u8, arg, "sp")) .self_propelled else return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    for (state.catalogue) |*entry| {
        if (entry.game_type != 1 or entry.placeable == 0) continue;
        const name = std.mem.sliceTo(&entry.name, 0);
        const role = state.editor.reserveRole(name) catch continue;
        if (role != want) continue;
        state.view.setPlacerObject(name);
        return .ok;
    }
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "the catalogue has no placeable unit of role {s}", .{arg}) catch "no such unit");
    return .refused;
}

/// `do=placer_name:Sdkfz_8`: the Place tool's object becomes the placeable unit of the
/// catalogue with that name. For a scenario that needs one particular unit - a truck
/// strong enough for the gun it tows - and refused, saying so, when the catalogue has no
/// such unit.
fn placerName(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    for (state.catalogue) |*entry| {
        if (entry.game_type != 1 or entry.placeable == 0) continue;
        const name = std.mem.sliceTo(&entry.name, 0);
        if (!std.mem.eql(u8, name, arg)) continue;
        state.view.setPlacerObject(name);
        return .ok;
    }
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "the catalogue has no placeable unit named {s}", .{arg}) catch "no such unit");
    return .refused;
}

/// `expect=reserve_delta:1`: the map holds that many reserve positions more than when
/// it opened.
fn reserveDelta(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i64, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const now = state.editor.reservePositions(state.allocator) catch return .refused;
    defer state.allocator.free(now);
    const delta = @as(i64, @intCast(now.len)) - @as(i64, @intCast(state.reserve_at_open));
    if (delta == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "reserve_delta is {d}, not {d}", .{ delta, want }) catch "reserve_delta differs");
    return .refused;
}

/// `expect=reserve_pending:gun` (a gun is picked), `:truck`, `:place` or `:none`: what
/// the Reserve Positions tool holds before Enter.
fn reservePending(state: *State, arg: []const u8) Outcome {
    const tool = &state.view.reserve_tool;
    const holds = if (std.mem.eql(u8, arg, "gun")) tool.gun != null else if (std.mem.eql(u8, arg, "truck")) tool.truck != null else if (std.mem.eql(u8, arg, "place")) tool.has_place else if (std.mem.eql(u8, arg, "none")) (tool.gun == null and tool.truck == null and !tool.has_place) else return .bad_arg;
    return if (holds) .ok else .refused;
}

// ---------------------------------------------------------------------------
// The AI general (04-12, D-19).
// ---------------------------------------------------------------------------

/// Chooses the side the AI General tool and its panel edit (a side the map lacks is
/// created by the first edit of it); the selection is let go of when the side changes.
/// Public: the panel's radios go through the named command.
pub fn setAiSide(state: *State, side: usize) Outcome {
    if (side >= records.max_ai_sides) return .bad_arg;
    const tool = &state.view.ai_tool;
    if (tool.side != side) {
        tool.side = side;
        tool.reset();
    }
    return .ok;
}

/// `do=ai_side:1`.
fn aiSide(state: *State, arg: []const u8) Outcome {
    const side = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return setAiSide(state, side);
}

/// The ground point at the centre of the view, as a pointer.
fn viewCentre(state: *State) ?core.tools.Pointer {
    const screen = state.real.screenSize() orelse return null;
    var centre = state.editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0) catch return null;
    centre.object = null;
    return centre;
}

/// `do=ai_parcel_here`: a defence parcel of radius 256 at the centre of the view on the
/// tool's side, as a click on open ground with the AI General tool makes one (and added
/// even inside another parcel, which a click would make a point of). One undo step.
fn aiParcelHere(state: *State, _: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const centre = viewCentre(state) orelse return .refused;
    const tool = &state.view.ai_tool;
    const added = state.editor.addDefenceParcel(tool.side, centre.map_x, centre.map_y);
    if (added) |index| {
        tool.select(index, null);
        return resultOutcome(state, {});
    } else |err| return resultOutcome(state, err);
}

/// `do=ai_point_here`: a reinforce point at the centre of the view, in the parcel of the
/// tool's side it is inside, as a click there would make one. Refused, saying so, when
/// no parcel holds the centre. One undo step.
fn aiPointHere(state: *State, _: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const centre = viewCentre(state) orelse return .refused;
    const tool = &state.view.ai_tool;
    var side = state.editor.aiSide(state.allocator, tool.side) catch return .refused;
    defer side.deinit(state.allocator);
    if (core.tools_ai.AIGeneral.hitHandle(side, .{ centre.map_x, centre.map_y }) != null or core.tools_ai.AIGeneral.parcelContaining(side, .{ centre.map_x, centre.map_y }) == null) {
        state.editor.note("the centre of the view is not inside a parcel of this side, or is on a handle: move the view");
        return .refused;
    }
    const result = blk: {
        tool.handle(state.editor, .{ .press = centre }) catch |err| break :blk err;
        tool.handle(state.editor, .{ .release = centre }) catch |err| break :blk err;
        break :blk {};
    };
    return resultOutcome(state, result);
}

/// `do=ai_select:0`: the panel's choice of parcel 0 of the side (its keys and Delete act on it).
fn aiSelect(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.refreshAi();
    if (index >= state.aiActive().parcels.len) return .refused;
    state.view.ai_tool.select(index, null);
    return .ok;
}

/// `do=ai_toggle_type:0`: parcel 0 of the side switches between defence and reinforce.
/// One undo step.
fn aiToggleType(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.refreshAi();
    if (index >= state.aiActive().parcels.len) return .refused;
    const tool = &state.view.ai_tool;
    tool.select(index, null);
    return resultOutcome(state, tool.switchType(state.editor));
}

/// Deletes the selected point, else the selected parcel. One undo step. Public for the panel.
pub fn deleteAiSelected(state: *State) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.view.ai_tool.deleteSelected(state.editor));
}

/// `do=ai_delete`.
fn aiDelete(state: *State, _: []const u8) Outcome {
    return deleteAiSelected(state);
}

/// Adds mobile script ID `id` to the tool's side; one already there is a note. One undo step.
/// Public for the panel.
pub fn addAiMobile(state: *State, id: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.addMobileScriptID(state.view.ai_tool.side, id));
}

/// `do=ai_mobile_add:4245`.
fn aiMobileAdd(state: *State, arg: []const u8) Outcome {
    const id = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    return addAiMobile(state, id);
}

/// Removes mobile script ID `id` from the tool's side. One undo step. Public for the panel.
pub fn removeAiMobile(state: *State, id: i32) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.removeMobileScriptID(state.view.ai_tool.side, id));
}

/// `do=ai_mobile_remove:4245`.
fn aiMobileRemove(state: *State, arg: []const u8) Outcome {
    const id = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    return removeAiMobile(state, id);
}

/// `side:rest` - a side's number and what follows the first colon.
fn parseSideArg(arg: []const u8) ?struct { side: usize, rest: []const u8 } {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return null;
    const side = std.fmt.parseInt(usize, arg[0..colon], 10) catch return null;
    return .{ .side = side, .rest = arg[colon + 1 ..] };
}

/// `expect=parcels:1:1`: side 1 holds that many parcels more than when the map opened
/// (a side the map did not have then counted 0).
fn parcelsDelta(state: *State, arg: []const u8) Outcome {
    const parsed = parseSideArg(arg) orelse return .bad_arg;
    const want = std.fmt.parseInt(i64, parsed.rest, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor) or parsed.side >= records.max_ai_sides) return .refused;
    var side = state.editor.aiSide(state.allocator, parsed.side) catch return .refused;
    defer side.deinit(state.allocator);
    const opened: usize = if (parsed.side < state.ai_parcels_at_open.items.len) state.ai_parcels_at_open.items[parsed.side] else 0;
    const delta = @as(i64, @intCast(side.parcels.len)) - @as(i64, @intCast(opened));
    if (delta == want) return .ok;
    var buffer: [112]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "side {d} holds {d} parcels more than at open, not {d}", .{ parsed.side, delta, want }) catch "parcels differs");
    return .refused;
}

/// `expect=mobile_has:1:4245`: side 1 has script ID 4245 among its mobile IDs.
fn mobileHas(state: *State, arg: []const u8) Outcome {
    const parsed = parseSideArg(arg) orelse return .bad_arg;
    const id = std.fmt.parseInt(i32, parsed.rest, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor) or parsed.side >= records.max_ai_sides) return .refused;
    var side = state.editor.aiSide(state.allocator, parsed.side) catch return .refused;
    defer side.deinit(state.allocator);
    if (side.hasMobile(id)) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "side {d} has no mobile script ID {d}", .{ parsed.side, id }) catch "no such mobile ID");
    return .refused;
}
