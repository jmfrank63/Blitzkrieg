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
const PaintCell = core.bridge.PaintCell;

comptime {
    // The core's PaintCell is handed to BkEditorPaint as it is.
    std.debug.assert(@sizeOf(PaintCell) == @sizeOf(c.BkEditorPaintCell));
    std.debug.assert(@offsetOf(PaintCell, "x") == @offsetOf(c.BkEditorPaintCell, "x"));
    std.debug.assert(@offsetOf(PaintCell, "y") == @offsetOf(c.BkEditorPaintCell, "y"));
    std.debug.assert(@offsetOf(PaintCell, "tile") == @offsetOf(c.BkEditorPaintCell, "tile"));
    // Every status the bridge answers has a name in the core.
    std.debug.assert(@intFromEnum(Status.failed) == c.BK_EDITOR_FAILED);
}

fn status(value: c.BkEditorStatus) Status {
    return std.enums.fromInt(Status, value) orelse .failed;
}

pub const RealBridge = struct {
    session: *c.BkEditorSession,
    message: [512]u8 = undefined,
    message_len: usize = 0,

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
    };

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        const text = std.mem.span(c.BkEditorLastMessage(self.session));
        const len = @min(text.len, self.message.len);
        @memcpy(self.message[0..len], text[0..len]);
        self.message_len = len;
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

    /// The engine tier's two agreement checks, for tests.
    pub fn engineMatches(self: *RealBridge) Status {
        const terrain = status(c.BkEditorTerrainMatchesEngine(self.session));
        if (terrain != .ok) return terrain;
        return status(c.BkEditorWorldMatchesMap(self.session));
    }
};
