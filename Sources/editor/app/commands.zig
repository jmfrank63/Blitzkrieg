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
    .{ .name = "bridge_desc", .handler = bridgeDesc },
    .{ .name = "bridge_rotate", .handler = bridgeRotate },
    .{ .name = "bridge_toggle_build", .handler = bridgeToggleBuild },
    .{ .name = "bridge_delete", .handler = bridgeDelete },
    .{ .name = "fence_desc", .handler = fenceDesc },
    .{ .name = "trench_player", .handler = trenchPlayer },
    .{ .name = "trench_delete", .handler = trenchDelete },
    .{ .name = "script_id", .handler = scriptId },
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

/// The width spinner, 1..16 (the MFC tool's; w * fWorldCellSize / 2 world
/// units).
fn vsoWidth(state: *State, arg: []const u8) Outcome {
    const width = std.fmt.parseInt(u8, arg, 10) catch return .bad_arg;
    if (width < 1 or width > 16) return .bad_arg;
    state.view.roads_rivers.width_tiles = @floatFromInt(width);
    return .ok;
}

/// The opacity slider, 0..100 %.
fn vsoOpacity(state: *State, arg: []const u8) Outcome {
    const percent = std.fmt.parseInt(u8, arg, 10) catch return .bad_arg;
    if (percent > 100) return .bad_arg;
    state.view.roads_rivers.opacity = @as(f32, @floatFromInt(percent)) / 100.0;
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
