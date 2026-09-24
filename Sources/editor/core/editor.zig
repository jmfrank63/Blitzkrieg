const std = @import("std");
const bridge_mod = @import("bridge.zig");
const fake_mod = @import("fake_bridge.zig");
const document_mod = @import("document.zig");
const history_mod = @import("history.zig");
const tools = @import("tools.zig");
const Bridge = bridge_mod.Bridge;
const EditError = bridge_mod.EditError;
const ObjectRecord = bridge_mod.ObjectRecord;
const PaintCell = bridge_mod.PaintCell;
const FakeBridge = fake_mod.FakeBridge;
const Document = document_mod.Document;
pub const Pose = history_mod.Pose;
const Command = history_mod.Command;

/// The core's one entry point for the app: every edit goes through here, so
/// the bridge, the document and the history never disagree.
pub const Editor = struct {
    allocator: std.mem.Allocator,
    bridge: Bridge,
    document: Document = .{},
    status_buffer: [256]u8 = undefined,
    status_len: usize = 0,
    history: history_mod.History = .{},
    selection: ?i32 = null,
    next_gesture: u32 = 1,

    pub fn init(allocator: std.mem.Allocator, b: Bridge) Editor {
        return .{ .allocator = allocator, .bridge = b };
    }

    pub fn deinit(self: *Editor) void {
        self.document.deinit(self.allocator);
        self.history.deinit(self.allocator);
        self.* = undefined;
    }

    /// The status bar's line: the bridge's reason for the last refusal or
    /// failure, empty after a success.
    pub fn status(self: *const Editor) []const u8 {
        return self.status_buffer[0..self.status_len];
    }

    fn noteOutcome(self: *Editor, status_code: bridge_mod.Status) EditError!void {
        if (status_code == .ok) {
            self.status_len = 0;
            return;
        }
        const message = self.bridge.lastMessage();
        const len = @min(message.len, self.status_buffer.len);
        @memcpy(self.status_buffer[0..len], message[0..len]);
        self.status_len = len;
        return bridge_mod.check(status_code);
    }

    /// Copies `prefix` then `message` into the status buffer, truncating
    /// `message` (never `prefix`) to what is left of the buffer.
    fn setStatus(self: *Editor, prefix: []const u8, message: []const u8) void {
        const prefix_len = @min(prefix.len, self.status_buffer.len);
        @memcpy(self.status_buffer[0..prefix_len], prefix[0..prefix_len]);
        const message_len = @min(message.len, self.status_buffer.len - prefix_len);
        @memcpy(self.status_buffer[prefix_len..][0..message_len], message[0..message_len]);
        self.status_len = prefix_len + message_len;
    }

    /// A failed `openMap` itself keeps the map that was open, as the bridge
    /// keeps it. But if `openMap` succeeds and the object or diplomacy
    /// listing then fails, the bridge no longer holds the old map either, so
    /// the document is emptied rather than left holding stale link IDs that
    /// edits could reach the wrong objects through.
    pub fn open(self: *Editor, path: []const u8) EditError!void {
        var info: bridge_mod.MapInfo = .{};
        try self.noteOutcome(self.bridge.openMap(path, &info));
        self.document.reload(self.allocator, self.bridge, path, info) catch |err| {
            self.setStatus("the map opened but its objects could not be read: ", self.bridge.lastMessage());
            self.document.deinit(self.allocator);
            self.document = .{};
            self.history.clear(self.allocator);
            self.selection = null;
            return err;
        };
        self.history.clear(self.allocator);
        self.selection = null;
    }

    pub fn save(self: *Editor, path: []const u8) EditError!void {
        try self.noteOutcome(self.bridge.saveMap(path));
        self.document.path.clearRetainingCapacity();
        try self.document.path.appendSlice(self.allocator, path);
        self.history.markClean();
    }

    pub fn dirty(self: *const Editor) bool {
        return self.history.dirty();
    }

    /// A fresh key for one press-drag-release. Never 0, which means "never merge".
    pub fn beginGesture(self: *Editor) u32 {
        const gesture = self.next_gesture;
        self.next_gesture +%= 1;
        if (self.next_gesture == 0) self.next_gesture = 1;
        return gesture;
    }

    /// A screen point as the tools want it: the world point, its tile, and the
    /// object under it. Refused when the point is off the terrain; a point on
    /// the terrain but past the map's edge has no tile, and a point over no
    /// object has no object - neither is an error.
    pub fn resolve(self: *Editor, sx: f32, sy: f32) EditError!tools.Pointer {
        var pointer: tools.Pointer = .{ .world_x = 0, .world_y = 0 };
        try bridge_mod.check(self.bridge.screenToWorld(sx, sy, &pointer.world_x, &pointer.world_y));
        var tx: i32 = 0;
        var ty: i32 = 0;
        if (self.bridge.worldToTile(pointer.world_x, pointer.world_y, &tx, &ty) == .ok) pointer.tile = .{ tx, ty };
        var link_id: i32 = -1;
        if (self.bridge.objectAt(sx, sy, &link_id) == .ok) pointer.object = link_id;
        return pointer;
    }

    fn mergeable(self: *Editor, gesture: u32, tag: std.meta.Tag(Command)) ?*history_mod.Entry {
        if (gesture == 0) return null;
        const entry = self.history.top() orelse return null;
        if (entry.gesture != gesture or std.meta.activeTag(entry.command) != tag) return null;
        return entry;
    }

    /// Reserves whatever a fresh (non-merged) paint entry will need before
    /// the bridge call: once the bridge has painted, recording it must not
    /// be able to fail.
    pub fn paint(self: *Editor, cells: []const PaintCell, gesture: u32) EditError!void {
        if (cells.len == 0) return;
        if (self.mergeable(gesture, .paint)) |entry| {
            try entry.command.paint.tokens.ensureUnusedCapacity(self.allocator, 1);
            var token: i32 = -1;
            try self.noteOutcome(self.bridge.paint(cells, &token));
            entry.command.paint.tokens.appendAssumeCapacity(token);
            self.history.touchTop(self.allocator);
            return;
        }
        try self.history.reserve(self.allocator);
        var tokens: std.ArrayListUnmanaged(i32) = .empty;
        try tokens.ensureUnusedCapacity(self.allocator, 1);
        var token: i32 = -1;
        self.noteOutcome(self.bridge.paint(cells, &token)) catch |err| {
            tokens.deinit(self.allocator);
            return err;
        };
        tokens.appendAssumeCapacity(token);
        self.history.recordAssumeCapacity(self.allocator, .{ .paint = .{ .tokens = tokens } }, gesture);
    }

    pub fn addObject(self: *Editor, name: []const u8, x: f32, y: f32, dir: i32, player: i32) EditError!i32 {
        try self.history.reserve(self.allocator);
        try self.document.objects.ensureUnusedCapacity(self.allocator, 1);
        var link_id: i32 = -1;
        try self.noteOutcome(self.bridge.addObject(name, x, y, dir, player, &link_id));
        var object: ObjectRecord = .{ .link_id = link_id, .x = x, .y = y, .dir = dir, .player = player };
        object.setName(name);
        const index = self.document.objects.items.len;
        self.document.objects.appendAssumeCapacity(object);
        self.history.recordAssumeCapacity(self.allocator, .{ .add = .{ .object = object, .index = index } }, 0);
        return link_id;
    }

    pub fn place(self: *Editor, link_id: i32, pose: Pose, gesture: u32) EditError!void {
        const object = self.document.find(link_id) orelse return error.Failed;
        const before: Pose = .{ .x = object.x, .y = object.y, .dir = object.dir, .player = object.player };
        if (std.meta.eql(before, pose)) return;
        const merge_entry = self.mergeable(gesture, .place);
        const merging = if (merge_entry) |entry| entry.command.place.link_id == link_id else false;
        if (!merging) try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.placeObject(link_id, pose.x, pose.y, pose.dir, pose.player));
        applyPose(object, pose);
        if (merging) {
            merge_entry.?.command.place.after = pose;
            self.history.touchTop(self.allocator);
        } else {
            self.history.recordAssumeCapacity(self.allocator, .{ .place = .{ .link_id = link_id, .before = before, .after = pose } }, gesture);
        }
    }

    pub fn delete(self: *Editor, link_id: i32) EditError!void {
        const index = self.document.indexOf(link_id) orelse return error.Failed;
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.deleteObject(link_id));
        const object = self.document.objects.orderedRemove(index);
        if (self.selection == link_id) self.selection = null;
        self.history.recordAssumeCapacity(self.allocator, .{ .delete = .{ .object = object, .index = index } }, 0);
    }

    pub fn setDiplomacy(self: *Editor, player: i32, value: i32) EditError!void {
        if (player < 0 or @as(usize, @intCast(player)) >= self.document.diplomacy.items.len) return error.Failed;
        const slot = &self.document.diplomacy.items[@intCast(player)];
        if (slot.* == value) return;
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.setDiplomacy(player, value));
        const before = slot.*;
        slot.* = value;
        self.history.recordAssumeCapacity(self.allocator, .{ .diplomacy = .{ .player = player, .before = before, .after = value } }, 0);
    }

    pub fn setMapType(self: *Editor, value: i32) EditError!void {
        const before = self.document.info.map_type;
        if (before == value) return;
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.setMapType(value));
        self.document.info.map_type = value;
        self.history.recordAssumeCapacity(self.allocator, .{ .map_type = .{ .before = before, .after = value } }, 0);
    }

    pub fn setAttackingSide(self: *Editor, value: i32) EditError!void {
        const before = self.document.info.attacking_side;
        if (before == value) return;
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.setAttackingSide(value));
        self.document.info.attacking_side = value;
        self.history.recordAssumeCapacity(self.allocator, .{ .attacking_side = .{ .before = before, .after = value } }, 0);
    }

    fn applyPose(object: *ObjectRecord, pose: Pose) void {
        object.x = pose.x;
        object.y = pose.y;
        object.dir = pose.dir;
        object.player = pose.player;
    }

    /// Runs a command backwards (`forwards` false) or forwards again. The
    /// bridge keeps its own order, so every call here is one it expects; a
    /// refusal means the two have drifted, which is a failure, not a note.
    fn replay(self: *Editor, command: *Command, forwards: bool) EditError!void {
        switch (command.*) {
            .paint => |p| {
                if (forwards) {
                    for (p.tokens.items) |token| try self.noteOutcome(self.bridge.redoPaint(token));
                } else {
                    var index = p.tokens.items.len;
                    while (index != 0) {
                        index -= 1;
                        try self.noteOutcome(self.bridge.undoPaint(p.tokens.items[index]));
                    }
                }
            },
            .add => |a| if (forwards) try self.restoreInto(a.object, a.index) else try self.removeFrom(a.object.link_id),
            .delete => |d| if (forwards) try self.removeFrom(d.object.link_id) else try self.restoreInto(d.object, d.index),
            .place => |p| {
                const pose = if (forwards) p.after else p.before;
                try self.noteOutcome(self.bridge.placeObject(p.link_id, pose.x, pose.y, pose.dir, pose.player));
                applyPose(self.document.find(p.link_id) orelse return error.Failed, pose);
            },
            .diplomacy => |d| {
                const value = if (forwards) d.after else d.before;
                try self.noteOutcome(self.bridge.setDiplomacy(d.player, value));
                self.document.diplomacy.items[@intCast(d.player)] = value;
            },
            .map_type => |m| {
                const value = if (forwards) m.after else m.before;
                try self.noteOutcome(self.bridge.setMapType(value));
                self.document.info.map_type = value;
            },
            .attacking_side => |s| {
                const value = if (forwards) s.after else s.before;
                try self.noteOutcome(self.bridge.setAttackingSide(value));
                self.document.info.attacking_side = value;
            },
        }
    }

    fn removeFrom(self: *Editor, link_id: i32) EditError!void {
        const index = self.document.indexOf(link_id) orelse return error.Failed;
        try self.noteOutcome(self.bridge.deleteObject(link_id));
        _ = self.document.objects.orderedRemove(index);
        if (self.selection == link_id) self.selection = null;
    }

    fn restoreInto(self: *Editor, object: ObjectRecord, index: usize) EditError!void {
        try self.noteOutcome(self.bridge.restoreObject(object.link_id));
        try self.document.objects.insert(self.allocator, @min(index, self.document.objects.items.len), object);
    }

    fn drifted(err: EditError) EditError {
        return if (err == error.Refused) error.Failed else err;
    }

    /// False when there is nothing to undo. On a failure the entry stays
    /// where it was and the status line says why; the map should be reopened.
    pub fn undo(self: *Editor) EditError!bool {
        const count = self.history.undo_stack.items.len;
        if (count == 0) return false;
        // Room first: once the bridge has undone it, the entry must not be
        // lost to an allocation failure.
        try self.history.redo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.undo_stack.items[count - 1];
        self.replay(&entry.command, false) catch |err| return drifted(err);
        _ = self.history.undo_stack.pop();
        self.history.redo_stack.appendAssumeCapacity(entry);
        return true;
    }

    pub fn redo(self: *Editor) EditError!bool {
        const count = self.history.redo_stack.items.len;
        if (count == 0) return false;
        try self.history.undo_stack.ensureUnusedCapacity(self.allocator, 1);
        var entry = self.history.redo_stack.items[count - 1];
        self.replay(&entry.command, true) catch |err| return drifted(err);
        _ = self.history.redo_stack.pop();
        self.history.undo_stack.appendAssumeCapacity(entry);
        return true;
    }
};

/// The same fixture fake_bridge.zig builds for its own tests, exposed here
/// under the name later tasks call it by.
pub const testFixture = fake_mod.fixture;

test "open fills the document from the bridge" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expectEqual(@as(i32, 8), editor.document.info.width_tiles);
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
    try std.testing.expectEqualStrings("T34", editor.document.find(1).?.nameSlice());
    try std.testing.expect(!editor.document.find(3).?.known);
    try std.testing.expectEqualSlices(i32, &.{ 0, 1 }, editor.document.diplomacy.items);
}

test "a failed open keeps the map that was open" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try std.testing.expectError(error.Failed, editor.open("missing.bzm"));
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expectEqualStrings("no such map", editor.status());
}

test "a failed listing after open empties the document and says why" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    fake.fail_objects = true;
    try std.testing.expectError(error.Failed, editor.open("fixture.bzm"));
    try std.testing.expect(std.mem.startsWith(u8, editor.status(), "the map opened but its objects could not be read: "));
    try std.testing.expect(std.mem.endsWith(u8, editor.status(), "the object listing failed"));
    try std.testing.expectEqual(@as(usize, 0), editor.document.path.items.len);
    try std.testing.expectEqual(@as(usize, 0), editor.document.objects.items.len);
    try std.testing.expectEqual(@as(usize, 0), editor.document.diplomacy.items.len);
    try std.testing.expectEqual(@as(i32, 0), editor.document.info.width_tiles);
}

test "save moves the document to the saved path" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try editor.save("renamed.bzm");
    try std.testing.expectEqualStrings("renamed.bzm", editor.document.path.items);
}

fn openFixture(fake: *FakeBridge) !Editor {
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    errdefer editor.deinit();
    try editor.open("fixture.bzm");
    return editor;
}

test "add, undo, redo keeps one link ID and the document in step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const link = try editor.addObject("T34", 60, 60, 0, 1);
    try std.testing.expect(editor.dirty());
    try std.testing.expect(try editor.undo());
    try std.testing.expect(editor.document.find(link) == null);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(i32, 1), editor.document.find(link).?.player);
    try std.testing.expect(fake.calls.items[fake.calls.items.len - 1].kind == .restore);
}

test "delete then undo restores the same object, player and link ID" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const before = editor.document.find(1).?.*;
    try editor.delete(1);
    try std.testing.expect(editor.document.find(1) == null);
    try std.testing.expect(try editor.undo());
    const after = editor.document.find(1).?.*;
    try std.testing.expectEqual(before.player, after.player);
    try std.testing.expectEqual(@as(usize, 0), editor.document.indexOf(1).?);
    try std.testing.expect(try editor.redo());
    try std.testing.expect(editor.document.find(1) == null);
}

test "a refused delete leaves the document and the history unchanged" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectError(error.Refused, editor.delete(2));
    try std.testing.expectEqualStrings("still referred to by bridge 0", editor.status());
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(!(try editor.undo()));
}

test "place, undo, redo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.place(1, .{ .x = 70, .y = 80, .dir = 4096, .player = 1 }, 0);
    try std.testing.expectEqual(@as(f32, 70), editor.document.find(1).?.x);
    _ = try editor.undo();
    try std.testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
    try std.testing.expectEqual(@as(i32, 0), editor.document.find(1).?.dir);
    _ = try editor.redo();
    try std.testing.expectEqual(@as(i32, 4096), editor.document.find(1).?.dir);
    try std.testing.expectError(error.Refused, editor.place(1, .{ .x = -1, .y = 0, .dir = 0, .player = 0 }, 0));
    try std.testing.expectEqual(@as(f32, 70), editor.document.find(1).?.x);
}

test "one gesture of moves is one undo step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    try editor.place(1, .{ .x = 50, .y = 40, .dir = 0, .player = 0 }, gesture);
    try editor.place(1, .{ .x = 60, .y = 40, .dir = 0, .player = 0 }, gesture);
    try editor.place(1, .{ .x = 70, .y = 40, .dir = 0, .player = 0 }, gesture);
    _ = try editor.undo();
    try std.testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
    try std.testing.expect(!(try editor.undo()));
}

test "one gesture of paints is one undo step, undone newest first" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    try editor.paint(&.{.{ .x = 1, .y = 1, .tile = 4 }}, gesture);
    try editor.paint(&.{.{ .x = 2, .y = 1, .tile = 4 }}, gesture);
    try editor.paint(&.{.{ .x = 3, .y = 1, .tile = 9 }}, editor.beginGesture());
    _ = try editor.undo();
    try std.testing.expectEqual(@as(u8, 0), fake.tile(3, 1));
    try std.testing.expectEqual(@as(u8, 4), fake.tile(2, 1));
    _ = try editor.undo();
    try std.testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    try std.testing.expectEqual(@as(u8, 0), fake.tile(2, 1));
    _ = try editor.redo();
    try std.testing.expectEqual(@as(u8, 4), fake.tile(2, 1));
}

test "diplomacy, map type and attacking side undo and redo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setDiplomacy(1, 2);
    try editor.setMapType(3);
    try editor.setAttackingSide(1);
    try std.testing.expectEqual(@as(i32, 2), editor.document.diplomacy.items[1]);
    _ = try editor.undo();
    _ = try editor.undo();
    _ = try editor.undo();
    try std.testing.expectEqual(@as(i32, 1), editor.document.diplomacy.items[1]);
    try std.testing.expectEqual(@as(i32, 0), editor.document.info.map_type);
    try std.testing.expectEqual(@as(i32, 0), editor.document.info.attacking_side);
    _ = try editor.redo();
    try std.testing.expectEqual(@as(i32, 2), editor.document.diplomacy.items[1]);
}

test "saving marks clean, and undoing past the save makes it dirty again" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setMapType(3);
    try editor.save("fixture.bzm");
    try std.testing.expect(!editor.dirty());
    _ = try editor.undo();
    try std.testing.expect(editor.dirty());
    _ = try editor.redo();
    try std.testing.expect(!editor.dirty());
    _ = try editor.undo();
    try editor.setMapType(5); // the saved state is no longer reachable
    try std.testing.expect(editor.dirty());
}

test "a new edit drops the redo branch" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setMapType(3);
    _ = try editor.undo();
    try editor.setAttackingSide(1);
    try std.testing.expect(!(try editor.redo()));
}

test "a merge drops the redo branch too" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    try editor.paint(&.{.{ .x = 1, .y = 1, .tile = 4 }}, gesture);
    try editor.place(1, .{ .x = 50, .y = 40, .dir = 0, .player = 0 }, gesture);
    _ = try editor.undo(); // undoes the place; the place entry is now on the redo branch
    try editor.paint(&.{.{ .x = 2, .y = 1, .tile = 4 }}, gesture); // merges into the paint entry
    try std.testing.expect(!(try editor.redo()));
}

test "an allocation failure before the bridge call leaves the bridge, document and history untouched" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var failing = std.testing.FailingAllocator.init(std.testing.allocator, .{ .fail_index = 0 });
    editor.allocator = failing.allocator();
    try std.testing.expectError(error.OutOfMemory, editor.addObject("T34", 60, 60, 0, 1));
    editor.allocator = std.testing.allocator;
    try std.testing.expect(fake.calls.items.len == 0 or fake.calls.items[fake.calls.items.len - 1].kind != .add);
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
    try std.testing.expect(!editor.dirty());
}
