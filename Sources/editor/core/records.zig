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
    /// A reinforcement group (04-09, D-16): keyed by its group ID, holding the
    /// script IDs of the objects the game holds back for it.
    group,
    /// The map's script file (04-10, D-20): one bare name, a singleton.
    script_file,
    /// A script area (04-10, D-21): keyed by its index in the map's list, the
    /// list the game's Lua finds by name.
    script_area,
    /// A start command (04-11, D-17): keyed by its index in the map's list, the
    /// orders the game gives units when a mission starts.
    start_command,
    /// An artillery reserve position (04-11, D-18): keyed by its index in the
    /// map's list, where the game puts a gun (and the truck towing it) at the start.
    reserve_position,
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

/// The most script IDs a group may name is the number of script IDs there
/// are: 0..32000 (an object's own is -1 for none, which no group may hold:
/// GetGroupById(-1) would match it, Pitfall 9).
pub const min_script_id: i32 = 0;
pub const max_script_id: i32 = 32000;

/// One reinforcement group (D-16): its ID and the script IDs it holds, in the
/// order the file has them. When it sits in a `Value` the slice is OWNED by
/// that value (`Value.deinit` frees it); a bare `Group` built by a caller
/// borrows.
pub const Group = struct {
    id: i32 = 0,
    ids: []const i32 = &.{},

    pub fn has(self: Group, script_id: i32) bool {
        return std.mem.indexOfScalar(i32, self.ids, script_id) != null;
    }

    pub fn eql(a: Group, b: Group) bool {
        return a.id == b.id and std.mem.eql(i32, a.ids, b.ids);
    }
};

/// BkEditorScriptFileRecord's name capacity: 63 characters and the NUL.
pub const script_file_capacity = 64;

/// The map's script file (D-20): the bare name of the Lua file beside the map,
/// empty for None. A value read from a file is kept verbatim until the user
/// changes it, so it may hold a folder or ".lua" - the bridge refuses such a
/// value only when it is NEW.
pub const ScriptFile = struct {
    name: [script_file_capacity]u8 = [_]u8{0} ** script_file_capacity,

    pub fn nameSlice(self: *const ScriptFile) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    /// Copies `text`, cut to what the record holds.
    pub fn setName(self: *ScriptFile, text: []const u8) void {
        const len = @min(text.len, script_file_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }

    pub fn eql(a: ScriptFile, b: ScriptFile) bool {
        return std.mem.eql(u8, std.mem.sliceTo(&a.name, 0), std.mem.sliceTo(&b.name, 0));
    }
};

/// BkEditorScriptAreaRecord's name capacity: 63 characters and the NUL.
pub const area_name_capacity = 64;

/// The file's own enum values (SScriptArea::EAreaTypes).
pub const AreaShape = enum(i32) { rectangle = 0, circle = 1 };

/// One script area as the file holds it, in MAP (AI) units - never world units:
/// a rectangle has its centre (cx, cy) and half size (hx, hy), a circle its
/// centre and radius r; the fields a shape does not use keep what the file had.
/// A new or edited area is converted from a drag by the bridge (the MFC editor's
/// truncation, once); the core only carries the result. `name` is non-empty and
/// unique among the map's areas, case-sensitive, which the bridge enforces.
pub const ScriptArea = struct {
    name: [area_name_capacity]u8 = [_]u8{0} ** area_name_capacity,
    shape: AreaShape = .rectangle,
    cx: f32 = 0,
    cy: f32 = 0,
    hx: f32 = 0,
    hy: f32 = 0,
    r: f32 = 0,

    pub fn nameSlice(self: *const ScriptArea) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    /// Copies `text`, cut to what the record holds.
    pub fn setName(self: *ScriptArea, text: []const u8) void {
        const len = @min(text.len, area_name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }

    pub fn eql(a: ScriptArea, b: ScriptArea) bool {
        return std.mem.eql(u8, std.mem.sliceTo(&a.name, 0), std.mem.sliceTo(&b.name, 0)) and a.shape == b.shape and
            a.cx == b.cx and a.cy == b.cy and a.hx == b.hx and a.hy == b.hy and a.r == b.r;
    }
};

/// STOP, entry 9 of Data/Editor/actions.ini: the type a new start command
/// starts as (CAISCHelper::DEFAULT_ACTION_COMMAND_INDEX; the bridge reports the
/// default's real place in the list).
pub const action_stop: i32 = 9;

/// The MFC's rounding of one coordinate that is already in map (AI) units:
/// Vis2AI cuts int(v + 0.3). A point clicked on the ground becomes a start
/// command's target or a reserve position's place this way; `BkEditorWorldToMap`
/// leaves the conversion to AI units unrounded, so the core applies the cut.
/// A value that is not finite or too large for an integer is returned as it is.
pub fn truncateToAi(value: f32) f32 {
    if (!std.math.isFinite(value) or @abs(value) > 1.0e9) return value;
    return @floatFromInt(@as(i32, @intFromFloat(value + 0.3)));
}

test "truncateToAi cuts int(v + 0.3), towards zero, as Vis2AI does" {
    try std.testing.expectEqual(@as(f32, 141), truncateToAi(141.42));
    try std.testing.expectEqual(@as(f32, 141), truncateToAi(140.75));
    try std.testing.expectEqual(@as(f32, 140), truncateToAi(140.69));
    try std.testing.expectEqual(@as(f32, 0), truncateToAi(0.25));
    try std.testing.expectEqual(@as(f32, 0), truncateToAi(-0.25));
    try std.testing.expect(std.math.isNan(truncateToAi(std.math.nan(f32))));
    try std.testing.expectEqual(@as(f32, 2.0e9), truncateToAi(2.0e9));
}

/// One start command as the file holds it (D-17), positions in MAP (AI) units:
/// the action `cmd_type` (an id of actions.ini), its target - the unit
/// `link_id` (0: none, never a reference) or the point (x, y) - a `number`, the
/// file's own `from_explosion` (never editable: a set keeps what the file has)
/// and the `units` it orders, link IDs of the map's units and squads (a
/// soldier stands for his squad). When it sits in a `Value` the slice is OWNED
/// by that value (`Value.deinit` frees it); a bare `StartCommand` a caller
/// builds borrows.
pub const StartCommand = struct {
    cmd_type: i32 = action_stop,
    link_id: i32 = 0,
    x: f32 = 0,
    y: f32 = 0,
    from_explosion: bool = false,
    number: f32 = 0,
    units: []const i32 = &.{},

    pub fn has(self: StartCommand, link: i32) bool {
        return std.mem.indexOfScalar(i32, self.units, link) != null;
    }

    pub fn eql(a: StartCommand, b: StartCommand) bool {
        return a.cmd_type == b.cmd_type and a.link_id == b.link_id and a.x == b.x and a.y == b.y and
            a.from_explosion == b.from_explosion and a.number == b.number and std.mem.eql(i32, a.units, b.units);
    }
};

/// One artillery reserve position as the file holds it (D-18): the gun's link ID
/// (`artillery`, above 0) and the truck's (`truck`, 0 for none - never a
/// reference), and the place (x, y) in MAP (AI) units. The bridge checks the roles
/// (a self-propelled or towed gun, a truck able to tow it) because that needs the
/// object database; the core carries the record.
pub const ReservePosition = struct {
    artillery: i32 = 0,
    truck: i32 = 0,
    x: f32 = 0,
    y: f32 = 0,

    pub fn eql(a: ReservePosition, b: ReservePosition) bool {
        return a.artillery == b.artillery and a.truck == b.truck and a.x == b.x and a.y == b.y;
    }
};

/// One whole record of some kind. The history owns the values it holds and
/// frees them with `deinit`; the camera anchors and the script file own no
/// memory, a group owns its script-ID list and a start command its unit list,
/// and the three functions let the history free, copy and compare any value of
/// any kind.
pub const Value = union(Kind) {
    camera_anchors: CameraAnchors,
    group: Group,
    script_file: ScriptFile,
    script_area: ScriptArea,
    start_command: StartCommand,
    reserve_position: ReservePosition,

    pub fn deinit(self: *Value, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .camera_anchors, .script_file, .script_area, .reserve_position => {},
            .group => |group| {
                allocator.free(group.ids);
                self.* = .{ .group = .{ .id = group.id } };
            },
            .start_command => |command| {
                allocator.free(command.units);
                var emptied = command;
                emptied.units = &.{};
                self.* = .{ .start_command = emptied };
            },
        }
    }

    pub fn clone(self: Value, allocator: std.mem.Allocator) std.mem.Allocator.Error!Value {
        return switch (self) {
            .camera_anchors => |anchors| .{ .camera_anchors = anchors },
            .script_file => |file| .{ .script_file = file },
            .script_area => |area| .{ .script_area = area },
            .reserve_position => |position| .{ .reserve_position = position },
            .group => |group| .{ .group = .{ .id = group.id, .ids = try allocator.dupe(i32, group.ids) } },
            .start_command => |command| blk: {
                var copy = command;
                copy.units = try allocator.dupe(i32, command.units);
                break :blk .{ .start_command = copy };
            },
        };
    }

    pub fn eql(a: Value, b: Value) bool {
        if (std.meta.activeTag(a) != std.meta.activeTag(b)) return false;
        return switch (a) {
            .camera_anchors => |left| left.eql(b.camera_anchors),
            .group => |left| left.eql(b.group),
            .script_file => |left| left.eql(b.script_file),
            .script_area => |left| left.eql(b.script_area),
            .start_command => |left| left.eql(b.start_command),
            .reserve_position => |left| left.eql(b.reserve_position),
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

test "a group value owns, clones, compares and frees its script IDs" {
    const allocator = std.testing.allocator;
    var first: Value = .{ .group = .{ .id = 3, .ids = try allocator.dupe(i32, &.{ 10, 20, 30 }) } };
    defer first.deinit(allocator);
    var copy = try first.clone(allocator);
    defer copy.deinit(allocator);
    try std.testing.expect(first.eql(copy));
    try std.testing.expect(first.group.ids.ptr != copy.group.ids.ptr);
    try std.testing.expect(first.group.has(20));
    try std.testing.expect(!first.group.has(-1));
    // Order is part of the value: the file keeps it.
    var reordered: Value = .{ .group = .{ .id = 3, .ids = try allocator.dupe(i32, &.{ 20, 10, 30 }) } };
    defer reordered.deinit(allocator);
    try std.testing.expect(!first.eql(reordered));
    // The ID is part of it too, and a group is never a camera value.
    var other: Value = .{ .group = .{ .id = 4, .ids = try allocator.dupe(i32, &.{ 10, 20, 30 }) } };
    defer other.deinit(allocator);
    try std.testing.expect(!first.eql(other));
    try std.testing.expect(!first.eql(.{ .camera_anchors = .{} }));
    // An empty group is a value with nothing to free.
    var empty: Value = .{ .group = .{ .id = 0 } };
    var empty_copy = try empty.clone(allocator);
    try std.testing.expect(empty.eql(empty_copy));
    empty_copy.deinit(allocator);
    empty.deinit(allocator);
}

test "a script file value compares by its text and never by the padding" {
    var first: ScriptFile = .{};
    first.setName("m2_script");
    var second: ScriptFile = .{};
    second.setName("m2_script");
    second.name[40] = 0; // padding past the NUL is not part of the value
    try std.testing.expect(first.eql(second));
    try std.testing.expectEqualStrings("m2_script", first.nameSlice());
    second.setName("other");
    try std.testing.expect(!first.eql(second));
    var long: ScriptFile = .{};
    long.setName("x" ** 100);
    try std.testing.expectEqual(@as(usize, script_file_capacity - 1), long.nameSlice().len);
    const a: Value = .{ .script_file = first };
    var b = try a.clone(std.testing.allocator);
    defer b.deinit(std.testing.allocator);
    try std.testing.expect(a.eql(b));
    try std.testing.expect(!a.eql(.{ .camera_anchors = .{} }));
}

test "a script area value compares by its name and numbers, and never by the padding" {
    var first: ScriptArea = .{ .shape = .circle, .cx = 100, .cy = 120, .r = 10 };
    first.setName("m2_area");
    var second = first;
    second.name[50] = 0; // past the NUL: not part of the value
    try std.testing.expect(first.eql(second));
    try std.testing.expectEqualStrings("m2_area", first.nameSlice());
    second.r = 11;
    try std.testing.expect(!first.eql(second));
    second = first;
    second.shape = .rectangle;
    try std.testing.expect(!first.eql(second));
    second = first;
    second.setName("M2_AREA"); // names are case-sensitive
    try std.testing.expect(!first.eql(second));
    var long: ScriptArea = .{};
    long.setName("n" ** 100);
    try std.testing.expectEqual(@as(usize, area_name_capacity - 1), long.nameSlice().len);
    const a: Value = .{ .script_area = first };
    var b = try a.clone(std.testing.allocator);
    defer b.deinit(std.testing.allocator);
    try std.testing.expect(a.eql(b));
    try std.testing.expect(!a.eql(.{ .script_file = .{} }));
}

test "a start command value owns, clones, compares and frees its units" {
    const allocator = std.testing.allocator;
    var first: Value = .{ .start_command = .{ .cmd_type = 0, .link_id = 7, .x = 10, .y = 20, .number = 2.5, .units = try allocator.dupe(i32, &.{ 3, 4 }) } };
    defer first.deinit(allocator);
    var copy = try first.clone(allocator);
    defer copy.deinit(allocator);
    try std.testing.expect(first.eql(copy));
    try std.testing.expect(first.start_command.units.ptr != copy.start_command.units.ptr);
    try std.testing.expect(first.start_command.has(4));
    try std.testing.expect(!first.start_command.has(0));
    // The order of the units is part of the value: the file keeps it.
    var reordered: Value = .{ .start_command = .{ .cmd_type = 0, .link_id = 7, .x = 10, .y = 20, .number = 2.5, .units = try allocator.dupe(i32, &.{ 4, 3 }) } };
    defer reordered.deinit(allocator);
    try std.testing.expect(!first.eql(reordered));
    // Every field is part of it, the explosion flag too, and a command is never another kind.
    var exploded = try first.clone(allocator);
    defer exploded.deinit(allocator);
    exploded.start_command.from_explosion = true;
    try std.testing.expect(!first.eql(exploded));
    try std.testing.expect(!first.eql(.{ .camera_anchors = .{} }));
    // A command with no units is a value with nothing to free.
    var empty: Value = .{ .start_command = .{} };
    var empty_copy = try empty.clone(allocator);
    try std.testing.expect(empty.eql(empty_copy));
    try std.testing.expectEqual(action_stop, empty.start_command.cmd_type);
    empty_copy.deinit(allocator);
    empty.deinit(allocator);
}

test "a reserve position value compares by every field and is never another kind" {
    const first: Value = .{ .reserve_position = .{ .artillery = 5, .truck = 6, .x = 100, .y = 200 } };
    var copy = try first.clone(std.testing.allocator);
    defer copy.deinit(std.testing.allocator);
    try std.testing.expect(first.eql(copy));
    copy.reserve_position.truck = 0;
    try std.testing.expect(!first.eql(copy));
    copy = first;
    copy.reserve_position.x = 101;
    try std.testing.expect(!first.eql(copy));
    try std.testing.expect(!first.eql(.{ .script_area = .{} }));
}
