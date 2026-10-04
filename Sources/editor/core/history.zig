//! Undo and redo. A command records what it needs to go both ways; the
//! Editor does the bridge calls, this file only keeps the stacks and knows
//! whether the map differs from the last save.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const ObjectRecord = bridge_mod.ObjectRecord;
const SoundRecord = bridge_mod.SoundRecord;
const records = @import("records.zig");

pub const Pose = struct { x: f32, y: f32, dir: i32, player: i32 };

/// One member of a delete-all (M3, D-25): the record as it was and the index
/// in the document's list it was removed from, so the undo puts it back
/// exactly there.
pub const DeletedRecord = struct { object: ObjectRecord, index: usize };

/// What a bridge-logged edit changed, so a replay knows what to refresh.
pub const EditScope = enum { vso, objects, altitudes, players };

pub const Command = union(enum) {
    /// One bridge token per paint call of the gesture, oldest first.
    paint: struct { tokens: std.ArrayListUnmanaged(i32) = .empty },
    /// The object as added and where the document holds it, for redo.
    add: struct { object: ObjectRecord, index: usize },
    place: struct { link_id: i32, before: Pose, after: Pose },
    delete: struct { object: ObjectRecord, index: usize },
    /// The selection's delete-all (M3, D-25): every member through the M2
    /// cascade, one undo step. `deleted` holds each member as it was with
    /// the index it was removed at, in deletion order; undo restores them
    /// last-deleted-first at their own recorded indices (the exact reverse
    /// of the way they went), redo deletes them again in the recorded order.
    multi_delete: struct { deleted: std.ArrayListUnmanaged(DeletedRecord) = .empty },
    diplomacy: struct { player: i32, before: i32, after: i32 },
    /// An object's script ID (D-15), -1 none; one entry per gesture, merged
    /// while the same object's value is typed.
    script_id: struct { link_id: i32, before: i32, after: i32 },
    map_type: struct { before: i32, after: i32 },
    attacking_side: struct { before: i32, after: i32 },
    /// The sound as added and where the bridge's list holds it, for undo
    /// (delete at `index`) and redo (add `record` back at `index`).
    sound_add: struct { index: usize, record: SoundRecord },
    /// `index` never moves for an edit - only the record at it changes.
    sound_edit: struct { index: usize, before: SoundRecord, after: SoundRecord },
    /// The sound as it was before it was deleted, and its index, so undo
    /// re-adds it exactly there.
    sound_delete: struct { index: usize, record: SoundRecord },
    /// The generic record command (D-02): one whole record before and after.
    /// `key` is an index, an ID, or 0 for a singleton record (the camera
    /// anchors). Undo and redo put `before` / `after` back through the same
    /// bridge call the edit used. The command owns both values.
    record_edit: struct { kind: records.Kind, key: i32, before: records.Value, after: records.Value },
    /// A record put in that was not there (D-02, 04-09): a new group. Undo
    /// removes the record at `key`, redo puts `value` back. The command owns
    /// the value.
    record_add: struct { kind: records.Kind, key: i32, value: records.Value },
    /// A record taken out: undo puts `value` (read before the removal) back
    /// under `key`, redo removes it again. The command owns the value.
    record_delete: struct { kind: records.Kind, key: i32, value: records.Value },
    /// An edit the bridge logs itself (BkEditorUndoEdit/RedoEdit, 04-05):
    /// the bridge keeps the records before and after and hands out a token;
    /// this keeps the tokens of one gesture, oldest first. `scope` says what
    /// the core refreshes after a replay: `vso` bumps the road and river
    /// generation, `objects` reloads the document's objects (the compound
    /// edits of bridges, fences and entrenchments, which add and remove map
    /// objects), `players` re-reads the diplomacy table, its length and the
    /// objects' owners (a player add or delete, 05-05).
    edit: struct { tokens: std.ArrayListUnmanaged(i32) = .empty, scope: EditScope },
    /// Several commands that undo and redo as ONE step (05-05, Check Map's Fix
    /// all): the commands exactly as they were recorded, oldest first. Undo
    /// takes them back last first, redo replays them in order. The command owns
    /// them.
    composite: struct { steps: std.ArrayListUnmanaged(Command) = .empty },

    pub fn deinit(self: *Command, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .paint => |*p| p.tokens.deinit(allocator),
            .edit => |*e| e.tokens.deinit(allocator),
            .multi_delete => |*d| d.deleted.deinit(allocator),
            .record_edit => |*e| {
                e.before.deinit(allocator);
                e.after.deinit(allocator);
            },
            .record_add => |*e| e.value.deinit(allocator),
            .record_delete => |*e| e.value.deinit(allocator),
            .composite => |*c| {
                for (c.steps.items) |*step| step.deinit(allocator);
                c.steps.deinit(allocator);
            },
            else => {},
        }
    }
};

pub const Entry = struct { command: Command, gesture: u32 };

pub const History = struct {
    undo_stack: std.ArrayListUnmanaged(Entry) = .empty,
    redo_stack: std.ArrayListUnmanaged(Entry) = .empty,
    /// The undo depth at the last open or save; null once that state can no
    /// longer be reached by undo or redo.
    clean_depth: ?usize = 0,
    /// Bumped by everything that changes what the document is: a command
    /// recorded, a merge into the top entry, a merge that drops it, an undo or
    /// a redo (the Editor bumps those two - it moves the stacks itself), a
    /// clear. A view of the map that is costly to rebuild (the Minimap's
    /// texture, 05-07) rebuilds when this moves and not otherwise.
    revision: u32 = 0,

    pub fn deinit(self: *History, allocator: std.mem.Allocator) void {
        self.clear(allocator);
        self.undo_stack.deinit(allocator);
        self.redo_stack.deinit(allocator);
        self.* = undefined;
    }

    pub fn clear(self: *History, allocator: std.mem.Allocator) void {
        for (self.undo_stack.items) |*entry| entry.command.deinit(allocator);
        for (self.redo_stack.items) |*entry| entry.command.deinit(allocator);
        self.undo_stack.clearRetainingCapacity();
        self.redo_stack.clearRetainingCapacity();
        self.clean_depth = 0;
        self.revision +%= 1;
    }

    /// Reserves room for one more undone command without touching anything
    /// else. Call this before the bridge call a command is about to make, so
    /// that once the bridge has committed, recording it cannot fail.
    pub fn reserve(self: *History, allocator: std.mem.Allocator) !void {
        try self.undo_stack.ensureUnusedCapacity(allocator, 1);
    }

    /// Frees every command on the redo branch. Called both when a new
    /// command is recorded and when a merge changes the top entry: either
    /// way the branch it pointed from is no longer the one being built on.
    fn dropRedoBranch(self: *History, allocator: std.mem.Allocator) void {
        for (self.redo_stack.items) |*entry| entry.command.deinit(allocator);
        self.redo_stack.clearRetainingCapacity();
    }

    /// Takes ownership of the command, even on error.
    pub fn record(self: *History, allocator: std.mem.Allocator, command: Command, gesture: u32) !void {
        var owned = command;
        errdefer owned.deinit(allocator);
        try self.reserve(allocator);
        self.recordAssumeCapacity(allocator, owned, gesture);
    }

    /// Same as `record`, but cannot fail: call `reserve` first so the append
    /// needs no allocation. Lets a command reserve room before its bridge
    /// call commits, so an allocation failure can never happen after the
    /// bridge has already acted.
    pub fn recordAssumeCapacity(self: *History, allocator: std.mem.Allocator, command: Command, gesture: u32) void {
        self.revision +%= 1;
        self.dropRedoBranch(allocator);
        if (self.clean_depth) |depth| {
            if (depth > self.undo_stack.items.len) self.clean_depth = null;
        }
        self.undo_stack.appendAssumeCapacity(.{ .command = command, .gesture = gesture });
    }

    pub fn top(self: *History) ?*Entry {
        if (self.undo_stack.items.len == 0) return null;
        return &self.undo_stack.items[self.undo_stack.items.len - 1];
    }

    /// A merge changed the top entry instead of recording a new one; the
    /// redo branch is just as unreachable from it as from a fresh command,
    /// so it goes too. If that entry was the saved state, the saved state is
    /// gone.
    pub fn touchTop(self: *History, allocator: std.mem.Allocator) void {
        self.revision +%= 1;
        self.dropRedoBranch(allocator);
        if (self.clean_depth) |depth| {
            if (depth == self.undo_stack.items.len) self.clean_depth = null;
        }
    }

    /// A merge brought the top entry back to where it began, so it is no
    /// step at all and goes, as if the gesture had never been. The redo
    /// branch goes as with any merge. The map is now in the state before the
    /// entry: clean if that was the saved one, and the saved state is gone if
    /// it was the entry's own.
    pub fn dropTop(self: *History, allocator: std.mem.Allocator) void {
        if (self.undo_stack.items.len == 0) return;
        self.revision +%= 1;
        self.dropRedoBranch(allocator);
        if (self.clean_depth) |depth| {
            if (depth == self.undo_stack.items.len) self.clean_depth = null;
        }
        var entry = self.undo_stack.pop().?;
        entry.command.deinit(allocator);
    }

    pub fn markClean(self: *History) void {
        self.clean_depth = self.undo_stack.items.len;
    }

    pub fn dirty(self: *const History) bool {
        return self.clean_depth == null or self.clean_depth.? != self.undo_stack.items.len;
    }

    pub fn canUndo(self: *const History) bool {
        return self.undo_stack.items.len != 0;
    }

    pub fn canRedo(self: *const History) bool {
        return self.redo_stack.items.len != 0;
    }
};

test "dropping the top entry goes back to the state before it" {
    var history: History = .{};
    defer history.deinit(std.testing.allocator);
    try history.record(std.testing.allocator, .{ .map_type = .{ .before = 0, .after = 1 } }, 0);
    history.dropTop(std.testing.allocator);
    try std.testing.expect(!history.dirty());
    try std.testing.expect(!history.canUndo());
    try history.record(std.testing.allocator, .{ .map_type = .{ .before = 0, .after = 1 } }, 0);
    history.markClean(); // saved with the entry applied
    history.dropTop(std.testing.allocator);
    try std.testing.expect(history.dirty());
}

test "the clean mark follows the undo depth" {
    var history: History = .{};
    defer history.deinit(std.testing.allocator);
    try std.testing.expect(!history.dirty());
    try history.record(std.testing.allocator, .{ .map_type = .{ .before = 0, .after = 1 } }, 0);
    try std.testing.expect(history.dirty());
    history.markClean();
    try std.testing.expect(!history.dirty());
    history.touchTop(std.testing.allocator);
    try std.testing.expect(history.dirty());
}

test "the revision moves with every change of the document and with nothing else" {
    var history: History = .{};
    defer history.deinit(std.testing.allocator);
    const start = history.revision;
    try history.record(std.testing.allocator, .{ .map_type = .{ .before = 0, .after = 1 } }, 0);
    try std.testing.expect(history.revision != start);
    const recorded = history.revision;
    history.touchTop(std.testing.allocator);
    try std.testing.expect(history.revision != recorded);
    const touched = history.revision;
    history.markClean();
    _ = history.dirty();
    try std.testing.expectEqual(touched, history.revision);
    history.dropTop(std.testing.allocator);
    try std.testing.expect(history.revision != touched);
    const dropped = history.revision;
    history.clear(std.testing.allocator);
    try std.testing.expect(history.revision != dropped);
}
