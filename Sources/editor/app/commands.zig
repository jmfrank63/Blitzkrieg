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
const minimap = @import("minimap.zig");

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
    // 05-09 (D-06..D-12): the Containers and Graphs Composers.
    .{ .name = "rmgc_window", .handler = rmgcWindow },
    .{ .name = "rmgc_new", .handler = rmgcNew },
    .{ .name = "rmgc_open", .handler = rmgcOpen },
    .{ .name = "rmgc_save", .handler = rmgcSave },
    .{ .name = "rmgc_saveas", .handler = rmgcSaveAs },
    .{ .name = "rmgc_patch_add", .handler = rmgcPatchAdd },
    .{ .name = "rmgc_patch_del", .handler = rmgcPatchDel },
    .{ .name = "rmgc_patch_set", .handler = rmgcPatchSet },
    .{ .name = "rmgc_import", .handler = rmgcImport },
    .{ .name = "rmgc_import_yes", .handler = rmgcImportYes },
    .{ .name = "rmgc_import_no", .handler = rmgcImportNo },
    .{ .name = "rmgc_check", .handler = rmgcCheck },
    .{ .name = "rmgc_fix", .handler = rmgcFix },
    .{ .name = "rmgc_fix_all", .handler = rmgcFixAll },
    .{ .name = "rmgc_undo", .handler = rmgcUndo },
    .{ .name = "rmgc_redo", .handler = rmgcRedo },
    .{ .name = "rmgg_window", .handler = rmggWindow },
    .{ .name = "rmgg_new", .handler = rmggNew },
    .{ .name = "rmgg_open", .handler = rmggOpen },
    .{ .name = "rmgg_save", .handler = rmggSave },
    .{ .name = "rmgg_saveas", .handler = rmggSaveAs },
    .{ .name = "rmgg_zoom", .handler = rmggZoom },
    .{ .name = "rmgg_drag", .handler = rmggDrag },
    .{ .name = "rmgg_ctrl_drag", .handler = rmggCtrlDrag },
    .{ .name = "rmgg_node", .handler = rmggNode },
    .{ .name = "rmgg_node_del", .handler = rmggNodeDel },
    .{ .name = "rmgg_link", .handler = rmggLink },
    .{ .name = "rmgg_link_del", .handler = rmggLinkDel },
    .{ .name = "rmgg_check", .handler = rmggCheck },
    .{ .name = "rmgg_fix", .handler = rmggFix },
    .{ .name = "rmgg_fix_all", .handler = rmggFixAll },
    .{ .name = "rmgg_undo", .handler = rmggUndo },
    .{ .name = "rmgg_redo", .handler = rmggRedo },
    // 05-10 (D-06/D-07/D-12): the Fields Composer.
    .{ .name = "rmgf_window", .handler = rmgfWindow },
    .{ .name = "rmgf_new", .handler = rmgfNew },
    .{ .name = "rmgf_open", .handler = rmgfOpen },
    .{ .name = "rmgf_save", .handler = rmgfSave },
    .{ .name = "rmgf_saveas", .handler = rmgfSaveAs },
    .{ .name = "rmgf_tab", .handler = rmgfTab },
    .{ .name = "rmgf_season", .handler = rmgfSeason },
    .{ .name = "rmgf_shell_add", .handler = rmgfShellAdd },
    .{ .name = "rmgf_shell_del", .handler = rmgfShellDel },
    .{ .name = "rmgf_shell_set", .handler = rmgfShellSet },
    .{ .name = "rmgf_shell_pick", .handler = rmgfShellPick },
    .{ .name = "rmgf_tile_add", .handler = rmgfTileAdd },
    .{ .name = "rmgf_tile_del", .handler = rmgfTileDel },
    .{ .name = "rmgf_tile_weight", .handler = rmgfTileWeight },
    .{ .name = "rmgf_object_add", .handler = rmgfObjectAdd },
    .{ .name = "rmgf_object_del", .handler = rmgfObjectDel },
    .{ .name = "rmgf_object_weight", .handler = rmgfObjectWeight },
    .{ .name = "rmgf_filter", .handler = rmgfFilter },
    .{ .name = "rmgf_set", .handler = rmgfSet },
    .{ .name = "rmgf_check", .handler = rmgfCheck },
    .{ .name = "rmgf_fix", .handler = rmgfFix },
    .{ .name = "rmgf_fix_all", .handler = rmgfFixAll },
    .{ .name = "rmgf_undo", .handler = rmgfUndo },
    .{ .name = "rmgf_redo", .handler = rmgfRedo },
    // 05-10 (D-06/D-07/D-12): the Templates Composer.
    .{ .name = "rmgt_window", .handler = rmgtWindow },
    .{ .name = "rmgt_new", .handler = rmgtNew },
    .{ .name = "rmgt_open", .handler = rmgtOpen },
    .{ .name = "rmgt_save", .handler = rmgtSave },
    .{ .name = "rmgt_saveas", .handler = rmgtSaveAs },
    .{ .name = "rmgt_graph_add", .handler = rmgtGraphAdd },
    .{ .name = "rmgt_graph_del", .handler = rmgtGraphDel },
    .{ .name = "rmgt_field_add", .handler = rmgtFieldAdd },
    .{ .name = "rmgt_field_del", .handler = rmgtFieldDel },
    .{ .name = "rmgt_vso_add", .handler = rmgtVsoAdd },
    .{ .name = "rmgt_vso_del", .handler = rmgtVsoDel },
    .{ .name = "rmgt_weight_set", .handler = rmgtWeightSet },
    .{ .name = "rmgt_vso_set", .handler = rmgtVsoSet },
    .{ .name = "rmgt_default_field", .handler = rmgtDefaultField },
    .{ .name = "rmgt_script", .handler = rmgtScript },
    .{ .name = "rmgt_mod", .handler = rmgtMod },
    .{ .name = "rmgt_player_add", .handler = rmgtPlayerAdd },
    .{ .name = "rmgt_player_del", .handler = rmgtPlayerDel },
    .{ .name = "rmgt_player_side", .handler = rmgtPlayerSide },
    .{ .name = "rmgt_game_type", .handler = rmgtGameType },
    .{ .name = "rmgt_units_set", .handler = rmgtUnitsSet },
    .{ .name = "rmgt_appear_add", .handler = rmgtAppearAdd },
    .{ .name = "rmgt_appear_set", .handler = rmgtAppearSet },
    .{ .name = "rmgt_appear_del", .handler = rmgtAppearDel },
    .{ .name = "rmgt_popup", .handler = rmgtPopup },
    .{ .name = "rmgt_check", .handler = rmgtCheck },
    .{ .name = "rmgt_fix", .handler = rmgtFix },
    .{ .name = "rmgt_fix_all", .handler = rmgtFixAll },
    .{ .name = "rmgt_undo", .handler = rmgtUndo },
    .{ .name = "rmgt_redo", .handler = rmgtRedo },
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
    // 05-06 (D-32): the Layers menu.
    .{ .name = "layer_toggle", .handler = layerToggle },
    .{ .name = "layer_set", .handler = layerSet },
    .{ .name = "fire_range", .handler = fireRange },
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
    .{ .name = "player_add", .handler = playerAdd },
    .{ .name = "player_delete", .handler = playerDelete },
    .{ .name = "player_side", .handler = playerSide },
    .{ .name = "unit_creation_window", .handler = unitCreationWindow },
    .{ .name = "unit_creation_player", .handler = unitCreationPlayer },
    .{ .name = "unit_creation_set", .handler = unitCreationSet },
    .{ .name = "appear_point_here", .handler = appearPointHere },
    .{ .name = "appear_point_add", .handler = appearPointAdd },
    .{ .name = "appear_point_set", .handler = appearPointSet },
    .{ .name = "appear_point_remove", .handler = appearPointRemove },
    .{ .name = "check_map", .handler = checkMapCommand },
    .{ .name = "check_map_fix_all", .handler = checkMapFixAll },
    .{ .name = "check_jump", .handler = checkJump },
    .{ .name = "check_window", .handler = checkWindow },
    // 05-07 (D-14..D-17): the Minimap panel and Create Minimap Images.
    .{ .name = "minimap_toggle", .handler = minimapToggle },
    .{ .name = "minimap_mode", .handler = minimapMode },
    .{ .name = "minimap_click", .handler = minimapClick },
    .{ .name = "minimap_create", .handler = minimapCreate },
    // 05-08 (D-01..D-05, D-13): Create Random Map and Tools > Export lists.
    .{ .name = "rmg_dialog", .handler = rmgDialogCommand },
    .{ .name = "rmg_set", .handler = rmgSetCommand },
    .{ .name = "rmg_generate", .handler = rmgGenerateCommand },
    .{ .name = "export_lists", .handler = exportListsCommand },
    .{ .name = "undo", .handler = undoCommand },
    .{ .name = "redo", .handler = redoCommand },
};

pub const predicate_table = [_]Entry{
    // 05-09: what the composers hold.
    .{ .name = "rmgc_patches", .handler = rmgcPatchesAre },
    .{ .name = "rmgc_dirty", .handler = rmgcDirtyIs },
    .{ .name = "rmgc_name", .handler = rmgcNameEndsWith },
    .{ .name = "rmgc_findings", .handler = rmgcFindingsAre },
    .{ .name = "rmgc_errors", .handler = rmgcErrorsAre },
    .{ .name = "rmgc_cell", .handler = rmgcCellIs },
    .{ .name = "rmgc_place", .handler = rmgcPlaceIs },
    .{ .name = "rmgc_size", .handler = rmgcSizeIs },
    .{ .name = "rmgc_pending", .handler = rmgcPendingIs },
    .{ .name = "rmgc_listed", .handler = rmgcListedAtLeast },
    .{ .name = "rmgg_nodes", .handler = rmggNodesAre },
    .{ .name = "rmgg_links", .handler = rmggLinksAre },
    .{ .name = "rmgg_dirty", .handler = rmggDirtyIs },
    .{ .name = "rmgg_name", .handler = rmggNameEndsWith },
    .{ .name = "rmgg_findings", .handler = rmggFindingsAre },
    .{ .name = "rmgg_errors", .handler = rmggErrorsAre },
    .{ .name = "rmgg_node_container", .handler = rmggNodeContainerIs },
    .{ .name = "rmgg_link_parts", .handler = rmggLinkPartsAre },
    .{ .name = "rmgg_zoom_is", .handler = rmggZoomIs },
    .{ .name = "rmgg_listed", .handler = rmggListedAtLeast },
    .{ .name = "rmgf_dirty", .handler = rmgfDirtyIs },
    .{ .name = "rmgf_name", .handler = rmgfNameEndsWith },
    .{ .name = "rmgf_findings", .handler = rmgfFindingsAre },
    .{ .name = "rmgf_errors", .handler = rmgfErrorsAre },
    .{ .name = "rmgf_season_is", .handler = rmgfSeasonIs },
    .{ .name = "rmgf_shells", .handler = rmgfShellsAre },
    .{ .name = "rmgf_tiles", .handler = rmgfTilesAre },
    .{ .name = "rmgf_objects", .handler = rmgfObjectsAre },
    .{ .name = "rmgf_value", .handler = rmgfValueIs },
    .{ .name = "rmgf_listed", .handler = rmgfListedAtLeast },
    .{ .name = "rmgf_avail", .handler = rmgfAvailableAtLeast },
    .{ .name = "rmgf_tab_is", .handler = rmgfTabIs },
    .{ .name = "rmgt_dirty", .handler = rmgtDirtyIs },
    .{ .name = "rmgt_name", .handler = rmgtNameEndsWith },
    .{ .name = "rmgt_findings", .handler = rmgtFindingsAre },
    .{ .name = "rmgt_errors", .handler = rmgtErrorsAre },
    .{ .name = "rmgt_graphs", .handler = rmgtGraphsAre },
    .{ .name = "rmgt_fields", .handler = rmgtFieldsAre },
    .{ .name = "rmgt_vsos", .handler = rmgtVsosAre },
    .{ .name = "rmgt_players", .handler = rmgtPlayersAre },
    .{ .name = "rmgt_default", .handler = rmgtDefaultIs },
    .{ .name = "rmgt_weight", .handler = rmgtWeightIs },
    .{ .name = "rmgt_vso_is", .handler = rmgtVsoIs },
    .{ .name = "rmgt_script_is", .handler = rmgtScriptIs },
    .{ .name = "rmgt_mod_is", .handler = rmgtModIs },
    .{ .name = "rmgt_sides", .handler = rmgtSidesAre },
    .{ .name = "rmgt_game_type_is", .handler = rmgtGameTypeIs },
    .{ .name = "rmgt_unit", .handler = rmgtUnitIs },
    .{ .name = "rmgt_appear", .handler = rmgtAppearIs },
    .{ .name = "rmgt_size", .handler = rmgtSizeIs },
    .{ .name = "rmgt_listed", .handler = rmgtListedAtLeast },
    .{ .name = "anchor_set", .handler = anchorSet },
    .{ .name = "anchor_unset", .handler = anchorUnset },
    .{ .name = "undo_depth", .handler = undoDepth },
    // 05-06 (D-32): what the renderer holds for a layer, and the fire ranges' areas.
    .{ .name = "layer", .handler = layerIs },
    .{ .name = "fire_areas", .handler = fireAreasAtLeast },
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
    .{ .name = "players", .handler = playersIs },
    .{ .name = "player_is", .handler = playerIs },
    .{ .name = "unit_creation_is", .handler = unitCreationIs },
    .{ .name = "check_findings", .handler = checkFindingsIs },
    .{ .name = "check_log_has", .handler = checkLogHas },
    .{ .name = "minimap_mode", .handler = minimapModeIs },
    .{ .name = "minimap_visible", .handler = minimapVisibleIs },
    .{ .name = "minimap_moved", .handler = minimapMovedIs },
    .{ .name = "minimap_files", .handler = minimapFilesExist },
    // 05-08: the dialog is up, the seed a generation reported, an export's file written.
    .{ .name = "rmg_dialog", .handler = rmgDialogIs },
    .{ .name = "rmg_seed", .handler = rmgSeedIs },
    .{ .name = "rmg_phase", .handler = rmgPhaseIs },
    .{ .name = "export_file", .handler = exportFileExists },
    .{ .name = "export_lines", .handler = exportLinesAtLeast },
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

// ---------------------------------------------------------------------------
// The Layers menu (M3, D-32): renderer state. Every change is remembered in
// the editor (re-applied after each open and new map) and carried into the
// settings for the next start. Nothing here is map data: no history, no dirt.
// ---------------------------------------------------------------------------

fn layerOutcome(state: *State, result: core.bridge.EditError!void) Outcome {
    const outcome = resultOutcome(state, result);
    if (outcome == .ok) {
        state.settings.layers = state.editor.layers;
        state.settings_changed = true;
    }
    return outcome;
}

/// `do=layer_toggle:<name>` - Layers > the named entry (`terrain`, `grid`,
/// `wireframe`, `depth_complexity`, `terrain_noise`, `black_stripes`, `units`,
/// `objects`, `bounding_boxes`, `shadows`, `haze`, `war_fog`,
/// `units_passability`). The fire ranges are `fire_range`, not a toggle.
pub fn layerToggle(state: *State, arg: []const u8) Outcome {
    const layer = core.layers.fromCommandName(arg) orelse return .bad_arg;
    if (!core.layers.isToggle(layer)) return .bad_arg;
    return layerOutcome(state, state.editor.toggleLayer(layer));
}

/// `do=layer_set:<name>:<0|1>` - the named layer to a state (what a script
/// wants when it does not know the state it is toggling from).
fn layerSet(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const layer = core.layers.fromCommandName(arg[0..colon]) orelse return .bad_arg;
    if (!core.layers.isToggle(layer)) return .bad_arg;
    const shown = parseZeroOne(arg[colon + 1 ..]) orelse return .bad_arg;
    return layerOutcome(state, state.editor.setLayer(layer, shown));
}

fn parseZeroOne(text: []const u8) ?bool {
    if (std.mem.eql(u8, text, "1")) return true;
    if (std.mem.eql(u8, text, "0")) return false;
    return null;
}

/// `do=fire_range:off`, `do=fire_range:selected` or
/// `do=fire_range:filter:<name>` - Layers > Unit Fire Ranges: none, the
/// selected units', or every unit a filter passes (underscores stand for the
/// spaces of a name, `Axis_Units`). An unknown filter name is refused and
/// changes nothing.
pub fn fireRange(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "off")) return layerOutcome(state, state.editor.setFireRange(.off, ""));
    if (std.mem.eql(u8, arg, "selected")) return layerOutcome(state, state.editor.setFireRange(.selected, ""));
    const prefix = "filter:";
    if (!std.mem.startsWith(u8, arg, prefix)) return .bad_arg;
    var buffer: [128]u8 = undefined;
    const name = logic.resolveFilterName(&buffer, state.editor.filtersSlice(), arg[prefix.len..]) orelse {
        state.view.setStatus("fire range: ", "no filter is named that");
        return .refused;
    };
    return fireRangeFilter(state, name);
}

/// The menu's own entry for a filter: the exact name, whatever characters it
/// holds (the script spelling `filter:Axis_Units` only exists because an auto
/// argument cannot hold a space).
pub fn fireRangeFilter(state: *State, name: []const u8) Outcome {
    return layerOutcome(state, state.editor.setFireRange(.filter, name));
}

/// `expect=layer:<name>:<0|1>` - what the renderer says for the layer, read
/// back from the bridge (not the editor's own memory): the proof a toggle, or
/// the re-apply after an open, reached the engine.
fn layerIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const layer = core.layers.fromCommandName(arg[0..colon]) orelse return .bad_arg;
    const shown = parseZeroOne(arg[colon + 1 ..]) orelse return .bad_arg;
    var bits: u32 = 0;
    var mask: u32 = 0;
    if (state.editor.bridge.layers(&bits, &mask) != .ok) return .refused;
    return if ((bits & core.layers.bit(layer) != 0) == shown) .ok else .refused;
}

/// `expect=fire_areas:<n>` - at least n shoot areas are shown (the minimap's
/// read of what the AI shows; line-shaped ranges are left out of it); `0`
/// means none are shown at all.
fn fireAreasAtLeast(state: *State, arg: []const u8) Outcome {
    const wanted = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    var none: [0]core.bridge.MinimapArea = .{};
    var total: usize = 0;
    const answer = state.editor.bridge.minimapAreas(&none, &total);
    if (answer != .ok and answer != .refused) return .refused;
    if (wanted == 0) return if (total == 0) .ok else .refused;
    return if (total >= wanted) .ok else .refused;
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


// ---------------------------------------------------------------------------
// Players and the Unit Creation Info (05-05, D-30)
// ---------------------------------------------------------------------------

/// `do=player_add[:side]`: a player of that side (0 or 1, default 0 - the
/// MFC's own insert) before the neutral entry; one undo step.
fn playerAdd(state: *State, arg: []const u8) Outcome {
    const side = if (arg.len == 0) 0 else std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.addPlayer(side));
}

/// `do=player_delete:<player>`: the player's objects become the neutral's, the
/// players above move down; one undo step. The neutral entry is refused.
fn playerDelete(state: *State, arg: []const u8) Outcome {
    const player = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    state.selected_player = null;
    return resultOutcome(state, state.editor.deletePlayer(player));
}

/// `do=player_side:<player>=<side>`: the MFC list's 0 and 1 keys.
fn playerSide(state: *State, arg: []const u8) Outcome {
    const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return .bad_arg;
    const player = std.fmt.parseInt(i32, arg[0..eq], 10) catch return .bad_arg;
    const side = std.fmt.parseInt(i32, arg[eq + 1 ..], 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    return resultOutcome(state, state.editor.setDiplomacy(player, side));
}

/// `do=unit_creation_window:1|0` opens or closes the Unit Creation Info window.
fn unitCreationWindow(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) {
        state.uc_open = true;
    } else if (std.mem.eql(u8, arg, "0")) {
        state.uc_open = false;
    } else return .bad_arg;
    return .ok;
}

/// `do=unit_creation_player:<n>`: the player the window shows and the fields
/// below edit (opens the window).
fn unitCreationPlayer(state: *State, arg: []const u8) Outcome {
    const player = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (player >= records.max_uc_slots) return .bad_arg;
    state.uc_player = player;
    state.uc_open = true;
    return .ok;
}

/// One field of a player's unit creation, as the commands name it.
const UcField = union(enum) {
    party,
    paratroop_name,
    paratroop_count,
    relax,
    aircraft_name: usize,
    aircraft_formation: usize,
    aircraft_count: usize,
};

/// `party`, `paratroop_name`, `paratroop_count`, `relax`, and
/// `aircraft<0..4>_name|formation|count`.
fn parseUcField(field: []const u8) ?UcField {
    if (std.mem.eql(u8, field, "party")) return .party;
    if (std.mem.eql(u8, field, "paratroop_name")) return .paratroop_name;
    if (std.mem.eql(u8, field, "paratroop_count")) return .paratroop_count;
    if (std.mem.eql(u8, field, "relax")) return .relax;
    const prefix = "aircraft";
    if (field.len > prefix.len + 2 and std.mem.startsWith(u8, field, prefix) and field[prefix.len + 1] == '_') {
        const slot = field[prefix.len] -% '0';
        if (slot >= records.uc_aircraft_slots) return null;
        const rest = field[prefix.len + 2 ..];
        if (std.mem.eql(u8, rest, "name")) return .{ .aircraft_name = slot };
        if (std.mem.eql(u8, rest, "formation")) return .{ .aircraft_formation = slot };
        if (std.mem.eql(u8, rest, "count")) return .{ .aircraft_count = slot };
    }
    return null;
}

/// Puts `value` into the field; null for a value that does not parse.
fn setUcField(unit: *records.UnitCreation, field: UcField, value: []const u8) ?void {
    switch (field) {
        .party => unit.setParty(value),
        .paratroop_name => unit.setParatroop(value),
        .paratroop_count => unit.paratroop_count = std.fmt.parseInt(i32, value, 10) catch return null,
        .relax => unit.relax_time = std.fmt.parseInt(i32, value, 10) catch return null,
        .aircraft_name => |slot| unit.aircraft[slot].setName(value),
        .aircraft_formation => |slot| unit.aircraft[slot].formation_size = std.fmt.parseInt(i32, value, 10) catch return null,
        .aircraft_count => |slot| unit.aircraft[slot].count = std.fmt.parseInt(i32, value, 10) catch return null,
    }
}

/// The field's value as text, for a predicate and the panel's own reading.
fn ucFieldText(buffer: []u8, unit: *const records.UnitCreation, field: UcField) []const u8 {
    return switch (field) {
        .party => unit.partySlice(),
        .paratroop_name => unit.paratroopSlice(),
        .paratroop_count => std.fmt.bufPrint(buffer, "{d}", .{unit.paratroop_count}) catch "",
        .relax => std.fmt.bufPrint(buffer, "{d}", .{unit.relax_time}) catch "",
        .aircraft_name => |slot| unit.aircraft[slot].nameSlice(),
        .aircraft_formation => |slot| std.fmt.bufPrint(buffer, "{d}", .{unit.aircraft[slot].formation_size}) catch "",
        .aircraft_count => |slot| std.fmt.bufPrint(buffer, "{d}", .{unit.aircraft[slot].count}) catch "",
    };
}

/// `do=unit_creation_set:<field>=<value>`: one field of the window's player,
/// one undo step; the bridge's MutableValidate-style rules answer, a refusal
/// names the field and changes nothing.
fn unitCreationSet(state: *State, arg: []const u8) Outcome {
    const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return .bad_arg;
    const field = parseUcField(arg[0..eq]) orelse return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    var unit = state.editor.unitCreation(state.uc_player) catch |err| return resultOutcome(state, err);
    setUcField(&unit, field, arg[eq + 1 ..]) orelse return .bad_arg;
    return resultOutcome(state, state.editor.editUnitCreation(state.uc_player, unit, 0));
}

/// `expect=unit_creation_is:<field>=<value>`: the window's player holds that
/// value (what the bridge reads, not the panel's cache).
fn unitCreationIs(state: *State, arg: []const u8) Outcome {
    const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return .bad_arg;
    const field = parseUcField(arg[0..eq]) orelse return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const unit = state.editor.unitCreation(state.uc_player) catch return .refused;
    var buffer: [32]u8 = undefined;
    const got = ucFieldText(&buffer, &unit, field);
    if (std.mem.eql(u8, got, arg[eq + 1 ..])) return .ok;
    var note: [128]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&note, "player {d}'s {s} is {s}, not {s}", .{ state.uc_player, arg[0..eq], got, arg[eq + 1 ..] }) catch "unit creation differs");
    return .refused;
}

/// An appear point in map (AI) units, the way the MFC rounds a point it is handed
/// (Vis2AI cuts int(v + 0.3)).
fn appearPoint(x: f32, y: f32) records.Vec3 {
    return .{ .x = records.truncateToAi(x), .y = records.truncateToAi(y) };
}

fn addAppear(state: *State, point: records.Vec3) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    var unit = state.editor.unitCreation(state.uc_player) catch |err| return resultOutcome(state, err);
    if (!unit.addAppear(point)) {
        state.editor.note("a player holds 32 appear points at most");
        return .refused;
    }
    return resultOutcome(state, state.editor.editUnitCreation(state.uc_player, unit, 0));
}

/// `do=appear_point_here`: an appear point at the ground under the centre of
/// the view, in map units; one undo step.
fn appearPointHere(state: *State, _: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const centre = viewCentre(state) orelse return .refused;
    return addAppear(state, appearPoint(centre.map_x, centre.map_y));
}

fn parseXY(arg: []const u8) ?[2]f32 {
    const slash = std.mem.indexOfScalar(u8, arg, '/') orelse return null;
    const x = std.fmt.parseFloat(f32, arg[0..slash]) catch return null;
    const y = std.fmt.parseFloat(f32, arg[slash + 1 ..]) catch return null;
    if (!std.math.isFinite(x) or !std.math.isFinite(y)) return null;
    return .{ x, y };
}

/// `do=appear_point_add:<x>/<y>`: an appear point at map (AI) units.
fn appearPointAdd(state: *State, arg: []const u8) Outcome {
    const xy = parseXY(arg) orelse return .bad_arg;
    return addAppear(state, .{ .x = xy[0], .y = xy[1] });
}

/// `do=appear_point_set:<index>/<x>/<y>`: moves one point (map units) - the
/// points list's own edit; one undo step.
fn appearPointSet(state: *State, arg: []const u8) Outcome {
    const slash = std.mem.indexOfScalar(u8, arg, '/') orelse return .bad_arg;
    const index = std.fmt.parseInt(usize, arg[0..slash], 10) catch return .bad_arg;
    const xy = parseXY(arg[slash + 1 ..]) orelse return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    var unit = state.editor.unitCreation(state.uc_player) catch |err| return resultOutcome(state, err);
    if (index >= unit.appear_count) return .bad_arg;
    unit.appear[index] = .{ .x = xy[0], .y = xy[1] };
    return resultOutcome(state, state.editor.editUnitCreation(state.uc_player, unit, 0));
}

/// `do=appear_point_remove:<index>`.
fn appearPointRemove(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    var unit = state.editor.unitCreation(state.uc_player) catch |err| return resultOutcome(state, err);
    if (!unit.removeAppear(index)) return .bad_arg;
    return resultOutcome(state, state.editor.editUnitCreation(state.uc_player, unit, 0));
}

/// `expect=players:<n>`: the diplomacy table holds n entries (the players and
/// the neutral).
fn playersIs(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const got = state.editor.document.diplomacy.items.len;
    if (got == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "the table holds {d} entries, not {d}", .{ got, want }) catch "player count differs");
    return .refused;
}

/// `expect=player_is:<object>:<player>`: the object (`@<n>`: the n-th selected,
/// or a link ID) belongs to that player.
fn playerIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const want = std.fmt.parseInt(i32, arg[colon + 1 ..], 10) catch return .bad_arg;
    const link_id = objectRef(state, arg[0..colon]) orelse return .bad_arg;
    const object = state.editor.document.find(link_id) orelse return .refused;
    if (object.player == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "object {d} belongs to player {d}, not {d}", .{ link_id, object.player, want }) catch "owner differs");
    return .refused;
}


// ---------------------------------------------------------------------------
// Check Map (05-05, D-33)
// ---------------------------------------------------------------------------

/// The type names of squads (game type 15), from the catalogue: the duplicate rule
/// leaves a squad alone. Freed by the caller (the names point into the catalogue).
fn squadNames(state: *State) ?[][]const u8 {
    var names: std.ArrayListUnmanaged([]const u8) = .empty;
    for (state.catalogue) |*entry| {
        if (entry.game_type == 15) names.append(state.allocator, std.mem.sliceTo(&entry.name, 0)) catch {
            names.deinit(state.allocator);
            return null;
        };
    }
    return names.toOwnedSlice(state.allocator) catch null;
}

/// `<user_root>mapeditor/logs/checkmap_log.txt` (D-33), an OS path.
fn checkLogPath(buffer: []u8, state: *const State) ?[]const u8 {
    const root = std.mem.sliceTo(&state.paths.user_root, 0);
    return std.fmt.bufPrint(buffer, "{s}mapeditor{c}logs{c}checkmap_log.txt", .{ root, std.fs.path.sep, std.fs.path.sep }) catch null;
}

/// Writes the findings to the log, MFC layout; a failure is a status note, never
/// a failed check.
fn writeCheckLog(state: *State) void {
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = checkLogPath(&path_buffer, state) orelse return;
    var out = std.Io.Writer.Allocating.init(state.allocator);
    defer out.deinit();
    core.checks.writeLog(&out.writer, state.check_findings) catch return;
    if (std.fs.path.dirname(path)) |dir| std.Io.Dir.cwd().createDirPath(state.io, dir) catch {
        state.view.setStatus("check map: ", "the log folder could not be made");
        return;
    };
    std.Io.Dir.cwd().writeFile(state.io, .{ .sub_path = path, .data = out.written() }) catch {
        state.view.setStatus("check map: ", "the log could not be written");
    };
}

/// Runs the checks and keeps their findings for the window (the old ones go).
/// `write_log` is Check Map's own run; the quiet one after a save leaves the log alone.
fn runChecks(state: *State) bool {
    const names = squadNames(state) orelse return false;
    defer state.allocator.free(names);
    const found = state.editor.checkMap(state.allocator, names) catch return false;
    state.allocator.free(state.check_findings);
    state.check_findings = found;
    return true;
}

/// `do=check_map`: runs every check, lists the findings in the Check Map window
/// and writes `<UserRoot>mapeditor/logs/checkmap_log.txt` - a run edits nothing.
fn checkMapCommand(state: *State, _: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (!runChecks(state)) {
        state.view.setStatus("check map: ", "the checks could not run");
        return .refused;
    }
    state.check_open = true;
    state.check_fix_report = null;
    writeCheckLog(state);
    var buffer: [96]u8 = undefined;
    const count = state.check_findings.len;
    state.view.setStatus("check map: ", if (count == 0) "no problems found" else std.fmt.bufPrint(&buffer, "{d} finding(s)", .{count}) catch "findings");
    return .ok;
}

/// After a Save or Save As landed: the checks run and the status bar only SAYS when
/// they find something - Save never fixes silently, unlike the MFC's CheckMap(false)
/// (TemplateEditorFrame1.cpp:2934; PARITY S1). The log is the Check Map command's.
pub fn noteChecksAfterSave(state: *State) void {
    if (!panels.mapIsOpen(state.editor)) return;
    if (!runChecks(state)) return;
    if (state.check_findings.len == 0) return;
    var buffer: [112]u8 = undefined;
    state.view.setStatus("checks failed: ", std.fmt.bufPrint(&buffer, "{d} finding(s) - Map > Check Map lists them; the save is as it was", .{state.check_findings.len}) catch "findings - Map > Check Map");
}

/// `do=check_map_fix_all[:remove]`: Fix all as ONE undo step. Without `remove` the
/// fixes that take something away (an unknown-type object, a short road or river)
/// wait; with it they go too - the window's confirmation is this argument. The
/// checks run again afterwards and the window shows what is left.
fn checkMapFixAll(state: *State, arg: []const u8) Outcome {
    const remove = std.mem.eql(u8, arg, "remove");
    if (arg.len != 0 and !remove) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    if (state.check_findings.len == 0 and !runChecks(state)) return .refused;
    const report = state.editor.fixAll(state.check_findings, remove) catch |err| return resultOutcome(state, err);
    state.check_fix_report = report;
    state.check_confirm_pending = false;
    _ = runChecks(state);
    writeCheckLog(state);
    var buffer: [112]u8 = undefined;
    state.view.setStatus("check map: ", std.fmt.bufPrint(&buffer, "fixed {d}, left {d}, refused {d}", .{ report.fixed, report.left, report.refused }) catch "fixed");
    return .ok;
}

/// `do=check_window:1|0` opens or closes the Check Map window.
fn checkWindow(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) {
        state.check_open = true;
    } else if (std.mem.eql(u8, arg, "0")) {
        state.check_open = false;
    } else return .bad_arg;
    return .ok;
}

/// `do=check_jump:<n>`: the n-th finding's place comes to the middle of the view and
/// its object is selected (the selection circle is the marker): an object's own place
/// is in map units, a road's first control point in world units.
pub fn jumpToFinding(state: *State, index: usize) Outcome {
    if (index >= state.check_findings.len) return .bad_arg;
    if (!panels.mapIsOpen(state.editor)) return .refused;
    const finding = state.check_findings[index];
    if (finding.kind == .unknown_party) {
        state.uc_player = @intCast(@max(finding.player, 0));
        state.uc_open = true;
        return .ok;
    }
    if (finding.kind == .short_vso and !finding.world) {
        state.view.setStatus("check map: ", "that road has no control point to go to");
        return .refused;
    }
    const world = if (finding.world) marker_logic.Vec2{ .x = finding.x, .y = finding.y } else marker_logic.aiToWorld(.{ .x = finding.x, .y = finding.y });
    state.view.centreOn(state.real, world.x, world.y);
    if (finding.kind != .short_vso and state.editor.document.find(finding.link_id) != null) state.editor.selectOnly(finding.link_id);
    return .ok;
}

fn checkJump(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return jumpToFinding(state, index);
}

/// `expect=check_findings:<n>` - or `<kind>=<n>`, one kind's count (the Kind's own
/// name: duplicate_object, invalid_link, duplicate_link, player_index,
/// unknown_party, unknown_object_type, short_vso) - in the last run.
fn checkFindingsIs(state: *State, arg: []const u8) Outcome {
    if (std.mem.indexOfScalar(u8, arg, '=')) |eq| {
        const kind = std.meta.stringToEnum(core.checks.Kind, arg[0..eq]) orelse return .bad_arg;
        const want = std.fmt.parseInt(usize, arg[eq + 1 ..], 10) catch return .bad_arg;
        const got = core.checks.count(state.check_findings, kind);
        if (got == want) return .ok;
        var buffer: [96]u8 = undefined;
        state.editor.note(std.fmt.bufPrint(&buffer, "{d} {s} finding(s), not {d}", .{ got, arg[0..eq], want }) catch "finding count differs");
        return .refused;
    }
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (state.check_findings.len == want) return .ok;
    var buffer: [96]u8 = undefined;
    state.editor.note(std.fmt.bufPrint(&buffer, "{d} finding(s), not {d}", .{ state.check_findings.len, want }) catch "finding count differs");
    return .refused;
}

/// `expect=check_log_has:<text>`: `checkmap_log.txt` exists and holds the text.
fn checkLogHas(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = checkLogPath(&path_buffer, state) orelse return .refused;
    const bytes = std.Io.Dir.cwd().readFileAlloc(state.io, path, state.allocator, .limited(1024 * 1024)) catch {
        state.editor.note("checkmap_log.txt is not there");
        return .refused;
    };
    defer state.allocator.free(bytes);
    if (std.mem.indexOf(u8, bytes, arg) != null) return .ok;
    state.editor.note("checkmap_log.txt does not hold that text");
    return .refused;
}


/// `do=undo` / `do=redo`: Edit > Undo / Redo as a named command, for a scenario whose
/// keyboard a modal (the unknown-objects prompt a map's open raises) is holding.
fn undoCommand(state: *State, _: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor) or !state.editor.history.canUndo()) return .refused;
    state.view.undo(state.editor);
    return .ok;
}

fn redoCommand(state: *State, _: []const u8) Outcome {
    if (!panels.mapIsOpen(state.editor) or !state.editor.history.canRedo()) return .refused;
    state.view.redo(state.editor);
    return .ok;
}

// ---------------------------------------------------------------------------
// The Minimap (05-07, D-14..D-17).
// ---------------------------------------------------------------------------

/// `do=minimap_toggle[:on|off]` - View > Minimap: the panel shown or hidden (no
/// argument flips it). A window state, never an edit.
fn minimapToggle(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) {
        state.minimap.visible = !state.minimap.visible;
    } else if (std.mem.eql(u8, arg, "on")) {
        state.minimap.visible = true;
    } else if (std.mem.eql(u8, arg, "off")) {
        state.minimap.visible = false;
    } else return .bad_arg;
    return .ok;
}

/// `do=minimap_mode:<editor|game>` - the panel's Editor and Game buttons. Game
/// is refused, saying how to get one, for a map with no picture of its own.
fn minimapMode(state: *State, arg: []const u8) Outcome {
    const mode = logic.MinimapMode.fromName(arg) orelse return .bad_arg;
    if (!panels.documentLoaded(state.editor)) return .refused;
    return if (minimap.setMode(state, mode)) .ok else .refused;
}

/// `do=minimap_click:<x>x<y>` - a click on the panel, as percents of the
/// picture's width and height from its top-left (0..100): the camera moves the
/// way the MFC's minimap click moved it. Refused while the panel is not shown
/// (there is no picture to click on).
fn minimapClick(state: *State, arg: []const u8) Outcome {
    const at = logic.parseMinimapClick(arg) orelse return .bad_arg;
    if (!minimap.clickPercent(state, at[0], at[1])) {
        state.view.setStatus("minimap: ", "the minimap is not shown");
        return .refused;
    }
    return .ok;
}

/// `do=minimap_create` - Map > Create Minimap Images (D-17): the four pictures
/// beside the saved map. Never part of Save. A shipped or never-saved map goes
/// through Save As first, and one with unsaved changes through Save (the MFC
/// saved first too, TemplateEditorFrame1.cpp:300): the pictures are made once
/// the save lands, so `ok` then means queued. Afterwards the panel shows them.
pub fn minimapCreate(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    if (!panels.documentLoaded(state.editor)) {
        state.view.setStatus("minimap: ", "no map is open");
        return .refused;
    }
    if (panels.documentNeedsSaveAs(state) or state.editor.dirty()) {
        state.minimap.create_pending = true;
        state.actions.save_requested = true;
        state.view.setStatus("minimap: ", "saving the map first");
        return .ok;
    }
    return if (minimap.createNow(state)) .ok else .refused;
}

fn minimapModeIs(state: *State, arg: []const u8) Outcome {
    const want = logic.MinimapMode.fromName(arg) orelse return .bad_arg;
    return if (state.minimap.mode == want) .ok else .refused;
}

fn minimapVisibleIs(state: *State, arg: []const u8) Outcome {
    if (arg.len != 1 or (arg[0] != '0' and arg[0] != '1')) return .bad_arg;
    return if (state.minimap.visible == (arg[0] == '1')) .ok else .refused;
}

/// `expect=minimap_moved[:0|1]` - the last minimap click did (1, the default) or
/// did not (0) move the camera.
fn minimapMovedIs(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0 and (arg.len != 1 or (arg[0] != '0' and arg[0] != '1'))) return .bad_arg;
    const want = arg.len == 0 or arg[0] == '1';
    return if (state.minimap.last_click_moved == want) .ok else .refused;
}

/// `expect=minimap_files` - Create Minimap Images' eight files (the four pictures,
/// the DDS ones as the engine's `_c`/`_l`/`_h` trio) are beside the document's map.
fn minimapFilesExist(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const files = state.editor.files orelse return .refused;
    var os_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = core.files.osPathFromEngine(&os_buffer, state.editor.document.path.items) orelse return .refused;
    const base = logic.minimapImageBase(path) orelse return .refused;
    const suffixes = [_][]const u8{ "_large.tga", "_large_c.dds", "_large_l.dds", "_large_h.dds", ".tga", "_c.dds", "_l.dds", "_h.dds" };
    var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    for (suffixes) |suffix| {
        const name = std.fmt.bufPrint(&buffer, "{s}{s}", .{ base, suffix }) catch return .refused;
        if (!files.exists(name)) return .refused;
    }
    return .ok;
}


// ---------------------------------------------------------------------------
// Create Random Map (05-08, D-01..D-05) and Tools > Export lists (D-13)
// ---------------------------------------------------------------------------

/// Create Random Map's generation, shared by the dialog's modal and
/// `rmg_generate`: the bridge's CreateRandomMap with the progress counter
/// wired in (the callback only counts - it never re-enters the bridge), run
/// to completion on the calling thread (D-03). On success the result, the name
/// and the status line's words are kept; a refusal's reason is the status
/// line's. Does not open the map - the dialog's Open map button and
/// `rmg_generate` do, through the normal open path (D-02).
pub fn rmgRun(state: *State, params: core.bridge.RmgGenerateParams) Outcome {
    state.rmg_progress = .{};
    var run_params = params;
    run_params.progress = panels.RmgProgress.report;
    run_params.user = &state.rmg_progress;
    state.editor.createRandomMap(run_params, &state.rmg_result) catch |err| {
        state.view.noteEditResult(state.editor, err);
        return .refused;
    };
    state.rmg_made = true;
    state.rmg_made_name.set(params.mapNameSlice());
    var line: [256]u8 = undefined;
    state.view.setStatus("", logic.rmgResultLine(&line, state.rmg_made_name.slice(), &state.rmg_result));
    return .ok;
}

/// `do=rmg_dialog[:open|close|ok|open_map]` - File > Create Random Map (D-01):
/// opens the dialog with the fields as last left, closes it, presses its OK, or
/// presses the result modal's Open map.
fn rmgDialogCommand(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or std.mem.eql(u8, arg, "open")) {
        if (state.rmg_phase != .idle) return .refused;
        panels.openRmgDialog(state);
        return .ok;
    }
    if (std.mem.eql(u8, arg, "close")) {
        state.rmg_open = false;
        return .ok;
    }
    // The dialog's own OK, and the result modal's Open map: the progress modal's
    // frames run as a person would see them (announced at 0 of 19, one frame later
    // the generator runs, then the result), where `rmg_generate` is the one-step form.
    if (std.mem.eql(u8, arg, "ok")) return if (panels.startRmgGeneration(state)) .ok else .refused;
    if (std.mem.eql(u8, arg, "open_map")) {
        if (state.rmg_phase != .done) return .refused;
        panels.openGeneratedRmgMap(state);
        return .ok;
    }
    return .bad_arg;
}

/// `do=rmg_set:<field>:<value>` - one of the dialog's fields, set the way the
/// dialog sets it (`logic.RmgFields.set` lists them: template, context,
/// setting, graph, angle, level, bzm, dds, overwrite, name, seed). The auto
/// grammar allows 64 characters an argument, which ten fields on one line
/// would not fit - and a script then drives exactly the fields a person does.
fn rmgSetCommand(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    if (!state.rmg_fields.set(arg[0..colon], arg[colon + 1 ..])) return .bad_arg;
    panels.syncRmgEdits(state);
    if (std.mem.eql(u8, arg[0..colon], "template")) panels.refreshRmgGraphCount(state);
    return .ok;
}

/// `do=rmg_generate` - the dialog's OK on the fields as they stand: the
/// generation runs synchronously (the window waits, D-03), the result is kept,
/// and the map opens as a normal document through the open path (D-02) - so
/// `ok` means generated and queued; the title shows the map once the open
/// ran, and `expect=rmg_seed:N` the seed the generation reported. Refused with
/// the reason in the status line when the fields are not enough (the MFC's own
/// OK rule), the bridge refuses a field (the status line names it), or the map
/// of that name exists and `overwrite` is not set.
fn rmgGenerateCommand(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    if (state.rmg_phase != .idle) return .refused;
    if (!state.rmg_fields.okEnabled()) {
        state.view.setStatus("random map: ", "a template, a context and a map name are needed");
        return .refused;
    }
    const params = state.rmg_fields.toParams() orelse {
        state.view.setStatus("random map: ", "the seed is a whole number, or blank for a fresh one");
        return .refused;
    };
    state.rmg_open = false;
    if (rmgRun(state, params) != .ok) return .refused;
    state.actions.requestOpenPath(state.rmg_result.mapPathSlice());
    return .ok;
}

/// `expect=rmg_dialog:1|0` - whether the Create Random Map dialog is up.
fn rmgDialogIs(state: *State, arg: []const u8) Outcome {
    const want = parseFlagArg(arg) orelse return .bad_arg;
    return if (state.rmg_open == want) .ok else .refused;
}

/// `expect=rmg_phase:<idle|announce|run|done>` - where the progress modal is.
fn rmgPhaseIs(state: *State, arg: []const u8) Outcome {
    const want = std.meta.stringToEnum(panels.RmgPhase, arg) orelse return .bad_arg;
    return if (state.rmg_phase == want) .ok else .refused;
}

/// `expect=rmg_seed:N` - the last generation of this run reported seed N (the
/// seed-shown predicate: the modal and the status line say the same number).
fn rmgSeedIs(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(u32, arg, 10) catch return .bad_arg;
    return if (state.rmg_made and state.rmg_result.seed == want) .ok else .refused;
}

fn parseFlagArg(arg: []const u8) ?bool {
    if (std.mem.eql(u8, arg, "1")) return true;
    if (std.mem.eql(u8, arg, "0")) return false;
    return null;
}

/// One of the storage listings the Export lists walk, appended to `list`
/// (the bridge's two-pass read; the sizing pass is refused with the total).
fn readStorageFiles(state: *State, folder: [:0]const u8, extension: [:0]const u8, list: *std.ArrayListUnmanaged(core.bridge.RmgName)) bool {
    var total: usize = 0;
    _ = state.editor.bridge.listStorageFiles(folder.ptr, extension.ptr, &.{}, &total);
    if (total == 0) return true;
    const names = state.allocator.alloc(core.bridge.RmgName, total) catch return false;
    defer state.allocator.free(names);
    var read_total: usize = 0;
    if (state.editor.bridge.listStorageFiles(folder.ptr, extension.ptr, names, &read_total) != .ok) return false;
    list.appendSlice(state.allocator, names[0..@min(read_total, names.len)]) catch return false;
    return true;
}

/// `<user_root>mapeditor/logs/<file>` (D-13), an OS path.
fn exportPath(buffer: []u8, state: *const State, kind: logic.ExportKind) ?[]const u8 {
    const root = std.mem.sliceTo(&state.paths.user_root, 0);
    return std.fmt.bufPrint(buffer, "{s}mapeditor{c}logs{c}{s}", .{ root, std.fs.path.sep, std.fs.path.sep, kind.fileName() }) catch null;
}

/// Writes one list (D-13): the MFC's Tools 0..3 line formats lifted whole
/// (`logic.writeNameLines`/`writeGraphsList`), to the user's logs folder - the
/// MFC wrote into Data\logs, which the editor never touches. The folder is made on
/// demand; the status bar names the file.
fn exportList(state: *State, kind: logic.ExportKind) Outcome {
    var names: std.ArrayListUnmanaged(core.bridge.RmgName) = .empty;
    defer names.deinit(state.allocator);
    for (kind.extensions()) |extension| {
        if (!readStorageFiles(state, kind.folder(), extension, &names)) {
            state.view.setStatus("export: ", "the list could not be read from the data");
            return .refused;
        }
    }
    var out = std.Io.Writer.Allocating.init(state.allocator);
    defer out.deinit();
    if (kind == .graphs) {
        for (names.items) |*entry| {
            const file = entry.nameSlice();
            if (file.len <= ".xml".len) continue;
            // The template's own name, as ListRmg lists it: the file less ".xml".
            var template_buffer: [core.bridge.field_set_name_capacity:0]u8 = undefined;
            const template = std.fmt.bufPrintZ(&template_buffer, "{s}", .{file[0 .. file.len - ".xml".len]}) catch continue;
            var graph_total: usize = 0;
            _ = state.editor.bridge.rmgTemplateGraphs(template.ptr, &.{}, &graph_total);
            const graphs = state.allocator.alloc(core.bridge.RmgGraph, graph_total) catch return .refused;
            defer state.allocator.free(graphs);
            var graph_read: usize = 0;
            if (graph_total != 0 and state.editor.bridge.rmgTemplateGraphs(template.ptr, graphs, &graph_read) != .ok) {
                state.view.setStatus("export: ", "a template's graphs could not be read");
                return .refused;
            }
            const entries = [_]logic.TemplateGraphs{.{ .name = file, .graphs = graphs[0..@min(graph_read, graphs.len)] }};
            logic.writeGraphsList(&out.writer, &entries) catch return .refused;
        }
    } else {
        const slices = state.allocator.alloc([]const u8, names.items.len) catch return .refused;
        defer state.allocator.free(slices);
        for (names.items, 0..) |*entry, i| slices[i] = entry.nameSlice();
        const skipped: []const []const u8 = if (kind == .maps) &logic.maps_list_skipped else &.{};
        logic.writeNameLines(&out.writer, slices, skipped) catch return .refused;
    }
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = exportPath(&path_buffer, state, kind) orelse return .refused;
    if (std.fs.path.dirname(path)) |dir| std.Io.Dir.cwd().createDirPath(state.io, dir) catch {
        state.view.setStatus("export: ", "the logs folder could not be made");
        return .refused;
    };
    std.Io.Dir.cwd().writeFile(state.io, .{ .sub_path = path, .data = out.written() }) catch {
        state.view.setStatus("export: ", "the list could not be written");
        return .refused;
    };
    var message: [std.Io.Dir.max_path_bytes + 48]u8 = undefined;
    state.view.setStatus("", std.fmt.bufPrint(&message, "{s} created: {s}", .{ kind.noun(), path }) catch "list created");
    return .ok;
}

/// `do=export_lists:<graphs|contexts|patches|maps>` - Tools > Export lists
/// (D-13), each to `<UserRoot>mapeditor/logs/<kind>_list.txt`. ID_TOOL_4 is not
/// a feature (PARITY T7): it would rewrite Data.
fn exportListsCommand(state: *State, arg: []const u8) Outcome {
    const kind = logic.ExportKind.fromName(arg) orelse return .bad_arg;
    return exportList(state, kind);
}

/// `expect=export_file:<graphs|contexts|patches|maps>` - the list's file is in
/// the user's logs folder.
fn exportFileExists(state: *State, arg: []const u8) Outcome {
    const kind = logic.ExportKind.fromName(arg) orelse return .bad_arg;
    const files = state.editor.files orelse return .refused;
    var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = exportPath(&buffer, state, kind) orelse return .refused;
    return if (files.exists(path)) .ok else .refused;
}

/// `expect=export_lines:<kind>:<N>` - the list file holds at least N lines
/// (a graphs list: its templates and their graphs together).
fn exportLinesAtLeast(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const kind = logic.ExportKind.fromName(arg[0..colon]) orelse return .bad_arg;
    const want = std.fmt.parseInt(usize, arg[colon + 1 ..], 10) catch return .bad_arg;
    var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = exportPath(&buffer, state, kind) orelse return .refused;
    const bytes = std.Io.Dir.cwd().readFileAlloc(state.io, path, state.allocator, .limited(16 << 20)) catch return .refused;
    defer state.allocator.free(bytes);
    var lines: usize = 0;
    for (bytes) |c| {
        if (c == '\n') lines += 1;
    }
    return if (lines >= want) .ok else .refused;
}

// ---------------------------------------------------------------------------
// The RMG composers (05-09, D-06..D-12): core.composers behind the two windows.
// Names are relative to the kind's folder (see core/composers.zig); a refusal
// puts its words on the status line.
// ---------------------------------------------------------------------------

fn composerResult(state: *State, result: anytype) Outcome {
    _ = result catch {
        const said = state.composers.message();
        state.view.setStatus("composer: ", if (said.len != 0) said else state.editor.status());
        return .refused;
    };
    return .ok;
}

fn composerSave(state: *State, saved: anytype) Outcome {
    const result = saved catch return .refused;
    switch (result) {
        .saved => return .ok,
        .needs_save_as, .failed => {
            state.view.setStatus("composer: ", state.composers.message());
            return .refused;
        },
    }
}

fn rmgcWindow(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.containers_open = !state.containers_open;
    if (state.containers_open) state.composers.ensureScanned(state.editor);
    return .ok;
}

fn rmgcNew(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerResult(state, state.composers.newContainer());
}

fn rmgcOpen(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    @memset(&state.cc_selected, false);
    return composerResult(state, state.composers.openContainer(state.editor, arg));
}

fn rmgcSave(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerSave(state, state.composers.saveContainer(state.editor));
}

fn rmgcSaveAs(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    return composerSave(state, state.composers.saveContainerAs(state.editor, arg));
}

/// A patch map of the storages, by name (relative to scenarios\patches\ unless it
/// starts at scenarios\); the MFC's checks run and a patch that does not belong is
/// refused naming why.
fn rmgcPatchAdd(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    var names = [_][]const u8{arg};
    const added = state.composers.addPatches(state.editor, &names) catch return composerResult(state, @as(error{Failed}!void, error.Failed));
    if (added == 1) return .ok;
    state.view.setStatus("composer: ", state.composers.message());
    return .refused;
}

fn rmgcPatchDel(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (index >= state.composers.cdoc.current.patchCount()) return .refused;
    return composerResult(state, state.composers.deletePatches(&.{index}));
}

const flag_names = [_][]const u8{ "north", "east", "south", "west" };

/// `<index>:<field>:<value>` - field place (a setting name, or `any`) or one of
/// north/east/south/west (0 or 1): one patch's properties, one undo step.
fn rmgcPatchSet(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const field = parts.next() orelse return .bad_arg;
    const value = parts.rest();
    if (index >= state.composers.cdoc.current.patchCount()) return .refused;
    var flags = [4]core.composers.Tri{ .keep, .keep, .keep, .keep };
    var place: ?[]const u8 = null;
    if (std.mem.eql(u8, field, "place")) {
        place = if (std.mem.eql(u8, value, "any")) "" else value;
    } else for (flag_names, 0..) |name, d| {
        if (!std.mem.eql(u8, field, name)) continue;
        if (std.mem.eql(u8, value, "1")) flags[d] = .on else if (std.mem.eql(u8, value, "0")) flags[d] = .off else return .bad_arg;
        break;
    } else return .bad_arg;
    return composerResult(state, state.composers.setPatchProperties(&.{index}, place, flags));
}

/// D-10: a map of the user's maps folder (`<UserRoot>maps/<name>.bzm`, or the
/// mod's) is offered a copy into the RMG root - the same path the Browse
/// button takes for a file outside Data. The window then asks; `rmgc_import_yes`
/// and `rmgc_import_no` answer.
fn rmgcImport(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or std.mem.indexOfAny(u8, arg, "/\\:") != null) return .bad_arg;
    var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const user = panels.userRoot(state);
    for ([_][]const u8{ ".bzm", ".xml" }) |extension| {
        const path = if (state.modFolder()) |folder|
            std.fmt.bufPrint(&buffer, "{s}mods{c}{s}{c}maps{c}{s}{s}", .{ user, std.fs.path.sep, folder, std.fs.path.sep, std.fs.path.sep, arg, extension }) catch return .refused
        else
            std.fmt.bufPrint(&buffer, "{s}maps{c}{s}{s}", .{ user, std.fs.path.sep, arg, extension }) catch return .refused;
        std.Io.Dir.cwd().access(state.io, path, .{}) catch continue;
        const result = state.composers.beginImport(state.editor, path);
        if (result) |_| {
            state.cc_popup = .import_copy;
            state.containers_open = true;
            return .ok;
        } else |_| return composerResult(state, @as(error{Failed}!void, error.Failed));
    }
    state.view.setStatus("composer: ", "no such map in the user's maps folder");
    return .refused;
}

fn rmgcImportYes(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.cc_popup = .none;
    const added = state.composers.confirmImport(state.editor) catch return composerResult(state, @as(error{Failed}!void, error.Failed));
    return if (added == 1) .ok else .refused;
}

fn rmgcImportNo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.cc_popup = .none;
    state.composers.cancelImport();
    return .ok;
}

fn rmgcCheck(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerResult(state, state.composers.checkContainer(state.editor));
}

fn rmgcFix(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return composerResult(state, state.composers.fixContainerFinding(state.editor, index));
}

fn rmgcFixAll(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerResult(state, state.composers.fixContainerAll(state.editor));
}

fn rmgcUndo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.undoContainer() catch return .refused;
    return if (done) .ok else .refused;
}

fn rmgcRedo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.redoContainer() catch return .refused;
    return if (done) .ok else .refused;
}

fn rmggWindow(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.graphs_open = !state.graphs_open;
    if (state.graphs_open) state.composers.ensureScanned(state.editor);
    return .ok;
}

fn rmggNew(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerResult(state, state.composers.newGraph());
}

fn rmggOpen(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    return composerResult(state, state.composers.openGraph(state.editor, arg));
}

fn rmggSave(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerSave(state, state.composers.saveGraph(state.editor));
}

fn rmggSaveAs(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    return composerSave(state, state.composers.saveGraphAs(state.editor, arg));
}

fn rmggZoom(state: *State, arg: []const u8) Outcome {
    const patches = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    if (patches < core.rmg.min_zoom or patches > core.rmg.max_zoom) return .bad_arg;
    state.composers.canvas.setPatches(patches);
    return .ok;
}

fn parseDrag(arg: []const u8) ?[4]i32 {
    var parts = std.mem.splitScalar(u8, arg, ':');
    var out: [4]i32 = undefined;
    for (&out) |*slot| slot.* = std.fmt.parseInt(i32, parts.next() orelse return null, 10) catch return null;
    if (parts.next() != null) return null;
    return out;
}

/// `<x1>:<y1>:<x2>:<y2>` in tile coordinates (y up): press, drag and release on
/// the canvas - the same state machine the mouse drives. Refused when the
/// gesture changed nothing (a rejected add, a reverted overlap).
fn canvasDrag(state: *State, arg: []const u8, ctrl: bool) Outcome {
    const p = parseDrag(arg) orelse return .bad_arg;
    const outcome = state.composers.gesture(.{ .x = p[0], .y = p[1] }, .{ .x = p[2], .y = p[3] }, ctrl, 1.0) catch return .refused;
    return switch (outcome) {
        .node_added, .node_moved, .node_resized, .link_added => .ok,
        else => blk: {
            state.view.setStatus("composer: ", state.composers.message());
            break :blk .refused;
        },
    };
}

fn rmggDrag(state: *State, arg: []const u8) Outcome {
    return canvasDrag(state, arg, false);
}

fn rmggCtrlDrag(state: *State, arg: []const u8) Outcome {
    return canvasDrag(state, arg, true);
}

/// `<node>:<container>` (`-` empties the node): the node properties' OK.
fn rmggNode(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const index = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    const name = arg[colon + 1 ..];
    if (name.len == 0) return .bad_arg;
    const result = state.composers.setNodeContainer(state.editor, index, if (std.mem.eql(u8, name, "-")) "" else name);
    if (result) |_| return .ok else |_| {
        state.view.setStatus("composer: ", state.composers.message());
        return .refused;
    }
}

fn rmggNodeDel(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (index >= state.composers.gdoc.current.nodes.items.len) return .refused;
    return composerResult(state, state.composers.deleteNode(index));
}

/// `<link>:<field>:<value>`: kind (0 road, 1 river), desc, radius and min_length
/// in cells, parts, distance, disturbance - the link properties' own edits.
fn rmggLink(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const field = core.composers.LinkField.fromName(parts.next() orelse return .bad_arg) orelse return .bad_arg;
    const value = parts.rest();
    if (value.len == 0 and field != .desc) return .bad_arg;
    return composerResult(state, state.composers.setLinkField(index, field, value));
}

fn rmggLinkDel(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (index >= state.composers.gdoc.current.links.items.len) return .refused;
    return composerResult(state, state.composers.deleteLink(index));
}

fn rmggCheck(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerResult(state, state.composers.checkGraph(state.editor));
}

fn rmggFix(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return composerResult(state, state.composers.fixGraphFinding(state.editor, index));
}

fn rmggFixAll(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerResult(state, state.composers.fixGraphAll(state.editor));
}

fn rmggUndo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.undoGraph() catch return .refused;
    return if (done) .ok else .refused;
}

fn rmggRedo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.redoGraph() catch return .refused;
    return if (done) .ok else .refused;
}

fn countIs(have: usize, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    return if (have == want) .ok else .refused;
}

fn flagIs(have: bool, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "1")) return if (have) .ok else .refused;
    if (std.mem.eql(u8, arg, "0")) return if (!have) .ok else .refused;
    return .bad_arg;
}

fn rmgcPatchesAre(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.cdoc.current.patchCount(), arg);
}
fn rmgcDirtyIs(state: *State, arg: []const u8) Outcome {
    return flagIs(state.composers.cdoc.dirty, arg);
}
fn rmgcNameEndsWith(state: *State, arg: []const u8) Outcome {
    return if (std.mem.endsWith(u8, state.composers.cdoc.name, arg)) .ok else .refused;
}
fn rmgcFindingsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.container_report orelse return .refused;
    return countIs(report.findings.items.len, arg);
}
fn rmgcErrorsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.container_report orelse return .refused;
    return countIs(report.errorCount(), arg);
}

/// `<patch>:<direction>:<0|1>` - north, east, south or west.
fn rmgcCellIs(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const field = parts.next() orelse return .bad_arg;
    const want = parts.rest();
    if (index >= state.composers.cdoc.current.patchCount()) return .refused;
    for (flag_names, 0..) |name, d| {
        if (std.mem.eql(u8, field, name)) return flagIs(state.composers.cdoc.current.hasDirection(index, @enumFromInt(d)), want);
    }
    return .bad_arg;
}

/// `<patch>:<setting>` (`any` is the empty setting).
fn rmgcPlaceIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const index = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    if (index >= state.composers.cdoc.current.patchCount()) return .refused;
    const want = if (std.mem.eql(u8, arg[colon + 1 ..], "any")) "" else arg[colon + 1 ..];
    return if (std.mem.eql(u8, state.composers.cdoc.current.patches.items[index].place, want)) .ok else .refused;
}

/// `<x>x<y>`: the container's size in patches.
fn rmgcSizeIs(state: *State, arg: []const u8) Outcome {
    const x = std.mem.indexOfScalar(u8, arg, 'x') orelse return .bad_arg;
    const want_x = std.fmt.parseInt(i32, arg[0..x], 10) catch return .bad_arg;
    const want_y = std.fmt.parseInt(i32, arg[x + 1 ..], 10) catch return .bad_arg;
    const c = &state.composers.cdoc.current;
    return if (c.size_x == want_x and c.size_y == want_y) .ok else .refused;
}

fn rmgcPendingIs(state: *State, arg: []const u8) Outcome {
    return flagIs(state.composers.pending_import.active, arg);
}

fn rmgcListedAtLeast(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.composers.ensureScanned(state.editor);
    return if (state.composers.container_names.items.len >= want) .ok else .refused;
}

fn rmggNodesAre(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.gdoc.current.nodes.items.len, arg);
}
fn rmggLinksAre(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.gdoc.current.links.items.len, arg);
}
fn rmggDirtyIs(state: *State, arg: []const u8) Outcome {
    return flagIs(state.composers.gdoc.dirty, arg);
}
fn rmggNameEndsWith(state: *State, arg: []const u8) Outcome {
    return if (std.mem.endsWith(u8, state.composers.gdoc.name, arg)) .ok else .refused;
}
fn rmggFindingsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.graph_report orelse return .refused;
    return countIs(report.findings.items.len, arg);
}
fn rmggErrorsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.graph_report orelse return .refused;
    return countIs(report.errorCount(), arg);
}

/// `<node>:<name>` - the node's container ends with the name (`-`: empty).
fn rmggNodeContainerIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const index = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    if (index >= state.composers.gdoc.current.nodes.items.len) return .refused;
    const have = state.composers.gdoc.current.nodes.items[index].container;
    const want = arg[colon + 1 ..];
    if (std.mem.eql(u8, want, "-")) return if (have.len == 0) .ok else .refused;
    return if (std.mem.endsWith(u8, have, want)) .ok else .refused;
}

fn rmggLinkPartsAre(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const index = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    const want = std.fmt.parseInt(i32, arg[colon + 1 ..], 10) catch return .bad_arg;
    if (index >= state.composers.gdoc.current.links.items.len) return .refused;
    return if (state.composers.gdoc.current.links.items[index].parts == want) .ok else .refused;
}


// ---------------------------------------------------------------------------
// The Fields Composer (05-10, D-06/D-07/D-12): core.composers' field set behind
// the window. Shell and entry arguments are `terrain` or `objects` (the MFC's
// two shell lists), indices from 0.
// ---------------------------------------------------------------------------

fn stateHasObject(ctx: *anyopaque, name: []const u8) bool {
    const state: *State = @ptrCast(@alignCast(ctx));
    for (state.catalogue) |*entry| {
        if (std.mem.eql(u8, std.mem.sliceTo(&entry.name, 0), name)) return true;
    }
    return false;
}

/// The Check! asks the object catalogue the palette already holds.
pub fn bindObjectLookup(state: *State) void {
    state.composers.object_lookup = .{ .ctx = state, .has_fn = stateHasObject };
}

/// A file opened or a new one: the lists' selections and the chosen shells go.
fn resetFieldUi(state: *State) void {
    state.fc_shell = .{ 0, 0 };
    state.fc_shell_chosen = .{ false, false };
    for (&state.fc_shell_selected) |*list| list.clearRetainingCapacity();
    for (&state.fc_entry_selected) |*list| list.clearRetainingCapacity();
    state.fc_type_selected.clearRetainingCapacity();
    state.fc_avail_selected.clearRetainingCapacity();
}

fn rmgfWindow(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.fields_composer_open = !state.fields_composer_open;
    if (state.fields_composer_open) state.composers.ensureScanned(state.editor);
    return .ok;
}

fn rmgfNew(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    resetFieldUi(state);
    return composerResult(state, state.composers.newField());
}

fn rmgfOpen(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    resetFieldUi(state);
    return composerResult(state, state.composers.openField(state.editor, arg));
}

fn rmgfSave(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerSave(state, state.composers.saveField(state.editor));
}

fn rmgfSaveAs(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    return composerSave(state, state.composers.saveFieldAs(state.editor, arg));
}

fn rmgfTab(state: *State, arg: []const u8) Outcome {
    const tab = std.meta.stringToEnum(panels.FieldTab, arg) orelse return .bad_arg;
    state.fc_tab = tab;
    state.fc_tab_request = true;
    return .ok;
}

fn rmgfSeason(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    if (index >= core.rmg.season_folders.len) return .bad_arg;
    const changed = state.composers.setFieldSeason(index) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `terrain` or `objects` to its slot (0, 1).
fn shellKind(text: []const u8) ?usize {
    if (std.mem.eql(u8, text, "terrain")) return 0;
    if (std.mem.eql(u8, text, "objects")) return 1;
    return null;
}

fn shellCount(state: *State, kind: usize) usize {
    const field = &state.composers.fdoc.current;
    return if (kind == 0) field.tile_shells.items.len else field.object_shells.items.len;
}

fn rmgfShellAdd(state: *State, arg: []const u8) Outcome {
    const kind = shellKind(arg) orelse return .bad_arg;
    const index = state.composers.addFieldShell(kind == 1) catch return .refused;
    state.fc_shell[kind] = index;
    state.fc_shell_chosen[kind] = true;
    state.fc_entry_selected[kind].clearRetainingCapacity();
    return .ok;
}

/// `<kind>:<index>`.
fn parseShell(state: *State, arg: []const u8) ?struct { kind: usize, index: usize, rest: []const u8 } {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const kind = shellKind(parts.next() orelse return null) orelse return null;
    const index = std.fmt.parseInt(usize, parts.next() orelse return null, 10) catch return null;
    if (index >= shellCount(state, kind)) return null;
    return .{ .kind = kind, .index = index, .rest = parts.rest() };
}

fn rmgfShellDel(state: *State, arg: []const u8) Outcome {
    const at = parseShell(state, arg) orelse return .refused;
    const removed = state.composers.removeFieldShells(at.kind == 1, &.{at.index}) catch return .refused;
    state.fc_shell_chosen[at.kind] = false;
    state.fc_shell_selected[at.kind].clearRetainingCapacity();
    state.fc_entry_selected[at.kind].clearRetainingCapacity();
    return if (removed) .ok else .refused;
}

fn rmgfShellPick(state: *State, arg: []const u8) Outcome {
    const at = parseShell(state, arg) orelse return .refused;
    state.fc_shell[at.kind] = at.index;
    state.fc_shell_chosen[at.kind] = true;
    state.fc_entry_selected[at.kind].clearRetainingCapacity();
    // The list shows the shell picked, and only it.
    const list = &state.fc_shell_selected[at.kind];
    list.clearRetainingCapacity();
    list.appendNTimes(state.allocator, false, shellCount(state, at.kind)) catch return .ok;
    list.items[at.index] = true;
    return .ok;
}

/// `<kind>:<index>:<width|step|ratio>:<value>`: ratio is a percent.
fn rmgfShellSet(state: *State, arg: []const u8) Outcome {
    const at = parseShell(state, arg) orelse return .refused;
    var parts = std.mem.splitScalar(u8, at.rest, ':');
    const field_name = parts.next() orelse return .bad_arg;
    const kind = std.meta.stringToEnum(core.composers.Composers.ShellField, field_name) orelse return .bad_arg;
    const value = std.fmt.parseFloat(f32, parts.rest()) catch return .bad_arg;
    const changed = state.composers.setFieldShell(at.kind == 1, at.index, kind, value) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<shell>:<terrain type>`: one tile into a terrain shell.
fn rmgfTileAdd(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const shell = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const tile = std.fmt.parseInt(i32, parts.rest(), 10) catch return .bad_arg;
    if (shell >= shellCount(state, 0)) return .refused;
    const added = state.composers.addShellTiles(shell, &.{tile}) catch return .refused;
    return if (added == 1) .ok else .refused;
}

/// `<shell>:<entry>`.
fn rmgfTileDel(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const shell = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const entry = std.fmt.parseInt(usize, parts.rest(), 10) catch return .bad_arg;
    state.fc_entry_selected[0].clearRetainingCapacity();
    const removed = state.composers.removeShellTiles(shell, &.{entry}) catch return .refused;
    return if (removed) .ok else .refused;
}

/// `<shell>:<entry>:<weight>`.
fn rmgfTileWeight(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const shell = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const entry = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const weight = std.fmt.parseInt(i32, parts.rest(), 10) catch return .bad_arg;
    const changed = state.composers.setShellTileWeights(shell, &.{entry}, weight) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<shell>:<object name>`.
fn rmgfObjectAdd(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const shell = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    if (arg.len == colon + 1) return .bad_arg;
    if (shell >= shellCount(state, 1)) return .refused;
    const added = state.composers.addShellObjects(shell, &.{arg[colon + 1 ..]}) catch return .refused;
    return if (added == 1) .ok else .refused;
}

fn rmgfObjectDel(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const shell = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const entry = std.fmt.parseInt(usize, parts.rest(), 10) catch return .bad_arg;
    state.fc_entry_selected[1].clearRetainingCapacity();
    const removed = state.composers.removeShellObjects(shell, &.{entry}) catch return .refused;
    return if (removed) .ok else .refused;
}

fn rmgfObjectWeight(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const shell = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const entry = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const weight = std.fmt.parseInt(i32, parts.rest(), 10) catch return .bad_arg;
    const changed = state.composers.setShellObjectWeights(shell, &.{entry}, weight) catch return .refused;
    return if (changed) .ok else .refused;
}

/// The objects tab's filter (a name of the D-31 filters, or `none`).
fn rmgfFilter(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0 or arg.len >= state.fc_filter.len) return .bad_arg;
    @memset(&state.fc_filter, 0);
    if (!std.mem.eql(u8, arg, "none")) @memcpy(state.fc_filter[0..arg.len], arg);
    return .ok;
}

/// `<height|pattern_min|pattern_max|positive|profile>:<value>`: the Heights
/// tab's fields (positive in percent).
fn rmgfSet(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const kind = std.meta.stringToEnum(core.composers.Composers.HeightField, arg[0..colon]) orelse return .bad_arg;
    const changed = state.composers.setFieldHeights(state.editor, kind, arg[colon + 1 ..]) catch return .refused;
    if (!changed) {
        state.view.setStatus("composer: ", state.composers.message());
        return .refused;
    }
    return .ok;
}

fn rmgfCheck(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    bindObjectLookup(state);
    return composerResult(state, state.composers.checkField(state.editor));
}

fn rmgfFix(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    bindObjectLookup(state);
    return composerResult(state, state.composers.fixFieldFinding(state.editor, index));
}

fn rmgfFixAll(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    bindObjectLookup(state);
    return composerResult(state, state.composers.fixFieldAll(state.editor));
}

fn rmgfUndo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.undoField() catch return .refused;
    return if (done) .ok else .refused;
}

fn rmgfRedo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.redoField() catch return .refused;
    return if (done) .ok else .refused;
}

fn rmgfDirtyIs(state: *State, arg: []const u8) Outcome {
    return flagIs(state.composers.fdoc.dirty, arg);
}
fn rmgfNameEndsWith(state: *State, arg: []const u8) Outcome {
    return if (std.mem.endsWith(u8, state.composers.fdoc.name, arg)) .ok else .refused;
}
fn rmgfFindingsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.field_report orelse return .refused;
    return countIs(report.findings.items.len, arg);
}
fn rmgfErrorsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.field_report orelse return .refused;
    return countIs(report.errorCount(), arg);
}
/// The season combo's slot (0 summer .. 3 spring).
fn rmgfSeasonIs(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.fdoc.current.seasonSlot(), arg);
}
/// `<terrain|objects>:<count>`.
fn rmgfShellsAre(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const kind = shellKind(arg[0..colon]) orelse return .bad_arg;
    return countIs(shellCount(state, kind), arg[colon + 1 ..]);
}
/// `<shell>:<count>`: the tiles of a terrain shell.
fn rmgfTilesAre(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const shell = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    if (shell >= shellCount(state, 0)) return .refused;
    return countIs(state.composers.fdoc.current.tile_shells.items[shell].tiles.items.len, arg[colon + 1 ..]);
}
/// `<shell>:<count>`: the objects of an objects shell.
fn rmgfObjectsAre(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const shell = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    if (shell >= shellCount(state, 1)) return .refused;
    return countIs(state.composers.fdoc.current.object_shells.items[shell].objects.items.len, arg[colon + 1 ..]);
}

/// `<field>:<text>` - a heights field as the tab shows it (height and percent
/// with two decimals, the pattern sizes whole, the profile as stored).
fn rmgfValueIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const kind = std.meta.stringToEnum(core.composers.Composers.HeightField, arg[0..colon]) orelse return .bad_arg;
    const field = &state.composers.fdoc.current;
    var buffer: [128]u8 = undefined;
    const have = switch (kind) {
        .height => std.fmt.bufPrint(&buffer, "{d:.2}", .{field.height}) catch return .refused,
        .pattern_min => std.fmt.bufPrint(&buffer, "{d}", .{field.pattern_min}) catch return .refused,
        .pattern_max => std.fmt.bufPrint(&buffer, "{d}", .{field.pattern_max}) catch return .refused,
        .positive => std.fmt.bufPrint(&buffer, "{d:.2}", .{field.positive_ratio * 100.0}) catch return .refused,
        .profile => field.profile,
    };
    return if (std.mem.eql(u8, have, arg[colon + 1 ..])) .ok else .refused;
}

fn rmgfListedAtLeast(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.composers.ensureScanned(state.editor);
    return if (state.composers.field_names.items.len >= want) .ok else .refused;
}

/// The objects tab offers at least N objects under its filter.
fn rmgfAvailableAtLeast(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    panels.refreshAvailableObjects(state);
    return if (state.fc_avail.items.len >= want) .ok else .refused;
}

fn rmgfTabIs(state: *State, arg: []const u8) Outcome {
    const tab = std.meta.stringToEnum(panels.FieldTab, arg) orelse return .bad_arg;
    return if (state.fc_tab == tab) .ok else .refused;
}


// ---------------------------------------------------------------------------
// The Templates Composer (05-10, D-06/D-07/D-12): core.composers' template behind
// the window. Names are relative to the kind's folder like every composer's; a
// weight edit names its list (`fields`, `graphs` or `vso`) and index from 0.
// ---------------------------------------------------------------------------

fn resetTemplateUi(state: *State) void {
    for (&state.tc_selected) |*list| list.clearRetainingCapacity();
    state.tc_cells_valid = false;
}

fn rmgtWindow(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    state.templates_composer_open = !state.templates_composer_open;
    if (state.templates_composer_open) state.composers.ensureScanned(state.editor);
    return .ok;
}

fn rmgtNew(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    resetTemplateUi(state);
    return composerResult(state, state.composers.newTemplate());
}

fn rmgtOpen(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    resetTemplateUi(state);
    return composerResult(state, state.composers.openTemplate(state.editor, arg));
}

fn rmgtSave(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    return composerSave(state, state.composers.saveTemplate(state.editor));
}

fn rmgtSaveAs(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    return composerSave(state, state.composers.saveTemplateAs(state.editor, arg));
}

fn rmgtAdded(state: *State, added: anytype, want: usize) Outcome {
    const count = added catch return composerResult(state, @as(error{Failed}!void, error.Failed));
    if (count == want) return .ok;
    state.view.setStatus("composer: ", state.composers.message());
    return .refused;
}

fn rmgtGraphAdd(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    state.tc_selected[1].clearRetainingCapacity();
    return rmgtAdded(state, state.composers.addTemplateGraphs(state.editor, &.{arg}), 1);
}

fn rmgtFieldAdd(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    state.tc_selected[0].clearRetainingCapacity();
    return rmgtAdded(state, state.composers.addTemplateFields(state.editor, &.{arg}), 1);
}

fn rmgtVsoAdd(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    state.tc_selected[2].clearRetainingCapacity();
    return rmgtAdded(state, state.composers.addTemplateVsos(state.editor, &.{arg}), 1);
}

fn rmgtDelete(state: *State, list: core.composers.Composers.TemplateList, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.tc_selected[@intFromEnum(list)].clearRetainingCapacity();
    const removed = state.composers.removeTemplateEntries(list, &.{index}) catch return .refused;
    return if (removed) .ok else .refused;
}

fn rmgtGraphDel(state: *State, arg: []const u8) Outcome {
    return rmgtDelete(state, .graphs, arg);
}
fn rmgtFieldDel(state: *State, arg: []const u8) Outcome {
    return rmgtDelete(state, .fields, arg);
}
fn rmgtVsoDel(state: *State, arg: []const u8) Outcome {
    return rmgtDelete(state, .vso, arg);
}

/// `<fields|graphs|vso>:<index>:<weight>`.
fn rmgtWeightSet(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const list = std.meta.stringToEnum(core.composers.Composers.TemplateList, parts.next() orelse return .bad_arg) orelse return .bad_arg;
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const weight = std.fmt.parseInt(i32, parts.rest(), 10) catch return .bad_arg;
    const changed = state.composers.setTemplateWeight(list, index, weight) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<index>:<width in cells>:<opacity in percent>`.
fn rmgtVsoSet(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const width = std.fmt.parseFloat(f32, parts.next() orelse return .bad_arg) catch return .bad_arg;
    const opacity = std.fmt.parseFloat(f32, parts.rest()) catch return .bad_arg;
    const changed = state.composers.setTemplateVso(index, width, opacity) catch return .refused;
    return if (changed) .ok else .refused;
}

/// The default field's index, or -1 for none.
fn rmgtDefaultField(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    const changed = state.composers.setTemplateDefaultField(index) catch return .refused;
    return if (changed) .ok else .refused;
}

/// The mission script's storage name without ".lua", or `none`.
fn rmgtScript(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    const changed = state.composers.setTemplateText(.script_file, if (std.mem.eql(u8, arg, "none")) "" else arg) catch return .refused;
    return if (changed) .ok else .refused;
}

/// The MOD combo: `none` or an installed mod's folder.
fn rmgtMod(state: *State, arg: []const u8) Outcome {
    if (arg.len == 0) return .bad_arg;
    var name: []const u8 = "";
    var version: []const u8 = "";
    if (!std.mem.eql(u8, arg, "none")) {
        panels.refreshModList(state);
        var found = false;
        for (state.mod_list_buffer[0..state.mod_list_count]) |*mod| {
            if (!std.mem.eql(u8, std.mem.sliceTo(&mod.folder, 0), arg)) continue;
            name = std.mem.sliceTo(&mod.name, 0);
            version = std.mem.sliceTo(&mod.version, 0);
            found = true;
            break;
        }
        if (!found) return .refused;
    }
    const changed = state.composers.setTemplateMod(name, version) catch return .refused;
    return if (changed) .ok else .refused;
}

fn rmgtPlayerAdd(state: *State, arg: []const u8) Outcome {
    const side = std.fmt.parseInt(u8, arg, 10) catch return .bad_arg;
    const changed = state.composers.addTemplatePlayer(side) catch return .refused;
    return if (changed) .ok else .refused;
}

fn rmgtPlayerDel(state: *State, arg: []const u8) Outcome {
    const player = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    const changed = state.composers.deleteTemplatePlayer(player) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<player>:<side>`.
fn rmgtPlayerSide(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const player = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const side = std.fmt.parseInt(u8, parts.rest(), 10) catch return .bad_arg;
    const changed = state.composers.setTemplatePlayerSide(player, side) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<game type 0..2>:<attacking side 0..1>`.
fn rmgtGameType(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const game_type = std.fmt.parseInt(i32, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const attacking = std.fmt.parseInt(i32, parts.rest(), 10) catch return .bad_arg;
    const changed = state.composers.setTemplateGameType(game_type, attacking) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<player>:<field>=<value>`: the Units... grid's fields (party, aircraft<N>_name,
/// aircraft<N>_formation, aircraft<N>_count, paratroop_name, paratroop_count, relax).
fn rmgtUnitsSet(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const player = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    const equals = std.mem.indexOfScalar(u8, arg[colon + 1 ..], '=') orelse return .bad_arg;
    const field_name = arg[colon + 1 .. colon + 1 + equals];
    const value = arg[colon + 2 + equals ..];
    const changed = state.composers.setTemplateUnit(state.editor, player, field_name, value) catch return .refused;
    if (!changed) {
        state.view.setStatus("composer: ", state.composers.message());
        return .refused;
    }
    return .ok;
}

/// `<player>:<x>:<y>` in map units.
fn rmgtAppearAdd(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const player = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const x = std.fmt.parseFloat(f32, parts.next() orelse return .bad_arg) catch return .bad_arg;
    const y = std.fmt.parseFloat(f32, parts.rest()) catch return .bad_arg;
    const changed = state.composers.addTemplateAppear(player, x, y) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<player>:<index>:<x>:<y>` in map units.
fn rmgtAppearSet(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const player = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const x = std.fmt.parseFloat(f32, parts.next() orelse return .bad_arg) catch return .bad_arg;
    const y = std.fmt.parseFloat(f32, parts.rest()) catch return .bad_arg;
    const changed = state.composers.setTemplateAppear(player, index, x, y) catch return .refused;
    return if (changed) .ok else .refused;
}

/// `<player>:<index>`.
fn rmgtAppearDel(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const player = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const index = std.fmt.parseInt(usize, parts.rest(), 10) catch return .bad_arg;
    const changed = state.composers.removeTemplateAppear(player, index) catch return .refused;
    return if (changed) .ok else .refused;
}

/// Opens (or closes) the template window's Diplomacy and Units popups: `units`,
/// `diplomacy` or `none`.
fn rmgtPopup(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "units")) {
        state.tc_popup = .unit_grid;
    } else if (std.mem.eql(u8, arg, "diplomacy")) {
        state.tc_popup = .diplomacy;
    } else if (std.mem.eql(u8, arg, "none")) {
        state.tc_popup = .none;
    } else return .bad_arg;
    state.templates_composer_open = true;
    return .ok;
}

fn rmgtCheck(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    bindObjectLookup(state);
    return composerResult(state, state.composers.checkTemplate(state.editor));
}

fn rmgtFix(state: *State, arg: []const u8) Outcome {
    const index = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    bindObjectLookup(state);
    return composerResult(state, state.composers.fixTemplateFinding(state.editor, index));
}

fn rmgtFixAll(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    bindObjectLookup(state);
    return composerResult(state, state.composers.fixTemplateAll(state.editor));
}

fn rmgtUndo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.undoTemplate() catch return .refused;
    return if (done) .ok else .refused;
}

fn rmgtRedo(state: *State, arg: []const u8) Outcome {
    if (arg.len != 0) return .bad_arg;
    const done = state.composers.redoTemplate() catch return .refused;
    return if (done) .ok else .refused;
}

fn rmgtDirtyIs(state: *State, arg: []const u8) Outcome {
    return flagIs(state.composers.tdoc.dirty, arg);
}
fn rmgtNameEndsWith(state: *State, arg: []const u8) Outcome {
    return if (std.mem.endsWith(u8, state.composers.tdoc.name, arg)) .ok else .refused;
}
fn rmgtFindingsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.template_report orelse return .refused;
    return countIs(report.findings.items.len, arg);
}
fn rmgtErrorsAre(state: *State, arg: []const u8) Outcome {
    const report = state.composers.template_report orelse return .refused;
    return countIs(report.errorCount(), arg);
}
fn rmgtGraphsAre(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.tdoc.current.graphs.items.len, arg);
}
fn rmgtFieldsAre(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.tdoc.current.fields.items.len, arg);
}
fn rmgtVsosAre(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.tdoc.current.vso.items.len, arg);
}
fn rmgtPlayersAre(state: *State, arg: []const u8) Outcome {
    return countIs(state.composers.tdoc.current.playerCount(), arg);
}
fn rmgtDefaultIs(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(i32, arg, 10) catch return .bad_arg;
    return if (state.composers.tdoc.current.default_field == want) .ok else .refused;
}

/// `<fields|graphs|vso>:<index>:<weight>`.
fn rmgtWeightIs(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const list = std.meta.stringToEnum(core.composers.Composers.TemplateList, parts.next() orelse return .bad_arg) orelse return .bad_arg;
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const want = std.fmt.parseInt(i32, parts.rest(), 10) catch return .bad_arg;
    const t = &state.composers.tdoc.current;
    const have: i32 = switch (list) {
        .fields => if (index < t.fields.items.len) t.fields.items[index].weight else return .refused,
        .graphs => if (index < t.graphs.items.len) t.graphs.items[index].weight else return .refused,
        .vso => if (index < t.vso.items.len) t.vso.items[index].weight else return .refused,
    };
    return if (have == want) .ok else .refused;
}

/// `<index>:<width in cells>:<opacity in percent>`, compared to two decimals.
fn rmgtVsoIs(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const index = std.fmt.parseInt(usize, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const width = std.fmt.parseFloat(f32, parts.next() orelse return .bad_arg) catch return .bad_arg;
    const opacity = std.fmt.parseFloat(f32, parts.rest()) catch return .bad_arg;
    const t = &state.composers.tdoc.current;
    if (index >= t.vso.items.len) return .refused;
    const entry = t.vso.items[index];
    return if (@abs(entry.width / core.rmg.world_cell - width) < 0.005 and @abs(entry.opacity * 100 - opacity) < 0.005) .ok else .refused;
}

fn rmgtScriptIs(state: *State, arg: []const u8) Outcome {
    if (std.mem.eql(u8, arg, "none")) return if (state.composers.tdoc.current.script_file.len == 0) .ok else .refused;
    return if (std.mem.endsWith(u8, state.composers.tdoc.current.script_file, arg)) .ok else .refused;
}

/// `none` or the mod's name (and version when the arg is `<name> <version>`).
fn rmgtModIs(state: *State, arg: []const u8) Outcome {
    const t = &state.composers.tdoc.current;
    if (std.mem.eql(u8, arg, "none")) return if (t.mod_name.len == 0 and t.mod_version.len == 0) .ok else .refused;
    return if (std.mem.eql(u8, t.mod_name, arg)) .ok else .refused;
}

/// The diplomacy table as digits, e.g. `0112` (two... three players and the neutral).
fn rmgtSidesAre(state: *State, arg: []const u8) Outcome {
    const sides = state.composers.tdoc.current.diplomacies.items;
    if (arg.len != sides.len) return .refused;
    for (sides, arg) |side, text| {
        if (text < '0' or text > '9' or side != text - '0') return .refused;
    }
    return .ok;
}

/// `<game type>:<attacking side>`.
fn rmgtGameTypeIs(state: *State, arg: []const u8) Outcome {
    var parts = std.mem.splitScalar(u8, arg, ':');
    const game_type = std.fmt.parseInt(i32, parts.next() orelse return .bad_arg, 10) catch return .bad_arg;
    const attacking = std.fmt.parseInt(i32, parts.rest(), 10) catch return .bad_arg;
    const t = &state.composers.tdoc.current;
    return if (t.game_type == game_type and t.attacking_side == attacking) .ok else .refused;
}

/// `<player>:<field>=<value>`, the grid's own fields as text.
fn rmgtUnitIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const player = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    const equals = std.mem.indexOfScalar(u8, arg[colon + 1 ..], '=') orelse return .bad_arg;
    const field_name = arg[colon + 1 .. colon + 1 + equals];
    const want = arg[colon + 2 + equals ..];
    const t = &state.composers.tdoc.current;
    if (player >= t.units.items.len) return .refused;
    const unit = &t.units.items[player];
    var buffer: [64]u8 = undefined;
    const have: []const u8 = blk: {
        if (std.mem.eql(u8, field_name, "party")) break :blk unit.partySlice();
        if (std.mem.eql(u8, field_name, "paratroop_name")) break :blk unit.paratroopSlice();
        if (std.mem.eql(u8, field_name, "paratroop_count")) break :blk std.fmt.bufPrint(&buffer, "{d}", .{unit.paratroop_count}) catch return .refused;
        if (std.mem.eql(u8, field_name, "relax")) break :blk std.fmt.bufPrint(&buffer, "{d}", .{unit.relax_time}) catch return .refused;
        if (std.mem.startsWith(u8, field_name, "aircraft") and field_name.len > 10 and field_name[9] == '_') {
            const slot = std.fmt.parseInt(usize, field_name[8..9], 10) catch return .bad_arg;
            if (slot >= unit.aircraft.len) return .bad_arg;
            const rest = field_name[10..];
            if (std.mem.eql(u8, rest, "name")) break :blk unit.aircraft[slot].nameSlice();
            if (std.mem.eql(u8, rest, "formation")) break :blk std.fmt.bufPrint(&buffer, "{d}", .{unit.aircraft[slot].formation_size}) catch return .refused;
            if (std.mem.eql(u8, rest, "count")) break :blk std.fmt.bufPrint(&buffer, "{d}", .{unit.aircraft[slot].count}) catch return .refused;
        }
        return .bad_arg;
    };
    return if (std.mem.eql(u8, have, want)) .ok else .refused;
}

/// `<player>:<count>`: the player's appear points.
fn rmgtAppearIs(state: *State, arg: []const u8) Outcome {
    const colon = std.mem.indexOfScalar(u8, arg, ':') orelse return .bad_arg;
    const player = std.fmt.parseInt(usize, arg[0..colon], 10) catch return .bad_arg;
    if (player >= state.composers.tdoc.current.units.items.len) return .refused;
    return countIs(state.composers.tdoc.current.units.items[player].appear_count, arg[colon + 1 ..]);
}

/// `<x>x<y>`: the template's size in patches (the first graph's).
fn rmgtSizeIs(state: *State, arg: []const u8) Outcome {
    const x = std.mem.indexOfScalar(u8, arg, 'x') orelse return .bad_arg;
    const want_x = std.fmt.parseInt(i32, arg[0..x], 10) catch return .bad_arg;
    const want_y = std.fmt.parseInt(i32, arg[x + 1 ..], 10) catch return .bad_arg;
    const t = &state.composers.tdoc.current;
    return if (t.size_x == want_x and t.size_y == want_y) .ok else .refused;
}

fn rmgtListedAtLeast(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.composers.ensureScanned(state.editor);
    return if (state.composers.template_names.items.len >= want) .ok else .refused;
}

fn rmggZoomIs(state: *State, arg: []const u8) Outcome {
    return countIs(@intCast(state.composers.canvas.patches), arg);
}

fn rmggListedAtLeast(state: *State, arg: []const u8) Outcome {
    const want = std.fmt.parseInt(usize, arg, 10) catch return .bad_arg;
    state.composers.ensureScanned(state.editor);
    return if (state.composers.graph_names.items.len >= want) .ok else .refused;
}
