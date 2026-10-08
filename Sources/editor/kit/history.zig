//! The generic save-mark + undo/redo stack primitive for the editor kit.
//! A `History(Command)` holds two stacks of command entries, tracks a save
//! mark (`clean_depth`) and a `revision` counter, and offers push/pop/merge
//! hooks; it has no knowledge of what a command is or how to replay it. The
//! editor on top owns the Command union and the replay path (which is what
//! keeps the kit map-agnostic): see `Sources/editor/core/history.zig` for
//! MapEditor's own Command union and its `editor.replay` driver.
//!
//! `Command` must define `pub fn deinit(self: *Command, allocator: std.mem.Allocator) void`
//! so the stacks can free owned payloads on `clear`, `dropTop`, drop of the
//! redo branch, and `deinit`.
const std = @import("std");

/// One recorded step: the command plus the gesture (merged-together strokes
/// share one). Generic over the editor's own command type.
pub fn Entry(comptime Command: type) type {
    return struct { command: Command, gesture: u32 };
}

/// Two stacks of command entries, a save mark that tracks where the clean
/// state sits in the undo depth, and a revision counter the UI watches to
/// know when to rebuild costly views (minimap, etc). Generic over the
/// editor's own command type.
pub fn History(comptime Command: type) type {
    return struct {
        const Self = @This();
        pub const EntryT = Entry(Command);

        undo_stack: std.ArrayListUnmanaged(EntryT) = .empty,
        redo_stack: std.ArrayListUnmanaged(EntryT) = .empty,
        /// The undo depth at the last open or save; null once that state can no
        /// longer be reached by undo or redo.
        clean_depth: ?usize = 0,
        /// Bumped by everything that changes what the document is: a command
        /// recorded, a merge into the top entry, a merge that drops it, an undo or
        /// a redo (the editor bumps those two - it moves the stacks itself), a
        /// clear. A view of the map that is costly to rebuild (the Minimap's
        /// texture, 05-07) rebuilds when this moves and not otherwise.
        revision: u32 = 0,

        pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
            self.clear(allocator);
            self.undo_stack.deinit(allocator);
            self.redo_stack.deinit(allocator);
            self.* = undefined;
        }

        pub fn clear(self: *Self, allocator: std.mem.Allocator) void {
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
        pub fn reserve(self: *Self, allocator: std.mem.Allocator) !void {
            try self.undo_stack.ensureUnusedCapacity(allocator, 1);
        }

        /// Frees every command on the redo branch. Called both when a new
        /// command is recorded and when a merge changes the top entry: either
        /// way the branch it pointed from is no longer the one being built on.
        fn dropRedoBranch(self: *Self, allocator: std.mem.Allocator) void {
            for (self.redo_stack.items) |*entry| entry.command.deinit(allocator);
            self.redo_stack.clearRetainingCapacity();
        }

        /// Takes ownership of the command, even on error.
        pub fn record(self: *Self, allocator: std.mem.Allocator, command: Command, gesture: u32) !void {
            var owned = command;
            errdefer owned.deinit(allocator);
            try self.reserve(allocator);
            self.recordAssumeCapacity(allocator, owned, gesture);
        }

        /// Same as `record`, but cannot fail: call `reserve` first so the append
        /// needs no allocation. Lets a command reserve room before its bridge
        /// call commits, so an allocation failure can never happen after the
        /// bridge has already acted.
        pub fn recordAssumeCapacity(self: *Self, allocator: std.mem.Allocator, command: Command, gesture: u32) void {
            self.revision +%= 1;
            self.dropRedoBranch(allocator);
            if (self.clean_depth) |depth| {
                if (depth > self.undo_stack.items.len) self.clean_depth = null;
            }
            self.undo_stack.appendAssumeCapacity(.{ .command = command, .gesture = gesture });
        }

        pub fn top(self: *Self) ?*EntryT {
            if (self.undo_stack.items.len == 0) return null;
            return &self.undo_stack.items[self.undo_stack.items.len - 1];
        }

        /// A merge changed the top entry instead of recording a new one; the
        /// redo branch is just as unreachable from it as from a fresh command,
        /// so it goes too. If that entry was the saved state, the saved state is
        /// gone.
        pub fn touchTop(self: *Self, allocator: std.mem.Allocator) void {
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
        pub fn dropTop(self: *Self, allocator: std.mem.Allocator) void {
            if (self.undo_stack.items.len == 0) return;
            self.revision +%= 1;
            self.dropRedoBranch(allocator);
            if (self.clean_depth) |depth| {
                if (depth == self.undo_stack.items.len) self.clean_depth = null;
            }
            var entry = self.undo_stack.pop().?;
            entry.command.deinit(allocator);
        }

        pub fn markClean(self: *Self) void {
            self.clean_depth = self.undo_stack.items.len;
        }

        pub fn dirty(self: *const Self) bool {
            return self.clean_depth == null or self.clean_depth.? != self.undo_stack.items.len;
        }

        pub fn canUndo(self: *const Self) bool {
            return self.undo_stack.items.len != 0;
        }

        pub fn canRedo(self: *const Self) bool {
            return self.redo_stack.items.len != 0;
        }
    };
}

// The kit's own tests use a toy command so the primitive is proven map-free.
const TestCommand = union(enum) {
    set_value: struct { before: i32, after: i32 },

    pub fn deinit(self: *TestCommand, allocator: std.mem.Allocator) void {
        _ = self;
        _ = allocator;
    }
};

test "dropping the top entry goes back to the state before it" {
    const H = History(TestCommand);
    var history: H = .{};
    defer history.deinit(std.testing.allocator);
    try history.record(std.testing.allocator, .{ .set_value = .{ .before = 0, .after = 1 } }, 0);
    history.dropTop(std.testing.allocator);
    try std.testing.expect(!history.dirty());
    try std.testing.expect(!history.canUndo());
    try history.record(std.testing.allocator, .{ .set_value = .{ .before = 0, .after = 1 } }, 0);
    history.markClean(); // saved with the entry applied
    history.dropTop(std.testing.allocator);
    try std.testing.expect(history.dirty());
}

test "the clean mark follows the undo depth" {
    const H = History(TestCommand);
    var history: H = .{};
    defer history.deinit(std.testing.allocator);
    try std.testing.expect(!history.dirty());
    try history.record(std.testing.allocator, .{ .set_value = .{ .before = 0, .after = 1 } }, 0);
    try std.testing.expect(history.dirty());
    history.markClean();
    try std.testing.expect(!history.dirty());
    history.touchTop(std.testing.allocator);
    try std.testing.expect(history.dirty());
}

test "the revision moves with every change of the document and with nothing else" {
    const H = History(TestCommand);
    var history: H = .{};
    defer history.deinit(std.testing.allocator);
    const start = history.revision;
    try history.record(std.testing.allocator, .{ .set_value = .{ .before = 0, .after = 1 } }, 0);
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
