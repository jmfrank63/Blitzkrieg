//! Undo and redo. A command records what it needs to go both ways; the
//! Editor does the bridge calls, this file only keeps the stacks and knows
//! whether the map differs from the last save.
const std = @import("std");
const ObjectRecord = @import("bridge.zig").ObjectRecord;

pub const Pose = struct { x: f32, y: f32, dir: i32, player: i32 };

pub const Command = union(enum) {
    /// One bridge token per paint call of the gesture, oldest first.
    paint: struct { tokens: std.ArrayListUnmanaged(i32) = .empty },
    /// The object as added and where the document holds it, for redo.
    add: struct { object: ObjectRecord, index: usize },
    place: struct { link_id: i32, before: Pose, after: Pose },
    delete: struct { object: ObjectRecord, index: usize },
    diplomacy: struct { player: i32, before: i32, after: i32 },
    map_type: struct { before: i32, after: i32 },
    attacking_side: struct { before: i32, after: i32 },

    pub fn deinit(self: *Command, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .paint => |*p| p.tokens.deinit(allocator),
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
        self.dropRedoBranch(allocator);
        if (self.clean_depth) |depth| {
            if (depth == self.undo_stack.items.len) self.clean_depth = null;
        }
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
