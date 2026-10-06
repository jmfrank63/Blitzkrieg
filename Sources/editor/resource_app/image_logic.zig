//! The image frame's logic, with no window and no ImGui: where each
//! sub-editor's picture comes from, which map position the tree selection
//! makes active, and the press-move-release gestures of ImageFrm's views
//! (click to place the active item, or Show crosses to pick a cross up and
//! drag it). Each gesture ends in one geometry command built by
//! resource_core's sub_editor_tools and committed through its `commit`, so
//! the drawing code never builds a command itself.
//! Runs under `zig build test-resource-app-logic` against the fake bridge.
const std = @import("std");
const core = @import("resource_core");

const tools = core.sub_editor_tools;
const bridge_mod = core.bridge;
const ResBridge = bridge_mod.ResBridge;
const Point2 = bridge_mod.Point2;
const GeometryChannel = bridge_mod.GeometryChannel;
const Document = core.document.Document;
const History = core.history.History;

pub const Error = bridge_mod.EditError;

/// The four sub-editors that show a picture: Mission and Chapter and
/// Campaign place map positions on it, Medal only shows its picture.
pub const Kind = enum {
    mission,
    chapter,
    campaign,
    medal,

    pub fn of(kind: bridge_mod.Kind) ?Kind {
        return switch (kind) {
            .mission => .mission,
            .chapter => .chapter,
            .campaign => .campaign,
            .medal => .medal,
            else => null,
        };
    }

    /// Whether the picture carries positions at all.
    pub fn places(self: Kind) bool {
        return self != .medal;
    }

    /// ChapterFrm and CampaignFrm's Show crosses toggle.
    pub fn hasShowCrosses(self: Kind) bool {
        return self == .chapter or self == .campaign;
    }
};

/// The side the engine's image decoder is asked for, and the largest picture
/// the frame takes: a decode that comes back at this side may have been
/// scaled, so it is refused rather than shown with positions that would no
/// longer be real pixels.
pub const max_side: i32 = 2048;

/// Whether a decoded picture still has its real size.
pub fn realSize(width: i32, height: i32) bool {
    return width > 0 and height > 0 and width < max_side and height < max_side;
}

/// The message for a picture `realSize` refuses.
pub const too_large_message = "the picture is 2048 pixels or more on a side; positions would not be real pixels";

/// The path BkEditorMinimapImage decodes (it takes "<base>.xml" and reads
/// "<base>.tga", then "<base>_h.dds"): the Mission's own map_h.dds beside the
/// project, or the picture named by the Chapter, Campaign or Medal's common
/// properties (property ids 4, 3 and 3: they count from 1, unlike the exporters' value indexes) as a .tga
/// in the project folder. Null while the name is empty or the path does not fit.
pub fn sourcePath(buffer: []u8, doc: *const Document, kind: Kind, project_folder: []const u8) ?[:0]const u8 {
    const name: []const u8 = switch (kind) {
        .mission => "map",
        else => blk: {
            const common = switch (kind) {
                .chapter => tools.firstOfClass(doc, tools.item_type.chapter_common_props),
                .campaign => tools.firstOfClass(doc, tools.item_type.campaign_common_props),
                else => tools.firstOfClass(doc, tools.item_type.medal_common_props),
            } orelse return null;
            const id: i32 = if (kind == .chapter) 4 else 3;
            break :blk tools.propValue(doc, common, id) orelse return null;
        },
    };
    if (name.len == 0) return null;
    const sep: []const u8 = if (project_folder.len == 0 or project_folder[project_folder.len - 1] == '/' or project_folder[project_folder.len - 1] == '\\') "" else "/";
    const text = std.fmt.bufPrintZ(buffer, "{s}{s}{s}.xml", .{ project_folder, sep, name }) catch return null;
    for (text) |*ch| if (ch.* == '\\') {
        ch.* = '/';
    };
    return text;
}

/// MFC's cross: the hit box is 32 x 32 around the position shifted by 15.4
/// (ChapterFrm.cpp zeroSizeX/Y, zeroShiftX/Y).
pub const cross_size: f32 = 32;
pub const cross_shift: f32 = 15.4;

/// How the click acts: place the active item, or ChapterFrm's Show crosses
/// (pick a cross up under the cursor and drag it).
pub const Mode = enum { place, drag_crosses };

/// One list of positions the frame edits: the node whose children are the
/// items, and the channel carrying one point per child.
pub const List = struct { node: i32, channel: GeometryChannel };

/// The lists the image shows crosses for: a Mission's Objectives, a
/// Chapter's Missions then Place holders (the order FindActiveCross walks
/// them) and a Campaign's Chapters.
pub fn crossLists(doc: *const Document, kind: Kind, out: *[2]List) []const List {
    var count: usize = 0;
    const wanted = switch (kind) {
        .mission => [_]struct { i32, GeometryChannel }{ .{ tools.item_type.mission_objectives, .mission_objectives }, .{ 0, .mission_objectives } },
        .chapter => [_]struct { i32, GeometryChannel }{ .{ tools.item_type.chapter_missions, .chapter_crosses }, .{ tools.item_type.chapter_places, .chapter_crosses } },
        .campaign => [_]struct { i32, GeometryChannel }{ .{ tools.item_type.campaign_chapters, .campaign_crosses }, .{ 0, .campaign_crosses } },
        .medal => return out[0..0],
    };
    for (wanted) |entry| {
        if (entry[0] == 0) continue;
        const node = tools.firstOfClass(doc, entry[0]) orelse continue;
        out[count] = .{ .node = node, .channel = entry[1] };
        count += 1;
    }
    return out[0..count];
}

/// The item a click places: one child of a list.
pub const Active = struct { list: List, index: usize };

/// SetActiveObjective and its Chapter and Campaign likes: the active item is
/// the tree's selected objective, mission, place holder or chapter. Null when
/// the selection is anything else.
pub fn activeOf(doc: *const Document, kind: Kind, selected: ?i32) ?Active {
    const id = selected orelse return null;
    const record = tools.findNode(doc, id) orelse return null;
    const props = switch (kind) {
        .mission => [_]i32{ tools.item_type.mission_objective_props, 0 },
        .chapter => [_]i32{ tools.item_type.chapter_mission_props, tools.item_type.chapter_place_props },
        .campaign => [_]i32{ tools.item_type.campaign_chapter_props, 0 },
        .medal => return null,
    };
    for (props) |class_type| {
        if (class_type == 0 or !tools.isClass(record, class_type)) continue;
        const place = tools.placeOf(doc, id) orelse return null;
        var lists: [2]List = undefined;
        for (crossLists(doc, kind, &lists)) |list| {
            if (list.node == place.parent) return .{ .list = list, .index = @intCast(place.index) };
        }
    }
    return null;
}

/// The picture's window onto the frame: `origin` is where image pixel (0, 0)
/// is on screen (the scroll offset already taken off), `size` the picture in
/// real pixels.
pub const View = struct {
    origin: Point2 = .{ .x = 0, .y = 0 },
    size: Point2 = .{ .x = 0, .y = 0 },

    pub fn toImage(self: View, screen: Point2) Point2 {
        return .{ .x = screen.x - self.origin.x, .y = screen.y - self.origin.y };
    }

    pub fn toScreen(self: View, image: Point2) Point2 {
        return .{ .x = image.x + self.origin.x, .y = image.y + self.origin.y };
    }

    /// ChapterFrm clamps every position to 0..vImageSize.
    pub fn clamp(self: View, image: Point2) Point2 {
        return .{ .x = std.math.clamp(image.x, 0, self.size.x), .y = std.math.clamp(image.y, 0, self.size.y) };
    }
};

/// A cross under the image point: ChapterFrm's FindActiveCross box (32 wide,
/// starting 15.4 before the position), first match wins. `shift` is how far
/// from the position the press landed, kept so the cross does not jump.
pub const Hit = struct { index: usize, shift: Point2 };

pub fn hitCross(points: []const Point2, image: Point2) ?Hit {
    for (points, 0..) |at, index| {
        const begin_x = at.x - cross_shift;
        const begin_y = at.y - cross_shift;
        if (image.x >= begin_x and image.x <= begin_x + cross_size and image.y >= begin_y and image.y <= begin_y + cross_size) {
            return .{ .index = index, .shift = .{ .x = image.x - at.x, .y = image.y - at.y } };
        }
    }
    return null;
}

/// One image frame's gesture state.
pub const Overlay = struct {
    allocator: std.mem.Allocator,
    mode: Mode = .place,
    view: View = .{},
    drag: ?tools.FormationDrag = null,
    index: usize = 0,
    shift: Point2 = .{ .x = 0, .y = 0 },

    pub fn init(allocator: std.mem.Allocator) Overlay {
        return .{ .allocator = allocator };
    }

    /// Drops a gesture still in progress, putting the position back.
    pub fn deinit(self: *Overlay, bridge: ResBridge) void {
        self.cancel(bridge);
    }

    pub fn busy(self: *const Overlay) bool {
        return self.drag != null;
    }

    /// OnShowCrosses: the toggle abandons the gesture in progress.
    pub fn setMode(self: *Overlay, bridge: ResBridge, mode: Mode) void {
        self.cancel(bridge);
        self.mode = mode;
    }

    /// A mouse press at `screen`. Placing puts the active item there; Show
    /// crosses grabs the cross under the cursor (a press on none starts
    /// nothing, as FindActiveCross left no active item).
    pub fn press(self: *Overlay, bridge: ResBridge, doc: *const Document, kind: Kind, active: ?Active, screen: Point2) Error!void {
        if (self.busy() or !kind.places()) return;
        const at = self.view.clamp(self.view.toImage(screen));
        switch (self.mode) {
            .place => {
                const item = active orelse return;
                self.drag = try tools.FormationDrag.beginChannel(self.allocator, bridge, item.list.node, item.list.channel);
                self.index = item.index;
                self.shift = .{ .x = 0, .y = 0 };
                self.follow(bridge, at) catch |err| {
                    self.cancel(bridge);
                    return err;
                };
            },
            .drag_crosses => {
                var lists: [2]List = undefined;
                for (crossLists(doc, kind, &lists)) |list| {
                    var read = try tools.readGeometry(bridge, list.node, list.channel);
                    defer read.deinit(self.allocator);
                    const hit = hitCross(read.points2, at) orelse continue;
                    self.drag = try tools.FormationDrag.beginChannel(self.allocator, bridge, list.node, list.channel);
                    self.index = hit.index;
                    self.shift = hit.shift;
                    return;
                }
            },
        }
    }

    /// The mouse moving with the button down: the position follows live.
    pub fn move(self: *Overlay, bridge: ResBridge, screen: Point2) Error!void {
        if (self.drag == null) return;
        try self.follow(bridge, self.view.clamp(self.view.toImage(screen)));
    }

    /// The release at `screen`: commits the gesture as one undo step, or
    /// nothing when it changed nothing.
    pub fn release(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, screen: Point2) Error!void {
        var drag = self.drag orelse return;
        self.drag = null;
        const to = self.view.clamp(.{ .x = self.view.toImage(screen).x - self.shift.x, .y = self.view.toImage(screen).y - self.shift.y });
        drag.moveSlot(bridge, self.index, to) catch |err| {
            drag.cancel(self.allocator, bridge);
            return err;
        };
        const command = drag.finish(self.allocator) orelse return;
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
    }

    /// Escape: the position goes back and nothing is recorded.
    pub fn cancel(self: *Overlay, bridge: ResBridge) void {
        if (self.drag) |*drag| drag.cancel(self.allocator, bridge);
        self.drag = null;
    }

    fn follow(self: *Overlay, bridge: ResBridge, image: Point2) Error!void {
        const drag = &self.drag.?;
        try drag.moveSlot(bridge, self.index, self.view.clamp(.{ .x = image.x - self.shift.x, .y = image.y - self.shift.y }));
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
    kind: Kind,
    lists: [2]i32 = .{ 0, 0 },
    items: [4]i32 = .{ 0, 0, 0, 0 },

    /// A chapter with two missions (100,100) (200,150) and one place holder
    /// (300,60), on a 400 x 300 picture; a mission or campaign has one list.
    fn init(allocator: std.mem.Allocator, kind: Kind) !Rig {
        var rig: Rig = .{ .fake = FakeResBridge.init(allocator), .kind = kind };
        errdefer rig.deinit(allocator);
        const bridge = rig.fake.bridge();
        try bridge_mod.check(bridge.new(switch (kind) {
            .mission => .mission,
            .chapter => .chapter,
            .campaign => .campaign,
            .medal => .medal,
        }));
        const root = rig.fake.nodes.items[0].id;
        const list_class: [2]i32 = switch (kind) {
            .mission => .{ tools.item_type.mission_objectives, 0 },
            .chapter => .{ tools.item_type.chapter_missions, tools.item_type.chapter_places },
            else => .{ tools.item_type.campaign_chapters, 0 },
        };
        const item_class: [2]i32 = switch (kind) {
            .mission => .{ tools.item_type.mission_objective_props, 0 },
            .chapter => .{ tools.item_type.chapter_mission_props, tools.item_type.chapter_place_props },
            else => .{ tools.item_type.campaign_chapter_props, 0 },
        };
        const channel: GeometryChannel = switch (kind) {
            .mission => .mission_objectives,
            .chapter => .chapter_crosses,
            else => .campaign_crosses,
        };
        var buf: [16]u8 = undefined;
        var item_count: usize = 0;
        for (0..2) |slot| {
            if (list_class[slot] == 0) continue;
            var list: i32 = 0;
            try bridge_mod.check(bridge.insertNode(root, try std.fmt.bufPrint(&buf, "{d}", .{list_class[slot]}), @intCast(slot), &list));
            rig.lists[slot] = list;
            const children: usize = if (slot == 0) 2 else 1;
            for (0..children) |child| {
                var id: i32 = 0;
                try bridge_mod.check(bridge.insertNode(list, try std.fmt.bufPrint(&buf, "{d}", .{item_class[slot]}), @intCast(child), &id));
                rig.items[item_count] = id;
                item_count += 1;
            }
            try rig.fake.addGeometryHome(list, channel);
            const seed_two = [_]Point2{ .{ .x = 100, .y = 100 }, .{ .x = 200, .y = 150 } };
            const seed_one = [_]Point2{.{ .x = 300, .y = 60 }};
            const seed: GeometryValue = .{ .points2 = @constCast(if (slot == 0) &seed_two else &seed_one) };
            try bridge_mod.check(bridge.geometryWrite(list, channel, &seed));
        }
        try rig.doc.reload(allocator, bridge);
        return rig;
    }

    fn deinit(self: *Rig, allocator: std.mem.Allocator) void {
        self.history.deinit(allocator);
        self.doc.deinit(allocator);
        self.fake.deinit();
    }

    fn overlay() Overlay {
        var o = Overlay.init(testing.allocator);
        // The picture sits 10 px right and 20 px down on screen.
        o.view = .{ .origin = .{ .x = 10, .y = 20 }, .size = .{ .x = 400, .y = 300 } };
        return o;
    }

    fn points(self: *Rig, slot: usize) ![]Point2 {
        const channel: GeometryChannel = switch (self.kind) {
            .mission => .mission_objectives,
            .chapter => .chapter_crosses,
            else => .campaign_crosses,
        };
        var read = try tools.readGeometry(self.fake.bridge(), self.lists[slot], channel);
        defer read.deinit(testing.allocator);
        return testing.allocator.dupe(Point2, read.points2);
    }

    fn active(self: *Rig, item: usize) ?Active {
        return activeOf(&self.doc, self.kind, self.items[item]);
    }
};

fn scr(x: f32, y: f32) Point2 {
    return .{ .x = x, .y = y };
}

test "image: the active item follows the tree selection" {
    var rig = try Rig.init(testing.allocator, .chapter);
    defer rig.deinit(testing.allocator);
    const mission = rig.active(1).?;
    try testing.expectEqual(rig.lists[0], mission.list.node);
    try testing.expectEqual(@as(usize, 1), mission.index);
    const place = rig.active(2).?;
    try testing.expectEqual(rig.lists[1], place.list.node);
    try testing.expectEqual(@as(usize, 0), place.index);
    // A list node, no selection or a node of another kind is no active item.
    try testing.expect(activeOf(&rig.doc, .chapter, rig.lists[0]) == null);
    try testing.expect(activeOf(&rig.doc, .chapter, null) == null);
    try testing.expect(activeOf(&rig.doc, .mission, rig.items[0]) == null);
}

test "image: a click places the active objective and undo puts it back" {
    var rig = try Rig.init(testing.allocator, .mission);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    try overlay.press(bridge, &rig.doc, .mission, rig.active(1), scr(10 + 250, 20 + 80));
    try testing.expect(overlay.busy());
    try overlay.release(bridge, &rig.doc, &rig.history, scr(10 + 250, 20 + 80));
    try testing.expect(!overlay.busy());
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    const after = try rig.points(0);
    defer testing.allocator.free(after);
    try testing.expectEqualSlices(Point2, &.{ .{ .x = 100, .y = 100 }, .{ .x = 250, .y = 80 } }, after);

    var entry = rig.history.undo_stack.pop().?;
    try rig.doc.undoOne(testing.allocator, bridge, &entry.command);
    try rig.history.redo_stack.append(testing.allocator, entry);
    const undone = try rig.points(0);
    defer testing.allocator.free(undone);
    try testing.expectEqual(Point2{ .x = 200, .y = 150 }, undone[1]);
    var redo = rig.history.redo_stack.pop().?;
    try rig.doc.redoOne(testing.allocator, bridge, &redo.command);
    try rig.history.undo_stack.append(testing.allocator, redo);
    const redone = try rig.points(0);
    defer testing.allocator.free(redone);
    try testing.expectEqual(Point2{ .x = 250, .y = 80 }, redone[1]);
}

test "image: placing clamps to the picture and follows the drag" {
    var rig = try Rig.init(testing.allocator, .mission);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    // Pressed left of and above the picture: the corner.
    try overlay.press(bridge, &rig.doc, .mission, rig.active(0), scr(-50, -50));
    var live = try rig.points(0);
    try testing.expectEqual(Point2{ .x = 0, .y = 0 }, live[0]);
    testing.allocator.free(live);
    // Dragged past the far edge: the picture's size, not beyond it.
    try overlay.move(bridge, scr(9999, 9999));
    live = try rig.points(0);
    try testing.expectEqual(Point2{ .x = 400, .y = 300 }, live[0]);
    testing.allocator.free(live);
    try overlay.release(bridge, &rig.doc, &rig.history, scr(10 + 120, 20 + 90));
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    const done = try rig.points(0);
    defer testing.allocator.free(done);
    try testing.expectEqual(Point2{ .x = 120, .y = 90 }, done[0]);
}

test "image: a press with no active item places nothing" {
    var rig = try Rig.init(testing.allocator, .campaign);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    try overlay.press(bridge, &rig.doc, .campaign, null, scr(50, 50));
    try testing.expect(!overlay.busy());
    try overlay.release(bridge, &rig.doc, &rig.history, scr(50, 50));
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
}

test "image: the cross hit box is 32 wide from 15.4 before the position" {
    const points = [_]Point2{ .{ .x = 100, .y = 100 }, .{ .x = 110, .y = 100 } };
    // Inside both boxes: the first wins.
    try testing.expectEqual(@as(usize, 0), hitCross(&points, scr(105, 105)).?.index);
    // Right of the first box (100 - 15.4 + 32 = 116.6) only the second hits.
    try testing.expectEqual(@as(usize, 1), hitCross(&points, scr(120, 100)).?.index);
    try testing.expect(hitCross(&points, scr(84.5, 100)) == null);
    try testing.expect(hitCross(&points, scr(84.7, 100)) != null);
    try testing.expect(hitCross(&points, scr(100, 116.7)) == null);
    const hit = hitCross(&points, scr(105, 98)).?;
    try testing.expectEqual(Point2{ .x = 5, .y = -2 }, hit.shift);
}

test "image: show crosses picks the cross up with its offset and drags it as one step" {
    var rig = try Rig.init(testing.allocator, .chapter);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    overlay.setMode(bridge, .drag_crosses);
    // The place holder (300, 60), grabbed 5 right and 4 below its position.
    try overlay.press(bridge, &rig.doc, .chapter, null, scr(10 + 305, 20 + 64));
    try testing.expect(overlay.busy());
    try overlay.move(bridge, scr(10 + 315, 20 + 94));
    try overlay.move(bridge, scr(10 + 325, 20 + 124));
    try overlay.release(bridge, &rig.doc, &rig.history, scr(10 + 325, 20 + 124));
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    const places = try rig.points(1);
    defer testing.allocator.free(places);
    // The grab offset stays: the cross moved by the cursor's travel, not to it.
    try testing.expectEqual(Point2{ .x = 320, .y = 120 }, places[0]);
    const missions = try rig.points(0);
    defer testing.allocator.free(missions);
    try testing.expectEqual(Point2{ .x = 100, .y = 100 }, missions[0]);

    var entry = rig.history.undo_stack.pop().?;
    try rig.doc.undoOne(testing.allocator, bridge, &entry.command);
    try rig.history.redo_stack.append(testing.allocator, entry);
    const undone = try rig.points(1);
    defer testing.allocator.free(undone);
    try testing.expectEqual(Point2{ .x = 300, .y = 60 }, undone[0]);
    var redo = rig.history.redo_stack.pop().?;
    try rig.doc.redoOne(testing.allocator, bridge, &redo.command);
    try rig.history.undo_stack.append(testing.allocator, redo);
    const redone = try rig.points(1);
    defer testing.allocator.free(redone);
    try testing.expectEqual(Point2{ .x = 320, .y = 120 }, redone[0]);
}

test "image: show crosses on empty ground starts nothing and a mission cross is found before a place holder" {
    var rig = try Rig.init(testing.allocator, .chapter);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    overlay.setMode(bridge, .drag_crosses);
    try overlay.press(bridge, &rig.doc, .chapter, null, scr(10 + 20, 20 + 250));
    try testing.expect(!overlay.busy());
    try overlay.press(bridge, &rig.doc, .chapter, null, scr(10 + 200, 20 + 150));
    try testing.expect(overlay.busy());
    try testing.expectEqual(rig.lists[0], overlay.drag.?.node);
    try testing.expectEqual(@as(usize, 1), overlay.index);
}

test "image: a cross dragged to where it was records nothing" {
    var rig = try Rig.init(testing.allocator, .campaign);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    overlay.setMode(bridge, .drag_crosses);
    try overlay.press(bridge, &rig.doc, .campaign, null, scr(10 + 100, 20 + 100));
    try overlay.move(bridge, scr(10 + 140, 20 + 140));
    try overlay.release(bridge, &rig.doc, &rig.history, scr(10 + 100, 20 + 100));
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
}

test "image: escape puts the position back and records nothing" {
    var rig = try Rig.init(testing.allocator, .chapter);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    try overlay.press(bridge, &rig.doc, .chapter, rig.active(0), scr(10 + 33, 20 + 44));
    try overlay.move(bridge, scr(10 + 55, 20 + 66));
    const live = try rig.points(0);
    try testing.expectEqual(Point2{ .x = 55, .y = 66 }, live[0]);
    testing.allocator.free(live);
    overlay.cancel(bridge);
    try testing.expect(!overlay.busy());
    const back = try rig.points(0);
    defer testing.allocator.free(back);
    try testing.expectEqual(Point2{ .x = 100, .y = 100 }, back[0]);
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
}

test "image: switching mode drops the gesture in progress" {
    var rig = try Rig.init(testing.allocator, .mission);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    try overlay.press(bridge, &rig.doc, .mission, rig.active(0), scr(60, 60));
    overlay.setMode(bridge, .drag_crosses);
    try testing.expect(!overlay.busy());
    const back = try rig.points(0);
    defer testing.allocator.free(back);
    try testing.expectEqual(Point2{ .x = 100, .y = 100 }, back[0]);
}

test "image: a medal's picture takes no positions" {
    var rig = try Rig.init(testing.allocator, .medal);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Rig.overlay();
    defer overlay.deinit(bridge);
    try overlay.press(bridge, &rig.doc, .medal, null, scr(60, 60));
    try testing.expect(!overlay.busy());
    try testing.expect(!Kind.medal.places());
    try testing.expect(Kind.chapter.hasShowCrosses() and Kind.campaign.hasShowCrosses() and !Kind.mission.hasShowCrosses());
}

test "image: a picture's real size is only trusted below the decode side" {
    try testing.expect(realSize(1024, 768));
    try testing.expect(realSize(2047, 2047));
    try testing.expect(!realSize(2048, 1200));
    try testing.expect(!realSize(100, 2048));
    try testing.expect(!realSize(0, 10));
}

test "image: the source path is the mission's map or the named picture, slashes forward" {
    var rig = try Rig.init(testing.allocator, .chapter);
    defer rig.deinit(testing.allocator);
    var buffer: [256]u8 = undefined;
    // No common properties node yet: no picture to show.
    try testing.expect(sourcePath(&buffer, &rig.doc, .chapter, "/p") == null);
    try testing.expectEqualStrings("/p/map.xml", sourcePath(&buffer, &rig.doc, .mission, "/p").?);
    try testing.expectEqualStrings("C:/p/map.xml", sourcePath(&buffer, &rig.doc, .mission, "C:\\p\\").?);

    const root = rig.fake.nodes.items[0].id;
    var buf: [16]u8 = undefined;
    var common: i32 = 0;
    try bridge_mod.check(rig.fake.bridge().insertNode(root, try std.fmt.bufPrint(&buf, "{d}", .{tools.item_type.chapter_common_props}), 2, &common));
    for (rig.fake.nodes.items) |*n| if (n.id == common) {
        var record: bridge_mod.PropRecord = .{ .id = 4 };
        _ = record.setDefault("Map image");
        _ = record.setDisplay("Map image");
        _ = record.setValue("Sub\\chapter1");
        try n.props.append(testing.allocator, record);
    };
    try rig.doc.reload(testing.allocator, rig.fake.bridge());
    try testing.expectEqualStrings("/p/Sub/chapter1.xml", sourcePath(&buffer, &rig.doc, .chapter, "/p").?);
    var tiny: [8]u8 = undefined;
    try testing.expect(sourcePath(&tiny, &rig.doc, .chapter, "/p") == null);
}
