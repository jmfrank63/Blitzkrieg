//! The project tree, the object inspector and the reference pickers without
//! a window: which node is selected, which widget a property gets, what text
//! a picker or a browse writes back, and every edit as an undoable
//! ResourceCommand on the open project's history. panels.zig draws them and
//! calls in here; the tests run them against resource_core's fake bridge.
//!
//! MFC's editor had no undo outside the GUI editor (06-PARITY A-38), so the
//! history is new. One user gesture is one undo step: the keystrokes or drag
//! frames of one property edit merge into the entry they began (`gesture`),
//! an edit applied to every selected node and a delete of several nodes are a
//! `composite`. An edit that brings a value back to where its step began
//! drops the step, so the project is clean again if that was the saved state.
const std = @import("std");
const core = @import("resource_core");

const bridge = core.bridge;
const ResBridge = bridge.ResBridge;
const EditError = bridge.EditError;
const NodeRecord = bridge.NodeRecord;
const PropRecord = bridge.PropRecord;
const ReferenceEntry = bridge.ReferenceEntry;
const Document = core.document.Document;
const History = core.history.History;
const ResourceCommand = core.history.ResourceCommand;
const OwnedBytes = core.history.OwnedBytes;

// --- Domain types ----------------------------------------------------------

/// NResourceModel's DomenID (Sources/src/ResourceModel/domen_id.h, MFC's
/// COI DT_* values): which widget the inspector gives a property.
pub const Domain = enum(i32) {
    err = 0,
    dec,
    hex,
    str,
    bool,
    browse,
    browse_dir,
    combo,
    color,
    float,
    animation_ref,
    func_particle_ref,
    effect_ref,
    weapon_ref,
    soldier_ref,
    action_ref,
    scenario_mission_ref,
    template_mission_ref,
    chapter_ref,
    sound_ref,
    setting_ref,
    ask_ref,
    death_ref,
    crater_ref,
    map_ref,
    music_ref,
    movie_ref,
    particle_texture_ref,
    water_texture_ref,
    road_texture_ref,
    custom,
    _,
};

/// CVariant::EKind (variant.h): how the bridge reads and writes value_text.
pub const ValueKind = enum(i32) {
    null = 0,
    int,
    float,
    bool,
    str,
    vec3,
    color,
    combo,
    ref,
    int64,
    _,
};

/// NResourceModel::EReferenceType (references.h, MFC's RefDlg.h): the 20
/// lists BkResRefList answers.
pub const RefType = enum(i32) {
    animations = 0,
    func_particles,
    effects,
    weapons,
    soldier,
    actions,
    scenario_missions,
    template_missions,
    chapters,
    sounds,
    setting,
    asks,
    crater,
    deathhole,
    map,
    music,
    movie,
    particle_texture,
    road_texture,
    water_texture,

    /// The picker's title, as RefDlg.cpp's caption names each list.
    pub fn label(self: RefType) []const u8 {
        return switch (self) {
            .animations => "Sprites",
            .func_particles => "Particles",
            .effects => "Effects",
            .weapons => "Weapons",
            .soldier => "Infantry",
            .actions => "Actions",
            .scenario_missions => "Missions",
            .template_missions => "Template missions",
            .chapters => "Chapters",
            .sounds => "Sounds",
            .setting => "Settings",
            .asks => "Acknowledgements",
            .crater => "Craters",
            .deathhole => "Death holes",
            .map => "Maps",
            .music => "Music",
            .movie => "Movies",
            .particle_texture => "Particle textures",
            .road_texture => "Road textures",
            .water_texture => "Water textures",
        };
    }
};

pub const ref_type_count = @typeInfo(RefType).@"enum".field_names.len;

/// MFC's ConvertFromDomenTypeToRef (COI/CtrlObjectInspector.cpp): the list a
/// DT_*_REF property picks from. The two enums are not in the same order
/// (death holes and craters, road and water textures swap).
pub fn refTypeFor(domain: Domain) ?RefType {
    return switch (domain) {
        .animation_ref => .animations,
        .func_particle_ref => .func_particles,
        .effect_ref => .effects,
        .weapon_ref => .weapons,
        .soldier_ref => .soldier,
        .action_ref => .actions,
        .scenario_mission_ref => .scenario_missions,
        .template_mission_ref => .template_missions,
        .chapter_ref => .chapters,
        .sound_ref => .sounds,
        .setting_ref => .setting,
        .ask_ref => .asks,
        .death_ref => .deathhole,
        .crater_ref => .crater,
        .map_ref => .map,
        .music_ref => .music,
        .movie_ref => .movie,
        .particle_texture_ref => .particle_texture,
        .water_texture_ref => .water_texture,
        .road_texture_ref => .road_texture,
        else => null,
    };
}

/// The inspector's widget for a property, as CCtrlObjectInspector picked an
/// editor control by domain: an edit for DT_DEC/FLOAT/STR (HEX shown in
/// hex), a combo for DT_COMBO and DT_BOOL, the browse edit for DT_BROWSE and
/// DT_BROWSEDIR, the colour edit, and the reference edit with its "..."
/// button for every DT_*_REF. DT_ACTION_REF's button opened CMultySelDialog
/// instead of the reference dialog. DT_ERROR and DT_CUSTOM had no editor.
pub const Widget = enum { text, int, hex, float, check, combo, browse_file, browse_dir, color, reference, action_mask, read_only };

pub fn widgetFor(domain: Domain) Widget {
    return switch (domain) {
        .dec => .int,
        .hex => .hex,
        .str => .text,
        .bool => .check,
        .browse => .browse_file,
        .browse_dir => .browse_dir,
        .combo => .combo,
        .color => .color,
        .float => .float,
        .action_ref => .action_mask,
        .err, .custom => .read_only,
        else => if (refTypeFor(domain) != null) .reference else .read_only,
    };
}

pub fn domainOf(prop: *const PropRecord) Domain {
    return @enumFromInt(prop.domain_type);
}

pub fn valueKindOf(prop: *const PropRecord) ValueKind {
    return @enumFromInt(prop.value_kind);
}

// --- Value text ------------------------------------------------------------

pub const value_capacity = bridge.value_text_capacity;

/// A value_text the inspector built: bounded like BkResPropRecord's buffer.
pub const ValueText = struct {
    buffer: [value_capacity]u8 = undefined,
    len: usize = 0,

    pub fn slice(self: *const ValueText) []const u8 {
        return self.buffer[0..self.len];
    }

    fn print(comptime format: []const u8, args: anytype) ?ValueText {
        var out: ValueText = .{};
        const text = std.fmt.bufPrint(out.buffer[0 .. value_capacity - 1], format, args) catch return null;
        out.len = text.len;
        return out;
    }

    fn copy(text: []const u8) ?ValueText {
        if (text.len >= value_capacity) return null;
        var out: ValueText = .{};
        @memcpy(out.buffer[0..text.len], text);
        out.len = text.len;
        return out;
    }
};

/// What the user typed for an edit-box widget, checked and put in the text
/// form BkResSetProp parses for the property's value kind. The bridge parses
/// with strtol/strtof and would quietly make "abc" a 0, so a number that does
/// not parse is refused here (null) and nothing is written. DT_HEX shows and
/// takes hex (with or without 0x) and is stored as the variant's own number.
pub fn parseTyped(widget: Widget, kind: ValueKind, typed: []const u8) ?ValueText {
    const text = std.mem.trim(u8, typed, " \t");
    switch (widget) {
        .int => {
            if (kind == .int64) {
                const v = std.fmt.parseInt(i64, text, 10) catch return null;
                return ValueText.print("{d}", .{v});
            }
            const v = std.fmt.parseInt(i32, text, 10) catch return null;
            return ValueText.print("{d}", .{v});
        },
        .hex => {
            const digits = if (std.ascii.startsWithIgnoreCase(text, "0x")) text[2..] else text;
            const v = std.fmt.parseInt(u32, digits, 16) catch return null;
            if (kind == .color) return ValueText.print("{X:0>8}", .{v});
            return ValueText.print("{d}", .{@as(i32, @bitCast(v))});
        },
        .float => {
            const v = std.fmt.parseFloat(f32, text) catch return null;
            if (!std.math.isFinite(v)) return null;
            return ValueText.print("{d}", .{v});
        },
        .text, .browse_file, .browse_dir, .reference => return ValueText.copy(typed),
        else => return null,
    }
}

/// DT_HEX's display: the stored decimal shown as MFC showed it, in hex.
pub fn hexDisplay(kind: ValueKind, value_text: []const u8) ?ValueText {
    if (kind == .color) return ValueText.copy(value_text);
    const v = std.fmt.parseInt(i32, value_text, 10) catch return null;
    return ValueText.print("0x{X}", .{@as(u32, @bitCast(v))});
}

/// A check box's text: CVariant's bool text ("1"/"0") for a bool value, and
/// for a DT_BOOL held as a string (MFC's combo of the prop's two strings)
/// the matching string.
pub fn boolText(kind: ValueKind, options: []const ReferenceEntry, on: bool) ?ValueText {
    if (kind == .bool or kind == .int or options.len < 2) return ValueText.copy(if (on) "1" else "0");
    return ValueText.copy(options[if (on) 1 else 0].nameSlice());
}

pub fn boolValue(kind: ValueKind, options: []const ReferenceEntry, value_text: []const u8) bool {
    if (kind == .bool or kind == .int or options.len < 2) return !std.mem.eql(u8, value_text, "0") and value_text.len != 0;
    return std.mem.eql(u8, value_text, options[1].nameSlice());
}

/// The combo's current choice: the index for a combo-index value, else the
/// string the value holds (SProp::value is the chosen string in most items,
/// e.g. CWeaponDamagePropsItem's "Trajectory type").
pub fn comboCurrent(kind: ValueKind, options: []const ReferenceEntry, value_text: []const u8) ?usize {
    if (kind == .combo or kind == .int) {
        const index = std.fmt.parseInt(usize, value_text, 10) catch return null;
        return if (index < options.len) index else null;
    }
    for (options, 0..) |option, i| if (std.mem.eql(u8, option.nameSlice(), value_text)) return i;
    return null;
}

/// What choosing `index` writes: the index or the string, as above.
pub fn comboText(kind: ValueKind, options: []const ReferenceEntry, index: usize) ?ValueText {
    if (index >= options.len) return null;
    if (kind == .combo or kind == .int) return ValueText.print("{d}", .{index});
    return ValueText.copy(options[index].nameSlice());
}

/// DT_COLOR's value as the picker's RGB, 0..1. MFC stored a COLORREF (red in
/// the low byte); the port keeps it as a VK_COLOR hex word or an int. The
/// high byte is not a channel and is kept by `colorText`.
pub fn colorRgb(kind: ValueKind, value_text: []const u8) ?[3]f32 {
    const word = colorWord(kind, value_text) orelse return null;
    return .{
        @as(f32, @floatFromInt(word & 0xFF)) / 255.0,
        @as(f32, @floatFromInt((word >> 8) & 0xFF)) / 255.0,
        @as(f32, @floatFromInt((word >> 16) & 0xFF)) / 255.0,
    };
}

fn colorWord(kind: ValueKind, value_text: []const u8) ?u32 {
    if (kind == .color) return std.fmt.parseInt(u32, value_text, 16) catch null;
    const v = std.fmt.parseInt(i64, value_text, 10) catch return null;
    return @truncate(@as(u64, @bitCast(v)));
}

pub fn colorText(kind: ValueKind, old_text: []const u8, rgb: [3]f32) ?ValueText {
    const high = (colorWord(kind, old_text) orelse 0) & 0xFF00_0000;
    var word: u32 = high;
    for (rgb, 0..) |channel, i| {
        const byte: u32 = @intFromFloat(@round(std.math.clamp(channel, 0, 1) * 255));
        word |= byte << @intCast(i * 8);
    }
    if (kind == .color) return ValueText.print("{X:0>8}", .{word});
    return ValueText.print("{d}", .{@as(i32, @bitCast(word))});
}

// --- Action masks (MFC's CMultySelDialog) ----------------------------------

/// The mask a DT_ACTION_REF holds: bit `id` set for every chosen action of
/// actions.ini (BkResRefList type 5 answers each action's id as its token).
/// MFC wrote it with _ui64toa base 16 into a string; the port's variant holds
/// it as an int64 written in decimal. Either text is read.
pub fn actionMask(kind: ValueKind, value_text: []const u8) u64 {
    const text = std.mem.trim(u8, value_text, " ");
    if (kind == .int64 or kind == .int) {
        const v = std.fmt.parseInt(i64, text, 10) catch return 0;
        return @bitCast(v);
    }
    return std.fmt.parseInt(u64, text, 16) catch 0;
}

pub fn actionMaskText(kind: ValueKind, mask: u64) ?ValueText {
    if (kind == .int64 or kind == .int) return ValueText.print("{d}", .{@as(i64, @bitCast(mask))});
    return ValueText.print("{x}", .{mask});
}

pub fn toggleAction(mask: u64, token: i32, on: bool) u64 {
    if (token < 0 or token > 63) return mask;
    const bit = @as(u64, 1) << @intCast(token);
    return if (on) mask | bit else mask & ~bit;
}

// --- Browse ----------------------------------------------------------------

/// What a browse writes for a chosen file or folder (A-32): the path relative
/// to the property's source folder (its first string, SProp::szStrings[0],
/// which MFC's browse edit opened in), else relative to the project file's
/// folder, else the path as chosen. MFC lower-cased the choice
/// (NStr::ToLower) and its sub-editors kept project-relative names with
/// backslashes, which the engine's case-insensitive lookups read on every
/// platform, so the result is lower case with backslashes.
pub fn browseValue(chosen: []const u8, source_dir: ?[]const u8, project_path: ?[]const u8) ?ValueText {
    var out: ValueText = .{};
    const project_dir: ?[]const u8 = if (project_path) |p| std.fs.path.dirname(p) else null;
    var source_buffer: [4096]u8 = undefined;
    const source: ?[]const u8 = blk: {
        const dir = source_dir orelse break :blk null;
        if (dir.len == 0) break :blk null;
        if (isAbsolute(dir)) break :blk dir;
        const base = project_dir orelse break :blk null;
        break :blk std.fmt.bufPrint(&source_buffer, "{s}/{s}", .{ base, dir }) catch null;
    };
    const rest = (if (source) |s| underFolder(chosen, s) else null) orelse
        (if (project_dir) |d| underFolder(chosen, d) else null) orelse chosen;
    if (rest.len >= value_capacity) return null;
    for (rest, 0..) |ch, i| out.buffer[i] = if (ch == '/') '\\' else std.ascii.toLower(ch);
    out.len = rest.len;
    return out;
}

fn isAbsolute(path: []const u8) bool {
    if (path.len > 0 and (path[0] == '/' or path[0] == '\\')) return true;
    return path.len > 2 and path[1] == ':' and (path[2] == '/' or path[2] == '\\');
}

/// `path` below `folder` (separators and case folded), without the folder
/// and its separator; null when it is not below it.
fn underFolder(path: []const u8, folder: []const u8) ?[]const u8 {
    var dir = folder;
    while (dir.len > 0 and (dir[dir.len - 1] == '/' or dir[dir.len - 1] == '\\')) dir = dir[0 .. dir.len - 1];
    if (path.len <= dir.len + 1) return null;
    for (dir, path[0..dir.len]) |a, b| {
        const fa = if (a == '\\') '/' else std.ascii.toLower(a);
        const fb = if (b == '\\') '/' else std.ascii.toLower(b);
        if (fa != fb) return null;
    }
    const sep = path[dir.len];
    if (sep != '/' and sep != '\\') return null;
    return path[dir.len + 1 ..];
}

// --- Tree shape ------------------------------------------------------------

/// The node no other node is the parent of (BkResNodes lists it first; the
/// real bridge gives it parent 0, the fake -1).
pub fn rootId(doc: *const Document) ?i32 {
    if (doc.tree.nodes.items.len == 0) return null;
    return doc.tree.nodes.items[0].id;
}

pub fn findNode(doc: *const Document, id: i32) ?*const NodeRecord {
    const i = doc.tree.indexOfNode(id) orelse return null;
    return &doc.tree.nodes.items[i];
}

/// A node's parent and its place among its siblings, in BkResNodes order
/// (the bridge's preorder keeps each item list's order).
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

pub fn childCount(doc: *const Document, parent: i32) i32 {
    var n: i32 = 0;
    for (doc.tree.nodes.items) |peer| if (peer.parent == parent) {
        n += 1;
    };
    return n;
}

/// The child of `parent` at `index`, in order.
pub fn childAt(doc: *const Document, parent: i32, index: i32) ?i32 {
    var i: i32 = 0;
    for (doc.tree.nodes.items) |peer| if (peer.parent == parent) {
        if (i == index) return peer.id;
        i += 1;
    };
    return null;
}

/// True when `ancestor` is `node` or above it.
pub fn isAncestor(doc: *const Document, ancestor: i32, node: i32) bool {
    var at = node;
    var guard: usize = 0;
    while (guard <= doc.tree.nodes.items.len) : (guard += 1) {
        if (at == ancestor) return true;
        const record = findNode(doc, at) orelse return false;
        at = record.parent;
    }
    return false;
}

/// MFC's "Insert item" (IDR_INSERT_TREE_ITEM_MENU): each container item's
/// MyRButtonClick / OnKeyDown(VK_INSERT) added one child of the one class
/// that container holds (CWeaponShootTypesItem a CWeaponDamagePropsItem,
/// CWeaponCratersItem a CWeaponCraterPropsItem, ...). The app shell knows a
/// container's class from the children it already holds; an empty container
/// offers nothing until its sub-editor slice (S06..S15) names the class.
pub fn insertClassFor(doc: *const Document, parent: i32) ?[]const u8 {
    var class: ?[]const u8 = null;
    for (doc.tree.nodes.items) |*peer| if (peer.parent == parent) {
        class = peer.classSlice();
    };
    return class;
}

/// Where a tree drag of `dragged` dropped on `target` puts it, or null when
/// it cannot go there. A node only goes where its kind of item lives: before
/// a sibling of its class under the same or another container, or at the end
/// of a container that already holds items of its class. Never into itself or
/// below itself, and the root does not move.
pub fn dropPlace(doc: *const Document, dragged: i32, target: i32) ?Place {
    if (dragged == target) return null;
    const node = findNode(doc, dragged) orelse return null;
    if (rootId(doc) == dragged) return null;
    if (isAncestor(doc, dragged, target)) return null;
    const class = node.classSlice();
    const target_node = findNode(doc, target) orelse return null;
    if (std.mem.eql(u8, target_node.classSlice(), class)) {
        const at = placeOf(doc, target) orelse return null;
        const from = placeOf(doc, dragged).?;
        var index = at.index;
        // The bridge takes the index after the dragged node has left its list.
        if (from.parent == at.parent and from.index < at.index) index -= 1;
        return .{ .parent = at.parent, .index = index };
    }
    if (insertClassFor(doc, target)) |held| if (std.mem.eql(u8, held, class)) {
        const from = placeOf(doc, dragged).?;
        const count = childCount(doc, target);
        return .{ .parent = target, .index = if (from.parent == target) count - 1 else count };
    };
    return null;
}

// --- Selection -------------------------------------------------------------

/// The tree's selection: several nodes (Ctrl adds or removes one, Shift takes
/// the run between the anchor and the click in display order) and the
/// primary, the node the inspector shows.
pub const Selection = struct {
    ids: std.ArrayListUnmanaged(i32) = .empty,
    primary: ?i32 = null,
    anchor: ?i32 = null,

    pub fn deinit(self: *Selection, allocator: std.mem.Allocator) void {
        self.ids.deinit(allocator);
    }

    pub fn contains(self: *const Selection, id: i32) bool {
        return std.mem.indexOfScalar(i32, self.ids.items, id) != null;
    }

    pub fn clear(self: *Selection) void {
        self.ids.clearRetainingCapacity();
        self.primary = null;
        self.anchor = null;
    }

    pub fn only(self: *Selection, allocator: std.mem.Allocator, id: i32) !void {
        self.ids.clearRetainingCapacity();
        try self.ids.append(allocator, id);
        self.primary = id;
        self.anchor = id;
    }

    /// A click on `id`. `order` is the tree's display order (for Shift).
    pub fn click(self: *Selection, allocator: std.mem.Allocator, id: i32, ctrl: bool, shift: bool, order: []const i32) !void {
        if (shift) if (self.anchor) |anchor| {
            const a = std.mem.indexOfScalar(i32, order, anchor);
            const b = std.mem.indexOfScalar(i32, order, id);
            if (a != null and b != null) {
                if (!ctrl) self.ids.clearRetainingCapacity();
                const lo = @min(a.?, b.?);
                const hi = @max(a.?, b.?);
                for (order[lo .. hi + 1]) |each| if (!self.contains(each)) try self.ids.append(allocator, each);
                self.primary = id;
                return;
            }
        };
        if (ctrl) {
            if (std.mem.indexOfScalar(i32, self.ids.items, id)) |i| {
                _ = self.ids.orderedRemove(i);
                if (self.primary == id) self.primary = if (self.ids.items.len > 0) self.ids.items[self.ids.items.len - 1] else null;
            } else {
                try self.ids.append(allocator, id);
                self.primary = id;
            }
            self.anchor = id;
            return;
        }
        try self.only(allocator, id);
    }

    /// Drops ids the project no longer has (after an undo, a delete, a
    /// reopen).
    pub fn prune(self: *Selection, doc: *const Document) void {
        var i: usize = 0;
        while (i < self.ids.items.len) {
            if (doc.tree.indexOfNode(self.ids.items[i]) == null) _ = self.ids.orderedRemove(i) else i += 1;
        }
        if (self.primary) |p| if (!self.contains(p)) {
            self.primary = if (self.ids.items.len > 0) self.ids.items[self.ids.items.len - 1] else null;
        };
        if (self.anchor) |a| if (doc.tree.indexOfNode(a) == null) {
            self.anchor = self.primary;
        };
    }
};

/// The nodes an inspector edit of `prop_id` reaches: the selected nodes of
/// the primary's class that carry that property (MFC's inspector showed one
/// item; editing the selection is the port's multi-select). Primary first.
pub fn editTargets(allocator: std.mem.Allocator, doc: *const Document, selection: *const Selection, prop_id: i32, out: *std.ArrayListUnmanaged(i32)) !void {
    out.clearRetainingCapacity();
    const primary = selection.primary orelse return;
    const primary_node = findNode(doc, primary) orelse return;
    try out.append(allocator, primary);
    for (selection.ids.items) |id| {
        if (id == primary) continue;
        const node = findNode(doc, id) orelse continue;
        if (!std.mem.eql(u8, node.classSlice(), primary_node.classSlice())) continue;
        if (findProp(doc, id, prop_id) == null) continue;
        try out.append(allocator, id);
    }
}

pub fn findProp(doc: *const Document, node: i32, prop_id: i32) ?*const PropRecord {
    for (doc.tree.props.items) |*p| if (p.node == node and p.record.id == prop_id) return &p.record;
    return null;
}

// --- Edits -----------------------------------------------------------------

/// What every edit needs: the bridge, the open project's mirror and its
/// history (panels_logic.Lifecycle owns both), and whether the project is
/// read-only (another user's lock): a read-only project takes no edit.
pub const Target = struct {
    allocator: std.mem.Allocator,
    bridge: ResBridge,
    doc: *Document,
    history: *History,
    read_only: bool = false,
};

/// Hands out gesture keys: one per press-drag-release or per edit box
/// session. 0 never merges.
pub const Gestures = struct {
    next: u32 = 1,

    pub fn begin(self: *Gestures) u32 {
        const g = self.next;
        self.next +%= 1;
        if (self.next == 0) self.next = 1;
        return g;
    }
};

fn refuseReadOnly(t: Target) EditError!void {
    if (t.read_only) return error.Refused;
}

/// Records a command that has already been applied. Room was reserved before
/// the bridge acted, so this cannot fail.
fn recordApplied(t: Target, command: ResourceCommand, gesture: u32) void {
    t.history.recordAssumeCapacity(t.allocator, command, gesture);
}

/// The top entry when `gesture` continues it.
fn mergeable(t: Target, gesture: u32) ?*core.history.Entry {
    if (gesture == 0) return null;
    const entry = t.history.top() orelse return null;
    if (entry.gesture != gesture) return null;
    return entry;
}

fn sameTargets(command: *const ResourceCommand, nodes: []const i32, prop_id: i32) bool {
    switch (command.*) {
        .set_prop => |c| return nodes.len == 1 and c.node == nodes[0] and c.prop_id == prop_id,
        .composite => |c| {
            if (c.steps.items.len != nodes.len) return false;
            for (c.steps.items, nodes) |*step, node| switch (step.*) {
                .set_prop => |s| if (s.node != node or s.prop_id != prop_id) return false,
                else => return false,
            };
            return true;
        },
        else => return false,
    }
}

/// Writes `text` to `prop_id` on every node of `nodes` as ONE undo step (a
/// set_prop, or a composite of them for several nodes). Within one
/// `gesture` the step grows instead of a new one being recorded; when every
/// value is back where the step began, the step goes. A write the bridge
/// refuses part-way puts the nodes already written back.
pub fn setProp(t: Target, nodes: []const i32, prop_id: i32, text: []const u8, gesture: u32) EditError!void {
    try refuseReadOnly(t);
    if (nodes.len == 0) return;
    if (mergeable(t, gesture)) |entry| if (sameTargets(&entry.command, nodes, prop_id)) {
        // Room for the new after-texts first, so nothing can fail once the
        // bridge holds them.
        const afters = try t.allocator.alloc(OwnedBytes, nodes.len);
        defer t.allocator.free(afters);
        var made: usize = 0;
        errdefer for (afters[0..made]) |*after| after.deinit(t.allocator);
        while (made < nodes.len) : (made += 1) afters[made] = try OwnedBytes.fromSlice(t.allocator, text);
        for (nodes, 0..) |node, i| {
            const status = t.bridge.setProp(node, prop_id, text);
            if (status != .ok) {
                // Back to the step's current after-values on the ones written.
                for (nodes[0..i], 0..) |undo_node, j| _ = t.bridge.setProp(undo_node, prop_id, stepOf(&entry.command, j).after.bytes);
                try bridge.check(status);
            }
        }
        var back_to_start = true;
        for (afters, 0..) |after, i| {
            const step = stepOf(&entry.command, i);
            if (!std.mem.eql(u8, step.before.bytes, text)) back_to_start = false;
            step.after.deinit(t.allocator);
            step.after = after;
        }
        made = 0;
        if (back_to_start) t.history.dropTop(t.allocator) else t.history.touchTop(t.allocator);
        try t.doc.reload(t.allocator, t.bridge);
        return;
    };

    // A value equal to what every node holds is no edit at all.
    var changes = false;
    for (nodes) |node| {
        const prop = findProp(t.doc, node, prop_id) orelse return error.Refused;
        if (!std.mem.eql(u8, prop.valueSlice(), text)) changes = true;
    }
    if (!changes) return;

    try t.history.reserve(t.allocator);
    var command = try buildSetProp(t, nodes, prop_id, text);
    t.doc.apply(t.allocator, t.bridge, &command) catch |err| {
        // Composite apply stops at the failing step; put back what it wrote.
        restoreBefore(t, &command);
        command.deinit(t.allocator);
        t.doc.reload(t.allocator, t.bridge) catch {};
        return err;
    };
    recordApplied(t, command, gesture);
}

fn stepOf(command: *ResourceCommand, i: usize) *@FieldType(ResourceCommand, "set_prop") {
    return switch (command.*) {
        .set_prop => |*c| c,
        .composite => |*c| &c.steps.items[i].set_prop,
        else => unreachable,
    };
}

fn buildSetProp(t: Target, nodes: []const i32, prop_id: i32, text: []const u8) EditError!ResourceCommand {
    if (nodes.len == 1) return try oneSetProp(t, nodes[0], prop_id, text);
    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    errdefer {
        for (steps.items) |*s| s.deinit(t.allocator);
        steps.deinit(t.allocator);
    }
    try steps.ensureTotalCapacity(t.allocator, nodes.len);
    for (nodes) |node| steps.appendAssumeCapacity(try oneSetProp(t, node, prop_id, text));
    return .{ .composite = .{ .steps = steps } };
}

fn oneSetProp(t: Target, node: i32, prop_id: i32, text: []const u8) EditError!ResourceCommand {
    const prop = findProp(t.doc, node, prop_id) orelse return error.Refused;
    var before = try OwnedBytes.fromSlice(t.allocator, prop.valueSlice());
    errdefer before.deinit(t.allocator);
    const after = try OwnedBytes.fromSlice(t.allocator, text);
    return .{ .set_prop = .{ .node = node, .prop_id = prop_id, .before = before, .after = after } };
}

fn restoreBefore(t: Target, command: *ResourceCommand) void {
    switch (command.*) {
        .set_prop => |c| _ = t.bridge.setProp(c.node, c.prop_id, c.before.bytes),
        .composite => |*c| for (c.steps.items) |*s| restoreBefore(t, s),
        else => {},
    }
}

/// Renames a node (A-27): its displayed name, one undo step. An unchanged
/// name records nothing.
pub fn rename(t: Target, node: i32, name: []const u8) EditError!void {
    try refuseReadOnly(t);
    const record = findNode(t.doc, node) orelse return error.Refused;
    if (std.mem.eql(u8, record.displaySlice(), name)) return;
    if (name.len == 0 or name.len >= bridge.name_capacity) return error.BadArgument;
    try t.history.reserve(t.allocator);
    var before = try OwnedBytes.fromSlice(t.allocator, record.displaySlice());
    errdefer before.deinit(t.allocator);
    var after = try OwnedBytes.fromSlice(t.allocator, name);
    errdefer after.deinit(t.allocator);
    var command: ResourceCommand = .{ .rename_node = .{ .node = node, .before = before, .after = after } };
    try t.doc.apply(t.allocator, t.bridge, &command);
    recordApplied(t, command, 0);
}

/// Opens or closes a node in the tree. The project keeps it (the item's
/// expand attribute, written on the next save as MFC's SaveTree did) but it
/// is view state, not an edit: no undo step and no unsaved mark, as in MFC.
pub fn setExpand(t: Target, node: i32, expand: bool) EditError!void {
    const record = findNode(t.doc, node) orelse return error.Refused;
    if (record.expand == expand) return;
    try bridge.check(t.bridge.setNodeExpand(node, expand));
    if (t.doc.tree.findNode(node)) |mirror| mirror.expand = expand;
}

/// View > Expand/Collapse all (A-17, MFC's OnExpandTree): opens or closes
/// every item under the root that has children, one `setExpand` each, so it
/// is view state as well: no undo step, no unsaved mark. The root keeps its
/// own state, as MFC walks only its children. Returns how many items moved.
pub fn setExpandAll(t: Target, expand: bool) EditError!usize {
    const root = rootId(t.doc) orelse return error.Refused;
    var moved: usize = 0;
    var i: usize = 0;
    while (i < t.doc.tree.nodes.items.len) : (i += 1) {
        const record = t.doc.tree.nodes.items[i];
        if (record.id == root or record.expand == expand or childCount(t.doc, record.id) == 0) continue;
        try setExpand(t, record.id, expand);
        moved += 1;
    }
    return moved;
}

/// The items under the root that have children and are open: what Expand/
/// Collapse all changes, read back by the auto tier.
pub fn expandedCount(doc: *const Document) usize {
    const root = rootId(doc) orelse return 0;
    var n: usize = 0;
    for (doc.tree.nodes.items) |record| {
        if (record.id != root and record.expand and childCount(doc, record.id) != 0) n += 1;
    }
    return n;
}

/// "Insert item" on `parent` (A-28): a new child of the class the container
/// holds, at the end of its list as MFC's AddChild put it. One undo step.
/// Returns the new node's id.
pub fn insert(t: Target, parent: i32) EditError!i32 {
    try refuseReadOnly(t);
    const class = insertClassFor(t.doc, parent) orelse return error.Refused;
    try t.history.reserve(t.allocator);
    const class_copy = try t.allocator.dupe(u8, class);
    var command: ResourceCommand = .{ .insert_node = .{
        .parent = parent,
        .class_name = class_copy,
        .index = childCount(t.doc, parent),
        .new_id = -1,
    } };
    errdefer command.deinit(t.allocator);
    try t.doc.apply(t.allocator, t.bridge, &command);
    const new_id = command.insert_node.new_id;
    recordApplied(t, command, 0);
    return new_id;
}

/// "Delete item" (A-28) on every node of `nodes` as ONE undo step. The root
/// is never deleted, and a node below another one being deleted goes with
/// it. Deleted last-first in tree order, so each recorded index is still
/// right when the undo restores them first-first. A refusal part-way restores
/// the ones already deleted and records nothing.
pub fn deleteNodes(t: Target, nodes: []const i32) EditError!void {
    try refuseReadOnly(t);
    const root = rootId(t.doc) orelse return error.Refused;
    var order: std.ArrayListUnmanaged(i32) = .empty;
    defer order.deinit(t.allocator);
    for (t.doc.tree.nodes.items) |record| {
        if (record.id == root) continue;
        if (std.mem.indexOfScalar(i32, nodes, record.id) == null) continue;
        var covered = false;
        for (nodes) |other| if (other != record.id and other != root and isAncestor(t.doc, other, record.id)) {
            covered = true;
        };
        if (!covered) try order.append(t.allocator, record.id);
    }
    if (order.items.len == 0) return error.Refused;

    try t.history.reserve(t.allocator);
    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    errdefer {
        var i = steps.items.len;
        while (i > 0) {
            i -= 1;
            t.doc.undoOne(t.allocator, t.bridge, &steps.items[i]) catch {};
        }
        for (steps.items) |*s| s.deinit(t.allocator);
        steps.deinit(t.allocator);
    }
    try steps.ensureTotalCapacity(t.allocator, order.items.len);
    var i = order.items.len;
    while (i > 0) {
        i -= 1;
        const id = order.items[i];
        const place = placeOf(t.doc, id) orelse return error.Refused;
        var step: ResourceCommand = .{ .delete_node = .{ .parent = place.parent, .index = place.index, .node = id, .blob = .{} } };
        t.doc.apply(t.allocator, t.bridge, &step) catch |err| {
            step.deinit(t.allocator);
            return err;
        };
        steps.appendAssumeCapacity(step);
    }
    if (steps.items.len == 1) {
        const only = steps.items[0];
        steps.deinit(t.allocator);
        recordApplied(t, only, 0);
    } else {
        recordApplied(t, .{ .composite = .{ .steps = steps } }, 0);
    }
}

/// Moves `node` to `place` (a drag in the tree, Move Up / Move Down). One
/// undo step; a move to where it already is records nothing.
pub fn move(t: Target, node: i32, place: Place) EditError!void {
    try refuseReadOnly(t);
    const from = placeOf(t.doc, node) orelse return error.Refused;
    if (from.parent == place.parent and from.index == place.index) return;
    try t.history.reserve(t.allocator);
    var command: ResourceCommand = .{ .move_node = .{
        .node = node,
        .before_parent = from.parent,
        .before_index = from.index,
        .after_parent = place.parent,
        .after_index = place.index,
    } };
    try t.doc.apply(t.allocator, t.bridge, &command);
    recordApplied(t, command, 0);
}

/// Move Up (-1) / Move Down (+1) among the node's siblings.
pub fn moveBy(t: Target, node: i32, delta: i32) EditError!void {
    const from = placeOf(t.doc, node) orelse return error.Refused;
    const to = from.index + delta;
    if (to < 0 or to >= childCount(t.doc, from.parent)) return error.Refused;
    try move(t, node, .{ .parent = from.parent, .index = to });
}

/// Edit > Undo (Ctrl+Z): the top step back through the bridge, onto the redo
/// stack. False when there is nothing to undo. A read-only project's history
/// cannot move (panels_logic.isEnabled).
pub fn undo(t: Target) EditError!bool {
    if (t.read_only) return false;
    const count = t.history.undo_stack.items.len;
    if (count == 0) return false;
    try t.history.redo_stack.ensureUnusedCapacity(t.allocator, 1);
    var entry = t.history.undo_stack.items[count - 1];
    try t.doc.undoOne(t.allocator, t.bridge, &entry.command);
    _ = t.history.undo_stack.pop();
    t.history.redo_stack.appendAssumeCapacity(entry);
    t.history.revision +%= 1;
    return true;
}

/// Edit > Redo (Ctrl+Y, Ctrl+Shift+Z).
pub fn redo(t: Target) EditError!bool {
    if (t.read_only) return false;
    const count = t.history.redo_stack.items.len;
    if (count == 0) return false;
    try t.history.undo_stack.ensureUnusedCapacity(t.allocator, 1);
    var entry = t.history.redo_stack.items[count - 1];
    try t.doc.redoOne(t.allocator, t.bridge, &entry.command);
    _ = t.history.redo_stack.pop();
    t.history.undo_stack.appendAssumeCapacity(entry);
    t.history.revision +%= 1;
    return true;
}

pub const Shortcut = enum { none, undo, redo, delete, insert, rename };

/// The edit keys: Ctrl+Z undo, Ctrl+Y or Ctrl+Shift+Z redo; in the tree,
/// Delete and Insert (MFC's OnKeyDown VK_DELETE / VK_INSERT) and F2 rename.
pub const Key = enum { z, y, delete, insert, f2, other };

pub fn shortcutFor(key: Key, ctrl: bool, shift: bool, tree_focused: bool) Shortcut {
    return switch (key) {
        .z => if (ctrl) (if (shift) .redo else .undo) else .none,
        .y => if (ctrl and !shift) .redo else .none,
        .delete => if (tree_focused and !ctrl) .delete else .none,
        .insert => if (tree_focused and !ctrl) .insert else .none,
        .f2 => if (tree_focused) .rename else .none,
        .other => .none,
    };
}

// --- Reference lists -------------------------------------------------------

/// The 20 reference lists, read once through BkResRefList and kept until
/// `invalidate` (a mod switch changes them). MFC's CReferenceDialog read its
/// list on every open; one read per list is enough here because nothing in
/// the editor writes the data folders they walk.
pub const RefLists = struct {
    lists: [ref_type_count]?[]ReferenceEntry = @splat(null),

    pub fn deinit(self: *RefLists, allocator: std.mem.Allocator) void {
        self.invalidate(allocator);
    }

    pub fn invalidate(self: *RefLists, allocator: std.mem.Allocator) void {
        for (&self.lists) |*slot| {
            if (slot.*) |list| allocator.free(list);
            slot.* = null;
        }
    }

    pub fn get(self: *RefLists, allocator: std.mem.Allocator, b: ResBridge, ref_type: RefType) EditError![]const ReferenceEntry {
        const slot = &self.lists[@intCast(@intFromEnum(ref_type))];
        if (slot.*) |list| return list;
        const list = try readEntries(allocator, b, .{ .ref_list = @intFromEnum(ref_type) });
        slot.* = list;
        return list;
    }
};

const EntrySource = union(enum) { ref_list: i32, prop: struct { node: i32, prop_id: i32 } };

/// Two-pass read of a reference list or a property's strings. Owned.
pub fn readEntries(allocator: std.mem.Allocator, b: ResBridge, source: EntrySource) EditError![]ReferenceEntry {
    var total: usize = 0;
    var none: [0]ReferenceEntry = .{};
    const sizing = switch (source) {
        .ref_list => |t| b.refList(t, &none, &total),
        .prop => |p| b.propStrings(p.node, p.prop_id, &none, &total),
    };
    if (sizing != .ok and sizing != .refused) try bridge.check(sizing);
    if (sizing == .refused and total == 0) try bridge.check(sizing);
    const list = try allocator.alloc(ReferenceEntry, total);
    errdefer allocator.free(list);
    if (total == 0) return list;
    try bridge.check(switch (source) {
        .ref_list => |t| b.refList(t, list, &total),
        .prop => |p| b.propStrings(p.node, p.prop_id, list, &total),
    });
    return list;
}

/// The picker's filter: entries whose name holds `filter`, case-insensitively,
/// as indexes into `entries`.
pub fn filterEntries(allocator: std.mem.Allocator, entries: []const ReferenceEntry, filter: []const u8, out: *std.ArrayListUnmanaged(usize)) !void {
    out.clearRetainingCapacity();
    const needle = std.mem.trim(u8, filter, " ");
    for (entries, 0..) |*entry, i| {
        if (needle.len == 0 or std.ascii.findIgnoreCase(entry.nameSlice(), needle) != null) try out.append(allocator, i);
    }
}

// --- Tests -----------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;

/// A project of a root, a "Shells" container with two "Shell" children, and a
/// "Craters" container with one "Crater"; every Shell has props 1 (combo
/// "Trajectory": line/howitzer/bomb) and 2 (float "Speed").
const Fixture = struct {
    fake: FakeResBridge,
    doc: Document = .{},
    history: History = .{},
    root: i32 = 0,
    shells: i32 = 0,
    shell_a: i32 = 0,
    shell_b: i32 = 0,
    craters: i32 = 0,
    crater: i32 = 0,

    fn init(self: *Fixture) !void {
        const a = testing.allocator;
        self.* = .{ .fake = FakeResBridge.init(a) };
        const b = self.fake.bridge();
        try bridge.check(b.new(.weapon));
        self.root = self.fake.nodes.items[0].id;
        try bridge.check(b.insertNode(self.root, "Shells", 0, &self.shells));
        try bridge.check(b.insertNode(self.shells, "Shell", 0, &self.shell_a));
        try bridge.check(b.insertNode(self.shells, "Shell", 1, &self.shell_b));
        try bridge.check(b.insertNode(self.root, "Craters", 1, &self.craters));
        try bridge.check(b.insertNode(self.craters, "Crater", 0, &self.crater));
        for (self.fake.nodes.items) |*node| {
            if (!std.mem.eql(u8, std.mem.sliceTo(&node.class, 0), "Shell")) continue;
            var trajectory: PropRecord = .{ .id = 1, .domain_type = @intFromEnum(Domain.combo), .value_kind = @intFromEnum(ValueKind.str), .combo_count = 3 };
            _ = trajectory.setDisplay("Trajectory");
            _ = trajectory.setValue("line");
            var speed: PropRecord = .{ .id = 2, .domain_type = @intFromEnum(Domain.float), .value_kind = @intFromEnum(ValueKind.float) };
            _ = speed.setDisplay("Speed");
            _ = speed.setValue("10");
            try node.props.append(a, trajectory);
            try node.props.append(a, speed);
            try self.fake.setPropStrings(node.id, 1, &.{ "line", "howitzer", "bomb" });
        }
        try self.doc.reload(a, b);
    }

    fn deinit(self: *Fixture) void {
        self.history.deinit(testing.allocator);
        self.doc.deinit(testing.allocator);
        self.fake.deinit();
    }

    fn target(self: *Fixture) Target {
        return .{ .allocator = testing.allocator, .bridge = self.fake.bridge(), .doc = &self.doc, .history = &self.history };
    }

    fn value(self: *Fixture, node: i32, prop_id: i32) []const u8 {
        return findProp(&self.doc, node, prop_id).?.valueSlice();
    }
};

test "every DT_* domain gets MFC's widget and every reference domain its list" {
    try testing.expectEqual(Widget.int, widgetFor(.dec));
    try testing.expectEqual(Widget.hex, widgetFor(.hex));
    try testing.expectEqual(Widget.text, widgetFor(.str));
    try testing.expectEqual(Widget.check, widgetFor(.bool));
    try testing.expectEqual(Widget.browse_file, widgetFor(.browse));
    try testing.expectEqual(Widget.browse_dir, widgetFor(.browse_dir));
    try testing.expectEqual(Widget.combo, widgetFor(.combo));
    try testing.expectEqual(Widget.color, widgetFor(.color));
    try testing.expectEqual(Widget.float, widgetFor(.float));
    try testing.expectEqual(Widget.action_mask, widgetFor(.action_ref));
    try testing.expectEqual(Widget.read_only, widgetFor(.err));
    try testing.expectEqual(Widget.read_only, widgetFor(.custom));
    try testing.expectEqual(Widget.read_only, widgetFor(@enumFromInt(99)));
    // 20 reference domains, each to a different list, all 20 lists reached.
    var seen = std.EnumSet(RefType).empty;
    var count: usize = 0;
    var d: i32 = @intFromEnum(Domain.animation_ref);
    while (d < @intFromEnum(Domain.custom)) : (d += 1) {
        const ref = refTypeFor(@enumFromInt(d)).?;
        try testing.expect(!seen.contains(ref));
        seen.insert(ref);
        count += 1;
        if (d != @intFromEnum(Domain.action_ref)) try testing.expectEqual(Widget.reference, widgetFor(@enumFromInt(d)));
    }
    try testing.expectEqual(@as(usize, 20), count);
    try testing.expectEqual(@as(usize, ref_type_count), seen.count());
    // ConvertFromDomenTypeToRef's two swaps.
    try testing.expectEqual(RefType.deathhole, refTypeFor(.death_ref).?);
    try testing.expectEqual(RefType.crater, refTypeFor(.crater_ref).?);
    try testing.expectEqual(RefType.water_texture, refTypeFor(.water_texture_ref).?);
    try testing.expectEqual(RefType.road_texture, refTypeFor(.road_texture_ref).?);
    try testing.expectEqual(@as(i32, 5), @intFromEnum(RefType.actions));
    try testing.expectEqual(@as(i32, 19), @intFromEnum(RefType.water_texture));
}

test "typed values are checked before the bridge parses them" {
    try testing.expectEqualStrings("42", parseTyped(.int, .int, " 42 ").?.slice());
    try testing.expect(parseTyped(.int, .int, "4x2") == null);
    try testing.expect(parseTyped(.int, .int, "") == null);
    try testing.expectEqualStrings("-7", parseTyped(.int, .int64, "-7").?.slice());
    try testing.expectEqualStrings("2.5", parseTyped(.float, .float, "2.5").?.slice());
    try testing.expect(parseTyped(.float, .float, "fast") == null);
    try testing.expect(parseTyped(.float, .float, "inf") == null);
    try testing.expectEqualStrings("255", parseTyped(.hex, .int, "0xFF").?.slice());
    try testing.expectEqualStrings("-1", parseTyped(.hex, .int, "ffffffff").?.slice());
    try testing.expectEqualStrings("0x1F", hexDisplay(.int, "31").?.slice());
    try testing.expectEqualStrings("any text", parseTyped(.text, .str, "any text").?.slice());
    var long: [value_capacity]u8 = undefined;
    @memset(&long, 'a');
    try testing.expect(parseTyped(.text, .str, &long) == null);
}

test "combo, bool and colour values in the form each value kind is stored in" {
    var options: [3]ReferenceEntry = .{ .{ .token = 0 }, .{ .token = 1 }, .{ .token = 2 } };
    _ = options[0].setName("line");
    _ = options[1].setName("howitzer");
    _ = options[2].setName("bomb");
    try testing.expectEqual(@as(?usize, 1), comboCurrent(.str, &options, "howitzer"));
    try testing.expectEqual(@as(?usize, null), comboCurrent(.str, &options, "rocket"));
    try testing.expectEqualStrings("bomb", comboText(.str, &options, 2).?.slice());
    try testing.expectEqual(@as(?usize, 2), comboCurrent(.combo, &options, "2"));
    try testing.expectEqualStrings("1", comboText(.combo, &options, 1).?.slice());
    try testing.expect(comboText(.combo, &options, 3) == null);

    try testing.expect(boolValue(.bool, &.{}, "1"));
    try testing.expect(!boolValue(.bool, &.{}, "0"));
    try testing.expectEqualStrings("0", boolText(.bool, &.{}, false).?.slice());
    var yes_no: [2]ReferenceEntry = .{ .{ .token = 0 }, .{ .token = 1 } };
    _ = yes_no[0].setName("No");
    _ = yes_no[1].setName("Yes");
    try testing.expect(boolValue(.str, &yes_no, "Yes"));
    try testing.expectEqualStrings("No", boolText(.str, &yes_no, false).?.slice());

    // COLORREF: red in the low byte; the high byte survives a pick.
    const rgb = colorRgb(.color, "FF0000FF").?;
    try testing.expectEqual(@as(f32, 1), rgb[0]);
    try testing.expectEqual(@as(f32, 0), rgb[2]);
    try testing.expectEqualStrings("FFFF0000", colorText(.color, "FF0000FF", .{ 0, 0, 1 }).?.slice());
    try testing.expectEqualStrings("65280", colorText(.int, "0", .{ 0, 1, 0 }).?.slice());
}

test "the actions mask sets one bit per chosen action id, in the variant's text" {
    var mask: u64 = 0;
    mask = toggleAction(mask, 0, true);
    mask = toggleAction(mask, 9, true);
    try testing.expectEqual(@as(u64, 0x201), mask);
    try testing.expectEqualStrings("513", actionMaskText(.int64, mask).?.slice());
    try testing.expectEqualStrings("201", actionMaskText(.str, mask).?.slice());
    try testing.expectEqual(mask, actionMask(.int64, "513"));
    try testing.expectEqual(mask, actionMask(.str, "201"));
    try testing.expectEqual(@as(u64, 1), toggleAction(mask, 9, false));
    try testing.expectEqual(mask, toggleAction(mask, 64, true));
}

test "browse writes the path relative to the source folder, else the project folder, lower case" {
    try testing.expectEqualStrings("textures\\grass.dds", browseValue("/home/u/proj/Textures/Grass.dds", null, "/home/u/proj/x.wpn").?.slice());
    try testing.expectEqualStrings("grass.dds", browseValue("/home/u/proj/Textures/Grass.dds", "Textures", "/home/u/proj/x.wpn").?.slice());
    try testing.expectEqualStrings("grass.dds", browseValue("C:\\Data\\Textures\\Grass.dds", "c:/data/textures/", null).?.slice());
    try testing.expectEqualStrings("\\other\\a.san", browseValue("/other/A.san", null, "/home/u/proj/x.wpn").?.slice());
    // A folder that only starts with the same letters is not the folder.
    try testing.expectEqualStrings("\\home\\u\\project2\\a", browseValue("/home/u/project2/a", null, "/home/u/proj/x.wpn").?.slice());
}

test "selection: click, Ctrl toggles, Shift takes the run, prune follows the tree" {
    const a = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    var sel: Selection = .{};
    defer sel.deinit(a);
    const order = [_]i32{ fx.root, fx.shells, fx.shell_a, fx.shell_b, fx.craters, fx.crater };
    try sel.click(a, fx.shell_a, false, false, &order);
    try testing.expectEqualSlices(i32, &.{fx.shell_a}, sel.ids.items);
    try sel.click(a, fx.crater, true, false, &order);
    try testing.expectEqualSlices(i32, &.{ fx.shell_a, fx.crater }, sel.ids.items);
    try testing.expectEqual(fx.crater, sel.primary.?);
    try sel.click(a, fx.crater, true, false, &order);
    try testing.expectEqualSlices(i32, &.{fx.shell_a}, sel.ids.items);
    try testing.expectEqual(fx.shell_a, sel.primary.?);
    try sel.click(a, fx.shell_a, false, false, &order);
    try sel.click(a, fx.craters, false, true, &order);
    try testing.expectEqualSlices(i32, &.{ fx.shell_a, fx.shell_b, fx.craters }, sel.ids.items);
    try deleteNodes(fx.target(), &.{fx.shell_b});
    sel.prune(&fx.doc);
    try testing.expectEqualSlices(i32, &.{ fx.shell_a, fx.craters }, sel.ids.items);
    try testing.expectEqual(fx.craters, sel.primary.?);
}

test "a property edit on several selected nodes is one undo step, and only reaches nodes of the primary's class" {
    const a = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    var sel: Selection = .{};
    defer sel.deinit(a);
    const order = [_]i32{ fx.root, fx.shells, fx.shell_a, fx.shell_b, fx.craters, fx.crater };
    try sel.click(a, fx.shell_a, false, false, &order);
    try sel.click(a, fx.shell_b, true, false, &order);
    try sel.click(a, fx.crater, true, false, &order);
    try sel.click(a, fx.shell_b, true, false, &order);
    try sel.click(a, fx.shell_b, true, false, &order); // primary shell_b again
    var targets: std.ArrayListUnmanaged(i32) = .empty;
    defer targets.deinit(a);
    try editTargets(a, &fx.doc, &sel, 1, &targets);
    try testing.expectEqualSlices(i32, &.{ fx.shell_b, fx.shell_a }, targets.items);

    try setProp(fx.target(), targets.items, 1, "bomb", 0);
    try testing.expectEqualStrings("bomb", fx.value(fx.shell_a, 1));
    try testing.expectEqualStrings("bomb", fx.value(fx.shell_b, 1));
    try testing.expectEqual(@as(usize, 1), fx.history.undo_stack.items.len);
    try testing.expect(fx.history.dirty());
    try testing.expect(try undo(fx.target()));
    try testing.expectEqualStrings("line", fx.value(fx.shell_a, 1));
    try testing.expectEqualStrings("line", fx.value(fx.shell_b, 1));
    try testing.expect(!fx.history.dirty());
    try testing.expect(try redo(fx.target()));
    try testing.expectEqualStrings("bomb", fx.value(fx.shell_a, 1));
    try testing.expect(!(try redo(fx.target())));
}

test "a gesture's edits collapse into one step, and coming back to the start drops it and the dirty mark" {
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    var gestures: Gestures = .{};
    const drag = gestures.begin();
    try setProp(fx.target(), &.{fx.shell_a}, 2, "11", drag);
    try setProp(fx.target(), &.{fx.shell_a}, 2, "12", drag);
    try setProp(fx.target(), &.{fx.shell_a}, 2, "13", drag);
    try testing.expectEqual(@as(usize, 1), fx.history.undo_stack.items.len);
    try testing.expectEqualStrings("10", fx.history.top().?.command.set_prop.before.bytes);
    try testing.expectEqualStrings("13", fx.history.top().?.command.set_prop.after.bytes);

    // Save here: clean; a new gesture is a second step and dirties again.
    fx.history.markClean();
    try testing.expect(!fx.history.dirty());
    const second = gestures.begin();
    try setProp(fx.target(), &.{fx.shell_a}, 2, "20", second);
    try testing.expectEqual(@as(usize, 2), fx.history.undo_stack.items.len);
    try testing.expect(fx.history.dirty());
    // Dragged back to 13: the step goes and the project is clean again.
    try setProp(fx.target(), &.{fx.shell_a}, 2, "13", second);
    try testing.expectEqual(@as(usize, 1), fx.history.undo_stack.items.len);
    try testing.expect(!fx.history.dirty());
    try testing.expectEqualStrings("13", fx.value(fx.shell_a, 2));

    // Undo past the save mark is dirty; redo back to it is clean.
    try testing.expect(try undo(fx.target()));
    try testing.expectEqualStrings("10", fx.value(fx.shell_a, 2));
    try testing.expect(fx.history.dirty());
    try testing.expect(try redo(fx.target()));
    try testing.expect(!fx.history.dirty());

    // Gesture 0 never merges; an unchanged value records nothing.
    try setProp(fx.target(), &.{fx.shell_a}, 2, "14", 0);
    try setProp(fx.target(), &.{fx.shell_a}, 2, "15", 0);
    try setProp(fx.target(), &.{fx.shell_a}, 2, "15", 0);
    try testing.expectEqual(@as(usize, 3), fx.history.undo_stack.items.len);
}

test "a refused property write leaves the project and the history as they were" {
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    try testing.expectError(error.Refused, setProp(fx.target(), &.{ fx.shell_a, fx.crater }, 2, "3", 0));
    try testing.expectEqualStrings("10", fx.value(fx.shell_a, 2));
    try testing.expectEqual(@as(usize, 0), fx.history.undo_stack.items.len);
    var t = fx.target();
    t.read_only = true;
    try testing.expectError(error.Refused, setProp(t, &.{fx.shell_a}, 2, "3", 0));
    try testing.expectError(error.Refused, rename(t, fx.shell_a, "x"));
    try testing.expectError(error.Refused, insert(t, fx.shells));
    try testing.expectError(error.Refused, deleteNodes(t, &.{fx.shell_a}));
    try testing.expect(!(try undo(t)));
}

test "rename is one undo step; expand is kept but is no edit" {
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    try rename(fx.target(), fx.shell_a, "Main shell");
    try testing.expectEqualStrings("Main shell", findNode(&fx.doc, fx.shell_a).?.displaySlice());
    try testing.expectError(error.BadArgument, rename(fx.target(), fx.shell_a, ""));
    try rename(fx.target(), fx.shell_a, "Main shell");
    try testing.expectEqual(@as(usize, 1), fx.history.undo_stack.items.len);
    try testing.expect(try undo(fx.target()));
    try testing.expectEqualStrings("Shell", findNode(&fx.doc, fx.shell_a).?.displaySlice());

    fx.history.markClean();
    try setExpand(fx.target(), fx.shells, false);
    try testing.expect(!findNode(&fx.doc, fx.shells).?.expand);
    for (fx.fake.nodes.items) |node| if (node.id == fx.shells) try testing.expect(!node.expand);
    try testing.expect(!fx.history.dirty());
    try testing.expectEqual(@as(usize, 0), fx.history.undo_stack.items.len);
}

test "Expand/Collapse all moves every container under the root, as view state only" {
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    fx.history.markClean();
    // Shells and Craters hold children; Shell and Crater are leaves and the root keeps its own state.
    _ = try setExpandAll(fx.target(), true);
    try testing.expectEqual(@as(usize, 2), expandedCount(&fx.doc));
    try testing.expectEqual(@as(usize, 0), try setExpandAll(fx.target(), true));
    try testing.expectEqual(@as(usize, 2), try setExpandAll(fx.target(), false));
    try testing.expectEqual(@as(usize, 0), expandedCount(&fx.doc));
    for (fx.fake.nodes.items) |node| {
        if (node.id == fx.shells or node.id == fx.craters) try testing.expect(!node.expand);
    }
    try testing.expectEqual(@as(usize, 2), try setExpandAll(fx.target(), true));
    try testing.expect(findNode(&fx.doc, fx.shells).?.expand);
    try testing.expect(!fx.history.dirty());
    try testing.expectEqual(@as(usize, 0), fx.history.undo_stack.items.len);
}

test "insert adds the container's own class at the end; an empty container offers nothing" {
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    try testing.expectEqualStrings("Shell", insertClassFor(&fx.doc, fx.shells).?);
    try testing.expect(insertClassFor(&fx.doc, fx.shell_a) == null);
    const added = try insert(fx.target(), fx.shells);
    try testing.expectEqualStrings("Shell", findNode(&fx.doc, added).?.classSlice());
    try testing.expectEqual(fx.shells, findNode(&fx.doc, added).?.parent);
    try testing.expectEqual(@as(i32, 3), childCount(&fx.doc, fx.shells));
    try testing.expectError(error.Refused, insert(fx.target(), fx.shell_a));
    try testing.expect(try undo(fx.target()));
    try testing.expectEqual(@as(i32, 2), childCount(&fx.doc, fx.shells));
    try testing.expect(try redo(fx.target()));
    try testing.expectEqual(@as(i32, 3), childCount(&fx.doc, fx.shells));
}

test "deleting a multi-selection is one step, skips the root and nodes under a deleted one, and undo brings all back" {
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    const before = fx.doc.tree.nodes.items.len;
    try deleteNodes(fx.target(), &.{ fx.root, fx.shells, fx.shell_a, fx.crater });
    try testing.expectEqual(@as(usize, 1), fx.history.undo_stack.items.len);
    try testing.expectEqual(before - 4, fx.doc.tree.nodes.items.len);
    try testing.expect(findNode(&fx.doc, fx.root) != null);
    try testing.expect(findNode(&fx.doc, fx.craters) != null);
    try testing.expect(try undo(fx.target()));
    try testing.expectEqual(before, fx.doc.tree.nodes.items.len);
    try testing.expectEqual(fx.shells, findNode(&fx.doc, fx.shell_a).?.parent);
    try testing.expectEqual(fx.craters, findNode(&fx.doc, fx.crater).?.parent);
    try testing.expect(try redo(fx.target()));
    try testing.expectEqual(before - 4, fx.doc.tree.nodes.items.len);
    try testing.expectError(error.Refused, deleteNodes(fx.target(), &.{fx.root}));
}

test "drag and move: a node goes only where its class lives, never below itself, one step each" {
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    try testing.expect(dropPlace(&fx.doc, fx.shell_a, fx.shell_a) == null);
    try testing.expect(dropPlace(&fx.doc, fx.shells, fx.shell_a) == null);
    try testing.expect(dropPlace(&fx.doc, fx.root, fx.craters) == null);
    try testing.expect(dropPlace(&fx.doc, fx.shell_a, fx.craters) == null);
    try testing.expect(dropPlace(&fx.doc, fx.crater, fx.shells) == null);
    const into = dropPlace(&fx.doc, fx.shell_b, fx.shells).?;
    try testing.expectEqual(fx.shells, into.parent);
    try testing.expectEqual(@as(i32, 1), into.index);
    const before_a = dropPlace(&fx.doc, fx.shell_b, fx.shell_a).?;
    try testing.expectEqual(@as(i32, 0), before_a.index);

    try move(fx.target(), fx.shell_b, .{ .parent = fx.craters, .index = 0 });
    try testing.expectEqual(fx.craters, findNode(&fx.doc, fx.shell_b).?.parent);
    try testing.expectEqual(@as(usize, 1), fx.history.undo_stack.items.len);
    try testing.expect(try undo(fx.target()));
    try testing.expectEqual(fx.shells, findNode(&fx.doc, fx.shell_b).?.parent);
    try testing.expectEqual(Place{ .parent = fx.shells, .index = 1 }, placeOf(&fx.doc, fx.shell_b).?);
    try move(fx.target(), fx.shell_b, .{ .parent = fx.shells, .index = 1 });
    try testing.expectEqual(@as(usize, 0), fx.history.undo_stack.items.len);
    try testing.expectError(error.Refused, moveBy(fx.target(), fx.shell_b, 1));
    try testing.expectError(error.Refused, moveBy(fx.target(), fx.shell_a, -1));
}

test "edit shortcuts: Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z, and the tree's Delete, Insert and F2" {
    try testing.expectEqual(Shortcut.undo, shortcutFor(.z, true, false, false));
    try testing.expectEqual(Shortcut.redo, shortcutFor(.z, true, true, false));
    try testing.expectEqual(Shortcut.redo, shortcutFor(.y, true, false, true));
    try testing.expectEqual(Shortcut.none, shortcutFor(.z, false, false, true));
    try testing.expectEqual(Shortcut.delete, shortcutFor(.delete, false, false, true));
    try testing.expectEqual(Shortcut.none, shortcutFor(.delete, false, false, false));
    try testing.expectEqual(Shortcut.insert, shortcutFor(.insert, false, false, true));
    try testing.expectEqual(Shortcut.rename, shortcutFor(.f2, false, false, true));
}

test "reference lists and property strings are read through the bridge, cached and filtered" {
    const a = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init();
    defer fx.deinit();
    var entries: [3]ReferenceEntry = .{ .{ .token = 0 }, .{ .token = 1 }, .{ .token = 2 } };
    _ = entries[0].setName("weapons\\mg34");
    _ = entries[1].setName("weapons\\Flak88");
    _ = entries[2].setName("weapons\\mp40");
    try fx.fake.setReferenceList(@intFromEnum(RefType.weapons), &entries);
    var lists: RefLists = .{};
    defer lists.deinit(a);
    const got = try lists.get(a, fx.fake.bridge(), .weapons);
    try testing.expectEqual(@as(usize, 3), got.len);
    // Cached: a changed list is seen only after invalidate (a mod switch).
    try fx.fake.setReferenceList(@intFromEnum(RefType.weapons), entries[0..1]);
    try testing.expectEqual(@as(usize, 3), (try lists.get(a, fx.fake.bridge(), .weapons)).len);
    lists.invalidate(a);
    try testing.expectEqual(@as(usize, 1), (try lists.get(a, fx.fake.bridge(), .weapons)).len);
    try testing.expectEqual(@as(usize, 0), (try lists.get(a, fx.fake.bridge(), .music)).len);

    var hits: std.ArrayListUnmanaged(usize) = .empty;
    defer hits.deinit(a);
    try filterEntries(a, &entries, "M", &hits);
    try testing.expectEqualSlices(usize, &.{ 0, 2 }, hits.items);
    try filterEntries(a, &entries, "flak", &hits);
    try testing.expectEqualSlices(usize, &.{1}, hits.items);
    try filterEntries(a, &entries, "", &hits);
    try testing.expectEqual(@as(usize, 3), hits.items.len);

    const strings = try readEntries(a, fx.fake.bridge(), .{ .prop = .{ .node = fx.shell_a, .prop_id = 1 } });
    defer a.free(strings);
    try testing.expectEqual(@as(usize, 3), strings.len);
    try testing.expectEqualStrings("howitzer", strings[1].nameSlice());
    const none = try readEntries(a, fx.fake.bridge(), .{ .prop = .{ .node = fx.shell_a, .prop_id = 2 } });
    defer a.free(none);
    try testing.expectEqual(@as(usize, 0), none.len);
    try testing.expectError(error.Refused, readEntries(a, fx.fake.bridge(), .{ .prop = .{ .node = fx.shell_a, .prop_id = 77 } }));

    // Picking from a reference list is an ordinary property write.
    try setProp(fx.target(), &.{fx.shell_a}, 1, comboText(.str, strings, 2).?.slice(), 0);
    try testing.expectEqualStrings("bomb", fx.value(fx.shell_a, 1));
}
