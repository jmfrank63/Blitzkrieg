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
    .{ .name = "map_new", .handler = mapNew },
    .{ .name = "brush_size", .handler = brushSize },
    .{ .name = "map_update", .handler = mapUpdate },
    .{ .name = "map_fill", .handler = mapFill },
    .{ .name = "instant_update", .handler = instantUpdate },
    .{ .name = "fit_grid", .handler = fitGrid },
    .{ .name = "tile_info", .handler = tileInfo },
    .{ .name = "file_save_xml", .handler = fileSaveXml },
    .{ .name = "file_save_bzm", .handler = fileSaveBzm },
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
    .{ .name = "heights_window", .handler = heightsWindow },
    .{ .name = "props_open", .handler = propsOpen },
    .{ .name = "wheel_turn", .handler = wheelTurn },
    .{ .name = "damage_percent", .handler = damagePercent },
    .{ .name = "damage", .handler = damageCommand },
    .{ .name = "band_select", .handler = bandSelect },
    .{ .name = "props_set", .handler = propsSet },
    .{ .name = "link_make", .handler = linkMake },
    .{ .name = "link_unlink", .handler = linkUnlink },
    .{ .name = "heights_brush", .handler = heightsBrush },
    .{ .name = "heights_speed", .handler = heightsSpeed },
    .{ .name = "heights_ratio", .handler = heightsRatio },
    .{ .name = "heights_mode", .handler = heightsMode },
    .{ .name = "heights_generate", .handler = heightsGenerate },
    .{ .name = "heights_set_zero", .handler = heightsSetZero },
    .{ .name = "filter_select", .handler = filterSelect },
    .{ .name = "filter_toggle", .handler = filterToggle },
    .{ .name = "filter_assign", .handler = filterAssign },
    .{ .name = "filter_new", .handler = filterNew },
    .{ .name = "filter_delete", .handler = filterDelete },
    .{ .name = "filter_rename", .handler = filterRename },
    .{ .name = "filter_words", .handler = filterWords },
    .{ .name = "filters_save", .handler = filtersSave },
    .{ .name = "filters_composer", .handler = filtersComposer },
    .{ .name = "fields_set", .handler = fieldsSet },
    .{ .name = "fields_randomize", .handler = fieldsRandomize },
    .{ .name = "fields_toggle", .handler = fieldsToggle },
    .{ .name = "fields_apply", .handler = fieldsApply },
    .{ .name = "fields_vertex_add", .handler = fieldsVertexAdd },
    .{ .name = "fields_vertex_clear", .handler = fieldsVertexClear },
};

pub const predicate_table = [_]Entry{
    .{ .name = "anchor_set", .handler = anchorSet },
    .{ .name = "anchor_unset", .handler = anchorUnset },
    .{ .name = "undo_depth", .handler = undoDepth },
    // M3 (D-26/D-28/D-29): the properties', the wheel's and the Damage
    // tool's own predicates.
    .{ .name = "placer_angle", .handler = placerAngleIs },
    .{ .name = "hp", .handler = hpIs },
    .{ .name = "angle", .handler = angleIs },
    .{ .name = "link_with", .handler = linkWithIs },
    .{ .name = "selection_count", .handler = selectionCountIs },
    .{ .name = "objects", .handler = objectsIs },
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
    .{ .name = "title", .handler = titleContains },
    .{ .name = "status", .handler = statusContains },
    .{ .name = "palette_count", .handler = paletteCount },
    .{ .name = "dirty", .handler = dirtyIs },
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

// ---------------------------------------------------------------------------
// File (M3, D-23/D-24): New Map, Save as XML/BZM.
// ---------------------------------------------------------------------------

/// `do=map_new:WxH:season[:name][:mod]` - File > New with the dialog's
/// fields given (the season by name: summer/winter/africa/spring; the name
/// and the mod folder - "", "none" or a bare installed folder - optional).
/// The map is built once the unsaved-changes prompt, if any, has been
/// answered, exactly the menu's own route through FileActions, so `ok`
/// means queued, not yet built. Examples: `map_new:8x8:summer`,
/// `map_new:16x16:winter:my_map:EditorTestMod`.
fn mapNew(state: *State, arg: []const u8) Outcome {
    const fields = logic.NewMapFields.parse(arg) orelse return .bad_arg;
    state.actions.requestNewMap(fields);
    state.view.clearStatus();
    return .ok;
}

/// `do=brush_size:NN` - the Brush tool's size in cells per axis, 1..16 even
/// sizes included (M3, D-22/PARITY V4): what the palette's combo and the
/// MFC toolbar's before it set.
fn brushSize(state: *State, arg: []const u8) Outcome {
    const size = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (size < 1 or size > 16) return .bad_arg;
    state.view.brush.size = size;
    return .ok;
}

/// Update Map's progress collector (M3, D-20): the C ABI callback counts the
/// steps as the bridge reports them - counter only, nothing is rendered and
/// nothing re-enters the bridge from here.
const UpdateProgress = struct {
    steps: i32 = 0,
    total: i32 = 0,

    fn report(step: c_int, total: c_int, user: ?*anyopaque) callconv(.c) void {
        const self: *UpdateProgress = @ptrCast(@alignCast(user.?));
        self.steps = step;
        self.total = total;
    }
};

/// `do=map_update` - Map > Update Map (M3, D-20, the MFC's Ctrl+U): the
/// whole composite as ONE undo step. The step count the bridge reported is
/// kept on the State for the report modal the next frame renders (the
/// update itself is synchronous; D-03's frozen-window model).
pub fn mapUpdate(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    if (!panels.documentLoaded(state.editor)) {
        state.editor.note("no map to update");
        return .refused;
    }
    var progress = UpdateProgress{};
    state.editor.updateMap(UpdateProgress.report, &progress) catch |err| {
        state.view.noteEditResult(state.editor, err);
        return .refused;
    };
    state.update_steps = progress.steps;
    state.update_total = progress.total;
    // A scripted run must keep its viewport clickable: the report's steps
    // are in the status line either way (the predicates read them).
    if (!state.automated) state.update_report_open = true;
    state.view.clearStatus();
    return .ok;
}

/// `do=map_fill[:NN]` - Map > Fill Entire Map (M3, D-22): every tile the
/// terrain type's own, one undo step. With no argument the brush tile is
/// filled (what the menu item means); `:NN` names the tile (what a script
/// means - a tile the map's tileset has no terrain type for is refused).
/// The confirmation is the caller's: the menu asks first, a script has
/// already said yes by naming the command.
pub fn mapFill(state: *State, arg: []const u8) Outcome {
    if (!panels.documentLoaded(state.editor)) {
        state.editor.note("no map to fill");
        return .refused;
    }
    var tile: u8 = state.view.brush.tile;
    if (arg.len != 0) {
        const parsed = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
        if (parsed < 0 or parsed > 255) return .bad_arg;
        tile = @intCast(parsed);
    }
    state.editor.fillEntireMap(tile) catch |err| {
        state.view.noteEditResult(state.editor, err);
        return .refused;
    };
    state.view.clearStatus();
    return .ok;
}

/// `do=instant_update` - Map > Instant Update Map Mode (M3, D-20): the
/// toggle, one way or the other, on the bridge session and in the settings
/// (mapeditor.cfg carries it to the next start; the session keeps the live
/// copy until the next call).
pub fn instantUpdate(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const next = !state.settings.instant_update;
    state.editor.setTerrainModes(next, state.settings.fit_to_grid) catch |err| {
        state.view.noteEditResult(state.editor, err);
        return .refused;
    };
    state.settings.instant_update = next;
    state.settings_changed = true;
    return .ok;
}

/// `do=fit_grid` - Map > Fit Objects To Grid (M3, D-20): the toggle, as
/// instant_update's. Default on, the MFC's own.
pub fn fitGrid(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const next = !state.settings.fit_to_grid;
    state.editor.setTerrainModes(state.settings.instant_update, next) catch |err| {
        state.view.noteEditResult(state.editor, err);
        return .refused;
    };
    state.settings.fit_to_grid = next;
    state.settings_changed = true;
    return .ok;
}

/// `do=tile_info:NN` - the tile properties (M3, D-35/TR2): the tile's
/// terrain type name and its variant count, read-only, into the status
/// line - tile 0 included, which the MFC's own `> 0` guard
/// (TabTileEditDialog.cpp:316) never answered. With no argument it is the
/// brush tile. A tile the map's tileset does not list is refused, naming it.
fn tileInfo(state: *State, arg: []const u8) Outcome {
    var tile: u8 = state.view.brush.tile;
    if (arg.len != 0) {
        const parsed = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
        if (parsed < 0 or parsed > 255) return .bad_arg;
        tile = @intCast(parsed);
    }
    const info = state.real.describeTile(tile) orelse {
        state.editor.note("the map's tileset does not list that tile");
        return .refused;
    };
    var line: [160]u8 = undefined;
    const text = std.fmt.bufPrint(&line, "tile {d}: {s}, {d} variants", .{
        tile, std.mem.sliceTo(&info.terrain, 0), info.variant_count,
    }) catch "tile";
    state.editor.note(text);
    return .ok;
}

/// A Save As with the format named (D-24: the bridge takes the format from
/// the path's extension, so the format is what an extensionless path is
/// given). With no argument it is the menu item - the Save As dialog, the
/// forced format riding the request. With `:<path>` it is the scripted leg
/// of the same: the path (an engine path, backslashes fine) is delivered
/// through the dialog slot exactly a dialog's own choice would be, and gets
/// the format's extension when it names none.
fn saveAsFormat(state: *State, arg: []const u8, which: core.settings.Format) Outcome {
    if (!panels.documentLoaded(state.editor)) {
        state.editor.note("no map to save");
        return .refused;
    }
    if (arg.len == 0) {
        panels.requestSaveAsFormat(state, which);
        return .ok;
    }
    if (arg.len >= core.files.max_path) return .bad_arg;
    var path_buffer: [core.files.max_path]u8 = undefined;
    var len = arg.len;
    @memcpy(path_buffer[0..len], arg);
    const extension = core.settings.formatExtension(which);
    const has_extension = std.ascii.endsWithIgnoreCase(arg, ".bzm") or std.ascii.endsWithIgnoreCase(arg, ".xml");
    if (!has_extension) {
        if (len + extension.len > path_buffer.len) return .bad_arg;
        @memcpy(path_buffer[len..][0..extension.len], extension);
        len += extension.len;
    }
    // A person picks an existing folder in the dialog; a script names one
    // that may not be there yet, so it is made (the saveas verb's own rule).
    if (std.fs.path.dirname(arg)) |folder| std.Io.Dir.cwd().createDirPath(state.io, folder) catch {};
    if (!state.actions.dialog.request(.save_as)) {
        state.editor.note("a dialog is already open");
        return .refused;
    }
    state.save_as_format_forced = which;
    state.actions.dialog.deliver(path_buffer[0..len]);
    return .ok;
}

fn fileSaveXml(state: *State, arg: []const u8) Outcome {
    return saveAsFormat(state, arg, .xml);
}

fn fileSaveBzm(state: *State, arg: []const u8) Outcome {
    return saveAsFormat(state, arg, .bzm);
}

/// `expect=title:...` (05-01, F15): the window title contains the text.
fn titleContains(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    const current = state.title[0..state.title_len];
    return if (std.mem.indexOf(u8, current, arg) != null) .ok else .refused;
}

/// `expect=status:...` (05-01, V6): the status line contains the text.
fn statusContains(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    var buffer: [1024]u8 = undefined;
    const line = panels.statusLine(state, &buffer);
    return if (std.mem.indexOf(u8, line, arg) != null) .ok else .refused;
}

/// `expect=palette_count:<n>` (M3, D-31): the object palette's visible row
/// count - the same two-stage query the palette draws with (the text filter
/// and the active object filters), so a filter_select/filter_toggle shows
/// its gate in the scenario.
fn paletteCount(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    const filter = std.mem.sliceTo(&state.filter, 0);
    var count: usize = 0;
    for (state.order) |index| {
        const entry = &state.catalogue[index];
        if (logic.paletteObjectVisible(std.mem.sliceTo(&entry.name, 0), std.mem.sliceTo(&entry.path, 0), filter, state.active_filters)) count += 1;
    }
    if (count == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "palette_count is {d}, not {d}", .{ count, want }) catch "palette_count differs");
    return .refused;
}

/// `expect=dirty:<0|1>` (M3, D-21): whether the document holds unsaved
/// edits - the fields apply is one undo step, and undoing it lands the
/// document back on its saved bytes.
fn dirtyIs(state: *State, arg: []const u8) Outcome {
    const want = std.mem.eql(u8, arg, "1");
    if (arg.len != 1 or (arg[0] != '0' and arg[0] != '1')) return .bad_arg;
    return if (state.editor.dirty() == want) .ok else .refused;
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

// ---------------------------------------------------------------------------
// Properties, links (M3, D-26/D-27): the Properties window's fields and the
// drop's link, every one a named command so the panel, the Selector's drop
// and a BK_EDITOR_AUTO `do=` run the same code.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// The direction wheel and the Damage tool (M3, D-28/D-29): the palette
// wheel's turn and the tool's hit, every one a named command so the widget,
// the tool and a BK_EDITOR_AUTO `do=` run the same code.
// ---------------------------------------------------------------------------

/// `do=wheel_turn:<degrees>`: the wheel's answer - the placer's placement
/// angle becomes `degrees` (0 east, counter-clockwise, the MFC's own dial),
/// and every selected object turns to face it, as the MFC frame turns each
/// selected object to the wheel's angle (TemplateEditorFrame1.cpp:1053-1090).
/// The frames of one drag over the dial share `state.wheel_gesture`, so the
/// whole drag is ONE undo step; a scripted call (gesture 0) is a step of
/// its own.
fn wheelTurn(state: *State, arg: []const u8) Outcome {
    const degrees = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (degrees < 0 or degrees >= 360) return .bad_arg;
    state.view.placer.dir = logic.degreesToDirection(@floatFromInt(degrees));
    if (!panels.mapIsOpen(state.editor)) return .ok;
    const members = state.editor.selectionMembers(state.allocator) catch return .refused;
    defer state.allocator.free(members);
    if (members.len == 0) return .ok;
    return resultOutcome(state, state.editor.turnSelection(members, @floatFromInt(degrees), state.wheel_gesture));
}

/// `do=damage_percent:<p>`: the Damage tool's percentage (0..100).
fn damagePercent(state: *State, arg: []const u8) Outcome {
    const percent = std.fmt.parseFloat(f32, arg) catch return .bad_arg;
    if (!(percent >= 0 and percent <= 100)) return .bad_arg;
    state.view.damage_tool.percent = percent;
    return .ok;
}

/// A scripted object reference (M3): a link ID, or `@<n>` - the n-th
/// selected object, the selection's members ascending by link ID (the order
/// `selectionMembers` answers and edits apply in) - so a scenario names the
/// objects it placed and banded without knowing the IDs the map handed out.
fn objectRef(state: *State, text: []const u8) ?i32 {
    if (text.len > 1 and text[0] == '@') {
        const index = std.fmt.parseInt(usize, text[1..], 10) catch return null;
        const members = state.editor.selectionMembers(state.allocator) catch return null;
        defer state.allocator.free(members);
        if (index >= members.len) {
            state.editor.note("the selection holds fewer objects");
            return null;
        }
        return members[index];
    }
    return std.fmt.parseInt(i32, text, 10) catch null;
}

fn damageModeOf(name: []const u8) ?core.bridge.DamageMode {
    if (std.mem.eql(u8, name, "damage")) return .damage;
    if (std.mem.eql(u8, name, "heal")) return .heal;
    if (std.mem.eql(u8, name, "repair")) return .repair_full;
    return null;
}

/// `do=damage:<object>:<mode>`: the Damage tool's hit as a command - the
/// object a link ID or `@<n>`, the mode one of damage, heal, repair - the
/// same editor call the tool's click makes, ONE undo step.
fn damageCommand(state: *State, arg: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const mode = damageModeOf(arg[colon + 1 ..]) orelse return .bad_arg;
    const link_id = objectRef(state, arg[0..colon]) orelse return .bad_arg;
    return resultOutcome(state, state.editor.damageObject(link_id, mode, state.view.damage_tool.percent / 100.0, 0));
}

/// `do=band_select:<tx0>-<ty0>-<tx1>-<ty1>`: the Ctrl rubber band as a
/// command - the tile rectangle over map cells, bridges and entrenchments
/// passed over, the pick's answer the selection - the same read the
/// Selector's Ctrl drag finishes with (the scripted pointer has no Ctrl).
fn bandSelect(state: *State, arg: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    // '-' separates the tiles: the auto scenario's entries are comma-joined.
    var parts = std.mem.splitScalar(u8, arg, '-');
    var tiles: [4]i32 = undefined;
    for (&tiles) |*tile| {
        const text = parts.next() orelse return .bad_arg;
        tile.* = std.fmt.parseInt(i32, text, 10) catch return .bad_arg;
    }
    if (parts.next() != null) return .bad_arg;
    const bridge = state.editor.bridge;
    var total: usize = 0;
    var none: [0]i32 = .{};
    const sizing = bridge.pickObjectsInTiles(tiles[0], tiles[1], tiles[2], tiles[3], &none, &total);
    if (sizing != .ok and sizing != .refused) return .refused;
    const members = state.allocator.alloc(i32, total) catch return .refused;
    defer state.allocator.free(members);
    var got: usize = 0;
    if (bridge.pickObjectsInTiles(tiles[0], tiles[1], tiles[2], tiles[3], members, &got) != .ok) return .refused;
    if (got != total) return .refused;
    state.editor.selectionReplace(members[0..got]);
    return .ok;
}

/// `expect=selection_count:<n>`: the selection holds exactly n objects.
fn selectionCountIs(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    const got = state.editor.selectionCount();
    if (got == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "{d} objects selected, not {d}", .{ got, want }) catch "selection differs");
    return .refused;
}

/// `expect=objects:<n>`: the document holds exactly n objects (a delete and
/// its undo, counted).
fn objectsIs(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const got = state.editor.document.objects.items.len;
    if (got == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "{d} objects on the map, not {d}", .{ got, want }) catch "object count differs");
    return .refused;
}

/// `expect=placer_angle:<degrees>`: the wheel's angle is the placer's.
fn placerAngleIs(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    const got = logic.directionToDegrees(state.view.placer.dir);
    const got_rounded: i32 = @intFromFloat(got);
    if (got_rounded == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "placer angle is {d}, not {d}", .{ got_rounded, want }) catch "placer angle differs");
    return .refused;
}

/// `expect=angle:<object>:<degrees>`: the object faces that many whole
/// degrees (the properties' own direction-to-degrees reading).
fn angleIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const want = std.fmt.parseInt(i32, arg[colon + 1 ..], 10) catch return .bad_arg;
    const link_id = objectRef(state, arg[0..colon]) orelse return .bad_arg;
    const object = state.editor.document.find(link_id) orelse return .refused;
    const got: i32 = @intFromFloat(logic.directionToDegrees(object.dir));
    if (got == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "angle is {d}, not {d}", .{ got, want }) catch "angle differs");
    return .refused;
}

/// `expect=hp:<object>:<percent>`: the object's health is that percentage
/// of full, read from the document (two decimals of slack).
fn hpIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const want_percent = std.fmt.parseFloat(f32, arg[colon + 1 ..]) catch return .bad_arg;
    const link_id = objectRef(state, arg[0..colon]) orelse return .bad_arg;
    const object = state.editor.document.find(link_id) orelse return .refused;
    const got_percent = object.hp * 100.0;
    if (@abs(got_percent - want_percent) < 0.011) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "hp is {d:.2}%, not {d:.2}%", .{ got_percent, want_percent }) catch "hp differs");
    return .refused;
}

/// `expect=link_with:<object>=<host>`: the object's nLinkWith names the
/// host (`0`: linked with nothing).
fn linkWithIs(state: *State, arg: []const u8) Outcome {
    const bar = std.mem.indexOfScalar(u8, arg, '=') orelse return .bad_arg;
    const link_id = objectRef(state, arg[0..bar]) orelse return .bad_arg;
    const want = objectRef(state, arg[bar + 1 ..]) orelse return .bad_arg;
    const object = state.editor.document.find(link_id) orelse return .refused;
    if (object.link_with == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "linked with {d}, not {d}", .{ object.link_with, want }) catch "link differs");
    return .refused;
}

/// `do=props_open:1|0` opens or closes the Properties window (the menu
/// checkbox is the same state).
fn propsOpen(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) {
        state.properties_open = true;
    } else if (std.mem.eql(u8, arg, "0")) {
        state.properties_open = false;
    } else return .bad_arg;
    return .ok;
}

/// `do=props_set:<field>=<value>`: the Properties panel's commit, the field
/// one of player, health (a percent), angle (degrees) or formation - the
/// Script ID rides the M2 `script_id` command. Applied to the whole
/// selection (the multi-selection's own set), ONE undo step.
fn propsSet(state: *State, arg: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    // '=' separates field and value: the auto scenario's entries are
    // comma-joined, so an argument may not carry a comma.
    const bar = std.mem.indexOfScalar(u8, arg, '=') orelse return .bad_arg;
    const field = arg[0..bar];
    const value_text = arg[bar + 1 ..];
    var fields: core.bridge.ObjectFieldsEdit = .{};
    if (std.mem.eql(u8, field, "player")) {
        fields.mask |= core.bridge.ObjectFieldsEdit.player_bit;
        fields.player = std.fmt.parseInt(c_int, value_text, 10) catch return .bad_arg;
    } else if (std.mem.eql(u8, field, "health")) {
        fields.mask |= core.bridge.ObjectFieldsEdit.hp_bit;
        const percent = std.fmt.parseFloat(f32, value_text) catch return .bad_arg;
        fields.hp = logic.clampHealthPercent(percent) / 100.0;
    } else if (std.mem.eql(u8, field, "angle")) {
        fields.mask |= core.bridge.ObjectFieldsEdit.angle_bit;
        fields.angle = std.fmt.parseFloat(f32, value_text) catch return .bad_arg;
    } else if (std.mem.eql(u8, field, "formation")) {
        fields.mask |= core.bridge.ObjectFieldsEdit.formation_bit;
        fields.formation = std.fmt.parseInt(c_int, value_text, 10) catch return .bad_arg;
    } else return .bad_arg;
    const members = state.editor.selectionMembers(state.allocator) catch return .refused;
    defer state.allocator.free(members);
    if (members.len == 0) {
        state.editor.note("select an object first");
        return .refused;
    }
    // The formation rides only when every member is a squad: any other
    // kind's frame index is its segment, never a formation.
    if (fields.mask & core.bridge.ObjectFieldsEdit.formation_bit != 0) {
        for (members) |member| {
            const object = state.editor.document.find(member) orelse return .refused;
            if (!isSquadName(state, object.nameSlice())) {
                state.editor.note("only a squad carries a formation");
                return .refused;
            }
        }
    }
    return resultOutcome(state, state.editor.applyObjectFieldsMany(members, fields));
}

fn isSquadName(state: *State, name: []const u8) bool {
    for (state.catalogue) |*entry| {
        if (std.mem.eql(u8, std.mem.sliceTo(&entry.name, 0), name)) return entry.game_type == 15;
    }
    return false;
}

/// `do=link_make:<source>=<target>`: the drop's link, refused with
/// CheckForInserting's own reason when the rules say no. Each side is a
/// link ID or `@<n>` (the n-th selected object); '=' separates them, as in
/// `props_set`, since the auto scenario's entries are comma-joined.
fn linkMake(state: *State, arg: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const bar = std.mem.indexOfScalar(u8, arg, '=') orelse return .bad_arg;
    const source = objectRef(state, arg[0..bar]) orelse return .bad_arg;
    const target = objectRef(state, arg[bar + 1 ..]) orelse return .bad_arg;
    return resultOutcome(state, state.editor.makeLink(source, target));
}

/// `do=link_unlink:<object>`: the properties' units list unlink (a link ID
/// or `@<n>`).
fn linkUnlink(state: *State, arg: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const link_id = objectRef(state, arg) orelse return .bad_arg;
    return resultOutcome(state, state.editor.unlinkObject(link_id));
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
    if (core.script_file.pickedName(arg) == null) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    // WR-C04: the pick's own answer, never the map's state afterwards - a
    // refused copy of a script the map already names must not read as OK.
    return switch (panels.pickScript(state, arg, false)) {
        .chosen => .ok,
        .asked => blk: {
            state.editor.note("a different file of that name is beside the map; the Replace it? question is up");
            break :blk .refused;
        },
        .refused => .refused,
    };
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
    return if (panels.pickScript(state, picked.slice(), true) == .chosen) .ok else .refused;
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
/// no object): what command 0's target is. The record has no "unset" flag, so a
/// target at exactly map point (0,0) - the map's corner - reads as no target
/// here and draws no line in the markers (IN-C06, an accepted limit).
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

/// `do=placer_name:Sdkfz_8`: the Place tool's object becomes the placeable object of the
/// catalogue with that name - a unit, a squad or a building, whatever the palette itself
/// would put in hand (M3: the multi-selection, properties and link frames place a squad
/// and a building). For a scenario that needs one particular object - a truck strong
/// enough for the gun it tows - and refused, saying so, when the catalogue has no such
/// placeable object (a lone soldier is not placeable and stays refused).
fn placerName(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    for (state.catalogue) |*entry| {
        if (entry.placeable == 0) continue;
        const name = std.mem.sliceTo(&entry.name, 0);
        if (!std.mem.eql(u8, name, arg)) continue;
        state.view.setPlacerObject(name);
        return .ok;
    }
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "the catalogue has no placeable object named {s}", .{arg}) catch "no such unit");
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

// ---------------------------------------------------------------------------
// Heights (M3, D-18): the Heights panel's fields and its two confirmed
// actions, every one a named command so the panel's controls and a
// BK_EDITOR_AUTO `do=` run the same code. The confirmations themselves are
// the panel's popups (the MFC's own Yes/No); a script's `do=` IS the yes.
// ---------------------------------------------------------------------------

/// `do=heights_window:1|0` opens or closes the Heights window.
fn heightsWindow(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) {
        state.heights_open = true;
    } else if (std.mem.eql(u8, arg, "0")) {
        state.heights_open = false;
    } else return .bad_arg;
    return .ok;
}

/// `do=heights_brush:NN` - the brush, 2..16, the MFC slider's own range.
fn heightsBrush(state: *State, arg: []const u8) Outcome {
    const brush = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (brush < 2 or brush > 16) return .bad_arg;
    state.view.heights_tool.brush = brush;
    return .ok;
}

/// `do=heights_speed:X` - the profile gradient's ceiling (world z units).
/// The MFC keeps the old value when the typed one is not a positive number
/// (TabTerrainAltitudesDialog.cpp:210-228); the command form is a bad_arg.
fn heightsSpeed(state: *State, arg: []const u8) Outcome {
    const speed = logic.parseHeightsFloat(arg) orelse return .bad_arg;
    if (speed <= 0) return .bad_arg;
    state.view.heights_tool.speed = speed;
    return .ok;
}

/// `do=heights_ratio:X` - the level step in percent of the distance to the
/// target; positive, like the MFC's own edit rule.
fn heightsRatio(state: *State, arg: []const u8) Outcome {
    const ratio = logic.parseHeightsFloat(arg) orelse return .bad_arg;
    if (ratio <= 0) return .bad_arg;
    state.view.heights_tool.ratio_percent = ratio;
    return .ok;
}

/// `do=heights_mode:zero|click_tile|instant_average|click_average` - what a
/// level stroke moves the terrain toward.
fn heightsMode(state: *State, arg: []const u8) Outcome {
    const mode = logic.heightsModeFromName(arg) orelse return .bad_arg;
    state.view.heights_tool.level_mode = mode;
    return .ok;
}

/// `do=heights_generate:hills|rocks|dunes:granularity:min_z:max_z` - the
/// MFC's Generate over the whole map, one undo step. The type names are the
/// dialog's own three (Hills TG_FBM, Rocks TG_HYBRID, Dunes TG_RIDGED); the
/// z values are world units per vertex, exactly the MFC's fParameters.
fn heightsGenerate(state: *State, arg: []const u8) Outcome {
    var fields = std.mem.splitScalar(u8, arg, ':');
    const type_name = fields.next() orelse return .bad_arg;
    const gen_type = logic.heightsGenerateTypeFromName(type_name) orelse return .bad_arg;
    const granularity = logic.parseHeightsFloat(fields.next() orelse return .bad_arg) orelse return .bad_arg;
    const min_z = logic.parseHeightsFloat(fields.next() orelse return .bad_arg) orelse return .bad_arg;
    const max_z = logic.parseHeightsFloat(fields.next() orelse return .bad_arg) orelse return .bad_arg;
    if (granularity <= 0 or max_z <= min_z) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.generateHeights(gen_type, granularity, min_z, max_z));
}

/// `do=heights_set_zero` - every height to 0, one undo step.
fn heightsSetZero(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.setZeroHeights());
}

// ---------------------------------------------------------------------------
// Object filters (M3, D-31): the palette's quick toggles, the combo and the
// Filters Composer. Filters are session data, never map data: nothing here
// touches the history, and the palette's cache refreshes off
// `filters_generation`.
// ---------------------------------------------------------------------------

fn findFilterByName(state: *State, name: []const u8) ?*core.bridge.ObjectFilter {
    for (state.editor.filtersSlice()) |*entry| {
        if (std.mem.eql(u8, entry.nameSlice(), name)) return entry;
    }
    return null;
}

/// `do=filter_select:<name>` - the palette's filter combo. Empty or `none`
/// clears it; an unknown name is refused.
fn filterSelect(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or std.mem.eql(u8, arg, "none")) {
        state.settings.filter_active.set("");
    } else {
        if (findFilterByName(state, arg) == null) {
            state.view.setStatus("filter: ", "no filter is named that");
            return .refused;
        }
        state.settings.filter_active.set(arg);
    }
    state.settings_changed = true;
    panels.refreshActiveFilters(state);
    return .ok;
}

/// `do=filter_toggle:<slot 0..8>` - a quick toggle's gate, on or off.
fn filterToggle(state: *State, arg: []const u8) Outcome {
    const slot = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (slot >= core.settings.Settings.filter_slot_count) return .bad_arg;
    state.filter_checked[slot] = !state.filter_checked[slot];
    panels.refreshActiveFilters(state);
    return .ok;
}

/// `do=filter_assign:<slot 0..8>` - Ctrl+click on a quick toggle: the combo's
/// filter becomes the slot's name (persisted, like the MFC's UpdateCheck
/// wrote the dialog parameter). Refused when the combo holds no filter or
/// the slot is out of range; a refusal changes nothing.
fn filterAssign(state: *State, arg: []const u8) Outcome {
    const slot = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (slot >= core.settings.Settings.filter_slot_count) return .bad_arg;
    const combo = state.settings.filter_active.slice();
    if (combo.len == 0) {
        state.view.setStatus("filter: ", "select a filter in the combo first, then Ctrl+click a toggle to assign it");
        return .refused;
    }
    state.settings.setFilterSlot(slot, combo);
    state.settings_changed = true;
    panels.refreshActiveFilters(state);
    return .ok;
}

/// `do=filter_new:<name>` - the Filters Composer's (and the palette popup's)
/// New Filter: an empty user filter appended and selected.
fn filterNew(state: *State, arg: []const u8) Outcome {
    const outcome = resultOutcome(state, state.editor.filterNew(arg));
    if (outcome == .ok) {
        state.settings.filter_active.set(arg);
        state.settings_changed = true;
        panels.refreshActiveFilters(state);
    }
    return outcome;
}

/// `do=filter_delete:<name>` - the composer's Delete: the filter leaves the
/// live list (its user-file override with it, on the next save). Deleting
/// the combo's or a slot's filter clears those references.
fn filterDelete(state: *State, arg: []const u8) Outcome {
    const combo = state.settings.filter_active.slice();
    const was_combo = std.mem.eql(u8, combo, arg);
    var slot_was: [core.settings.Settings.filter_slot_count]bool = @splat(false);
    for (0..core.settings.Settings.filter_slot_count) |i| {
        slot_was[i] = std.mem.eql(u8, state.settings.filterSlot(i), arg);
    }
    const outcome = resultOutcome(state, state.editor.filterDelete(arg));
    if (outcome == .ok) {
        if (was_combo) {
            state.settings.filter_active.set("");
            state.settings_changed = true;
        }
        for (0..core.settings.Settings.filter_slot_count) |i| {
            if (slot_was[i]) {
                state.settings.setFilterSlot(i, "");
                state.settings_changed = true;
            }
        }
        panels.refreshActiveFilters(state);
    }
    return outcome;
}

/// `do=filter_rename:<old>|<new>` - the composer's Rename (the `|` is the
/// separator; filter names reject it, so the pair is unambiguous).
fn filterRename(state: *State, arg: []const u8) Outcome {
    const bar = std.mem.indexOfScalar(u8, arg, '|') orelse return .bad_arg;
    const old_name = arg[0..bar];
    const new_name = arg[bar + 1 ..];
    const combo = state.settings.filter_active.slice();
    var slot_was: [core.settings.Settings.filter_slot_count]bool = @splat(false);
    for (0..core.settings.Settings.filter_slot_count) |i| {
        slot_was[i] = std.mem.eql(u8, state.settings.filterSlot(i), old_name);
    }
    const outcome = resultOutcome(state, state.editor.filterRename(old_name, new_name));
    if (outcome == .ok) {
        if (std.mem.eql(u8, combo, old_name)) {
            state.settings.filter_active.set(new_name);
            state.settings_changed = true;
        }
        for (0..core.settings.Settings.filter_slot_count) |i| {
            if (slot_was[i]) {
                state.settings.setFilterSlot(i, new_name);
                state.settings_changed = true;
            }
        }
        panels.refreshActiveFilters(state);
    }
    return outcome;
}

/// `do=filters_save` - the composer's Save: the user-owned filters are
/// written to <UserRoot>mapeditor/filter.xml in the shipped file's own XML
/// shape; a shipped name the user has not touched is not copied.
fn filtersSave(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const outcome = resultOutcome(state, state.editor.saveFilters());
    if (outcome == .ok) panels.refreshActiveFilters(state);
    return outcome;
}

/// `do=filters_composer` - the Tools menu's Filters Composer checkbox as a
/// command, so BK_EDITOR_AUTO opens and closes the window (D-31, O4).
fn filtersComposer(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.filters_composer_open = !state.filters_composer_open;
    return .ok;
}

/// `do=filter_words:<name>|<list>|<words space separated>` - the composer's
/// word-list edit: condition `list` of the named filter becomes exactly the
/// words given (an empty word list empties the condition). The `|`-separated
/// form never appears in a BK_EDITOR_AUTO frame (names reject `|`, words
/// reject spaces); the composer's commit-on-deactivate is its caller. The
/// other conditions stand.
fn filterWords(state: *State, arg: []const u8) Outcome {
    const name_end = std.mem.indexOfScalar(u8, arg, '|') orelse return .bad_arg;
    const name = arg[0..name_end];
    const rest = arg[name_end + 1 ..];
    const list_end = std.mem.indexOfScalar(u8, rest, '|') orelse return .bad_arg;
    const list_index = std.fmt.parseInt(usize, rest[0..list_end], 10) catch return .bad_arg;
    if (list_index >= core.bridge.filter_max_lists) return .bad_arg;
    const words_text = rest[list_end + 1 ..];
    const filter = findFilterByName(state, name) orelse {
        state.view.setStatus("filter: ", "no filter is named that");
        return .refused;
    };
    var updated = filter.*;
    updated.user = 1;
    var words: [core.bridge.filter_max_words][]const u8 = undefined;
    var count: usize = 0;
    var it = std.mem.splitScalar(u8, words_text, ' ');
    while (it.next()) |word| {
        if (word.len == 0) continue;
        if (count == core.bridge.filter_max_words) {
            state.view.setStatus("filter: ", "a word list holds at most 8 words");
            return .refused;
        }
        if (word.len >= core.bridge.filter_word_capacity) {
            state.view.setStatus("filter: ", "a word is at most 31 characters");
            return .refused;
        }
        words[count] = word;
        count += 1;
    }
    for (words[0..count], 0..) |word, w| {
        @memcpy(updated.lists[list_index].words[w][0..word.len], word);
        updated.lists[list_index].words[w][word.len] = 0;
    }
    if (count < core.bridge.filter_max_words) {
        @memset(&updated.lists[list_index].words[count], 0);
    }
    updated.lists[list_index].word_count = @intCast(count);
    return resultOutcome(state, state.editor.filterPut(updated));
}


// ---------------------------------------------------------------------------
// The Fields tool (M3, D-21): the panel's controls as named commands, so
// BK_EDITOR_AUTO drives the same path. The polygon itself is the tool's
// gesture (press/right/double-click, view.zig's handleFields) - the vertex
// commands exist for scripts that name world points directly.
// ---------------------------------------------------------------------------

/// `do=fields_set:<storage-relative name>` - the field-set combo's choice.
fn fieldsSet(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or arg.len >= core.bridge.field_set_name_capacity) return .bad_arg;
    state.fields_set_name = [_:0]u8{0} ** core.bridge.field_set_name_capacity;
    @memcpy(state.fields_set_name[0..arg.len], arg[0..arg.len]);
    return .ok;
}

/// `do=fields_randomize:<min>:<width>:<dist>` - the MFC Randomize dialog's
/// three numbers (min length cells >= 2, width 0..0.5, disturbance 0..1).
fn fieldsRandomize(state: *State, arg: []const u8) Outcome {
    var it = std.mem.splitScalar(u8, arg, ':');
    const min_text = it.next() orelse return .bad_arg;
    const width_text = it.next() orelse return .bad_arg;
    const dist_text = it.next() orelse return .bad_arg;
    const min_length = std.fmt.parseFloat(f32, min_text) catch return .bad_arg;
    const width = std.fmt.parseFloat(f32, width_text) catch return .bad_arg;
    const disturbance = std.fmt.parseFloat(f32, dist_text) catch return .bad_arg;
    if (!std.math.isFinite(min_length) or !std.math.isFinite(width) or !std.math.isFinite(disturbance)) return .bad_arg;
    if (min_length < 2 or width < 0 or width > 0.5 or disturbance < 0 or disturbance > 1) return .bad_arg;
    state.fields_randomize = true;
    state.fields_min_length = min_length;
    state.fields_width = width;
    state.fields_disturbance = disturbance;
    return .ok;
}

/// `do=fields_toggle:<what>` - one of the dialog's checkboxes:
/// randomize|terrain|objects|heights|update|passability|filter.
fn fieldsToggle(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "randomize")) {
        state.fields_randomize = !state.fields_randomize;
    } else if (std.mem.eql(u8, arg, "terrain")) {
        state.fields_fill_terrain = !state.fields_fill_terrain;
    } else if (std.mem.eql(u8, arg, "objects")) {
        state.fields_place_objects = !state.fields_place_objects;
    } else if (std.mem.eql(u8, arg, "heights")) {
        state.fields_modify_heights = !state.fields_modify_heights;
    } else if (std.mem.eql(u8, arg, "update")) {
        state.fields_update_after = !state.fields_update_after;
    } else if (std.mem.eql(u8, arg, "passability")) {
        state.fields_check_passability = !state.fields_check_passability;
    } else if (std.mem.eql(u8, arg, "filter")) {
        state.fields_filter_objects = !state.fields_filter_objects;
    } else return .bad_arg;
    return .ok;
}

/// `do=fields_apply` / `:yes` / `:passability` - the application over the
/// tool's pending polygon. A season mismatch refuses with the two seasons
/// named (the MFC's IDS_INVALID_FIELD_SEASON question), and `:yes` is the
/// answer - the panel's popup sends it, exactly the heights confirmations'
/// split. `:passability` runs the report over the polygon, changing nothing.
fn fieldsApply(state: *State, arg: []const u8) Outcome {
    var diag_buffer: [128]u8 = undefined;
    const confirmed = std.mem.eql(u8, arg, "yes");
    const passability_only = std.mem.eql(u8, arg, "passability");
    if (arg.len != 0 and !confirmed and !passability_only) return .bad_arg;
    const tool = &state.view.fields_tool;
    var points: [core.tools_fields.max_points]core.bridge.FieldVec3 = undefined;
    const count = tool.applyPoints(&points) orelse {
        state.view.setStatus("fields: ", std.fmt.bufPrint(&diag_buffer, "the polygon is not closed: {d} point(s), state {s}", .{ tool.points().len, @tagName(tool.state) }) catch "the polygon is not closed: three points and a real area are needed");
        return .refused;
    };
    const name = std.mem.sliceTo(&state.fields_set_name, 0);
    if (name.len == 0) {
        state.view.setStatus("fields: ", "no field set is chosen");
        return .refused;
    }
    if (!panels.mapIsOpen(state.editor)) return .refused;

    // The season confirmation (the MFC's own flow, the dialog above the
    // application): a mismatch asks, `yes` answers.
    const season = state.editor.fieldSetSeason(name) catch |err| {
        state.view.noteEditResult(state.editor, err);
        return .refused;
    };
    const map_season = state.editor.document.info.season;
    if (season != map_season and !confirmed and !passability_only) {
        var buffer: [160]u8 = undefined;
        const message = std.fmt.bufPrint(&buffer, "the field set's season ({d}) differs from the map's ({d}); apply anyway?", .{ season, map_season }) catch
            "the field set's season differs from the map's";
        state.view.setStatus("fields: ", message);
        return .refused;
    }

    var params: core.bridge.FieldApplyParams = .{};
    params.setFieldSet(name);
    params.point_count = @intCast(count);
    params.points = &points;
    params.randomize = if (state.fields_randomize) 1 else 0;
    params.min_length = state.fields_min_length;
    params.width = state.fields_width;
    params.disturbance = state.fields_disturbance;
    params.fill_terrain = if (state.fields_fill_terrain) 1 else 0;
    params.place_objects = if (state.fields_place_objects) 1 else 0;
    params.modify_heights = if (state.fields_modify_heights) 1 else 0;
    params.update_map_after = if (state.fields_update_after) 1 else 0;
    params.check_passability_only = if (passability_only) 1 else 0;
    params.can_add_object_filter = if (state.fields_filter_objects) 1 else 0;
    if (state.fields_filter_objects) params.setFilter(state.settings.filter_active.slice());

    var report: std.ArrayListUnmanaged(core.bridge.FieldObjectReport) = .empty;
    const outcome = resultOutcome(state, state.editor.applyField(params, &report, state.allocator));
    report.deinit(state.allocator);
    if (outcome != .ok) {
        // Why the bridge refused: the polygon as the tool holds it.
        var pts: [128]u8 = undefined;
        var len: usize = 0;
        for (tool.points(), 0..) |pt, i| {
            const one = std.fmt.bufPrint(pts[len..], "{s}({d:.0},{d:.0})", .{ if (i != 0) " " else "", pt.x, pt.y }) catch break;
            len += one.len;
        }
        state.editor.note(std.fmt.bufPrint(&diag_buffer, "fields polygon: {s}", .{pts[0..len]}) catch "fields polygon: ?");
    }
    if (outcome == .ok) {
        // The MFC cleared the points on a successful place
        // (StateTerrainFields.cpp:506-512); a report-only run keeps them.
        if (!passability_only) tool.clear();
        state.view.clearStatus();
    }
    return outcome;
}

/// `do=fields_vertex_add:<wx>:<wy>` - one polygon vertex at a world point,
/// for scripts that name coordinates directly (the tool's own gesture is
/// presses and drags).
fn fieldsVertexAdd(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const wx = std.fmt.parseFloat(f32, arg[0..colon]) catch return .bad_arg;
    const wy = std.fmt.parseFloat(f32, arg[colon + 1 ..]) catch return .bad_arg;
    if (!std.math.isFinite(wx) or !std.math.isFinite(wy)) return .bad_arg;
    if (!state.view.fields_tool.vertexAdd(wx, wy)) {
        state.view.setStatus("fields: ", "the polygon holds 64 points at most");
        return .refused;
    }
    return .ok;
}

/// `do=fields_vertex_clear` - the pending polygon goes.
fn fieldsVertexClear(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.view.fields_tool.clear();
    return .ok;
}
