//! The core's view of the engine bridge (Sources/src/EditorBridge/bridge.h).
//! One call per C entry point, same arguments, same statuses, so the fake and
//! the real adapter (plan 5) can be read side by side. The core never sees a
//! C type: this file is plain Zig so the core builds on every target,
//! including x86_64-windows-gnu, where the engine C++ does not.
const std = @import("std");
const records = @import("records.zig");

pub const Status = enum(c_int) {
    ok = 0,
    bad_argument = 1,
    no_session = 2,
    no_device = 3,
    data_missing = 4,
    refused = 5,
    failed = 6,
};

/// What a command sees of a status: a refusal is an ordinary answer with a
/// message for the status bar, anything else is a failure.
pub const EditError = error{ Refused, Failed, OutOfMemory };

pub fn check(status: Status) EditError!void {
    return switch (status) {
        .ok => {},
        .refused => error.Refused,
        else => error.Failed,
    };
}

pub const MapInfo = struct {
    width_tiles: i32 = 0,
    height_tiles: i32 = 0,
    season: i32 = 0,
    player_count: i32 = 0,
    map_type: i32 = 0,
    attacking_side: i32 = 0,
};

pub const name_capacity = 64;

/// BkEditorObjectRecord.
pub const ObjectRecord = struct {
    link_id: i32 = -1,
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    x: f32 = 0,
    y: f32 = 0,
    dir: i32 = 0,
    player: i32 = 0,
    scenario: bool = false,
    known: bool = true,

    pub fn nameSlice(self: *const ObjectRecord) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *ObjectRecord, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// BkEditorPaintCell, layout included: the adapter hands a slice of these
/// straight to the C call.
pub const PaintCell = extern struct { x: c_int, y: c_int, tile: u8 };

/// BkEditorSoundRecord. Positions are world (scene) units, not map units
/// (bridge.h's own comment on BkEditorSounds); radii are vis tiles; times
/// are milliseconds.
pub const SoundRecord = struct {
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    x: f32 = 0,
    y: f32 = 0,
    z: f32 = 0,
    repeat_ms: i32 = 0,
    repeat_random_ms: i32 = 0,
    mute_in_combat: bool = false,
    min_radius: i32 = 0,
    max_radius: i32 = 0,

    pub fn nameSlice(self: *const SoundRecord) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *SoundRecord, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

pub const Bridge = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        lastMessage: *const fn (ptr: *anyopaque) []const u8,
        openMap: *const fn (ptr: *anyopaque, path: []const u8, info: *MapInfo) Status,
        saveMap: *const fn (ptr: *anyopaque, path: []const u8) Status,
        objects: *const fn (ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status,
        diplomacy: *const fn (ptr: *anyopaque, player: i32, value: *i32) Status,
        addObject: *const fn (ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status,
        placeObject: *const fn (ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status,
        deleteObject: *const fn (ptr: *anyopaque, link_id: i32) Status,
        restoreObject: *const fn (ptr: *anyopaque, link_id: i32) Status,
        setDiplomacy: *const fn (ptr: *anyopaque, player: i32, value: i32) Status,
        setMapType: *const fn (ptr: *anyopaque, value: i32) Status,
        setAttackingSide: *const fn (ptr: *anyopaque, value: i32) Status,
        paint: *const fn (ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status,
        undoPaint: *const fn (ptr: *anyopaque, token: i32) Status,
        redoPaint: *const fn (ptr: *anyopaque, token: i32) Status,
        screenToWorld: *const fn (ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status,
        worldToTile: *const fn (ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status,
        /// World units (the camera's, screenToWorld's) to map units (every
        /// object position's). BkEditorWorldToMap.
        worldToMap: *const fn (ptr: *anyopaque, wx: f32, wy: f32, mx: *f32, my: *f32) Status,
        objectAt: *const fn (ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status,
        /// BkEditorSounds. Like `objects`: `total` is always the full count,
        /// so a caller sizes `out` from a first sizing call the way
        /// `document.reload` does for `objects`.
        sounds: *const fn (ptr: *anyopaque, out: []SoundRecord, total: *usize) Status,
        /// BkEditorAddSound. `index` is 0..count to insert there, -1 to
        /// append - the C call's own sentinel, kept as-is rather than an
        /// optional, since the core passes it straight through.
        addSound: *const fn (ptr: *anyopaque, index: i32, record: SoundRecord) Status,
        /// BkEditorSetSound.
        setSound: *const fn (ptr: *anyopaque, index: i32, record: SoundRecord) Status,
        /// BkEditorDeleteSound.
        deleteSound: *const fn (ptr: *anyopaque, index: i32) Status,
        /// Reads the whole record of `kind` at `key` (an index or ID; 0 for a
        /// singleton) into `out`, allocating any owned memory with `allocator`.
        /// The camera anchors: BkEditorCameraAnchors, world units.
        readRecord: *const fn (ptr: *anyopaque, kind: records.Kind, key: i32, allocator: std.mem.Allocator, out: *records.Value) Status,
        /// A raw put of the whole record at `key`, the kind taken from the
        /// union tag: the same call an edit, an undo and a redo all use. The
        /// camera anchors: BkEditorSetCameraAnchors, an exact put (the vector
        /// becomes exactly `player_count` long), world units.
        putRecord: *const fn (ptr: *anyopaque, key: i32, value: *const records.Value) Status,
        /// BkEditorGroundHeight: the terrain height at a world point, world
        /// units in and out. Refused off the map.
        groundHeight: *const fn (ptr: *anyopaque, wx: f32, wy: f32, z: *f32) Status,
    };

    pub fn lastMessage(self: Bridge) []const u8 { return self.vtable.lastMessage(self.ptr); }
    pub fn openMap(self: Bridge, path: []const u8, info: *MapInfo) Status { return self.vtable.openMap(self.ptr, path, info); }
    pub fn saveMap(self: Bridge, path: []const u8) Status { return self.vtable.saveMap(self.ptr, path); }
    pub fn objects(self: Bridge, out: []ObjectRecord, total: *usize) Status { return self.vtable.objects(self.ptr, out, total); }
    pub fn diplomacy(self: Bridge, player: i32, value: *i32) Status { return self.vtable.diplomacy(self.ptr, player, value); }
    pub fn addObject(self: Bridge, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status { return self.vtable.addObject(self.ptr, name, x, y, dir, player, link_id); }
    pub fn placeObject(self: Bridge, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status { return self.vtable.placeObject(self.ptr, link_id, x, y, dir, player); }
    pub fn deleteObject(self: Bridge, link_id: i32) Status { return self.vtable.deleteObject(self.ptr, link_id); }
    pub fn restoreObject(self: Bridge, link_id: i32) Status { return self.vtable.restoreObject(self.ptr, link_id); }
    pub fn setDiplomacy(self: Bridge, player: i32, value: i32) Status { return self.vtable.setDiplomacy(self.ptr, player, value); }
    pub fn setMapType(self: Bridge, value: i32) Status { return self.vtable.setMapType(self.ptr, value); }
    pub fn setAttackingSide(self: Bridge, value: i32) Status { return self.vtable.setAttackingSide(self.ptr, value); }
    pub fn paint(self: Bridge, cells: []const PaintCell, token: *i32) Status { return self.vtable.paint(self.ptr, cells, token); }
    pub fn undoPaint(self: Bridge, token: i32) Status { return self.vtable.undoPaint(self.ptr, token); }
    pub fn redoPaint(self: Bridge, token: i32) Status { return self.vtable.redoPaint(self.ptr, token); }
    pub fn screenToWorld(self: Bridge, sx: f32, sy: f32, wx: *f32, wy: *f32) Status { return self.vtable.screenToWorld(self.ptr, sx, sy, wx, wy); }
    pub fn worldToTile(self: Bridge, wx: f32, wy: f32, tx: *i32, ty: *i32) Status { return self.vtable.worldToTile(self.ptr, wx, wy, tx, ty); }
    pub fn worldToMap(self: Bridge, wx: f32, wy: f32, mx: *f32, my: *f32) Status { return self.vtable.worldToMap(self.ptr, wx, wy, mx, my); }
    pub fn objectAt(self: Bridge, sx: f32, sy: f32, link_id: *i32) Status { return self.vtable.objectAt(self.ptr, sx, sy, link_id); }
    pub fn sounds(self: Bridge, out: []SoundRecord, total: *usize) Status { return self.vtable.sounds(self.ptr, out, total); }
    pub fn addSound(self: Bridge, index: i32, rec: SoundRecord) Status { return self.vtable.addSound(self.ptr, index, rec); }
    pub fn setSound(self: Bridge, index: i32, rec: SoundRecord) Status { return self.vtable.setSound(self.ptr, index, rec); }
    pub fn deleteSound(self: Bridge, index: i32) Status { return self.vtable.deleteSound(self.ptr, index); }
    pub fn readRecord(self: Bridge, kind: records.Kind, key: i32, allocator: std.mem.Allocator, out: *records.Value) Status { return self.vtable.readRecord(self.ptr, kind, key, allocator, out); }
    pub fn putRecord(self: Bridge, key: i32, value: *const records.Value) Status { return self.vtable.putRecord(self.ptr, key, value); }
    pub fn groundHeight(self: Bridge, wx: f32, wy: f32, z: *f32) Status { return self.vtable.groundHeight(self.ptr, wx, wy, z); }
};

test "check turns a refusal into Refused and everything else into Failed" {
    try check(.ok);
    try std.testing.expectError(error.Refused, check(.refused));
    try std.testing.expectError(error.Failed, check(.failed));
    try std.testing.expectError(error.Failed, check(.no_device));
}

test "a paint cell has the C struct's layout" {
    try std.testing.expectEqual(@as(usize, 12), @sizeOf(PaintCell));
    try std.testing.expectEqual(@as(usize, 8), @offsetOf(PaintCell, "tile"));
}
