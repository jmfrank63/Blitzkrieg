//! The Layers menu's model (M3, D-32): which layers exist, whether each is
//! shown, and the fire-range mode - the state the editor remembers across
//! opens, new maps and restarts, and re-applies to the renderer after every
//! open (the MFC editor's check marks and the scene's own flags drifted apart
//! across an open; here the one list is the truth and the renderer is told).
//!
//! Std-only and map-free: a layer is renderer state, never map data, so
//! nothing here touches the document or the history. The numbers are the
//! bridge's `BkEditorLayer` values (bridge.h) and never change - the C bridge
//! passes them straight through.
const std = @import("std");

/// One entry of the Layers menu, in the bridge's own order. `fire_ranges` is
/// not a toggle but a mode (see `FireMode`); its bit says whether any range
/// is shown.
pub const Layer = enum(u32) {
    terrain,
    grid,
    wireframe,
    depth_complexity,
    terrain_noise,
    black_stripes,
    units,
    objects,
    bounding_boxes,
    shadows,
    haze,
    war_fog,
    units_passability,
    fire_ranges,
};

pub const layer_count = @typeInfo(Layer).@"enum".field_names.len;

/// Every layer's bit: the mask of a renderer that can draw them all.
pub const all_bits: u32 = (1 << layer_count) - 1;

pub fn bit(layer: Layer) u32 {
    return @as(u32, 1) << @as(u5, @intCast(@intFromEnum(layer)));
}

/// The Layers menu's entries, MFC wording (TemplateEditorFrame1's menu
/// resource): "Hase" in the MFC resource is a typo for Haze, which the
/// editor spells right.
pub fn label(layer: Layer) [:0]const u8 {
    return switch (layer) {
        .terrain => "Terrain",
        .grid => "Grid",
        .wireframe => "Wire Frame",
        .depth_complexity => "Depth Complexity",
        .terrain_noise => "Terrain Noise",
        .black_stripes => "Black Stripes",
        .units => "Units",
        .objects => "Objects",
        .bounding_boxes => "Bounding Boxes",
        .shadows => "Shadows",
        .haze => "Haze",
        .war_fog => "War Fog",
        .units_passability => "Units Passability",
        .fire_ranges => "Unit Fire Ranges",
    };
}

/// The name a command and a scenario use (`layer_toggle=<name>`): lowercase,
/// underscores - the enum's own tag.
pub fn commandName(layer: Layer) []const u8 {
    return @tagName(layer);
}

pub fn fromCommandName(name: []const u8) ?Layer {
    inline for (std.enums.values(Layer)) |tag| {
        if (std.mem.eql(u8, name, @tagName(tag))) return tag;
    }
    return null;
}

/// The toggles a layer can be, which is every one but the fire ranges.
pub fn isToggle(layer: Layer) bool {
    return layer != .fire_ranges;
}

/// What the MFC editor's menu starts with (TemplateEditorFrame1.cpp:379, and
/// ClearAllDataBeforeNewMap resetting the grid, noise and war fog): terrain,
/// noise, black stripes, units, objects, shadows and haze shown; the grid,
/// wire frame, depth complexity, bounding boxes, war fog, passability and the
/// fire ranges hidden. The fire-range bit is never part of the persisted bits
/// (it is derived from the mode).
pub const default_bits: u32 = bit(.terrain) | bit(.terrain_noise) | bit(.black_stripes) | bit(.units) |
    bit(.objects) | bit(.shadows) | bit(.haze);

/// Unit Fire Ranges' three modes (the MFC's combo: off through the toolbar
/// button, the "selected units" entry, any filter's name).
pub const FireMode = enum(u32) { off, selected, filter };

pub const max_filter_len = 63;

pub const State = struct {
    /// One bit per `Layer`, the toggles only (the fire-range bit is derived).
    bits: u32 = default_bits,
    fire_mode: FireMode = .off,
    fire_filter_buffer: [max_filter_len]u8 = undefined,
    fire_filter_len: usize = 0,

    pub fn shown(self: *const State, layer: Layer) bool {
        if (layer == .fire_ranges) return self.fire_mode != .off;
        return self.bits & bit(layer) != 0;
    }

    /// Sets a toggle's bit. The fire ranges are not one: `setFireRange`.
    pub fn set(self: *State, layer: Layer, on: bool) void {
        if (!isToggle(layer)) return;
        if (on) self.bits |= bit(layer) else self.bits &= ~bit(layer);
    }

    pub fn fireFilter(self: *const State) []const u8 {
        return self.fire_filter_buffer[0..self.fire_filter_len];
    }

    /// The mode and, for `.filter`, the filter's name (truncated to the
    /// buffer; a filter's name is at most 63 characters by the composer's own
    /// rule). A non-filter mode forgets the name, so a stale one never shows.
    pub fn setFireRange(self: *State, mode: FireMode, filter: []const u8) void {
        self.fire_mode = mode;
        if (mode == .filter) {
            self.fire_filter_len = @min(filter.len, max_filter_len);
            @memcpy(self.fire_filter_buffer[0..self.fire_filter_len], filter[0..self.fire_filter_len]);
        } else {
            self.fire_filter_len = 0;
        }
    }

    /// The bits the bridge is told: what `bits` holds, limited to the layers
    /// `mask` says the renderer can draw (a layer it cannot draw keeps the
    /// renderer's own state, whatever was remembered for it).
    pub fn bitsFor(self: *const State, mask: u32) u32 {
        return self.bits & mask & ~bit(.fire_ranges);
    }

    /// An FNV-1a over the whole state, for "did anything change since I last
    /// sent it".
    pub fn hash(self: *const State) u64 {
        var h: u64 = 0xcbf29ce484222325;
        const mix = struct {
            fn byte(h_in: u64, b: u8) u64 {
                return (h_in ^ b) *% 0x100000001b3;
            }
        };
        var word: u32 = self.bits;
        for (0..4) |_| {
            h = mix.byte(h, @truncate(word));
            word >>= 8;
        }
        h = mix.byte(h, @intCast(@intFromEnum(self.fire_mode)));
        for (self.fireFilter()) |b| h = mix.byte(h, b);
        return h;
    }
};

test "the layers are the bridge's fourteen, in its order" {
    try std.testing.expectEqual(@as(usize, 14), layer_count);
    try std.testing.expectEqual(@as(u32, 0), @intFromEnum(Layer.terrain));
    try std.testing.expectEqual(@as(u32, 2), @intFromEnum(Layer.wireframe));
    try std.testing.expectEqual(@as(u32, 3), @intFromEnum(Layer.depth_complexity));
    try std.testing.expectEqual(@as(u32, 13), @intFromEnum(Layer.fire_ranges));
    try std.testing.expectEqual(@as(u32, 0x3fff), all_bits);
}

test "the defaults are the MFC editor's own starting menu" {
    var state: State = .{};
    for ([_]Layer{ .terrain, .terrain_noise, .black_stripes, .units, .objects, .shadows, .haze }) |layer| {
        try std.testing.expect(state.shown(layer));
    }
    for ([_]Layer{ .grid, .wireframe, .depth_complexity, .bounding_boxes, .war_fog, .units_passability, .fire_ranges }) |layer| {
        try std.testing.expect(!state.shown(layer));
    }
    try std.testing.expectEqual(FireMode.off, state.fire_mode);
    try std.testing.expectEqual(@as(u32, 0), default_bits & bit(.fire_ranges));
    _ = &state;
}

test "set flips one bit and leaves the rest" {
    var state: State = .{};
    state.set(.grid, true);
    try std.testing.expect(state.shown(.grid));
    try std.testing.expectEqual(default_bits | bit(.grid), state.bits);
    state.set(.terrain, false);
    try std.testing.expect(!state.shown(.terrain));
    try std.testing.expect(state.shown(.haze));
    // The fire ranges are a mode: set leaves them alone.
    state.set(.fire_ranges, true);
    try std.testing.expect(!state.shown(.fire_ranges));
    try std.testing.expectEqual(@as(u32, 0), state.bits & bit(.fire_ranges));
}

test "the fire ranges read as shown exactly while the mode is not off" {
    var state: State = .{};
    state.setFireRange(.selected, "ignored");
    try std.testing.expect(state.shown(.fire_ranges));
    try std.testing.expectEqualStrings("", state.fireFilter());
    state.setFireRange(.filter, "Axis Units");
    try std.testing.expect(state.shown(.fire_ranges));
    try std.testing.expectEqualStrings("Axis Units", state.fireFilter());
    state.setFireRange(.off, "Axis Units");
    try std.testing.expect(!state.shown(.fire_ranges));
    try std.testing.expectEqualStrings("", state.fireFilter());
    // A name past the buffer is cut, never overruns.
    var long: [100]u8 = undefined;
    @memset(&long, 'x');
    state.setFireRange(.filter, &long);
    try std.testing.expectEqual(@as(usize, max_filter_len), state.fireFilter().len);
}

test "a mask hides the layers a renderer cannot draw from what is sent" {
    var state: State = .{};
    state.set(.depth_complexity, true);
    state.set(.grid, true);
    const mask = all_bits & ~bit(.depth_complexity);
    const sent = state.bitsFor(mask);
    try std.testing.expect(sent & bit(.depth_complexity) == 0);
    try std.testing.expect(sent & bit(.grid) != 0);
    try std.testing.expect(sent & bit(.fire_ranges) == 0);
}

test "command names round trip and are the enum's tags" {
    for (std.enums.values(Layer)) |layer| {
        try std.testing.expectEqual(layer, fromCommandName(commandName(layer)).?);
    }
    try std.testing.expect(fromCommandName("nonsense") == null);
    try std.testing.expect(fromCommandName("") == null);
    try std.testing.expectEqualStrings("units_passability", commandName(.units_passability));
}

test "the hash moves with every part of the state" {
    var state: State = .{};
    const base = state.hash();
    state.set(.grid, true);
    const with_grid = state.hash();
    try std.testing.expect(base != with_grid);
    state.setFireRange(.selected, "");
    const selected = state.hash();
    try std.testing.expect(selected != with_grid);
    state.setFireRange(.filter, "A");
    const filter_a = state.hash();
    try std.testing.expect(filter_a != selected);
    state.setFireRange(.filter, "B");
    try std.testing.expect(state.hash() != filter_a);
}
