const std = @import("std");
const bridge_mod = @import("bridge.zig");
const fake_mod = @import("fake_bridge.zig");
const document_mod = @import("document.zig");
const history_mod = @import("history.zig");
const tools = @import("tools.zig");
const files_mod = @import("files.zig");
const shipped_mod = @import("shipped.zig");
const Bridge = bridge_mod.Bridge;
const EditError = bridge_mod.EditError;
const ObjectRecord = bridge_mod.ObjectRecord;
const SoundRecord = bridge_mod.SoundRecord;
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
    /// Bumped by every sound-list change - addSound, editSound, deleteSound,
    /// and their undo/redo alike (`replay`'s own three cases) - so the panel
    /// knows to read the list again; it has no mirror of its own to compare
    /// against (unlike `document.objects`, the bridge's sound list is read
    /// fresh through `bridge.sounds` whenever this changes).
    sounds_generation: u32 = 0,
    /// null in a mode that never saves (a headless tier with no need to);
    /// `save` refuses with "saving needs a file system" rather than write
    /// unsafely when this is unset (D-19).
    files: ?files_mod.Files = null,
    /// The OS paths `save` has already taken this session's one `.bak` for
    /// (D-19: once per file per session, at the first write); owned keys,
    /// freed in `deinit`.
    backed_up: std.StringHashMapUnmanaged(void) = .empty,
    /// The installation the editor runs from (BkEditorPaths' base root),
    /// copied by `setBaseRoot`: what `save` classifies a shipped map against
    /// (shipped.zig's rule 1). Empty until set - rules 2 and 3 still apply.
    base_root_buffer: [shipped_mod.max_path]u8 = undefined,
    base_root_len: usize = 0,

    pub fn init(allocator: std.mem.Allocator, b: Bridge) Editor {
        return .{ .allocator = allocator, .bridge = b };
    }

    pub fn deinit(self: *Editor) void {
        self.document.deinit(self.allocator);
        self.history.deinit(self.allocator);
        var backed_up_keys = self.backed_up.keyIterator();
        while (backed_up_keys.next()) |key| self.allocator.free(key.*);
        self.backed_up.deinit(self.allocator);
        self.* = undefined;
    }

    /// Copies `root` (truncated to the buffer, which a real root never
    /// reaches: BkEditorPathSet's own is 1024 bytes).
    pub fn setBaseRoot(self: *Editor, root: []const u8) void {
        const len = @min(root.len, self.base_root_buffer.len);
        @memcpy(self.base_root_buffer[0..len], root[0..len]);
        self.base_root_len = len;
    }

    pub fn baseRoot(self: *const Editor) []const u8 {
        return self.base_root_buffer[0..self.base_root_len];
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

    /// A broken map is rejected as a whole. When `openMap` refuses the file
    /// - it is missing or will not read, `data_missing` or `bad_argument` -
    /// the bridge keeps the map that was open, and so does the document.
    /// When it answers `failed`, the engine failed while the new map was
    /// being built into it and the bridge has no map open any more; and when
    /// `openMap` succeeds but the object or diplomacy listing then fails, the
    /// bridge holds a map the document could not read. Either way the
    /// document is emptied rather than left holding link IDs that edits
    /// could reach the wrong objects through.
    pub fn open(self: *Editor, path: []const u8) EditError!void {
        var info: bridge_mod.MapInfo = .{};
        const opened = self.bridge.openMap(path, &info);
        self.noteOutcome(opened) catch |err| {
            if (opened == .failed) self.closeDocument();
            return err;
        };
        self.document.reload(self.allocator, self.bridge, path, info) catch |err| {
            self.setStatus("the map opened but its objects could not be read: ", self.bridge.lastMessage());
            self.closeDocument();
            return err;
        };
        self.history.clear(self.allocator);
        self.selection = null;
    }

    /// Forgets the open document: no path, no objects, nothing to undo or
    /// redo, nothing selected, a clean status. The bridge's own map is the
    /// caller's to close - File > Mod (D-26, revised 2026-09-29) calls this
    /// right after `BkEditorSetMod`, which closes the engine's map itself
    /// before it swaps the object database. The session's `.bak` bookkeeping
    /// (`backed_up`) is kept: D-19's "once per file per session" outlives a
    /// close, exactly as it outlives an Open of another map.
    pub fn close(self: *Editor) void {
        self.closeDocument();
        self.status_len = 0;
    }

    fn closeDocument(self: *Editor) void {
        self.document.deinit(self.allocator);
        self.document = .{};
        self.history.clear(self.allocator);
        self.selection = null;
    }

    /// The new path is copied before the bridge writes: once the file is
    /// saved, nothing here may fail and leave the document without a path.
    /// Copying first also keeps `path` valid when it is the document's own.
    ///
    /// D-19's safe save: `path` (the engine's form - see `files_mod`) is
    /// never written to directly. The bridge writes and verifies a temporary
    /// file beside it (`tempPathFor`), which itself reads back and compares
    /// what it wrote (BkEditorSaveMap -> SaveSessionMap); this session's
    /// first write to `path` copies whatever was there to `path.bak`; only
    /// then is the temporary file swapped over `path`. Any failure along the
    /// way deletes the temporary file and leaves `path` exactly as it was.
    ///
    /// D-18, defence in depth: a `path` inside any game's data (shipped.zig)
    /// is refused before anything is written - not even the temporary file
    /// goes beside it - whichever caller asked. The panels already turn Save
    /// on such a map into Save As and autosave it only to a recovery copy;
    /// this is what holds if one of them ever forgets.
    pub fn save(self: *Editor, path: []const u8) EditError!void {
        var new_path: std.ArrayListUnmanaged(u8) = .empty;
        errdefer new_path.deinit(self.allocator);
        try new_path.appendSlice(self.allocator, path);

        const files = self.files orelse {
            self.setStatus("", "saving needs a file system");
            return error.Failed;
        };

        if (shipped_mod.isShipped(path, self.baseRoot(), files)) {
            const cut = std.mem.lastIndexOfAny(u8, path, "/\\");
            const name = if (cut) |c| path[c + 1 ..] else path;
            var buffer: [200]u8 = undefined;
            const message = std.fmt.bufPrint(&buffer, "{s} is inside a game's data folder, which is read-only - Save As into your maps folder instead", .{name}) catch
                "a game's data folder is read-only - Save As into your maps folder instead";
            self.setStatus("not saved: ", message);
            return error.Refused;
        }

        var temp_buffer: [files_mod.max_path]u8 = undefined;
        const temp = files_mod.tempPathFor(&temp_buffer, path) orelse {
            self.setStatus("", "the path is too long to save safely");
            return error.Failed;
        };
        var temp_os_buffer: [files_mod.max_path]u8 = undefined;
        const temp_os = files_mod.osPathFromEngine(&temp_os_buffer, temp) orelse {
            self.setStatus("", "the path is too long to save safely");
            return error.Failed;
        };
        // A stale temp from an earlier failed save must not be mistaken for
        // this save's write, nor left behind if this one fails before ever
        // reaching the bridge.
        files.delete(temp_os);

        self.noteOutcome(self.bridge.saveMap(temp)) catch |err| {
            files.delete(temp_os);
            return err;
        };

        var path_os_buffer: [files_mod.max_path]u8 = undefined;
        const path_os = files_mod.osPathFromEngine(&path_os_buffer, path) orelse {
            files.delete(temp_os);
            self.setStatus("", "the path is too long to keep a backup of");
            return error.Failed;
        };

        if (!self.backed_up.contains(path_os)) {
            if (files.exists(path_os)) {
                var backup_buffer: [files_mod.max_path]u8 = undefined;
                const backup_os = files_mod.backupPathFor(&backup_buffer, path_os) orelse {
                    files.delete(temp_os);
                    self.setStatus("", "the path is too long to keep a backup of");
                    return error.Failed;
                };
                files.copy(path_os, backup_os) catch {
                    files.delete(temp_os);
                    self.saveFailureStatus("could not keep a backup of ", path_os, files.lastError());
                    return error.Failed;
                };
            }
            // Recorded even when `path_os` did not exist yet, so a file
            // created this session never gets a .bak of a later version.
            const owned_key = self.allocator.dupe(u8, path_os) catch |err| {
                files.delete(temp_os);
                return err;
            };
            self.backed_up.put(self.allocator, owned_key, {}) catch |err| {
                self.allocator.free(owned_key);
                files.delete(temp_os);
                return err;
            };
        }

        files.rename(temp_os, path_os) catch {
            files.delete(temp_os);
            self.saveFailureStatus("could not replace ", path_os, files.lastError());
            return error.Failed;
        };

        self.document.path.deinit(self.allocator);
        self.document.path = new_path;
        self.history.markClean();
    }

    /// "<prefix><name>: <reason>", the name being `path_os`'s last
    /// component - the status bar names the file, not its full path.
    fn saveFailureStatus(self: *Editor, comptime prefix: []const u8, path_os: []const u8, reason: []const u8) void {
        const cut = std.mem.lastIndexOfAny(u8, path_os, "/\\");
        const name = if (cut) |c| path_os[c + 1 ..] else path_os;
        var buffer: [200]u8 = undefined;
        const combined = std.fmt.bufPrint(&buffer, "{s}: {s}", .{ name, reason }) catch buffer[0..];
        self.setStatus(prefix, combined);
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
        var pointer: tools.Pointer = .{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0 };
        try bridge_mod.check(self.bridge.screenToWorld(sx, sy, &pointer.world_x, &pointer.world_y));
        try bridge_mod.check(self.bridge.worldToMap(pointer.world_x, pointer.world_y, &pointer.map_x, &pointer.map_y));
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
            const entry = merge_entry.?;
            // A drag that ends where it began is no edit: the entry goes,
            // and with it the dirty mark it would have left.
            if (std.meta.eql(entry.command.place.before, pose)) {
                self.history.dropTop(self.allocator);
            } else {
                entry.command.place.after = pose;
                self.history.touchTop(self.allocator);
            }
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

    /// The bridge's sound list has no per-index read of its own (unlike an
    /// object, which the document mirrors) - `BkEditorSounds` is the only
    /// accessor, and it reads the whole list. `editSound` and `deleteSound`
    /// need the record at `index` before they change it, to record it for
    /// undo, so they read the whole list here and pick it out - the same
    /// two-pass sizing `document.reload` uses for `bridge.objects`.
    fn readSoundAt(self: *Editor, index: usize) EditError!SoundRecord {
        var none: [0]SoundRecord = .{};
        var total: usize = 0;
        const sizing = self.bridge.sounds(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        if (index >= total) return error.Failed;
        const buffer = self.allocator.alloc(SoundRecord, total) catch return error.OutOfMemory;
        defer self.allocator.free(buffer);
        try bridge_mod.check(self.bridge.sounds(buffer, &total));
        if (index >= buffer.len) return error.Failed;
        return buffer[index];
    }

    /// Adds `record` to the bridge's sound list at `index` (0..count inserts
    /// there, -1 appends - BkEditorAddSound's own sentinel) and records it
    /// for undo. The recorded index is always the one the sound actually
    /// landed at, resolved from the list's new count when `index` was -1, so
    /// undo always names the right position to delete from.
    pub fn addSound(self: *Editor, index: i32, record: SoundRecord) EditError!void {
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.addSound(index, record));
        var none: [0]SoundRecord = .{};
        var total: usize = 0;
        _ = self.bridge.sounds(&none, &total);
        const actual: usize = if (index >= 0) @intCast(index) else (if (total == 0) 0 else total - 1);
        self.history.recordAssumeCapacity(self.allocator, .{ .sound_add = .{ .index = actual, .record = record } }, 0);
        self.sounds_generation +%= 1;
    }

    /// Replaces the sound at `index` with `record`, one undo step per
    /// gesture (merged the same way `place` merges a drag): a later edit of
    /// the same sound within the same gesture updates the entry's `after`
    /// rather than pushing a new one, and an edit that lands back on the
    /// gesture's own `before` drops the entry entirely, the same as `place`.
    pub fn editSound(self: *Editor, index: usize, record: SoundRecord, gesture: u32) EditError!void {
        const before = try self.readSoundAt(index);
        if (std.meta.eql(before, record)) return;
        const merge_entry = self.mergeable(gesture, .sound_edit);
        const merging = if (merge_entry) |entry| entry.command.sound_edit.index == index else false;
        if (!merging) try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.setSound(@intCast(index), record));
        if (merging) {
            const entry = merge_entry.?;
            if (std.meta.eql(entry.command.sound_edit.before, record)) {
                self.history.dropTop(self.allocator);
            } else {
                entry.command.sound_edit.after = record;
                self.history.touchTop(self.allocator);
            }
        } else {
            self.history.recordAssumeCapacity(self.allocator, .{ .sound_edit = .{ .index = index, .before = before, .after = record } }, gesture);
        }
        self.sounds_generation +%= 1;
    }

    /// Reads the record at `index` first, so undo can re-add it exactly
    /// there (BkEditorDeleteSound has no restore call of its own to undo
    /// through, unlike an object's tombstone - a plain `addSound` at the
    /// same index does the same job).
    pub fn deleteSound(self: *Editor, index: usize) EditError!void {
        const record = try self.readSoundAt(index);
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.deleteSound(@intCast(index)));
        self.history.recordAssumeCapacity(self.allocator, .{ .sound_delete = .{ .index = index, .record = record } }, 0);
        self.sounds_generation +%= 1;
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
            .sound_add => |a| {
                if (forwards) try self.noteOutcome(self.bridge.addSound(@intCast(a.index), a.record)) else try self.noteOutcome(self.bridge.deleteSound(@intCast(a.index)));
                self.sounds_generation +%= 1;
            },
            .sound_edit => |e| {
                const value = if (forwards) e.after else e.before;
                try self.noteOutcome(self.bridge.setSound(@intCast(e.index), value));
                self.sounds_generation +%= 1;
            },
            .sound_delete => |d| {
                if (forwards) try self.noteOutcome(self.bridge.deleteSound(@intCast(d.index))) else try self.noteOutcome(self.bridge.addSound(@intCast(d.index), d.record));
                self.sounds_generation +%= 1;
            },
        }
    }

    fn removeFrom(self: *Editor, link_id: i32) EditError!void {
        const index = self.document.indexOf(link_id) orelse return error.Failed;
        try self.noteOutcome(self.bridge.deleteObject(link_id));
        _ = self.document.objects.orderedRemove(index);
        if (self.selection == link_id) self.selection = null;
    }

    /// Needs one free slot in `document.objects`, which `undo` and `redo`
    /// reserve before they replay: once the bridge has restored the object,
    /// the document must not be able to miss it.
    fn restoreInto(self: *Editor, object: ObjectRecord, index: usize) EditError!void {
        try self.noteOutcome(self.bridge.restoreObject(object.link_id));
        self.document.objects.insertAssumeCapacity(@min(index, self.document.objects.items.len), object);
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
        // lost to an allocation failure, nor a restored object be missing
        // from the document.
        try self.history.redo_stack.ensureUnusedCapacity(self.allocator, 1);
        try self.document.objects.ensureUnusedCapacity(self.allocator, 1);
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
        try self.document.objects.ensureUnusedCapacity(self.allocator, 1);
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

test "an open the engine failed while building empties the document" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try editor.setMapType(3);
    fake.fail_build = true;
    try std.testing.expectError(error.Failed, editor.open("fixture.bzm"));
    try std.testing.expectEqualStrings("the engine threw", editor.status());
    try std.testing.expectEqual(@as(usize, 0), editor.document.path.items.len);
    try std.testing.expectEqual(@as(usize, 0), editor.document.objects.items.len);
    try std.testing.expect(!(try editor.undo()));
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

test "close forgets the document, its history and selection, and leaves it clean" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    const link = try editor.addObject("T34", 60, 60, 0, 1);
    editor.selection = link;
    try std.testing.expect(editor.dirty());
    editor.close();
    try std.testing.expectEqual(@as(usize, 0), editor.document.path.items.len);
    try std.testing.expectEqual(@as(usize, 0), editor.document.objects.items.len);
    try std.testing.expectEqual(@as(usize, 0), editor.document.diplomacy.items.len);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(!editor.history.canUndo());
    try std.testing.expect(!editor.history.canRedo());
    try std.testing.expect(!(try editor.undo()));
    try std.testing.expect(editor.selection == null);
    try std.testing.expectEqual(@as(usize, 0), editor.status().len);
    // A second close, or one with nothing open, is harmless; the map opens again.
    editor.close();
    try editor.open("fixture.bzm");
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(!editor.dirty());
}

test "save moves the document to the saved path" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    editor.files = fake_files.files();
    try editor.open("fixture.bzm");
    try editor.save("renamed.bzm");
    try std.testing.expectEqualStrings("renamed.bzm", editor.document.path.items);
    try editor.save(editor.document.path.items); // the document's own path
    try std.testing.expectEqualStrings("renamed.bzm", editor.document.path.items);
}

test "a save that cannot copy its path fails before the bridge writes, and keeps the path" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    editor.files = fake_files.files();
    try editor.open("fixture.bzm");
    try editor.setMapType(3);
    var failing = std.testing.FailingAllocator.init(std.testing.allocator, .{ .fail_index = 0 });
    editor.allocator = failing.allocator();
    try std.testing.expectError(error.OutOfMemory, editor.save("renamed.bzm"));
    editor.allocator = std.testing.allocator;
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(fake.calls.items[fake.calls.items.len - 1].kind != .save);
    try std.testing.expect(editor.dirty());
}

test "save: a temp file beside the map, then the swap, in order; one .bak per session" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    // The "old" bytes at fixture.bzm's OS path, as if it already existed on
    // disk - the first save's backup copies exactly these.
    try fake_files.write("fixture.bzm", "old bytes");
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    editor.files = fake_files.files();
    try editor.open("fixture.bzm");

    try editor.save("fixture.bzm");
    try std.testing.expectEqualStrings("old bytes", fake_files.contents("fixture.bzm.bak").?);
    try std.testing.expectEqualStrings("fake map: 3 objects", fake_files.contents("fixture.bzm").?);
    try std.testing.expect(fake_files.contents("fixture.~save.bzm") == null); // swapped away
    // The op order: the temp is written (fake_bridge.saveMap -> FakeFiles.write),
    // then copied to .bak, then the temp is renamed over the real path.
    var saw_write = false;
    var saw_copy = false;
    var saw_rename = false;
    for (fake_files.op_log.items) |op| {
        switch (op.kind) {
            .write => if (std.mem.eql(u8, op.from, "fixture.~save.bzm")) {
                try std.testing.expect(!saw_copy and !saw_rename);
                saw_write = true;
            },
            .copy => if (std.mem.eql(u8, op.from, "fixture.bzm")) {
                try std.testing.expect(saw_write and !saw_rename);
                saw_copy = true;
            },
            .rename => if (std.mem.eql(u8, op.from, "fixture.~save.bzm")) {
                try std.testing.expect(saw_write and saw_copy);
                saw_rename = true;
            },
            .delete => {},
        }
    }
    try std.testing.expect(saw_write and saw_copy and saw_rename);

    // A second save in the same session does not copy the backup again -
    // the .bak still reads the very first "old bytes".
    try editor.setMapType(3);
    try editor.save("fixture.bzm");
    try std.testing.expectEqualStrings("old bytes", fake_files.contents("fixture.bzm.bak").?);
}

test "save: a path in any game's data is refused before anything is written (D-18)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    // Another installation's Data, recognised by its marker alone.
    fake_files.data_roots = &.{"/Games/Other/Data"};
    fake.files = &fake_files;
    try fake_files.write("/Games/Other/Data/Maps/coldwinter.bzm", "shipped bytes");
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    editor.files = fake_files.files();
    editor.setBaseRoot("/Users/me/MapEditor/");
    try editor.open("fixture.bzm");
    try editor.setMapType(3);
    const ops_before = fake_files.op_log.items.len;

    for ([_][]const u8{
        "/Games/Other/Data/Maps/coldwinter.bzm", // another tree's Data, absolute
        "\\Games\\Other\\Data\\Maps\\coldwinter.bzm", // the same, in the engine's form
        "Data\\Maps\\Multiplayer\\coldwinter.bzm", // this installation's, relative
        "/Users/me/MapEditor/Data/Maps/a.bzm", // this installation's, absolute
        "/Users/me/MapEditor/mods/X/data/maps/a.bzm", // a mod's data
    }) |target| {
        try std.testing.expectError(error.Refused, editor.save(target));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "read-only") != null);
    }
    // Nothing written, copied, renamed or even deleted; the document is
    // still the one it was, and still has its changes.
    try std.testing.expectEqual(ops_before, fake_files.op_log.items.len);
    try std.testing.expectEqualStrings("shipped bytes", fake_files.contents("/Games/Other/Data/Maps/coldwinter.bzm").?);
    try std.testing.expect(fake_files.contents("/Games/Other/Data/Maps/coldwinter.bzm.bak") == null);
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(editor.dirty());

    // The user's own folder, even one called Data with no marker, saves.
    try editor.save("/Users/me/Data/maps/mine.bzm");
    try std.testing.expectEqualStrings("/Users/me/Data/maps/mine.bzm", editor.document.path.items);
    try std.testing.expect(!editor.dirty());
}

test "save: a new file gets no .bak, but is never backed up later either" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    var editor = try openFixture(&fake);
    defer editor.deinit();
    editor.files = fake_files.files();

    try editor.save("new-map.bzm"); // nothing existed at new-map.bzm before
    try std.testing.expect(fake_files.contents("new-map.bzm.bak") == null);
    try editor.setMapType(3);
    try editor.save("new-map.bzm"); // a later version exists now - still no .bak
    try std.testing.expect(fake_files.contents("new-map.bzm.bak") == null);
}

test "save: a refused bridge write leaves the original bytes, no temp, the map dirty" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    try fake_files.write("fixture.bzm", "original");
    var editor = try openFixture(&fake);
    defer editor.deinit();
    editor.files = fake_files.files();
    try editor.setMapType(3);

    fake.fail_save = true;
    try std.testing.expectError(error.Failed, editor.save("fixture.bzm"));
    try std.testing.expectEqualStrings("original", fake_files.contents("fixture.bzm").?);
    try std.testing.expect(fake_files.contents("fixture.~save.bzm") == null);
    try std.testing.expect(fake_files.contents("fixture.bzm.bak") == null);
    try std.testing.expect(editor.dirty());
}

test "save: a failed swap leaves the original bytes and no temp" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    try fake_files.write("fixture.bzm", "original");
    var editor = try openFixture(&fake);
    defer editor.deinit();
    editor.files = fake_files.files();
    try editor.setMapType(3);

    fake_files.fail_rename = true;
    try std.testing.expectError(error.Failed, editor.save("fixture.bzm"));
    try std.testing.expectEqualStrings("original", fake_files.contents("fixture.bzm").?);
    try std.testing.expect(fake_files.contents("fixture.~save.bzm") == null);
    try std.testing.expect(std.mem.startsWith(u8, editor.status(), "could not replace fixture.bzm: "));
    try std.testing.expect(editor.dirty());
}

test "save: a failed backup stops before the swap" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    try fake_files.write("fixture.bzm", "original");
    var editor = try openFixture(&fake);
    defer editor.deinit();
    editor.files = fake_files.files();
    try editor.setMapType(3);

    fake_files.fail_copy = true;
    try std.testing.expectError(error.Failed, editor.save("fixture.bzm"));
    try std.testing.expectEqualStrings("original", fake_files.contents("fixture.bzm").?);
    try std.testing.expect(fake_files.contents("fixture.~save.bzm") == null);
    try std.testing.expect(fake_files.contents("fixture.bzm.bak") == null);
    try std.testing.expect(std.mem.startsWith(u8, editor.status(), "could not keep a backup of fixture.bzm: "));
    try std.testing.expect(editor.dirty());
}

test "save: with no files, saving is refused rather than done unsafely" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setMapType(3);
    try std.testing.expectError(error.Failed, editor.save("fixture.bzm"));
    try std.testing.expectEqualStrings("saving needs a file system", editor.status());
    try std.testing.expect(editor.dirty());
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

test "sound add, undo, redo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var record: SoundRecord = .{ .x = 50, .y = 50, .repeat_ms = 1000, .min_radius = 1, .max_radius = 5 };
    record.setName("Explosion");
    try editor.addSound(-1, record);
    try std.testing.expectEqual(@as(usize, 1), fake.sounds_list.items.len);
    try std.testing.expectEqualStrings("Explosion", fake.sounds_list.items[0].nameSlice());
    try std.testing.expect(editor.dirty());
    const generation_after_add = editor.sounds_generation;
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 0), fake.sounds_list.items.len);
    try std.testing.expect(editor.sounds_generation != generation_after_add);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 1), fake.sounds_list.items.len);
}

test "sound edit, undo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var seed: SoundRecord = .{ .x = 50, .y = 50, .min_radius = 1, .max_radius = 5 };
    seed.setName("Explosion");
    try fake.addSoundFixture(seed);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var edited = seed;
    edited.min_radius = 2;
    edited.max_radius = 9;
    try editor.editSound(0, edited, 0);
    try std.testing.expectEqual(@as(i32, 2), fake.sounds_list.items[0].min_radius);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(i32, 1), fake.sounds_list.items[0].min_radius);
    try std.testing.expectEqual(@as(i32, 5), fake.sounds_list.items[0].max_radius);
}

test "delete then undo restores the same sound at the same index" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var first: SoundRecord = .{ .x = 10, .y = 10 };
    first.setName("Wind");
    var second: SoundRecord = .{ .x = 20, .y = 20, .max_radius = 3 };
    second.setName("Explosion");
    try fake.addSoundFixture(first);
    try fake.addSoundFixture(second);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.deleteSound(0);
    try std.testing.expectEqual(@as(usize, 1), fake.sounds_list.items.len);
    try std.testing.expectEqualStrings("Explosion", fake.sounds_list.items[0].nameSlice());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 2), fake.sounds_list.items.len);
    try std.testing.expectEqualStrings("Wind", fake.sounds_list.items[0].nameSlice());
    try std.testing.expectEqualStrings("Explosion", fake.sounds_list.items[1].nameSlice());
}

test "a refused sound add leaves history and generation unchanged" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var off_map: SoundRecord = .{ .x = 9999, .y = 9999 };
    off_map.setName("Explosion");
    const generation_before = editor.sounds_generation;
    const undo_depth_before = editor.history.undo_stack.items.len;
    try std.testing.expectError(error.Refused, editor.addSound(-1, off_map));
    try std.testing.expectEqual(@as(usize, 0), fake.sounds_list.items.len);
    try std.testing.expectEqual(generation_before, editor.sounds_generation);
    try std.testing.expectEqual(undo_depth_before, editor.history.undo_stack.items.len);
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

test "a gesture that ends where it began leaves no undo step and a clean map" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    try editor.place(1, .{ .x = 50, .y = 40, .dir = 0, .player = 0 }, gesture);
    try std.testing.expect(editor.dirty());
    try editor.place(1, .{ .x = 40, .y = 40, .dir = 0, .player = 0 }, gesture);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(!(try editor.undo()));
    try std.testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
    // And the gesture can go on from there as a fresh step.
    try editor.place(1, .{ .x = 60, .y = 40, .dir = 0, .player = 0 }, gesture);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(f32, 40), editor.document.find(1).?.x);
    try std.testing.expect(!editor.dirty());
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

test "a diplomacy or attacking side out of range is a caller bug and changes nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectError(error.Failed, editor.setDiplomacy(1, 3));
    try std.testing.expectEqualStrings("3 is no diplomacy: 0 and 1 are the two sides, 2 is neutral", editor.status());
    try std.testing.expectError(error.Failed, editor.setDiplomacy(1, 256)); // a BYTE would make it 0
    try std.testing.expectError(error.Failed, editor.setAttackingSide(2));
    try std.testing.expectError(error.Failed, editor.setAttackingSide(-1));
    try std.testing.expectEqualSlices(i32, &.{ 0, 1 }, editor.document.diplomacy.items);
    try std.testing.expectEqualSlices(i32, &.{ 0, 1 }, fake.diplomacy_table.items);
    try std.testing.expectEqual(@as(i32, 0), editor.document.info.attacking_side);
    try std.testing.expect(!editor.dirty());
}

test "saving marks clean, and undoing past the save makes it dirty again" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var fake_files = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake_files.deinit();
    fake.files = &fake_files;
    var editor = try openFixture(&fake);
    defer editor.deinit();
    editor.files = fake_files.files();
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

test "an undo that cannot make room restores nothing, and one that restores is in the document" {
    // Every allocation the undo of a delete makes, failed in turn, with the
    // document's list full so that reinserting the object would need one.
    var fail_index: usize = 0;
    while (fail_index < 4) : (fail_index += 1) {
        var fake = try testFixture(std.testing.allocator);
        defer fake.deinit();
        var editor = try openFixture(&fake);
        defer editor.deinit();
        try editor.delete(1);
        editor.document.objects.shrinkAndFree(std.testing.allocator, editor.document.objects.items.len);
        var failing = std.testing.FailingAllocator.init(std.testing.allocator, .{ .fail_index = fail_index });
        editor.allocator = failing.allocator();
        const undone = editor.undo();
        editor.allocator = std.testing.allocator;
        const in_bridge = for (fake.objects_list.items) |object| {
            if (object.link_id == 1) break true;
        } else false;
        try std.testing.expectEqual(in_bridge, editor.document.find(1) != null);
        if (undone) |_| {
            try std.testing.expect(in_bridge);
        } else |err| {
            try std.testing.expectEqual(error.OutOfMemory, err);
            try std.testing.expect(!in_bridge);
            try std.testing.expect(try editor.undo()); // and it can still be undone
            try std.testing.expect(editor.document.find(1) != null);
        }
    }
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
