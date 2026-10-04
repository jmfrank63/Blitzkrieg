//! Check Map (05-05, D-33): the MFC's checks over the data the editor already
//! holds, std-only. The MFC ran them as it fixed (TemplateEditorFrame1.cpp:5952-6390,
//! silently on every save); here a check only REPORTS - a finding names what is
//! wrong and where - and the fixing is a separate, undoable command the editor
//! builds from the same edits a person would make (`Editor.fixAll`).
//!
//! The rules, with the MFC's own for each:
//!  - duplicate objects: the same place, the same type name and the same frame
//!    index (TEF:6006-6071). The later one is the duplicate. A squad is never
//!    one: the MFC skipped a unit that sits in a formation, and a squad's
//!    record IS its formation.
//!  - links: a record whose host (`link_with`) is not on the map is an invalid
//!    link; two records that carry the same link ID are a duplicate link
//!    (TEF:6078-6144 - the MFC deleted both kinds of object; the invalid
//!    link is cleared here instead, the evident intent, and a shared link ID
//!    is reported only: the bridge keeps every edit away from such a record).
//!    Link ID 0 is "no link" and never counts.
//!  - an owner outside the diplomacy table (TEF:6010-6014): re-mapped to the
//!    neutral, the table's last entry.
//!  - a unit-creation party the party table does not list (TEF:6330-6360).
//!  - M3's additions (D-33): an object whose type the database does not know
//!    (the M1 warning, which replaces the MFC's RemoveNonExistingObjects), and a
//!    road or river with fewer than two control points - the record that
//!    crashed the game's loader (the carried railroad bug).
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const records = @import("records.zig");
const ObjectRecord = bridge_mod.ObjectRecord;

pub const Kind = enum {
    duplicate_object,
    invalid_link,
    duplicate_link,
    player_index,
    unknown_party,
    unknown_object_type,
    short_vso,

    /// The log's heading for the group, in the MFC's own words where it had them.
    pub fn heading(self: Kind) [:0]const u8 {
        return switch (self) {
            .duplicate_object => "Double objects:",
            .invalid_link => "Invalid links:",
            .duplicate_link => "Double links:",
            .player_index => "Invalid player numbers:",
            .unknown_party => "Invalid unit creation info player parties:",
            .unknown_object_type => "Objects the object database does not know:",
            .short_vso => "Roads and rivers with fewer than two control points:",
        };
    }
};

pub const detail_capacity = 160;

/// One thing wrong. Object findings carry the record's link ID, its MAP (AI)
/// units place and its index in the document's object list; a party finding the
/// player; a road or river its kind (0 road, 1 river), its index and the WORLD
/// place of its first control point (`world` is true then, and `x`/`y` are
/// world units). `detail` is the log line: the MFC's own "%s, pos: [%.2f, %.2f],
/// scriptID: %d" for an object, the place in tiles.
pub const Finding = struct {
    kind: Kind,
    link_id: i32 = 0,
    object_index: usize = 0,
    x: f32 = 0,
    y: f32 = 0,
    world: bool = false,
    player: i32 = -1,
    vso_kind: u8 = 0,
    vso_index: usize = 0,
    detail: [detail_capacity]u8 = [_]u8{0} ** detail_capacity,
    detail_len: usize = 0,

    pub fn text(self: *const Finding) []const u8 {
        return self.detail[0..self.detail_len];
    }

    fn setText(self: *Finding, comptime fmt: []const u8, args: anytype) void {
        const written = std.fmt.bufPrint(&self.detail, fmt, args) catch self.detail[0..0];
        self.detail_len = written.len;
    }

    /// Whether the fix waits for an explicit say-so (D-33): removing an object the
    /// editor cannot even show, or a road it cannot edit, is not something Fix all
    /// does unasked. A duplicate's removal is the MFC's own and is not asked.
    pub fn needsConfirmation(self: Finding) bool {
        return self.kind == .unknown_object_type or self.kind == .short_vso;
    }
};

/// A road or river with fewer than two control points, as the app read it.
pub const ShortVso = struct { kind: u8, index: usize, x: f32 = 0, y: f32 = 0, control_points: usize = 0 };

/// What the checks read; nothing here is owned by the checks.
pub const Reads = struct {
    objects: []const ObjectRecord,
    /// The diplomacy table's length: the players and the neutral.
    players: usize,
    /// partys.xml's names; an empty list means the table could not be read and the
    /// party check is skipped rather than failing every party.
    parties: []const []const u8 = &.{},
    /// The party of each unit-creation entry, in player order.
    unit_parties: []const []const u8 = &.{},
    /// The type names of squads: a duplicate check never calls one.
    squad_names: []const []const u8 = &.{},
    short_vsos: []const ShortVso = &.{},
};

/// MAP units to a tile in the log's positions (`2 * SAIConsts::TILE_SIZE`).
pub const map_units_per_tile: f32 = 64.0;

const Place = struct { x: u32, y: u32 };

fn placeOf(object: ObjectRecord) Place {
    return .{ .x = @bitCast(object.x), .y = @bitCast(object.y) };
}

fn isSquad(reads: Reads, name: []const u8) bool {
    for (reads.squad_names) |squad| {
        if (std.mem.eql(u8, squad, name)) return true;
    }
    return false;
}

fn objectFinding(kind: Kind, object: ObjectRecord, index: usize) Finding {
    var finding: Finding = .{ .kind = kind, .link_id = object.link_id, .object_index = index, .x = object.x, .y = object.y, .player = object.player };
    finding.setText("{s}, pos: [{d:.2}, {d:.2}], scriptID: {d}", .{ object.nameSlice(), object.x / map_units_per_tile, object.y / map_units_per_tile, object.script_id });
    return finding;
}

/// Every finding over `reads`, grouped by kind in the order of `Kind`, each group
/// in the order of the document's objects (or of the lists the other reads
/// came in). The caller frees the slice.
pub fn checkMap(allocator: std.mem.Allocator, reads: Reads) std.mem.Allocator.Error![]Finding {
    var found: std.ArrayListUnmanaged(Finding) = .empty;
    errdefer found.deinit(allocator);

    // Duplicate objects: a hash of the place finds the candidates; the later one
    // of two alike is the duplicate. A squad is skipped, and an object whose link
    // ID is not its own (0) is still reported - the fix refuses it, saying so.
    {
        var by_place: std.AutoHashMapUnmanaged(Place, std.ArrayListUnmanaged(usize)) = .empty;
        defer {
            var it = by_place.valueIterator();
            while (it.next()) |list| list.deinit(allocator);
            by_place.deinit(allocator);
        }
        for (reads.objects, 0..) |object, index| {
            const entry = try by_place.getOrPut(allocator, placeOf(object));
            if (!entry.found_existing) entry.value_ptr.* = .empty;
            if (!isSquad(reads, object.nameSlice())) {
                for (entry.value_ptr.items) |earlier| {
                    const other = reads.objects[earlier];
                    if (std.mem.eql(u8, other.nameSlice(), object.nameSlice()) and other.frame_index == object.frame_index) {
                        try found.append(allocator, objectFinding(.duplicate_object, object, index));
                        break;
                    }
                }
            }
            try entry.value_ptr.append(allocator, index);
        }
    }

    // Links: the IDs the map holds, then every record's host against them.
    {
        var ids: std.AutoHashMapUnmanaged(i32, void) = .empty;
        defer ids.deinit(allocator);
        for (reads.objects) |object| {
            if (object.link_id > 0) try ids.put(allocator, object.link_id, {});
        }
        for (reads.objects, 0..) |object, index| {
            if (object.link_with > 0 and !ids.contains(object.link_with)) {
                try found.append(allocator, objectFinding(.invalid_link, object, index));
            }
        }
        var seen: std.AutoHashMapUnmanaged(i32, void) = .empty;
        defer seen.deinit(allocator);
        for (reads.objects, 0..) |object, index| {
            if (object.link_id <= 0) continue;
            const entry = try seen.getOrPut(allocator, object.link_id);
            if (entry.found_existing) try found.append(allocator, objectFinding(.duplicate_link, object, index));
        }
    }

    // An owner outside the table: the neutral (the last entry) is where it goes.
    if (reads.players > 0) {
        for (reads.objects, 0..) |object, index| {
            if (object.player < 0 or @as(usize, @intCast(object.player)) >= reads.players) {
                var finding = objectFinding(.player_index, object, index);
                finding.setText("{d}->{d}, {s}, pos: [{d:.2}, {d:.2}], scriptID: {d}", .{ object.player, reads.players - 1, object.nameSlice(), object.x / map_units_per_tile, object.y / map_units_per_tile, object.script_id });
                try found.append(allocator, finding);
            }
        }
    }

    // A unit-creation party the table does not list.
    if (reads.parties.len > 0) {
        for (reads.unit_parties, 0..) |party, player| {
            var listed = false;
            for (reads.parties) |name| {
                if (std.mem.eql(u8, name, party)) listed = true;
            }
            if (listed) continue;
            var finding: Finding = .{ .kind = .unknown_party, .player = @intCast(player) };
            finding.setText("Player: {d}, party \"{s}\" is not in partys.xml, part set to: {s}", .{ player, party, default_party });
            try found.append(allocator, finding);
        }
    }

    // M3's own: an object the database does not know, a road or river too short to load.
    for (reads.objects, 0..) |object, index| {
        if (!object.known) try found.append(allocator, objectFinding(.unknown_object_type, object, index));
    }
    for (reads.short_vsos) |vso| {
        var finding: Finding = .{ .kind = .short_vso, .x = vso.x, .y = vso.y, .world = vso.control_points > 0, .vso_kind = vso.kind, .vso_index = vso.index };
        finding.setText("{s} {d} has {d} control point(s)", .{ if (vso.kind == 0) "road" else "river", vso.index, vso.control_points });
        try found.append(allocator, finding);
    }

    // Grouped by kind (the log's own order), each group in discovery order.
    std.mem.sort(Finding, found.items, {}, lessByKind);
    return found.toOwnedSlice(allocator);
}

/// The party a bad one is set to (SUnitCreationInfo::DEFAULT_PARTY_NAME).
pub const default_party = "USSR";

fn lessByKind(_: void, left: Finding, right: Finding) bool {
    return @intFromEnum(left.kind) < @intFromEnum(right.kind);
}

/// `checkmap_log.txt`'s text: a heading per kind that has findings, one line per
/// finding under it (the MFC's own layout), or one line saying all is well.
pub fn writeLog(writer: *std.Io.Writer, findings: []const Finding) std.Io.Writer.Error!void {
    if (findings.len == 0) {
        try writer.writeAll("Check Map: no problems found\n");
        return;
    }
    var current: ?Kind = null;
    for (findings) |finding| {
        if (current == null or current.? != finding.kind) {
            if (current != null) try writer.writeAll("\n");
            try writer.print("{s}\n", .{finding.kind.heading()});
            current = finding.kind;
        }
        try writer.print("{s}\n", .{finding.text()});
    }
}

/// How many findings of `kind`.
pub fn count(findings: []const Finding, kind: Kind) usize {
    var total: usize = 0;
    for (findings) |finding| {
        if (finding.kind == kind) total += 1;
    }
    return total;
}

// ---------------------------------------------------------------------------
// Tests: every kind on a table of fixtures, the MFC's rule for each.
// ---------------------------------------------------------------------------

fn fixtureObject(link_id: i32, name: []const u8, x: f32, y: f32, player: i32) ObjectRecord {
    var record: ObjectRecord = .{ .link_id = link_id, .x = x, .y = y, .player = player };
    record.setName(name);
    return record;
}

test "two objects at the same place with the same type and frame are a duplicate: the later one" {
    const allocator = std.testing.allocator;
    const objects = [_]ObjectRecord{
        fixtureObject(1, "T-34", 640, 640, 0),
        fixtureObject(2, "T-34", 640, 640, 0), // the duplicate
        fixtureObject(3, "T-34", 704, 640, 0), // another place
        fixtureObject(4, "Pak40", 640, 640, 0), // another type
        fixtureObject(5, "T-34", 640, 640, 0), // a second duplicate of the first
    };
    var other_frame = fixtureObject(6, "T-34", 640, 640, 0);
    other_frame.frame_index = 3; // another frame: no duplicate
    const all = objects ++ [_]ObjectRecord{other_frame};
    const findings = try checkMap(allocator, .{ .objects = &all, .players = 3 });
    defer allocator.free(findings);
    try std.testing.expectEqual(@as(usize, 2), count(findings, .duplicate_object));
    try std.testing.expectEqual(@as(usize, 2), findings.len);
    try std.testing.expectEqual(@as(i32, 2), findings[0].link_id);
    try std.testing.expectEqual(@as(usize, 1), findings[0].object_index);
    try std.testing.expectEqual(@as(i32, 5), findings[1].link_id);
    try std.testing.expectEqualStrings("T-34, pos: [10.00, 10.00], scriptID: -1", findings[0].text());
}

test "a squad is never a duplicate: its record is its formation" {
    const allocator = std.testing.allocator;
    const objects = [_]ObjectRecord{ fixtureObject(1, "US_sniper", 64, 64, 0), fixtureObject(2, "US_sniper", 64, 64, 0) };
    const squads = [_][]const u8{"US_sniper"};
    const findings = try checkMap(allocator, .{ .objects = &objects, .players = 3, .squad_names = &squads });
    defer allocator.free(findings);
    try std.testing.expectEqual(@as(usize, 0), findings.len);
}

test "a host that is not on the map is an invalid link; a shared link ID is a duplicate link; link ID 0 is no link" {
    const allocator = std.testing.allocator;
    var passenger = fixtureObject(1, "US_rifle", 64, 64, 0);
    passenger.link_with = 99; // no object 99
    var fine = fixtureObject(2, "US_rifle", 128, 64, 0);
    fine.link_with = 3; // the house
    const house = fixtureObject(3, "A_H01_1", 256, 256, 0);
    var zero_a = fixtureObject(0, "Tree", 1, 1, 0);
    zero_a.link_with = 0;
    const zero_b = fixtureObject(0, "Tree", 2, 2, 0);
    const shared = fixtureObject(3, "Fence", 500, 500, 0); // link ID 3 again
    const objects = [_]ObjectRecord{ passenger, fine, house, zero_a, zero_b, shared };
    const findings = try checkMap(allocator, .{ .objects = &objects, .players = 3 });
    defer allocator.free(findings);
    try std.testing.expectEqual(@as(usize, 1), count(findings, .invalid_link));
    try std.testing.expectEqual(@as(usize, 1), count(findings, .duplicate_link));
    try std.testing.expectEqual(@as(usize, 2), findings.len);
    try std.testing.expectEqual(Kind.invalid_link, findings[0].kind);
    try std.testing.expectEqual(@as(i32, 1), findings[0].link_id);
    try std.testing.expectEqual(Kind.duplicate_link, findings[1].kind);
    try std.testing.expectEqual(@as(usize, 5), findings[1].object_index);
}

test "an owner outside the table is a player-index finding naming the neutral it goes to" {
    const allocator = std.testing.allocator;
    const objects = [_]ObjectRecord{ fixtureObject(1, "T-34", 64, 64, 9), fixtureObject(2, "T-34", 128, 64, 2), fixtureObject(3, "T-34", 192, 64, -1) };
    const findings = try checkMap(allocator, .{ .objects = &objects, .players = 3 });
    defer allocator.free(findings);
    try std.testing.expectEqual(@as(usize, 2), count(findings, .player_index));
    try std.testing.expectEqual(@as(i32, 9), findings[0].player);
    try std.testing.expectEqualStrings("9->2, T-34, pos: [1.00, 1.00], scriptID: -1", findings[0].text());
}

test "a unit-creation party the table does not list is a finding; an empty table skips the check" {
    const allocator = std.testing.allocator;
    const parties = [_][]const u8{ "USSR", "Germany" };
    const used = [_][]const u8{ "USSR", "Narnia", "Germany" };
    {
        const findings = try checkMap(allocator, .{ .objects = &.{}, .players = 4, .parties = &parties, .unit_parties = &used });
        defer allocator.free(findings);
        try std.testing.expectEqual(@as(usize, 1), findings.len);
        try std.testing.expectEqual(Kind.unknown_party, findings[0].kind);
        try std.testing.expectEqual(@as(i32, 1), findings[0].player);
        try std.testing.expect(std.mem.indexOf(u8, findings[0].text(), "Narnia") != null);
    }
    {
        const findings = try checkMap(allocator, .{ .objects = &.{}, .players = 4, .parties = &.{}, .unit_parties = &used });
        defer allocator.free(findings);
        try std.testing.expectEqual(@as(usize, 0), findings.len);
    }
}

test "an unknown object type and a road with one control point are findings of their own" {
    const allocator = std.testing.allocator;
    var mystery = fixtureObject(7, "No_Such_Object", 64, 64, 0);
    mystery.known = false;
    const objects = [_]ObjectRecord{ fixtureObject(1, "T-34", 128, 128, 0), mystery };
    const short = [_]ShortVso{ .{ .kind = 0, .index = 3, .x = 100, .y = 200, .control_points = 1 }, .{ .kind = 1, .index = 0, .control_points = 0 } };
    const findings = try checkMap(allocator, .{ .objects = &objects, .players = 3, .short_vsos = &short });
    defer allocator.free(findings);
    try std.testing.expectEqual(@as(usize, 1), count(findings, .unknown_object_type));
    try std.testing.expectEqual(@as(usize, 2), count(findings, .short_vso));
    try std.testing.expectEqual(@as(i32, 7), findings[0].link_id);
    try std.testing.expect(findings[1].world);
    try std.testing.expectEqual(@as(f32, 100), findings[1].x);
    try std.testing.expect(!findings[2].world); // no control point, no place
    try std.testing.expectEqualStrings("road 3 has 1 control point(s)", findings[1].text());
}

test "a clean map has no findings and the log says so; findings are grouped by kind in the log" {
    const allocator = std.testing.allocator;
    var buffer: [1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try writeLog(&writer, &.{});
    try std.testing.expectEqualStrings("Check Map: no problems found\n", writer.buffered());

    const objects = [_]ObjectRecord{ fixtureObject(1, "T-34", 64, 64, 0), fixtureObject(2, "T-34", 64, 64, 0), fixtureObject(3, "T-34", 128, 64, 7) };
    const findings = try checkMap(allocator, .{ .objects = &objects, .players = 3 });
    defer allocator.free(findings);
    var log: std.Io.Writer = .fixed(&buffer);
    try writeLog(&log, findings);
    const text = log.buffered();
    try std.testing.expect(std.mem.startsWith(u8, text, "Double objects:\nT-34, pos: [1.00, 1.00], scriptID: -1\n\nInvalid player numbers:\n"));
}

test "every kind is a finding the fix can name, and the destructive ones ask first" {
    try std.testing.expect((Finding{ .kind = .unknown_object_type }).needsConfirmation());
    try std.testing.expect((Finding{ .kind = .short_vso }).needsConfirmation());
    try std.testing.expect(!(Finding{ .kind = .duplicate_object }).needsConfirmation());
    try std.testing.expect(!(Finding{ .kind = .invalid_link }).needsConfirmation());
    try std.testing.expect(!(Finding{ .kind = .player_index }).needsConfirmation());
    try std.testing.expect(!(Finding{ .kind = .unknown_party }).needsConfirmation());
    inline for (std.meta.fields(Kind)) |field| {
        const kind: Kind = @enumFromInt(field.value);
        try std.testing.expect(kind.heading().len > 0);
    }
}
