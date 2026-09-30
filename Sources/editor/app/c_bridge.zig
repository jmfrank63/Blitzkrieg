//! The core's Bridge over the real C ABI (Sources/src/EditorBridge/bridge.h).
//! The core is std-only and runs on every target; this file is where it meets
//! the engine, so it lives in the app. Statuses map one to one, strings are
//! copied into NUL-terminated buffers on the way in, and the bridge's last
//! message is copied out, because the bridge's pointer is valid only until
//! its next call.
const std = @import("std");
const core = @import("editor_core");
pub const c = @cImport(@cInclude("bridge.h"));

const Status = core.bridge.Status;
const Bridge = core.bridge.Bridge;
const MapInfo = core.bridge.MapInfo;
const ObjectRecord = core.bridge.ObjectRecord;
const SoundRecord = core.bridge.SoundRecord;
const record_types = core.records;
const PaintCell = core.bridge.PaintCell;
const VsoKind = core.bridge.VsoKind;
const VsoDescriptor = core.bridge.VsoDescriptor;
const VsoKeyPoint = core.bridge.VsoKeyPoint;
const VsoView = core.bridge.VsoView;

comptime {
    // The core's PaintCell is handed to BkEditorPaint as it is.
    std.debug.assert(@sizeOf(PaintCell) == @sizeOf(c.BkEditorPaintCell));
    std.debug.assert(@offsetOf(PaintCell, "x") == @offsetOf(c.BkEditorPaintCell, "x"));
    std.debug.assert(@offsetOf(PaintCell, "y") == @offsetOf(c.BkEditorPaintCell, "y"));
    std.debug.assert(@offsetOf(PaintCell, "tile") == @offsetOf(c.BkEditorPaintCell, "tile"));
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
        .sounds = vtableSounds,
        .addSound = vtableAddSound,
        .setSound = vtableSetSound,
        .deleteSound = vtableDeleteSound,
        .readRecord = vtableReadRecord,
        .putRecord = vtablePutRecord,
        .groundHeight = vtableGroundHeight,
        .undoEdit = vtableUndoEdit,
        .redoEdit = vtableRedoEdit,
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
    };

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        const text = std.mem.span(c.BkEditorLastMessage(self.session));
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
        };
        object.setName(std.mem.sliceTo(&record.name, 0));
        return object;
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

    /// The generic record read: one switch arm per kind, each over its own C
    /// call. The camera anchors are a singleton, so `key` is 0.
    fn vtableReadRecord(ptr: *anyopaque, kind: record_types.Kind, key: i32, allocator: std.mem.Allocator, out: *record_types.Value) Status {
        _ = allocator; // the kinds so far own no memory
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
        }
    }

    fn vtableGroundHeight(ptr: *anyopaque, wx: f32, wy: f32, z: *f32) Status {
        return status(c.BkEditorGroundHeight(from(ptr).session, wx, wy, z));
    }

    fn vtableUndoEdit(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorUndoEdit(from(ptr).session, token));
    }

    fn vtableRedoEdit(ptr: *anyopaque, token: i32) Status {
        return status(c.BkEditorRedoEdit(from(ptr).session, token));
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
