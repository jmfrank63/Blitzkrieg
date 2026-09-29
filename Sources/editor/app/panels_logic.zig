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

/// SMapSoundInfo's timeRepeat/timeRepeatRandom are milliseconds
/// (NTimer::STime); the Sounds panel shows them in seconds.
pub fn msToSeconds(ms: i32) f32 {
    return @as(f32, @floatFromInt(ms)) / 1000.0;
}

/// The other direction: rounded to the nearest millisecond. A non-finite
/// input (an empty or mid-edit field ImGui left at NaN) is 0 rather than an
/// undefined cast - BkEditorAddSound/SetSound would refuse a negative one
/// anyway, but this must never be a trap.
pub fn secondsToMs(seconds: f32) i32 {
    if (!std.math.isFinite(seconds)) return 0;
    const rounded: f64 = @round(@as(f64, seconds) * 1000.0);
    const clamped = std.math.clamp(rounded, @as(f64, @floatFromInt(std.math.minInt(i32))), @as(f64, @floatFromInt(std.math.maxInt(i32))));
    return @intFromFloat(clamped);
}

/// Sorts `names` case-insensitively in place, for the Sounds panel's "known
/// sounds" combo (catalogue entries of game type 100) - the list is built
/// once, from the catalogue's own name buffers, so sorting a caller-owned
/// slice of borrowed strings (rather than returning a new one) is enough.
pub fn sortNamesIgnoreCase(names: [][]const u8) void {
    std.sort.insertion([]const u8, names, {}, struct {
        fn lessThan(_: void, a: []const u8, b: []const u8) bool {
            return std.ascii.lessThanIgnoreCase(a, b);
        }
    }.lessThan);
}

/// A message for the Sounds panel's own fields, shown next to them so an
/// impossible radius pair is visible before the bridge ever sees it (which
/// would otherwise only report it after the fact, in the status bar).
/// BkEditorAddSound/SetSound refuse the same thing themselves.
pub fn soundRadiusError(min_radius: i32, max_radius: i32) ?[]const u8 {
    if (min_radius > max_radius) return "minimum radius is above maximum";
    return null;
}

/// The object pictures cache's ordering and dedup rules (D-29), pure enough
/// to run under this file's own tests with no GPU or bridge: `request`
/// queues a name once, `take` serves up to a per-frame budget in request
/// order, and a name reported `markMissing` (no shipped icon.tga, or one
/// that would not fit) is never queued again until `clear` (a mod switch,
/// D-26, drops every name so the new mod's objects get a fresh try).
/// `pictures.Pictures` owns one of these for its pending/missing side; the
/// ready textures themselves live in `Pictures.entries`, which needs a GPU
/// and so cannot be tested here.
pub const PictureQueue = struct {
    allocator: std.mem.Allocator,
    /// Names decoded to nothing so far - kept apart from `Pictures.entries`
    /// (which only ever holds a texture that decoded) so this struct never
    /// touches the GPU.
    missing: std.StringHashMapUnmanaged(void) = .empty,
    /// Names waiting for `take`, in request order.
    queue: std.ArrayListUnmanaged([]u8) = .empty,
    queued: std.StringHashMapUnmanaged(void) = .empty,

    pub fn init(allocator: std.mem.Allocator) PictureQueue {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *PictureQueue) void {
        self.clear();
        self.missing.deinit(self.allocator);
        self.queue.deinit(self.allocator);
        self.queued.deinit(self.allocator);
        self.* = undefined;
    }

    /// Forgets every queued and missing name - a mod switch (D-26): the new
    /// mod's objects may reuse a name with a different icon, or none at all.
    pub fn clear(self: *PictureQueue) void {
        var it = self.missing.keyIterator();
        while (it.next()) |key| self.allocator.free(key.*);
        self.missing.clearAndFree(self.allocator);
        for (self.queue.items) |name| self.allocator.free(name);
        self.queue.clearAndFree(self.allocator);
        self.queued.clearAndFree(self.allocator);
    }

    pub fn isMissing(self: *const PictureQueue, name: []const u8) bool {
        return self.missing.contains(name);
    }

    pub fn pendingCount(self: *const PictureQueue) usize {
        return self.queue.items.len;
    }

    /// Queues `name` once: already queued, or already marked missing, is a
    /// no-op, so a group redrawn every frame does not grow the queue every
    /// frame and a name with no picture is never retried. The caller (the
    /// cache holding the ready textures) must also check its own resolved
    /// names before calling - this queue only knows about names still
    /// pending or already missing.
    pub fn request(self: *PictureQueue, name: []const u8) void {
        if (self.missing.contains(name)) return;
        if (self.queued.contains(name)) return;
        const owned = self.allocator.dupe(u8, name) catch return;
        self.queued.put(self.allocator, owned, {}) catch {
            self.allocator.free(owned);
            return;
        };
        self.queue.append(self.allocator, owned) catch {
            _ = self.queued.remove(owned);
            self.allocator.free(owned);
        };
    }

    /// Removes and returns up to `buffer.len` names from the front of the
    /// queue, in request order - `buffer.len` is the frame's decode budget.
    /// Ownership of each returned name passes to the caller, who reports
    /// back with `markMissing` on a failed decode and frees the name either
    /// way; a name not marked missing is simply forgotten (its ready texture,
    /// if any, is the caller's own cache to keep).
    pub fn take(self: *PictureQueue, buffer: [][]u8) [][]u8 {
        var count: usize = 0;
        while (count < buffer.len and self.queue.items.len != 0) : (count += 1) {
            const name = self.queue.orderedRemove(0);
            _ = self.queued.remove(name);
            buffer[count] = name;
        }
        return buffer[0..count];
    }

    /// Marks `name` (already taken) as missing - never queued again until
    /// `clear`.
    pub fn markMissing(self: *PictureQueue, name: []const u8) void {
        if (self.missing.contains(name)) return;
        const owned = self.allocator.dupe(u8, name) catch return;
        self.missing.put(self.allocator, owned, {}) catch self.allocator.free(owned);
    }
};

/// One tile the Brush's picker offers (03-15 gap fix, Johannes's M1 hand try:
/// "tile 0", "tile 1", ... was hard to choose from without seeing them): its
/// index, and the terrain type the tileset description lists it under
/// (BkEditorDescribeTile) - `terrain_index` -1 and an empty name when the
/// bridge would not say.
pub const TileEntry = struct {
    tile: u8,
    terrain_index: i32 = -1,
    terrain: NameText = .{},
};

/// The picker's order: by terrain type, in the tileset description's own
/// order (Icicle, Ice, Blizzard, ... on coldwinter), then by tile within it;
/// tiles the bridge could not describe last. In place, stable.
pub fn sortTilesForPicker(entries: []TileEntry) void {
    std.sort.block(TileEntry, entries, {}, struct {
        fn key(entry: TileEntry) i64 {
            const group: i64 = if (entry.terrain_index < 0) std.math.maxInt(i32) else entry.terrain_index;
            return group * 256 + entry.tile;
        }
        fn less(_: void, a: TileEntry, b: TileEntry) bool {
            return key(a) < key(b);
        }
    }.less);
}

/// One section of the picker: a run of `sortTilesForPicker`'s order with one
/// terrain type, `entries[start..end]`.
pub const TileGroup = struct { start: usize, end: usize };

/// The section starting at `start` (< entries.len): every following entry
/// of the same terrain type.
pub fn nextTileGroup(entries: []const TileEntry, start: usize) TileGroup {
    var end = start + 1;
    while (end < entries.len and entries[end].terrain_index == entries[start].terrain_index) : (end += 1) {}
    return .{ .start = start, .end = end };
}

/// Where `tile` sits in the picker's entries, or null when the tileset does
/// not offer it.
pub fn indexOfTile(entries: []const TileEntry, tile: u8) ?usize {
    for (entries, 0..) |entry, index| {
        if (entry.tile == tile) return index;
    }
    return null;
}

/// How many cells `cell` wide, `spacing` apart, fit side by side in `avail`
/// - never fewer than one, so a narrow popup still shows a column.
pub fn gridColumns(avail: f32, cell: f32, spacing: f32) usize {
    if (!(cell > 0) or !(avail > cell)) return 1;
    const columns = @floor((avail + spacing) / (cell + @max(spacing, 0)));
    if (!std.math.isFinite(columns) or columns < 1) return 1;
    return @intFromFloat(@min(columns, 256));
}

/// A tile as the picker names it: "tile 12 - Snow", or "tile 12" alone when
/// the tileset gave no terrain type for it. NUL-terminated for ImGui; cut to
/// `buffer` if it must be.
pub fn tileLabel(buffer: []u8, entry: TileEntry) [:0]const u8 {
    std.debug.assert(buffer.len >= 2);
    const room = buffer[0 .. buffer.len - 1];
    const written = if (entry.terrain.len != 0)
        std.fmt.bufPrint(room, "tile {d} - {s}", .{ entry.tile, entry.terrain.slice() }) catch room
    else
        std.fmt.bufPrint(room, "tile {d}", .{entry.tile}) catch room;
    buffer[written.len] = 0;
    return buffer[0..written.len :0];
}

/// The name a tile's picture is cached under (pictures.zig keys by string):
/// its index in decimal.
pub fn tileKey(buffer: *[3]u8, tile: u8) []const u8 {
    return std.fmt.bufPrint(buffer, "{d}", .{tile}) catch unreachable;
}

/// The tile index a `tileKey` names, or null for anything else.
pub fn tileFromKey(key: []const u8) ?u8 {
    if (key.len == 0 or key.len > 3) return null;
    return std.fmt.parseInt(u8, key, 10) catch null;
}

/// Whether the tile pictures cached for tileset `cached` are still this
/// map's: a map with another tileset (or one the bridge would not name)
/// needs them all again - the same tile index is another picture there.
pub fn tilePicturesStale(cached: []const u8, now: []const u8) bool {
    return now.len == 0 or !std.mem.eql(u8, cached, now);
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

/// One distinct unknown object type, and how many objects of it the map
/// holds - `summarizeUnknown`'s own rows (spec Errors -> Open).
pub const UnknownType = struct {
    name: [core.bridge.name_capacity]u8 = [_]u8{0} ** core.bridge.name_capacity,
    count: usize = 0,

    pub fn nameSlice(self: *const UnknownType) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    fn setName(self: *UnknownType, text: []const u8) void {
        const len = @min(text.len, core.bridge.name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// Every unknown object's type once, with its count, most frequent first
/// (names compared exactly) - the warning the spec's Errors -> Open asks for.
/// Fills at most `out.len` distinct types (stable order for equal counts, so
/// the result is deterministic); returns how many it filled. A type past
/// `out.len` is silently left out of the summary rather than shown
/// incomplete - the same convention `State.mod_list_buffer` already uses for
/// a similarly-bounded list.
pub fn summarizeUnknown(objects: []const ObjectRecord, out: []UnknownType) usize {
    var count: usize = 0;
    for (objects) |object| {
        if (object.known) continue;
        const name = object.nameSlice();
        var found = false;
        for (out[0..count]) |*entry| {
            if (std.mem.eql(u8, entry.nameSlice(), name)) {
                entry.count += 1;
                found = true;
                break;
            }
        }
        if (!found and count < out.len) {
            out[count] = .{};
            out[count].setName(name);
            out[count].count = 1;
            count += 1;
        }
    }
    std.sort.insertion(UnknownType, out[0..count], {}, struct {
        fn lessThan(_: void, a: UnknownType, b: UnknownType) bool {
            return a.count > b.count;
        }
    }.lessThan);
    return count;
}

/// The window title: the map's file name, a `*` while it has changes the
/// file does not, and " (read-only)" for a shipped map (D-18) - it can be
/// both at once: a shipped map may still be edited in memory, just not
/// saved back over itself.
pub fn formatTitle(buffer: []u8, path: []const u8, dirty: bool, read_only: bool) [:0]const u8 {
    const plain = "Map Editor";
    if (path.len == 0) return std.fmt.bufPrintZ(buffer, plain, .{}) catch "";
    const name = baseName(path);
    const star = if (dirty) "*" else "";
    const suffix = if (read_only) " (read-only)" else "";
    return std.fmt.bufPrintZ(buffer, plain ++ " - {s}{s}{s}", .{ name, star, suffix }) catch
        std.fmt.bufPrintZ(buffer, plain, .{}) catch "";
}

const normalizeForCompare = core.shipped.normalizeForCompare;
const isAbsolutePath = core.shipped.isAbsolutePath;

/// Whether an engine-form path (the document's own, backslash-separated,
/// possibly relative to the installation the editor runs from) names a file
/// the game ships (D-18) - core/shipped.zig's rule: under `<base_root>Data`
/// or `<base_root>mods/<any>/data`, under any `mods/<any>/data`, or under a
/// `Data` of ANY game installation (a marker file at its top says so), each
/// for the path as given and for its real path. `files` answers the disk's
/// questions (the marker, the real path); null judges the text alone -
/// which is how another installation's Data, or this one's reached through
/// a symlink, used to pass for a user's file (03-15's hand try: Save wrote
/// straight into another checkout's coldwinter.bzm).
pub fn isShippedMap(engine_path: []const u8, base_root: []const u8, files: ?core.files.Files) bool {
    return core.shipped.isShipped(engine_path, base_root, files);
}

/// Whether an engine-form path sits inside `<user_root>mapeditor/recovery/`
/// (D-22): a reopened recovery copy is saved with Save As, the same as a
/// shipped map, since the recovery folder is never the map's real home. Only
/// an absolute path can be inside it - `user_root` is always absolute, and a
/// relative document path (every shipped map, and every map opened by a bare
/// relative argument) can never resolve there.
fn isRecoveryPath(engine_path: []const u8, user_root: []const u8) bool {
    if (user_root.len == 0) return false;
    var path_buffer: [PathSlot.max_path]u8 = undefined;
    var root_buffer: [PathSlot.max_path]u8 = undefined;
    const path = normalizeForCompare(&path_buffer, engine_path) orelse return false;
    const root = normalizeForCompare(&root_buffer, user_root) orelse return false;
    if (!isAbsolutePath(path)) return false;
    if (!std.mem.startsWith(u8, path, root)) return false;
    return std.mem.startsWith(u8, path[root.len..], "mapeditor/recovery/");
}

/// Whether Save must behave like Save As (D-18, D-22): an empty path (a new
/// map, never saved), a shipped one, or a path inside the recovery folder -
/// every case where writing straight to `doc_path` is either impossible or
/// not where the map actually belongs.
pub fn needsSaveAs(doc_path: []const u8, base_root: []const u8, user_root: []const u8, files: ?core.files.Files) bool {
    return needsSaveAsKnowing(doc_path, isShippedMap(doc_path, base_root, files), user_root);
}

/// `needsSaveAs` with `isShippedMap`'s answer already known - the panels
/// cache it per document path, since it asks the disk.
pub fn needsSaveAsKnowing(doc_path: []const u8, shipped: bool, user_root: []const u8) bool {
    return doc_path.len == 0 or shipped or isRecoveryPath(doc_path, user_root);
}

/// `<user_root>maps`, or `<user_root>mods/<mod_folder>/maps` with a mod
/// active (D-17, D-28) - written with `user_root`'s own separator (it
/// already ends with one, BkEditorPaths' doc comment). Null when the buffer
/// is too small, or `mod_folder` is unsafe as a directory component: empty,
/// a separator, "." or ".." (the same refusal GeneratedData.h's ModKey
/// makes, for the same reason - this would-be folder name came from a mod's
/// own name, not something the editor chose).
pub fn defaultMapsFolder(buffer: []u8, user_root: []const u8, mod_folder: ?[]const u8) ?[]const u8 {
    const sep: u8 = if (std.mem.indexOfScalar(u8, user_root, '\\') != null) '\\' else '/';
    if (mod_folder) |folder| {
        if (folder.len == 0 or std.mem.eql(u8, folder, ".") or std.mem.eql(u8, folder, "..")) return null;
        if (std.mem.indexOfAny(u8, folder, "/\\") != null) return null;
        return std.fmt.bufPrint(buffer, "{s}mods{c}{s}{c}maps", .{ user_root, sep, folder, sep }) catch null;
    }
    return std.fmt.bufPrint(buffer, "{s}maps", .{user_root}) catch null;
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
/// `open_path` (03-07's Open Recent) and `switch_mod` (03-08's Mod switch)
/// guard through the same prompt without changing its shape.
pub const Pending = union(enum) {
    open_dialog,
    open_path: PathText,
    quit,
    switch_mod: NameText,
    /// File > Close (03-15 gap fix).
    close,
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
    /// File > Open Recent (D-27): an OS path to open, guarded through the
    /// same unsaved-changes prompt as a plain Open.
    open_path_requested: ?PathText = null,
    /// `stepForPending`'s own copy of an `open_path` Pending's bytes: a
    /// `Pending` travels by value through `UnsavedPrompt.pending`, `answer`'s
    /// and `guard`'s return values, and `stepForPending`'s own parameter -
    /// each a fresh stack copy. Copying into this field (owned by `self`,
    /// which outlives every one of those stack frames) before returning a
    /// `Step.act_on_path.path` slice is what keeps that slice valid once
    /// `stepForPending` returns; slicing straight from the parameter would
    /// point into a stack frame that no longer exists.
    open_path_scratch: PathText = .{},
    /// File > Mod (D-26, D-23): the chosen mod's folder, guarded through the
    /// same unsaved-changes prompt as Open - an empty folder is "None" (the
    /// base game), the same convention `RealBridge.setMod`'s null/"" already
    /// carries, so this layer needs no separate "cleared" case.
    switch_mod_requested: ?NameText = null,
    /// `stepForPending`'s own copy of a `switch_mod` Pending's folder, for the
    /// same use-after-return reason `open_path_scratch` exists.
    switch_mod_scratch: NameText = .{},
    /// File > Close and Cmd/Ctrl+W (03-15 gap fix): guarded through the same
    /// unsaved-changes prompt as Open; the caller closes the map once
    /// `next()` returns `.close` (`closeMapAndDocument`).
    close_requested: bool = false,

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
        /// File > Mod: switch to this folder, or "" for None (D-26).
        switch_mod: []const u8,
        /// File > Close: close the map, its edits saved or abandoned already.
        close,
        quit,
        /// A second Open or Save As while a dialog is already up (Task 5,
        /// carried from plan 5: this used to be dropped silently) - the
        /// caller shows it on the status line.
        dialog_busy,
    };

    /// The save `next`/`act` was told to make, for `saveFinished`, resolved
    /// into a Step.
    fn stepForPending(self: *FileActions, pending: Pending) Step {
        return switch (pending) {
            .open_dialog => if (self.dialog.request(.open)) Step{ .show_dialog = .open } else Step.dialog_busy,
            .quit => .quit,
            .close => .close,
            .open_path => |path| blk: {
                // See open_path_scratch's own doc comment for why this copy
                // has to happen before the slice is built.
                self.open_path_scratch = path;
                break :blk Step{ .act_on_path = .{ .kind = .open, .path = self.open_path_scratch.slice() } };
            },
            .switch_mod => |folder| blk: {
                // See switch_mod_scratch's own doc comment - the same
                // use-after-return reason open_path_scratch exists for.
                self.switch_mod_scratch = folder;
                break :blk Step{ .switch_mod = self.switch_mod_scratch.slice() };
            },
        };
    }

    /// File > Open Recent (D-27, D-23): guards through the same
    /// unsaved-changes prompt Open itself uses, and converts/opens through
    /// `actOnPath` (the `.act_on_path` Step `next()` returns, once guarded,
    /// is identical in shape to what a dialog's own choice produces).
    pub fn requestOpenPath(self: *FileActions, os_path: []const u8) void {
        self.open_path_requested = PathText.init(os_path);
    }

    /// File > Mod (D-26, D-23; revised 2026-09-29): guards the chosen folder
    /// ("" for None) through the same unsaved-changes prompt; the caller
    /// (panels.zig) closes the map and makes the switch once `next()` returns
    /// `.switch_mod` (`switchModClosingMap`). `active` is the mod active now
    /// (null for None): choosing it again is a no-op - nothing is queued, so
    /// no prompt asks and the open map stays open.
    pub fn requestSwitchMod(self: *FileActions, folder: []const u8, active: ?[]const u8) void {
        if (isSameMod(folder, active)) return;
        self.switch_mod_requested = NameText.init(folder);
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
                } else Step.dialog_busy,
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
                return .dialog_busy;
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
        if (self.open_path_requested) |path| {
            self.open_path_requested = null;
            return switch (self.prompt.guard(dirty, .{ .open_path = path })) {
                .proceed => |pending| self.stepForPending(pending),
                .asked => .ask_unsaved,
            };
        }
        if (self.switch_mod_requested) |folder| {
            self.switch_mod_requested = null;
            return switch (self.prompt.guard(dirty, .{ .switch_mod = folder })) {
                .proceed => |pending| self.stepForPending(pending),
                .asked => .ask_unsaved,
            };
        }
        if (self.close_requested) {
            self.close_requested = false;
            return switch (self.prompt.guard(dirty, .close)) {
                .proceed => |pending| self.stepForPending(pending),
                .asked => .ask_unsaved,
            };
        }
        if (self.save_as_requested) {
            self.save_as_requested = false;
            if (self.dialog.request(.save_as)) return .{ .show_dialog = .save_as };
            return .dialog_busy;
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

/// Whether File > Mod's `folder` ("" for None) is the mod already active
/// (`active`, null for None) - the same exact comparison the menu's own
/// checkmark makes (panels.zig's drawModItems).
pub fn isSameMod(folder: []const u8, active: ?[]const u8) bool {
    const current = active orelse "";
    return std.mem.eql(u8, folder, current);
}

/// What `switchModClosingMap` did.
pub const ModSwitchOutcome = enum {
    /// The mod is switched and the map closed: the editor has no document.
    switched,
    /// The bridge refused before touching anything (an unknown or bad
    /// folder - BkEditorSetMod's own contract): the mod and the open map are
    /// exactly as they were.
    refused,
    /// The bridge failed partway (the engine's map is already closed, the
    /// object database may be half-loaded): the document is closed too, so
    /// it never claims a map the engine no longer has.
    failed,
};

/// File > Mod's guarded step (D-26, revised 2026-09-29 in the hand try:
/// switching closes the map). By the time this runs the unsaved-changes
/// prompt (D-23) has already been answered - Save landed, or Don't save
/// abandoned the edits - so the map is closed, not reopened under the new
/// mod: a map read under one object database and shown under another is
/// what mixed the databases (a map saved under a mod, switched to None,
/// showed 1359 unknown objects). `switcher` is anything with
/// `setMod(?[]const u8) core.bridge.Status` - `RealBridge` in the app, a
/// recording fake in the tests; BkEditorSetMod closes the engine's own map
/// before it swaps the database, and works with no map open at all.
/// `folder` "" is None.
pub fn switchModClosingMap(editor: *Editor, switcher: anytype, folder: []const u8) ModSwitchOutcome {
    const requested: ?[]const u8 = if (folder.len == 0) null else folder;
    return switch (switcher.setMod(requested)) {
        .ok => blk: {
            editor.close();
            break :blk .switched;
        },
        // Checked before anything changed: the map and the mod stay.
        .refused, .bad_argument, .no_session => .refused,
        else => blk: {
            editor.close();
            break :blk .failed;
        },
    };
}

/// File > Close's guarded step (03-15 gap fix, Johannes's M1 hand try). By
/// the time this runs the unsaved-changes prompt (D-23) has been answered -
/// a Save landed or Don't save abandoned the edits - so the engine's map is
/// closed (`closer.closeMap()`: `RealBridge.closeMap`, BkEditorCloseMap, in
/// the app; a recording fake in the tests) and the document with it
/// (`Editor.close`: no path, nothing to undo or redo, nothing selected). The
/// document is closed whatever the bridge answers: its only refusal is an
/// engine that is not started, which holds no map to keep either, and after
/// a failure the engine's map is in no state to keep editing - the editor
/// must never claim a map the engine may no longer have (`switchModClosingMap`'s
/// own rule for a failure partway). The bridge's answer is returned for the
/// status line.
pub fn closeMapAndDocument(editor: *Editor, closer: anytype) core.bridge.Status {
    const result = closer.closeMap();
    editor.close();
    return result;
}

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

test "sound times: milliseconds to seconds and back, non-finite is 0" {
    try std.testing.expectEqual(@as(f32, 1.5), msToSeconds(1500));
    try std.testing.expectEqual(@as(f32, 0), msToSeconds(0));
    try std.testing.expectEqual(@as(i32, 1500), secondsToMs(1.5));
    try std.testing.expectEqual(@as(i32, 0), secondsToMs(0));
    try std.testing.expectEqual(@as(i32, 0), secondsToMs(std.math.nan(f32)));
    var ms: i32 = 250;
    while (ms < 10000) : (ms += 250) try std.testing.expectEqual(ms, secondsToMs(msToSeconds(ms)));
}

test "sortNamesIgnoreCase sorts case-insensitively" {
    var names = [_][]const u8{ "Wind", "amb_forest", "Explosion" };
    sortNamesIgnoreCase(&names);
    try std.testing.expectEqualStrings("amb_forest", names[0]);
    try std.testing.expectEqualStrings("Explosion", names[1]);
    try std.testing.expectEqualStrings("Wind", names[2]);
}

test "soundRadiusError names a min above max, and nothing else" {
    try std.testing.expect(soundRadiusError(5, 1) != null);
    try std.testing.expect(soundRadiusError(1, 5) == null);
    try std.testing.expect(soundRadiusError(3, 3) == null);
}

test "PictureQueue: a name is queued once, served in request order" {
    var queue = PictureQueue.init(std.testing.allocator);
    defer queue.deinit();
    queue.request("b");
    queue.request("a");
    queue.request("b"); // already queued: not requeued, not duplicated
    try std.testing.expectEqual(@as(usize, 2), queue.pendingCount());
    var buffer: [8][]u8 = undefined;
    const taken = queue.take(&buffer);
    defer for (taken) |name| std.testing.allocator.free(name);
    try std.testing.expectEqual(@as(usize, 2), taken.len);
    try std.testing.expectEqualStrings("b", taken[0]);
    try std.testing.expectEqualStrings("a", taken[1]);
    try std.testing.expectEqual(@as(usize, 0), queue.pendingCount());
}

test "PictureQueue: take serves at most the given budget, the rest waits for the next frame" {
    var queue = PictureQueue.init(std.testing.allocator);
    defer queue.deinit();
    queue.request("a");
    queue.request("b");
    queue.request("c");
    var buffer: [2][]u8 = undefined;
    const first = queue.take(&buffer);
    try std.testing.expectEqual(@as(usize, 2), first.len);
    try std.testing.expectEqualStrings("a", first[0]);
    try std.testing.expectEqualStrings("b", first[1]);
    for (first) |name| std.testing.allocator.free(name);
    try std.testing.expectEqual(@as(usize, 1), queue.pendingCount());
    const second = queue.take(&buffer);
    try std.testing.expectEqual(@as(usize, 1), second.len);
    try std.testing.expectEqualStrings("c", second[0]);
    for (second) |name| std.testing.allocator.free(name);
}

test "PictureQueue: a name marked missing is never queued again until clear" {
    var queue = PictureQueue.init(std.testing.allocator);
    defer queue.deinit();
    queue.request("x");
    var buffer: [1][]u8 = undefined;
    const taken = queue.take(&buffer);
    try std.testing.expectEqual(@as(usize, 1), taken.len);
    queue.markMissing(taken[0]);
    std.testing.allocator.free(taken[0]);
    try std.testing.expect(queue.isMissing("x"));
    queue.request("x"); // already missing: never retried
    try std.testing.expectEqual(@as(usize, 0), queue.pendingCount());
    queue.clear();
    try std.testing.expect(!queue.isMissing("x"));
    queue.request("x");
    try std.testing.expectEqual(@as(usize, 1), queue.pendingCount());
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
    try std.testing.expectEqualStrings("Map Editor", formatTitle(&buffer, "", false, false));
    try std.testing.expectEqualStrings("Map Editor - coldwinter.bzm", formatTitle(&buffer, "Data\\Maps\\Multiplayer\\coldwinter.bzm", false, false));
    try std.testing.expectEqualStrings("Map Editor - mine.xml*", formatTitle(&buffer, "/Users/me/mine.xml", true, false));
}

test "the title marks a shipped map read-only, dirty or not" {
    var buffer: [128]u8 = undefined;
    try std.testing.expectEqualStrings("Map Editor - coldwinter.bzm (read-only)", formatTitle(&buffer, "Data\\Maps\\Multiplayer\\coldwinter.bzm", false, true));
    try std.testing.expectEqualStrings("Map Editor - coldwinter.bzm* (read-only)", formatTitle(&buffer, "Data\\Maps\\Multiplayer\\coldwinter.bzm", true, true));
}

test "isShippedMap: Data and mods/*/data are shipped, relative or absolute, separators and case ignored" {
    const base = "/Users/me/MapEditor/";
    const user_root = "/Users/me/.local/share/Nival/Blitzkrieg/";
    try std.testing.expect(isShippedMap("Data\\Maps\\Multiplayer\\coldwinter.bzm", base, null));
    try std.testing.expect(isShippedMap("/Users/me/MapEditor/Data/Maps/Multiplayer/coldwinter.bzm", base, null));
    try std.testing.expect(isShippedMap("/Users/me/MapEditor/mods/X/data/maps/a.bzm", base, null));
    try std.testing.expect(isShippedMap("mods\\X\\data\\maps\\a.bzm", base, null));
    try std.testing.expect(!isShippedMap(user_root ++ "maps/a.bzm", base, null));
    try std.testing.expect(!isShippedMap("/Users/me/MapEditor/mods/X/maps/a.bzm", base, null)); // no "data" segment
    // Mixed case and separators, absolute and relative.
    try std.testing.expect(isShippedMap("/USERS/ME/MAPEDITOR/DATA/maps/a.BZM", base, null));
    try std.testing.expect(isShippedMap("DATA\\maps\\a.bzm", base, null));
}

test "needsSaveAs: an empty path, a shipped map, or a recovery-folder path redirects Save to Save As" {
    const base = "/Users/me/MapEditor/";
    const user_root = "/Users/me/.local/share/Nival/Blitzkrieg/";
    try std.testing.expect(needsSaveAs("", base, user_root, null));
    try std.testing.expect(needsSaveAs("Data\\Maps\\Multiplayer\\coldwinter.bzm", base, user_root, null));
    try std.testing.expect(!needsSaveAs(user_root ++ "maps/a.bzm", base, user_root, null));
    try std.testing.expect(needsSaveAs(user_root ++ "mapeditor/recovery/coldwinter.bzm", base, user_root, null));
    try std.testing.expect(needsSaveAs("/USERS/ME/.LOCAL/SHARE/NIVAL/BLITZKRIEG/mapeditor\\recovery\\a.bzm", base, user_root, null));
    // A path outside the recovery folder, still under user_root, is unaffected.
    try std.testing.expect(!needsSaveAs(user_root ++ "mapeditor/mapeditor.cfg", base, user_root, null));
}

test "isShippedMap and needsSaveAs: another installation's Data, found by its marker, is read-only too" {
    // 03-15's hand try: the release stage's MapEditor opened the main
    // checkout's coldwinter.bzm by its absolute path, and Save wrote into it.
    const base = "/Users/me/Blitzkrieg/.worktrees/map-editor-6/zig-out/game/macos/arm64/release/";
    const user_root = "/Users/me/.local/share/Nival/Blitzkrieg/";
    const other = "/Users/me/Blitzkrieg/Data/Maps/Multiplayer/coldwinter.bzm";
    var fake = core.files.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    fake.data_roots = &.{ "/Users/me/Blitzkrieg/Data", "/Users/me/Blitzkrieg/.worktrees/map-editor-6/Data" };
    fake.links = &.{.{ .from = base ++ "Data", .to = "/Users/me/Blitzkrieg/.worktrees/map-editor-6/Data" }};
    const files = fake.files();
    try std.testing.expect(!isShippedMap(other, base, null)); // the text alone cannot tell
    try std.testing.expect(isShippedMap(other, base, files));
    try std.testing.expect(isShippedMap("\\Users\\me\\Blitzkrieg\\Data\\Maps\\Multiplayer\\coldwinter.bzm", base, files));
    // The stage's own Data, reached through the symlink's target.
    try std.testing.expect(isShippedMap("/Users/me/Blitzkrieg/.worktrees/map-editor-6/Data/Maps/a.bzm", base, files));
    try std.testing.expect(needsSaveAs(other, base, user_root, files));
    // Autosave follows the same answer: a recovery copy, never the file.
    try std.testing.expectEqual(core.autosave.Target.recovery_copy, core.autosave.target(needsSaveAs(other, base, user_root, files)));
    // The user's own folders stay writable, and autosave into themselves.
    try std.testing.expect(!needsSaveAs(user_root ++ "maps/mine.bzm", base, user_root, files));
    try std.testing.expect(!needsSaveAs(user_root ++ "mods/MyMod/maps/mine.bzm", base, user_root, files));
    try std.testing.expectEqual(core.autosave.Target.map_file, core.autosave.target(needsSaveAs(user_root ++ "maps/mine.bzm", base, user_root, files)));
}

test "defaultMapsFolder: the user root plus maps, or mods/<name>/maps; a bad mod folder is refused" {
    var buffer: [256]u8 = undefined;
    try std.testing.expectEqualStrings("/Users/me/Blitzkrieg/maps", defaultMapsFolder(&buffer, "/Users/me/Blitzkrieg/", null).?);
    try std.testing.expectEqualStrings("/Users/me/Blitzkrieg/mods/MyMod/maps", defaultMapsFolder(&buffer, "/Users/me/Blitzkrieg/", "MyMod").?);
    try std.testing.expect(defaultMapsFolder(&buffer, "/Users/me/Blitzkrieg/", "..") == null);
    try std.testing.expect(defaultMapsFolder(&buffer, "/Users/me/Blitzkrieg/", ".") == null);
    try std.testing.expect(defaultMapsFolder(&buffer, "/Users/me/Blitzkrieg/", "a/b") == null);
    try std.testing.expect(defaultMapsFolder(&buffer, "/Users/me/Blitzkrieg/", "") == null);
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
    // A second Open while the dialog is up reports dialog_busy (Task 5,
    // carried from plan 5 - this used to be dropped as a silent .none).
    actions.open_requested = true;
    try std.testing.expectEqual(FileActions.Step.dialog_busy, actions.next(false, false));

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

test "file actions: Save As, and Save on a path-less map, both report dialog_busy while a dialog is already up" {
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    actions.save_as_requested = true;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next(false, false));
    actions.save_as_requested = true;
    try std.testing.expectEqual(FileActions.Step.dialog_busy, actions.next(false, false));

    actions.save_requested = true;
    // needs_save_as = true: a plain Save on a new/shipped map shows Save As
    // too, so it is just as busy while the same dialog is still up.
    try std.testing.expectEqual(FileActions.Step.dialog_busy, actions.next(false, true));
}

test "summarizeUnknown: the fake bridge's one mystery object is one type, one object" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    var out: [8]UnknownType = undefined;
    const count = summarizeUnknown(editor.document.objects.items, &out);
    try std.testing.expectEqual(@as(usize, 1), count);
    try std.testing.expectEqualStrings("No_Such_Object", out[0].nameSlice());
    try std.testing.expectEqual(@as(usize, 1), out[0].count);
}

test "summarizeUnknown: repeats of the same name count together, and the most frequent type comes first" {
    var one: ObjectRecord = .{ .link_id = 1, .known = false };
    one.setName("Alpha");
    var two: ObjectRecord = .{ .link_id = 2, .known = false };
    two.setName("Beta");
    var three: ObjectRecord = .{ .link_id = 3, .known = false };
    three.setName("Alpha");
    var known: ObjectRecord = .{ .link_id = 4, .known = true };
    known.setName("T34");
    var four: ObjectRecord = .{ .link_id = 5, .known = false };
    four.setName("Alpha");
    const objects = [_]ObjectRecord{ one, two, three, known, four };
    var out: [8]UnknownType = undefined;
    const count = summarizeUnknown(&objects, &out);
    try std.testing.expectEqual(@as(usize, 2), count);
    try std.testing.expectEqualStrings("Alpha", out[0].nameSlice());
    try std.testing.expectEqual(@as(usize, 3), out[0].count);
    try std.testing.expectEqualStrings("Beta", out[1].nameSlice());
    try std.testing.expectEqual(@as(usize, 1), out[1].count);
}

test "summarizeUnknown: no unknown objects is zero rows" {
    var known: ObjectRecord = .{ .link_id = 1 };
    known.setName("T34");
    const objects = [_]ObjectRecord{known};
    var out: [8]UnknownType = undefined;
    try std.testing.expectEqual(@as(usize, 0), summarizeUnknown(&objects, &out));
}

test "file actions: Open Recent opens directly on a clean map, guarded on a dirty one" {
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    actions.requestOpenPath("/maps/recent.bzm");
    const clean_step = actions.next(false, false);
    try std.testing.expectEqual(DialogKind.open, clean_step.act_on_path.kind);
    try std.testing.expectEqualStrings("/maps/recent.bzm", clean_step.act_on_path.path);

    actions.requestOpenPath("/maps/another.bzm");
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    try std.testing.expect(actions.prompt.isAsking());
    actions.answer_pending = .dont_save;
    const dirty_step = actions.next(true, false);
    try std.testing.expectEqual(DialogKind.open, dirty_step.act_on_path.kind);
    try std.testing.expectEqualStrings("/maps/another.bzm", dirty_step.act_on_path.path);
    try std.testing.expect(!actions.prompt.isAsking());
}

test "file actions: Open Recent's path opens through the editor, converted with enginePath" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    actions.requestOpenPath("/maps/fixture.bzm");
    const step = actions.next(false, false);
    try actOnPath(&editor, step.act_on_path.kind, step.act_on_path.path);
    try std.testing.expectEqualStrings("\\maps\\fixture.bzm", editor.document.path.items);
}

test "file actions: switching mods guards through the unsaved prompt, and an empty folder is None" {
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    // A clean map switches at once.
    actions.requestSwitchMod("OtherMod", null);
    const clean_step = actions.next(false, false);
    try std.testing.expectEqualStrings("OtherMod", clean_step.switch_mod);

    // A dirty map asks first; Cancel keeps the mod (no switch_mod step).
    actions.requestSwitchMod("", "OtherMod");
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    try std.testing.expect(actions.prompt.isAsking());
    actions.answer_pending = .cancel;
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));
    try std.testing.expect(!actions.prompt.isAsking());

    // Don't save proceeds with the switch, folder "" naming None.
    actions.requestSwitchMod("", "OtherMod");
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    actions.answer_pending = .dont_save;
    const dirty_step = actions.next(true, false);
    try std.testing.expectEqualStrings("", dirty_step.switch_mod);
    try std.testing.expect(!actions.prompt.isAsking());
}

test "isSameMod: the active mod, or None while none is active" {
    try std.testing.expect(isSameMod("", null));
    try std.testing.expect(isSameMod("MyMod", "MyMod"));
    try std.testing.expect(!isSameMod("MyMod", null));
    try std.testing.expect(!isSameMod("", "MyMod"));
    try std.testing.expect(!isSameMod("OtherMod", "MyMod"));
}

/// What `switchModClosingMap` hands the bridge, and what it answers - a
/// stand-in for `RealBridge.setMod`, which needs the engine.
const RecordingModSwitcher = struct {
    answer: core.bridge.Status = .ok,
    calls: usize = 0,
    last: NameText = .{},
    last_was_null: bool = false,

    pub fn setMod(self: *RecordingModSwitcher, folder: ?[]const u8) core.bridge.Status {
        self.calls += 1;
        self.last_was_null = folder == null;
        self.last = NameText.init(folder orelse "");
        return self.answer;
    }
};

/// Opens the fake's fixture and makes one edit, so the map is dirty.
fn openDirtyFixture(editor: *Editor) !void {
    try editor.open("fixture.bzm");
    _ = try editor.addObject("T34", 60, 60, 0, 1);
    try std.testing.expect(editor.dirty());
}

fn expectNoMapOpen(editor: *const Editor) !void {
    try std.testing.expectEqual(@as(usize, 0), editor.document.path.items.len);
    try std.testing.expectEqual(@as(usize, 0), editor.document.objects.items.len);
    try std.testing.expect(!editor.dirty());
    try std.testing.expect(!editor.history.canUndo());
    try std.testing.expect(!editor.history.canRedo());
    try std.testing.expect(editor.selection == null);
}

test "mod switch (D-26 revised): Cancel on a dirty map leaves the map open and the mod unchanged" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try openDirtyFixture(&editor);
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    actions.requestSwitchMod("OtherMod", null);
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(editor.dirty(), false));
    actions.answer_pending = .cancel;
    // No switch_mod step ever comes: nothing reaches the bridge.
    try std.testing.expectEqual(FileActions.Step.none, actions.next(editor.dirty(), false));
    try std.testing.expectEqual(FileActions.Step.none, actions.next(editor.dirty(), false));
    try std.testing.expect(!actions.prompt.isAsking());
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(editor.dirty());
}

test "mod switch (D-26 revised): Don't save closes the map, then switches" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try openDirtyFixture(&editor);
    editor.selection = 1;
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };
    var switcher: RecordingModSwitcher = .{};

    actions.requestSwitchMod("OtherMod", null);
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(editor.dirty(), false));
    actions.answer_pending = .dont_save;
    const step = actions.next(editor.dirty(), false);
    try std.testing.expectEqualStrings("OtherMod", step.switch_mod);
    try std.testing.expectEqual(ModSwitchOutcome.switched, switchModClosingMap(&editor, &switcher, step.switch_mod));
    try std.testing.expectEqual(@as(usize, 1), switcher.calls);
    try std.testing.expectEqualStrings("OtherMod", switcher.last.slice());
    try expectNoMapOpen(&editor);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(editor.dirty(), false));

    // With no map open a switch goes straight through (nothing to guard),
    // and None reaches the bridge as null.
    actions.requestSwitchMod("", "OtherMod");
    const back = actions.next(editor.dirty(), false);
    try std.testing.expectEqualStrings("", back.switch_mod);
    try std.testing.expectEqual(ModSwitchOutcome.switched, switchModClosingMap(&editor, &switcher, back.switch_mod));
    try std.testing.expect(switcher.last_was_null);
    try expectNoMapOpen(&editor);
}

test "mod switch (D-26 revised): Save saves first, then closes and switches" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try openDirtyFixture(&editor);
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };
    var switcher: RecordingModSwitcher = .{};

    // A map with a writable path: a plain Save, reported back.
    actions.requestSwitchMod("OtherMod", null);
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(editor.dirty(), false));
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step.save, actions.next(editor.dirty(), false));
    // The switch waits for the save's outcome.
    try std.testing.expectEqual(@as(usize, 0), switcher.calls);
    editor.history.markClean(); // what a landed save leaves behind
    actions.noteSaveOutcome(true);
    const step = actions.next(editor.dirty(), false);
    try std.testing.expectEqualStrings("OtherMod", step.switch_mod);
    try std.testing.expectEqual(ModSwitchOutcome.switched, switchModClosingMap(&editor, &switcher, step.switch_mod));
    try expectNoMapOpen(&editor);
}

test "mod switch (D-26 revised): a failed Save, or a cancelled Save As, cancels the switch" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try openDirtyFixture(&editor);
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    // A plain save that failed: no switch, the map still dirty and open.
    actions.requestSwitchMod("OtherMod", null);
    _ = actions.next(true, false);
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step.save, actions.next(true, false));
    actions.noteSaveOutcome(false);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));

    // A read-only/shipped/new map: Save becomes Save As, whose dialog the
    // user cancels - no switch either.
    actions.requestSwitchMod("OtherMod", null);
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, true));
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next(true, true));
    slot.deliver(null);
    try std.testing.expectEqual(FileActions.Step.dialog_cancelled, actions.next(true, true));
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, true));
    try std.testing.expect(!actions.prompt.isAsking());
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(editor.dirty());

    // Save As that lands (the dialog's path acted on, then reported): the
    // switch follows.
    actions.requestSwitchMod("OtherMod", null);
    _ = actions.next(true, true);
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next(true, true));
    slot.deliver("/maps/mine.bzm");
    const chosen = actions.next(true, true);
    try std.testing.expectEqual(DialogKind.save_as, chosen.act_on_path.kind);
    actions.noteSaveOutcome(true);
    try std.testing.expectEqualStrings("OtherMod", actions.next(false, false).switch_mod);
}

test "mod switch (D-26 revised): the active mod again is a no-op, even on a dirty map" {
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };
    actions.requestSwitchMod("MyMod", "MyMod");
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));
    try std.testing.expect(!actions.prompt.isAsking());
    actions.requestSwitchMod("", null);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));
    try std.testing.expect(!actions.prompt.isAsking());
}

test "mod switch (D-26 revised): a refusal keeps the map; a failure partway closes it" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try openDirtyFixture(&editor);

    var refusing: RecordingModSwitcher = .{ .answer = .refused };
    try std.testing.expectEqual(ModSwitchOutcome.refused, switchModClosingMap(&editor, &refusing, "NoSuchMod"));
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(editor.dirty());
    var bad: RecordingModSwitcher = .{ .answer = .bad_argument };
    try std.testing.expectEqual(ModSwitchOutcome.refused, switchModClosingMap(&editor, &bad, "../x"));
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);

    var failing: RecordingModSwitcher = .{ .answer = .failed };
    try std.testing.expectEqual(ModSwitchOutcome.failed, switchModClosingMap(&editor, &failing, "OtherMod"));
    try expectNoMapOpen(&editor);
}

fn tileEntry(tile: u8, terrain_index: i32, terrain: []const u8) TileEntry {
    return .{ .tile = tile, .terrain_index = terrain_index, .terrain = NameText.init(terrain) };
}

test "tile picker: tiles sort by terrain type, then tile, the undescribed last, and group into sections" {
    var entries = [_]TileEntry{
        tileEntry(28, 2, "Blizzard"),
        tileEntry(14, 1, "Ice"),
        tileEntry(2, 0, "Icicle"),
        tileEntry(250, -1, ""),
        tileEntry(15, 1, "Ice"),
        tileEntry(0, 0, "Icicle"),
        // A tileset may list a higher tile under an earlier type.
        tileEntry(100, 0, "Icicle"),
    };
    sortTilesForPicker(&entries);
    const want = [_]u8{ 0, 2, 100, 14, 15, 28, 250 };
    for (want, entries) |tile, entry| try std.testing.expectEqual(tile, entry.tile);

    var groups: [8]TileGroup = undefined;
    var count: usize = 0;
    var start: usize = 0;
    while (start < entries.len) : (count += 1) {
        groups[count] = nextTileGroup(&entries, start);
        start = groups[count].end;
    }
    try std.testing.expectEqual(@as(usize, 4), count);
    try std.testing.expectEqual(TileGroup{ .start = 0, .end = 3 }, groups[0]);
    try std.testing.expectEqual(TileGroup{ .start = 3, .end = 5 }, groups[1]);
    try std.testing.expectEqual(TileGroup{ .start = 5, .end = 6 }, groups[2]);
    try std.testing.expectEqual(TileGroup{ .start = 6, .end = 7 }, groups[3]);
    try std.testing.expectEqualStrings("Ice", entries[groups[1].start].terrain.slice());

    try std.testing.expectEqual(@as(?usize, 4), indexOfTile(&entries, 15));
    try std.testing.expectEqual(@as(?usize, null), indexOfTile(&entries, 1));
}

test "tile picker: labels name the terrain type when there is one" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("tile 12 - Snow", tileLabel(&buffer, tileEntry(12, 4, "Snow")));
    try std.testing.expectEqualStrings("tile 7", tileLabel(&buffer, tileEntry(7, -1, "")));
    // Cut to the buffer, still terminated.
    var small: [8]u8 = undefined;
    const cut = tileLabel(&small, tileEntry(200, 0, "Dirty Snow"));
    try std.testing.expect(cut.len <= 7);
    try std.testing.expectEqual(@as(u8, 0), small[cut.len]);
}

test "tile picker: the grid fits whole cells, never fewer than one" {
    // 64-wide cells 8 apart: 6 fit in 424, 5 in 423.
    try std.testing.expectEqual(@as(usize, 6), gridColumns(424, 64, 8));
    try std.testing.expectEqual(@as(usize, 5), gridColumns(423, 64, 8));
    try std.testing.expectEqual(@as(usize, 1), gridColumns(30, 64, 8));
    try std.testing.expectEqual(@as(usize, 1), gridColumns(0, 64, 8));
    try std.testing.expectEqual(@as(usize, 1), gridColumns(500, 0, 8));
    try std.testing.expectEqual(@as(usize, 1), gridColumns(std.math.nan(f32), 64, 8));
}

test "tile picker: picture keys round-trip, and a new tileset drops the cache" {
    var buffer: [3]u8 = undefined;
    for ([_]u8{ 0, 9, 14, 255 }) |tile| try std.testing.expectEqual(@as(?u8, tile), tileFromKey(tileKey(&buffer, tile)));
    try std.testing.expectEqual(@as(?u8, null), tileFromKey(""));
    try std.testing.expectEqual(@as(?u8, null), tileFromKey("256"));
    try std.testing.expectEqual(@as(?u8, null), tileFromKey("T34"));

    try std.testing.expect(!tilePicturesStale("terrain\\sets\\2\\tileset", "terrain\\sets\\2\\tileset"));
    try std.testing.expect(tilePicturesStale("terrain\\sets\\2\\tileset", "terrain\\sets\\1\\tileset"));
    try std.testing.expect(tilePicturesStale("", "terrain\\sets\\1\\tileset"));
    // A tileset the bridge would not name is never trusted to be the same.
    try std.testing.expect(tilePicturesStale("", ""));
}

/// A stand-in for `RealBridge.closeMap`, which needs the engine.
const RecordingCloser = struct {
    answer: core.bridge.Status = .ok,
    calls: usize = 0,

    pub fn closeMap(self: *RecordingCloser) core.bridge.Status {
        self.calls += 1;
        return self.answer;
    }
};

test "File > Close: a clean map closes at once" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };
    var closer: RecordingCloser = .{};

    actions.close_requested = true;
    try std.testing.expectEqual(FileActions.Step.close, actions.next(editor.dirty(), false));
    try std.testing.expectEqual(core.bridge.Status.ok, closeMapAndDocument(&editor, &closer));
    try std.testing.expectEqual(@as(usize, 1), closer.calls);
    try expectNoMapOpen(&editor);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(editor.dirty(), false));
}

test "File > Close: a dirty map asks; Cancel keeps it, Don't save closes it" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try openDirtyFixture(&editor);
    editor.selection = 1;
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };
    var closer: RecordingCloser = .{};

    actions.close_requested = true;
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(editor.dirty(), false));
    try std.testing.expect(actions.prompt.isAsking());
    // Still asking on the next frame, until an answer comes.
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(editor.dirty(), false));
    actions.answer_pending = .cancel;
    try std.testing.expectEqual(FileActions.Step.none, actions.next(editor.dirty(), false));
    try std.testing.expectEqual(FileActions.Step.none, actions.next(editor.dirty(), false));
    try std.testing.expect(!actions.prompt.isAsking());
    try std.testing.expectEqual(@as(usize, 0), closer.calls);
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(editor.dirty());
    try std.testing.expect(editor.history.canUndo());

    actions.close_requested = true;
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(editor.dirty(), false));
    actions.answer_pending = .dont_save;
    try std.testing.expectEqual(FileActions.Step.close, actions.next(editor.dirty(), false));
    try std.testing.expectEqual(core.bridge.Status.ok, closeMapAndDocument(&editor, &closer));
    try expectNoMapOpen(&editor);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(editor.dirty(), false));
}

test "File > Close: Save saves first; a failed Save or a cancelled Save As keeps the map" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try openDirtyFixture(&editor);
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    // A plain Save that failed: no close.
    actions.close_requested = true;
    _ = actions.next(true, false);
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step.save, actions.next(true, false));
    actions.noteSaveOutcome(false);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));

    // A shipped/read-only/new map: Save becomes Save As, cancelled - no close.
    actions.close_requested = true;
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, true));
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next(true, true));
    slot.deliver(null);
    try std.testing.expectEqual(FileActions.Step.dialog_cancelled, actions.next(true, true));
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, true));
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expect(editor.dirty());

    // Save As that lands: the close follows.
    actions.close_requested = true;
    _ = actions.next(true, true);
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next(true, true));
    slot.deliver("/maps/mine.bzm");
    try std.testing.expectEqual(DialogKind.save_as, actions.next(true, true).act_on_path.kind);
    actions.noteSaveOutcome(true);
    try std.testing.expectEqual(FileActions.Step.close, actions.next(false, false));

    // A plain Save that lands: the close follows too.
    actions.close_requested = true;
    _ = actions.next(true, false);
    actions.answer_pending = .save;
    try std.testing.expectEqual(FileActions.Step.save, actions.next(true, false));
    actions.noteSaveOutcome(true);
    try std.testing.expectEqual(FileActions.Step.close, actions.next(false, false));
}

test "File > Close: the document closes whatever the bridge answers" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    for ([_]core.bridge.Status{ .refused, .failed }) |answer| {
        try openDirtyFixture(&editor);
        var closer: RecordingCloser = .{ .answer = answer };
        try std.testing.expectEqual(answer, closeMapAndDocument(&editor, &closer));
        try expectNoMapOpen(&editor);
    }
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
