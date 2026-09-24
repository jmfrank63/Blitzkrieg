//! The open map as the panels and the history see it: its path, its own
//! fields, the diplomacy table and every object, in the order the map was
//! opened in, with added objects at the end. It holds no terrain - tiles
//! live in the bridge - and no dirty flag: the history knows whether the map
//! differs from what was saved.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const Bridge = bridge_mod.Bridge;
const MapInfo = bridge_mod.MapInfo;
const ObjectRecord = bridge_mod.ObjectRecord;
const EditError = bridge_mod.EditError;

pub const Document = struct {
    path: std.ArrayListUnmanaged(u8) = .empty,
    info: MapInfo = .{},
    diplomacy: std.ArrayListUnmanaged(i32) = .empty,
    objects: std.ArrayListUnmanaged(ObjectRecord) = .empty,

    pub fn deinit(self: *Document, allocator: std.mem.Allocator) void {
        self.path.deinit(allocator);
        self.diplomacy.deinit(allocator);
        self.objects.deinit(allocator);
        self.* = undefined;
    }

    /// Replaces everything with what the bridge holds for the map it just
    /// opened. Asks for the object count first, the way BkEditorObjects is
    /// meant to be called. Everything - the path included - is staged in a
    /// local first; `self` is only touched once nothing else can fail, so a
    /// failed reload leaves `self` exactly as it was.
    pub fn reload(self: *Document, allocator: std.mem.Allocator, b: Bridge, path: []const u8, info: MapInfo) EditError!void {
        var total: usize = 0;
        var none: [0]ObjectRecord = .{};
        const sizing = b.objects(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        var objects: std.ArrayListUnmanaged(ObjectRecord) = .empty;
        errdefer objects.deinit(allocator);
        try objects.resize(allocator, total);
        try bridge_mod.check(b.objects(objects.items, &total));

        var diplomacy: std.ArrayListUnmanaged(i32) = .empty;
        errdefer diplomacy.deinit(allocator);
        try diplomacy.resize(allocator, @intCast(info.player_count));
        for (diplomacy.items, 0..) |*side, player| try bridge_mod.check(b.diplomacy(@intCast(player), side));

        var new_path: std.ArrayListUnmanaged(u8) = .empty;
        errdefer new_path.deinit(allocator);
        try new_path.appendSlice(allocator, path);

        self.path.deinit(allocator);
        self.path = new_path;
        self.objects.deinit(allocator);
        self.objects = objects;
        self.diplomacy.deinit(allocator);
        self.diplomacy = diplomacy;
        self.info = info;
    }

    pub fn indexOf(self: *const Document, link_id: i32) ?usize {
        for (self.objects.items, 0..) |object, index| {
            if (object.link_id == link_id) return index;
        }
        return null;
    }

    pub fn find(self: *Document, link_id: i32) ?*ObjectRecord {
        const index = self.indexOf(link_id) orelse return null;
        return &self.objects.items[index];
    }
};
