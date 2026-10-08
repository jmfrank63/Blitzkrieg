//! The GUI sub-editor's edits of an open game UI screen (kind gui) as
//! `ResourceCommand.gui` builders and their replay: set rects (a move, a
//! resize, an align, an equal size, one gesture whatever the number of
//! windows), insert a template, paste, delete and cut, and set an attribute.
//! A builder reads what the command needs from the bridge and returns it
//! unapplied; the caller records it through `sub_editor_tools.commit`, so
//! one gesture is one undo step with a redo.
//!
//! The bridge never reuses a window id, so an undone insert or delete comes
//! back under a new one. Commands therefore store LOGICAL ids (the id a
//! window had when the command first ran) and the document's `IdMap` says
//! which window each one is now; a builder takes the ids the app sees (the
//! bridge's own) and stores their logical form. An undo of a delete puts the
//! windows back at the end of their parent's children: the bridge's paste has
//! no position, so their drawing order among siblings may change.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const history_mod = @import("history.zig");
const geometry = @import("gui_geometry.zig");

const ResBridge = bridge_mod.ResBridge;
const EditError = bridge_mod.EditError;
const GuiWindow = bridge_mod.GuiWindow;
const GuiRect = bridge_mod.GuiRect;
const GuiIdChange = bridge_mod.GuiIdChange;
const ResourceCommand = history_mod.ResourceCommand;
const GuiCommand = history_mod.GuiCommand;
const GuiDeleted = history_mod.GuiDeleted;
const OwnedBytes = history_mod.OwnedBytes;

/// Which window each logical id is now. An id that is not in the map is
/// itself, so a screen nobody has undone through needs no entries.
pub const IdMap = struct {
    map: std.AutoHashMapUnmanaged(i32, i32) = .empty,

    pub fn deinit(self: *IdMap, allocator: std.mem.Allocator) void {
        self.map.deinit(allocator);
    }

    /// Forgets every mapping; for a screen that was just opened or closed.
    pub fn clear(self: *IdMap) void {
        self.map.clearRetainingCapacity();
    }

    /// The bridge's id of the window a command calls `logical_id`.
    pub fn actual(self: *const IdMap, logical_id: i32) i32 {
        return self.map.get(logical_id) orelse logical_id;
    }

    /// The logical id of the window the bridge calls `actual_id`.
    pub fn logical(self: *const IdMap, actual_id: i32) i32 {
        var it = self.map.iterator();
        while (it.next()) |entry| {
            if (entry.value_ptr.* == actual_id) return entry.key_ptr.*;
        }
        return actual_id;
    }

    fn set(self: *IdMap, allocator: std.mem.Allocator, logical_id: i32, actual_id: i32) !void {
        try self.map.put(allocator, logical_id, actual_id);
    }
};

/// How many outermost windows one paste can report. The bridge answers a
/// paste once, so the ids buffer cannot be sized by a first pass.
const max_pasted = 4096;

/// Every window of the open screen, in document order.
pub fn readWindows(allocator: std.mem.Allocator, bridge: ResBridge) EditError![]GuiWindow {
    var total: usize = 0;
    var none: [0]GuiWindow = .{};
    const sizing = bridge.guiWindows(&none, &total);
    if (sizing != .ok and sizing != .refused) try bridge_mod.check(sizing);
    const out = try allocator.alloc(GuiWindow, total);
    errdefer allocator.free(out);
    try bridge_mod.check(bridge.guiWindows(out, &total));
    if (total > out.len) return error.Failed;
    return allocator.realloc(out, total) catch out[0..total];
}

/// The clipboard text of the outermost of `ids` (bridge ids).
pub fn copyText(allocator: std.mem.Allocator, bridge: ResBridge, ids: []const i32) EditError!OwnedBytes {
    var size: usize = 0;
    var none: [0]u8 = .{};
    const sizing = bridge.guiCopy(ids, &none, &size);
    if (sizing != .ok and (sizing != .refused or size == 0)) try bridge_mod.check(sizing);
    const buffer = try allocator.alloc(u8, size);
    errdefer allocator.free(buffer);
    try bridge_mod.check(bridge.guiCopy(ids, buffer, &size));
    return .{ .bytes = buffer[0..@min(size, buffer.len)] };
}

/// Every window under or equal to one of `tops`, in document order.
fn subtrees(allocator: std.mem.Allocator, windows: []const GuiWindow, tops: []const i32) std.mem.Allocator.Error![]i32 {
    var out: std.ArrayListUnmanaged(i32) = .empty;
    errdefer out.deinit(allocator);
    for (windows) |w| {
        var at = w.id;
        var depth: usize = 0;
        while (at >= 0 and depth < 64) : (depth += 1) {
            if (std.mem.indexOfScalar(i32, tops, at) != null) {
                try out.append(allocator, w.id);
                break;
            }
            const next = geometry.find(windows, at) orelse break;
            at = next.parent;
        }
    }
    return out.toOwnedSlice(allocator);
}

fn actualIds(allocator: std.mem.Allocator, map: *const IdMap, logical_ids: []const i32) std.mem.Allocator.Error![]i32 {
    const out = try allocator.alloc(i32, logical_ids.len);
    for (logical_ids, out) |l, *a| a.* = map.actual(l);
    return out;
}

// --- Builders -----------------------------------------------------------------

/// A gesture's rects as one command, or null when no window changes. `rects`
/// carry the bridge's ids and are the new state (what `gui_geometry`'s
/// `moveBy`, `align_` and `equalize` return); the before side is read here.
pub fn setRects(allocator: std.mem.Allocator, bridge: ResBridge, map: *const IdMap, rects: []const GuiRect) EditError!?ResourceCommand {
    const windows = try readWindows(allocator, bridge);
    defer allocator.free(windows);
    var before: std.ArrayListUnmanaged(GuiRect) = .empty;
    errdefer before.deinit(allocator);
    var after: std.ArrayListUnmanaged(GuiRect) = .empty;
    errdefer after.deinit(allocator);
    for (rects) |r| {
        const w = geometry.find(windows, r.id) orelse return error.BadArgument;
        if (w.flag == r.flag and w.x == r.x and w.y == r.y and w.w == r.w and w.h == r.h) continue;
        const logical = map.logical(r.id);
        try before.append(allocator, .{ .id = logical, .flag = w.flag, .x = w.x, .y = w.y, .w = w.w, .h = w.h });
        var changed = r;
        changed.id = logical;
        try after.append(allocator, changed);
    }
    if (before.items.len == 0) return null;
    const before_owned = try before.toOwnedSlice(allocator);
    errdefer allocator.free(before_owned);
    const after_owned = try after.toOwnedSlice(allocator);
    return .{ .gui = .{ .set_rects = .{ .before = before_owned, .after = after_owned } } };
}

/// Inserts the window tree of the template file `template_path` under
/// `parent` with its top left at (`x`, `y`).
pub fn insertTemplate(allocator: std.mem.Allocator, map: *const IdMap, parent: i32, template_path: []const u8, x: i32, y: i32) EditError!ResourceCommand {
    return .{ .gui = .{ .create = .{
        .parent = map.logical(parent),
        .from_template = true,
        .source = try OwnedBytes.fromSlice(allocator, template_path),
        .x = x,
        .y = y,
    } } };
}

/// Pastes clipboard text under `parent`, each outermost window moved by
/// (`dx`, `dy`).
pub fn paste(allocator: std.mem.Allocator, map: *const IdMap, parent: i32, clipboard: []const u8, dx: i32, dy: i32) EditError!ResourceCommand {
    return .{ .gui = .{ .create = .{
        .parent = map.logical(parent),
        .from_template = false,
        .source = try OwnedBytes.fromSlice(allocator, clipboard),
        .x = dx,
        .y = dy,
    } } };
}

/// The clipboard text of `ids` for a copy; owned by the caller.
pub fn copy(allocator: std.mem.Allocator, bridge: ResBridge, ids: []const i32) EditError!OwnedBytes {
    return copyText(allocator, bridge, ids);
}

/// Deletes `ids` and their subtrees. The root is refused; a window under
/// another selected one goes with it and is not stored twice.
pub fn delete(allocator: std.mem.Allocator, bridge: ResBridge, map: *const IdMap, ids: []const i32) EditError!ResourceCommand {
    const windows = try readWindows(allocator, bridge);
    defer allocator.free(windows);
    var items: std.ArrayListUnmanaged(GuiDeleted) = .empty;
    errdefer {
        for (items.items) |*item| item.deinit(allocator);
        items.deinit(allocator);
    }
    for (ids) |id| {
        const w = geometry.find(windows, id) orelse return error.BadArgument;
        if (w.parent < 0) return error.Refused;
        var nested = false;
        for (ids) |other| {
            if (other != id and isUnder(windows, id, other)) nested = true;
        }
        if (nested) continue;
        var text = try copyText(allocator, bridge, &.{id});
        errdefer text.deinit(allocator);
        const under = try subtrees(allocator, windows, &.{id});
        defer allocator.free(under);
        const subtree = try allocator.alloc(i32, under.len);
        errdefer allocator.free(subtree);
        for (under, subtree) |a, *l| l.* = map.logical(a);
        try items.append(allocator, .{ .id = map.logical(id), .parent = map.logical(w.parent), .text = text, .subtree = subtree });
    }
    if (items.items.len == 0) return error.BadArgument;
    return .{ .gui = .{ .delete = .{ .items = try items.toOwnedSlice(allocator) } } };
}

fn isUnder(windows: []const GuiWindow, id: i32, ancestor: i32) bool {
    var at = geometry.find(windows, id) orelse return false;
    var depth: usize = 0;
    while (at.parent >= 0 and depth < 64) : (depth += 1) {
        if (at.parent == ancestor) return true;
        at = geometry.find(windows, at.parent) orelse return false;
    }
    return false;
}

/// A cut: the clipboard text and the delete that removes the windows.
pub const Cut = struct {
    clipboard: OwnedBytes,
    command: ResourceCommand,
};

pub fn cut(allocator: std.mem.Allocator, bridge: ResBridge, map: *const IdMap, ids: []const i32) EditError!Cut {
    var command = try delete(allocator, bridge, map, ids);
    errdefer command.deinit(allocator);
    return .{ .clipboard = try copyText(allocator, bridge, ids), .command = command };
}

/// Sets one XML attribute of window `id`.
pub fn setAttr(allocator: std.mem.Allocator, bridge: ResBridge, map: *const IdMap, id: i32, name: []const u8, value: []const u8) EditError!ResourceCommand {
    var size: usize = 0;
    var none: [0]u8 = .{};
    var had_before = true;
    var before: OwnedBytes = .{};
    errdefer before.deinit(allocator);
    const sizing = bridge.guiGetAttr(id, name, &none, &size);
    switch (sizing) {
        .ok, .refused => {
            if (sizing == .refused and size == 0) return error.Refused;
            const buffer = try allocator.alloc(u8, size);
            errdefer allocator.free(buffer);
            try bridge_mod.check(bridge.guiGetAttr(id, name, buffer, &size));
            before = .{ .bytes = buffer[0..@min(size, buffer.len)] };
        },
        .data_missing => had_before = false,
        else => try bridge_mod.check(sizing),
    }
    var name_owned = try OwnedBytes.fromSlice(allocator, name);
    errdefer name_owned.deinit(allocator);
    const after = try OwnedBytes.fromSlice(allocator, value);
    return .{ .gui = .{ .set_attr = .{ .id = map.logical(id), .name = name_owned, .had_before = had_before, .before = before, .after = after } } };
}

// --- Replay -------------------------------------------------------------------

fn writeRects(allocator: std.mem.Allocator, bridge: ResBridge, map: *const IdMap, rects: []const GuiRect) EditError!void {
    const actual = try allocator.alloc(GuiRect, rects.len);
    defer allocator.free(actual);
    for (rects, actual) |r, *a| {
        a.* = r;
        a.id = map.actual(r.id);
    }
    try bridge_mod.check(bridge.guiSetRects(actual));
}

/// The windows a paste made, as `ids` in document order, and the logical
/// ids they stand for: each new window takes over the logical id of the one
/// at its place in `logical_ids`.
fn adopt(allocator: std.mem.Allocator, bridge: ResBridge, map: *IdMap, new_tops: []const i32, logical_ids: []const i32) EditError!void {
    const windows = try readWindows(allocator, bridge);
    defer allocator.free(windows);
    const made = try subtrees(allocator, windows, new_tops);
    defer allocator.free(made);
    if (made.len != logical_ids.len) return error.Failed;
    for (logical_ids, made) |l, a| try map.set(allocator, l, a);
}

/// Pastes `text` and returns the outermost new windows. Only a user's paste
/// (`changes` given) makes the ElementIDs unique; the replays (an undone
/// delete, a redone paste) put back exactly the text they kept.
fn pasteText(allocator: std.mem.Allocator, bridge: ResBridge, parent: i32, text: []const u8, dx: i32, dy: i32, changes: ?*[]GuiIdChange) EditError![]i32 {
    var buffer: [max_pasted]i32 = undefined;
    var total: usize = 0;
    var change_buffer: [max_pasted]GuiIdChange = undefined;
    var changed: usize = 0;
    try bridge_mod.check(bridge.guiPaste(parent, text, dx, dy, changes != null, &buffer, &total, &change_buffer, &changed));
    if (total > buffer.len or changed > change_buffer.len) return error.Failed;
    const tops = try allocator.dupe(i32, buffer[0..total]);
    errdefer allocator.free(tops);
    if (changes) |out| out.* = try allocator.dupe(GuiIdChange, change_buffer[0..changed]);
    return tops;
}

/// Runs the command forwards: the first time it is applied, and as a redo.
pub fn apply(allocator: std.mem.Allocator, bridge: ResBridge, map: *IdMap, command: *GuiCommand) EditError!void {
    switch (command.*) {
        .set_rects => |c| try writeRects(allocator, bridge, map, c.after),
        .create => |*c| try applyCreate(allocator, bridge, map, c),
        .delete => |c| {
            const tops = try allocator.alloc(i32, c.items.len);
            defer allocator.free(tops);
            for (c.items, tops) |item, *t| t.* = map.actual(item.id);
            try bridge_mod.check(bridge.guiDelete(tops));
        },
        .set_attr => |c| try bridge_mod.check(bridge.guiSetAttr(map.actual(c.id), c.name.bytes, c.after.bytes)),
    }
}

fn applyCreate(allocator: std.mem.Allocator, bridge: ResBridge, map: *IdMap, c: anytype) EditError!void {
    const parent = map.actual(c.parent);
    if (c.ids.len == 0) {
        // The first run: the new windows are known by the ids they got.
        var tops: []i32 = undefined;
        if (c.from_template) {
            var id: i32 = -1;
            try bridge_mod.check(bridge.guiInsertTemplate(parent, c.source.bytes, c.x, c.y, &id));
            tops = try allocator.dupe(i32, &.{id});
        } else {
            if (c.id_changes.len != 0) allocator.free(c.id_changes);
            c.id_changes = &.{};
            tops = try pasteText(allocator, bridge, parent, c.source.bytes, c.x, c.y, &c.id_changes);
        }
        errdefer allocator.free(tops);
        const windows = try readWindows(allocator, bridge);
        defer allocator.free(windows);
        const ids = try subtrees(allocator, windows, tops);
        c.tops = tops;
        c.ids = ids;
        return;
    }
    // A redo: the bridge cannot insert at an id, so the windows come back from
    // the text the undo kept and take over their logical ids.
    const new_tops = try pasteText(allocator, bridge, parent, c.undo_text.bytes, 0, 0, null);
    defer allocator.free(new_tops);
    try adopt(allocator, bridge, map, new_tops, c.ids);
}

/// Runs the command backwards.
pub fn undo(allocator: std.mem.Allocator, bridge: ResBridge, map: *IdMap, command: *GuiCommand) EditError!void {
    switch (command.*) {
        .set_rects => |c| try writeRects(allocator, bridge, map, c.before),
        .create => |*c| {
            const tops = try actualIds(allocator, map, c.tops);
            defer allocator.free(tops);
            const text = try copyText(allocator, bridge, tops);
            try bridge_mod.check(bridge.guiDelete(tops));
            c.undo_text.deinit(allocator);
            c.undo_text = text;
        },
        .delete => |c| {
            for (c.items) |item| {
                const new_tops = try pasteText(allocator, bridge, map.actual(item.parent), item.text.bytes, 0, 0, null);
                defer allocator.free(new_tops);
                try adopt(allocator, bridge, map, new_tops, item.subtree);
            }
        },
        .set_attr => |c| try bridge_mod.check(bridge.guiSetAttr(map.actual(c.id), c.name.bytes, if (c.had_before) c.before.bytes else "")),
    }
}

/// Runs the command forwards again after an undo.
pub fn redo(allocator: std.mem.Allocator, bridge: ResBridge, map: *IdMap, command: *GuiCommand) EditError!void {
    return apply(allocator, bridge, map, command);
}

// --- Tests ------------------------------------------------------------------

const testing = std.testing;
const fake_bridge = @import("fake_bridge.zig");
const document_mod = @import("document.zig");
const tools = @import("sub_editor_tools.zig");
const FakeResBridge = fake_bridge.FakeResBridge;
const Document = document_mod.Document;
const History = history_mod.History;

const button_template = "data/editor/ui/Buttons/Button.xml";

const Fixture = struct {
    fake: FakeResBridge,
    doc: Document = .{},
    hist: History = .{},

    fn init(self: *Fixture, allocator: std.mem.Allocator) !void {
        self.* = .{ .fake = FakeResBridge.init(allocator) };
        try bridge_mod.check(self.fake.bridge().new(.gui_frame));
        try self.fake.addGuiTemplate(button_template, 0x10001104, 120, 30);
        // The root is 800 x 600 in the fake. Window 1: left, top, (10, 20) 100 x 50.
        // Window 2: right, bottom, (30, 40) 60 x 30. Window 3 is window 1's child,
        // middle, middle, (0, 0) 20 x 10.
        try self.fake.gui_windows.append(allocator, .{ .id = 1, .parent = 0, .class_type = 7, .element_id = 11, .flag = 0x11, .x = 10, .y = 20, .w = 100, .h = 50, .visible = 1 });
        try self.fake.gui_windows.append(allocator, .{ .id = 2, .parent = 0, .class_type = 8, .element_id = 12, .flag = 0x33, .x = 30, .y = 40, .w = 60, .h = 30, .visible = 1 });
        try self.fake.gui_windows.append(allocator, .{ .id = 3, .parent = 1, .class_type = 9, .element_id = 13, .flag = 0x22, .x = 0, .y = 0, .w = 20, .h = 10, .visible = 1 });
        self.fake.gui_next_id = 4;
    }

    fn deinit(self: *Fixture, allocator: std.mem.Allocator) void {
        self.hist.deinit(allocator);
        self.doc.deinit(allocator);
        self.fake.deinit();
    }

    fn commit(self: *Fixture, allocator: std.mem.Allocator, command: ResourceCommand) !void {
        try tools.commit(allocator, self.fake.bridge(), &self.doc, &self.hist, command, 0);
    }

    fn undoAt(self: *Fixture, allocator: std.mem.Allocator, index: usize) !void {
        try self.doc.undoOne(allocator, self.fake.bridge(), &self.hist.undo_stack.items[index].command);
    }

    fn redoAt(self: *Fixture, allocator: std.mem.Allocator, index: usize) !void {
        try self.doc.redoOne(allocator, self.fake.bridge(), &self.hist.undo_stack.items[index].command);
    }

    fn window(self: *Fixture, id: i32) GuiWindow {
        return geometry.find(self.fake.gui_windows.items, id).?;
    }
};

/// What a window list holds apart from the ids, which an undone insert
/// changes: the parent's own rect stands for the parent.
fn shape(allocator: std.mem.Allocator, windows: []const GuiWindow) ![]const [8]i32 {
    const out = try allocator.alloc([8]i32, windows.len);
    for (windows, out) |w, *o| {
        const parent_x: i32 = if (geometry.find(windows, w.parent)) |p| p.x else -1;
        o.* = .{ parent_x, w.class_type, w.element_id, w.flag, w.x, w.y, w.w, w.h };
    }
    return out;
}

fn expectSameShape(want: []const [8]i32, windows: []const GuiWindow) !void {
    const got = try shape(testing.allocator, windows);
    defer testing.allocator.free(got);
    try testing.expectEqual(want.len, got.len);
    for (want, got) |w, g| try testing.expectEqualSlices(i32, &w, &g);
}

test "set rects: a move, an align and an equal size undo to the exact ints and redo to the edits" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);

    const original = try allocator.dupe(GuiWindow, fx.fake.gui_windows.items);
    defer allocator.free(original);

    // A move of windows 1 and 2 by (7, -3): window 2's far-edge ints go the other way.
    var windows = try readWindows(allocator, fx.fake.bridge());
    var edits = try geometry.moveBy(allocator, windows, &.{ 1, 2 }, 7, -3);
    allocator.free(windows);
    var command = (try setRects(allocator, fx.fake.bridge(), &fx.doc.gui_ids, edits.items)).?;
    edits.deinit(allocator);
    try fx.commit(allocator, command);
    try testing.expectEqual(@as(i32, 17), fx.window(1).x);
    try testing.expectEqual(@as(i32, 17), fx.window(1).y);
    try testing.expectEqual(@as(i32, 23), fx.window(2).x);
    try testing.expectEqual(@as(i32, 43), fx.window(2).y);
    const after_move = try allocator.dupe(GuiWindow, fx.fake.gui_windows.items);
    defer allocator.free(after_move);

    // Align window 2 to window 1's left edge, then give it window 1's size.
    windows = try readWindows(allocator, fx.fake.bridge());
    edits = try geometry.align_(allocator, windows, &.{ 1, 2 }, .left);
    allocator.free(windows);
    command = (try setRects(allocator, fx.fake.bridge(), &fx.doc.gui_ids, edits.items)).?;
    edits.deinit(allocator);
    try fx.commit(allocator, command);
    // Window 1's left is 17: window 2 (60 wide, right anchored) ends at 77, offset 800 - 77.
    try testing.expectEqual(@as(i32, 723), fx.window(2).x);

    windows = try readWindows(allocator, fx.fake.bridge());
    edits = try geometry.equalize(allocator, windows, &.{ 1, 2 }, .size);
    allocator.free(windows);
    command = (try setRects(allocator, fx.fake.bridge(), &fx.doc.gui_ids, edits.items)).?;
    edits.deinit(allocator);
    try fx.commit(allocator, command);
    try testing.expectEqual(@as(i32, 100), fx.window(2).w);
    try testing.expectEqual(@as(i32, 50), fx.window(2).h);
    const after_size = try allocator.dupe(GuiWindow, fx.fake.gui_windows.items);
    defer allocator.free(after_size);

    // Three gestures, three entries; undo each in turn and compare every int.
    try testing.expectEqual(@as(usize, 3), fx.hist.undo_stack.items.len);
    try fx.undoAt(allocator, 2);
    try fx.undoAt(allocator, 1);
    try testing.expectEqualSlices(GuiWindow, after_move, fx.fake.gui_windows.items);
    try fx.undoAt(allocator, 0);
    try testing.expectEqualSlices(GuiWindow, original, fx.fake.gui_windows.items);
    try fx.redoAt(allocator, 0);
    try testing.expectEqualSlices(GuiWindow, after_move, fx.fake.gui_windows.items);
    try fx.redoAt(allocator, 1);
    try fx.redoAt(allocator, 2);
    try testing.expectEqualSlices(GuiWindow, after_size, fx.fake.gui_windows.items);
}

test "set rects: an edit that changes nothing builds no command, an unknown window is refused" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);
    const same = [_]GuiRect{.{ .id = 1, .flag = 0x11, .x = 10, .y = 20, .w = 100, .h = 50 }};
    try testing.expect((try setRects(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &same)) == null);
    const unknown = [_]GuiRect{.{ .id = 99, .flag = 0x11, .x = 1, .y = 1, .w = 1, .h = 1 }};
    try testing.expectError(error.BadArgument, setRects(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &unknown));
}

test "insert template then undo then redo keeps the window list, and later edits still find the window" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);

    const before = try shape(allocator, fx.fake.gui_windows.items);
    defer allocator.free(before);

    try fx.commit(allocator, try insertTemplate(allocator, &fx.doc.gui_ids, 0, button_template, 300, 200));
    try testing.expectEqual(@as(usize, 5), fx.fake.gui_windows.items.len);
    const inserted = fx.fake.gui_windows.items[4];
    try testing.expectEqual(@as(i32, 4), inserted.id);
    try testing.expectEqual(@as(i32, 300), inserted.x);
    try testing.expectEqual(@as(i32, 120), inserted.w);
    const with_button = try shape(allocator, fx.fake.gui_windows.items);
    defer allocator.free(with_button);

    // A move of the new button, recorded as its own entry.
    const move = [_]GuiRect{.{ .id = 4, .flag = inserted.flag, .x = 310, .y = 205, .w = 120, .h = 30 }};
    try fx.commit(allocator, (try setRects(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &move)).?);

    try fx.undoAt(allocator, 1);
    try fx.undoAt(allocator, 0);
    try expectSameShape(before, fx.fake.gui_windows.items);

    // The redo brings the button back under a new id; the move still finds it.
    try fx.redoAt(allocator, 0);
    try testing.expectEqual(@as(usize, 5), fx.fake.gui_windows.items.len);
    const reborn = fx.fake.gui_windows.items[4];
    try testing.expect(reborn.id != 4);
    try testing.expectEqual(reborn.id, fx.doc.gui_ids.actual(4));
    try expectSameShape(with_button, fx.fake.gui_windows.items);
    try fx.redoAt(allocator, 1);
    try testing.expectEqual(@as(i32, 310), fx.window(reborn.id).x);
    try testing.expectEqual(@as(i32, 205), fx.window(reborn.id).y);

    // And the whole way back again, now through two remaps.
    try fx.undoAt(allocator, 1);
    try testing.expectEqual(@as(i32, 300), fx.window(reborn.id).x);
    try fx.undoAt(allocator, 0);
    try expectSameShape(before, fx.fake.gui_windows.items);
}

test "insert refuses a template the bridge cannot read and records nothing" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);
    const command = try insertTemplate(allocator, &fx.doc.gui_ids, 0, "data/editor/ui/Nope.xml", 0, 0);
    try testing.expectError(error.Failed, fx.commit(allocator, command));
    try testing.expectEqual(@as(usize, 0), fx.hist.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 4), fx.fake.gui_windows.items.len);
}

test "delete then undo restores the subtree, and redo and undo again work through the remap" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);

    // Window 1 and its child 3 go; window 2 stays. Put 1 and 3 last so the
    // restore, which appends, keeps the order.
    fx.fake.gui_windows.clearRetainingCapacity();
    try fx.fake.gui_windows.append(allocator, .{ .id = 0, .parent = -1, .class_type = 0, .element_id = 0, .flag = 0, .x = 0, .y = 0, .w = 800, .h = 600, .visible = 1 });
    try fx.fake.gui_windows.append(allocator, .{ .id = 2, .parent = 0, .class_type = 8, .element_id = 12, .flag = 0x33, .x = 30, .y = 40, .w = 60, .h = 30, .visible = 1 });
    try fx.fake.gui_windows.append(allocator, .{ .id = 1, .parent = 0, .class_type = 7, .element_id = 11, .flag = 0x11, .x = 10, .y = 20, .w = 100, .h = 50, .visible = 1 });
    try fx.fake.gui_windows.append(allocator, .{ .id = 3, .parent = 1, .class_type = 9, .element_id = 13, .flag = 0x22, .x = 0, .y = 0, .w = 20, .h = 10, .visible = 1 });

    const before = try shape(allocator, fx.fake.gui_windows.items);
    defer allocator.free(before);

    // Selecting the child as well as its parent stores the parent's subtree once.
    try fx.commit(allocator, try delete(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &.{ 1, 3 }));
    try testing.expectEqual(@as(usize, 2), fx.fake.gui_windows.items.len);
    try testing.expectEqual(@as(i32, 2), fx.fake.gui_windows.items[1].id);

    try fx.undoAt(allocator, 0);
    try expectSameShape(before, fx.fake.gui_windows.items);
    try testing.expectEqual(fx.fake.gui_windows.items[3].parent, fx.fake.gui_windows.items[2].id);

    try fx.redoAt(allocator, 0);
    try testing.expectEqual(@as(usize, 2), fx.fake.gui_windows.items.len);
    try fx.undoAt(allocator, 0);
    try expectSameShape(before, fx.fake.gui_windows.items);
}

test "delete refuses the root and an unknown window" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);
    try testing.expectError(error.Refused, delete(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &.{0}));
    try testing.expectError(error.BadArgument, delete(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &.{42}));
    try testing.expectError(error.BadArgument, delete(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &.{}));
}

test "cut and paste: the clipboard carries the windows, a paste is one undo step" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);

    // Copy window 2 and paste it 15 right and 25 down.
    var clip = try copy(allocator, fx.fake.bridge(), &.{2});
    defer clip.deinit(allocator);
    try testing.expect(clip.bytes.len != 0);
    try fx.commit(allocator, try paste(allocator, &fx.doc.gui_ids, 0, clip.bytes, 15, 25));
    try testing.expectEqual(@as(usize, 5), fx.fake.gui_windows.items.len);
    const pasted = fx.fake.gui_windows.items[4];
    try testing.expectEqual(@as(i32, 45), pasted.x);
    try testing.expectEqual(@as(i32, 65), pasted.y);
    try testing.expectEqual(@as(i32, 0x33), pasted.flag);

    try fx.undoAt(allocator, 0);
    try testing.expectEqual(@as(usize, 4), fx.fake.gui_windows.items.len);
    try fx.redoAt(allocator, 0);
    try testing.expectEqual(@as(usize, 5), fx.fake.gui_windows.items.len);
    try testing.expectEqual(@as(i32, 45), fx.fake.gui_windows.items[4].x);

    // A cut of window 2 leaves its clipboard and one delete step.
    var cut_result = try cut(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &.{2});
    defer cut_result.clipboard.deinit(allocator);
    try fx.commit(allocator, cut_result.command);
    try testing.expect(cut_result.clipboard.bytes.len != 0);
    try testing.expectEqual(@as(usize, 4), fx.fake.gui_windows.items.len);
    try fx.undoAt(allocator, 1);
    try testing.expectEqual(@as(usize, 5), fx.fake.gui_windows.items.len);

    // Pasting garbage is refused and records nothing.
    const bad = try paste(allocator, &fx.doc.gui_ids, 0, "not a window", 0, 0);
    try testing.expectError(error.Refused, fx.commit(allocator, bad));
    try testing.expectEqual(@as(usize, 2), fx.hist.undo_stack.items.len);
}

test "paste gives taken ElementIDs the next free ones, undo restores the screen exactly, redo keeps the new ids" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);
    const original = try allocator.dupe(GuiWindow, fx.fake.gui_windows.items);
    defer allocator.free(original);

    // Window 1 (ElementID 11) with its child 3 (13): 11 and 13 are taken, as
    // are 12 and, once the parent has it, 14.
    var clip = try copy(allocator, fx.fake.bridge(), &.{1});
    defer clip.deinit(allocator);
    try fx.commit(allocator, try paste(allocator, &fx.doc.gui_ids, 0, clip.bytes, 5, 5));
    try testing.expectEqual(@as(usize, 6), fx.fake.gui_windows.items.len);
    try testing.expectEqual(@as(i32, 14), fx.fake.gui_windows.items[4].element_id);
    try testing.expectEqual(@as(i32, 15), fx.fake.gui_windows.items[5].element_id);
    const changes = fx.hist.undo_stack.items[0].command.gui.create.id_changes;
    try testing.expectEqual(@as(usize, 2), changes.len);
    try testing.expectEqual(GuiIdChange{ .window = 4, .old = 11, .new = 14 }, changes[0]);
    try testing.expectEqual(GuiIdChange{ .window = 5, .old = 13, .new = 15 }, changes[1]);
    try testing.expectEqual(@as(i32, 11), fx.window(1).element_id);
    try testing.expectEqual(@as(i32, 13), fx.window(3).element_id);

    try fx.undoAt(allocator, 0);
    try testing.expectEqualSlices(GuiWindow, original, fx.fake.gui_windows.items);
    // The redo puts back the text the undo kept: the new ids, not a third pair.
    try fx.redoAt(allocator, 0);
    try testing.expectEqual(@as(usize, 6), fx.fake.gui_windows.items.len);
    try testing.expectEqual(@as(i32, 14), fx.fake.gui_windows.items[4].element_id);
    try testing.expectEqual(@as(i32, 15), fx.fake.gui_windows.items[5].element_id);
    try fx.undoAt(allocator, 0);
    try testing.expectEqualSlices(GuiWindow, original, fx.fake.gui_windows.items);
}

test "undo of a delete keeps an ElementID another window shares" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);
    // Shipped screens repeat ids across dialogs; window 2 shares window 1's 11.
    fx.fake.gui_windows.items[2].element_id = 11;
    try fx.commit(allocator, try delete(allocator, fx.fake.bridge(), &fx.doc.gui_ids, &.{2}));
    try fx.undoAt(allocator, 0);
    try testing.expectEqual(@as(i32, 11), fx.window(fx.doc.gui_ids.actual(2)).element_id);
}

test "set attribute undoes to the previous value, or to empty when there was none" {
    const allocator = testing.allocator;
    var fx: Fixture = undefined;
    try fx.init(allocator);
    defer fx.deinit(allocator);
    try bridge_mod.check(fx.fake.bridge().guiSetAttr(1, "TextColor", "0xffffffff"));

    try fx.commit(allocator, try setAttr(allocator, fx.fake.bridge(), &fx.doc.gui_ids, 1, "TextColor", "0xff000000"));
    var buffer: [32]u8 = undefined;
    var size: usize = 0;
    try bridge_mod.check(fx.fake.bridge().guiGetAttr(1, "TextColor", &buffer, &size));
    try testing.expectEqualStrings("0xff000000", buffer[0..size]);
    try fx.undoAt(allocator, 0);
    try bridge_mod.check(fx.fake.bridge().guiGetAttr(1, "TextColor", &buffer, &size));
    try testing.expectEqualStrings("0xffffffff", buffer[0..size]);
    try fx.redoAt(allocator, 0);
    try bridge_mod.check(fx.fake.bridge().guiGetAttr(1, "TextColor", &buffer, &size));
    try testing.expectEqualStrings("0xff000000", buffer[0..size]);

    // An attribute the window did not have.
    try fx.commit(allocator, try setAttr(allocator, fx.fake.bridge(), &fx.doc.gui_ids, 2, "FontSize", "3"));
    try fx.undoAt(allocator, 1);
    try bridge_mod.check(fx.fake.bridge().guiGetAttr(2, "FontSize", &buffer, &size));
    try testing.expectEqual(@as(usize, 0), size);
}
