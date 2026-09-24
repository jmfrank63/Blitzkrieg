//! An in-memory map behind the Bridge interface, for the core tier. It
//! follows the real bridge's rules where the core can see them - refusals,
//! link IDs, the order paints undo and redo in, the object list's shape - and
//! nothing else: there is no terrain function, so a paint sets exactly the
//! cells it names. Screen and world coordinates are the same thing here.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const Status = bridge_mod.Status;
const MapInfo = bridge_mod.MapInfo;
const ObjectRecord = bridge_mod.ObjectRecord;
const PaintCell = bridge_mod.PaintCell;
const Bridge = bridge_mod.Bridge;

/// World units per tile, standing in for the engine's own conversion.
pub const tile_size: f32 = 32.0;
/// How far from an object's centre a point still picks it.
pub const pick_radius: f32 = 16.0;

pub const CallKind = enum { open, save, add, place, delete, restore, diplomacy, map_type, attacking_side, paint, undo_paint, redo_paint };
pub const Call = struct { kind: CallKind, id: i32 = 0 };

const Tombstone = struct { record: ObjectRecord, index: usize };
/// A mutable empty slice to start from; Allocator.free ignores a zero length.
var no_tiles: [0]u8 = .{};
const PaintRecord = struct { cells: []PaintCell, before: []u8 };

pub const FakeBridge = struct {
    allocator: std.mem.Allocator,
    info: MapInfo,
    objects_list: std.ArrayListUnmanaged(ObjectRecord) = .empty,
    referenced: std.AutoHashMapUnmanaged(i32, void) = .empty,
    tombstones: std.AutoHashMapUnmanaged(i32, Tombstone) = .empty,
    diplomacy_table: std.ArrayListUnmanaged(i32) = .empty,
    tiles: []u8 = &no_tiles,
    paints: std.ArrayListUnmanaged(PaintRecord) = .empty,
    applied: std.ArrayListUnmanaged(i32) = .empty,
    undone: std.ArrayListUnmanaged(i32) = .empty,
    link_floor: i32 = 0,
    calls: std.ArrayListUnmanaged(Call) = .empty,
    message_buffer: [160]u8 = undefined,
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
        self.referenced.deinit(self.allocator);
        self.tombstones.deinit(self.allocator);
        self.diplomacy_table.deinit(self.allocator);
        self.calls.deinit(self.allocator);
        self.allocator.free(self.tiles);
        self.* = undefined;
    }

    /// An object in the map before it opens; `referenced` makes its delete
    /// refused, as a bridge or start command holding it would.
    pub fn addFixture(self: *FakeBridge, new_record: ObjectRecord, referenced: bool) !void {
        try self.objects_list.append(self.allocator, new_record);
        if (referenced) try self.referenced.put(self.allocator, new_record.link_id, {});
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

    fn indexOf(self: *const FakeBridge, link_id: i32) ?usize {
        for (self.objects_list.items, 0..) |object, index| {
            if (object.link_id == link_id) return index;
        }
        return null;
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
        .objectAt = objectAt,
    };

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
        if (!self.onMap(x, y)) {
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
        if (!object.known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        if (!self.onMap(x, y)) {
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
        if (self.referenced.contains(link_id)) {
            self.say("still referred to by bridge 0", .{});
            return .refused;
        }
        const removed = self.objects_list.orderedRemove(index);
        self.tombstones.put(self.allocator, link_id, .{ .record = removed, .index = index }) catch return .failed;
        self.link_floor = @max(self.link_floor, link_id + 1);
        self.record(.delete, link_id);
        return .ok;
    }

    fn restoreObject(ptr: *anyopaque, link_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const tombstone = self.tombstones.get(link_id) orelse {
            self.say("no deleted object has that link ID", .{});
            return .refused;
        };
        if (self.indexOf(link_id) != null) {
            self.say("the link ID is in use again", .{});
            return .refused;
        }
        const index = @min(tombstone.index, self.objects_list.items.len);
        self.objects_list.insert(self.allocator, index, tombstone.record) catch return .failed;
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

    fn objectAt(ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status {
        const self = from(ptr);
        // The last one listed wins, as the topmost drawn would.
        var index = self.objects_list.items.len;
        while (index != 0) {
            index -= 1;
            const object = self.objects_list.items[index];
            if (!object.known) continue; // never placed, so never drawn
            if (@abs(object.x - sx) <= pick_radius and @abs(object.y - sy) <= pick_radius) {
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
