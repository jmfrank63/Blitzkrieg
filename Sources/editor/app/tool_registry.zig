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

pub const ToolId = enum { select, brush, place, roads_rivers, bridge, fence, entrenchment, script_areas, start_target, reserve_positions, ai_general, heights };

pub const Entry = struct {
    id: ToolId,
    /// The Tools menu and the tool palette.
    label: [:0]const u8,
    /// A bare digit key ('1'..'9'), never with a modifier: phase 5 publishes
    /// Ctrl+U, Ctrl+N, Ctrl+Shift+X, Ctrl+Shift+B and Ctrl+click, and the
    /// camera and edit keys (W A S D Q E Z Y, Delete, Home, the arrows) are
    /// taken. Null for a tool with no key.
    shortcut: ?u8 = null,
    /// Not listed in the Tools menu or the tool palette. No tool uses it now: every
    /// tool is visible (04-12 - a user must see every tool; 04-11 had hidden the
    /// Start Target and Reserve Positions buttons only to keep the M1 reference
    /// frame under its threshold, and the reference was refreshed instead). A
    /// hidden tool would have no shortcut.
    hidden: bool = false,
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
    // 04-08 (D-13): the MFC trench builder. A click adds a point, a right
    // click clears the polyline (Ctrl+click stands in for it on a one-button
    // trackpad), a double click commits it; a click on a piece selects its
    // entrenchment. Its markers are the preview and the hovered and selected
    // entrenchments' outlines.
    .{
        .id = .entrenchment,
        .label = "Entrenchment",
        .shortcut = '7',
        .needs_right_button = true,
        .ctrl_click_is_right = true,
        .needs_double_click = true,
        .marker_kinds = marker_logic.MarkerSet.only(&.{.selection_outline}),
    },
    // 04-10 (D-21): the MFC area tab. A drag draws a rectangle or a circle, a click
    // inside an area selects it, a drag from the selected area's centre handle moves
    // it and from its corner or edge handle resizes it; Delete removes it. All
    // left button and keys. Its markers are the areas themselves, whatever View ->
    // Markers says, so the tool never edits what it cannot see.
    .{
        .id = .script_areas,
        .label = "Script Areas",
        .shortcut = '8',
        .marker_kinds = marker_logic.MarkerSet.only(&.{.script_areas}),
    },
    // 04-11 (D-17): the start command's target click, entered from the Start
    // Commands window's "Set target" button, left after one click. No shortcut (a
    // digit would switch to it with no command chosen); left button only. In the
    // palette like every tool; with no command chosen a click only says so. Its
    // markers are the red lines from the units to their targets.
    .{
        .id = .start_target,
        .label = "Start Target",
        .marker_kinds = marker_logic.MarkerSet.only(&.{.start_commands}),
    },
    // 04-11 (D-18): the MFC editor's artillery positions mode, also entered from Unit >
    // Artillery positions mode. Clicks pick the gun, the truck and the place, Enter
    // commits, Escape clears, Delete removes the listed position - left button and
    // keys. No digit shortcut. Its markers are the gun-truck-place lines.
    .{
        .id = .reserve_positions,
        .label = "Reserve Positions",
        .marker_kinds = marker_logic.MarkerSet.only(&.{.reserve_positions}),
    },
    // 04-12 (D-19): the MFC AI general tab. A click on open ground makes a defence
    // parcel, a click inside one a reinforce point; drags move and size them; Enter,
    // Insert and Space switch the type, Delete removes. Left button and keys. Its
    // markers are the parcels, whatever View -> Markers says, so the tool never edits
    // what it cannot see.
    .{
        .id = .ai_general,
        .label = "AI General",
        .shortcut = '9',
        .marker_kinds = marker_logic.MarkerSet.only(&.{.parcels}),
    },
    // M3, D-18: the MFC terrain tab's Heights tool. Left-drag raises,
    // right-drag lowers, middle-drag - or left and right held together, or
    // Alt+drag which the view maps to the middle button - levels toward the
    // level mode's target. The right button is its own gesture; Ctrl is the
    // invalid-height override, never a right click. No digit left (1..9 are
    // taken), like the other mode-entered tools. Its marker is the brush
    // outline the view draws for it (PARITY TR9).
    .{
        .id = .heights,
        .label = "Heights",
        .needs_right_button = true,
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
    try std.testing.expectEqual(@as(?ToolId, .ai_general), byShortcut('9'));
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

test "the Entrenchment tool is key 7: right button, Ctrl-as-right and double click, with the selection outline" {
    const item = entry(.entrenchment);
    try std.testing.expectEqual(@as(?u8, '7'), item.shortcut);
    try std.testing.expect(item.needs_right_button and item.ctrl_click_is_right and item.needs_double_click);
    try std.testing.expect(item.marker_kinds.has(.selection_outline));
    try std.testing.expectEqual(@as(?ToolId, .entrenchment), byShortcut('7'));
    try std.testing.expectEqual(@as(?ToolId, .entrenchment), byLabel("entrenchment"));
}

test "the Script Areas tool is key 8: left button and keys only, with the areas as its markers" {
    const item = entry(.script_areas);
    try std.testing.expectEqual(@as(?u8, '8'), item.shortcut);
    try std.testing.expect(!item.needs_right_button and !item.ctrl_click_is_right and !item.needs_double_click);
    try std.testing.expect(item.marker_kinds.has(.script_areas));
    try std.testing.expectEqual(@as(?ToolId, .script_areas), byShortcut('8'));
    try std.testing.expectEqual(@as(?ToolId, .script_areas), byLabel("script_areas"));
}

test "the AI General tool is key 9: left button and keys only, with the parcels as its markers" {
    const item = entry(.ai_general);
    try std.testing.expectEqual(@as(?u8, '9'), item.shortcut);
    try std.testing.expect(!item.needs_right_button and !item.ctrl_click_is_right and !item.needs_double_click);
    try std.testing.expect(item.marker_kinds.has(.parcels));
    try std.testing.expect(!item.hidden);
    try std.testing.expectEqual(@as(?ToolId, .ai_general), byShortcut('9'));
    try std.testing.expectEqual(@as(?ToolId, .ai_general), byLabel("ai_general"));
}

test "the Start Target tool has no key, takes the left button only, and shows the start commands' lines" {
    const item = entry(.start_target);
    try std.testing.expectEqual(@as(?u8, null), item.shortcut);
    try std.testing.expect(!item.hidden); // in the palette: a user sees every tool
    try std.testing.expect(!item.needs_right_button and !item.ctrl_click_is_right and !item.needs_double_click);
    try std.testing.expect(item.marker_kinds.has(.start_commands));
    try std.testing.expectEqual(@as(?ToolId, .start_target), byLabel("start_target"));
}

test "the Reserve Positions tool has no key, takes the left button and keys, and shows the reserve lines" {
    const item = entry(.reserve_positions);
    try std.testing.expectEqual(@as(?u8, null), item.shortcut);
    try std.testing.expect(!item.hidden);
    try std.testing.expect(!item.needs_right_button and !item.ctrl_click_is_right and !item.needs_double_click);
    try std.testing.expect(item.marker_kinds.has(.reserve_positions));
    try std.testing.expectEqual(@as(?ToolId, .reserve_positions), byLabel("reserve_positions"));
}

test "the M1 tools take no right button, double click or Ctrl-as-right, and carry no markers" {
    for (entries[0..3]) |item| {
        try std.testing.expect(!item.needs_right_button);
        try std.testing.expect(!item.ctrl_click_is_right);
        try std.testing.expect(!item.needs_double_click);
        try std.testing.expect(item.marker_kinds.kinds.count() == 0);
    }
}

test "every tool is in the palette: none is hidden, and a hidden one would have no shortcut" {
    for (entries) |item| {
        try std.testing.expect(!item.hidden);
        if (item.hidden) try std.testing.expectEqual(@as(?u8, null), item.shortcut);
    }
}

test "the Heights tool takes the right button, never Ctrl-as-right, with no key" {
    const item = entry(.heights);
    try std.testing.expectEqual(@as(?u8, null), item.shortcut);
    try std.testing.expect(item.needs_right_button);
    // Ctrl is the MFC's own invalid-height override there (DrawShadeState.cpp:261),
    // so Ctrl+left must arrive as a left press with the modifier, not as the right button.
    try std.testing.expect(!item.ctrl_click_is_right);
    try std.testing.expect(!item.needs_double_click);
    try std.testing.expectEqual(@as(?ToolId, .heights), byLabel("heights"));
    try std.testing.expect(!item.hidden);
}

test "a tool label is a valid tool= word" {
    inline for (std.meta.fields(ToolId)) |field| {
        try std.testing.expect(field.name.len >= 1 and field.name.len <= 32);
        for (field.name) |ch| try std.testing.expect((ch >= 'a' and ch <= 'z') or ch == '_');
    }
}
