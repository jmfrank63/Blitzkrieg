//! The panels' parts that need no window, no ImGui and no SDL: the file
//! dialogs' hand-over and the file actions it drives, the object palette's
//! names and filter, the direction conversion, what the properties panel may
//! edit, and the window title. Kept apart from panels.zig so these run under
//! `zig build test-map-editor-panels` against the core's fake bridge, without
//! the engine's libraries or a GPU.
const std = @import("std");
const core = @import("editor_core");

const Editor = core.editor.Editor;
const Pose = core.editor.Pose;
const ObjectRecord = core.bridge.ObjectRecord;
const EditError = core.bridge.EditError;

/// A full turn in the engine's directions (SEngineObjectState::wDir is a WORD).
pub const full_turn: i32 = 65536;

pub fn dirToDegrees(dir: i32) f32 {
    const wrapped = @mod(dir, full_turn);
    return @as(f32, @floatFromInt(wrapped)) * 360.0 / @as(f32, @floatFromInt(full_turn));
}

/// Degrees back to the engine's 65536, rounded to the nearest step and
/// wrapped into 0..65535, so -90 and 270 are the same direction.
pub fn degreesToDir(degrees: f32) i32 {
    if (!std.math.isFinite(degrees)) return 0;
    const steps: f64 = @round(@as(f64, degrees) * @as(f64, full_turn) / 360.0);
    const clamped = std.math.clamp(steps, -1.0e9, 1.0e9);
    return @mod(@as(i32, @intFromFloat(clamped)), full_turn);
}

/// The pose a properties edit asks for. The direction is taken from the
/// degrees only when the user changed them: the engine's 65536 steps do not
/// all survive a trip through a displayed number of degrees, so an edit of
/// x alone must not turn the object by a rounding step.
pub fn editedPose(original: Pose, x: f32, y: f32, degrees: f32, degrees_shown: f32, player: i32) Pose {
    return .{
        .x = x,
        .y = y,
        .dir = if (degrees == degrees_shown) original.dir else degreesToDir(degrees),
        .player = player,
    };
}

/// The object database's game types (SGVOGT_*, Sources/src/Main/GameDB.h),
/// named for the palette's headers.
pub fn gameTypeName(game_type: i32) []const u8 {
    return switch (game_type) {
        0 => "SGVOGT_UNKNOWN",
        1 => "SGVOGT_UNIT",
        2 => "SGVOGT_BUILDING",
        3 => "SGVOGT_FORTIFICATION",
        4 => "SGVOGT_ENTRENCHMENT",
        5 => "SGVOGT_TANK_PIT",
        6 => "SGVOGT_BRIDGE",
        7 => "SGVOGT_MINE",
        8 => "SGVOGT_OBJECT",
        9 => "SGVOGT_FENCE",
        10 => "SGVOGT_TERRAOBJ",
        11 => "SGVOGT_EFFECT",
        12 => "SGVOGT_PROJECTILE",
        13 => "SGVOGT_SHADOW",
        14 => "SGVOGT_ICON",
        15 => "SGVOGT_SQUAD",
        16 => "SGVOGT_FLASH",
        17 => "SGVOGT_FLAG",
        100 => "SGVOGT_SOUND",
        else => "SGVOGT_?",
    };
}

/// The palette's filter: a case-insensitive substring. An empty filter
/// matches everything.
pub fn matchesFilter(name: []const u8, filter: []const u8) bool {
    if (filter.len == 0) return true;
    return std.ascii.indexOfIgnoreCase(name, filter) != null;
}

/// Why the properties panel shows an object without editable fields, or
/// null when it may be edited. The bridge refuses to move, turn or re-own an
/// object whose type the database does not know, or whose link ID more than
/// one object of the map carries (bridge.h, "The edits"): both are kept as
/// they are.
pub fn readOnlyReason(objects: []const ObjectRecord, object: ObjectRecord) ?[]const u8 {
    if (!object.known) return "unknown to the object database";
    var carriers: usize = 0;
    for (objects) |other| {
        if (other.link_id == object.link_id) carriers += 1;
    }
    if (carriers > 1) return "its link ID is shared";
    return null;
}

/// The window title: the map's file name, and a `*` while it has changes
/// the file does not.
pub fn formatTitle(buffer: []u8, path: []const u8, dirty: bool) [:0]const u8 {
    const plain = "Map Editor";
    if (path.len == 0) return std.fmt.bufPrintZ(buffer, plain, .{}) catch "";
    const name = baseName(path);
    return std.fmt.bufPrintZ(buffer, plain ++ " - {s}{s}", .{ name, if (dirty) "*" else "" }) catch
        std.fmt.bufPrintZ(buffer, plain, .{}) catch "";
}

/// The last component of a path written with either separator: the engine
/// uses backslashes on every OS, a file dialog the OS's own.
pub fn baseName(path: []const u8) []const u8 {
    const cut = std.mem.lastIndexOfAny(u8, path, "/\\") orelse return path;
    return path[cut + 1 ..];
}

/// A path as the bridge takes it: an OS path written with the engine's
/// separator, because OpenFileStream splits on backslash only (bridge.h,
/// BkEditorOpenMap). A save path without .bzm or .xml gets .bzm, since the
/// bridge picks the format from the extension. Null when it does not fit.
pub fn enginePath(buffer: []u8, os_path: []const u8, kind: DialogKind) ?[]const u8 {
    const needs_extension = kind == .save_as and !hasMapExtension(os_path);
    const extension = if (needs_extension) ".bzm" else "";
    if (os_path.len + extension.len > buffer.len) return null;
    for (os_path, buffer[0..os_path.len]) |char, *out| out.* = if (char == '/') '\\' else char;
    @memcpy(buffer[os_path.len..][0..extension.len], extension);
    return buffer[0 .. os_path.len + extension.len];
}

fn hasMapExtension(path: []const u8) bool {
    return std.ascii.endsWithIgnoreCase(path, ".bzm") or std.ascii.endsWithIgnoreCase(path, ".xml");
}

pub const DialogKind = enum(u8) { open, save_as };

/// The path an SDL file dialog chose, handed from its callback to the frame
/// loop. SDL may call the callback on another thread (SDL_dialog.h), so the
/// callback only writes into this slot and the main thread acts on it in its
/// next frame. One dialog at a time: `request` refuses while one is up, so
/// the callback never writes a buffer the main thread is reading. The state
/// is the only thing both threads touch; the buffer is written before it is
/// published (release) and read after it is seen (acquire).
pub const PathSlot = struct {
    state: std.atomic.Value(u8) = .init(@intFromEnum(State.idle)),
    kind: DialogKind = .open,
    buffer: [max_path]u8 = undefined,
    len: usize = 0,

    pub const max_path = 4096;
    const State = enum(u8) { idle, waiting, arrived, failed };

    pub const Result = union(enum) {
        path: struct { kind: DialogKind, path: []const u8 },
        failed: []const u8,
    };

    /// Main thread, before showing a dialog. False while one is already up.
    pub fn request(self: *PathSlot, kind: DialogKind) bool {
        if (self.state.cmpxchgStrong(@intFromEnum(State.idle), @intFromEnum(State.waiting), .acquire, .monotonic) != null) return false;
        self.kind = kind;
        return true;
    }

    pub fn waiting(self: *const PathSlot) bool {
        return self.state.load(.acquire) == @intFromEnum(State.waiting);
    }

    /// The dialog's callback, on whatever thread SDL calls it: the chosen
    /// path, or null for a cancel. Ignored unless a dialog was requested.
    pub fn deliver(self: *PathSlot, path: ?[]const u8) void {
        if (self.state.load(.acquire) != @intFromEnum(State.waiting)) return;
        const chosen = path orelse {
            self.state.store(@intFromEnum(State.idle), .release);
            return;
        };
        if (chosen.len > self.buffer.len) return self.deliverFailure("the chosen path is too long");
        @memcpy(self.buffer[0..chosen.len], chosen);
        self.len = chosen.len;
        self.state.store(@intFromEnum(State.arrived), .release);
    }

    /// The dialog's callback, when SDL reports an error instead of a choice.
    pub fn deliverFailure(self: *PathSlot, message: []const u8) void {
        if (self.state.load(.acquire) != @intFromEnum(State.waiting)) return;
        const len = @min(message.len, self.buffer.len);
        @memcpy(self.buffer[0..len], message[0..len]);
        self.len = len;
        self.state.store(@intFromEnum(State.failed), .release);
    }

    /// Main thread, once a frame: what arrived since the last call, once.
    /// The slice stays valid until the next `request`, which cannot succeed
    /// before this has emptied the slot.
    pub fn take(self: *PathSlot) ?Result {
        const state: State = @enumFromInt(self.state.load(.acquire));
        const result: Result = switch (state) {
            .idle, .waiting => return null,
            .arrived => .{ .path = .{ .kind = self.kind, .path = self.buffer[0..self.len] } },
            .failed => .{ .failed = self.buffer[0..self.len] },
        };
        self.state.store(@intFromEnum(State.idle), .release);
        return result;
    }
};

/// What the menu asked for this frame, and the dialog hand-over. The panels
/// set the flags while drawing; `next` turns them, and whatever a dialog
/// delivered, into steps for the frame loop, one at a time, until `.none`.
pub const FileActions = struct {
    open_requested: bool = false,
    save_requested: bool = false,
    save_as_requested: bool = false,
    quit_requested: bool = false,
    /// Not owned: a dialog's callback may come after whoever showed it has
    /// gone (a quit with the dialog still up), so the slot it writes into
    /// must outlive every FileActions - panels.zig keeps it in a global.
    dialog: *PathSlot,

    pub const Step = union(enum) {
        none,
        /// Show SDL's open or save dialog; the slot is already waiting for it.
        show_dialog: DialogKind,
        /// Save to the document's own path.
        save,
        /// Open, or save to, a path a dialog chose (an OS path).
        act_on_path: struct { kind: DialogKind, path: []const u8 },
        dialog_failed: []const u8,
        quit,
    };

    pub fn next(self: *FileActions) Step {
        if (self.dialog.take()) |result| return switch (result) {
            .path => |chosen| .{ .act_on_path = .{ .kind = chosen.kind, .path = chosen.path } },
            .failed => |message| .{ .dialog_failed = message },
        };
        if (self.quit_requested) {
            self.quit_requested = false;
            return .quit;
        }
        if (self.save_requested) {
            self.save_requested = false;
            return .save;
        }
        inline for (.{ .{ "open_requested", DialogKind.open }, .{ "save_as_requested", DialogKind.save_as } }) |pair| {
            if (@field(self, pair[0])) {
                @field(self, pair[0]) = false;
                // A dialog already up swallows the second request: one at a
                // time, so the slot is never written twice.
                if (self.dialog.request(pair[1])) return .{ .show_dialog = pair[1] };
            }
        }
        return .none;
    }
};

/// Opens or saves to a path a dialog chose, through the editor, so the
/// document, the history and the status line follow as for any edit.
pub fn actOnPath(editor: *Editor, kind: DialogKind, os_path: []const u8) EditError!void {
    var buffer: [PathSlot.max_path + 4]u8 = undefined;
    const path = enginePath(&buffer, os_path, kind) orelse return error.Failed;
    switch (kind) {
        .open => try editor.open(path),
        .save_as => try editor.save(path),
    }
}

test "directions: the engine's 65536 to degrees and back" {
    try std.testing.expectEqual(@as(f32, 0), dirToDegrees(0));
    try std.testing.expectEqual(@as(f32, 90), dirToDegrees(16384));
    try std.testing.expectEqual(@as(f32, 22.5), dirToDegrees(4096));
    try std.testing.expectEqual(@as(i32, 16384), degreesToDir(90));
    try std.testing.expectEqual(@as(i32, 49152), degreesToDir(-90));
    try std.testing.expectEqual(@as(i32, 0), degreesToDir(360));
    try std.testing.expectEqual(@as(i32, 0), degreesToDir(std.math.nan(f32)));
    var dir: i32 = 0;
    while (dir < full_turn) : (dir += 4096) try std.testing.expectEqual(dir, degreesToDir(dirToDegrees(dir)));
}

test "a properties edit keeps the direction unless the degrees changed" {
    const original: Pose = .{ .x = 10, .y = 20, .dir = 12345, .player = 0 };
    const shown = dirToDegrees(original.dir);
    const moved = editedPose(original, 11, 20, shown, shown, 0);
    try std.testing.expectEqual(@as(i32, 12345), moved.dir);
    try std.testing.expectEqual(@as(f32, 11), moved.x);
    const turned = editedPose(original, 10, 20, 90, shown, 1);
    try std.testing.expectEqual(@as(i32, 16384), turned.dir);
    try std.testing.expectEqual(@as(i32, 1), turned.player);
}

test "the palette's filter is a case-insensitive substring, and types have their engine names" {
    try std.testing.expect(matchesFilter("T34_76", ""));
    try std.testing.expect(matchesFilter("T34_76", "t34"));
    try std.testing.expect(matchesFilter("Pz_IV_H", "iv_h"));
    try std.testing.expect(!matchesFilter("T34_76", "pz"));
    try std.testing.expectEqualStrings("SGVOGT_UNIT", gameTypeName(1));
    try std.testing.expectEqualStrings("SGVOGT_SQUAD", gameTypeName(15));
    try std.testing.expectEqualStrings("SGVOGT_SOUND", gameTypeName(100));
}

test "unknown and shared-ID objects are kept as they are" {
    var tank: ObjectRecord = .{ .link_id = 1 };
    tank.setName("T34");
    var tree_a: ObjectRecord = .{ .link_id = 0 };
    tree_a.setName("Tree");
    var tree_b = tree_a;
    tree_b.x = 5;
    var mystery: ObjectRecord = .{ .link_id = 3, .known = false };
    mystery.setName("No_Such_Object");
    const objects = [_]ObjectRecord{ tank, tree_a, tree_b, mystery };
    try std.testing.expect(readOnlyReason(&objects, tank) == null);
    try std.testing.expect(readOnlyReason(&objects, tree_a) != null);
    try std.testing.expect(readOnlyReason(&objects, mystery) != null);
}

test "the title names the map's file and marks changes" {
    var buffer: [128]u8 = undefined;
    try std.testing.expectEqualStrings("Map Editor", formatTitle(&buffer, "", false));
    try std.testing.expectEqualStrings("Map Editor - coldwinter.bzm", formatTitle(&buffer, "Data\\Maps\\Multiplayer\\coldwinter.bzm", false));
    try std.testing.expectEqualStrings("Map Editor - mine.xml*", formatTitle(&buffer, "/Users/me/mine.xml", true));
}

test "a dialog's path is written with the engine's separator, and a save gets an extension" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("\\Users\\me\\a.bzm", enginePath(&buffer, "/Users/me/a.bzm", .open).?);
    try std.testing.expectEqualStrings("C:\\maps\\a.xml", enginePath(&buffer, "C:\\maps\\a.xml", .save_as).?);
    try std.testing.expectEqualStrings("\\tmp\\b.bzm", enginePath(&buffer, "/tmp/b", .save_as).?);
    try std.testing.expectEqualStrings("\\tmp\\b", enginePath(&buffer, "/tmp/b", .open).?);
    var tiny: [4]u8 = undefined;
    try std.testing.expect(enginePath(&tiny, "/tmp/b", .open) == null);
}

test "the dialog slot: requested, path arrived, taken once; one dialog at a time; a cancel leaves nothing" {
    var slot: PathSlot = .{};
    try std.testing.expect(slot.take() == null);
    slot.deliver("/ignored.bzm"); // no dialog was up
    try std.testing.expect(slot.take() == null);

    try std.testing.expect(slot.request(.open));
    try std.testing.expect(!slot.request(.save_as));
    try std.testing.expect(slot.take() == null); // still waiting
    slot.deliver("/maps/a.bzm");
    const got = slot.take().?;
    try std.testing.expectEqual(DialogKind.open, got.path.kind);
    try std.testing.expectEqualStrings("/maps/a.bzm", got.path.path);
    try std.testing.expect(slot.take() == null);

    try std.testing.expect(slot.request(.save_as));
    slot.deliver(null);
    try std.testing.expect(slot.take() == null);
    try std.testing.expect(!slot.waiting());

    try std.testing.expect(slot.request(.open));
    slot.deliverFailure("no portal");
    try std.testing.expectEqualStrings("no portal", slot.take().?.failed);
}

test "file actions: a request shows a dialog, the path it delivers is acted on the next frame" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    // Frame 1: the menu asked for Open; the loop is told to show the dialog.
    actions.open_requested = true;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .open }, actions.next());
    try std.testing.expectEqual(FileActions.Step.none, actions.next());
    // A second Open while the dialog is up does nothing.
    actions.open_requested = true;
    try std.testing.expectEqual(FileActions.Step.none, actions.next());

    // Between frames the callback delivers; frame 2 acts on it.
    actions.dialog.deliver("/maps/fixture.bzm");
    const step = actions.next();
    try std.testing.expectEqual(DialogKind.open, step.act_on_path.kind);
    try actOnPath(&editor, step.act_on_path.kind, step.act_on_path.path);
    try std.testing.expectEqualStrings("\\maps\\fixture.bzm", editor.document.path.items);
    try std.testing.expectEqual(FileActions.Step.none, actions.next());

    // Save As, then a plain Save to the path it chose.
    actions.save_as_requested = true;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next());
    actions.dialog.deliver("/maps/copy");
    const save_step = actions.next();
    try actOnPath(&editor, save_step.act_on_path.kind, save_step.act_on_path.path);
    try std.testing.expectEqualStrings("\\maps\\copy.bzm", editor.document.path.items);
    actions.save_requested = true;
    actions.quit_requested = true;
    try std.testing.expectEqual(FileActions.Step.quit, actions.next());
    try std.testing.expectEqual(FileActions.Step.save, actions.next());
    try std.testing.expectEqual(FileActions.Step.none, actions.next());
}
