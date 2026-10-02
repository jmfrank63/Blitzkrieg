const std = @import("std");
const bridge_mod = @import("bridge.zig");
const fake_mod = @import("fake_bridge.zig");
const document_mod = @import("document.zig");
const history_mod = @import("history.zig");
const tools = @import("tools.zig");
const files_mod = @import("files.zig");
const shipped_mod = @import("shipped.zig");
const records = @import("records.zig");
const core_filters = @import("filters.zig");
const checks = @import("checks.zig");
const layers_mod = @import("layers.zig");
const rmg_mod = @import("rmg.zig");
const Bridge = bridge_mod.Bridge;
const EditError = bridge_mod.EditError;
const ObjectRecord = bridge_mod.ObjectRecord;
const SoundRecord = bridge_mod.SoundRecord;
const PaintCell = bridge_mod.PaintCell;
const VsoKind = bridge_mod.VsoKind;
const VsoView = bridge_mod.VsoView;
const VsoDescriptor = bridge_mod.VsoDescriptor;
const FakeBridge = fake_mod.FakeBridge;
const FakeStartCommand = fake_mod.FakeStartCommand;
const FakeReservePosition = fake_mod.FakeReservePosition;
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
    /// The selection's other members (M3, D-25): link IDs, the anchor
    /// `selection` always among them when it is set - one map holds both, so
    /// every single-selection reader keeps working and the Selector's set
    /// semantics sit beside them. Plain click: set = {link}; Ctrl+click
    /// toggles; a band replaces the whole set.
    selection_set: std.AutoHashMapUnmanaged(i32, void) = .empty,
    next_gesture: u32 = 1,
    /// Bumped by every sound-list change - addSound, editSound, deleteSound,
    /// and their undo/redo alike (`replay`'s own three cases) - so the panel
    /// knows to read the list again; it has no mirror of its own to compare
    /// against (unlike `document.objects`, the bridge's sound list is read
    /// fresh through `bridge.sounds` whenever this changes).
    sounds_generation: u32 = 0,
    /// One counter per record kind, bumped by every record edit of that kind
    /// and by its undo and redo (`editRecord`, `replay`), so a panel or a
    /// marker layer knows to read the records again.
    record_generations: std.EnumArray(records.Kind, u32) = std.EnumArray(records.Kind, u32).initFill(0),
    /// Bumped by every road and river edit, its undo and its redo, and by an
    /// open or a close, so the Roads & Rivers panel and the markers know to
    /// read the roads and rivers again (the bridge holds them; the core keeps
    /// no mirror of their sampled points).
    vso_generation: u32 = 0,
    /// Bumped by every bridge edit, its undo and its redo, and by an open or
    /// a close, so the Bridges panel and the markers read the bridges again
    /// (04-06; the bridge holds them).
    bridges_generation: u32 = 0,
    /// The same for entrenchments (04-08): bumped with every edit of the
    /// `objects` scope (a trench draw or delete, and every bridge or fence
    /// edit, which may renumber nothing but costs a panel one re-read), its
    /// undo and redo, and an open or a close.
    entrenchments_generation: u32 = 0,
    /// The same for altitudes (M3, D-19): bumped by every setAltitudes, its
    /// undo and redo, and an open or a close, so whatever shows the terrain's
    /// heights (the minimap's gradient, the Heights panel) reads them again.
    altitudes_generation: u32 = 0,
    /// The players (05-05, D-30): bumped by a player add or delete, their undo
    /// and redo, and an open or a close, so the Diplomacy and Unit Creation
    /// panels read the table and the entries again.
    players_generation: u32 = 0,
    /// The object filters (M3, D-31): the shipped Data/Editor/filter.xml
    /// merged with the user <UserRoot>mapeditor/filter.xml, read through the
    /// bridge once at startup (`loadFilters`). The Filters Composer's
    /// New/Delete/Rename and word edits mutate this list in place - filters
    /// are installation-level data, never map data, so no history and no
    /// document dirtying - and `saveFilters` writes the user 1 entries back
    /// through the bridge. Panels re-read when `filters_generation` moves.
    filters: std.ArrayListUnmanaged(bridge_mod.ObjectFilter) = .empty,
    filters_generation: u32 = 0,
    /// The Layers menu (M3, D-32): what the renderer shows, remembered here
    /// and re-applied to it after every open and new map (`applyLayers`) - the
    /// MFC editor's check marks and the scene's own flags drifted apart across
    /// an open. Renderer state: never in the document, the history or a save.
    /// `layers_mask` is what the renderer can draw (the bridge's answer, read
    /// at the first apply); `layers_generation` moves with every change so the
    /// menu knows to redraw; `fire_sent_key` is what the bridge was last told
    /// for the fire ranges (`syncFireRange` resends when it moves).
    layers: layers_mod.State = .{},
    layers_mask: u32 = layers_mod.all_bits,
    layers_generation: u32 = 0,
    fire_sent_key: ?u64 = null,
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
    /// Set when a bridge-logged edit (`.edit`) failed part-way through an
    /// undo or redo and the tokens already replayed could not be put back:
    /// the bridge's own edit log and the history no longer agree, so every
    /// further undo and redo is refused with one clear message instead of
    /// failing on the same entry forever (WR-B01). An open or a close clears it.
    replay_broken: bool = false,

    pub fn init(allocator: std.mem.Allocator, b: Bridge) Editor {
        return .{ .allocator = allocator, .bridge = b };
    }

    pub fn deinit(self: *Editor) void {
        self.document.deinit(self.allocator);
        self.history.deinit(self.allocator);
        self.filters.deinit(self.allocator);
        self.selection_set.deinit(self.allocator);
        var backed_up_keys = self.backed_up.keyIterator();
        while (backed_up_keys.next()) |key| self.allocator.free(key.*);
        self.backed_up.deinit(self.allocator);
        self.* = undefined;
    }

    /// The selection set with the anchor, ascending - the order edits apply
    /// in and tests read. Owned by the caller (`free` with the allocator).
    pub fn selectionMembers(self: *Editor, allocator: std.mem.Allocator) EditError![]i32 {
        var members: std.ArrayListUnmanaged(i32) = .empty;
        errdefer members.deinit(allocator);
        if (self.selection) |anchor| {
            if (!self.selection_set.contains(anchor)) try members.append(allocator, anchor);
        }
        var it = self.selection_set.keyIterator();
        while (it.next()) |key| try members.append(allocator, key.*);
        std.mem.sort(i32, members.items, {}, std.sort.asc(i32));
        return members.toOwnedSlice(allocator);
    }

    pub fn selectionCount(self: *const Editor) usize {
        var count = self.selection_set.count();
        if (self.selection) |anchor| {
            if (!self.selection_set.contains(anchor)) count += 1;
        }
        return count;
    }

    pub fn isSelected(self: *const Editor, link_id: i32) bool {
        return (self.selection != null and self.selection.? == link_id) or self.selection_set.contains(link_id);
    }

    /// The plain click: the set becomes exactly {link_id}.
    pub fn selectOnly(self: *Editor, link_id: i32) void {
        self.selection_set.clearRetainingCapacity();
        self.selection = link_id;
    }

    /// The band's answer (M3, D-25): the selection becomes exactly `members`
    /// (unique, as the picks answer them); empty clears it. The anchor is the
    /// first member, so the properties panel shows the band's first object.
    pub fn selectionReplace(self: *Editor, members: []const i32) void {
        self.selection_set.clearRetainingCapacity();
        self.selection = if (members.len > 0) members[0] else null;
        for (members[if (members.len > 0) 1 else 0..]) |member| {
            if (self.selection == member) continue;
            self.selection_set.put(self.allocator, member, {}) catch return;
        }
    }

    /// Everything selected goes (a press on empty ground, a right click
    /// alone): anchor and set.
    pub fn clearSelection(self: *Editor) void {
        self.selection = null;
        self.selection_set.clearRetainingCapacity();
    }

    /// Ctrl+click (M3, D-25): the clicked object's membership flips. The
    /// anchor follows the click either way - a member toggled out leaves the
    /// anchor with another member (the set is what stays selected), and the
    /// last one out leaves nothing selected.
    pub fn selectionToggle(self: *Editor, link_id: i32) void {
        if (self.isSelected(link_id)) {
            const was_anchor = self.selection == link_id;
            _ = self.selection_set.remove(link_id);
            if (was_anchor) {
                self.selection = null;
                var it = self.selection_set.keyIterator();
                if (it.next()) |key| {
                    self.selection = key.*;
                    _ = self.selection_set.remove(key.*);
                }
            }
        } else {
            if (self.selection) |anchor| {
                if (anchor != link_id) self.selection_set.put(self.allocator, anchor, {}) catch return;
            }
            self.selection = link_id;
        }
    }

    fn clearSelectionSet(self: *Editor) void {
        self.selection_set.clearRetainingCapacity();
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

    /// After a delete (or its redo) that answered ok: the bridge's summary of
    /// what else the delete changed - start commands it left, a script ID a
    /// group still names - goes into the status as a note. It is not an error;
    /// the next command's outcome replaces it, so a later scripted save is not
    /// judged failed by it.
    fn noteCascade(self: *Editor) void {
        const message = self.bridge.lastMessage();
        if (message.len == 0) return;
        const len = @min(message.len, self.status_buffer.len);
        @memcpy(self.status_buffer[0..len], message[0..len]);
        self.status_len = len;
    }

    /// A note for the status bar that is not a bridge answer: a tool saying
    /// why a gesture did nothing (a road finished with one point). The next
    /// command's outcome replaces it.
    pub fn note(self: *Editor, message: []const u8) void {
        self.setStatus("", message);
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
        self.replay_broken = false;
        self.selection = null;
        self.clearSelectionSet();
        self.bumpDocumentGenerations();
        self.applyLayers();
    }

    /// File -> New (M3, D-23): the engine builds the map of `params`
    /// (patches per axis, season, name, mod folder) and it opens as a
    /// never-saved document - no path, clean history, every generation
    /// moved, exactly an open's own reset. The params' name is the caller's
    /// to keep for the title and the first Save As: the document holds no
    /// name of its own, and the map none at all. A failure works like an
    /// open's: the document is emptied rather than left holding fields of a
    /// map the bridge could not finish building.
    pub fn newMap(self: *Editor, params: bridge_mod.NewMapParams) EditError!void {
        var info: bridge_mod.MapInfo = .{};
        const created = self.bridge.newMap(params, &info);
        self.noteOutcome(created) catch |err| {
            if (created == .failed) self.closeDocument();
            return err;
        };
        self.document.deinit(self.allocator);
        self.document = .{};
        self.document.reload(self.allocator, self.bridge, "", info) catch |err| {
            self.setStatus("the map was created but its fields could not be read: ", self.bridge.lastMessage());
            self.closeDocument();
            return err;
        };
        self.history.clear(self.allocator);
        self.replay_broken = false;
        self.selection = null;
        self.clearSelectionSet();
        self.bumpDocumentGenerations();
        self.applyLayers();
    }

    /// Every generation counter a panel or marker layer keys on moves: a new
    /// map (or none) changes every list they show (IN-B04).
    fn bumpDocumentGenerations(self: *Editor) void {
        self.vso_generation +%= 1;
        self.bridges_generation +%= 1;
        self.entrenchments_generation +%= 1;
        self.sounds_generation +%= 1;
        self.altitudes_generation +%= 1;
        self.players_generation +%= 1;
        for (std.enums.values(records.Kind)) |kind| self.record_generations.set(kind, self.record_generations.get(kind) +% 1);
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
        self.replay_broken = false;
        self.selection = null;
        self.clearSelectionSet();
        self.bumpDocumentGenerations();
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

    /// A counter that moves whenever the map's content may have changed (an
    /// edit, a merge into one, an undo, a redo, an open, a new map, a close):
    /// what the Minimap's texture keys its rebuild on (05-07, D-14). It says
    /// nothing about which part changed.
    pub fn mapRevision(self: *const Editor) u32 {
        return self.history.revision;
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
        var pointer: tools.Pointer = .{ .world_x = 0, .world_y = 0, .map_x = 0, .map_y = 0, .screen_x = sx, .screen_y = sy };
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

    /// The placement rule's own answer (M3, D-20): where the fit would put
    /// (x, y) for `name` - the input, when the session's Fit Objects To Grid
    /// is off or the kind is one the rule does not fit. The tools snap the
    /// positions they are about to add or drag with this, the way the MFC's
    /// placer snaps before it edits: the map never holds a position the
    /// caller did not mean, and undo replays raw.
    pub fn snapToGrid(self: *Editor, name: []const u8, x: f32, y: f32) struct { x: f32, y: f32 } {
        var name_z: [bridge_mod.name_capacity:0]u8 = undefined;
        const len = @min(name.len, bridge_mod.name_capacity);
        @memcpy(name_z[0..len], name[0..len]);
        name_z[len] = 0;
        var out_x = x;
        var out_y = y;
        _ = self.bridge.snapToGrid(@ptrCast(&name_z), x, y, &out_x, &out_y);
        return .{ .x = out_x, .y = out_y };
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
        self.noteCascade();
        self.bumpCascadeGenerations();
        const object = self.document.objects.orderedRemove(index);
        if (self.selection == link_id) self.selection = null;
        _ = self.selection_set.remove(link_id);
        self.history.recordAssumeCapacity(self.allocator, .{ .delete = .{ .object = object, .index = index } }, 0);
    }

    /// The selection's delete-all (M3, D-25): every member through the M2
    /// cascade, ONE undo step. A member's refused delete refuses the whole
    // delete and changes nothing - the members are checked against the same
    // refusals a single delete meets, before any of them goes (a span, a
    // trench piece, an unknown type; a bridge or an entrenchment is not in a
    // selection to begin with, the picks pass them over). The link IDs need
    // not be sorted; the recorded order is the one given.
    pub fn deleteMany(self: *Editor, link_ids: []const i32) EditError!void {
        if (link_ids.len == 0) return;
        // Check every member first (reserve included), so a refusal leaves
        // nothing half-done.
        for (link_ids, 0..) |link_id, i| {
            if (std.mem.indexOfScalar(i32, link_ids[0..i], link_id) != null) return error.Failed;
            if (self.document.indexOf(link_id) == null) return error.Failed;
        }
        try self.history.reserve(self.allocator);
        var deleted: std.ArrayListUnmanaged(history_mod.DeletedRecord) = .empty;
        errdefer deleted.deinit(self.allocator);
        try deleted.ensureTotalCapacity(self.allocator, link_ids.len);
        for (link_ids) |link_id| {
            // The index at the member's own deletion: the earlier removals
            // shifted the list, so it is read fresh each time.
            const index = self.document.indexOf(link_id) orelse return error.Failed;
            try self.noteOutcome(self.bridge.deleteObject(link_id));
            const object = self.document.objects.orderedRemove(index);
            deleted.appendAssumeCapacity(.{ .object = object, .index = index });
            if (self.selection == link_id) self.selection = null;
            _ = self.selection_set.remove(link_id);
        }
        self.noteCascade();
        self.bumpCascadeGenerations();
        self.history.recordAssumeCapacity(self.allocator, .{ .multi_delete = .{ .deleted = deleted } }, 0);
    }

    /// The selection's group move (M3, D-25): every member of `links` moved
    /// by one (dx, dy) delta in MAP units through one BkEditorMoveObjects
    /// call - one bridge edit per call, the calls of one drag (`gesture`)
    /// merged into one undo step the way a paint's are. The document's own
    /// copies move by the same delta here (the bridge moved whole records;
    /// direction and owner came back as they were). A refusal - a member the
    /// bridge cannot move, a destination off the map - changes nothing.
    pub fn moveSelection(self: *Editor, links: []const i32, dx: f32, dy: f32, gesture: u32) EditError!void {
        if (links.len == 0 or (dx == 0 and dy == 0)) return;
        var prepared = try self.prepareEdit(gesture, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.moveObjects(links, dx, dy, &token));
        self.commitEdit(&prepared, token, gesture, .objects);
        for (links) |link_id| {
            const object = self.document.find(link_id) orelse continue;
            object.x += dx;
            object.y += dy;
        }
    }

    /// The properties' commit (M3, D-26): the masked fields of one object's
    /// record through one BkEditorSetObjectFields call - ONE edit per
    /// deactivation (`gesture` 0; the panel commits once). The document
    /// re-reads its objects (the flag swap renames and moves the record).
    /// A refusal - a bad player, a non-finite field, a formation on a kind
    /// that carries none - changes nothing. An edit that changes nothing
    /// records nothing (the token answers -1).
    pub fn applyObjectFields(self: *Editor, link_id: i32, fields: bridge_mod.ObjectFieldsEdit) EditError!void {
        return self.applyObjectFieldsMany(&.{link_id}, fields);
    }

    /// The same commit over several objects (the multi-selection's fields):
    /// ONE undo step for the whole commit - one bridge edit per member, the
    /// tokens of one call merged. A member's refusal stops the commit there
    /// and keeps what applied (each member's edit is its own whole token,
    /// the MFC's own per-object multi set), and the status names it.
    pub fn applyObjectFieldsMany(self: *Editor, links: []const i32, fields: bridge_mod.ObjectFieldsEdit) EditError!void {
        return self.applyObjectFieldsInGesture(links, fields, 0);
    }

    /// The direction wheel's turn of the selection (M3, D-28/PARITY O6):
    /// every member faces `degrees` - the MFC frame's own answer to the
    /// wheel (TemplateEditorFrame1.cpp:1053-1090 turns each selected object,
    /// a soldier's formation for him, to the wheel's angle) - through the
    /// properties' angle field, so the record's direction is the MFC's own
    /// degrees formula. The calls of one drag (`gesture`) merge into ONE
    /// undo step, the way the group move's do; `gesture` 0 is a step of its
    /// own. A member already facing there records nothing.
    pub fn turnSelection(self: *Editor, links: []const i32, degrees: f32, gesture: u32) EditError!void {
        if (!std.math.isFinite(degrees)) return error.Refused;
        return self.applyObjectFieldsInGesture(links, .{ .mask = bridge_mod.ObjectFieldsEdit.angle_bit, .angle = degrees }, gesture);
    }

    fn applyObjectFieldsInGesture(self: *Editor, links: []const i32, fields: bridge_mod.ObjectFieldsEdit, gesture: u32) EditError!void {
        if (links.len == 0) return;
        for (links) |link_id| {
            if (self.document.find(link_id) == null) return error.Failed;
        }
        var prepared = try self.prepareEdit(gesture, .objects);
        defer prepared.tokens.deinit(self.allocator);
        if (prepared.entry) |entry| {
            try entry.command.edit.tokens.ensureUnusedCapacity(self.allocator, links.len);
        } else {
            try prepared.tokens.ensureTotalCapacity(self.allocator, links.len);
        }
        var applied: usize = 0;
        for (links) |link_id| {
            var token: i32 = -1;
            const outcome = self.bridge.setObjectFields(link_id, &fields, &token);
            if (outcome != .ok) {
                // The status carries the refusal. Nothing applied yet: the
                // refusal is the commit's answer; some members applied: a
                // partial commit stands (each member's edit is its own whole
                // token, the MFC's own per-object multi set).
                const message = self.bridge.lastMessage();
                const len = @min(message.len, self.status_buffer.len);
                @memcpy(self.status_buffer[0..len], message[0..len]);
                self.status_len = len;
                if (applied == 0) return bridge_mod.check(outcome);
                break;
            }
            self.status_len = 0;
            if (token >= 0) {
                applied += 1;
                if (prepared.entry) |entry| {
                    entry.command.edit.tokens.appendAssumeCapacity(token);
                    self.history.touchTop(self.allocator);
                } else {
                    prepared.tokens.appendAssumeCapacity(token);
                }
            }
        }
        if (prepared.entry == null and prepared.tokens.items.len != 0) {
            const tokens = prepared.tokens;
            prepared.tokens = .empty;
            self.history.recordAssumeCapacity(self.allocator, .{ .edit = .{ .tokens = tokens, .scope = .objects } }, gesture);
            self.bumpScope(.objects);
        }
        try self.reloadObjectsAfterEdit();
    }

    /// The drop's link (M3, D-27): `source` linked to `host` through
    /// BkEditorSetLink - ONE edit; the document re-reads. The drop asks
    /// `canLink` first (the cursor feedback); a refusal here names the rule.
    pub fn makeLink(self: *Editor, source: i32, host: i32) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.setLink(source, host, &token));
        if (token >= 0) self.commitEdit(&prepared, token, 0, .objects);
        try self.reloadObjectsAfterEdit();
    }

    /// The properties' units list unlink (M3, D-27): `link_id`'s nLinkWith
    /// back to 0, ONE edit; the document re-reads. Nothing linked records
    /// nothing.
    pub fn unlinkObject(self: *Editor, link_id: i32) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.unlink(link_id, &token));
        if (token >= 0) self.commitEdit(&prepared, token, 0, .objects);
        try self.reloadObjectsAfterEdit();
    }

    /// CheckForInserting's answer for a drop (M3, D-27): the link type -
    /// 0 garrison, 1 train coupling, 2 tow - refused naming the rule. A
    /// read; the status line is left alone.
    pub fn canLink(self: *Editor, source: i32, host: i32) EditError!i32 {
        var link_type: i32 = 0;
        const result = self.bridge.canLink(source, host, &link_type);
        if (result == .refused) {
            const message = self.bridge.lastMessage();
            const len = @min(message.len, self.status_buffer.len);
            @memcpy(self.status_buffer[0..len], message[0..len]);
            self.status_len = len;
            return error.Refused;
        }
        try bridge_mod.check(result);
        return link_type;
    }

    /// The Damage tool's hit (M3, D-29): the object's fHP moved by
    /// `delta` (the tool's percentage/100) with the MFC's clamps, ONE
    /// bridge edit; the document re-reads. The clicks of one gesture
    /// (`gesture`) merge, though a click is a gesture of its own. Missing
    /// stats refuse (the MFC's null dereference is not copied); the clamps
    /// leaving nothing to change records nothing.
    pub fn damageObject(self: *Editor, link_id: i32, mode: bridge_mod.DamageMode, delta: f32, gesture: u32) EditError!void {
        var prepared = try self.prepareEdit(gesture, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.damageObject(link_id, delta, @intFromEnum(mode), &token));
        if (token >= 0) self.commitEdit(&prepared, token, gesture, .objects);
        try self.reloadObjectsAfterEdit();
    }

    /// Deleting a host takes its passengers with it (M3, D-27): the members
    /// of the delete are the passengers - the records whose nLinkWith names
    /// the host - plus the host itself, passengers first, so ONE deleteMany
    /// undo restores the host before the passengers that point at it. The
    /// M1 refusal is lifted for exactly this path; the M2 refusals (a span,
    /// a trench piece) still refuse the delete whole. Passes through to a
    /// plain delete when the host carries none.
    pub fn deleteHost(self: *Editor, link_id: i32) EditError!void {
        var members: std.ArrayListUnmanaged(i32) = .empty;
        defer members.deinit(self.allocator);
        for (self.document.objects.items) |object| {
            if (object.link_with == link_id and object.link_id != link_id)
                members.append(self.allocator, object.link_id) catch return error.OutOfMemory;
        }
        if (members.items.len == 0) return self.delete(link_id);
        members.append(self.allocator, link_id) catch return error.OutOfMemory;
        try self.deleteMany(members.items);
    }

    /// The selection's Delete, host-aware (M3, D-25/D-27): every member and
    /// every passenger of every member, ONE undo step, duplicates collapsed.
    pub fn deleteSelection(self: *Editor) EditError!void {
        const members = try self.selectionMembers(self.allocator);
        defer self.allocator.free(members);
        if (members.len == 0) return;
        if (members.len == 1) return self.deleteHost(members[0]);
        var all: std.ArrayListUnmanaged(i32) = .empty;
        defer all.deinit(self.allocator);
        for (members) |member| {
            for (self.document.objects.items) |object| {
                if (object.link_with == member and object.link_id != member and
                    std.mem.indexOfScalar(i32, all.items, object.link_id) == null and
                    std.mem.indexOfScalar(i32, members, object.link_id) == null)
                    all.append(self.allocator, object.link_id) catch return error.OutOfMemory;
            }
        }
        for (members) |member| all.append(self.allocator, member) catch return error.OutOfMemory;
        try self.deleteMany(all.items);
    }

    /// An object's script ID (D-15): -1 none, else 0..32000. The bridge
    /// changes both map copies only (C7) and refuses a value out of range, an
    /// unknown or shared link ID and link ID 0, naming why, so a refusal
    /// changes nothing: not the document, not the history. An equal value
    /// records nothing; within one gesture (the same object) edits merge into
    /// one undo step, and one that lands back on the step's own before-value
    /// drops it, as `place` does.
    pub fn setScriptID(self: *Editor, link_id: i32, value: i32, gesture: u32) EditError!void {
        const object = self.document.find(link_id) orelse return error.Failed;
        const before = object.script_id;
        if (before == value) return;
        const merge_entry = self.mergeable(gesture, .script_id);
        const merging = if (merge_entry) |entry| entry.command.script_id.link_id == link_id else false;
        if (!merging) try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.setObjectScriptID(link_id, value));
        // The bridge's note when a group now holds this object back while a
        // start command or a reserve position names it (Pitfall 8).
        self.noteCascade();
        object.script_id = value;
        if (merging) {
            const entry = merge_entry.?;
            if (entry.command.script_id.before == value) {
                self.history.dropTop(self.allocator);
            } else {
                entry.command.script_id.after = value;
                self.history.touchTop(self.allocator);
            }
        } else {
            self.history.recordAssumeCapacity(self.allocator, .{ .script_id = .{ .link_id = link_id, .before = before, .after = value } }, gesture);
        }
    }

    /// Adds a player of `side` (0 or 1) just before the neutral entry (05-05,
    /// D-30): ONE undo step. The bridge keeps the table, the unit creation, the
    /// camera anchors and the re-owned objects; a refusal (17 entries, a bad
    /// side) changes nothing, not the bridge and not the history.
    pub fn addPlayer(self: *Editor, side: i32) EditError!void {
        var prepared = try self.prepareEdit(0, .players);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.addPlayer(side, &token));
        if (token < 0) return error.Failed;
        self.commitEdit(&prepared, token, 0, .players);
        try self.reloadPlayersAfterEdit();
    }

    /// Deletes player `player` (never the neutral; 05-05, D-30): its objects
    /// become the neutral's, the players above move down, ONE undo step.
    pub fn deletePlayer(self: *Editor, player: i32) EditError!void {
        var prepared = try self.prepareEdit(0, .players);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.deletePlayer(player, &token));
        if (token < 0) return error.Failed;
        self.commitEdit(&prepared, token, 0, .players);
        try self.reloadPlayersAfterEdit();
    }

    /// One player's unit creation, read through the record call (05-05, D-30).
    pub fn unitCreation(self: *Editor, player: usize) EditError!records.UnitCreation {
        if (player >= records.max_uc_slots) return error.Failed;
        var value: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.unit_creation, @intCast(player), self.allocator, &value));
        return value.unit_creation;
    }

    /// Puts one player's unit creation (a whole record) as ONE undo step per
    /// gesture, merging while the same player is edited. The slot count is raised
    /// to cover the player when the vector does not hold it yet; the rules are the
    /// bridge's, a refusal names the field and changes nothing.
    pub fn editUnitCreation(self: *Editor, player: usize, value: records.UnitCreation, gesture: u32) EditError!void {
        const covered = value.withPlayerCovered(player) orelse return error.Failed;
        const record: records.Value = .{ .unit_creation = covered };
        try self.editRecord(.unit_creation, @intCast(player), &record, gesture);
    }

    /// Check Map (05-05, D-33): every finding over what the editor holds - the
    /// document's objects, the table, each player's unit-creation party against
    /// partys.xml, and the roads and rivers with fewer than two control points
    /// (read through the bridge). A READ: nothing is edited, and the status line
    /// is left alone. `squad_names` are the type names of squads (the duplicate rule
    /// skips them; the catalogue the app holds knows which they are). The caller
    /// frees the slice with `allocator`.
    pub fn checkMap(self: *Editor, allocator: std.mem.Allocator, squad_names: []const []const u8) EditError![]checks.Finding {
        // The party table, and the party of each unit-creation entry.
        var parties: std.ArrayListUnmanaged(bridge_mod.UcName) = .empty;
        defer parties.deinit(allocator);
        var total: usize = 0;
        var none: [0]bridge_mod.UcName = .{};
        const sizing = self.bridge.unitCreationChoices(.parties, &none, &total);
        if ((sizing == .ok or sizing == .refused) and total > 0) {
            try parties.resize(allocator, total);
            if (self.bridge.unitCreationChoices(.parties, parties.items, &total) != .ok) parties.clearRetainingCapacity();
        }
        var party_names: std.ArrayListUnmanaged([]const u8) = .empty;
        defer party_names.deinit(allocator);
        for (parties.items) |*name| try party_names.append(allocator, name.nameSlice());

        var unit_party_buffers: std.ArrayListUnmanaged(records.UnitCreation) = .empty;
        defer unit_party_buffers.deinit(allocator);
        var slot_count: usize = 0;
        {
            var value: records.Value = undefined;
            if (self.bridge.readRecord(.unit_creation, 0, allocator, &value) == .ok) slot_count = value.unit_creation.slot_count;
        }
        var player: usize = 0;
        while (player < slot_count and player < records.max_uc_slots) : (player += 1) {
            var value: records.Value = undefined;
            if (self.bridge.readRecord(.unit_creation, @intCast(player), allocator, &value) != .ok) break;
            try unit_party_buffers.append(allocator, value.unit_creation);
        }
        var unit_parties: std.ArrayListUnmanaged([]const u8) = .empty;
        defer unit_parties.deinit(allocator);
        for (unit_party_buffers.items) |*unit| try unit_parties.append(allocator, unit.partySlice());

        // Roads and rivers with fewer than two control points.
        var short: std.ArrayListUnmanaged(checks.ShortVso) = .empty;
        defer short.deinit(allocator);
        inline for ([_]VsoKind{ .road, .river }) |kind| {
            var count: usize = 0;
            if (self.bridge.vsoCount(kind, &count) == .ok) {
                var index: usize = 0;
                while (index < count) : (index += 1) {
                    var view: VsoView = .{};
                    if (self.bridge.readVso(kind, @intCast(index), allocator, &view) != .ok) continue;
                    defer view.deinit(allocator);
                    if (view.control_points.len >= 2) continue;
                    const first: records.Vec3 = if (view.control_points.len > 0) view.control_points[0] else .{};
                    try short.append(allocator, .{ .kind = @intFromEnum(kind), .index = index, .x = first.x, .y = first.y, .control_points = view.control_points.len });
                }
            }
        }

        return checks.checkMap(allocator, .{
            .objects = self.document.objects.items,
            .players = self.document.diplomacy.items.len,
            .parties = party_names.items,
            .unit_parties = unit_parties.items,
            .squad_names = squad_names,
            .short_vsos = short.items,
        });
    }

    /// What `fixAll` did: how many findings it fixed, how many it left (a shared
    /// link ID, a destructive fix nobody confirmed, an object that was gone by
    /// then) and how many the bridge refused.
    pub const FixReport = struct { fixed: usize = 0, left: usize = 0, refused: usize = 0 };

    /// Fix all (05-05, D-33): the findings' fixes as ONE undo step. Each fix is the
    /// edit a person would make - a duplicate deleted, an invalid link cleared, an
    /// owner out of range moved to the neutral, a party set to the default - so a
    /// fix the bridge refuses (a record it keeps untouched) is counted and the
    /// rest go on. `confirmed` allows the destructive ones: an unknown-type object
    /// removed, a short road or river deleted (the MFC's silent
    /// RemoveNonExistingObjects, now asked first). The edits are recorded one by
    /// one and then folded into a single composite step; a failure part-way keeps
    /// what was applied inside that one step.
    pub fn fixAll(self: *Editor, findings: []const checks.Finding, confirmed: bool) EditError!FixReport {
        var report: FixReport = .{};
        const start = self.history.undo_stack.items.len;
        const players = self.document.diplomacy.items.len;
        if (players == 0) return error.Failed;
        // Duplicates first (the MFC's own order), the later records going first so no
        // index a later fix needs moves under it.
        for (findings) |finding| {
            if (finding.kind != .duplicate_object) continue;
            self.countFix(&report, finding, self.delete(finding.link_id));
        }
        for (findings) |finding| {
            switch (finding.kind) {
                .invalid_link => self.countFix(&report, finding, self.unlinkObject(finding.link_id)),
                .player_index => {
                    const fields: bridge_mod.ObjectFieldsEdit = .{ .mask = bridge_mod.ObjectFieldsEdit.player_bit, .player = @intCast(players - 1) };
                    self.countFix(&report, finding, self.applyObjectFields(finding.link_id, fields));
                },
                .unknown_party => {
                    const player: usize = @intCast(finding.player);
                    if (self.unitCreation(player)) |held| {
                        var wanted = held;
                        wanted.setParty(checks.default_party);
                        self.countFix(&report, finding, self.editUnitCreation(player, wanted, 0));
                    } else |err| self.countFix(&report, finding, err);
                },
                .duplicate_link => report.left += 1,
                .duplicate_object, .unknown_object_type, .short_vso => {},
            }
        }
        if (confirmed) {
            for (findings) |finding| {
                if (finding.kind == .unknown_object_type) self.countFix(&report, finding, self.delete(finding.link_id));
            }
            // The highest index of a kind first: a delete renumbers the ones after it.
            var wanted_kind: u8 = 0;
            while (wanted_kind < 2) : (wanted_kind += 1) {
                var indices: std.ArrayListUnmanaged(usize) = .empty;
                defer indices.deinit(self.allocator);
                for (findings) |finding| {
                    if (finding.kind == .short_vso and finding.vso_kind == wanted_kind) try indices.append(self.allocator, finding.vso_index);
                }
                std.mem.sort(usize, indices.items, {}, std.sort.desc(usize));
                for (indices.items) |index| {
                    const finding: checks.Finding = .{ .kind = .short_vso, .vso_kind = wanted_kind, .vso_index = index };
                    self.countFix(&report, finding, self.deleteVso(@enumFromInt(wanted_kind), index));
                }
            }
        } else {
            for (findings) |finding| {
                if (finding.needsConfirmation()) report.left += 1;
            }
        }
        try self.foldSince(start);
        return report;
    }

    fn countFix(self: *Editor, report: *FixReport, finding: checks.Finding, result: EditError!void) void {
        _ = self;
        _ = finding;
        if (result) |_| {
            report.fixed += 1;
        } else |err| switch (err) {
            // The object was gone by the time its turn came (an earlier fix took it).
            error.Failed => report.left += 1,
            else => report.refused += 1,
        }
    }

    /// Folds the undo entries recorded after `start` into one composite step, in
    /// order. Fewer than two stay as they are.
    fn foldSince(self: *Editor, start: usize) EditError!void {
        const stack = &self.history.undo_stack;
        if (stack.items.len < start + 2) return;
        var steps: std.ArrayListUnmanaged(Command) = .empty;
        errdefer steps.deinit(self.allocator);
        try steps.ensureTotalCapacity(self.allocator, stack.items.len - start);
        for (stack.items[start..]) |entry| steps.appendAssumeCapacity(entry.command);
        stack.shrinkRetainingCapacity(start);
        stack.appendAssumeCapacity(.{ .command = .{ .composite = .{ .steps = steps } }, .gesture = 0 });
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

    /// The generic record command (D-02). Reads the whole record at `key`
    /// through the bridge, puts `value` through the same bridge's `putRecord`
    /// and records the pair for undo. An equal value records nothing; within
    /// one gesture (same kind and key) edits merge into one undo step, and one
    /// that lands back on the step's own before-record drops it, as `place`
    /// and `editSound` do. History room is reserved before the bridge call, so
    /// once the bridge has committed, recording cannot fail. A refusal changes
    /// nothing: not the bridge, not the history, not the generation.
    pub fn editRecord(self: *Editor, kind: records.Kind, key: i32, value: *const records.Value, gesture: u32) EditError!void {
        if (std.meta.activeTag(value.*) != kind) return error.Failed;
        var before: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(kind, key, self.allocator, &before));
        errdefer before.deinit(self.allocator);
        if (before.eql(value.*)) {
            before.deinit(self.allocator);
            return;
        }
        var after = try value.clone(self.allocator);
        errdefer after.deinit(self.allocator);
        const merge_entry = self.mergeable(gesture, .record_edit);
        const merging = if (merge_entry) |entry|
            entry.command.record_edit.kind == kind and entry.command.record_edit.key == key
        else
            false;
        if (!merging) try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.putRecord(key, &after));
        if (merging) {
            const entry = merge_entry.?;
            const edit = &entry.command.record_edit;
            if (edit.before.eql(after)) {
                after.deinit(self.allocator);
                self.history.dropTop(self.allocator);
            } else {
                edit.after.deinit(self.allocator);
                edit.after = after;
                self.history.touchTop(self.allocator);
            }
            // The step keeps its own before-record; this call's is not needed.
            before.deinit(self.allocator);
        } else {
            self.history.recordAssumeCapacity(self.allocator, .{ .record_edit = .{ .kind = kind, .key = key, .before = before, .after = after } }, gesture);
            // Owned by the history now: the errdefers above cannot run after
            // this point, since nothing below can fail.
        }
        self.record_generations.set(kind, self.record_generations.get(kind) +% 1);
    }

    /// Sets a camera anchor to a world point: `slot` -1 is the neutral anchor,
    /// 0.. a player's. The z comes from the terrain (`groundHeight`), so a
    /// point off the map is refused before anything changes. Setting player N
    /// pads the vector with unset slots up to N + 1 and never shrinks it (C8);
    /// one undo step, and undo puts the old vector back exactly.
    pub fn setCameraAnchor(self: *Editor, slot: i32, wx: f32, wy: f32) EditError!void {
        if (slot < -1) return error.Failed;
        var z: f32 = 0;
        try self.noteOutcome(self.bridge.groundHeight(wx, wy, &z));
        var current: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.camera_anchors, 0, self.allocator, &current));
        defer current.deinit(self.allocator);
        const anchor: records.Vec3 = .{ .x = wx, .y = wy, .z = z };
        var wanted = current.camera_anchors;
        if (slot == -1) {
            wanted.neutral = anchor;
        } else wanted = wanted.withPlayer(@intCast(slot), anchor) orelse {
            self.setStatus("camera anchors: ", "the editor edits the anchors of 32 players at most");
            return error.Refused;
        };
        const value: records.Value = .{ .camera_anchors = wanted };
        try self.editRecord(.camera_anchors, 0, &value, 0);
    }

    /// Makes a camera anchor unset (`slot` -1 is the neutral one). The vector
    /// keeps its size.
    pub fn clearCameraAnchor(self: *Editor, slot: i32) EditError!void {
        if (slot < -1) return error.Failed;
        var current: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.camera_anchors, 0, self.allocator, &current));
        defer current.deinit(self.allocator);
        var wanted = current.camera_anchors;
        if (slot == -1) wanted.neutral = .{} else wanted = wanted.withPlayerCleared(@intCast(slot));
        const value: records.Value = .{ .camera_anchors = wanted };
        try self.editRecord(.camera_anchors, 0, &value, 0);
    }

    /// The map's script file name (D-20), copied into `out`: empty for None.
    /// Refused for a value the record cannot hold (64 characters or more): the
    /// map keeps it byte-exact and it is not editable here.
    pub fn scriptFileName(self: *Editor, out: *[records.script_file_capacity]u8) EditError![]const u8 {
        var value: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.script_file, 0, self.allocator, &value));
        defer value.deinit(self.allocator);
        out.* = value.script_file.name;
        return std.mem.sliceTo(out, 0);
    }

    /// Map -> Script (D-20): the map's script file becomes `name`, a bare name
    /// or empty for None, as one undo step. A name that carries a folder or
    /// ".lua" is Refused by the bridge and changes nothing; the value a file
    /// held when the map was opened is always accepted back, so the undo of
    /// this edit can restore a verbatim path. An equal value records nothing.
    pub fn setScriptFile(self: *Editor, name: []const u8) EditError!void {
        if (name.len >= records.script_file_capacity) {
            self.setStatus("script: ", "a script name is 63 characters at most");
            return error.Refused;
        }
        var file: records.ScriptFile = .{};
        file.setName(name);
        const value: records.Value = .{ .script_file = file };
        try self.editRecord(.script_file, 0, &value, 0);
    }

    /// The map's script areas (D-21) in list order, owned by the caller (free
    /// with the same allocator). Read one by one through the record path, so a
    /// map whose area names the record cannot hold is Refused whole.
    pub fn scriptAreas(self: *Editor, allocator: std.mem.Allocator) EditError![]records.ScriptArea {
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.script_area, allocator, &keys));
        defer allocator.free(keys);
        const out = try allocator.alloc(records.ScriptArea, keys.len);
        errdefer allocator.free(out);
        for (keys, out) |key, *area| {
            var value: records.Value = undefined;
            try bridge_mod.check(self.bridge.readRecord(.script_area, key, allocator, &value));
            area.* = value.script_area;
        }
        return out;
    }

    /// A new area appended to the map's list (D-21), one undo step (undo removes
    /// it, redo puts it back at the same index). `area` is in AI units, made by
    /// `scriptAreaFromVis`; the bridge refuses an empty or taken name, a centre
    /// off the map and a negative size, and a refusal changes nothing. Returns
    /// the index it took.
    pub fn addScriptArea(self: *Editor, area: records.ScriptArea) EditError!usize {
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.script_area, self.allocator, &keys));
        const index = keys.len;
        self.allocator.free(keys);
        const value: records.Value = .{ .script_area = area };
        try self.addRecord(.script_area, @intCast(index), &value);
        return index;
    }

    /// Replaces area `index` (a move, a resize; D-21) through the generic record
    /// command: within one gesture the edits are one undo step, and one that
    /// returns the area to where the gesture began leaves none.
    pub fn editScriptArea(self: *Editor, index: usize, area: records.ScriptArea, gesture: u32) EditError!void {
        const value: records.Value = .{ .script_area = area };
        try self.editRecord(.script_area, @intCast(index), &value, gesture);
    }

    /// Renames area `index`, one undo step; names are unique and non-empty
    /// (Refused otherwise, nothing changed).
    pub fn renameScriptArea(self: *Editor, index: usize, name: []const u8) EditError!void {
        if (name.len >= records.area_name_capacity) {
            self.setStatus("script area: ", "a name is 63 characters at most");
            return error.Refused;
        }
        var value: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.script_area, @intCast(index), self.allocator, &value));
        var area = value.script_area;
        area.setName(name);
        try self.editScriptArea(index, area, 0);
    }

    /// Deletes area `index`; undo puts it back at its index. One undo step.
    pub fn deleteScriptArea(self: *Editor, index: usize) EditError!void {
        try self.deleteRecord(.script_area, @intCast(index));
    }

    /// The area a drag makes (D-21): world units in, AI units out, the MFC
    /// truncation applied once, nothing changed in the map.
    pub fn scriptAreaFromVis(self: *Editor, shape: records.AreaShape, wx0: f32, wy0: f32, wx1: f32, wy1: f32, name: []const u8) EditError!records.ScriptArea {
        var area: records.ScriptArea = .{};
        try self.noteOutcome(self.bridge.scriptAreaFromVis(shape, wx0, wy0, wx1, wy1, name, &area));
        return area;
    }

    /// The area with its centre at the world point, size kept (D-21).
    pub fn scriptAreaMoved(self: *Editor, area: records.ScriptArea, wx: f32, wy: f32) EditError!records.ScriptArea {
        var out: records.ScriptArea = .{};
        try self.noteOutcome(self.bridge.scriptAreaMoved(area, wx, wy, &out));
        return out;
    }

    /// The area with its corner or edge handle at the world point (D-21).
    pub fn scriptAreaResized(self: *Editor, area: records.ScriptArea, wx: f32, wy: f32) EditError!records.ScriptArea {
        var out: records.ScriptArea = .{};
        try self.noteOutcome(self.bridge.scriptAreaResized(area, wx, wy, &out));
        return out;
    }

    /// A record put in that was not there (D-02, 04-09): the kind from the
    /// value's tag, `key` its identity (a group ID). One undo step: undo
    /// removes the record, redo puts it back. The bridge refuses an insert
    /// where the key is taken, and a refusal changes nothing: not the bridge,
    /// not the history, not the generation.
    pub fn addRecord(self: *Editor, kind: records.Kind, key: i32, value: *const records.Value) EditError!void {
        if (std.meta.activeTag(value.*) != kind) return error.Failed;
        var owned = try value.clone(self.allocator);
        errdefer owned.deinit(self.allocator);
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.insertRecord(key, &owned));
        self.history.recordAssumeCapacity(self.allocator, .{ .record_add = .{ .kind = kind, .key = key, .value = owned } }, 0);
        self.record_generations.set(kind, self.record_generations.get(kind) +% 1);
    }

    /// A record taken out (D-02, 04-09): read first, so undo puts the whole
    /// record back under the same key; redo removes it again. One undo step.
    /// A refusal (no such record) changes nothing.
    pub fn deleteRecord(self: *Editor, kind: records.Kind, key: i32) EditError!void {
        var value: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(kind, key, self.allocator, &value));
        errdefer value.deinit(self.allocator);
        try self.history.reserve(self.allocator);
        try self.noteOutcome(self.bridge.removeRecord(kind, key));
        self.history.recordAssumeCapacity(self.allocator, .{ .record_delete = .{ .kind = kind, .key = key, .value = value } }, 0);
        self.record_generations.set(kind, self.record_generations.get(kind) +% 1);
    }

    /// The reinforcement groups' IDs, ascending, owned by the caller (`free`
    /// with the same allocator).
    pub fn groupIDs(self: *Editor, allocator: std.mem.Allocator) EditError![]i32 {
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.group, allocator, &keys));
        return keys;
    }

    /// The script IDs group `id` holds, owned by the caller.
    pub fn groupScriptIDs(self: *Editor, allocator: std.mem.Allocator, id: i32) EditError![]i32 {
        var value: records.Value = undefined;
        try bridge_mod.check(self.bridge.readRecord(.group, id, allocator, &value));
        defer value.deinit(allocator);
        return try allocator.dupe(i32, value.group.ids);
    }

    /// The Group Manager's New (D-16, C9): a group with no script IDs under
    /// the first unused ID at or above `from_id` (the ID field, default 0).
    /// One undo step. Returns the ID it took.
    pub fn newGroup(self: *Editor, from_id: i32) EditError!i32 {
        var id: i32 = 0;
        try self.noteOutcome(self.bridge.firstFreeGroupID(from_id, &id));
        const value: records.Value = .{ .group = .{ .id = id } };
        try self.addRecord(.group, id, &value);
        return id;
    }

    /// Adds a script ID (0..32000) to a group (D-16, "Add"). One already in
    /// the group is skipped with a status note and records nothing, as the MFC
    /// dialog skips it; -1 and anything out of range is Refused (Pitfall 9).
    /// One undo step.
    pub fn addScriptIDToGroup(self: *Editor, group: i32, script_id: i32) EditError!void {
        if (script_id < records.min_script_id or script_id > records.max_script_id) {
            self.setStatus("group: ", "a script ID in a group is 0..32000");
            return error.Refused;
        }
        var current: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.group, group, self.allocator, &current));
        defer current.deinit(self.allocator);
        if (current.group.has(script_id)) {
            var buffer: [96]u8 = undefined;
            self.note(std.fmt.bufPrint(&buffer, "script ID {d} is already in group {d}", .{ script_id, group }) catch "already in the group");
            return;
        }
        const ids = try self.allocator.alloc(i32, current.group.ids.len + 1);
        defer self.allocator.free(ids);
        @memcpy(ids[0..current.group.ids.len], current.group.ids);
        ids[current.group.ids.len] = script_id;
        const value: records.Value = .{ .group = .{ .id = group, .ids = ids } };
        try self.editRecord(.group, group, &value, 0);
        // The bridge's note when the group now holds back an object a start
        // command or a reserve position names (Pitfall 8).
        self.noteCascade();
    }

    /// Takes a script ID out of a group (D-16, "Remove"). One that is not
    /// there is a status note and records nothing. One undo step.
    pub fn removeScriptIDFromGroup(self: *Editor, group: i32, script_id: i32) EditError!void {
        var current: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.group, group, self.allocator, &current));
        defer current.deinit(self.allocator);
        const at = std.mem.indexOfScalar(i32, current.group.ids, script_id) orelse {
            var buffer: [96]u8 = undefined;
            self.note(std.fmt.bufPrint(&buffer, "script ID {d} is not in group {d}", .{ script_id, group }) catch "not in the group");
            return;
        };
        const ids = try self.allocator.alloc(i32, current.group.ids.len - 1);
        defer self.allocator.free(ids);
        @memcpy(ids[0..at], current.group.ids[0..at]);
        @memcpy(ids[at..], current.group.ids[at + 1 ..]);
        const value: records.Value = .{ .group = .{ .id = group, .ids = ids } };
        try self.editRecord(.group, group, &value, 0);
    }

    /// The Group Manager's Delete (D-16): the group and its script-ID list;
    /// undo puts both back. One undo step.
    pub fn deleteGroup(self: *Editor, group: i32) EditError!void {
        try self.deleteRecord(.group, group);
    }

    /// The action types start commands can carry (04-11, D-17), from
    /// Data/Editor/actions.ini in the file's order, and the entry a new command
    /// starts at (STOP). Owned by the caller: `freeActionList`. Refused, with the
    /// reason in the status, when the file is missing - a start command cannot be
    /// made then.
    pub const ActionList = struct {
        items: []bridge_mod.ActionCommand,
        default_index: usize,

        /// The entry whose id is `id`, or null.
        pub fn find(self: ActionList, id: i32) ?*const bridge_mod.ActionCommand {
            for (self.items) |*item| {
                if (item.id == id) return item;
            }
            return null;
        }

        /// The entry named `name`, or null.
        pub fn byName(self: ActionList, name: []const u8) ?*const bridge_mod.ActionCommand {
            for (self.items) |*item| {
                if (std.mem.eql(u8, item.nameSlice(), name)) return item;
            }
            return null;
        }
    };

    pub fn actionCommands(self: *Editor, allocator: std.mem.Allocator) EditError!ActionList {
        var items: []bridge_mod.ActionCommand = &.{};
        var default_index: usize = 0;
        try self.noteOutcome(self.bridge.actionCommands(allocator, &items, &default_index));
        return .{ .items = items, .default_index = default_index };
    }

    pub fn freeActionList(allocator: std.mem.Allocator, list: ActionList) void {
        allocator.free(list.items);
    }

    // -----------------------------------------------------------------
    // Object filters (M3, D-31). Not map data: nothing here touches the
    // document, the history or an open/close - filters gate the palette,
    // the composer edits them session-wide, and save writes the user file.
    // -----------------------------------------------------------------

    /// Reads the merged filters through the bridge (shipped files + user
    /// file, user wins by name). Replaces whatever this session held.
    pub fn loadFilters(self: *Editor) EditError!void {
        var filters: []bridge_mod.ObjectFilter = &.{};
        try self.noteOutcome(self.bridge.objectFilters(self.allocator, &filters));
        self.filters.deinit(self.allocator);
        self.filters = .fromOwnedSlice(filters);
        self.filters_generation +%= 1;
    }

    pub fn filtersSlice(self: *const Editor) []bridge_mod.ObjectFilter {
        return self.filters.items;
    }

    fn findFilter(self: *Editor, name: []const u8) ?*bridge_mod.ObjectFilter {
        for (self.filters.items) |*one| {
            if (std.mem.eql(u8, one.nameSlice(), name)) return one;
        }
        return null;
    }

    fn filterNameTaken(self: *Editor, name: []const u8) bool {
        return self.findFilter(name) != null;
    }

    /// The composer's New Filter: an empty user filter appended. Refused for
    /// an invalid name (see filters.nameValid) or a taken one; a refusal
    /// changes nothing.
    pub fn filterNew(self: *Editor, name: []const u8) EditError!void {
        if (!core_filters.nameValid(name)) {
            self.setStatus("filters: ", "a filter name is 1..63 characters, no control characters, no |");
            return error.Refused;
        }
        if (self.filterNameTaken(name)) {
            self.setStatus("filters: ", "a filter named that already exists");
            return error.Refused;
        }
        var filter: bridge_mod.ObjectFilter = .{};
        filter.setName(name);
        filter.user = 1;
        try self.filters.append(self.allocator, filter);
        self.filters_generation +%= 1;
    }

    /// The composer's Delete Filter: the named filter leaves the live list
    /// (the next save no longer writes it, so a user-file override of a
    /// shipped name disappears with it - exactly the MFC's erase). Refused
    /// when there is none.
    pub fn filterDelete(self: *Editor, name: []const u8) EditError!void {
        for (self.filters.items, 0..) |*one, index| {
            if (std.mem.eql(u8, one.nameSlice(), name)) {
                _ = self.filters.orderedRemove(index);
                self.filters_generation +%= 1;
                return;
            }
        }
        self.setStatus("filters: ", "no filter is named that");
        return error.Refused;
    }

    /// The composer's Rename Filter. The renamed filter becomes user-owned
    /// (it must be written or the rename is lost). Refused for an invalid or
    /// taken new name or a missing old one.
    pub fn filterRename(self: *Editor, old: []const u8, new: []const u8) EditError!void {
        const filter = self.findFilter(old) orelse {
            self.setStatus("filters: ", "no filter is named that");
            return error.Refused;
        };
        if (!core_filters.nameValid(new)) {
            self.setStatus("filters: ", "a filter name is 1..63 characters, no control characters, no |");
            return error.Refused;
        }
        if (!std.mem.eql(u8, old, new) and self.filterNameTaken(new)) {
            self.setStatus("filters: ", "a filter named that already exists");
            return error.Refused;
        }
        filter.setName(new);
        filter.user = 1;
        self.filters_generation +%= 1;
    }

    /// The composer's word-list edit: the named filter's conditions are
    /// replaced whole (the composer edits one list at a time; it sends the
    /// full list it now wants). The filter becomes user-owned. Refused when
    /// the filter does not exist or the lists do not fit the bridge's caps
    /// (8 lists of 8 words of 31 characters) - the caps are the ABI's, and a
    /// word list the file could not hold must not silently truncate.
    pub fn filterPut(self: *Editor, updated_in: bridge_mod.ObjectFilter) EditError!void {
        var updated = updated_in;
        const name = updated.nameSlice();
        const filter = self.findFilter(name) orelse {
            self.setStatus("filters: ", "no filter is named that");
            return error.Refused;
        };
        if (updated.list_count < 0 or updated.list_count > bridge_mod.filter_max_lists) {
            self.setStatus("filters: ", "a filter carries at most 8 word lists");
            return error.Refused;
        }
        for (updated.lists[0..@intCast(updated.list_count)]) |list| {
            if (list.word_count < 0 or list.word_count > bridge_mod.filter_max_words) {
                self.setStatus("filters: ", "a word list holds at most 8 words");
                return error.Refused;
            }
        }
        const index = (@intFromPtr(filter) - @intFromPtr(self.filters.items.ptr)) / @sizeOf(bridge_mod.ObjectFilter);
        updated.user = 1;
        self.filters.items[index] = updated;
        self.filters_generation +%= 1;
    }

    /// Writes the user-owned filters through the bridge. Refused when the
    /// bridge refuses (an unwritable user root names why); a refusal changes
    /// nothing on disk.
    pub fn saveFilters(self: *Editor) EditError!void {
        var user: std.ArrayListUnmanaged(bridge_mod.ObjectFilter) = .empty;
        defer user.deinit(self.allocator);
        for (self.filters.items) |one| {
            if (one.user == 1) user.append(self.allocator, one) catch return error.OutOfMemory;
        }
        try self.noteOutcome(self.bridge.saveObjectFilters(user.items));
        // Entries written are user-file entries now; their user flag stands.
        self.filters_generation +%= 1;
    }

    /// The map's start commands (D-17) in list order, each one's units owned by
    /// the caller: `freeStartCommands` with the same allocator.
    pub fn startCommands(self: *Editor, allocator: std.mem.Allocator) EditError![]records.StartCommand {
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.start_command, allocator, &keys));
        defer allocator.free(keys);
        const out = try allocator.alloc(records.StartCommand, keys.len);
        var filled: usize = 0;
        errdefer {
            for (out[0..filled]) |command| allocator.free(command.units);
            allocator.free(out);
        }
        for (keys) |key| {
            var value: records.Value = undefined;
            try bridge_mod.check(self.bridge.readRecord(.start_command, key, allocator, &value));
            out[filled] = value.start_command;
            filled += 1;
        }
        return out;
    }

    pub fn freeStartCommands(allocator: std.mem.Allocator, commands: []records.StartCommand) void {
        for (commands) |command| allocator.free(command.units);
        allocator.free(commands);
    }

    /// Unit -> Add start command (D-17): a command for the unit `unit_link_id`
    /// (a soldier's click already answers his squad's link ID), the type the
    /// action list starts at (STOP), no target, appended; one undo step. Returns
    /// the index it took. The bridge refuses a link ID that is not a unit or a
    /// squad of the map, and says when the unit is one a group holds back.
    pub fn addStartCommand(self: *Editor, unit_link_id: i32) EditError!usize {
        const list = try self.actionCommands(self.allocator);
        defer freeActionList(self.allocator, list);
        if (list.items.len == 0) return error.Failed;
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.start_command, self.allocator, &keys));
        const index = keys.len;
        self.allocator.free(keys);
        const units = [_]i32{unit_link_id};
        const value: records.Value = .{ .start_command = .{ .cmd_type = list.items[list.default_index].id, .units = &units } };
        try self.addRecord(.start_command, @intCast(index), &value);
        self.noteCascade();
        return index;
    }

    /// Replaces start command `index` through the generic record command: a
    /// type, a number, a target, its units. Within one gesture the edits are one
    /// undo step. The record's `from_explosion` is ignored: the file's stays.
    pub fn editStartCommand(self: *Editor, index: usize, command: records.StartCommand, gesture: u32) EditError!void {
        // The flag is the file's (D-17): take it from the command as it is, so a
        // value that changes nothing else is recognised as no edit, and what the
        // history keeps is what the bridge holds.
        var current: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.start_command, @intCast(index), self.allocator, &current));
        defer current.deinit(self.allocator);
        var wanted = command;
        wanted.from_explosion = current.start_command.from_explosion;
        const value: records.Value = .{ .start_command = wanted };
        try self.editRecord(.start_command, @intCast(index), &value, gesture);
        self.noteCascade();
    }

    /// Deletes start command `index`; undo puts it back at its index, every
    /// field as it was. One undo step.
    pub fn deleteStartCommand(self: *Editor, index: usize) EditError!void {
        try self.deleteRecord(.start_command, @intCast(index));
    }

    /// "Add selected unit": `link_id` joins command `index`'s units. One already
    /// there is a status note and records nothing. One undo step.
    pub fn addUnitToStartCommand(self: *Editor, index: usize, link_id: i32) EditError!void {
        var current: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.start_command, @intCast(index), self.allocator, &current));
        defer current.deinit(self.allocator);
        if (current.start_command.has(link_id)) {
            var buffer: [96]u8 = undefined;
            self.note(std.fmt.bufPrint(&buffer, "unit {d} is already in start command {d}", .{ link_id, index }) catch "already in the command");
            return;
        }
        const units = try self.allocator.alloc(i32, current.start_command.units.len + 1);
        defer self.allocator.free(units);
        @memcpy(units[0..current.start_command.units.len], current.start_command.units);
        units[current.start_command.units.len] = link_id;
        var wanted = current.start_command;
        wanted.units = units;
        try self.editStartCommand(index, wanted, 0);
    }

    /// "Remove" on a unit of command `index`. Taking out the last unit deletes
    /// the command, as one step, as the delete cascade would. A unit that is not
    /// there is a status note and records nothing.
    pub fn removeUnitFromStartCommand(self: *Editor, index: usize, link_id: i32) EditError!void {
        var current: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.start_command, @intCast(index), self.allocator, &current));
        defer current.deinit(self.allocator);
        const at = std.mem.indexOfScalar(i32, current.start_command.units, link_id) orelse {
            var buffer: [96]u8 = undefined;
            self.note(std.fmt.bufPrint(&buffer, "unit {d} is not in start command {d}", .{ link_id, index }) catch "not in the command");
            return;
        };
        if (current.start_command.units.len == 1) return self.deleteStartCommand(index);
        const units = try self.allocator.alloc(i32, current.start_command.units.len - 1);
        defer self.allocator.free(units);
        @memcpy(units[0..at], current.start_command.units[0..at]);
        @memcpy(units[at..], current.start_command.units[at + 1 ..]);
        var wanted = current.start_command;
        wanted.units = units;
        try self.editStartCommand(index, wanted, 0);
    }

    /// What an object type can be in a reserve position (D-18): none, a
    /// self-propelled gun, a towed gun or a truck able to tow, from the bridge's
    /// object database. A name it does not know is none.
    pub fn reserveRole(self: *Editor, name: []const u8) EditError!bridge_mod.ReserveRole {
        var role: i32 = 0;
        try self.noteOutcome(self.bridge.reserveRole(name, &role));
        return std.enums.fromInt(bridge_mod.ReserveRole, role) orelse .none;
    }

    /// The map's reserve positions (D-18) in list order, owned by the caller (free
    /// with the same allocator).
    pub fn reservePositions(self: *Editor, allocator: std.mem.Allocator) EditError![]records.ReservePosition {
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.reserve_position, allocator, &keys));
        defer allocator.free(keys);
        const out = try allocator.alloc(records.ReservePosition, keys.len);
        errdefer allocator.free(out);
        for (keys, out) |key, *position| {
            var value: records.Value = undefined;
            try bridge_mod.check(self.bridge.readRecord(.reserve_position, key, allocator, &value));
            position.* = value.reserve_position;
        }
        return out;
    }

    /// A new reserve position appended to the list (D-18), one undo step (undo
    /// removes it, redo puts it back at its index). The bridge refuses - naming why,
    /// changing nothing - a squad or a non-unit in either role, a gun that is not
    /// artillery, a towed gun with no truck, a truck that cannot tow the gun and a
    /// place off the map. Returns the index it took.
    pub fn addReservePosition(self: *Editor, position: records.ReservePosition) EditError!usize {
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.reserve_position, self.allocator, &keys));
        const index = keys.len;
        self.allocator.free(keys);
        const value: records.Value = .{ .reserve_position = position };
        try self.addRecord(.reserve_position, @intCast(index), &value);
        return index;
    }

    /// Replaces reserve position `index` through the generic record command; within
    /// one gesture the edits are one undo step.
    pub fn editReservePosition(self: *Editor, index: usize, position: records.ReservePosition, gesture: u32) EditError!void {
        const value: records.Value = .{ .reserve_position = position };
        try self.editRecord(.reserve_position, @intCast(index), &value, gesture);
    }

    /// Deletes reserve position `index`; undo puts it back at its index. One undo step.
    pub fn deleteReservePosition(self: *Editor, index: usize) EditError!void {
        try self.deleteRecord(.reserve_position, @intCast(index));
    }

    /// How many sides the map's AI general has (D-19): the size of its side list. A
    /// side at or above it reads empty, and an edit of one creates it and every side
    /// below it, empty.
    pub fn aiSideCount(self: *Editor) EditError!usize {
        var keys: []i32 = &.{};
        try bridge_mod.check(self.bridge.recordKeys(.ai_side, self.allocator, &keys));
        defer self.allocator.free(keys);
        return keys.len;
    }

    /// The AI general's side `side` (D-19) with the map's side count, owned by the
    /// caller (`deinit` with the same allocator). A side the map does not have is empty.
    pub fn aiSide(self: *Editor, allocator: std.mem.Allocator, side: usize) EditError!records.AiSide {
        if (side >= records.max_ai_sides) return error.Failed;
        var value: records.Value = undefined;
        try self.noteOutcome(self.bridge.readRecord(.ai_side, @intCast(side), allocator, &value));
        return value.ai_side;
    }

    /// Replaces side `side` (and the side count) through the generic record command
    /// with `value`, a whole side: within one gesture the edits are one undo step, and
    /// undo puts the old side and the old side count back, so a side this edit created
    /// is taken away again. `value.side` is set to `side`; its count must cover it.
    pub fn editAiSide(self: *Editor, side: usize, value: records.AiSide, gesture: u32) EditError!void {
        if (side >= records.max_ai_sides) return error.Failed;
        var wanted = value;
        wanted.side = @intCast(side);
        wanted.ensureSideExists();
        const record: records.Value = .{ .ai_side = wanted };
        try self.editRecord(.ai_side, @intCast(side), &record, gesture);
    }

    /// The AI General tool's click on open ground (D-19, C2): a defence parcel of the
    /// default radius (256 AI units, four map tiles) and direction 0 at the map point,
    /// cut as Vis2AI cuts it, appended to side `side`. The side is created, with every
    /// side below it empty, when the map lacks it. `gesture` joins the edit to a drag
    /// (the tool keeps dragging the new parcel's centre), 0 for a step of its own.
    /// Returns the new parcel's index.
    pub fn addDefenceParcelIn(self: *Editor, side: usize, map_x: f32, map_y: f32, gesture: u32) EditError!usize {
        var current = try self.aiSide(self.allocator, side);
        defer current.deinit(self.allocator);
        const index = current.parcels.len;
        try current.appendParcel(self.allocator, .{
            .kind = .defence,
            .cx = records.truncateToAi(map_x),
            .cy = records.truncateToAi(map_y),
            .radius = records.parcel_min_radius,
            .defence_dir = 0,
        });
        try self.editAiSide(side, current, gesture);
        return index;
    }

    pub fn addDefenceParcel(self: *Editor, side: usize, map_x: f32, map_y: f32) EditError!usize {
        return self.addDefenceParcelIn(side, map_x, map_y, 0);
    }

    /// The panel's Add: `id` joins side `side`'s mobile script IDs (0..32000). One
    /// already there is a status note and records nothing; one outside the range is
    /// Refused with a note. One undo step.
    pub fn addMobileScriptID(self: *Editor, side: usize, id: i32) EditError!void {
        if (id < records.min_script_id or id > records.max_script_id) {
            self.note("a mobile script ID is 0..32000");
            return error.Refused;
        }
        var current = try self.aiSide(self.allocator, side);
        defer current.deinit(self.allocator);
        if (current.hasMobile(id)) {
            var buffer: [96]u8 = undefined;
            self.note(std.fmt.bufPrint(&buffer, "script ID {d} is already a mobile ID of side {d}", .{ id, side }) catch "already a mobile script ID");
            return;
        }
        try current.appendMobile(self.allocator, id);
        try self.editAiSide(side, current, 0);
    }

    /// The panel's Remove: `id` leaves side `side`'s mobile script IDs. One that is not
    /// there is a status note and records nothing. One undo step.
    pub fn removeMobileScriptID(self: *Editor, side: usize, id: i32) EditError!void {
        var current = try self.aiSide(self.allocator, side);
        defer current.deinit(self.allocator);
        if (!try current.removeMobile(self.allocator, id)) {
            var buffer: [96]u8 = undefined;
            self.note(std.fmt.bufPrint(&buffer, "script ID {d} is not a mobile ID of side {d}", .{ id, side }) catch "not a mobile script ID");
            return;
        }
        try self.editAiSide(side, current, 0);
    }

    /// An object's delete, restore or either's replay changes the start
    /// commands and the reserve positions that name it - the cascade - so the
    /// panels that list them read again.
    fn bumpCascadeGenerations(self: *Editor) void {
        self.record_generations.set(.start_command, self.record_generations.get(.start_command) +% 1);
        self.record_generations.set(.reserve_position, self.record_generations.get(.reserve_position) +% 1);
    }

    /// What a bridge-logged edit needs before its bridge call: room for its
    /// token in the entry it merges into, or history room and a one-token list
    /// for a new entry. Everything that can fail happens here, so once the
    /// bridge has committed, `commitEdit` cannot fail.
    const PreparedEdit = struct { entry: ?*history_mod.Entry = null, tokens: std.ArrayListUnmanaged(i32) = .empty };

    fn prepareEdit(self: *Editor, gesture: u32, scope: history_mod.EditScope) EditError!PreparedEdit {
        if (self.mergeable(gesture, .edit)) |entry| {
            if (entry.command.edit.scope == scope) {
                try entry.command.edit.tokens.ensureUnusedCapacity(self.allocator, 1);
                return .{ .entry = entry };
            }
        }
        try self.history.reserve(self.allocator);
        var prepared: PreparedEdit = .{};
        try prepared.tokens.ensureUnusedCapacity(self.allocator, 1);
        return prepared;
    }

    /// Records the bridge's token: appended to the gesture's entry (one undo
    /// step per drag), or a new entry that takes `prepared.tokens` over.
    fn commitEdit(self: *Editor, prepared: *PreparedEdit, token: i32, gesture: u32, scope: history_mod.EditScope) void {
        if (prepared.entry) |entry| {
            entry.command.edit.tokens.appendAssumeCapacity(token);
            self.history.touchTop(self.allocator);
        } else {
            prepared.tokens.appendAssumeCapacity(token);
            self.history.recordAssumeCapacity(self.allocator, .{ .edit = .{ .tokens = prepared.tokens, .scope = scope } }, gesture);
            prepared.tokens = .empty;
        }
        self.bumpScope(scope);
    }

    fn bumpScope(self: *Editor, scope: history_mod.EditScope) void {
        switch (scope) {
            .vso => self.vso_generation +%= 1,
            .objects => {
                self.bridges_generation +%= 1;
                self.entrenchments_generation +%= 1;
            },
            .altitudes => self.altitudes_generation +%= 1,
            .players => {
                self.players_generation +%= 1;
                self.record_generations.set(.camera_anchors, self.record_generations.get(.camera_anchors) +% 1);
                self.record_generations.set(.unit_creation, self.record_generations.get(.unit_creation) +% 1);
            },
        }
    }

    /// Re-reads the document's objects from the bridge after a compound edit
    /// that added or removed map objects (the `objects` scope). The path,
    /// fields and diplomacy stay.
    fn reloadObjects(self: *Editor) EditError!void {
        var total: usize = 0;
        var none: [0]ObjectRecord = .{};
        const sizing = self.bridge.objects(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        var objects: std.ArrayListUnmanaged(ObjectRecord) = .empty;
        errdefer objects.deinit(self.allocator);
        try objects.resize(self.allocator, total);
        try bridge_mod.check(self.bridge.objects(objects.items, &total));
        // IN-B03: a second pass that answered fewer leaves no unread tail.
        if (total < objects.items.len) objects.shrinkRetainingCapacity(total);
        self.document.objects.deinit(self.allocator);
        self.document.objects = objects;
        // A compound edit that adds and removes map objects may have taken a
        // selected one with it: the set is rebuilt from the fresh list, so
        // it keeps exactly the members the document holds.
        var kept: std.AutoHashMapUnmanaged(i32, void) = .empty;
        kept.ensureTotalCapacity(self.allocator, @intCast(@min(objects.items.len, std.math.maxInt(i32)))) catch {};
        for (objects.items) |object| {
            if (self.selection_set.contains(object.link_id)) kept.putAssumeCapacity(object.link_id, {});
        }
        self.selection_set.deinit(self.allocator);
        self.selection_set = kept;
        if (self.selection) |link_id| {
            if (self.document.find(link_id) == null) self.selection = null;
        }
    }

    /// Draws a road or river (D-08) through `points` (world units; the
    /// bridge fits them to the ground), of type `desc` (a bare name from
    /// `vsoDescriptors`), `width_tiles` 1..16 and `opacity` 0..1 at every
    /// point. One undo step. Returns where it landed in the bridge's list. A
    /// refusal (too short, an unknown type, a point off the map) changes
    /// nothing: not the bridge, not the history.
    pub fn addVso(self: *Editor, kind: VsoKind, desc: []const u8, points: []const records.Vec3, width_tiles: f32, opacity: f32) EditError!usize {
        var prepared = try self.prepareEdit(0, .vso);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        var index: i32 = -1;
        try self.noteOutcome(self.bridge.addVso(kind, desc, points, width_tiles, opacity, &token, &index));
        self.commitEdit(&prepared, token, 0, .vso);
        // IN-B03: an OK with no index is a bridge fault, never entry 0.
        return if (index >= 0) @intCast(index) else error.Failed;
    }

    /// Deletes the whole road or river at `index` (D-08's Delete with no point
    /// grabbed); one undo step, which puts it back where it was.
    pub fn deleteVso(self: *Editor, kind: VsoKind, index: usize) EditError!void {
        var prepared = try self.prepareEdit(0, .vso);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.deleteVso(kind, @intCast(index), &token));
        self.commitEdit(&prepared, token, 0, .vso);
    }

    /// Moves the control points of the road or river at `index` to `points`
    /// (every one of them, world units); the bridge resamples keeping the key
    /// points. The calls of one drag (`gesture`) are one undo step.
    pub fn moveVsoPoints(self: *Editor, kind: VsoKind, index: usize, points: []const records.Vec3, gesture: u32) EditError!void {
        var prepared = try self.prepareEdit(gesture, .vso);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.moveVsoPoints(kind, @intCast(index), points, &token));
        self.commitEdit(&prepared, token, gesture, .vso);
    }

    /// The width (world units, centre line to edge) at key point `key` in
    /// `mode`; the calls of one drag are one undo step.
    pub fn setVsoWidth(self: *Editor, kind: VsoKind, index: usize, key: usize, width: f32, mode: bridge_mod.VsoWidthMode, gesture: u32) EditError!void {
        var prepared = try self.prepareEdit(gesture, .vso);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.setVsoWidth(kind, @intCast(index), @intCast(key), width, mode, &token));
        self.commitEdit(&prepared, token, gesture, .vso);
    }

    /// The opacity (0..1) at key point `key` in `mode`; the calls of one drag
    /// are one undo step.
    pub fn setVsoOpacity(self: *Editor, kind: VsoKind, index: usize, key: usize, opacity: f32, mode: bridge_mod.VsoWidthMode, gesture: u32) EditError!void {
        var prepared = try self.prepareEdit(gesture, .vso);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.setVsoOpacity(kind, @intCast(index), @intCast(key), opacity, mode, &token));
        self.commitEdit(&prepared, token, gesture, .vso);
    }

    /// Insert (D-08): the midpoint after control point `control`, or before
    /// it when it is the last. One undo step.
    pub fn insertVsoPoint(self: *Editor, kind: VsoKind, index: usize, control: usize) EditError!void {
        var prepared = try self.prepareEdit(0, .vso);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.insertVsoPoint(kind, @intCast(index), @intCast(control), &token));
        self.commitEdit(&prepared, token, 0, .vso);
    }

    /// Delete of one control point; refused while only 2 remain. One undo
    /// step.
    pub fn deleteVsoPoint(self: *Editor, kind: VsoKind, index: usize, control: usize) EditError!void {
        var prepared = try self.prepareEdit(0, .vso);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.deleteVsoPoint(kind, @intCast(index), @intCast(control), &token));
        self.commitEdit(&prepared, token, 0, .vso);
    }

    /// Sets the terrain vertex heights (WORLD z units) over `region`
    /// (terrain-vertex indices, half-open, row-major like `heights`), one
    /// bridge edit per call (M3, D-19): the bridge sets the heights,
    /// recomputes the shades over the region grown by the shade kernel and
    /// pushes the covering patches into the engine; undo restores the
    /// recorded region raw, so nothing outside it moves. The calls of one
    /// drag (`gesture`) merge into one undo step, exactly like a paint. A
    /// refusal (a region off the map, a count that does not match the
    /// region, a non-finite height) changes nothing: not the bridge, not
    /// the history, not the generation.
    pub fn setAltitudes(self: *Editor, region: bridge_mod.AltitudeRegion, heights: []const f32, gesture: u32) EditError!void {
        var prepared = try self.prepareEdit(gesture, .altitudes);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.setAltitudes(region, heights, &token));
        self.commitEdit(&prepared, token, gesture, .altitudes);
    }

    /// One step of one Heights-tool stroke (M3, D-18): the bridge derives
    /// the pattern and the level target, applies the D-19 function and logs
    /// one edit; the steps of one drag (`gesture`) merge into one undo
    /// step, exactly like a paint. A refused step - the cursor off the map,
    /// the invalid-height rollback - changes nothing: not the bridge, not
    /// the history, not the generation, and a tool may send the next step
    /// of the same stroke anyway (the refused-stamp rule).
    pub fn heightsStroke(self: *Editor, params: bridge_mod.HeightsStrokeParams, gesture: u32) EditError!void {
        var prepared = try self.prepareEdit(gesture, .altitudes);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.heightsStroke(params, &token));
        self.commitEdit(&prepared, token, gesture, .altitudes);
    }

    /// Generate heights (M3, D-18): the engine's own noise over the whole
    /// sheet, one undo step. The confirmation is the caller's.
    pub fn generateHeights(self: *Editor, gen_type: bridge_mod.HeightsGenerateType, granularity: f32, min_z: f32, max_z: f32) EditError!void {
        var prepared = try self.prepareEdit(0, .altitudes);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.generateHeights(gen_type, granularity, min_z, max_z, &token));
        self.commitEdit(&prepared, token, 0, .altitudes);
    }

    /// Set Zero (M3, D-18): every height to 0, one undo step. The
    /// confirmation is the caller's.
    pub fn setZeroHeights(self: *Editor) EditError!void {
        var prepared = try self.prepareEdit(0, .altitudes);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.setZeroHeights(&token));
        self.commitEdit(&prepared, token, 0, .altitudes);
    }

    /// Update Map (M3, D-20): the whole composite as ONE undo step - the
    /// engine's height and terrain updates, the full crosses and shades, the
    /// VSO z refresh, the fit pass. `progress` (nullable, never called back
    /// into the bridge) hears each of the MFC's own steps. The record moves
    /// objects, so the objects scope reloads the document after a replay and
    /// the altitudes generation moves too (the minimap's future key).
    pub fn updateMap(self: *Editor, progress: ?bridge_mod.ProgressFn, user: ?*anyopaque) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.updateMap(progress, user, &token));
        self.commitEdit(&prepared, token, 0, .objects);
        self.altitudes_generation +%= 1;
        // The composite moves objects (the fit pass) - the document reads
        // the map's objects again, exactly a trench edit's own reload.
        try self.reloadObjectsAfterEdit();
    }

    /// Fill Entire Map (M3, D-22): every tile the type's own, one undo step -
    /// one paint of the bridge's log, so its token is a PAINT token
    /// (undoPaint/redoPaint), recorded exactly like a brush paint's.
    pub fn fillEntireMap(self: *Editor, tile: u8) EditError!void {
        try self.history.reserve(self.allocator);
        var tokens: std.ArrayListUnmanaged(i32) = .empty;
        try tokens.ensureUnusedCapacity(self.allocator, 1);
        var token: i32 = -1;
        self.noteOutcome(self.bridge.fillEntireMap(tile, &token)) catch |err| {
            tokens.deinit(self.allocator);
            return err;
        };
        tokens.appendAssumeCapacity(token);
        self.history.recordAssumeCapacity(self.allocator, .{ .paint = .{ .tokens = tokens } }, 0);
    }

    // ------------------------------------------------------------------
    // The Layers menu (M3, D-32). Renderer state: none of it is map data,
    // none of it dirties the document or enters the history.
    // ------------------------------------------------------------------

    /// Whether the renderer can draw `layer` at all (the bridge's mask).
    pub fn layerAvailable(self: *const Editor, layer: layers_mod.Layer) bool {
        return self.layers_mask & layers_mod.bit(layer) != 0;
    }

    /// One toggle layer to a state, in the renderer and in what is remembered.
    /// Refused (nothing changes) for the fire ranges - a mode, see
    /// `setFireRange` - for a layer the renderer cannot draw, and with no map
    /// open (the bridge's own refusal, message carried).
    pub fn setLayer(self: *Editor, layer: layers_mod.Layer, shown: bool) EditError!void {
        if (!layers_mod.isToggle(layer)) {
            self.setStatus("layers: ", "the fire ranges are a mode, not a toggle");
            return error.Refused;
        }
        if (!self.layerAvailable(layer)) {
            self.setStatus("layers: ", "this renderer cannot draw that layer");
            return error.Refused;
        }
        try self.noteOutcome(self.bridge.setLayerShow(@intFromEnum(layer), shown));
        self.layers.set(layer, shown);
        self.layers_generation +%= 1;
    }

    pub fn toggleLayer(self: *Editor, layer: layers_mod.Layer) EditError!void {
        try self.setLayer(layer, !self.layers.shown(layer));
    }

    /// Unit Fire Ranges (TemplateEditorFrame1::ShowFireRange): off, the
    /// selected units' ranges, or the ranges of every unit a named filter
    /// passes. Nothing changes when the bridge refuses (an unknown filter name,
    /// no map open).
    pub fn setFireRange(self: *Editor, mode: layers_mod.FireMode, filter: []const u8) EditError!void {
        var next = self.layers;
        next.setFireRange(mode, filter);
        try self.sendFireRange(&next, false);
        self.layers = next;
        self.layers_generation +%= 1;
        self.fire_sent_key = self.fireKey();
    }

    /// Tells the bridge `state`'s fire-range mode with the selection it needs.
    /// `quiet` leaves the status line alone on success (a per-frame resend must
    /// not wipe the message of the edit before it).
    fn sendFireRange(self: *Editor, state: *const layers_mod.State, quiet: bool) EditError!void {
        var members: []i32 = &.{};
        defer if (members.len != 0) self.allocator.free(members);
        if (state.fire_mode == .selected) members = try self.selectionMembers(self.allocator);
        const answer = self.bridge.setFireRangeMode(@intFromEnum(state.fire_mode), state.fireFilter(), members);
        if (quiet and answer == .ok) return;
        // A filter name the bridge does not know (one made in the composer and
        // not saved yet, or deleted since) is a refusal like any other: the
        // mode that showed stays, the status line says which name.
        if (answer == .bad_argument and state.fire_mode == .filter) {
            self.setStatus("fire range: ", self.bridge.lastMessage());
            return error.Refused;
        }
        try self.noteOutcome(answer);
    }

    /// What decides which units' ranges show: the mode, the filter's name, and
    /// - for the selected mode - the selection, for the filter mode the set of
    /// objects (the history's revision moves with every change of the document).
    /// Order-independent over the selection.
    fn fireKey(self: *const Editor) u64 {
        const mix = struct {
            fn word(h: u64, w: u64) u64 {
                return (h ^ w) *% 0x100000001b3;
            }
            fn scramble(w: u64) u64 {
                var x = w +% 0x9e3779b97f4a7c15;
                x = (x ^ (x >> 30)) *% 0xbf58476d1ce4e5b9;
                x = (x ^ (x >> 27)) *% 0x94d049bb133111eb;
                return x ^ (x >> 31);
            }
        };
        var h = self.layers.hash();
        h = mix.word(h, self.history.revision);
        h = mix.word(h, self.document.objects.items.len);
        if (self.layers.fire_mode == .selected) {
            var acc: u64 = 0;
            var it = self.selection_set.keyIterator();
            while (it.next()) |key| acc +%= mix.scramble(@as(u32, @bitCast(key.*)));
            if (self.selection) |anchor| {
                if (!self.selection_set.contains(anchor)) acc +%= mix.scramble(@as(u32, @bitCast(anchor)));
            }
            h = mix.word(h, acc);
        }
        return h;
    }

    /// The per-frame call: while a fire-range mode is on, the bridge is told
    /// again whenever what decides the ranges moved (the selection, an edit).
    /// Cheap when nothing did.
    pub fn syncFireRange(self: *Editor) void {
        if (self.layers.fire_mode == .off or self.document.info.width_tiles == 0) return;
        const key = self.fireKey();
        if (self.fire_sent_key != null and self.fire_sent_key.? == key) return;
        self.fire_sent_key = key;
        self.sendFireRange(&self.layers, true) catch {};
    }

    /// After every open and new map: the renderer's state is put to what is
    /// remembered - the MFC editor's desync fix. The bridge reads its own state
    /// back (a map just built into the engine brings the renderer up as the
    /// engine's own memory has it), only the layers that differ are sent, and
    /// only those the renderer can draw. Best effort and quiet: a layer the
    /// renderer would not take must not turn an open into an error or wipe the
    /// open's own status.
    pub fn applyLayers(self: *Editor) void {
        var bits: u32 = 0;
        var mask: u32 = 0;
        if (self.bridge.layers(&bits, &mask) != .ok) return;
        self.layers_mask = mask & layers_mod.all_bits;
        const wanted = self.layers.bitsFor(mask);
        for (std.enums.values(layers_mod.Layer)) |layer| {
            if (!layers_mod.isToggle(layer) or mask & layers_mod.bit(layer) == 0) continue;
            const want = wanted & layers_mod.bit(layer) != 0;
            const have = bits & layers_mod.bit(layer) != 0;
            if (want != have) _ = self.bridge.setLayerShow(@intFromEnum(layer), want);
        }
        self.layers_generation +%= 1;
        // The AI forgot its groups with the map: the mode is asked again (the
        // selection is empty after an open, so a selected mode shows nothing
        // until something is selected).
        self.fire_sent_key = null;
        if (self.layers.fire_mode != .off) {
            self.sendFireRange(&self.layers, true) catch {};
            self.fire_sent_key = self.fireKey();
        }
    }

    /// The terrain-mode toggles (M3, D-20): a view setting on the bridge
    /// session, never map data and never in the history.
    pub fn setTerrainModes(self: *Editor, instant_update: bool, fit_to_grid: bool) EditError!void {
        try self.noteOutcome(self.bridge.setTerrainModes(instant_update, fit_to_grid));
    }

    /// The terrain vertex heights (WORLD z units) over `region`, row-major
    /// into `out` (sized by a first sizing call, `altitudes`'s two-pass
    /// rule). A read: the status line is left alone.
    pub fn altitudes(self: *Editor, region: bridge_mod.AltitudeRegion, out: []f32, total: *usize) bridge_mod.Status {
        return self.bridge.altitudes(region, out, total);
    }

    /// The road or river under a world point, `cycle` skipping that many
    /// earlier hits; null when nothing is there. A read: the status line is
    /// left alone.
    pub fn pickVso(self: *Editor, wx: f32, wy: f32, cycle: u32) EditError!?bridge_mod.VsoRef {
        var kind: VsoKind = .road;
        var index: i32 = -1;
        const result = self.bridge.pickVso(wx, wy, @intCast(@min(cycle, std.math.maxInt(i32))), &kind, &index);
        if (result == .refused) return null;
        try bridge_mod.check(result);
        if (index < 0) return null;
        return .{ .kind = kind, .index = @intCast(index) };
    }

    /// How many roads or rivers the map holds. A read: the status line is
    /// left alone.
    pub fn vsoCount(self: *Editor, kind: VsoKind) EditError!usize {
        var count: usize = 0;
        try bridge_mod.check(self.bridge.vsoCount(kind, &count));
        return count;
    }

    /// The road or river at `index`, its point arrays owned by the caller
    /// (`deinit(editor.allocator)`). A read: the status line is left alone.
    pub fn readVso(self: *Editor, kind: VsoKind, index: usize) EditError!VsoView {
        var view: VsoView = .{};
        try bridge_mod.check(self.bridge.readVso(kind, @intCast(index), self.allocator, &view));
        return view;
    }

    /// The season's road or river types, sorted; the caller frees the slice
    /// with `allocator`.
    pub fn vsoDescriptors(self: *Editor, kind: VsoKind, allocator: std.mem.Allocator) EditError![]VsoDescriptor {
        var total: usize = 0;
        var none: [0]VsoDescriptor = .{};
        const sizing = self.bridge.vsoDescriptors(kind, &none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        const out = try allocator.alloc(VsoDescriptor, total);
        errdefer allocator.free(out);
        try bridge_mod.check(self.bridge.vsoDescriptors(kind, out, &total));
        return out;
    }

    /// Draws a bridge of type `desc` (a name from `bridgeDescriptors`) along
    /// the drag from (wx0, wy0) to (wx1, wy1), WORLD units (D-10): the bridge
    /// plans the spans, adds them and a new bridges entry as one undo step,
    /// and the document's objects are read again (scope `objects`). Returns
    /// the entry's index. A refusal (a drag along the other axis, a span off
    /// the map, a bad type) changes nothing: not the bridge, not the history.
    pub fn drawBridge(self: *Editor, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32) EditError!usize {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        var index: i32 = -1;
        try self.noteOutcome(self.bridge.drawBridge(desc, wx0, wy0, wx1, wy1, &token, &index));
        self.commitEdit(&prepared, token, 0, .objects);
        // The bridge has committed and the history holds the step: a failed
        // re-read leaves the document short of the new spans, which the
        // status line reports; the next open or undo reads it again.
        try self.reloadObjectsAfterEdit();
        // IN-B03: an OK with no index is a bridge fault, never entry 0.
        return if (index >= 0) @intCast(index) else error.Failed;
    }

    /// Deletes the whole bridge at `index` (D-11): its entry and every span,
    /// one undo step that puts them back at the same index.
    pub fn deleteBridge(self: *Editor, index: usize) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.deleteBridge(@intCast(index), &token));
        self.commitEdit(&prepared, token, 0, .objects);
        try self.reloadObjectsAfterEdit();
    }

    /// Rotates the bridge at `index` (D-11): its `_01`/`_02` partner about
    /// the same centre with the same span count, at the same index; one undo
    /// step. Refused (no partner, a span off the map) with nothing changed.
    pub fn rotateBridge(self: *Editor, index: usize) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.rotateBridge(@intCast(index), &token));
        self.commitEdit(&prepared, token, 0, .objects);
        try self.reloadObjectsAfterEdit();
    }

    /// D-12: toggles the bridge at `index` between intact and built during
    /// play; one undo step. Refused unless a WoodenBig_Heavy_ bridge. The
    /// objects keep their link IDs, so the document needs no re-read.
    pub fn toggleBridgeBuild(self: *Editor, index: usize) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.toggleBridgeBuild(@intCast(index), &token));
        self.commitEdit(&prepared, token, 0, .objects);
    }

    /// Places a fence run of type `desc` (a name from `fenceDescriptors`)
    /// along the drag from (wx0, wy0) to (wx1, wy1), WORLD units (D-14): the
    /// bridge plans the fences (one every second AI tile, the direction in
    /// the frame index; a drag that stays on its tile is one fence, flipped
    /// with `ctrl`) and adds them as one undo step; the document's objects
    /// are read again (scope `objects`). A refusal (a run off the map, a bad
    /// type, a fence the engine will not place) changes nothing: not the
    /// bridge, not the history.
    pub fn drawFences(self: *Editor, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.drawFences(desc, wx0, wy0, wx1, wy1, ctrl, &token));
        self.commitEdit(&prepared, token, 0, .objects);
        try self.reloadObjectsAfterEdit();
    }

    /// The object database's fence types, sorted; the caller frees the slice
    /// with `allocator`. A read: the status line is left alone.
    pub fn fenceDescriptors(self: *Editor, allocator: std.mem.Allocator) EditError![]bridge_mod.FenceDescriptor {
        var total: usize = 0;
        var none: [0]bridge_mod.FenceDescriptor = .{};
        const sizing = self.bridge.fenceDescriptors(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        const out = try allocator.alloc(bridge_mod.FenceDescriptor, total);
        errdefer allocator.free(out);
        try bridge_mod.check(self.bridge.fenceDescriptors(out, &total));
        return out;
    }

    /// The fences a drag would place (MAP units), changing nothing - the
    /// ghost. Writes at most `out.len` and returns how many the plan has;
    /// null when the run is refused, the reason in `bridge.lastMessage()`.
    /// A read: the status line is left alone (the ghost asks every frame).
    pub fn planFences(self: *Editor, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, out: []bridge_mod.PlannedPiece) EditError!?usize {
        var total: usize = 0;
        const result = self.bridge.planFences(desc, wx0, wy0, wx1, wy1, ctrl, out, &total);
        if (result == .refused and total > out.len) return total;
        if (result == .refused) return null;
        try bridge_mod.check(result);
        return total;
    }

    /// Draws the entrenchment the clicks `points` commit (WORLD units, in
    /// order; z ignored) for `player` (D-13): the bridge runs the MFC builder
    /// (straight runs of fireplaces and lines, arcs at turns, a terminator at
    /// each end), adds the pieces and a new entrenchments entry as one undo
    /// step, and the document's objects are read again (scope `objects`).
    /// Returns the entry's index. A refusal (fewer than two points a piece
    /// apart, a piece off the map) changes nothing: not the bridge, not the
    /// history.
    pub fn drawEntrenchment(self: *Editor, points: []const records.Vec3, player: i32) EditError!usize {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        var index: i32 = -1;
        try self.noteOutcome(self.bridge.drawEntrenchment(points, player, &token, &index));
        self.commitEdit(&prepared, token, 0, .objects);
        try self.reloadObjectsAfterEdit();
        // IN-B03: an OK with no index is a bridge fault, never entry 0.
        return if (index >= 0) @intCast(index) else error.Failed;
    }

    /// The pieces the clicks would commit (MAP units), changing nothing - the
    /// preview. Writes at most `out.len` and returns how many the plan has;
    /// null when the trench is refused, the reason in `bridge.lastMessage()`.
    /// A read: the status line is left alone (the preview asks as the
    /// pointer moves).
    pub fn planEntrenchment(self: *Editor, points: []const records.Vec3, out: []bridge_mod.PlannedPiece) EditError!?usize {
        var total: usize = 0;
        const result = self.bridge.planEntrenchment(points, out, &total);
        if (result == .refused and total > out.len) return total;
        if (result == .refused) return null;
        try bridge_mod.check(result);
        return total;
    }

    /// Deletes the whole entrenchment at `index` (D-13): its entry and every
    /// piece, one undo step that puts them back at the same index. Refused
    /// (it holds units, a piece the editor could not put back) with nothing
    /// changed.
    pub fn deleteEntrenchment(self: *Editor, index: usize) EditError!void {
        var prepared = try self.prepareEdit(0, .objects);
        defer prepared.tokens.deinit(self.allocator);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.deleteEntrenchment(@intCast(index), &token));
        self.commitEdit(&prepared, token, 0, .objects);
        try self.reloadObjectsAfterEdit();
    }

    /// The map's entrenchments in list order; the caller frees the slice with
    /// `allocator`. A read: the status line is left alone.
    pub fn entrenchments(self: *Editor, allocator: std.mem.Allocator) EditError![]bridge_mod.EntrenchmentInfo {
        var total: usize = 0;
        var none: [0]bridge_mod.EntrenchmentInfo = .{};
        const sizing = self.bridge.entrenchments(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        const out = try allocator.alloc(bridge_mod.EntrenchmentInfo, total);
        errdefer allocator.free(out);
        try bridge_mod.check(self.bridge.entrenchments(out, &total));
        return out;
    }

    /// The bridge or entrenchment under a screen point (window pixels), or
    /// null. A read: the status line is left alone.
    pub fn pickGroup(self: *Editor, sx: f32, sy: f32) EditError!?bridge_mod.GroupRef {
        var kind: bridge_mod.GroupKind = .bridge;
        var index: i32 = -1;
        const result = self.bridge.pickGroup(sx, sy, &kind, &index);
        if (result == .refused) return null;
        try bridge_mod.check(result);
        if (index < 0) return null;
        return .{ .kind = kind, .index = @intCast(index) };
    }

    /// The object database's bridge types, sorted; the caller frees the
    /// slice with `allocator`. A read: the status line is left alone.
    pub fn bridgeDescriptors(self: *Editor, allocator: std.mem.Allocator) EditError![]bridge_mod.BridgeDescriptor {
        var total: usize = 0;
        var none: [0]bridge_mod.BridgeDescriptor = .{};
        const sizing = self.bridge.bridgeDescriptors(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        const out = try allocator.alloc(bridge_mod.BridgeDescriptor, total);
        errdefer allocator.free(out);
        try bridge_mod.check(self.bridge.bridgeDescriptors(out, &total));
        return out;
    }

    /// The spans a drag would place (MAP units), changing nothing - the
    /// ghost. Writes at most `out.len` and returns how many the plan has;
    /// null when the drag is refused, the reason in `bridge.lastMessage()`.
    /// A read: the status line is left alone (the ghost asks every frame).
    pub fn planBridge(self: *Editor, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, out: []bridge_mod.PlannedPiece) EditError!?usize {
        var total: usize = 0;
        const result = self.bridge.planBridge(desc, wx0, wy0, wx1, wy1, out, &total);
        if (result == .refused and total > out.len) return total;
        if (result == .refused) return null;
        try bridge_mod.check(result);
        return total;
    }

    /// The map's bridges entries in list order; the caller frees the slice
    /// with `allocator`. A read: the status line is left alone.
    pub fn bridges(self: *Editor, allocator: std.mem.Allocator) EditError![]bridge_mod.BridgeInfo {
        var total: usize = 0;
        var none: [0]bridge_mod.BridgeInfo = .{};
        const sizing = self.bridge.bridges(&none, &total);
        if (sizing != .ok and sizing != .refused) return error.Failed;
        const out = try allocator.alloc(bridge_mod.BridgeInfo, total);
        errdefer allocator.free(out);
        try bridge_mod.check(self.bridge.bridges(out, &total));
        return out;
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
            .multi_delete => |*d| {
                if (forwards) {
                    for (d.deleted.items) |record| try self.removeFrom(record.object.link_id);
                } else {
                    // Last deleted first, each at the index its own deletion
                    // recorded: the exact reverse of the way they went.
                    var i = d.deleted.items.len;
                    while (i != 0) {
                        i -= 1;
                        const member = d.deleted.items[i];
                        try self.restoreInto(member.object, member.index);
                    }
                }
            },
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
            .script_id => |sid| {
                const value = if (forwards) sid.after else sid.before;
                try self.noteOutcome(self.bridge.setObjectScriptID(sid.link_id, value));
                (self.document.find(sid.link_id) orelse return error.Failed).script_id = value;
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
            .record_edit => |*e| {
                const value = if (forwards) &e.after else &e.before;
                try self.noteOutcome(self.bridge.putRecord(e.key, value));
                self.record_generations.set(e.kind, self.record_generations.get(e.kind) +% 1);
            },
            .record_add => |*e| {
                if (forwards) try self.noteOutcome(self.bridge.insertRecord(e.key, &e.value)) else try self.noteOutcome(self.bridge.removeRecord(e.kind, e.key));
                self.record_generations.set(e.kind, self.record_generations.get(e.kind) +% 1);
            },
            .record_delete => |*e| {
                if (forwards) try self.noteOutcome(self.bridge.removeRecord(e.kind, e.key)) else try self.noteOutcome(self.bridge.insertRecord(e.key, &e.value));
                self.record_generations.set(e.kind, self.record_generations.get(e.kind) +% 1);
            },
            .composite => |*c| try self.replayComposite(c.steps.items, forwards),
            .edit => |e| {
                // A token that fails part-way leaves the ones before it replayed
                // in the bridge's own log: they are put back, so the log and this
                // entry agree again and a retry starts where this one did.
                if (forwards) {
                    for (e.tokens.items, 0..) |token, done| {
                        self.noteOutcome(self.bridge.redoEdit(token)) catch |err| {
                            self.unwindTokens(e.tokens.items[0..done], .undo);
                            return err;
                        };
                    }
                } else {
                    var index = e.tokens.items.len;
                    while (index != 0) {
                        index -= 1;
                        self.noteOutcome(self.bridge.undoEdit(e.tokens.items[index])) catch |err| {
                            self.unwindTokens(e.tokens.items[index + 1 ..], .redo);
                            return err;
                        };
                    }
                }
                // The document's re-read (scope `objects`) is `afterReplay`'s:
                // it runs once the entry has moved, so a failed re-read cannot
                // leave the entry on the stack the bridge has already left.
                self.bumpScope(e.scope);
            },
        }
    }

    /// How many objects a replay of `command` may put back in the document (each one
    /// needs a free slot before the bridge acts): one for a restore, a composite's
    /// steps in all.
    fn replayRoom(command: *const Command) usize {
        return switch (command.*) {
            .composite => |c| @max(c.steps.items.len, 1),
            else => 1,
        };
    }

    /// A composite's steps one after another - last first when undoing - and, when
    /// one fails after others went through, the ones that did put back the other
    /// way (best effort): if even that fails, the history no longer matches the map
    /// and `replay_broken` says so.
    fn replayComposite(self: *Editor, steps: []Command, forwards: bool) EditError!void {
        var done: usize = 0;
        while (done < steps.len) : (done += 1) {
            const index = if (forwards) done else steps.len - 1 - done;
            self.replay(&steps[index], forwards) catch |err| {
                var back: usize = done;
                while (back > 0) {
                    back -= 1;
                    const undone = if (forwards) back else steps.len - 1 - back;
                    self.replay(&steps[undone], !forwards) catch {
                        self.replay_broken = true;
                        break;
                    };
                }
                return err;
            };
        }
    }

    fn removeFrom(self: *Editor, link_id: i32) EditError!void {
        const index = self.document.indexOf(link_id) orelse return error.Failed;
        try self.noteOutcome(self.bridge.deleteObject(link_id));
        self.noteCascade();
        self.bumpCascadeGenerations();
        _ = self.document.objects.orderedRemove(index);
        if (self.selection == link_id) self.selection = null;
        _ = self.selection_set.remove(link_id);
    }

    /// Needs one free slot in `document.objects`, which `undo` and `redo`
    /// reserve before they replay: once the bridge has restored the object,
    /// the document must not be able to miss it.
    fn restoreInto(self: *Editor, object: ObjectRecord, index: usize) EditError!void {
        try self.noteOutcome(self.bridge.restoreObject(object.link_id));
        self.bumpCascadeGenerations();
        self.document.objects.insertAssumeCapacity(@min(index, self.document.objects.items.len), object);
    }

    fn drifted(err: EditError) EditError {
        return if (err == error.Refused) error.Failed else err;
    }

    const Unwind = enum { undo, redo };

    /// Puts back the bridge-logged tokens a failed replay had already replayed:
    /// `.redo` re-applies tokens an undo took back (oldest first, the order the
    /// bridge undid them in reverse), `.undo` takes back tokens a redo applied
    /// (newest first). The status line keeps the failure's reason. A token
    /// that will not go back marks the history as broken (`replay_broken`).
    fn unwindTokens(self: *Editor, tokens: []const i32, direction: Unwind) void {
        switch (direction) {
            .redo => for (tokens) |token| {
                if (self.bridge.redoEdit(token) != .ok) {
                    self.replay_broken = true;
                    return;
                }
            },
            .undo => {
                var index = tokens.len;
                while (index != 0) {
                    index -= 1;
                    if (self.bridge.undoEdit(tokens[index]) != .ok) {
                        self.replay_broken = true;
                        return;
                    }
                }
            },
        }
    }

    /// What a replayed command needs once its entry has moved stacks: the
    /// document's objects read again after an `objects`-scope edit - and
    /// after an `altitudes`-scope one, whose composites (the Fields tool's
    /// apply, which fills objects as well as heights) add and take objects
    /// back too: applyField re-reads after the apply, so its undo and redo
    /// must as well, or the document keeps the fill's objects the map no
    /// longer holds. A failed re-read is reported (the history stays as the
    /// bridge is) and asks for a reopen.
    fn afterReplay(self: *Editor, command: *const history_mod.Command) EditError!void {
        switch (command.*) {
            .composite => |c| for (c.steps.items) |*step| try self.afterReplay(step),
            .edit => |e| switch (e.scope) {
                .objects, .altitudes => try self.reloadObjectsAfterEdit(),
                .players => try self.reloadPlayersAfterEdit(),
                .vso => {},
            },
            else => {},
        }
    }

    /// Re-reads what a player add or delete (or its replay) changed: the
    /// diplomacy table - its length is the player count - and the objects, whose
    /// owners moved. The table is probed player by player until the bridge says
    /// there is no such player (a map holds at most 17 entries), and everything
    /// is staged before `self` is touched.
    fn reloadPlayers(self: *Editor) EditError!void {
        var table: std.ArrayListUnmanaged(i32) = .empty;
        errdefer table.deinit(self.allocator);
        var player: i32 = 0;
        while (player < 32) : (player += 1) {
            var side: i32 = 0;
            if (self.bridge.diplomacy(player, &side) != .ok) break;
            try table.append(self.allocator, side);
        }
        try self.reloadObjects();
        self.document.diplomacy.deinit(self.allocator);
        self.document.diplomacy = table;
        self.document.info.player_count = @intCast(self.document.diplomacy.items.len);
    }

    fn reloadPlayersAfterEdit(self: *Editor) EditError!void {
        self.reloadPlayers() catch |err| {
            self.setStatus("", "the edit went through but the map's players could not be read again; reopen the map");
            return err;
        };
    }

    /// `reloadObjects` after a bridge edit has committed (or been undone or
    /// redone): the history already holds the step, so a failure only leaves
    /// the document's object list stale, which the status line says.
    fn reloadObjectsAfterEdit(self: *Editor) EditError!void {
        self.reloadObjects() catch |err| {
            self.setStatus("", "the edit went through but the map's objects could not be read again; reopen the map");
            return err;
        };
    }

    fn refuseBrokenReplay(self: *Editor) EditError {
        self.setStatus("", "the undo history no longer matches the map after a failed undo or redo; reopen the map");
        return error.Failed;
    }

    /// False when there is nothing to undo. On a failure the entry stays
    /// where it was and the status line says why; the map should be reopened.
    pub fn undo(self: *Editor) EditError!bool {
        const count = self.history.undo_stack.items.len;
        if (count == 0) return false;
        if (self.replay_broken) return self.refuseBrokenReplay();
        // Room first: once the bridge has undone it, the entry must not be
        // lost to an allocation failure, nor a restored object be missing
        // from the document.
        try self.history.redo_stack.ensureUnusedCapacity(self.allocator, 1);
        try self.document.objects.ensureUnusedCapacity(self.allocator, replayRoom(&self.history.undo_stack.items[count - 1].command));
        var entry = self.history.undo_stack.items[count - 1];
        self.replay(&entry.command, false) catch |err| return drifted(err);
        _ = self.history.undo_stack.pop();
        self.history.redo_stack.appendAssumeCapacity(entry);
        self.history.revision +%= 1;
        try self.afterReplay(&entry.command);
        return true;
    }

    pub fn redo(self: *Editor) EditError!bool {
        const count = self.history.redo_stack.items.len;
        if (count == 0) return false;
        if (self.replay_broken) return self.refuseBrokenReplay();
        try self.history.undo_stack.ensureUnusedCapacity(self.allocator, 1);
        try self.document.objects.ensureUnusedCapacity(self.allocator, replayRoom(&self.history.redo_stack.items[count - 1].command));
        var entry = self.history.redo_stack.items[count - 1];
        self.replay(&entry.command, true) catch |err| return drifted(err);
        _ = self.history.redo_stack.pop();
        self.history.undo_stack.appendAssumeCapacity(entry);
        self.history.revision +%= 1;
        try self.afterReplay(&entry.command);
        return true;
    }
    // -----------------------------------------------------------------
    // The Fields tool (M3, D-21). One application is one undoable command
    // (gesture 0); the polygon keys live in tools_fields.Fields.
    // -----------------------------------------------------------------

    /// The fields application: ONE command (reserve, bridge, record - the
    /// reserve-before-bridge rule), whatever mixture of tiles, objects,
    /// heights and the nested update map it ran. `report` (nullable) is
    /// filled with what the object shells produced; the buffer is one call's
    /// upper bound (the fill places at most one object per half-tile cell),
    /// so the bridge runs once.
    pub fn applyField(self: *Editor, params: bridge_mod.FieldApplyParams, report: ?*std.ArrayListUnmanaged(bridge_mod.FieldObjectReport), allocator: std.mem.Allocator) EditError!void {
        var prepared = try self.prepareEdit(0, .altitudes);
        defer prepared.tokens.deinit(self.allocator);
        var reports: []bridge_mod.FieldObjectReport = &.{};
        if (report != null) {
            const bound = @as(usize, @intCast(@max(self.document.info.width_tiles, 1))) *
                @as(usize, @intCast(@max(self.document.info.height_tiles, 1))) / 2 + 64;
            reports = allocator.alloc(bridge_mod.FieldObjectReport, bound) catch return error.OutOfMemory;
        }
        defer if (report != null) allocator.free(reports);
        var total: usize = 0;
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.applyField(params, reports[0..], &total, &token));
        if (report) |out| {
            out.clearRetainingCapacity();
            out.appendSlice(allocator, reports[0..@min(total, reports.len)]) catch return error.OutOfMemory;
        }
        self.commitEdit(&prepared, token, 0, .altitudes);
        // The objects the fill may have added: the document's object list is
        // stale until re-read (a failure here leaves the edit committed).
        self.reloadObjectsAfterEdit() catch {};
    }

    /// Create Random Map (05-08, D-01..D-05): one generation, written into the
    /// user's (or the mod's) maps folder - not an edit of the open map, so
    /// nothing joins the undo history and the document is untouched; the
    /// caller opens the generated file (`result.mapPathSlice()`) through the
    /// normal open path. A refusal's reason is the status line's.
    pub fn createRandomMap(self: *Editor, params: bridge_mod.RmgGenerateParams, result: *bridge_mod.RmgGenerateResult) EditError!void {
        result.* = .{};
        try self.noteOutcome(self.bridge.createRandomMap(params, result));
    }

    /// The field set's season, for the YES/NO confirmation before an apply.
    pub fn fieldSetSeason(self: *Editor, name: []const u8) EditError!i32 {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = std.fmt.bufPrintZ(&buffer, "{s}", .{name}) catch return error.Refused;
        var season: i32 = -1;
        try self.noteOutcome(self.bridge.fieldSetSeason(name_z, &season));
        return season;
    }

    // --- The RMG composers (05-09, D-06/D-07/D-10) --------------------------
    //
    // Composer files are file-level: nothing here joins the map's undo
    // history, touches the open map or marks the document dirty. A record is
    // read in the two passes bridge.h describes (size with empty arrays, then
    // exactly what was answered) and written whole.

    /// A record the bridge answered, with the arrays it points into.
    const OwnedContainer = struct {
        record: bridge_mod.RmgContainerRecord = .{},
        patches: []bridge_mod.RmgPatch = &.{},
        indices: []c_int = &.{},
        ids: []c_int = &.{},
        areas: []bridge_mod.RmgName = &.{},

        fn deinit(self: *OwnedContainer, a: std.mem.Allocator) void {
            a.free(self.patches);
            a.free(self.indices);
            a.free(self.ids);
            a.free(self.areas);
            self.* = .{};
        }
    };

    const OwnedGraph = struct {
        record: bridge_mod.RmgGraphRecord = .{},
        nodes: []bridge_mod.RmgNode = &.{},
        links: []bridge_mod.RmgLink = &.{},
        ids: []c_int = &.{},
        areas: []bridge_mod.RmgName = &.{},

        fn deinit(self: *OwnedGraph, a: std.mem.Allocator) void {
            a.free(self.nodes);
            a.free(self.links);
            a.free(self.ids);
            a.free(self.areas);
            self.* = .{};
        }
    };

    fn countOf(value: c_int) usize {
        return @intCast(@max(value, 0));
    }

    /// The sizing pass said "counts returned" when it was refused with something
    /// to size; a refusal with every count 0 is a real one.
    fn sizedRefusal(scripts: bridge_mod.RmgScripts, extra: usize) bool {
        return countOf(scripts.id_count) + countOf(scripts.area_count) + extra > 0;
    }

    fn fetchContainer(self: *Editor, a: std.mem.Allocator, name: [*:0]const u8, out: *OwnedContainer) std.mem.Allocator.Error!bridge_mod.Status {
        out.* = .{};
        errdefer out.deinit(a);
        var first = bridge_mod.RmgContainerRecord{};
        const sizing = self.bridge.rmgReadContainer(name, &first);
        const index_total = countOf(first.index_counts[0]) + countOf(first.index_counts[1]) + countOf(first.index_counts[2]) + countOf(first.index_counts[3]);
        if (sizing == .ok) {
            out.record = first;
            return .ok;
        }
        if (sizing != .refused or !sizedRefusal(first.scripts, countOf(first.patch_count) + index_total)) return sizing;
        out.patches = try a.alloc(bridge_mod.RmgPatch, countOf(first.patch_count));
        out.indices = try a.alloc(c_int, index_total);
        out.ids = try a.alloc(c_int, countOf(first.scripts.id_count));
        out.areas = try a.alloc(bridge_mod.RmgName, countOf(first.scripts.area_count));
        var second = first;
        second.patches = out.patches.ptr;
        second.patch_capacity = @intCast(out.patches.len);
        second.indices = out.indices.ptr;
        second.index_capacity = @intCast(out.indices.len);
        second.scripts.ids = out.ids.ptr;
        second.scripts.id_capacity = @intCast(out.ids.len);
        second.scripts.areas = out.areas.ptr;
        second.scripts.area_capacity = @intCast(out.areas.len);
        const read = self.bridge.rmgReadContainer(name, &second);
        out.record = second;
        // The totals cannot have moved between the passes: anything but ok is a failure.
        return if (read == .refused) .failed else read;
    }

    fn containerFromOwned(a: std.mem.Allocator, owned: *const OwnedContainer) std.mem.Allocator.Error!rmg_mod.Container {
        const record = &owned.record;
        var out: rmg_mod.Container = .{ .size_x = record.size_x, .size_y = record.size_y, .season = record.season };
        errdefer out.deinit(a);
        out.season_folder = try a.dupe(u8, std.mem.sliceTo(&record.season_folder, 0));
        for (owned.patches[0..@min(owned.patches.len, countOf(record.patch_count))]) |patch| {
            var made: rmg_mod.Patch = .{ .size_x = patch.size_x, .size_y = patch.size_y };
            made.name = try a.dupe(u8, std.mem.sliceTo(&patch.name, 0));
            errdefer a.free(made.name);
            made.place = try a.dupe(u8, std.mem.sliceTo(&patch.place, 0));
            errdefer a.free(made.place);
            try out.patches.append(a, made);
        }
        var at: usize = 0;
        for (record.index_counts, 0..) |count, d| {
            for (0..countOf(count)) |_| {
                if (at >= owned.indices.len) break;
                try out.indices[d].append(a, owned.indices[at]);
                at += 1;
            }
        }
        try out.script_ids.appendSlice(a, owned.ids[0..@min(owned.ids.len, countOf(record.scripts.id_count))]);
        for (owned.areas[0..@min(owned.areas.len, countOf(record.scripts.area_count))]) |area| {
            const copy = try a.dupe(u8, std.mem.sliceTo(&area.name, 0));
            errdefer a.free(copy);
            try out.script_areas.append(a, copy);
        }
        return out;
    }

    fn nameZ(buffer: *[bridge_mod.field_set_name_capacity:0]u8, name: []const u8) ?[*:0]const u8 {
        if (name.len == 0 or name.len >= bridge_mod.field_set_name_capacity) return null;
        const text = std.fmt.bufPrintZ(buffer, "{s}", .{name}) catch return null;
        return text.ptr;
    }

    /// Container `name` as the storages hold it (the user's root first). The
    /// caller owns the result (`deinit(allocator)`).
    pub fn readContainer(self: *Editor, name: []const u8) EditError!rmg_mod.Container {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return error.Refused;
        var owned: OwnedContainer = .{};
        defer owned.deinit(self.allocator);
        try self.noteOutcome(try self.fetchContainer(self.allocator, name_z, &owned));
        return try containerFromOwned(self.allocator, &owned);
    }

    /// Writes `container` as `name` under the user RMG root. A shipped name is
    /// refused (the status says Save As); nothing changes then.
    pub fn writeContainer(self: *Editor, name: []const u8, container: *const rmg_mod.Container) EditError!void {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return error.Refused;
        const a = self.allocator;
        const patches = try a.alloc(bridge_mod.RmgPatch, container.patches.items.len);
        defer a.free(patches);
        var index_total: usize = 0;
        for (container.indices) |list| index_total += list.items.len;
        const indices = try a.alloc(c_int, index_total);
        defer a.free(indices);
        const areas = try a.alloc(bridge_mod.RmgName, container.script_areas.items.len);
        defer a.free(areas);
        const ids = try a.alloc(c_int, container.script_ids.items.len);
        defer a.free(ids);
        var record = bridge_mod.RmgContainerRecord{ .size_x = container.size_x, .size_y = container.size_y, .season = container.season };
        if (!bridge_mod.putName(&record.season_folder, container.season_folder)) return error.Refused;
        for (container.patches.items, 0..) |patch, i| {
            patches[i] = .{ .size_x = patch.size_x, .size_y = patch.size_y };
            if (!bridge_mod.putName(&patches[i].name, patch.name) or !bridge_mod.putName(&patches[i].place, patch.place)) return error.Refused;
        }
        var at: usize = 0;
        for (container.indices, 0..) |list, d| {
            record.index_counts[d] = @intCast(list.items.len);
            for (list.items) |entry| {
                indices[at] = entry;
                at += 1;
            }
        }
        for (container.script_ids.items, 0..) |id, i| ids[i] = id;
        for (container.script_areas.items, 0..) |area, i| {
            areas[i] = .{};
            if (!bridge_mod.putName(&areas[i].name, area)) return error.Refused;
        }
        record.patches = patches.ptr;
        record.patch_count = @intCast(patches.len);
        record.patch_capacity = record.patch_count;
        record.indices = indices.ptr;
        record.index_capacity = @intCast(indices.len);
        record.scripts = .{ .ids = ids.ptr, .id_capacity = @intCast(ids.len), .id_count = @intCast(ids.len), .areas = areas.ptr, .area_capacity = @intCast(areas.len), .area_count = @intCast(areas.len) };
        try self.noteOutcome(self.bridge.rmgWriteContainer(name_z, &record));
    }

    fn fetchGraph(self: *Editor, a: std.mem.Allocator, name: [*:0]const u8, out: *OwnedGraph) std.mem.Allocator.Error!bridge_mod.Status {
        out.* = .{};
        errdefer out.deinit(a);
        var first = bridge_mod.RmgGraphRecord{};
        const sizing = self.bridge.rmgReadGraph(name, &first);
        if (sizing == .ok) {
            out.record = first;
            return .ok;
        }
        if (sizing != .refused or !sizedRefusal(first.scripts, countOf(first.node_count) + countOf(first.link_count))) return sizing;
        out.nodes = try a.alloc(bridge_mod.RmgNode, countOf(first.node_count));
        out.links = try a.alloc(bridge_mod.RmgLink, countOf(first.link_count));
        out.ids = try a.alloc(c_int, countOf(first.scripts.id_count));
        out.areas = try a.alloc(bridge_mod.RmgName, countOf(first.scripts.area_count));
        var second = first;
        second.nodes = out.nodes.ptr;
        second.node_capacity = @intCast(out.nodes.len);
        second.links = out.links.ptr;
        second.link_capacity = @intCast(out.links.len);
        second.scripts.ids = out.ids.ptr;
        second.scripts.id_capacity = @intCast(out.ids.len);
        second.scripts.areas = out.areas.ptr;
        second.scripts.area_capacity = @intCast(out.areas.len);
        const read = self.bridge.rmgReadGraph(name, &second);
        out.record = second;
        return if (read == .refused) .failed else read;
    }

    /// Graph `name` as the storages hold it. The caller owns the result.
    pub fn readGraph(self: *Editor, name: []const u8) EditError!rmg_mod.Graph {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return error.Refused;
        const a = self.allocator;
        var owned: OwnedGraph = .{};
        defer owned.deinit(a);
        try self.noteOutcome(try self.fetchGraph(a, name_z, &owned));
        const record = &owned.record;
        var out: rmg_mod.Graph = .{ .size_x = record.size_x, .size_y = record.size_y, .season = record.season };
        errdefer out.deinit(a);
        out.season_folder = try a.dupe(u8, std.mem.sliceTo(&record.season_folder, 0));
        for (owned.nodes[0..@min(owned.nodes.len, countOf(record.node_count))]) |node| {
            const container = try a.dupe(u8, std.mem.sliceTo(&node.container, 0));
            errdefer a.free(container);
            try out.nodes.append(a, .{ .rect = .{ .x1 = node.x1, .y1 = node.y1, .x2 = node.x2, .y2 = node.y2 }, .container = container });
        }
        for (owned.links[0..@min(owned.links.len, countOf(record.link_count))]) |link| {
            const desc = try a.dupe(u8, std.mem.sliceTo(&link.desc, 0));
            errdefer a.free(desc);
            try out.links.append(a, .{ .a = link.a, .b = link.b, .kind = link.kind, .desc = desc, .radius = link.radius, .parts = link.parts, .min_length = link.min_length, .distance = link.distance, .disturbance = link.disturbance });
        }
        try out.script_ids.appendSlice(a, owned.ids[0..@min(owned.ids.len, countOf(record.scripts.id_count))]);
        for (owned.areas[0..@min(owned.areas.len, countOf(record.scripts.area_count))]) |area| {
            const copy = try a.dupe(u8, std.mem.sliceTo(&area.name, 0));
            errdefer a.free(copy);
            try out.script_areas.append(a, copy);
        }
        return out;
    }

    pub fn writeGraph(self: *Editor, name: []const u8, graph: *const rmg_mod.Graph) EditError!void {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return error.Refused;
        const a = self.allocator;
        const nodes = try a.alloc(bridge_mod.RmgNode, graph.nodes.items.len);
        defer a.free(nodes);
        const links = try a.alloc(bridge_mod.RmgLink, graph.links.items.len);
        defer a.free(links);
        const areas = try a.alloc(bridge_mod.RmgName, graph.script_areas.items.len);
        defer a.free(areas);
        const ids = try a.alloc(c_int, graph.script_ids.items.len);
        defer a.free(ids);
        var record = bridge_mod.RmgGraphRecord{ .size_x = graph.size_x, .size_y = graph.size_y, .season = graph.season };
        if (!bridge_mod.putName(&record.season_folder, graph.season_folder)) return error.Refused;
        for (graph.nodes.items, 0..) |node, i| {
            nodes[i] = .{ .x1 = node.rect.x1, .y1 = node.rect.y1, .x2 = node.rect.x2, .y2 = node.rect.y2 };
            if (!bridge_mod.putName(&nodes[i].container, node.container)) return error.Refused;
        }
        for (graph.links.items, 0..) |link, i| {
            links[i] = .{ .a = link.a, .b = link.b, .kind = link.kind, .radius = link.radius, .parts = link.parts, .min_length = link.min_length, .distance = link.distance, .disturbance = link.disturbance };
            if (!bridge_mod.putName(&links[i].desc, link.desc)) return error.Refused;
        }
        for (graph.script_ids.items, 0..) |id, i| ids[i] = id;
        for (graph.script_areas.items, 0..) |area, i| {
            areas[i] = .{};
            if (!bridge_mod.putName(&areas[i].name, area)) return error.Refused;
        }
        record.nodes = nodes.ptr;
        record.node_count = @intCast(nodes.len);
        record.node_capacity = record.node_count;
        record.links = links.ptr;
        record.link_count = @intCast(links.len);
        record.link_capacity = record.link_count;
        record.scripts = .{ .ids = ids.ptr, .id_capacity = @intCast(ids.len), .id_count = @intCast(ids.len), .areas = areas.ptr, .area_capacity = @intCast(areas.len), .area_count = @intCast(areas.len) };
        try self.noteOutcome(self.bridge.rmgWriteGraph(name_z, &record));
    }

    /// What a patch map says about itself (null: the data does not hold it or
    /// it does not load). Does not touch the status line: Check! asks this for
    /// every patch.
    fn patchSummary(self: *Editor, a: std.mem.Allocator, name: []const u8) ?rmg_mod.Summary {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return null;
        var first = bridge_mod.RmgPatchInfo{};
        const sizing = self.bridge.rmgPatchInfo(name_z, &first);
        var info = first;
        var ids: []c_int = &.{};
        var areas: []bridge_mod.RmgName = &.{};
        defer a.free(ids);
        defer a.free(areas);
        if (sizing != .ok) {
            if (sizing != .refused or !sizedRefusal(first.scripts, 0)) return null;
            ids = a.alloc(c_int, countOf(first.scripts.id_count)) catch return null;
            areas = a.alloc(bridge_mod.RmgName, countOf(first.scripts.area_count)) catch return null;
            info.scripts.ids = ids.ptr;
            info.scripts.id_capacity = @intCast(ids.len);
            info.scripts.areas = areas.ptr;
            info.scripts.area_capacity = @intCast(areas.len);
            if (self.bridge.rmgPatchInfo(name_z, &info) != .ok) return null;
        }
        return summaryFrom(a, info.size_x, info.size_y, info.season, &info.season_folder, ids[0..@min(ids.len, countOf(info.scripts.id_count))], areas[0..@min(areas.len, countOf(info.scripts.area_count))]) catch null;
    }

    fn summaryFrom(a: std.mem.Allocator, size_x: i32, size_y: i32, season: i32, folder: []const u8, ids: []const c_int, areas: []const bridge_mod.RmgName) std.mem.Allocator.Error!rmg_mod.Summary {
        var out: rmg_mod.Summary = .{ .size_x = size_x, .size_y = size_y, .season = season };
        errdefer out.deinit(a);
        out.season_folder = try a.dupe(u8, std.mem.sliceTo(folder, 0));
        out.script_ids = try a.dupe(i32, ids);
        const list = try a.alloc([]u8, areas.len);
        out.script_areas = list[0..0];
        errdefer {
            for (list[0..out.script_areas.len]) |area| a.free(area);
            a.free(list);
        }
        for (areas, 0..) |area, i| {
            list[i] = try a.dupe(u8, std.mem.sliceTo(&area.name, 0));
            out.script_areas = list[0 .. i + 1];
        }
        return out;
    }

    fn containerSummary(self: *Editor, a: std.mem.Allocator, name: []const u8) ?rmg_mod.Summary {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return null;
        var owned: OwnedContainer = .{};
        defer owned.deinit(a);
        const status_code = self.fetchContainer(a, name_z, &owned) catch return null;
        if (status_code != .ok) return null;
        const record = &owned.record;
        return summaryFrom(a, record.size_x, record.size_y, record.season, &record.season_folder, owned.ids[0..@min(owned.ids.len, countOf(record.scripts.id_count))], owned.areas[0..@min(owned.areas.len, countOf(record.scripts.area_count))]) catch null;
    }

    fn patchSourceFn(ctx: *anyopaque, a: std.mem.Allocator, name: []const u8) ?rmg_mod.Summary {
        const self: *Editor = @ptrCast(@alignCast(ctx));
        return self.patchSummary(a, name);
    }

    fn containerSourceFn(ctx: *anyopaque, a: std.mem.Allocator, name: []const u8) ?rmg_mod.Summary {
        const self: *Editor = @ptrCast(@alignCast(ctx));
        return self.containerSummary(a, name);
    }

    /// The facts Check! and the add rules ask: patch maps and containers as
    /// the storages hold them.
    pub fn rmgSource(self: *Editor) rmg_mod.Source {
        return .{ .ctx = self, .patch_fn = patchSourceFn, .container_fn = containerSourceFn };
    }

    /// D-10: a map outside the storages, copied into the user RMG root's
    /// Scenarios/Patches/<season>/. `apply` false only names the destination
    /// (what the YES/NO popup shows). Returns the storage name, in `out`.
    pub fn importPatch(self: *Editor, source_path: []const u8, apply: bool, out: *bridge_mod.RmgName) EditError!void {
        var buffer: [2048:0]u8 = undefined;
        if (source_path.len == 0 or source_path.len >= buffer.len) return error.Refused;
        const path_z = std.fmt.bufPrintZ(&buffer, "{s}", .{source_path}) catch return error.Refused;
        out.* = .{};
        try self.noteOutcome(self.bridge.rmgImportPatch(path_z.ptr, apply, out));
    }

    // --- The Fields Composer's field sets (05-10, D-06/D-07/D-12) ----------

    const OwnedFieldSet = struct {
        record: bridge_mod.RmgFieldSetRecord = .{},
        tile_shells: []bridge_mod.RmgTileShell = &.{},
        tiles: []bridge_mod.RmgWeightedTile = &.{},
        object_shells: []bridge_mod.RmgObjectShell = &.{},
        objects: []bridge_mod.RmgWeightedName = &.{},

        fn deinit(self: *OwnedFieldSet, a: std.mem.Allocator) void {
            a.free(self.tile_shells);
            a.free(self.tiles);
            a.free(self.object_shells);
            a.free(self.objects);
            self.* = .{};
        }
    };

    fn fetchFieldSet(self: *Editor, a: std.mem.Allocator, name: [*:0]const u8, out: *OwnedFieldSet) std.mem.Allocator.Error!bridge_mod.Status {
        out.* = .{};
        errdefer out.deinit(a);
        var first = bridge_mod.RmgFieldSetRecord{};
        const sizing = self.bridge.rmgReadFieldSet(name, &first);
        if (sizing == .ok) {
            out.record = first;
            return .ok;
        }
        const totals = countOf(first.tile_shell_count) + countOf(first.tile_total) + countOf(first.object_shell_count) + countOf(first.object_total);
        if (sizing != .refused or totals == 0) return sizing;
        out.tile_shells = try a.alloc(bridge_mod.RmgTileShell, countOf(first.tile_shell_count));
        out.tiles = try a.alloc(bridge_mod.RmgWeightedTile, countOf(first.tile_total));
        out.object_shells = try a.alloc(bridge_mod.RmgObjectShell, countOf(first.object_shell_count));
        out.objects = try a.alloc(bridge_mod.RmgWeightedName, countOf(first.object_total));
        var second = first;
        second.tile_shells = out.tile_shells.ptr;
        second.tile_shell_capacity = @intCast(out.tile_shells.len);
        second.tiles = out.tiles.ptr;
        second.tile_capacity = @intCast(out.tiles.len);
        second.object_shells = out.object_shells.ptr;
        second.object_shell_capacity = @intCast(out.object_shells.len);
        second.objects = out.objects.ptr;
        second.object_capacity = @intCast(out.objects.len);
        const read = self.bridge.rmgReadFieldSet(name, &second);
        out.record = second;
        // The totals cannot have moved between the passes: anything but ok is a failure.
        return if (read == .refused) .failed else read;
    }

    /// Field set `name` as the storages hold it (the user's root first). The
    /// caller owns the result.
    pub fn readFieldSet(self: *Editor, name: []const u8) EditError!rmg_mod.FieldSet {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return error.Refused;
        const a = self.allocator;
        var owned: OwnedFieldSet = .{};
        defer owned.deinit(a);
        try self.noteOutcome(try self.fetchFieldSet(a, name_z, &owned));
        const record = &owned.record;
        var out: rmg_mod.FieldSet = .{ .season = record.season, .height = record.height, .pattern_min = record.pattern_min, .pattern_max = record.pattern_max, .positive_ratio = record.positive_ratio };
        errdefer out.deinit(a);
        out.season_folder = try a.dupe(u8, std.mem.sliceTo(&record.season_folder, 0));
        out.profile = try a.dupe(u8, std.mem.sliceTo(&record.profile, 0));
        var tile_at: usize = 0;
        for (owned.tile_shells[0..@min(owned.tile_shells.len, countOf(record.tile_shell_count))]) |shell| {
            var made: rmg_mod.TileShell = .{ .width = shell.width };
            errdefer made.deinit(a);
            const count = @min(countOf(shell.tile_count), owned.tiles.len - tile_at);
            for (owned.tiles[tile_at .. tile_at + count]) |entry| try made.tiles.append(a, .{ .tile = entry.tile, .weight = entry.weight });
            tile_at += count;
            try out.tile_shells.append(a, made);
        }
        var object_at: usize = 0;
        for (owned.object_shells[0..@min(owned.object_shells.len, countOf(record.object_shell_count))]) |shell| {
            var made: rmg_mod.ObjectShell = .{ .width = shell.width, .step = shell.step, .ratio = shell.ratio };
            errdefer made.deinit(a);
            const count = @min(countOf(shell.object_count), owned.objects.len - object_at);
            for (owned.objects[object_at .. object_at + count]) |entry| {
                const copy = try a.dupe(u8, std.mem.sliceTo(&entry.name, 0));
                errdefer a.free(copy);
                try made.objects.append(a, .{ .name = copy, .weight = entry.weight });
            }
            object_at += count;
            try out.object_shells.append(a, made);
        }
        return out;
    }

    /// Writes `field` as `name` under the user RMG root. A shipped name is
    /// refused (the status says Save As); nothing changes then.
    pub fn writeFieldSet(self: *Editor, name: []const u8, field: *const rmg_mod.FieldSet) EditError!void {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return error.Refused;
        const a = self.allocator;
        const tile_shells = try a.alloc(bridge_mod.RmgTileShell, field.tile_shells.items.len);
        defer a.free(tile_shells);
        const tiles = try a.alloc(bridge_mod.RmgWeightedTile, field.tileEntryCount());
        defer a.free(tiles);
        const object_shells = try a.alloc(bridge_mod.RmgObjectShell, field.object_shells.items.len);
        defer a.free(object_shells);
        const objects = try a.alloc(bridge_mod.RmgWeightedName, field.objectEntryCount());
        defer a.free(objects);
        var record = bridge_mod.RmgFieldSetRecord{ .season = field.season, .height = field.height, .pattern_min = field.pattern_min, .pattern_max = field.pattern_max, .positive_ratio = field.positive_ratio };
        if (!bridge_mod.putName(&record.season_folder, field.season_folder) or !bridge_mod.putName(&record.profile, field.profile)) return error.Refused;
        var tile_at: usize = 0;
        for (field.tile_shells.items, 0..) |shell, i| {
            tile_shells[i] = .{ .width = shell.width, .tile_count = @intCast(shell.tiles.items.len) };
            for (shell.tiles.items) |entry| {
                tiles[tile_at] = .{ .tile = entry.tile, .weight = entry.weight };
                tile_at += 1;
            }
        }
        var object_at: usize = 0;
        for (field.object_shells.items, 0..) |shell, i| {
            object_shells[i] = .{ .width = shell.width, .step = shell.step, .ratio = shell.ratio, .object_count = @intCast(shell.objects.items.len) };
            for (shell.objects.items) |entry| {
                objects[object_at] = .{ .weight = entry.weight };
                if (!bridge_mod.putName(&objects[object_at].name, entry.name)) return error.Refused;
                object_at += 1;
            }
        }
        record.tile_shells = tile_shells.ptr;
        record.tile_shell_count = @intCast(tile_shells.len);
        record.tile_shell_capacity = record.tile_shell_count;
        record.tiles = tiles.ptr;
        record.tile_total = @intCast(tiles.len);
        record.tile_capacity = record.tile_total;
        record.object_shells = object_shells.ptr;
        record.object_shell_count = @intCast(object_shells.len);
        record.object_shell_capacity = record.object_shell_count;
        record.objects = objects.ptr;
        record.object_total = @intCast(objects.len);
        record.object_capacity = record.object_total;
        try self.noteOutcome(self.bridge.rmgWriteFieldSet(name_z, &record));
    }

    /// The terrain types of a season's tileset (0 summer .. 3 spring): names and
    /// tile counts, the caller frees the slice. An empty slice when the tileset
    /// will not load. Never touches the status line.
    pub fn tilesetTypes(self: *Editor, a: std.mem.Allocator, season: usize) std.mem.Allocator.Error![]bridge_mod.RmgTerrainType {
        var total: usize = 0;
        _ = self.bridge.rmgTileset(@intCast(season), &.{}, &total);
        if (total == 0) return try a.alloc(bridge_mod.RmgTerrainType, 0);
        const out = try a.alloc(bridge_mod.RmgTerrainType, total);
        errdefer a.free(out);
        var got: usize = 0;
        if (self.bridge.rmgTileset(@intCast(season), out, &got) != .ok or got != total) {
            a.free(out);
            return try a.alloc(bridge_mod.RmgTerrainType, 0);
        }
        return out;
    }

    /// Whether `name` + `extension` is in the storage stack (a profile's
    /// ".tga", a script's ".lua", a descriptor's ".xml"). False for anything
    /// the bridge refuses. Never touches the status line.
    pub fn rmgFileExists(self: *Editor, name: []const u8, extension: [:0]const u8) bool {
        var buffer: [bridge_mod.field_set_name_capacity:0]u8 = undefined;
        const name_z = nameZ(&buffer, name) orelse return false;
        var exists = false;
        if (self.bridge.rmgFileExists(name_z, extension.ptr, &exists) != .ok) return false;
        return exists;
    }

    /// The user RMG root as the host spells it, in `buffer`.
    pub fn rmgRoot(self: *Editor, buffer: []u8) EditError![]const u8 {
        try self.noteOutcome(self.bridge.rmgRoot(buffer));
        return std.mem.sliceTo(buffer, 0);
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

test "an open moves every generation a panel keys on (IN-B04)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    const groups = editor.record_generations.get(.group);
    const sounds = editor.sounds_generation;
    try editor.open("fixture.bzm");
    try std.testing.expect(editor.record_generations.get(.group) != groups);
    try std.testing.expect(editor.sounds_generation != sounds);
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

test "deleting a unit named by a start command cascades and undo restores it" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixture(&.{1}, 0); // A: only unit 1, so the delete erases it
    try fake.addStartCommandFixture(&.{ 1, 3 }, 0); // B: units 1 and 3, so the delete edits it
    try fake.addStartCommandFixture(&.{3}, 0); // C: not naming unit 1, never touched
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.delete(1);
    try std.testing.expect(editor.document.find(1) == null);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(usize, 2), fake.start_commands.items.len);
    try std.testing.expectEqualSlices(i32, &.{3}, fake.start_commands.items[0].units[0..fake.start_commands.items[0].unit_count]);
    try std.testing.expectEqualSlices(i32, &.{3}, fake.start_commands.items[1].units[0..fake.start_commands.items[1].unit_count]);
    try std.testing.expectEqualStrings("also start command 0 erased; removed from start command 1", editor.status());
    const after_delete = try std.testing.allocator.dupe(FakeStartCommand, fake.start_commands.items);
    defer std.testing.allocator.free(after_delete);

    try std.testing.expect(try editor.undo());
    try std.testing.expect(editor.document.find(1) != null);
    try std.testing.expectEqual(@as(usize, 3), fake.start_commands.items.len);
    try std.testing.expectEqualSlices(i32, &.{1}, fake.start_commands.items[0].units[0..fake.start_commands.items[0].unit_count]);
    try std.testing.expectEqualSlices(i32, &.{ 1, 3 }, fake.start_commands.items[1].units[0..fake.start_commands.items[1].unit_count]);
    try std.testing.expectEqualSlices(i32, &.{3}, fake.start_commands.items[2].units[0..fake.start_commands.items[2].unit_count]);
    try std.testing.expectEqual(@as(usize, 0), editor.status().len);

    try std.testing.expect(try editor.redo());
    try std.testing.expectEqualSlices(FakeStartCommand, after_delete, fake.start_commands.items);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
}

test "the full cascade: units, targets and reserve positions, one undo step, exact undo and redo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixture(&.{1}, 0); // A: only unit 1 - erased
    try fake.addStartCommandFixture(&.{ 1, 3 }, 0); // B: unit 1 among others - edited
    try fake.addStartCommandFixture(&.{3}, 1); // C: unit 1 is its target - cleared to 0
    try fake.addStartCommandFixture(&.{3}, 3); // D: names neither - never touched
    try fake.addReservePositionFixture(1, 0); // R0: unit 1 as artillery - erased
    try fake.addReservePositionFixture(3, 3); // R1: neither - kept
    try fake.addReservePositionFixture(0, 1); // R2: unit 1 as truck - erased
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const commands_before = try std.testing.allocator.dupe(FakeStartCommand, fake.start_commands.items);
    defer std.testing.allocator.free(commands_before);
    const reserves_before = try std.testing.allocator.dupe(FakeReservePosition, fake.reserve_positions.items);
    defer std.testing.allocator.free(reserves_before);

    try editor.delete(1);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(usize, 3), fake.start_commands.items.len);
    try std.testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    try std.testing.expectEqual(@as(i32, 3), fake.reserve_positions.items[0].artillery);
    try std.testing.expectEqual(@as(i32, 0), fake.start_commands.items[1].target); // C
    try std.testing.expectEqual(@as(usize, 1), fake.start_commands.items[1].unit_count);
    try std.testing.expectEqual(@as(i32, 3), fake.start_commands.items[2].target); // D, as it was
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "reserve position 0 erased") != null);
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "reserve position 2 erased") != null);
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "target of start command 2 cleared") != null);
    const commands_after = try std.testing.allocator.dupe(FakeStartCommand, fake.start_commands.items);
    defer std.testing.allocator.free(commands_after);
    const reserves_after = try std.testing.allocator.dupe(FakeReservePosition, fake.reserve_positions.items);
    defer std.testing.allocator.free(reserves_after);

    try std.testing.expect(try editor.undo());
    try std.testing.expect(editor.document.find(1) != null);
    try std.testing.expectEqualSlices(FakeStartCommand, commands_before, fake.start_commands.items);
    try std.testing.expectEqualSlices(FakeReservePosition, reserves_before, fake.reserve_positions.items);

    try std.testing.expect(try editor.redo());
    try std.testing.expectEqualSlices(FakeStartCommand, commands_after, fake.start_commands.items);
    try std.testing.expectEqualSlices(FakeReservePosition, reserves_after, fake.reserve_positions.items);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);

    // And back once more: the redo's own cascade undoes exactly too.
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqualSlices(FakeStartCommand, commands_before, fake.start_commands.items);
    try std.testing.expectEqualSlices(FakeReservePosition, reserves_before, fake.reserve_positions.items);
}

test "a span and a trench piece are refused with the document and history untouched" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixture(&.{ 1, 2 }, 0);
    try fake.addTrenchPieceFixture(1, 4);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const commands_before = try std.testing.allocator.dupe(FakeStartCommand, fake.start_commands.items);
    defer std.testing.allocator.free(commands_before);

    try std.testing.expectError(error.Refused, editor.delete(2)); // a bridge span
    try std.testing.expectEqualStrings("still referred to by bridge 0", editor.status());
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);

    try std.testing.expectError(error.Refused, editor.delete(1)); // a trench piece
    try std.testing.expectEqualStrings("still part of entrenchment 4", editor.status());
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
    try std.testing.expect(!editor.dirty());
    // A refused delete never reaches the start commands either.
    try std.testing.expectEqualSlices(FakeStartCommand, commands_before, fake.start_commands.items);
}

test "a cascade note is replaced by the next successful command" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixture(&.{ 1, 3 }, 0);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.delete(1);
    try std.testing.expect(editor.status().len != 0);
    _ = try editor.addObject("T34", 60, 60, 0, 1);
    try std.testing.expectEqual(@as(usize, 0), editor.status().len);
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

test "camera anchor: setting player 2 pads a one-anchor map to three, undo restores the old size" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var seeded: records.CameraAnchors = .{};
    seeded.player_count = 1;
    seeded.players[0] = .{ .x = 10, .y = 20, .z = 0 };
    fake.setCameraAnchorsFixture(seeded);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setCameraAnchor(2, 100, 120);
    try std.testing.expectEqual(@as(u32, 3), fake.camera_anchors.player_count);
    try std.testing.expect(fake.camera_anchors.players[1].isUnset());
    try std.testing.expectEqual(@as(f32, 100), fake.camera_anchors.players[2].x);
    try std.testing.expectEqual(@as(f32, 10), fake.camera_anchors.players[0].x);
    try std.testing.expect(editor.dirty());
    const generation = editor.record_generations.get(.camera_anchors);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(u32, 1), fake.camera_anchors.player_count);
    try std.testing.expect(fake.camera_anchors.eql(seeded));
    try std.testing.expect(editor.record_generations.get(.camera_anchors) != generation);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(u32, 3), fake.camera_anchors.player_count);
    try std.testing.expectEqual(@as(f32, 120), fake.camera_anchors.players[2].y);
}

test "camera anchor: setting a player inside the vector never shrinks it" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var seeded: records.CameraAnchors = .{};
    seeded.player_count = 4;
    fake.setCameraAnchorsFixture(seeded);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setCameraAnchor(0, 50, 60);
    try std.testing.expectEqual(@as(u32, 4), fake.camera_anchors.player_count);
    try editor.clearCameraAnchor(0);
    try std.testing.expect(fake.camera_anchors.players[0].isUnset());
    try std.testing.expectEqual(@as(u32, 4), fake.camera_anchors.player_count);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(f32, 50), fake.camera_anchors.players[0].x);
}

test "camera anchor: the file's own off-map anchor comes back on undo (WR-B03)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var seeded: records.CameraAnchors = .{};
    seeded.player_count = 2;
    seeded.players[1] = .{ .x = -500, .y = 99999, .z = 0 };
    seeded.neutral = .{ .x = 99999, .y = -1, .z = 0 };
    fake.setCameraAnchorsFixture(seeded);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setCameraAnchor(1, 50, 60);
    try editor.setCameraAnchor(-1, 70, 80);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(f32, -500), fake.camera_anchors.players[1].x);
    try std.testing.expectEqual(@as(f32, 99999), fake.camera_anchors.neutral.x);
    try std.testing.expect(!editor.dirty());
    // A NEW off-map value is still refused.
    try std.testing.expectError(error.Refused, editor.setCameraAnchor(0, -10, -10));
}

test "camera anchor: setting the same value twice records nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setCameraAnchor(1, 70, 80);
    const depth = editor.history.undo_stack.items.len;
    const generation = editor.record_generations.get(.camera_anchors);
    try editor.setCameraAnchor(1, 70, 80);
    try std.testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try std.testing.expectEqual(generation, editor.record_generations.get(.camera_anchors));
}

test "camera anchor: a refused set (off the map) leaves the history and the generation alone" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const depth = editor.history.undo_stack.items.len;
    const generation = editor.record_generations.get(.camera_anchors);
    try std.testing.expectError(error.Refused, editor.setCameraAnchor(0, 9999, 9999));
    try std.testing.expect(editor.status().len != 0);
    try std.testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try std.testing.expectEqual(generation, editor.record_generations.get(.camera_anchors));
    try std.testing.expectEqual(@as(u32, 0), fake.camera_anchors.player_count);
    try std.testing.expect(!editor.dirty());
    // A player the record cannot hold is refused as well, changing nothing.
    try std.testing.expectError(error.Refused, editor.setCameraAnchor(records.max_camera_players, 10, 10));
    try std.testing.expectEqual(depth, editor.history.undo_stack.items.len);
}

test "camera anchor: the neutral slot edits the neutral anchor only" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var seeded: records.CameraAnchors = .{};
    seeded.player_count = 2;
    seeded.players[1] = .{ .x = 9, .y = 9, .z = 0 };
    fake.setCameraAnchorsFixture(seeded);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setCameraAnchor(-1, 30, 40);
    try std.testing.expectEqual(@as(f32, 30), fake.camera_anchors.neutral.x);
    try std.testing.expectEqual(@as(u32, 2), fake.camera_anchors.player_count);
    try std.testing.expectEqual(@as(f32, 9), fake.camera_anchors.players[1].x);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(fake.camera_anchors.eql(seeded));
}

fn expectGroup(fake: *const FakeBridge, id: i32, want: []const i32) !void {
    const got = fake.groupIDs(id) orelse return error.NoSuchGroup;
    try std.testing.expectEqualSlices(i32, want, got);
}

test "group: New takes the first unused ID at or above the field, one undo step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addGroupFixture(0, &.{ 10, 11 });
    try fake.addGroupFixture(1, &.{});
    try fake.addGroupFixture(3, &.{20});
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.record_generations.get(.group);
    try std.testing.expectEqual(@as(i32, 2), try editor.newGroup(0));
    try expectGroup(&fake, 2, &.{});
    try std.testing.expect(editor.record_generations.get(.group) != generation);
    try std.testing.expectEqual(@as(i32, 4), try editor.newGroup(0));
    try std.testing.expectEqual(@as(i32, 10), try editor.newGroup(10));
    // A negative field clamps to 0.
    try std.testing.expectEqual(@as(i32, 5), try editor.newGroup(-5));
    try std.testing.expectEqual(@as(usize, 4), editor.history.undo_stack.items.len);
    // Undo removes the newest, redo puts it back; the ID is free again after the undo.
    try std.testing.expect(try editor.undo());
    try std.testing.expect(fake.groupIDs(5) == null);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(fake.groupIDs(10) == null);
    try std.testing.expect(try editor.redo());
    try expectGroup(&fake, 10, &.{});
    const ids = try editor.groupIDs(std.testing.allocator);
    defer std.testing.allocator.free(ids);
    try std.testing.expectEqualSlices(i32, &.{ 0, 1, 2, 3, 4, 10 }, ids);
}

test "group: script IDs are added (a duplicate is skipped with a note) and removed, each one undo step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addGroupFixture(7, &.{ 100, 200 });
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.addScriptIDToGroup(7, 300);
    try expectGroup(&fake, 7, &.{ 100, 200, 300 });
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    // A duplicate records nothing and says so.
    try editor.addScriptIDToGroup(7, 200);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "already") != null);
    try editor.removeScriptIDFromGroup(7, 100);
    try expectGroup(&fake, 7, &.{ 200, 300 });
    // One that is not there records nothing either.
    try editor.removeScriptIDFromGroup(7, 999);
    try std.testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    const held = try editor.groupScriptIDs(std.testing.allocator, 7);
    defer std.testing.allocator.free(held);
    try std.testing.expectEqualSlices(i32, &.{ 200, 300 }, held);
    // Undo and redo of each, in order.
    try std.testing.expect(try editor.undo());
    try expectGroup(&fake, 7, &.{ 100, 200, 300 });
    try std.testing.expect(try editor.undo());
    try expectGroup(&fake, 7, &.{ 100, 200 });
    try std.testing.expect(!(try editor.undo()));
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expect(try editor.redo());
    try expectGroup(&fake, 7, &.{ 200, 300 });
}

test "group: a refused add leaves the history, the generation and the group alone" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addGroupFixture(7, &.{100});
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.record_generations.get(.group);
    // -1 must never be in a group (GetGroupById(-1) would match every unscripted object), nor may anything past 32000.
    try std.testing.expectError(error.Refused, editor.addScriptIDToGroup(7, -1));
    try std.testing.expectError(error.Refused, editor.addScriptIDToGroup(7, 32001));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "32000") != null);
    // A group that is not there is refused by the bridge.
    try std.testing.expectError(error.Refused, editor.addScriptIDToGroup(8, 5));
    try std.testing.expectError(error.Refused, editor.deleteGroup(8));
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(generation, editor.record_generations.get(.group));
    try expectGroup(&fake, 7, &.{100});
    try std.testing.expect(!editor.dirty());
    // The limits themselves are fine.
    try editor.addScriptIDToGroup(7, 0);
    try editor.addScriptIDToGroup(7, 32000);
    try expectGroup(&fake, 7, &.{ 100, 0, 32000 });
    // An insert over an existing ID is refused by the bridge, and changes nothing.
    const clash: records.Value = .{ .group = .{ .id = 7 } };
    try std.testing.expectError(error.Refused, editor.addRecord(.group, 7, &clash));
    try std.testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    try expectGroup(&fake, 7, &.{ 100, 0, 32000 });
}

test "group: delete takes the group and its IDs out, undo puts both back exactly, redo removes it again" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addGroupFixture(4, &.{ 42, 7, 9 });
    try fake.addGroupFixture(5, &.{});
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.deleteGroup(4);
    try std.testing.expect(fake.groupIDs(4) == null);
    try std.testing.expect(editor.dirty());
    try std.testing.expect(try editor.undo());
    try expectGroup(&fake, 4, &.{ 42, 7, 9 });
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expect(fake.groupIDs(4) == null);
    try expectGroup(&fake, 5, &.{});
    try std.testing.expect(try editor.undo());
    try expectGroup(&fake, 4, &.{ 42, 7, 9 });
}

test "group: a file group holding an odd ID (a duplicate, one out of range) comes back on undo of its delete (WR-B06)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addGroupFixture(4, &.{ 7, 7, 40000 });
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.deleteGroup(4);
    try std.testing.expect(try editor.undo());
    try expectGroup(&fake, 4, &.{ 7, 7, 40000 });
    try std.testing.expect(!editor.dirty());
    // A NEW group is still held to the rules.
    const value: records.Value = .{ .group = .{ .id = 9, .ids = &.{ 40000 } } };
    try std.testing.expectError(error.Refused, editor.addRecord(.group, 9, &value));
}

test "group: New, add and delete all undone leave the map as it opened" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addGroupFixture(0, &.{ 1, 2 });
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const created = try editor.newGroup(0);
    try editor.addScriptIDToGroup(created, 30);
    try editor.addScriptIDToGroup(created, 31);
    try editor.removeScriptIDFromGroup(created, 30);
    try editor.addScriptIDToGroup(0, 3);
    try editor.deleteGroup(0);
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    try expectGroup(&fake, 0, &.{ 1, 2 });
    try std.testing.expect(fake.groupIDs(created) == null);
    try std.testing.expectEqual(@as(u32, 1), fake.groups.count());
}

test "script file: set, undo and redo are one step each, and an equal value records nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.setScriptFileFixture("coldwinter");
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var buffer: [records.script_file_capacity]u8 = undefined;
    try std.testing.expectEqualStrings("coldwinter", try editor.scriptFileName(&buffer));
    const generation = editor.record_generations.get(.script_file);
    try editor.setScriptFile("m2_script");
    try std.testing.expectEqualStrings("m2_script", try editor.scriptFileName(&buffer));
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expect(editor.dirty());
    try std.testing.expect(editor.record_generations.get(.script_file) != generation);
    // The same value again records nothing.
    try editor.setScriptFile("m2_script");
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    // None is a value like any other.
    try editor.setScriptFile("");
    try std.testing.expectEqualStrings("", try editor.scriptFileName(&buffer));
    try std.testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqualStrings("m2_script", fake.script_file.nameSlice());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqualStrings("coldwinter", fake.script_file.nameSlice());
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqualStrings("m2_script", fake.script_file.nameSlice());
}

test "script file: a name with a folder or .lua is Refused and changes nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.setScriptFileFixture("coldwinter");
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.record_generations.get(.script_file);
    for ([_][]const u8{ "..\\x", "a/b", "x.lua", "..", "a b", "x" ** 64 }) |name| {
        try std.testing.expectError(error.Refused, editor.setScriptFile(name));
    }
    try std.testing.expectEqualStrings("coldwinter", fake.script_file.nameSlice());
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(generation, editor.record_generations.get(.script_file));
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(editor.status().len != 0);
}

test "script file: a value read from the file is kept verbatim, and an undo puts it back" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.setScriptFileFixture("..\\odd name.lua");
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var buffer: [records.script_file_capacity]u8 = undefined;
    try std.testing.expectEqualStrings("..\\odd name.lua", try editor.scriptFileName(&buffer));
    try editor.setScriptFile("m2_script");
    // Another odd name is a new value: refused. The file's own goes back.
    try std.testing.expectError(error.Refused, editor.setScriptFile("..\\other.lua"));
    try editor.setScriptFile("..\\odd name.lua");
    try std.testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqualStrings("..\\odd name.lua", fake.script_file.nameSlice());
    try std.testing.expect(!editor.dirty());
}

test "script areas: add appends one step, undo removes, redo puts it back at its index" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var existing: records.ScriptArea = .{ .shape = .circle, .cx = 40, .cy = 40, .r = 8 };
    existing.setName("old");
    try fake.addScriptAreaFixture(existing);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.record_generations.get(.script_area);
    const area = try editor.scriptAreaFromVis(.rectangle, 100, 200, 300, 260, "m2_area");
    // The MFC truncation, once: centre (200, 230) and half size (100, 30) times sqrt 2 plus 0.3, cut.
    try std.testing.expectEqual(@as(f32, 283), area.cx);
    try std.testing.expectEqual(@as(f32, 325), area.cy);
    try std.testing.expectEqual(@as(f32, 141), area.hx);
    try std.testing.expectEqual(@as(f32, 42), area.hy);
    try std.testing.expectEqual(@as(usize, 1), fake.script_areas.items.len); // a conversion adds nothing
    try std.testing.expectEqual(@as(usize, 1), try editor.addScriptArea(area));
    try std.testing.expectEqual(@as(usize, 2), fake.script_areas.items.len);
    try std.testing.expectEqualStrings("m2_area", fake.script_areas.items[1].nameSlice());
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expect(editor.record_generations.get(.script_area) != generation);
    try std.testing.expect(editor.dirty());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqualStrings("m2_area", fake.script_areas.items[1].nameSlice());
    const listed = try editor.scriptAreas(std.testing.allocator);
    defer std.testing.allocator.free(listed);
    try std.testing.expectEqual(@as(usize, 2), listed.len);
    try std.testing.expectEqualStrings("old", listed[0].nameSlice());
}

test "script areas: an empty or taken name is Refused with the history unchanged, and names are case-sensitive" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var existing: records.ScriptArea = .{ .shape = .circle, .cx = 40, .cy = 40, .r = 8 };
    existing.setName("zone");
    try fake.addScriptAreaFixture(existing);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var same = existing;
    try std.testing.expectError(error.Refused, editor.addScriptArea(same));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "zone") != null);
    same.setName("");
    try std.testing.expectError(error.Refused, editor.addScriptArea(same));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "needs a name") != null);
    var off_map: records.ScriptArea = .{ .shape = .circle, .cx = -50, .cy = 10, .r = 8 };
    off_map.setName("off");
    try std.testing.expectError(error.Refused, editor.addScriptArea(off_map));
    var negative: records.ScriptArea = .{ .shape = .circle, .cx = 10, .cy = 10, .r = -8 };
    negative.setName("neg");
    try std.testing.expectError(error.Refused, editor.addScriptArea(negative));
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try std.testing.expect(!editor.dirty());
    // Another case is another name.
    same.setName("ZONE");
    try std.testing.expectEqual(@as(usize, 1), try editor.addScriptArea(same));
    // A rename onto a taken name is refused; onto a free one it is one step.
    try std.testing.expectError(error.Refused, editor.renameScriptArea(1, "zone"));
    try editor.renameScriptArea(1, "zone2");
    try std.testing.expectEqualStrings("zone2", fake.script_areas.items[1].nameSlice());
    try std.testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqualStrings("ZONE", fake.script_areas.items[1].nameSlice());
    try std.testing.expectError(error.Refused, editor.renameScriptArea(1, ""));
    try std.testing.expectError(error.Refused, editor.renameScriptArea(1, "n" ** 64));
}

test "script areas: a move and a resize are one undo step each within a gesture, and back at the start leaves none" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var start: records.ScriptArea = .{ .shape = .rectangle, .cx = 100, .cy = 100, .hx = 30, .hy = 20 };
    start.setName("box");
    try fake.addScriptAreaFixture(start);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    const first = try editor.scriptAreaMoved(start, 80, 90);
    try editor.editScriptArea(0, first, gesture);
    const second = try editor.scriptAreaMoved(first, 90, 95);
    try editor.editScriptArea(0, second, gesture);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(f32, 127), fake.script_areas.items[0].cx); // 90 * sqrt 2 + 0.3, cut
    try std.testing.expectEqual(@as(f32, 30), fake.script_areas.items[0].hx); // the size kept
    try std.testing.expect(try editor.undo());
    try std.testing.expect(fake.script_areas.items[0].eql(start));
    try std.testing.expect(!editor.dirty());
    // A resize: the handle at 40 world units right of the centre and 12 below it.
    const resize_gesture = editor.beginGesture();
    const centre_x = start.cx / 1.4142135;
    const centre_y = start.cy / 1.4142135;
    const bigger = try editor.scriptAreaResized(start, centre_x + 40, centre_y - 12);
    try editor.editScriptArea(0, bigger, resize_gesture);
    try std.testing.expectEqual(@as(f32, 56), fake.script_areas.items[0].hx);
    try std.testing.expectEqual(@as(f32, 17), fake.script_areas.items[0].hy);
    try std.testing.expectEqual(@as(f32, 100), fake.script_areas.items[0].cx);
    // Dragged back to where it began within the gesture: no step at all.
    try editor.editScriptArea(0, start, resize_gesture);
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expect(!editor.dirty());
    // A circle resizes by its radius.
    var ring: records.ScriptArea = .{ .shape = .circle, .cx = 200, .cy = 200, .r = 10 };
    ring.setName("ring");
    _ = try editor.addScriptArea(ring);
    const wider = try editor.scriptAreaResized(ring, ring.cx / 1.4142135 + 30, ring.cy / 1.4142135 + 40);
    try std.testing.expectEqual(@as(f32, 71), wider.r);
}

test "script areas: delete puts the area back at its index on undo, and everything undone is the map as it opened" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var first: records.ScriptArea = .{ .shape = .circle, .cx = 40, .cy = 40, .r = 8 };
    first.setName("a");
    var second: records.ScriptArea = .{ .shape = .rectangle, .cx = 60, .cy = 60, .hx = 5, .hy = 6 };
    second.setName("b");
    try fake.addScriptAreaFixture(first);
    try fake.addScriptAreaFixture(second);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var third: records.ScriptArea = .{ .shape = .circle, .cx = 20, .cy = 20, .r = 3 };
    third.setName("c");
    _ = try editor.addScriptArea(third);
    try editor.deleteScriptArea(0);
    try std.testing.expectEqual(@as(usize, 2), fake.script_areas.items.len);
    try std.testing.expectEqualStrings("b", fake.script_areas.items[0].nameSlice());
    try editor.renameScriptArea(0, "b2");
    try std.testing.expect(try editor.undo());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 3), fake.script_areas.items.len);
    try std.testing.expectEqualStrings("a", fake.script_areas.items[0].nameSlice());
    try std.testing.expectEqualStrings("b", fake.script_areas.items[1].nameSlice());
    try std.testing.expect(try editor.redo());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqualStrings("b2", fake.script_areas.items[0].nameSlice());
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(usize, 2), fake.script_areas.items.len);
    try std.testing.expect(fake.script_areas.items[0].eql(first) and fake.script_areas.items[1].eql(second));
    // A delete past the end changes nothing.
    try std.testing.expectError(error.Failed, editor.deleteScriptArea(5));
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "script areas: a name the file held twice can be put back beside its twin, a third cannot" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var twin: records.ScriptArea = .{ .shape = .circle, .cx = 40, .cy = 40, .r = 8 };
    twin.setName("twin");
    var other_twin = twin;
    other_twin.cx = 90;
    try fake.addScriptAreaFixture(twin);
    try fake.addScriptAreaFixture(other_twin);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.deleteScriptArea(0);
    try std.testing.expect(try editor.undo()); // the put back of the file's own duplicate
    try std.testing.expectEqual(@as(usize, 2), fake.script_areas.items.len);
    var third = twin;
    third.cx = 70;
    try std.testing.expectError(error.Refused, editor.addScriptArea(third));
}

test "script areas: a file's own off-map, negative-size area can be deleted or moved and undone (WR-A04)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var odd: records.ScriptArea = .{ .shape = .rectangle, .cx = -50, .cy = 100000, .hx = -3, .hy = 4 };
    odd.setName("odd");
    try fake.addScriptAreaFixture(odd);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    // A delete, and its undo puts the file's own area back exactly.
    try editor.deleteScriptArea(0);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try std.testing.expect(fake.script_areas.items[0].eql(odd));
    // A move onto the map (the size made sane with it), and its undo sets the odd one back.
    var moved = odd;
    moved.cx = 10;
    moved.cy = 10;
    moved.hx = 3;
    try editor.editScriptArea(0, moved, 0);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(fake.script_areas.items[0].eql(odd));
    try std.testing.expect(!editor.dirty());
    // A NEW area is still held to the rules.
    var new_odd = odd;
    new_odd.setName("new_odd");
    try std.testing.expectError(error.Refused, editor.addScriptArea(new_odd));
}

test "record edits of one gesture are one undo step, and one that returns to its start leaves none" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    var first: records.CameraAnchors = .{};
    first.player_count = 1;
    first.players[0] = .{ .x = 10, .y = 10, .z = 0 };
    var second = first;
    second.players[0].x = 20;
    const first_value: records.Value = .{ .camera_anchors = first };
    const second_value: records.Value = .{ .camera_anchors = second };
    try editor.editRecord(.camera_anchors, 0, &first_value, gesture);
    try editor.editRecord(.camera_anchors, 0, &second_value, gesture);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(f32, 20), fake.camera_anchors.players[0].x);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(u32, 0), fake.camera_anchors.player_count);
    // A gesture that goes out and comes back is no step at all.
    const later = editor.beginGesture();
    const empty_value: records.Value = .{ .camera_anchors = .{} };
    try editor.editRecord(.camera_anchors, 0, &first_value, later);
    try editor.editRecord(.camera_anchors, 0, &empty_value, later);
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(usize, 0), fake.camera_anchors.player_count);
}

test "script ID: set, undo, redo, in the document and the bridge" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectEqual(@as(i32, -1), editor.document.find(1).?.script_id);
    try editor.setScriptID(1, 4242, 0);
    try std.testing.expectEqual(@as(i32, 4242), editor.document.find(1).?.script_id);
    try std.testing.expect(editor.dirty());
    var objects: [3]ObjectRecord = undefined;
    var total: usize = 0;
    try std.testing.expectEqual(bridge_mod.Status.ok, editor.bridge.objects(&objects, &total));
    try std.testing.expectEqual(@as(i32, 4242), objects[0].script_id);
    try std.testing.expectEqual(@as(i32, -1), objects[1].script_id);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(i32, -1), editor.document.find(1).?.script_id);
    try std.testing.expect(!editor.dirty());
    _ = editor.bridge.objects(&objects, &total);
    try std.testing.expectEqual(@as(i32, -1), objects[0].script_id);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(i32, 4242), editor.document.find(1).?.script_id);
    // -1 (none) is a value like any other.
    try editor.setScriptID(1, -1, 0);
    try std.testing.expectEqual(@as(i32, -1), editor.document.find(1).?.script_id);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(try editor.undo());
    try std.testing.expect(!(try editor.undo()));
}

test "script ID: an equal value records nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setScriptID(1, -1, 0);
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "script ID: an object whose type the database does not know is Refused and changes nothing (WR-A05)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const before = editor.document.find(3).?.script_id;
    try std.testing.expectError(error.Refused, editor.setScriptID(3, 5, 0));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "does not know") != null);
    try std.testing.expectEqual(before, editor.document.find(3).?.script_id);
    try std.testing.expect(!editor.dirty());
}

test "script ID: a value out of range or an object that cannot take one is Refused and changes nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectError(error.Refused, editor.setScriptID(1, 32001, 0));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "32000") != null);
    try std.testing.expectError(error.Refused, editor.setScriptID(1, -2, 0));
    try std.testing.expectEqual(@as(i32, -1), editor.document.find(1).?.script_id);
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expect(!editor.dirty());
    // An object the document does not hold is a failure, not a bridge call.
    try std.testing.expectError(error.Failed, editor.setScriptID(99, 5, 0));
    // The limits themselves are fine.
    try editor.setScriptID(1, 32000, 0);
    try editor.setScriptID(1, 0, 0);
    try std.testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
}

test "script ID: the edits of one gesture are one undo step, and one that returns to its start leaves none" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    try editor.setScriptID(1, 5, gesture);
    try editor.setScriptID(1, 50, gesture);
    try editor.setScriptID(1, 500, gesture);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(i32, -1), editor.document.find(1).?.script_id);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(i32, 500), editor.document.find(1).?.script_id);
    try std.testing.expect(try editor.undo());
    // A gesture that lands back on its own before-value is no step at all.
    const second = editor.beginGesture();
    try editor.setScriptID(1, 7, second);
    try editor.setScriptID(1, -1, second);
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    // Another object in the same gesture is its own step.
    try editor.setScriptID(1, 3, second);
    try editor.setScriptID(2, 4, second);
    try std.testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
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

// -- 04-11: start commands (D-17) ---------------------------------------------

test "a start command: add is one step of STOP with no target, undo removes it, redo puts it back" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.record_generations.get(.start_command);
    const index = try editor.addStartCommand(1);
    try std.testing.expectEqual(@as(usize, 0), index);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expect(editor.record_generations.get(.start_command) != generation);
    try std.testing.expectEqual(@as(usize, 1), fake.start_commands.items.len);
    const command = fake.start_commands.items[0];
    try std.testing.expectEqual(records.action_stop, command.cmd_type);
    try std.testing.expectEqual(@as(i32, 0), command.target);
    try std.testing.expect(!command.from_explosion);
    try std.testing.expectEqualSlices(i32, &.{1}, command.units[0..command.unit_count]);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 0), fake.start_commands.items.len);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 1), fake.start_commands.items.len);
    try std.testing.expect(fake.start_commands.items[0].eql(&command));
    // The second is appended after the first.
    try std.testing.expectEqual(@as(usize, 1), try editor.addStartCommand(1));
}

test "the action list comes from the bridge, STOP at the default, and a missing list refuses an add" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const list = try editor.actionCommands(std.testing.allocator);
    defer Editor.freeActionList(std.testing.allocator, list);
    try std.testing.expectEqualStrings("STOP", list.items[list.default_index].nameSlice());
    try std.testing.expectEqual(@as(i32, 9), list.items[list.default_index].id);
    try std.testing.expectEqual(@as(i32, 0), list.byName("MOVE_TO").?.id);
    try std.testing.expect(list.find(9999) == null);
    fake.no_action_list = true;
    try std.testing.expectError(error.Refused, editor.actionCommands(std.testing.allocator));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "actions.ini") != null);
    try std.testing.expectError(error.Refused, editor.addStartCommand(1));
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

test "start command: a type, a number and a target edit in one gesture are one undo step and the flag stays" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixtureFull(.{ .link_id = 0, .from_explosion = true, .units = &.{1} });
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gesture = editor.beginGesture();
    const listed = try editor.startCommands(std.testing.allocator);
    defer Editor.freeStartCommands(std.testing.allocator, listed);
    var command = listed[0];
    command.cmd_type = 0;
    try editor.editStartCommand(0, command, gesture);
    command.number = 4.5;
    command.x = 60;
    command.y = 70;
    command.from_explosion = false; // a set never changes it
    try editor.editStartCommand(0, command, gesture);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    const stored = fake.start_commands.items[0];
    try std.testing.expectEqual(@as(i32, 0), stored.cmd_type);
    try std.testing.expectEqual(@as(f32, 4.5), stored.number);
    try std.testing.expect(stored.from_explosion);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(records.action_stop, fake.start_commands.items[0].cmd_type);
    try std.testing.expectEqual(@as(f32, 0), fake.start_commands.items[0].number);
    try std.testing.expect(fake.start_commands.items[0].from_explosion);
    // An edit that changes nothing records nothing.
    const again = try editor.startCommands(std.testing.allocator);
    defer Editor.freeStartCommands(std.testing.allocator, again);
    var same = again[0];
    const depth = editor.history.undo_stack.items.len;
    try editor.editStartCommand(0, same, 0);
    same.from_explosion = !same.from_explosion;
    try editor.editStartCommand(0, same, 0);
    try std.testing.expectEqual(depth, editor.history.undo_stack.items.len);
}

test "start command refusals leave the map, the history and the generation alone" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixtureFull(.{ .units = &.{1} });
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.record_generations.get(.start_command);
    const commands = try editor.startCommands(std.testing.allocator);
    defer Editor.freeStartCommands(std.testing.allocator, commands);
    var bad = commands[0];
    bad.units = &.{};
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "at least one unit") != null);
    bad.units = &.{0};
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    bad.units = &.{99};
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    bad.units = &.{ 1, 1 };
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    bad = commands[0];
    bad.cmd_type = 4242;
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    bad = commands[0];
    bad.link_id = 99;
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    bad = commands[0];
    bad.x = -3;
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "not on the map") != null);
    try fake.markNonUnitFixture(2);
    bad = commands[0];
    bad.units = &.{2};
    try std.testing.expectError(error.Refused, editor.editStartCommand(0, bad, 0));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "not a unit") != null);
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(generation, editor.record_generations.get(.start_command));
    const unchanged = FakeStartCommand.fromRecord(commands[0]) orelse return error.TooManyUnits;
    try std.testing.expect(fake.start_commands.items[0].eql(&unchanged));
}

test "start command units: Add selected unit skips a duplicate, Remove of the last unit deletes the command in one step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const second = try editor.addObject("T34", 60, 60, 0, 0);
    _ = try editor.addStartCommand(1);
    const depth = editor.history.undo_stack.items.len;
    try editor.addUnitToStartCommand(0, second);
    try std.testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try std.testing.expectEqualSlices(i32, &.{ 1, second }, fake.start_commands.items[0].units[0..fake.start_commands.items[0].unit_count]);
    // A duplicate is a note and no step.
    try editor.addUnitToStartCommand(0, second);
    try std.testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "already in start command 0") != null);
    // Remove one, then the last: the command goes as one step.
    try editor.removeUnitFromStartCommand(0, 1);
    try std.testing.expectEqualSlices(i32, &.{second}, fake.start_commands.items[0].units[0..fake.start_commands.items[0].unit_count]);
    try editor.removeUnitFromStartCommand(0, 1); // not there: a note
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "not in start command 0") != null);
    const before_last = editor.history.undo_stack.items.len;
    try editor.removeUnitFromStartCommand(0, second);
    try std.testing.expectEqual(@as(usize, 0), fake.start_commands.items.len);
    try std.testing.expectEqual(before_last + 1, editor.history.undo_stack.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqualSlices(i32, &.{second}, fake.start_commands.items[0].units[0..fake.start_commands.items[0].unit_count]);
}

test "a unit a group holds back is accepted, and the status warns" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setScriptID(1, 4245, 0);
    const group = try editor.newGroup(0);
    try editor.addScriptIDToGroup(group, 4245);
    _ = try editor.addStartCommand(1);
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "held back by reinforcement group") != null);
    try std.testing.expectEqual(@as(usize, 1), fake.start_commands.items.len);
}

test "deleting and restoring a unit move the start-command generation, both ways" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixture(&.{ 1, 3 }, 0);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var generation = editor.record_generations.get(.start_command);
    try editor.delete(1);
    try std.testing.expect(editor.record_generations.get(.start_command) != generation);
    generation = editor.record_generations.get(.start_command);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(editor.record_generations.get(.start_command) != generation);
    generation = editor.record_generations.get(.start_command);
    try std.testing.expect(try editor.redo());
    try std.testing.expect(editor.record_generations.get(.start_command) != generation);
}

test "a start command deleted and put back by undo keeps its flag, its target and every unit" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addStartCommandFixtureFull(.{ .cmd_type = 0, .link_id = 1, .x = 12, .y = 13, .from_explosion = true, .number = 2, .units = &.{ 1, 3 } });
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const before = fake.start_commands.items[0];
    try editor.deleteStartCommand(0);
    try std.testing.expectEqual(@as(usize, 0), fake.start_commands.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(fake.start_commands.items[0].eql(&before));
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 0), fake.start_commands.items.len);
}

// -- 04-11: reserve positions (D-18) -------------------------------------------

/// A fixture with a towed gun, a truck, a self-propelled gun and a heavy gun on the
/// fake's role table (the fixture's own objects are a T34 and a bridge span).
fn reserveFixture(allocator: std.mem.Allocator) !FakeBridge {
    var fake = try testFixture(allocator);
    errdefer fake.deinit();
    try fake.setRoleFixture("Gun", .towed, 1000);
    try fake.setRoleFixture("Truck", .truck, 2000);
    try fake.setRoleFixture("Weak_Truck", .truck, 500);
    try fake.setRoleFixture("Panzer", .self_propelled, 30000);
    return fake;
}

fn placeNamed(editor: *Editor, name: []const u8, x: f32) !i32 {
    return try editor.addObject(name, x, 100, 0, 0);
}

test "reserve role comes from the bridge's table" {
    var fake = try reserveFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectEqual(bridge_mod.ReserveRole.towed, try editor.reserveRole("Gun"));
    try std.testing.expectEqual(bridge_mod.ReserveRole.truck, try editor.reserveRole("Truck"));
    try std.testing.expectEqual(bridge_mod.ReserveRole.self_propelled, try editor.reserveRole("Panzer"));
    try std.testing.expectEqual(bridge_mod.ReserveRole.none, try editor.reserveRole("T34"));
    try std.testing.expectEqual(bridge_mod.ReserveRole.none, try editor.reserveRole("No_Such"));
}

test "a reserve position: a towed gun with its truck, one undo step each, appended, exact undo and redo" {
    var fake = try reserveFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gun = try placeNamed(&editor, "Gun", 60);
    const truck = try placeNamed(&editor, "Truck", 90);
    const panzer = try placeNamed(&editor, "Panzer", 120);
    const depth = editor.history.undo_stack.items.len;
    const generation = editor.record_generations.get(.reserve_position);
    try std.testing.expectEqual(@as(usize, 0), try editor.addReservePosition(.{ .artillery = gun, .truck = truck, .x = 50, .y = 60 }));
    try std.testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try std.testing.expect(editor.record_generations.get(.reserve_position) != generation);
    // A self-propelled gun needs no truck and is appended after the first.
    try std.testing.expectEqual(@as(usize, 1), try editor.addReservePosition(.{ .artillery = panzer, .x = 70, .y = 80 }));
    const listed = try editor.reservePositions(std.testing.allocator);
    defer std.testing.allocator.free(listed);
    try std.testing.expectEqual(@as(usize, 2), listed.len);
    try std.testing.expectEqual(gun, listed[0].artillery);
    try std.testing.expectEqual(truck, listed[0].truck);
    try std.testing.expectEqual(panzer, listed[1].artillery);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 0), fake.reserve_positions.items.len);
    try std.testing.expect(try editor.redo());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 2), fake.reserve_positions.items.len);
}

test "reserve position refusals change nothing: the roles, the towing check, squads, non-units and the map edge" {
    var fake = try reserveFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const gun = try placeNamed(&editor, "Gun", 60);
    const truck = try placeNamed(&editor, "Truck", 90);
    const weak = try placeNamed(&editor, "Weak_Truck", 100);
    const panzer = try placeNamed(&editor, "Panzer", 120);
    const squad = try placeNamed(&editor, "Gun", 130);
    try fake.markSquadFixture(squad);
    const depth = editor.history.undo_stack.items.len;
    const generation = editor.record_generations.get(.reserve_position);
    // A towed gun without a truck.
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = gun, .x = 50, .y = 60 }));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "towed gun needs a truck") != null);
    // A self-propelled gun with a truck.
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = panzer, .truck = truck, .x = 50, .y = 60 }));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "takes no truck") != null);
    // A truck that cannot tow the gun (500 against a weight of 1000).
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = gun, .truck = weak, .x = 50, .y = 60 }));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "cannot tow") != null);
    // A squad in either role, a non-unit (the T34 has role 0), a truck as the gun, a gun as the truck.
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = squad, .truck = truck, .x = 50, .y = 60 }));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "squad") != null);
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = gun, .truck = squad, .x = 50, .y = 60 }));
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = 1, .x = 50, .y = 60 }));
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = truck, .x = 50, .y = 60 }));
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = gun, .truck = gun, .x = 50, .y = 60 }));
    // Link ID 0 as the gun, both 0, a missing object, off the map.
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = 0, .truck = truck, .x = 50, .y = 60 }));
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = 0, .truck = 0, .x = 50, .y = 60 }));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "needs a gun") != null);
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = 9999, .x = 50, .y = 60 }));
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = panzer, .x = -5, .y = 60 }));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "not on the map") != null);
    try std.testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try std.testing.expectEqual(generation, editor.record_generations.get(.reserve_position));
    try std.testing.expectEqual(@as(usize, 0), fake.reserve_positions.items.len);
}

test "reserve position: a move of the place is one undo step per gesture, and delete puts it back at its index" {
    var fake = try reserveFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const panzer = try placeNamed(&editor, "Panzer", 120);
    const other = try placeNamed(&editor, "Panzer", 150);
    _ = try editor.addReservePosition(.{ .artillery = panzer, .x = 70, .y = 80 });
    _ = try editor.addReservePosition(.{ .artillery = other, .x = 10, .y = 20 });
    const gesture = editor.beginGesture();
    try editor.editReservePosition(0, .{ .artillery = panzer, .x = 75, .y = 80 }, gesture);
    try editor.editReservePosition(0, .{ .artillery = panzer, .x = 90, .y = 85 }, gesture);
    try std.testing.expectEqual(@as(f32, 90), fake.reserve_positions.items[0].x);
    const depth = editor.history.undo_stack.items.len;
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(f32, 70), fake.reserve_positions.items[0].x);
    try std.testing.expectEqual(depth - 1, editor.history.undo_stack.items.len);
    try editor.deleteReservePosition(0);
    try std.testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    try std.testing.expectEqual(other, fake.reserve_positions.items[0].artillery);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(panzer, fake.reserve_positions.items[0].artillery);
    try std.testing.expectEqual(other, fake.reserve_positions.items[1].artillery);
}

test "a file's own odd reserve position is accepted back by an undo, a new one like it is not" {
    var fake = try reserveFixture(std.testing.allocator);
    defer fake.deinit();
    try fake.addReservePositionFixtureFull(.{ .artillery = 777, .truck = 0, .x = 5, .y = 5 });
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.deleteReservePosition(0);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(i32, 777), fake.reserve_positions.items[0].artillery);
    try std.testing.expectError(error.Refused, editor.addReservePosition(.{ .artillery = 777, .truck = 0, .x = 6, .y = 5 }));
    // Its place may move though its gun is not one the map has (the gun is unchanged).
    try editor.editReservePosition(0, .{ .artillery = 777, .truck = 0, .x = 9, .y = 9 }, 0);
    try std.testing.expectEqual(@as(f32, 9), fake.reserve_positions.items[0].x);
}

test "deleting a gun erases its reserve position and the undo brings it back, both generations moving" {
    var fake = try reserveFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const panzer = try placeNamed(&editor, "Panzer", 120);
    _ = try editor.addReservePosition(.{ .artillery = panzer, .x = 70, .y = 80 });
    var generation = editor.record_generations.get(.reserve_position);
    try editor.delete(panzer);
    try std.testing.expectEqual(@as(usize, 0), fake.reserve_positions.items.len);
    try std.testing.expect(editor.record_generations.get(.reserve_position) != generation);
    generation = editor.record_generations.get(.reserve_position);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    try std.testing.expect(editor.record_generations.get(.reserve_position) != generation);
    try std.testing.expectEqual(@as(f32, 70), fake.reserve_positions.items[0].x);
}

test "an AI parcel: addDefenceParcel is one step, creates the sides below, and undo restores the count exactly" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1;
    try fake.addAiSideFixture(.{});
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const depth = editor.history.undo_stack.items.len;
    const generation = editor.record_generations.get(.ai_side);
    try std.testing.expectEqual(@as(usize, 0), try editor.addDefenceParcel(2, 100.4, 200.8));
    try std.testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try std.testing.expect(editor.record_generations.get(.ai_side) != generation);
    try std.testing.expectEqual(@as(usize, 3), try editor.aiSideCount());
    try std.testing.expectEqual(@as(usize, 0), fake.ai_sides.items[1].parcels.len); // created empty
    const parcel = fake.ai_sides.items[2].parcels[0];
    try std.testing.expectEqual(@as(f32, 100), parcel.cx); // 100.4 + 0.3 cut
    try std.testing.expectEqual(@as(f32, 201), parcel.cy); // 200.8 + 0.3 cut
    try std.testing.expectEqual(records.parcel_min_radius, parcel.radius);
    // A second parcel on the same side is appended, a step of its own.
    try std.testing.expectEqual(@as(usize, 1), try editor.addDefenceParcel(2, 220, 230));
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 1), fake.ai_sides.items[2].parcels.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 1), fake.ai_sides.items.len); // the count the map had
    try std.testing.expect(try editor.redo());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 2), fake.ai_sides.items[2].parcels.len);
    // A side the record cannot hold is refused before the bridge.
    try std.testing.expectError(error.Failed, editor.addDefenceParcel(records.max_ai_sides, 10, 10));
}

test "an AI side read past the count is empty with the count, and an off-map parcel is Refused unchanged" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1;
    try fake.addAiSideFixture(.{});
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var side = try editor.aiSide(std.testing.allocator, 5);
    defer side.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(u32, 1), side.side_count);
    try std.testing.expectEqual(@as(i32, 5), side.side);
    try std.testing.expectEqual(@as(usize, 0), side.parcels.len);
    const depth = editor.history.undo_stack.items.len;
    try std.testing.expectError(error.Refused, editor.addDefenceParcel(0, -10, 50));
    try std.testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(usize, 0), fake.ai_sides.items[0].parcels.len);
}

test "mobile script IDs: add and remove are one step each, a duplicate is a note, the range is 0..32000" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1;
    try fake.addAiSideFixture(.{});
    try fake.addAiSideFixture(.{});
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const depth = editor.history.undo_stack.items.len;
    try editor.addMobileScriptID(1, 4245);
    try std.testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try std.testing.expectEqualSlices(i32, &.{4245}, fake.ai_sides.items[1].mobile_ids);
    // A duplicate is a note and records nothing.
    try editor.addMobileScriptID(1, 4245);
    try std.testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "already") != null);
    // Out of range is refused with a note, and changes nothing.
    try std.testing.expectError(error.Refused, editor.addMobileScriptID(1, 32001));
    try std.testing.expectError(error.Refused, editor.addMobileScriptID(1, -1));
    try editor.addMobileScriptID(1, 0);
    try editor.addMobileScriptID(1, 32000);
    try std.testing.expectEqualSlices(i32, &.{ 4245, 0, 32000 }, fake.ai_sides.items[1].mobile_ids);
    try editor.removeMobileScriptID(1, 0);
    try std.testing.expectEqualSlices(i32, &.{ 4245, 32000 }, fake.ai_sides.items[1].mobile_ids);
    try editor.removeMobileScriptID(1, 77); // not there: a note, no step
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "not a mobile") != null);
    try std.testing.expectEqual(depth + 4, editor.history.undo_stack.items.len);
    // Undo walks back exactly, the other side untouched.
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqualSlices(i32, &.{ 4245, 0, 32000 }, fake.ai_sides.items[1].mobile_ids);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(try editor.undo());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 0), fake.ai_sides.items[1].mobile_ids.len);
    try std.testing.expectEqual(@as(usize, 0), fake.ai_sides.items[0].mobile_ids.len);
    try std.testing.expectEqual(@as(usize, 2), fake.ai_sides.items.len);
    // On a side the map lacks, the first ID creates it (and the ones below), and undo takes them away.
    try editor.addMobileScriptID(4, 9);
    try std.testing.expectEqual(@as(usize, 5), fake.ai_sides.items.len);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 2), fake.ai_sides.items.len);
}

test "altitude region edits: set, merge within a gesture, undo and redo (M3 D-19)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try fake.setAltitudeFixture(4, 4, 100);
    const region: bridge_mod.AltitudeRegion = .{ .x0 = 2, .y0 = 2, .x1 = 6, .y1 = 6 };
    // (4,4) sits at row 2, column 2 of the region.
    try std.testing.expectEqual(@as(f32, 100), fake.altitude(4, 4));
    const generation_at_open = editor.altitudes_generation;
    var ramp: [16]f32 = undefined;
    for (&ramp, 0..) |*height, i| height.* = @floatFromInt(i);
    const gesture = editor.beginGesture();
    try editor.setAltitudes(region, &ramp, gesture);
    try std.testing.expectEqual(@as(f32, 10), fake.altitude(4, 4));
    try std.testing.expectEqual(@as(f32, 5), fake.altitude(3, 3));
    try std.testing.expectEqual(generation_at_open + 1, editor.altitudes_generation);
    // A second call in the same gesture merges into the one undo step.
    try editor.setAltitudes(region, &ramp, gesture);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(generation_at_open + 2, editor.altitudes_generation);
    try std.testing.expect(editor.dirty());
    // The read is two-pass: the total is answered even to a short buffer.
    var short: [4]f32 = undefined;
    var read_back: [16]f32 = undefined;
    var total: usize = 0;
    try std.testing.expectEqual(bridge_mod.Status.refused, editor.bridge.altitudes(region, &short, &total));
    try std.testing.expectEqual(@as(usize, 16), total);
    try std.testing.expectEqual(bridge_mod.Status.ok, editor.bridge.altitudes(region, &read_back, &total));
    // (4,4) sits at row 2, column 2 of the region: index 2*4+2.
    try std.testing.expectEqual(@as(f32, 10), read_back[2 * 4 + 2]);
    // Undo puts the recorded region back raw, redo reapplies it.
    _ = try editor.undo();
    try std.testing.expectEqual(@as(f32, 100), fake.altitude(4, 4));
    try std.testing.expectEqual(@as(f32, 0), fake.altitude(3, 3));
    try std.testing.expectEqual(generation_at_open + 3, editor.altitudes_generation);
    _ = try editor.redo();
    try std.testing.expectEqual(@as(f32, 10), fake.altitude(4, 4));
    while (try editor.undo()) {}
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(f32, 100), fake.altitude(4, 4));
}

test "altitude refusals change nothing: off-map, count mismatch, empty region, non-finite" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try fake.setAltitudeFixture(4, 4, 100);
    const region: bridge_mod.AltitudeRegion = .{ .x0 = 2, .y0 = 2, .x1 = 6, .y1 = 6 };
    var ramp: [16]f32 = undefined;
    for (&ramp, 0..) |*height, i| height.* = @floatFromInt(i);
    const good: [16]f32 = ramp;
    // A region off the vertex sheet is Refused (an ordinary no); the count
    // still has to match it, exactly as the C ABI orders its checks.
    try std.testing.expectError(error.Refused, editor.setAltitudes(.{ .x0 = 8, .y0 = 8, .x1 = 12, .y1 = 12 }, &good, 0));
    // A count mismatch, an empty region and a non-finite height are caller
    // bugs: Failed, and nothing moves.
    try std.testing.expectError(error.Failed, editor.setAltitudes(region, good[0..15], 0));
    try std.testing.expectError(error.Failed, editor.setAltitudes(.{ .x0 = 2, .y0 = 2, .x1 = 2, .y1 = 6 }, &good, 0));
    var nan_heights: [16]f32 = ramp;
    nan_heights[7] = std.math.nan(f32);
    try std.testing.expectError(error.Failed, editor.setAltitudes(region, &nan_heights, 0));
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(f32, 100), fake.altitude(4, 4));
    // The generation has not moved either: the open bumped it once, and no
    // refusal has touched it since.
    try std.testing.expectEqual(@as(u32, 1), editor.altitudes_generation);
    // The read refuses the same off-map region.
    var heights: [16]f32 = undefined;
    var total: usize = 0;
    try std.testing.expectEqual(bridge_mod.Status.refused, editor.bridge.altitudes(.{ .x0 = 8, .y0 = 8, .x1 = 12, .y1 = 12 }, &heights, &total));
}

test "new map: the engine builds it, the document opens never-saved (M3 D-23)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try editor.paint(&.{.{ .x = 0, .y = 0, .tile = 14 }}, 0);
    try std.testing.expect(editor.dirty());
    var params: bridge_mod.NewMapParams = .{};
    params.size_x = 8;
    params.size_y = 4;
    params.season = 3; // Spring: the map stores Summer's own real value
    params.setName("m3_new");
    try editor.newMap(params);
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(@as(usize, 0), editor.document.path.items.len);
    try std.testing.expectEqual(@as(i32, 8 * 16), editor.document.info.width_tiles);
    try std.testing.expectEqual(@as(i32, 4 * 16), editor.document.info.height_tiles);
    try std.testing.expectEqual(@as(i32, 0), editor.document.info.season);
    try std.testing.expectEqual(@as(usize, 0), editor.document.objects.items.len);
    try std.testing.expectEqualSlices(i32, &.{ 2, 2 }, editor.document.diplomacy.items);
    // The tiles are the season's most common terrain type; the altitudes a
    // zero sheet one bigger than the tiles per axis.
    try std.testing.expectEqual(fake_mod.most_common_tiles[3], fake.tile(0, 0));
    try std.testing.expectEqual(fake_mod.most_common_tiles[3], fake.tile(8 * 16 - 1, 4 * 16 - 1));
    var read: [4]f32 = undefined;
    var total: usize = 0;
    const sheet: bridge_mod.AltitudeRegion = .{ .x0 = 0, .y0 = 0, .x1 = 4, .y1 = 1 };
    try std.testing.expectEqual(bridge_mod.Status.ok, editor.bridge.altitudes(sheet, &read, &total));
    try std.testing.expectEqual(@as(usize, 4), total);
    try std.testing.expectEqualSlices(f32, &.{ 0, 0, 0, 0 }, &read);
    // The history of the map before is gone with it.
    try std.testing.expect(!editor.history.canUndo());
    try std.testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    // Every generation moved (the open bumped them once, the new map again;
    // the paint in between does not touch the altitudes').
    try std.testing.expectEqual(@as(u32, 2), editor.altitudes_generation);
}

test "new map refusals: sizes and season are caller bugs, nothing changes" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    const width_at_open = editor.document.info.width_tiles;
    var bad: bridge_mod.NewMapParams = .{};
    bad.size_x = 0;
    try std.testing.expectError(error.Failed, editor.newMap(bad));
    bad = .{};
    bad.size_y = 33;
    try std.testing.expectError(error.Failed, editor.newMap(bad));
    bad = .{};
    bad.season = 4;
    try std.testing.expectError(error.Failed, editor.newMap(bad));
    // The map that was open is exactly as it was.
    try std.testing.expectEqual(width_at_open, editor.document.info.width_tiles);
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
}

test "filters: load merges through the bridge, the composer edits session-wide, save writes the user ones" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();

    // The fake's two shipped fixtures answer as the merged list.
    try editor.loadFilters();
    try std.testing.expectEqual(@as(u32, 1), editor.filters_generation);
    try std.testing.expectEqual(@as(usize, 2), editor.filtersSlice().len);
    try std.testing.expectEqualStrings("Buildings", editor.filtersSlice()[0].nameSlice());
    try std.testing.expectEqual(@as(c_int, 0), editor.filtersSlice()[0].user);

    // New: an empty user filter. Invalid and taken names are refused, and a
    // refusal leaves the list and its generation alone.
    try std.testing.expectError(error.Refused, editor.filterNew("a|b"));
    try std.testing.expectError(error.Refused, editor.filterNew("Buildings"));
    const generation = editor.filters_generation;
    try std.testing.expectEqual(@as(usize, 2), editor.filtersSlice().len);
    try editor.filterNew("Mine Fields");
    try std.testing.expectEqual(generation + 1, editor.filters_generation);
    try std.testing.expectEqual(@as(usize, 3), editor.filtersSlice().len);
    try std.testing.expectEqual(@as(c_int, 1), editor.filtersSlice()[2].user);
    try std.testing.expectEqual(@as(c_int, 0), editor.filtersSlice()[2].list_count);

    // Rename: the filter becomes user-owned under its new name.
    try std.testing.expectError(error.Refused, editor.filterRename("No Such", "Other"));
    try std.testing.expectError(error.Refused, editor.filterRename("Mine Fields", "Squads"));
    try editor.filterRename("Mine Fields", "Flora");
    try std.testing.expectEqualStrings("Flora", editor.filtersSlice()[2].nameSlice());
    try std.testing.expectEqual(@as(c_int, 1), editor.filtersSlice()[2].user);

    // Put: the composer's word-list edit replaces whole and marks user.
    var edited = editor.filtersSlice()[0];
    edited.lists[0].word_count = 2;
    @memcpy(edited.lists[0].words[1][0.."terrain".len], "terrain");
    try editor.filterPut(edited);
    try std.testing.expectEqual(@as(c_int, 1), editor.filtersSlice()[0].user);
    try std.testing.expectEqual(@as(c_int, 2), editor.filtersSlice()[0].lists[0].word_count);
    var off: bridge_mod.ObjectFilter = .{};
    off.setName("Off");
    off.list_count = 9;
    try std.testing.expectError(error.Refused, editor.filterPut(off));

    // Save writes only the user ones; the fake answers the written set on
    // the next read.
    try editor.saveFilters();
    try std.testing.expectEqual(@as(u32, 5), editor.filters_generation);
    var reloaded: []bridge_mod.ObjectFilter = &.{};
    try bridge_mod.check(fake.bridge().objectFilters(std.testing.allocator, &reloaded));
    defer std.testing.allocator.free(reloaded);
    try std.testing.expectEqual(@as(usize, 3), reloaded.len);

    // Delete: the filter leaves the list (its user-file override with it).
    try std.testing.expectError(error.Refused, editor.filterDelete("No Such"));
    try editor.filterDelete("Flora");
    try std.testing.expectEqual(@as(usize, 2), editor.filtersSlice().len);
    try std.testing.expectEqual(@as(u32, 6), editor.filters_generation);
}


test "fields: the apply's objects leave the document on undo and come back on redo" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    fake.field_adds_object = true;
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const before = editor.document.objects.items.len;
    const points = [_]bridge_mod.FieldVec3{ .{ .x = 64, .y = 64 }, .{ .x = 192, .y = 64 }, .{ .x = 192, .y = 192 } };
    try editor.applyField(.{ .point_count = points.len, .points = &points }, null, std.testing.allocator);
    try std.testing.expectEqual(before + 1, editor.document.objects.items.len);
    // The composite's undo takes the fill's object out of the document too
    // (it is altitudes-scoped, and used to leave the document stale).
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(before, editor.document.objects.items.len);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(before + 1, editor.document.objects.items.len);
}

fn playersFixture(fake: *FakeBridge) void {
    // Three players and the neutral: 0 and 1 are the sides, the last entry the neutral's.
    fake.info.player_count = 4;
}

test "players: add puts a player before the neutral and undoes and redoes as one step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    playersFixture(&fake);
    fake.objects_list.items[0].player = 3; // an object of the neutral
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectEqual(@as(usize, 4), editor.document.diplomacy.items.len);
    const generation = editor.players_generation;
    try editor.addPlayer(1);
    try std.testing.expectEqual(@as(usize, 5), editor.document.diplomacy.items.len);
    try std.testing.expectEqual(@as(i32, 5), editor.document.info.player_count);
    try std.testing.expectEqual(@as(i32, 1), editor.document.diplomacy.items[3]); // the new player's side
    try std.testing.expectEqual(@as(i32, 1), editor.document.diplomacy.items[4]); // the old neutral's, moved up
    // The neutral's object stays the neutral's: its index moved up with it.
    try std.testing.expectEqual(@as(i32, 4), editor.document.objects.items[0].player);
    try std.testing.expect(editor.players_generation != generation);
    try std.testing.expect(editor.dirty());
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 4), editor.document.diplomacy.items.len);
    try std.testing.expectEqual(@as(i32, 3), editor.document.objects.items[0].player);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 5), editor.document.diplomacy.items.len);
    try std.testing.expectEqual(@as(i32, 4), editor.document.objects.items[0].player);
}

test "players: delete re-owns the player's objects to the neutral, moves the others down, and undoes as one step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    playersFixture(&fake);
    fake.objects_list.items[0].player = 0;
    fake.objects_list.items[1].player = 1;
    fake.objects_list.items[2].player = 2;
    try fake.addUnitCreationFixture(records.UnitCreation.defaults());
    var second = records.UnitCreation.defaults();
    second.relax_time = 41;
    try fake.addUnitCreationFixture(second);
    var third = records.UnitCreation.defaults();
    third.relax_time = 52;
    try fake.addUnitCreationFixture(third);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.deletePlayer(1);
    try std.testing.expectEqual(@as(usize, 3), editor.document.diplomacy.items.len);
    try std.testing.expectEqual(@as(i32, 0), editor.document.objects.items[0].player); // below: unchanged
    try std.testing.expectEqual(@as(i32, 2), editor.document.objects.items[1].player); // the deleted player's: the neutral (index 2 now)
    try std.testing.expectEqual(@as(i32, 1), editor.document.objects.items[2].player); // above: moved down
    // The unit creation of the player above followed it, and the deleted one's slot is gone.
    try std.testing.expectEqual(@as(usize, 2), fake.unit_creation_list.items.len);
    try std.testing.expectEqual(@as(i32, 52), fake.unit_creation_list.items[1].relax_time);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 4), editor.document.diplomacy.items.len);
    try std.testing.expectEqual(@as(i32, 1), editor.document.objects.items[1].player);
    try std.testing.expectEqual(@as(i32, 2), editor.document.objects.items[2].player);
    try std.testing.expectEqual(@as(usize, 3), fake.unit_creation_list.items.len);
    try std.testing.expectEqual(@as(i32, 41), fake.unit_creation_list.items[1].relax_time);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(i32, 2), editor.document.objects.items[1].player);
    try std.testing.expectEqual(@as(usize, 2), fake.unit_creation_list.items.len);
}

test "players: the neutral, a bad player, a bad side and the 17th entry are refused and change nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    playersFixture(&fake);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try std.testing.expectError(error.Refused, editor.deletePlayer(3)); // the neutral
    try std.testing.expectEqualStrings("the neutral player cannot be deleted", editor.status());
    try std.testing.expectError(error.Refused, editor.deletePlayer(9));
    try std.testing.expectError(error.Refused, editor.deletePlayer(-1));
    try std.testing.expectError(error.Refused, editor.addPlayer(2));
    try std.testing.expect(!editor.history.canUndo());
    // Up to 16 players and the neutral, then a refusal.
    while (editor.document.diplomacy.items.len < max_player_entries) try editor.addPlayer(0);
    try std.testing.expectEqual(@as(usize, 17), editor.document.diplomacy.items.len);
    try std.testing.expectError(error.Refused, editor.addPlayer(0));
    try std.testing.expectEqualStrings("a map holds 16 players and the neutral", editor.status());
    try std.testing.expectEqual(@as(usize, 17), editor.document.diplomacy.items.len);
    // A table at its floor keeps its players.
    var small = try testFixture(std.testing.allocator);
    defer small.deinit();
    var small_editor = try openFixture(&small); // two entries: a player and the neutral
    defer small_editor.deinit();
    try std.testing.expectError(error.Refused, small_editor.deletePlayer(0));
    try std.testing.expectEqualStrings("a map keeps at least two players and the neutral", small_editor.status());
}

const max_player_entries = fake_mod.max_player_entries;

test "unit creation: an edit is one undo step, a put for a player past the vector grows it and its undo shrinks it back" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    playersFixture(&fake);
    try fake.addUnitCreationFixture(records.UnitCreation.defaults());
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var held = try editor.unitCreation(0);
    try std.testing.expectEqual(@as(u32, 1), held.slot_count);
    held.relax_time = 77;
    held.aircraft[2].count = 6;
    try std.testing.expect(held.addAppear(.{ .x = 100, .y = 200 }));
    try editor.editUnitCreation(0, held, 0);
    try std.testing.expectEqual(@as(i32, 77), fake.unit_creation_list.items[0].relax_time);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(i32, 30), fake.unit_creation_list.items[0].relax_time);
    try std.testing.expectEqual(@as(u32, 0), fake.unit_creation_list.items[0].appear_count);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(u32, 1), fake.unit_creation_list.items[0].appear_count);
    try std.testing.expect(try editor.undo());

    // Player 2 is not in the vector: it reads the defaults, a put grows the vector.
    var beyond = try editor.unitCreation(2);
    try std.testing.expectEqual(@as(u32, 1), beyond.slot_count);
    try std.testing.expectEqualStrings("USSR", beyond.partySlice());
    beyond.relax_time = 99;
    try editor.editUnitCreation(2, beyond, 0);
    try std.testing.expectEqual(@as(usize, 3), fake.unit_creation_list.items.len);
    try std.testing.expectEqual(@as(i32, 99), fake.unit_creation_list.items[2].relax_time);
    try std.testing.expectEqual(@as(i32, 30), fake.unit_creation_list.items[1].relax_time); // the padding is the defaults
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(usize, 1), fake.unit_creation_list.items.len); // the vector's old size, exactly
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 3), fake.unit_creation_list.items.len);
}

test "unit creation: a gesture merges, and an unchanged record records nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    playersFixture(&fake);
    try fake.addUnitCreationFixture(records.UnitCreation.defaults());
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var unit = try editor.unitCreation(0);
    try editor.editUnitCreation(0, unit, 0);
    try std.testing.expect(!editor.history.canUndo());
    unit.relax_time = 31;
    try editor.editUnitCreation(0, unit, 7);
    unit.relax_time = 32;
    try editor.editUnitCreation(0, unit, 7);
    unit.relax_time = 33;
    try editor.editUnitCreation(0, unit, 7);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expectEqual(@as(i32, 33), fake.unit_creation_list.items[0].relax_time);
    try std.testing.expect(try editor.undo());
    try std.testing.expectEqual(@as(i32, 30), fake.unit_creation_list.items[0].relax_time);
}

test "unit creation: the MutableValidate rules refuse by field and change nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    playersFixture(&fake);
    try fake.addUnitCreationFixture(records.UnitCreation.defaults());
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const base = try editor.unitCreation(0);
    var bad = base;
    bad.setParty("Narnia");
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    try std.testing.expectEqualStrings("the party \"Narnia\" is not in partys.xml", editor.status());
    bad = base;
    bad.aircraft[1].setName("Spitfire");
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    try std.testing.expectEqualStrings("Fighters: \"Spitfire\" is no aircraft of the object database", editor.status());
    bad = base;
    bad.aircraft[3].formation_size = 0;
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    try std.testing.expectEqualStrings("Bombers: formation size 0 is outside 1..32", editor.status());
    bad = base;
    bad.aircraft[4].count = 300;
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    bad = base;
    bad.setParatroop("Ghosts");
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    bad = base;
    bad.paratroop_count = -1;
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    bad = base;
    bad.relax_time = 0;
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    try std.testing.expectEqualStrings("the relax time 0 is below 1 second", editor.status());
    bad = base;
    _ = bad.addAppear(.{ .x = -5, .y = 10 });
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    bad = base;
    _ = bad.addAppear(.{ .x = 1.0e6, .y = 10 });
    try std.testing.expectError(error.Refused, editor.editUnitCreation(0, bad, 0));
    // A player the map does not have.
    bad = base;
    bad.relax_time = 40;
    try std.testing.expectError(error.Refused, editor.editUnitCreation(5, bad, 0));
    try std.testing.expectError(error.Failed, editor.editUnitCreation(records.max_uc_slots, bad, 0));
    // Nothing changed: not the bridge, not the history.
    try std.testing.expect(!editor.history.canUndo());
    try std.testing.expect(base.eql(try editor.unitCreation(0)));
    // A good edit of the same kinds goes through.
    var good = base;
    good.setParty("Germany");
    good.aircraft[1].setName("Ju-87");
    good.setParatroop("German_rpd_43");
    try editor.editUnitCreation(0, good, 0);
    try std.testing.expectEqualStrings("Germany", fake.unit_creation_list.items[0].partySlice());
}

test "unit creation: choices list the parties, aircraft and squads" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var names: [8]bridge_mod.UcName = undefined;
    var total: usize = 0;
    try bridge_mod.check(fake.bridge().unitCreationChoices(.parties, &names, &total));
    try std.testing.expectEqual(@as(usize, 3), total);
    try std.testing.expectEqualStrings("USSR", names[0].nameSlice());
    try bridge_mod.check(fake.bridge().unitCreationChoices(.aircraft, &names, &total));
    try std.testing.expectEqual(@as(usize, 5), total);
    try bridge_mod.check(fake.bridge().unitCreationChoices(.squads, &names, &total));
    try std.testing.expectEqual(@as(usize, 2), total);
    var none: [0]bridge_mod.UcName = .{};
    try std.testing.expectEqual(bridge_mod.Status.refused, fake.bridge().unitCreationChoices(.parties, &none, &total)); // the sizing pass
    try std.testing.expectEqual(@as(usize, 3), total);
}

test "players: add and delete shift the camera anchors with their players" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    playersFixture(&fake);
    var anchors: records.CameraAnchors = .{ .player_count = 3 };
    anchors.players[0] = .{ .x = 10, .y = 10, .z = 1 };
    anchors.players[1] = .{ .x = 20, .y = 20, .z = 1 };
    anchors.players[2] = .{ .x = 30, .y = 30, .z = 1 };
    fake.setCameraAnchorsFixture(anchors);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.record_generations.get(.camera_anchors);
    try editor.addPlayer(0);
    try std.testing.expectEqual(@as(u32, 4), fake.camera_anchors.player_count);
    try std.testing.expect(fake.camera_anchors.players[3].isUnset());
    try std.testing.expect(editor.record_generations.get(.camera_anchors) != generation);
    try editor.deletePlayer(0);
    try std.testing.expectEqual(@as(u32, 3), fake.camera_anchors.player_count);
    try std.testing.expectEqual(@as(f32, 20), fake.camera_anchors.players[0].x);
    try std.testing.expect(try editor.undo());
    try std.testing.expect(try editor.undo());
    try std.testing.expect(fake.camera_anchors.eql(anchors));
}

fn checkFixture(fake: *FakeBridge) !void {
    playersFixture(fake); // 4 entries, the neutral is 3
    var duplicate: ObjectRecord = .{ .link_id = 10, .x = 40, .y = 40, .dir = 0, .player = 0 };
    duplicate.setName("T34"); // the fixture's tank (link 1) again
    try fake.addFixture(duplicate, false);
    var stray: ObjectRecord = .{ .link_id = 11, .x = 300, .y = 300, .dir = 0, .player = 9 };
    stray.setName("Pak40"); // an owner the table does not have
    try fake.addFixture(stray, false);
    var passenger: ObjectRecord = .{ .link_id = 12, .x = 320, .y = 320, .dir = 0, .player = 0, .link_with = 99 };
    passenger.setName("US_rifle"); // a host that is not there
    try fake.addFixture(passenger, false);
    var party: records.UnitCreation = records.UnitCreation.defaults();
    party.setParty("Narnia");
    try fake.addUnitCreationFixture(party);
    // A road with one control point: the record that crashed the game's loader.
    var short: fake_mod.FakeVso = .{ .count = 1 };
    short.controls[0] = .{ .x = 50, .y = 60 };
    try fake.vso_lists[0].append(fake.allocator, short);
}

test "check map finds every kind, and fix all fixes them as ONE undo step" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    try checkFixture(&fake);
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const findings = try editor.checkMap(std.testing.allocator, &.{});
    defer std.testing.allocator.free(findings);
    try std.testing.expectEqual(@as(usize, 1), checks.count(findings, .duplicate_object));
    try std.testing.expectEqual(@as(usize, 1), checks.count(findings, .invalid_link));
    try std.testing.expectEqual(@as(usize, 1), checks.count(findings, .player_index));
    try std.testing.expectEqual(@as(usize, 1), checks.count(findings, .unknown_party));
    try std.testing.expectEqual(@as(usize, 1), checks.count(findings, .unknown_object_type));
    try std.testing.expectEqual(@as(usize, 1), checks.count(findings, .short_vso));
    try std.testing.expectEqual(@as(usize, 6), findings.len);
    // A check is a read: nothing changed, nothing to undo.
    try std.testing.expect(!editor.history.canUndo());
    try std.testing.expect(!editor.dirty());

    const objects_before = editor.document.objects.items.len;
    // Without the say-so the destructive fixes (an unknown object, the short road) wait.
    const report = try editor.fixAll(findings, false);
    try std.testing.expectEqual(@as(usize, 4), report.fixed);
    try std.testing.expectEqual(@as(usize, 2), report.left);
    try std.testing.expectEqual(@as(usize, 0), report.refused);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len); // ONE step
    try std.testing.expectEqual(objects_before - 1, editor.document.objects.items.len); // the duplicate went
    try std.testing.expect(editor.document.find(10) == null);
    try std.testing.expectEqual(@as(i32, 3), editor.document.find(11).?.player); // the neutral
    try std.testing.expectEqual(@as(i32, 0), editor.document.find(12).?.link_with);
    try std.testing.expectEqualStrings("USSR", fake.unit_creation_list.items[0].partySlice());
    {
        const again = try editor.checkMap(std.testing.allocator, &.{});
        defer std.testing.allocator.free(again);
        try std.testing.expectEqual(@as(usize, 2), again.len); // only the two that were left
    }

    // One undo takes every one of the four back; one redo does them again.
    try std.testing.expect(try editor.undo());
    try std.testing.expect(!editor.history.canUndo());
    try std.testing.expect(!editor.dirty());
    try std.testing.expectEqual(objects_before, editor.document.objects.items.len);
    try std.testing.expectEqual(@as(i32, 9), editor.document.find(11).?.player);
    try std.testing.expectEqual(@as(i32, 99), editor.document.find(12).?.link_with);
    try std.testing.expectEqualStrings("Narnia", fake.unit_creation_list.items[0].partySlice());
    try std.testing.expectEqual(objects_before, editor.document.objects.items.len);
    try std.testing.expect(editor.document.find(10) != null);
    try std.testing.expect(try editor.redo());
    try std.testing.expect(editor.document.find(10) == null);
    try std.testing.expectEqual(@as(i32, 3), editor.document.find(11).?.player);
    try std.testing.expect(try editor.undo());

    // Confirmed, the unknown object and the short road go too - still one step, and
    // after it nothing is left to find.
    const all = try editor.fixAll(findings, true);
    try std.testing.expectEqual(@as(usize, 6), all.fixed);
    try std.testing.expectEqual(@as(usize, 0), all.left);
    try std.testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try std.testing.expect(editor.document.find(3) == null); // the unknown type
    try std.testing.expectEqual(@as(usize, 0), try editor.vsoCount(.road));
    {
        const none = try editor.checkMap(std.testing.allocator, &.{});
        defer std.testing.allocator.free(none);
        try std.testing.expectEqual(@as(usize, 0), none.len);
    }
    try std.testing.expect(try editor.undo());
    try std.testing.expect(editor.document.find(3) != null);
    try std.testing.expectEqual(@as(usize, 1), try editor.vsoCount(.road));
    try std.testing.expect(!editor.history.canUndo());
    try std.testing.expectEqual(objects_before, editor.document.objects.items.len);
    try std.testing.expect(try editor.redo());
    try std.testing.expectEqual(@as(usize, 0), try editor.vsoCount(.road));
}

test "fix all with nothing to fix records nothing; a refused fix is counted and the rest go on" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const none = try editor.fixAll(&.{}, true);
    try std.testing.expectEqual(@as(usize, 0), none.fixed + none.left + none.refused);
    try std.testing.expect(!editor.history.canUndo());
    // A duplicate finding whose object is a bridge span the bridge keeps: refused, counted.
    var findings = [_]checks.Finding{
        .{ .kind = .duplicate_object, .link_id = 2 }, // the span: referred to by a bridge
        .{ .kind = .duplicate_link, .link_id = 1 }, // report only
    };
    const report = try editor.fixAll(&findings, false);
    try std.testing.expectEqual(@as(usize, 0), report.fixed);
    try std.testing.expectEqual(@as(usize, 1), report.refused);
    try std.testing.expectEqual(@as(usize, 1), report.left);
    try std.testing.expect(!editor.history.canUndo());
}

test "checks.zig is under the core's own tests" {
    _ = checks;
}

test "the map revision moves with every edit, undo, redo and open, and with nothing else" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    const opened = editor.mapRevision();
    // A read, a status and a dirty question are no change.
    _ = editor.dirty();
    _ = editor.status();
    try std.testing.expectEqual(opened, editor.mapRevision());
    try editor.setMapType(3);
    const edited = editor.mapRevision();
    try std.testing.expect(edited != opened);
    try std.testing.expect(try editor.undo());
    const undone = editor.mapRevision();
    try std.testing.expect(undone != edited);
    try std.testing.expect(try editor.redo());
    try std.testing.expect(editor.mapRevision() != undone);
    const redone = editor.mapRevision();
    try editor.open("fixture.bzm");
    try std.testing.expect(editor.mapRevision() != redone);
}

test "the minimap's reads are two-pass and answer the fake's map (05-07)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    const bridge = editor.bridge;
    // No map open: the reads refuse.
    var total: usize = 99;
    var none: [0]u8 = .{};
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.tiles(.{ .x0 = 0, .y0 = 0, .x1 = 2, .y1 = 2 }, &none, &total));
    try editor.open("fixture.bzm");

    // The tiles: the sizing pass answers the area, the read is the fake's own tiles.
    const region: bridge_mod.TileRegion = .{ .x0 = 1, .y0 = 2, .x1 = 4, .y1 = 4 };
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.tiles(region, &none, &total));
    try std.testing.expectEqual(@as(usize, 6), total);
    var tile_buffer: [6]u8 = undefined;
    try std.testing.expectEqual(bridge_mod.Status.ok, bridge.tiles(region, &tile_buffer, &total));
    try std.testing.expectEqual(fake.tile(1, 2), tile_buffer[0]);
    try std.testing.expectEqual(fake.tile(3, 3), tile_buffer[5]);
    var short: [2]u8 = undefined;
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.tiles(region, &short, &total));
    try std.testing.expectEqual(@as(usize, 6), total);
    try std.testing.expectEqual(bridge_mod.Status.bad_argument, bridge.tiles(.{ .x0 = 3, .y0 = 0, .x1 = 3, .y1 = 2 }, &none, &total));
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.tiles(.{ .x0 = 0, .y0 = 0, .x1 = 9, .y1 = 2 }, &none, &total));

    // The colours: one per tile index, distinct.
    var no_colors: [0]u32 = .{};
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.minimapTileColors(&no_colors, &total));
    try std.testing.expectEqual(FakeBridge.minimap_tile_count, total);
    var colors: [FakeBridge.minimap_tile_count]u32 = undefined;
    try std.testing.expectEqual(bridge_mod.Status.ok, bridge.minimapTileColors(&colors, &total));
    try std.testing.expectEqual(FakeBridge.minimapColorOf(5), colors[5]);
    try std.testing.expect(colors[1] != colors[2]);

    // The markers: one per known object, five AI tiles square, the player's colour.
    var no_units: [0]bridge_mod.MinimapUnit = .{};
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.minimapUnits(&no_units, &total));
    try std.testing.expect(total >= 1);
    var units: [8]bridge_mod.MinimapUnit = undefined;
    const want = total;
    try std.testing.expectEqual(bridge_mod.Status.ok, bridge.minimapUnits(&units, &total));
    try std.testing.expectEqual(want, total);
    for (units[0..total]) |unit| {
        try std.testing.expect(unit.x1 > unit.x0 and unit.y1 > unit.y0);
        try std.testing.expect(unit.color_index >= 0 and unit.color_index <= 16);
    }

    // The areas: none until the fake's AI shows some.
    var no_areas: [0]bridge_mod.MinimapArea = .{};
    try std.testing.expectEqual(bridge_mod.Status.ok, bridge.minimapAreas(&no_areas, &total));
    try std.testing.expectEqual(@as(usize, 0), total);
    try fake.minimap_areas.append(std.testing.allocator, .{ .kind = 0, .cx = 100, .cy = 200, .radius = 300, .min_radius = 0, .start_angle = 65535, .finish_angle = 65535, .rgb = 0x88ff88 });
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.minimapAreas(&no_areas, &total));
    try std.testing.expectEqual(@as(usize, 1), total);
    var areas: [1]bridge_mod.MinimapArea = undefined;
    try std.testing.expectEqual(bridge_mod.Status.ok, bridge.minimapAreas(&areas, &total));
    try std.testing.expectEqual(@as(f32, 300), areas[0].radius);

    // Create Minimap Images: a full path of a map goes, a relative one and a
    // non-map are not.
    try std.testing.expectEqual(bridge_mod.Status.refused, bridge.createMinimapImages("Data\\Maps\\a.bzm"));
    try std.testing.expectEqual(bridge_mod.Status.bad_argument, bridge.createMinimapImages("/maps/a.txt"));
    try std.testing.expectEqual(bridge_mod.Status.ok, bridge.createMinimapImages("/maps/a.bzm"));
    try std.testing.expectEqual(@as(u32, 1), fake.images_created);
}

// ---------------------------------------------------------------------------
// The Layers menu (M3, D-32, 05-06): renderer state, remembered and re-applied.
// ---------------------------------------------------------------------------

fn layerBits(fake: *FakeBridge) u32 {
    var bits: u32 = 0;
    var mask: u32 = 0;
    _ = fake.bridge().layers(&bits, &mask);
    return bits;
}

test "layers: a toggle reaches the renderer, reads back, and is no map edit" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const generation = editor.layers_generation;
    try std.testing.expect(!editor.layers.shown(.grid));
    try editor.toggleLayer(.grid);
    try std.testing.expect(editor.layers.shown(.grid));
    try std.testing.expect(layerBits(&fake) & layers_mod.bit(.grid) != 0);
    try std.testing.expect(editor.layers_generation != generation);
    try editor.setLayer(.terrain, false);
    try std.testing.expect(!editor.layers.shown(.terrain));
    try std.testing.expect(layerBits(&fake) & layers_mod.bit(.terrain) == 0);
    try std.testing.expect(layerBits(&fake) & layers_mod.bit(.haze) != 0);
    // A renderer state is no document edit: nothing to undo, nothing dirty.
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(!editor.history.canUndo());
    // The same request twice is fine and stays put.
    try editor.setLayer(.grid, true);
    try std.testing.expect(editor.layers.shown(.grid));
}

test "layers: a layer the renderer cannot draw and the fire-range toggle are refused, changing nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    // The open read the mask: the fixture renderer cannot draw the depth complexity.
    try std.testing.expect(!editor.layerAvailable(.depth_complexity));
    try std.testing.expect(editor.layerAvailable(.grid));
    const bits = layerBits(&fake);
    const state_bits = editor.layers.bits;
    try std.testing.expectError(error.Refused, editor.toggleLayer(.depth_complexity));
    try std.testing.expect(!editor.layers.shown(.depth_complexity));
    try std.testing.expectEqual(state_bits, editor.layers.bits);
    try std.testing.expectEqual(bits, layerBits(&fake));
    try std.testing.expectEqualStrings("layers: this renderer cannot draw that layer", editor.status());
    try std.testing.expectError(error.Refused, editor.setLayer(.fire_ranges, true));
    try std.testing.expect(!editor.layers.shown(.fire_ranges));
    // The fake refuses what it is asked past the mask too (the bridge's own rule).
    try std.testing.expectEqual(bridge_mod.Status.refused, fake.bridge().setLayerShow(@intFromEnum(layers_mod.Layer.depth_complexity), true));
    try std.testing.expectEqual(bridge_mod.Status.bad_argument, fake.bridge().setLayerShow(99, true));
    try std.testing.expectEqual(bridge_mod.Status.bad_argument, fake.bridge().setLayerShow(@intFromEnum(layers_mod.Layer.fire_ranges), true));
}

test "layers: with no map open a toggle is refused and the remembered state stays" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try std.testing.expectError(error.Refused, editor.setLayer(.grid, true));
    try std.testing.expect(!editor.layers.shown(.grid));
    try std.testing.expectEqualStrings("no map is open", editor.status());
}

test "layers: the state is re-applied after every open and every new map (the MFC desync)" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.setLayer(.grid, true);
    try editor.setLayer(.terrain_noise, false);
    try editor.setLayer(.war_fog, true);
    try editor.setLayer(.units_passability, true);
    const chosen = layerBits(&fake);
    // The fake renderer comes up at its defaults on an open (what the engine's
    // own memory does to the MFC editor's menu): the editor puts its state back.
    try editor.open("fixture.bzm");
    try std.testing.expectEqual(chosen, layerBits(&fake));
    try std.testing.expect(layerBits(&fake) & layers_mod.bit(.grid) != 0);
    try std.testing.expect(layerBits(&fake) & layers_mod.bit(.terrain_noise) == 0);
    try std.testing.expect(layerBits(&fake) & layers_mod.bit(.war_fog) != 0);
    try editor.newMap(.{ .size_x = 2, .size_y = 2, .season = 0 });
    try std.testing.expectEqual(chosen, layerBits(&fake));
    try editor.open("fixture.bzm");
    try std.testing.expectEqual(chosen, layerBits(&fake));
    // The re-apply is quiet: an open's status is not wiped by it, and a failed
    // open does not touch the remembered state.
    try std.testing.expectError(error.Failed, editor.open("missing.bzm"));
    try std.testing.expect(editor.layers.shown(.grid));
}

test "layers: a state loaded before any map (settings) is what the first open applies" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    var remembered: layers_mod.State = .{};
    remembered.set(.bounding_boxes, true);
    remembered.set(.shadows, false);
    remembered.set(.depth_complexity, true); // the renderer cannot draw it: skipped, not an error
    editor.layers = remembered;
    try editor.open("fixture.bzm");
    const bits = layerBits(&fake);
    try std.testing.expect(bits & layers_mod.bit(.bounding_boxes) != 0);
    try std.testing.expect(bits & layers_mod.bit(.shadows) == 0);
    try std.testing.expect(bits & layers_mod.bit(.depth_complexity) == 0);
    // What was remembered for the layer it cannot draw is kept for a renderer that can.
    try std.testing.expect(editor.layers.shown(.depth_complexity));
}

test "layers: fire ranges - selected follows the selection, a filter names a known filter, off clears" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.loadFilters();
    const first = try editor.addObject("T34", 60, 60, 0, 1);
    const second = try editor.addObject("T34", 90, 90, 0, 1);
    // Selected: the bridge is told the selection (one unit).
    editor.selectOnly(first);
    try editor.setFireRange(.selected, "");
    try std.testing.expect(editor.layers.shown(.fire_ranges));
    try std.testing.expectEqual(@as(u32, 1), fake.fire_mode);
    try std.testing.expectEqualSlices(i32, &.{first}, fake.fire_link_ids.items);
    try std.testing.expect(layerBits(&fake) & layers_mod.bit(.fire_ranges) != 0);
    // The selection moves: the per-frame sync tells the bridge again, once.
    editor.selectionReplace(&.{ first, second });
    const calls_before = fake.calls.items.len;
    editor.syncFireRange();
    try std.testing.expectEqualSlices(i32, &.{ first, second }, fake.fire_link_ids.items);
    try std.testing.expectEqual(calls_before + 1, fake.calls.items.len);
    editor.syncFireRange();
    editor.syncFireRange();
    try std.testing.expectEqual(calls_before + 1, fake.calls.items.len);
    // An edit moves the key too (a deleted selected unit must leave the group).
    _ = try editor.addObject("T34", 120, 120, 0, 1);
    const calls_after_add = fake.calls.items.len;
    editor.syncFireRange();
    try std.testing.expectEqual(calls_after_add + 1, fake.calls.items.len);
    try std.testing.expect(fake.calls.items[calls_after_add].kind == .fire_range);
    // A filter: a known name is sent and remembered, an unknown one refused with the old mode kept.
    try editor.setFireRange(.filter, "Buildings");
    try std.testing.expectEqual(@as(u32, 2), fake.fire_mode);
    try std.testing.expectEqualStrings("Buildings", editor.layers.fireFilter());
    try std.testing.expectError(error.Refused, editor.setFireRange(.filter, "No Such Filter"));
    try std.testing.expectEqual(layers_mod.FireMode.filter, editor.layers.fire_mode);
    try std.testing.expectEqualStrings("Buildings", editor.layers.fireFilter());
    try std.testing.expectEqual(@as(u32, 2), fake.fire_mode);
    try std.testing.expectEqualStrings("fire range: no object filter is named that", editor.status());
    // Off hides every range and forgets the filter.
    try editor.setFireRange(.off, "");
    try std.testing.expect(!editor.layers.shown(.fire_ranges));
    try std.testing.expectEqual(@as(u32, 0), fake.fire_mode);
    try std.testing.expectEqualStrings("", editor.layers.fireFilter());
    // With the mode off the sync sends nothing.
    const calls_off = fake.calls.items.len;
    editor.syncFireRange();
    try std.testing.expectEqual(calls_off, fake.calls.items.len);
}

test "layers: the fire-range mode is asked again after an open - the AI forgot its groups with the map" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    try editor.loadFilters();
    try editor.setFireRange(.filter, "Buildings");
    try std.testing.expectEqual(@as(u32, 2), fake.fire_mode);
    try editor.open("fixture.bzm");
    try std.testing.expectEqual(@as(u32, 2), fake.fire_mode);
    try std.testing.expectEqualStrings("Buildings", editor.layers.fireFilter());
    try std.testing.expectEqualStrings("Buildings", fake.fire_filter_buffer[0..fake.fire_filter_len]);
    // Selected mode: the open cleared the selection, so the group is empty - and
    // the mode survives for the next selection.
    try editor.setFireRange(.selected, "");
    try editor.open("fixture.bzm");
    try std.testing.expectEqual(@as(u32, 1), fake.fire_mode);
    try std.testing.expectEqual(@as(usize, 0), fake.fire_link_ids.items.len);
}


const GenerateProgress = struct {
    steps: i32 = 0,
    total: i32 = 0,

    fn report(step: c_int, total: c_int, user: ?*anyopaque) callconv(.c) void {
        const self: *GenerateProgress = @ptrCast(@alignCast(user.?));
        self.steps = step;
        self.total = total;
    }
};

fn generateParams() bridge_mod.RmgGenerateParams {
    var params: bridge_mod.RmgGenerateParams = .{};
    params.setTemplate("scenarios\\templates\\summer\\small");
    params.setContext("scenarios\\chapters\\allies\\france\\context");
    params.setSetting(bridge_mod.rmg_any_setting);
    params.setMapName("rmg_test");
    return params;
}

test "create random map: a generation reports 19 steps, names its seed and graph and leaves the document alone" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    const objects = editor.document.objects.items.len;
    const undo_depth = editor.history.undo_stack.items.len;
    var progress = GenerateProgress{};
    var params = generateParams();
    params.has_seed = 1;
    params.seed = 4242;
    params.graph = 2;
    params.angle = 1;
    params.progress = GenerateProgress.report;
    params.user = &progress;
    var result: bridge_mod.RmgGenerateResult = .{};
    try editor.createRandomMap(params, &result);
    try std.testing.expectEqual(@as(i32, 19), progress.steps);
    try std.testing.expectEqual(@as(i32, 19), progress.total);
    try std.testing.expectEqual(@as(c_uint, 4242), result.seed);
    try std.testing.expectEqual(@as(c_int, 2), result.graph);
    try std.testing.expectEqual(@as(c_int, 1), result.angle);
    try std.testing.expect(std.mem.endsWith(u8, result.mapPathSlice(), "rmg_test.bzm"));
    // Not an edit of the open map: nothing for the history, the document as it was.
    try std.testing.expectEqual(undo_depth, editor.history.undo_stack.items.len);
    try std.testing.expectEqual(objects, editor.document.objects.items.len);
    // A blank seed draws one, and the result names it.
    params.has_seed = 0;
    params.overwrite = 1;
    try editor.createRandomMap(params, &result);
    try std.testing.expectEqual(fake.drawn_seed, result.seed);
}

test "create random map: every refusal names its field and the bridge writes nothing" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = try openFixture(&fake);
    defer editor.deinit();
    var result: bridge_mod.RmgGenerateResult = .{};

    var params = generateParams();
    params.setTemplate("scenarios\\templates\\summer\\nope");
    try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "template") != null);

    params = generateParams();
    params.setContext("nope");
    try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "context") != null);

    params = generateParams();
    params.setSetting("nope");
    try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "setting") != null);

    params = generateParams();
    params.level = 3;
    try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "level") != null);

    params = generateParams();
    params.graph = fake.template_graph_count;
    try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "graph") != null);

    params = generateParams();
    params.angle = 4;
    try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "angle") != null);

    for ([_][]const u8{ "", "..", "a/b", "a\\b", "c:x" }) |bad| {
        params = generateParams();
        params.setMapName(bad);
        try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
        try std.testing.expect(std.mem.indexOf(u8, editor.status(), "map name") != null);
    }
    try std.testing.expectEqual(@as(usize, 0), fake.generated_names.items.len);

    // A repeat of a name is refused until overwrite is set.
    params = generateParams();
    try editor.createRandomMap(params, &result);
    try std.testing.expectError(error.Refused, editor.createRandomMap(params, &result));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "already exists") != null);
    params.overwrite = 1;
    try editor.createRandomMap(params, &result);
    try std.testing.expectEqual(@as(usize, 1), fake.generated_names.items.len);
}

test "composer containers read through two passes, write under a user name and keep shipped ones read-only" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    const name = "scenarios\\containers\\summer\\road_a";
    var container = try editor.readContainer(name);
    defer container.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 2), container.patchCount());
    try std.testing.expectEqualStrings("scenarios\\patches\\summer\\road_a2", container.patches.items[1].name);
    try std.testing.expectEqual(@as(i32, 2), container.size_x);
    try std.testing.expectEqualSlices(i32, &.{ 0, 1 }, container.indices[2].items);
    try std.testing.expectEqualSlices(i32, &.{3}, container.script_ids.items);
    try std.testing.expectEqualStrings("Ambush", container.script_areas.items[0]);
    // The shipped name is read-only: Save As.
    try std.testing.expectError(error.Refused, editor.writeContainer(name, &container));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "Save As") != null);
    // A user name takes it, and reads back equal.
    const user_name = "scenarios\\containers\\user\\mine";
    try editor.writeContainer(user_name, &container);
    var back = try editor.readContainer(user_name);
    defer back.deinit(std.testing.allocator);
    try std.testing.expect(container.eql(&back));
    // An edited container writes over the user's own file.
    try back.setDirection(std.testing.allocator, 0, .north, false);
    try editor.writeContainer(user_name, &back);
    var again = try editor.readContainer(user_name);
    defer again.deinit(std.testing.allocator);
    try std.testing.expect(!again.hasDirection(0, .north));
    // An unknown or not-plain name is refused with the reason.
    try std.testing.expectError(error.Refused, editor.readContainer("scenarios\\containers\\nope"));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "container") != null);
    try std.testing.expectError(error.Refused, editor.readContainer(""));
    try std.testing.expectError(error.Failed, editor.writeContainer("..\\x", &container));
}

test "composer graphs round-trip through the bridge" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    var graph = try editor.readGraph("scenarios\\graphs\\summer\\graph_a");
    defer graph.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 2), graph.nodes.items.len);
    try std.testing.expectEqual(@as(usize, 1), graph.links.items.len);
    try std.testing.expectEqualStrings("scenarios\\containers\\summer\\road_a", graph.nodes.items[0].container);
    try std.testing.expectEqualStrings("terrain\\sets\\1\\roads3d\\road_grunt", graph.links.items[0].desc);
    try std.testing.expectError(error.Refused, editor.writeGraph("scenarios\\graphs\\summer\\graph_a", &graph));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "Save As") != null);
    _ = try graph.addNode(std.testing.allocator, .{ .x1 = 96, .y1 = 0, .x2 = 128, .y2 = 32 });
    _ = try graph.addLink(std.testing.allocator, 1, 2);
    try editor.writeGraph("scenarios\\graphs\\user\\mine", &graph);
    var back = try editor.readGraph("scenarios\\graphs\\user\\mine");
    defer back.deinit(std.testing.allocator);
    try std.testing.expect(graph.eql(&back));
    try std.testing.expectEqual(@as(usize, 3), back.nodes.items.len);
    // A link to a node that is not there is refused by the bridge too.
    back.links.items[0].a = 7;
    try std.testing.expectError(error.Refused, editor.writeGraph("scenarios\\graphs\\user\\bad", &back));
}

test "Check! and the add rules ask the editor's own source: patches and containers as the storages hold them" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    var container = try editor.readContainer("scenarios\\containers\\summer\\road_a");
    defer container.deinit(std.testing.allocator);
    var report = try rmg_mod.checkContainer(std.testing.allocator, &container, editor.rmgSource());
    defer report.deinit(std.testing.allocator);
    // road_a2 is two patches wide in the map and the fixture container lists it
    // as 2x2; the one wrong thing the fixture holds is the stale size of road_a1? No:
    // every size agrees, so the shipped fixture checks clean.
    try std.testing.expectEqual(@as(usize, 0), report.errorCount());
    // A winter patch does not belong in a summer container.
    var outcome = try rmg_mod.addPatchChecked(std.testing.allocator, &container, editor.rmgSource(), "scenarios\\patches\\winter\\snow_a1");
    defer outcome.deinit(std.testing.allocator);
    try std.testing.expect(outcome == .mismatch);
    try std.testing.expect(std.mem.indexOf(u8, outcome.mismatch, "Winter") != null);
    var missing = try rmg_mod.addPatchChecked(std.testing.allocator, &container, editor.rmgSource(), "scenarios\\patches\\summer\\nope");
    defer missing.deinit(std.testing.allocator);
    try std.testing.expect(missing == .unreadable);
    // A different script-ID set is refused too (road_b1 uses 9, the container 3).
    var ids = try rmg_mod.addPatchChecked(std.testing.allocator, &container, editor.rmgSource(), "scenarios\\patches\\summer\\road_b1");
    defer ids.deinit(std.testing.allocator);
    try std.testing.expect(ids == .mismatch and std.mem.indexOf(u8, ids.mismatch, "ScriptIDs") != null);
    // The graph's check goes through the container source.
    var graph = try editor.readGraph("scenarios\\graphs\\summer\\graph_a");
    defer graph.deinit(std.testing.allocator);
    var graph_report = try rmg_mod.checkGraph(std.testing.allocator, &graph, editor.rmgSource());
    defer graph_report.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 0), graph_report.errorCount());
    // Node 1 is empty: a warning, not an error.
    try std.testing.expect(graph_report.findings.items.len >= 1);
    // The status line was never touched by those lookups.
    try std.testing.expectEqual(@as(usize, 0), editor.status().len);
}

test "a patch outside the storages is copied in, not refused (D-10); a bad one is refused naming why" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    var named: bridge_mod.RmgName = .{};
    try editor.importPatch("/somewhere/else/My Patch.bzm", false, &named);
    try std.testing.expectEqualStrings("scenarios\\patches\\summer\\my patch", named.nameSlice());
    try std.testing.expectEqual(@as(usize, 0), fake.rmg_imports);
    try editor.importPatch("/somewhere/else/My Patch.bzm", true, &named);
    try std.testing.expectEqual(@as(usize, 1), fake.rmg_imports);
    // The copy is a patch the container can now list.
    var container: rmg_mod.Container = .{};
    defer container.deinit(std.testing.allocator);
    var outcome = try rmg_mod.addPatchChecked(std.testing.allocator, &container, editor.rmgSource(), named.nameSlice());
    defer outcome.deinit(std.testing.allocator);
    try std.testing.expect(outcome == .added);
    try std.testing.expectError(error.Refused, editor.importPatch("relative/patch.bzm", false, &named));
    try std.testing.expect(std.mem.indexOf(u8, editor.status(), "full path") != null);
    try std.testing.expectError(error.Refused, editor.importPatch("/x/notamap.bzm", true, &named));
    try std.testing.expectError(error.Refused, editor.importPatch("/x/readme.txt", false, &named));
    var root: [128]u8 = undefined;
    try std.testing.expectEqualStrings("/fake/user/rmg", try editor.rmgRoot(&root));
}
