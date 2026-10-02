//! The RMG composers' data model (M3 05-09, D-06..D-12): containers and graphs
//! as owned std-only values, their file-level documents with their own undo
//! (composer files are not map history - every MFC composer saved to its own
//! file), the Check! rules as pure functions over what a `Source` reports, and
//! the Graphs Composer canvas gestures (D-11) as a small state machine over a
//! tile grid. No UI, no engine, no C: the bridge half is `Editor.readContainer`
//! and friends, the ImGui half is panels_m3.zig.
//!
//! The formats are never re-typed here: a record crosses the bridge as the
//! engine's own SRMContainer / SRMGraph fields and is written by the engine's
//! own serialisers (bridge.h "The RMG composers").

const std = @import("std");

const Allocator = std.mem.Allocator;

/// VIS tiles per patch side (STerrainPatchInfo::nSizeX/nSizeY).
pub const patch_tiles: i32 = 16;
/// The engine raises a link's part count below this (RMGeneration.cpp:1455-1458
/// "nParts < 8 -> 8"), which the graph Check! reports and can fix.
pub const min_parts: i32 = 8;
/// A graph's patch zoom (the MFC canvas slider, RMG_CreateGraphDialog.cpp:640).
pub const min_zoom: i32 = 1;
pub const max_zoom: i32 = 32;
/// The MFC defaults for a new link (RMG_Consts.cpp:30-38): the world cell is 32.
pub const world_cell: f32 = 32.0;
pub const default_radius: f32 = world_cell * 4;
pub const default_parts: i32 = 8;
pub const default_min_length: f32 = world_cell * 4;
pub const default_distance: f32 = 0.3;
pub const default_disturbance: f32 = 0.1;
/// Undo steps one composer file keeps.
pub const undo_depth: usize = 64;

pub const season_names = [_][]const u8{ "Summer", "Winter", "Africa", "Spring" };

/// The tileset folders of the four seasons (CMapInfo::SEASON_FOLDERS) and the
/// season number each stores (CMapInfo::REAL_SEASONS: Spring is season 0 with
/// its own folder).
pub const season_folders = [_][]const u8{ "terrain\\sets\\1\\", "terrain\\sets\\2\\", "terrain\\sets\\3\\", "terrain\\sets\\4\\" };
pub const real_seasons = [_]i32{ 0, 1, 2, 0 };

/// The MFC's RMGGetSeasonNameString: season 0 whose folder is the spring one
/// (terrain\sets\4\) is Spring.
pub fn seasonName(season: i32, season_folder: []const u8) []const u8 {
    if (season == 0 and std.ascii.eqlIgnoreCase(season_folder, season_folders[3])) return season_names[3];
    if (season < 0 or season >= season_names.len) return "?";
    return season_names[@intCast(season)];
}

fn dupe(allocator: Allocator, text: []const u8) Allocator.Error![]u8 {
    return try allocator.dupe(u8, text);
}

fn setText(allocator: Allocator, slot: *[]u8, text: []const u8) Allocator.Error!void {
    const copy = try allocator.dupe(u8, text);
    allocator.free(slot.*);
    slot.* = copy;
}

fn lessThanText(_: void, a: []u8, b: []u8) bool {
    return std.mem.lessThan(u8, a, b);
}

// ---------------------------------------------------------------------------
// Summaries: what a patch map or a container says about itself
// ---------------------------------------------------------------------------

/// The facts the Check! rules compare (a patch's own, or a container's for a
/// graph node): size in patches, season, folder, script IDs and areas.
pub const Summary = struct {
    size_x: i32 = 0,
    size_y: i32 = 0,
    season: i32 = 0,
    season_folder: []u8 = &.{},
    script_ids: []i32 = &.{},
    script_areas: [][]u8 = &.{},

    pub fn deinit(self: *Summary, allocator: Allocator) void {
        allocator.free(self.season_folder);
        allocator.free(self.script_ids);
        for (self.script_areas) |area| allocator.free(area);
        allocator.free(self.script_areas);
        self.* = .{};
    }
};

/// Where Check! and the add rules get their facts: the patch map named, and
/// the container named (a graph node's). A null answer is "does not load".
/// The Editor's own source asks the bridge; tests supply fixtures.
pub const Source = struct {
    ctx: *anyopaque,
    patch_fn: *const fn (ctx: *anyopaque, allocator: Allocator, name: []const u8) ?Summary,
    container_fn: *const fn (ctx: *anyopaque, allocator: Allocator, name: []const u8) ?Summary,

    pub fn patch(self: Source, allocator: Allocator, name: []const u8) ?Summary {
        return self.patch_fn(self.ctx, allocator, name);
    }
    pub fn container(self: Source, allocator: Allocator, name: []const u8) ?Summary {
        return self.container_fn(self.ctx, allocator, name);
    }
};

fn sameInts(a: []const i32, b: []const i32) bool {
    return std.mem.eql(i32, a, b);
}

fn sameAreas(a: []const []u8, b: []const []u8) bool {
    if (a.len != b.len) return false;
    for (a, b) |x, y| if (!std.mem.eql(u8, x, y)) return false;
    return true;
}

fn insertSorted(allocator: Allocator, list: *std.ArrayListUnmanaged(i32), value: i32) Allocator.Error!void {
    const at = std.sort.lowerBound(i32, list.items, value, struct {
        fn order(context: i32, item: i32) std.math.Order {
            return std.math.order(context, item);
        }
    }.order);
    if (at < list.items.len and list.items[at] == value) return;
    try list.insert(allocator, at, value);
}

// ---------------------------------------------------------------------------
// Containers
// ---------------------------------------------------------------------------

pub const Direction = enum(usize) { north = 0, east = 1, south = 2, west = 3 };
pub const direction_names = [_][]const u8{ "NORTH (0)", "EAST (90)", "SOUTH (180)", "WEST (270)" };

pub const Patch = struct {
    name: []u8 = &.{},
    size_x: i32 = 1,
    size_y: i32 = 1,
    /// The setting this patch is for; empty is "any setting".
    place: []u8 = &.{},

    pub fn clone(self: Patch, allocator: Allocator) Allocator.Error!Patch {
        const name = try dupe(allocator, self.name);
        errdefer allocator.free(name);
        return .{ .name = name, .size_x = self.size_x, .size_y = self.size_y, .place = try dupe(allocator, self.place) };
    }
    pub fn deinit(self: *Patch, allocator: Allocator) void {
        allocator.free(self.name);
        allocator.free(self.place);
        self.* = .{};
    }
};

/// SRMContainer. `indices[d]` lists the patches usable in direction d, in the
/// file's own order; the table's Yes/blank cells are membership in those lists
/// (RMG_CreateContainerDialog.cpp:880-900 builds the cells the same way).
pub const Container = struct {
    size_x: i32 = 0,
    size_y: i32 = 0,
    season: i32 = 0,
    season_folder: []u8 = &.{},
    patches: std.ArrayListUnmanaged(Patch) = .empty,
    indices: [4]std.ArrayListUnmanaged(i32) = .{ .empty, .empty, .empty, .empty },
    script_ids: std.ArrayListUnmanaged(i32) = .empty,
    script_areas: std.ArrayListUnmanaged([]u8) = .empty,

    pub fn deinit(self: *Container, allocator: Allocator) void {
        allocator.free(self.season_folder);
        for (self.patches.items) |*patch| patch.deinit(allocator);
        self.patches.deinit(allocator);
        for (&self.indices) |*list| list.deinit(allocator);
        self.script_ids.deinit(allocator);
        for (self.script_areas.items) |area| allocator.free(area);
        self.script_areas.deinit(allocator);
        self.* = .{};
    }

    pub fn clone(self: *const Container, allocator: Allocator) Allocator.Error!Container {
        var out: Container = .{ .size_x = self.size_x, .size_y = self.size_y, .season = self.season };
        errdefer out.deinit(allocator);
        out.season_folder = try dupe(allocator, self.season_folder);
        try out.patches.ensureTotalCapacity(allocator, self.patches.items.len);
        for (self.patches.items) |patch| out.patches.appendAssumeCapacity(try patch.clone(allocator));
        for (self.indices, 0..) |list, d| try out.indices[d].appendSlice(allocator, list.items);
        try out.script_ids.appendSlice(allocator, self.script_ids.items);
        try out.script_areas.ensureTotalCapacity(allocator, self.script_areas.items.len);
        for (self.script_areas.items) |area| out.script_areas.appendAssumeCapacity(try dupe(allocator, area));
        return out;
    }

    pub fn eql(self: *const Container, other: *const Container) bool {
        if (self.size_x != other.size_x or self.size_y != other.size_y or self.season != other.season) return false;
        if (!std.mem.eql(u8, self.season_folder, other.season_folder)) return false;
        if (self.patches.items.len != other.patches.items.len) return false;
        for (self.patches.items, other.patches.items) |a, b| {
            if (a.size_x != b.size_x or a.size_y != b.size_y or !std.mem.eql(u8, a.name, b.name) or !std.mem.eql(u8, a.place, b.place)) return false;
        }
        for (self.indices, other.indices) |a, b| if (!sameInts(a.items, b.items)) return false;
        return sameInts(self.script_ids.items, other.script_ids.items) and sameAreas(self.script_areas.items, other.script_areas.items);
    }

    pub fn patchCount(self: *const Container) usize {
        return self.patches.items.len;
    }

    /// Whether patch `index` is listed for `direction` (a cell's "Yes").
    pub fn hasDirection(self: *const Container, index: usize, direction: Direction) bool {
        const want: i32 = @intCast(index);
        for (self.indices[@intFromEnum(direction)].items) |entry| if (entry == want) return true;
        return false;
    }

    /// Sets or clears patch `index` in `direction`'s list. A list keeps the
    /// order the file gave it; a patch added to it goes where the MFC's
    /// rebuild would put it - in patch order (RMG_CreateContainerDialog.cpp:
    /// SaveContainerFromControls rebuilds every list ascending).
    pub fn setDirection(self: *Container, allocator: Allocator, index: usize, direction: Direction, on: bool) Allocator.Error!void {
        const list = &self.indices[@intFromEnum(direction)];
        const want: i32 = @intCast(index);
        if (!on) {
            var kept: usize = 0;
            for (list.items) |entry| {
                if (entry != want) {
                    list.items[kept] = entry;
                    kept += 1;
                }
            }
            list.shrinkRetainingCapacity(kept);
            return;
        }
        if (self.hasDirection(index, direction)) return;
        const at = std.sort.lowerBound(i32, list.items, want, struct {
            fn order(context: i32, item: i32) std.math.Order {
                return std.math.order(context, item);
            }
        }.order);
        try list.insert(allocator, at, want);
    }

    /// Appends a patch usable in every direction (the MFC's Add sets all four
    /// cells to Yes) and refreshes the container's size.
    pub fn addPatch(self: *Container, allocator: Allocator, name: []const u8, size_x: i32, size_y: i32) Allocator.Error!void {
        try self.patches.ensureUnusedCapacity(allocator, 1);
        var patch: Patch = .{ .name = try dupe(allocator, name), .size_x = size_x, .size_y = size_y, .place = &.{} };
        errdefer patch.deinit(allocator);
        const index: i32 = @intCast(self.patches.items.len);
        for (&self.indices) |*list| try list.ensureUnusedCapacity(allocator, 1);
        self.patches.appendAssumeCapacity(patch);
        for (&self.indices) |*list| list.appendAssumeCapacity(index);
        self.recomputeSize();
    }

    /// Takes the patches at `doomed` (any order, repeats ignored) out, renumbering
    /// every direction list; entries that named a removed patch go.
    pub fn removePatches(self: *Container, allocator: Allocator, doomed: []const usize) void {
        const count = self.patches.items.len;
        var gone = std.ArrayListUnmanaged(bool).empty;
        defer gone.deinit(allocator);
        gone.resize(allocator, count) catch return;
        @memset(gone.items, false);
        for (doomed) |index| if (index < count) {
            gone.items[index] = true;
        };
        var new_index = std.ArrayListUnmanaged(i32).empty;
        defer new_index.deinit(allocator);
        new_index.resize(allocator, count) catch return;
        var kept: usize = 0;
        for (self.patches.items, 0..) |*patch, i| {
            if (gone.items[i]) {
                patch.deinit(allocator);
                new_index.items[i] = -1;
                continue;
            }
            new_index.items[i] = @intCast(kept);
            self.patches.items[kept] = patch.*;
            kept += 1;
        }
        self.patches.shrinkRetainingCapacity(kept);
        for (&self.indices) |*list| {
            var at: usize = 0;
            for (list.items) |entry| {
                if (entry < 0 or entry >= count) continue;
                const mapped = new_index.items[@intCast(entry)];
                if (mapped < 0) continue;
                list.items[at] = mapped;
                at += 1;
            }
            list.shrinkRetainingCapacity(at);
        }
        if (kept == 0) self.clearHeader(allocator);
        self.recomputeSize();
    }

    /// The MFC's own rule when the last patch leaves: no season, folder or
    /// script lists (SaveContainerFromControls).
    fn clearHeader(self: *Container, allocator: Allocator) void {
        self.season = 0;
        allocator.free(self.season_folder);
        self.season_folder = &.{};
        self.script_ids.clearRetainingCapacity();
        for (self.script_areas.items) |area| allocator.free(area);
        self.script_areas.clearRetainingCapacity();
    }

    /// The container's size is the largest patch size (SaveContainerFromControls).
    pub fn recomputeSize(self: *Container) void {
        var x: i32 = 0;
        var y: i32 = 0;
        for (self.patches.items) |patch| {
            x = @max(x, patch.size_x);
            y = @max(y, patch.size_y);
        }
        self.size_x = x;
        self.size_y = y;
    }

    pub fn setPlace(self: *Container, allocator: Allocator, index: usize, place: []const u8) Allocator.Error!void {
        if (index >= self.patches.items.len) return;
        try setText(allocator, &self.patches.items[index].place, place);
    }

    /// Takes season, folder and script lists from `summary` (the first patch's
    /// own: the MFC copies them when the container has none).
    pub fn setHeaderFrom(self: *Container, allocator: Allocator, summary: Summary) Allocator.Error!void {
        self.season = summary.season;
        try setText(allocator, &self.season_folder, summary.season_folder);
        self.script_ids.clearRetainingCapacity();
        try self.script_ids.appendSlice(allocator, summary.script_ids);
        for (self.script_areas.items) |area| allocator.free(area);
        self.script_areas.clearRetainingCapacity();
        for (summary.script_areas) |area| {
            const copy = try dupe(allocator, area);
            errdefer allocator.free(copy);
            try self.script_areas.append(allocator, copy);
        }
    }

    /// The settings every direction can be built for (SRMContainer::
    /// GetSupportedSettings, RMG_Methods.cpp:130-190): "<any setting>" alone when
    /// the unplaced patches cover all four directions, else each named setting
    /// that, with the unplaced ones, does. Lower-cased and sorted (the MFC's
    /// hash-map order is arbitrary). Caller frees with `freeNames`.
    pub fn supportedSettings(self: *const Container, allocator: Allocator) Allocator.Error![][]u8 {
        var counts = std.StringArrayHashMapUnmanaged([4]u32).empty;
        defer {
            for (counts.keys()) |key| allocator.free(key);
            counts.deinit(allocator);
        }
        try counts.put(allocator, try dupe(allocator, "<any setting>"), .{ 0, 0, 0, 0 });
        for (self.indices, 0..) |list, d| {
            for (list.items) |entry| {
                if (entry < 0 or entry >= self.patches.items.len) continue;
                const place = self.patches.items[@intCast(entry)].place;
                const lowered = try allocator.alloc(u8, if (place.len == 0) "<any setting>".len else place.len);
                defer allocator.free(lowered);
                if (place.len == 0) {
                    @memcpy(lowered, "<any setting>");
                } else {
                    for (place, 0..) |byte, i| lowered[i] = std.ascii.toLower(byte);
                }
                if (counts.getPtr(lowered)) |slot| {
                    slot[d] += 1;
                } else {
                    var fresh = [4]u32{ 0, 0, 0, 0 };
                    fresh[d] = 1;
                    try counts.put(allocator, try dupe(allocator, lowered), fresh);
                }
            }
        }
        const any = counts.get("<any setting>").?;
        var out = std.ArrayListUnmanaged([]u8).empty;
        errdefer {
            for (out.items) |item| allocator.free(item);
            out.deinit(allocator);
        }
        if (any[0] > 0 and any[1] > 0 and any[2] > 0 and any[3] > 0) {
            try out.append(allocator, try dupe(allocator, "<any setting>"));
            return try out.toOwnedSlice(allocator);
        }
        for (counts.keys(), counts.values()) |key, value| {
            var ok = true;
            for (0..4) |d| ok = ok and (value[d] > 0 or any[d] > 0);
            if (ok) try out.append(allocator, try dupe(allocator, key));
        }
        std.mem.sort([]u8, out.items, {}, lessThanText);
        return try out.toOwnedSlice(allocator);
    }
};

pub fn freeNames(allocator: Allocator, names: [][]u8) void {
    for (names) |name| allocator.free(name);
    allocator.free(names);
}

/// What adding a patch to a container found (RMG_CreateContainerDialog.cpp:
/// 255-310): a patch of another season, folder or script lists is refused,
/// naming the difference. The first patch gives the container its header.
pub const AddOutcome = union(enum) {
    added,
    /// The patch map would not load.
    unreadable,
    /// Refused; the text names what differs ("Invalid Season: ...").
    mismatch: []u8,

    pub fn deinit(self: *AddOutcome, allocator: Allocator) void {
        switch (self.*) {
            .mismatch => |text| allocator.free(text),
            else => {},
        }
        self.* = .added;
    }
};

/// The difference between two summaries as the MFC's messages word it
/// ("Invalid Season: ..."), `header` named `left`, `other` named `right`;
/// empty when they agree. Trailing space trimmed.
pub fn describeMismatch(allocator: Allocator, header: Summary, other: Summary, left: []const u8, right: []const u8) Allocator.Error![]u8 {
    var text = std.ArrayListUnmanaged(u8).empty;
    errdefer text.deinit(allocator);
    if (header.season != other.season) {
        try text.print(allocator, "Invalid Season: {s} {s}, {s} {s}. ", .{ left, seasonName(header.season, header.season_folder), right, seasonName(other.season, other.season_folder) });
    }
    if (!std.ascii.eqlIgnoreCase(header.season_folder, other.season_folder)) {
        try text.print(allocator, "Invalid Season Folder: {s} <{s}>, {s} <{s}>. ", .{ left, header.season_folder, right, other.season_folder });
    }
    if (!sameInts(header.script_ids, other.script_ids)) {
        try text.print(allocator, "Invalid ScriptIDs: {s} <", .{left});
        try printIds(allocator, &text, header.script_ids);
        try text.print(allocator, ">, {s} <", .{right});
        try printIds(allocator, &text, other.script_ids);
        try text.appendSlice(allocator, ">. ");
    }
    if (!sameAreas(header.script_areas, other.script_areas)) {
        try text.print(allocator, "Invalid ScriptAreas: {s} <", .{left});
        try printAreas(allocator, &text, header.script_areas);
        try text.print(allocator, ">, {s} <", .{right});
        try printAreas(allocator, &text, other.script_areas);
        try text.appendSlice(allocator, ">. ");
    }
    while (text.items.len != 0 and text.items[text.items.len - 1] == ' ') _ = text.pop();
    return try text.toOwnedSlice(allocator);
}

fn printIds(allocator: Allocator, text: *std.ArrayListUnmanaged(u8), ids: []const i32) Allocator.Error!void {
    for (ids, 0..) |id, i| try text.print(allocator, "{s}{d}", .{ if (i == 0) "" else "; ", id });
}

fn printAreas(allocator: Allocator, text: *std.ArrayListUnmanaged(u8), areas: []const []u8) Allocator.Error!void {
    for (areas, 0..) |area, i| try text.print(allocator, "{s}{s}", .{ if (i == 0) "" else "; ", area });
}

fn headerSummary(c: *const Container) Summary {
    return .{
        .size_x = c.size_x,
        .size_y = c.size_y,
        .season = c.season,
        .season_folder = c.season_folder,
        .script_ids = c.script_ids.items,
        .script_areas = c.script_areas.items,
    };
}

/// Adds the patch map `name` to `container` the MFC's way, with the D-10
/// difference that a patch outside the storages never reaches here (the
/// caller copies it in first). A repeated name replaces the earlier entry
/// (the MFC deleted the old list item first); a refusal changes nothing.
pub fn addPatchChecked(allocator: Allocator, container: *Container, source: Source, name: []const u8) Allocator.Error!AddOutcome {
    var info = source.patch(allocator, name) orelse return .unreadable;
    defer info.deinit(allocator);
    var others = false;
    for (container.patches.items) |patch| {
        if (!std.ascii.eqlIgnoreCase(patch.name, name)) others = true;
    }
    if (others) {
        const text = try describeMismatch(allocator, headerSummary(container), info, "container", "patch");
        if (text.len != 0) return .{ .mismatch = text };
        allocator.free(text);
    }
    var at: usize = 0;
    while (at < container.patches.items.len) {
        if (std.ascii.eqlIgnoreCase(container.patches.items[at].name, name)) {
            container.removePatches(allocator, &.{at});
        } else at += 1;
    }
    if (container.patches.items.len == 0) try container.setHeaderFrom(allocator, info);
    try container.addPatch(allocator, name, info.size_x, info.size_y);
    return .added;
}

// ---------------------------------------------------------------------------
// Check!
// ---------------------------------------------------------------------------

pub const Severity = enum { warning, @"error" };

/// What a finding can be fixed with. Every removal is a fix the person picks
/// (D-12): "Fix" on one finding, or "Fix all"; each is one undo step.
pub const Fix = union(enum) {
    none,
    remove_patch: usize,
    set_patch_size: struct { index: usize, size_x: i32, size_y: i32 },
    drop_bad_indices,
    recompute_header,
    recompute_size,
    remove_link: usize,
    set_parts: usize,
    clear_node_container: usize,
    /// A node's container name made storage-relative (a shipped graph holds one
    /// with its author's drive in front).
    strip_node_name: usize,
    strip_link_desc: usize,
    // Field sets (05-10): each is one explicit repair of what the MFC's own
    // Check! rewrote silently (RMG_CreateFieldDialog.cpp OnCheckFieldsButton).
    set_season_summer,
    clamp_height,
    clamp_ratio,
    reset_profile,
    fix_pattern,
    /// A shell's width clamped to 0..512 (`objects`: an object shell).
    shell_width: struct { objects: bool, shell: usize },
    remove_tile: struct { shell: usize, index: usize },
    zero_tile_weight: struct { shell: usize, index: usize },
    remove_object: struct { shell: usize, index: usize },
    zero_object_weight: struct { shell: usize, index: usize },
    object_step: usize,
    object_ratio: usize,
};

pub const Finding = struct {
    severity: Severity,
    /// The patch, node or link the finding is about, -1 for the file.
    index: i32 = -1,
    fix: Fix = .none,
    text: []u8,
};

pub const Report = struct {
    findings: std.ArrayListUnmanaged(Finding) = .empty,

    pub fn deinit(self: *Report, allocator: Allocator) void {
        for (self.findings.items) |finding| allocator.free(finding.text);
        self.findings.deinit(allocator);
        self.* = .{};
    }

    pub fn errorCount(self: *const Report) usize {
        var n: usize = 0;
        for (self.findings.items) |finding| {
            if (finding.severity == .@"error") n += 1;
        }
        return n;
    }

    fn add(self: *Report, allocator: Allocator, severity: Severity, index: i32, fix: Fix, comptime fmt: []const u8, args: anytype) Allocator.Error!void {
        const text = try std.fmt.allocPrint(allocator, fmt, args);
        errdefer allocator.free(text);
        try self.findings.append(allocator, .{ .severity = severity, .index = index, .fix = fix, .text = text });
    }
};

/// The Containers Composer's Check! (RMG_CreateContainerDialog.cpp:
/// OnCheckContainersButton, D-12): every patch is read again through the
/// storages; the first readable patch is the reference the rest must match
/// (season, folder, script IDs and areas); the stored patch and container sizes
/// must be the patches' own; every direction list must name patches that exist.
/// The MFC stopped at the first message and rewrote the container silently;
/// this lists everything and rewrites nothing.
pub fn checkContainer(allocator: Allocator, container: *const Container, source: Source) Allocator.Error!Report {
    var report: Report = .{};
    errdefer report.deinit(allocator);
    var reference: ?Summary = null;
    defer if (reference) |*info| info.deinit(allocator);
    var biggest_x: i32 = 0;
    var biggest_y: i32 = 0;
    for (container.patches.items, 0..) |patch, i| {
        var info = source.patch(allocator, patch.name) orelse {
            try report.add(allocator, .@"error", @intCast(i), .{ .remove_patch = i }, "patch {d} \"{s}\" cannot be loaded through the storages", .{ i, patch.name });
            continue;
        };
        var keep = false;
        defer if (!keep) info.deinit(allocator);
        biggest_x = @max(biggest_x, info.size_x);
        biggest_y = @max(biggest_y, info.size_y);
        if (info.size_x != patch.size_x or info.size_y != patch.size_y) {
            try report.add(allocator, .@"error", @intCast(i), .{ .set_patch_size = .{ .index = i, .size_x = info.size_x, .size_y = info.size_y } }, "patch {d} \"{s}\" is listed as {d}x{d} patches, the map is {d}x{d}", .{ i, patch.name, patch.size_x, patch.size_y, info.size_x, info.size_y });
        }
        if (reference == null) {
            reference = info;
            keep = true;
            continue;
        }
        const differ = try describeMismatch(allocator, reference.?, info, "first patch", "this patch");
        defer allocator.free(differ);
        if (differ.len != 0) {
            try report.add(allocator, .@"error", @intCast(i), .{ .remove_patch = i }, "patch {d} \"{s}\" does not belong with the first patch: {s}", .{ i, patch.name, differ });
        }
    }
    // The first readable patch's header is what the container should carry.
    if (reference) |ref| {
        const differ = try describeMismatch(allocator, headerSummary(container), ref, "container", "first patch");
        defer allocator.free(differ);
        if (differ.len != 0) {
            try report.add(allocator, .@"error", -1, .recompute_header, "the container's own season, folder or script lists are not its first patch's: {s}", .{differ});
        }
    }
    if (container.size_x != biggest_x or container.size_y != biggest_y) {
        try report.add(allocator, .@"error", -1, .recompute_size, "the container is {d}x{d} patches, its largest patch is {d}x{d}", .{ container.size_x, container.size_y, biggest_x, biggest_y });
    }
    for (container.indices, 0..) |list, d| {
        for (list.items) |entry| {
            if (entry < 0 or entry >= container.patches.items.len) {
                try report.add(allocator, .@"error", -1, .drop_bad_indices, "{s} lists patch {d}, and there are {d} patches", .{ direction_names[d], entry, container.patches.items.len });
                break;
            }
        }
    }
    if (container.patches.items.len > 0) {
        for (container.indices, 0..) |list, d| {
            if (list.items.len == 0) try report.add(allocator, .warning, -1, .none, "no patch is usable {s}: the generator cannot place this container that way", .{direction_names[d]});
        }
    }
    return report;
}

// ---------------------------------------------------------------------------
// Graphs
// ---------------------------------------------------------------------------

pub const Rect = struct {
    x1: i32 = 0,
    y1: i32 = 0,
    x2: i32 = 0,
    y2: i32 = 0,

    pub fn width(self: Rect) i32 {
        return self.x2 - self.x1;
    }
    pub fn height(self: Rect) i32 {
        return self.y2 - self.y1;
    }
    /// CTRect::IsIntersect: a shared edge is not an overlap.
    pub fn intersects(self: Rect, other: Rect) bool {
        return @max(self.x1, other.x1) < @min(self.x2, other.x2) and @max(self.y1, other.y1) < @min(self.y2, other.y2);
    }
    /// A tile inside (the max edge is exclusive, as the MFC's IsValidPoint).
    pub fn contains(self: Rect, x: i32, y: i32) bool {
        return x >= self.x1 and x < self.x2 and y >= self.y1 and y < self.y2;
    }
    pub fn eql(self: Rect, other: Rect) bool {
        return self.x1 == other.x1 and self.y1 == other.y1 and self.x2 == other.x2 and self.y2 == other.y2;
    }
};

pub const Node = struct {
    rect: Rect = .{},
    /// A container's storage name; empty is an empty node.
    container: []u8 = &.{},

    pub fn deinit(self: *Node, allocator: Allocator) void {
        allocator.free(self.container);
        self.* = .{};
    }
};

pub const link_road: i32 = 0;
pub const link_river: i32 = 1;

pub const Link = struct {
    a: i32 = -1,
    b: i32 = -1,
    kind: i32 = link_road,
    /// The VSO descriptor's storage name; empty is an empty link.
    desc: []u8 = &.{},
    /// World units (the dialog shows them divided by `world_cell`).
    radius: f32 = default_radius,
    parts: i32 = default_parts,
    min_length: f32 = default_min_length,
    /// 0..1, the dialog's "Width".
    distance: f32 = default_distance,
    disturbance: f32 = default_disturbance,

    pub fn clone(self: Link, allocator: Allocator) Allocator.Error!Link {
        var copy = self;
        copy.desc = try dupe(allocator, self.desc);
        return copy;
    }
    pub fn deinit(self: *Link, allocator: Allocator) void {
        allocator.free(self.desc);
        self.desc = &.{};
    }
    fn sameFloat(a: f32, b: f32) bool {
        return @abs(a - b) <= 1e-5 * @max(1.0, @max(@abs(a), @abs(b)));
    }
    pub fn eql(self: Link, other: Link) bool {
        return self.a == other.a and self.b == other.b and self.kind == other.kind and self.parts == other.parts and std.mem.eql(u8, self.desc, other.desc) and
            sameFloat(self.radius, other.radius) and sameFloat(self.min_length, other.min_length) and sameFloat(self.distance, other.distance) and sameFloat(self.disturbance, other.disturbance);
    }
};

/// SRMGraph. The size (patches, per axis) follows the nodes (`refreshSize`),
/// as the MFC's SetGraphItem keeps it.
pub const Graph = struct {
    size_x: i32 = 0,
    size_y: i32 = 0,
    season: i32 = 0,
    season_folder: []u8 = &.{},
    nodes: std.ArrayListUnmanaged(Node) = .empty,
    links: std.ArrayListUnmanaged(Link) = .empty,
    script_ids: std.ArrayListUnmanaged(i32) = .empty,
    script_areas: std.ArrayListUnmanaged([]u8) = .empty,

    pub fn deinit(self: *Graph, allocator: Allocator) void {
        allocator.free(self.season_folder);
        for (self.nodes.items) |*node| node.deinit(allocator);
        self.nodes.deinit(allocator);
        for (self.links.items) |*link| link.deinit(allocator);
        self.links.deinit(allocator);
        self.script_ids.deinit(allocator);
        for (self.script_areas.items) |area| allocator.free(area);
        self.script_areas.deinit(allocator);
        self.* = .{};
    }

    pub fn clone(self: *const Graph, allocator: Allocator) Allocator.Error!Graph {
        var out: Graph = .{ .size_x = self.size_x, .size_y = self.size_y, .season = self.season };
        errdefer out.deinit(allocator);
        out.season_folder = try dupe(allocator, self.season_folder);
        try out.nodes.ensureTotalCapacity(allocator, self.nodes.items.len);
        for (self.nodes.items) |node| out.nodes.appendAssumeCapacity(.{ .rect = node.rect, .container = try dupe(allocator, node.container) });
        try out.links.ensureTotalCapacity(allocator, self.links.items.len);
        for (self.links.items) |link| out.links.appendAssumeCapacity(try link.clone(allocator));
        try out.script_ids.appendSlice(allocator, self.script_ids.items);
        try out.script_areas.ensureTotalCapacity(allocator, self.script_areas.items.len);
        for (self.script_areas.items) |area| out.script_areas.appendAssumeCapacity(try dupe(allocator, area));
        return out;
    }

    pub fn eql(self: *const Graph, other: *const Graph) bool {
        if (self.size_x != other.size_x or self.size_y != other.size_y or self.season != other.season or !std.mem.eql(u8, self.season_folder, other.season_folder)) return false;
        if (self.nodes.items.len != other.nodes.items.len or self.links.items.len != other.links.items.len) return false;
        for (self.nodes.items, other.nodes.items) |a, b| if (!a.rect.eql(b.rect) or !std.mem.eql(u8, a.container, b.container)) return false;
        for (self.links.items, other.links.items) |a, b| if (!a.eql(b)) return false;
        return sameInts(self.script_ids.items, other.script_ids.items) and sameAreas(self.script_areas.items, other.script_areas.items);
    }

    /// The size in patches the nodes need (MFC SetGraphItem: ceil(max/16)).
    pub fn refreshSize(self: *Graph) void {
        var x: i32 = 0;
        var y: i32 = 0;
        for (self.nodes.items) |node| {
            x = @max(x, @divTrunc(node.rect.x2 - 1 + patch_tiles, patch_tiles));
            y = @max(y, @divTrunc(node.rect.y2 - 1 + patch_tiles, patch_tiles));
        }
        self.size_x = x;
        self.size_y = y;
    }

    pub fn emptyNodeCount(self: *const Graph) usize {
        var n: usize = 0;
        for (self.nodes.items) |node| {
            if (node.container.len == 0) n += 1;
        }
        return n;
    }

    pub fn emptyLinkCount(self: *const Graph) usize {
        var n: usize = 0;
        for (self.links.items) |link| {
            if (link.desc.len == 0) n += 1;
        }
        return n;
    }

    pub fn linksOfNode(self: *const Graph, node: usize) usize {
        var n: usize = 0;
        for (self.links.items) |link| {
            if (link.a == @as(i32, @intCast(node)) or link.b == @as(i32, @intCast(node))) n += 1;
        }
        return n;
    }

    fn anyFilledExcept(self: *const Graph, skip: usize) bool {
        for (self.nodes.items, 0..) |node, i| {
            if (i != skip and node.container.len != 0) return true;
        }
        return false;
    }

    /// Appends an empty node; refused (false) with less than a patch in either
    /// direction or an overlap with another node (the MFC's add rule,
    /// RMG_CreateGraphDialog.cpp:1119-1141).
    pub fn addNode(self: *Graph, allocator: Allocator, rect: Rect) Allocator.Error!bool {
        if (rect.width() < patch_tiles or rect.height() < patch_tiles) return false;
        for (self.nodes.items) |node| if (node.rect.intersects(rect)) return false;
        try self.nodes.append(allocator, .{ .rect = rect, .container = &.{} });
        self.refreshSize();
        return true;
    }

    /// Deletes node `index` with its links, renumbering the others' ends
    /// (RMG_CreateGraphDialog.cpp OnDeleteMenu). With no filled node left the
    /// graph forgets its season and script lists, as the MFC did.
    pub fn removeNode(self: *Graph, allocator: Allocator, index: usize) void {
        if (index >= self.nodes.items.len) return;
        const doomed: i32 = @intCast(index);
        var kept: usize = 0;
        for (self.links.items) |*link| {
            if (link.a == doomed or link.b == doomed) {
                link.deinit(allocator);
                continue;
            }
            if (link.a > doomed) link.a -= 1;
            if (link.b > doomed) link.b -= 1;
            self.links.items[kept] = link.*;
            kept += 1;
        }
        self.links.shrinkRetainingCapacity(kept);
        if (!self.anyFilledExcept(index)) self.clearHeader(allocator);
        self.nodes.items[index].deinit(allocator);
        _ = self.nodes.orderedRemove(index);
        self.refreshSize();
    }

    pub fn removeLink(self: *Graph, allocator: Allocator, index: usize) void {
        if (index >= self.links.items.len) return;
        self.links.items[index].deinit(allocator);
        _ = self.links.orderedRemove(index);
    }

    /// A default link between two nodes (the MFC allowed repeats).
    pub fn addLink(self: *Graph, allocator: Allocator, a: usize, b: usize) Allocator.Error!bool {
        if (a == b or a >= self.nodes.items.len or b >= self.nodes.items.len) return false;
        try self.links.append(allocator, .{ .a = @intCast(a), .b = @intCast(b), .desc = &.{} });
        return true;
    }

    fn clearHeader(self: *Graph, allocator: Allocator) void {
        self.season = 0;
        allocator.free(self.season_folder);
        self.season_folder = &.{};
        self.script_ids.clearRetainingCapacity();
        for (self.script_areas.items) |area| allocator.free(area);
        self.script_areas.clearRetainingCapacity();
    }

    pub fn setHeaderFrom(self: *Graph, allocator: Allocator, summary: Summary) Allocator.Error!void {
        self.season = summary.season;
        try setText(allocator, &self.season_folder, summary.season_folder);
        self.script_ids.clearRetainingCapacity();
        try self.script_ids.appendSlice(allocator, summary.script_ids);
        for (self.script_areas.items) |area| allocator.free(area);
        self.script_areas.clearRetainingCapacity();
        for (summary.script_areas) |area| {
            const copy = try dupe(allocator, area);
            errdefer allocator.free(copy);
            try self.script_areas.append(allocator, copy);
        }
    }

    /// The union the MFC keeps in the graph header: script IDs and areas of
    /// every filled node's container (sorted, no repeats).
    pub fn mergeScripts(self: *Graph, allocator: Allocator, summary: Summary) Allocator.Error!void {
        for (summary.script_ids) |id| try insertSorted(allocator, &self.script_ids, id);
        for (summary.script_areas) |area| {
            var present = false;
            for (self.script_areas.items) |known| {
                if (std.mem.eql(u8, known, area)) present = true;
            }
            if (present) continue;
            const copy = try dupe(allocator, area);
            errdefer allocator.free(copy);
            const at = std.sort.lowerBound([]u8, self.script_areas.items, copy, struct {
                fn order(context: []u8, item: []u8) std.math.Order {
                    return std.mem.order(u8, context, item);
                }
            }.order);
            try self.script_areas.insert(allocator, at, copy);
        }
    }
};

pub const SetContainerOutcome = union(enum) {
    set,
    unreadable,
    /// Refused: the container's season or folder differs from the graph's.
    mismatch: []u8,
    pub fn deinit(self: *SetContainerOutcome, allocator: Allocator) void {
        switch (self.*) {
            .mismatch => |text| allocator.free(text),
            else => {},
        }
        self.* = .set;
    }
};

/// Gives node `index` the container `name` (the node properties' OK,
/// RMG_CreateGraphDialog.cpp OnPropertiesMenu): the first filled node gives
/// the graph its season, folder and script lists; later ones must match season
/// and folder and add their script IDs and areas. An empty name empties the
/// node. A container larger than the node is not refused, only reported by
/// `containerFitsNode` (the MFC warned and went on).
pub fn setNodeContainer(allocator: Allocator, graph: *Graph, index: usize, name: []const u8, source: Source) Allocator.Error!SetContainerOutcome {
    if (index >= graph.nodes.items.len) return .unreadable;
    if (name.len == 0) {
        try setText(allocator, &graph.nodes.items[index].container, "");
        return .set;
    }
    var info = source.container(allocator, name) orelse return .unreadable;
    defer info.deinit(allocator);
    if (!graph.anyFilledExcept(index)) {
        try graph.setHeaderFrom(allocator, info);
    } else {
        var differ = std.ArrayListUnmanaged(u8).empty;
        defer differ.deinit(allocator);
        if (graph.season != info.season) {
            try differ.print(allocator, "Invalid Season: graph {s}, container {s}. ", .{ seasonName(graph.season, graph.season_folder), seasonName(info.season, info.season_folder) });
        }
        if (!std.ascii.eqlIgnoreCase(graph.season_folder, info.season_folder)) {
            try differ.print(allocator, "Invalid Season Folder: graph <{s}>, container <{s}>. ", .{ graph.season_folder, info.season_folder });
        }
        if (differ.items.len != 0) return .{ .mismatch = try differ.toOwnedSlice(allocator) };
        try graph.mergeScripts(allocator, info);
    }
    try setText(allocator, &graph.nodes.items[index].container, name);
    return .set;
}

/// The MFC's size warning (OnPropertiesMenu): the node, in whole patches, is
/// smaller than the container in some direction.
pub fn containerFitsNode(node: Rect, container_size_x: i32, container_size_y: i32) bool {
    return @divTrunc(node.width(), patch_tiles) >= container_size_x and @divTrunc(node.height(), patch_tiles) >= container_size_y;
}

/// A name the storages can resolve: no drive colon, not rooted.
pub fn isStorageRelative(name: []const u8) bool {
    if (name.len == 0) return true;
    if (std.mem.indexOfScalar(u8, name, ':') != null) return false;
    return name[0] != '\\' and name[0] != '/';
}

/// The part of `name` from the first `anchor` component on ("terrain\\" for a
/// descriptor, "scenarios\\" for a container), found at the start or after a
/// separator, case ignored; null when there is none. What the MFC's Check!
/// did with the storage's own prefix.
pub fn storageSuffix(name: []const u8, anchor: []const u8) ?[]const u8 {
    var at: usize = 0;
    while (at + anchor.len <= name.len) : (at += 1) {
        if (at != 0 and name[at - 1] != '\\' and name[at - 1] != '/') continue;
        var same = true;
        for (anchor, name[at .. at + anchor.len]) |want, got| {
            const g = if (got == '/') '\\' else std.ascii.toLower(got);
            if (g != want) same = false;
        }
        if (same) return name[at..];
    }
    return null;
}

/// The Graphs Composer's Check! (OnCheckGraphsButton, D-12): every filled
/// node's container must load; the first filled node's container is the
/// reference season and folder; a link's part count is at least 8 (the engine
/// raises a smaller one itself, so the file should say what happens); a link
/// must join two different nodes. The graph's own header is compared with what
/// its containers give. Nothing is rewritten.
pub fn checkGraph(allocator: Allocator, graph: *const Graph, source: Source) Allocator.Error!Report {
    var report: Report = .{};
    errdefer report.deinit(allocator);
    var reference: ?Summary = null;
    defer if (reference) |*info| info.deinit(allocator);
    var union_ids = std.ArrayListUnmanaged(i32).empty;
    defer union_ids.deinit(allocator);
    var union_areas = std.ArrayListUnmanaged([]u8).empty;
    defer {
        for (union_areas.items) |area| allocator.free(area);
        union_areas.deinit(allocator);
    }
    for (graph.nodes.items, 0..) |node, i| {
        if (node.container.len == 0) {
            try report.add(allocator, .warning, @intCast(i), .none, "node {d} is empty: it holds no container", .{i});
            continue;
        }
        if (!isStorageRelative(node.container)) {
            const fix: Fix = if (storageSuffix(node.container, "scenarios\\") != null) .{ .strip_node_name = i } else .{ .clear_node_container = i };
            try report.add(allocator, .@"error", @intCast(i), fix, "node {d}: container \"{s}\" is not a storage name (it has a drive or a root in front)", .{ i, node.container });
            continue;
        }
        var info = source.container(allocator, node.container) orelse {
            try report.add(allocator, .@"error", @intCast(i), .{ .clear_node_container = i }, "node {d}: container \"{s}\" cannot be loaded through the storages", .{ i, node.container });
            continue;
        };
        if (!containerFitsNode(node.rect, info.size_x, info.size_y)) {
            try report.add(allocator, .warning, @intCast(i), .none, "node {d} is {d}x{d} tiles, its container is {d}x{d} patches: possibly invalid size in some directions", .{ i, node.rect.width(), node.rect.height(), info.size_x, info.size_y });
        }
        if (reference == null) {
            var kept: Summary = .{
                .size_x = info.size_x,
                .size_y = info.size_y,
                .season = info.season,
                .season_folder = try dupe(allocator, info.season_folder),
            };
            errdefer kept.deinit(allocator);
            reference = kept;
        } else {
            var differ = std.ArrayListUnmanaged(u8).empty;
            defer differ.deinit(allocator);
            if (reference.?.season != info.season) {
                try differ.print(allocator, "Invalid Season: first node {s}, this one {s}. ", .{ seasonName(reference.?.season, reference.?.season_folder), seasonName(info.season, info.season_folder) });
            }
            if (!std.ascii.eqlIgnoreCase(reference.?.season_folder, info.season_folder)) {
                try differ.print(allocator, "Invalid Season Folder: first node <{s}>, this one <{s}>. ", .{ reference.?.season_folder, info.season_folder });
            }
            if (differ.items.len != 0) {
                try report.add(allocator, .@"error", @intCast(i), .{ .clear_node_container = i }, "node {d} \"{s}\" does not belong with the first node: {s}", .{ i, node.container, std.mem.trimEnd(u8, differ.items, " ") });
                info.deinit(allocator);
                continue;
            }
        }
        for (info.script_ids) |id| try insertSorted(allocator, &union_ids, id);
        for (info.script_areas) |area| {
            var present = false;
            for (union_areas.items) |known| {
                if (std.mem.eql(u8, known, area)) present = true;
            }
            if (!present) try union_areas.append(allocator, try dupe(allocator, area));
        }
        info.deinit(allocator);
    }
    if (reference) |ref| {
        std.mem.sort([]u8, union_areas.items, {}, lessThanText);
        const expected: Summary = .{ .season = ref.season, .season_folder = ref.season_folder, .script_ids = union_ids.items, .script_areas = union_areas.items };
        const actual: Summary = .{ .season = graph.season, .season_folder = graph.season_folder, .script_ids = graph.script_ids.items, .script_areas = graph.script_areas.items };
        const differ = try describeMismatch(allocator, actual, expected, "graph", "its containers");
        defer allocator.free(differ);
        if (differ.len != 0) try report.add(allocator, .@"error", -1, .recompute_header, "the graph's own season, folder or script lists are not what its containers give: {s}", .{std.mem.trimEnd(u8, differ, " ")});
    }
    for (graph.links.items, 0..) |link, i| {
        const count: i32 = @intCast(graph.nodes.items.len);
        if (link.a < 0 or link.a >= count or link.b < 0 or link.b >= count or link.a == link.b) {
            try report.add(allocator, .@"error", @intCast(i), .{ .remove_link = i }, "link {d} joins nodes {d} and {d}: not two different nodes of this graph", .{ i, link.a, link.b });
            continue;
        }
        if (link.parts < min_parts) {
            try report.add(allocator, .@"error", @intCast(i), .{ .set_parts = i }, "link {d} has {d} parts: the engine raises it to {d} (RMGeneration.cpp:1455)", .{ i, link.parts, min_parts });
        }
        if (!isStorageRelative(link.desc)) {
            const fix: Fix = if (storageSuffix(link.desc, "terrain\\") != null) .{ .strip_link_desc = i } else .none;
            try report.add(allocator, .@"error", @intCast(i), fix, "link {d}: descriptor \"{s}\" is not a storage name (it has a drive or a root in front)", .{ i, link.desc });
        }
        if (link.kind != link_road and link.kind != link_river) {
            try report.add(allocator, .warning, @intCast(i), .none, "link {d} is typed {d}: the generator treats every type but road as a river", .{ i, link.kind });
        }
        if (link.desc.len == 0) try report.add(allocator, .warning, @intCast(i), .none, "link {d} is empty: it names no road or river", .{i});
    }
    return report;
}

// ---------------------------------------------------------------------------
// Fixes
// ---------------------------------------------------------------------------

/// Applies one container fix. Removals renumber the lists; `recompute_header`
/// takes the first patch's own season, folder and script lists from `source`.
pub fn applyContainerFix(allocator: Allocator, container: *Container, source: Source, fix: Fix) Allocator.Error!void {
    switch (fix) {
        .remove_patch => |index| container.removePatches(allocator, &.{index}),
        .set_patch_size => |change| {
            if (change.index < container.patches.items.len) {
                container.patches.items[change.index].size_x = change.size_x;
                container.patches.items[change.index].size_y = change.size_y;
                container.recomputeSize();
            }
        },
        .drop_bad_indices => {
            const count = container.patches.items.len;
            for (&container.indices) |*list| {
                var at: usize = 0;
                for (list.items) |entry| {
                    if (entry < 0 or entry >= count) continue;
                    list.items[at] = entry;
                    at += 1;
                }
                list.shrinkRetainingCapacity(at);
            }
        },
        .recompute_header => for (container.patches.items) |patch| {
            var info = source.patch(allocator, patch.name) orelse continue;
            defer info.deinit(allocator);
            try container.setHeaderFrom(allocator, info);
            break;
        },
        .recompute_size => container.recomputeSize(),
        else => {},
    }
}

/// Fix all (one undo step): sizes first (they name patches by their old
/// numbers), then every removal in one pass so the numbers stay true, then the
/// header and size are taken from what is left. Returns the findings fixed.
pub fn fixAllContainer(allocator: Allocator, container: *Container, source: Source, report: *const Report) Allocator.Error!usize {
    var doomed = std.ArrayListUnmanaged(usize).empty;
    defer doomed.deinit(allocator);
    var fixed: usize = 0;
    var header = false;
    var bad_indices = false;
    for (report.findings.items) |finding| {
        switch (finding.fix) {
            .none => continue,
            .set_patch_size => try applyContainerFix(allocator, container, source, finding.fix),
            .remove_patch => |index| try doomed.append(allocator, index),
            .recompute_header => header = true,
            .drop_bad_indices => bad_indices = true,
            else => {},
        }
        fixed += 1;
    }
    if (bad_indices) try applyContainerFix(allocator, container, source, .drop_bad_indices);
    if (doomed.items.len != 0) container.removePatches(allocator, doomed.items);
    if (header or doomed.items.len != 0) try applyContainerFix(allocator, container, source, .recompute_header);
    container.recomputeSize();
    return fixed;
}

pub fn applyGraphFix(allocator: Allocator, graph: *Graph, source: Source, fix: Fix) Allocator.Error!void {
    switch (fix) {
        .remove_link => |index| graph.removeLink(allocator, index),
        .set_parts => |index| {
            if (index < graph.links.items.len) graph.links.items[index].parts = min_parts;
        },
        .clear_node_container => |index| {
            if (index < graph.nodes.items.len) try setText(allocator, &graph.nodes.items[index].container, "");
        },
        .strip_node_name => |index| {
            if (index < graph.nodes.items.len) {
                if (storageSuffix(graph.nodes.items[index].container, "scenarios\\")) |stripped| {
                    const copy = try allocator.dupe(u8, stripped);
                    allocator.free(graph.nodes.items[index].container);
                    graph.nodes.items[index].container = copy;
                }
            }
        },
        .strip_link_desc => |index| {
            if (index < graph.links.items.len) {
                if (storageSuffix(graph.links.items[index].desc, "terrain\\")) |stripped| {
                    const copy = try allocator.dupe(u8, stripped);
                    allocator.free(graph.links.items[index].desc);
                    graph.links.items[index].desc = copy;
                }
            }
        },
        .recompute_header => try recomputeGraphHeader(allocator, graph, source),
        else => {},
    }
}

/// The graph's season, folder and script lists from the filled nodes'
/// containers (the MFC's Check! rewrote them this way).
pub fn recomputeGraphHeader(allocator: Allocator, graph: *Graph, source: Source) Allocator.Error!void {
    var first = true;
    for (graph.nodes.items) |node| {
        if (node.container.len == 0) continue;
        var info = source.container(allocator, node.container) orelse continue;
        defer info.deinit(allocator);
        if (first) {
            try graph.setHeaderFrom(allocator, info);
            first = false;
        } else {
            try graph.mergeScripts(allocator, info);
        }
    }
    if (first) graph.clearHeader(allocator);
}

pub fn fixAllGraph(allocator: Allocator, graph: *Graph, source: Source, report: *const Report) Allocator.Error!usize {
    var fixed: usize = 0;
    var header = false;
    // Parts and cleared nodes do not renumber anything; links go highest first.
    var doomed_links = std.ArrayListUnmanaged(usize).empty;
    defer doomed_links.deinit(allocator);
    for (report.findings.items) |finding| switch (finding.fix) {
        .set_parts => |index| {
            try applyGraphFix(allocator, graph, source, .{ .set_parts = index });
            fixed += 1;
        },
        .clear_node_container => |index| {
            try applyGraphFix(allocator, graph, source, .{ .clear_node_container = index });
            fixed += 1;
            header = true;
        },
        .strip_node_name, .strip_link_desc => {
            try applyGraphFix(allocator, graph, source, finding.fix);
            fixed += 1;
            header = true;
        },
        .remove_link => |index| try doomed_links.append(allocator, index),
        .recompute_header => header = true,
        else => {},
    };
    std.mem.sort(usize, doomed_links.items, {}, std.sort.desc(usize));
    for (doomed_links.items) |index| {
        graph.removeLink(allocator, index);
        fixed += 1;
    }
    if (header) {
        try recomputeGraphHeader(allocator, graph, source);
        fixed += 1;
    }
    return fixed;
}

// ---------------------------------------------------------------------------
// Composer documents: a file, its own dirty flag and its own undo
// ---------------------------------------------------------------------------

/// A container file being edited. Edits go through `begin()`, which keeps the
/// state before the edit for Undo; a refused edit is `cancel()`led. This is
/// file-level state, never the map's history (composer writes are
/// file-level, D-40.8).
pub fn Document(comptime T: type) type {
    return struct {
        const Self = @This();

        allocator: Allocator,
        /// The dirty flag from before the last `begin`, for `cancel`.
        dirty_before: bool = false,
        /// The storage name the file has, or empty for a new one.
        name: []u8 = &.{},
        current: T = .{},
        /// Whether `name` is a shipped file (Save becomes Save As).
        shipped: bool = false,
        dirty: bool = false,
        undo_stack: std.ArrayListUnmanaged(T) = .empty,
        redo_stack: std.ArrayListUnmanaged(T) = .empty,

        pub fn init(allocator: Allocator) Self {
            return .{ .allocator = allocator };
        }

        pub fn deinit(self: *Self) void {
            self.allocator.free(self.name);
            self.current.deinit(self.allocator);
            self.clearHistory();
            self.undo_stack.deinit(self.allocator);
            self.redo_stack.deinit(self.allocator);
        }

        fn clearHistory(self: *Self) void {
            for (self.undo_stack.items) |*item| item.deinit(self.allocator);
            self.undo_stack.clearRetainingCapacity();
            for (self.redo_stack.items) |*item| item.deinit(self.allocator);
            self.redo_stack.clearRetainingCapacity();
        }

        /// A fresh document: the named file's content (taken over), clean.
        pub fn load(self: *Self, name: []const u8, content: T, shipped: bool) Allocator.Error!void {
            const owned = try dupe(self.allocator, name);
            self.allocator.free(self.name);
            self.name = owned;
            self.current.deinit(self.allocator);
            self.current = content;
            self.shipped = shipped;
            self.dirty = false;
            self.clearHistory();
        }

        pub fn setName(self: *Self, name: []const u8, shipped: bool) Allocator.Error!void {
            try setText(self.allocator, &self.name, name);
            self.shipped = shipped;
        }

        /// Keeps the state before an edit. Returns the content to edit.
        pub fn begin(self: *Self) Allocator.Error!*T {
            var before = try self.current.clone(self.allocator);
            errdefer before.deinit(self.allocator);
            if (self.undo_stack.items.len >= undo_depth) {
                var oldest = self.undo_stack.orderedRemove(0);
                oldest.deinit(self.allocator);
            }
            try self.undo_stack.append(self.allocator, before);
            for (self.redo_stack.items) |*item| item.deinit(self.allocator);
            self.redo_stack.clearRetainingCapacity();
            self.dirty_before = self.dirty;
            self.dirty = true;
            return &self.current;
        }

        /// Takes back what `begin` kept, for an edit that changed nothing.
        pub fn cancel(self: *Self) void {
            if (self.undo_stack.pop()) |popped| {
                var item = popped;
                item.deinit(self.allocator);
            }
            self.dirty = self.dirty_before;
        }

        /// Adopts a state the caller made as the undo point (a gesture that
        /// cloned the content when it began).
        pub fn pushUndo(self: *Self, before: T) Allocator.Error!void {
            var owned = before;
            errdefer owned.deinit(self.allocator);
            if (self.undo_stack.items.len >= undo_depth) {
                var oldest = self.undo_stack.orderedRemove(0);
                oldest.deinit(self.allocator);
            }
            try self.undo_stack.append(self.allocator, owned);
            for (self.redo_stack.items) |*item| item.deinit(self.allocator);
            self.redo_stack.clearRetainingCapacity();
            self.dirty = true;
        }

        pub fn canUndo(self: *const Self) bool {
            return self.undo_stack.items.len != 0;
        }
        pub fn canRedo(self: *const Self) bool {
            return self.redo_stack.items.len != 0;
        }

        pub fn undo(self: *Self) Allocator.Error!bool {
            const popped = self.undo_stack.pop() orelse return false;
            var previous = popped;
            errdefer previous.deinit(self.allocator);
            const now = try self.current.clone(self.allocator);
            self.redo_stack.append(self.allocator, now) catch |err| {
                var drop = now;
                drop.deinit(self.allocator);
                return err;
            };
            self.current.deinit(self.allocator);
            self.current = previous;
            self.dirty = true;
            return true;
        }

        pub fn redo(self: *Self) Allocator.Error!bool {
            const popped = self.redo_stack.pop() orelse return false;
            var next = popped;
            errdefer next.deinit(self.allocator);
            const now = try self.current.clone(self.allocator);
            self.undo_stack.append(self.allocator, now) catch |err| {
                var drop = now;
                drop.deinit(self.allocator);
                return err;
            };
            self.current.deinit(self.allocator);
            self.current = next;
            self.dirty = true;
            return true;
        }

        /// After a successful save: the file is the content, under `name`.
        pub fn markSaved(self: *Self, name: []const u8) Allocator.Error!void {
            try setText(self.allocator, &self.name, name);
            self.shipped = false;
            self.dirty = false;
        }
    };
}

pub const ContainerDoc = Document(Container);
pub const GraphDoc = Document(Graph);

// ---------------------------------------------------------------------------
// Field sets (M3 05-10, D-06/D-07/D-12): SRMFieldSet as an owned value, its
// edit rules (the three MFC tabs' own limits) and its Check! rules
// ---------------------------------------------------------------------------

/// The MFC's defaults: a new shell (RMG_FieldTerrainDialog.cpp OnAddShell,
/// RMG_FieldObjectsDialog.cpp OnAddShell), a tile or object added to one, and a
/// field set made new (RMG_CreateFieldDialog.cpp OnAddFieldButton).
pub const default_tile_weight: i32 = 1;
pub const default_object_weight: i32 = 1;
pub const default_shell_width: f32 = 2.0;
pub const default_shell_step: i32 = 4;
pub const default_shell_ratio: f32 = 0.3;
pub const default_field_height: f32 = 2.0;
pub const default_pattern_min: i32 = 3;
pub const default_pattern_max: i32 = 5;
pub const default_profile = "scenarios\\profiles\\profile";
/// The limits the Check! and the Heights tab hold a field set to.
pub const max_field_height: f32 = 5.0;
pub const min_pattern: i32 = 1;
pub const max_pattern: i32 = 16;
pub const max_shell_width: f32 = 512.0;
/// CMapInfo::REAL_SEASONS_COUNT: a field set's season is 0..2.
pub const real_season_count: i32 = 3;

/// The season (0 summer, 1 winter, 2 africa, 3 spring) the tileset of a field
/// set, a template or a graph is the one of.
pub fn seasonIndex(season: i32, season_folder: []const u8) usize {
    if (season == 0 and std.ascii.eqlIgnoreCase(season_folder, season_folders[3])) return 3;
    return if (season < 0) 0 else @min(@as(usize, @intCast(season)), 3);
}

pub const WeightedTile = struct { tile: i32, weight: i32 };

pub const WeightedName = struct {
    name: []u8 = &.{},
    weight: i32 = 1,

    pub fn clone(self: WeightedName, allocator: Allocator) Allocator.Error!WeightedName {
        return .{ .name = try dupe(allocator, self.name), .weight = self.weight };
    }
    pub fn deinit(self: *WeightedName, allocator: Allocator) void {
        allocator.free(self.name);
        self.name = &.{};
    }
};

fn sameFloat32(a: f32, b: f32) bool {
    return @abs(a - b) <= 1e-5 * @max(1.0, @max(@abs(a), @abs(b)));
}

pub const TileShell = struct {
    width: f32 = default_shell_width,
    tiles: std.ArrayListUnmanaged(WeightedTile) = .empty,

    pub fn clone(self: *const TileShell, allocator: Allocator) Allocator.Error!TileShell {
        var out: TileShell = .{ .width = self.width };
        errdefer out.deinit(allocator);
        try out.tiles.appendSlice(allocator, self.tiles.items);
        return out;
    }
    pub fn deinit(self: *TileShell, allocator: Allocator) void {
        self.tiles.deinit(allocator);
        self.* = .{};
    }
    pub fn eql(self: *const TileShell, other: *const TileShell) bool {
        if (!sameFloat32(self.width, other.width) or self.tiles.items.len != other.tiles.items.len) return false;
        for (self.tiles.items, other.tiles.items) |a, b| if (a.tile != b.tile or a.weight != b.weight) return false;
        return true;
    }
};

pub const ObjectShell = struct {
    width: f32 = default_shell_width,
    /// The distance between objects, VIS tiles.
    step: i32 = default_shell_step,
    /// 0..1 (the dialog shows percent).
    ratio: f32 = default_shell_ratio,
    objects: std.ArrayListUnmanaged(WeightedName) = .empty,

    pub fn clone(self: *const ObjectShell, allocator: Allocator) Allocator.Error!ObjectShell {
        var out: ObjectShell = .{ .width = self.width, .step = self.step, .ratio = self.ratio };
        errdefer out.deinit(allocator);
        try out.objects.ensureTotalCapacity(allocator, self.objects.items.len);
        for (self.objects.items) |object| out.objects.appendAssumeCapacity(try object.clone(allocator));
        return out;
    }
    pub fn deinit(self: *ObjectShell, allocator: Allocator) void {
        for (self.objects.items) |*object| object.deinit(allocator);
        self.objects.deinit(allocator);
        self.* = .{};
    }
    pub fn eql(self: *const ObjectShell, other: *const ObjectShell) bool {
        if (!sameFloat32(self.width, other.width) or self.step != other.step or !sameFloat32(self.ratio, other.ratio) or self.objects.items.len != other.objects.items.len) return false;
        for (self.objects.items, other.objects.items) |a, b| if (a.weight != b.weight or !std.mem.eql(u8, a.name, b.name)) return false;
        return true;
    }
};

/// SRMFieldSet: the terrain shells (tiles of the season's tileset by weight),
/// the object shells (objects of the database by weight) and the heights block.
pub const FieldSet = struct {
    season: i32 = 0,
    season_folder: []u8 = &.{},
    /// The height profile's storage name, no ".tga".
    profile: []u8 = &.{},
    height: f32 = default_field_height,
    pattern_min: i32 = default_pattern_min,
    pattern_max: i32 = default_pattern_max,
    /// 0..1 (the dialog shows percent).
    positive_ratio: f32 = 0.5,
    tile_shells: std.ArrayListUnmanaged(TileShell) = .empty,
    object_shells: std.ArrayListUnmanaged(ObjectShell) = .empty,

    /// What File > New starts from (OnAddFieldButton's own defaults: summer, the
    /// stock profile, height 2, patterns 3..5, 50% positive).
    pub fn initNew(allocator: Allocator) Allocator.Error!FieldSet {
        var out: FieldSet = .{};
        errdefer out.deinit(allocator);
        out.season_folder = try dupe(allocator, season_folders[0]);
        out.profile = try dupe(allocator, default_profile);
        return out;
    }

    pub fn deinit(self: *FieldSet, allocator: Allocator) void {
        allocator.free(self.season_folder);
        allocator.free(self.profile);
        for (self.tile_shells.items) |*shell| shell.deinit(allocator);
        self.tile_shells.deinit(allocator);
        for (self.object_shells.items) |*shell| shell.deinit(allocator);
        self.object_shells.deinit(allocator);
        self.* = .{};
    }

    pub fn clone(self: *const FieldSet, allocator: Allocator) Allocator.Error!FieldSet {
        var out: FieldSet = .{ .season = self.season, .height = self.height, .pattern_min = self.pattern_min, .pattern_max = self.pattern_max, .positive_ratio = self.positive_ratio };
        errdefer out.deinit(allocator);
        out.season_folder = try dupe(allocator, self.season_folder);
        out.profile = try dupe(allocator, self.profile);
        try out.tile_shells.ensureTotalCapacity(allocator, self.tile_shells.items.len);
        for (self.tile_shells.items) |*shell| out.tile_shells.appendAssumeCapacity(try shell.clone(allocator));
        try out.object_shells.ensureTotalCapacity(allocator, self.object_shells.items.len);
        for (self.object_shells.items) |*shell| out.object_shells.appendAssumeCapacity(try shell.clone(allocator));
        return out;
    }

    pub fn eql(self: *const FieldSet, other: *const FieldSet) bool {
        if (self.season != other.season or !std.mem.eql(u8, self.season_folder, other.season_folder) or !std.mem.eql(u8, self.profile, other.profile)) return false;
        if (!sameFloat32(self.height, other.height) or self.pattern_min != other.pattern_min or self.pattern_max != other.pattern_max or !sameFloat32(self.positive_ratio, other.positive_ratio)) return false;
        if (self.tile_shells.items.len != other.tile_shells.items.len or self.object_shells.items.len != other.object_shells.items.len) return false;
        for (self.tile_shells.items, other.tile_shells.items) |*a, *b| if (!a.eql(b)) return false;
        for (self.object_shells.items, other.object_shells.items) |*a, *b| if (!a.eql(b)) return false;
        return true;
    }

    /// The season combo's choice (0 summer .. 3 spring): the season number it
    /// stores and its tileset folder (RMG_FieldTerrainDialog.cpp
    /// OnSelchangeSeasonCombo).
    pub fn setSeasonIndex(self: *FieldSet, allocator: Allocator, index: usize) Allocator.Error!void {
        if (index >= season_folders.len) return;
        const folder = try dupe(allocator, season_folders[index]);
        allocator.free(self.season_folder);
        self.season_folder = folder;
        self.season = real_seasons[index];
    }

    pub fn seasonSlot(self: *const FieldSet) usize {
        return seasonIndex(self.season, self.season_folder);
    }

    pub fn setProfile(self: *FieldSet, allocator: Allocator, name: []const u8) Allocator.Error!void {
        try setText(allocator, &self.profile, name);
    }

    /// A shell appended with the MFC's default width; returns its index.
    pub fn addTileShell(self: *FieldSet, allocator: Allocator) Allocator.Error!usize {
        try self.tile_shells.append(allocator, .{});
        return self.tile_shells.items.len - 1;
    }

    pub fn addObjectShell(self: *FieldSet, allocator: Allocator) Allocator.Error!usize {
        try self.object_shells.append(allocator, .{});
        return self.object_shells.items.len - 1;
    }

    /// Takes the shells at `doomed` (any order) out.
    pub fn removeTileShells(self: *FieldSet, allocator: Allocator, doomed: []const usize) void {
        removeIndexed(TileShell, allocator, &self.tile_shells, doomed);
    }

    pub fn removeObjectShells(self: *FieldSet, allocator: Allocator, doomed: []const usize) void {
        removeIndexed(ObjectShell, allocator, &self.object_shells, doomed);
    }

    /// Adds terrain type `tile` to a shell with the default weight; false when
    /// the shell is not there or already holds it (the MFC's OnAddTile skipped
    /// a repeat).
    pub fn addTile(self: *FieldSet, allocator: Allocator, shell: usize, tile: i32) Allocator.Error!bool {
        if (shell >= self.tile_shells.items.len) return false;
        const list = &self.tile_shells.items[shell].tiles;
        for (list.items) |entry| if (entry.tile == tile) return false;
        try list.append(allocator, .{ .tile = tile, .weight = default_tile_weight });
        return true;
    }

    pub fn removeTiles(self: *FieldSet, shell: usize, doomed: []const usize) void {
        if (shell >= self.tile_shells.items.len) return;
        removeIndexedPlain(WeightedTile, &self.tile_shells.items[shell].tiles, doomed);
    }

    pub fn addObject(self: *FieldSet, allocator: Allocator, shell: usize, name: []const u8) Allocator.Error!bool {
        if (shell >= self.object_shells.items.len) return false;
        const list = &self.object_shells.items[shell].objects;
        for (list.items) |entry| if (std.mem.eql(u8, entry.name, name)) return false;
        var made: WeightedName = .{ .name = try dupe(allocator, name), .weight = default_object_weight };
        errdefer made.deinit(allocator);
        try list.append(allocator, made);
        return true;
    }

    pub fn removeObjects(self: *FieldSet, allocator: Allocator, shell: usize, doomed: []const usize) void {
        if (shell >= self.object_shells.items.len) return;
        removeIndexed(WeightedName, allocator, &self.object_shells.items[shell].objects, doomed);
    }

    /// The Heights tab's pattern edits (RMG_FieldHeightsDialog.cpp
    /// OnChangeSizeMinEdit / OnChangeSizeMaxEdit): a value outside 1..16 is
    /// ignored, and the other end follows so min never passes max.
    pub fn setPatternMin(self: *FieldSet, value: i32) bool {
        if (value < min_pattern or value > max_pattern) return false;
        self.pattern_min = value;
        if (self.pattern_max < value) self.pattern_max = value;
        return true;
    }

    pub fn setPatternMax(self: *FieldSet, value: i32) bool {
        if (value < min_pattern or value > max_pattern) return false;
        self.pattern_max = value;
        if (self.pattern_min > value) self.pattern_min = value;
        return true;
    }

    /// Height 0..5 (OnChangeHeightEdit); false leaves it.
    pub fn setHeight(self: *FieldSet, value: f32) bool {
        if (!std.math.isFinite(value) or value < 0 or value > max_field_height) return false;
        self.height = value;
        return true;
    }

    /// The positive ratio in PERCENT 0..100 (OnChangePositiveRatioEdit).
    pub fn setPositivePercent(self: *FieldSet, percent: f32) bool {
        if (!std.math.isFinite(percent) or percent < 0 or percent > 100) return false;
        self.positive_ratio = percent / 100.0;
        return true;
    }

    pub fn tileEntryCount(self: *const FieldSet) usize {
        var n: usize = 0;
        for (self.tile_shells.items) |shell| n += shell.tiles.items.len;
        return n;
    }

    pub fn objectEntryCount(self: *const FieldSet) usize {
        var n: usize = 0;
        for (self.object_shells.items) |shell| n += shell.objects.items.len;
        return n;
    }
};

fn removeIndexed(comptime T: type, allocator: Allocator, list: *std.ArrayListUnmanaged(T), doomed: []const usize) void {
    var kept: usize = 0;
    for (list.items, 0..) |*item, i| {
        var gone = false;
        for (doomed) |d| if (d == i) {
            gone = true;
        };
        if (gone) {
            item.deinit(allocator);
            continue;
        }
        list.items[kept] = item.*;
        kept += 1;
    }
    list.shrinkRetainingCapacity(kept);
}

fn removeIndexedPlain(comptime T: type, list: *std.ArrayListUnmanaged(T), doomed: []const usize) void {
    var kept: usize = 0;
    for (list.items, 0..) |item, i| {
        var gone = false;
        for (doomed) |d| if (d == i) {
            gone = true;
        };
        if (gone) continue;
        list.items[kept] = item;
        kept += 1;
    }
    list.shrinkRetainingCapacity(kept);
}

/// What the field set's Check! needs to know that the value itself does not
/// say: how many terrain types the season's tileset has (null: it will not
/// load), whether an object name is in the database's catalogue, whether the
/// profile's .tga is in the storages. The Editor's own source asks the bridge;
/// tests supply a table.
pub const FieldSource = struct {
    ctx: *anyopaque,
    tile_count_fn: *const fn (ctx: *anyopaque, season_slot: usize) ?usize,
    object_fn: *const fn (ctx: *anyopaque, name: []const u8) bool,
    profile_fn: *const fn (ctx: *anyopaque, name: []const u8) bool,

    pub fn tileCount(self: FieldSource, season_slot: usize) ?usize {
        return self.tile_count_fn(self.ctx, season_slot);
    }
    pub fn hasObject(self: FieldSource, name: []const u8) bool {
        return self.object_fn(self.ctx, name);
    }
    pub fn hasProfile(self: FieldSource, name: []const u8) bool {
        return self.profile_fn(self.ctx, name);
    }
};

/// The Fields Composer's Check! (RMG_CreateFieldDialog.cpp
/// OnCheckFieldsButton, D-12): the MFC rewrote every field set it did not like
/// without a word; this lists what it would have changed - ranges, tile indices
/// against the tileset, object names against the catalogue, the profile against
/// the storage - and rewrites nothing. Every finding that has a repair names it
/// (`Finding.fix`); removals are the person's choice.
pub fn checkFieldSet(allocator: Allocator, field: *const FieldSet, source: FieldSource) Allocator.Error!Report {
    var report: Report = .{};
    errdefer report.deinit(allocator);
    if (field.season < 0 or field.season >= real_season_count) {
        try report.add(allocator, .@"error", -1, .set_season_summer, "season {d} is not a season of the game (0..{d}); the MFC made it summer", .{ field.season, real_season_count - 1 });
    }
    const slot = if (field.season < 0 or field.season >= real_season_count) 0 else field.seasonSlot();
    const tile_count = source.tileCount(slot);
    if (tile_count == null) {
        try report.add(allocator, .warning, -1, .none, "the tileset of {s} does not load: tile indices cannot be checked", .{season_names[slot]});
    }
    if (!std.math.isFinite(field.height) or field.height < 0 or field.height > max_field_height) {
        try report.add(allocator, .@"error", -1, .clamp_height, "height {d:.2} is outside 0..{d}", .{ field.height, @as(i32, @intFromFloat(max_field_height)) });
    }
    if (!std.math.isFinite(field.positive_ratio) or field.positive_ratio < 0 or field.positive_ratio > 1) {
        try report.add(allocator, .@"error", -1, .clamp_ratio, "the positive ratio {d:.2}% is outside 0..100%", .{field.positive_ratio * 100});
    }
    if (!source.hasProfile(field.profile)) {
        try report.add(allocator, .@"error", -1, .reset_profile, "the profile \"{s}\" is not in the storages (.tga); the MFC reset it to {s}", .{ field.profile, default_profile });
    }
    if (field.pattern_min < min_pattern or field.pattern_min > max_pattern or field.pattern_max < min_pattern or field.pattern_max > max_pattern or field.pattern_min > field.pattern_max) {
        try report.add(allocator, .@"error", -1, .fix_pattern, "the pattern size {d} - {d} is not within {d}..{d} with min not above max", .{ field.pattern_min, field.pattern_max, min_pattern, max_pattern });
    }
    if (field.tile_shells.items.len == 0 and field.object_shells.items.len == 0) {
        try report.add(allocator, .warning, -1, .none, "the field set has no shells: applying it paints nothing", .{});
    }
    for (field.tile_shells.items, 0..) |shell, si| {
        if (!std.math.isFinite(shell.width) or shell.width < 0 or shell.width > max_shell_width) {
            try report.add(allocator, .@"error", @intCast(si), .{ .shell_width = .{ .objects = false, .shell = si } }, "terrain shell {d}: width {d:.2} is outside 0..{d}", .{ si, shell.width, @as(i32, @intFromFloat(max_shell_width)) });
        }
        if (shell.tiles.items.len == 0) try report.add(allocator, .warning, @intCast(si), .none, "terrain shell {d} holds no tiles", .{si});
        for (shell.tiles.items, 0..) |entry, ti| {
            if (entry.tile < 0 or (tile_count != null and entry.tile >= tile_count.?)) {
                try report.add(allocator, .@"error", @intCast(si), .{ .remove_tile = .{ .shell = si, .index = ti } }, "terrain shell {d}: tile {d} is not a terrain type of the {s} tileset ({d} types)", .{ si, entry.tile, season_names[slot], tile_count orelse 0 });
            } else if (entry.weight < 0) {
                try report.add(allocator, .@"error", @intCast(si), .{ .zero_tile_weight = .{ .shell = si, .index = ti } }, "terrain shell {d}: tile {d} has weight {d} below 0", .{ si, entry.tile, entry.weight });
            }
        }
    }
    for (field.object_shells.items, 0..) |shell, si| {
        if (!std.math.isFinite(shell.width) or shell.width < 0 or shell.width > max_shell_width) {
            try report.add(allocator, .@"error", @intCast(si), .{ .shell_width = .{ .objects = true, .shell = si } }, "objects shell {d}: width {d:.2} is outside 0..{d}", .{ si, shell.width, @as(i32, @intFromFloat(max_shell_width)) });
        }
        if (!std.math.isFinite(shell.ratio) or shell.ratio < 0 or shell.ratio > 1) {
            try report.add(allocator, .@"error", @intCast(si), .{ .object_ratio = si }, "objects shell {d}: probability {d:.2}% is outside 0..100%", .{ si, shell.ratio * 100 });
        }
        if (shell.step <= 0) {
            try report.add(allocator, .@"error", @intCast(si), .{ .object_step = si }, "objects shell {d}: step {d} is not above 0", .{ si, shell.step });
        }
        // An objects shell with no objects is a deliberate gap between two rings (the
        // shipped field sets have thirty of them), so it is not reported.
        for (shell.objects.items, 0..) |entry, oi| {
            if (!source.hasObject(entry.name)) {
                try report.add(allocator, .@"error", @intCast(si), .{ .remove_object = .{ .shell = si, .index = oi } }, "objects shell {d}: \"{s}\" is not an object of the database", .{ si, entry.name });
            } else if (entry.weight < 0) {
                try report.add(allocator, .@"error", @intCast(si), .{ .zero_object_weight = .{ .shell = si, .index = oi } }, "objects shell {d}: \"{s}\" has weight {d} below 0", .{ si, entry.name, entry.weight });
            }
        }
    }
    return report;
}

/// Applies one field set fix. A removal renumbers what follows, so a caller
/// with several removals goes through `fixAllField`.
pub fn applyFieldFix(allocator: Allocator, field: *FieldSet, fix: Fix) Allocator.Error!void {
    switch (fix) {
        .set_season_summer => try field.setSeasonIndex(allocator, 0),
        .clamp_height => field.height = if (!std.math.isFinite(field.height)) default_field_height else std.math.clamp(field.height, 0, max_field_height),
        .clamp_ratio => field.positive_ratio = if (!std.math.isFinite(field.positive_ratio)) 0.5 else std.math.clamp(field.positive_ratio, 0, 1),
        .reset_profile => try field.setProfile(allocator, default_profile),
        .fix_pattern => {
            field.pattern_min = std.math.clamp(field.pattern_min, min_pattern, max_pattern);
            field.pattern_max = std.math.clamp(field.pattern_max, min_pattern, max_pattern);
            if (field.pattern_min > field.pattern_max) std.mem.swap(i32, &field.pattern_min, &field.pattern_max);
        },
        .shell_width => |at| {
            const width: *f32 = if (at.objects) (if (at.shell < field.object_shells.items.len) &field.object_shells.items[at.shell].width else return) else (if (at.shell < field.tile_shells.items.len) &field.tile_shells.items[at.shell].width else return);
            width.* = if (!std.math.isFinite(width.*)) default_shell_width else std.math.clamp(width.*, 0, max_shell_width);
        },
        .remove_tile => |at| field.removeTiles(at.shell, &.{at.index}),
        .zero_tile_weight => |at| {
            if (at.shell < field.tile_shells.items.len and at.index < field.tile_shells.items[at.shell].tiles.items.len) field.tile_shells.items[at.shell].tiles.items[at.index].weight = 0;
        },
        .remove_object => |at| field.removeObjects(allocator, at.shell, &.{at.index}),
        .zero_object_weight => |at| {
            if (at.shell < field.object_shells.items.len and at.index < field.object_shells.items[at.shell].objects.items.len) field.object_shells.items[at.shell].objects.items[at.index].weight = 0;
        },
        .object_step => |shell| {
            if (shell < field.object_shells.items.len) field.object_shells.items[shell].step = 1;
        },
        .object_ratio => |shell| {
            if (shell < field.object_shells.items.len) {
                const ratio = &field.object_shells.items[shell].ratio;
                ratio.* = if (!std.math.isFinite(ratio.*)) default_shell_ratio else std.math.clamp(ratio.*, 0, 1);
            }
        },
        else => {},
    }
}

/// Fix all (one undo step): every repair that renumbers nothing first, then
/// the removals highest index first within each shell. Returns the findings
/// fixed.
pub fn fixAllField(allocator: Allocator, field: *FieldSet, report: *const Report) Allocator.Error!usize {
    var fixed: usize = 0;
    var tile_removals = std.ArrayListUnmanaged([2]usize).empty;
    defer tile_removals.deinit(allocator);
    var object_removals = std.ArrayListUnmanaged([2]usize).empty;
    defer object_removals.deinit(allocator);
    for (report.findings.items) |finding| switch (finding.fix) {
        .none => {},
        .remove_tile => |at| {
            try tile_removals.append(allocator, .{ at.shell, at.index });
            fixed += 1;
        },
        .remove_object => |at| {
            try object_removals.append(allocator, .{ at.shell, at.index });
            fixed += 1;
        },
        else => {
            try applyFieldFix(allocator, field, finding.fix);
            fixed += 1;
        },
    };
    const order = struct {
        fn desc(_: void, a: [2]usize, b: [2]usize) bool {
            return if (a[0] != b[0]) a[0] > b[0] else a[1] > b[1];
        }
    }.desc;
    std.mem.sort([2]usize, tile_removals.items, {}, order);
    for (tile_removals.items) |at| field.removeTiles(at[0], &.{at[1]});
    std.mem.sort([2]usize, object_removals.items, {}, order);
    for (object_removals.items) |at| field.removeObjects(allocator, at[0], &.{at[1]});
    return fixed;
}

pub const FieldSetDoc = Document(FieldSet);

// ---------------------------------------------------------------------------
// The Graphs Composer canvas (D-11)
// ---------------------------------------------------------------------------

pub const Tile = struct { x: i32, y: i32 };

pub const side_min_x: u8 = 1;
pub const side_min_y: u8 = 2;
pub const side_max_x: u8 = 4;
pub const side_max_y: u8 = 8;

pub const Hit = struct {
    node: i32 = -1,
    /// `side_*` flags for a tile on the node's edge row or column.
    sides: u8 = 0,
    link: i32 = -1,
    links: u32 = 0,

    pub fn any(self: Hit) bool {
        return self.node >= 0 or self.link >= 0;
    }
};

fn nodeCenter(rect: Rect) [2]f32 {
    return .{ @as(f32, @floatFromInt(rect.x1 + rect.x2)) * 0.5, @as(f32, @floatFromInt(rect.y1 + rect.y2)) * 0.5 };
}

/// What the point `tile` is on (the MFC's CheckForGraphElement): the first
/// node holding it with the edge flags, and the links whose centre line passes
/// within `half_width` tiles of it (a link outranks nothing: a node under the
/// point is still reported). A link is hit along its segment extended by the
/// half width at both ends.
pub fn hitTest(graph: *const Graph, tile: Tile, half_width: f32) Hit {
    var hit: Hit = .{};
    for (graph.links.items, 0..) |_, i| {
        if (!linkHit(graph, i, tile, half_width)) continue;
        if (hit.link < 0) hit.link = @intCast(i);
        hit.links += 1;
    }
    for (graph.nodes.items, 0..) |node, i| {
        if (!node.rect.contains(tile.x, tile.y)) continue;
        hit.node = @intCast(i);
        if (tile.x == node.rect.x1) hit.sides |= side_min_x;
        if (tile.x == node.rect.x2 - 1) hit.sides |= side_max_x;
        if (tile.y == node.rect.y1) hit.sides |= side_min_y;
        if (tile.y == node.rect.y2 - 1) hit.sides |= side_max_y;
        break;
    }
    return hit;
}

/// Whether link `index`'s centre line passes within `half_width` tiles of
/// `tile` (the segment extended by the half width at both ends).
fn linkHit(graph: *const Graph, index: usize, tile: Tile, half_width: f32) bool {
    const link = graph.links.items[index];
    const count: i32 = @intCast(graph.nodes.items.len);
    if (link.a < 0 or link.a >= count or link.b < 0 or link.b >= count) return false;
    const px: f32 = @floatFromInt(tile.x);
    const py: f32 = @floatFromInt(tile.y);
    const ca = nodeCenter(graph.nodes.items[@intCast(link.a)].rect);
    const cb = nodeCenter(graph.nodes.items[@intCast(link.b)].rect);
    const dx = cb[0] - ca[0];
    const dy = cb[1] - ca[1];
    const length = @sqrt(dx * dx + dy * dy);
    if (length <= 0) return false;
    const ux = dx / length;
    const uy = dy / length;
    const along = (px - ca[0]) * ux + (py - ca[1]) * uy;
    const across = (px - ca[0]) * -uy + (py - ca[1]) * ux;
    return along >= -half_width and along <= length + half_width and @abs(across) <= half_width;
}

/// Every link under `tile`, in link order, up to `out.len`; returns how many
/// were written (the link properties dialog lists them all).
pub fn hitLinks(graph: *const Graph, tile: Tile, half_width: f32, out: []usize) usize {
    var n: usize = 0;
    for (graph.links.items, 0..) |_, i| {
        if (n >= out.len) break;
        if (linkHit(graph, i, tile, half_width)) {
            out[n] = i;
            n += 1;
        }
    }
    return n;
}

pub const CanvasState = enum { none, add, move, resize, link };

pub const CanvasOutcome = enum { none, node_added, node_moved, node_resized, reverted, rejected, link_added };

/// One drag on the canvas: press picks the gesture from what is under the
/// point (Ctrl starts a link, an edge a resize, a node's inside a move, empty
/// ground an add), drag moves/resizes the node live, release decides - an add
/// needs a patch each way and no overlap, a move or resize that overlaps
/// another node goes back, a link needs two different nodes.
pub const Canvas = struct {
    /// Patches shown across (the slider, 1..32); a graph bigger than this
    /// raises it (LoadGraphToControls).
    patches: i32 = 8,
    state: CanvasState = .none,
    start: Tile = .{ .x = 0, .y = 0 },
    current: Tile = .{ .x = 0, .y = 0 },
    node: i32 = -1,
    sides: u8 = 0,
    original: Rect = .{},
    /// The graph as it was when a move or resize began: the undo point.
    pending_before: ?Graph = null,

    pub fn deinit(self: *Canvas, allocator: Allocator) void {
        if (self.pending_before) |*graph| graph.deinit(allocator);
        self.pending_before = null;
    }

    pub fn setPatches(self: *Canvas, patches: i32) void {
        self.patches = std.math.clamp(patches, min_zoom, max_zoom);
    }

    pub fn fitTo(self: *Canvas, graph: *const Graph) void {
        self.setPatches(@max(self.patches, @max(graph.size_x, graph.size_y)));
    }

    pub fn limit(self: *const Canvas) i32 {
        return self.patches * patch_tiles;
    }

    /// The rectangle the add drag is showing (normalised, max exclusive).
    pub fn dragRect(self: *const Canvas) Rect {
        return normalised(self.start, self.current);
    }

    fn normalised(a: Tile, b: Tile) Rect {
        return .{ .x1 = @min(a.x, b.x), .y1 = @min(a.y, b.y), .x2 = @max(a.x, b.x) + 1, .y2 = @max(a.y, b.y) + 1 };
    }

    pub fn press(self: *Canvas, allocator: Allocator, doc: *GraphDoc, tile: Tile, ctrl: bool, half_width: f32) Allocator.Error!void {
        self.finish(allocator);
        self.start = tile;
        self.current = tile;
        if (ctrl) {
            self.state = .link;
            return;
        }
        const hit = hitTest(&doc.current, tile, half_width);
        if (hit.node >= 0) {
            self.node = hit.node;
            self.sides = hit.sides;
            self.original = doc.current.nodes.items[@intCast(hit.node)].rect;
            self.state = if (hit.sides != 0) .resize else .move;
            self.pending_before = try doc.current.clone(allocator);
        } else if (hit.link < 0) {
            self.state = .add;
        }
    }

    fn moved(self: *const Canvas, tile: Tile) Rect {
        const dx = tile.x - self.start.x;
        const dy = tile.y - self.start.y;
        var rect = self.original;
        if (self.state == .move) {
            rect.x1 += dx;
            rect.x2 += dx;
            rect.y1 += dy;
            rect.y2 += dy;
        } else {
            if (self.sides & side_min_x != 0) {
                rect.x1 += dx;
                if (rect.x2 - rect.x1 < patch_tiles) rect.x1 = rect.x2 - patch_tiles;
            } else if (self.sides & side_max_x != 0) {
                rect.x2 += dx;
                if (rect.x2 - rect.x1 < patch_tiles) rect.x2 = rect.x1 + patch_tiles;
            }
            if (self.sides & side_min_y != 0) {
                rect.y1 += dy;
                if (rect.y2 - rect.y1 < patch_tiles) rect.y1 = rect.y2 - patch_tiles;
            } else if (self.sides & side_max_y != 0) {
                rect.y2 += dy;
                if (rect.y2 - rect.y1 < patch_tiles) rect.y2 = rect.y1 + patch_tiles;
            }
        }
        // The canvas's own edges: a rectangle pushed out is slid back in.
        const max = self.limit();
        if (rect.x1 < 0) {
            rect.x2 -= rect.x1;
            rect.x1 = 0;
        }
        if (rect.x2 > max) {
            rect.x1 -= rect.x2 - max;
            rect.x2 = max;
        }
        if (rect.y1 < 0) {
            rect.y2 -= rect.y1;
            rect.y1 = 0;
        }
        if (rect.y2 > max) {
            rect.y1 -= rect.y2 - max;
            rect.y2 = max;
        }
        return rect;
    }

    pub fn drag(self: *Canvas, doc: *GraphDoc, tile: Tile) void {
        self.current = tile;
        if ((self.state == .move or self.state == .resize) and self.node >= 0 and self.node < doc.current.nodes.items.len) {
            doc.current.nodes.items[@intCast(self.node)].rect = self.moved(tile);
            doc.current.refreshSize();
        }
    }

    /// Ends the gesture. A change joins the undo history; an overlap puts the
    /// node back; nothing changed leaves the history alone.
    pub fn release(self: *Canvas, allocator: Allocator, doc: *GraphDoc, tile: Tile) Allocator.Error!CanvasOutcome {
        self.current = tile;
        defer self.finish(allocator);
        switch (self.state) {
            .none => return .none,
            .add => {
                const rect = normalised(self.start, tile);
                var before = try doc.current.clone(allocator);
                if (try doc.current.addNode(allocator, rect)) {
                    try doc.pushUndo(before);
                    return .node_added;
                }
                before.deinit(allocator);
                return .rejected;
            },
            .move, .resize => {
                if (self.node < 0 or self.node >= doc.current.nodes.items.len) return .none;
                const index: usize = @intCast(self.node);
                const now = doc.current.nodes.items[index].rect;
                var overlap = false;
                for (doc.current.nodes.items, 0..) |other, i| {
                    if (i != index and other.rect.intersects(now)) overlap = true;
                }
                if (overlap) {
                    doc.current.nodes.items[index].rect = self.original;
                    doc.current.refreshSize();
                    return .reverted;
                }
                if (now.eql(self.original)) return .none;
                const before = self.pending_before.?;
                self.pending_before = null;
                try doc.pushUndo(before);
                return if (self.state == .move) .node_moved else .node_resized;
            },
            .link => {
                var a: i32 = -1;
                var b: i32 = -1;
                for (doc.current.nodes.items, 0..) |node, i| {
                    if (node.rect.contains(self.start.x, self.start.y)) a = @intCast(i);
                    if (node.rect.contains(tile.x, tile.y)) b = @intCast(i);
                }
                if (a < 0 or b < 0 or a == b) return .rejected;
                var before = try doc.current.clone(allocator);
                errdefer before.deinit(allocator);
                if (!try doc.current.addLink(allocator, @intCast(a), @intCast(b))) return .rejected;
                try doc.pushUndo(before);
                return .link_added;
            },
        }
    }

    fn finish(self: *Canvas, allocator: Allocator) void {
        self.state = .none;
        self.node = -1;
        self.sides = 0;
        if (self.pending_before) |*graph| graph.deinit(allocator);
        self.pending_before = null;
    }

    /// An abandoned gesture (focus lost, window closed): a moved node goes back.
    pub fn cancel(self: *Canvas, allocator: Allocator, doc: *GraphDoc) void {
        if ((self.state == .move or self.state == .resize) and self.node >= 0 and self.node < doc.current.nodes.items.len) {
            doc.current.nodes.items[@intCast(self.node)].rect = self.original;
            doc.current.refreshSize();
        }
        self.finish(allocator);
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

const testing = std.testing;

fn makeSummary(allocator: Allocator, season: i32, folder: []const u8, ids: []const i32, areas: []const []const u8, size: i32) !Summary {
    var summary: Summary = .{ .size_x = size, .size_y = size, .season = season };
    summary.season_folder = try dupe(allocator, folder);
    summary.script_ids = try allocator.dupe(i32, ids);
    const out = try allocator.alloc([]u8, areas.len);
    for (areas, 0..) |area, i| out[i] = try dupe(allocator, area);
    summary.script_areas = out;
    return summary;
}

/// A Source over a small table, for the Check! rules.
const TestSource = struct {
    const Entry = struct { name: []const u8, season: i32, folder: []const u8, ids: []const i32, areas: []const []const u8, size: i32 };
    entries: []const Entry,

    fn find(self: *TestSource, name: []const u8) ?Entry {
        for (self.entries) |entry| if (std.mem.eql(u8, entry.name, name)) return entry;
        return null;
    }
    fn lookup(ctx: *anyopaque, allocator: Allocator, name: []const u8) ?Summary {
        const self: *TestSource = @ptrCast(@alignCast(ctx));
        const entry = self.find(name) orelse return null;
        return makeSummary(allocator, entry.season, entry.folder, entry.ids, entry.areas, entry.size) catch null;
    }
    fn source(self: *TestSource) Source {
        return .{ .ctx = self, .patch_fn = lookup, .container_fn = lookup };
    }
};

test "a container adds patches usable every way and keeps its size" {
    const a = testing.allocator;
    var c: Container = .{};
    defer c.deinit(a);
    try c.addPatch(a, "scenarios\\patches\\summer\\a", 1, 1);
    try c.addPatch(a, "scenarios\\patches\\summer\\b", 2, 3);
    try testing.expectEqual(@as(usize, 2), c.patchCount());
    try testing.expectEqual(@as(i32, 2), c.size_x);
    try testing.expectEqual(@as(i32, 3), c.size_y);
    inline for (0..4) |d| try testing.expect(c.hasDirection(1, @enumFromInt(d)));
    try c.setDirection(a, 1, .east, false);
    try testing.expect(!c.hasDirection(1, .east));
    try testing.expect(c.hasDirection(0, .east));
    try c.setDirection(a, 1, .east, true);
    try testing.expectEqualSlices(i32, &.{ 0, 1 }, c.indices[1].items);
    var copy = try c.clone(a);
    defer copy.deinit(a);
    try testing.expect(c.eql(&copy));
    try c.setPlace(a, 0, "summer_france");
    try testing.expect(!c.eql(&copy));
}

test "removing patches renumbers every direction list and drops entries for the gone" {
    const a = testing.allocator;
    var c: Container = .{};
    defer c.deinit(a);
    for (0..4) |i| {
        var buffer: [16]u8 = undefined;
        try c.addPatch(a, try std.fmt.bufPrint(&buffer, "p{d}", .{i}), 1, 1);
    }
    try c.setDirection(a, 1, .north, false);
    c.removePatches(a, &.{ 2, 0 });
    try testing.expectEqual(@as(usize, 2), c.patchCount());
    try testing.expectEqualStrings("p1", c.patches.items[0].name);
    try testing.expectEqualStrings("p3", c.patches.items[1].name);
    try testing.expectEqualSlices(i32, &.{1}, c.indices[0].items);
    try testing.expectEqualSlices(i32, &.{ 0, 1 }, c.indices[1].items);
    c.removePatches(a, &.{ 0, 1 });
    try testing.expectEqual(@as(usize, 0), c.patchCount());
    try testing.expectEqual(@as(i32, 0), c.size_x);
}

test "supported settings follow the MFC rule: any alone when the unplaced patches cover every way" {
    const a = testing.allocator;
    var c: Container = .{};
    defer c.deinit(a);
    try c.addPatch(a, "a", 1, 1);
    {
        const names = try c.supportedSettings(a);
        defer freeNames(a, names);
        try testing.expectEqual(@as(usize, 1), names.len);
        try testing.expectEqualStrings("<any setting>", names[0]);
    }
    // The only patch is for one setting: that setting is supported, any is not.
    try c.setPlace(a, 0, "Summer_France");
    {
        const names = try c.supportedSettings(a);
        defer freeNames(a, names);
        try testing.expectEqual(@as(usize, 1), names.len);
        try testing.expectEqualStrings("summer_france", names[0]);
    }
    // Take a direction away and it is supported by none.
    try c.setDirection(a, 0, .west, false);
    {
        const names = try c.supportedSettings(a);
        defer freeNames(a, names);
        try testing.expectEqual(@as(usize, 0), names.len);
    }
}

test "adding a patch: the first gives the header, a patch of another season is refused naming it" {
    const a = testing.allocator;
    var entries = [_]TestSource.Entry{
        .{ .name = "p\\one", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{ 3, 4 }, .areas = &.{"Ambush"}, .size = 1 },
        .{ .name = "p\\two", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{ 3, 4 }, .areas = &.{"Ambush"}, .size = 2 },
        .{ .name = "p\\summer", .season = 0, .folder = "terrain\\sets\\1\\", .ids = &.{ 3, 4 }, .areas = &.{"Ambush"}, .size = 1 },
        .{ .name = "p\\ids", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{9}, .areas = &.{"Ambush"}, .size = 1 },
    };
    var table: TestSource = .{ .entries = &entries };
    var c: Container = .{};
    defer c.deinit(a);
    var first = try addPatchChecked(a, &c, table.source(), "p\\one");
    defer first.deinit(a);
    try testing.expect(first == .added);
    try testing.expectEqual(@as(i32, 1), c.season);
    try testing.expectEqualStrings("terrain\\sets\\2\\", c.season_folder);
    try testing.expectEqualSlices(i32, &.{ 3, 4 }, c.script_ids.items);
    var second = try addPatchChecked(a, &c, table.source(), "p\\two");
    defer second.deinit(a);
    try testing.expect(second == .added);
    try testing.expectEqual(@as(i32, 2), c.size_x);
    var wrong = try addPatchChecked(a, &c, table.source(), "p\\summer");
    defer wrong.deinit(a);
    try testing.expect(wrong == .mismatch);
    try testing.expect(std.mem.indexOf(u8, wrong.mismatch, "Invalid Season") != null);
    try testing.expect(std.mem.indexOf(u8, wrong.mismatch, "Winter") != null);
    var ids = try addPatchChecked(a, &c, table.source(), "p\\ids");
    defer ids.deinit(a);
    try testing.expect(ids == .mismatch and std.mem.indexOf(u8, ids.mismatch, "ScriptIDs") != null);
    var missing = try addPatchChecked(a, &c, table.source(), "p\\nope");
    defer missing.deinit(a);
    try testing.expect(missing == .unreadable);
    try testing.expectEqual(@as(usize, 2), c.patchCount());
    // A repeat replaces the earlier entry.
    var again = try addPatchChecked(a, &c, table.source(), "p\\one");
    defer again.deinit(a);
    try testing.expect(again == .added);
    try testing.expectEqual(@as(usize, 2), c.patchCount());
    try testing.expectEqualStrings("p\\one", c.patches.items[1].name);
}

test "container Check! finds a season mismatch, a missing patch and a script-ID mismatch, and Fix all removes them as one step" {
    const a = testing.allocator;
    var entries = [_]TestSource.Entry{
        .{ .name = "p\\good", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{ 3, 4 }, .areas = &.{}, .size = 1 },
        .{ .name = "p\\summer", .season = 0, .folder = "terrain\\sets\\1\\", .ids = &.{ 3, 4 }, .areas = &.{}, .size = 1 },
        .{ .name = "p\\ids", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{9}, .areas = &.{}, .size = 1 },
    };
    var table: TestSource = .{ .entries = &entries };
    var c: Container = .{};
    defer c.deinit(a);
    for ([_][]const u8{ "p\\good", "p\\summer", "p\\nope", "p\\ids" }) |name| try c.addPatch(a, name, 1, 1);
    var header = try makeSummary(a, 1, "terrain\\sets\\2\\", &.{ 3, 4 }, &.{}, 1);
    defer header.deinit(a);
    try c.setHeaderFrom(a, header);
    var report = try checkContainer(a, &c, table.source());
    defer report.deinit(a);
    var season: usize = 0;
    var missing: usize = 0;
    var ids: usize = 0;
    for (report.findings.items) |finding| {
        if (std.mem.indexOf(u8, finding.text, "Invalid Season:") != null) season += 1;
        if (std.mem.indexOf(u8, finding.text, "cannot be loaded") != null) missing += 1;
        if (std.mem.indexOf(u8, finding.text, "ScriptIDs") != null) ids += 1;
    }
    try testing.expectEqual(@as(usize, 1), season);
    try testing.expectEqual(@as(usize, 1), missing);
    try testing.expectEqual(@as(usize, 1), ids);
    try testing.expect(report.errorCount() >= 3);
    // Nothing was rewritten by the check.
    try testing.expectEqual(@as(usize, 4), c.patchCount());
    var before = try c.clone(a);
    defer before.deinit(a);
    const fixed = try fixAllContainer(a, &c, table.source(), &report);
    try testing.expect(fixed >= 3);
    try testing.expectEqual(@as(usize, 1), c.patchCount());
    try testing.expectEqualStrings("p\\good", c.patches.items[0].name);
    var again = try checkContainer(a, &c, table.source());
    defer again.deinit(a);
    try testing.expectEqual(@as(usize, 0), again.errorCount());
}

test "container Check! finds stale sizes and indices that name no patch" {
    const a = testing.allocator;
    var entries = [_]TestSource.Entry{
        .{ .name = "p\\a", .season = 0, .folder = "terrain\\sets\\1\\", .ids = &.{}, .areas = &.{}, .size = 1 },
    };
    var table: TestSource = .{ .entries = &entries };
    var c: Container = .{};
    defer c.deinit(a);
    try c.addPatch(a, "p\\a", 1, 1);
    var header = try makeSummary(a, 0, "terrain\\sets\\1\\", &.{}, &.{}, 1);
    defer header.deinit(a);
    try c.setHeaderFrom(a, header);
    try c.indices[2].append(a, 5);
    c.size_x = 4;
    var report = try checkContainer(a, &c, table.source());
    defer report.deinit(a);
    var sizes: usize = 0;
    var bad: usize = 0;
    for (report.findings.items) |finding| {
        if (finding.fix == .recompute_size) sizes += 1;
        if (finding.fix == .drop_bad_indices) bad += 1;
    }
    try testing.expectEqual(@as(usize, 1), sizes);
    try testing.expectEqual(@as(usize, 1), bad);
    _ = try fixAllContainer(a, &c, table.source(), &report);
    try testing.expectEqual(@as(usize, 1), c.indices[2].items.len);
    try testing.expectEqual(@as(i32, 1), c.size_x);
}

test "a graph adds a node of a patch each way that overlaps nothing" {
    const a = testing.allocator;
    var g: Graph = .{};
    defer g.deinit(a);
    try testing.expect(!try g.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 15, .y2 = 16 }));
    try testing.expect(try g.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 16, .y2 = 16 }));
    try testing.expect(!try g.addNode(a, .{ .x1 = 8, .y1 = 8, .x2 = 40, .y2 = 40 }));
    // A shared edge is not an overlap.
    try testing.expect(try g.addNode(a, .{ .x1 = 16, .y1 = 0, .x2 = 48, .y2 = 16 }));
    try testing.expectEqual(@as(usize, 2), g.nodes.items.len);
    try testing.expectEqual(@as(i32, 3), g.size_x);
    try testing.expectEqual(@as(i32, 1), g.size_y);
}

test "deleting a node takes its links and renumbers the others" {
    const a = testing.allocator;
    var g: Graph = .{};
    defer g.deinit(a);
    for (0..4) |i| _ = try g.addNode(a, .{ .x1 = @intCast(i * 16), .y1 = 0, .x2 = @intCast(i * 16 + 16), .y2 = 16 });
    _ = try g.addLink(a, 0, 1);
    _ = try g.addLink(a, 1, 2);
    _ = try g.addLink(a, 2, 3);
    _ = try g.addLink(a, 0, 3);
    g.removeNode(a, 1);
    try testing.expectEqual(@as(usize, 3), g.nodes.items.len);
    try testing.expectEqual(@as(usize, 2), g.links.items.len);
    try testing.expectEqual(@as(i32, 1), g.links.items[0].a);
    try testing.expectEqual(@as(i32, 2), g.links.items[0].b);
    try testing.expectEqual(@as(i32, 0), g.links.items[1].a);
    try testing.expectEqual(@as(i32, 2), g.links.items[1].b);
    try testing.expect(!try g.addLink(a, 1, 1));
    try testing.expect(!try g.addLink(a, 0, 9));
}

test "hit testing: a node's edge flags and a link along its centre line" {
    const a = testing.allocator;
    var g: Graph = .{};
    defer g.deinit(a);
    _ = try g.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 16, .y2 = 16 });
    _ = try g.addNode(a, .{ .x1 = 48, .y1 = 0, .x2 = 64, .y2 = 16 });
    _ = try g.addLink(a, 0, 1);
    const inside = hitTest(&g, .{ .x = 5, .y = 5 }, 1);
    try testing.expectEqual(@as(i32, 0), inside.node);
    try testing.expectEqual(@as(u8, 0), inside.sides);
    const corner = hitTest(&g, .{ .x = 0, .y = 0 }, 1);
    try testing.expectEqual(@as(u8, side_min_x | side_min_y), corner.sides);
    const far_edge = hitTest(&g, .{ .x = 15, .y = 7 }, 1);
    try testing.expectEqual(side_max_x, far_edge.sides);
    const on_link = hitTest(&g, .{ .x = 30, .y = 8 }, 1);
    try testing.expectEqual(@as(i32, -1), on_link.node);
    try testing.expectEqual(@as(i32, 0), on_link.link);
    const beside = hitTest(&g, .{ .x = 30, .y = 12 }, 1);
    try testing.expect(!beside.any());
    var listed: [4]usize = undefined;
    try testing.expectEqual(@as(usize, 1), hitLinks(&g, .{ .x = 30, .y = 8 }, 1, &listed));
    try testing.expectEqual(@as(usize, 0), listed[0]);
    try testing.expectEqual(@as(usize, 0), hitLinks(&g, .{ .x = 30, .y = 12 }, 1, &listed));
}

test "the canvas adds by dragging on empty ground, and an overlapping add changes nothing" {
    const a = testing.allocator;
    var doc = GraphDoc.init(a);
    defer doc.deinit();
    var canvas: Canvas = .{};
    defer canvas.deinit(a);
    try canvas.press(a, &doc, .{ .x = 0, .y = 0 }, false, 1);
    canvas.drag(&doc, .{ .x = 20, .y = 20 });
    try testing.expectEqual(CanvasOutcome.node_added, try canvas.release(a, &doc, .{ .x = 31, .y = 31 }));
    try testing.expectEqual(@as(usize, 1), doc.current.nodes.items.len);
    try testing.expect(doc.current.nodes.items[0].rect.eql(.{ .x1 = 0, .y1 = 0, .x2 = 32, .y2 = 32 }));
    try testing.expect(doc.dirty and doc.canUndo());
    // Dragging on the node moves it; onto a tile of another add attempt that overlaps is rejected.
    try canvas.press(a, &doc, .{ .x = 40, .y = 40 }, false, 1);
    try testing.expectEqual(CanvasOutcome.rejected, try canvas.release(a, &doc, .{ .x = 10, .y = 10 }));
    // Too small an add is rejected.
    try canvas.press(a, &doc, .{ .x = 64, .y = 64 }, false, 1);
    try testing.expectEqual(CanvasOutcome.rejected, try canvas.release(a, &doc, .{ .x = 70, .y = 70 }));
    try testing.expectEqual(@as(usize, 1), doc.current.nodes.items.len);
    // Undo takes the node back, redo gives it again.
    try testing.expect(try doc.undo());
    try testing.expectEqual(@as(usize, 0), doc.current.nodes.items.len);
    try testing.expect(try doc.redo());
    try testing.expectEqual(@as(usize, 1), doc.current.nodes.items.len);
}

test "the canvas moves a node, stays inside the canvas and reverts an overlap" {
    const a = testing.allocator;
    var doc = GraphDoc.init(a);
    defer doc.deinit();
    _ = try doc.current.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 32, .y2 = 32 });
    _ = try doc.current.addNode(a, .{ .x1 = 64, .y1 = 0, .x2 = 96, .y2 = 32 });
    var canvas: Canvas = .{ .patches = 8 };
    defer canvas.deinit(a);
    // Move node 0 right by 16 tiles: it stops clear of node 1 (96 < 64? no: 16..48).
    try canvas.press(a, &doc, .{ .x = 10, .y = 10 }, false, 1);
    canvas.drag(&doc, .{ .x = 26, .y = 10 });
    try testing.expectEqual(CanvasOutcome.node_moved, try canvas.release(a, &doc, .{ .x = 26, .y = 10 }));
    try testing.expect(doc.current.nodes.items[0].rect.eql(.{ .x1 = 16, .y1 = 0, .x2 = 48, .y2 = 32 }));
    try testing.expectEqual(@as(usize, 1), doc.undo_stack.items.len);
    // Move it onto node 1: the overlap puts it back, no undo step is added.
    try canvas.press(a, &doc, .{ .x = 20, .y = 10 }, false, 1);
    canvas.drag(&doc, .{ .x = 50, .y = 10 });
    try testing.expectEqual(CanvasOutcome.reverted, try canvas.release(a, &doc, .{ .x = 50, .y = 10 }));
    try testing.expect(doc.current.nodes.items[0].rect.eql(.{ .x1 = 16, .y1 = 0, .x2 = 48, .y2 = 32 }));
    try testing.expectEqual(@as(usize, 1), doc.undo_stack.items.len);
    // Out of the canvas on the left: slid back to 0.
    try canvas.press(a, &doc, .{ .x = 30, .y = 10 }, false, 1);
    canvas.drag(&doc, .{ .x = -50, .y = 10 });
    _ = try canvas.release(a, &doc, .{ .x = -50, .y = 10 });
    try testing.expectEqual(@as(i32, 0), doc.current.nodes.items[0].rect.x1);
    try testing.expectEqual(@as(i32, 32), doc.current.nodes.items[0].rect.width());
}

test "the canvas resizes by an edge and keeps a patch each way" {
    const a = testing.allocator;
    var doc = GraphDoc.init(a);
    defer doc.deinit();
    _ = try doc.current.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 32, .y2 = 32 });
    var canvas: Canvas = .{ .patches = 8 };
    defer canvas.deinit(a);
    // The right edge column is x = 31.
    try canvas.press(a, &doc, .{ .x = 31, .y = 10 }, false, 1);
    canvas.drag(&doc, .{ .x = 47, .y = 10 });
    try testing.expectEqual(CanvasOutcome.node_resized, try canvas.release(a, &doc, .{ .x = 47, .y = 10 }));
    try testing.expectEqual(@as(i32, 48), doc.current.nodes.items[0].rect.x2);
    // Dragged past the other side: it stops at a patch.
    try canvas.press(a, &doc, .{ .x = 47, .y = 10 }, false, 1);
    canvas.drag(&doc, .{ .x = -10, .y = 10 });
    _ = try canvas.release(a, &doc, .{ .x = -10, .y = 10 });
    try testing.expectEqual(@as(i32, 16), doc.current.nodes.items[0].rect.x2);
    try testing.expectEqual(@as(i32, 1), doc.current.size_x);
}

test "Ctrl+drag from one node to another links them, and not to nothing or itself" {
    const a = testing.allocator;
    var doc = GraphDoc.init(a);
    defer doc.deinit();
    _ = try doc.current.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 16, .y2 = 16 });
    _ = try doc.current.addNode(a, .{ .x1 = 32, .y1 = 0, .x2 = 48, .y2 = 16 });
    var canvas: Canvas = .{};
    defer canvas.deinit(a);
    try canvas.press(a, &doc, .{ .x = 5, .y = 5 }, true, 1);
    try testing.expectEqual(CanvasOutcome.link_added, try canvas.release(a, &doc, .{ .x = 40, .y = 5 }));
    try testing.expectEqual(@as(usize, 1), doc.current.links.items.len);
    try testing.expectEqual(@as(i32, 0), doc.current.links.items[0].a);
    try testing.expectEqual(@as(i32, 1), doc.current.links.items[0].b);
    try testing.expectEqual(@as(f32, default_radius), doc.current.links.items[0].radius);
    try canvas.press(a, &doc, .{ .x = 5, .y = 5 }, true, 1);
    try testing.expectEqual(CanvasOutcome.rejected, try canvas.release(a, &doc, .{ .x = 6, .y = 6 }));
    try canvas.press(a, &doc, .{ .x = 5, .y = 5 }, true, 1);
    try testing.expectEqual(CanvasOutcome.rejected, try canvas.release(a, &doc, .{ .x = 100, .y = 100 }));
    try testing.expectEqual(@as(usize, 1), doc.current.links.items.len);
}

test "setting a node's container: the first gives the graph its header, a later season is refused, merge adds IDs" {
    const a = testing.allocator;
    var entries = [_]TestSource.Entry{
        .{ .name = "c\\one", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{ 3, 4 }, .areas = &.{"Ambush"}, .size = 1 },
        .{ .name = "c\\two", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{ 4, 5 }, .areas = &.{"Bridge"}, .size = 3 },
        .{ .name = "c\\summer", .season = 0, .folder = "terrain\\sets\\1\\", .ids = &.{}, .areas = &.{}, .size = 1 },
    };
    var table: TestSource = .{ .entries = &entries };
    var g: Graph = .{};
    defer g.deinit(a);
    for (0..3) |i| _ = try g.addNode(a, .{ .x1 = @intCast(i * 16), .y1 = 0, .x2 = @intCast(i * 16 + 16), .y2 = 16 });
    var one = try setNodeContainer(a, &g, 0, "c\\one", table.source());
    defer one.deinit(a);
    try testing.expect(one == .set);
    try testing.expectEqual(@as(i32, 1), g.season);
    var two = try setNodeContainer(a, &g, 1, "c\\two", table.source());
    defer two.deinit(a);
    try testing.expect(two == .set);
    try testing.expectEqualSlices(i32, &.{ 3, 4, 5 }, g.script_ids.items);
    try testing.expectEqual(@as(usize, 2), g.script_areas.items.len);
    var wrong = try setNodeContainer(a, &g, 2, "c\\summer", table.source());
    defer wrong.deinit(a);
    try testing.expect(wrong == .mismatch and std.mem.indexOf(u8, wrong.mismatch, "Invalid Season") != null);
    try testing.expectEqual(@as(usize, 0), g.nodes.items[2].container.len);
    var missing = try setNodeContainer(a, &g, 2, "c\\nope", table.source());
    defer missing.deinit(a);
    try testing.expect(missing == .unreadable);
    // c\two is three patches wide; a 16-tile node is smaller than it.
    try testing.expect(!containerFitsNode(g.nodes.items[1].rect, 3, 3));
    try testing.expect(containerFitsNode(.{ .x1 = 0, .y1 = 0, .x2 = 48, .y2 = 48 }, 3, 3));
    // Removing the last filled node's neighbours leaves the header; removing the last forgets it.
    g.removeNode(a, 1);
    try testing.expectEqual(@as(i32, 1), g.season);
    g.removeNode(a, 0);
    try testing.expectEqual(@as(i32, 0), g.season);
    try testing.expectEqual(@as(usize, 0), g.season_folder.len);
}

test "graph Check! reports a missing container, a mixed season, a link under 8 parts and a bad link, and Fix all repairs them" {
    const a = testing.allocator;
    var entries = [_]TestSource.Entry{
        .{ .name = "c\\one", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{3}, .areas = &.{}, .size = 1 },
        .{ .name = "c\\summer", .season = 0, .folder = "terrain\\sets\\1\\", .ids = &.{}, .areas = &.{}, .size = 1 },
    };
    var table: TestSource = .{ .entries = &entries };
    var g: Graph = .{};
    defer g.deinit(a);
    for (0..4) |i| _ = try g.addNode(a, .{ .x1 = @intCast(i * 16), .y1 = 0, .x2 = @intCast(i * 16 + 16), .y2 = 16 });
    try setText(a, &g.nodes.items[0].container, "c\\one");
    try setText(a, &g.nodes.items[1].container, "c\\summer");
    try setText(a, &g.nodes.items[2].container, "c\\nope");
    _ = try g.addLink(a, 0, 1);
    _ = try g.addLink(a, 1, 2);
    g.links.items[0].parts = 4;
    try setText(a, &g.links.items[0].desc, "terrain\\sets\\2\\roads3d\\road_grunt");
    g.links.items[1].a = 9;
    var report = try checkGraph(a, &g, table.source());
    defer report.deinit(a);
    var kinds = [_]usize{ 0, 0, 0, 0, 0, 0 };
    for (report.findings.items) |finding| {
        if (std.mem.indexOf(u8, finding.text, "cannot be loaded") != null) kinds[0] += 1;
        if (finding.fix == .clear_node_container and std.mem.indexOf(u8, finding.text, "Invalid Season") != null) kinds[1] += 1;
        if (finding.fix == .set_parts) kinds[2] += 1;
        if (finding.fix == .remove_link) kinds[3] += 1;
        if (std.mem.indexOf(u8, finding.text, "is empty") != null) kinds[4] += 1;
        if (finding.fix == .recompute_header) kinds[5] += 1;
    }
    try testing.expectEqual(@as(usize, 1), kinds[0]);
    try testing.expectEqual(@as(usize, 1), kinds[1]);
    try testing.expectEqual(@as(usize, 1), kinds[2]);
    try testing.expectEqual(@as(usize, 1), kinds[3]);
    try testing.expect(kinds[4] >= 1);
    try testing.expectEqual(@as(usize, 1), kinds[5]);
    const fixed = try fixAllGraph(a, &g, table.source(), &report);
    try testing.expect(fixed >= 4);
    try testing.expectEqual(min_parts, g.links.items[0].parts);
    try testing.expectEqual(@as(usize, 1), g.links.items.len);
    try testing.expectEqual(@as(usize, 0), g.nodes.items[1].container.len);
    var again = try checkGraph(a, &g, table.source());
    defer again.deinit(a);
    try testing.expectEqual(@as(usize, 0), again.errorCount());
}

test "a document keeps its own undo, dirty flag and shipped state" {
    const a = testing.allocator;
    var doc = ContainerDoc.init(a);
    defer doc.deinit();
    var content: Container = .{};
    try content.addPatch(a, "p\\a", 1, 1);
    try doc.load("scenarios\\containers\\winter\\army_s", content, true);
    try testing.expect(doc.shipped and !doc.dirty and !doc.canUndo());
    const edit = try doc.begin();
    try edit.addPatch(a, "p\\b", 2, 2);
    try testing.expect(doc.dirty and doc.canUndo());
    try testing.expectEqual(@as(usize, 2), doc.current.patchCount());
    try testing.expect(try doc.undo());
    try testing.expectEqual(@as(usize, 1), doc.current.patchCount());
    try testing.expect(doc.canRedo());
    try testing.expect(try doc.redo());
    try testing.expectEqual(@as(usize, 2), doc.current.patchCount());
    try doc.markSaved("scenarios\\containers\\user\\mine");
    try testing.expect(!doc.shipped and !doc.dirty);
    try testing.expectEqualStrings("scenarios\\containers\\user\\mine", doc.name);
    // A new edit drops the redo branch; the depth is bounded.
    _ = try doc.undo();
    _ = try doc.begin();
    try testing.expect(!doc.canRedo());
    for (0..undo_depth + 10) |_| _ = try doc.begin();
    try testing.expectEqual(undo_depth, doc.undo_stack.items.len);
}

test "season names follow the MFC: season 0 on the spring folder is Spring" {
    try testing.expectEqualStrings("Summer", seasonName(0, "terrain\\sets\\1\\"));
    try testing.expectEqualStrings("Spring", seasonName(0, "Terrain\\Sets\\4\\"));
    try testing.expectEqualStrings("Summer", seasonName(0, "terrain\\sets\\3\\"));
    try testing.expectEqualStrings("Winter", seasonName(1, "terrain\\sets\\2\\"));
    try testing.expectEqualStrings("Africa", seasonName(2, ""));
    try testing.expectEqualStrings("?", seasonName(9, ""));
}

test "graph Check! reports a name with its author's drive in front and strips it on request" {
    const a = testing.allocator;
    var entries = [_]TestSource.Entry{
        .{ .name = "scenarios\\containers\\winter\\army_s", .season = 1, .folder = "terrain\\sets\\2\\", .ids = &.{}, .areas = &.{}, .size = 1 },
    };
    var table: TestSource = .{ .entries = &entries };
    var g: Graph = .{};
    defer g.deinit(a);
    _ = try g.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 16, .y2 = 16 });
    _ = try g.addNode(a, .{ .x1 = 32, .y1 = 0, .x2 = 48, .y2 = 16 });
    try setText(a, &g.nodes.items[0].container, "c:\\a7\\data\\Scenarios\\containers\\winter\\army_s");
    try setText(a, &g.nodes.items[1].container, "d:\\somewhere\\else");
    _ = try g.addLink(a, 0, 1);
    try setText(a, &g.links.items[0].desc, "c:\\a7\\data\\terrain\\sets\\2\\roads3d\\road_asphalt_ground");
    g.links.items[0].kind = 2;
    var report = try checkGraph(a, &g, table.source());
    defer report.deinit(a);
    var strips: usize = 0;
    var clears: usize = 0;
    var typed: usize = 0;
    for (report.findings.items) |finding| {
        switch (finding.fix) {
            .strip_node_name, .strip_link_desc => strips += 1,
            .clear_node_container => clears += 1,
            else => {},
        }
        if (std.mem.indexOf(u8, finding.text, "typed 2") != null) typed += 1;
    }
    try testing.expectEqual(@as(usize, 2), strips);
    try testing.expectEqual(@as(usize, 1), clears);
    try testing.expectEqual(@as(usize, 1), typed);
    _ = try fixAllGraph(a, &g, table.source(), &report);
    try testing.expectEqualStrings("Scenarios\\containers\\winter\\army_s", g.nodes.items[0].container);
    try testing.expectEqualStrings("terrain\\sets\\2\\roads3d\\road_asphalt_ground", g.links.items[0].desc);
    try testing.expectEqual(@as(usize, 0), g.nodes.items[1].container.len);
    try testing.expect(isStorageRelative("terrain\\x") and !isStorageRelative("c:\\x") and !isStorageRelative("\\x") and isStorageRelative(""));
    try testing.expect(storageSuffix("x\\terrain\\y", "terrain\\") != null and storageSuffix("xterrain\\y", "terrain\\") == null);
}

/// A FieldSource over small tables, for the field set Check! rules.
const TestFieldFacts = struct {
    counts: [4]?usize = .{ 10, 8, null, 6 },
    objects: []const []const u8 = &.{ "_Birch", "_Lime" },
    profiles: []const []const u8 = &.{"scenarios\\profiles\\profile"},

    fn tileCount(ctx: *anyopaque, slot: usize) ?usize {
        const self: *TestFieldFacts = @ptrCast(@alignCast(ctx));
        return self.counts[slot];
    }
    fn hasObject(ctx: *anyopaque, name: []const u8) bool {
        const self: *TestFieldFacts = @ptrCast(@alignCast(ctx));
        for (self.objects) |known| if (std.mem.eql(u8, known, name)) return true;
        return false;
    }
    fn hasProfile(ctx: *anyopaque, name: []const u8) bool {
        const self: *TestFieldFacts = @ptrCast(@alignCast(ctx));
        for (self.profiles) |known| if (std.ascii.eqlIgnoreCase(known, name)) return true;
        return false;
    }
    fn source(self: *TestFieldFacts) FieldSource {
        return .{ .ctx = self, .tile_count_fn = tileCount, .object_fn = hasObject, .profile_fn = hasProfile };
    }
};

test "a field set takes the three tabs' edits with the dialogs' own limits" {
    const a = testing.allocator;
    var f = try FieldSet.initNew(a);
    defer f.deinit(a);
    // File > New: the MFC's own defaults.
    try testing.expectEqualStrings("terrain\\sets\\1\\", f.season_folder);
    try testing.expectEqualStrings(default_profile, f.profile);
    try testing.expect(f.height == 2.0 and f.pattern_min == 3 and f.pattern_max == 5 and f.positive_ratio == 0.5);
    // Terrain: a shell is 2 wide, a tile joins once with weight 1, a weight edits, a tile and a shell go.
    const shell = try f.addTileShell(a);
    try testing.expect(f.tile_shells.items[shell].width == 2.0);
    try testing.expect(try f.addTile(a, shell, 4));
    try testing.expect(try f.addTile(a, shell, 7));
    try testing.expect(!(try f.addTile(a, shell, 4)));
    try testing.expect(!(try f.addTile(a, 9, 4)));
    try testing.expectEqual(@as(i32, 1), f.tile_shells.items[shell].tiles.items[0].weight);
    f.tile_shells.items[shell].tiles.items[1].weight = 9;
    f.removeTiles(shell, &.{0});
    try testing.expectEqual(@as(usize, 1), f.tileEntryCount());
    try testing.expectEqual(@as(i32, 7), f.tile_shells.items[shell].tiles.items[0].tile);
    // Objects: 2 wide, step 4, 30%; an object joins once; an object and a shell go.
    const oshell = try f.addObjectShell(a);
    try testing.expect(f.object_shells.items[oshell].width == 2.0 and f.object_shells.items[oshell].step == 4 and f.object_shells.items[oshell].ratio == 0.3);
    try testing.expect(try f.addObject(a, oshell, "_Birch"));
    try testing.expect(try f.addObject(a, oshell, "_Lime"));
    try testing.expect(!(try f.addObject(a, oshell, "_Birch")));
    f.removeObjects(a, oshell, &.{0});
    try testing.expectEqualStrings("_Lime", f.object_shells.items[oshell].objects.items[0].name);
    f.removeObjectShells(a, &.{oshell});
    f.removeTileShells(a, &.{shell});
    try testing.expectEqual(@as(usize, 0), f.tile_shells.items.len + f.object_shells.items.len);
    // Heights: pattern sizes 1..16 and min never passes max, height 0..5, percent 0..100.
    try testing.expect(f.setPatternMin(8) and f.pattern_min == 8 and f.pattern_max == 8);
    try testing.expect(f.setPatternMax(3) and f.pattern_max == 3 and f.pattern_min == 3);
    try testing.expect(!f.setPatternMin(0) and !f.setPatternMax(17) and f.pattern_min == 3);
    try testing.expect(f.setHeight(5.0) and !f.setHeight(5.1) and !f.setHeight(-1) and f.height == 5.0);
    try testing.expect(f.setPositivePercent(25) and f.positive_ratio == 0.25 and !f.setPositivePercent(101) and !f.setPositivePercent(std.math.nan(f32)));
    // The season combo sets the folder with the season.
    try f.setSeasonIndex(a, 3);
    try testing.expect(f.season == 0 and std.mem.eql(u8, f.season_folder, "terrain\\sets\\4\\") and f.seasonSlot() == 3);
    try testing.expectEqualStrings("Spring", seasonName(f.season, f.season_folder));
    try f.setSeasonIndex(a, 2);
    try testing.expect(f.season == 2 and f.seasonSlot() == 2);
}

test "a field set clones and compares by content" {
    const a = testing.allocator;
    var f = try FieldSet.initNew(a);
    defer f.deinit(a);
    const shell = try f.addTileShell(a);
    _ = try f.addTile(a, shell, 2);
    const oshell = try f.addObjectShell(a);
    _ = try f.addObject(a, oshell, "_Birch");
    var copy = try f.clone(a);
    defer copy.deinit(a);
    try testing.expect(f.eql(&copy));
    copy.object_shells.items[0].objects.items[0].weight = 5;
    try testing.expect(!f.eql(&copy));
    copy.object_shells.items[0].objects.items[0].weight = 1;
    try testing.expect(f.eql(&copy));
    copy.tile_shells.items[0].width = 3;
    try testing.expect(!f.eql(&copy));
}

test "field set Check! reports every range, tile, object and profile rule, and Fix all repairs them as one step without a silent removal" {
    const a = testing.allocator;
    var facts: TestFieldFacts = .{};
    var f = try FieldSet.initNew(a);
    defer f.deinit(a);
    // A clean set checks clean.
    {
        const shell = try f.addTileShell(a);
        _ = try f.addTile(a, shell, 3);
        const oshell = try f.addObjectShell(a);
        _ = try f.addObject(a, oshell, "_Birch");
        var report = try checkFieldSet(a, &f, facts.source());
        defer report.deinit(a);
        try testing.expectEqual(@as(usize, 0), report.findings.items.len);
    }
    // Now break it every way: a season, a height, a ratio, a profile, a pattern, a
    // shell width, a tile past the tileset, a negative tile weight, an object the
    // catalogue does not know, a negative object weight, a step, a probability.
    f.season = 7;
    f.height = 9;
    f.positive_ratio = 1.5;
    try f.setProfile(a, "scenarios\\profiles\\gone");
    f.pattern_min = 9;
    f.pattern_max = 2;
    f.tile_shells.items[0].width = 600;
    _ = try f.addTile(a, 0, 50);
    _ = try f.addTile(a, 0, 4);
    f.tile_shells.items[0].tiles.items[2].weight = -3;
    f.object_shells.items[0].step = 0;
    f.object_shells.items[0].ratio = 2;
    f.object_shells.items[0].width = -1;
    _ = try f.addObject(a, 0, "Ghost");
    _ = try f.addObject(a, 0, "_Lime");
    f.object_shells.items[0].objects.items[2].weight = -1;
    var report = try checkFieldSet(a, &f, facts.source());
    defer report.deinit(a);
    for (report.findings.items) |finding| {
        if (finding.fix == .none) continue;
        try testing.expect(finding.severity == .@"error");
    }
    const has = struct {
        fn tag(r: *const Report, want: std.meta.Tag(Fix)) bool {
            for (r.findings.items) |finding| if (std.meta.activeTag(finding.fix) == want) return true;
            return false;
        }
    };
    try testing.expect(has.tag(&report, .set_season_summer) and has.tag(&report, .clamp_height) and has.tag(&report, .clamp_ratio) and has.tag(&report, .reset_profile) and has.tag(&report, .fix_pattern));
    try testing.expect(has.tag(&report, .shell_width) and has.tag(&report, .remove_tile) and has.tag(&report, .zero_tile_weight));
    try testing.expect(has.tag(&report, .remove_object) and has.tag(&report, .zero_object_weight) and has.tag(&report, .object_step) and has.tag(&report, .object_ratio));
    // The tile past the tileset (50) is found; 4, inside the 10 types, is not.
    var off_range: usize = 0;
    for (report.findings.items) |finding| switch (finding.fix) {
        .remove_tile => |at| {
            off_range += 1;
            try testing.expectEqual(@as(usize, 1), at.index);
        },
        .remove_object => |at| try testing.expectEqual(@as(usize, 1), at.index),
        else => {},
    };
    try testing.expectEqual(@as(usize, 1), off_range);
    // Check! alone rewrote nothing.
    try testing.expectEqual(@as(usize, 3), f.tile_shells.items[0].tiles.items.len);
    try testing.expectEqual(@as(i32, 7), f.season);
    // Fix all: the removals go (highest first), the rest are repaired.
    const fixed = try fixAllField(a, &f, &report);
    try testing.expect(fixed >= 10);
    try testing.expectEqual(@as(usize, 2), f.tile_shells.items[0].tiles.items.len);
    try testing.expectEqual(@as(usize, 2), f.object_shells.items[0].objects.items.len);
    try testing.expectEqual(@as(i32, 0), f.tile_shells.items[0].tiles.items[1].weight);
    try testing.expectEqual(@as(i32, 0), f.object_shells.items[0].objects.items[1].weight);
    var again = try checkFieldSet(a, &f, facts.source());
    defer again.deinit(a);
    try testing.expectEqual(@as(usize, 0), again.findings.items.len);
}

test "field set Check! says when a tileset does not load and warns about empty shells" {
    const a = testing.allocator;
    var facts: TestFieldFacts = .{};
    var f = try FieldSet.initNew(a);
    defer f.deinit(a);
    try f.setSeasonIndex(a, 2); // africa: the table's tileset is null
    _ = try f.addTileShell(a);
    _ = try f.addTile(a, 0, 400);
    _ = try f.addTileShell(a); // empty: a warning
    _ = try f.addObjectShell(a); // empty: a deliberate gap, no warning
    var report = try checkFieldSet(a, &f, facts.source());
    defer report.deinit(a);
    var warnings: usize = 0;
    var tile_errors: usize = 0;
    for (report.findings.items) |finding| {
        if (finding.severity == .warning) warnings += 1;
        if (std.meta.activeTag(finding.fix) == .remove_tile) tile_errors += 1;
    }
    // The tileset warning and the empty terrain shell; no tile can be called off range.
    try testing.expectEqual(@as(usize, 2), warnings);
    try testing.expectEqual(@as(usize, 0), tile_errors);
    // A set with no shells at all paints nothing.
    var empty = try FieldSet.initNew(a);
    defer empty.deinit(a);
    var empty_report = try checkFieldSet(a, &empty, facts.source());
    defer empty_report.deinit(a);
    try testing.expectEqual(@as(usize, 1), empty_report.findings.items.len);
}

test "a field set document keeps its own undo and shipped state" {
    const a = testing.allocator;
    var doc = FieldSetDoc.init(a);
    defer doc.deinit();
    try doc.load("scenarios\\fieldsets\\summer\\field00", try FieldSet.initNew(a), true);
    try testing.expect(doc.shipped and !doc.dirty and !doc.canUndo());
    const field = try doc.begin();
    _ = try field.addTileShell(a);
    try testing.expect(doc.dirty and doc.canUndo());
    try testing.expect(try doc.undo());
    try testing.expectEqual(@as(usize, 0), doc.current.tile_shells.items.len);
    try testing.expect(try doc.redo());
    try testing.expectEqual(@as(usize, 1), doc.current.tile_shells.items.len);
    try doc.markSaved("scenarios\\fieldsets\\user\\mine");
    try testing.expect(!doc.shipped and !doc.dirty);
}
