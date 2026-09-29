//! The record kinds behind the generic record command (D-02). One command,
//! `record_edit`, carries a whole record before and a whole record after; the
//! Editor reads the before-record through the bridge, puts the after-record
//! through the same bridge call, and undo puts the before-record back through
//! it. A later plan adds a kind here (script_file, group, script_area,
//! start_command, reserve_position, ai_side, and the road, river, bridge and
//! entrenchment kinds), its C calls and its panel - the command, the undo and
//! the fake stay as they are.
//!
//! Units: every position in a record is documented at the record. Camera
//! anchors are WORLD (Vis) units, like the camera; map (AI) units belong to
//! object positions, areas, start-command targets, reserve positions and
//! parcels, and the bridge converts, never the core.
const std = @import("std");

pub const Kind = enum {
    /// The map's camera anchors: one neutral, one per player.
    camera_anchors,
};

/// A world-unit point. The all-zero value is the file's VNULL3: "not set".
pub const Vec3 = struct {
    x: f32 = 0,
    y: f32 = 0,
    z: f32 = 0,

    pub fn isUnset(self: Vec3) bool {
        return self.x == 0 and self.y == 0 and self.z == 0;
    }

    pub fn eql(a: Vec3, b: Vec3) bool {
        return a.x == b.x and a.y == b.y and a.z == b.z;
    }
};

/// BkEditorCameraAnchorRecord's player capacity.
pub const max_camera_players = 32;

/// BkEditorCameraAnchorRecord, world units. `player_count` is the size of the
/// map's playersCameraAnchors vector, 0..32; slots at or above it are unset.
/// The game starts the camera at players[user] and falls back to `neutral`.
pub const CameraAnchors = struct {
    neutral: Vec3 = .{},
    player_count: u32 = 0,
    players: [max_camera_players]Vec3 = [_]Vec3{.{}} ** max_camera_players,

    /// The slot as the game would read it: unset past the end.
    pub fn slot(self: CameraAnchors, index: usize) Vec3 {
        return if (index < self.player_count) self.players[index] else .{};
    }

    /// A copy with player `index` set: the vector pads with unset slots up to
    /// index + 1 and never shrinks (C8), so setting player 2 on a one-anchor
    /// map gives three slots and setting player 0 on an eight-anchor map
    /// keeps all eight. Null for a player the record cannot hold.
    pub fn withPlayer(self: CameraAnchors, index: usize, value: Vec3) ?CameraAnchors {
        if (index >= max_camera_players) return null;
        var out = self;
        out.player_count = @max(self.player_count, @as(u32, @intCast(index + 1)));
        out.players[index] = value;
        return out;
    }

    /// A copy with player `index` unset in place; the vector keeps its size,
    /// and a player past the end is already unset.
    pub fn withPlayerCleared(self: CameraAnchors, index: usize) CameraAnchors {
        var out = self;
        if (index < self.player_count) out.players[index] = .{};
        return out;
    }

    pub fn eql(a: CameraAnchors, b: CameraAnchors) bool {
        if (!a.neutral.eql(b.neutral) or a.player_count != b.player_count) return false;
        for (a.players[0..a.player_count], b.players[0..b.player_count]) |left, right| {
            if (!left.eql(right)) return false;
        }
        return true;
    }
};

/// One whole record of some kind. The history owns the values it holds and
/// frees them with `deinit`; the kinds so far own no memory, and the three
/// functions exist now so the history can free, copy and compare any value the
/// list kinds of later plans put here.
pub const Value = union(Kind) {
    camera_anchors: CameraAnchors,

    pub fn deinit(self: *Value, allocator: std.mem.Allocator) void {
        _ = allocator;
        switch (self.*) {
            .camera_anchors => {},
        }
    }

    pub fn clone(self: Value, allocator: std.mem.Allocator) std.mem.Allocator.Error!Value {
        _ = allocator;
        return switch (self) {
            .camera_anchors => |anchors| .{ .camera_anchors = anchors },
        };
    }

    pub fn eql(a: Value, b: Value) bool {
        if (std.meta.activeTag(a) != std.meta.activeTag(b)) return false;
        return switch (a) {
            .camera_anchors => |left| left.eql(b.camera_anchors),
        };
    }
};

test "withPlayer pads with unset slots and never shrinks" {
    var anchors: CameraAnchors = .{};
    anchors.player_count = 1;
    anchors.players[0] = .{ .x = 10, .y = 20, .z = 1 };
    const grown = anchors.withPlayer(2, .{ .x = 5, .y = 6, .z = 7 }).?;
    try std.testing.expectEqual(@as(u32, 3), grown.player_count);
    try std.testing.expect(grown.players[1].isUnset());
    try std.testing.expectEqual(@as(f32, 5), grown.players[2].x);
    try std.testing.expectEqual(@as(u32, 1), anchors.player_count);
    const kept = grown.withPlayer(0, .{ .x = 1, .y = 1, .z = 1 }).?;
    try std.testing.expectEqual(@as(u32, 3), kept.player_count);
    try std.testing.expect(anchors.withPlayer(max_camera_players, .{}) == null);
}

test "withPlayerCleared unsets in place and keeps the size" {
    var anchors: CameraAnchors = .{};
    anchors.player_count = 2;
    anchors.players[1] = .{ .x = 3, .y = 3, .z = 3 };
    const cleared = anchors.withPlayerCleared(1);
    try std.testing.expectEqual(@as(u32, 2), cleared.player_count);
    try std.testing.expect(cleared.players[1].isUnset());
    try std.testing.expect(anchors.withPlayerCleared(9).eql(anchors));
}

test "values compare by their used slots only" {
    var left: CameraAnchors = .{};
    left.player_count = 1;
    var right = left;
    right.players[5] = .{ .x = 1, .y = 1, .z = 1 }; // past player_count: not part of the value
    try std.testing.expect(left.eql(right));
    right.player_count = 2;
    try std.testing.expect(!left.eql(right));
    const a: Value = .{ .camera_anchors = left };
    var b = try a.clone(std.testing.allocator);
    defer b.deinit(std.testing.allocator);
    try std.testing.expect(a.eql(b));
}
