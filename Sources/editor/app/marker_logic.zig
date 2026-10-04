//! The pure half of the M2 marker layer (D-06): which kinds of marker exist,
//! which are switched on (View -> Markers), when a kind is forced on by the
//! active tool, the AI-unit to world-unit conversion the markers of AI-unit
//! records need, and the per-kind caps that keep a bad map from making a frame
//! draw thousands of shapes (T-04-03-02). markers.zig does the drawing.
//!
//! No ImGui, no SDL, no engine: imported by panels_logic.zig so its tests run
//! under `zig build test-map-editor-panels` (Pitfall 18).
const std = @import("std");

pub const MarkerKind = enum {
    camera_anchors,
    roads_rivers,
    script_areas,
    parcels,
    start_commands,
    reserve_positions,
    groups,
    selection_outline,

    /// The View -> Markers entry.
    pub fn label(self: MarkerKind) [:0]const u8 {
        return switch (self) {
            .camera_anchors => "Camera anchors",
            .roads_rivers => "Roads and rivers",
            .script_areas => "Script areas",
            .parcels => "AI parcels",
            .start_commands => "Start commands",
            .reserve_positions => "Reserve positions",
            .groups => "Reinforcement groups",
            .selection_outline => "Selection outline",
        };
    }
};

/// A set of marker kinds. The default value has every kind on, which is what
/// View -> Markers starts as; `none()` is what a tool with no markers of its
/// own carries.
pub const MarkerSet = struct {
    kinds: std.EnumSet(MarkerKind) = std.EnumSet(MarkerKind).initFull(),

    pub fn all() MarkerSet {
        return .{};
    }

    pub fn none() MarkerSet {
        return .{ .kinds = std.EnumSet(MarkerKind).initEmpty() };
    }

    pub fn only(list: []const MarkerKind) MarkerSet {
        var set = none();
        for (list) |kind| set.kinds.insert(kind);
        return set;
    }

    pub fn has(self: MarkerSet, kind: MarkerKind) bool {
        return self.kinds.contains(kind);
    }

    pub fn setKind(self: *MarkerSet, kind: MarkerKind, on: bool) void {
        if (on) self.kinds.insert(kind) else self.kinds.remove(kind);
    }
};

/// Whether `kind` is drawn: switched on in View -> Markers, or one of the
/// active tool's own kinds (those stay on so a tool never edits what it
/// cannot see).
pub fn visible(set: MarkerSet, kind: MarkerKind, active_tool_kinds: MarkerSet) bool {
    return set.has(kind) or active_tool_kinds.has(kind);
}

/// The most items of a kind one frame draws. Real maps hold a handful of each;
/// the cap only bounds a malformed file's cost.
pub fn cap(kind: MarkerKind) usize {
    return switch (kind) {
        .camera_anchors => 33, // 32 players plus the neutral one
        .roads_rivers => 256,
        .script_areas => 256,
        .parcels => 128,
        .start_commands => 512,
        .reserve_positions => 256,
        .groups => 256,
        .selection_outline => 256, // the selector's double circles, one pair per selected object (M3, D-25)
    };
}

/// fAITileXCoeff (Sources/src/Formats/fmtTerrain.h:7): `32.0f * FP_SQRT_2 /
/// 64.0f`. AI units times this are world (Vis) units, world units divided by
/// it are AI units (`Vis2AIFast`). Both conversions here are the unrounded
/// ones; the MFC editor's truncating `Vis2AI` (`int( x + 0.3f )`) belongs to
/// storing a script area (Pitfall 14), not to drawing.
pub const ai_tile_coeff: f32 = 32.0 * @as(f32, std.math.sqrt2) / 64.0;

pub const Vec2 = struct { x: f32, y: f32 };

pub fn aiToWorld(v: Vec2) Vec2 {
    return .{ .x = v.x * ai_tile_coeff, .y = v.y * ai_tile_coeff };
}

pub fn worldToAi(v: Vec2) Vec2 {
    return .{ .x = v.x / ai_tile_coeff, .y = v.y / ai_tile_coeff };
}

test "the default marker set has every kind on and none() has none" {
    const all_on = MarkerSet.all();
    const off = MarkerSet.none();
    inline for (std.meta.fields(MarkerKind)) |field| {
        const kind: MarkerKind = @enumFromInt(field.value);
        try std.testing.expect(all_on.has(kind));
        try std.testing.expect(!off.has(kind));
    }
}

test "a kind the active tool owns is visible even when switched off" {
    var set = MarkerSet.all();
    set.setKind(.roads_rivers, false);
    const active = MarkerSet.only(&.{.roads_rivers});
    try std.testing.expect(visible(set, .roads_rivers, active));
    try std.testing.expect(!visible(set, .roads_rivers, MarkerSet.none()));
    try std.testing.expect(visible(set, .camera_anchors, MarkerSet.none()));
    set.setKind(.camera_anchors, false);
    try std.testing.expect(!visible(set, .camera_anchors, active));
}

test "AI to world uses fAITileXCoeff and the two conversions invert each other" {
    try std.testing.expectApproxEqAbs(@as(f32, 0.70710678), ai_tile_coeff, 1e-6);
    const world = aiToWorld(.{ .x = 100, .y = 64 });
    try std.testing.expectApproxEqAbs(@as(f32, 70.710678), world.x, 1e-3);
    try std.testing.expectApproxEqAbs(@as(f32, 45.254834), world.y, 1e-3);
    const back = worldToAi(world);
    try std.testing.expectApproxEqAbs(@as(f32, 100), back.x, 1e-3);
    try std.testing.expectApproxEqAbs(@as(f32, 64), back.y, 1e-3);
}

test "every kind has a non-empty label and a positive cap" {
    inline for (std.meta.fields(MarkerKind)) |field| {
        const kind: MarkerKind = @enumFromInt(field.value);
        try std.testing.expect(kind.label().len != 0);
        try std.testing.expect(cap(kind) > 0);
    }
}
