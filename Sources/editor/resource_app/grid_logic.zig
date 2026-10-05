//! The grid sub-editors' view and tools with no window and no ImGui: the
//! mapping between the overlay's pixels and the AI tile grid, the tool modes of
//! GridFrm's toolbar (move, draw grid, draw transparency, set zero, one-way
//! line, centre on tile), the colours the overlay paints and the per-kind
//! registration the Object and Fence editors share. Building and Bridge (S10,
//! S11) add a `registrationFor` entry and reuse the rest. Each gesture ends in
//! one command from resource_core's grid_tools, committed through
//! `sub_editor_tools.commit`, so the drawing code never builds a command.
//! Runs under `zig build test-resource-app-logic` against the fake bridge.
//!
//! Three spaces meet here. "Grid space" is MFC's screen space of the preview,
//! where tile (0, 0)'s leftmost corner is (-622, 296); the view puts it on the
//! window. "World space" is what the bridge stores for the zero point and the
//! sprite; the default editor camera maps grid space to it (GridProjection's
//! DefaultEditorCamera). One-way lines are kept in grid space, as MFC keeps
//! STransLine in screen coordinates.
const std = @import("std");
const core = @import("resource_core");

const tools = core.sub_editor_tools;
const grid_tools = core.grid_tools;
const bridge_mod = core.bridge;
const ResBridge = bridge_mod.ResBridge;
const Point2 = bridge_mod.Point2;
const Kind = bridge_mod.Kind;
const GeometryChannel = bridge_mod.GeometryChannel;
const Document = core.document.Document;
const History = core.history.History;

pub const Error = bridge_mod.EditError;

/// The grid is 60 x 60 tiles (CGridFrame draws that many lines).
pub const grid_tiles: i32 = 60;

// --- Tile <-> grid space: the trig of GridProjection (grid_projection.cpp) ---

// GridFrm.cpp: "do not change, measured for compatibility with old projects".
const origin_x: f32 = -622;
const origin_y: f32 = 296;
const cell_size_y: f32 = 16;
const cell_size_x: f32 = cell_size_y * 2.0;
/// fWorldCellSize of items/stats_item.h, for the default editor camera.
const world_cell_size: f32 = 16 * 2.0 * 1.41421356;

fn alpha() f32 {
    return std.math.asin(@as(f32, 1.0) / @sqrt(@as(f32, 5.0)));
}

fn tileRealSize() f32 {
    return @sqrt((cell_size_x / 2) * (cell_size_x / 2) + (cell_size_y / 2) * (cell_size_y / 2));
}

/// ComputeGameTileCoordinates: the fractional tile coordinates of a grid-space
/// point (the C++ takes ints; callers pass whole pixels as MFC's CPoint did).
pub fn gridToTileF(x: i32, y: i32) [2]f32 {
    const sx: f32 = @floatFromInt(x);
    const sy: f32 = @floatFromInt(y);
    const om = @sqrt((origin_x - sx) * (origin_x - sx) + (origin_y - sy) * (origin_y - sy));
    const a = alpha();
    const beta = std.math.atan2(sy - origin_y, sx - origin_x);
    const temp = om / @sin(2 * a);
    const oc = temp * @sin(a + beta);
    const cm = temp * @sin(a - beta);
    const size = tileRealSize();
    return .{ oc / size, cm / size };
}

/// GetGameTileCoordinates: MFC's corner numbering, 2 the leftmost corner, 4 the
/// opposite one, 1 and 3 the others.
pub const Corners = struct { c1: Point2, c2: Point2, c3: Point2, c4: Point2 };

pub fn tileCorners(tx: i32, ty: i32) Corners {
    const a = alpha();
    const size = tileRealSize();
    const c = @cos(a);
    const s = @sin(a);
    const fx: f32 = @floatFromInt(tx);
    const fy: f32 = @floatFromInt(ty);
    return .{
        .c1 = .{ .x = origin_x + size * fx * c + size * (fy + 1) * c, .y = origin_y + size * fx * s - size * (fy + 1) * s },
        .c2 = .{ .x = origin_x + size * fx * c + size * fy * c, .y = origin_y + size * fx * s - size * fy * s },
        .c3 = .{ .x = origin_x + size * (fx + 1) * c + size * fy * c, .y = origin_y + size * (fx + 1) * s - size * fy * s },
        .c4 = .{ .x = origin_x + size * (fx + 1) * c + size * (fy + 1) * c, .y = origin_y + size * (fx + 1) * s - size * (fy + 1) * s },
    };
}

/// The tile under a grid-space point, or null outside the 60 x 60 grid. The
/// point is truncated to whole pixels and the tile coordinate to an int, as
/// the MFC callers did; a fractional coordinate below zero is outside, not
/// tile 0 as the C++ cast would make it.
pub fn tileAt(point: Point2) ?[2]i32 {
    // An absent pointer reads as NaN or a huge sentinel; neither is a pixel of the grid.
    const limit: f32 = 1.0e6;
    if (!(@abs(point.x) < limit and @abs(point.y) < limit)) return null;
    const f = gridToTileF(@intFromFloat(@trunc(point.x)), @intFromFloat(@trunc(point.y)));
    if (!(f[0] >= 0 and f[1] >= 0)) return null;
    if (f[0] >= @as(f32, @floatFromInt(grid_tiles)) or f[1] >= @as(f32, @floatFromInt(grid_tiles))) return null;
    return .{ @intFromFloat(f[0]), @intFromFloat(f[1]) };
}

/// The middle of a tile in grid space (the middle of corners 2 and 4).
pub fn tileCentre(tx: i32, ty: i32) Point2 {
    const c = tileCorners(tx, ty);
    return .{ .x = (c.c2.x + c.c4.x) / 2, .y = (c.c2.y + c.c4.y) / 2 };
}

// --- Grid space <-> world space: the default editor camera -------------------

const cam_m11: f32 = cell_size_x / 2 / world_cell_size;
const cam_m12: f32 = cell_size_x / 2 / world_cell_size;
const cam_m21: f32 = cell_size_y / 2 / world_cell_size;
const cam_m22: f32 = -cell_size_y / 2 / world_cell_size;

/// CScene::GetPos2 through DefaultEditorCamera: world (x, y, z = 0) to grid space.
pub fn worldToGrid(world: Point2) Point2 {
    return .{
        .x = cam_m11 * world.x + cam_m12 * world.y + origin_x,
        .y = cam_m21 * world.x + cam_m22 * world.y + origin_y,
    };
}

/// CScene::GetPos3 on the z = 0 plane, the same solve as GridProjection::Pos2To3.
pub fn gridToWorld(grid: Point2) Point2 {
    const det = cam_m11 * cam_m22 - cam_m12 * cam_m21;
    return .{
        .x = (cam_m12 * origin_y - cam_m12 * grid.y - origin_x * cam_m22 + grid.x * cam_m22) / det,
        .y = -(cam_m11 * origin_y - cam_m11 * grid.y - origin_x * cam_m21 + grid.x * cam_m21) / det,
    };
}

/// The overlay's window onto grid space: a window point is the origin plus the
/// grid-space point times the scale (grid space already has Y running down).
pub const View = struct {
    origin: Point2 = .{ .x = 0, .y = 0 },
    scale: f32 = 1,

    pub fn toScreen(self: View, grid: Point2) Point2 {
        return .{ .x = self.origin.x + grid.x * self.scale, .y = self.origin.y + grid.y * self.scale };
    }

    pub fn toGrid(self: View, screen: Point2) Point2 {
        return .{ .x = (screen.x - self.origin.x) / self.scale, .y = (screen.y - self.origin.y) / self.scale };
    }

    /// The window point of tile (0, 0)'s leftmost corner is `point`: how the app
    /// anchors the grid on the canvas.
    pub fn anchored(point: Point2, scale: f32) View {
        return .{ .origin = .{ .x = point.x - origin_x * scale, .y = point.y - origin_y * scale }, .scale = scale };
    }
};

// --- Colours (GridFrm.cpp:182-217, ObjectFrm.cpp:183, 893) -------------------

pub const Argb = u32;

pub const locked_color: Argb = 0xffff0000;
pub const unlocked_color: Argb = 0xff00ff00;
pub const entrance_color: Argb = 0xff00ff00;
pub const trans_line_color: Argb = 0xff0000ff;
pub const normal_tile_color: Argb = 0xff008000;
pub const zero_cross_color: Argb = 0xffff0000;
pub const grid_line_color: Argb = 0xffc0c0c0;

/// A transparency tile's colour: 0x202000 per step from value 1; null for 0
/// (no tile) and for what the bridge refuses.
pub fn transparencyColor(value: u8) ?Argb {
    if (value < 1 or value > grid_tools.max_transparency_value) return null;
    return 0xff202000 + @as(u32, value - 1) * 0x202000;
}

/// A passability cell's colour: any set cell is locked.
pub fn passabilityColor(value: u8) ?Argb {
    return if (value != 0) locked_color else null;
}

/// ImGui packs a colour as ABGR; MFC's are ARGB. The one place that swaps.
pub fn toImGui(argb: Argb) u32 {
    return (argb & 0xff00ff00) | ((argb & 0x00ff0000) >> 16) | ((argb & 0x000000ff) << 16);
}

// --- Tools -------------------------------------------------------------------

/// GridFrm's toolbar: what a click on the preview does.
pub const Tool = enum {
    move,
    draw_grid,
    draw_transparency,
    set_zero,
    one_way_line,
    centre_on_tile,

    pub fn label(self: Tool) [:0]const u8 {
        return switch (self) {
            .move => "Move",
            .draw_grid => "Draw grid",
            .draw_transparency => "Draw transparency",
            .set_zero => "Set zero",
            .one_way_line => "One-way line",
            .centre_on_tile => "Centre on tile",
        };
    }
};

/// What a kind's grid editor offers. Object and Fence register here; S10 and
/// S11 add Building and Bridge.
pub const Registration = struct {
    kind: Kind,
    tools: []const Tool,
    passability: GeometryChannel = .passability_cells,
    transparency: GeometryChannel,

    pub fn has(self: Registration, tool: Tool) bool {
        return std.mem.indexOfScalar(Tool, self.tools, tool) != null;
    }
};

const object_tools = [_]Tool{ .move, .draw_grid, .draw_transparency, .set_zero, .one_way_line };
const fence_tools = [_]Tool{ .move, .draw_grid, .draw_transparency, .centre_on_tile };

pub fn registrationFor(kind: Kind) ?Registration {
    return switch (kind) {
        .object => .{ .kind = .object, .tools = &object_tools, .transparency = .transparency_cells },
        .fence => .{ .kind = .fence, .tools = &fence_tools, .transparency = .fence_transparences },
        else => null,
    };
}

/// The node the tools edit: an object's root, a fence's selected segment (else
/// the first one, as FenceFrm's thumb list selects the first on load).
pub fn targetNode(doc: *const Document, kind: Kind, selected: ?i32) ?i32 {
    switch (kind) {
        .object => return tools.firstOfClass(doc, tools.item_type.object_root),
        .fence => {
            if (selected) |id| if (tools.findNode(doc, id)) |node| {
                if (tools.isClass(node, tools.item_type.fence_props)) return id;
            };
            return tools.firstOfClass(doc, tools.item_type.fence_props);
        },
        else => return null,
    }
}

/// FenceFrm's active insert item (SetActiveFenceInsertItem): the selected insert
/// type, or the one the selected segment belongs to, else the first.
pub fn activeInsert(doc: *const Document, selected: ?i32) ?i32 {
    if (selected) |id| if (tools.findNode(doc, id)) |node| {
        if (tools.isClass(node, tools.item_type.fence_insert)) return id;
        if (tools.isClass(node, tools.item_type.fence_props)) return node.parent;
    };
    return tools.firstOfClass(doc, tools.item_type.fence_insert);
}

/// One grid editor: the tool, its settings, the view and the gesture in progress.
pub const GridEditor = struct {
    allocator: std.mem.Allocator,
    registration: Registration,
    node: i32,
    tool: Tool = .move,
    /// The transparency combo's value, 1..7.
    transparency_value: u8 = 1,
    view: View = .{},
    stroke: ?grid_tools.BrushStroke = null,
    line_tool: grid_tools.TransLineTool,
    drag: ?grid_tools.SpriteDrag = null,
    /// Where in the sprite the press landed, so it does not jump to the cursor.
    grab: Point2 = .{},
    /// The tile under the cursor, for the status line.
    hover: ?[2]i32 = null,

    pub fn init(allocator: std.mem.Allocator, registration: Registration, node: i32) GridEditor {
        return .{ .allocator = allocator, .registration = registration, .node = node, .line_tool = grid_tools.TransLineTool.init(node) };
    }

    /// Drops a gesture in progress, putting the grid back.
    pub fn deinit(self: *GridEditor, bridge: ResBridge) void {
        self.cancel(bridge);
    }

    pub fn busy(self: *const GridEditor) bool {
        return self.stroke != null or self.drag != null or self.line_tool.dragging;
    }

    /// Switching tool abandons the gesture; a tool the kind does not offer is refused.
    pub fn setTool(self: *GridEditor, bridge: ResBridge, tool: Tool) Error!void {
        if (!self.registration.has(tool)) return error.Refused;
        self.cancel(bridge);
        self.line_tool.selected = null;
        self.tool = tool;
    }

    pub fn setTransparency(self: *GridEditor, value: u8) void {
        self.transparency_value = std.math.clamp(value, 1, grid_tools.max_transparency_value);
    }

    fn gridPoint(self: *const GridEditor, screen: Point2) Point2 {
        return self.view.toGrid(screen);
    }

    fn updateHover(self: *GridEditor, screen: Point2) void {
        self.hover = tileAt(self.gridPoint(screen));
    }

    /// A press at `screen`. `erase` is the right button of the drawing tools,
    /// painting the value 0. Set zero and centre on tile act on the release.
    pub fn press(self: *GridEditor, bridge: ResBridge, screen: Point2, erase: bool) Error!void {
        self.updateHover(screen);
        if (self.busy()) return;
        const grid = self.gridPoint(screen);
        switch (self.tool) {
            .draw_grid, .draw_transparency => {
                const tile = self.hover orelse return;
                const channel = if (self.tool == .draw_grid) self.registration.passability else self.registration.transparency;
                const value: u8 = if (erase) 0 else if (self.tool == .draw_grid) 1 else self.transparency_value;
                var stroke = try grid_tools.BrushStroke.begin(self.allocator, bridge, self.node, channel, value);
                errdefer stroke.deinit(self.allocator);
                try stroke.press(self.allocator, bridge, tile[0], tile[1]);
                self.stroke = stroke;
            },
            .one_way_line => try self.line_tool.press(self.allocator, bridge, grid),
            .move => {
                const drag = try grid_tools.SpriteDrag.begin(bridge, self.node);
                const world = gridToWorld(grid);
                self.grab = .{ .x = drag.before.x - world.x, .y = drag.before.y - world.y };
                self.drag = drag;
            },
            .set_zero, .centre_on_tile => {},
        }
    }

    /// The mouse moving, button down or not: the hover follows, a gesture runs on.
    pub fn move(self: *GridEditor, bridge: ResBridge, screen: Point2) Error!void {
        self.updateHover(screen);
        if (self.stroke) |*stroke| {
            if (self.hover) |tile| try stroke.move(self.allocator, bridge, tile[0], tile[1]);
        } else if (self.drag) |*drag| {
            const world = gridToWorld(self.gridPoint(screen));
            try drag.move(bridge, .{ .x = world.x + self.grab.x, .y = world.y + self.grab.y });
        } else {
            self.line_tool.move(self.gridPoint(screen));
        }
    }

    /// The release: the gesture's one command, committed, or nothing when it
    /// changed nothing.
    pub fn release(self: *GridEditor, bridge: ResBridge, doc: *Document, history: *History, screen: Point2) Error!void {
        self.updateHover(screen);
        const grid = self.gridPoint(screen);
        switch (self.tool) {
            .draw_grid, .draw_transparency => {
                var stroke = self.stroke orelse return;
                self.stroke = null;
                const command = (try stroke.finish(self.allocator, bridge)) orelse return;
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
            .one_way_line => {
                const command = (try self.line_tool.release(self.allocator, bridge, grid)) orelse return;
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
            .move => {
                var drag = self.drag orelse return;
                self.drag = null;
                const command = drag.finish() orelse return;
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
            .set_zero => {
                const command = try grid_tools.setZero(self.allocator, bridge, self.node, gridToWorld(grid));
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
            .centre_on_tile => {
                const tile = tileAt(grid) orelse return;
                const command = try grid_tools.centreSpriteOnTile(self.allocator, bridge, self.node, gridToWorld(tileCentre(tile[0], tile[1])));
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
        }
    }

    /// A right click with the line tool: cancels the dragged line or deletes the
    /// selected one. Other tools use the right button to erase (see `press`).
    pub fn rightClick(self: *GridEditor, bridge: ResBridge, doc: *Document, history: *History) Error!void {
        if (self.tool != .one_way_line) return;
        const command = (try self.line_tool.rightClick(self.allocator, bridge)) orelse return;
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
    }

    /// Escape: the grid and the sprite go back and nothing is recorded.
    pub fn cancel(self: *GridEditor, bridge: ResBridge) void {
        if (self.stroke) |*stroke| stroke.cancel(self.allocator, bridge);
        self.stroke = null;
        if (self.drag) |*drag| drag.cancel(bridge);
        self.drag = null;
        self.line_tool.cancel();
    }

    /// The status line: the active tool and the tile under the cursor.
    pub fn statusLine(self: *const GridEditor, buffer: []u8) []const u8 {
        const tool = self.tool.label();
        if (self.hover) |tile| {
            return std.fmt.bufPrint(buffer, "{s}: tile ({d}, {d})", .{ tool, tile[0], tile[1] }) catch tool;
        }
        return std.fmt.bufPrint(buffer, "{s}: outside the grid", .{tool}) catch tool;
    }
};

// --- Tests -------------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;
const GeometryValue = bridge_mod.GeometryValue;

const Rig = struct {
    fake: FakeResBridge,
    doc: Document = .{},
    history: History = .{},
    node: i32 = 0,

    fn init(allocator: std.mem.Allocator, kind: Kind) !Rig {
        var rig: Rig = .{ .fake = FakeResBridge.init(allocator) };
        errdefer rig.deinit(allocator);
        const bridge = rig.fake.bridge();
        try bridge_mod.check(bridge.new(kind));
        rig.node = rig.fake.nodes.items[0].id;
        try rig.doc.reload(allocator, bridge);
        return rig;
    }

    fn deinit(self: *Rig, allocator: std.mem.Allocator) void {
        self.history.deinit(allocator);
        self.doc.deinit(allocator);
        self.fake.deinit();
    }

    fn res(self: *Rig) ResBridge {
        return self.fake.bridge();
    }

    fn addNode(self: *Rig, parent: i32, class_type: i32) !i32 {
        var buf: [16]u8 = undefined;
        var id: i32 = 0;
        try bridge_mod.check(self.res().insertNode(parent, try std.fmt.bufPrint(&buf, "{d}", .{class_type}), tools.childCount(&self.doc, parent), &id));
        try self.doc.reload(testing.allocator, self.res());
        return id;
    }

    fn grid(self: *Rig, channel: GeometryChannel) !GeometryValue {
        return tools.readGeometry(self.res(), self.node, channel);
    }
};

/// A view whose window point is the grid-space point plus (50, 20), so the tests
/// also prove the view's offset and scale are applied.
const test_view: View = .{ .origin = .{ .x = 50, .y = 20 }, .scale = 2 };

fn onTile(tx: i32, ty: i32) Point2 {
    return test_view.toScreen(tileCentre(tx, ty));
}

// T01's hand-computed cases, the same numbers resource_grid_projection_test.cpp
// checks: the leftmost corner of tile (tx, ty) is (-622 + 16(tx+ty), 296 + 8(tx-ty)).
const hand_cases = [_][4]i32{ .{ 0, 0, -622, 296 }, .{ 3, 1, -558, 312 }, .{ 10, 4, -398, 344 } };

test "tile corners agree with GridProjection on T01's hand cases" {
    for (hand_cases) |h| {
        const c = tileCorners(h[0], h[1]);
        const x: f32 = @floatFromInt(h[2]);
        const y: f32 = @floatFromInt(h[3]);
        try testing.expectApproxEqAbs(x, c.c2.x, 1e-2);
        try testing.expectApproxEqAbs(y, c.c2.y, 1e-2);
        try testing.expectApproxEqAbs(x + 32, c.c4.x, 1e-2);
        try testing.expectApproxEqAbs(y, c.c4.y, 1e-2);
        try testing.expectApproxEqAbs(x + 16, c.c3.x, 1e-2);
        try testing.expectApproxEqAbs(y + 8, c.c3.y, 1e-2);
        try testing.expectApproxEqAbs(x + 16, c.c1.x, 1e-2);
        try testing.expectApproxEqAbs(y - 8, c.c1.y, 1e-2);
        // The point one half step right of corner 2 is the tile's centre.
        const f = gridToTileF(h[2] + 16, h[3]);
        try testing.expectApproxEqAbs(@as(f32, @floatFromInt(h[0])) + 0.5, f[0], 0.05);
        try testing.expectApproxEqAbs(@as(f32, @floatFromInt(h[1])) + 0.5, f[1], 0.05);
    }
}

test "the camera agrees with GridProjection: Pos2To3 inverts Pos3To2 and the first tile's corner is the origin" {
    // The default camera puts world (0, 0) at the grid origin.
    const origin = worldToGrid(.{ .x = 0, .y = 0 });
    try testing.expectApproxEqAbs(@as(f32, -622), origin.x, 1e-3);
    try testing.expectApproxEqAbs(@as(f32, 296), origin.y, 1e-3);
    const back = gridToWorld(worldToGrid(.{ .x = 7, .y = -4 }));
    try testing.expectApproxEqAbs(@as(f32, 7), back.x, 1e-3);
    try testing.expectApproxEqAbs(@as(f32, -4), back.y, 1e-3);
    // One world cell along x is a tile step of (16, 8) in grid space.
    const step = worldToGrid(.{ .x = world_cell_size, .y = 0 });
    try testing.expectApproxEqAbs(@as(f32, -622 + 16), step.x, 1e-2);
    try testing.expectApproxEqAbs(@as(f32, 296 + 8), step.y, 1e-2);
}

test "mapping: every tile of the 60 x 60 grid round-trips through its centre and the view" {
    var bad: usize = 0;
    var ty: i32 = 0;
    while (ty < grid_tiles) : (ty += 1) {
        var tx: i32 = 0;
        while (tx < grid_tiles) : (tx += 1) {
            const tile = tileAt(test_view.toGrid(onTile(tx, ty))) orelse {
                bad += 1;
                continue;
            };
            if (tile[0] != tx or tile[1] != ty) bad += 1;
        }
    }
    try testing.expectEqual(@as(usize, 0), bad);
    try testing.expect(tileAt(.{ .x = -700, .y = 296 }) == null);
    try testing.expect(tileAt(tileCentre(60, 0)) == null);
    try testing.expect(tileAt(tileCentre(0, -1)) == null);
}

test "view: toScreen and toGrid invert each other and anchored puts the origin corner on the point" {
    const p: Point2 = .{ .x = -100.5, .y = 77 };
    const back = test_view.toGrid(test_view.toScreen(p));
    try testing.expectApproxEqAbs(p.x, back.x, 1e-4);
    try testing.expectApproxEqAbs(p.y, back.y, 1e-4);
    const anchored = View.anchored(.{ .x = 10, .y = 30 }, 0.5);
    const corner = anchored.toScreen(tileCorners(0, 0).c2);
    try testing.expectApproxEqAbs(@as(f32, 10), corner.x, 1e-3);
    try testing.expectApproxEqAbs(@as(f32, 30), corner.y, 1e-3);
}

test "colour rules: locked red, unlocked and entrance green, transparency steps, trans-lines blue" {
    try testing.expectEqual(@as(Argb, 0xffff0000), passabilityColor(1).?);
    try testing.expect(passabilityColor(0) == null);
    try testing.expectEqual(@as(Argb, 0xff00ff00), unlocked_color);
    try testing.expectEqual(@as(Argb, 0xff00ff00), entrance_color);
    try testing.expectEqual(@as(Argb, 0xff0000ff), trans_line_color);
    const want = [_]Argb{ 0xff202000, 0xff404000, 0xff606000, 0xff808000, 0xffa0a000, 0xffc0c000, 0xffe0e000 };
    for (want, 1..) |colour, v| try testing.expectEqual(colour, transparencyColor(@intCast(v)).?);
    try testing.expect(transparencyColor(0) == null);
    try testing.expect(transparencyColor(8) == null);
    // ARGB to ImGui's ABGR swaps red and blue only.
    try testing.expectEqual(@as(u32, 0xff0000ff), toImGui(0xffff0000));
    try testing.expectEqual(@as(u32, 0xff00ff00), toImGui(0xff00ff00));
    try testing.expectEqual(@as(u32, 0xff007080), toImGui(0xff807000));
}

test "registration: object and fence tools, kinds without a grid editor" {
    const object = registrationFor(.object).?;
    try testing.expect(object.has(.set_zero) and object.has(.one_way_line) and !object.has(.centre_on_tile));
    try testing.expectEqual(GeometryChannel.transparency_cells, object.transparency);
    const fence = registrationFor(.fence).?;
    try testing.expect(fence.has(.centre_on_tile) and !fence.has(.set_zero) and !fence.has(.one_way_line));
    try testing.expectEqual(GeometryChannel.fence_transparences, fence.transparency);
    try testing.expect(registrationFor(.weapon) == null);
}

test "target node: an object's root, a fence's selected segment else the first, the active insert" {
    var object = try Rig.init(testing.allocator, .object);
    defer object.deinit(testing.allocator);
    try testing.expect(targetNode(&object.doc, .object, null) == null);
    const root = try object.addNode(object.node, tools.item_type.object_root);
    try testing.expectEqual(root, targetNode(&object.doc, .object, null).?);

    var fence = try Rig.init(testing.allocator, .fence);
    defer fence.deinit(testing.allocator);
    const insert_a = try fence.addNode(fence.node, tools.item_type.fence_insert);
    const insert_b = try fence.addNode(fence.node, tools.item_type.fence_insert);
    const seg_a = try fence.addNode(insert_a, tools.item_type.fence_props);
    const seg_b = try fence.addNode(insert_b, tools.item_type.fence_props);
    try testing.expectEqual(seg_a, targetNode(&fence.doc, .fence, null).?);
    try testing.expectEqual(seg_b, targetNode(&fence.doc, .fence, seg_b).?);
    try testing.expectEqual(seg_a, targetNode(&fence.doc, .fence, insert_b).?);
    try testing.expectEqual(insert_a, activeInsert(&fence.doc, null).?);
    try testing.expectEqual(insert_b, activeInsert(&fence.doc, seg_b).?);
    try testing.expectEqual(insert_b, activeInsert(&fence.doc, insert_b).?);
}

fn expectCells(rig: *Rig, channel: GeometryChannel, w: i32, h: i32, cells: []const u8) !void {
    var read = try rig.grid(channel);
    defer read.deinit(testing.allocator);
    try testing.expectEqual(w, read.bytes_grid.width);
    try testing.expectEqual(h, read.bytes_grid.height);
    try testing.expectEqualSlices(u8, cells, read.bytes_grid.bytes);
}

test "draw grid: a stroke over tiles is one undo step with state before and after" {
    var rig = try Rig.init(testing.allocator, .object);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.object).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .draw_grid);
    try expectCells(&rig, .passability_cells, 0, 0, &.{});
    try editor.press(bridge, onTile(1, 0), false);
    try testing.expect(editor.busy());
    try editor.move(bridge, onTile(2, 0));
    try editor.move(bridge, onTile(3, 0));
    try editor.release(bridge, &rig.doc, &rig.history, onTile(3, 0));
    try testing.expect(!editor.busy());
    try expectCells(&rig, .passability_cells, 4, 1, &.{ 0, 1, 1, 1 });
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    // The right button erases.
    try editor.press(bridge, onTile(2, 0), true);
    try editor.release(bridge, &rig.doc, &rig.history, onTile(2, 0));
    try expectCells(&rig, .passability_cells, 4, 1, &.{ 0, 1, 0, 1 });
    try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
    // A press outside the grid starts nothing.
    try editor.press(bridge, test_view.toScreen(.{ .x = -900, .y = 0 }), false);
    try testing.expect(!editor.busy());
    try testing.expect(editor.hover == null);
}

test "draw transparency: the combo's value is painted, per kind channel, and a stroke can be cancelled" {
    var rig = try Rig.init(testing.allocator, .fence);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.fence).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .draw_transparency);
    editor.setTransparency(9);
    try testing.expectEqual(@as(u8, 7), editor.transparency_value);
    editor.setTransparency(0);
    try testing.expectEqual(@as(u8, 1), editor.transparency_value);
    editor.setTransparency(5);
    try editor.press(bridge, onTile(0, 0), false);
    try editor.move(bridge, onTile(0, 1));
    try expectCells(&rig, .fence_transparences, 1, 2, &.{ 5, 5 });
    editor.cancel(bridge);
    try expectCells(&rig, .fence_transparences, 0, 0, &.{});
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
    try editor.press(bridge, onTile(0, 0), false);
    try editor.release(bridge, &rig.doc, &rig.history, onTile(0, 0));
    try expectCells(&rig, .fence_transparences, 1, 1, &.{5});
    try expectCells(&rig, .passability_cells, 0, 0, &.{});
}

test "set zero: the click's world point, one undo step" {
    var rig = try Rig.init(testing.allocator, .object);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.object).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .set_zero);
    const click = onTile(4, 2);
    try editor.press(bridge, click, false);
    try editor.release(bridge, &rig.doc, &rig.history, click);
    const world = gridToWorld(tileCentre(4, 2));
    var zero = try rig.grid(.zero_point);
    try testing.expectApproxEqAbs(world.x, zero.point2.x, 1e-3);
    try testing.expectApproxEqAbs(world.y, zero.point2.y, 1e-3);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try testing.expect(editor.registration.has(.set_zero));
    zero.deinit(testing.allocator);
}

test "one-way line: drag, release, right-click delete, all undoable" {
    var rig = try Rig.init(testing.allocator, .object);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.object).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .one_way_line);
    const a: Point2 = .{ .x = -600, .y = 300 };
    const b: Point2 = .{ .x = -560, .y = 300 };
    try editor.press(bridge, test_view.toScreen(a), false);
    try editor.move(bridge, test_view.toScreen(.{ .x = -580, .y = 300 }));
    try editor.release(bridge, &rig.doc, &rig.history, test_view.toScreen(b));
    {
        var lines = try rig.grid(.transparency_lines);
        defer lines.deinit(testing.allocator);
        try testing.expectEqual(@as(usize, 2), lines.points2.len);
        try testing.expectApproxEqAbs(a.x, lines.points2[0].x, 1e-3);
        try testing.expectApproxEqAbs(b.x, lines.points2[1].x, 1e-3);
    }
    // Select it by its middle, delete it with the right button.
    try editor.press(bridge, test_view.toScreen(.{ .x = -580, .y = 301 }), false);
    try testing.expectEqual(@as(?usize, 0), editor.line_tool.selected);
    try editor.rightClick(bridge, &rig.doc, &rig.history);
    {
        var lines = try rig.grid(.transparency_lines);
        defer lines.deinit(testing.allocator);
        try testing.expectEqual(@as(usize, 0), lines.points2.len);
    }
    try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
}

test "move: a sprite drag keeps the grab offset and is one undo step; centre on tile snaps to the tile's centre" {
    var rig = try Rig.init(testing.allocator, .fence);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.fence).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    // A sprite at world (10, 20); the press lands on grid point `start`.
    const seed: GeometryValue = .{ .point2 = .{ .x = 10, .y = 20 } };
    try bridge_mod.check(bridge.geometryWrite(rig.node, .sprite_pos, &seed));
    const start = worldToGrid(.{ .x = 12, .y = 21 });
    try editor.press(bridge, test_view.toScreen(start), false);
    try editor.move(bridge, test_view.toScreen(worldToGrid(.{ .x = 32, .y = 41 })));
    try editor.release(bridge, &rig.doc, &rig.history, test_view.toScreen(worldToGrid(.{ .x = 32, .y = 41 })));
    {
        const pos = (try rig.grid(.sprite_pos)).point2;
        try testing.expectApproxEqAbs(@as(f32, 30), pos.x, 1e-2);
        try testing.expectApproxEqAbs(@as(f32, 40), pos.y, 1e-2);
    }
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    // A click without a move changes nothing and records nothing.
    try editor.press(bridge, onTile(2, 2), false);
    try editor.release(bridge, &rig.doc, &rig.history, onTile(2, 2));
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);

    try editor.setTool(bridge, .centre_on_tile);
    try editor.press(bridge, onTile(5, 3), false);
    try editor.release(bridge, &rig.doc, &rig.history, onTile(5, 3));
    const centre = gridToWorld(tileCentre(5, 3));
    const pos = (try rig.grid(.sprite_pos)).point2;
    try testing.expectApproxEqAbs(centre.x, pos.x, 1e-3);
    try testing.expectApproxEqAbs(centre.y, pos.y, 1e-3);
    try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
}

test "tool transitions: a switch drops the gesture, an unregistered tool is refused, the status names tool and tile" {
    var rig = try Rig.init(testing.allocator, .fence);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.fence).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try testing.expectEqual(Tool.move, editor.tool);
    try testing.expectError(error.Refused, editor.setTool(bridge, .set_zero));
    try testing.expectError(error.Refused, editor.setTool(bridge, .one_way_line));
    try editor.setTool(bridge, .draw_grid);
    try editor.press(bridge, onTile(1, 1), false);
    try testing.expect(editor.busy());
    try editor.setTool(bridge, .draw_transparency);
    try testing.expect(!editor.busy());
    try expectCells(&rig, .passability_cells, 0, 0, &.{});
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);

    var buffer: [96]u8 = undefined;
    try editor.move(bridge, onTile(7, 9));
    try testing.expectEqualStrings("Draw transparency: tile (7, 9)", editor.statusLine(&buffer));
    try editor.move(bridge, test_view.toScreen(.{ .x = 5000, .y = 5000 }));
    try testing.expectEqualStrings("Draw transparency: outside the grid", editor.statusLine(&buffer));
}
