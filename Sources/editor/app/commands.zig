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
};

pub const predicate_table = [_]Entry{
    .{ .name = "anchor_set", .handler = anchorSet },
    .{ .name = "anchor_unset", .handler = anchorUnset },
    .{ .name = "undo_depth", .handler = undoDepth },
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
