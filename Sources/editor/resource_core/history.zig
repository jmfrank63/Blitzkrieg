//! Resource editor's command vocabulary on top of the kit's generic save-mark
//! + stack primitive (`editor_kit.history`). The Command union lives here;
//! the stack mechanics (undo/redo, clean depth, revision, gesture merge) are
//! inherited from the kit so this file stays small and the map and resource
//! cores share one tested stack implementation.
const std = @import("std");
const kit_history = @import("editor_kit").history;
const bridge_mod = @import("bridge.zig");
const GeometryChannel = bridge_mod.GeometryChannel;
const GeometryValue = bridge_mod.GeometryValue;
const GuiRect = bridge_mod.GuiRect;

/// What a bridge-logged edit changed, so a replay knows what to refresh. The
/// `.geometry` variant below has one of these so T04 can wire a specific
/// channel without re-shaping the enum.
pub const EditScope = enum { tree, props, geometry, references, preview };

/// Owned bytes buffer. Used both for `.set_prop`'s before/after text and for
/// `.delete_node`'s serialised subtree blob (what BkResDeleteNode writes).
pub const OwnedBytes = struct {
    bytes: []u8 = &.{},

    pub fn fromSlice(allocator: std.mem.Allocator, source: []const u8) !OwnedBytes {
        return .{ .bytes = try allocator.dupe(u8, source) };
    }

    pub fn deinit(self: *OwnedBytes, allocator: std.mem.Allocator) void {
        if (self.bytes.len != 0) allocator.free(self.bytes);
        self.bytes = &.{};
    }
};

/// The GUI sub-editor's edits of an open screen (kind gui). Window ids in a
/// command are LOGICAL: the id a window had when the command first ran. The
/// bridge never reuses an id, so an undo of an insert or a delete and its
/// redo bring a window back under a new one; `gui_tools.IdMap` maps each
/// logical id to the window's current id, and every replay translates
/// through it, so the commands recorded after an insert still find its
/// window.
pub const GuiCommand = union(enum) {
    /// A gesture's rects: every touched window's flag and rect before and
    /// after. Undo writes `before`, redo `after`, each in one bridge call.
    set_rects: struct {
        before: []GuiRect,
        after: []GuiRect,
    },
    /// Windows made under `parent`, from a template file or from clipboard
    /// text (a paste). Empty `ids` means not yet applied. Undo copies the
    /// created windows' text into `undo_text` and deletes them; redo pastes
    /// that text back, as the bridge has no insert at a given id.
    create: struct {
        parent: i32,
        from_template: bool,
        /// The template path, or the clipboard text.
        source: OwnedBytes,
        /// The template's position, or the paste's offset.
        x: i32,
        y: i32,
        /// The outermost windows made, then every window made in document
        /// order (the outermost ones included).
        tops: []i32 = &.{},
        ids: []i32 = &.{},
        undo_text: OwnedBytes = .{},
    },
    /// Windows deleted with their subtrees, one entry per outermost window.
    /// Undo pastes each entry's text back under its parent.
    delete: struct { items: []GuiDeleted },
    /// One XML attribute of a window. `had_before` is false when the window
    /// had no such attribute; the bridge cannot remove one, so undo then
    /// writes an empty value.
    set_attr: struct {
        id: i32,
        name: OwnedBytes,
        had_before: bool,
        before: OwnedBytes,
        after: OwnedBytes,
    },

    pub fn deinit(self: *GuiCommand, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .set_rects => |*c| {
                allocator.free(c.before);
                allocator.free(c.after);
            },
            .create => |*c| {
                c.source.deinit(allocator);
                if (c.tops.len != 0) allocator.free(c.tops);
                if (c.ids.len != 0) allocator.free(c.ids);
                c.undo_text.deinit(allocator);
            },
            .delete => |*c| {
                for (c.items) |*item| item.deinit(allocator);
                allocator.free(c.items);
            },
            .set_attr => |*c| {
                c.name.deinit(allocator);
                c.before.deinit(allocator);
                c.after.deinit(allocator);
            },
        }
    }
};

/// One outermost window of a GUI delete: its logical id and parent, the
/// clipboard text of its subtree, and the logical ids of the subtree's
/// windows in document order.
pub const GuiDeleted = struct {
    id: i32,
    parent: i32,
    text: OwnedBytes,
    subtree: []i32,

    pub fn deinit(self: *GuiDeleted, allocator: std.mem.Allocator) void {
        self.text.deinit(allocator);
        allocator.free(self.subtree);
    }
};

/// The resource editor's command vocabulary. Each variant records one logical
/// user edit; undo/redo replay through the bridge in exactly the opposite
/// order. The `.geometry` variant is reserved for T04: it carries any
/// GeometryChannel's before/after payload (points, aimed points, vec3s, or
/// a byte grid), so T04 can wire specific channel commands without touching
/// this union's shape.
pub const ResourceCommand = union(enum) {
    /// A property write: node id + prop id, the previous and new text form.
    /// Undo rewrites `before`, redo rewrites `after`. Merged while the same
    /// (node, prop) is typed - the kit's gesture mechanism handles the merge.
    set_prop: struct {
        node: i32,
        prop_id: i32,
        before: OwnedBytes,
        after: OwnedBytes,
    },
    /// A node insert: the parent, the class name used, the index that was
    /// handed out, and the node's new id (what BkResInsertNode wrote). Undo
    /// deletes the node (and captures the subtree blob into `undo_blob` so a
    /// redo can rebuild from the same bytes); redo re-inserts from that blob.
    /// `undo_blob` is populated when the command is undone the first time -
    /// before that it is empty, because the fresh node has nothing to
    /// serialise beyond its default state.
    insert_node: struct {
        parent: i32,
        class_name: []u8,
        index: i32,
        new_id: i32,
        undo_blob: OwnedBytes = .{},
        /// The displayed name the new node takes right after the insert
        /// (MFC's SetItemName before AddChild, as a thumbnail double-click
        /// names a frame after its picture); empty keeps the class default.
        /// A redo from `undo_blob` already carries it.
        name: OwnedBytes = .{},
    },
    /// A node delete: the parent/index it was at, and the serialised subtree
    /// bytes BkResDeleteNode wrote. Undo re-inserts the subtree at
    /// parent/index; redo deletes it again, overwriting the blob.
    delete_node: struct {
        parent: i32,
        index: i32,
        node: i32,
        blob: OwnedBytes,
    },
    /// A node move: the move's before/after position. Undo moves back,
    /// redo moves forward.
    move_node: struct {
        node: i32,
        before_parent: i32,
        before_index: i32,
        after_parent: i32,
        after_index: i32,
    },
    /// A rename: the node's displayed name before and after. Undo writes
    /// `before`, redo `after`.
    rename_node: struct {
        node: i32,
        before: OwnedBytes,
        after: OwnedBytes,
    },
    /// A geometry write (reserved for T04): one GeometryChannel, before and
    /// after payloads. Undo writes `before`, redo writes `after`. T04 will
    /// extend the fake bridge to serve all channels; the shape itself is set
    /// here so the history layer does not shift.
    geometry: struct {
        node: i32,
        channel: GeometryChannel,
        before: GeometryValue,
        after: GeometryValue,
    },
    /// Several commands that undo and redo as ONE step: a gesture
    /// collapsed by the editor (a drag of several property writes, a
    /// multi-node delete). Owns the inner commands - deinit frees them in
    /// reverse order.
    composite: struct { steps: std.ArrayListUnmanaged(ResourceCommand) = .empty },
    /// An edit of a game UI screen (the GUI sub-editor).
    gui: GuiCommand,

    pub fn deinit(self: *ResourceCommand, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .set_prop => |*c| {
                c.before.deinit(allocator);
                c.after.deinit(allocator);
            },
            .insert_node => |*c| {
                allocator.free(c.class_name);
                c.undo_blob.deinit(allocator);
                c.name.deinit(allocator);
            },
            .delete_node => |*c| c.blob.deinit(allocator),
            .move_node => {},
            .rename_node => |*c| {
                c.before.deinit(allocator);
                c.after.deinit(allocator);
            },
            .geometry => |*c| {
                c.before.deinit(allocator);
                c.after.deinit(allocator);
            },
            .composite => |*c| {
                var i: usize = c.steps.items.len;
                while (i > 0) {
                    i -= 1;
                    c.steps.items[i].deinit(allocator);
                }
                c.steps.deinit(allocator);
            },
            .gui => |*c| c.deinit(allocator),
        }
    }
};

/// Resource editor's `Entry` is the kit's generic Entry over its own command.
pub const Entry = kit_history.Entry(ResourceCommand);

/// Resource editor's `History` is the kit's generic stack over its own
/// command. The stack mechanics (push/pop, clean depth, revision counter,
/// merge hooks) come from the kit; this file only brings the ResourceCommand
/// union.
pub const History = kit_history.History(ResourceCommand);

test "an empty ResourceCommand deinit does not leak" {
    var cmd: ResourceCommand = .{ .move_node = .{
        .node = 1,
        .before_parent = 0,
        .before_index = 0,
        .after_parent = 0,
        .after_index = 1,
    } };
    cmd.deinit(std.testing.allocator);
}

test "set_prop deinit frees both owned buffers" {
    const allocator = std.testing.allocator;
    var cmd: ResourceCommand = .{ .set_prop = .{
        .node = 7,
        .prop_id = 2,
        .before = try OwnedBytes.fromSlice(allocator, "old"),
        .after = try OwnedBytes.fromSlice(allocator, "new"),
    } };
    cmd.deinit(allocator);
}

test "composite deinit frees inner commands in reverse order" {
    const allocator = std.testing.allocator;
    var steps: std.ArrayListUnmanaged(ResourceCommand) = .empty;
    try steps.append(allocator, .{ .set_prop = .{
        .node = 1,
        .prop_id = 1,
        .before = try OwnedBytes.fromSlice(allocator, "a"),
        .after = try OwnedBytes.fromSlice(allocator, "b"),
    } });
    try steps.append(allocator, .{ .move_node = .{
        .node = 2,
        .before_parent = 0,
        .before_index = 0,
        .after_parent = 0,
        .after_index = 1,
    } });
    var cmd: ResourceCommand = .{ .composite = .{ .steps = steps } };
    cmd.deinit(allocator);
}
