//! The grid sub-editors' view and tools with no window and no ImGui: the
//! mapping between the overlay's pixels and the AI tile grid, the tool modes of
//! GridFrm's toolbar (move, draw grid, draw transparency, set zero, one-way
//! line, centre on tile), the colours the overlay paints and the per-kind
//! registration the Object and Fence editors share. The Building editor adds
//! the point modes of BuildFrm (entrance, shoot, fire, smoke, directed
//! explosion, move point, horizontal position, angle and cone handles, generate
//! points) and Bridge (S11) adds a `registrationFor` entry. Each gesture ends in
//! one command from resource_core's grid_tools or point_tools, committed through
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
const point_tools = core.point_tools;
const bridge_mod = core.bridge;
const ResBridge = bridge_mod.ResBridge;
const Point2 = bridge_mod.Point2;
const AimedPoint = bridge_mod.AimedPoint;
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
/// BuildShoot/Fire/Smoke/DirExp.cpp ComputeAngleLines: the direction line and
/// the two cone edges are yellow, the arrow head at the direction's tip red.
pub const cone_line_color: Argb = 0xffffff00;
pub const arrow_color: Argb = 0xffff0000;
/// SetActiveShootPoint: the active point's sprite is opaque, the others 120/255.
pub const point_color: Argb = 0xffffffff;
pub const inactive_point_alpha: u32 = 120;

/// A point's marker colour: its family's tint, opaque for the active point,
/// `inactive_point_alpha` for the rest.
pub fn pointColor(mode: point_tools.Mode, active: bool) Argb {
    const rgb: u32 = switch (mode) {
        .shoot => point_color,
        .fire => 0xffff8000,
        .smoke => 0xffc0c0c0,
        .dir_explosion => 0xffff00ff,
    } & 0x00ffffff;
    return (if (active) @as(u32, 0xff) else inactive_point_alpha) << 24 | rgb;
}

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

// --- Aimed points: handles and hit-testing -----------------------------------

/// BuildShoot.cpp's EDGE_LENGTH, in world units: how far the direction line and
/// the cone edges reach from the point.
pub const edge_length: f32 = 200;

/// A point's direction in world space: ComputeAngleLines steps (-sin a, cos a).
pub fn aimDirection(angle_degrees: f32) Point2 {
    const a = std.math.degreesToRadians(angle_degrees);
    return .{ .x = -@sin(a), .y = @cos(a) };
}

/// Where along a point's aim a handle sits, in world space.
pub fn aimTip(at: Point2, angle_degrees: f32) Point2 {
    const d = aimDirection(angle_degrees);
    return .{ .x = at.x + edge_length * d.x, .y = at.y + edge_length * d.y };
}

/// The three handles of one point, in grid space: the direction's tip (where the
/// red arrow is) and the two cone edges' tips (direction -/+ half the cone).
pub const Handles = struct { origin: Point2, direction: Point2, cone_minus: Point2, cone_plus: Point2 };

pub fn handlesOf(point: AimedPoint) Handles {
    const angle: f32 = @floatFromInt(point.angle);
    const half: f32 = @as(f32, @floatFromInt(point.cone)) / 2;
    return .{
        .origin = worldToGrid(point.at),
        .direction = worldToGrid(aimTip(point.at, angle)),
        .cone_minus = worldToGrid(aimTip(point.at, angle - half)),
        .cone_plus = worldToGrid(aimTip(point.at, angle + half)),
    };
}

fn distance(a: Point2, b: Point2) f32 {
    return @sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

/// The point under a grid-space position, the nearest within `radius` (a later
/// point wins a tie, as the one drawn on top). Null when none is.
pub fn hitPoint(points: []const AimedPoint, grid: Point2, radius: f32) ?usize {
    var best: ?usize = null;
    var best_distance = radius;
    for (points, 0..) |point, i| {
        const d = distance(worldToGrid(point.at), grid);
        if (d <= best_distance) {
            best = i;
            best_distance = d;
        }
    }
    return best;
}

/// The handle of one point under a grid-space position. The direction's tip
/// wins over a cone edge's when they coincide (a zero cone).
pub fn hitHandle(point: AimedPoint, grid: Point2, radius: f32) ?point_tools.Part {
    const handles = handlesOf(point);
    const dd = distance(handles.direction, grid);
    const dm = distance(handles.cone_minus, grid);
    const dp = distance(handles.cone_plus, grid);
    if (dd <= radius and dd <= dm and dd <= dp) return .direction;
    if (dm <= radius or dp <= radius) return .cone;
    return null;
}

/// The whole degree of the direction from `centre` to `pointer` (world), 0..359,
/// the inverse of `aimDirection`. The same point gives 0.
pub fn angleToward(centre: Point2, pointer: Point2) i32 {
    const dx = pointer.x - centre.x;
    const dy = pointer.y - centre.y;
    if (dx == 0 and dy == 0) return 0;
    const degrees = std.math.radiansToDegrees(std.math.atan2(-dx, dy));
    const whole: i32 = @intFromFloat(@round(degrees));
    return @mod(whole, 360);
}

/// The cone the pointer asks for with the direction `angle`: twice the way the
/// pointer's direction differs from it, 0..360.
pub fn coneToward(centre: Point2, angle: i32, pointer: Point2) i32 {
    const diff = @mod(angleToward(centre, pointer) - angle + 180, 360) - 180;
    return std.math.clamp(2 * @as(i32, @intCast(@abs(diff))), 0, 360);
}

fn gridFrameToChannel(_: ?*anyopaque, screen: Point2) Point2 {
    return gridToWorld(screen);
}

/// How GenerateSmokePoints' grid pixels become the channel's world positions.
pub const generate_frame: point_tools.Frame = .{ .toChannel = gridFrameToChannel };

// --- Tools -------------------------------------------------------------------

/// GridFrm's toolbar: what a click on the preview does.
pub const Tool = enum {
    move,
    draw_grid,
    draw_transparency,
    set_zero,
    one_way_line,
    centre_on_tile,
    // The Building editor's point modes (BuildFrm SetActiveMode). The four
    // aimed families place, select and delete their points; the other three
    // act on the active family's active point, as E_SUB_MOVE, E_SUB_HOR and
    // E_SUB_DIR do.
    entrance,
    shoot,
    fire,
    smoke,
    dir_explosion,
    move_point,
    horizontal,
    angle,

    pub fn label(self: Tool) [:0]const u8 {
        return switch (self) {
            .move => "Move",
            .draw_grid => "Draw grid",
            .draw_transparency => "Draw transparency",
            .set_zero => "Set zero",
            .one_way_line => "One-way line",
            .centre_on_tile => "Centre on tile",
            .entrance => "Entrance",
            .shoot => "Shoot points",
            .fire => "Fire points",
            .smoke => "Smoke points",
            .dir_explosion => "Directed explosions",
            .move_point => "Move point",
            .horizontal => "Horizontal position",
            .angle => "Angle and cone",
        };
    }

    /// The aimed family a place/select mode works on.
    pub fn family(self: Tool) ?point_tools.Mode {
        return switch (self) {
            .shoot => .shoot,
            .fire => .fire,
            .smoke => .smoke,
            .dir_explosion => .dir_explosion,
            else => null,
        };
    }

    /// Whether the tool works on the points of the active family.
    pub fn isPointTool(self: Tool) bool {
        return self.family() != null or self == .move_point or self == .horizontal or self == .angle;
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
const building_tools = [_]Tool{ .move, .draw_grid, .draw_transparency, .set_zero, .entrance, .shoot, .fire, .smoke, .dir_explosion, .move_point, .horizontal, .angle };

pub fn registrationFor(kind: Kind) ?Registration {
    return switch (kind) {
        .object => .{ .kind = .object, .tools = &object_tools, .transparency = .transparency_cells },
        .fence => .{ .kind = .fence, .tools = &fence_tools, .transparency = .fence_transparences },
        .build => .{ .kind = .build, .tools = &building_tools, .transparency = .transparency_cells },
        else => null,
    };
}

/// The node the tools edit: an object's root, a fence's selected segment (else
/// the first one, as FenceFrm's thumb list selects the first on load).
pub fn targetNode(doc: *const Document, kind: Kind, selected: ?i32) ?i32 {
    switch (kind) {
        .object => return tools.firstOfClass(doc, tools.item_type.object_root),
        .build => return tools.firstOfClass(doc, tools.item_type.building_root),
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
    /// The Building editor's aimed family (the last of the four place modes
    /// chosen, which Move point, Horizontal position and Angle then act on) and
    /// its active point, MFC's pActiveShootPoint and kin.
    family: point_tools.Mode = .shoot,
    active: ?usize = null,
    point_drag: ?point_tools.PointDrag = null,

    pub fn init(allocator: std.mem.Allocator, registration: Registration, node: i32) GridEditor {
        return .{ .allocator = allocator, .registration = registration, .node = node, .line_tool = grid_tools.TransLineTool.init(node) };
    }

    /// Drops a gesture in progress, putting the grid back.
    pub fn deinit(self: *GridEditor, bridge: ResBridge) void {
        self.cancel(bridge);
    }

    pub fn busy(self: *const GridEditor) bool {
        return self.stroke != null or self.drag != null or self.line_tool.dragging or self.point_drag != null;
    }

    /// Switching tool abandons the gesture; a tool the kind does not offer is refused.
    pub fn setTool(self: *GridEditor, bridge: ResBridge, tool: Tool) Error!void {
        if (!self.registration.has(tool)) return error.Refused;
        self.cancel(bridge);
        self.line_tool.selected = null;
        if (tool.family()) |family| {
            if (family != self.family) self.active = null;
            self.family = family;
        }
        self.tool = tool;
    }

    fn channelOf(mode: point_tools.Mode) GeometryChannel {
        return switch (mode) {
            .shoot => .shoot_points,
            .fire => .fire_points,
            .smoke => .smoke_points,
            .dir_explosion => .directed_explosion_points,
        };
    }

    /// What a drag needs of the family: the container and child class matter
    /// only to place and delete, which have the document.
    fn dragTarget(self: *const GridEditor) point_tools.Target {
        return .{ .node = self.node, .channel = channelOf(self.family), .container = 0, .child_class = 0 };
    }

    fn documentTarget(self: *const GridEditor, doc: *const Document) Error!point_tools.Target {
        return point_tools.buildingTarget(doc, self.node, self.family) orelse error.Refused;
    }

    /// The hit radius of a point or handle: 10 window pixels, in grid pixels.
    fn hitRadius(self: *const GridEditor) f32 {
        return 10 / self.view.scale;
    }

    fn readPoints(self: *const GridEditor, bridge: ResBridge) Error!bridge_mod.GeometryValue {
        return tools.readGeometry(bridge, self.node, channelOf(self.family));
    }

    /// The active point, dropped when the list no longer has it (an undo).
    fn activePoint(self: *GridEditor, points: []const AimedPoint) ?usize {
        if (self.active) |index| {
            if (index < points.len) return index;
            self.active = null;
        }
        return null;
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
            .set_zero, .centre_on_tile, .entrance, .shoot, .fire, .smoke, .dir_explosion => {},
            .move_point, .horizontal => {
                var points = try self.readPoints(bridge);
                defer points.deinit(self.allocator);
                const index = hitPoint(points.aimed, grid, self.hitRadius()) orelse return;
                self.active = index;
                const at = points.aimed[index].at;
                const world = gridToWorld(grid);
                self.grab = .{ .x = at.x - world.x, .y = at.y - world.y };
                const part: point_tools.Part = if (self.tool == .move_point) .move else .horizontal;
                self.point_drag = try point_tools.PointDrag.begin(self.allocator, bridge, self.dragTarget(), index, part);
            },
            .angle => {
                var points = try self.readPoints(bridge);
                defer points.deinit(self.allocator);
                if (self.activePoint(points.aimed)) |index| {
                    if (hitHandle(points.aimed[index], grid, self.hitRadius())) |part| {
                        self.point_drag = try point_tools.PointDrag.begin(self.allocator, bridge, self.dragTarget(), index, part);
                        return;
                    }
                }
                if (hitPoint(points.aimed, grid, self.hitRadius())) |index| self.active = index;
            },
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
        } else if (self.point_drag) |*drag| {
            const world = gridToWorld(self.gridPoint(screen));
            const held = drag.current[drag.index];
            const sample: point_tools.Sample = switch (drag.part) {
                .move, .horizontal => .{ .at = .{ .x = world.x + self.grab.x, .y = world.y + self.grab.y } },
                .direction => .{ .angle = angleToward(held.at, world) },
                .cone => .{ .cone = coneToward(held.at, held.angle, world) },
                .aim => .{ .angle = angleToward(held.at, world), .cone = held.cone },
            };
            try drag.move(bridge, sample);
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
            .entrance => {
                const command = try point_tools.setEntrance(self.allocator, bridge, self.node, gridToWorld(grid));
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
            .shoot, .fire, .smoke, .dir_explosion => try self.pointClick(bridge, doc, history, grid),
            .move_point, .horizontal, .angle => {
                var drag = self.point_drag orelse return;
                self.point_drag = null;
                const command = drag.finish(self.allocator) orelse return;
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
        }
    }

    /// AddOrSelect...Point: a click on a point makes it the active one, a click
    /// on free ground adds a point there (copying the active one's direction)
    /// and makes that the active one. The directed explosions are five fixed
    /// points: a click only selects.
    fn pointClick(self: *GridEditor, bridge: ResBridge, doc: *Document, history: *History, grid: Point2) Error!void {
        var points = try self.readPoints(bridge);
        defer points.deinit(self.allocator);
        if (hitPoint(points.aimed, grid, self.hitRadius())) |index| {
            self.active = index;
            return;
        }
        if (self.family == .dir_explosion) return;
        const target = try self.documentTarget(doc);
        const command = try point_tools.placePoint(self.allocator, doc, bridge, target, gridToWorld(grid), self.activePoint(points.aimed));
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        self.active = points.aimed.len;
    }

    /// A right click with a point mode: the point under the cursor is deleted
    /// with its tree child, as DeleteShootPoint does. A no-op on free ground and
    /// for the directed explosions, which MFC never deletes.
    pub fn deletePointAt(self: *GridEditor, bridge: ResBridge, doc: *Document, history: *History, screen: Point2) Error!void {
        if (!self.tool.isPointTool() or self.family == .dir_explosion) return;
        var points = try self.readPoints(bridge);
        defer points.deinit(self.allocator);
        const index = hitPoint(points.aimed, self.gridPoint(screen), self.hitRadius()) orelse return;
        try self.deleteIndex(bridge, doc, history, index);
    }

    /// The Delete key: the active point goes.
    pub fn deleteActive(self: *GridEditor, bridge: ResBridge, doc: *Document, history: *History) Error!void {
        if (!self.tool.isPointTool() or self.family == .dir_explosion) return;
        const index = self.active orelse return;
        try self.deleteIndex(bridge, doc, history, index);
    }

    fn deleteIndex(self: *GridEditor, bridge: ResBridge, doc: *Document, history: *History, index: usize) Error!void {
        const target = try self.documentTarget(doc);
        const command = try point_tools.deletePoint(self.allocator, doc, bridge, target, index);
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        if (self.active) |active| {
            if (active == index) self.active = null else if (active > index) self.active = active - 1;
        }
    }

    /// OnUpdateGeneratePoints: the button works in the smoke and
    /// directed-explosion modes only.
    pub fn canGenerate(self: *const GridEditor) bool {
        return self.registration.kind == .build and self.tool.isPointTool() and (self.family == .smoke or self.family == .dir_explosion);
    }

    /// OnGeneratePoints: the smoke points are replaced by ones along the
    /// footprint's edges, the five explosions are put at their places. One undo
    /// step, none when the explosions are there already.
    pub fn generate(self: *GridEditor, bridge: ResBridge, doc: *Document, history: *History) Error!void {
        if (!self.canGenerate()) return error.Refused;
        const target = try self.documentTarget(doc);
        const command = switch (self.family) {
            .smoke => try point_tools.generateSmokePoints(self.allocator, doc, bridge, target, generate_frame),
            .dir_explosion => (try point_tools.generateDirExpPoints(self.allocator, bridge, target, generate_frame)) orelse return,
            else => return error.Refused,
        };
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        self.active = null;
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
        if (self.point_drag) |*drag| drag.cancel(self.allocator, bridge);
        self.point_drag = null;
        self.line_tool.cancel();
    }

    /// The status line: the active tool and the tile under the cursor.
    pub fn statusLine(self: *const GridEditor, buffer: []u8) []const u8 {
        const tool = self.tool.label();
        if (self.tool.isPointTool()) {
            if (self.active) |index| {
                return std.fmt.bufPrint(buffer, "{s}: point {d} of the {s} list is active", .{ tool, index + 1, @tagName(self.family) }) catch tool;
            }
        }
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

    /// A building with the four point containers of its tree and the five
    /// fixed directed-explosion children (zero points, as a new project has).
    fn building(allocator: std.mem.Allocator) !Rig {
        var rig = try Rig.init(allocator, .build);
        errdefer rig.deinit(allocator);
        const item = tools.item_type;
        _ = try rig.addNode(rig.node, item.building_slots);
        _ = try rig.addNode(rig.node, item.building_fire_points);
        _ = try rig.addNode(rig.node, item.building_smokes);
        const explosions = try rig.addNode(rig.node, item.building_dir_explosions);
        var n: usize = 0;
        while (n < 5) : (n += 1) _ = try rig.addNode(explosions, item.building_dir_explosion_props);
        const five = [_]AimedPoint{.{}} ** 5;
        try bridge_mod.check(rig.res().geometryWrite(rig.node, .directed_explosion_points, &.{ .aimed = @constCast(&five) }));
        return rig;
    }

    fn undo(self: *Rig) !void {
        try self.history.redo_stack.ensureUnusedCapacity(testing.allocator, 1);
        var entry = self.history.undo_stack.pop().?;
        try self.doc.undoOne(testing.allocator, self.res(), &entry.command);
        self.history.redo_stack.appendAssumeCapacity(entry);
    }

    fn redo(self: *Rig) !void {
        try self.history.undo_stack.ensureUnusedCapacity(testing.allocator, 1);
        var entry = self.history.redo_stack.pop().?;
        try self.doc.redoOne(testing.allocator, self.res(), &entry.command);
        self.history.undo_stack.appendAssumeCapacity(entry);
    }

    fn points(self: *Rig, channel: GeometryChannel) !GeometryValue {
        return self.grid(channel);
    }

    fn pointCount(self: *Rig, channel: GeometryChannel) !usize {
        var read = try self.points(channel);
        defer read.deinit(testing.allocator);
        return read.aimed.len;
    }

    fn pointAt(self: *Rig, channel: GeometryChannel, index: usize) !AimedPoint {
        var read = try self.points(channel);
        defer read.deinit(testing.allocator);
        return read.aimed[index];
    }
};

/// A gesture of the editor's mouse: press at `from`, move and release at `to`.
fn gesture(editor: *GridEditor, rig: *Rig, from: Point2, to: Point2) !void {
    const bridge = rig.res();
    try editor.press(bridge, from, false);
    try editor.move(bridge, to);
    try editor.release(bridge, &rig.doc, &rig.history, to);
}

fn clickAt(editor: *GridEditor, rig: *Rig, at: Point2) !void {
    try gesture(editor, rig, at, at);
}

fn screenOfWorld(world: Point2) Point2 {
    return test_view.toScreen(worldToGrid(world));
}

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
    try testing.expectEqual(@as(Argb, 0xffffff00), cone_line_color);
    try testing.expectEqual(@as(Argb, 0xffff0000), arrow_color);
    try testing.expectEqual(@as(Argb, 0xffffffff), pointColor(.shoot, true));
    try testing.expectEqual(@as(Argb, 0x78ff8000), pointColor(.fire, false));
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
    const building = registrationFor(.build).?;
    try testing.expectEqual(GeometryChannel.transparency_cells, building.transparency);
    try testing.expectEqual(GeometryChannel.passability_cells, building.passability);
    for ([_]Tool{ .move, .draw_grid, .draw_transparency, .set_zero, .entrance, .shoot, .fire, .smoke, .dir_explosion, .move_point, .horizontal, .angle }) |tool| try testing.expect(building.has(tool));
    try testing.expect(!building.has(.one_way_line) and !building.has(.centre_on_tile));
    try testing.expect(!object.has(.shoot) and !fence.has(.angle));
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

// --- Building point modes ----------------------------------------------------

test "building target node is the building root; point tools and families" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    try testing.expect(targetNode(&rig.doc, .build, null) == null);
    const root = try rig.addNode(rig.node, tools.item_type.building_root);
    try testing.expectEqual(root, targetNode(&rig.doc, .build, null).?);
    try testing.expectEqual(point_tools.Mode.smoke, Tool.smoke.family().?);
    try testing.expect(Tool.angle.family() == null and Tool.angle.isPointTool());
    try testing.expect(!Tool.entrance.isPointTool() and !Tool.draw_grid.isPointTool());
}

test "aim geometry: directions follow ComputeAngleLines, handles and angles invert, hits find the nearest" {
    // Angle 0 steps along +y, 90 along -x, 180 along -y, 270 along +x.
    const d0 = aimDirection(0);
    const d90 = aimDirection(90);
    try testing.expectApproxEqAbs(@as(f32, 0), d0.x, 1e-5);
    try testing.expectApproxEqAbs(@as(f32, 1), d0.y, 1e-5);
    try testing.expectApproxEqAbs(@as(f32, -1), d90.x, 1e-5);
    const centre: Point2 = .{ .x = 30, .y = -20 };
    for ([_]i32{ 0, 1, 45, 90, 179, 180, 270, 359 }) |angle| {
        try testing.expectEqual(angle, angleToward(centre, aimTip(centre, @floatFromInt(angle))));
    }
    try testing.expectEqual(@as(i32, 0), angleToward(centre, centre));
    // The cone is twice the way the pointer is off the direction, on either side.
    try testing.expectEqual(@as(i32, 80), coneToward(centre, 10, aimTip(centre, 50)));
    try testing.expectEqual(@as(i32, 80), coneToward(centre, 10, aimTip(centre, 330)));
    try testing.expectEqual(@as(i32, 0), coneToward(centre, 10, aimTip(centre, 10)));

    const points = [_]AimedPoint{ .{ .at = .{ .x = 0, .y = 0 }, .angle = 0, .cone = 80 }, .{ .at = .{ .x = 3, .y = 0 }, .angle = 90, .cone = 40 } };
    const at_first = worldToGrid(points[0].at);
    try testing.expectEqual(@as(?usize, 0), hitPoint(&points, at_first, 1));
    // Between two points the nearer wins; the radius bounds the hit.
    const near_second: Point2 = .{ .x = worldToGrid(points[1].at).x + 0.5, .y = worldToGrid(points[1].at).y };
    try testing.expectEqual(@as(?usize, 1), hitPoint(&points, near_second, 5));
    try testing.expectEqual(@as(?usize, null), hitPoint(&points, .{ .x = at_first.x + 100, .y = at_first.y }, 5));
    try testing.expectEqual(@as(?usize, null), hitPoint(&.{}, at_first, 5));

    const handles = handlesOf(points[0]);
    try testing.expectEqual(@as(?point_tools.Part, .direction), hitHandle(points[0], handles.direction, 3));
    try testing.expectEqual(@as(?point_tools.Part, .cone), hitHandle(points[0], handles.cone_plus, 3));
    try testing.expectEqual(@as(?point_tools.Part, .cone), hitHandle(points[0], handles.cone_minus, 3));
    try testing.expectEqual(@as(?point_tools.Part, null), hitHandle(points[0], handles.origin, 3));
    // A zero cone puts all three tips together: the direction handle wins.
    const flat: AimedPoint = .{ .angle = 30, .cone = 0 };
    try testing.expectEqual(@as(?point_tools.Part, .direction), hitHandle(flat, handlesOf(flat).direction, 3));
}

test "shoot mode: a click on free ground adds a point and its child, a click on a point selects, undo and redo" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.build).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .shoot);
    try clickAt(&editor, &rig, onTile(4, 2));
    try testing.expectEqual(@as(usize, 1), try rig.pointCount(.shoot_points));
    try testing.expectEqual(@as(?usize, 0), editor.active);
    const first = try rig.pointAt(.shoot_points, 0);
    const want = gridToWorld(tileCentre(4, 2));
    try testing.expectApproxEqAbs(want.x, first.at.x, 1e-3);
    try testing.expectEqual(@as(i32, 80), first.cone);
    const slots = tools.firstOfClass(&rig.doc, tools.item_type.building_slots).?;
    try testing.expectEqual(@as(i32, 1), tools.childCount(&rig.doc, slots));

    // Aim the first point, then add a second: it copies direction and cone.
    try editor.setTool(bridge, .angle);
    const tip = test_view.toScreen(handlesOf(first).direction);
    try gesture(&editor, &rig, tip, screenOfWorld(aimTip(first.at, 90)));
    try testing.expectEqual(@as(i32, 90), (try rig.pointAt(.shoot_points, 0)).angle);
    try editor.setTool(bridge, .shoot);
    try clickAt(&editor, &rig, onTile(10, 6));
    try testing.expectEqual(@as(usize, 2), try rig.pointCount(.shoot_points));
    try testing.expectEqual(@as(?usize, 1), editor.active);
    var line: [96]u8 = undefined;
    try testing.expect(std.mem.indexOf(u8, editor.statusLine(&line), "point 2") != null);
    try testing.expectEqual(@as(i32, 90), (try rig.pointAt(.shoot_points, 1)).angle);

    // A click on the first point selects it and adds nothing.
    const steps = rig.history.undo_stack.items.len;
    try clickAt(&editor, &rig, onTile(4, 2));
    try testing.expectEqual(@as(?usize, 0), editor.active);
    try testing.expectEqual(@as(usize, 2), try rig.pointCount(.shoot_points));
    try testing.expectEqual(steps, rig.history.undo_stack.items.len);

    try rig.undo();
    try testing.expectEqual(@as(usize, 1), try rig.pointCount(.shoot_points));
    try testing.expectEqual(@as(i32, 1), tools.childCount(&rig.doc, slots));
    try rig.redo();
    try testing.expectEqual(@as(usize, 2), try rig.pointCount(.shoot_points));
    try testing.expectEqual(@as(i32, 2), tools.childCount(&rig.doc, slots));
}

test "fire and smoke modes edit their own lists; the explosions are fixed and only select" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.build).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .fire);
    try clickAt(&editor, &rig, onTile(3, 3));
    try editor.setTool(bridge, .smoke);
    try testing.expectEqual(@as(?usize, null), editor.active);
    try clickAt(&editor, &rig, onTile(6, 3));
    try clickAt(&editor, &rig, onTile(8, 8));
    try testing.expectEqual(@as(usize, 1), try rig.pointCount(.fire_points));
    try testing.expectEqual(@as(usize, 2), try rig.pointCount(.smoke_points));
    try testing.expectEqual(@as(usize, 0), try rig.pointCount(.shoot_points));
    // A fire or smoke point copies the direction, not the cone.
    try testing.expectEqual(@as(i32, 0), (try rig.pointAt(.smoke_points, 1)).cone);

    // The explosions sit at the origin: a click there selects, one elsewhere does nothing.
    try editor.setTool(bridge, .dir_explosion);
    const steps = rig.history.undo_stack.items.len;
    try clickAt(&editor, &rig, screenOfWorld(.{ .x = 0, .y = 0 }));
    try testing.expect(editor.active != null);
    try clickAt(&editor, &rig, onTile(20, 20));
    try testing.expectEqual(@as(usize, 5), try rig.pointCount(.directed_explosion_points));
    try testing.expectEqual(steps, rig.history.undo_stack.items.len);
    try editor.deleteActive(bridge, &rig.doc, &rig.history);
    try testing.expectEqual(steps, rig.history.undo_stack.items.len);
}

test "delete: a right click or the Delete key removes the point and its child, undoable" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.build).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .shoot);
    try clickAt(&editor, &rig, onTile(4, 2));
    try clickAt(&editor, &rig, onTile(10, 6));
    try clickAt(&editor, &rig, onTile(14, 14));
    const slots = tools.firstOfClass(&rig.doc, tools.item_type.building_slots).?;
    const before = try rig.pointAt(.shoot_points, 2);
    // Free ground deletes nothing.
    try editor.deletePointAt(bridge, &rig.doc, &rig.history, onTile(30, 30));
    try testing.expectEqual(@as(usize, 3), try rig.pointCount(.shoot_points));
    // The first point goes; the active (third) point's index follows it down.
    try editor.deletePointAt(bridge, &rig.doc, &rig.history, onTile(4, 2));
    try testing.expectEqual(@as(usize, 2), try rig.pointCount(.shoot_points));
    try testing.expectEqual(@as(i32, 2), tools.childCount(&rig.doc, slots));
    try testing.expectEqual(@as(?usize, 1), editor.active);
    try testing.expectEqual(before.at.x, (try rig.pointAt(.shoot_points, 1)).at.x);
    // The Delete key removes the active one.
    try editor.deleteActive(bridge, &rig.doc, &rig.history);
    try testing.expectEqual(@as(usize, 1), try rig.pointCount(.shoot_points));
    try testing.expectEqual(@as(?usize, null), editor.active);
    try rig.undo();
    try rig.undo();
    try testing.expectEqual(@as(usize, 3), try rig.pointCount(.shoot_points));
    try testing.expectEqual(@as(i32, 3), tools.childCount(&rig.doc, slots));
    try rig.redo();
    try testing.expectEqual(@as(usize, 2), try rig.pointCount(.shoot_points));
}

test "move point and horizontal position: one undo step per drag, the grab offset kept, x fixed when horizontal" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.build).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .fire);
    try clickAt(&editor, &rig, onTile(4, 4));
    const home = try rig.pointAt(.fire_points, 0);
    const steps = rig.history.undo_stack.items.len;

    // Press off the point's centre by a few pixels: it does not jump.
    try editor.setTool(bridge, .move_point);
    const grab = Point2{ .x = onTile(4, 4).x + 4, .y = onTile(4, 4).y };
    try editor.press(bridge, grab, false);
    try editor.move(bridge, .{ .x = grab.x + 20, .y = grab.y + 10 });
    try editor.move(bridge, .{ .x = grab.x + 40, .y = grab.y + 20 });
    try editor.release(bridge, &rig.doc, &rig.history, .{ .x = grab.x + 40, .y = grab.y + 20 });
    try testing.expectEqual(steps + 1, rig.history.undo_stack.items.len);
    const moved = try rig.pointAt(.fire_points, 0);
    const delta = test_view.toGrid(.{ .x = 40, .y = 20 });
    const origin = test_view.toGrid(.{ .x = 0, .y = 0 });
    const want = worldToGrid(home.at);
    const got = worldToGrid(moved.at);
    try testing.expectApproxEqAbs(want.x + delta.x - origin.x, got.x, 0.05);
    try testing.expectApproxEqAbs(want.y + delta.y - origin.y, got.y, 0.05);
    try rig.undo();
    try testing.expectEqual(home.at.x, (try rig.pointAt(.fire_points, 0)).at.x);
    try rig.redo();

    // Horizontal: only the world y follows.
    try editor.setTool(bridge, .horizontal);
    const start = screenOfWorld(moved.at);
    try gesture(&editor, &rig, start, .{ .x = start.x + 30, .y = start.y - 30 });
    const slid = try rig.pointAt(.fire_points, 0);
    try testing.expectEqual(moved.at.x, slid.at.x);
    try testing.expect(slid.at.y != moved.at.y);
    try testing.expectEqual(steps + 2, rig.history.undo_stack.items.len);

    // A press off every point starts nothing; a click without a move records nothing.
    try editor.press(bridge, onTile(30, 30), false);
    try testing.expect(!editor.busy());
    try clickAt(&editor, &rig, start);
    try testing.expectEqual(steps + 2, rig.history.undo_stack.items.len);
}

test "angle mode: the direction handle and the cone handle drag, a press elsewhere selects, Escape puts it back" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.build).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .shoot);
    try clickAt(&editor, &rig, onTile(4, 4));
    try clickAt(&editor, &rig, onTile(20, 4));
    try editor.setTool(bridge, .angle);
    try testing.expectEqual(@as(?usize, 1), editor.active);
    try clickAt(&editor, &rig, onTile(4, 4));
    try testing.expectEqual(@as(?usize, 0), editor.active);
    const steps = rig.history.undo_stack.items.len;

    const home = try rig.pointAt(.shoot_points, 0);
    // Direction: the tip is dragged round to 90 degrees; the cone keeps its 80.
    try gesture(&editor, &rig, test_view.toScreen(handlesOf(home).direction), screenOfWorld(aimTip(home.at, 90)));
    var aimed = try rig.pointAt(.shoot_points, 0);
    try testing.expectEqual(@as(i32, 90), aimed.angle);
    try testing.expectEqual(@as(i32, 80), aimed.cone);
    // The tree child's own properties follow the list.
    try testing.expectEqual(steps + 1, rig.history.undo_stack.items.len);

    // Cone: the plus edge's tip dragged to 60 degrees off the direction makes it 120.
    try gesture(&editor, &rig, test_view.toScreen(handlesOf(aimed).cone_plus), screenOfWorld(aimTip(aimed.at, 90 + 60)));
    aimed = try rig.pointAt(.shoot_points, 0);
    try testing.expectEqual(@as(i32, 90), aimed.angle);
    try testing.expectEqual(@as(i32, 120), aimed.cone);
    try testing.expectEqual(steps + 2, rig.history.undo_stack.items.len);
    try rig.undo();
    try testing.expectEqual(@as(i32, 80), (try rig.pointAt(.shoot_points, 0)).cone);
    try rig.undo();
    try testing.expectEqual(@as(i32, 0), (try rig.pointAt(.shoot_points, 0)).angle);

    // Escape in the middle of a drag: nothing recorded, the list is as it was.
    const now = rig.history.undo_stack.items.len;
    const tip = test_view.toScreen(handlesOf(home).direction);
    try editor.press(bridge, tip, false);
    try editor.move(bridge, screenOfWorld(aimTip(home.at, 200)));
    try testing.expectEqual(@as(i32, 200), (try rig.pointAt(.shoot_points, 0)).angle);
    editor.cancel(bridge);
    try testing.expect(!editor.busy());
    try testing.expectEqual(@as(i32, 0), (try rig.pointAt(.shoot_points, 0)).angle);
    try testing.expectEqual(now, rig.history.undo_stack.items.len);
}

test "entrance: the click's world point, one undo step; set zero and Move reach the building too" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.build).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    try editor.setTool(bridge, .entrance);
    try clickAt(&editor, &rig, onTile(5, 7));
    const want = gridToWorld(tileCentre(5, 7));
    var entrance = try rig.grid(.entrance);
    try testing.expectApproxEqAbs(want.x, entrance.point2.x, 1e-3);
    try testing.expectApproxEqAbs(want.y, entrance.point2.y, 1e-3);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try rig.undo();
    entrance = try rig.grid(.entrance);
    try testing.expectEqual(@as(f32, 0), entrance.point2.x);
    try rig.redo();
    try editor.setTool(bridge, .set_zero);
    try clickAt(&editor, &rig, onTile(2, 2));
    try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
    try editor.setTool(bridge, .draw_grid);
    try clickAt(&editor, &rig, onTile(3, 3));
    try testing.expectEqual(@as(usize, 3), rig.history.undo_stack.items.len);
}

test "generate points: enabled in smoke and explosion modes only, one undo step each" {
    var rig = try Rig.building(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.res();
    var editor = GridEditor.init(testing.allocator, registrationFor(.build).?, rig.node);
    defer editor.deinit(bridge);
    editor.view = test_view;
    // A 4 x 4 locked footprint at tile (6, 6), the way the draw grid tool paints it.
    try editor.setTool(bridge, .draw_grid);
    try gesture(&editor, &rig, onTile(6, 6), onTile(6, 9));
    try gesture(&editor, &rig, onTile(7, 6), onTile(7, 9));
    try gesture(&editor, &rig, onTile(8, 6), onTile(8, 9));
    try gesture(&editor, &rig, onTile(9, 6), onTile(9, 9));
    const painted = rig.history.undo_stack.items.len;

    try testing.expect(!editor.canGenerate());
    try editor.setTool(bridge, .shoot);
    try testing.expect(!editor.canGenerate());
    try testing.expectError(error.Refused, editor.generate(bridge, &rig.doc, &rig.history));
    try editor.setTool(bridge, .draw_grid);
    try testing.expect(!editor.canGenerate());

    try editor.setTool(bridge, .smoke);
    try testing.expect(editor.canGenerate());
    try clickAt(&editor, &rig, onTile(25, 25));
    try testing.expectEqual(@as(usize, 1), try rig.pointCount(.smoke_points));
    try editor.generate(bridge, &rig.doc, &rig.history);
    // Half as many points as an edge has tiles, on all four edges, the old one gone.
    try testing.expectEqual(@as(usize, 8), try rig.pointCount(.smoke_points));
    const smokes = tools.firstOfClass(&rig.doc, tools.item_type.building_smokes).?;
    try testing.expectEqual(@as(i32, 8), tools.childCount(&rig.doc, smokes));
    try testing.expectEqual(painted + 2, rig.history.undo_stack.items.len);
    try rig.undo();
    try testing.expectEqual(@as(usize, 1), try rig.pointCount(.smoke_points));
    try rig.redo();

    // Explosions: moved to their places, once; the second press changes nothing.
    try editor.setTool(bridge, .dir_explosion);
    try testing.expect(editor.canGenerate());
    try editor.generate(bridge, &rig.doc, &rig.history);
    var five = try rig.points(.directed_explosion_points);
    defer five.deinit(testing.allocator);
    try testing.expectEqual(@as(i32, 225), five.aimed[4].angle);
    const steps = rig.history.undo_stack.items.len;
    try editor.generate(bridge, &rig.doc, &rig.history);
    try testing.expectEqual(steps, rig.history.undo_stack.items.len);
    try rig.undo();
    try testing.expectEqual(@as(i32, 0), (try rig.pointAt(.directed_explosion_points, 4)).angle);

    // An empty footprint cannot be generated from (MFC asserts there is a locked tile).
    var bare = try Rig.building(testing.allocator);
    defer bare.deinit(testing.allocator);
    var bare_editor = GridEditor.init(testing.allocator, registrationFor(.build).?, bare.node);
    defer bare_editor.deinit(bare.res());
    try bare_editor.setTool(bare.res(), .smoke);
    try testing.expectError(error.Refused, bare_editor.generate(bare.res(), &bare.doc, &bare.history));
}
