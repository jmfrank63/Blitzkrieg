//! The tool registry (04-03, C13): replaces the closed `Tool` enum in view.zig.
//! Each tool has a label, a shortcut and the input it needs, so view.zig's
//! routing asks the registry instead of naming tools: which key switches to a
//! tool, whether a tool receives the right button, whether Ctrl+left counts as
//! the right button for it (macOS has no second button on a trackpad), whether
//! it wants a double click, and which marker kinds stay on while it is active.
//!
//! Later plans add their `ToolId` (roads_rivers, bridge, fence, entrenchment,
//! script_areas, ai_general, reserve_positions, start_target) and one entry
//! each - nothing else in the routing changes.
//!
//! Pure Zig, imported from panels_logic.zig so its tests run under
//! `zig build test-map-editor-panels` (Pitfall 18).
const std = @import("std");
const marker_logic = @import("marker_logic.zig");

pub const ToolId = enum { select, brush, place, roads_rivers, bridge, fence };

pub const Entry = struct {
    id: ToolId,
    /// The Tools menu and the tool palette.
    label: [:0]const u8,
    /// A bare digit key ('1'..'9'), never with a modifier: phase 5 publishes
    /// Ctrl+U, Ctrl+N, Ctrl+Shift+X, Ctrl+Shift+B and Ctrl+click, and the
    /// camera and edit keys (W A S D Q E Z Y, Delete, Home, the arrows) are
    /// taken. Null for a tool with no key.
    shortcut: ?u8 = null,
    /// The tool receives right_press/right_drag/right_release.
    needs_right_button: bool = false,
    /// Ctrl+left is sent as the right button, in this tool only.
    ctrl_click_is_right: bool = false,
    /// The tool receives double_click after the single click's own events.
    needs_double_click: bool = false,
    /// Marker kinds drawn while this tool is active, whatever View ->
    /// Markers says.
    marker_kinds: marker_logic.MarkerSet = marker_logic.MarkerSet.none(),
};

/// In `ToolId` order (checked below).
pub const entries = [_]Entry{
    .{ .id = .select, .label = "Select", .shortcut = '1' },
    .{ .id = .brush, .label = "Brush", .shortcut = '2' },
    .{ .id = .place, .label = "Place", .shortcut = '3' },
    // 04-05 (D-08): the MFC Roads and Rivers tabs as one tool. Right click
    // takes a point back and right-drag sets opacity; Ctrl+click stands in
    // for it on a one-button trackpad; a double click finishes a line.
    .{
        .id = .roads_rivers,
        .label = "Roads & Rivers",
        .shortcut = '4',
        .needs_right_button = true,
        .ctrl_click_is_right = true,
        .needs_double_click = true,
        .marker_kinds = marker_logic.MarkerSet.only(&.{.roads_rivers}),
    },
    // 04-06 (D-10..D-12): the MFC Bridges tab. A drag draws, a click on a span
    // selects its bridge; Q/E rotate, Enter toggles built during play, Delete
    // removes it - all left-button and keys, so no right button or double
    // click. Its markers are the ghost and the selected bridge's outline.
    .{
        .id = .bridge,
        .label = "Bridge",
        .shortcut = '5',
        .marker_kinds = marker_logic.MarkerSet.only(&.{.selection_outline}),
    },
    // 04-07 (D-14): the MFC Fences tab. A drag places a run of fences, a click
    // one fence; Ctrl is a modifier there (it flips a single fence), never a
    // right click (C13), so the flag stays off. It draws only its ghost, so
    // it asks for no marker kind.
    .{
        .id = .fence,
        .label = "Fence",
        .shortcut = '6',
    },
};

comptime {
    for (entries, 0..) |item, index| {
        if (@intFromEnum(item.id) != index) @compileError("tool_registry.entries must follow ToolId's order");
    }
    if (entries.len != std.meta.fields(ToolId).len) @compileError("every ToolId needs one registry entry");
}

pub fn entry(id: ToolId) *const Entry {
    return &entries[@intFromEnum(id)];
}

/// The tool a bare key switches to: the key as its character, '1'..'9'.
pub fn byShortcut(key: u32) ?ToolId {
    for (&entries) |*item| {
        if (item.shortcut) |shortcut| {
            if (key == shortcut) return item.id;
        }
    }
    return null;
}

/// The tool `tool=<label>` names: the lower-case identifier of the tool
/// (`[a-z_]{1,32}`, the ToolId's own name). Null for an unknown label.
pub fn byLabel(text: []const u8) ?ToolId {
    inline for (std.meta.fields(ToolId)) |field| {
        if (std.mem.eql(u8, text, field.name)) return @enumFromInt(field.value);
    }
    return null;
}

/// The shortcut as menu text ("1"); empty for a tool with none.
pub fn shortcutText(item: *const Entry, buffer: *[2:0]u8) [:0]const u8 {
    const key = item.shortcut orelse return "";
    buffer[0] = key;
    buffer[1] = 0;
    return buffer[0..1 :0];
}

test "every tool has exactly one entry, found by id, label name and shortcut" {
    inline for (std.meta.fields(ToolId)) |field| {
        const id: ToolId = @enumFromInt(field.value);
        try std.testing.expectEqual(id, entry(id).id);
        try std.testing.expectEqual(@as(?ToolId, id), byLabel(field.name));
    }
    try std.testing.expectEqual(@as(?ToolId, .select), byShortcut('1'));
    try std.testing.expectEqual(@as(?ToolId, .brush), byShortcut('2'));
    try std.testing.expectEqual(@as(?ToolId, .place), byShortcut('3'));
    try std.testing.expectEqual(@as(?ToolId, null), byShortcut('9'));
    try std.testing.expectEqual(@as(?ToolId, null), byLabel("Select"));
    try std.testing.expectEqual(@as(?ToolId, null), byLabel(""));
    try std.testing.expectEqual(@as(?ToolId, null), byLabel("frobnicate"));
}

test "registry shortcuts are unique bare digits, and outside the M1 three none is a taken key" {
    for (entries, 0..) |item, index| {
        const shortcut = item.shortcut orelse continue;
        // A bare digit: never a letter (Ctrl+U, Ctrl+N, W A S D Q E Z Y are
        // spoken for) and never a modifier chord.
        try std.testing.expect(shortcut >= '1' and shortcut <= '9');
        for (entries[index + 1 ..]) |other| {
            if (other.shortcut) |other_shortcut| try std.testing.expect(shortcut != other_shortcut);
        }
        // M1's own keys stay where they were.
        const m1 = item.id == .select or item.id == .brush or item.id == .place;
        if (!m1) try std.testing.expect(shortcut >= '4');
    }
    // The three M1 tools keep 1, 2, 3 exactly.
    try std.testing.expectEqual(@as(?u8, '1'), entry(.select).shortcut);
    try std.testing.expectEqual(@as(?u8, '2'), entry(.brush).shortcut);
    try std.testing.expectEqual(@as(?u8, '3'), entry(.place).shortcut);
}

test "no registry shortcut is a letter or a key another part of the editor owns" {
    const taken = "QEWASDZYqewasdzy";
    for (entries) |item| {
        const shortcut = item.shortcut orelse continue;
        try std.testing.expect(std.mem.indexOfScalar(u8, taken, shortcut) == null);
    }
}

test "the Roads & Rivers tool takes the right button, Ctrl-as-right and double click, with its markers" {
    const item = entry(.roads_rivers);
    try std.testing.expectEqual(@as(?u8, '4'), item.shortcut);
    try std.testing.expect(item.needs_right_button and item.ctrl_click_is_right and item.needs_double_click);
    try std.testing.expect(item.marker_kinds.has(.roads_rivers));
    try std.testing.expectEqual(@as(?ToolId, .roads_rivers), byShortcut('4'));
    try std.testing.expectEqual(@as(?ToolId, .roads_rivers), byLabel("roads_rivers"));
}

test "the Bridge tool is key 5, left button and keys only, with the selection outline" {
    const item = entry(.bridge);
    try std.testing.expectEqual(@as(?u8, '5'), item.shortcut);
    try std.testing.expect(!item.needs_right_button and !item.ctrl_click_is_right and !item.needs_double_click);
    try std.testing.expect(item.marker_kinds.has(.selection_outline));
    try std.testing.expectEqual(@as(?ToolId, .bridge), byShortcut('5'));
    try std.testing.expectEqual(@as(?ToolId, .bridge), byLabel("bridge"));
}

test "the Fence tool is key 6, left button only, and Ctrl is never a right click there" {
    const item = entry(.fence);
    try std.testing.expectEqual(@as(?u8, '6'), item.shortcut);
    try std.testing.expect(!item.needs_right_button and !item.ctrl_click_is_right and !item.needs_double_click);
    try std.testing.expectEqual(@as(?ToolId, .fence), byShortcut('6'));
    try std.testing.expectEqual(@as(?ToolId, .fence), byLabel("fence"));
}

test "the M1 tools take no right button, double click or Ctrl-as-right, and carry no markers" {
    for (entries[0..3]) |item| {
        try std.testing.expect(!item.needs_right_button);
        try std.testing.expect(!item.ctrl_click_is_right);
        try std.testing.expect(!item.needs_double_click);
        try std.testing.expect(item.marker_kinds.kinds.count() == 0);
    }
}

test "a tool label is a valid tool= word" {
    inline for (std.meta.fields(ToolId)) |field| {
        try std.testing.expect(field.name.len >= 1 and field.name.len <= 32);
        for (field.name) |ch| try std.testing.expect((ch >= 'a' and ch <= 'z') or ch == '_');
    }
}
