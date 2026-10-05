//! The point tools of the Building sub-editor (BuildFrm's shoot, fire, smoke
//! and directed-explosion modes, entrance and Generate points), kept free of
//! the kind so the bridge editor (S11) reuses them for its fire and smoke
//! points: a tool is given the geometry node and channel the points live in
//! and, where MFC keeps a tree child per point, the container node and the
//! child class.
//!
//! A building keeps every aimed point twice: its position, direction and cone
//! in the geometry channel, and a tree child (CBuildingSlotPropsItem and its
//! kin) that MFC adds when a point is placed and removes when one is deleted.
//! The bridge copies direction and cone to the same-index child on every
//! write of the list, so a tool that places or deletes a point emits the
//! child's insert or delete and the list's write as ONE composite, and an undo
//! puts both back. The five directed explosions are fixed children MFC never
//! inserts or deletes, so that mode has no place or delete.
//!
//! A drag writes the list through the bridge while it runs so the view
//! follows, and its release yields ONE geometry command from the list at the
//! press to the list at the release. A cancelled drag puts the list back and
//! yields nothing, so the history never sees it.
//!
//! Generate points ports GenerateSmokePoints and GenerateDirExpPoints as pure
//! functions of the passability tiles. MFC computes them in screen pixels of
//! the AI grid (origin (-622, 296), a tile step of (16, 8) and (16, -8)); the
//! functions return those screen positions and a `Frame` turns each into the
//! channel's own position, which the app resolves from the camera it uses.
//!
//! Geometry values are allocated with the bridge's allocator; a tool's
//! `allocator` must be that same allocator, as everywhere in the core.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const document_mod = @import("document.zig");
const history_mod = @import("history.zig");
const tools = @import("sub_editor_tools.zig");

const ResBridge = bridge_mod.ResBridge;
const EditError = bridge_mod.EditError;
const GeometryChannel = bridge_mod.GeometryChannel;
const GeometryValue = bridge_mod.GeometryValue;
const Point2 = bridge_mod.Point2;
const AimedPoint = bridge_mod.AimedPoint;
const Document = document_mod.Document;
const ResourceCommand = history_mod.ResourceCommand;
const item_type = tools.item_type;

/// The four aimed point modes of a building.
pub const Mode = enum { shoot, fire, smoke, dir_explosion };

/// Where one family of aimed points lives. `container` and `child_class` are
/// the tree item that holds the per-point children and their class; a
/// `fixed_children` target (the directed explosions) never gets a child
/// inserted or deleted.
pub const Target = struct {
    node: i32,
    channel: GeometryChannel,
    container: i32,
    child_class: i32,
    fixed_children: bool = false,
    /// MFC's angle and cone for a point placed with none to copy from.
    default_angle: i32 = 0,
    default_cone: i32 = 0,
    /// Whether a new point copies the active point's cone as well as its
    /// direction (a shoot point does, fire and smoke points copy the
    /// direction only).
    copies_cone: bool = false,
    /// Where the footprint the generate commands walk is read from, when it
    /// is not the target's own passability grid: a bridge's span part keeps
    /// its footprint as locked tiles.
    footprint_node: ?i32 = null,
    footprint_channel: GeometryChannel = .passability_cells,
};

/// The Target of `mode` on the building whose root is `root`, or null when
/// the tree lacks the container (not a building).
pub fn buildingTarget(doc: *const Document, root: i32, mode: Mode) ?Target {
    const container_class: i32 = switch (mode) {
        .shoot => item_type.building_slots,
        .fire => item_type.building_fire_points,
        .smoke => item_type.building_smokes,
        .dir_explosion => item_type.building_dir_explosions,
    };
    const container = tools.firstOfClass(doc, container_class) orelse return null;
    return switch (mode) {
        // BuildShoot.cpp AddOrSelectShootPoint: cone 80, direction 0.
        .shoot => .{ .node = root, .channel = .shoot_points, .container = container, .child_class = item_type.building_slot_props, .default_cone = 80, .copies_cone = true },
        .fire => .{ .node = root, .channel = .fire_points, .container = container, .child_class = item_type.building_fire_point_props },
        .smoke => .{ .node = root, .channel = .smoke_points, .container = container, .child_class = item_type.building_smoke_props },
        .dir_explosion => .{ .node = root, .channel = .directed_explosion_points, .container = container, .child_class = item_type.building_dir_explosion_props, .fixed_children = true },
    };
}

/// The Target of `mode` on the bridge whose root is `root`, or null when the
/// tree lacks the container or a span part. CBridgeFrame has fire and smoke
/// points and the directed explosions, no shoot points. Its generate commands
/// walk the first span part's locked tiles.
pub fn bridgeTarget(doc: *const Document, root: i32, mode: Mode) ?Target {
    const container_class: i32 = switch (mode) {
        .shoot => return null,
        .fire => item_type.bridge_fire_points,
        .smoke => item_type.bridge_smokes,
        .dir_explosion => item_type.bridge_dir_explosions,
    };
    const container = tools.firstOfClass(doc, container_class) orelse return null;
    const part = tools.firstOfClass(doc, item_type.bridge_parts) orelse return null;
    const base: Target = .{ .node = root, .channel = .fire_points, .container = container, .child_class = 0, .footprint_node = part, .footprint_channel = .locked_tiles };
    return switch (mode) {
        .shoot => null,
        .fire => with(base, .fire_points, item_type.bridge_fire_point_props, false),
        .smoke => with(base, .smoke_points, item_type.bridge_smoke_props, false),
        // The five explosions are fixed, as a building's are.
        .dir_explosion => with(base, .directed_explosion_points, item_type.bridge_dir_explosion_props, true),
    };
}

fn with(base: Target, channel: GeometryChannel, child_class: i32, fixed: bool) Target {
    var target = base;
    target.channel = channel;
    target.child_class = child_class;
    target.fixed_children = fixed;
    return target;
}

/// A geometry command from the list as it was read to `after_points`.
fn listCommand(before: GeometryValue, after_points: []AimedPoint, target: Target) ResourceCommand {
    return .{ .geometry = .{ .node = target.node, .channel = target.channel, .before = before, .after = .{ .aimed = after_points } } };
}

/// MFC's AddOrSelectShootPoint (and the fire and smoke kin) on an empty spot:
/// a new tree child at the end of the container and a new point at the end of
/// the list, as one undo step. `copy_from` is the active point's index; its
/// direction (and for a shoot point its cone) is copied, else the mode's
/// defaults apply.
pub fn placePoint(allocator: std.mem.Allocator, doc: *const Document, bridge: ResBridge, target: Target, at: Point2, copy_from: ?usize) EditError!ResourceCommand {
    if (target.fixed_children) return error.Refused;
    var before = try tools.readGeometry(bridge, target.node, target.channel);
    errdefer before.deinit(allocator);
    const old = before.aimed;
    if (copy_from) |index| if (index >= old.len) return error.BadArgument;

    var angle = target.default_angle;
    var cone = target.default_cone;
    if (copy_from) |index| {
        angle = old[index].angle;
        if (target.copies_cone) cone = old[index].cone;
    }
    const grown = try allocator.alloc(AimedPoint, old.len + 1);
    errdefer allocator.free(grown);
    @memcpy(grown[0..old.len], old);
    grown[old.len] = .{ .at = at, .angle = angle, .cone = cone };

    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    errdefer {
        for (steps.items) |*step| step.deinit(allocator);
        steps.deinit(allocator);
    }
    try steps.ensureTotalCapacity(allocator, 2);
    steps.appendAssumeCapacity(try tools.appendChild(allocator, doc, target.container, target.child_class));
    steps.appendAssumeCapacity(listCommand(before, grown, target));
    return .{ .composite = .{ .steps = steps } };
}

/// MFC's DeleteShootPoint (and kin): the point `index` and its tree child go
/// together, as one undo step.
pub fn deletePoint(allocator: std.mem.Allocator, doc: *const Document, bridge: ResBridge, target: Target, index: usize) EditError!ResourceCommand {
    if (target.fixed_children) return error.Refused;
    var before = try tools.readGeometry(bridge, target.node, target.channel);
    errdefer before.deinit(allocator);
    const old = before.aimed;
    if (index >= old.len) return error.BadArgument;
    const child = tools.childOfClass(doc, target.container, target.child_class, index) orelse return error.Refused;

    const shrunk = try allocator.alloc(AimedPoint, old.len - 1);
    errdefer allocator.free(shrunk);
    @memcpy(shrunk[0..index], old[0..index]);
    @memcpy(shrunk[index..], old[index + 1 ..]);

    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    errdefer {
        for (steps.items) |*step| step.deinit(allocator);
        steps.deinit(allocator);
    }
    try steps.ensureTotalCapacity(allocator, 2);
    steps.appendAssumeCapacity(listCommand(before, shrunk, target));
    steps.appendAssumeCapacity(try tools.deleteNode(doc, child));
    return .{ .composite = .{ .steps = steps } };
}

/// What a drag changes of the point it holds.
pub const Part = enum {
    /// The point itself (E_SUB_MOVE: MoveShootPoint and kin).
    move,
    /// The point along its horizontal line (E_SUB_HOR: SetHorShootPoint): the
    /// x stays where it was, only y follows.
    horizontal,
    /// The direction handle alone.
    direction,
    /// The cone handle alone.
    cone,
    /// Direction and cone together, as one drag of the angle lines
    /// (E_SUB_DIR: SetShootPointAngle).
    aim,
};

/// What the pointer says now: only the fields the part uses are read.
pub const Sample = struct { at: Point2 = .{}, angle: i32 = 0, cone: i32 = 0 };

fn applySample(point: *AimedPoint, from: AimedPoint, part: Part, sample: Sample) void {
    switch (part) {
        .move => point.at = sample.at,
        .horizontal => point.at = .{ .x = from.at.x, .y = sample.at.y },
        .direction => point.angle = sample.angle,
        .cone => point.cone = sample.cone,
        .aim => {
            point.angle = sample.angle;
            point.cone = sample.cone;
        },
    }
}

/// A drag of one point (or one of its handles): press, any number of moves,
/// then `finish` for the one command or `cancel`.
pub const PointDrag = struct {
    target: Target,
    index: usize,
    part: Part,
    before: GeometryValue,
    current: []AimedPoint,
    moved: bool = false,

    /// Reads the list at the press. A point that is not there is
    /// BK_EDITOR_BAD_ARGUMENT.
    pub fn begin(allocator: std.mem.Allocator, bridge: ResBridge, target: Target, index: usize, part: Part) EditError!PointDrag {
        var read = try tools.readGeometry(bridge, target.node, target.channel);
        errdefer read.deinit(allocator);
        if (index >= read.aimed.len) return error.BadArgument;
        const current = try allocator.dupe(AimedPoint, read.aimed);
        return .{ .target = target, .index = index, .part = part, .before = read, .current = current };
    }

    /// One mouse move: the point follows and the list is written through the
    /// bridge.
    pub fn move(self: *PointDrag, bridge: ResBridge, sample: Sample) EditError!void {
        applySample(&self.current[self.index], self.before.aimed[self.index], self.part, sample);
        self.moved = true;
        const live: GeometryValue = .{ .aimed = self.current };
        try bridge_mod.check(bridge.geometryWrite(self.target.node, self.target.channel, &live));
    }

    /// The release: the drag's one command, or null when the point ended
    /// where it began (a click). The drag is spent either way.
    pub fn finish(self: *PointDrag, allocator: std.mem.Allocator) ?ResourceCommand {
        if (!self.moved or std.mem.eql(u8, std.mem.sliceAsBytes(self.before.aimed), std.mem.sliceAsBytes(self.current))) {
            self.deinit(allocator);
            return null;
        }
        const command: ResourceCommand = .{ .geometry = .{
            .node = self.target.node,
            .channel = self.target.channel,
            .before = self.before,
            .after = .{ .aimed = self.current },
        } };
        self.before = .{ .aimed = &.{} };
        self.current = &.{};
        return command;
    }

    /// Escape or a right click during a drag: the list goes back.
    pub fn cancel(self: *PointDrag, allocator: std.mem.Allocator, bridge: ResBridge) void {
        if (self.moved) _ = bridge.geometryWrite(self.target.node, self.target.channel, &self.before);
        self.deinit(allocator);
    }

    pub fn deinit(self: *PointDrag, allocator: std.mem.Allocator) void {
        self.before.deinit(allocator);
        if (self.current.len != 0) allocator.free(self.current);
        self.before = .{ .aimed = &.{} };
        self.current = &.{};
    }
};

/// A one-shot edit of one point (a typed value or a click on a handle): the
/// same change a drag of `part` ends with, as one command. Null when nothing
/// would change.
pub fn editPoint(allocator: std.mem.Allocator, bridge: ResBridge, target: Target, index: usize, part: Part, sample: Sample) EditError!?ResourceCommand {
    var drag = try PointDrag.begin(allocator, bridge, target, index, part);
    errdefer drag.deinit(allocator);
    applySample(&drag.current[index], drag.before.aimed[index], part, sample);
    drag.moved = true;
    return drag.finish(allocator);
}

/// SetEntrance: BuildFrm's entrance click. MFC stores it as one tile; the
/// bridge's entrance channel is the point the building's entrance sits on.
pub fn setEntrance(allocator: std.mem.Allocator, bridge: ResBridge, node: i32, to: Point2) EditError!ResourceCommand {
    return tools.setGeometry(allocator, bridge, node, .entrance, .{ .point2 = to });
}

// --- Bridge span marks -----------------------------------------------------------

/// CBridgeFrame's 'Bridge type' (SetBridgeType): the bridge runs along the red
/// line either down-right (horizontal) or up-right (vertical).
pub const BridgeType = enum { horizontal, vertical };

/// The four marks of the span-mark tool. Begin and End sit on the line; Front
/// and Back are offsets of a span from its centre cross, across the line.
pub const SpanMark = enum { begin, end, front, back };

// BridgeFrm.cpp SetBridgeType: the line starts at grid index 16 (horizontal) or
// 14 (vertical) and runs 1000 pixels right and 500 down or up.
const span_line_width: f32 = 1000;
const span_line_rise: f32 = 500;

/// CBridgeFrame::GetPointOnLine: the line's y at screen x.
pub fn spanLineY(kind: BridgeType, x: f32) f32 {
    const x1: f32 = grid_ox + cell_x * @as(f32, if (kind == .horizontal) 16 else 14);
    const y1: f32 = if (kind == .horizontal) grid_oy - cell_y * 16 else grid_oy + cell_y * 14;
    const y2: f32 = if (kind == .horizontal) y1 + span_line_rise else y1 - span_line_rise;
    return y1 + (x - x1) * (y2 - y1) / span_line_width;
}

// The frame's constructor value for Begin and End (BridgeFrm.cpp:91).
const world_cell: f32 = 16 * 2.0 * 1.41421356;
const default_span_mark: Point2 = .{ .x = 16 * world_cell - 300, .y = 16 * world_cell };

/// Moves one span mark as one undo step. The geometry is MFC's own_data in the
/// order Begin, End and (Front, Back as one point): a Begin or End drag keeps
/// the x of the pointer and takes the y of the line for the bridge type; a
/// Front or Back drag stores the pointer's offset from `papa` (the span's
/// Begin, End or centre cross) across the line, y for a horizontal bridge and
/// x for a vertical one. The other marks, a stale Front and Back included, stay
/// as they are. Null when the mark does not change.
pub fn setSpanMark(allocator: std.mem.Allocator, bridge: ResBridge, node: i32, mark: SpanMark, to: Point2, kind: BridgeType, papa: Point2) EditError!?ResourceCommand {
    var before = try tools.readGeometry(bridge, node, .bridge_span_marks);
    defer before.deinit(allocator);
    var marks: [3]Point2 = .{ default_span_mark, default_span_mark, .{} };
    if (before.points2.len == 3) @memcpy(&marks, before.points2) else if (before.points2.len != 0) return error.BadArgument;
    switch (mark) {
        .begin => marks[0] = .{ .x = to.x, .y = spanLineY(kind, to.x) },
        .end => marks[1] = .{ .x = to.x, .y = spanLineY(kind, to.x) },
        .front => marks[2].x = if (kind == .horizontal) to.y - papa.y else to.x - papa.x,
        .back => marks[2].y = if (kind == .horizontal) to.y - papa.y else to.x - papa.x,
    }
    if (before.points2.len == 3 and std.mem.eql(u8, std.mem.sliceAsBytes(before.points2), std.mem.sliceAsBytes(&marks))) return null;
    return try tools.setGeometry(allocator, bridge, node, .bridge_span_marks, .{ .points2 = &marks });
}

// --- Generate points -----------------------------------------------------------

// GridFrm.cpp's fixed grid: origin and cell size in screen pixels.
const grid_ox: f32 = -622;
const grid_oy: f32 = 296;
const cell_x: f32 = 32;
const cell_y: f32 = 16;

/// The tiles of a passability grid in the tile frame: the bounding box of the
/// set cells, and whether a tile is one.
pub const Footprint = struct {
    grid: []const u8,
    width: usize,
    min_x: i32,
    min_y: i32,
    max_x: i32,
    max_y: i32,

    /// Null for a grid with no set cell (MFC asserts there is a locked tile).
    pub fn of(cells: []const u8, width: usize, height: usize) ?Footprint {
        var found: ?Footprint = null;
        var y: usize = 0;
        while (y < height) : (y += 1) {
            var x: usize = 0;
            while (x < width) : (x += 1) {
                if (cells[y * width + x] == 0) continue;
                const tx: i32 = @intCast(x);
                const ty: i32 = @intCast(y);
                if (found) |*box| {
                    box.min_x = @min(box.min_x, tx);
                    box.max_x = @max(box.max_x, tx);
                    box.min_y = @min(box.min_y, ty);
                    box.max_y = @max(box.max_y, ty);
                } else found = .{ .grid = cells, .width = width, .min_x = tx, .min_y = ty, .max_x = tx, .max_y = ty };
            }
        }
        return found;
    }

    fn isSet(self: Footprint, x: i32, y: i32) bool {
        if (x < 0 or y < 0) return false;
        const ux: usize = @intCast(x);
        const uy: usize = @intCast(y);
        if (ux >= self.width or uy * self.width + ux >= self.grid.len) return false;
        return self.grid[uy * self.width + ux] != 0;
    }

    /// CBuildingFrame::IsTileLocked: the tile under a screen pixel, by the
    /// same truncating conversions as ComputeGameTileCoordinates.
    fn lockedAt(self: Footprint, px: f32, py: f32) bool {
        const sx: i32 = @intFromFloat(px);
        const sy: i32 = @intFromFloat(py);
        const alpha: f32 = std.math.asin(@as(f32, 1.0) / @sqrt(@as(f32, 5.0)));
        const size: f32 = @sqrt((cell_x / 2) * (cell_x / 2) + (cell_y / 2) * (cell_y / 2));
        const dx = grid_ox - @as(f32, @floatFromInt(sx));
        const dy = grid_oy - @as(f32, @floatFromInt(sy));
        const om = @sqrt(dx * dx + dy * dy);
        const beta = std.math.atan2(@as(f32, @floatFromInt(sy)) - grid_oy, @as(f32, @floatFromInt(sx)) - grid_ox);
        const temp = om / @sin(2 * alpha);
        const oc = temp * @sin(alpha + beta);
        const cm = temp * @sin(alpha - beta);
        return self.isSet(@intFromFloat(oc / size), @intFromFloat(cm / size));
    }
};

/// The four corners of the footprint in screen pixels, named as MFC names
/// them (left is the minimum tile's leftmost corner, top the corner of the
/// minimum x and maximum y tile, and so on).
const Corners = struct { left: Point2, top: Point2, right: Point2, bottom: Point2 };

fn tileCorner2(tx: i32, ty: i32) Point2 {
    return .{
        .x = grid_ox + (cell_x / 2) * @as(f32, @floatFromInt(tx + ty)),
        .y = grid_oy + (cell_y / 2) * @as(f32, @floatFromInt(tx - ty)),
    };
}

fn cornersOf(box: Footprint) Corners {
    const c1 = tileCorner2(box.min_x, box.max_y + 1);
    const c3 = tileCorner2(box.max_x + 1, box.min_y);
    const c4 = tileCorner2(box.max_x + 1, box.max_y + 1);
    return .{ .left = tileCorner2(box.min_x, box.min_y), .top = c1, .right = c4, .bottom = c3 };
}

/// How a generated screen position becomes the channel's own: the app passes
/// the conversion of the camera it views through, tests the identity.
pub const Frame = struct {
    context: ?*anyopaque = null,
    toChannel: *const fn (context: ?*anyopaque, screen: Point2) Point2 = identity,

    fn identity(_: ?*anyopaque, screen: Point2) Point2 {
        return screen;
    }
};

fn mid(a: Point2, b: Point2) Point2 {
    return .{ .x = (a.x + b.x) / 2, .y = (a.y + b.y) / 2 };
}

/// MFC's GenerateSmokePoints: along each of the four edges of the footprint,
/// half as many points as the edge has tiles, each one pointing outwards
/// (180, 270, 0 and 90 degrees), inset a quarter cell. Cones are 0.
pub fn generateSmoke(allocator: std.mem.Allocator, box: Footprint, frame: Frame) ![]AimedPoint {
    const corners = cornersOf(box);
    const nx: usize = @intCast(@divTrunc(box.max_x - box.min_x + 1, 2));
    const ny: usize = @intCast(@divTrunc(box.max_y - box.min_y + 1, 2));
    var out = try allocator.alloc(AimedPoint, 2 * nx + 2 * ny);
    errdefer allocator.free(out);
    var n: usize = 0;
    var i: usize = 0;
    while (i < nx) : (i += 1) {
        const f: f32 = @floatFromInt(i);
        out[n] = .{ .at = frame.toChannel(frame.context, .{ .x = corners.left.x + f * cell_x + cell_x / 4, .y = corners.left.y + f * cell_y + cell_y / 4 }), .angle = 180 };
        n += 1;
    }
    i = 0;
    while (i < ny) : (i += 1) {
        const f: f32 = @floatFromInt(i);
        out[n] = .{ .at = frame.toChannel(frame.context, .{ .x = corners.bottom.x + f * cell_x + cell_x / 4, .y = corners.bottom.y - f * cell_y - cell_y / 4 }), .angle = 270 };
        n += 1;
    }
    i = 0;
    while (i < nx) : (i += 1) {
        const f: f32 = @floatFromInt(i);
        out[n] = .{ .at = frame.toChannel(frame.context, .{ .x = corners.right.x - f * cell_x - cell_x / 4, .y = corners.right.y - f * cell_y - cell_y / 4 }), .angle = 0 };
        n += 1;
    }
    i = 0;
    while (i < ny) : (i += 1) {
        const f: f32 = @floatFromInt(i);
        out[n] = .{ .at = frame.toChannel(frame.context, .{ .x = corners.top.x - f * cell_x - cell_x / 4, .y = corners.top.y + f * cell_y + cell_y / 4 }), .angle = 90 };
        n += 1;
    }
    return out;
}

/// One side of GenerateDirExpPoints: start at the middle of an edge and walk
/// inwards `steps` half-cells until a locked tile is under the probe, then
/// the point sits where the probe stopped. The probe is a whole pixel, as
/// MFC's POINT is.
fn walk(box: Footprint, start: Point2, probe_dy: f32, step_x: f32, step_y: f32, steps: i32) Point2 {
    var px: f32 = @trunc(start.x);
    var py: f32 = @trunc(start.y + probe_dy);
    var k: i32 = 0;
    while (k < steps) : (k += 1) {
        if (box.lockedAt(px, py)) break;
        px = @trunc(px + step_x);
        py = @trunc(py + step_y);
    }
    if (k == steps) return start;
    const f: f32 = @floatFromInt(k);
    return .{ .x = start.x + f * step_x, .y = start.y + f * step_y };
}

/// MFC's GenerateDirExpPoints: the five explosions, one on each edge's middle
/// walked inwards to the first locked tile (directions 180, 270, 0 and 90) and
/// one at the footprint's centre (225). `cones` are the cones the explosions
/// hold now, kept as they are (MFC does not touch the vertical angle).
pub fn generateDirExp(box: Footprint, frame: Frame, cones: [5]i32) [5]AimedPoint {
    const corners = cornersOf(box);
    const half_x = cell_x / 2;
    const half_y = cell_y / 2;
    const width = box.max_x - box.min_x;
    const depth = box.max_y - box.min_y;
    const spots = [5]Point2{
        walk(box, mid(corners.left, corners.bottom), -cell_y / 4, half_x, -half_y, depth),
        walk(box, mid(corners.right, corners.bottom), -cell_y / 4, -half_x, -half_y, width),
        walk(box, mid(corners.top, corners.right), cell_y / 4, -half_x, half_y, depth),
        walk(box, mid(corners.left, corners.top), cell_y / 4, half_x, half_y, width),
        mid(corners.left, corners.right),
    };
    const angles = [5]i32{ 180, 270, 0, 90, 225 };
    var out: [5]AimedPoint = undefined;
    for (&out, spots, angles, cones) |*point, spot, angle, cone| {
        point.* = .{ .at = frame.toChannel(frame.context, spot), .angle = angle, .cone = cone };
    }
    return out;
}

/// OnGeneratePoints in smoke mode: the smoke points are replaced by the
/// generated ones (MFC asks first when there are some; that confirmation is
/// the caller's), children and list in one undo step. `passability` is the
/// building's passability channel as the bridge reads it.
pub fn generateSmokePoints(allocator: std.mem.Allocator, doc: *const Document, bridge: ResBridge, target: Target, frame: Frame) EditError!ResourceCommand {
    if (target.fixed_children) return error.Refused;
    var pass = try tools.readGeometry(bridge, target.footprint_node orelse target.node, target.footprint_channel);
    defer pass.deinit(allocator);
    const grid = pass.bytes_grid;
    const box = Footprint.of(grid.bytes, @intCast(grid.width), @intCast(grid.height)) orelse return error.Refused;
    const generated = try generateSmoke(allocator, box, frame);
    errdefer allocator.free(generated);
    var before = try tools.readGeometry(bridge, target.node, target.channel);
    errdefer before.deinit(allocator);

    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    errdefer {
        for (steps.items) |*step| step.deinit(allocator);
        steps.deinit(allocator);
    }
    // The children go from the last to the first so each delete's place is
    // still right when its turn comes.
    var existing: usize = 0;
    for (doc.tree.nodes.items) |node| {
        if (node.parent == target.container) existing += 1;
    }
    try steps.ensureTotalCapacity(allocator, existing + generated.len + 1);
    var n = existing;
    while (n > 0) {
        n -= 1;
        const child = tools.childOfClass(doc, target.container, target.child_class, n) orelse return error.Refused;
        steps.appendAssumeCapacity(try tools.deleteNode(doc, child));
    }
    for (generated, 0..) |_, i| {
        steps.appendAssumeCapacity(try tools.insertChild(allocator, target.container, target.child_class, @intCast(i)));
    }
    steps.appendAssumeCapacity(listCommand(before, generated, target));
    return .{ .composite = .{ .steps = steps } };
}

/// OnGeneratePoints in directed-explosion mode: the five fixed explosions move
/// to their generated places. Null when they are there already.
pub fn generateDirExpPoints(allocator: std.mem.Allocator, bridge: ResBridge, target: Target, frame: Frame) EditError!?ResourceCommand {
    var pass = try tools.readGeometry(bridge, target.footprint_node orelse target.node, target.footprint_channel);
    defer pass.deinit(allocator);
    const grid = pass.bytes_grid;
    const box = Footprint.of(grid.bytes, @intCast(grid.width), @intCast(grid.height)) orelse return error.Refused;
    var before = try tools.readGeometry(bridge, target.node, target.channel);
    errdefer before.deinit(allocator);
    if (before.aimed.len != 5) return error.Refused;
    var cones: [5]i32 = undefined;
    for (&cones, before.aimed) |*cone, point| cone.* = point.cone;
    const generated = generateDirExp(box, frame, cones);
    const after = try allocator.dupe(AimedPoint, &generated);
    errdefer allocator.free(after);
    if (std.mem.eql(u8, std.mem.sliceAsBytes(before.aimed), std.mem.sliceAsBytes(after))) {
        allocator.free(after);
        before.deinit(allocator);
        return null;
    }
    return listCommand(before, after, target);
}

// --- Tests -------------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = @import("fake_bridge.zig").FakeResBridge;
const History = history_mod.History;

const Rig = struct {
    allocator: std.mem.Allocator,
    fake: FakeResBridge,
    doc: Document = .{},
    history: History = .{},
    root: i32 = 0,
    /// The first span part of a bridge rig (its locked tiles are the footprint).
    part: i32 = 0,
    on_bridge: bool = false,

    /// A bridge with CBridgeFrame's three point containers (the directed
    /// explosions' five fixed children among them) and one span part.
    fn initBridge(allocator: std.mem.Allocator) !Rig {
        var rig: Rig = .{ .allocator = allocator, .fake = FakeResBridge.init(allocator), .on_bridge = true };
        try bridge_mod.check(rig.fake.bridge().new(.bridge));
        rig.root = rig.fake.nodes.items[0].id;
        const classes = [_]i32{ item_type.bridge_parts, item_type.bridge_fire_points, item_type.bridge_smokes, item_type.bridge_dir_explosions };
        var explosions: i32 = -1;
        for (classes, 0..) |class, i| {
            var id: i32 = -1;
            var name: [16]u8 = undefined;
            const text = try std.fmt.bufPrint(&name, "{d}", .{class});
            try bridge_mod.check(rig.fake.bridge().insertNode(rig.root, text, @intCast(i), &id));
            if (class == item_type.bridge_parts) rig.part = id;
            if (class == item_type.bridge_dir_explosions) explosions = id;
        }
        var name: [16]u8 = undefined;
        const child = try std.fmt.bufPrint(&name, "{d}", .{item_type.bridge_dir_explosion_props});
        var n: i32 = 0;
        while (n < 5) : (n += 1) {
            var id: i32 = -1;
            try bridge_mod.check(rig.fake.bridge().insertNode(explosions, child, n, &id));
        }
        const five = [_]AimedPoint{.{}} ** 5;
        try bridge_mod.check(rig.fake.bridge().geometryWrite(rig.root, .directed_explosion_points, &.{ .aimed = @constCast(&five) }));
        try rig.doc.reload(allocator, rig.fake.bridge());
        return rig;
    }

    /// A building with the four containers the editor's tree has, the
    /// directed explosions' five fixed children among them.
    fn init(allocator: std.mem.Allocator) !Rig {
        var rig: Rig = .{ .allocator = allocator, .fake = FakeResBridge.init(allocator) };
        try bridge_mod.check(rig.fake.bridge().new(.build));
        rig.root = rig.fake.nodes.items[0].id;
        const classes = [_]i32{ item_type.building_slots, item_type.building_fire_points, item_type.building_smokes, item_type.building_dir_explosions };
        var explosions: i32 = -1;
        for (classes, 0..) |class, i| {
            var id: i32 = -1;
            var name: [16]u8 = undefined;
            const text = try std.fmt.bufPrint(&name, "{d}", .{class});
            try bridge_mod.check(rig.fake.bridge().insertNode(rig.root, text, @intCast(i), &id));
            if (class == item_type.building_dir_explosions) explosions = id;
        }
        var name: [16]u8 = undefined;
        const child = try std.fmt.bufPrint(&name, "{d}", .{item_type.building_dir_explosion_props});
        var n: i32 = 0;
        while (n < 5) : (n += 1) {
            var id: i32 = -1;
            try bridge_mod.check(rig.fake.bridge().insertNode(explosions, child, n, &id));
        }
        const five = [_]AimedPoint{.{}} ** 5;
        try bridge_mod.check(rig.fake.bridge().geometryWrite(rig.root, .directed_explosion_points, &.{ .aimed = @constCast(&five) }));
        try rig.doc.reload(allocator, rig.fake.bridge());
        return rig;
    }

    fn deinit(self: *Rig) void {
        self.history.deinit(self.allocator);
        self.doc.deinit(self.allocator);
        self.fake.deinit();
    }

    fn bridge(self: *Rig) ResBridge {
        return self.fake.bridge();
    }

    fn target(self: *Rig, mode: Mode) Target {
        if (self.on_bridge) return bridgeTarget(&self.doc, self.root, mode).?;
        return buildingTarget(&self.doc, self.root, mode).?;
    }

    fn commitCommand(self: *Rig, command: ResourceCommand) !void {
        try tools.commit(self.allocator, self.bridge(), &self.doc, &self.history, command, 0);
    }

    fn undo(self: *Rig) !void {
        try self.history.redo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.undo_stack.pop().?;
        try self.doc.undoOne(self.allocator, self.bridge(), &entry.command);
        self.history.redo_stack.appendAssumeCapacity(entry);
    }

    fn redo(self: *Rig) !void {
        try self.history.undo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.redo_stack.pop().?;
        try self.doc.redoOne(self.allocator, self.bridge(), &entry.command);
        self.history.undo_stack.appendAssumeCapacity(entry);
    }

    fn expectPoints(self: *Rig, t: Target, want: []const AimedPoint) !void {
        var read = try tools.readGeometry(self.bridge(), t.node, t.channel);
        defer read.deinit(self.allocator);
        try testing.expectEqualSlices(AimedPoint, want, read.aimed);
    }

    fn expectChildren(self: *Rig, t: Target, want: i32) !void {
        try testing.expectEqual(want, tools.childCount(&self.doc, t.container));
    }

    /// The value of a child's property by MFC's name, as text.
    fn childProp(self: *Rig, t: Target, nth: usize, name: []const u8) ![]const u8 {
        const child = tools.childOfClass(&self.doc, t.container, t.child_class, nth).?;
        return tools.propValue(&self.doc, child, tools.propIdByName(&self.doc, child, name).?).?;
    }

    fn setPassability(self: *Rig, cells: []const u8, w: i32, h: i32) !void {
        try bridge_mod.check(self.bridge().geometryWrite(self.root, .passability_cells, &.{ .bytes_grid = .{ .bytes = @constCast(cells), .width = w, .height = h } }));
    }
};

const pt = struct {
    fn at(x: f32, y: f32) Point2 {
        return .{ .x = x, .y = y };
    }
};

/// Place two points of `mode` (the second copies from the first), delete the
/// first, and check the list and the child count before, after, after undo and
/// after redo of each step.
fn placeAndDeleteCase(mode: Mode, default_angle: i32, default_cone: i32, copied_cone: i32) !void {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit();
    try placeAndDeleteOn(&rig, mode, default_angle, default_cone, copied_cone);
}

fn placeAndDeleteOn(rig: *Rig, mode: Mode, default_angle: i32, default_cone: i32, copied_cone: i32) !void {
    const t = rig.target(mode);
    try rig.expectPoints(t, &.{});
    try rig.expectChildren(t, 0);

    try rig.commitCommand(try placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(10, 20), null));
    const first: AimedPoint = .{ .at = pt.at(10, 20), .angle = default_angle, .cone = default_cone };
    try rig.expectPoints(t, &.{first});
    try rig.expectChildren(t, 1);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectPoints(t, &.{});
    try rig.expectChildren(t, 0);
    try rig.redo();
    try rig.expectPoints(t, &.{first});
    try rig.expectChildren(t, 1);

    // The first point turned by a handle drag, then a second point that copies its direction.
    var drag = try PointDrag.begin(testing.allocator, rig.bridge(), t, 0, .aim);
    try drag.move(rig.bridge(), .{ .angle = 90, .cone = 40 });
    try rig.commitCommand(drag.finish(testing.allocator).?);
    const turned: AimedPoint = .{ .at = pt.at(10, 20), .angle = 90, .cone = 40 };
    try rig.commitCommand(try placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(30, 40), 0));
    const second: AimedPoint = .{ .at = pt.at(30, 40), .angle = 90, .cone = copied_cone };
    try rig.expectPoints(t, &.{ turned, second });
    try rig.expectChildren(t, 2);
    // The bridge copied direction and cone to the child of the same index.
    try testing.expectEqualStrings("90", try rig.childProp(t, 1, "Direction"));

    try rig.commitCommand(try deletePoint(testing.allocator, &rig.doc, rig.bridge(), t, 0));
    try rig.expectPoints(t, &.{second});
    try rig.expectChildren(t, 1);
    try testing.expectEqual(@as(usize, 4), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectPoints(t, &.{ turned, second });
    try rig.expectChildren(t, 2);
    try rig.redo();
    try rig.expectPoints(t, &.{second});
    try rig.expectChildren(t, 1);

    try testing.expectError(error.BadArgument, deletePoint(testing.allocator, &rig.doc, rig.bridge(), t, 1));
    try testing.expectError(error.BadArgument, placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(0, 0), 5));
}

test "shoot points are placed with cone 80 and copy the active point's direction and cone" {
    try placeAndDeleteCase(.shoot, 0, 80, 40);
}

test "fire points are placed with direction 0 and copy the active point's direction" {
    try placeAndDeleteCase(.fire, 0, 0, 0);
}

test "smoke points are placed with direction 0 and copy the active point's direction" {
    try placeAndDeleteCase(.smoke, 0, 0, 0);
}

test "directed explosions have no place and no delete" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit();
    const t = rig.target(.dir_explosion);
    try testing.expectError(error.Refused, placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(1, 1), null));
    try testing.expectError(error.Refused, deletePoint(testing.allocator, &rig.doc, rig.bridge(), t, 0));
    try rig.expectChildren(t, 5);
}

test "a drag of each part is one undo step and leaves the other fields alone" {
    const cases = [_]struct { part: Part, sample: Sample, want: AimedPoint }{
        .{ .part = .move, .sample = .{ .at = pt.at(15, 25), .angle = 7, .cone = 7 }, .want = .{ .at = pt.at(15, 25), .angle = 0, .cone = 80 } },
        .{ .part = .horizontal, .sample = .{ .at = pt.at(99, 33), .angle = 7, .cone = 7 }, .want = .{ .at = pt.at(10, 33), .angle = 0, .cone = 80 } },
        .{ .part = .direction, .sample = .{ .at = pt.at(99, 99), .angle = 270, .cone = 7 }, .want = .{ .at = pt.at(10, 20), .angle = 270, .cone = 80 } },
        .{ .part = .cone, .sample = .{ .at = pt.at(99, 99), .angle = 7, .cone = 25 }, .want = .{ .at = pt.at(10, 20), .angle = 0, .cone = 25 } },
        .{ .part = .aim, .sample = .{ .at = pt.at(99, 99), .angle = 135, .cone = 60 }, .want = .{ .at = pt.at(10, 20), .angle = 135, .cone = 60 } },
    };
    for (cases) |case| {
        var rig = try Rig.init(testing.allocator);
        defer rig.deinit();
        const t = rig.target(.shoot);
        try rig.commitCommand(try placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(10, 20), null));
        const start: AimedPoint = .{ .at = pt.at(10, 20), .angle = 0, .cone = 80 };

        var drag = try PointDrag.begin(testing.allocator, rig.bridge(), t, 0, case.part);
        // Several moves still make one step: the last sample wins.
        try drag.move(rig.bridge(), .{ .at = pt.at(1, 1), .angle = 1, .cone = 1 });
        try drag.move(rig.bridge(), case.sample);
        try rig.commitCommand(drag.finish(testing.allocator).?);
        try rig.expectPoints(t, &.{case.want});
        try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
        try rig.undo();
        try rig.expectPoints(t, &.{start});
        try rig.redo();
        try rig.expectPoints(t, &.{case.want});
    }
}

test "a cancelled drag and a click leave no history entry and the list as it was" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit();
    const t = rig.target(.smoke);
    try rig.commitCommand(try placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(10, 20), null));
    const start: AimedPoint = .{ .at = pt.at(10, 20) };

    var drag = try PointDrag.begin(testing.allocator, rig.bridge(), t, 0, .move);
    try drag.move(rig.bridge(), .{ .at = pt.at(50, 60) });
    try rig.expectPoints(t, &.{.{ .at = pt.at(50, 60) }});
    drag.cancel(testing.allocator, rig.bridge());
    try rig.expectPoints(t, &.{start});
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);

    var click = try PointDrag.begin(testing.allocator, rig.bridge(), t, 0, .move);
    try testing.expect(click.finish(testing.allocator) == null);
    var same = try PointDrag.begin(testing.allocator, rig.bridge(), t, 0, .move);
    try same.move(rig.bridge(), .{ .at = pt.at(10, 20) });
    try testing.expect(same.finish(testing.allocator) == null);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);

    try testing.expectError(error.BadArgument, PointDrag.begin(testing.allocator, rig.bridge(), t, 1, .move));
    try testing.expect((try editPoint(testing.allocator, rig.bridge(), t, 0, .move, .{ .at = pt.at(10, 20) })) == null);
}

test "a one-shot edit of the direction is one undo step" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit();
    const t = rig.target(.dir_explosion);
    try rig.commitCommand((try editPoint(testing.allocator, rig.bridge(), t, 2, .direction, .{ .angle = 45 })).?);
    var want = [_]AimedPoint{.{}} ** 5;
    want[2].angle = 45;
    try rig.expectPoints(t, &want);
    try testing.expectEqualStrings("45", try rig.childProp(t, 2, "Direction"));
    try rig.undo();
    try rig.expectPoints(t, &([_]AimedPoint{.{}} ** 5));
    try rig.redo();
    try rig.expectPoints(t, &want);
}

test "set entrance undoes and redoes with the point before and after" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit();
    try rig.commitCommand(try setEntrance(testing.allocator, rig.bridge(), rig.root, pt.at(5.5, -2)));
    var read = try tools.readGeometry(rig.bridge(), rig.root, .entrance);
    try testing.expectEqual(pt.at(5.5, -2), read.point2);
    try rig.undo();
    read = try tools.readGeometry(rig.bridge(), rig.root, .entrance);
    try testing.expectEqual(pt.at(0, 0), read.point2);
    try rig.redo();
    read = try tools.readGeometry(rig.bridge(), rig.root, .entrance);
    try testing.expectEqual(pt.at(5.5, -2), read.point2);
}

test "generate smoke points on a 4x2 footprint gives MFC's edge points" {
    // Tiles x 0..3, y 0..1: two points on each long edge, one on each short one.
    const cells = [_]u8{1} ** 8;
    const box = Footprint.of(&cells, 4, 2).?;
    const points = try generateSmoke(testing.allocator, box, .{});
    defer testing.allocator.free(points);
    try testing.expectEqual(@as(usize, 2 * 2 + 2 * 1), points.len);
    // The left corner of tile (0, 0) is the grid origin; the first point is a quarter cell in.
    try testing.expectEqual(AimedPoint{ .at = pt.at(-622 + 8, 296 + 4), .angle = 180 }, points[0]);
    try testing.expectEqual(AimedPoint{ .at = pt.at(-622 + 32 + 8, 296 + 16 + 4), .angle = 180 }, points[1]);
    // Bottom corner of tile (3, 0): the corner of tile (4, 0).
    try testing.expectEqual(AimedPoint{ .at = pt.at(-622 + 16 * 4 + 8, 296 + 8 * 4 - 4), .angle = 270 }, points[2]);
    var angles = [_]i32{ 0, 0, 0, 0 };
    for (points) |p| switch (p.angle) {
        180 => angles[0] += 1,
        270 => angles[1] += 1,
        0 => angles[2] += 1,
        90 => angles[3] += 1,
        else => return error.TestUnexpectedResult,
    };
    try testing.expectEqualSlices(i32, &.{ 2, 1, 2, 1 }, &angles);
}

test "generate directed explosions puts five points with MFC's directions and keeps the cones" {
    const cells = [_]u8{1} ** 9;
    const box = Footprint.of(&cells, 3, 3).?;
    const points = generateDirExp(box, .{}, .{ 10, 20, 30, 40, 50 });
    const angles = [5]i32{ 180, 270, 0, 90, 225 };
    for (points, angles, [5]i32{ 10, 20, 30, 40, 50 }) |p, angle, cone| {
        try testing.expectEqual(angle, p.angle);
        try testing.expectEqual(cone, p.cone);
    }
    // The centre one is the middle of the left and right corners.
    const corners = cornersOf(box);
    try testing.expectEqual(mid(corners.left, corners.right), points[4].at);
    // A full square has a locked tile under every probe, so the first probe stops at once: the edge middles.
    try testing.expectEqual(mid(corners.left, corners.bottom), points[0].at);
    try testing.expectEqual(mid(corners.top, corners.right), points[2].at);
}

test "generate commands change list and children in one step and undo it" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit();
    const cells = [_]u8{1} ** 8;
    try rig.setPassability(&cells, 4, 2);

    const smoke = rig.target(.smoke);
    try rig.commitCommand(try placePoint(testing.allocator, &rig.doc, rig.bridge(), smoke, pt.at(1, 1), null));
    try rig.commitCommand(try generateSmokePoints(testing.allocator, &rig.doc, rig.bridge(), smoke, .{}));
    var read = try tools.readGeometry(rig.bridge(), smoke.node, smoke.channel);
    try testing.expectEqual(@as(usize, 6), read.aimed.len);
    try rig.expectChildren(smoke, 6);
    try testing.expectEqualStrings("180", try rig.childProp(smoke, 0, "Direction"));
    read.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectPoints(smoke, &.{.{ .at = pt.at(1, 1) }});
    try rig.expectChildren(smoke, 1);
    try rig.redo();
    try rig.expectChildren(smoke, 6);

    const blasts = rig.target(.dir_explosion);
    try rig.commitCommand((try generateDirExpPoints(testing.allocator, rig.bridge(), blasts, .{})).?);
    var after = try tools.readGeometry(rig.bridge(), blasts.node, blasts.channel);
    defer after.deinit(testing.allocator);
    try testing.expectEqual(@as(i32, 225), after.aimed[4].angle);
    try rig.expectChildren(blasts, 5);
    try rig.undo();
    try rig.expectPoints(blasts, &([_]AimedPoint{.{}} ** 5));
    try rig.redo();
    // Generating again changes nothing.
    try testing.expect((try generateDirExpPoints(testing.allocator, rig.bridge(), blasts, .{})) == null);
}

test "generate refuses a building with no passability tile" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit();
    try testing.expectError(error.Refused, generateSmokePoints(testing.allocator, &rig.doc, rig.bridge(), rig.target(.smoke), .{}));
    try testing.expectError(error.Refused, generateDirExpPoints(testing.allocator, rig.bridge(), rig.target(.dir_explosion), .{}));
}

// --- Bridge tests ------------------------------------------------------------

const grid_tools = @import("grid_tools.zig");

test "bridge fire and smoke points are placed, copied and deleted as one step each" {
    inline for (.{ Mode.fire, Mode.smoke }) |mode| {
        var rig = try Rig.initBridge(testing.allocator);
        defer rig.deinit();
        try placeAndDeleteOn(&rig, mode, 0, 0, 0);
    }
}

test "a bridge has no shoot points and its directed explosions have no place and no delete" {
    var rig = try Rig.initBridge(testing.allocator);
    defer rig.deinit();
    try testing.expect(bridgeTarget(&rig.doc, rig.root, .shoot) == null);
    const t = rig.target(.dir_explosion);
    try testing.expectError(error.Refused, placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(1, 1), null));
    try testing.expectError(error.Refused, deletePoint(testing.allocator, &rig.doc, rig.bridge(), t, 0));
    try rig.expectChildren(t, 5);
}

test "bridge points move, run horizontally and turn in one undo step each" {
    const cases = [_]struct { mode: Mode, part: Part, sample: Sample, want: AimedPoint }{
        .{ .mode = .fire, .part = .move, .sample = .{ .at = pt.at(15, 25) }, .want = .{ .at = pt.at(15, 25), .angle = 0 } },
        .{ .mode = .smoke, .part = .horizontal, .sample = .{ .at = pt.at(99, 33) }, .want = .{ .at = pt.at(10, 33), .angle = 0 } },
        .{ .mode = .fire, .part = .direction, .sample = .{ .angle = 270 }, .want = .{ .at = pt.at(10, 20), .angle = 270 } },
        .{ .mode = .smoke, .part = .direction, .sample = .{ .angle = 90 }, .want = .{ .at = pt.at(10, 20), .angle = 90 } },
    };
    for (cases) |case| {
        var rig = try Rig.initBridge(testing.allocator);
        defer rig.deinit();
        const t = rig.target(case.mode);
        try rig.commitCommand(try placePoint(testing.allocator, &rig.doc, rig.bridge(), t, pt.at(10, 20), null));
        const start: AimedPoint = .{ .at = pt.at(10, 20) };
        var drag = try PointDrag.begin(testing.allocator, rig.bridge(), t, 0, case.part);
        try drag.move(rig.bridge(), .{ .at = pt.at(1, 1), .angle = 1 });
        try drag.move(rig.bridge(), case.sample);
        try rig.commitCommand(drag.finish(testing.allocator).?);
        try rig.expectPoints(t, &.{case.want});
        try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
        try rig.undo();
        try rig.expectPoints(t, &.{start});
        try rig.redo();
        try rig.expectPoints(t, &.{case.want});
    }
}

test "a bridge's directed explosion turns in one undo step" {
    var rig = try Rig.initBridge(testing.allocator);
    defer rig.deinit();
    const t = rig.target(.dir_explosion);
    try rig.commitCommand((try editPoint(testing.allocator, rig.bridge(), t, 1, .direction, .{ .angle = 135 })).?);
    var want = [_]AimedPoint{.{}} ** 5;
    want[1].angle = 135;
    try rig.expectPoints(t, &want);
    try testing.expectEqualStrings("135", try rig.childProp(t, 1, "Direction"));
    try rig.undo();
    try rig.expectPoints(t, &([_]AimedPoint{.{}} ** 5));
    try rig.redo();
    try rig.expectPoints(t, &want);
}

test "bridge generate commands walk the span part's locked tiles and undo in one step" {
    var rig = try Rig.initBridge(testing.allocator);
    defer rig.deinit();
    const smoke = rig.target(.smoke);
    const blasts = rig.target(.dir_explosion);
    // A bridge with no locked tile has no footprint to walk.
    try testing.expectError(error.Refused, generateSmokePoints(testing.allocator, &rig.doc, rig.bridge(), smoke, .{}));
    try testing.expectError(error.Refused, generateDirExpPoints(testing.allocator, rig.bridge(), blasts, .{}));

    const cells = [_]u8{1} ** 8;
    try bridge_mod.check(rig.bridge().geometryWrite(rig.part, .locked_tiles, &.{ .bytes_grid = .{ .bytes = @constCast(&cells), .width = 4, .height = 2 } }));
    try rig.commitCommand(try generateSmokePoints(testing.allocator, &rig.doc, rig.bridge(), smoke, .{}));
    var read = try tools.readGeometry(rig.bridge(), smoke.node, smoke.channel);
    try testing.expectEqual(@as(usize, 6), read.aimed.len);
    read.deinit(testing.allocator);
    try rig.expectChildren(smoke, 6);
    try testing.expectEqualStrings("180", try rig.childProp(smoke, 0, "Direction"));
    try rig.undo();
    try rig.expectPoints(smoke, &.{});
    try rig.expectChildren(smoke, 0);
    try rig.redo();
    try rig.expectChildren(smoke, 6);

    try rig.commitCommand((try generateDirExpPoints(testing.allocator, rig.bridge(), blasts, .{})).?);
    var after = try tools.readGeometry(rig.bridge(), blasts.node, blasts.channel);
    defer after.deinit(testing.allocator);
    try testing.expectEqual(@as(i32, 225), after.aimed[4].angle);
    try rig.undo();
    try rig.expectPoints(blasts, &([_]AimedPoint{.{}} ** 5));
    try rig.redo();
    try rig.expectChildren(blasts, 5);
}

fn expectMarks(rig: *Rig, want: [3]Point2) !void {
    var read = try tools.readGeometry(rig.bridge(), rig.root, .bridge_span_marks);
    defer read.deinit(rig.allocator);
    try testing.expectEqualSlices(Point2, &want, read.points2);
}

test "each span mark moves in one undo step and leaves the others, a stale Front and Back too" {
    inline for (.{ BridgeType.horizontal, BridgeType.vertical }) |kind| {
        var rig = try Rig.initBridge(testing.allocator);
        defer rig.deinit();
        const line_y = struct {
            fn at(x: f32) f32 {
                return spanLineY(kind, x);
            }
        }.at;
        // A project with stale Front and Back, as a reopened .bdg holds them.
        const stale: [3]Point2 = .{ pt.at(100, line_y(100)), pt.at(700, line_y(700)), pt.at(-12.5, 30.25) };
        try bridge_mod.check(rig.bridge().geometryWrite(rig.root, .bridge_span_marks, &.{ .points2 = @constCast(&stale) }));

        // Begin and End keep x and take the line's y, whatever y the pointer has.
        var want = stale;
        try rig.commitCommand((try setSpanMark(testing.allocator, rig.bridge(), rig.root, .begin, pt.at(240, 9999), kind, .{})).?);
        want[0] = pt.at(240, line_y(240));
        try expectMarks(&rig, want);
        try rig.commitCommand((try setSpanMark(testing.allocator, rig.bridge(), rig.root, .end, pt.at(820, -9999), kind, .{})).?);
        want[1] = pt.at(820, line_y(820));
        try expectMarks(&rig, want);
        // Front and Back are offsets from the span's papa across the line.
        const papa = pt.at(50, 60);
        try rig.commitCommand((try setSpanMark(testing.allocator, rig.bridge(), rig.root, .front, pt.at(80, 100), kind, papa)).?);
        want[2].x = if (kind == .horizontal) 40 else 30;
        try expectMarks(&rig, want);
        try rig.commitCommand((try setSpanMark(testing.allocator, rig.bridge(), rig.root, .back, pt.at(10, 20), kind, papa)).?);
        want[2].y = -40;
        try expectMarks(&rig, want);
        try testing.expectEqual(@as(usize, 4), rig.history.undo_stack.items.len);

        // Undo walks back one mark at a time, redo forward again.
        const final = want;
        try rig.undo();
        want[2].y = stale[2].y;
        try expectMarks(&rig, want);
        try rig.undo();
        want[2].x = stale[2].x;
        try expectMarks(&rig, want);
        try rig.undo();
        want[1] = stale[1];
        try expectMarks(&rig, want);
        try rig.undo();
        try expectMarks(&rig, stale);
        try rig.redo();
        try rig.redo();
        try rig.redo();
        try rig.redo();
        try testing.expectEqual(final, try readMarks(&rig));
    }
}

fn readMarks(rig: *Rig) ![3]Point2 {
    var read = try tools.readGeometry(rig.bridge(), rig.root, .bridge_span_marks);
    defer read.deinit(rig.allocator);
    var out: [3]Point2 = undefined;
    @memcpy(&out, read.points2);
    return out;
}

test "the bridge type picks the line a span mark sits on" {
    // Horizontal runs down-right from grid index 16, vertical up-right from 14.
    try testing.expectEqual(@as(f32, 296 - 16 * 16), spanLineY(.horizontal, -622 + 32 * 16));
    try testing.expectEqual(@as(f32, 296 + 14 * 16), spanLineY(.vertical, -622 + 32 * 14));
    try testing.expect(spanLineY(.horizontal, 300) > spanLineY(.horizontal, 100));
    try testing.expect(spanLineY(.vertical, 300) < spanLineY(.vertical, 100));
}

test "a span mark with nothing set starts from the frame's defaults, a repeat is no step, a bad list is refused" {
    var rig = try Rig.initBridge(testing.allocator);
    defer rig.deinit();
    var read = try tools.readGeometry(rig.bridge(), rig.root, .bridge_span_marks);
    try testing.expectEqual(@as(usize, 0), read.points2.len);
    read.deinit(testing.allocator);

    try rig.commitCommand((try setSpanMark(testing.allocator, rig.bridge(), rig.root, .front, pt.at(0, 7), .horizontal, .{})).?);
    const marks = try readMarks(&rig);
    try testing.expectEqual(default_span_mark, marks[0]);
    try testing.expectEqual(default_span_mark, marks[1]);
    try testing.expectEqual(pt.at(7, 0), marks[2]);
    try testing.expect((try setSpanMark(testing.allocator, rig.bridge(), rig.root, .front, pt.at(0, 7), .horizontal, .{})) == null);
    try rig.undo();
    read = try tools.readGeometry(rig.bridge(), rig.root, .bridge_span_marks);
    try testing.expectEqual(@as(usize, 0), read.points2.len);
    read.deinit(testing.allocator);

    const two = [_]Point2{ .{}, .{} };
    try bridge_mod.check(rig.bridge().geometryWrite(rig.root, .bridge_span_marks, &.{ .points2 = @constCast(&two) }));
    try testing.expectError(error.BadArgument, setSpanMark(testing.allocator, rig.bridge(), rig.root, .begin, pt.at(1, 1), .horizontal, .{}));
}

test "a span part's passability brush and the bridge's set zero undo and redo" {
    var rig = try Rig.initBridge(testing.allocator);
    defer rig.deinit();
    var stroke = try grid_tools.BrushStroke.begin(testing.allocator, rig.bridge(), rig.part, .locked_tiles, 1);
    try stroke.press(testing.allocator, rig.bridge(), 1, 0);
    try stroke.move(testing.allocator, rig.bridge(), 3, 0);
    try rig.commitCommand((try stroke.finish(testing.allocator, rig.bridge())).?);
    const painted = [_]u8{ 0, 1, 1, 1 };
    const grid = struct {
        fn expect(r: *Rig, w: i32, h: i32, cells: []const u8) !void {
            var read = try tools.readGeometry(r.bridge(), r.part, .locked_tiles);
            defer read.deinit(r.allocator);
            try testing.expectEqual(w, read.bytes_grid.width);
            try testing.expectEqual(h, read.bytes_grid.height);
            try testing.expectEqualSlices(u8, cells, read.bytes_grid.bytes);
        }
    }.expect;
    try grid(&rig, 4, 1, &painted);
    try rig.undo();
    try grid(&rig, 0, 0, &.{});
    try rig.redo();
    try grid(&rig, 4, 1, &painted);

    try rig.commitCommand(try grid_tools.setZero(testing.allocator, rig.bridge(), rig.root, pt.at(33.5, -4)));
    var zero = try tools.readGeometry(rig.bridge(), rig.root, .zero_point);
    try testing.expectEqual(pt.at(33.5, -4), zero.point2);
    try rig.undo();
    zero = try tools.readGeometry(rig.bridge(), rig.root, .zero_point);
    try testing.expectEqual(pt.at(0, 0), zero.point2);
    try rig.redo();
    zero = try tools.readGeometry(rig.bridge(), rig.root, .zero_point);
    try testing.expectEqual(pt.at(33.5, -4), zero.point2);
}
