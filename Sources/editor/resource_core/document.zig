//! The open resource project as the panels and history see it. A per-project
//! mirror of what the bridge holds: the path (null until first save), the
//! kind, the tree (nodes and props keyed by id), the current advisory lock
//! owner, and `dirty_through_history` which is read from `History.dirty()`
//! by callers that want the derived flag without threading the history
//! argument through the panels. The dirty flag is NOT stored - it is
//! computed from the history passed to `isDirty`.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const history_mod = @import("history.zig");
const ResBridge = bridge_mod.ResBridge;
const Kind = bridge_mod.Kind;
const NodeRecord = bridge_mod.NodeRecord;
const PropRecord = bridge_mod.PropRecord;
const EditError = bridge_mod.EditError;
const ResourceCommand = history_mod.ResourceCommand;
const OwnedBytes = history_mod.OwnedBytes;
const History = history_mod.History;

/// A lock owner, as `BkResLockOwner` writes it: the user names of the
/// folder's `locked_*` files, comma-separated, that the status bar shows
/// and that other sessions compare against. The buffer is owned - deinit
/// frees it.
pub const LockOwner = struct {
    name: []u8,

    pub fn fromSlice(allocator: std.mem.Allocator, text: []const u8) !LockOwner {
        return .{ .name = try allocator.dupe(u8, text) };
    }

    pub fn deinit(self: *LockOwner, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
        self.name = &.{};
    }
};

/// Everything the renderer needs about one node and its props. The core
/// stores props by (node_id, prop_id) via a flat ArrayList the Tree keeps -
/// a resource project's prop tables are small (a few hundred entries), so a
/// linear scan on apply is cheap and avoids a hash of composite keys.
pub const Node = NodeRecord;
pub const Prop = struct { node: i32, record: PropRecord };

pub const Tree = struct {
    nodes: std.ArrayListUnmanaged(Node) = .empty,
    props: std.ArrayListUnmanaged(Prop) = .empty,

    pub fn deinit(self: *Tree, allocator: std.mem.Allocator) void {
        self.nodes.deinit(allocator);
        self.props.deinit(allocator);
    }

    pub fn indexOfNode(self: *const Tree, node_id: i32) ?usize {
        for (self.nodes.items, 0..) |node, index| {
            if (node.id == node_id) return index;
        }
        return null;
    }

    pub fn findNode(self: *Tree, node_id: i32) ?*Node {
        const i = self.indexOfNode(node_id) orelse return null;
        return &self.nodes.items[i];
    }

    pub fn findProp(self: *Tree, node_id: i32, prop_id: i32) ?*Prop {
        for (self.props.items) |*p| {
            if (p.node == node_id and p.record.id == prop_id) return p;
        }
        return null;
    }

    /// Clears both lists while keeping their capacity, so a reload does not
    /// churn the allocator.
    pub fn clear(self: *Tree) void {
        self.nodes.clearRetainingCapacity();
        self.props.clearRetainingCapacity();
    }
};

pub const Document = struct {
    path: ?std.ArrayListUnmanaged(u8) = null,
    kind: Kind = .weapon,
    tree: Tree = .{},
    lock_owner: ?LockOwner = null,

    pub fn deinit(self: *Document, allocator: std.mem.Allocator) void {
        if (self.path) |*p| p.deinit(allocator);
        self.path = null;
        self.tree.deinit(allocator);
        if (self.lock_owner) |*owner| owner.deinit(allocator);
        self.lock_owner = null;
    }

    pub fn pathSlice(self: *const Document) ?[]const u8 {
        if (self.path) |p| return p.items;
        return null;
    }

    pub fn setPath(self: *Document, allocator: std.mem.Allocator, text: ?[]const u8) !void {
        if (self.path) |*p| {
            p.deinit(allocator);
            self.path = null;
        }
        if (text) |bytes| {
            var list: std.ArrayListUnmanaged(u8) = .empty;
            errdefer list.deinit(allocator);
            try list.appendSlice(allocator, bytes);
            self.path = list;
        }
    }

    /// Whether the document differs from the saved state. The history is the
    /// source of truth: a merge, a drop, a clear and the clean mark all live
    /// in the kit's primitive. Not a stored flag.
    pub fn isDirty(self: *const Document, history: *const History) bool {
        _ = self;
        return history.dirty();
    }

    /// Replaces the tree with what the bridge holds. Used after Open/New or
    /// after a Restore-of-root undo path. Staged in locals first so a failed
    /// reload leaves `self` as it was.
    pub fn reload(self: *Document, allocator: std.mem.Allocator, bridge: ResBridge) EditError!void {
        var total: usize = 0;
        var none: [0]NodeRecord = .{};
        const sizing = bridge.nodes(&none, &total);
        if (sizing != .ok and sizing != .refused) return bridge_mod.check(sizing);
        var fresh_nodes: std.ArrayListUnmanaged(Node) = .empty;
        errdefer fresh_nodes.deinit(allocator);
        try fresh_nodes.resize(allocator, total);
        try bridge_mod.check(bridge.nodes(fresh_nodes.items, &total));

        var fresh_props: std.ArrayListUnmanaged(Prop) = .empty;
        errdefer fresh_props.deinit(allocator);
        for (fresh_nodes.items) |node| {
            var prop_total: usize = 0;
            var none_props: [0]PropRecord = .{};
            const prop_sizing = bridge.props(node.id, &none_props, &prop_total);
            if (prop_sizing != .ok and prop_sizing != .refused) return bridge_mod.check(prop_sizing);
            if (prop_total == 0) continue;
            const base = fresh_props.items.len;
            try fresh_props.resize(allocator, base + prop_total);
            var scratch: std.ArrayListUnmanaged(PropRecord) = .empty;
            defer scratch.deinit(allocator);
            try scratch.resize(allocator, prop_total);
            try bridge_mod.check(bridge.props(node.id, scratch.items, &prop_total));
            for (scratch.items, 0..) |record, i| {
                fresh_props.items[base + i] = .{ .node = node.id, .record = record };
            }
        }

        self.tree.deinit(allocator);
        self.tree = .{ .nodes = fresh_nodes, .props = fresh_props };
    }

    /// Reads the open project's kind from the bridge and stores it.
    pub fn refreshKind(self: *Document, bridge: ResBridge) EditError!void {
        var k: Kind = .weapon;
        try bridge_mod.check(bridge.kindOf(&k));
        self.kind = k;
    }

    /// Takes advisory ownership and stores the owner string. The caller
    /// passes the owner text in; the mirror keeps it so the status bar can
    /// display it without re-reading the bridge each frame.
    pub fn takeLock(self: *Document, allocator: std.mem.Allocator, bridge: ResBridge, owner: []const u8) EditError!void {
        try bridge_mod.check(bridge.lock(owner));
        if (self.lock_owner) |*current| current.deinit(allocator);
        self.lock_owner = try LockOwner.fromSlice(allocator, owner);
    }

    /// Applies one command through the bridge, then updates the local
    /// mirror. The history layer records the command; this method is pure
    /// bridge+mirror. Callers that want gesture merging or undo bookkeeping
    /// use the History methods.
    pub fn apply(self: *Document, allocator: std.mem.Allocator, bridge: ResBridge, command: *ResourceCommand) EditError!void {
        switch (command.*) {
            .set_prop => |c| try bridge_mod.check(bridge.setProp(c.node, c.prop_id, c.after.bytes)),
            .insert_node => |*c| {
                var new_id: i32 = -1;
                try bridge_mod.check(bridge.insertNode(c.parent, c.class_name, c.index, &new_id));
                c.new_id = new_id;
            },
            .delete_node => |*c| {
                var size: usize = 0;
                var none: [0]u8 = .{};
                const sizing = bridge.deleteNode(c.node, &none, &size);
                if (sizing != .ok and sizing != .refused) try bridge_mod.check(sizing);
                const scratch = try allocator.alloc(u8, size);
                errdefer allocator.free(scratch);
                try bridge_mod.check(bridge.deleteNode(c.node, scratch, &size));
                c.blob.deinit(allocator);
                c.blob = .{ .bytes = scratch };
            },
            .move_node => |c| try bridge_mod.check(bridge.moveNode(c.node, c.after_parent, c.after_index)),
            .rename_node => |c| try bridge_mod.check(bridge.setNodeName(c.node, c.after.bytes)),
            .geometry => |c| try bridge_mod.check(bridge.geometryWrite(c.node, c.channel, &c.after)),
            .composite => |*c| for (c.steps.items) |*step| try self.apply(allocator, bridge, step),
        }
        try self.reload(allocator, bridge);
    }

    /// Replays a command as an undo through the bridge. The command's shape
    /// decides what the inverse is - the same split as the map editor's
    /// `editor.replay`.
    pub fn undoOne(self: *Document, allocator: std.mem.Allocator, bridge: ResBridge, command: *ResourceCommand) EditError!void {
        switch (command.*) {
            .set_prop => |c| try bridge_mod.check(bridge.setProp(c.node, c.prop_id, c.before.bytes)),
            .insert_node => |*c| {
                var size: usize = 0;
                var none: [0]u8 = .{};
                const sizing = bridge.deleteNode(c.new_id, &none, &size);
                if (sizing != .ok and sizing != .refused) try bridge_mod.check(sizing);
                const scratch = try allocator.alloc(u8, size);
                errdefer allocator.free(scratch);
                try bridge_mod.check(bridge.deleteNode(c.new_id, scratch, &size));
                c.undo_blob.deinit(allocator);
                c.undo_blob = .{ .bytes = scratch };
            },
            .delete_node => |*c| {
                // Both bridges give the node its old id back when it is free,
                // but the redo must delete whatever id the restore answered.
                var restored_id: i32 = -1;
                try bridge_mod.check(bridge.restoreNode(c.blob.bytes, c.parent, c.index, &restored_id));
                c.node = restored_id;
            },
            .move_node => |c| try bridge_mod.check(bridge.moveNode(c.node, c.before_parent, c.before_index)),
            .rename_node => |c| try bridge_mod.check(bridge.setNodeName(c.node, c.before.bytes)),
            .geometry => |c| try bridge_mod.check(bridge.geometryWrite(c.node, c.channel, &c.before)),
            .composite => |*c| {
                var i: usize = c.steps.items.len;
                while (i > 0) {
                    i -= 1;
                    try self.undoOne(allocator, bridge, &c.steps.items[i]);
                }
            },
        }
        try self.reload(allocator, bridge);
    }

    /// Replays a command as a redo - the forward side of the switch above.
    pub fn redoOne(self: *Document, allocator: std.mem.Allocator, bridge: ResBridge, command: *ResourceCommand) EditError!void {
        switch (command.*) {
            .set_prop => |c| try bridge_mod.check(bridge.setProp(c.node, c.prop_id, c.after.bytes)),
            .insert_node => |*c| {
                var new_id: i32 = -1;
                if (c.undo_blob.bytes.len != 0) {
                    try bridge_mod.check(bridge.restoreNode(c.undo_blob.bytes, c.parent, c.index, &new_id));
                } else {
                    try bridge_mod.check(bridge.insertNode(c.parent, c.class_name, c.index, &new_id));
                }
                c.new_id = new_id;
            },
            .delete_node => |*c| {
                var size: usize = 0;
                var none: [0]u8 = .{};
                const sizing = bridge.deleteNode(c.node, &none, &size);
                if (sizing != .ok and sizing != .refused) try bridge_mod.check(sizing);
                const scratch = try allocator.alloc(u8, size);
                errdefer allocator.free(scratch);
                try bridge_mod.check(bridge.deleteNode(c.node, scratch, &size));
                c.blob.deinit(allocator);
                c.blob = .{ .bytes = scratch };
            },
            .move_node => |c| try bridge_mod.check(bridge.moveNode(c.node, c.after_parent, c.after_index)),
            .rename_node => |c| try bridge_mod.check(bridge.setNodeName(c.node, c.after.bytes)),
            .geometry => |c| try bridge_mod.check(bridge.geometryWrite(c.node, c.channel, &c.after)),
            .composite => |*c| for (c.steps.items) |*step| try self.redoOne(allocator, bridge, step),
        }
        try self.reload(allocator, bridge);
    }
};
