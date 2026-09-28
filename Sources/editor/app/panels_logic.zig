//! The panels' parts that need no window, no ImGui and no SDL: the file
//! dialogs' hand-over and the file actions it drives, the object palette's
//! names and filter, the direction conversion, what the properties panel may
//! edit, and the window title. Kept apart from panels.zig so these run under
//! `zig build test-map-editor-panels` against the core's fake bridge, without
//! the engine's libraries or a GPU.
const std = @import("std");
const core = @import("editor_core");
const testlaunch = @import("testlaunch.zig");

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

/// Whether an object of this game type can be put on a map, and so belongs
/// in the palette. A sound (100) and a tank pit (5) are in the object
/// database but are not map objects: a map keeps its sounds in their own
/// list, and tank pits are dug during play. The bridge refuses both
/// (WhyNotAMapObject, Sources/src/EditorBridge/session.cpp) and the MFC
/// editor's palette never listed either (TabSimpleObjectsDialog.cpp:158).
pub fn isPlaceable(game_type: i32) bool {
    return switch (game_type) {
        5, 100 => false,
        else => true,
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
    const State = enum(u8) { idle, waiting, arrived, failed, cancelled };

    pub const Result = union(enum) {
        path: struct { kind: DialogKind, path: []const u8 },
        failed: []const u8,
        cancelled,
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
    /// path, or null for a cancel - a result now, not silence, so the
    /// unsaved-changes prompt can tell a cancelled Save As from one that
    /// never happened (D-23). Ignored unless a dialog was requested.
    pub fn deliver(self: *PathSlot, path: ?[]const u8) void {
        if (self.state.load(.acquire) != @intFromEnum(State.waiting)) return;
        const chosen = path orelse {
            self.state.store(@intFromEnum(State.cancelled), .release);
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
            .cancelled => .cancelled,
        };
        self.state.store(@intFromEnum(State.idle), .release);
        return result;
    }
};

/// A fixed-buffer path, for a `Pending` that must outlive the frame it was
/// made on (no allocation, matching `PathSlot`'s own buffers).
pub const PathText = struct {
    buffer: [PathSlot.max_path]u8 = undefined,
    len: usize = 0,

    pub fn init(text: []const u8) PathText {
        var self: PathText = .{};
        self.set(text);
        return self;
    }

    pub fn set(self: *PathText, text: []const u8) void {
        self.len = @min(text.len, self.buffer.len);
        @memcpy(self.buffer[0..self.len], text[0..self.len]);
    }

    pub fn slice(self: *const PathText) []const u8 {
        return self.buffer[0..self.len];
    }
};

/// A fixed-buffer name (a mod's folder), for the same reason as `PathText`.
pub const NameText = struct {
    buffer: [256]u8 = undefined,
    len: usize = 0,

    pub fn init(text: []const u8) NameText {
        var self: NameText = .{};
        self.set(text);
        return self;
    }

    pub fn set(self: *NameText, text: []const u8) void {
        self.len = @min(text.len, self.buffer.len);
        @memcpy(self.buffer[0..self.len], text[0..self.len]);
    }

    pub fn slice(self: *const NameText) []const u8 {
        return self.buffer[0..self.len];
    }
};

/// The action an unsaved-changes prompt is guarding: what to do once the
/// user says it is fine to go ahead (or once a redirected save lands).
/// `open_path` and `switch_mod` are not produced by this plan - they are
/// here so 03-07's Open Recent and 03-08's Mod switch can guard through the
/// same prompt without changing its shape.
pub const Pending = union(enum) {
    open_dialog,
    open_path: PathText,
    quit,
    switch_mod: NameText,
};

/// D-23's Open/Quit/window-close prompt: idle until a guarded action finds
/// the map dirty, then asking until the user answers Save, Don't save or
/// Cancel. Save moves to saving without doing any I/O itself - the caller
/// (which owns the editor) makes the save and reports back through
/// `saveFinished`, which continues with the guarded action only once that
/// save actually landed.
pub const UnsavedPrompt = struct {
    pending: ?Pending = null,
    phase: Phase = .idle,

    const Phase = enum { idle, asking, saving };

    pub const Choice = enum { save, dont_save, cancel };

    pub const GuardResult = union(enum) {
        /// The map is clean, or a save just landed: `action` may run now.
        proceed: Pending,
        /// The map is dirty: the modal must ask first.
        asked,
    };

    /// Before Open, Quit or a window close: a clean map proceeds with
    /// `action` at once; a dirty one is remembered and the modal is asked.
    pub fn guard(self: *UnsavedPrompt, dirty: bool, action: Pending) GuardResult {
        if (!dirty) return .{ .proceed = action };
        self.pending = action;
        self.phase = .asking;
        return .asked;
    }

    pub fn isAsking(self: *const UnsavedPrompt) bool {
        return self.phase == .asking;
    }

    pub const AnswerResult = union(enum) {
        /// Save to the document's own path, then report through `saveFinished`.
        save,
        /// The map has no path yet, or is shipped: show Save As instead,
        /// then report through `saveFinished` once its dialog resolves.
        save_as,
        /// Don't save, or nothing was ever guarded: go ahead with `action` at once.
        proceed: Pending,
        /// Cancel: the guarded action never happens.
        dropped,
    };

    /// The modal's button. Cancel and Don't save resolve the prompt
    /// immediately; Save only says what saving needs - the actual write
    /// happens outside, reported back through `saveFinished`.
    pub fn answer(self: *UnsavedPrompt, choice: Choice, needs_save_as: bool) AnswerResult {
        if (self.phase != .asking) return .dropped;
        switch (choice) {
            .cancel => {
                self.pending = null;
                self.phase = .idle;
                return .dropped;
            },
            .dont_save => {
                const pending = self.pending;
                self.pending = null;
                self.phase = .idle;
                return if (pending) |action| .{ .proceed = action } else .dropped;
            },
            .save => {
                self.phase = .saving;
                return if (needs_save_as) .save_as else .save;
            },
        }
    }

    /// The caller reports whether the save it was asked to make (plain or
    /// Save As) actually landed. A cancelled Save As is reported as `false`
    /// too - both drop the guarded action and leave the map exactly as it
    /// was (a failed save keeps it dirty; a cancelled dialog never touched
    /// it). A call while nothing was ever asked for is ignored.
    pub fn saveFinished(self: *UnsavedPrompt, ok: bool) ?Pending {
        if (self.phase != .saving) return null;
        self.phase = .idle;
        const pending = self.pending;
        self.pending = null;
        return if (ok) pending else null;
    }
};

/// What the menu asked for this frame, and the dialog hand-over. The panels
/// set the flags while drawing; `next` turns them, and whatever a dialog
/// delivered, into steps for the frame loop, one at a time, until `.none`.
/// Open and Quit are routed through `prompt`'s unsaved-changes guard (D-23);
/// Save and Save As are not - they are themselves how the user saves.
pub const FileActions = struct {
    open_requested: bool = false,
    save_requested: bool = false,
    save_as_requested: bool = false,
    quit_requested: bool = false,
    /// Not owned: a dialog's callback may come after whoever showed it has
    /// gone (a quit with the dialog still up), so the slot it writes into
    /// must outlive every FileActions - panels.zig keeps it in a global.
    dialog: *PathSlot,
    prompt: UnsavedPrompt = .{},
    /// Set by the modal's Save/Don't save/Cancel buttons (and by the smoke's
    /// `.answer`), for `next` to act on next.
    answer_pending: ?UnsavedPrompt.Choice = null,
    /// True while the dialog now waiting belongs to the prompt's own Save As
    /// (asked for by `answer`), not a plain user request - so its outcome
    /// reports back to the prompt through `saveFinished` instead of just
    /// being an ordinary act_on_path.
    dialog_for_prompt: bool = false,
    /// A guarded action `saveFinished` resolved, waiting for its Step.
    resume_pending: ?Pending = null,

    pub const Step = union(enum) {
        none,
        /// The prompt is asking; the modal is (or must become) visible.
        ask_unsaved,
        /// Show SDL's open or save dialog; the slot is already waiting for it.
        show_dialog: DialogKind,
        /// Save to the document's own path.
        save,
        /// Open, or save to, a path a dialog chose (an OS path).
        act_on_path: struct { kind: DialogKind, path: []const u8 },
        dialog_failed: []const u8,
        /// A dialog was cancelled - the prompt has already been told, if it
        /// was the one waiting on it.
        dialog_cancelled,
        quit,
    };

    /// The save `next`/`act` was told to make, for `saveFinished`, resolved
    /// into a Step - `open_path` and `switch_mod` are not produced by this
    /// plan (03-07/03-08 wire them).
    fn stepForPending(self: *FileActions, pending: Pending) Step {
        return switch (pending) {
            .open_dialog => if (self.dialog.request(.open)) Step{ .show_dialog = .open } else Step.none,
            .quit => .quit,
            .open_path, .switch_mod => .none,
        };
    }

    /// The save `act` made for the prompt (plain Save or a Save As whose
    /// dialog delivered a path) succeeded or failed; a step for whatever the
    /// prompt was guarding follows on the next `next()`, if anything does.
    /// A no-op when the prompt was not the one asking for this save.
    pub fn noteSaveOutcome(self: *FileActions, ok: bool) void {
        if (self.prompt.saveFinished(ok)) |pending| self.resume_pending = pending;
    }

    pub fn next(self: *FileActions, dirty: bool, needs_save_as: bool) Step {
        if (self.dialog.take()) |result| {
            switch (result) {
                .path => |chosen| {
                    self.dialog_for_prompt = false;
                    return .{ .act_on_path = .{ .kind = chosen.kind, .path = chosen.path } };
                },
                .failed => |message| {
                    self.dialog_for_prompt = false;
                    return .{ .dialog_failed = message };
                },
                .cancelled => {
                    const was_for_prompt = self.dialog_for_prompt;
                    self.dialog_for_prompt = false;
                    if (was_for_prompt) self.noteSaveOutcome(false);
                    return .dialog_cancelled;
                },
            }
        }
        if (self.resume_pending) |pending| {
            self.resume_pending = null;
            return self.stepForPending(pending);
        }
        if (self.answer_pending) |choice| {
            self.answer_pending = null;
            return switch (self.prompt.answer(choice, needs_save_as)) {
                .save => .save,
                .save_as => if (self.dialog.request(.save_as)) blk: {
                    self.dialog_for_prompt = true;
                    break :blk Step{ .show_dialog = .save_as };
                } else Step.none,
                .proceed => |pending| self.stepForPending(pending),
                .dropped => .none,
            };
        }
        if (self.prompt.isAsking()) return .ask_unsaved;
        if (self.quit_requested) {
            self.quit_requested = false;
            return switch (self.prompt.guard(dirty, .quit)) {
                .proceed => |pending| self.stepForPending(pending),
                .asked => .ask_unsaved,
            };
        }
        if (self.save_requested) {
            self.save_requested = false;
            if (needs_save_as) {
                if (self.dialog.request(.save_as)) return .{ .show_dialog = .save_as };
                return .none;
            }
            return .save;
        }
        if (self.open_requested) {
            self.open_requested = false;
            return switch (self.prompt.guard(dirty, .open_dialog)) {
                .proceed => |pending| self.stepForPending(pending),
                .asked => .ask_unsaved,
            };
        }
        if (self.save_as_requested) {
            self.save_as_requested = false;
            if (self.dialog.request(.save_as)) return .{ .show_dialog = .save_as };
        }
        return .none;
    }
};

/// D-06's "Test while a test game still runs" prompt: idle until a request
/// while one is up, then asking_restart until the user answers, then either
/// back to idle (Keep) or restarting - waiting for the old game to actually
/// exit before the new one starts, so the two are never running at once
/// under the same profile. reporting holds a failure's message until the
/// caller shows and acknowledges it; a clean exit is never reported at all.
pub const TestLaunchPrompt = struct {
    state: State = .idle,
    report_buffer: [256]u8 = undefined,
    report_len: usize = 0,

    const State = enum { idle, asking_restart, restarting, reporting };

    pub const Step = enum { none, start, ask_restart };
    pub const Answer = enum { restart, keep };

    /// The menu item or F5: running says whether a test game is already up.
    /// A pending report is not cleared by this - the caller shows both.
    pub fn request(self: *TestLaunchPrompt, running: bool) Step {
        if (!running) {
            if (self.state != .reporting) self.state = .idle;
            return .start;
        }
        self.state = .asking_restart;
        return .ask_restart;
    }

    pub fn isAskingRestart(self: *const TestLaunchPrompt) bool {
        return self.state == .asking_restart;
    }

    /// The user's answer to the still-running prompt.
    pub fn answer(self: *TestLaunchPrompt, choice: Answer) void {
        if (self.state != .asking_restart) return;
        self.state = switch (choice) {
            .keep => .idle,
            .restart => .restarting,
        };
    }

    /// The running game's exit reached the poll. Returns .start when this
    /// was the old game restarting was waiting for - the caller starts the
    /// new one immediately. A non-clean exit (describe() != .clean) is kept
    /// as a report until acknowledgeReport(); a clean one reports nothing,
    /// including the D-06 "Keep it running" case, since the prompt was
    /// never re-asked for that game.
    pub fn gameExited(self: *TestLaunchPrompt, exit: testlaunch.Exit, log_path: []const u8) Step {
        const was_restarting = self.state == .restarting;
        self.state = .idle;
        if (testlaunch.describe(exit) != .clean) {
            self.setReport(exit, log_path);
            self.state = .reporting;
        }
        return if (was_restarting) .start else .none;
    }

    /// The failure report to show, or null when there is none pending.
    pub fn report(self: *const TestLaunchPrompt) ?[]const u8 {
        return if (self.state == .reporting) self.report_buffer[0..self.report_len] else null;
    }

    /// The caller has shown the report (an OK on its modal).
    pub fn acknowledgeReport(self: *TestLaunchPrompt) void {
        if (self.state == .reporting) self.state = .idle;
    }

    /// A launch that never got as far as running at all (spawn itself
    /// failed - no game beside the editor, say): shown the same way as a bad
    /// exit, since both are "Test in game did not work" from the player's
    /// side.
    pub fn reportFailure(self: *TestLaunchPrompt, message: []const u8) void {
        const len = @min(message.len, self.report_buffer.len);
        @memcpy(self.report_buffer[0..len], message[0..len]);
        self.report_len = len;
        self.state = .reporting;
    }

    fn setReport(self: *TestLaunchPrompt, exit: testlaunch.Exit, log_path: []const u8) void {
        const seconds = @as(f64, @floatFromInt(exit.lifetime_ms)) / 1000.0;
        const text = std.fmt.bufPrint(&self.report_buffer, "The game exited with code {d} after {d:.1} s. Its log: {s}", .{ exit.code orelse 0, seconds, log_path }) catch self.report_buffer[0..0];
        self.report_len = text.len;
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

test "the palette leaves out the types a map cannot hold" {
    try std.testing.expect(!isPlaceable(100));
    try std.testing.expect(!isPlaceable(5));
    for ([_]i32{ 1, 2, 3, 4, 6, 7, 8, 9, 10, 15, 17 }) |game_type| try std.testing.expect(isPlaceable(game_type));
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
    try std.testing.expect(!slot.waiting());
    switch (slot.take().?) {
        .cancelled => {},
        else => return error.TestUnexpectedResult,
    }
    try std.testing.expect(slot.take() == null);

    try std.testing.expect(slot.request(.open));
    slot.deliverFailure("no portal");
    try std.testing.expectEqualStrings("no portal", slot.take().?.failed);
}

test "UnsavedPrompt: a clean map proceeds with Open or Quit at once" {
    var prompt: UnsavedPrompt = .{};
    switch (prompt.guard(false, .open_dialog)) {
        .proceed => |pending| switch (pending) {
            .open_dialog => {},
            else => return error.TestUnexpectedResult,
        },
        .asked => return error.TestUnexpectedResult,
    }
    switch (prompt.guard(false, .quit)) {
        .proceed => |pending| switch (pending) {
            .quit => {},
            else => return error.TestUnexpectedResult,
        },
        .asked => return error.TestUnexpectedResult,
    }
    try std.testing.expect(!prompt.isAsking());
}

test "UnsavedPrompt: a dirty map asks for both Open and Quit" {
    var prompt: UnsavedPrompt = .{};
    switch (prompt.guard(true, .open_dialog)) {
        .asked => {},
        .proceed => return error.TestUnexpectedResult,
    }
    try std.testing.expect(prompt.isAsking());
    _ = prompt.answer(.dont_save, false); // resolve it before asking again
    switch (prompt.guard(true, .quit)) {
        .asked => {},
        .proceed => return error.TestUnexpectedResult,
    }
    try std.testing.expect(prompt.isAsking());
}

test "UnsavedPrompt: Cancel drops the guarded action" {
    var prompt: UnsavedPrompt = .{};
    _ = prompt.guard(true, .quit);
    switch (prompt.answer(.cancel, false)) {
        .dropped => {},
        else => return error.TestUnexpectedResult,
    }
    try std.testing.expect(!prompt.isAsking());
    try std.testing.expect(prompt.pending == null);
}

test "UnsavedPrompt: Don't save proceeds with the guarded action" {
    var prompt: UnsavedPrompt = .{};
    _ = prompt.guard(true, .open_dialog);
    switch (prompt.answer(.dont_save, false)) {
        .proceed => |pending| switch (pending) {
            .open_dialog => {},
            else => return error.TestUnexpectedResult,
        },
        else => return error.TestUnexpectedResult,
    }
    try std.testing.expect(!prompt.isAsking());
}

test "UnsavedPrompt: Save with a path saves, then proceeds once it lands" {
    var prompt: UnsavedPrompt = .{};
    _ = prompt.guard(true, .quit);
    switch (prompt.answer(.save, false)) {
        .save => {},
        else => return error.TestUnexpectedResult,
    }
    try std.testing.expect(!prompt.isAsking()); // now saving, not asking
    switch (prompt.saveFinished(true) orelse return error.TestUnexpectedResult) {
        .quit => {},
        else => return error.TestUnexpectedResult,
    }
}

test "UnsavedPrompt: Save on a path-less or shipped map shows Save As, proceeding only once that save lands" {
    var prompt: UnsavedPrompt = .{};
    _ = prompt.guard(true, .open_dialog);
    switch (prompt.answer(.save, true)) {
        .save_as => {},
        else => return error.TestUnexpectedResult,
    }
    switch (prompt.saveFinished(true) orelse return error.TestUnexpectedResult) {
        .open_dialog => {},
        else => return error.TestUnexpectedResult,
    }
}

test "UnsavedPrompt: a cancelled Save As drops the guarded action" {
    var prompt: UnsavedPrompt = .{};
    _ = prompt.guard(true, .quit);
    _ = prompt.answer(.save, true); // .save_as
    try std.testing.expect(prompt.saveFinished(false) == null);
    try std.testing.expect(!prompt.isAsking());
    try std.testing.expect(prompt.pending == null);
}

test "UnsavedPrompt: a failed save drops the guarded action" {
    var prompt: UnsavedPrompt = .{};
    _ = prompt.guard(true, .quit);
    _ = prompt.answer(.save, false); // .save
    try std.testing.expect(prompt.saveFinished(false) == null);
    try std.testing.expect(!prompt.isAsking());
}

test "file actions: a dirty Quit asks; Save reports success and the quit follows" {
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    actions.quit_requested = true;
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    try std.testing.expect(actions.prompt.isAsking());

    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step.save, actions.next(true, false));
    actions.noteSaveOutcome(true);
    try std.testing.expectEqual(FileActions.Step.quit, actions.next(true, false));
    try std.testing.expect(!actions.prompt.isAsking());
}

test "file actions: a dirty Open with no path asks Save As; a cancelled dialog drops the open" {
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    actions.open_requested = true;
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, true));
    actions.answer_pending = .save;
    const step = actions.next(true, true);
    try std.testing.expectEqual(DialogKind.save_as, step.show_dialog);
    try std.testing.expect(actions.dialog_for_prompt);
    actions.dialog.deliver(null);
    try std.testing.expectEqual(FileActions.Step.dialog_cancelled, actions.next(true, true));
    try std.testing.expect(!actions.dialog_for_prompt);
    try std.testing.expect(!actions.prompt.isAsking());
    try std.testing.expectEqual(FileActions.Step.none, actions.next(false, false));
}

test "file actions: a request shows a dialog, the path it delivers is acted on the next frame" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = core.files.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    editor.files = fake_files.files();
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    // Frame 1: the menu asked for Open; the loop is told to show the dialog.
    actions.open_requested = true;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .open }, actions.next(false, false));
    try std.testing.expectEqual(FileActions.Step.none, actions.next(false, false));
    // A second Open while the dialog is up does nothing.
    actions.open_requested = true;
    try std.testing.expectEqual(FileActions.Step.none, actions.next(false, false));

    // Between frames the callback delivers; frame 2 acts on it.
    actions.dialog.deliver("/maps/fixture.bzm");
    const step = actions.next(false, false);
    try std.testing.expectEqual(DialogKind.open, step.act_on_path.kind);
    try actOnPath(&editor, step.act_on_path.kind, step.act_on_path.path);
    try std.testing.expectEqualStrings("\\maps\\fixture.bzm", editor.document.path.items);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(false, false));

    // Save As, then a plain Save to the path it chose.
    actions.save_as_requested = true;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next(false, false));
    actions.dialog.deliver("/maps/copy");
    const save_step = actions.next(false, false);
    try actOnPath(&editor, save_step.act_on_path.kind, save_step.act_on_path.path);
    try std.testing.expectEqualStrings("\\maps\\copy.bzm", editor.document.path.items);
    actions.save_requested = true;
    actions.quit_requested = true;
    try std.testing.expectEqual(FileActions.Step.quit, actions.next(false, false));
    try std.testing.expectEqual(FileActions.Step.save, actions.next(false, false));
    try std.testing.expectEqual(FileActions.Step.none, actions.next(false, false));
}

test "TestLaunchPrompt: nothing running starts it directly" {
    var prompt: TestLaunchPrompt = .{};
    try std.testing.expectEqual(TestLaunchPrompt.Step.start, prompt.request(false));
    try std.testing.expect(!prompt.isAskingRestart());
}

test "TestLaunchPrompt: a Test while one runs asks; Keep leaves it running" {
    var prompt: TestLaunchPrompt = .{};
    try std.testing.expectEqual(TestLaunchPrompt.Step.ask_restart, prompt.request(true));
    try std.testing.expect(prompt.isAskingRestart());
    prompt.answer(.keep);
    try std.testing.expect(!prompt.isAskingRestart());
    // The kept game later exits cleanly: nothing to report, and it was never
    // the thing restarting was waiting for.
    try std.testing.expectEqual(TestLaunchPrompt.Step.none, prompt.gameExited(.{ .code = 0, .signal = null, .lifetime_ms = 30_000 }, "log"));
    try std.testing.expect(prompt.report() == null);
}

test "TestLaunchPrompt: Restart waits for the old game's exit, then starts the new one" {
    var prompt: TestLaunchPrompt = .{};
    _ = prompt.request(true);
    prompt.answer(.restart);
    try std.testing.expectEqual(TestLaunchPrompt.Step.start, prompt.gameExited(.{ .code = 0, .signal = null, .lifetime_ms = 30_000 }, "log"));
    try std.testing.expect(prompt.report() == null);
}

test "TestLaunchPrompt: an early failure produces a report naming the code and the log" {
    var prompt: TestLaunchPrompt = .{};
    _ = prompt.request(false);
    const step = prompt.gameExited(.{ .code = 53, .signal = null, .lifetime_ms = 200 }, "zig-out/local-test/mapeditor/test-game.log");
    try std.testing.expectEqual(TestLaunchPrompt.Step.none, step);
    const message = prompt.report() orelse return error.TestUnexpectedResult;
    try std.testing.expect(std.mem.indexOf(u8, message, "53") != null);
    try std.testing.expect(std.mem.indexOf(u8, message, "zig-out/local-test/mapeditor/test-game.log") != null);
    prompt.acknowledgeReport();
    try std.testing.expect(prompt.report() == null);
}

test "TestLaunchPrompt: a clean exit reports nothing" {
    var prompt: TestLaunchPrompt = .{};
    _ = prompt.request(false);
    _ = prompt.gameExited(.{ .code = 0, .signal = null, .lifetime_ms = 30_000 }, "log");
    try std.testing.expect(prompt.report() == null);
}
