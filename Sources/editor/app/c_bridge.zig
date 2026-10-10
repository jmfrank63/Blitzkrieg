//! The core's Bridge over the real C ABI (Sources/src/EditorBridge/bridge.h).
//! The core is std-only and runs on every target; this file is where it meets
//! the engine, so it lives in the app. Statuses map one to one, strings are
//! copied into NUL-terminated buffers on the way in, and the bridge's last
//! message is copied out, because the bridge's pointer is valid only until
//! its next call.
const std = @import("std");
const core = @import("editor_core");
/// bridge.h, translated once (build.zig's `editorBridgeC`) and taken through
/// `editor_kit.host`, so the kit's `Host.session` and this file's
/// `RealBridge.session` share one opaque type. Two translations of the same
/// header produce incompatible opaque types (observed when S02/T04 moved
/// host.zig into the kit).
pub const c = @import("editor_kit").host.c;

const Status = core.bridge.Status;
const Bridge = core.bridge.Bridge;
const MapInfo = core.bridge.MapInfo;
const ObjectRecord = core.bridge.ObjectRecord;
const ObjectFieldsEdit = core.bridge.ObjectFieldsEdit;
const SoundRecord = core.bridge.SoundRecord;
const record_types = core.records;
const PaintCell = core.bridge.PaintCell;
const VsoKind = core.bridge.VsoKind;
const VsoDescriptor = core.bridge.VsoDescriptor;
const VsoKeyPoint = core.bridge.VsoKeyPoint;
const VsoView = core.bridge.VsoView;

/// Hands an extern struct to the C side (or back) as the struct of the same layout
/// that the comptime asserts below pin. `@bitCast` accepts only packed types in
/// 0.17, so the bytes are copied across instead.
fn reinterpret(comptime To: type, value: anytype) To {
    comptime std.debug.assert(@sizeOf(To) == @sizeOf(@TypeOf(value)));
    return std.mem.bytesToValue(To, std.mem.asBytes(&value));
}

comptime {
    // The core's PaintCell is handed to BkEditorPaint as it is.
    std.debug.assert(@sizeOf(PaintCell) == @sizeOf(c.BkEditorPaintCell));
    std.debug.assert(@offsetOf(PaintCell, "x") == @offsetOf(c.BkEditorPaintCell, "x"));
    std.debug.assert(@offsetOf(PaintCell, "y") == @offsetOf(c.BkEditorPaintCell, "y"));
    std.debug.assert(@offsetOf(PaintCell, "tile") == @offsetOf(c.BkEditorPaintCell, "tile"));
    // The core's AltitudeRegion (M3) is handed to BkEditorAltitudes and
    // BkEditorSetAltitudes as it is: four ints, 16 bytes.
    std.debug.assert(@sizeOf(core.bridge.AltitudeRegion) == @sizeOf(c.BkEditorAltitudeRegion));
    std.debug.assert(@sizeOf(core.bridge.AltitudeRegion) == 16);
    std.debug.assert(@offsetOf(core.bridge.AltitudeRegion, "x0") == @offsetOf(c.BkEditorAltitudeRegion, "x0"));
    std.debug.assert(@offsetOf(core.bridge.AltitudeRegion, "y1") == @offsetOf(c.BkEditorAltitudeRegion, "y1"));
    // The new-map params (M3): three ints and two name buffers, handed to
    // BkEditorNewMap as they are.
    std.debug.assert(@sizeOf(core.bridge.NewMapParams) == @sizeOf(c.BkEditorNewMapParams));
    std.debug.assert(@sizeOf(c.BkEditorNewMapParams) == 12 + 2 * core.bridge.name_capacity);
    std.debug.assert(@offsetOf(core.bridge.NewMapParams, "name") == @offsetOf(c.BkEditorNewMapParams, "szName"));
    std.debug.assert(@offsetOf(core.bridge.NewMapParams, "mod_folder") == @offsetOf(c.BkEditorNewMapParams, "szModFolder"));
    // The heights stroke params (M3, D-18): three ints, six floats, two ints
    // - 44 bytes - handed to BkEditorHeightsStroke as they are.
    std.debug.assert(@sizeOf(core.bridge.HeightsStrokeParams) == @sizeOf(c.BkEditorHeightsStrokeParams));
    std.debug.assert(@sizeOf(c.BkEditorHeightsStrokeParams) == 44);
    std.debug.assert(@offsetOf(core.bridge.HeightsStrokeParams, "action") == @offsetOf(c.BkEditorHeightsStrokeParams, "action"));
    std.debug.assert(@offsetOf(core.bridge.HeightsStrokeParams, "height_speed") == @offsetOf(c.BkEditorHeightsStrokeParams, "height_speed"));
    std.debug.assert(@offsetOf(core.bridge.HeightsStrokeParams, "pos_x") == @offsetOf(c.BkEditorHeightsStrokeParams, "pos_x"));
    std.debug.assert(@offsetOf(core.bridge.HeightsStrokeParams, "click_x") == @offsetOf(c.BkEditorHeightsStrokeParams, "click_x"));
    std.debug.assert(@offsetOf(core.bridge.HeightsStrokeParams, "stroke_start") == @offsetOf(c.BkEditorHeightsStrokeParams, "stroke_start"));
    std.debug.assert(@offsetOf(core.bridge.HeightsStrokeParams, "ctrl_held") == @offsetOf(c.BkEditorHeightsStrokeParams, "ctrl_held"));
    // The core's camera anchors are read and put field by field, but the C
    // record's layout is part of the ABI: 12 (neutral) + 4 (count) + 32 * 12.
    std.debug.assert(@sizeOf(c.BkEditorVec3) == 12);
    std.debug.assert(@sizeOf(c.BkEditorCameraAnchorRecord) == 12 + 4 + record_types.max_camera_players * 12);
    std.debug.assert(@typeInfo(@TypeOf(@as(c.BkEditorCameraAnchorRecord, undefined).players)).array.len == record_types.max_camera_players);
    // The road and river records' C layouts (BkEditorVsoInfo is read field
    // by field; the sizes are the ABI).
    std.debug.assert(@sizeOf(c.BkEditorVsoDescriptor) == core.bridge.vso_name_capacity);
    std.debug.assert(@sizeOf(c.BkEditorVsoKeyPoint) == 8 * 4);
    std.debug.assert(@sizeOf(c.BkEditorVsoInfo) == 4 + core.bridge.vso_name_capacity + 4 + 4);
    // The bridge records (04-06): read and written field by field; the sizes
    // are the ABI.
    std.debug.assert(@sizeOf(c.BkEditorBridgeDescriptor) == core.bridge.name_capacity + 3 * 4);
    std.debug.assert(@sizeOf(c.BkEditorPlannedPiece) == 4 * 4);
    std.debug.assert(@sizeOf(c.BkEditorBridgeInfo) == core.bridge.name_capacity + 4 + 4 * 4 + 4);
    std.debug.assert(@sizeOf(c.BkEditorFenceDescriptor) == core.bridge.name_capacity);
    std.debug.assert(@sizeOf(c.BkEditorEntrenchmentInfo) == 3 * 4 + 4 * 4);
    std.debug.assert(@sizeOf(c.BkEditorScriptFileRecord) == core.records.script_file_capacity);
    std.debug.assert(@sizeOf(c.BkEditorScriptAreaRecord) == core.records.area_name_capacity + 4 + 5 * 4);
    // The start command's record (04-11): type, target link, x, y, flag, number,
    // unit count - seven 4-byte fields; the action type's name and id.
    std.debug.assert(@sizeOf(c.BkEditorStartCommandRecord) == 7 * 4);
    std.debug.assert(@sizeOf(c.BkEditorActionCommand) == core.bridge.name_capacity + 4);
    // The object filters (M3, D-31): handed to BkEditorObjectFilters and
    // BkEditorSaveObjectFilters as they are.
    std.debug.assert(@sizeOf(c.BkEditorObjectFilterWords) == @sizeOf(core.bridge.ObjectFilterWords));
    std.debug.assert(@sizeOf(c.BkEditorObjectFilter) == @sizeOf(core.bridge.ObjectFilter));
    std.debug.assert(@offsetOf(c.BkEditorObjectFilter, "list_count") == @offsetOf(core.bridge.ObjectFilter, "list_count"));
    std.debug.assert(@offsetOf(c.BkEditorObjectFilter, "user") == @offsetOf(core.bridge.ObjectFilter, "user"));
    std.debug.assert(@offsetOf(c.BkEditorObjectFilter, "lists") == @offsetOf(core.bridge.ObjectFilter, "lists"));
    // The fields application (M3, D-21): handed to BkEditorApplyField as it
    // is; the report and the RMG name read back field by field with sizes
    // asserted.
    std.debug.assert(@sizeOf(c.BkEditorFieldApplyParams) == @sizeOf(core.bridge.FieldApplyParams));
    std.debug.assert(@offsetOf(c.BkEditorFieldApplyParams, "point_count") == @offsetOf(core.bridge.FieldApplyParams, "point_count"));
    std.debug.assert(@offsetOf(c.BkEditorFieldApplyParams, "object_filter") == @offsetOf(core.bridge.FieldApplyParams, "object_filter"));
    std.debug.assert(@sizeOf(c.BkEditorFieldObjectReport) == @sizeOf(core.bridge.FieldObjectReport));
    std.debug.assert(@sizeOf(c.BkEditorRmgName) == @sizeOf(core.bridge.RmgName));
    // Create Random Map (05-08): the params and the result are handed over as
    // they are.
    std.debug.assert(@sizeOf(c.BkEditorRmgGenerateParams) == @sizeOf(core.bridge.RmgGenerateParams));
    std.debug.assert(@offsetOf(c.BkEditorRmgGenerateParams, "map_name") == @offsetOf(core.bridge.RmgGenerateParams, "map_name"));
    std.debug.assert(@offsetOf(c.BkEditorRmgGenerateParams, "seed") == @offsetOf(core.bridge.RmgGenerateParams, "seed"));
    std.debug.assert(@offsetOf(c.BkEditorRmgGenerateParams, "progress_fn") == @offsetOf(core.bridge.RmgGenerateParams, "progress"));
    std.debug.assert(@sizeOf(c.BkEditorRmgGraph) == @sizeOf(core.bridge.RmgGraph));
    std.debug.assert(@sizeOf(c.BkEditorRmgScripts) == @sizeOf(core.bridge.RmgScripts));
    std.debug.assert(@sizeOf(c.BkEditorRmgPatch) == @sizeOf(core.bridge.RmgPatch));
    std.debug.assert(@sizeOf(c.BkEditorRmgContainerRecord) == @sizeOf(core.bridge.RmgContainerRecord));
    std.debug.assert(@sizeOf(c.BkEditorRmgNode) == @sizeOf(core.bridge.RmgNode));
    std.debug.assert(@sizeOf(c.BkEditorRmgLink) == @sizeOf(core.bridge.RmgLink));
    std.debug.assert(@sizeOf(c.BkEditorRmgGraphRecord) == @sizeOf(core.bridge.RmgGraphRecord));
    std.debug.assert(@sizeOf(c.BkEditorRmgPatchInfo) == @sizeOf(core.bridge.RmgPatchInfo));
    // The field set record (05-10) and the tileset's terrain type.
    std.debug.assert(@sizeOf(c.BkEditorRmgWeightedName) == @sizeOf(core.bridge.RmgWeightedName));
    std.debug.assert(@sizeOf(c.BkEditorRmgWeightedTile) == @sizeOf(core.bridge.RmgWeightedTile));
    std.debug.assert(@sizeOf(c.BkEditorRmgTileShell) == @sizeOf(core.bridge.RmgTileShell));
    std.debug.assert(@sizeOf(c.BkEditorRmgObjectShell) == @sizeOf(core.bridge.RmgObjectShell));
    std.debug.assert(@sizeOf(c.BkEditorRmgFieldSetRecord) == @sizeOf(core.bridge.RmgFieldSetRecord));
    std.debug.assert(@sizeOf(c.BkEditorRmgTerrainType) == @sizeOf(core.bridge.RmgTerrainType));
    // The template record (05-10): its unit creation entry is the map's own record
    // layout, and its vso and whole record are handed over as they are.
    std.debug.assert(@sizeOf(c.BkEditorUnitCreationRecord) == @sizeOf(core.bridge.RmgUnit));
    std.debug.assert(@offsetOf(c.BkEditorUnitCreationRecord, "appear") == @offsetOf(core.bridge.RmgUnit, "appear"));
    std.debug.assert(@sizeOf(c.BkEditorRmgVso) == @sizeOf(core.bridge.RmgVso));
    std.debug.assert(@sizeOf(c.BkEditorRmgTemplateRecord) == @sizeOf(core.bridge.RmgTemplateRecord));
    std.debug.assert(@offsetOf(c.BkEditorRmgTemplateRecord, "units") == @offsetOf(core.bridge.RmgTemplateRecord, "units"));
    std.debug.assert(@sizeOf(c.BkEditorRmgGenerateResult) == @sizeOf(core.bridge.RmgGenerateResult));
    std.debug.assert(@offsetOf(c.BkEditorRmgGenerateResult, "map_path") == @offsetOf(core.bridge.RmgGenerateResult, "map_path"));
    // The reserve position's record: gun, truck, x, y.
    std.debug.assert(@sizeOf(c.BkEditorReservePositionRecord) == 4 * 4);
    // The AI general's records (04-12): four counts; a parcel's type, centre, radius,
    // direction and point range; a point's place and direction.
    std.debug.assert(@sizeOf(c.BkEditorAISideInfo) == 4 * 4);
    std.debug.assert(@sizeOf(c.BkEditorAIParcel) == 7 * 4);
    std.debug.assert(@sizeOf(c.BkEditorAIPoint) == 3 * 4);
    // The unit creation's record (05-05, D-30): read and put field by field; the
    // sizes are the ABI - an aircraft slot is a name and two ints, the record the
    // slot count, the party, five slots, the paratroop name and two ints, the
    // appear count and 32 points.
    std.debug.assert(@sizeOf(c.BkEditorUcAircraft) == record_types.uc_name_capacity + 2 * 4);
    std.debug.assert(@sizeOf(c.BkEditorUnitCreationRecord) == 4 + record_types.uc_name_capacity + record_types.uc_aircraft_slots * (record_types.uc_name_capacity + 8) +
        record_types.uc_name_capacity + 4 + 4 + 4 + record_types.max_appear_points * 12);
    std.debug.assert(@sizeOf(c.BkEditorUcName) == @sizeOf(core.bridge.UcName));
    // The minimap's reads (05-07, D-14): a tile region and the unit and area
    // records are handed to and filled by the bridge as they are.
    std.debug.assert(@sizeOf(core.bridge.TileRegion) == @sizeOf(c.BkEditorTileRegion));
    std.debug.assert(@sizeOf(core.bridge.TileRegion) == 16);
    std.debug.assert(@sizeOf(core.bridge.MinimapUnit) == @sizeOf(c.BkEditorMinimapUnit));
    std.debug.assert(@sizeOf(c.BkEditorMinimapUnit) == 7 * 4);
    std.debug.assert(@offsetOf(core.bridge.MinimapUnit, "color_index") == @offsetOf(c.BkEditorMinimapUnit, "color_index"));
    std.debug.assert(@offsetOf(core.bridge.MinimapUnit, "squad") == @offsetOf(c.BkEditorMinimapUnit, "squad"));
    std.debug.assert(@sizeOf(core.bridge.MinimapArea) == @sizeOf(c.BkEditorMinimapArea));
    std.debug.assert(@sizeOf(c.BkEditorMinimapArea) == 8 * 4);
    std.debug.assert(@offsetOf(core.bridge.MinimapArea, "radius") == @offsetOf(c.BkEditorMinimapArea, "radius"));
    std.debug.assert(@offsetOf(core.bridge.MinimapArea, "start_angle") == @offsetOf(c.BkEditorMinimapArea, "start_angle"));
    std.debug.assert(@offsetOf(core.bridge.MinimapArea, "rgb") == @offsetOf(c.BkEditorMinimapArea, "rgb"));
    // Every status the bridge answers has a name in the core.
    std.debug.assert(@intFromEnum(Status.failed) == c.BK_EDITOR_FAILED);
}

fn status(value: c.BkEditorStatus) Status {
    return std.enums.fromInt(Status, value) orelse .failed;
}

/// One object's decoded picture (D-29): `bytes` is RGBA8, top row first,
/// `width * height * 4` of it - a slice of the caller's own buffer, valid
/// only as long as that buffer is.
pub const Picture = struct {
    width: i32,
    height: i32,
    bytes: []const u8,
};

pub const RealBridge = struct {
    session: *c.BkEditorSession,
    message: [512]u8 = undefined,
    /// A refusal the adapter itself decides (an insert over a taken group ID:
    /// BkEditorSetGroup creates or replaces, so the check is here). The next
    /// `lastMessage` answers it instead of the bridge's, once.
    own_message: ?[]const u8 = null,

    pub fn init(session: *c.BkEditorSession) RealBridge {
        return .{ .session = session };
    }

    pub fn bridge(self: *RealBridge) Bridge {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *RealBridge {
        return @ptrCast(@alignCast(ptr));
    }

    /// A path or name as the bridge wants it: NUL-terminated, and short enough
    /// for the buffer - a longer one is refused rather than cut.
    fn terminated(buffer: []u8, text: []const u8) ?[*:0]const u8 {
        if (text.len >= buffer.len) return null;
        @memcpy(buffer[0..text.len], text);
        buffer[text.len] = 0;
        return @ptrCast(buffer.ptr);
    }

    const vtable: Bridge.VTable = .{
        .lastMessage = lastMessage,
        .openMap = openMap,
        .saveMap = saveMap,
        .objects = objects,
        .diplomacy = diplomacy,
        .addObject = addObject,
        .placeObject = placeObject,
        .deleteObject = deleteObject,
        .restoreObject = restoreObject,
        .setDiplomacy = setDiplomacy,
        .setMapType = setMapType,
        .setAttackingSide = setAttackingSide,
        .paint = paint,
        .undoPaint = undoPaint,
        .redoPaint = redoPaint,
        .screenToWorld = screenToWorld,
        .worldToTile = worldToTile,
        .worldToMap = worldToMap,
        .objectAt = objectAt,
        .setPlacementGhost = setPlacementGhost,
        .clearPlacementGhost = clearPlacementGhost,
        .placementGhost = placementGhost,
        .pickObjects = pickObjects,
        .pickObjectsInTiles = pickObjectsInTiles,
        .moveObjects = moveObjects,
        .setObjectFields = setObjectFields,
        .canLink = canLink,
        .setLink = setLink,
        .unlink = unlink,
        .damageObject = damageObject,
        .sounds = vtableSounds,
        .addSound = vtableAddSound,
        .setSound = vtableSetSound,
        .deleteSound = vtableDeleteSound,
        .readRecord = vtableReadRecord,
        .putRecord = vtablePutRecord,
        .recordKeys = vtableRecordKeys,
        .insertRecord = vtableInsertRecord,
        .removeRecord = vtableRemoveRecord,
        .firstFreeGroupID = vtableFirstFreeGroupID,
        .setHiddenScriptIDs = vtableSetHiddenScriptIDs,
        .groundHeight = vtableGroundHeight,
        .setObjectScriptID = setObjectScriptID,
        .scriptAreaFromVis = vtableScriptAreaFromVis,
        .scriptAreaMoved = vtableScriptAreaMoved,
        .scriptAreaResized = vtableScriptAreaResized,
        .actionCommands = vtableActionCommands,
        .reserveRole = vtableReserveRole,
        .undoEdit = vtableUndoEdit,
        .redoEdit = vtableRedoEdit,
        .altitudes = vtableAltitudes,
        .setAltitudes = vtableSetAltitudes,
        .newMap = vtableNewMap,
        .heightsStroke = vtableHeightsStroke,
        .generateHeights = vtableGenerateHeights,
        .setZeroHeights = vtableSetZeroHeights,
        .updateMap = vtableUpdateMap,
        .fillEntireMap = vtableFillEntireMap,
        .setTerrainModes = vtableSetTerrainModes,
        .layers = vtableLayers,
        .setLayerShow = vtableSetLayerShow,
        .setFireRangeMode = vtableSetFireRangeMode,
        .snapToGrid = vtableSnapToGrid,
        .vsoDescriptors = vtableVsoDescriptors,
        .vsoCount = vtableVsoCount,
        .readVso = vtableReadVso,
        .addVso = vtableAddVso,
        .deleteVso = vtableDeleteVso,
        .moveVsoPoints = vtableMoveVsoPoints,
        .setVsoWidth = vtableSetVsoWidth,
        .setVsoOpacity = vtableSetVsoOpacity,
        .insertVsoPoint = vtableInsertVsoPoint,
        .deleteVsoPoint = vtableDeleteVsoPoint,
        .pickVso = vtablePickVso,
        .bridgeDescriptors = vtableBridgeDescriptors,
        .planBridge = vtablePlanBridge,
        .drawBridge = vtableDrawBridge,
        .bridges = vtableBridges,
        .pickGroup = vtablePickGroup,
        .deleteBridge = vtableDeleteBridge,
        .rotateBridge = vtableRotateBridge,
        .toggleBridgeBuild = vtableToggleBridgeBuild,
        .fenceDescriptors = vtableFenceDescriptors,
        .planFences = vtablePlanFences,
        .drawFences = vtableDrawFences,
        .planEntrenchment = vtablePlanEntrenchment,
        .drawEntrenchment = vtableDrawEntrenchment,
        .entrenchments = vtableEntrenchments,
        .deleteEntrenchment = vtableDeleteEntrenchment,
        .objectFilters = vtableObjectFilters,
        .saveObjectFilters = vtableSaveObjectFilters,
        .applyField = vtableApplyField,
        .fieldSetSeason = vtableFieldSetSeason,
        .listRmg = vtableListRmg,
        .createRandomMap = vtableCreateRandomMap,
        .listStorageFiles = vtableListStorageFiles,
        .rmgTemplateGraphs = vtableRmgTemplateGraphs,
        .rmgCheckSetting = vtableRmgCheckSetting,
        .rmgReadContainer = vtableRmgReadContainer,
        .rmgWriteContainer = vtableRmgWriteContainer,
        .rmgReadGraph = vtableRmgReadGraph,
        .rmgWriteGraph = vtableRmgWriteGraph,
        .rmgPatchInfo = vtableRmgPatchInfo,
        .rmgImportPatch = vtableRmgImportPatch,
        .rmgRoot = vtableRmgRoot,
        .rmgReadFieldSet = vtableRmgReadFieldSet,
        .rmgReadTemplate = vtableRmgReadTemplate,
        .rmgWriteTemplate = vtableRmgWriteTemplate,
        .rmgWriteFieldSet = vtableRmgWriteFieldSet,
        .rmgTileset = vtableRmgTileset,
        .rmgFileExists = vtableRmgFileExists,
        .addPlayer = vtableAddPlayer,
        .deletePlayer = vtableDeletePlayer,
        .unitCreationChoices = vtableUnitCreationChoices,
        .tiles = vtableTiles,
        .minimapTileColors = vtableMinimapTileColors,
        .minimapUnits = vtableMinimapUnits,
        .minimapAreas = vtableMinimapAreas,
        .createMinimapImages = vtableCreateMinimapImages,
    };

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        const text = if (self.own_message) |own| own else std.mem.span(c.BkEditorLastMessage(self.session));
        self.own_message = null;
        const len = @min(text.len, self.message.len);
        @memcpy(self.message[0..len], text[0..len]);
        return self.message[0..len];
    }

    fn openMap(ptr: *anyopaque, path: []const u8, info: *MapInfo) Status {
        const self = from(ptr);
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, path) orelse return .bad_argument;
        var summary: c.BkEditorMapSummary = std.mem.zeroes(c.BkEditorMapSummary);
        const result = status(c.BkEditorOpenMap(self.session, z, &summary));
        if (result == .ok) info.* = .{
            .width_tiles = summary.width_tiles,
            .height_tiles = summary.height_tiles,
            .season = summary.season,
            .player_count = summary.player_count,
            .map_type = summary.map_type,
            .attacking_side = summary.attacking_side,
        };
        return result;
    }

    fn saveMap(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, path) orelse return .bad_argument;
        return status(c.BkEditorSaveMap(self.session, z));
    }

    fn objects(ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorObjects(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        // BkEditorObjects has no offset, so the whole list is read into C
        // records once and converted. The core has already sized `out`; this
        // buffer lives only for the call. Not the C allocator: the app links
        // no libc on Windows (the engine's CRT is linked by the build), and
        // std.heap.c_allocator needs it.
        const records = std.heap.page_allocator.alloc(c.BkEditorObjectRecord, total.*) catch return .failed;
        defer std.heap.page_allocator.free(records);
        var got: c_int = 0;
        const read = status(c.BkEditorObjects(self.session, records.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (records, out[0..records.len]) |record, *object| object.* = toRecord(record);
        return .ok;
    }

    fn toRecord(record: c.BkEditorObjectRecord) ObjectRecord {
        var object: ObjectRecord = .{
            .link_id = record.link_id,
            .x = record.x,
            .y = record.y,
            .dir = record.dir,
            .player = record.player,
            .scenario = record.scenario != 0,
            .known = record.known != 0,
            .script_id = record.script_id,
            .hp = record.hp,
            .frame_index = record.frame_index,
            .link_with = record.link_with,
        };
        object.setName(std.mem.sliceTo(&record.name, 0));
        return object;
    }

    /// BkEditorSetObjectFields (M3, D-26).
    fn setObjectFields(ptr: *anyopaque, link_id: i32, edit: *const ObjectFieldsEdit, token: *i32) Status {
        const c_edit: c.BkEditorObjectFieldsEdit = .{
            .mask = edit.mask,
            .player = edit.player,
            .hp = edit.hp,
            .angle = edit.angle,
            .formation = edit.formation,
        };
        return status(c.BkEditorSetObjectFields(from(ptr).session, link_id, &c_edit, token));
    }

    /// BkEditorCanLink (M3, D-27).
    fn canLink(ptr: *anyopaque, source: i32, target: i32, link_type: *i32) Status {
        return status(c.BkEditorCanLink(from(ptr).session, source, target, link_type));
    }

    /// BkEditorSetLink (M3, D-27).
    fn setLink(ptr: *anyopaque, source: i32, target: i32, token: *i32) Status {
        return status(c.BkEditorSetLink(from(ptr).session, source, target, token));
    }

    /// BkEditorUnlink (M3, D-27).
    fn unlink(ptr: *anyopaque, link_id: i32, token: *i32) Status {
        return status(c.BkEditorUnlink(from(ptr).session, link_id, token));
    }

    /// BkEditorDamageObject (M3, D-29).
    fn damageObject(ptr: *anyopaque, link_id: i32, delta: f32, mode: i32, token: *i32) Status {
        return status(c.BkEditorDamageObject(from(ptr).session, link_id, delta, mode, token));
    }

    fn diplomacy(ptr: *anyopaque, player: i32, value: *i32) Status {
        return status(c.BkEditorDiplomacy(from(ptr).session, player, value));
    }

    fn addObject(ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status {
        var buffer: [core.bridge.name_capacity]u8 = undefined;
        const z = terminated(&buffer, name) orelse return .bad_argument;
        return status(c.BkEditorAddObject(from(ptr).session, z, x, y, dir, player, link_id));
    }

    fn placeObject(ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status {
        return status(c.BkEditorPlaceObject(from(ptr).session, link_id, x, y, dir, player));
    }

    fn deleteObject(ptr: *anyopaque, link_id: i32) Status {
        return status(c.BkEditorDeleteObject(from(ptr).session, link_id));
    }

    fn restoreObject(ptr: *anyopaque, link_id: i32) Status {
        return status(c.BkEditorRestoreObject(from(ptr).session, link_id));
    }

    fn setDiplomacy(ptr: *anyopaque, player: i32, value: i32) Status {
        return status(c.BkEditorSetDiplomacy(from(ptr).session, player, value));
    }

    fn setMapType(ptr: *anyopaque, value: i32) Status {
        return status(c.BkEditorSetMapType(from(ptr).session, value));
    }

    fn setAttackingSide(ptr: *anyopaque, value: i32) Status {
        return status(c.BkEditorSetAttackingSide(from(ptr).session, value));
    }

    fn paint(ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status {
        const count = std.math.cast(c_int, cells.len) orelse return .bad_argument;
        return status(c.BkEditorPaint(from(ptr).session, @ptrCast(cells.ptr), count, token));
    }

    fn undoPaint(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorUndoPaint(from(ptr).session, token));
    }

    fn redoPaint(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorRedoPaint(from(ptr).session, token));
    }

    fn screenToWorld(ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status {
        return status(c.BkEditorScreenToWorld(from(ptr).session, sx, sy, wx, wy));
    }

    fn worldToTile(ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status {
        return status(c.BkEditorWorldToTile(from(ptr).session, wx, wy, tx, ty));
    }

    fn worldToMap(ptr: *anyopaque, wx: f32, wy: f32, mx: *f32, my: *f32) Status {
        return status(c.BkEditorWorldToMap(from(ptr).session, wx, wy, mx, my));
    }

    fn objectAt(ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status {
        return status(c.BkEditorObjectAt(from(ptr).session, sx, sy, link_id));
    }

    fn setPlacementGhost(ptr: *anyopaque, name: []const u8, wx: f32, wy: f32, dir: i32) Status {
        var name_buffer: [core.bridge.name_capacity]u8 = undefined;
        const name_z = terminated(&name_buffer, name) orelse return .bad_argument;
        return status(c.BkEditorSetPlacementGhost(from(ptr).session, name_z, wx, wy, dir));
    }

    fn clearPlacementGhost(ptr: *anyopaque) Status {
        return status(c.BkEditorClearPlacementGhost(from(ptr).session));
    }

    fn placementGhost(ptr: *anyopaque, shown: *bool, wx: *f32, wy: *f32, dir: *i32) Status {
        var raw_shown: c_int = 0;
        const result = status(c.BkEditorPlacementGhost(from(ptr).session, &raw_shown, wx, wy, dir));
        shown.* = raw_shown != 0;
        return result;
    }

    /// BkEditorPickObjects in two passes, like `vtableSounds` (M3, D-25).
    fn pickObjects(ptr: *anyopaque, sx0: f32, sy0: f32, sx1: f32, sy1: f32, out: []i32, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorPickObjects(self.session, sx0, sy0, sx1, sy1, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        var got: c_int = 0;
        const read = status(c.BkEditorPickObjects(self.session, sx0, sy0, sx1, sy1, out.ptr, @intCast(out.len), &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        return .ok;
    }

    /// BkEditorPickObjectsInTiles, the same two passes.
    fn pickObjectsInTiles(ptr: *anyopaque, tx0: i32, ty0: i32, tx1: i32, ty1: i32, out: []i32, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorPickObjectsInTiles(self.session, tx0, ty0, tx1, ty1, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        var got: c_int = 0;
        const read = status(c.BkEditorPickObjectsInTiles(self.session, tx0, ty0, tx1, ty1, out.ptr, @intCast(out.len), &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        return .ok;
    }

    /// BkEditorMoveObjects (M3, D-25).
    fn moveObjects(ptr: *anyopaque, link_ids: []const i32, dx: f32, dy: f32, token: *i32) Status {
        return status(c.BkEditorMoveObjects(from(ptr).session, link_ids.ptr, @intCast(link_ids.len), dx, dy, token));
    }

    /// core.bridge.SoundRecord from a BkEditorSoundRecord - `toRecord`'s own
    /// shape, for sounds.
    fn toSoundRecord(record: c.BkEditorSoundRecord) SoundRecord {
        var sound: SoundRecord = .{
            .x = record.x,
            .y = record.y,
            .z = record.z,
            .repeat_ms = record.repeat_ms,
            .repeat_random_ms = record.repeat_random_ms,
            .mute_in_combat = record.mute_in_combat != 0,
            .min_radius = record.min_radius,
            .max_radius = record.max_radius,
        };
        sound.setName(std.mem.sliceTo(&record.name, 0));
        return sound;
    }

    /// The other direction, for addSound/setSound: the core never builds a
    /// BkEditorSoundRecord itself.
    fn toCSoundRecord(record: SoundRecord) c.BkEditorSoundRecord {
        var out: c.BkEditorSoundRecord = std.mem.zeroes(c.BkEditorSoundRecord);
        const name = record.nameSlice();
        const len = @min(name.len, out.name.len - 1);
        @memcpy(out.name[0..len], name[0..len]);
        out.x = record.x;
        out.y = record.y;
        out.z = record.z;
        out.repeat_ms = record.repeat_ms;
        out.repeat_random_ms = record.repeat_random_ms;
        out.mute_in_combat = if (record.mute_in_combat) 1 else 0;
        out.min_radius = record.min_radius;
        out.max_radius = record.max_radius;
        return out;
    }

    /// BkEditorSounds - `objects`'s own two-pass shape, for the core's
    /// `readSoundAt`/`document`-less sound list.
    fn vtableSounds(ptr: *anyopaque, out: []SoundRecord, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorSounds(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        const records = std.heap.page_allocator.alloc(c.BkEditorSoundRecord, total.*) catch return .failed;
        defer std.heap.page_allocator.free(records);
        var got: c_int = 0;
        const read = status(c.BkEditorSounds(self.session, records.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (records, out[0..records.len]) |record, *sound| sound.* = toSoundRecord(record);
        return .ok;
    }

    fn vtableAddSound(ptr: *anyopaque, index: i32, record: SoundRecord) Status {
        const self = from(ptr);
        var c_record = toCSoundRecord(record);
        return status(c.BkEditorAddSound(self.session, index, &c_record));
    }

    fn vtableSetSound(ptr: *anyopaque, index: i32, record: SoundRecord) Status {
        const self = from(ptr);
        var c_record = toCSoundRecord(record);
        return status(c.BkEditorSetSound(self.session, index, &c_record));
    }

    fn vtableDeleteSound(ptr: *anyopaque, index: i32) Status {
        return status(c.BkEditorDeleteSound(from(ptr).session, index));
    }

    fn toVec3(point: c.BkEditorVec3) record_types.Vec3 {
        return .{ .x = point.x, .y = point.y, .z = point.z };
    }

    fn toCVec3(point: record_types.Vec3) c.BkEditorVec3 {
        return .{ .x = point.x, .y = point.y, .z = point.z };
    }

    /// BkEditorCameraAnchors into the core's record, slot by slot: a count
    /// outside 0..32 from the bridge would be a bridge bug, and is a failure.
    fn toCameraAnchors(record: c.BkEditorCameraAnchorRecord) ?record_types.CameraAnchors {
        if (record.player_count < 0 or record.player_count > record_types.max_camera_players) return null;
        var anchors: record_types.CameraAnchors = .{ .neutral = toVec3(record.neutral), .player_count = @intCast(record.player_count) };
        for (0..anchors.player_count) |index| anchors.players[index] = toVec3(record.players[index]);
        return anchors;
    }

    fn toCCameraAnchors(anchors: record_types.CameraAnchors) c.BkEditorCameraAnchorRecord {
        var record: c.BkEditorCameraAnchorRecord = std.mem.zeroes(c.BkEditorCameraAnchorRecord);
        record.neutral = toCVec3(anchors.neutral);
        record.player_count = @intCast(anchors.player_count);
        for (0..anchors.player_count) |index| record.players[index] = toCVec3(anchors.players[index]);
        return record;
    }

    fn toScriptArea(record: c.BkEditorScriptAreaRecord) ?record_types.ScriptArea {
        var area: record_types.ScriptArea = .{
            .shape = std.enums.fromInt(record_types.AreaShape, record.type) orelse return null,
            .cx = record.cx,
            .cy = record.cy,
            .hx = record.hx,
            .hy = record.hy,
            .r = record.r,
        };
        area.setName(std.mem.sliceTo(&record.name, 0));
        return area;
    }

    fn toCScriptArea(area: record_types.ScriptArea) c.BkEditorScriptAreaRecord {
        var record: c.BkEditorScriptAreaRecord = std.mem.zeroes(c.BkEditorScriptAreaRecord);
        const name = area.nameSlice();
        @memcpy(record.name[0..name.len], name);
        record.type = @intFromEnum(area.shape);
        record.cx = area.cx;
        record.cy = area.cy;
        record.hx = area.hx;
        record.hy = area.hy;
        record.r = area.r;
        return record;
    }

    /// One script area by its index: the list is read whole (two-pass) and the
    /// record picked from it; an index past the end is a bad argument.
    fn readScriptArea(self: *RealBridge, key: i32, allocator: std.mem.Allocator, out: *record_types.Value) Status {
        var count: c_int = 0;
        const sizing = status(c.BkEditorScriptAreas(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (key < 0 or key >= count) return .bad_argument;
        const all = allocator.alloc(c.BkEditorScriptAreaRecord, @intCast(count)) catch return .failed;
        defer allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorScriptAreas(self.session, all.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        out.* = .{ .script_area = toScriptArea(all[@intCast(key)]) orelse return .failed };
        return .ok;
    }

    fn vtableScriptAreaFromVis(ptr: *anyopaque, shape: record_types.AreaShape, wx0: f32, wy0: f32, wx1: f32, wy1: f32, name: []const u8, out: *record_types.ScriptArea) Status {
        var name_buffer: [record_types.area_name_capacity]u8 = undefined;
        const name_z = terminated(&name_buffer, name) orelse return .bad_argument;
        var record: c.BkEditorScriptAreaRecord = std.mem.zeroes(c.BkEditorScriptAreaRecord);
        const result = status(c.BkEditorScriptAreaFromVis(from(ptr).session, @intFromEnum(shape), wx0, wy0, wx1, wy1, name_z, &record));
        if (result != .ok) return result;
        out.* = toScriptArea(record) orelse return .failed;
        return .ok;
    }

    fn vtableScriptAreaMoved(ptr: *anyopaque, area: record_types.ScriptArea, wx: f32, wy: f32, out: *record_types.ScriptArea) Status {
        const record_in = toCScriptArea(area);
        var record: c.BkEditorScriptAreaRecord = std.mem.zeroes(c.BkEditorScriptAreaRecord);
        const result = status(c.BkEditorScriptAreaMoved(from(ptr).session, &record_in, wx, wy, &record));
        if (result != .ok) return result;
        out.* = toScriptArea(record) orelse return .failed;
        return .ok;
    }

    fn vtableScriptAreaResized(ptr: *anyopaque, area: record_types.ScriptArea, wx: f32, wy: f32, out: *record_types.ScriptArea) Status {
        const record_in = toCScriptArea(area);
        var record: c.BkEditorScriptAreaRecord = std.mem.zeroes(c.BkEditorScriptAreaRecord);
        const result = status(c.BkEditorScriptAreaResized(from(ptr).session, &record_in, wx, wy, &record));
        if (result != .ok) return result;
        out.* = toScriptArea(record) orelse return .failed;
        return .ok;
    }

    /// BkEditorActionCommands in two passes (04-11): the sizing pass is REFUSED
    /// when the file lists anything (as a buffer too short is) and still answers
    /// the total; a file that is not there is REFUSED with a total of 0.
    fn vtableActionCommands(ptr: *anyopaque, allocator: std.mem.Allocator, out: *[]core.bridge.ActionCommand, default_index: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        var default_c: c_int = 0;
        const sizing = status(c.BkEditorActionCommands(self.session, null, 0, &count, &default_c));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count <= 0) return if (sizing == .ok) .refused else sizing;
        const all = allocator.alloc(c.BkEditorActionCommand, @intCast(count)) catch return .failed;
        defer allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorActionCommands(self.session, all.ptr, count, &got, &default_c));
        if (read != .ok) return read;
        if (got != count or default_c < 0 or default_c >= count) return .failed;
        const list = allocator.alloc(core.bridge.ActionCommand, @intCast(count)) catch return .failed;
        for (all, list) |item, *entry| {
            entry.* = .{ .id = item.id };
            entry.setName(std.mem.sliceTo(&item.name, 0));
        }
        out.* = list;
        default_index.* = @intCast(default_c);
        return .ok;
    }

    /// BkEditorObjectFilters in two passes (M3, D-31): the sizing pass is
    /// REFUSED when there are filters and still answers the total, like the
    /// action list's.
    fn vtableObjectFilters(ptr: *anyopaque, allocator: std.mem.Allocator, out: *[]core.bridge.ObjectFilter) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorObjectFilters(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count <= 0) {
            out.* = &.{};
            return .ok;
        }
        const all = allocator.alloc(c.BkEditorObjectFilter, @intCast(count)) catch return .failed;
        defer allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorObjectFilters(self.session, all.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        const list = allocator.alloc(core.bridge.ObjectFilter, @intCast(count)) catch return .failed;
        for (all, list) |item, *entry| entry.* = reinterpret(@TypeOf(entry.*), item);
        out.* = list;
        return .ok;
    }

    /// BkEditorSaveObjectFilters (M3, D-31): the slice is handed over as it
    /// is (the layouts are asserted above).
    fn vtableSaveObjectFilters(ptr: *anyopaque, filters: []const core.bridge.ObjectFilter) Status {
        const self = from(ptr);
        const c_filters = @as([*c]const c.BkEditorObjectFilter, @ptrCast(filters.ptr));
        return status(c.BkEditorSaveObjectFilters(self.session, if (filters.len == 0) null else c_filters, @intCast(filters.len)));
    }

    /// BkEditorApplyField (M3, D-21): the params and the report slice are
    /// handed over as they are (the layouts are asserted above); `total` is
    /// always the full count.
    fn vtableApplyField(ptr: *anyopaque, params: core.bridge.FieldApplyParams, report: []core.bridge.FieldObjectReport, total: *usize, token: *i32) Status {
        const self = from(ptr);
        var c_params: c.BkEditorFieldApplyParams = reinterpret(c.BkEditorFieldApplyParams, params);
        var count: c_int = 0;
        const c_report: [*c]c.BkEditorFieldObjectReport = if (report.len == 0) null else @ptrCast(report.ptr);
        const result = status(c.BkEditorApplyField(self.session, &c_params, c_report, @intCast(report.len), &count, token));
        if (result == .ok or result == .refused) total.* = @intCast(@max(count, 0));
        return result;
    }

    /// BkEditorFieldSetSeason (M3, D-21).
    fn vtableFieldSetSeason(ptr: *anyopaque, name: [*:0]const u8, season: *i32) Status {
        const self = from(ptr);
        var out: c_int = -1;
        const result = status(c.BkEditorFieldSetSeason(self.session, name, &out));
        if (result == .ok) season.* = out;
        return result;
    }

    /// BkEditorCreateRandomMap (05-08, D-01): the params and the result are
    /// the C structs' own layout (asserted above); the progress callback is C
    /// ABI on both sides and runs inside the call.
    fn vtableCreateRandomMap(ptr: *anyopaque, params: core.bridge.RmgGenerateParams, result: *core.bridge.RmgGenerateResult) Status {
        const self = from(ptr);
        var c_params: c.BkEditorRmgGenerateParams = reinterpret(c.BkEditorRmgGenerateParams, params);
        const c_result: *c.BkEditorRmgGenerateResult = @ptrCast(result);
        return status(c.BkEditorCreateRandomMap(self.session, &c_params, c_result));
    }

    /// The composer records (05-09): the Zig and C structs share one layout
    /// (asserted above), so each is the C call on the same memory. The two-pass
    /// reads are the core's (`Editor.readContainer` sizes, allocates exactly,
    /// reads) - the adapter never loops a buffer to a returned total.
    fn vtableRmgReadContainer(ptr: *anyopaque, name: [*:0]const u8, record: *core.bridge.RmgContainerRecord) Status {
        return status(c.BkEditorRmgReadContainer(from(ptr).session, name, @ptrCast(record)));
    }
    fn vtableRmgWriteContainer(ptr: *anyopaque, name: [*:0]const u8, record: *const core.bridge.RmgContainerRecord) Status {
        return status(c.BkEditorRmgWriteContainer(from(ptr).session, name, @ptrCast(record)));
    }
    fn vtableRmgReadGraph(ptr: *anyopaque, name: [*:0]const u8, record: *core.bridge.RmgGraphRecord) Status {
        return status(c.BkEditorRmgReadGraph(from(ptr).session, name, @ptrCast(record)));
    }
    fn vtableRmgWriteGraph(ptr: *anyopaque, name: [*:0]const u8, record: *const core.bridge.RmgGraphRecord) Status {
        return status(c.BkEditorRmgWriteGraph(from(ptr).session, name, @ptrCast(record)));
    }
    fn vtableRmgPatchInfo(ptr: *anyopaque, name: [*:0]const u8, info: *core.bridge.RmgPatchInfo) Status {
        return status(c.BkEditorRmgPatchInfoRead(from(ptr).session, name, @ptrCast(info)));
    }
    fn vtableRmgImportPatch(ptr: *anyopaque, source: [*:0]const u8, apply: bool, out: *core.bridge.RmgName) Status {
        return status(c.BkEditorRmgImportPatch(from(ptr).session, source, @intFromBool(apply), @ptrCast(out)));
    }
    fn vtableRmgRoot(ptr: *anyopaque, out: []u8) Status {
        if (out.len == 0) return .bad_argument;
        return status(c.BkEditorRmgRoot(from(ptr).session, out.ptr, @intCast(out.len)));
    }

    /// The field set record (05-10): one layout on both sides, the two-pass read
    /// is the core's (`Editor.readFieldSet`).
    fn vtableRmgReadFieldSet(ptr: *anyopaque, name: [*:0]const u8, record: *core.bridge.RmgFieldSetRecord) Status {
        return status(c.BkEditorRmgReadFieldSet(from(ptr).session, name, @ptrCast(record)));
    }
    fn vtableRmgWriteFieldSet(ptr: *anyopaque, name: [*:0]const u8, record: *const core.bridge.RmgFieldSetRecord) Status {
        return status(c.BkEditorRmgWriteFieldSet(from(ptr).session, name, @ptrCast(record)));
    }

    fn vtableRmgReadTemplate(ptr: *anyopaque, name: [*:0]const u8, record: *core.bridge.RmgTemplateRecord) Status {
        return status(c.BkEditorRmgReadTemplate(from(ptr).session, name, @ptrCast(record)));
    }
    fn vtableRmgWriteTemplate(ptr: *anyopaque, name: [*:0]const u8, record: *const core.bridge.RmgTemplateRecord) Status {
        return status(c.BkEditorRmgWriteTemplate(from(ptr).session, name, @ptrCast(record)));
    }

    /// BkEditorRmgTileset in two passes: size, then exactly the total.
    fn vtableRmgTileset(ptr: *anyopaque, season: i32, out: []core.bridge.RmgTerrainType, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorRmgTileset(self.session, season, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count <= 0) {
            total.* = 0;
            return .refused;
        }
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        const all = std.heap.page_allocator.alloc(c.BkEditorRmgTerrainType, @intCast(count)) catch return .failed;
        defer std.heap.page_allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorRmgTileset(self.session, season, all.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (all, 0..) |item, i| {
            if (i >= out.len) break;
            out[i] = reinterpret(@TypeOf(out[i]), item);
        }
        return .ok;
    }

    fn vtableRmgFileExists(ptr: *anyopaque, name: [*:0]const u8, extension: [*:0]const u8, exists: *bool) Status {
        var flag: c_int = 0;
        const result = status(c.BkEditorRmgFileExists(from(ptr).session, name, extension, &flag));
        exists.* = result == .ok and flag != 0;
        return result;
    }

    /// BkEditorListStorageFiles (05-08, D-13) in two passes, like `vtableListRmg`.
    fn vtableListStorageFiles(ptr: *anyopaque, folder: [*:0]const u8, extension: [*:0]const u8, out: []core.bridge.RmgName, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorListStorageFiles(self.session, folder, extension, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        total.* = @intCast(@max(count, 0));
        // Nothing listed: ok for an empty folder, the refusal itself when the
        // folder or the extension was not plain (the sizing pass of a real
        // listing is refused WITH a count above 0).
        if (count <= 0) return sizing;
        if (out.len < total.*) return .refused;
        const all = std.heap.page_allocator.alloc(c.BkEditorRmgName, @intCast(count)) catch return .failed;
        defer std.heap.page_allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorListStorageFiles(self.session, folder, extension, all.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (all, 0..) |item, i| {
            if (i >= out.len) break;
            out[i] = reinterpret(@TypeOf(out[i]), item);
        }
        return .ok;
    }

    /// BkEditorRmgCheckSetting: the generator's own fit test, before it runs.
    fn vtableRmgCheckSetting(ptr: *anyopaque, template: [*:0]const u8, graph: i32, angle: i32, setting: [*:0]const u8) Status {
        const self = from(ptr);
        return status(c.BkEditorRmgCheckSetting(self.session, template, graph, angle, setting));
    }

    /// BkEditorRmgTemplateGraphs (05-08, D-13) in two passes.
    fn vtableRmgTemplateGraphs(ptr: *anyopaque, template: [*:0]const u8, out: []core.bridge.RmgGraph, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorRmgTemplateGraphs(self.session, template, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        total.* = @intCast(@max(count, 0));
        if (count <= 0) return sizing;
        if (out.len < total.*) return .refused;
        const all = std.heap.page_allocator.alloc(c.BkEditorRmgGraph, @intCast(count)) catch return .failed;
        defer std.heap.page_allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorRmgTemplateGraphs(self.session, template, all.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (all, 0..) |item, i| {
            if (i >= out.len) break;
            out[i] = reinterpret(@TypeOf(out[i]), item);
        }
        return .ok;
    }

    /// BkEditorListRmg (M3, D-08) in two passes: the sizing pass is REFUSED
    /// when the folder lists anything, like the action list's.
    fn vtableListRmg(ptr: *anyopaque, kind: core.bridge.RmgKind, out: []core.bridge.RmgName, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorListRmg(self.session, @intFromEnum(kind), null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        total.* = @intCast(@max(count, 0));
        if (count <= 0) return .ok;
        if (out.len < total.*) return .refused;
        const all = std.heap.page_allocator.alloc(c.BkEditorRmgName, @intCast(count)) catch return .failed;
        defer std.heap.page_allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorListRmg(self.session, @intFromEnum(kind), all.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (all, 0..) |item, i| {
            if (i >= out.len) break;
            out[i] = reinterpret(@TypeOf(out[i]), item);
        }
        return .ok;
    }

    /// BkEditorTiles (05-07, D-14): two-pass, the read never past the total
    /// the sizing call answered.
    fn vtableTiles(ptr: *anyopaque, region: core.bridge.TileRegion, out: []u8, total: *usize) Status {
        const self = from(ptr);
        total.* = 0;
        const c_region: c.BkEditorTileRegion = reinterpret(c.BkEditorTileRegion, region);
        var count: c_int = 0;
        const sizing = status(c.BkEditorTiles(self.session, &c_region, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (sizing == .refused and count == 0) return .refused;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        var got: c_int = 0;
        const read = status(c.BkEditorTiles(self.session, &c_region, out.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        return .ok;
    }

    /// BkEditorMinimapTileColors.
    fn vtableMinimapTileColors(ptr: *anyopaque, out: []u32, total: *usize) Status {
        const self = from(ptr);
        total.* = 0;
        var count: c_int = 0;
        const sizing = status(c.BkEditorMinimapTileColors(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (sizing == .refused and count == 0) return .refused;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        var got: c_int = 0;
        const read = status(c.BkEditorMinimapTileColors(self.session, out.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        return .ok;
    }

    /// BkEditorMinimapUnits.
    fn vtableMinimapUnits(ptr: *anyopaque, out: []core.bridge.MinimapUnit, total: *usize) Status {
        const self = from(ptr);
        total.* = 0;
        var count: c_int = 0;
        const sizing = status(c.BkEditorMinimapUnits(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (sizing == .refused and count == 0) return .refused;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        var got: c_int = 0;
        const read = status(c.BkEditorMinimapUnits(self.session, @ptrCast(out.ptr), count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        return .ok;
    }

    /// BkEditorMinimapAreas.
    fn vtableMinimapAreas(ptr: *anyopaque, out: []core.bridge.MinimapArea, total: *usize) Status {
        const self = from(ptr);
        total.* = 0;
        var count: c_int = 0;
        const sizing = status(c.BkEditorMinimapAreas(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (sizing == .refused and count == 0) return .refused;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        var got: c_int = 0;
        const read = status(c.BkEditorMinimapAreas(self.session, @ptrCast(out.ptr), count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        return .ok;
    }

    /// BkEditorCreateMiniMapImage (05-07, D-17): the path as an OS path.
    fn vtableCreateMinimapImages(ptr: *anyopaque, map_path: []const u8) Status {
        const self = from(ptr);
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, map_path) orelse return .bad_argument;
        return status(c.BkEditorCreateMiniMapImage(self.session, z));
    }

    /// BkEditorAddPlayer / BkEditorDeletePlayer (05-05, D-30).
    fn vtableAddPlayer(ptr: *anyopaque, side: i32, token: *i32) Status {
        var out: c_int = -1;
        const result = status(c.BkEditorAddPlayer(from(ptr).session, side, &out));
        token.* = out;
        return result;
    }

    fn vtableDeletePlayer(ptr: *anyopaque, player: i32, token: *i32) Status {
        var out: c_int = -1;
        const result = status(c.BkEditorDeletePlayer(from(ptr).session, player, &out));
        token.* = out;
        return result;
    }

    /// BkEditorUnitCreationChoices (05-05) in two passes, like `vtableListRmg`.
    fn vtableUnitCreationChoices(ptr: *anyopaque, kind: core.bridge.UcChoice, out: []core.bridge.UcName, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorUnitCreationChoices(self.session, @intFromEnum(kind), null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        total.* = @intCast(@max(count, 0));
        if (count <= 0) return .ok;
        if (out.len < total.*) return .refused;
        const all = std.heap.page_allocator.alloc(c.BkEditorUcName, @intCast(count)) catch return .failed;
        defer std.heap.page_allocator.free(all);
        var got: c_int = 0;
        const read = status(c.BkEditorUnitCreationChoices(self.session, @intFromEnum(kind), all.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (all, 0..) |item, i| {
            if (i >= out.len) break;
            out[i] = reinterpret(@TypeOf(out[i]), item);
        }
        return .ok;
    }

    fn toUnitCreation(record: c.BkEditorUnitCreationRecord) ?record_types.UnitCreation {
        if (record.slot_count < 0 or record.slot_count > record_types.max_uc_slots) return null;
        if (record.appear_count < 0 or record.appear_count > record_types.max_appear_points) return null;
        var unit: record_types.UnitCreation = .{
            .slot_count = @intCast(record.slot_count),
            .paratroop_count = record.paratroop_count,
            .relax_time = record.relax_time,
            .appear_count = @intCast(record.appear_count),
        };
        unit.setParty(std.mem.sliceTo(&record.party, 0));
        unit.setParatroop(std.mem.sliceTo(&record.paratroop_name, 0));
        for (&unit.aircraft, record.aircraft) |*slot, source| {
            slot.setName(std.mem.sliceTo(&source.name, 0));
            slot.formation_size = source.formation_size;
            slot.count = source.count;
        }
        for (0..unit.appear_count) |index| unit.appear[index] = toVec3(record.appear[index]);
        return unit;
    }

    fn toCUnitCreation(unit: record_types.UnitCreation) c.BkEditorUnitCreationRecord {
        var record: c.BkEditorUnitCreationRecord = std.mem.zeroes(c.BkEditorUnitCreationRecord);
        record.slot_count = @intCast(unit.slot_count);
        const party = unit.partySlice();
        @memcpy(record.party[0..party.len], party);
        const squad = unit.paratroopSlice();
        @memcpy(record.paratroop_name[0..squad.len], squad);
        record.paratroop_count = unit.paratroop_count;
        record.relax_time = unit.relax_time;
        for (&record.aircraft, unit.aircraft) |*slot, source| {
            const name = source.nameSlice();
            @memcpy(slot.name[0..name.len], name);
            slot.formation_size = source.formation_size;
            slot.count = source.count;
        }
        record.appear_count = @intCast(unit.appear_count);
        for (0..unit.appear_count) |index| record.appear[index] = toCVec3(unit.appear[index]);
        return record;
    }

    /// BkEditorReserveRole (04-11): the object type's role in a reserve position.
    fn vtableReserveRole(ptr: *anyopaque, name: []const u8, role: *i32) Status {
        var buffer: [core.bridge.name_capacity]u8 = undefined;
        const name_z = terminated(&buffer, name) orelse return .bad_argument;
        var out: c_int = 0;
        const result = status(c.BkEditorReserveRole(from(ptr).session, name_z, &out));
        if (result == .ok) role.* = out;
        return result;
    }

    fn toCReservePosition(position: record_types.ReservePosition) c.BkEditorReservePositionRecord {
        return .{ .artillery_link_id = position.artillery, .truck_link_id = position.truck, .x = position.x, .y = position.y };
    }

    fn readReservePosition(self: *RealBridge, key: i32, out: *record_types.Value) Status {
        var record: c.BkEditorReservePositionRecord = std.mem.zeroes(c.BkEditorReservePositionRecord);
        const result = status(c.BkEditorReservePosition(self.session, key, &record));
        if (result != .ok) return result;
        out.* = .{ .reserve_position = .{ .artillery = record.artillery_link_id, .truck = record.truck_link_id, .x = record.x, .y = record.y } };
        return .ok;
    }

    fn toCStartCommand(command: record_types.StartCommand) c.BkEditorStartCommandRecord {
        return .{
            .cmd_type = command.cmd_type,
            .link_id = command.link_id,
            .x = command.x,
            .y = command.y,
            .from_explosion = if (command.from_explosion) 1 else 0,
            .number = command.number,
            .unit_count = @intCast(command.units.len),
        };
    }

    /// One start command by its index: two passes, the units allocated with
    /// `allocator` (the value owns them). The sizing pass is REFUSED when the
    /// command has units and still answers the count.
    fn readStartCommand(self: *RealBridge, key: i32, allocator: std.mem.Allocator, out: *record_types.Value) Status {
        var record: c.BkEditorStartCommandRecord = std.mem.zeroes(c.BkEditorStartCommandRecord);
        // WR-C02: a REFUSED that wrote no count ("no map is open") is a refusal,
        // not the sizing pass: the sentinel tells the two apart.
        record.unit_count = -1;
        const sizing = status(c.BkEditorStartCommand(self.session, key, &record, null, 0));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (sizing == .refused and record.unit_count == -1) return .refused;
        if (record.unit_count < 0) return .failed;
        const units = allocator.alloc(i32, @intCast(record.unit_count)) catch return .failed;
        // WR-C01: an errdefer never runs here - this returns a Status, not an
        // error - so every non-OK exit frees through this flag instead.
        var handed_over = false;
        defer if (!handed_over) allocator.free(units);
        if (units.len != 0) {
            const read = status(c.BkEditorStartCommand(self.session, key, &record, units.ptr, record.unit_count));
            if (read != .ok) return read;
            if (@as(usize, @intCast(record.unit_count)) != units.len) return .failed;
        }
        handed_over = true;
        out.* = .{ .start_command = .{
            .cmd_type = record.cmd_type,
            .link_id = record.link_id,
            .x = record.x,
            .y = record.y,
            .from_explosion = record.from_explosion != 0,
            .number = record.number,
            .units = units,
        } };
        return .ok;
    }

    /// BkEditorAIGeneralSide as a core value (04-12): two passes - the sizing pass is
    /// REFUSED when the side holds anything (as a buffer too short is) and still answers
    /// the counts - the script IDs, parcels and points allocated with `allocator` (the
    /// value owns them). A side the map does not have reads empty with the side count.
    fn readAiSide(self: *RealBridge, key: i32, allocator: std.mem.Allocator, out: *record_types.Value) Status {
        var info: c.BkEditorAISideInfo = std.mem.zeroes(c.BkEditorAISideInfo);
        info.side_count = -1; // WR-C02: still -1 after a REFUSED that read nothing
        const sizing = status(c.BkEditorAIGeneralSide(self.session, key, &info, null, 0, null, 0, null, 0));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (sizing == .refused and info.side_count == -1) return .refused;
        if (info.side_count < 0 or info.mobile_count < 0 or info.parcel_count < 0 or info.point_count < 0) return .failed;
        const counted = info;
        const mobile = allocator.alloc(i32, @intCast(counted.mobile_count)) catch return .failed;
        // WR-C01: Status is not an error, so errdefer never runs; these flags
        // free what is not handed over on every non-OK exit.
        var handed_over = false;
        defer if (!handed_over) allocator.free(mobile);
        const c_parcels = std.heap.page_allocator.alloc(c.BkEditorAIParcel, @intCast(counted.parcel_count)) catch return .failed;
        defer std.heap.page_allocator.free(c_parcels);
        const c_points = std.heap.page_allocator.alloc(c.BkEditorAIPoint, @intCast(counted.point_count)) catch return .failed;
        defer std.heap.page_allocator.free(c_points);
        if (mobile.len != 0 or c_parcels.len != 0 or c_points.len != 0) {
            const read = status(c.BkEditorAIGeneralSide(
                self.session,
                key,
                &info,
                if (mobile.len != 0) mobile.ptr else null,
                counted.mobile_count,
                if (c_parcels.len != 0) c_parcels.ptr else null,
                counted.parcel_count,
                if (c_points.len != 0) c_points.ptr else null,
                counted.point_count,
            ));
            if (read != .ok) return read;
            if (info.mobile_count != counted.mobile_count or info.parcel_count != counted.parcel_count or info.point_count != counted.point_count) return .failed;
        }
        const parcels = allocator.alloc(record_types.Parcel, c_parcels.len) catch return .failed;
        var done: usize = 0;
        defer if (!handed_over) {
            for (parcels[0..done]) |parcel| allocator.free(parcel.points);
            allocator.free(parcels);
        };
        for (c_parcels, parcels) |source, *target| {
            if (source.first_point < 0 or source.point_count < 0) return .failed;
            const first: usize = @intCast(source.first_point);
            const count: usize = @intCast(source.point_count);
            if (first > c_points.len or count > c_points.len - first) return .failed;
            if (source.defence_dir < 0 or source.defence_dir > 65535) return .failed;
            const points = allocator.alloc(record_types.ParcelPoint, count) catch return .failed;
            for (c_points[first .. first + count], points) |point, *slot| {
                if (point.dir < 0 or point.dir > 65535) {
                    allocator.free(points);
                    return .failed;
                }
                slot.* = .{ .x = point.x, .y = point.y, .dir = @intCast(point.dir) };
            }
            target.* = .{
                .kind = @enumFromInt(source.type),
                .cx = source.cx,
                .cy = source.cy,
                .radius = source.radius,
                .defence_dir = @intCast(source.defence_dir),
                .points = points,
            };
            done += 1;
        }
        handed_over = true;
        out.* = .{ .ai_side = .{ .side = key, .side_count = @intCast(counted.side_count), .mobile_ids = mobile, .parcels = parcels } };
        return .ok;
    }

    /// BkEditorSetAIGeneralSide: the side flattened into the parcel and point arrays of
    /// the ABI (points in parcel order, each parcel naming its range), the side count
    /// with it.
    fn putAiSide(self: *RealBridge, key: i32, side: record_types.AiSide) Status {
        if (side.side != key or side.side_count > record_types.max_ai_sides) return .bad_argument;
        const total_points = side.pointCount();
        const c_parcels = std.heap.page_allocator.alloc(c.BkEditorAIParcel, side.parcels.len) catch return .failed;
        defer std.heap.page_allocator.free(c_parcels);
        const c_points = std.heap.page_allocator.alloc(c.BkEditorAIPoint, total_points) catch return .failed;
        defer std.heap.page_allocator.free(c_points);
        var first: usize = 0;
        for (side.parcels, c_parcels) |parcel, *target| {
            target.* = .{
                .type = @intFromEnum(parcel.kind),
                .cx = parcel.cx,
                .cy = parcel.cy,
                .radius = parcel.radius,
                .defence_dir = parcel.defence_dir,
                .first_point = @intCast(first),
                .point_count = @intCast(parcel.points.len),
            };
            for (parcel.points, c_points[first .. first + parcel.points.len]) |point, *slot| {
                slot.* = .{ .x = point.x, .y = point.y, .dir = point.dir };
            }
            first += parcel.points.len;
        }
        return status(c.BkEditorSetAIGeneralSide(
            self.session,
            key,
            @intCast(side.side_count),
            if (side.mobile_ids.len != 0) side.mobile_ids.ptr else null,
            @intCast(side.mobile_ids.len),
            if (c_parcels.len != 0) c_parcels.ptr else null,
            @intCast(c_parcels.len),
            if (c_points.len != 0) c_points.ptr else null,
            @intCast(c_points.len),
        ));
    }

    /// BkEditorGroup as a core value: two-pass, the script IDs allocated with
    /// `allocator` (the value owns them). The total comes back in `count` even
    /// when the buffer was too short, and -1 for a group that is not there.
    fn readGroup(self: *RealBridge, key: i32, allocator: std.mem.Allocator, out: *record_types.Value) Status {
        // WR-C02: -2 is "nothing written" (no map is open); -1 is the bridge's
        // "no such group"; either is a refusal, never an empty group.
        var count: c_int = -2;
        const sizing = status(c.BkEditorGroup(self.session, key, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .refused;
        const ids = allocator.alloc(i32, @intCast(count)) catch return .failed;
        // WR-C01: freed on every non-OK exit (errdefer never runs for a Status).
        var handed_over = false;
        defer if (!handed_over) allocator.free(ids);
        if (count > 0) {
            var got: c_int = 0;
            const read = status(c.BkEditorGroup(self.session, key, ids.ptr, count, &got));
            if (read != .ok) return read;
            if (got != count) return .failed;
        }
        handed_over = true;
        out.* = .{ .group = .{ .id = key, .ids = ids } };
        return .ok;
    }

    /// The generic record read: one switch arm per kind, each over its own C
    /// call. The camera anchors are a singleton, so `key` is 0; a group's key
    /// is its ID.
    fn vtableReadRecord(ptr: *anyopaque, kind: record_types.Kind, key: i32, allocator: std.mem.Allocator, out: *record_types.Value) Status {
        const self = from(ptr);
        switch (kind) {
            .camera_anchors => {
                if (key != 0) return .bad_argument;
                var record: c.BkEditorCameraAnchorRecord = std.mem.zeroes(c.BkEditorCameraAnchorRecord);
                const result = status(c.BkEditorCameraAnchors(self.session, &record));
                if (result != .ok) return result;
                out.* = .{ .camera_anchors = toCameraAnchors(record) orelse return .failed };
                return .ok;
            },
            .group => return self.readGroup(key, allocator, out),
            .script_area => return self.readScriptArea(key, allocator, out),
            .start_command => return self.readStartCommand(key, allocator, out),
            .reserve_position => return self.readReservePosition(key, out),
            .ai_side => return self.readAiSide(key, allocator, out),
            .unit_creation => {
                var record: c.BkEditorUnitCreationRecord = std.mem.zeroes(c.BkEditorUnitCreationRecord);
                const result = status(c.BkEditorUnitCreation(self.session, key, &record));
                if (result != .ok) return result;
                out.* = .{ .unit_creation = toUnitCreation(record) orelse return .failed };
                return .ok;
            },
            .script_file => {
                if (key != 0) return .bad_argument;
                var record: c.BkEditorScriptFileRecord = std.mem.zeroes(c.BkEditorScriptFileRecord);
                const result = status(c.BkEditorScriptFile(self.session, &record));
                if (result != .ok) return result;
                var value: record_types.ScriptFile = .{};
                value.setName(std.mem.sliceTo(&record.name, 0));
                out.* = .{ .script_file = value };
                return .ok;
            },
        }
    }

    fn vtablePutRecord(ptr: *anyopaque, key: i32, value: *const record_types.Value) Status {
        const self = from(ptr);
        switch (value.*) {
            .camera_anchors => |anchors| {
                if (key != 0) return .bad_argument;
                const record = toCCameraAnchors(anchors);
                return status(c.BkEditorSetCameraAnchors(self.session, &record));
            },
            .group => |group| {
                if (group.id != key) return .bad_argument;
                return status(c.BkEditorSetGroup(self.session, key, group.ids.ptr, @intCast(group.ids.len)));
            },
            .script_area => |area| {
                const record = toCScriptArea(area);
                return status(c.BkEditorSetScriptArea(self.session, key, &record));
            },
            .start_command => |command| {
                const record = toCStartCommand(command);
                return status(c.BkEditorSetStartCommand(self.session, key, &record, command.units.ptr));
            },
            .reserve_position => |position| {
                const record = toCReservePosition(position);
                return status(c.BkEditorSetReservePosition(self.session, key, &record));
            },
            .ai_side => |side| return self.putAiSide(key, side),
            .unit_creation => |unit| {
                const record = toCUnitCreation(unit);
                return status(c.BkEditorSetUnitCreation(self.session, key, &record));
            },
            .script_file => |file| {
                if (key != 0) return .bad_argument;
                var record: c.BkEditorScriptFileRecord = std.mem.zeroes(c.BkEditorScriptFileRecord);
                const name = file.nameSlice();
                @memcpy(record.name[0..name.len], name);
                return status(c.BkEditorSetScriptFile(self.session, &record));
            },
        }
    }

    fn vtableRecordKeys(ptr: *anyopaque, kind: record_types.Kind, allocator: std.mem.Allocator, out: *[]i32) Status {
        const self = from(ptr);
        switch (kind) {
            .script_area => {
                var count: c_int = -1; // WR-C02: still -1 after a REFUSED that read nothing
                const sizing = status(c.BkEditorScriptAreas(self.session, null, 0, &count));
                if (sizing != .ok and sizing != .refused) return sizing;
                if (sizing == .refused and count == -1) return .refused;
                if (count < 0) return .failed;
                const keys = allocator.alloc(i32, @intCast(count)) catch return .failed;
                for (keys, 0..) |*key, index| key.* = @intCast(index);
                out.* = keys;
                return .ok;
            },
            .start_command => {
                var count: c_int = 0;
                const counted = status(c.BkEditorStartCommandCount(self.session, &count));
                if (counted != .ok) return counted;
                if (count < 0) return .failed;
                const keys = allocator.alloc(i32, @intCast(count)) catch return .failed;
                for (keys, 0..) |*key, index| key.* = @intCast(index);
                out.* = keys;
                return .ok;
            },
            .reserve_position => {
                var count: c_int = 0;
                const counted = status(c.BkEditorReservePositionCount(self.session, &count));
                if (counted != .ok) return counted;
                if (count < 0) return .failed;
                const keys = allocator.alloc(i32, @intCast(count)) catch return .failed;
                for (keys, 0..) |*key, index| key.* = @intCast(index);
                out.* = keys;
                return .ok;
            },
            .unit_creation => return .bad_argument,
            .camera_anchors, .script_file => {
                const keys = allocator.alloc(i32, 1) catch return .failed;
                keys[0] = 0;
                out.* = keys;
                return .ok;
            },
            .ai_side => {
                // The sides the map has: the side count from any side's read.
                var info: c.BkEditorAISideInfo = std.mem.zeroes(c.BkEditorAISideInfo);
                info.side_count = -1; // WR-C02: still -1 after a REFUSED that read nothing
                const counted = status(c.BkEditorAIGeneralSide(self.session, 0, &info, null, 0, null, 0, null, 0));
                if (counted != .ok and counted != .refused) return counted;
                if (counted == .refused and info.side_count == -1) return .refused;
                if (info.side_count < 0) return .failed;
                const keys = allocator.alloc(i32, @intCast(info.side_count)) catch return .failed;
                for (keys, 0..) |*key, index| key.* = @intCast(index);
                out.* = keys;
                return .ok;
            },
            .group => {
                var count: c_int = -1; // WR-C02: still -1 after a REFUSED that read nothing
                const sizing = status(c.BkEditorGroupIDs(self.session, null, 0, &count));
                if (sizing != .ok and sizing != .refused) return sizing;
                if (sizing == .refused and count == -1) return .refused;
                if (count < 0) return .failed;
                const keys = allocator.alloc(i32, @intCast(count)) catch return .failed;
                // WR-C01: freed on every non-OK exit (errdefer never runs for a Status).
                var handed_over = false;
                defer if (!handed_over) allocator.free(keys);
                if (count > 0) {
                    var got: c_int = 0;
                    const read = status(c.BkEditorGroupIDs(self.session, keys.ptr, count, &got));
                    if (read != .ok) return read;
                    if (got != count) return .failed;
                }
                handed_over = true;
                out.* = keys;
                return .ok;
            },
        }
    }

    /// An insert is a put that must not replace: a group whose ID is taken is
    /// refused here, since BkEditorSetGroup creates or replaces.
    fn vtableInsertRecord(ptr: *anyopaque, key: i32, value: *const record_types.Value) Status {
        const self = from(ptr);
        self.own_message = null;
        switch (value.*) {
            .camera_anchors, .script_file, .ai_side, .unit_creation => return .bad_argument,
            .script_area => |area| {
                const record = toCScriptArea(area);
                return status(c.BkEditorAddScriptArea(self.session, key, &record));
            },
            .start_command => |command| {
                const record = toCStartCommand(command);
                return status(c.BkEditorAddStartCommand(self.session, key, &record, command.units.ptr));
            },
            .reserve_position => |position| {
                const record = toCReservePosition(position);
                return status(c.BkEditorAddReservePosition(self.session, key, &record));
            },
            .group => |group| {
                if (group.id != key or key < 0) return .bad_argument;
                // WR-C02: -2 is "nothing written" (no map is open: the probe's
                // own refusal, passed on); -1 is "no such group", the free ID.
                var count: c_int = -2;
                const probe = status(c.BkEditorGroup(self.session, key, null, 0, &count));
                if (probe != .ok and probe != .refused) return probe;
                if (count == -2) return if (probe == .ok) .failed else probe;
                if (count >= 0) {
                    self.own_message = "there is already a reinforcement group with that ID";
                    return .refused;
                }
                return status(c.BkEditorSetGroup(self.session, key, group.ids.ptr, @intCast(group.ids.len)));
            },
        }
    }

    fn vtableRemoveRecord(ptr: *anyopaque, kind: record_types.Kind, key: i32) Status {
        const self = from(ptr);
        switch (kind) {
            .camera_anchors, .script_file, .ai_side, .unit_creation => return .bad_argument,
            .script_area => return status(c.BkEditorDeleteScriptArea(self.session, key)),
            .start_command => return status(c.BkEditorDeleteStartCommand(self.session, key)),
            .reserve_position => return status(c.BkEditorDeleteReservePosition(self.session, key)),
            .group => return status(c.BkEditorDeleteGroup(self.session, key)),
        }
    }

    fn vtableFirstFreeGroupID(ptr: *anyopaque, from_id: i32, out: *i32) Status {
        var id: c_int = 0;
        const result = status(c.BkEditorFirstFreeGroupID(from(ptr).session, from_id, &id));
        if (result == .ok) out.* = id;
        return result;
    }

    fn vtableGroundHeight(ptr: *anyopaque, wx: f32, wy: f32, z: *f32) Status {
        return status(c.BkEditorGroundHeight(from(ptr).session, wx, wy, z));
    }

    fn vtableSetHiddenScriptIDs(ptr: *anyopaque, script_ids: []const i32) Status {
        return status(c.BkEditorSetHiddenScriptIDs(from(ptr).session, script_ids.ptr, @intCast(script_ids.len)));
    }

    fn setObjectScriptID(ptr: *anyopaque, link_id: i32, script_id: i32) Status {
        return status(c.BkEditorSetObjectScriptID(from(ptr).session, link_id, script_id));
    }

    fn vtableUndoEdit(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorUndoEdit(from(ptr).session, token));
    }

    fn vtableRedoEdit(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorRedoEdit(from(ptr).session, token));
    }

    fn toCAltitudeRegion(region: core.bridge.AltitudeRegion) c.BkEditorAltitudeRegion {
        return .{ .x0 = region.x0, .y0 = region.y0, .x1 = region.x1, .y1 = region.y1 };
    }

    /// BkEditorAltitudes in two passes, like `vtableSounds`. A sizing pass
    /// that answered REFUSED with no count is the refusal itself (no map,
    /// off the map) - a real sizing answer always carries the region's
    /// vertex count, which is never zero for a well-formed region.
    fn vtableAltitudes(ptr: *anyopaque, region: core.bridge.AltitudeRegion, heights: []f32, total: *usize) Status {
        const self = from(ptr);
        const c_region = toCAltitudeRegion(region);
        var count: c_int = 0;
        const sizing = status(c.BkEditorAltitudes(self.session, &c_region, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (sizing == .refused and count == 0) return .refused;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (heights.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        var got: c_int = 0;
        const read = status(c.BkEditorAltitudes(self.session, &c_region, heights.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        return .ok;
    }

    fn vtableSetAltitudes(ptr: *anyopaque, region: core.bridge.AltitudeRegion, heights: []const f32, token: *i32) Status {
        const count = std.math.cast(c_int, heights.len) orelse return .bad_argument;
        const c_region = toCAltitudeRegion(region);
        return status(c.BkEditorSetAltitudes(from(ptr).session, &c_region, heights.ptr, count, token));
    }

    /// BkEditorNewMap: the params are layout-identical (asserted above), so
    /// they pass straight through; the summary's fields land in the core's
    /// MapInfo exactly an open's own do.
    fn vtableNewMap(ptr: *anyopaque, params: core.bridge.NewMapParams, info: *MapInfo) Status {
        const self = from(ptr);
        const c_params: c.BkEditorNewMapParams = .{
            .size_x = params.size_x,
            .size_y = params.size_y,
            .season = params.season,
            .szName = params.name,
            .szModFolder = params.mod_folder,
        };
        var summary: c.BkEditorMapSummary = std.mem.zeroes(c.BkEditorMapSummary);
        const result = status(c.BkEditorNewMap(self.session, &c_params, &summary));
        if (result == .ok) info.* = .{
            .width_tiles = summary.width_tiles,
            .height_tiles = summary.height_tiles,
            .season = summary.season,
            .player_count = summary.player_count,
            .map_type = summary.map_type,
            .attacking_side = summary.attacking_side,
        };
        return result;
    }

    /// BkEditorHeightsStroke: the params are layout-identical (asserted
    /// above), so they pass straight through.
    fn vtableHeightsStroke(ptr: *anyopaque, params: core.bridge.HeightsStrokeParams, token: *i32) Status {
        const c_params: c.BkEditorHeightsStrokeParams = .{
            .action = params.action,
            .level_mode = params.level_mode,
            .brush = params.brush,
            .height_speed = params.height_speed,
            .level_ratio_percent = params.level_ratio_percent,
            .pos_x = params.pos_x,
            .pos_y = params.pos_y,
            .click_x = params.click_x,
            .click_y = params.click_y,
            .stroke_start = params.stroke_start,
            .ctrl_held = params.ctrl_held,
        };
        return status(c.BkEditorHeightsStroke(from(ptr).session, &c_params, token));
    }

    /// BkEditorGenerateHeights.
    fn vtableGenerateHeights(ptr: *anyopaque, gen_type: core.bridge.HeightsGenerateType, granularity: f32, min_z: f32, max_z: f32, token: *i32) Status {
        return status(c.BkEditorGenerateHeights(from(ptr).session, @intFromEnum(gen_type), granularity, min_z, max_z, token));
    }

    /// BkEditorSetZeroHeights.
    fn vtableSetZeroHeights(ptr: *anyopaque, token: *i32) Status {
        return status(c.BkEditorSetZeroHeights(from(ptr).session, token));
    }

    /// BkEditorUpdateMap: the progress pointer is C ABI on both sides
    /// (core.bridge.ProgressFn's callconv(.c) matches BkEditorProgressFn),
    /// so it passes straight through with the caller's user pointer.
    fn vtableUpdateMap(ptr: *anyopaque, progress: ?core.bridge.ProgressFn, user: ?*anyopaque, token: *i32) Status {
        const c_progress: c.BkEditorProgressFn = if (progress) |report| @ptrCast(report) else null;
        return status(c.BkEditorUpdateMap(from(ptr).session, c_progress, user, token));
    }

    /// BkEditorFillEntireMap.
    fn vtableFillEntireMap(ptr: *anyopaque, tile: u8, token: *i32) Status {
        return status(c.BkEditorFillEntireMap(from(ptr).session, @intCast(tile), token));
    }

    /// BkEditorSetTerrainModes.
    fn vtableSetTerrainModes(ptr: *anyopaque, instant_update: bool, fit_to_grid: bool) Status {
        return status(c.BkEditorSetTerrainModes(from(ptr).session, @intFromBool(instant_update), @intFromBool(fit_to_grid)));
    }

    /// BkEditorLayers (M3, D-32): two fixed-size words, no sizing pass.
    fn vtableLayers(ptr: *anyopaque, bits: *u32, mask: *u32) Status {
        var c_bits: c_uint = 0;
        var c_mask: c_uint = 0;
        const result = status(c.BkEditorLayers(from(ptr).session, &c_bits, &c_mask));
        bits.* = c_bits;
        mask.* = c_mask;
        return result;
    }

    /// BkEditorSetLayerShow.
    fn vtableSetLayerShow(ptr: *anyopaque, layer: u32, shown: bool) Status {
        return status(c.BkEditorSetLayerShow(from(ptr).session, @intCast(layer), @intFromBool(shown)));
    }

    /// BkEditorSetFireRangeMode: the filter name NUL-terminated (a name past
    /// the object filters' own 63-character rule cannot name a filter, so it is
    /// a bad argument here and never reaches the engine), the selection by
    /// pointer and count.
    fn vtableSetFireRangeMode(ptr: *anyopaque, mode: u32, filter: []const u8, link_ids: []const i32) Status {
        var buffer: [128]u8 = undefined;
        const filter_z = terminated(&buffer, filter) orelse return .bad_argument;
        const ids: ?[*]const c_int = if (link_ids.len == 0) null else @ptrCast(link_ids.ptr);
        return status(c.BkEditorSetFireRangeMode(from(ptr).session, @intCast(mode), filter_z, ids, @intCast(link_ids.len)));
    }

    /// BkEditorSnapToGrid.
    fn vtableSnapToGrid(ptr: *anyopaque, name: [*:0]const u8, x: f32, y: f32, out_x: *f32, out_y: *f32) Status {
        return status(c.BkEditorSnapToGrid(from(ptr).session, name, x, y, out_x, out_y));
    }

    fn kindInt(kind: VsoKind) c_int {
        return @intFromEnum(kind);
    }

    /// BkEditorVsoDescriptors in two passes, like `vtableSounds`.
    fn vtableVsoDescriptors(ptr: *anyopaque, kind: VsoKind, out: []VsoDescriptor, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorVsoDescriptors(self.session, kindInt(kind), null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        const names = std.heap.page_allocator.alloc(c.BkEditorVsoDescriptor, total.*) catch return .failed;
        defer std.heap.page_allocator.free(names);
        var got: c_int = 0;
        const read = status(c.BkEditorVsoDescriptors(self.session, kindInt(kind), names.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (names, out[0..names.len]) |name, *descriptor| descriptor.setName(std.mem.sliceTo(&name.name, 0));
        return .ok;
    }

    fn vtableVsoCount(ptr: *anyopaque, kind: VsoKind, count: *usize) Status {
        var value: c_int = 0;
        const result = status(c.BkEditorVsoCount(from(ptr).session, kindInt(kind), &value));
        if (result == .ok) count.* = if (value < 0) 0 else @intCast(value);
        return result;
    }

    /// BkEditorVso in two passes: the counts, then the arrays, converted into
    /// arrays the caller's allocator owns.
    fn vtableReadVso(ptr: *anyopaque, kind: VsoKind, index: i32, allocator: std.mem.Allocator, out: *VsoView) Status {
        const self = from(ptr);
        var info: c.BkEditorVsoInfo = std.mem.zeroes(c.BkEditorVsoInfo);
        const sizing = status(c.BkEditorVso(self.session, kindInt(kind), index, &info, null, 0, null, 0));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (info.control_count < 0 or info.key_count < 0) return .failed;
        const control_count: usize = @intCast(info.control_count);
        const key_count: usize = @intCast(info.key_count);
        const c_controls = std.heap.page_allocator.alloc(c.BkEditorVec3, @max(control_count, 1)) catch return .failed;
        defer std.heap.page_allocator.free(c_controls);
        const c_keys = std.heap.page_allocator.alloc(c.BkEditorVsoKeyPoint, @max(key_count, 1)) catch return .failed;
        defer std.heap.page_allocator.free(c_keys);
        const read = status(c.BkEditorVso(self.session, kindInt(kind), index, &info, c_controls.ptr, @intCast(c_controls.len), c_keys.ptr, @intCast(c_keys.len)));
        if (read != .ok) return read;
        if (info.control_count != control_count or info.key_count != key_count) return .failed;
        const controls = allocator.alloc(record_types.Vec3, control_count) catch return .failed;
        const keys = allocator.alloc(VsoKeyPoint, key_count) catch {
            allocator.free(controls);
            return .failed;
        };
        for (controls, c_controls[0..control_count]) |*point, c_point| point.* = toVec3(c_point);
        for (keys, c_keys[0..key_count]) |*key, c_key| key.* = .{
            .x = c_key.x,
            .y = c_key.y,
            .z = c_key.z,
            .nx = c_key.nx,
            .ny = c_key.ny,
            .nz = c_key.nz,
            .width = c_key.width,
            .opacity = c_key.opacity,
        };
        out.* = .{ .saved_id = info.saved_id, .control_points = controls, .key_points = keys };
        const desc = std.mem.sliceTo(&info.desc, 0);
        @memcpy(out.desc[0..desc.len], desc);
        return .ok;
    }

    fn vtableAddVso(ptr: *anyopaque, kind: VsoKind, desc: []const u8, points: []const record_types.Vec3, width_tiles: f32, opacity: f32, token: *i32, index: *i32) Status {
        const self = from(ptr);
        var desc_buffer: [core.bridge.vso_name_capacity]u8 = undefined;
        const desc_z = terminated(&desc_buffer, desc) orelse return .bad_argument;
        const count = std.math.cast(c_int, points.len) orelse return .bad_argument;
        const c_points = std.heap.page_allocator.alloc(c.BkEditorVec3, @max(points.len, 1)) catch return .failed;
        defer std.heap.page_allocator.free(c_points);
        for (points, c_points[0..points.len]) |point, *c_point| c_point.* = toCVec3(point);
        return status(c.BkEditorAddVso(self.session, kindInt(kind), desc_z, c_points.ptr, count, width_tiles, opacity, token, index));
    }

    fn vtableDeleteVso(ptr: *anyopaque, kind: VsoKind, index: i32, token: *i32) Status {
        return status(c.BkEditorDeleteVso(from(ptr).session, kindInt(kind), index, token));
    }

    fn vtableMoveVsoPoints(ptr: *anyopaque, kind: VsoKind, index: i32, points: []const record_types.Vec3, token: *i32) Status {
        const self = from(ptr);
        const count = std.math.cast(c_int, points.len) orelse return .bad_argument;
        const c_points = std.heap.page_allocator.alloc(c.BkEditorVec3, @max(points.len, 1)) catch return .failed;
        defer std.heap.page_allocator.free(c_points);
        for (points, c_points[0..points.len]) |point, *c_point| c_point.* = toCVec3(point);
        return status(c.BkEditorMoveVsoPoints(self.session, kindInt(kind), index, c_points.ptr, count, token));
    }

    fn vtableSetVsoWidth(ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, width: f32, mode: core.bridge.VsoWidthMode, token: *i32) Status {
        return status(c.BkEditorSetVsoWidth(from(ptr).session, kindInt(kind), index, key, width, @intFromEnum(mode), token));
    }

    fn vtableSetVsoOpacity(ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, opacity: f32, mode: core.bridge.VsoWidthMode, token: *i32) Status {
        return status(c.BkEditorSetVsoOpacity(from(ptr).session, kindInt(kind), index, key, opacity, @intFromEnum(mode), token));
    }

    fn vtableInsertVsoPoint(ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status {
        return status(c.BkEditorInsertVsoPoint(from(ptr).session, kindInt(kind), index, control, token));
    }

    fn vtableDeleteVsoPoint(ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status {
        return status(c.BkEditorDeleteVsoPoint(from(ptr).session, kindInt(kind), index, control, token));
    }

    fn vtablePickVso(ptr: *anyopaque, wx: f32, wy: f32, cycle: i32, kind: *VsoKind, index: *i32) Status {
        var c_kind: c_int = -1;
        const result = status(c.BkEditorPickVso(from(ptr).session, wx, wy, cycle, &c_kind, index));
        if (result == .ok) kind.* = std.enums.fromInt(VsoKind, c_kind) orelse return .failed;
        return result;
    }

    /// BkEditorBridgeDescriptors in two passes, like `vtableVsoDescriptors`.
    fn vtableBridgeDescriptors(ptr: *anyopaque, out: []core.bridge.BridgeDescriptor, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorBridgeDescriptors(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        const raw = std.heap.page_allocator.alloc(c.BkEditorBridgeDescriptor, total.*) catch return .failed;
        defer std.heap.page_allocator.free(raw);
        var got: c_int = 0;
        const read = status(c.BkEditorBridgeDescriptors(self.session, raw.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (raw, out[0..raw.len]) |item, *descriptor| {
            descriptor.* = .{
                .direction = if (item.direction == 0) .vertical else .horizontal,
                .has_partner = item.has_partner != 0,
                .build_during_play_allowed = item.build_during_play_allowed != 0,
            };
            descriptor.setName(std.mem.sliceTo(&item.name, 0));
        }
        return .ok;
    }

    /// BkEditorPlanBridge: `total` is always the planned count; the pieces
    /// that fit `out` are converted.
    fn vtablePlanBridge(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, out: []core.bridge.PlannedPiece, total: *usize) Status {
        const self = from(ptr);
        var desc_buffer: [core.bridge.name_capacity]u8 = undefined;
        const desc_z = terminated(&desc_buffer, desc) orelse return .bad_argument;
        var raw: [256]c.BkEditorPlannedPiece = undefined;
        const capacity = @min(out.len, raw.len);
        var count: c_int = 0;
        const result = status(c.BkEditorPlanBridge(self.session, desc_z, wx0, wy0, wx1, wy1, &raw, @intCast(capacity), &count));
        total.* = if (count < 0) 0 else @intCast(count);
        for (raw[0..@min(capacity, total.*)], out[0..@min(capacity, total.*)]) |piece, *planned| planned.* = .{ .x = piece.x, .y = piece.y, .type = piece.type, .dir = piece.dir };
        // A plan longer than this adapter's own buffer but not the caller's
        // is this adapter's shortfall, not the caller's.
        if (result == .refused and total.* > capacity and capacity < out.len) return .failed;
        return result;
    }

    fn vtableDrawBridge(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, token: *i32, index: *i32) Status {
        const self = from(ptr);
        var desc_buffer: [core.bridge.name_capacity]u8 = undefined;
        const desc_z = terminated(&desc_buffer, desc) orelse return .bad_argument;
        return status(c.BkEditorDrawBridge(self.session, desc_z, wx0, wy0, wx1, wy1, token, index));
    }

    /// BkEditorBridges in two passes.
    fn vtableBridges(ptr: *anyopaque, out: []core.bridge.BridgeInfo, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorBridges(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        const raw = std.heap.page_allocator.alloc(c.BkEditorBridgeInfo, total.*) catch return .failed;
        defer std.heap.page_allocator.free(raw);
        var got: c_int = 0;
        const read = status(c.BkEditorBridges(self.session, raw.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (raw, out[0..raw.len]) |item, *info| {
            info.* = .{
                .span_count = item.span_count,
                .min_x = item.min_x,
                .min_y = item.min_y,
                .max_x = item.max_x,
                .max_y = item.max_y,
                .built_during_play = item.built_during_play != 0,
            };
            info.setDesc(std.mem.sliceTo(&item.desc, 0));
        }
        return .ok;
    }

    fn vtablePickGroup(ptr: *anyopaque, sx: f32, sy: f32, kind: *core.bridge.GroupKind, index: *i32) Status {
        var c_kind: c_int = -1;
        const result = status(c.BkEditorPickGroup(from(ptr).session, sx, sy, &c_kind, index));
        if (result == .ok) kind.* = std.enums.fromInt(core.bridge.GroupKind, c_kind) orelse return .failed;
        return result;
    }

    fn vtableDeleteBridge(ptr: *anyopaque, index: i32, token: *i32) Status {
        return status(c.BkEditorDeleteBridge(from(ptr).session, index, token));
    }

    fn vtableRotateBridge(ptr: *anyopaque, index: i32, token: *i32) Status {
        return status(c.BkEditorRotateBridge(from(ptr).session, index, token));
    }

    fn vtableToggleBridgeBuild(ptr: *anyopaque, index: i32, token: *i32) Status {
        return status(c.BkEditorToggleBridgeBuild(from(ptr).session, index, token));
    }

    /// BkEditorFenceDescriptors in two passes.
    fn vtableFenceDescriptors(ptr: *anyopaque, out: []core.bridge.FenceDescriptor, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorFenceDescriptors(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        const raw = std.heap.page_allocator.alloc(c.BkEditorFenceDescriptor, total.*) catch return .failed;
        defer std.heap.page_allocator.free(raw);
        var got: c_int = 0;
        const read = status(c.BkEditorFenceDescriptors(self.session, raw.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (raw, out[0..raw.len]) |item, *descriptor| {
            descriptor.* = .{};
            descriptor.setName(std.mem.sliceTo(&item.name, 0));
        }
        return .ok;
    }

    /// BkEditorPlanFences in two passes (a run has hundreds of fences, more
    /// than a bridge's stack buffer): `total` is always the planned count.
    fn vtablePlanFences(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, out: []core.bridge.PlannedPiece, total: *usize) Status {
        const self = from(ptr);
        var desc_buffer: [core.bridge.name_capacity]u8 = undefined;
        const desc_z = terminated(&desc_buffer, desc) orelse return .bad_argument;
        const flag: c_int = if (ctrl) 1 else 0;
        var count: c_int = 0;
        const sizing = status(c.BkEditorPlanFences(self.session, desc_z, wx0, wy0, wx1, wy1, flag, null, 0, &count));
        total.* = if (count < 0) 0 else @intCast(count);
        // A refusal with nothing planned is the plan's (a bad type, off the map).
        if (sizing == .ok) return .ok;
        if (sizing != .refused or count <= 0) return sizing;
        if (out.len < total.*) return .refused;
        const raw = std.heap.page_allocator.alloc(c.BkEditorPlannedPiece, total.*) catch return .failed;
        defer std.heap.page_allocator.free(raw);
        var got: c_int = 0;
        const read = status(c.BkEditorPlanFences(self.session, desc_z, wx0, wy0, wx1, wy1, flag, raw.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (raw, out[0..raw.len]) |piece, *planned| planned.* = .{ .x = piece.x, .y = piece.y, .type = piece.type, .dir = piece.dir };
        return .ok;
    }

    fn vtableDrawFences(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, token: *i32) Status {
        const self = from(ptr);
        var desc_buffer: [core.bridge.name_capacity]u8 = undefined;
        const desc_z = terminated(&desc_buffer, desc) orelse return .bad_argument;
        return status(c.BkEditorDrawFences(self.session, desc_z, wx0, wy0, wx1, wy1, if (ctrl) 1 else 0, token));
    }

    /// The clicks as the C call takes them; null for more than the bridge
    /// takes (it answers BK_EDITOR_BAD_ARGUMENT for over 256 anyway).
    fn trenchPoints(points: []const record_types.Vec3, out: *[257]c.BkEditorVec3) ?[]const c.BkEditorVec3 {
        if (points.len > out.len) return null;
        for (points, out[0..points.len]) |point, *c_point| c_point.* = toCVec3(point);
        return out[0..points.len];
    }

    /// BkEditorPlanEntrenchment in two passes (a long trench has hundreds of
    /// pieces): `total` is always the planned count.
    fn vtablePlanEntrenchment(ptr: *anyopaque, points: []const record_types.Vec3, out: []core.bridge.PlannedPiece, total: *usize) Status {
        const self = from(ptr);
        var buffer: [257]c.BkEditorVec3 = undefined;
        const c_points = trenchPoints(points, &buffer) orelse return .bad_argument;
        const points_ptr: [*c]const c.BkEditorVec3 = if (c_points.len == 0) null else c_points.ptr;
        var count: c_int = 0;
        const sizing = status(c.BkEditorPlanEntrenchment(self.session, points_ptr, @intCast(c_points.len), null, 0, &count));
        total.* = if (count < 0) 0 else @intCast(count);
        // A refusal with nothing planned is the plan's (too short, off the map).
        if (sizing == .ok) return .ok;
        if (sizing != .refused or count <= 0) return sizing;
        if (out.len < total.*) return .refused;
        const raw = std.heap.page_allocator.alloc(c.BkEditorPlannedPiece, total.*) catch return .failed;
        defer std.heap.page_allocator.free(raw);
        var got: c_int = 0;
        const read = status(c.BkEditorPlanEntrenchment(self.session, points_ptr, @intCast(c_points.len), raw.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (raw, out[0..raw.len]) |piece, *planned| planned.* = .{ .x = piece.x, .y = piece.y, .type = piece.type, .dir = piece.dir };
        return .ok;
    }

    fn vtableDrawEntrenchment(ptr: *anyopaque, points: []const record_types.Vec3, player: i32, token: *i32, index: *i32) Status {
        const self = from(ptr);
        var buffer: [257]c.BkEditorVec3 = undefined;
        const c_points = trenchPoints(points, &buffer) orelse return .bad_argument;
        const points_ptr: [*c]const c.BkEditorVec3 = if (c_points.len == 0) null else c_points.ptr;
        return status(c.BkEditorDrawEntrenchment(self.session, points_ptr, @intCast(c_points.len), player, token, index));
    }

    fn vtableDeleteEntrenchment(ptr: *anyopaque, index: i32, token: *i32) Status {
        return status(c.BkEditorDeleteEntrenchment(from(ptr).session, index, token));
    }

    /// BkEditorEntrenchments in two passes.
    fn vtableEntrenchments(ptr: *anyopaque, out: []core.bridge.EntrenchmentInfo, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        const sizing = status(c.BkEditorEntrenchments(self.session, null, 0, &count));
        if (sizing != .ok and sizing != .refused) return sizing;
        if (count < 0) return .failed;
        total.* = @intCast(count);
        if (out.len < total.*) return .refused;
        if (total.* == 0) return .ok;
        const raw = std.heap.page_allocator.alloc(c.BkEditorEntrenchmentInfo, total.*) catch return .failed;
        defer std.heap.page_allocator.free(raw);
        var got: c_int = 0;
        const read = status(c.BkEditorEntrenchments(self.session, raw.ptr, count, &got));
        if (read != .ok) return read;
        if (got != count) return .failed;
        for (raw, out[0..raw.len]) |item, *info| info.* = .{
            .piece_count = item.piece_count,
            .section_count = item.section_count,
            .player = item.player,
            .min_x = item.min_x,
            .min_y = item.min_y,
            .max_x = item.max_x,
            .max_y = item.max_y,
        };
        return .ok;
    }

    /// The engine tier's road and river agreement check.
    pub fn vsoMatchesEngine(self: *RealBridge) Status {
        return status(c.BkEditorVsoMatchesEngine(self.session));
    }

    pub fn setCamera(self: *RealBridge, wx: f32, wy: f32) Status {
        return status(c.BkEditorSetCamera(self.session, wx, wy));
    }

    /// The bridge's view: the anchor, the zoom step (already clamped to the
    /// window's current maximum) and the scale it draws at. Null on any
    /// refusal (the engine is not started).
    pub fn viewState(self: *RealBridge) ?c.BkEditorView {
        var out: c.BkEditorView = std.mem.zeroes(c.BkEditorView);
        if (c.BkEditorViewState(self.session, &out) != c.BK_EDITOR_OK) return null;
        return out;
    }

    /// Zooms by `steps` (positive in, negative out) anchored at the screen
    /// point (sx, sy) - Shift+wheel/swipe and pinch both go through this.
    pub fn zoomAt(self: *RealBridge, steps: i32, sx: f32, sy: f32) Status {
        return status(c.BkEditorZoomAt(self.session, steps, sx, sy));
    }

    /// The same recipe with an absolute step count, anchored at the screen's
    /// centre: Home/Reset view and restoring a remembered view.
    pub fn setZoom(self: *RealBridge, steps: i32) Status {
        return status(c.BkEditorSetZoom(self.session, steps));
    }

    /// The other direction of `screenToWorld`: a world point to the screen
    /// point it draws at now, at whatever zoom is set. Null on any refusal.
    pub fn worldToScreen(self: *RealBridge, wx: f32, wy: f32) ?[2]f32 {
        var sx: f32 = 0;
        var sy: f32 = 0;
        if (c.BkEditorWorldToScreen(self.session, wx, wy, &sx, &sy) != c.BK_EDITOR_OK) return null;
        return .{ sx, sy };
    }

    pub fn screenSize(self: *RealBridge) ?[2]i32 {
        var width: c_int = 0;
        var height: c_int = 0;
        if (c.BkEditorScreenSize(self.session, &width, &height) != c.BK_EDITOR_OK) return null;
        return .{ width, height };
    }

    /// The object database, for the palette. Caller frees.
    pub fn catalogue(self: *RealBridge, allocator: std.mem.Allocator) ![]c.BkEditorCatalogueEntry {
        var count: c_int = 0;
        const sizing = c.BkEditorCatalogue(self.session, null, 0, &count);
        if (sizing != c.BK_EDITOR_OK and sizing != c.BK_EDITOR_REFUSED) return error.CatalogueFailed;
        if (count < 0) return error.CatalogueFailed;
        const entries = try allocator.alloc(c.BkEditorCatalogueEntry, @intCast(count));
        errdefer allocator.free(entries);
        if (count == 0) return entries;
        if (c.BkEditorCatalogue(self.session, entries.ptr, count, &count) != c.BK_EDITOR_OK) return error.CatalogueFailed;
        if (count != entries.len) return error.CatalogueFailed;
        return entries;
    }

    /// D-29: one object's own picture (its icon.tga, decoded and scaled by
    /// the engine - BkEditorObjectPicture), for the palette. `buffer` must
    /// hold at least `max_side * max_side * 4` bytes (the worst case, no
    /// scaling below that); the returned `bytes` is only the decoded
    /// width*height*4 prefix of it. Null on any refusal (no such picture,
    /// an unknown name, or `buffer` too short for the real size) - the
    /// caller has no use for a short-buffer retry here, unlike `catalogue`'s
    /// two-call convention, because `max_side` already bounds the size.
    pub fn objectPicture(self: *RealBridge, name: []const u8, buffer: []u8, max_side: i32) ?Picture {
        var name_buffer: [core.bridge.name_capacity]u8 = undefined;
        const z = terminated(&name_buffer, name) orelse return null;
        var width: c_int = 0;
        var height: c_int = 0;
        const capacity = std.math.cast(c_int, buffer.len) orelse return null;
        if (c.BkEditorObjectPicture(self.session, z, buffer.ptr, capacity, max_side, &width, &height) != c.BK_EDITOR_OK) return null;
        if (width <= 0 or height <= 0) return null;
        const needed: usize = @as(usize, @intCast(width)) * @as(usize, @intCast(height)) * 4;
        if (needed > buffer.len) return null;
        return .{ .width = width, .height = height, .bytes = buffer[0..needed] };
    }

    /// 03-15 gap fix: one tile's picture for the Brush's picker (its diamond
    /// out of the tileset texture - BkEditorTilePicture), the same buffer
    /// contract as `objectPicture`. Null on any refusal (no map open, a tile
    /// the tileset does not list, a texture that will not load).
    pub fn tilePicture(self: *RealBridge, tile: u8, buffer: []u8, max_side: i32) ?Picture {
        var width: c_int = 0;
        var height: c_int = 0;
        const capacity = std.math.cast(c_int, buffer.len) orelse return null;
        if (c.BkEditorTilePicture(self.session, tile, buffer.ptr, capacity, max_side, &width, &height) != c.BK_EDITOR_OK) return null;
        if (width <= 0 or height <= 0) return null;
        const needed: usize = @as(usize, @intCast(width)) * @as(usize, @intCast(height)) * 4;
        if (needed > buffer.len) return null;
        return .{ .width = width, .height = height, .bytes = buffer[0..needed] };
    }

    /// 05-07, D-16: the map's own minimap picture (`<map>.tga`, else `<map>_h.dds`)
    /// decoded by the engine, same contract as `tilePicture`: RGBA8 in the
    /// caller's buffer, scaled down to `max_side` (8..2048) only when bigger. Null
    /// when the map has none, it will not decode, or the buffer is too small.
    pub fn minimapImage(self: *RealBridge, map_path: []const u8, buffer: []u8, max_side: i32) ?Picture {
        var path_buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&path_buffer, map_path) orelse return null;
        var width: c_int = 0;
        var height: c_int = 0;
        const capacity = std.math.cast(c_int, buffer.len) orelse return null;
        if (c.BkEditorMinimapImage(self.session, z, buffer.ptr, capacity, max_side, &width, &height) != c.BK_EDITOR_OK) return null;
        if (width <= 0 or height <= 0) return null;
        const needed: usize = @as(usize, @intCast(width)) * @as(usize, @intCast(height)) * 4;
        if (needed > buffer.len) return null;
        return .{ .width = width, .height = height, .bytes = buffer[0..needed] };
    }

    /// 03-15 gap fix: the terrain type a tile belongs to and the tileset's
    /// storage name (BkEditorDescribeTile), for the picker's sections and
    /// its per-tileset picture cache. Null on any refusal.
    pub fn describeTile(self: *RealBridge, tile: u8) ?c.BkEditorTile {
        var out: c.BkEditorTile = std.mem.zeroes(c.BkEditorTile);
        if (c.BkEditorDescribeTile(self.session, tile, &out) != c.BK_EDITOR_OK) return null;
        return out;
    }

    /// File > Close (03-15 gap fix): closes the engine's map
    /// (BkEditorCloseMap); OK with none open. The document is the caller's
    /// to close (`panels_logic.closeMapAndDocument`).
    pub fn closeMap(self: *RealBridge) Status {
        return status(c.BkEditorCloseMap(self.session));
    }

    /// The tiles the open map's tileset has, ascending, for the brush's
    /// palette: a tile is an unsigned char, so 256 always holds them all.
    /// Null when no map is open or the bridge would not say.
    pub fn tilesetTiles(self: *RealBridge, out: *[256]u8) ?[]const u8 {
        var count: c_int = 0;
        if (c.BkEditorTilesetTiles(self.session, out, out.len, &count) != c.BK_EDITOR_OK) return null;
        if (count < 0 or count > out.len) return null;
        return out[0..@intCast(count)];
    }

    /// The engine's two host roots (BaseRoot, UserRoot), for Test in game's
    /// log and window placement. Null on any refusal (the engine is not
    /// started, or a root does not fit) - see BkEditorPaths' doc comment.
    pub fn paths(self: *RealBridge, out: *c.BkEditorPathSet) Status {
        return status(c.BkEditorPaths(self.session, out));
    }

    /// Where a test-launch copy of the current map goes (D-01, D-02, D-09):
    /// out is written and returned as the slice up to the NUL
    /// BkEditorTestMapPath left in it; null on any refusal, in which case
    /// nothing was created and the profile/mod/file_name arguments (or the
    /// buffer) are why - BkEditorLastMessage has the reason.
    pub fn testMapPath(self: *RealBridge, profile: []const u8, mod_folder: ?[]const u8, file_name: []const u8, out: []u8) ?[]const u8 {
        var profile_buffer: [256]u8 = undefined;
        const profile_z = terminated(&profile_buffer, profile) orelse return null;
        var mod_buffer: [256]u8 = undefined;
        const mod_z: ?[*:0]const u8 = if (mod_folder) |folder| (terminated(&mod_buffer, folder) orelse return null) else null;
        var name_buffer: [128]u8 = undefined;
        const name_z = terminated(&name_buffer, file_name) orelse return null;
        const capacity = std.math.cast(c_int, out.len) orelse return null;
        if (c.BkEditorTestMapPath(self.session, profile_z, mod_z, name_z, out.ptr, capacity) != c.BK_EDITOR_OK) return null;
        return std.mem.sliceTo(out, 0);
    }

    /// BkEditorSaveMap without touching the Editor: no path change, no
    /// markClean (D-01 - Test in game must never mark the document saved or
    /// move its path, only the engine's map file it wrote a copy of).
    pub fn saveCopy(self: *RealBridge, engine_path: []const u8) Status {
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, engine_path) orelse return .bad_argument;
        return status(c.BkEditorSaveMap(self.session, z));
    }

    /// Every installed mod (D-26), sorted by folder as the bridge lists them.
    /// Caller frees.
    pub fn mods(self: *RealBridge, allocator: std.mem.Allocator) ![]c.BkEditorMod {
        var count: c_int = 0;
        const sizing = c.BkEditorMods(self.session, null, 0, &count);
        if (sizing != c.BK_EDITOR_OK and sizing != c.BK_EDITOR_REFUSED) return error.ModsFailed;
        if (count < 0) return error.ModsFailed;
        const entries = try allocator.alloc(c.BkEditorMod, @intCast(count));
        errdefer allocator.free(entries);
        if (count == 0) return entries;
        if (c.BkEditorMods(self.session, entries.ptr, count, &count) != c.BK_EDITOR_OK) return error.ModsFailed;
        if (count != entries.len) return error.ModsFailed;
        return entries;
    }

    /// Switches the active mod (D-26, D-09): null or "" clears it (the base
    /// game). `.refused` names an installed mod that was not found (or has no
    /// mod.xml); `.bad_argument` a folder name that is not bare (a separator,
    /// ".", ".." or too long). Either way nothing changed - see bridge.h's own
    /// BkEditorSetMod comment.
    pub fn setMod(self: *RealBridge, folder: ?[]const u8) Status {
        var buffer: [64]u8 = undefined;
        const z: ?[*:0]const u8 = if (folder) |f| (terminated(&buffer, f) orelse return .bad_argument) else null;
        return status(c.BkEditorSetMod(self.session, z));
    }

    /// The session's active mod, or null when none is active.
    pub fn activeMod(self: *RealBridge) ?c.BkEditorMod {
        var out: c.BkEditorMod = std.mem.zeroes(c.BkEditorMod);
        if (c.BkEditorActiveMod(self.session, &out) != c.BK_EDITOR_OK) return null;
        if (out.folder[0] == 0) return null;
        return out;
    }

    /// The engine's SDL_GPUDevice (BkEditorGpuDevice), for building the
    /// palette's picture textures against the device that will draw them -
    /// see pictures.zig. Null when the renderer has none.
    pub fn gpuDevice(self: *RealBridge) ?*anyopaque {
        var device: ?*anyopaque = null;
        var format: c_uint = 0;
        if (c.BkEditorGpuDevice(self.session, &device, &format) != c.BK_EDITOR_OK) return null;
        return device;
    }

    /// The engine tier's two agreement checks, for tests.
    pub fn engineMatches(self: *RealBridge) Status {
        const terrain = status(c.BkEditorTerrainMatchesEngine(self.session));
        if (terrain != .ok) return terrain;
        return status(c.BkEditorWorldMatchesMap(self.session));
    }
};
