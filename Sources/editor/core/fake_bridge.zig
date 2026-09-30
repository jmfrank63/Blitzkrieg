//! An in-memory map behind the Bridge interface, for the core tier. It keeps
//! the real bridge's rules the core can see: the refusals (a bridge span, an
//! unknown object, a shared link ID, a cell or a placement off the map, a
//! value out of range), how link IDs are handed out, the order paints undo
//! and redo in, what a reopen forgets, and the object list's shape. It is
//! knowingly kinder or simpler in these ways, which a core test must not
//! lean on:
//!  - there is no file: a reopen forgets deleted objects and paint history,
//!    as the real one does, but keeps objects, tiles, diplomacy and map
//!    fields as they are, as if every edit had been saved;
//!  - there is no terrain function: a paint sets exactly the cells it names;
//!  - there is no engine: a known object is placed, moved, turned and
//!    re-owned anywhere on the map, where the real engine refuses some (a
//!    tree takes no direction and no owner);
//!  - any name is accepted on add, for an object or a sound; the real bridge
//!    refuses one the object database does not know (a sound additionally
//!    checked as game type 100);
//!  - edits are accepted before any map is open and after a failed open;
//!    the real bridge refuses them with "no map is open";
//!  - screen and world coordinates are the same thing;
//!  - references are modelled only as far as a delete reads them: a bridge
//!    span (`bridge_spans`, link ID to bridge index) and a trench piece
//!    (`trench_pieces`, link ID to entrenchment index) refuse, and start
//!    commands (`start_commands`, at most 8 units each) and reserve
//!    positions (`reserve_positions`) are edited by the cascade in the same
//!    order as the real bridge, undone in reverse. Reinforcement groups and
//!    mobile script IDs name script IDs, which a delete never edits, so the
//!    fake does not hold them, and it words the summary one change at a time
//!    where the real one groups ("start commands 2 and 5");
//!  - the ground is flat: `groundHeight` answers 0 on the map, where the real
//!    one reads the terrain's altitudes;
//!  - roads and rivers are not sampled: a record's key points are its control
//!    points (after the real builder's rule that drops a point within 2 units
//!    of the one before), each with the drawn width and opacity and a normal
//!    across its neighbours; "too short" is fewer than two such points (the
//!    real one also refuses a line shorter than one 30-unit sampling step);
//!    a record holds at most `max_vso_points` points; there is no engine to
//!    redraw and no AI to lock river tiles in;
//!  - `saveMap` never touches a real file on its own: the real
//!    `BkEditorSaveMap`'s read-back verification (session.cpp,
//!    `SaveSessionMap`) lives entirely inside the engine, invisible to this
//!    fake, which only writes to `files` (a shared `FakeFiles`) when a test
//!    sets it, so `Editor.save`'s temp+backup+swap has something to see land.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const files_mod = @import("files.zig");
const records = @import("records.zig");
const Status = bridge_mod.Status;
const MapInfo = bridge_mod.MapInfo;
const ObjectRecord = bridge_mod.ObjectRecord;
const SoundRecord = bridge_mod.SoundRecord;
const PaintCell = bridge_mod.PaintCell;
const Bridge = bridge_mod.Bridge;
const VsoKind = bridge_mod.VsoKind;
const VsoDescriptor = bridge_mod.VsoDescriptor;
const VsoKeyPoint = bridge_mod.VsoKeyPoint;
const VsoView = bridge_mod.VsoView;

/// World units per tile, standing in for the engine's own conversion.
pub const tile_size: f32 = 32.0;
/// How far from an object's centre a point still picks it.
pub const pick_radius: f32 = 16.0;

pub const CallKind = enum { open, save, add, place, delete, restore, diplomacy, map_type, attacking_side, paint, undo_paint, redo_paint, sound_add, sound_edit, sound_delete, record_put, vso_edit, undo_edit, redo_edit };
pub const Call = struct { kind: CallKind, id: i32 = 0 };

/// One start command as far as a delete reads it: up to eight units (link
/// IDs) and the target's link ID, 0 meaning none.
pub const FakeStartCommand = struct {
    units: [8]i32 = @splat(0),
    unit_count: usize = 0,
    target: i32 = 0,

    fn holds(self: *const FakeStartCommand, link_id: i32) bool {
        for (self.units[0..self.unit_count]) |unit| {
            if (unit == link_id) return true;
        }
        return false;
    }

    fn remove(self: *FakeStartCommand, link_id: i32) void {
        var kept: usize = 0;
        for (self.units[0..self.unit_count]) |unit| {
            if (unit == link_id) continue;
            self.units[kept] = unit;
            kept += 1;
        }
        self.unit_count = kept;
    }
};
/// A reserve position as far as a delete reads it: its artillery's and its
/// truck's link IDs, 0 meaning none.
pub const FakeReservePosition = struct { artillery: i32 = 0, truck: i32 = 0 };
/// What a delete did to one start command: its position when it was changed
/// (an earlier erase had already shifted the later ones), the record as it
/// was, whether the command went and whether its target was cleared. Undone
/// in reverse.
const StartChange = struct { position: usize, before: FakeStartCommand, unit_removed: bool, erased: bool, target_cleared: bool };
const ReserveChange = struct { position: usize, before: FakeReservePosition };
const Tombstone = struct {
    record: ObjectRecord,
    index: usize,
    changes: std.ArrayListUnmanaged(StartChange) = .empty,
    reserves: std.ArrayListUnmanaged(ReserveChange) = .empty,
};
/// The most control points a fake road or river holds.
pub const max_vso_points = 32;
/// CreateVSO's UniquePolygon distance (RMGC_MINIMAL_VIS_POINT_DISTANCE): a
/// point this close to the one before it is dropped.
pub const vso_min_point_distance: f32 = 2.0;

/// A road or river as the fake keeps it: its type's bare name, the bridge's
/// saved ID, and one key point per control point (see the header).
pub const FakeVso = struct {
    desc: VsoDescriptor = .{},
    saved_id: i32 = 0,
    controls: [max_vso_points]records.Vec3 = @splat(.{}),
    keys: [max_vso_points]VsoKeyPoint = @splat(.{}),
    count: usize = 0,

    pub fn controlSlice(self: *const FakeVso) []const records.Vec3 {
        return self.controls[0..self.count];
    }

    pub fn keySlice(self: *const FakeVso) []const VsoKeyPoint {
        return self.keys[0..self.count];
    }

    /// Every key point's normal, from the control points around it: a unit
    /// vector across the line, as the real sampler's normals are.
    fn renormal(self: *FakeVso) void {
        for (0..self.count) |index| {
            const previous = self.controls[if (index == 0) 0 else index - 1];
            const next = self.controls[@min(index + 1, self.count - 1)];
            const dx = next.x - previous.x;
            const dy = next.y - previous.y;
            const length = @sqrt(dx * dx + dy * dy);
            const key = &self.keys[index];
            key.x = self.controls[index].x;
            key.y = self.controls[index].y;
            key.z = 0;
            key.nx = if (length > 0) -dy / length else 0;
            key.ny = if (length > 0) dx / length else 1;
            key.nz = 0;
        }
    }
};
/// One logged road or river edit: the records before and after (either
/// absent for an add or a delete) at a list position.
const FakeVsoEdit = struct { kind: VsoKind, index: usize, before: ?FakeVso, after: ?FakeVso };

/// A mutable empty slice to start from; Allocator.free ignores a zero length.
var no_tiles: [0]u8 = .{};
const PaintRecord = struct { cells: []PaintCell, before: []u8 };

pub const FakeBridge = struct {
    allocator: std.mem.Allocator,
    info: MapInfo,
    objects_list: std.ArrayListUnmanaged(ObjectRecord) = .empty,
    /// The map's own sound list (CMapInfo::sounds.sounds - see bridge.h's
    /// own comment on BkEditorSounds). Kept across a fake reopen, the same
    /// simplification `objects_list`/`tiles`/`diplomacy_table` make: there is
    /// no file, so nothing here is actually lost.
    sounds_list: std.ArrayListUnmanaged(SoundRecord) = .empty,
    /// The map's camera anchors (BkEditorCameraAnchors): world units, kept
    /// across a fake reopen like the sounds. `setCameraAnchorsFixture` seeds it.
    camera_anchors: records.CameraAnchors = .{},
    /// Bridge spans: link ID to the bridge that holds it. A span cannot be
    /// deleted singly (the game's loaders assert every link of a bridge).
    bridge_spans: std.AutoHashMapUnmanaged(i32, i32) = .empty,
    /// Trench pieces: link ID to the entrenchment that holds it. A piece cannot
    /// be deleted singly (LoadEntrenchments asserts every link).
    trench_pieces: std.AutoHashMapUnmanaged(i32, i32) = .empty,
    /// The map's start commands, in file order. `addStartCommandFixture`.
    start_commands: std.ArrayListUnmanaged(FakeStartCommand) = .empty,
    /// The map's reserve positions, in file order. `addReservePositionFixture`.
    reserve_positions: std.ArrayListUnmanaged(FakeReservePosition) = .empty,
    tombstones: std.AutoHashMapUnmanaged(i32, Tombstone) = .empty,
    /// The season's road (0) and river (1) types. `addVsoDescriptorFixture`.
    vso_descriptors: [2]std.ArrayListUnmanaged(VsoDescriptor) = .{ .empty, .empty },
    /// The map's roads (0) and rivers (1), in list order. Kept across a reopen.
    vso_lists: [2]std.ArrayListUnmanaged(FakeVso) = .{ .empty, .empty },
    /// The edit log (BkEditorUndoEdit/RedoEdit): by token, and the two stacks.
    vso_edits: std.ArrayListUnmanaged(FakeVsoEdit) = .empty,
    applied_edits: std.ArrayListUnmanaged(i32) = .empty,
    undone_edits: std.ArrayListUnmanaged(i32) = .empty,
    diplomacy_table: std.ArrayListUnmanaged(i32) = .empty,
    tiles: []u8 = &no_tiles,
    paints: std.ArrayListUnmanaged(PaintRecord) = .empty,
    applied: std.ArrayListUnmanaged(i32) = .empty,
    undone: std.ArrayListUnmanaged(i32) = .empty,
    link_floor: i32 = 0,
    calls: std.ArrayListUnmanaged(Call) = .empty,
    message_buffer: [384]u8 = undefined,
    message_len: usize = 0,
    /// Set by a test to make the next `objects()` call fail, as a listing
    /// might after a successful open (a corrupt scenario, say).
    fail_objects: bool = false,
    /// Set by a test to make `openMap` answer `failed`, as the real bridge
    /// does when the engine throws while it builds the new map.
    fail_build: bool = false,
    /// Set by a test to make `paint` refuse cells that are on the map, as
    /// the real bridge does when the map will not take a paint (no tileset).
    refuse_paints: bool = false,
    /// Set by a test so `saveMap` writes its result into a shared FakeFiles,
    /// at the OS form of whatever path it is given - the same fake
    /// filesystem `Editor.files`'s temp+backup+swap operates on, so a
    /// plan-6 save test can see the whole dance land in one place. Not
    /// owned: the test that sets this owns the FakeFiles.
    files: ?*files_mod.FakeFiles = null,
    /// Set by a test to make `saveMap` answer `failed` before writing
    /// anything, as the real bridge does when the disk write itself fails.
    fail_save: bool = false,
    /// Map units per world unit. 1 keeps screen, world and map the same
    /// point, which most tests read most easily; the real engine's is
    /// sqrt 2 (BkEditorWorldToMap), and a test sets another to catch a
    /// world point used as a map position.
    map_per_world: f32 = 1,

    pub fn init(allocator: std.mem.Allocator, width_tiles: i32, height_tiles: i32, players: i32) FakeBridge {
        return .{
            .allocator = allocator,
            .info = .{ .width_tiles = width_tiles, .height_tiles = height_tiles, .player_count = players },
        };
    }

    pub fn deinit(self: *FakeBridge) void {
        for (self.paints.items) |paint_record| {
            self.allocator.free(paint_record.cells);
            self.allocator.free(paint_record.before);
        }
        self.paints.deinit(self.allocator);
        self.applied.deinit(self.allocator);
        self.undone.deinit(self.allocator);
        self.objects_list.deinit(self.allocator);
        self.sounds_list.deinit(self.allocator);
        self.bridge_spans.deinit(self.allocator);
        self.trench_pieces.deinit(self.allocator);
        self.start_commands.deinit(self.allocator);
        self.reserve_positions.deinit(self.allocator);
        self.freeTombstones();
        self.tombstones.deinit(self.allocator);
        for (&self.vso_descriptors) |*list| list.deinit(self.allocator);
        for (&self.vso_lists) |*list| list.deinit(self.allocator);
        self.vso_edits.deinit(self.allocator);
        self.applied_edits.deinit(self.allocator);
        self.undone_edits.deinit(self.allocator);
        self.diplomacy_table.deinit(self.allocator);
        self.calls.deinit(self.allocator);
        self.allocator.free(self.tiles);
        self.* = undefined;
    }

    /// An object in the map before it opens; `bridge_span` makes it a span of
    /// bridge 0, so its delete is refused as the real bridge refuses one.
    pub fn addFixture(self: *FakeBridge, new_record: ObjectRecord, bridge_span: bool) !void {
        try self.objects_list.append(self.allocator, new_record);
        if (bridge_span) try self.bridge_spans.put(self.allocator, new_record.link_id, 0);
    }

    /// A start command in the map before it opens: the unit link IDs it
    /// commands (at most eight) and its target's link ID (0: none).
    pub fn addStartCommandFixture(self: *FakeBridge, units: []const i32, target: i32) !void {
        var command: FakeStartCommand = .{ .target = target };
        std.debug.assert(units.len <= command.units.len);
        @memcpy(command.units[0..units.len], units);
        command.unit_count = units.len;
        try self.start_commands.append(self.allocator, command);
    }

    /// A sound in the map before it opens, for a test to seed the list
    /// `sounds()`/`editSound`/`deleteSound` then see.
    pub fn addSoundFixture(self: *FakeBridge, new_record: SoundRecord) !void {
        try self.sounds_list.append(self.allocator, new_record);
    }

    /// A reserve position in the map before it opens: the artillery's and the
    /// truck's link IDs (0: none).
    pub fn addReservePositionFixture(self: *FakeBridge, artillery: i32, truck: i32) !void {
        try self.reserve_positions.append(self.allocator, .{ .artillery = artillery, .truck = truck });
    }

    /// Makes an object of the map a piece of entrenchment `entrenchment`, so
    /// its delete is refused as the real bridge refuses one.
    pub fn addTrenchPieceFixture(self: *FakeBridge, link_id: i32, entrenchment: i32) !void {
        try self.trench_pieces.put(self.allocator, link_id, entrenchment);
    }

    /// A road (`.road`) or river (`.river`) type the season offers.
    pub fn addVsoDescriptorFixture(self: *FakeBridge, kind: VsoKind, name: []const u8) !void {
        var descriptor: VsoDescriptor = .{};
        descriptor.setName(name);
        try self.vso_descriptors[@intFromEnum(kind)].append(self.allocator, descriptor);
    }

    /// A road or river in the map before it opens, built as `addVso` builds
    /// one; returns false when the fake's own rules refuse it.
    pub fn addVsoFixture(self: *FakeBridge, kind: VsoKind, desc: []const u8, points: []const records.Vec3, width: f32, opacity: f32) !bool {
        var built: FakeVso = .{};
        if (!self.buildVso(desc, points, width, opacity, &built)) return false;
        try self.vso_lists[@intFromEnum(kind)].append(self.allocator, built);
        return true;
    }

    /// The road or river at `index`, for a test to read.
    pub fn vso(self: *const FakeBridge, kind: VsoKind, index: usize) *const FakeVso {
        return &self.vso_lists[@intFromEnum(kind)].items[index];
    }

    pub fn vsoLen(self: *const FakeBridge, kind: VsoKind) usize {
        return self.vso_lists[@intFromEnum(kind)].items.len;
    }

    /// The camera anchors the map holds before it opens.
    pub fn setCameraAnchorsFixture(self: *FakeBridge, anchors: records.CameraAnchors) void {
        self.camera_anchors = anchors;
    }

    pub fn tile(self: *const FakeBridge, x: i32, y: i32) u8 {
        return self.tiles[@intCast(y * self.info.width_tiles + x)];
    }

    pub fn bridge(self: *FakeBridge) Bridge {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *FakeBridge {
        return @ptrCast(@alignCast(ptr));
    }

    fn say(self: *FakeBridge, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }

    fn record(self: *FakeBridge, kind: CallKind, id: i32) void {
        self.calls.append(self.allocator, .{ .kind = kind, .id = id }) catch {};
    }

    fn onMap(self: *const FakeBridge, x: f32, y: f32) bool {
        return x >= 0 and y >= 0 and
            x < @as(f32, @floatFromInt(self.info.width_tiles)) * tile_size and
            y < @as(f32, @floatFromInt(self.info.height_tiles)) * tile_size;
    }

    /// onMap for a map position rather than a world point.
    fn onMapAt(self: *const FakeBridge, x: f32, y: f32) bool {
        return self.onMap(x / self.map_per_world, y / self.map_per_world);
    }

    fn indexOf(self: *const FakeBridge, link_id: i32) ?usize {
        for (self.objects_list.items, 0..) |object, index| {
            if (object.link_id == link_id) return index;
        }
        return null;
    }

    fn forgetHistory(self: *FakeBridge) void {
        for (self.paints.items) |paint_record| {
            self.allocator.free(paint_record.cells);
            self.allocator.free(paint_record.before);
        }
        self.paints.clearRetainingCapacity();
        self.applied.clearRetainingCapacity();
        self.undone.clearRetainingCapacity();
        self.freeTombstones();
        self.tombstones.clearRetainingCapacity();
        self.vso_edits.clearRetainingCapacity();
        self.applied_edits.clearRetainingCapacity();
        self.undone_edits.clearRetainingCapacity();
    }

    fn freeTombstones(self: *FakeBridge) void {
        var tombstones = self.tombstones.valueIterator();
        while (tombstones.next()) |tombstone| {
            tombstone.changes.deinit(self.allocator);
            tombstone.reserves.deinit(self.allocator);
        }
    }

    /// The real bridge's summary of what a delete changed besides the object,
    /// in the message buffer: "also removed from start command 0; start
    /// command 3 erased; reserve position 1 erased". Positions are the ones
    /// before the delete.
    fn describeCascade(self: *FakeBridge, changes: []const StartChange, reserves: []const ReserveChange) void {
        self.message_len = 0;
        var erased_before: usize = 0;
        for (changes) |change| {
            const original = change.position + erased_before;
            if (change.erased) erased_before += 1;
            if (change.erased) {
                self.appendPart("start command {d} erased", .{original});
            } else if (change.unit_removed) {
                self.appendPart("removed from start command {d}", .{original});
            }
            if (change.target_cleared) self.appendPart("target of start command {d} cleared", .{original});
        }
        erased_before = 0;
        for (reserves) |reserve| {
            self.appendPart("reserve position {d} erased", .{reserve.position + erased_before});
            erased_before += 1;
        }
    }

    fn appendPart(self: *FakeBridge, comptime format: []const u8, args: anytype) void {
        const prefix: []const u8 = if (self.message_len == 0) "also " else "; ";
        var buffer: [96]u8 = undefined;
        const part = std.fmt.bufPrint(&buffer, format, args) catch return;
        const room = self.message_buffer.len - self.message_len;
        if (prefix.len + part.len > room) return;
        @memcpy(self.message_buffer[self.message_len..][0..prefix.len], prefix);
        self.message_len += prefix.len;
        @memcpy(self.message_buffer[self.message_len..][0..part.len], part);
        self.message_len += part.len;
    }

    /// More than one object carrying the link ID: the real bridge cannot
    /// tell which one an edit means, and refuses it.
    fn shared(self: *FakeBridge, link_id: i32) bool {
        var count: usize = 0;
        for (self.objects_list.items) |object| {
            if (object.link_id == link_id) count += 1;
        }
        if (count <= 1) return false;
        self.say("{d} objects of the map share link ID {d}, so the editor cannot tell which of them it would change; they are kept as they are", .{ count, link_id });
        return true;
    }

    fn nextLinkId(self: *const FakeBridge) i32 {
        var next: i32 = self.link_floor;
        for (self.objects_list.items) |object| next = @max(next, object.link_id + 1);
        return next;
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
        .sounds = sounds,
        .addSound = addSound,
        .setSound = setSound,
        .deleteSound = deleteSound,
        .readRecord = readRecord,
        .putRecord = putRecord,
        .groundHeight = groundHeight,
        .undoEdit = undoEdit,
        .redoEdit = redoEdit,
        .vsoDescriptors = vsoDescriptors,
        .vsoCount = vsoCount,
        .readVso = readVso,
        .addVso = addVso,
        .deleteVso = deleteVso,
        .moveVsoPoints = moveVsoPoints,
        .setVsoWidth = setVsoWidth,
        .setVsoOpacity = setVsoOpacity,
        .insertVsoPoint = insertVsoPoint,
        .deleteVsoPoint = deleteVsoPoint,
        .pickVso = pickVso,
    };

    /// The real builder's rules the core sees, without the sampling: drops a
    /// point within `vso_min_point_distance` of the one kept before it; "too
    /// short" below two points; a width in world units at every key point.
    fn buildVso(self: *FakeBridge, desc: []const u8, points: []const records.Vec3, width: f32, opacity: f32, out: *FakeVso) bool {
        var vso_record: FakeVso = .{};
        vso_record.desc.setName(desc);
        for (points) |point| {
            if (vso_record.count > 0) {
                const last = vso_record.controls[vso_record.count - 1];
                const dx = point.x - last.x;
                const dy = point.y - last.y;
                if (@sqrt(dx * dx + dy * dy) <= vso_min_point_distance) continue;
            }
            if (vso_record.count == max_vso_points) {
                self.say("the fake holds at most {d} points per road", .{max_vso_points});
                return false;
            }
            vso_record.controls[vso_record.count] = .{ .x = point.x, .y = point.y, .z = 0 };
            vso_record.count += 1;
        }
        if (vso_record.count < 2) {
            self.say("that road is too short: it needs two points at least 2 units apart", .{});
            return false;
        }
        vso_record.renormal();
        for (vso_record.keys[0..vso_record.count]) |*key| {
            key.width = width;
            key.opacity = opacity;
        }
        vso_record.saved_id = self.nextVsoId();
        out.* = vso_record;
        return true;
    }

    /// One above every saved ID of both lists, at least 1 (NextVsoID).
    fn nextVsoId(self: *const FakeBridge) i32 {
        var next: i32 = 1;
        for (&self.vso_lists) |*list| {
            for (list.items) |item| next = @max(next, item.saved_id + 1);
        }
        return next;
    }

    /// The one put an edit, its undo and its redo share (PutVso).
    fn putVso(self: *FakeBridge, kind: VsoKind, index: usize, before: ?*const FakeVso, after: ?*const FakeVso) Status {
        const list = &self.vso_lists[@intFromEnum(kind)];
        if (before != null and after != null) {
            if (index >= list.items.len) return .failed;
            list.items[index] = after.?.*;
        } else if (after) |record_after| {
            if (index > list.items.len) return .failed;
            list.insert(self.allocator, index, record_after.*) catch return .failed;
        } else if (before != null) {
            if (index >= list.items.len) return .failed;
            _ = list.orderedRemove(index);
        }
        return .ok;
    }

    /// Puts an edit through and logs it, handing out its token.
    fn logVsoEdit(self: *FakeBridge, edit: FakeVsoEdit, token: *i32) Status {
        self.vso_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const put = self.putVso(edit.kind, edit.index, if (edit.before) |*b| b else null, if (edit.after) |*a| a else null);
        if (put != .ok) return put;
        self.vso_edits.appendAssumeCapacity(edit);
        token.* = @intCast(self.vso_edits.items.len - 1);
        self.applied_edits.appendAssumeCapacity(token.*);
        self.undone_edits.clearRetainingCapacity();
        self.record(.vso_edit, token.*);
        return .ok;
    }

    fn undoEdit(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (self.applied_edits.items.len == 0 or self.applied_edits.items[self.applied_edits.items.len - 1] != token) {
            self.say("edits are undone newest first", .{});
            return .refused;
        }
        self.undone_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const edit = &self.vso_edits.items[@intCast(token)];
        const put = self.putVso(edit.kind, edit.index, if (edit.after) |*a| a else null, if (edit.before) |*b| b else null);
        if (put != .ok) return put;
        _ = self.applied_edits.pop();
        self.undone_edits.appendAssumeCapacity(token);
        self.record(.undo_edit, token);
        return .ok;
    }

    fn redoEdit(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (self.undone_edits.items.len == 0 or self.undone_edits.items[self.undone_edits.items.len - 1] != token) {
            self.say("edits are redone in the order they were undone", .{});
            return .refused;
        }
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const edit = &self.vso_edits.items[@intCast(token)];
        const put = self.putVso(edit.kind, edit.index, if (edit.before) |*b| b else null, if (edit.after) |*a| a else null);
        if (put != .ok) return put;
        _ = self.undone_edits.pop();
        self.applied_edits.appendAssumeCapacity(token);
        self.record(.redo_edit, token);
        return .ok;
    }

    fn vsoDescriptors(ptr: *anyopaque, kind: VsoKind, out: []VsoDescriptor, total: *usize) Status {
        const self = from(ptr);
        const list = self.vso_descriptors[@intFromEnum(kind)].items;
        total.* = list.len;
        const count = @min(out.len, list.len);
        @memcpy(out[0..count], list[0..count]);
        return if (out.len >= list.len) .ok else .refused;
    }

    fn vsoCount(ptr: *anyopaque, kind: VsoKind, count: *usize) Status {
        const self = from(ptr);
        count.* = self.vso_lists[@intFromEnum(kind)].items.len;
        return .ok;
    }

    fn readVso(ptr: *anyopaque, kind: VsoKind, index: i32, allocator: std.mem.Allocator, out: *VsoView) Status {
        const self = from(ptr);
        const list = self.vso_lists[@intFromEnum(kind)].items;
        if (index < 0 or index >= list.len) return .bad_argument;
        const item = &list[@intCast(index)];
        const controls = allocator.dupe(records.Vec3, item.controlSlice()) catch return .failed;
        const keys = allocator.dupe(VsoKeyPoint, item.keySlice()) catch {
            allocator.free(controls);
            return .failed;
        };
        out.* = .{ .saved_id = item.saved_id, .control_points = controls, .key_points = keys };
        @memcpy(out.desc[0..item.desc.name.len], &item.desc.name);
        return .ok;
    }

    fn deleteVso(ptr: *anyopaque, kind: VsoKind, index: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const list = self.vso_lists[@intFromEnum(kind)].items;
        if (index < 0 or index >= list.len) return .bad_argument;
        const at: usize = @intCast(index);
        return self.logVsoEdit(.{ .kind = kind, .index = at, .before = list[at], .after = null }, token);
    }

    /// The record at `index`, or null (a caller bug: bad_argument).
    fn vsoAt(self: *FakeBridge, kind: VsoKind, index: i32) ?FakeVso {
        const list = self.vso_lists[@intFromEnum(kind)].items;
        if (index < 0 or index >= list.len) return null;
        return list[@intCast(index)];
    }

    fn logReplace(self: *FakeBridge, kind: VsoKind, index: i32, before: FakeVso, after: FakeVso, token: *i32) Status {
        return self.logVsoEdit(.{ .kind = kind, .index = @intCast(index), .before = before, .after = after }, token);
    }

    fn moveVsoPoints(ptr: *anyopaque, kind: VsoKind, index: i32, points: []const records.Vec3, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (points.len != before.count) return .bad_argument;
        for (points) |point| {
            if (!finite(point)) return .bad_argument;
        }
        for (points, 0..) |point, at| {
            if (!self.onMap(point.x, point.y)) {
                self.say("point {d} would be off the map", .{at});
                return .refused;
            }
            if (at > 0) {
                const dx = point.x - points[at - 1].x;
                const dy = point.y - points[at - 1].y;
                if (dx * dx + dy * dy <= vso_min_point_distance * vso_min_point_distance) {
                    self.say("two neighbouring points would be closer than 2 units", .{});
                    return .refused;
                }
            }
        }
        var after = before;
        for (points, 0..) |point, at| after.controls[at] = .{ .x = point.x, .y = point.y, .z = 0 };
        after.renormal();
        return self.logReplace(kind, index, before, after, token);
    }

    /// Whether key `at` is changed by an edit at `key` in `mode`.
    fn inMode(mode: bridge_mod.VsoWidthMode, key: usize, at: usize) bool {
        return switch (mode) {
            .single => at == key,
            .multi => at >= key,
            .all => true,
        };
    }

    fn setVsoWidth(ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, width: f32, mode: bridge_mod.VsoWidthMode, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (key < 0 or key >= before.count or !std.math.isFinite(width) or width <= 0) return .bad_argument;
        var after = before;
        for (after.keys[0..after.count], 0..) |*item, at| {
            if (inMode(mode, @intCast(key), at)) item.width = width;
        }
        return self.logReplace(kind, index, before, after, token);
    }

    fn setVsoOpacity(ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, opacity: f32, mode: bridge_mod.VsoWidthMode, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (key < 0 or key >= before.count or !std.math.isFinite(opacity) or opacity < 0 or opacity > 1) return .bad_argument;
        var after = before;
        for (after.keys[0..after.count], 0..) |*item, at| {
            if (inMode(mode, @intCast(key), at)) item.opacity = opacity;
        }
        return self.logReplace(kind, index, before, after, token);
    }

    fn insertVsoPoint(ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (control < 0 or control >= before.count) return .bad_argument;
        if (before.count == max_vso_points) {
            self.say("the fake holds at most {d} points per road", .{max_vso_points});
            return .refused;
        }
        const c: usize = @intCast(control);
        const other = if (c + 1 < before.count) c + 1 else c - 1;
        const at = if (c + 1 < before.count) c + 1 else c;
        var after = before;
        var middle = before.controls[c];
        middle.x = (before.controls[c].x + before.controls[other].x) / 2;
        middle.y = (before.controls[c].y + before.controls[other].y) / 2;
        var key = before.keys[c];
        key.width = (before.keys[c].width + before.keys[other].width) / 2;
        key.opacity = (before.keys[c].opacity + before.keys[other].opacity) / 2;
        var move = after.count;
        while (move > at) : (move -= 1) {
            after.controls[move] = after.controls[move - 1];
            after.keys[move] = after.keys[move - 1];
        }
        after.controls[at] = middle;
        after.keys[at] = key;
        after.count += 1;
        after.renormal();
        return self.logReplace(kind, index, before, after, token);
    }

    fn deleteVsoPoint(ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (control < 0 or control >= before.count) return .bad_argument;
        if (before.count <= 2) {
            self.say("a road needs at least 2 points", .{});
            return .refused;
        }
        var after = before;
        var at: usize = @intCast(control);
        while (at + 1 < after.count) : (at += 1) {
            after.controls[at] = after.controls[at + 1];
            after.keys[at] = after.keys[at + 1];
        }
        after.count -= 1;
        after.renormal();
        return self.logReplace(kind, index, before, after, token);
    }

    /// The fake's hit test: within the wider of the two key widths of a
    /// segment of the line, roads before rivers (the real one tests the
    /// sampled stripe's bounding polygon).
    fn pickVso(ptr: *anyopaque, wx: f32, wy: f32, cycle: i32, kind: *VsoKind, index: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        index.* = -1;
        if (cycle < 0 or !std.math.isFinite(wx) or !std.math.isFinite(wy)) return .bad_argument;
        var hits: [64]bridge_mod.VsoRef = undefined;
        var count: usize = 0;
        for ([_]VsoKind{ .road, .river }) |candidate_kind| {
            for (self.vso_lists[@intFromEnum(candidate_kind)].items, 0..) |*item, at| {
                if (count == hits.len) break;
                if (!hitsLine(item, wx, wy)) continue;
                hits[count] = .{ .kind = candidate_kind, .index = at };
                count += 1;
            }
        }
        if (count == 0) {
            self.say("no road or river there", .{});
            return .refused;
        }
        const hit = hits[@as(usize, @intCast(cycle)) % count];
        kind.* = hit.kind;
        index.* = @intCast(hit.index);
        return .ok;
    }

    fn hitsLine(item: *const FakeVso, x: f32, y: f32) bool {
        var at: usize = 0;
        while (at + 1 < item.count) : (at += 1) {
            const a = item.controls[at];
            const b = item.controls[at + 1];
            const dx = b.x - a.x;
            const dy = b.y - a.y;
            const length2 = dx * dx + dy * dy;
            const t = if (length2 > 0) std.math.clamp(((x - a.x) * dx + (y - a.y) * dy) / length2, 0, 1) else 0;
            const px = a.x + t * dx - x;
            const py = a.y + t * dy - y;
            const reach = @max(item.keys[at].width, item.keys[at + 1].width);
            if (px * px + py * py <= reach * reach) return true;
        }
        return false;
    }

    fn knownDescriptor(self: *const FakeBridge, kind: VsoKind, desc: []const u8) bool {
        for (self.vso_descriptors[@intFromEnum(kind)].items) |*item| {
            if (std.mem.eql(u8, item.nameSlice(), desc)) return true;
        }
        return false;
    }

    fn addVso(ptr: *anyopaque, kind: VsoKind, desc: []const u8, points: []const records.Vec3, width_tiles: f32, opacity: f32, token: *i32, index: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        index.* = -1;
        if (points.len > 1024) return .bad_argument;
        if (!std.math.isFinite(width_tiles) or !std.math.isFinite(opacity) or width_tiles < 1 or width_tiles > 16 or opacity < 0 or opacity > 1) return .bad_argument;
        for (points) |point| {
            if (!finite(point)) return .bad_argument;
        }
        if (!self.knownDescriptor(kind, desc)) {
            self.say("there is no {s} type \"{s}\" in this map's season", .{ kind.label(), desc });
            return .refused;
        }
        for (points, 0..) |point, at| {
            if (!self.onMap(point.x, point.y)) {
                self.say("point {d} of the {s} is not on the map", .{ at, kind.label() });
                return .refused;
            }
        }
        var built: FakeVso = .{};
        if (!self.buildVso(desc, points, width_tiles * tile_size / 2.0, opacity, &built)) return .refused;
        const at = self.vso_lists[@intFromEnum(kind)].items.len;
        const logged = self.logVsoEdit(.{ .kind = kind, .index = at, .before = null, .after = built }, token);
        if (logged != .ok) return logged;
        index.* = @intCast(at);
        return .ok;
    }

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        return self.message_buffer[0..self.message_len];
    }

    fn openMap(ptr: *anyopaque, path: []const u8, info: *MapInfo) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.record(.open, 0);
        if (std.mem.eql(u8, path, "missing.bzm")) {
            self.say("no such map", .{});
            return .data_missing;
        }
        if (self.fail_build) {
            self.say("the engine threw", .{});
            return .failed;
        }
        // What the real bridge forgets on an open: every tombstone and every
        // paint token of the map before.
        self.forgetHistory();
        if (self.tiles.len == 0) {
            self.tiles = self.allocator.alloc(u8, @intCast(self.info.width_tiles * self.info.height_tiles)) catch return .failed;
            @memset(self.tiles, 0);
            self.diplomacy_table.resize(self.allocator, @intCast(self.info.player_count)) catch return .failed;
            for (self.diplomacy_table.items, 0..) |*side, player| side.* = @intCast(player % 2);
        }
        self.link_floor = self.nextLinkId();
        info.* = self.info;
        return .ok;
    }

    fn saveMap(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.record(.save, 0);
        if (path.len == 0) return .bad_argument;
        if (self.fail_save) {
            self.say("the disk is full", .{});
            return .failed;
        }
        if (self.files) |fake_files| {
            var os_buffer: [files_mod.max_path]u8 = undefined;
            const os_path = files_mod.osPathFromEngine(&os_buffer, path) orelse return .bad_argument;
            var content_buffer: [64]u8 = undefined;
            const content = std.fmt.bufPrint(&content_buffer, "fake map: {d} objects", .{self.objects_list.items.len}) catch return .failed;
            fake_files.write(os_path, content) catch return .failed;
        }
        return .ok;
    }

    fn objects(ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status {
        const self = from(ptr);
        if (self.fail_objects) {
            self.say("the object listing failed", .{});
            return .failed;
        }
        total.* = self.objects_list.items.len;
        const count = @min(out.len, self.objects_list.items.len);
        @memcpy(out[0..count], self.objects_list.items[0..count]);
        return if (out.len >= self.objects_list.items.len) .ok else .refused;
    }

    fn diplomacy(ptr: *anyopaque, player: i32, value: *i32) Status {
        const self = from(ptr);
        if (player < 0 or @as(usize, @intCast(player)) >= self.diplomacy_table.items.len) return .bad_argument;
        value.* = self.diplomacy_table.items[@intCast(player)];
        return .ok;
    }

    fn addObject(ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (name.len == 0) return .bad_argument;
        if (!self.onMapAt(x, y)) {
            self.say("the engine would not place the object there", .{});
            return .refused;
        }
        var object: ObjectRecord = .{ .link_id = self.nextLinkId(), .x = x, .y = y, .dir = dir, .player = player };
        object.setName(name);
        self.objects_list.append(self.allocator, object) catch return .failed;
        self.link_floor = object.link_id + 1;
        link_id.* = object.link_id;
        self.record(.add, object.link_id);
        return .ok;
    }

    fn placeObject(ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const index = self.indexOf(link_id) orelse return .bad_argument;
        const object = &self.objects_list.items[index];
        if (self.shared(link_id)) return .refused;
        if (!object.known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        if (!self.onMapAt(x, y)) {
            self.say("the engine would not take that placement for the object", .{});
            return .refused;
        }
        object.x = x;
        object.y = y;
        object.dir = dir;
        object.player = player;
        self.record(.place, link_id);
        return .ok;
    }

    fn deleteObject(ptr: *anyopaque, link_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (!self.objects_list.items[index].known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        if (self.shared(link_id)) return .refused;
        if (self.bridge_spans.get(link_id)) |bridge_index| {
            self.say("still referred to by bridge {d}", .{bridge_index});
            return .refused;
        }
        if (self.trench_pieces.get(link_id)) |entrenchment_index| {
            self.say("still part of entrenchment {d}", .{entrenchment_index});
            return .refused;
        }
        // Everything that can fail comes first, so a failure leaves the map as it was.
        var changes: std.ArrayListUnmanaged(StartChange) = .empty;
        errdefer changes.deinit(self.allocator);
        var reserves: std.ArrayListUnmanaged(ReserveChange) = .empty;
        errdefer reserves.deinit(self.allocator);
        changes.ensureTotalCapacity(self.allocator, self.start_commands.items.len) catch return .failed;
        reserves.ensureTotalCapacity(self.allocator, self.reserve_positions.items.len) catch return .failed;
        self.tombstones.ensureUnusedCapacity(self.allocator, 1) catch return .failed;

        const removed = self.objects_list.orderedRemove(index);
        // Link ID 0 is "none": a record listing 0 is not naming this object.
        if (link_id != 0) {
            var position: usize = 0;
            while (position < self.start_commands.items.len) {
                const command = &self.start_commands.items[position];
                const holds = command.holds(link_id);
                const targets = command.target == link_id;
                if (!holds and !targets) {
                    position += 1;
                    continue;
                }
                const before = command.*;
                if (holds) command.remove(link_id);
                const erased = holds and command.unit_count == 0;
                var cleared = false;
                if (!erased and targets) {
                    command.target = 0;
                    cleared = true;
                }
                changes.appendAssumeCapacity(.{ .position = position, .before = before, .unit_removed = holds, .erased = erased, .target_cleared = cleared });
                if (erased) _ = self.start_commands.orderedRemove(position) else position += 1;
            }
            position = 0;
            while (position < self.reserve_positions.items.len) {
                const reserve = self.reserve_positions.items[position];
                if (reserve.artillery != link_id and reserve.truck != link_id) {
                    position += 1;
                    continue;
                }
                reserves.appendAssumeCapacity(.{ .position = position, .before = reserve });
                _ = self.reserve_positions.orderedRemove(position);
            }
        }
        self.tombstones.putAssumeCapacity(link_id, .{ .record = removed, .index = index, .changes = changes, .reserves = reserves });
        self.link_floor = @max(self.link_floor, link_id + 1);
        self.describeCascade(changes.items, reserves.items);
        self.record(.delete, link_id);
        return .ok;
    }

    fn restoreObject(ptr: *anyopaque, link_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const tombstone = self.tombstones.getPtr(link_id) orelse {
            self.say("no deleted object has that link ID", .{});
            return .refused;
        };
        if (self.indexOf(link_id) != null) {
            self.say("the link ID is in use again", .{});
            return .refused;
        }
        self.objects_list.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.start_commands.ensureUnusedCapacity(self.allocator, tombstone.changes.items.len) catch return .failed;
        self.reserve_positions.ensureUnusedCapacity(self.allocator, tombstone.reserves.items.len) catch return .failed;
        // Reverse order of application: a later erase shifted the positions after it.
        var back = tombstone.reserves.items.len;
        while (back != 0) {
            back -= 1;
            const reserve = tombstone.reserves.items[back];
            self.reserve_positions.insertAssumeCapacity(@min(reserve.position, self.reserve_positions.items.len), reserve.before);
        }
        back = tombstone.changes.items.len;
        while (back != 0) {
            back -= 1;
            const change = tombstone.changes.items[back];
            if (change.erased) {
                self.start_commands.insertAssumeCapacity(@min(change.position, self.start_commands.items.len), change.before);
            } else {
                self.start_commands.items[change.position] = change.before;
            }
        }
        const index = @min(tombstone.index, self.objects_list.items.len);
        self.objects_list.insertAssumeCapacity(index, tombstone.record);
        tombstone.changes.deinit(self.allocator);
        tombstone.reserves.deinit(self.allocator);
        _ = self.tombstones.remove(link_id);
        self.record(.restore, link_id);
        return .ok;
    }

    fn setDiplomacy(ptr: *anyopaque, player: i32, value: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (player < 0 or @as(usize, @intCast(player)) >= self.diplomacy_table.items.len) return .bad_argument;
        if (value < 0 or value > 2) {
            self.say("{d} is no diplomacy: 0 and 1 are the two sides, 2 is neutral", .{value});
            return .bad_argument;
        }
        self.diplomacy_table.items[@intCast(player)] = value;
        self.record(.diplomacy, player);
        return .ok;
    }

    fn setMapType(ptr: *anyopaque, value: i32) Status {
        const self = from(ptr);
        self.info.map_type = value;
        self.record(.map_type, value);
        return .ok;
    }

    fn setAttackingSide(ptr: *anyopaque, value: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (value < 0 or value > 1) {
            self.say("{d} is no side: the attacking side is 0 or 1", .{value});
            return .bad_argument;
        }
        self.info.attacking_side = value;
        self.record(.attacking_side, value);
        return .ok;
    }

    fn sounds(ptr: *anyopaque, out: []SoundRecord, total: *usize) Status {
        const self = from(ptr);
        total.* = self.sounds_list.items.len;
        const count = @min(out.len, self.sounds_list.items.len);
        @memcpy(out[0..count], self.sounds_list.items[0..count]);
        return if (out.len >= self.sounds_list.items.len) .ok else .refused;
    }

    /// The real bridge's rules the core can see: a non-empty name (the real
    /// one additionally checks the object database knows it as a sound,
    /// which this fake has no database for - any non-empty name is accepted,
    /// the same simplification `addObject`'s own doc comment above makes for
    /// objects), a position on the map (`tile_size` standing in for
    /// `fWorldCellSize`, `onMap`'s own reasoning), and non-negative times and
    /// radii with min <= max.
    fn validSound(self: *FakeBridge, sound: SoundRecord) Status {
        if (sound.nameSlice().len == 0) return .bad_argument;
        if (!self.onMap(sound.x, sound.y)) {
            self.say("that position is not on the map", .{});
            return .refused;
        }
        if (sound.repeat_ms < 0 or sound.repeat_random_ms < 0 or sound.min_radius < 0 or sound.max_radius < 0 or sound.min_radius > sound.max_radius) {
            self.say("a sound's times and radii must not be negative, and its minimum radius must not be above its maximum", .{});
            return .refused;
        }
        return .ok;
    }

    fn addSound(ptr: *anyopaque, index: i32, new_record: SoundRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        const valid = self.validSound(new_record);
        if (valid != .ok) return valid;
        const count: i32 = @intCast(self.sounds_list.items.len);
        if (index < -1 or index > count) return .bad_argument;
        const at: usize = if (index < 0) self.sounds_list.items.len else @intCast(index);
        self.sounds_list.insert(self.allocator, at, new_record) catch return .failed;
        self.record(.sound_add, @intCast(at));
        return .ok;
    }

    fn setSound(ptr: *anyopaque, index: i32, new_record: SoundRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (index < 0 or index >= @as(i32, @intCast(self.sounds_list.items.len))) return .bad_argument;
        const valid = self.validSound(new_record);
        if (valid != .ok) return valid;
        self.sounds_list.items[@intCast(index)] = new_record;
        self.record(.sound_edit, index);
        return .ok;
    }

    fn deleteSound(ptr: *anyopaque, index: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (index < 0 or index >= @as(i32, @intCast(self.sounds_list.items.len))) return .bad_argument;
        _ = self.sounds_list.orderedRemove(@intCast(index));
        self.record(.sound_delete, index);
        return .ok;
    }

    fn readRecord(ptr: *anyopaque, kind: records.Kind, key: i32, allocator: std.mem.Allocator, out: *records.Value) Status {
        _ = allocator; // the kinds so far own no memory
        const self = from(ptr);
        self.message_len = 0;
        switch (kind) {
            .camera_anchors => {
                if (key != 0) return .bad_argument;
                out.* = .{ .camera_anchors = self.camera_anchors };
            },
        }
        return .ok;
    }

    /// The real bridge's rules the core can see: the count 0..32 and finite
    /// coordinates are a caller bug; a slot this put changes that is not unset
    /// must be on the map (world units), or the put is refused naming it.
    fn putRecord(ptr: *anyopaque, key: i32, value: *const records.Value) Status {
        const self = from(ptr);
        self.message_len = 0;
        switch (value.*) {
            .camera_anchors => |wanted| {
                if (key != 0) return .bad_argument;
                if (wanted.player_count > records.max_camera_players) return .bad_argument;
                if (!finite(wanted.neutral)) return .bad_argument;
                for (wanted.players[0..wanted.player_count]) |slot| {
                    if (!finite(slot)) return .bad_argument;
                }
                const current = self.camera_anchors;
                if (!wanted.neutral.eql(current.neutral) and !wanted.neutral.isUnset() and !self.onMap(wanted.neutral.x, wanted.neutral.y)) {
                    self.say("the neutral camera anchor is not on the map", .{});
                    return .refused;
                }
                for (wanted.players[0..wanted.player_count], 0..) |slot, index| {
                    if (slot.eql(current.slot(index)) or slot.isUnset()) continue;
                    if (!self.onMap(slot.x, slot.y)) {
                        self.say("the camera anchor of player {d} is not on the map", .{index});
                        return .refused;
                    }
                }
                // Exact: slots past the new count are dropped, as the real
                // put resizes the vector.
                var exact: records.CameraAnchors = .{};
                exact.neutral = wanted.neutral;
                exact.player_count = wanted.player_count;
                @memcpy(exact.players[0..wanted.player_count], wanted.players[0..wanted.player_count]);
                self.camera_anchors = exact;
                self.record(.record_put, key);
            },
        }
        return .ok;
    }

    fn finite(point: records.Vec3) bool {
        return std.math.isFinite(point.x) and std.math.isFinite(point.y) and std.math.isFinite(point.z);
    }

    /// Flat ground: 0 on the map. A simplification listed in the header.
    fn groundHeight(ptr: *anyopaque, wx: f32, wy: f32, z: *f32) Status {
        const self = from(ptr);
        self.message_len = 0;
        z.* = 0;
        if (!self.onMap(wx, wy)) {
            self.say("that point is not on the map", .{});
            return .refused;
        }
        return .ok;
    }

    fn paint(ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        for (cells) |cell| {
            if (cell.x < 0 or cell.y < 0 or cell.x >= self.info.width_tiles or cell.y >= self.info.height_tiles) {
                self.say("cell {d},{d} is not on the map", .{ cell.x, cell.y });
                return .refused;
            }
        }
        if (self.refuse_paints) {
            self.say("the map would not take that paint", .{});
            return .refused;
        }
        const copy = self.allocator.dupe(PaintCell, cells) catch return .failed;
        const before = self.allocator.alloc(u8, cells.len) catch {
            self.allocator.free(copy);
            return .failed;
        };
        for (cells, 0..) |cell, index| {
            before[index] = self.tile(cell.x, cell.y);
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = cell.tile;
        }
        self.paints.append(self.allocator, .{ .cells = copy, .before = before }) catch return .failed;
        token.* = @intCast(self.paints.items.len - 1);
        self.applied.append(self.allocator, token.*) catch return .failed;
        self.undone.clearRetainingCapacity();
        self.record(.paint, token.*);
        return .ok;
    }

    fn undoPaint(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        if (self.applied.items.len == 0 or self.applied.items[self.applied.items.len - 1] != token) {
            self.say("paints are undone newest first", .{});
            return .refused;
        }
        const entry = self.paints.items[@intCast(token)];
        // Backwards, so a cell named twice in one paint ends at its first value.
        var index = entry.cells.len;
        while (index != 0) {
            index -= 1;
            const cell = entry.cells[index];
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = entry.before[index];
        }
        _ = self.applied.pop();
        self.undone.append(self.allocator, token) catch return .failed;
        self.record(.undo_paint, token);
        return .ok;
    }

    fn redoPaint(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        if (self.undone.items.len == 0 or self.undone.items[self.undone.items.len - 1] != token) {
            self.say("paints are redone in the order they were undone", .{});
            return .refused;
        }
        for (self.paints.items[@intCast(token)].cells) |cell| {
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = cell.tile;
        }
        _ = self.undone.pop();
        self.applied.append(self.allocator, token) catch return .failed;
        self.record(.redo_paint, token);
        return .ok;
    }

    fn screenToWorld(ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status {
        const self = from(ptr);
        if (!self.onMap(sx, sy)) return .refused;
        wx.* = sx;
        wy.* = sy;
        return .ok;
    }

    fn worldToTile(ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status {
        const self = from(ptr);
        if (!self.onMap(wx, wy)) return .refused;
        tx.* = @intFromFloat(@floor(wx / tile_size));
        ty.* = @intFromFloat(@floor(wy / tile_size));
        return .ok;
    }

    fn worldToMap(ptr: *anyopaque, wx: f32, wy: f32, mx: *f32, my: *f32) Status {
        const self = from(ptr);
        mx.* = wx * self.map_per_world;
        my.* = wy * self.map_per_world;
        return .ok;
    }

    fn objectAt(ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status {
        const self = from(ptr);
        // The last one listed wins, as the topmost drawn would.
        var index = self.objects_list.items.len;
        while (index != 0) {
            index -= 1;
            const object = self.objects_list.items[index];
            if (!object.known) continue; // never placed, so never drawn
            // The screen is the world here; the object is drawn at its map
            // position's world point.
            const x = object.x / self.map_per_world;
            const y = object.y / self.map_per_world;
            if (@abs(x - sx) <= pick_radius and @abs(y - sy) <= pick_radius) {
                link_id.* = object.link_id;
                return .ok;
            }
        }
        return .refused;
    }
};

/// The same three fixture objects every fake-bridge test starts from: a known
/// tank, a known object another holds a reference to, and an object of a type
/// the object database does not know. Public so later tasks' editor tests can
/// reuse it instead of copying it.
pub fn fixture(allocator: std.mem.Allocator) !FakeBridge {
    var fake = FakeBridge.init(allocator, 8, 8, 2);
    errdefer fake.deinit();
    var tank: ObjectRecord = .{ .link_id = 1, .x = 40, .y = 40, .dir = 0, .player = 0 };
    tank.setName("T34");
    try fake.addFixture(tank, false);
    var bridge_span: ObjectRecord = .{ .link_id = 2, .x = 100, .y = 40, .dir = 0, .player = 0 };
    bridge_span.setName("Bridge_Span");
    try fake.addFixture(bridge_span, true); // referred to, like a span named by a bridge
    var mystery: ObjectRecord = .{ .link_id = 3, .x = 150, .y = 150, .dir = 0, .player = 1, .known = false };
    mystery.setName("No_Such_Object");
    try fake.addFixture(mystery, false);
    try fake.addVsoDescriptorFixture(.road, "rail_road_grass");
    try fake.addVsoDescriptorFixture(.road, "road_track");
    try fake.addVsoDescriptorFixture(.river, "defaultriver");
    return fake;
}

test "the fake refuses what the bridge refuses" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    try std.testing.expectEqual(Status.ok, b.openMap("fixture.bzm", &info));
    try std.testing.expectEqual(Status.refused, b.deleteObject(2)); // referenced
    try std.testing.expectEqual(Status.refused, b.deleteObject(3)); // unknown type
    try std.testing.expectEqual(Status.refused, b.placeObject(3, 10, 10, 0, 1)); // unknown type
    try std.testing.expectEqual(Status.refused, b.placeObject(1, -5, 10, 0, 0)); // off the map
    var link: i32 = -1;
    try std.testing.expectEqual(Status.refused, b.addObject("T34", 9999, 10, 0, 0, &link));
    try std.testing.expectEqual(Status.bad_argument, b.setDiplomacy(7, 0));
    try std.testing.expectEqual(Status.bad_argument, b.setDiplomacy(0, 3));
    try std.testing.expectEqual(Status.bad_argument, b.setDiplomacy(0, -1));
    try std.testing.expectEqual(Status.bad_argument, b.setAttackingSide(2));
}

test "the fake never reuses a deleted object's link ID" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    try std.testing.expectEqual(Status.ok, b.deleteObject(1));
    var added: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.addObject("T34", 60, 60, 0, 0, &added));
    try std.testing.expect(added > 3);
    try std.testing.expectEqual(Status.ok, b.restoreObject(1));
    try std.testing.expectEqual(Status.refused, b.restoreObject(1));
}

test "the fake's paints undo newest first and redo in undo order" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var first: i32 = -1;
    var second: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 5 }}, &first));
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 6 }}, &second));
    try std.testing.expectEqual(Status.refused, b.undoPaint(first));
    try std.testing.expectEqual(Status.ok, b.undoPaint(second));
    try std.testing.expectEqual(@as(u8, 5), fake.tile(1, 1));
    try std.testing.expectEqual(Status.ok, b.undoPaint(first));
    try std.testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    try std.testing.expectEqual(Status.refused, b.redoPaint(second));
    try std.testing.expectEqual(Status.ok, b.redoPaint(first));
    try std.testing.expectEqual(Status.refused, b.paint(&.{.{ .x = 8, .y = 0, .tile = 1 }}, &first)); // off the map
}

test "a reopen forgets deleted objects and paint tokens, as the bridge's does" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var token: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 5 }}, &token));
    try std.testing.expectEqual(Status.ok, b.deleteObject(1));
    try std.testing.expectEqual(Status.ok, b.openMap("fixture.bzm", &info));
    try std.testing.expectEqual(Status.refused, b.undoPaint(token));
    try std.testing.expectEqual(Status.refused, b.restoreObject(1));
}

test "the fake refuses an edit of a link ID two objects share" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    var twin: ObjectRecord = .{ .link_id = 1, .x = 60, .y = 60 };
    twin.setName("Flowers");
    try fake.addFixture(twin, false);
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    try std.testing.expectEqual(Status.refused, b.deleteObject(1));
    try std.testing.expectEqual(Status.refused, b.placeObject(1, 50, 50, 0, 0));
    try std.testing.expectEqual(@as(usize, 4), fake.objects_list.items.len);
}

test "the fake lists objects the way BkEditorObjects does" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var total: usize = 0;
    var none: [0]ObjectRecord = .{};
    try std.testing.expectEqual(Status.refused, b.objects(&none, &total));
    try std.testing.expectEqual(@as(usize, 3), total);
    var out: [3]ObjectRecord = undefined;
    try std.testing.expectEqual(Status.ok, b.objects(&out, &total));
    try std.testing.expectEqualStrings("T34", out[0].nameSlice());
    try std.testing.expect(!out[2].known);
}

test "the fake's camera anchors refuse what the bridge refuses and put exactly" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    try std.testing.expectEqual(Status.ok, b.openMap("fixture.bzm", &info));
    var value: records.Value = undefined;
    try std.testing.expectEqual(Status.ok, b.readRecord(.camera_anchors, 0, std.testing.allocator, &value));
    try std.testing.expectEqual(@as(u32, 0), value.camera_anchors.player_count);
    var wanted: records.CameraAnchors = .{};
    wanted.player_count = 3;
    wanted.players[2] = .{ .x = 100, .y = 100, .z = 0 };
    var put: records.Value = .{ .camera_anchors = wanted };
    try std.testing.expectEqual(Status.ok, b.putRecord(0, &put));
    try std.testing.expectEqual(@as(u32, 3), fake.camera_anchors.player_count);
    // Off the map, a count over 32 and a non-finite value.
    put.camera_anchors.players[1] = .{ .x = 9999, .y = 1, .z = 0 };
    try std.testing.expectEqual(Status.refused, b.putRecord(0, &put));
    try std.testing.expect(fake.camera_anchors.players[1].isUnset());
    put.camera_anchors.players[1] = .{};
    put.camera_anchors.player_count = 33;
    try std.testing.expectEqual(Status.bad_argument, b.putRecord(0, &put));
    put.camera_anchors.player_count = 1;
    put.camera_anchors.players[0] = .{ .x = std.math.nan(f32), .y = 0, .z = 0 };
    try std.testing.expectEqual(Status.bad_argument, b.putRecord(0, &put));
    // An exact put shrinks: what undo needs.
    put.camera_anchors = .{};
    try std.testing.expectEqual(Status.ok, b.putRecord(0, &put));
    try std.testing.expectEqual(@as(u32, 0), fake.camera_anchors.player_count);
    var z: f32 = 5;
    try std.testing.expectEqual(Status.ok, b.groundHeight(10, 10, &z));
    try std.testing.expectEqual(@as(f32, 0), z);
    try std.testing.expectEqual(Status.refused, b.groundHeight(-1, 10, &z));
}
