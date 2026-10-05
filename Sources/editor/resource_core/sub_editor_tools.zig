//! The S06 sub-editors' tools as ResourceCommand builders: the squad
//! formation's slot drag, zero point and direction arrow, the weapon tree's
//! shoot types, damage, sound, effect, flash and crater edits, and the
//! trench's sources. Each builder reads the state the command needs (the
//! document mirror for tree and props, the bridge for geometry) and returns
//! a command the caller applies and records, usually through `commit`, so
//! undo, redo, save and autosave come with every tool and the app only picks
//! which builder to call.
//!
//! Geometry values are read through the bridge, which allocates them with
//! its own allocator; a builder's `allocator` must be that same allocator,
//! as everywhere a GeometryValue changes hands in the core.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const document_mod = @import("document.zig");
const history_mod = @import("history.zig");

const ResBridge = bridge_mod.ResBridge;
const EditError = bridge_mod.EditError;
const GeometryChannel = bridge_mod.GeometryChannel;
const GeometryValue = bridge_mod.GeometryValue;
const Point2 = bridge_mod.Point2;
const Document = document_mod.Document;
const History = history_mod.History;
const ResourceCommand = history_mod.ResourceCommand;
const OwnedBytes = history_mod.OwnedBytes;

/// The tree item class types these tools insert and look for, as
/// Sources/src/ResourceModel/items/tree_item_types.h numbers them (MFC's
/// EDITOR_TREE_BASE_VALUE plus the offsets of TreeItem.h). A node's class
/// name on the bridge is this integer in decimal.
pub const item_type = struct {
    pub const base: i32 = 0x11000000;
    pub const weapon_root: i32 = base + 81;
    pub const weapon_common_props: i32 = base + 82;
    pub const weapon_shoot_types: i32 = base + 83;
    pub const weapon_damage_props: i32 = base + 84;
    pub const weapon_sound_props: i32 = base + 85;
    pub const weapon_effects: i32 = base + 86;
    pub const weapon_effect_props: i32 = base + 87;
    pub const weapon_flash_props: i32 = base + 88;
    pub const weapon_craters: i32 = base + 281;
    pub const weapon_crater_props: i32 = base + 282;
    pub const trench_root: i32 = base + 151;
    pub const trench_common_props: i32 = base + 152;
    pub const trench_sources: i32 = base + 153;
    pub const trench_source_props: i32 = base + 154;
    pub const squad_root: i32 = base + 161;
    pub const squad_formations: i32 = base + 165;
    pub const squad_formation_props: i32 = base + 166;
    pub const animation_root: i32 = base + 1;
    pub const unit_directory_props: i32 = base + 7;
    pub const unit_season_props: i32 = base + 8;
    pub const unit_directories: i32 = base + 9;
    pub const unit_animations: i32 = base + 10;
    pub const unit_animation_props: i32 = base + 11;
    pub const unit_frame_props: i32 = base + 12;
    pub const sprite_root: i32 = base + 21;
    pub const sprites: i32 = base + 22;
    pub const sprite_props: i32 = base + 23;
    pub const mesh_root: i32 = base + 61;
    pub const mesh_graphics: i32 = base + 65;
    pub const mesh_platforms: i32 = base + 71;
    pub const mesh_platform_props: i32 = base + 72;
    pub const mesh_guns: i32 = base + 73;
    pub const mesh_gun_props: i32 = base + 74;
    pub const mesh_locators: i32 = base + 77;
    pub const mesh_locator_props: i32 = base + 78;
    pub const object_root: i32 = base + 51;
    pub const fence_root: i32 = base + 121;
    pub const fence_common_props: i32 = base + 122;
    pub const fence_direction: i32 = base + 123;
    pub const fence_insert: i32 = base + 124;
    pub const fence_props: i32 = base + 125;
    pub const building_root: i32 = base + 91;
    pub const building_slots: i32 = base + 95;
    pub const building_slot_props: i32 = base + 96;
    pub const building_smokes: i32 = base + 260;
    pub const building_fire_points: i32 = base + 268;
    pub const building_fire_point_props: i32 = base + 269;
    pub const building_dir_explosions: i32 = base + 270;
    pub const building_dir_explosion_props: i32 = base + 271;
    pub const building_smoke_props: i32 = base + 272;
    pub const bridge_root: i32 = base + 220;
    pub const bridge_parts: i32 = base + 225;
    pub const bridge_fire_points: i32 = base + 330;
    pub const bridge_fire_point_props: i32 = base + 331;
    pub const bridge_dir_explosions: i32 = base + 332;
    pub const bridge_dir_explosion_props: i32 = base + 333;
    pub const bridge_smokes: i32 = base + 334;
    pub const bridge_smoke_props: i32 = base + 335;
};

// --- Reading the mirror ------------------------------------------------------

/// Whether the node was built from `class_type`.
pub fn isClass(node: *const document_mod.Node, class_type: i32) bool {
    const value = std.fmt.parseInt(i32, node.classSlice(), 10) catch return false;
    return value == class_type;
}

pub fn findNode(doc: *const Document, id: i32) ?*const document_mod.Node {
    for (doc.tree.nodes.items) |*node| if (node.id == id) return node;
    return null;
}

/// The first node of `class_type` in tree order, anywhere in the project.
pub fn firstOfClass(doc: *const Document, class_type: i32) ?i32 {
    for (doc.tree.nodes.items) |*node| if (isClass(node, class_type)) return node.id;
    return null;
}

/// The `nth` child of `parent` that is of `class_type`, in child order.
pub fn childOfClass(doc: *const Document, parent: i32, class_type: i32, nth: usize) ?i32 {
    var seen: usize = 0;
    for (doc.tree.nodes.items) |*node| {
        if (node.parent != parent or !isClass(node, class_type)) continue;
        if (seen == nth) return node.id;
        seen += 1;
    }
    return null;
}

pub fn childCount(doc: *const Document, parent: i32) i32 {
    var count: i32 = 0;
    for (doc.tree.nodes.items) |node| if (node.parent == parent) {
        count += 1;
    };
    return count;
}

/// Where a node sits: its parent and its index among the parent's children.
pub const Place = struct { parent: i32, index: i32 };

pub fn placeOf(doc: *const Document, id: i32) ?Place {
    const node = findNode(doc, id) orelse return null;
    var index: i32 = 0;
    for (doc.tree.nodes.items) |peer| {
        if (peer.id == id) return .{ .parent = node.parent, .index = index };
        if (peer.parent == node.parent) index += 1;
    }
    return null;
}

/// A property's id by its default (untranslated) name, as MFC's SProp names
/// it, so a tool does not hard-code the numbering of each item's table.
pub fn propIdByName(doc: *const Document, node: i32, default_name: []const u8) ?i32 {
    for (doc.tree.props.items) |*p| {
        if (p.node == node and std.mem.eql(u8, p.record.defaultSlice(), default_name)) return p.record.id;
    }
    return null;
}

pub fn propValue(doc: *const Document, node: i32, prop_id: i32) ?[]const u8 {
    for (doc.tree.props.items) |*p| {
        if (p.node == node and p.record.id == prop_id) return p.record.valueSlice();
    }
    return null;
}

/// One geometry channel of a node, through the bridge. Refused where the
/// channel has no MFC home on that node.
pub fn readGeometry(bridge: ResBridge, node: i32, channel: GeometryChannel) EditError!GeometryValue {
    var out: GeometryValue = undefined;
    try bridge_mod.check(bridge.geometryRead(node, channel, &out));
    return out;
}

// --- Generic builders --------------------------------------------------------

/// A property write from the mirror's current text to `text`.
pub fn setProp(allocator: std.mem.Allocator, doc: *const Document, node: i32, prop_id: i32, text: []const u8) EditError!ResourceCommand {
    const before = propValue(doc, node, prop_id) orelse return error.Refused;
    var before_bytes = try OwnedBytes.fromSlice(allocator, before);
    errdefer before_bytes.deinit(allocator);
    const after_bytes = try OwnedBytes.fromSlice(allocator, text);
    return .{ .set_prop = .{ .node = node, .prop_id = prop_id, .before = before_bytes, .after = after_bytes } };
}

/// `setProp` for the property MFC names `default_name`.
pub fn setPropByName(allocator: std.mem.Allocator, doc: *const Document, node: i32, default_name: []const u8, text: []const u8) EditError!ResourceCommand {
    const prop_id = propIdByName(doc, node, default_name) orelse return error.Refused;
    return setProp(allocator, doc, node, prop_id, text);
}

/// A new node of `class_type` at parent/index.
pub fn insertChild(allocator: std.mem.Allocator, parent: i32, class_type: i32, index: i32) EditError!ResourceCommand {
    const class_name = try std.fmt.allocPrint(allocator, "{d}", .{class_type});
    return .{ .insert_node = .{ .parent = parent, .class_name = class_name, .index = index, .new_id = -1 } };
}

/// A new node of `class_type` after the parent's last child, where MFC's
/// tree puts an inserted item.
pub fn appendChild(allocator: std.mem.Allocator, doc: *const Document, parent: i32, class_type: i32) EditError!ResourceCommand {
    if (findNode(doc, parent) == null) return error.Refused;
    return insertChild(allocator, parent, class_type, childCount(doc, parent));
}

/// A delete of `node` and its subtree; the blob is filled when it is applied.
pub fn deleteNode(doc: *const Document, node: i32) EditError!ResourceCommand {
    const place = placeOf(doc, node) orelse return error.Refused;
    if (place.parent == -1 or place.parent == 0) return error.Refused;
    return .{ .delete_node = .{ .parent = place.parent, .index = place.index, .node = node, .blob = .{} } };
}

/// A geometry write of `after` (copied) with the channel's current value as
/// `before`. A channel with no MFC home on the node is refused here, before
/// any command exists, so the history never sees it.
pub fn setGeometry(allocator: std.mem.Allocator, bridge: ResBridge, node: i32, channel: GeometryChannel, after: GeometryValue) EditError!ResourceCommand {
    if (std.meta.activeTag(after) != channel.family()) return error.BadArgument;
    var before = try readGeometry(bridge, node, channel);
    errdefer before.deinit(allocator);
    const owned_after = try after.dupe(allocator);
    return .{ .geometry = .{ .node = node, .channel = channel, .before = before, .after = owned_after } };
}

/// Applies `command` through the bridge and records it as one undo step.
/// Takes ownership of the command: on any error it is freed and the history
/// is as it was.
pub fn commit(allocator: std.mem.Allocator, bridge: ResBridge, doc: *Document, history: *History, command: ResourceCommand, gesture: u32) EditError!void {
    var owned = command;
    history.reserve(allocator) catch |err| {
        owned.deinit(allocator);
        return err;
    };
    doc.apply(allocator, bridge, &owned) catch |err| {
        owned.deinit(allocator);
        return err;
    };
    history.recordAssumeCapacity(allocator, owned, gesture);
}

// --- Squad (SquadFrm) --------------------------------------------------------

/// A slot drag in SquadFrm's view: every mouse move writes the slots live so
/// the view follows, and the release yields ONE geometry command from the
/// slots at the press to the slots at the release, so a drag undoes in one
/// step however many moves it took.
pub const FormationDrag = struct {
    node: i32,
    before: []Point2,
    current: []Point2,
    moved: bool = false,

    /// Reads the formation's slots at the press.
    pub fn begin(allocator: std.mem.Allocator, bridge: ResBridge, formation: i32) EditError!FormationDrag {
        var read = try readGeometry(bridge, formation, .formation_positions);
        errdefer read.deinit(allocator);
        const current = try allocator.dupe(Point2, read.points2);
        return .{ .node = formation, .before = read.points2, .current = current };
    }

    /// Moves one slot and writes the slots through the bridge.
    pub fn moveSlot(self: *FormationDrag, bridge: ResBridge, slot: usize, to: Point2) EditError!void {
        if (slot >= self.current.len) return error.BadArgument;
        self.current[slot] = to;
        self.moved = true;
        const live: GeometryValue = .{ .points2 = self.current };
        try bridge_mod.check(bridge.geometryWrite(self.node, .formation_positions, &live));
    }

    /// The release: the drag's one command, or null when no slot ended
    /// anywhere new (a click). The drag is spent either way.
    pub fn finish(self: *FormationDrag, allocator: std.mem.Allocator) ?ResourceCommand {
        if (!self.moved or std.mem.eql(u8, std.mem.sliceAsBytes(self.before), std.mem.sliceAsBytes(self.current))) {
            self.deinit(allocator);
            return null;
        }
        const command: ResourceCommand = .{ .geometry = .{
            .node = self.node,
            .channel = .formation_positions,
            .before = .{ .points2 = self.before },
            .after = .{ .points2 = self.current },
        } };
        self.before = &.{};
        self.current = &.{};
        return command;
    }

    /// Escape during a drag: the slots go back to where they were.
    pub fn cancel(self: *FormationDrag, allocator: std.mem.Allocator, bridge: ResBridge) void {
        if (self.moved) {
            const back: GeometryValue = .{ .points2 = self.before };
            _ = bridge.geometryWrite(self.node, .formation_positions, &back);
        }
        self.deinit(allocator);
    }

    pub fn deinit(self: *FormationDrag, allocator: std.mem.Allocator) void {
        if (self.before.len != 0) allocator.free(self.before);
        if (self.current.len != 0) allocator.free(self.current);
        self.before = &.{};
        self.current = &.{};
    }
};

/// SquadFrm's Set Zero click: the formation's ZeroPos.
pub fn setZeroPoint(allocator: std.mem.Allocator, bridge: ResBridge, formation: i32, to: Point2) EditError!ResourceCommand {
    return setGeometry(allocator, bridge, formation, .zero_point, .{ .point2 = to });
}

/// The slots turned by `delta` radians about `centre`, as SquadFrm's
/// CalculateNewPositions turns them.
pub fn turnSlots(allocator: std.mem.Allocator, slots: []const Point2, centre: Point2, delta: f32) ![]Point2 {
    const out = try allocator.alloc(Point2, slots.len);
    const c = @cos(delta);
    const s = @sin(delta);
    for (slots, out) |slot, *turned| {
        const dx = slot.x - centre.x;
        const dy = slot.y - centre.y;
        turned.* = .{ .x = centre.x + dx * c - dy * s, .y = centre.y + dx * s + dy * c };
    }
    return out;
}

/// SquadFrm's direction arrow in Set Zero mode (WM_ANGLE_CHANGED): the
/// formation's FormationDir becomes `angle` (radians) and every slot turns
/// by the change about the formation's centre, both in one undo step.
/// `centre` is where the view draws the zero cross; null takes the stored
/// zero point. (MFC turns about the cross, which it draws 15.4 pixels off
/// ZeroPos on screen; that offset is a view matter the app resolves.)
/// The arrow in drag mode turns one slot's own Dir, which the bridge does
/// not carry yet - see the S06 parity notes.
pub fn setFormationDirection(allocator: std.mem.Allocator, bridge: ResBridge, formation: i32, angle: f32, centre: ?Point2) EditError!ResourceCommand {
    var direction_before = try readGeometry(bridge, formation, .formation_direction);
    errdefer direction_before.deinit(allocator);
    var slots_before = try readGeometry(bridge, formation, .formation_positions);
    errdefer slots_before.deinit(allocator);
    const pivot = centre orelse blk: {
        const zero = try readGeometry(bridge, formation, .zero_point);
        break :blk zero.point2;
    };
    const slots_after = try turnSlots(allocator, slots_before.points2, pivot, angle - direction_before.point2.x);
    errdefer allocator.free(slots_after);

    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    errdefer steps.deinit(allocator);
    try steps.ensureTotalCapacity(allocator, 2);
    steps.appendAssumeCapacity(.{ .geometry = .{
        .node = formation,
        .channel = .formation_direction,
        .before = direction_before,
        .after = .{ .point2 = .{ .x = angle, .y = 0 } },
    } });
    steps.appendAssumeCapacity(.{ .geometry = .{
        .node = formation,
        .channel = .formation_positions,
        .before = slots_before,
        .after = .{ .points2 = slots_after },
    } });
    return .{ .composite = .{ .steps = steps } };
}

// --- Weapon (WeaponFrm) ------------------------------------------------------

/// The parts of one shoot type (a damage props item) the weapon tools edit.
/// MFC keeps the sound and effect choices on the shoot type's Effects item
/// (CWeaponEffectsItem: "Human fire sound", "Gun fire effect", ...), the two
/// flashes on its Flash fire / Flash explosion items, and the crater
/// directory on its Picture craters item. Effects and flashes are static
/// children MFC neither inserts nor deletes (bStaticElements), so they only
/// take property edits.
pub const WeaponPart = enum { damage, sound, effect, flash_fire, flash_explosion, craters };

pub fn weaponPartNode(doc: *const Document, shoot_type: i32, part: WeaponPart) ?i32 {
    return switch (part) {
        .damage => if (findNode(doc, shoot_type)) |node| (if (isClass(node, item_type.weapon_damage_props)) shoot_type else null) else null,
        .sound, .effect => childOfClass(doc, shoot_type, item_type.weapon_effects, 0),
        .flash_fire => childOfClass(doc, shoot_type, item_type.weapon_flash_props, 0),
        .flash_explosion => childOfClass(doc, shoot_type, item_type.weapon_flash_props, 1),
        .craters => childOfClass(doc, shoot_type, item_type.weapon_craters, 0),
    };
}

/// A new shoot type at the end of the weapon's Shoot types.
pub fn weaponAddShootType(allocator: std.mem.Allocator, doc: *const Document) EditError!ResourceCommand {
    const shoot_types = firstOfClass(doc, item_type.weapon_shoot_types) orelse return error.Refused;
    return appendChild(allocator, doc, shoot_types, item_type.weapon_damage_props);
}

pub fn weaponDeleteShootType(doc: *const Document, shoot_type: i32) EditError!ResourceCommand {
    const node = findNode(doc, shoot_type) orelse return error.Refused;
    if (!isClass(node, item_type.weapon_damage_props)) return error.Refused;
    return deleteNode(doc, shoot_type);
}

/// A property edit on one part of a shoot type: `default_name` is MFC's
/// name for it, e.g. "Damage power", "Human fire sound", "Gun fire effect",
/// "Flash power", "Craters directory".
pub fn weaponEdit(allocator: std.mem.Allocator, doc: *const Document, shoot_type: i32, part: WeaponPart, default_name: []const u8, text: []const u8) EditError!ResourceCommand {
    const node = weaponPartNode(doc, shoot_type, part) orelse return error.Refused;
    return setPropByName(allocator, doc, node, default_name, text);
}

/// A new crater at the end of a shoot type's Picture craters.
pub fn weaponAddCrater(allocator: std.mem.Allocator, doc: *const Document, shoot_type: i32) EditError!ResourceCommand {
    const craters = weaponPartNode(doc, shoot_type, .craters) orelse return error.Refused;
    return appendChild(allocator, doc, craters, item_type.weapon_crater_props);
}

pub fn weaponDeleteCrater(doc: *const Document, crater: i32) EditError!ResourceCommand {
    const node = findNode(doc, crater) orelse return error.Refused;
    if (!isClass(node, item_type.weapon_crater_props)) return error.Refused;
    return deleteNode(doc, crater);
}

/// A crater's reference ("Crater file", shown as "Crater reference").
pub fn weaponEditCrater(allocator: std.mem.Allocator, doc: *const Document, crater: i32, text: []const u8) EditError!ResourceCommand {
    return setPropByName(allocator, doc, crater, "Crater file", text);
}

// --- Trench (TrenchFrm) ------------------------------------------------------

/// A new source at the end of one of the trench's source lists (Trenches
/// with embrasure, Trenches line, Trench ends, Trench arcs).
pub fn trenchAddSource(allocator: std.mem.Allocator, doc: *const Document, sources: i32) EditError!ResourceCommand {
    const node = findNode(doc, sources) orelse return error.Refused;
    if (!isClass(node, item_type.trench_sources)) return error.Refused;
    return appendChild(allocator, doc, sources, item_type.trench_source_props);
}

pub fn trenchRemoveSource(doc: *const Document, source: i32) EditError!ResourceCommand {
    const node = findNode(doc, source) orelse return error.Refused;
    if (!isClass(node, item_type.trench_source_props)) return error.Refused;
    return deleteNode(doc, source);
}

// --- Sprite and Infantry (SpriteFrm, AnimationFrm) -----------------------------

/// A new frame item named after `picture` (a listed file name without its
/// extension) at the end of `parent`: MFC's thumbnail double-click makes the
/// item, calls SetItemName and AddChild. The name rides on the insert, so
/// the add is one undo step.
fn addFrame(allocator: std.mem.Allocator, doc: *const Document, parent: i32, parent_class: i32, frame_class: i32, picture: []const u8) EditError!ResourceCommand {
    const node = findNode(doc, parent) orelse return error.Refused;
    if (!isClass(node, parent_class)) return error.Refused;
    if (picture.len == 0 or picture.len >= bridge_mod.name_capacity) return error.BadArgument;
    var command = try appendChild(allocator, doc, parent, frame_class);
    errdefer command.deinit(allocator);
    command.insert_node.name = try OwnedBytes.fromSlice(allocator, picture);
    return command;
}

/// SpriteFrm's DoubleClickOnThumbList: a CSpritePropsItem under the
/// "Sprites" item.
pub fn spriteAddFrame(allocator: std.mem.Allocator, doc: *const Document, sprites_node: i32, picture_name: []const u8) EditError!ResourceCommand {
    return addFrame(allocator, doc, sprites_node, item_type.sprites, item_type.sprite_props, picture_name);
}

/// AnimationFrm's DoubleClickOnThumbList: a CUnitFramePropsItem under the
/// active animation.
pub fn infantryAddFrame(allocator: std.mem.Allocator, doc: *const Document, animation_node: i32, picture_name: []const u8) EditError!ResourceCommand {
    return addFrame(allocator, doc, animation_node, item_type.unit_animation_props, item_type.unit_frame_props, picture_name);
}

/// FenceFrm's DoubleClickOnThumbList: a CFencePropsItem named after the
/// picture under the active insert item (the fence's insert type). MFC also
/// numbers the segment (GetFreeFenceIndex); the exporter assigns it here.
pub fn fenceAddSegment(allocator: std.mem.Allocator, doc: *const Document, insert_node: i32, picture_name: []const u8) EditError!ResourceCommand {
    return addFrame(allocator, doc, insert_node, item_type.fence_insert, item_type.fence_props, picture_name);
}

/// DeleteFrameInTree: only a frame item goes; the unt tree's directories,
/// animations, actions, exposures and acks are static (bStaticElements) and
/// the sprite's "Sprites" item is the root's only child.
pub fn deleteFrame(doc: *const Document, frame: i32) EditError!ResourceCommand {
    const node = findNode(doc, frame) orelse return error.Refused;
    if (!isClass(node, item_type.sprite_props) and !isClass(node, item_type.unit_frame_props)) return error.Refused;
    return deleteNode(doc, frame);
}

/// The sprite directory ("Directory" of the Sprites item).
pub fn spriteSetDirectory(allocator: std.mem.Allocator, doc: *const Document, sprites_node: i32, text: []const u8) EditError!ResourceCommand {
    return spriteEdit(allocator, doc, sprites_node, "Directory", text);
}

/// The time every frame is shown for, in milliseconds.
pub fn spriteSetFrameTime(allocator: std.mem.Allocator, doc: *const Document, sprites_node: i32, text: []const u8) EditError!ResourceCommand {
    return spriteEdit(allocator, doc, sprites_node, "Frame time", text);
}

/// The frame's hot spot, X and Y position, as one undo step.
pub fn spriteSetPosition(allocator: std.mem.Allocator, doc: *const Document, sprites_node: i32, x: []const u8, y: []const u8) EditError!ResourceCommand {
    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    errdefer {
        for (steps.items) |*step| step.deinit(allocator);
        steps.deinit(allocator);
    }
    try steps.ensureTotalCapacity(allocator, 2);
    steps.appendAssumeCapacity(try spriteEdit(allocator, doc, sprites_node, "X position", x));
    steps.appendAssumeCapacity(try spriteEdit(allocator, doc, sprites_node, "Y position", y));
    return .{ .composite = .{ .steps = steps } };
}

fn spriteEdit(allocator: std.mem.Allocator, doc: *const Document, sprites_node: i32, default_name: []const u8, text: []const u8) EditError!ResourceCommand {
    const node = findNode(doc, sprites_node) orelse return error.Refused;
    if (!isClass(node, item_type.sprites)) return error.Refused;
    return setPropByName(allocator, doc, sprites_node, default_name, text);
}

/// A property of one animation ("Frame time", "Action frame", "Animation
/// speed", "Is cycled?", "Number of directions", "Sound file name").
pub fn infantrySetAnimationProp(allocator: std.mem.Allocator, doc: *const Document, animation_node: i32, default_name: []const u8, text: []const u8) EditError!ResourceCommand {
    const node = findNode(doc, animation_node) orelse return error.Refused;
    if (!isClass(node, item_type.unit_animation_props)) return error.Refused;
    return setPropByName(allocator, doc, animation_node, default_name, text);
}

/// The directory of one of a season's directions (Right Up, Up, ...), by its
/// place among the season's children.
pub fn infantrySetSeasonDir(allocator: std.mem.Allocator, doc: *const Document, season_node: i32, direction: usize, text: []const u8) EditError!ResourceCommand {
    const node = findNode(doc, season_node) orelse return error.Refused;
    if (!isClass(node, item_type.unit_season_props)) return error.Refused;
    const dir = childOfClass(doc, season_node, item_type.unit_directory_props, direction) orelse return error.Refused;
    return setPropByName(allocator, doc, dir, "Directory", text);
}

// --- Tests (fake bridge) -----------------------------------------------------

const testing = std.testing;
const FakeResBridge = @import("fake_bridge.zig").FakeResBridge;
const PropRecord = bridge_mod.PropRecord;

/// A fake project, its mirror and its history, with undo and redo moving the
/// stacks the way the app's Edit menu does.
const Harness = struct {
    allocator: std.mem.Allocator,
    fake: FakeResBridge,
    doc: Document = .{},
    history: History = .{},
    /// What the dump reads besides the tree: (node, channel) pairs.
    watch: std.ArrayListUnmanaged(Watch) = .empty,

    const Watch = struct { node: i32, channel: GeometryChannel };

    fn init(allocator: std.mem.Allocator) Harness {
        return .{ .allocator = allocator, .fake = FakeResBridge.init(allocator) };
    }

    fn deinit(self: *Harness) void {
        self.history.deinit(self.allocator);
        self.doc.deinit(self.allocator);
        self.watch.deinit(self.allocator);
        self.fake.deinit();
    }

    fn bridge(self: *Harness) ResBridge {
        return self.fake.bridge();
    }

    fn node(self: *Harness, parent: i32, class_type: i32, display: []const u8) !i32 {
        var buf: [16]u8 = undefined;
        var id: i32 = 0;
        try bridge_mod.check(self.bridge().insertNode(parent, try std.fmt.bufPrint(&buf, "{d}", .{class_type}), childCount(&self.doc, parent), &id));
        try bridge_mod.check(self.bridge().setNodeName(id, display));
        try self.doc.reload(self.allocator, self.bridge());
        return id;
    }

    fn prop(self: *Harness, node_id: i32, id: i32, name: []const u8, value: []const u8) !void {
        for (self.fake.nodes.items) |*n| if (n.id == node_id) {
            var record: PropRecord = .{ .id = id };
            _ = record.setDefault(name);
            _ = record.setDisplay(name);
            _ = record.setValue(value);
            try n.props.append(self.allocator, record);
        };
        try self.doc.reload(self.allocator, self.bridge());
    }

    fn apply(self: *Harness, command: ResourceCommand) !void {
        try commit(self.allocator, self.bridge(), &self.doc, &self.history, command, 0);
    }

    fn undo(self: *Harness) !void {
        try self.history.redo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.undo_stack.pop().?;
        try self.doc.undoOne(self.allocator, self.bridge(), &entry.command);
        self.history.redo_stack.appendAssumeCapacity(entry);
    }

    fn redo(self: *Harness) !void {
        try self.history.undo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.redo_stack.pop().?;
        try self.doc.redoOne(self.allocator, self.bridge(), &entry.command);
        self.history.undo_stack.appendAssumeCapacity(entry);
    }

    fn saveAndReopen(self: *Harness, path: []const u8) !void {
        try bridge_mod.check(self.bridge().save(path));
        try bridge_mod.check(self.bridge().open(path));
        try self.doc.reload(self.allocator, self.bridge());
    }

    /// The whole state a tool can change, as text: the tree (classes,
    /// names, props, by depth, no ids, so a restored node under a new id
    /// still compares) and every watched channel as the bridge reads it.
    fn dump(self: *Harness) ![]u8 {
        var out: std.Io.Writer.Allocating = .init(self.allocator);
        errdefer out.deinit();
        const w = &out.writer;
        for (self.doc.tree.nodes.items) |*n| {
            if (findNode(&self.doc, n.parent) == null) try self.dumpNode(w, n, 0);
        }
        for (self.watch.items) |item| {
            try w.print("channel {s} node {d}: ", .{ @tagName(item.channel), item.node });
            var value = readGeometry(self.bridge(), item.node, item.channel) catch |err| {
                try w.print("{s}\n", .{@errorName(err)});
                continue;
            };
            defer value.deinit(self.allocator);
            switch (value) {
                .points2 => |list| for (list) |p| try w.print("({d},{d}) ", .{ p.x, p.y }),
                .point2 => |p| try w.print("({d},{d})", .{ p.x, p.y }),
                else => try w.print("{s}", .{@tagName(value)}),
            }
            try w.writeByte('\n');
        }
        return out.toOwnedSlice();
    }

    /// One node, its props and then its children, in child order.
    fn dumpNode(self: *Harness, w: *std.Io.Writer, n: *const document_mod.Node, depth: usize) !void {
        try w.splatByteAll(' ', depth * 2);
        try w.print("{s} \"{s}\"\n", .{ n.classSlice(), n.displaySlice() });
        for (self.doc.tree.props.items) |*p| if (p.node == n.id) {
            try w.splatByteAll(' ', depth * 2 + 2);
            try w.print("{s} = {s}\n", .{ p.record.defaultSlice(), p.record.valueSlice() });
        };
        for (self.doc.tree.nodes.items) |*child| {
            if (child.parent == n.id) try self.dumpNode(w, child, depth + 1);
        }
    }

    /// The test plan's sequence for one tool: get-before, apply, get-after,
    /// undo == before, redo == after, save and reopen == after. A mismatch
    /// prints the tool, the step and both states (channel, node and values).
    fn runTool(self: *Harness, tool: []const u8, command: ResourceCommand) !void {
        const before = try self.dump();
        defer self.allocator.free(before);
        const depth = self.history.undo_stack.items.len;
        try self.apply(command);
        try testing.expectEqual(depth + 1, self.history.undo_stack.items.len);
        const after = try self.dump();
        defer self.allocator.free(after);
        try expectState(tool, "apply changes the project", before, after, false);

        try self.undo();
        const undone = try self.dump();
        defer self.allocator.free(undone);
        try expectState(tool, "undo", before, undone, true);

        try self.redo();
        const redone = try self.dump();
        defer self.allocator.free(redone);
        try expectState(tool, "redo", after, redone, true);

        try self.saveAndReopen("fake/s06-tool.project");
        const reopened = try self.dump();
        defer self.allocator.free(reopened);
        try expectState(tool, "save and reopen", after, reopened, true);
    }
};

fn expectState(tool: []const u8, step: []const u8, want: []const u8, got: []const u8, equal: bool) !void {
    if (std.mem.eql(u8, want, got) == equal) return;
    std.debug.print("\n{s}: {s}: expected the state {s}\n--- expected ---\n{s}--- got ---\n{s}", .{ tool, step, if (equal) "to match" else "to change", want, got });
    return error.TestUnexpectedResult;
}

/// A squad with one formation of three slots; its zero point, slots and
/// direction are the only geometry homes.
fn squadProject(h: *Harness) !i32 {
    try bridge_mod.check(h.bridge().new(.squad));
    try h.doc.reload(h.allocator, h.bridge());
    const root = h.fake.nodes.items[0].id;
    const formations = try h.node(root, item_type.squad_formations, "Formations");
    const formation = try h.node(formations, item_type.squad_formation_props, "Formation");
    try h.prop(formation, 1, "Formation type", "default");
    for ([_]GeometryChannel{ .formation_positions, .zero_point, .formation_direction }) |channel| {
        try h.fake.addGeometryHome(formation, channel);
        try h.watch.append(h.allocator, .{ .node = formation, .channel = channel });
    }
    const slots = [_]Point2{ .{ .x = 700, .y = 350 }, .{ .x = 732, .y = 350 }, .{ .x = 716, .y = 382 } };
    const seeds = [_]struct { GeometryChannel, GeometryValue }{
        .{ .formation_positions, .{ .points2 = @constCast(&slots) } },
        .{ .zero_point, .{ .point2 = .{ .x = 716, .y = 366 } } },
        .{ .formation_direction, .{ .point2 = .{ .x = 0.25, .y = 0 } } },
    };
    for (seeds) |seed| try bridge_mod.check(h.bridge().geometryWrite(formation, seed[0], &seed[1]));
    return formation;
}

/// A weapon with one shoot type laid out as MFC's defaults lay it out.
fn weaponProject(h: *Harness) !i32 {
    try bridge_mod.check(h.bridge().new(.weapon));
    try h.doc.reload(h.allocator, h.bridge());
    const root = h.fake.nodes.items[0].id;
    const name = try h.node(root, item_type.weapon_common_props, "Name");
    try h.prop(name, 1, "Name", "Unknown Weapon");
    const shoot_types = try h.node(root, item_type.weapon_shoot_types, "Shoot types");
    const shell = try h.node(shoot_types, item_type.weapon_damage_props, "Shell");
    try h.prop(shell, 4, "Damage power", "5");
    const craters = try h.node(shell, item_type.weapon_craters, "Picture craters");
    try h.prop(craters, 1, "Craters directory", "");
    const crater = try h.node(craters, item_type.weapon_crater_props, "Crater");
    try h.prop(crater, 1, "Crater file", "craters\\small");
    const effects = try h.node(shell, item_type.weapon_effects, "Effects");
    try h.prop(effects, 1, "Human fire sound", "");
    try h.prop(effects, 2, "Gun fire effect", "");
    const flash_fire = try h.node(shell, item_type.weapon_flash_props, "Flash fire");
    try h.prop(flash_fire, 0, "Flash power", "100");
    const flash_explosion = try h.node(shell, item_type.weapon_flash_props, "Flash explosion");
    try h.prop(flash_explosion, 0, "Flash power", "100");
    return shell;
}

/// A trench with two source lists, the line holding one source.
fn trenchProject(h: *Harness) !struct { line: i32, ends: i32, source: i32 } {
    try bridge_mod.check(h.bridge().new(.trench));
    try h.doc.reload(h.allocator, h.bridge());
    const root = h.fake.nodes.items[0].id;
    const line = try h.node(root, item_type.trench_sources, "Trenches line");
    const source = try h.node(line, item_type.trench_source_props, "Trench");
    try h.prop(source, 1, "Image", "1.tga");
    const ends = try h.node(root, item_type.trench_sources, "Trench ends");
    return .{ .line = line, .ends = ends, .source = source };
}

test "tool: squad formation drag of several moves is one undo step" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const formation = try squadProject(&h);
    var drag = try FormationDrag.begin(h.allocator, h.bridge(), formation);
    // The tool's before state is the press; the live writes in between are
    // what the view shows while the mouse moves.
    const pressed = try h.dump();
    defer h.allocator.free(pressed);
    try drag.moveSlot(h.bridge(), 1, .{ .x = 740, .y = 352 });
    try drag.moveSlot(h.bridge(), 1, .{ .x = 748, .y = 356.5 });
    try drag.moveSlot(h.bridge(), 2, .{ .x = 712.25, .y = 390 });
    const command = drag.finish(h.allocator).?;
    // Back to the press so runTool's get-before is the state before the drag.
    var back: GeometryValue = .{ .points2 = command.geometry.before.points2 };
    try bridge_mod.check(h.bridge().geometryWrite(formation, .formation_positions, &back));
    try h.doc.reload(h.allocator, h.bridge());
    const reset = try h.dump();
    defer h.allocator.free(reset);
    try expectState("squad formation drag", "back at the press", pressed, reset, true);
    try h.runTool("squad formation drag", command);
    try testing.expectEqual(@as(usize, 1), h.history.undo_stack.items.len);
    var slots = try readGeometry(h.bridge(), formation, .formation_positions);
    defer slots.deinit(h.allocator);
    try testing.expectEqualSlices(Point2, &.{ .{ .x = 700, .y = 350 }, .{ .x = 748, .y = 356.5 }, .{ .x = 712.25, .y = 390 } }, slots.points2);
    // A press and release with no move is no step.
    var click = try FormationDrag.begin(h.allocator, h.bridge(), formation);
    try testing.expect(click.finish(h.allocator) == null);
}

test "tool: squad zero point" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const formation = try squadProject(&h);
    try h.runTool("squad zero point", try setZeroPoint(h.allocator, h.bridge(), formation, .{ .x = 600.5, .y = 300.25 }));
    var zero = try readGeometry(h.bridge(), formation, .zero_point);
    defer zero.deinit(h.allocator);
    try testing.expectEqual(Point2{ .x = 600.5, .y = 300.25 }, zero.point2);
}

test "tool: squad direction arrow turns the formation and its slots in one step" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const formation = try squadProject(&h);
    const angle: f32 = 0.25 + std.math.pi / 2.0;
    try h.runTool("squad direction arrow", try setFormationDirection(h.allocator, h.bridge(), formation, angle, null));
    var direction = try readGeometry(h.bridge(), formation, .formation_direction);
    defer direction.deinit(h.allocator);
    try testing.expectEqual(angle, direction.point2.x);
    // A quarter turn about the zero point (716, 366): (700, 350) is 16 left
    // and 16 up of it and lands 16 right and 16 up.
    var slots = try readGeometry(h.bridge(), formation, .formation_positions);
    defer slots.deinit(h.allocator);
    try testing.expectApproxEqAbs(@as(f32, 732), slots.points2[0].x, 1e-3);
    try testing.expectApproxEqAbs(@as(f32, 350), slots.points2[0].y, 1e-3);
}

/// CSquadFrame::CalculateNewPositions written out per member, independent of
/// turnSlots, so the test pins the convention and not the helper.
fn mfcTurned(slot: Point2, c: Point2, alpha: f32) Point2 {
    return .{
        .x = c.x + (slot.x - c.x) * @cos(alpha) - (slot.y - c.y) * @sin(alpha),
        .y = c.y + (slot.x - c.x) * @sin(alpha) + (slot.y - c.y) * @cos(alpha),
    };
}

test "tool: squad direction rotation equals MFC CalculateNewPositions" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const formation = try squadProject(&h);
    const zero = Point2{ .x = 716, .y = 366 };
    // MFC starts from FormationDir 0 here; the project seeds 0.25.
    const flat: GeometryValue = .{ .point2 = .{ .x = 0, .y = 0 } };
    try bridge_mod.check(h.bridge().geometryWrite(formation, .formation_direction, &flat));
    try h.doc.reload(h.allocator, h.bridge());

    var previous: f32 = 0;
    for ([_]f32{ std.math.pi / 2.0, 1.234 }) |angle| {
        var before = try readGeometry(h.bridge(), formation, .formation_positions);
        defer before.deinit(h.allocator);
        try h.runTool("squad direction rotation", try setFormationDirection(h.allocator, h.bridge(), formation, angle, null));
        var after = try readGeometry(h.bridge(), formation, .formation_positions);
        defer after.deinit(h.allocator);
        var direction = try readGeometry(h.bridge(), formation, .formation_direction);
        defer direction.deinit(h.allocator);
        try testing.expectEqual(angle, direction.point2.x);
        for (before.points2, after.points2, 0..) |old, got, i| {
            const want = mfcTurned(old, zero, angle - previous);
            testing.expectApproxEqAbs(want.x, got.x, 1e-3) catch |err| {
                std.debug.print("\nmember {d} angle {d}: expected ({d}, {d}) actual ({d}, {d})\n", .{ i, angle, want.x, want.y, got.x, got.y });
                return err;
            };
            testing.expectApproxEqAbs(want.y, got.y, 1e-3) catch |err| {
                std.debug.print("\nmember {d} angle {d}: expected ({d}, {d}) actual ({d}, {d})\n", .{ i, angle, want.x, want.y, got.x, got.y });
                return err;
            };
        }
        previous = angle;
    }
}

test "tool: weapon shoot type insert" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    _ = try weaponProject(&h);
    try h.runTool("weapon shoot type insert", try weaponAddShootType(h.allocator, &h.doc));
    const shoot_types = firstOfClass(&h.doc, item_type.weapon_shoot_types).?;
    try testing.expectEqual(@as(i32, 2), childCount(&h.doc, shoot_types));
}

test "tool: weapon shoot type delete" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const shell = try weaponProject(&h);
    try h.runTool("weapon shoot type delete", try weaponDeleteShootType(&h.doc, shell));
    try testing.expect(firstOfClass(&h.doc, item_type.weapon_damage_props) == null);
    try testing.expect(firstOfClass(&h.doc, item_type.weapon_crater_props) == null);
}

test "tool: weapon damage edit" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const shell = try weaponProject(&h);
    try h.runTool("weapon damage edit", try weaponEdit(h.allocator, &h.doc, shell, .damage, "Damage power", "40"));
    try testing.expectEqualStrings("40", propValue(&h.doc, shell, propIdByName(&h.doc, shell, "Damage power").?).?);
}

test "tool: weapon sound edit" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const shell = try weaponProject(&h);
    try h.runTool("weapon sound edit", try weaponEdit(h.allocator, &h.doc, shell, .sound, "Human fire sound", "rifle_shot"));
    const effects = weaponPartNode(&h.doc, shell, .sound).?;
    try testing.expectEqualStrings("rifle_shot", propValue(&h.doc, effects, 1).?);
}

test "tool: weapon effect edit" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const shell = try weaponProject(&h);
    try h.runTool("weapon effect edit", try weaponEdit(h.allocator, &h.doc, shell, .effect, "Gun fire effect", "gun_smoke"));
    const effects = weaponPartNode(&h.doc, shell, .effect).?;
    try testing.expectEqualStrings("gun_smoke", propValue(&h.doc, effects, 2).?);
}

test "tool: weapon flash edit" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const shell = try weaponProject(&h);
    try h.runTool("weapon flash edit", try weaponEdit(h.allocator, &h.doc, shell, .flash_explosion, "Flash power", "250"));
    try testing.expectEqualStrings("100", propValue(&h.doc, weaponPartNode(&h.doc, shell, .flash_fire).?, 0).?);
    try testing.expectEqualStrings("250", propValue(&h.doc, weaponPartNode(&h.doc, shell, .flash_explosion).?, 0).?);
}

test "tool: weapon crater insert, edit and delete" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const shell = try weaponProject(&h);
    try h.runTool("weapon crater insert", try weaponAddCrater(h.allocator, &h.doc, shell));
    const craters = weaponPartNode(&h.doc, shell, .craters).?;
    try testing.expectEqual(@as(i32, 2), childCount(&h.doc, craters));
    const crater = childOfClass(&h.doc, craters, item_type.weapon_crater_props, 0).?;
    try h.runTool("weapon crater edit", try weaponEditCrater(h.allocator, &h.doc, crater, "craters\\big"));
    try h.runTool("weapon crater delete", try weaponDeleteCrater(&h.doc, crater));
    try testing.expectEqual(@as(i32, 1), childCount(&h.doc, craters));
}

test "tool: trench source add" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const trench = try trenchProject(&h);
    try h.runTool("trench source add", try trenchAddSource(h.allocator, &h.doc, trench.ends));
    try testing.expectEqual(@as(i32, 1), childCount(&h.doc, trench.ends));
    try testing.expectError(error.Refused, trenchAddSource(h.allocator, &h.doc, trench.source));
}

test "tool: trench source remove" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const trench = try trenchProject(&h);
    try h.runTool("trench source remove", try trenchRemoveSource(&h.doc, trench.source));
    try testing.expectEqual(@as(i32, 0), childCount(&h.doc, trench.line));
    try testing.expectError(error.Refused, trenchRemoveSource(&h.doc, trench.line));
}

test "tool: a geometry set with no MFC home is refused and leaves the history unchanged" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const formation = try squadProject(&h);
    const root = h.fake.nodes.items[0].id;
    const before = try h.dump();
    defer h.allocator.free(before);
    const revision = h.history.revision;
    // The squad root has no zero point and no slots: refused while building.
    try testing.expectError(error.Refused, setZeroPoint(h.allocator, h.bridge(), root, .{ .x = 1, .y = 2 }));
    try testing.expectError(error.Refused, setFormationDirection(h.allocator, h.bridge(), root, 1.0, null));
    try testing.expectError(error.Refused, FormationDrag.begin(h.allocator, h.bridge(), root));
    try testing.expectEqualStrings("this geometry channel has no MFC home on this node", h.bridge().lastMessage());
    // A command built by hand for a homeless node is refused by the bridge
    // inside commit, which frees it and records nothing.
    const after_points = try h.allocator.alloc(Point2, 1);
    after_points[0] = .{ .x = 5, .y = 6 };
    const forged: ResourceCommand = .{ .geometry = .{
        .node = root,
        .channel = .formation_positions,
        .before = .{ .points2 = try h.allocator.alloc(Point2, 0) },
        .after = .{ .points2 = after_points },
    } };
    try testing.expectError(error.Refused, commit(h.allocator, h.bridge(), &h.doc, &h.history, forged, 0));
    try testing.expectEqual(@as(usize, 0), h.history.undo_stack.items.len);
    try testing.expectEqual(revision, h.history.revision);
    const after = try h.dump();
    defer h.allocator.free(after);
    try expectState("no MFC home", "refused set leaves the project", before, after, true);
    _ = formation;
}

/// A sprite project: the Sprites item with MFC's four properties and one frame.
fn spriteProject(h: *Harness) !struct { sprites: i32, frame: i32 } {
    try bridge_mod.check(h.bridge().new(.sprite));
    try h.doc.reload(h.allocator, h.bridge());
    const root = h.fake.nodes.items[0].id;
    const sprites = try h.node(root, item_type.sprites, "Sprites");
    try h.prop(sprites, 1, "Directory", "_.");
    try h.prop(sprites, 2, "Frame time", "125");
    try h.prop(sprites, 3, "X position", "32");
    try h.prop(sprites, 4, "Y position", "32");
    const frame = try h.node(sprites, item_type.sprite_props, "walk_00");
    return .{ .sprites = sprites, .frame = frame };
}

/// An infantry project: static directories (a season with two directions),
/// the animations list with one animation holding one frame, and the acks.
fn infantryProject(h: *Harness) !struct { season: i32, animation: i32, frame: i32, animations: i32, acks: i32 } {
    try bridge_mod.check(h.bridge().new(.animation_infantry));
    try h.doc.reload(h.allocator, h.bridge());
    const root = h.fake.nodes.items[0].id;
    const directories = try h.node(root, item_type.unit_directories, "Directories");
    const season = try h.node(directories, item_type.unit_season_props, "Summer");
    for ([_][]const u8{ "Right Up", "Up" }) |name| {
        const dir = try h.node(season, item_type.unit_directory_props, name);
        try h.prop(dir, 1, "Directory", "_.");
    }
    const animations = try h.node(root, item_type.unit_animations, "Animations");
    const animation = try h.node(animations, item_type.unit_animation_props, "Run");
    try h.prop(animation, 1, "Frame time", "125");
    try h.prop(animation, 2, "Animation speed", "1");
    const frame = try h.node(animation, item_type.unit_frame_props, "run_00");
    const acks = try h.node(root, item_type.base + 16, "Acks");
    return .{ .season = season, .animation = animation, .frame = frame, .animations = animations, .acks = acks };
}

test "tool: sprite frame add is named after its picture, undoable, and delete restores it" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const p = try spriteProject(&h);
    try h.runTool("sprite add frame", try spriteAddFrame(h.allocator, &h.doc, p.sprites, "walk_01"));
    try testing.expectEqual(@as(i32, 2), childCount(&h.doc, p.sprites));
    const added = childOfClass(&h.doc, p.sprites, item_type.sprite_props, 1).?;
    try testing.expectEqualStrings("walk_01", findNode(&h.doc, added).?.displaySlice());
    try h.runTool("sprite delete frame", try deleteFrame(&h.doc, p.frame));
}

test "tool: sprite directory, frame time and position" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const p = try spriteProject(&h);
    try h.runTool("sprite directory", try spriteSetDirectory(h.allocator, &h.doc, p.sprites, "units\\walk\\"));
    try h.runTool("sprite frame time", try spriteSetFrameTime(h.allocator, &h.doc, p.sprites, "80"));
    // Position is two writes and one undo step.
    try h.runTool("sprite position", try spriteSetPosition(h.allocator, &h.doc, p.sprites, "16", "48"));
    try testing.expectEqualStrings("16", propValue(&h.doc, p.sprites, propIdByName(&h.doc, p.sprites, "X position").?).?);
}

test "tool: infantry frame add, delete and animation and season edits" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const p = try infantryProject(&h);
    try h.runTool("infantry add frame", try infantryAddFrame(h.allocator, &h.doc, p.animation, "run_01"));
    const added = childOfClass(&h.doc, p.animation, item_type.unit_frame_props, 1).?;
    try testing.expectEqualStrings("run_01", findNode(&h.doc, added).?.displaySlice());
    try h.runTool("infantry delete frame", try deleteFrame(&h.doc, p.frame));
    try h.runTool("infantry frame time", try infantrySetAnimationProp(h.allocator, &h.doc, p.animation, "Frame time", "60"));
    try h.runTool("infantry animation speed", try infantrySetAnimationProp(h.allocator, &h.doc, p.animation, "Animation speed", "2"));
    try h.runTool("infantry season dir", try infantrySetSeasonDir(h.allocator, &h.doc, p.season, 1, "units\\up\\"));
}

test "tool: frame tools refuse static items, wrong parents and bad names, leaving the project as it was" {
    var h = Harness.init(testing.allocator);
    defer h.deinit();
    const s = try spriteProject(&h);
    const before_sprite = try h.dump();
    defer h.allocator.free(before_sprite);
    const root = h.fake.nodes.items[0].id;
    try testing.expectError(error.Refused, spriteAddFrame(h.allocator, &h.doc, root, "x"));
    try testing.expectError(error.Refused, spriteAddFrame(h.allocator, &h.doc, 9999, "x"));
    try testing.expectError(error.BadArgument, spriteAddFrame(h.allocator, &h.doc, s.sprites, ""));
    try testing.expectError(error.BadArgument, spriteAddFrame(h.allocator, &h.doc, s.sprites, "n" ** 64));
    try testing.expectError(error.Refused, deleteFrame(&h.doc, s.sprites));
    try testing.expectError(error.Refused, deleteFrame(&h.doc, root));
    try testing.expectError(error.Refused, spriteSetDirectory(h.allocator, &h.doc, root, "x"));
    const after_sprite = try h.dump();
    defer h.allocator.free(after_sprite);
    try expectState("refused sprite tools", "nothing changed", before_sprite, after_sprite, true);

    var u = Harness.init(testing.allocator);
    defer u.deinit();
    const p = try infantryProject(&u);
    // Directories, animations, acks and the root are static: not deletable here.
    try testing.expectError(error.Refused, deleteFrame(&u.doc, p.animation));
    try testing.expectError(error.Refused, deleteFrame(&u.doc, p.animations));
    try testing.expectError(error.Refused, deleteFrame(&u.doc, p.acks));
    try testing.expectError(error.Refused, deleteFrame(&u.doc, p.season));
    // A frame goes under an animation only, and a season takes only its directions.
    try testing.expectError(error.Refused, infantryAddFrame(u.allocator, &u.doc, p.animations, "x"));
    try testing.expectError(error.Refused, infantryAddFrame(u.allocator, &u.doc, p.frame, "x"));
    try testing.expectError(error.Refused, infantrySetAnimationProp(u.allocator, &u.doc, p.season, "Frame time", "1"));
    try testing.expectError(error.Refused, infantrySetAnimationProp(u.allocator, &u.doc, p.animation, "No such property", "1"));
    try testing.expectError(error.Refused, infantrySetSeasonDir(u.allocator, &u.doc, p.season, 2, "x"));
    try testing.expectError(error.Refused, infantrySetSeasonDir(u.allocator, &u.doc, p.animation, 0, "x"));
}
