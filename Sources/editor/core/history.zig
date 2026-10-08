//! MapEditor's own command vocabulary on top of the kit's generic save-mark +
//! stack primitive (`editor_kit.history`). The Command union, the deleted-
//! record payload for multi-delete, the Pose record and the EditScope live
//! here; the stack mechanics, the save mark, the revision counter and the
//! gesture-merge hook live in the kit so other editors (Resource, Mission)
//! can share them without pulling MapEditor's bridge records in.
const std = @import("std");
const kit_history = @import("editor_kit").history;
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

/// MapEditor's `Entry` is the kit's generic Entry over MapEditor's `Command`.
/// Callers that spelled `history_mod.Entry` keep the same path.
pub const Entry = kit_history.Entry(Command);

/// MapEditor's `History` is the kit's generic stack over MapEditor's
/// `Command`. The stack mechanics, save mark and revision counter all come
/// from the kit; MapEditor only brings the Command union.
pub const History = kit_history.History(Command);

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
