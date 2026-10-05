//! An in-memory resource project behind the ResBridge interface, for the
//! core tier. Keeps the real bridge's rules the core can see: refusals for
//! missing node / unknown prop / project not open; `BadArgument` for a
//! duplicate id or an out-of-range insert index; two-pass readers return the
//! full count and refuse a short buffer. The fake is knowingly simpler in
//! these ways, which a core test must not lean on:
//!
//! * there is no CTreeItemFactory: `insertNode` accepts any class name and
//!   builds a fresh node with no default props - a real parent/class
//!   acceptance check is the C adapter's problem (S05+);
//! * there is no NResourceXml: the "serialised subtree" `deleteNode` returns
//!   is a plain byte dump of the fake's own tree struct, which
//!   `restoreNode` reads back; the bytes are self-describing enough to
//!   support delete -> restore (and only that);
//! * there is no file: `open` reads the fake's own prepared fixtures by
//!   path, `save` records that the path was written to the message buffer;
//!   a lock is kept purely in-memory;
//! * references are returned from a tiny fixed list the test fills
//!   through `setReferenceList`; a property's strings (combo choices, a
//!   browse's folder and filter) likewise through `setPropStrings`, kept
//!   beside the tree, so a delete and restore does not carry them;
//! * export writes nothing: a kind the test marks with `setExportable` counts
//!   one written file per export, every other kind is refused as "not
//!   ported yet" as the real bridge answers until a sub-editor registers its
//!   exporter. A batch walks the projects `addBatchProject` declared whose
//!   path starts with the source folder;
//! * MOD settings live in memory; `shipped_root` (and its Data folder) stands
//!   for the shipped game the real bridge refuses to write into. A pack
//!   stores a marker in `files` and needs a prior `modSettingsSet`;
//! * import knows only the folders `addGameFolder` declared, each with its
//!   key name; it builds a root plus one "Basic Info" node whose Name prop
//!   is that key name. Only infantry imports, as in the real bridge;
//! * the preview methods only set the message buffer - there is no scene,
//!   no device, no draw. `no GPU device` is simulated by `setNoDevice`. A
//!   unit preview keeps its variant, direction and two display flags, and
//!   `meshLocators` answers the Locators children with positions made up
//!   from their order;
//! * a unit project's Locators children follow the Graphics combat model name
//!   the way the real bridge's rebuild does, from the models a test declared
//!   (`addMeshModel`: a file name and its skeleton nodes). The three locator
//!   combos are computed from those children, not stored;
//! * geometry channels round-trip through a per-(node, channel) map but
//!   do no engine-shaped validation - a shoot point far outside a mesh is
//!   accepted. The one rule kept is the payload family: a value whose tag
//!   is not `channel.family()` is `BadArgument`, since the C entry point
//!   for that channel could not even be called with it. The real bridge
//!   also refuses a channel on a node where MFC has no home for it; the
//!   fake does that only once a test declares homes (`addGeometryHome`),
//!   so the older tests that write any channel on the root keep working.
//!   A save carries the geometry map, so a reopen reads it back.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const Status = bridge_mod.Status;
const Kind = bridge_mod.Kind;
const NodeRecord = bridge_mod.NodeRecord;
const PropRecord = bridge_mod.PropRecord;
const ReferenceEntry = bridge_mod.ReferenceEntry;
const GeometryChannel = bridge_mod.GeometryChannel;
const GeometryValue = bridge_mod.GeometryValue;
const ExportFlags = bridge_mod.ExportFlags;
const ExportReport = bridge_mod.ExportReport;
const Warning = bridge_mod.Warning;
const ModSettings = bridge_mod.ModSettings;
const ResBridge = bridge_mod.ResBridge;
const MeshLocator = bridge_mod.MeshLocator;
const putName = bridge_mod.putName;
const item_type = @import("sub_editor_tools.zig").item_type;

const name_capacity = bridge_mod.name_capacity;
const value_text_capacity = bridge_mod.value_text_capacity;
const message_capacity: usize = 256;

/// One node of the fake tree. Owns its props.
pub const FakeNode = struct {
    id: i32,
    parent: i32,
    class: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    display: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    expand: bool = true,
    props: std.ArrayListUnmanaged(PropRecord) = .empty,

    pub fn deinit(self: *FakeNode, allocator: std.mem.Allocator) void {
        self.props.deinit(allocator);
    }
};

/// A geometry mapping entry: a node id + a channel + the value it holds.
const GeometryEntry = struct {
    node: i32,
    channel: GeometryChannel,
    value: GeometryValue,
};

pub const FakeResBridge = struct {
    allocator: std.mem.Allocator,
    message_buffer: [message_capacity]u8 = [_]u8{0} ** message_capacity,
    message_len: usize = 0,
    /// Null when no project is open; otherwise the project kind.
    kind: ?Kind = null,
    nodes: std.ArrayListUnmanaged(FakeNode) = .empty,
    /// Monotonic id source - never reused in the lifetime of the fake, so a
    /// test can trust that an inserted node keeps a stable id.
    next_id: i32 = 1,
    /// A per-process advisory lock owner; null is unlocked.
    lock_owner: ?[]u8 = null,
    references: std.AutoHashMapUnmanaged(i32, std.ArrayListUnmanaged(ReferenceEntry)) = .empty,
    prop_strings: std.ArrayListUnmanaged(PropStrings) = .empty,
    geometry: std.ArrayListUnmanaged(GeometryEntry) = .empty,
    /// The (node, channel) pairs a test declared as MFC homes. Empty means
    /// every node takes every channel, as before homes existed.
    geometry_homes: std.ArrayListUnmanaged(GeometryHome) = .empty,
    /// When true, every preview call answers BK_EDITOR_NO_DEVICE - the fake's
    /// stand-in for a headless host without a GPU.
    no_device: bool = false,
    /// Tracks preview state: `.closed` is the start, `.open` after
    /// previewBegin, `.showing` after previewShow. previewStop goes back to
    /// `.closed`.
    preview_state: PreviewState = .closed,
    /// Whether the last previewPlayback asked for the animation to run.
    preview_playing: bool = false,
    /// The unit preview's model variant, turn in degrees and display flags.
    mesh_variant: u8 = 0,
    mesh_direction: i32 = 0,
    show_locators: bool = false,
    show_bounding_boxes: bool = false,
    /// The skeletons `addMeshModel` declared, by combat model file name.
    mesh_models: std.ArrayListUnmanaged(MeshModel) = .empty,
    /// Rolling file store the test can read saved blobs out of. Keyed by
    /// path. Owns its values.
    files: std.StringHashMapUnmanaged([]u8) = .empty,
    /// True once the open project was opened from or saved to a path: an
    /// export reads its sources beside the project file, so it needs one.
    has_path: bool = false,
    /// Kinds whose exporter the test declares ported (`setExportable`).
    exportable: std.EnumSet(Kind) = .initEmpty(),
    /// What the last successful export was asked, and how many there were.
    exports: u32 = 0,
    last_flags: ExportFlags = .{},
    last_stats_only: bool = false,
    mod: ModSettings = .{},
    mod_set: bool = false,
    shipped_root: []const u8 = "game",
    batch_projects: std.ArrayListUnmanaged(BatchProject) = .empty,
    /// Runtime folder path -> the KeyName its 1.xml holds. Owns both.
    game_folders: std.StringHashMapUnmanaged([]u8) = .empty,

    pub const PreviewState = enum { closed, open, showing };
    pub const PropStrings = struct { node: i32, prop_id: i32, entries: std.ArrayListUnmanaged(ReferenceEntry) = .empty };
    pub const BatchProject = struct { path: []u8, kind: Kind };
    pub const MeshNodeDef = struct { name: []const u8, locator: bool = false };
    pub const MeshModel = struct { file: []u8, nodes: []MeshNodeDef };
    pub const GeometryHome = struct { node: i32, channel: GeometryChannel };

    pub fn init(allocator: std.mem.Allocator) FakeResBridge {
        var fake: FakeResBridge = .{ .allocator = allocator };
        _ = fake.mod.setExportDir("mods/mymod");
        return fake;
    }

    pub fn deinit(self: *FakeResBridge) void {
        for (self.nodes.items) |*n| n.deinit(self.allocator);
        self.nodes.deinit(self.allocator);
        if (self.lock_owner) |owner| self.allocator.free(owner);
        self.lock_owner = null;
        var ref_it = self.references.iterator();
        while (ref_it.next()) |entry| entry.value_ptr.deinit(self.allocator);
        self.references.deinit(self.allocator);
        for (self.prop_strings.items) |*p| p.entries.deinit(self.allocator);
        self.prop_strings.deinit(self.allocator);
        for (self.geometry.items) |*g| g.value.deinit(self.allocator);
        self.geometry.deinit(self.allocator);
        self.geometry_homes.deinit(self.allocator);
        var file_it = self.files.iterator();
        while (file_it.next()) |e| {
            self.allocator.free(e.key_ptr.*);
            self.allocator.free(e.value_ptr.*);
        }
        self.files.deinit(self.allocator);
        for (self.mesh_models.items) |m| {
            for (m.nodes) |n| self.allocator.free(n.name);
            self.allocator.free(m.nodes);
            self.allocator.free(m.file);
        }
        self.mesh_models.deinit(self.allocator);
        for (self.batch_projects.items) |p| self.allocator.free(p.path);
        self.batch_projects.deinit(self.allocator);
        var folder_it = self.game_folders.iterator();
        while (folder_it.next()) |e| {
            self.allocator.free(e.key_ptr.*);
            self.allocator.free(e.value_ptr.*);
        }
        self.game_folders.deinit(self.allocator);
    }

    pub fn setExportable(self: *FakeResBridge, kind: Kind, ported: bool) void {
        self.exportable.setPresent(kind, ported);
    }

    /// Declares the skeleton a combat model file holds: the nodes in
    /// skeleton order, `locator` marking those the skeleton lists as
    /// locators. A later Graphics model name equal to `file` gets one Locators
    /// child per node.
    pub fn addMeshModel(self: *FakeResBridge, file: []const u8, nodes: []const MeshNodeDef) !void {
        const owned_file = try self.allocator.dupe(u8, file);
        errdefer self.allocator.free(owned_file);
        const owned = try self.allocator.alloc(MeshNodeDef, nodes.len);
        var made: usize = 0;
        errdefer {
            for (owned[0..made]) |n| self.allocator.free(n.name);
            self.allocator.free(owned);
        }
        for (nodes, 0..) |n, i| {
            owned[i] = .{ .name = try self.allocator.dupe(u8, n.name), .locator = n.locator };
            made += 1;
        }
        try self.mesh_models.append(self.allocator, .{ .file = owned_file, .nodes = owned });
    }

    pub fn addBatchProject(self: *FakeResBridge, path: []const u8, kind: Kind) !void {
        const owned = try self.allocator.dupe(u8, path);
        errdefer self.allocator.free(owned);
        try self.batch_projects.append(self.allocator, .{ .path = owned, .kind = kind });
    }

    pub fn addGameFolder(self: *FakeResBridge, path: []const u8, key_name: []const u8) !void {
        const key = try self.allocator.dupe(u8, path);
        errdefer self.allocator.free(key);
        const value = try self.allocator.dupe(u8, key_name);
        errdefer self.allocator.free(value);
        try self.game_folders.put(self.allocator, key, value);
    }

    /// Preloads a reference list for a given type (0..19 is the real range).
    /// Owns its entries; subsequent calls replace the list.
    pub fn setReferenceList(self: *FakeResBridge, ref_type: i32, entries: []const ReferenceEntry) !void {
        const gop = try self.references.getOrPut(self.allocator, ref_type);
        if (gop.found_existing) gop.value_ptr.deinit(self.allocator);
        gop.value_ptr.* = .empty;
        try gop.value_ptr.appendSlice(self.allocator, entries);
    }

    /// Gives a property its strings (SProp::szStrings): the order is kept,
    /// each entry's token is its index. Replaces earlier strings.
    pub fn setPropStrings(self: *FakeResBridge, node: i32, prop_id: i32, strings: []const []const u8) !void {
        var entries: std.ArrayListUnmanaged(ReferenceEntry) = .empty;
        errdefer entries.deinit(self.allocator);
        for (strings, 0..) |text, i| {
            var entry: ReferenceEntry = .{ .token = @intCast(i) };
            if (!entry.setName(text)) return error.NameTooLong;
            try entries.append(self.allocator, entry);
        }
        for (self.prop_strings.items) |*p| if (p.node == node and p.prop_id == prop_id) {
            p.entries.deinit(self.allocator);
            p.entries = entries;
            return;
        };
        try self.prop_strings.append(self.allocator, .{ .node = node, .prop_id = prop_id, .entries = entries });
    }

    /// Declares that MFC keeps `channel` on `node` (a squad formation's
    /// slots and zero point, say). From the first declaration on, a channel
    /// read or write on any undeclared pair is refused, as the real bridge
    /// refuses a channel with no MFC home. `new` and `close` forget them.
    pub fn addGeometryHome(self: *FakeResBridge, node: i32, channel: GeometryChannel) !void {
        try self.geometry_homes.append(self.allocator, .{ .node = node, .channel = channel });
    }

    fn requireHome(self: *FakeResBridge, node: i32, channel: GeometryChannel) Status {
        if (self.geometry_homes.items.len == 0) return .ok;
        for (self.geometry_homes.items) |home| if (home.node == node and home.channel == channel) return .ok;
        self.say("this geometry channel has no MFC home on this node", .{});
        return .refused;
    }

    pub fn setNoDevice(self: *FakeResBridge, value: bool) void {
        self.no_device = value;
    }

    pub fn bridge(self: *FakeResBridge) ResBridge {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *FakeResBridge {
        return @ptrCast(@alignCast(ptr));
    }

    fn say(self: *FakeResBridge, comptime fmt: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, fmt, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }

    fn clearMessage(self: *FakeResBridge) void {
        self.message_len = 0;
    }

    fn indexOfNode(self: *const FakeResBridge, node_id: i32) ?usize {
        for (self.nodes.items, 0..) |n, i| if (n.id == node_id) return i;
        return null;
    }

    fn indexOfProp(self: *const FakeResBridge, node_index: usize, prop_id: i32) ?usize {
        const node = &self.nodes.items[node_index];
        for (node.props.items, 0..) |p, i| if (p.id == prop_id) return i;
        return null;
    }

    fn indexOfGeometry(self: *const FakeResBridge, node_id: i32, channel: GeometryChannel) ?usize {
        for (self.geometry.items, 0..) |g, i| if (g.node == node_id and g.channel == channel) return i;
        return null;
    }

    fn descendants(self: *FakeResBridge, root_id: i32, out: *std.ArrayListUnmanaged(i32)) !void {
        try out.append(self.allocator, root_id);
        var i: usize = 0;
        while (i < out.items.len) : (i += 1) {
            const parent_id = out.items[i];
            for (self.nodes.items) |node| {
                if (node.parent == parent_id) try out.append(self.allocator, node.id);
            }
        }
    }

    fn requireOpen(self: *FakeResBridge) Status {
        if (self.kind == null) {
            self.say("no project is open", .{});
            return .refused;
        }
        return .ok;
    }

    fn requireHasId(self: *FakeResBridge, node_id: i32) Status {
        if (self.indexOfNode(node_id) == null) {
            self.say("node id {d} is unknown", .{node_id});
            return .refused;
        }
        return .ok;
    }

    fn classIs(node: *const FakeNode, class_type: i32) bool {
        const value = std.fmt.parseInt(i32, std.mem.sliceTo(&node.class, 0), 10) catch return false;
        return value == class_type;
    }

    fn firstNodeOfClass(self: *const FakeResBridge, class_type: i32) ?usize {
        for (self.nodes.items, 0..) |*n, i| if (classIs(n, class_type)) return i;
        return null;
    }

    fn meshModelNamed(self: *const FakeResBridge, file: []const u8) ?*const MeshModel {
        for (self.mesh_models.items) |*m| if (std.mem.eql(u8, m.file, file)) return m;
        return null;
    }

    /// CMeshFrame::SetCombatMesh's rebuild: the Locators children are made
    /// again from the skeleton of the Graphics combat model. A name no
    /// declared model matches leaves the item empty, as a missing .mod does.
    fn rebuildMeshLocators(self: *FakeResBridge, model_name: []const u8) !void {
        const locators_index = self.firstNodeOfClass(item_type.mesh_locators) orelse return;
        const locators_id = self.nodes.items[locators_index].id;
        var i: usize = self.nodes.items.len;
        while (i > 0) {
            i -= 1;
            if (self.nodes.items[i].parent == locators_id) {
                self.nodes.items[i].deinit(self.allocator);
                _ = self.nodes.orderedRemove(i);
            }
        }
        const model = self.meshModelNamed(model_name) orelse return;
        for (model.nodes) |def| {
            var child: FakeNode = .{ .id = self.next_id, .parent = locators_id };
            _ = std.fmt.bufPrint(&child.class, "{d}", .{item_type.mesh_locator_props}) catch unreachable;
            _ = putName(&child.display, def.name);
            try self.nodes.append(self.allocator, child);
            self.next_id += 1;
        }
    }

    /// The locator combos of the unit editor (LoadGunPointPropsComboBox and
    /// the three after it), from the Locators children and the skeleton's
    /// locator flags. Null when the pair is no locator combo.
    fn meshComboNames(self: *FakeResBridge, node_idx: usize, prop_id: i32, out: *std.ArrayListUnmanaged([]const u8)) !bool {
        const is_platform = classIs(&self.nodes.items[node_idx], item_type.mesh_platform_props);
        const is_gun = classIs(&self.nodes.items[node_idx], item_type.mesh_gun_props);
        const kind: enum { part, carriage, point, shoot_part } = if (is_platform and prop_id == 1)
            .part
        else if (is_platform and (prop_id == 2 or prop_id == 3))
            .carriage
        else if (is_gun and prop_id == 1)
            .point
        else if (is_gun and prop_id == 2)
            .shoot_part
        else
            return false;
        const locators_index = self.firstNodeOfClass(item_type.mesh_locators);
        if (locators_index) |li| {
            const locators_id = self.nodes.items[li].id;
            const model_name = self.currentMeshModelName();
            for (self.nodes.items) |*n| {
                if (n.parent != locators_id) continue;
                const name = std.mem.sliceTo(&n.display, 0);
                const flagged = if (self.meshModelNamed(model_name)) |m| for (m.nodes) |def| {
                    if (std.mem.eql(u8, def.name, name)) break def.locator;
                } else false else false;
                const carriage = std.mem.startsWith(u8, name, "GunCarriage");
                const take = switch (kind) {
                    .part => !flagged,
                    .carriage => carriage,
                    .point => flagged and (std.mem.startsWith(u8, name, "LMainGun") or std.mem.startsWith(u8, name, "LMachineGun")),
                    .shoot_part => !flagged and name.len > 0 and name[0] != 'L' and !carriage,
                };
                if (take) try out.append(self.allocator, name);
            }
        }
        try out.append(self.allocator, "NA");
        return true;
    }

    /// The Graphics item's combat model name (its first property).
    fn currentMeshModelName(self: *const FakeResBridge) []const u8 {
        const gi = self.firstNodeOfClass(item_type.mesh_graphics) orelse return "";
        for (self.nodes.items[gi].props.items) |*p| if (p.id == 1) return std.mem.sliceTo(&p.value_text, 0);
        return "";
    }

    /// --- vtable ---------------------------------------------------------

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        return self.message_buffer[0..self.message_len];
    }

    fn new(ptr: *anyopaque, kind: Kind) Status {
        const self = from(ptr);
        self.clearMessage();
        for (self.nodes.items) |*n| n.deinit(self.allocator);
        self.nodes.clearRetainingCapacity();
        for (self.geometry.items) |*g| g.value.deinit(self.allocator);
        self.geometry.clearRetainingCapacity();
        self.geometry_homes.clearRetainingCapacity();
        self.kind = kind;
        self.has_path = false;
        var root: FakeNode = .{ .id = self.next_id, .parent = -1 };
        _ = putName(&root.class, "Root");
        _ = putName(&root.display, "Root");
        self.nodes.append(self.allocator, root) catch return .failed;
        self.next_id += 1;
        return .ok;
    }

    fn open(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        self.clearMessage();
        if (self.files.get(path)) |blob| {
            if (!deserialiseInto(self, blob)) {
                self.say("could not parse {s}", .{path});
                return .data_missing;
            }
            self.has_path = true;
            return .ok;
        }
        self.say("file not found: {s}", .{path});
        return .data_missing;
    }

    fn save(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        const blob = serialiseAll(self) catch return .failed;
        errdefer self.allocator.free(blob);
        if (self.files.fetchRemove(path)) |old| {
            self.allocator.free(old.key);
            self.allocator.free(old.value);
        }
        const key = self.allocator.dupe(u8, path) catch {
            self.allocator.free(blob);
            return .failed;
        };
        self.files.put(self.allocator, key, blob) catch {
            self.allocator.free(key);
            self.allocator.free(blob);
            return .failed;
        };
        self.has_path = true;
        return .ok;
    }

    fn close(ptr: *anyopaque) Status {
        const self = from(ptr);
        self.clearMessage();
        for (self.nodes.items) |*n| n.deinit(self.allocator);
        self.nodes.clearRetainingCapacity();
        for (self.geometry.items) |*g| g.value.deinit(self.allocator);
        self.geometry.clearRetainingCapacity();
        self.geometry_homes.clearRetainingCapacity();
        self.kind = null;
        self.has_path = false;
        return .ok;
    }

    fn kindOf(ptr: *anyopaque, out: *Kind) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        out.* = self.kind.?;
        return .ok;
    }

    fn lock(ptr: *anyopaque, owner: []const u8) Status {
        const self = from(ptr);
        self.clearMessage();
        if (self.lock_owner) |current| {
            self.say("already locked by {s}", .{current});
            return .refused;
        }
        self.lock_owner = self.allocator.dupe(u8, owner) catch return .failed;
        return .ok;
    }

    fn lockOwner(ptr: *anyopaque, out: []u8, out_len: *usize) Status {
        const self = from(ptr);
        self.clearMessage();
        const owner = self.lock_owner orelse {
            out_len.* = 0;
            return .ok;
        };
        out_len.* = owner.len;
        if (out.len < owner.len) return .refused;
        @memcpy(out[0..owner.len], owner);
        return .ok;
    }

    fn nodes_fn(ptr: *anyopaque, out: []NodeRecord, total: *usize) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        total.* = self.nodes.items.len;
        if (out.len < total.*) return .refused;
        for (self.nodes.items, 0..) |node, i| {
            var r: NodeRecord = .{
                .id = node.id,
                .parent = node.parent,
                .class_name = node.class,
                .display_name = node.display,
                .expand = node.expand,
                .child_count = 0,
            };
            for (self.nodes.items) |peer| {
                if (peer.parent == node.id) r.child_count += 1;
            }
            out[i] = r;
        }
        return .ok;
    }

    fn props_fn(ptr: *anyopaque, node: i32, out: []PropRecord, total: *usize) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        const idx = self.indexOfNode(node) orelse {
            self.say("node id {d} is unknown", .{node});
            return .refused;
        };
        const list = &self.nodes.items[idx].props;
        total.* = list.items.len;
        if (out.len < total.*) return .refused;
        for (list.items, 0..) |p, i| out[i] = p;
        return .ok;
    }

    fn setProp(ptr: *anyopaque, node: i32, prop_id: i32, value_text: []const u8) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        const node_idx = self.indexOfNode(node) orelse {
            self.say("node id {d} is unknown", .{node});
            return .refused;
        };
        const prop_idx = self.indexOfProp(node_idx, prop_id) orelse {
            self.say("prop id {d} is unknown on node {d}", .{ prop_id, node });
            return .refused;
        };
        var prop = &self.nodes.items[node_idx].props.items[prop_idx];
        if (value_text.len >= value_text_capacity) {
            self.say("value too long for prop {d}", .{prop_id});
            return .bad_argument;
        }
        @memset(&prop.value_text, 0);
        @memcpy(prop.value_text[0..value_text.len], value_text);
        if (prop_id == 1 and classIs(&self.nodes.items[node_idx], item_type.mesh_graphics)) {
            self.rebuildMeshLocators(value_text) catch return .failed;
        }
        return .ok;
    }

    fn insertNode(ptr: *anyopaque, parent: i32, class_name: []const u8, index: i32, out_id: *i32) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        if (parent != -1 and self.indexOfNode(parent) == null) {
            self.say("parent {d} is unknown", .{parent});
            return .refused;
        }
        if (class_name.len == 0 or class_name.len >= name_capacity) {
            self.say("class name is empty or too long", .{});
            return .bad_argument;
        }
        var siblings: i32 = 0;
        for (self.nodes.items) |n| if (n.parent == parent) {
            siblings += 1;
        };
        if (index < 0 or index > siblings) {
            self.say("insert index {d} is out of range (0..{d})", .{ index, siblings });
            return .refused;
        }
        var fresh: FakeNode = .{ .id = self.next_id, .parent = parent };
        _ = putName(&fresh.class, class_name);
        _ = putName(&fresh.display, class_name);
        seedBuildingPointProps(&fresh, self.allocator) catch return .failed;
        self.nodes.append(self.allocator, fresh) catch return .failed;
        out_id.* = self.next_id;
        self.next_id += 1;
        return .ok;
    }

    fn deleteNode(ptr: *anyopaque, node: i32, out_blob: []u8, out_size: *usize) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        const root = self.nodes.items[0].id;
        if (node == root) {
            self.say("cannot delete the root", .{});
            return .refused;
        }
        if (self.indexOfNode(node) == null) {
            self.say("node id {d} is unknown", .{node});
            return .refused;
        }
        var ids: std.ArrayListUnmanaged(i32) = .empty;
        defer ids.deinit(self.allocator);
        self.descendants(node, &ids) catch return .failed;
        const blob = serialiseSubtree(self, ids.items) catch return .failed;
        defer self.allocator.free(blob);
        out_size.* = blob.len;
        if (out_blob.len < blob.len) return .refused;
        @memcpy(out_blob[0..blob.len], blob);
        for (ids.items) |id| {
            const i = self.indexOfNode(id).?;
            var removed = self.nodes.orderedRemove(i);
            removed.deinit(self.allocator);
            var g: usize = 0;
            while (g < self.geometry.items.len) {
                if (self.geometry.items[g].node == id) {
                    var entry = self.geometry.orderedRemove(g);
                    entry.value.deinit(self.allocator);
                } else g += 1;
            }
        }
        return .ok;
    }

    fn restoreNode(ptr: *anyopaque, blob: []const u8, parent: i32, index: i32, out_id: *i32) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        if (parent != -1 and self.indexOfNode(parent) == null) {
            self.say("parent {d} is unknown", .{parent});
            return .refused;
        }
        // The subtree goes back in front of the parent's index-th child, so
        // the children read back in the order they had before the delete.
        var at: usize = self.nodes.items.len;
        var sibling: i32 = 0;
        for (self.nodes.items, 0..) |n, i| if (n.parent == parent) {
            if (sibling == index) {
                at = i;
                break;
            }
            sibling += 1;
        };
        const restored = parseSubtree(self.allocator, blob) catch |err| switch (err) {
            error.BadBlob => {
                self.say("could not parse blob", .{});
                return .failed;
            },
            else => return .failed,
        };
        defer self.allocator.free(restored);
        var remap = std.AutoHashMapUnmanaged(i32, i32).empty;
        defer remap.deinit(self.allocator);
        if (restored.len == 0) {
            self.say("empty blob", .{});
            return .failed;
        }
        for (restored) |node_in| {
            var node = node_in;
            const old = node.id;
            if (self.indexOfNode(old) != null) {
                node.id = self.next_id;
                self.next_id += 1;
            } else {
                self.next_id = @max(self.next_id, node.id + 1);
            }
            remap.put(self.allocator, old, node.id) catch return .failed;
            self.nodes.append(self.allocator, node) catch return .failed;
        }
        for (self.nodes.items[self.nodes.items.len - restored.len ..]) |*node| {
            if (remap.get(node.parent)) |remapped| node.parent = remapped;
        }
        self.nodes.items[self.nodes.items.len - restored.len].parent = parent;
        const moved = self.nodes.items[at..];
        std.mem.rotate(FakeNode, moved, moved.len - restored.len);
        out_id.* = self.nodes.items[at].id;
        return .ok;
    }

    fn moveNode(ptr: *anyopaque, node: i32, new_parent: i32, new_index: i32) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        const idx = self.indexOfNode(node) orelse {
            self.say("node {d} is unknown", .{node});
            return .refused;
        };
        if (new_parent != -1 and self.indexOfNode(new_parent) == null) {
            self.say("parent {d} is unknown", .{new_parent});
            return .refused;
        }
        // Cycle refusal: new_parent must not be in the subtree rooted at node.
        var ids: std.ArrayListUnmanaged(i32) = .empty;
        defer ids.deinit(self.allocator);
        self.descendants(node, &ids) catch return .failed;
        for (ids.items) |id| if (id == new_parent) {
            self.say("move would create a cycle", .{});
            return .refused;
        };
        var siblings: i32 = 0;
        for (self.nodes.items) |n| if (n.parent == new_parent and n.id != node) {
            siblings += 1;
        };
        if (new_index < 0 or new_index > siblings) {
            self.say("move index {d} out of range (0..{d})", .{ new_index, siblings });
            return .refused;
        }
        self.nodes.items[idx].parent = new_parent;
        return .ok;
    }

    fn setNodeName(ptr: *anyopaque, node: i32, name: []const u8) Status {
        const self = from(ptr);
        self.clearMessage();
        if (name.len == 0 or name.len >= name_capacity) {
            self.say("a node name must be 1 to {d} characters", .{name_capacity - 1});
            return .bad_argument;
        }
        const check = self.requireOpen();
        if (check != .ok) return check;
        const idx = self.indexOfNode(node) orelse {
            self.say("node {d} is unknown", .{node});
            return .refused;
        };
        _ = putName(&self.nodes.items[idx].display, name);
        return .ok;
    }

    fn setNodeExpand(ptr: *anyopaque, node: i32, expand: bool) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        const idx = self.indexOfNode(node) orelse {
            self.say("node {d} is unknown", .{node});
            return .refused;
        };
        self.nodes.items[idx].expand = expand;
        return .ok;
    }

    fn propStrings(ptr: *anyopaque, node: i32, prop_id: i32, out: []ReferenceEntry, total: *usize) Status {
        const self = from(ptr);
        self.clearMessage();
        total.* = 0;
        const check = self.requireOpen();
        if (check != .ok) return check;
        const node_idx = self.indexOfNode(node) orelse {
            self.say("node {d} is unknown", .{node});
            return .refused;
        };
        if (self.indexOfProp(node_idx, prop_id) == null) {
            self.say("prop id {d} is unknown on node {d}", .{ prop_id, node });
            return .refused;
        }
        var combo: std.ArrayListUnmanaged([]const u8) = .empty;
        defer combo.deinit(self.allocator);
        const is_combo = self.meshComboNames(node_idx, prop_id, &combo) catch return .failed;
        if (is_combo) {
            total.* = combo.items.len;
            if (out.len < total.*) return .refused;
            for (combo.items, out[0..total.*], 0..) |text, *dst, i| {
                dst.* = .{ .token = @intCast(i) };
                _ = dst.setName(text);
            }
            return .ok;
        }
        for (self.prop_strings.items) |p| if (p.node == node and p.prop_id == prop_id) {
            total.* = p.entries.items.len;
            if (out.len < total.*) return .refused;
            @memcpy(out[0..total.*], p.entries.items);
            return .ok;
        };
        return .ok;
    }

    fn refList(ptr: *anyopaque, ref_type: i32, out: []ReferenceEntry, total: *usize) Status {
        const self = from(ptr);
        self.clearMessage();
        if (ref_type < 0 or ref_type > 19) {
            self.say("reference type {d} is out of range", .{ref_type});
            return .bad_argument;
        }
        const list = self.references.get(ref_type) orelse {
            total.* = 0;
            return .ok;
        };
        total.* = list.items.len;
        if (out.len < total.*) return .refused;
        for (list.items, 0..) |entry, i| out[i] = entry;
        return .ok;
    }

    fn geometryRead(ptr: *anyopaque, node: i32, channel: GeometryChannel, out: *GeometryValue) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        if (self.indexOfNode(node) == null) {
            self.say("node {d} is unknown", .{node});
            return .refused;
        }
        const home = self.requireHome(node, channel);
        if (home != .ok) return home;
        if (self.indexOfGeometry(node, channel)) |i| {
            out.* = self.geometry.items[i].value.dupe(self.allocator) catch return .failed;
        } else {
            out.* = emptyFor(channel);
        }
        return .ok;
    }

    fn geometryWrite(ptr: *anyopaque, node: i32, channel: GeometryChannel, value: *const GeometryValue) Status {
        const self = from(ptr);
        self.clearMessage();
        const check = self.requireOpen();
        if (check != .ok) return check;
        if (self.indexOfNode(node) == null) {
            self.say("node {d} is unknown", .{node});
            return .refused;
        }
        if (std.meta.activeTag(value.*) != channel.family()) {
            self.say("channel {s} does not carry a {s} payload", .{ @tagName(channel), @tagName(std.meta.activeTag(value.*)) });
            return .bad_argument;
        }
        const home = self.requireHome(node, channel);
        if (home != .ok) return home;
        const duplicated = value.dupe(self.allocator) catch return .failed;
        if (self.kind == .build) self.syncBuildingPointChildren(channel, duplicated);
        if (self.indexOfGeometry(node, channel)) |i| {
            var existing = &self.geometry.items[i];
            existing.value.deinit(self.allocator);
            existing.value = duplicated;
        } else {
            self.geometry.append(self.allocator, .{ .node = node, .channel = channel, .value = duplicated }) catch {
                var dup_mut = duplicated;
                dup_mut.deinit(self.allocator);
                return .failed;
            };
        }
        return .ok;
    }

    /// The real bridge's SyncAimedChildren: a building keeps each aimed point
    /// twice, in the list and on a tree child, and a write of the list copies
    /// point i's angle and cone to child i (children beyond the list keep what
    /// they hold). Inserting and removing the child is the editor's own step.
    fn syncBuildingPointChildren(self: *FakeResBridge, channel: GeometryChannel, value: GeometryValue) void {
        const container_class: i32 = switch (channel) {
            .shoot_points => item_type.building_slots,
            .fire_points => item_type.building_fire_points,
            .smoke_points => item_type.building_smokes,
            .directed_explosion_points => item_type.building_dir_explosions,
            else => return,
        };
        const container_index = self.firstNodeOfClass(container_class) orelse return;
        const container_id = self.nodes.items[container_index].id;
        var i: usize = 0;
        for (self.nodes.items) |*child| {
            if (child.parent != container_id) continue;
            if (i >= value.aimed.len) break;
            for (child.props.items) |*prop| {
                const name = prop.defaultSlice();
                const number: i32 = if (std.mem.eql(u8, name, "Direction")) value.aimed[i].angle else if (std.mem.eql(u8, name, "Angle") or std.mem.eql(u8, name, "Vertical angle")) value.aimed[i].cone else continue;
                @memset(&prop.value_text, 0);
                _ = std.fmt.bufPrint(&prop.value_text, "{d}", .{number}) catch unreachable;
            }
            i += 1;
        }
    }

    fn isShipped(self: *const FakeResBridge, dir: []const u8) bool {
        const trimmed = std.mem.trimEnd(u8, dir, "/");
        if (std.ascii.eqlIgnoreCase(trimmed, self.shipped_root)) return true;
        if (trimmed.len < 5 or !std.ascii.eqlIgnoreCase(trimmed[trimmed.len - 5 ..], "/data")) return false;
        return std.ascii.eqlIgnoreCase(trimmed[0 .. trimmed.len - 5], self.shipped_root);
    }

    fn notPorted(self: *FakeResBridge, kind: Kind) void {
        self.say("exporting .{s} projects is not ported yet; the exporter comes with its sub-editor", .{kind.extension()});
    }

    fn putFile(self: *FakeResBridge, path: []const u8, bytes: []const u8) Status {
        const blob = self.allocator.dupe(u8, bytes) catch return .failed;
        if (self.files.fetchRemove(path)) |old| {
            self.allocator.free(old.key);
            self.allocator.free(old.value);
        }
        const key = self.allocator.dupe(u8, path) catch {
            self.allocator.free(blob);
            return .failed;
        };
        self.files.put(self.allocator, key, blob) catch {
            self.allocator.free(key);
            self.allocator.free(blob);
            return .failed;
        };
        return .ok;
    }

    fn exportProject(ptr: *anyopaque, flags: ExportFlags, stats_only: bool, report: *ExportReport, warnings: []Warning) Status {
        const self = from(ptr);
        self.clearMessage();
        _ = warnings;
        report.* = .{};
        const check = self.requireOpen();
        if (check != .ok) return check;
        if (!self.has_path) {
            self.say("save the project first: an export reads its sources beside the project file", .{});
            return .refused;
        }
        if (!self.exportable.contains(self.kind.?)) {
            self.notPorted(self.kind.?);
            return .refused;
        }
        self.exports += 1;
        self.last_flags = flags;
        self.last_stats_only = stats_only;
        report.written = 1;
        return .ok;
    }

    fn batch(ptr: *anyopaque, kind: ?Kind, src: []const u8, dst: []const u8, flags: ExportFlags, report: *ExportReport, warnings: []Warning) Status {
        const self = from(ptr);
        self.clearMessage();
        report.* = .{};
        if (src.len == 0 or dst.len == 0) return .bad_argument;
        if (!flags.open_save and self.isShipped(dst)) {
            self.say("the batch destination is the shipped Data folder", .{});
            return .refused;
        }
        var any_below = false;
        for (self.batch_projects.items) |project| {
            if (!std.mem.startsWith(u8, project.path, src)) continue;
            any_below = true;
            if (kind != null and kind.? != project.kind) continue;
            if (flags.open_save or self.exportable.contains(project.kind)) {
                report.written += 1;
                continue;
            }
            report.skipped += 1;
            if (report.warning_total < warnings.len) {
                var line: [bridge_mod.warning_text_capacity]u8 = undefined;
                const text = std.fmt.bufPrint(&line, "{s}: exporting .{s} projects is not ported yet", .{ project.path, project.kind.extension() }) catch line[0..];
                warnings[report.warning_total].setText(text);
            }
            report.warning_total += 1;
        }
        if (!any_below) {
            self.say("{s} is not a folder", .{src});
            return .data_missing;
        }
        return .ok;
    }

    fn modSettingsGet(ptr: *anyopaque, out: *ModSettings) Status {
        const self = from(ptr);
        self.clearMessage();
        out.* = self.mod;
        return .ok;
    }

    fn modSettingsSet(ptr: *anyopaque, in: *const ModSettings) Status {
        const self = from(ptr);
        self.clearMessage();
        const dir = in.exportDirSlice();
        if (dir.len == 0) return .bad_argument;
        if (self.isShipped(dir)) {
            self.say("the shipped Data folder is not a mod folder", .{});
            return .refused;
        }
        self.mod = in.*;
        self.mod_set = true;
        return .ok;
    }

    fn packMod(ptr: *anyopaque, out_path: []const u8) Status {
        const self = from(ptr);
        self.clearMessage();
        if (out_path.len == 0) return .bad_argument;
        if (!self.mod_set) {
            self.say("there is no data folder to pack", .{});
            return .refused;
        }
        const dir = std.mem.trimEnd(u8, self.mod.exportDirSlice(), "/");
        if (std.mem.startsWith(u8, out_path, dir) and out_path.len > dir.len + 6 and
            std.ascii.eqlIgnoreCase(out_path[dir.len .. dir.len + 6], "/data/"))
        {
            self.say("the archive would be inside the folder it packs", .{});
            return .refused;
        }
        return self.putFile(out_path, "PK\x05\x06");
    }

    fn importFromGame(ptr: *anyopaque, kind: Kind, path: []const u8) Status {
        const self = from(ptr);
        self.clearMessage();
        if (path.len == 0) return .bad_argument;
        if (kind == .sprite) {
            self.say("importing .spt is refused: MFC's sprite export only composes .san packs and has no reverse path", .{});
            return .refused;
        }
        if (kind != .animation_infantry) {
            self.say("importing .{s} is not ported yet; it comes with its sub-editor", .{kind.extension()});
            return .refused;
        }
        const key_name = self.game_folders.get(path) orelse {
            self.say("no 1.xml in {s}", .{path});
            return .data_missing;
        };
        const status = new(ptr, kind);
        if (status != .ok) return status;
        var info: FakeNode = .{ .id = self.next_id, .parent = self.nodes.items[0].id };
        _ = putName(&info.class, "UnitCommonProps");
        _ = putName(&info.display, "Basic Info");
        var name: PropRecord = .{ .id = 1 };
        _ = name.setDefault("Name");
        _ = name.setDisplay("Name");
        if (!name.setValue(key_name)) return .failed;
        info.props.append(self.allocator, name) catch return .failed;
        self.nodes.append(self.allocator, info) catch {
            info.deinit(self.allocator);
            return .failed;
        };
        self.next_id += 1;
        return .ok;
    }

    fn previewBegin(ptr: *anyopaque, kind: Kind) Status {
        const self = from(ptr);
        self.clearMessage();
        _ = kind;
        if (self.no_device) {
            self.say("no GPU device", .{});
            return .no_device;
        }
        self.preview_state = .open;
        return .ok;
    }

    fn previewShow(ptr: *anyopaque) Status {
        const self = from(ptr);
        self.clearMessage();
        if (self.preview_state == .closed) {
            self.say("preview not open", .{});
            return .refused;
        }
        if (self.requireOpen() != .ok) return .refused;
        self.preview_state = .showing;
        return .ok;
    }

    fn previewStop(ptr: *anyopaque) Status {
        const self = from(ptr);
        self.clearMessage();
        self.preview_state = .closed;
        self.preview_playing = false;
        self.mesh_variant = 0;
        self.mesh_direction = 0;
        self.show_locators = false;
        self.show_bounding_boxes = false;
        return .ok;
    }

    fn previewPlayback(ptr: *anyopaque, run: bool) Status {
        const self = from(ptr);
        self.clearMessage();
        if (self.preview_state != .showing) {
            self.say("the preview is not showing", .{});
            return .refused;
        }
        self.preview_playing = run;
        return .ok;
    }

    fn requireUnitShowing(self: *FakeResBridge) Status {
        if (self.preview_state != .showing or self.kind != .mesh_unit) {
            self.say("the preview shows no unit", .{});
            return .refused;
        }
        return .ok;
    }

    fn previewMeshVariant(ptr: *anyopaque, variant: u8) Status {
        const self = from(ptr);
        self.clearMessage();
        const shown = self.requireUnitShowing();
        if (shown != .ok) return shown;
        if (variant > 2) {
            self.say("model variant {d} is outside 0..2", .{variant});
            return .refused;
        }
        self.mesh_variant = variant;
        return .ok;
    }

    fn previewDirection(ptr: *anyopaque, angle: i32) Status {
        const self = from(ptr);
        self.clearMessage();
        const shown = self.requireUnitShowing();
        if (shown != .ok) return shown;
        self.mesh_direction = @mod(angle, 360);
        return .ok;
    }

    fn previewShowLocators(ptr: *anyopaque, locators: bool, bounding_boxes: bool) Status {
        const self = from(ptr);
        self.clearMessage();
        const shown = self.requireUnitShowing();
        if (shown != .ok) return shown;
        self.show_locators = locators;
        self.show_bounding_boxes = bounding_boxes;
        return .ok;
    }

    fn meshLocators(ptr: *anyopaque, out: []MeshLocator, total: *usize) Status {
        const self = from(ptr);
        self.clearMessage();
        total.* = 0;
        const shown = self.requireUnitShowing();
        if (shown != .ok) return shown;
        const li = self.firstNodeOfClass(item_type.mesh_locators) orelse return .ok;
        const locators_id = self.nodes.items[li].id;
        var count: usize = 0;
        for (self.nodes.items) |*n| {
            if (n.parent != locators_id) continue;
            if (count < out.len) {
                const x: f32 = @floatFromInt(count);
                out[count] = .{ .node_id = @intCast(count), .wx = x * 10, .wy = 0, .wz = 0, .sx = 100 + x * 20, .sy = 200 };
                _ = putName(&out[count].name, std.mem.sliceTo(&n.display, 0));
            }
            count += 1;
        }
        total.* = count;
        if (out.len < count) return .refused;
        return .ok;
    }

    const vtable: ResBridge.VTable = .{
        .lastMessage = lastMessage,
        .new = new,
        .open = open,
        .save = save,
        .close = close,
        .kindOf = kindOf,
        .lock = lock,
        .lockOwner = lockOwner,
        .nodes = nodes_fn,
        .props = props_fn,
        .setProp = setProp,
        .insertNode = insertNode,
        .deleteNode = deleteNode,
        .restoreNode = restoreNode,
        .moveNode = moveNode,
        .setNodeName = setNodeName,
        .setNodeExpand = setNodeExpand,
        .propStrings = propStrings,
        .refList = refList,
        .geometryRead = geometryRead,
        .geometryWrite = geometryWrite,
        .exportProject = exportProject,
        .batch = batch,
        .modSettingsGet = modSettingsGet,
        .modSettingsSet = modSettingsSet,
        .packMod = packMod,
        .importFromGame = importFromGame,
        .previewBegin = previewBegin,
        .previewShow = previewShow,
        .previewStop = previewStop,
        .previewPlayback = previewPlayback,
        .previewMeshVariant = previewMeshVariant,
        .previewDirection = previewDirection,
        .previewShowLocators = previewShowLocators,
        .meshLocators = meshLocators,
    };
};

/// A building point item (slot, fire, smoke or directed explosion) is born
/// with the two properties the editor writes for each point: its direction
/// and its cone ("Angle" on a slot, "Vertical angle" on the others).
fn seedBuildingPointProps(node: *FakeNode, allocator: std.mem.Allocator) !void {
    const cone_name: []const u8 = if (FakeResBridge.classIs(node, item_type.building_slot_props))
        "Angle"
    else if (FakeResBridge.classIs(node, item_type.building_fire_point_props) or FakeResBridge.classIs(node, item_type.building_smoke_props) or FakeResBridge.classIs(node, item_type.building_dir_explosion_props))
        "Vertical angle"
    else
        return;
    const names = [_][]const u8{ "Direction", cone_name };
    for (names, 0..) |name, n| {
        var prop: PropRecord = .{ .id = @intCast(n + 1), .domain_type = 0, .value_kind = 0 };
        _ = prop.setDefault(name);
        _ = prop.setDisplay(name);
        _ = prop.setValue("0");
        try node.props.append(allocator, prop);
    }
}

fn emptyFor(channel: GeometryChannel) GeometryValue {
    return switch (channel.family()) {
        .bytes_grid => .{ .bytes_grid = .{ .bytes = &.{}, .width = 0, .height = 0 } },
        .points2 => .{ .points2 = &.{} },
        .point2 => .{ .point2 = .{} },
        .aimed => .{ .aimed = &.{} },
        .vec3 => .{ .vec3 = &.{} },
    };
}

/// A tiny self-describing serialiser: `[count:u32]` followed by one record
/// per node: `[id:i32][parent:i32][class:64][display:64][expand:u8]
/// [prop_count:u32]` followed by one PropRecord per prop. Enough to let
/// delete -> restore round-trip - a real XML serialiser is the C adapter's
/// problem. A whole-project save adds the geometry map after the nodes:
/// `[count:u32]` then per entry `[node:i32][channel:i32][family:u8]` and the
/// payload (a grid's `[w:i32][h:i32][len:u32]` and bytes, a list's
/// `[count:u32]` and its records, a single Point2's 8 bytes).
fn serialiseAll(self: *FakeResBridge) ![]u8 {
    var out: std.ArrayListUnmanaged(u8) = .empty;
    errdefer out.deinit(self.allocator);
    try writeU32(self.allocator, &out, @intCast(self.nodes.items.len));
    for (self.nodes.items) |node| try writeNode(self.allocator, &out, &node);
    try writeU32(self.allocator, &out, @intCast(self.geometry.items.len));
    for (self.geometry.items) |entry| try writeGeometry(self.allocator, &out, entry);
    return out.toOwnedSlice(self.allocator);
}

fn writeGeometry(allocator: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), entry: GeometryEntry) !void {
    try writeI32(allocator, out, entry.node);
    try writeI32(allocator, out, @intFromEnum(entry.channel));
    try out.append(allocator, @intFromEnum(std.meta.activeTag(entry.value)));
    switch (entry.value) {
        .bytes_grid => |g| {
            try writeI32(allocator, out, g.width);
            try writeI32(allocator, out, g.height);
            try writeU32(allocator, out, @intCast(g.bytes.len));
            try out.appendSlice(allocator, g.bytes);
        },
        .points2 => |list| {
            try writeU32(allocator, out, @intCast(list.len));
            try out.appendSlice(allocator, std.mem.sliceAsBytes(list));
        },
        .aimed => |list| {
            try writeU32(allocator, out, @intCast(list.len));
            try out.appendSlice(allocator, std.mem.sliceAsBytes(list));
        },
        .vec3 => |list| {
            try writeU32(allocator, out, @intCast(list.len));
            try out.appendSlice(allocator, std.mem.sliceAsBytes(list));
        },
        .point2 => |p| {
            var copy = p;
            try out.appendSlice(allocator, std.mem.asBytes(&copy));
        },
    }
}

/// Reads one list of extern records written by `writeGeometry`.
fn readRecords(comptime T: type, allocator: std.mem.Allocator, blob: []const u8, cursor: *usize) ![]T {
    const count = try readU32(blob, cursor);
    const size = @as(usize, count) * @sizeOf(T);
    if (cursor.* + size > blob.len) return error.BadBlob;
    const list = try allocator.alloc(T, count);
    @memcpy(std.mem.sliceAsBytes(list), blob[cursor.*..][0..size]);
    cursor.* += size;
    return list;
}

fn readGeometry(allocator: std.mem.Allocator, blob: []const u8, cursor: *usize) !GeometryEntry {
    const node = try readI32(blob, cursor);
    const channel_int = try readI32(blob, cursor);
    const channel = std.enums.fromInt(GeometryChannel, channel_int) orelse return error.BadBlob;
    if (cursor.* >= blob.len) return error.BadBlob;
    const family = std.enums.fromInt(std.meta.Tag(GeometryValue), blob[cursor.*]) orelse return error.BadBlob;
    cursor.* += 1;
    const value: GeometryValue = switch (family) {
        .bytes_grid => blk: {
            const width = try readI32(blob, cursor);
            const height = try readI32(blob, cursor);
            break :blk .{ .bytes_grid = .{ .bytes = try readRecords(u8, allocator, blob, cursor), .width = width, .height = height } };
        },
        .points2 => .{ .points2 = try readRecords(bridge_mod.Point2, allocator, blob, cursor) },
        .aimed => .{ .aimed = try readRecords(bridge_mod.AimedPoint, allocator, blob, cursor) },
        .vec3 => .{ .vec3 = try readRecords(bridge_mod.Vec3, allocator, blob, cursor) },
        .point2 => blk: {
            var p: bridge_mod.Point2 = .{};
            if (cursor.* + @sizeOf(bridge_mod.Point2) > blob.len) return error.BadBlob;
            @memcpy(std.mem.asBytes(&p), blob[cursor.*..][0..@sizeOf(bridge_mod.Point2)]);
            cursor.* += @sizeOf(bridge_mod.Point2);
            break :blk .{ .point2 = p };
        },
    };
    return .{ .node = node, .channel = channel, .value = value };
}

fn serialiseSubtree(self: *FakeResBridge, ids: []const i32) ![]u8 {
    var out: std.ArrayListUnmanaged(u8) = .empty;
    errdefer out.deinit(self.allocator);
    try writeU32(self.allocator, &out, @intCast(ids.len));
    for (ids) |id| {
        const i = self.indexOfNode(id).?;
        try writeNode(self.allocator, &out, &self.nodes.items[i]);
    }
    return out.toOwnedSlice(self.allocator);
}

fn deserialiseInto(self: *FakeResBridge, blob: []const u8) bool {
    for (self.nodes.items) |*n| n.deinit(self.allocator);
    self.nodes.clearRetainingCapacity();
    var cursor: usize = 0;
    const count = readU32(blob, &cursor) catch return false;
    var i: u32 = 0;
    while (i < count) : (i += 1) {
        var node: FakeNode = .{ .id = 0, .parent = -1 };
        if (!readNode(self.allocator, blob, &cursor, &node)) {
            node.deinit(self.allocator);
            return false;
        }
        self.nodes.append(self.allocator, node) catch return false;
        self.next_id = @max(self.next_id, node.id + 1);
    }
    // The geometry map is the file's, not what the session last held. A
    // file saved before the fake carried geometry simply ends here.
    for (self.geometry.items) |*g| g.value.deinit(self.allocator);
    self.geometry.clearRetainingCapacity();
    if (cursor < blob.len) {
        const geometry_count = readU32(blob, &cursor) catch return false;
        var g: u32 = 0;
        while (g < geometry_count) : (g += 1) {
            var entry = readGeometry(self.allocator, blob, &cursor) catch return false;
            self.geometry.append(self.allocator, entry) catch {
                entry.value.deinit(self.allocator);
                return false;
            };
        }
    }
    if (self.kind == null) self.kind = .weapon;
    return true;
}

fn parseSubtree(allocator: std.mem.Allocator, blob: []const u8) ![]FakeNode {
    var cursor: usize = 0;
    const count = readU32(blob, &cursor) catch return error.BadBlob;
    var out = try allocator.alloc(FakeNode, count);
    var filled: usize = 0;
    errdefer {
        var i: usize = 0;
        while (i < filled) : (i += 1) out[i].deinit(allocator);
        allocator.free(out);
    }
    var i: u32 = 0;
    while (i < count) : (i += 1) {
        var node: FakeNode = .{ .id = 0, .parent = -1 };
        if (!readNode(allocator, blob, &cursor, &node)) {
            node.deinit(allocator);
            return error.BadBlob;
        }
        out[i] = node;
        filled = i + 1;
    }
    return out;
}

fn writeU32(allocator: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), value: u32) !void {
    var bytes: [4]u8 = undefined;
    std.mem.writeInt(u32, &bytes, value, .little);
    try out.appendSlice(allocator, &bytes);
}

fn writeI32(allocator: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), value: i32) !void {
    var bytes: [4]u8 = undefined;
    std.mem.writeInt(i32, &bytes, value, .little);
    try out.appendSlice(allocator, &bytes);
}

fn writeNode(allocator: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), node: *const FakeNode) !void {
    try writeI32(allocator, out, node.id);
    try writeI32(allocator, out, node.parent);
    try out.appendSlice(allocator, &node.class);
    try out.appendSlice(allocator, &node.display);
    try out.append(allocator, if (node.expand) 1 else 0);
    try writeU32(allocator, out, @intCast(node.props.items.len));
    for (node.props.items) |prop| {
        var p = prop;
        try out.appendSlice(allocator, std.mem.asBytes(&p));
    }
}

fn readU32(blob: []const u8, cursor: *usize) !u32 {
    if (cursor.* + 4 > blob.len) return error.BadBlob;
    const value = std.mem.readInt(u32, blob[cursor.*..][0..4], .little);
    cursor.* += 4;
    return value;
}

fn readI32(blob: []const u8, cursor: *usize) !i32 {
    if (cursor.* + 4 > blob.len) return error.BadBlob;
    const value = std.mem.readInt(i32, blob[cursor.*..][0..4], .little);
    cursor.* += 4;
    return value;
}

fn readNode(allocator: std.mem.Allocator, blob: []const u8, cursor: *usize, out: *FakeNode) bool {
    out.id = readI32(blob, cursor) catch return false;
    out.parent = readI32(blob, cursor) catch return false;
    if (cursor.* + name_capacity * 2 + 1 > blob.len) return false;
    @memcpy(&out.class, blob[cursor.*..][0..name_capacity]);
    cursor.* += name_capacity;
    @memcpy(&out.display, blob[cursor.*..][0..name_capacity]);
    cursor.* += name_capacity;
    out.expand = blob[cursor.*] != 0;
    cursor.* += 1;
    const prop_count = readU32(blob, cursor) catch return false;
    const prop_size = @sizeOf(PropRecord);
    if (cursor.* + prop_count * prop_size > blob.len) return false;
    out.props = .empty;
    out.props.ensureTotalCapacity(allocator, prop_count) catch return false;
    var i: u32 = 0;
    while (i < prop_count) : (i += 1) {
        var record: PropRecord = undefined;
        @memcpy(std.mem.asBytes(&record), blob[cursor.*..][0..prop_size]);
        cursor.* += prop_size;
        out.props.appendAssumeCapacity(record);
    }
    return true;
}
