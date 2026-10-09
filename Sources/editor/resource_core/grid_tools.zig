//! The grid tools of the Object and Fence sub-editors (GridFrm's base, shared
//! by ObjectFrm and FenceFrm): the brush over tiles for passability and
//! transparency cells, the one-way transparency line drag, the zero point,
//! the fence sprite's centring and its move drag. Each tool is parameterised
//! by a node and a geometry channel instead of a kind, so the building and
//! bridge editors (S10, S11) reuse the same state machines.
//!
//! A tool writes its changes through the bridge while a gesture runs so the
//! view follows, and its release yields ONE geometry command from the state at
//! the press to the state at the release, so a stroke or drag undoes in one
//! step however many moves it took. A cancelled gesture puts the bridge back
//! and yields nothing, so the history never sees it.
//!
//! Grids are in the tile frame the bridge documents: cell (x, y) is tile
//! (x, y), and a read grid ends at the furthest set tile. A stroke trims its
//! result the same way, so what the tool leaves and what the bridge reads back
//! agree. As in MFC's SetTileInListOfTiles, painting the value 0 erases.
//!
//! Geometry values are allocated with the bridge's allocator; a tool's
//! `allocator` must be that same allocator, as everywhere in the core.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const history_mod = @import("history.zig");
const tools = @import("sub_editor_tools.zig");

const ResBridge = bridge_mod.ResBridge;
const EditError = bridge_mod.EditError;
const GeometryChannel = bridge_mod.GeometryChannel;
const GeometryValue = bridge_mod.GeometryValue;
const Point2 = bridge_mod.Point2;
const ResourceCommand = history_mod.ResourceCommand;

/// The highest value a transparency cell or a fence transparency tile holds
/// (the bridge refuses anything above).
pub const max_transparency_value: u8 = 7;

/// The furthest set cell of a grid, as MFC's list of tiles would end.
fn trimmedSize(cells: []const u8, width: usize, height: usize) struct { w: usize, h: usize } {
    var w: usize = 0;
    var h: usize = 0;
    var y: usize = 0;
    while (y < height) : (y += 1) {
        var x: usize = 0;
        while (x < width) : (x += 1) {
            if (cells[y * width + x] != 0) {
                w = @max(w, x + 1);
                h = @max(h, y + 1);
            }
        }
    }
    return .{ .w = w, .h = h };
}

/// A brush stroke over the tiles of one grid channel: press paints a tile,
/// moves paint the tiles along the way (a fast mouse skips tiles, so each move
/// is joined to the last tile by a line, as the drag would have visited them),
/// release yields the stroke's one command.
pub const BrushStroke = struct {
    node: i32,
    channel: GeometryChannel,
    value: u8,
    before: GeometryValue,
    cells: []u8,
    width: usize,
    height: usize,
    last: ?[2]i32 = null,
    changed: bool = false,

    /// Reads the grid at the press. A channel of another family, or a value no
    /// transparency cell holds, is refused before anything is touched.
    pub fn begin(allocator: std.mem.Allocator, bridge: ResBridge, node: i32, channel: GeometryChannel, value: u8) EditError!BrushStroke {
        if (channel.family() != .bytes_grid) return error.BadArgument;
        if ((channel == .transparency_cells or channel == .fence_transparences) and value > max_transparency_value) return error.BadArgument;
        var read = try tools.readGeometry(bridge, node, channel);
        errdefer read.deinit(allocator);
        const grid = read.bytes_grid;
        const cells = try allocator.dupe(u8, grid.bytes);
        return .{
            .node = node,
            .channel = channel,
            .value = value,
            .before = read,
            .cells = cells,
            .width = @intCast(grid.width),
            .height = @intCast(grid.height),
        };
    }

    /// Paints one tile; a tile outside the frame (negative) is ignored, and
    /// erasing beyond the grid's end changes nothing.
    fn paint(self: *BrushStroke, allocator: std.mem.Allocator, x: i32, y: i32) EditError!void {
        if (x < 0 or y < 0) return;
        const ux: usize = @intCast(x);
        const uy: usize = @intCast(y);
        if (ux >= self.width or uy >= self.height) {
            if (self.value == 0) return;
            const new_w = @max(self.width, ux + 1);
            const new_h = @max(self.height, uy + 1);
            const grown = try allocator.alloc(u8, new_w * new_h);
            @memset(grown, 0);
            var row: usize = 0;
            while (row < self.height) : (row += 1) {
                @memcpy(grown[row * new_w ..][0..self.width], self.cells[row * self.width ..][0..self.width]);
            }
            allocator.free(self.cells);
            self.cells = grown;
            self.width = new_w;
            self.height = new_h;
        }
        const cell = &self.cells[uy * self.width + ux];
        if (cell.* == self.value) return;
        cell.* = self.value;
        self.changed = true;
    }

    fn write(self: *const BrushStroke, bridge: ResBridge) EditError!void {
        const live: GeometryValue = .{ .bytes_grid = .{
            .bytes = self.cells,
            .width = @intCast(self.width),
            .height = @intCast(self.height),
        } };
        try bridge_mod.check(bridge.geometryWrite(self.node, self.channel, &live));
    }

    /// The press: paints the tile under the cursor.
    pub fn press(self: *BrushStroke, allocator: std.mem.Allocator, bridge: ResBridge, x: i32, y: i32) EditError!void {
        try self.paint(allocator, x, y);
        self.last = .{ x, y };
        if (self.changed) try self.write(bridge);
    }

    /// A mouse move with the button down: the tiles from the last one to this.
    pub fn move(self: *BrushStroke, allocator: std.mem.Allocator, bridge: ResBridge, x: i32, y: i32) EditError!void {
        const from = self.last orelse .{ x, y };
        var tiles = std.ArrayListUnmanaged([2]i32).empty;
        defer tiles.deinit(allocator);
        try lineTiles(allocator, &tiles, from[0], from[1], x, y);
        for (tiles.items) |tile| try self.paint(allocator, tile[0], tile[1]);
        self.last = .{ x, y };
        if (self.changed) try self.write(bridge);
    }

    /// The release: the stroke's one command, or null when no tile ended
    /// differently (a click on a tile that already held the value). The
    /// stroke is spent either way.
    pub fn finish(self: *BrushStroke, allocator: std.mem.Allocator, bridge: ResBridge) EditError!?ResourceCommand {
        defer self.deinit(allocator);
        if (!self.changed) return null;
        const size = trimmedSize(self.cells, self.width, self.height);
        const trimmed = try allocator.alloc(u8, size.w * size.h);
        errdefer allocator.free(trimmed);
        var row: usize = 0;
        while (row < size.h) : (row += 1) {
            @memcpy(trimmed[row * size.w ..][0..size.w], self.cells[row * self.width ..][0..size.w]);
        }
        const after: GeometryValue = .{ .bytes_grid = .{ .bytes = trimmed, .width = @intCast(size.w), .height = @intCast(size.h) } };
        // The bridge holds the untrimmed live grid; the command's after is the
        // trimmed one, so the document's redo and a reload agree.
        try bridge_mod.check(bridge.geometryWrite(self.node, self.channel, &after));
        const before = try self.before.dupe(allocator);
        return .{ .geometry = .{ .node = self.node, .channel = self.channel, .before = before, .after = after } };
    }

    /// Escape or a right click during a stroke: the grid goes back.
    pub fn cancel(self: *BrushStroke, allocator: std.mem.Allocator, bridge: ResBridge) void {
        if (self.changed) _ = bridge.geometryWrite(self.node, self.channel, &self.before);
        self.deinit(allocator);
    }

    pub fn deinit(self: *BrushStroke, allocator: std.mem.Allocator) void {
        self.before.deinit(allocator);
        allocator.free(self.cells);
        self.cells = &.{};
        self.changed = false;
    }
};

/// Bresenham between two tiles, both ends included: the tiles a drag from one
/// to the other passes (the same walk as GridProjection::FillBresenham).
fn lineTiles(allocator: std.mem.Allocator, out: *std.ArrayListUnmanaged([2]i32), x0: i32, y0: i32, x1: i32, y1: i32) !void {
    var x = x0;
    var y = y0;
    const dx: i32 = @intCast(@abs(x1 - x0));
    const dy: i32 = -@as(i32, @intCast(@abs(y1 - y0)));
    const sx: i32 = if (x0 < x1) 1 else -1;
    const sy: i32 = if (y0 < y1) 1 else -1;
    var err = dx + dy;
    while (true) {
        try out.append(allocator, .{ x, y });
        if (x == x1 and y == y1) break;
        const e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

/// ObjectFrm's one-way transparency line: press on empty ground starts a
/// line, moves drag its far end, release adds it; a press on a line's middle
/// selects that line instead, and a right click deletes the selected one (or
/// cancels the line being dragged). The lines are the channel's point list,
/// two points per line.
pub const TransLineTool = struct {
    node: i32,
    dragging: bool = false,
    p1: Point2 = .{},
    p2: Point2 = .{},
    selected: ?usize = null,
    /// How close, per axis, a press must be to a line's middle to select it
    /// (ObjectFrm's DELTA_X / DELTA_Y), in the units of the points.
    hit_delta: f32 = 3,

    pub fn init(node: i32) TransLineTool {
        return .{ .node = node };
    }

    fn lines(self: *const TransLineTool, bridge: ResBridge) EditError!GeometryValue {
        return tools.readGeometry(bridge, self.node, .transparency_lines);
    }

    /// The press: selects the line whose middle is under `at`, else starts a
    /// new line there. Nothing is written until the release.
    pub fn press(self: *TransLineTool, allocator: std.mem.Allocator, bridge: ResBridge, at: Point2) EditError!void {
        var read = try self.lines(bridge);
        defer read.deinit(allocator);
        const points = read.points2;
        var i: usize = 0;
        while (i + 1 < points.len) : (i += 2) {
            const cx = (points[i].x + points[i + 1].x) / 2;
            const cy = (points[i].y + points[i + 1].y) / 2;
            if (@abs(at.x - cx) <= self.hit_delta and @abs(at.y - cy) <= self.hit_delta) {
                const line = i / 2;
                self.selected = if (self.selected != null and self.selected.? == line) null else line;
                self.dragging = false;
                return;
            }
        }
        self.selected = null;
        self.p1 = at;
        self.p2 = at;
        self.dragging = true;
    }

    /// A move with the button down: the dragged end follows.
    pub fn move(self: *TransLineTool, at: Point2) void {
        if (self.dragging) self.p2 = at;
    }

    /// The release: the command that adds the dragged line, or null when no
    /// line was being dragged or it has no length (a click).
    pub fn release(self: *TransLineTool, allocator: std.mem.Allocator, bridge: ResBridge, at: Point2) EditError!?ResourceCommand {
        if (!self.dragging) return null;
        self.dragging = false;
        self.p2 = at;
        if (self.p1.x == self.p2.x and self.p1.y == self.p2.y) return null;
        var read = try self.lines(bridge);
        defer read.deinit(allocator);
        const grown = try allocator.alloc(Point2, read.points2.len + 2);
        defer allocator.free(grown);
        @memcpy(grown[0..read.points2.len], read.points2);
        grown[read.points2.len] = self.p1;
        grown[read.points2.len + 1] = self.p2;
        return try tools.setGeometry(allocator, bridge, self.node, .transparency_lines, .{ .points2 = grown });
    }

    /// A right click: while a line is being dragged it is dropped and nothing
    /// is recorded; otherwise the selected line, if any, is deleted.
    pub fn rightClick(self: *TransLineTool, allocator: std.mem.Allocator, bridge: ResBridge) EditError!?ResourceCommand {
        if (self.dragging) {
            self.dragging = false;
            return null;
        }
        const line = self.selected orelse return null;
        self.selected = null;
        var read = try self.lines(bridge);
        defer read.deinit(allocator);
        const points = read.points2;
        if (line * 2 + 1 >= points.len) return null;
        const kept = try allocator.alloc(Point2, points.len - 2);
        defer allocator.free(kept);
        @memcpy(kept[0 .. line * 2], points[0 .. line * 2]);
        @memcpy(kept[line * 2 ..], points[line * 2 + 2 ..]);
        return try tools.setGeometry(allocator, bridge, self.node, .transparency_lines, .{ .points2 = kept });
    }

    /// Escape: the same as a right click while dragging.
    pub fn cancel(self: *TransLineTool) void {
        self.dragging = false;
    }
};

/// ObjectFrm's Set Zero click: the object's zero point. The passability grid's
/// origin follows it at save (the bridge keeps desc consistent).
pub fn setZero(allocator: std.mem.Allocator, bridge: ResBridge, node: i32, to: Point2) EditError!ResourceCommand {
    return tools.setGeometry(allocator, bridge, node, .zero_point, .{ .point2 = to });
}

/// A whole grid emptied in one step (the toolbar's clear): the command, or
/// null when the grid is already empty.
pub fn clearGrid(allocator: std.mem.Allocator, bridge: ResBridge, node: i32, channel: GeometryChannel) EditError!?ResourceCommand {
    if (channel.family() != .bytes_grid) return error.BadArgument;
    var read = try tools.readGeometry(bridge, node, channel);
    defer read.deinit(allocator);
    var any = false;
    for (read.bytes_grid.bytes) |cell| any = any or cell != 0;
    if (!any and read.bytes_grid.width == 0 and read.bytes_grid.height == 0) return null;
    return try tools.setGeometry(allocator, bridge, node, channel, .{ .bytes_grid = .{ .bytes = &.{}, .width = 0, .height = 0 } });
}

/// FenceFrm's CenterSpriteAboutTile: the sprite goes to the middle of the tile
/// it is on. The caller works the tile and its centre out with the grid
/// projection (the middle of the tile's corners, in world units) and hands the
/// centre over, so the core stays free of the camera.
pub fn centreSpriteOnTile(allocator: std.mem.Allocator, bridge: ResBridge, node: i32, tile_centre: Point2) EditError!ResourceCommand {
    return tools.setGeometry(allocator, bridge, node, .sprite_pos, .{ .point2 = tile_centre });
}

/// A drag of the sprite: moves write live, the release yields one command.
pub const SpriteDrag = struct {
    node: i32,
    before: Point2,
    current: Point2,
    moved: bool = false,

    pub fn begin(bridge: ResBridge, node: i32) EditError!SpriteDrag {
        const read = try tools.readGeometry(bridge, node, .sprite_pos);
        return .{ .node = node, .before = read.point2, .current = read.point2 };
    }

    pub fn move(self: *SpriteDrag, bridge: ResBridge, to: Point2) EditError!void {
        self.current = to;
        self.moved = true;
        const live: GeometryValue = .{ .point2 = to };
        try bridge_mod.check(bridge.geometryWrite(self.node, .sprite_pos, &live));
    }

    /// The release: the drag's one command, or null when the sprite ended
    /// where it began.
    pub fn finish(self: *SpriteDrag) ?ResourceCommand {
        if (!self.moved or (self.before.x == self.current.x and self.before.y == self.current.y)) return null;
        return .{ .geometry = .{
            .node = self.node,
            .channel = .sprite_pos,
            .before = .{ .point2 = self.before },
            .after = .{ .point2 = self.current },
        } };
    }

    pub fn cancel(self: *SpriteDrag, bridge: ResBridge) void {
        if (self.moved) {
            const back: GeometryValue = .{ .point2 = self.before };
            _ = bridge.geometryWrite(self.node, .sprite_pos, &back);
        }
    }
};

// --- Tests -------------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = @import("fake_bridge.zig").FakeResBridge;
const document_mod = @import("document.zig");
const Document = document_mod.Document;
const History = history_mod.History;

const Rig = struct {
    allocator: std.mem.Allocator,
    fake: FakeResBridge,
    doc: Document = .{},
    history: History = .{},
    root: i32 = 0,

    fn init(allocator: std.mem.Allocator, kind: bridge_mod.Kind) !Rig {
        var rig: Rig = .{ .allocator = allocator, .fake = FakeResBridge.init(allocator) };
        try bridge_mod.check(rig.fake.bridge().new(kind));
        rig.root = rig.fake.nodes.items[0].id;
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

    /// The channel's grid as the bridge reads it: width, height, then cells.
    fn expectGrid(self: *Rig, channel: GeometryChannel, w: i32, h: i32, cells: []const u8) !void {
        var read = try tools.readGeometry(self.bridge(), self.root, channel);
        defer read.deinit(self.allocator);
        try testing.expectEqual(w, read.bytes_grid.width);
        try testing.expectEqual(h, read.bytes_grid.height);
        try testing.expectEqualSlices(u8, cells, read.bytes_grid.bytes);
    }

    fn expectPoints(self: *Rig, channel: GeometryChannel, want: []const Point2) !void {
        var read = try tools.readGeometry(self.bridge(), self.root, channel);
        defer read.deinit(self.allocator);
        try testing.expectEqualSlices(Point2, want, read.points2);
    }

    fn expectPoint(self: *Rig, channel: GeometryChannel, want: Point2) !void {
        const read = try tools.readGeometry(self.bridge(), self.root, channel);
        try testing.expectEqual(want, read.point2);
    }
};

/// One brush stroke over `channel` on the kind's root, checked before, after,
/// after undo and after redo; the stroke covers a diagonal jump (the move
/// fills the tiles between) and ends as one history step.
fn strokeCase(kind: bridge_mod.Kind, channel: GeometryChannel, value: u8) !void {
    var rig = try Rig.init(testing.allocator, kind);
    defer rig.deinit();
    try rig.expectGrid(channel, 0, 0, &.{});

    var stroke = try BrushStroke.begin(testing.allocator, rig.bridge(), rig.root, channel, value);
    try stroke.press(testing.allocator, rig.bridge(), 1, 0);
    try stroke.move(testing.allocator, rig.bridge(), 3, 2);
    try stroke.move(testing.allocator, rig.bridge(), 3, 2);
    const command = (try stroke.finish(testing.allocator, rig.bridge())).?;
    try rig.commitCommand(command);

    const v = value;
    const painted = [_]u8{
        0, v, 0, 0, //
        0, 0, v, 0, //
        0, 0, 0, v,
    };
    // (1,0) (2,1) (3,2) on the line from the press; the grid ends at (3,2).
    try rig.expectGrid(channel, 4, 3, &painted);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectGrid(channel, 0, 0, &.{});
    try rig.redo();
    try rig.expectGrid(channel, 4, 3, &painted);

    // Erasing: value 0 over (2,1) and (3,2) is one more step and the grid
    // shrinks to the furthest tile left.
    var erase = try BrushStroke.begin(testing.allocator, rig.bridge(), rig.root, channel, 0);
    try erase.press(testing.allocator, rig.bridge(), 3, 2);
    try erase.move(testing.allocator, rig.bridge(), 2, 1);
    try rig.commitCommand((try erase.finish(testing.allocator, rig.bridge())).?);
    try rig.expectGrid(channel, 2, 1, &.{ 0, v });
    try testing.expectEqual(@as(usize, 2), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectGrid(channel, 4, 3, &painted);
}

test "brush stroke on object passability is one undo step" {
    try strokeCase(.object, .passability_cells, 1);
}

test "brush stroke on object transparency cells is one undo step" {
    try strokeCase(.object, .transparency_cells, 3);
}

test "brush stroke on fence locked tiles is one undo step" {
    try strokeCase(.fence, .passability_cells, 1);
}

test "brush stroke on fence transparences is one undo step" {
    try strokeCase(.fence, .fence_transparences, 5);
}

test "a brush refuses a transparency value above 7 and a channel that is no grid" {
    var rig = try Rig.init(testing.allocator, .object);
    defer rig.deinit();
    try testing.expectError(error.BadArgument, BrushStroke.begin(testing.allocator, rig.bridge(), rig.root, .transparency_cells, 8));
    try testing.expectError(error.BadArgument, BrushStroke.begin(testing.allocator, rig.bridge(), rig.root, .zero_point, 1));
}

test "a cancelled stroke and a click on a tile that already holds the value leave no history entry" {
    var rig = try Rig.init(testing.allocator, .object);
    defer rig.deinit();
    var stroke = try BrushStroke.begin(testing.allocator, rig.bridge(), rig.root, .passability_cells, 1);
    try stroke.press(testing.allocator, rig.bridge(), 2, 2);
    try stroke.move(testing.allocator, rig.bridge(), 4, 2);
    try rig.expectGrid(.passability_cells, 5, 3, &(@as([10]u8, @splat(0)) ++ [_]u8{ 0, 0, 1, 1, 1 }));
    stroke.cancel(testing.allocator, rig.bridge());
    try rig.expectGrid(.passability_cells, 0, 0, &.{});
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);

    var erase = try BrushStroke.begin(testing.allocator, rig.bridge(), rig.root, .passability_cells, 0);
    try erase.press(testing.allocator, rig.bridge(), 5, 5);
    try testing.expect((try erase.finish(testing.allocator, rig.bridge())) == null);
}

test "set zero, sprite centring and a sprite drag undo and redo with state before and after" {
    var rig = try Rig.init(testing.allocator, .fence);
    defer rig.deinit();
    try rig.expectPoint(.zero_point, .{});
    try rig.commitCommand(try setZero(testing.allocator, rig.bridge(), rig.root, .{ .x = 2.5, .y = -1 }));
    try rig.expectPoint(.zero_point, .{ .x = 2.5, .y = -1 });
    try rig.undo();
    try rig.expectPoint(.zero_point, .{});
    try rig.redo();
    try rig.expectPoint(.zero_point, .{ .x = 2.5, .y = -1 });

    try rig.expectPoint(.sprite_pos, .{});
    try rig.commitCommand(try centreSpriteOnTile(testing.allocator, rig.bridge(), rig.root, .{ .x = 96, .y = 48 }));
    try rig.expectPoint(.sprite_pos, .{ .x = 96, .y = 48 });
    try rig.undo();
    try rig.expectPoint(.sprite_pos, .{});
    try rig.redo();

    var drag = try SpriteDrag.begin(rig.bridge(), rig.root);
    try drag.move(rig.bridge(), .{ .x = 100, .y = 50 });
    try drag.move(rig.bridge(), .{ .x = 110, .y = 60 });
    try rig.commitCommand(drag.finish().?);
    try rig.expectPoint(.sprite_pos, .{ .x = 110, .y = 60 });
    try testing.expectEqual(@as(usize, 3), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectPoint(.sprite_pos, .{ .x = 96, .y = 48 });
    try rig.redo();
    try rig.expectPoint(.sprite_pos, .{ .x = 110, .y = 60 });

    var back = try SpriteDrag.begin(rig.bridge(), rig.root);
    try back.move(rig.bridge(), .{ .x = 1, .y = 1 });
    back.cancel(rig.bridge());
    try rig.expectPoint(.sprite_pos, .{ .x = 110, .y = 60 });
}

test "clear grid empties a grid in one step and does nothing on an empty one" {
    var rig = try Rig.init(testing.allocator, .object);
    defer rig.deinit();
    try testing.expect((try clearGrid(testing.allocator, rig.bridge(), rig.root, .transparency_cells)) == null);
    var stroke = try BrushStroke.begin(testing.allocator, rig.bridge(), rig.root, .transparency_cells, 2);
    try stroke.press(testing.allocator, rig.bridge(), 0, 0);
    try rig.commitCommand((try stroke.finish(testing.allocator, rig.bridge())).?);
    try rig.expectGrid(.transparency_cells, 1, 1, &.{2});
    try rig.commitCommand((try clearGrid(testing.allocator, rig.bridge(), rig.root, .transparency_cells)).?);
    try rig.expectGrid(.transparency_cells, 0, 0, &.{});
    try rig.undo();
    try rig.expectGrid(.transparency_cells, 1, 1, &.{2});
    try rig.redo();
    try rig.expectGrid(.transparency_cells, 0, 0, &.{});
}

test "trans-line drag adds a line, a cancelled drag adds nothing, a right click deletes the selected line" {
    var rig = try Rig.init(testing.allocator, .object);
    defer rig.deinit();
    var tool = TransLineTool.init(rig.root);

    try tool.press(testing.allocator, rig.bridge(), .{ .x = 10, .y = 10 });
    tool.move(.{ .x = 20, .y = 10 });
    try rig.commitCommand((try tool.release(testing.allocator, rig.bridge(), .{ .x = 30, .y = 10 })).?);
    const first = [_]Point2{ .{ .x = 10, .y = 10 }, .{ .x = 30, .y = 10 } };
    try rig.expectPoints(.transparency_lines, &first);

    // Cancelled: right click while dragging.
    try tool.press(testing.allocator, rig.bridge(), .{ .x = 50, .y = 50 });
    tool.move(.{ .x = 60, .y = 70 });
    try testing.expect((try tool.rightClick(testing.allocator, rig.bridge())) == null);
    try testing.expect((try tool.release(testing.allocator, rig.bridge(), .{ .x = 60, .y = 70 })) == null);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try rig.expectPoints(.transparency_lines, &first);

    // A click without length adds nothing.
    try tool.press(testing.allocator, rig.bridge(), .{ .x = 50, .y = 50 });
    try testing.expect((try tool.release(testing.allocator, rig.bridge(), .{ .x = 50, .y = 50 })) == null);

    // A second line, then select the first by its middle and delete it.
    try tool.press(testing.allocator, rig.bridge(), .{ .x = 0, .y = 40 });
    try rig.commitCommand((try tool.release(testing.allocator, rig.bridge(), .{ .x = 0, .y = 60 })).?);
    const both = [_]Point2{ .{ .x = 10, .y = 10 }, .{ .x = 30, .y = 10 }, .{ .x = 0, .y = 40 }, .{ .x = 0, .y = 60 } };
    try rig.expectPoints(.transparency_lines, &both);

    try tool.press(testing.allocator, rig.bridge(), .{ .x = 20, .y = 11 });
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    try rig.commitCommand((try tool.rightClick(testing.allocator, rig.bridge())).?);
    const second = [_]Point2{ .{ .x = 0, .y = 40 }, .{ .x = 0, .y = 60 } };
    try rig.expectPoints(.transparency_lines, &second);
    try testing.expectEqual(@as(usize, 3), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectPoints(.transparency_lines, &both);
    try rig.redo();
    try rig.expectPoints(.transparency_lines, &second);
    // With nothing selected a right click does nothing.
    try testing.expect((try tool.rightClick(testing.allocator, rig.bridge())) == null);
}
