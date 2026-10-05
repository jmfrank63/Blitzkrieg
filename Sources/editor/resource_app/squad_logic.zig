//! The squad formation overlay's logic, with no window and no ImGui: the
//! view's mapping between world and screen, which slot a click hits, and the
//! press-move-release gestures of SquadFrm's three modes (drag a member,
//! set the zero point, turn the direction arrow). Each gesture ends in one
//! command built by resource_core's sub_editor_tools and committed through
//! its `commit`, so the app's drawing code never builds a command itself.
//! The weapon and trench tree's context actions (shoot types, craters,
//! trench sources) are decided and run here for the same reason.
//! Runs under `zig build test-resource-app-logic` against the fake bridge.
const std = @import("std");
const core = @import("resource_core");

const tools = core.sub_editor_tools;
const bridge_mod = core.bridge;
const ResBridge = bridge_mod.ResBridge;
const Point2 = bridge_mod.Point2;
const Document = core.document.Document;
const History = core.history.History;

pub const Error = bridge_mod.EditError;

/// SquadFrm's three tool modes: moving a member, placing the zero point and
/// turning the direction arrow.
pub const Mode = enum { drag, set_zero, direction };

/// How far from a member's marker, in screen pixels, a press still hits it.
pub const hit_radius_px: f32 = 10;

/// The overlay's window onto the formation's plane: a screen point is the
/// origin plus the world point times the scale.
pub const View = struct {
    origin: Point2 = .{ .x = 0, .y = 0 },
    scale: f32 = 1,

    pub fn toScreen(self: View, world: Point2) Point2 {
        return .{ .x = self.origin.x + world.x * self.scale, .y = self.origin.y + world.y * self.scale };
    }

    pub fn toWorld(self: View, screen: Point2) Point2 {
        return .{ .x = (screen.x - self.origin.x) / self.scale, .y = (screen.y - self.origin.y) / self.scale };
    }
};

/// The member whose marker is nearest the screen point and within
/// `hit_radius_px` of it; the later member wins a tie, as it is drawn on top.
pub fn hitSlot(slots: []const Point2, view: View, screen: Point2) ?usize {
    var best: ?usize = null;
    var best_distance: f32 = hit_radius_px * hit_radius_px;
    for (slots, 0..) |slot, index| {
        const at = view.toScreen(slot);
        const dx = at.x - screen.x;
        const dy = at.y - screen.y;
        const distance = dx * dx + dy * dy;
        if (distance <= best_distance) {
            best_distance = distance;
            best = index;
        }
    }
    return best;
}

/// The direction in radians an arrow drawn from `centre` to `cursor` (both
/// world points) points in.
pub fn arrowAngle(centre: Point2, cursor: Point2) f32 {
    return std.math.atan2(cursor.y - centre.y, cursor.x - centre.x);
}

/// One formation's overlay: the mode, the view and the gesture in progress.
pub const Overlay = struct {
    allocator: std.mem.Allocator,
    formation: i32,
    mode: Mode = .drag,
    view: View = .{},
    drag: ?tools.FormationDrag = null,
    slot: usize = 0,
    /// Where in the marker the press landed, so the member does not jump to
    /// the cursor.
    grab: Point2 = .{ .x = 0, .y = 0 },
    /// The arrow gesture in progress, and the angle it would commit.
    arrowing: bool = false,
    arrow_angle: f32 = 0,

    pub fn init(allocator: std.mem.Allocator, formation: i32) Overlay {
        return .{ .allocator = allocator, .formation = formation };
    }

    /// Drops a gesture still in progress, putting the slots back.
    pub fn deinit(self: *Overlay, bridge: ResBridge) void {
        self.cancel(bridge);
    }

    pub fn busy(self: *const Overlay) bool {
        return self.drag != null or self.arrowing;
    }

    /// Switching mode abandons the gesture in progress.
    pub fn setMode(self: *Overlay, bridge: ResBridge, mode: Mode) void {
        self.cancel(bridge);
        self.mode = mode;
    }

    /// A mouse press at `screen`. In drag mode it grabs the member under the
    /// cursor (a press on nothing starts nothing); in direction mode it
    /// starts the arrow; set-zero acts on the release.
    pub fn press(self: *Overlay, bridge: ResBridge, screen: Point2) Error!void {
        if (self.busy()) return;
        switch (self.mode) {
            .drag => {
                var read = try tools.readGeometry(bridge, self.formation, .formation_positions);
                defer read.deinit(self.allocator);
                const hit = hitSlot(read.points2, self.view, screen) orelse return;
                const world = self.view.toWorld(screen);
                self.grab = .{ .x = read.points2[hit].x - world.x, .y = read.points2[hit].y - world.y };
                self.slot = hit;
                self.drag = try tools.FormationDrag.begin(self.allocator, bridge, self.formation);
            },
            .direction => {
                self.arrowing = true;
                try self.aim(bridge, screen);
            },
            .set_zero => {},
        }
    }

    /// The mouse moving with the button down: the member follows live and
    /// the arrow turns.
    pub fn move(self: *Overlay, bridge: ResBridge, screen: Point2) Error!void {
        if (self.drag) |*drag| {
            const world = self.view.toWorld(screen);
            try drag.moveSlot(bridge, self.slot, .{ .x = world.x + self.grab.x, .y = world.y + self.grab.y });
        } else if (self.arrowing) {
            try self.aim(bridge, screen);
        }
    }

    /// The release at `screen`: commits the gesture as one undo step, or
    /// nothing when it changed nothing.
    pub fn release(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, screen: Point2) Error!void {
        switch (self.mode) {
            .drag => {
                var drag = self.drag orelse return;
                self.drag = null;
                const command = drag.finish(self.allocator) orelse return;
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
            .set_zero => {
                const command = try tools.setZeroPoint(self.allocator, bridge, self.formation, self.view.toWorld(screen));
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
            .direction => {
                if (!self.arrowing) return;
                try self.aim(bridge, screen);
                self.arrowing = false;
                const command = try tools.setFormationDirection(self.allocator, bridge, self.formation, self.arrow_angle, null);
                try tools.commit(self.allocator, bridge, doc, history, command, 0);
            },
        }
    }

    /// Escape: the slots go back and nothing is recorded.
    pub fn cancel(self: *Overlay, bridge: ResBridge) void {
        if (self.drag) |*drag| drag.cancel(self.allocator, bridge);
        self.drag = null;
        self.arrowing = false;
    }

    fn aim(self: *Overlay, bridge: ResBridge, screen: Point2) Error!void {
        const zero = try tools.readGeometry(bridge, self.formation, .zero_point);
        self.arrow_angle = arrowAngle(zero.point2, self.view.toWorld(screen));
    }
};

// --- Weapon and trench tree actions ------------------------------------------

/// What the tree's context menu offers on a node of the weapon or trench
/// tree, each one T04 helper.
pub const TreeAction = enum {
    add_shoot_type,
    delete_shoot_type,
    add_crater,
    delete_crater,
    add_source,
    remove_source,

    pub fn label(self: TreeAction) [:0]const u8 {
        return switch (self) {
            .add_shoot_type => "Add shoot type",
            .delete_shoot_type => "Delete shoot type",
            .add_crater => "Add crater",
            .delete_crater => "Delete crater",
            .add_source => "Add source",
            .remove_source => "Remove source",
        };
    }
};

pub const TreeActions = struct {
    items: [2]TreeAction = undefined,
    len: usize = 0,

    fn appendAssumeCapacity(self: *TreeActions, action: TreeAction) void {
        self.items[self.len] = action;
        self.len += 1;
    }

    pub fn constSlice(self: *const TreeActions) []const TreeAction {
        return self.items[0..self.len];
    }
};

/// The actions that apply to `node`, at most two. A shoot type offers its
/// own delete and a new crater; the Shoot types item a new shoot type; a
/// crater its delete; a source list a new source and a source its removal.
pub fn treeActionsFor(doc: *const Document, node: i32) TreeActions {
    var out: TreeActions = .{};
    const record = tools.findNode(doc, node) orelse return out;
    if (tools.isClass(record, tools.item_type.weapon_shoot_types)) {
        out.appendAssumeCapacity(.add_shoot_type);
    } else if (tools.isClass(record, tools.item_type.weapon_damage_props)) {
        out.appendAssumeCapacity(.delete_shoot_type);
        if (tools.weaponPartNode(doc, node, .craters) != null) out.appendAssumeCapacity(.add_crater);
    } else if (tools.isClass(record, tools.item_type.weapon_crater_props)) {
        out.appendAssumeCapacity(.delete_crater);
    } else if (tools.isClass(record, tools.item_type.trench_sources)) {
        out.appendAssumeCapacity(.add_source);
    } else if (tools.isClass(record, tools.item_type.trench_source_props)) {
        out.appendAssumeCapacity(.remove_source);
    }
    return out;
}

/// Runs `action` on `node` as one undo step.
pub fn runTreeAction(allocator: std.mem.Allocator, bridge: ResBridge, doc: *Document, history: *History, action: TreeAction, node: i32) Error!void {
    const command = switch (action) {
        .add_shoot_type => try tools.weaponAddShootType(allocator, doc),
        .delete_shoot_type => try tools.weaponDeleteShootType(doc, node),
        .add_crater => try tools.weaponAddCrater(allocator, doc, node),
        .delete_crater => try tools.weaponDeleteCrater(doc, node),
        .add_source => try tools.trenchAddSource(allocator, doc, node),
        .remove_source => try tools.trenchRemoveSource(doc, node),
    };
    try tools.commit(allocator, bridge, doc, history, command, 0);
}

// --- Tests -------------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;
const GeometryChannel = bridge_mod.GeometryChannel;
const GeometryValue = bridge_mod.GeometryValue;

const Rig = struct {
    fake: FakeResBridge,
    doc: Document = .{},
    history: History = .{},
    formation: i32 = 0,

    fn init(allocator: std.mem.Allocator) !Rig {
        var rig: Rig = .{ .fake = FakeResBridge.init(allocator) };
        errdefer rig.deinit(allocator);
        const bridge = rig.fake.bridge();
        try bridge_mod.check(bridge.new(.squad));
        const root = rig.fake.nodes.items[0].id;
        var buf: [16]u8 = undefined;
        var id: i32 = 0;
        try bridge_mod.check(bridge.insertNode(root, try std.fmt.bufPrint(&buf, "{d}", .{tools.item_type.squad_formation_props}), 0, &id));
        rig.formation = id;
        for ([_]GeometryChannel{ .formation_positions, .zero_point, .formation_direction }) |channel|
            try rig.fake.addGeometryHome(id, channel);
        const slots = [_]Point2{ .{ .x = 100, .y = 100 }, .{ .x = 140, .y = 100 }, .{ .x = 120, .y = 140 } };
        const seeds = [_]struct { GeometryChannel, GeometryValue }{
            .{ .formation_positions, .{ .points2 = @constCast(&slots) } },
            .{ .zero_point, .{ .point2 = .{ .x = 120, .y = 120 } } },
            .{ .formation_direction, .{ .point2 = .{ .x = 0, .y = 0 } } },
        };
        for (seeds) |seed| try bridge_mod.check(bridge.geometryWrite(id, seed[0], &seed[1]));
        try rig.doc.reload(allocator, bridge);
        return rig;
    }

    fn deinit(self: *Rig, allocator: std.mem.Allocator) void {
        self.history.deinit(allocator);
        self.doc.deinit(allocator);
        self.fake.deinit();
    }

    fn read(self: *Rig, channel: GeometryChannel) !GeometryValue {
        return tools.readGeometry(self.fake.bridge(), self.formation, channel);
    }
};

test "overlay: the view maps both ways" {
    const view: View = .{ .origin = .{ .x = 50, .y = 20 }, .scale = 2 };
    const screen = view.toScreen(.{ .x = 10, .y = 5 });
    try testing.expectEqual(Point2{ .x = 70, .y = 30 }, screen);
    try testing.expectEqual(Point2{ .x = 10, .y = 5 }, view.toWorld(screen));
}

test "overlay: a press hits the nearest marker within the radius and nothing past it" {
    const slots = [_]Point2{ .{ .x = 100, .y = 100 }, .{ .x = 106, .y = 100 } };
    const view: View = .{};
    try testing.expectEqual(@as(?usize, 1), hitSlot(&slots, view, .{ .x = 105, .y = 100 }));
    try testing.expectEqual(@as(?usize, 0), hitSlot(&slots, view, .{ .x = 98, .y = 101 }));
    try testing.expectEqual(@as(?usize, null), hitSlot(&slots, view, .{ .x = 130, .y = 100 }));
    // The radius is in screen pixels: zoomed out, the same world distance is closer.
    try testing.expectEqual(@as(?usize, 0), hitSlot(&slots, .{ .scale = 0.5 }, .{ .x = 49, .y = 50 }));
}

test "overlay: dragging a member is one undo step and undo puts it back" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Overlay.init(testing.allocator, rig.formation);
    defer overlay.deinit(bridge);
    // Press 2 px off the marker so the grab offset matters.
    try overlay.press(bridge, .{ .x = 142, .y = 100 });
    try testing.expect(overlay.busy());
    try overlay.move(bridge, .{ .x = 152, .y = 110 });
    try overlay.move(bridge, .{ .x = 162, .y = 120 });
    try overlay.release(bridge, &rig.doc, &rig.history, .{ .x = 162, .y = 120 });
    try testing.expect(!overlay.busy());
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    var after = try rig.read(.formation_positions);
    defer after.deinit(testing.allocator);
    try testing.expectEqual(Point2{ .x = 160, .y = 120 }, after.points2[1]);
    try testing.expectEqual(Point2{ .x = 100, .y = 100 }, after.points2[0]);

    var entry = rig.history.undo_stack.pop().?;
    try rig.doc.undoOne(testing.allocator, bridge, &entry.command);
    try rig.history.redo_stack.append(testing.allocator, entry);
    var undone = try rig.read(.formation_positions);
    defer undone.deinit(testing.allocator);
    try testing.expectEqual(Point2{ .x = 140, .y = 100 }, undone.points2[1]);
}

test "overlay: a press on empty space and a click without a move record nothing" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Overlay.init(testing.allocator, rig.formation);
    defer overlay.deinit(bridge);
    try overlay.press(bridge, .{ .x = 300, .y = 300 });
    try testing.expect(!overlay.busy());
    try overlay.release(bridge, &rig.doc, &rig.history, .{ .x = 300, .y = 300 });
    try overlay.press(bridge, .{ .x = 100, .y = 100 });
    try overlay.release(bridge, &rig.doc, &rig.history, .{ .x = 100, .y = 100 });
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
}

test "overlay: escape during a drag puts the slots back and records nothing" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Overlay.init(testing.allocator, rig.formation);
    defer overlay.deinit(bridge);
    try overlay.press(bridge, .{ .x = 100, .y = 100 });
    try overlay.move(bridge, .{ .x = 200, .y = 200 });
    overlay.cancel(bridge);
    var slots = try rig.read(.formation_positions);
    defer slots.deinit(testing.allocator);
    try testing.expectEqual(Point2{ .x = 100, .y = 100 }, slots.points2[0]);
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
}

test "overlay: set zero point is one undoable step at the clicked world point" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Overlay.init(testing.allocator, rig.formation);
    defer overlay.deinit(bridge);
    overlay.view = .{ .origin = .{ .x = 10, .y = 10 }, .scale = 2 };
    overlay.setMode(bridge, .set_zero);
    try overlay.press(bridge, .{ .x = 70, .y = 50 });
    try overlay.release(bridge, &rig.doc, &rig.history, .{ .x = 70, .y = 50 });
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    const zero = try rig.read(.zero_point);
    try testing.expectEqual(Point2{ .x = 30, .y = 20 }, zero.point2);
}

test "overlay: the direction arrow turns the formation and its slots in one step" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Overlay.init(testing.allocator, rig.formation);
    defer overlay.deinit(bridge);
    overlay.setMode(bridge, .direction);
    // From the zero point (120, 120) straight down: a quarter turn.
    try overlay.press(bridge, .{ .x = 120, .y = 150 });
    try testing.expect(overlay.busy());
    try overlay.move(bridge, .{ .x = 120, .y = 170 });
    try testing.expectApproxEqAbs(@as(f32, std.math.pi / 2.0), overlay.arrow_angle, 1e-5);
    try overlay.release(bridge, &rig.doc, &rig.history, .{ .x = 120, .y = 170 });
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    const direction = try rig.read(.formation_direction);
    try testing.expectApproxEqAbs(@as(f32, std.math.pi / 2.0), direction.point2.x, 1e-5);
    var slots = try rig.read(.formation_positions);
    defer slots.deinit(testing.allocator);
    // (100, 100) is 20 left and 20 up of the zero point; a quarter turn puts it 20 right and 20 up.
    try testing.expectApproxEqAbs(@as(f32, 140), slots.points2[0].x, 1e-3);
    try testing.expectApproxEqAbs(@as(f32, 100), slots.points2[0].y, 1e-3);
}

test "overlay: changing mode drops the gesture in progress" {
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    var overlay = Overlay.init(testing.allocator, rig.formation);
    defer overlay.deinit(bridge);
    try overlay.press(bridge, .{ .x = 100, .y = 100 });
    try overlay.move(bridge, .{ .x = 180, .y = 180 });
    overlay.setMode(bridge, .set_zero);
    try testing.expect(!overlay.busy());
    var slots = try rig.read(.formation_positions);
    defer slots.deinit(testing.allocator);
    try testing.expectEqual(Point2{ .x = 100, .y = 100 }, slots.points2[0]);
}

fn expectActions(rig: *Rig, node: i32, want: []const TreeAction) !void {
    const found = treeActionsFor(&rig.doc, node);
    try testing.expectEqualSlices(TreeAction, want, found.constSlice());
}

fn addNode(rig: *Rig, parent: i32, class_type: i32) !i32 {
    var buf: [16]u8 = undefined;
    var id: i32 = 0;
    try bridge_mod.check(rig.fake.bridge().insertNode(parent, try std.fmt.bufPrint(&buf, "{d}", .{class_type}), tools.childCount(&rig.doc, parent), &id));
    try rig.doc.reload(testing.allocator, rig.fake.bridge());
    return id;
}

test "tree actions: the weapon's shoot types and craters and the trench's sources" {
    const item_type = tools.item_type;
    var rig = try Rig.init(testing.allocator);
    defer rig.deinit(testing.allocator);
    const bridge = rig.fake.bridge();
    const root = rig.fake.nodes.items[0].id;
    const shoot_types = try addNode(&rig, root, item_type.weapon_shoot_types);
    const shell = try addNode(&rig, shoot_types, item_type.weapon_damage_props);
    const craters = try addNode(&rig, shell, item_type.weapon_craters);
    const sources = try addNode(&rig, root, item_type.trench_sources);

    try expectActions(&rig, shoot_types, &.{.add_shoot_type});
    try expectActions(&rig, shell, &.{ .delete_shoot_type, .add_crater });
    try expectActions(&rig, sources, &.{.add_source});
    try expectActions(&rig, craters, &.{});

    // Each action is one step that undo takes back.
    const before = tools.childCount(&rig.doc, shoot_types);
    try runTreeAction(testing.allocator, bridge, &rig.doc, &rig.history, .add_shoot_type, shoot_types);
    try testing.expectEqual(before + 1, tools.childCount(&rig.doc, shoot_types));
    try runTreeAction(testing.allocator, bridge, &rig.doc, &rig.history, .add_crater, shell);
    const crater = tools.childOfClass(&rig.doc, craters, item_type.weapon_crater_props, 0).?;
    try expectActions(&rig, crater, &.{.delete_crater});
    try runTreeAction(testing.allocator, bridge, &rig.doc, &rig.history, .delete_crater, crater);
    try runTreeAction(testing.allocator, bridge, &rig.doc, &rig.history, .add_source, sources);
    const source = tools.childOfClass(&rig.doc, sources, item_type.trench_source_props, 0).?;
    try expectActions(&rig, source, &.{.remove_source});
    try runTreeAction(testing.allocator, bridge, &rig.doc, &rig.history, .remove_source, source);
    try runTreeAction(testing.allocator, bridge, &rig.doc, &rig.history, .delete_shoot_type, shell);
    try testing.expectEqual(@as(usize, 6), rig.history.undo_stack.items.len);
    try testing.expectEqual(@as(?i32, null), tools.childOfClass(&rig.doc, sources, item_type.trench_source_props, 0));

    // An action that does not fit the node is refused and records nothing.
    try testing.expectError(error.Refused, runTreeAction(testing.allocator, bridge, &rig.doc, &rig.history, .delete_crater, sources));
    try testing.expectEqual(@as(usize, 6), rig.history.undo_stack.items.len);
}
