//! The panels' parts that need no window, no ImGui and no SDL: the file
//! dialogs' hand-over and the file actions it drives, the object palette's
//! names and filter, the direction conversion, what the properties panel may
//! edit, and the window title. Kept apart from panels.zig so these run under
//! `zig build test-map-editor-panels` against the core's fake bridge, without
//! the engine's libraries or a GPU.
const std = @import("std");
const builtin = @import("builtin");
const core = @import("editor_core");
const kit = @import("editor_kit");
const testlaunch = kit.testlaunch;
const view_math = @import("view_math.zig");
const tool_registry = @import("tool_registry.zig");

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

/// Whether an object of this game type belongs in the palette. A sound (100)
/// and a tank pit (5) are in the object database but are not map objects: a
/// map keeps its sounds in their own list, and tank pits are dug during play.
/// The bridge refuses both (WhyNotAMapObject, Sources/src/EditorBridge/
/// session.cpp) and the MFC editor's palette never listed either
/// (TabSimpleObjectsDialog.cpp:158).
///
/// D-05 (M2): an entrenchment piece (4), a bridge span (6) and a fence (9) are
/// out too. They only make sense inside their entry - a trench, a bridge, a
/// fence run - which the Entrenchment, Bridge and Fence tools draw, so a piece
/// placed alone would be an object the map's own lists do not know about. The
/// bridge says the same through `WhyNotPlacedByPalette`, and such objects in a
/// loaded map still load, draw and move.
pub fn isPlaceable(game_type: i32) bool {
    return switch (game_type) {
        4, 5, 6, 9, 100 => false,
        else => true,
    };
}

/// The palette's filter: a case-insensitive substring. An empty filter
/// matches everything.
pub fn matchesFilter(name: []const u8, filter: []const u8) bool {
    if (filter.len == 0) return true;
    return std.ascii.indexOfIgnoreCase(name, filter) != null;
}

/// The most object filters one palette frame can have active: the combo's
/// plus the nine quick toggles.
pub const max_active_filters = core.settings.Settings.filter_slot_count + 1;

/// The palette's active object filters (M3, D-31) as indices into `filters`:
/// the combo's filter first (when one is named), then every checked slot's
/// in slot order. A name no filter carries any more, an empty slot and an
/// unchecked slot contribute nothing, and a filter already collected (the
/// combo naming a checked slot's own filter) is not collected twice. Returns
/// how many of `out`'s entries were filled.
pub fn collectActiveFilterIndices(
    combo: []const u8,
    slots: [core.settings.Settings.filter_slot_count][]const u8,
    checked: [core.settings.Settings.filter_slot_count]bool,
    filters: []const core.bridge.ObjectFilter,
    out: *[max_active_filters]usize,
) usize {
    var count: usize = 0;
    if (combo.len != 0) addNamedFilterIndex(combo, filters, out, &count);
    for (slots, checked) |slot, is_checked| {
        if (!is_checked or slot.len == 0) continue;
        addNamedFilterIndex(slot, filters, out, &count);
    }
    return count;
}

fn addNamedFilterIndex(name: []const u8, filters: []const core.bridge.ObjectFilter, out: *[max_active_filters]usize, count: *usize) void {
    for (filters, 0..) |*one, index| {
        if (!std.mem.eql(u8, one.nameSlice(), name)) continue;
        for (out[0..count.*]) |already| {
            if (already == index) return;
        }
        if (count.* >= max_active_filters) return;
        out[count.*] = index;
        count.* += 1;
        return;
    }
}

/// The palette's two filter stages together (M3, D-31): the text filter (a
/// case-insensitive substring over the object's name) is ANDed with the
/// object filters, which match the object's folder path - the MFC editor's
/// own FilterName argument (pDesc->szPath). With no active object filter
/// every placeable object shows.
pub fn paletteObjectVisible(name: []const u8, folder_path: []const u8, text_filter: []const u8, active_filters: []const core.filters.Filter) bool {
    if (!matchesFilter(name, text_filter)) return false;
    if (active_filters.len == 0) return true;
    for (active_filters) |filter| {
        if (filter.matches(folder_path)) return true;
    }
    return false;
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

/// The sound "Add at view centre" starts a new map sound with: the rivers'
/// own loop, which the game plays without a break while the view is near
/// it. The catalogue's alphabetical first (20mm_aviacannon) was the default
/// before - a single cannon report the game plays at most once every few
/// seconds (Scene/SoundScene.cpp CMapSounds), easy to miss in a test game.
pub const default_sound_name = "Amb_Water_circle";

/// Under the Sounds panel's buttons: what decides whether a placed sound is
/// heard in the game (Scene/SoundScene.cpp's map sounds), and which of the
/// record's fields the game leaves unread.
pub const sound_panel_note = "In the game a map sound plays only while the view is near it, " ++
    "about a screen away: a looped one without a break, any other every few seconds. " ++
    "The sound itself decides how far it carries and whether combat mutes it; " ++
    "repeat, radius and mute are saved with the map, but the game does not use them.";

/// `default_sound_name` when `names` knows it (a mod may not), else the
/// first name; null for an empty list.
pub fn defaultSoundName(names: []const []const u8) ?[]const u8 {
    for (names) |name| {
        if (std.ascii.eqlIgnoreCase(name, default_sound_name)) return name;
    }
    return if (names.len != 0) names[0] else null;
}

/// A message for the Sounds panel's own fields, shown next to them so an
/// impossible radius pair is visible before the bridge ever sees it (which
/// would otherwise only report it after the fact, in the status bar).
/// BkEditorAddSound/SetSound refuse the same thing themselves.
pub fn soundRadiusError(min_radius: i32, max_radius: i32) ?[]const u8 {
    if (min_radius > max_radius) return "minimum radius is above maximum";
    return null;
}

/// The object pictures cache's ordering and dedup rules (D-29), owned by
/// `kit.pictures_cache` (where it is also tested); re-exported here as a
/// type alias so MapEditor callers that spelled it `panels_logic.PictureQueue`
/// before S02/T04's extraction keep compiling.
pub const PictureQueue = core.kit.pictures_cache.PictureQueue;

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

/// The window title's M3 shape (D-34/PARITY F15), the MFC SetWindowTitle's
/// own fields (TemplateEditorFrame1.cpp:153-205): the map's name with its
/// extension, `*` when modified, the size in patches, and the mod key. A
/// never-saved map has no path, so `name` stands in (the New Map dialog's
/// own field; empty shows the plain editor name); `size_patches` is the
/// document's tiles / 16, null when no map is open; `mod_key` is empty with
/// no mod. The MFC's 133-character clipping is kept, so a long name cannot
/// push the size and the mod off the title.
pub fn formatTitleM3(
    buffer: []u8,
    name: []const u8,
    dirty: bool,
    read_only: bool,
    size_patches: ?[2]i32,
    mod_key: []const u8,
) [:0]const u8 {
    const plain = "Map Editor";
    if (name.len == 0 and size_patches == null) return std.fmt.bufPrintZ(buffer, plain, .{}) catch "";
    var tail: [96]u8 = undefined;
    var tail_len: usize = 0;
    const star = if (dirty) "*" else "";
    const suffix = if (read_only) " (read-only)" else "";
    if (size_patches) |size| {
        tail_len += (std.fmt.bufPrint(tail[tail_len..], " {d}x{d}", .{ size[0], size[1] }) catch return plainZero(buffer)).len;
    }
    if (mod_key.len != 0) {
        tail_len += (std.fmt.bufPrint(tail[tail_len..], " MOD: {s}", .{mod_key}) catch return plainZero(buffer)).len;
    }
    // The MFC's own budget (MAX_NAME_SIZE 133), star and read-only mark
    // included: clip the NAME, never the fields after it.
    const budget = if (133 > tail_len + star.len + suffix.len) 133 - tail_len - star.len - suffix.len else 0;
    const shown = if (name.len > budget) name[0..budget] else name;
    return std.fmt.bufPrintZ(buffer, plain ++ " - {s}{s}{s}{s}", .{ shown, star, suffix, tail[0..tail_len] }) catch
        plainZero(buffer);
}

fn plainZero(buffer: []u8) [:0]const u8 {
    return std.fmt.bufPrintZ(buffer, "Map Editor", .{}) catch "";
}

/// The status bar's VIS/SCRIPT coordinate line (M3, D-34/PARITY V6), the MFC
/// editor's own format (InputState.cpp:123-137): VIS in tiles, SCRIPT in AI
/// units, the MFC's own dashes when there is no point.
pub fn visScriptLine(buffer: []u8, vis: ?[3]f32, script: ?[2]i32) []const u8 {
    if (vis == null or script == null)
        return "VIS: (-, -, -), SCRIPT: (-, -)";
    const v = vis.?;
    const s = script.?;
    return std.fmt.bufPrint(buffer, "VIS: ({d:.2}, {d:.2}, {d:.2}), SCRIPT: ({d}, {d})", .{ v[0], v[1], v[2], s[0], s[1] }) catch
        "VIS: (-, -, -), SCRIPT: (-, -)";
}

/// The status bar's object line (M3, D-34/PARITY V6), the MFC editor's own
/// words (TemplateEditorFrame1.cpp:1219-1278): one object - name, Script
/// ID, position in AI tiles, and the box when it is known (the MFC's own
/// else-branch leaves it out when the stats had none); several - "N objects
/// selected"; none - "Name: no selected".
pub fn objectLine(buffer: []u8, selected_count: usize, name: []const u8, script_id: i32, pos_ai: ?[2]f32, box_cells: ?[2]i32) []const u8 {
    if (selected_count == 0) return "Name: no selected";
    if (selected_count > 1)
        return std.fmt.bufPrint(buffer, "{d} objects selected", .{selected_count}) catch "Name: no selected";
    const pos = pos_ai orelse
        return std.fmt.bufPrint(buffer, "Name: {s}, Script ID: {d}", .{ name, script_id }) catch "Name: no selected";
    if (box_cells) |box|
        return std.fmt.bufPrint(buffer, "Name: {s}, Script ID: {d}, Pos: [{d:.2}, {d:.2}], Box: [{d}, {d}]", .{ name, script_id, pos[0], pos[1], box[0], box[1] }) catch "Name: no selected";
    return std.fmt.bufPrint(buffer, "Name: {s}, Script ID: {d}, Pos: [{d:.2}, {d:.2}]", .{ name, script_id, pos[0], pos[1] }) catch "Name: no selected";
}

/// The selector's double circles (M3, D-25/PARITY L13): the two concentric
/// radii the selection layer draws around one selected object. The MFC's
/// marker scales with the object's screen footprint and never vanishes on a
/// small one, so the inner ring takes the footprint's half-diagonal (at
/// least the minimum) and the outer rides 3 screen pixels past it - a fat
/// enough pair to read as the MFC's at any zoom.
pub fn selectionCircles(footprint: [2]f32) [2]f32 {
    const half_diagonal = 0.5 * std.math.hypot(footprint[0], footprint[1]);
    const inner: f32 = @max(half_diagonal + 2.0, selection_circle_min);
    return .{ inner, inner + 3.0 };
}

pub const selection_circle_min: f32 = 6.0;

/// The Properties panel's per-kind field set (M3, D-26), from the catalogue's
/// game type: a building garrisons (units, Script ID, player, health), a
/// trench piece garrisons (units, Script ID), a unit or a squad carries the
/// full set (units, angle, Script ID, scenario unit, player, health, and the
/// squad its formation), and every other kind has no SEditorMApObject
/// manipulator - the MFC shows nothing for it either.
pub const PropKind = enum { building, trench, unit, squad, other };

pub fn propertyKind(game_type: i32) PropKind {
    return switch (game_type) {
        2 => .building, // SGVOGT_BUILDING
        4 => .trench, // SGVOGT_ENTRENCHMENT
        1 => .unit, // SGVOGT_UNIT
        15 => .squad, // SGVOGT_SQUAD
        else => .other,
    };
}

/// The Formation combo's labels, the MFC's own FORMATIONS_LABELS
/// (SEditorMApObject.cpp:286-292); the combo indexes the squad stats'
/// formations by type.
pub const formation_labels = [_][]const u8{ "DEFAULT", "MOVEMENT", "DEFENSIVE", "OFFENSIVE", "SNEAK" };

/// The Health field's percent clamp (the MFC's own 0.01..1.0 record range,
/// shown and typed as a percent): anything below 1% stands at 1%, anything
/// above 100% at 100%. The record keeps the fraction.
pub fn clampHealthPercent(percent: f32) f32 {
    if (!std.math.isFinite(percent)) return 100.0;
    return std.math.clamp(percent, 1.0, 100.0);
}

/// The Angle field's degrees-to-direction and back, the MFC's own pair
/// (SEditorMApObject.cpp:384-386 and :395-397): a degree turn into the
/// record's 65536 direction, rounded once, and the direction back to whole
/// degrees.
pub fn degreesToDirection(degrees: f32) i32 {
    if (!std.math.isFinite(degrees)) return 0;
    return @intFromFloat((degrees * 65536.0) / 360.0 + 0.5);
}

pub fn directionToDegrees(direction: i32) f32 {
    return @as(f32, @floatFromInt(direction)) * 360.0 / 65536.0 + 0.5;
}

/// The direction wheel's angle (M3, D-28/PARITY O6): the drag point against
/// the dial's centre, as the MFC CDirectionButton reads it -
/// `atan2(cy, cx)` with y up (DirectionButton.cpp:104-113) - as whole
/// degrees 0..359, 0 at east and turning counter-clockwise on screen like
/// the MFC's. Nothing here snaps: the MFC's wheel does not either. It is the
/// WHEEL's angle - the placer's placement angle follows it; what the wheel
/// turns the selection by is `wheelDeltaDegrees` of two of these.
pub fn wheelAngleDegrees(centre: [2]f32, point: [2]f32) i32 {
    const dx = point[0] - centre[0];
    const dy = centre[1] - point[1]; // y up, the MFC's own flip
    if (dx == 0 and dy == 0) return 0;
    var degrees = std.math.radiansToDegrees(std.math.atan2(dy, dx));
    if (degrees < 0) degrees += 360.0;
    // 359.6 rounds to 360, which is 0 again.
    return @mod(@as(i32, @intFromFloat(degrees + 0.5)), 360);
}

/// The turn from one wheel angle to another, in whole degrees, the short way
/// round: -179..180, positive counter-clockwise on screen (the wheel's sense).
/// The direction wheel turns the selection BY THIS (the user's ruling of
/// 2026-10-03, and the 05-04 plan's own text; not TO the angle as the MFC does):
/// a drag across the 359/0 seam is a turn of a degree or two, never of 359. A
/// half turn is +180.
pub fn wheelDeltaDegrees(from: i32, to: i32) i32 {
    const turn = @mod(to - from, 360);
    return if (turn > 180) turn - 360 else turn;
}

/// The wheel's angle for a placement direction (a turn is 65536): whole
/// degrees 0..359, rounded - the inverse of `degreesToDirection` on the
/// wheel's own whole degrees, and the nearest degree for a direction the
/// Q and E keys left between two.
pub fn wheelDegreesOfDirection(direction: i32) i32 {
    const degrees: f32 = @as(f32, @floatFromInt(@mod(direction, 65536))) * 360.0 / 65536.0;
    return @mod(@as(i32, @intFromFloat(@round(degrees))), 360);
}

const normalizeForCompare = kit.shipped.normalizeForCompare;
const isAbsolutePath = kit.shipped.isAbsolutePath;

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
pub fn isShippedMap(engine_path: []const u8, base_root: []const u8, files: ?kit.files.Files) bool {
    return kit.shipped.isShipped(engine_path, base_root, files);
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
pub fn needsSaveAs(doc_path: []const u8, base_root: []const u8, user_root: []const u8, files: ?kit.files.Files) bool {
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

/// Where the Open and Save As dialogs start (panels.zig's dialogFolder): the
/// Settings window's maps folder when one is set (D-25), otherwise
/// `defaultMapsFolder`. The maps folder is typed text: a relative one is
/// taken under `user_root`, never the working directory, which a shortcut or
/// the Start menu sets to anything.
pub fn dialogFolderFor(buffer: []u8, custom_folder: []const u8, user_root: []const u8, mod_folder: ?[]const u8) ?[]const u8 {
    if (custom_folder.len == 0) return defaultMapsFolder(buffer, user_root, mod_folder);
    if (std.fs.path.isAbsolute(custom_folder) or user_root.len == 0) return custom_folder;
    return std.fmt.bufPrint(buffer, "{s}{s}", .{ user_root, custom_folder }) catch null;
}

/// The last component of a path written with either separator: the engine
/// uses backslashes on every OS, a file dialog the OS's own.
pub fn baseName(path: []const u8) []const u8 {
    const cut = std.mem.lastIndexOfAny(u8, path, "/\\") orelse return path;
    return path[cut + 1 ..];
}

/// A path as the bridge takes it: an OS path written with the engine's
/// separator, because OpenFileStream splits on backslash only (bridge.h,
/// BkEditorOpenMap). A save path without .bzm or .xml gets the default
/// format's extension (D-24, M3: the setting the MFC Options field held),
/// since the bridge picks the format from the extension. Null when it does
/// not fit.
pub fn enginePath(buffer: []u8, os_path: []const u8, kind: DialogKind, default_format: core.settings.Format) ?[]const u8 {
    const needs_extension = kind == .save_as and !hasMapExtension(os_path);
    const extension = if (needs_extension) core.settings.formatExtension(default_format) else "";
    if (os_path.len + extension.len > buffer.len) return null;
    for (os_path, buffer[0..os_path.len]) |char, *out| out.* = if (char == '/') '\\' else char;
    @memcpy(buffer[os_path.len..][0..extension.len], extension);
    return buffer[0..os_path.len + extension.len];
}

/// A path typed on the command line, made absolute against the directory the
/// editor was launched from (`cwd`) - the one place a relative path means the
/// launch directory. Absolute from here on, so the document path, Open Recent,
/// the recovery sidecar and the shipped-map check never depend on the working
/// directory again. `typed` may use the engine's backslashes on any OS (the
/// build's own tiers pass `Data\Maps\...`); on POSIX they become '/' first,
/// so "." and ".." components resolve. Null when the result does not fit
/// `buffer`.
pub fn absoluteFromLaunchDir(buffer: []u8, cwd: []const u8, typed: []const u8) ?[]const u8 {
    var typed_buffer: [PathSlot.max_path]u8 = undefined;
    if (typed.len > typed_buffer.len) return null;
    for (typed, typed_buffer[0..typed.len]) |char, *out| out.* = if (builtin.os.tag != .windows and char == '\\') '/' else char;
    var scratch: [4 * PathSlot.max_path]u8 = undefined;
    var fixed: std.heap.FixedBufferAllocator = .init(&scratch);
    const resolved = std.fs.path.resolve(fixed.allocator(), &.{ cwd, typed_buffer[0..typed.len] }) catch return null;
    if (resolved.len > buffer.len) return null;
    @memcpy(buffer[0..resolved.len], resolved);
    return buffer[0..resolved.len];
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

/// File > New's fields (M3, D-23): the MFC dialog's own four - sizes in
/// patches per axis, the season (0 Summer .. 3 Spring, the dialog's own
/// numbering; the bridge maps Spring onto the Summer the map stores), the
/// name, and the mod ("" keeps the current one, "none" is none, anything
/// else a bare installed folder). A plain value: it travels through
/// `Pending` by copy, like every other guarded action.
pub const NewMapFields = struct {
    size_x: i32 = 8,
    size_y: i32 = 8,
    season: i32 = 0,
    name: NameText = .{},
    mod_folder: NameText = .{},

    pub const season_names = [4][]const u8{ "Summer", "Winter", "Africa", "Spring" };

    pub fn seasonName(self: *const NewMapFields) []const u8 {
        return season_names[@intCast(@min(self.season, 3))];
    }

    /// The dialog's own bounds (M3, D-23): 1..32 patches per axis, the
    /// season 0..3. False - with the offending fields clamped back - when
    /// anything was outside, so a caller can both correct and report.
    pub fn clampToValid(self: *NewMapFields) bool {
        var ok = true;
        if (self.size_x < 1) {
            self.size_x = 1;
            ok = false;
        }
        if (self.size_x > 32) {
            self.size_x = 32;
            ok = false;
        }
        if (self.size_y < 1) {
            self.size_y = 1;
            ok = false;
        }
        if (self.size_y > 32) {
            self.size_y = 32;
            ok = false;
        }
        if (self.season < 0) {
            self.season = 0;
            ok = false;
        }
        if (self.season > 3) {
            self.season = 3;
            ok = false;
        }
        return ok;
    }

    /// `WxH:season[:name][:mod]`, the `map_new` command's own format
    /// (commands.zig documents it; the auto grammar allows no space, and a
    /// comma would end the schedule entry). The season is summer/winter/
    /// africa/spring by name. Null when the text does not parse.
    pub fn parse(arg: []const u8) ?NewMapFields {
        var fields: NewMapFields = .{};
        var parts = std.mem.splitScalar(u8, arg, ':');
        const size = parts.next() orelse return null;
        const x_mark = std.mem.indexOfScalar(u8, size, 'x') orelse return null;
        fields.size_x = std.fmt.parseInt(i32, size[0..x_mark], 10) catch return null;
        fields.size_y = std.fmt.parseInt(i32, size[x_mark + 1 ..], 10) catch return null;
        const season = parts.next() orelse return null;
        fields.season = for (season_names, 0..) |name, index| {
            if (std.ascii.eqlIgnoreCase(name, season)) break @intCast(index);
        } else return null;
        if (parts.next()) |name| fields.name.set(name);
        if (parts.next()) |mod_folder| fields.mod_folder.set(mod_folder);
        if (parts.next() != null) return null;
        if (!fields.clampToValid()) return null;
        return fields;
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
    /// File > New (M3, D-23): the map to build once the prompt, if any,
    /// has been answered.
    new_map: NewMapFields,
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
    /// File > New (M3, D-23), guarded through the same unsaved-changes prompt
    /// as Open; the caller builds the map once `next()` returns `.new_map`.
    new_map_requested: ?NewMapFields = null,
    /// `stepForPending`'s own copy of a `new_map` Pending's fields, for the
    /// same use-after-return reason `open_path_scratch` exists.
    new_map_scratch: NewMapFields = .{},

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
        /// File > New (M3, D-23): build this map (the prompt, if any, has
        /// been answered already).
        new_map: NewMapFields,
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
            .new_map => |fields| blk: {
                // See open_path_scratch's own doc comment - the same
                // use-after-return reason, for a value that travels by copy.
                self.new_map_scratch = fields;
                break :blk Step{ .new_map = self.new_map_scratch };
            },
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

    /// File > New (M3, D-23): guards the map's fields through the same
    /// unsaved-changes prompt Open itself uses; the caller builds the map
    /// once `next()` returns `.new_map`.
    pub fn requestNewMap(self: *FileActions, fields: NewMapFields) void {
        self.new_map_requested = fields;
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
        if (self.new_map_requested) |fields| {
            self.new_map_requested = null;
            return switch (self.prompt.guard(dirty, .{ .new_map = fields })) {
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

/// Test in game (D-20): copies the map's script beside the test copy, where the
/// game looks for it (`script_file.copyForTest`: overwriting there, never
/// asking - the test folder is ours). `test_map_path` is what
/// BkEditorTestMapPath answered. Null when there is nothing to say (no script,
/// or it was copied); otherwise a warning for the status bar, formatted into
/// `note` - a missing script is a warning, never an error, and the launch goes
/// on. Reads the script's name straight from the bridge, so the status line the
/// last edit left is not touched.
pub fn copyScriptForTest(editor: *Editor, test_map_path: []const u8, note: []u8) ?[]const u8 {
    const files = editor.files orelse return null;
    var value: core.records.Value = undefined;
    if (editor.bridge.readRecord(.script_file, 0, editor.allocator, &value) != .ok) return null;
    defer value.deinit(editor.allocator);
    const name = value.script_file.nameSlice();
    if (name.len == 0) return null;
    const test_dir = kit.script_file.directoryOf(test_map_path);
    return switch (kit.script_file.copyForTest(files, editor.baseRoot(), editor.document.path.items, test_dir, name)) {
        .copied => null,
        .missing => std.fmt.bufPrint(note, "the script {s}.lua is not beside the map; the test game runs without it", .{name}) catch "the script is not beside the map",
        .failed => std.fmt.bufPrint(note, "the script {s}.lua could not be copied for the test: {s}", .{ name, files.lastError() }) catch "the script could not be copied",
        .not_a_bare_name => std.fmt.bufPrint(note, "the script name \"{s}\" is not a plain name, so it was not copied", .{name}) catch "the script was not copied",
        .shipped => "the test map's folder is inside a game's data folder, which is never written; the script was not copied",
    };
}

/// How one Test-in-game launch ended (`TestLaunchPrompt.noteLaunch`).
pub const LaunchAttempt = union(enum) {
    /// Failed before the game was asked to start: no test map path, the
    /// test copy would not save, the log's path too long. The reason.
    status_failure: []const u8,
    /// The game could not be started: none beside the editor, or the spawn
    /// failed. The modal's message.
    report_failure: []const u8,
    /// The game is running.
    started,
    /// The game is running, and there is something to say about the launch that
    /// is not a failure: the map's script was not beside the map to copy (04-10,
    /// D-20). Shown on the status bar as a warning; nothing stops.
    started_with_note: []const u8,
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
    /// new one immediately. That exit is the one Restart asked for
    /// (`Running.terminate`: TerminateProcess's code 1 on Windows, SIGTERM
    /// or SIGKILL on POSIX), so it is expected and never reported, whatever
    /// its shape. Any other non-clean exit (describe() != .clean) is kept
    /// as a report until acknowledgeReport(); a clean one reports nothing,
    /// including the D-06 "Keep it running" case, since the prompt was
    /// never re-asked for that game.
    pub fn gameExited(self: *TestLaunchPrompt, exit: testlaunch.Exit, log_path: []const u8) Step {
        const was_restarting = self.state == .restarting;
        self.state = .idle;
        if (was_restarting) return .start;
        if (testlaunch.describe(exit) != .clean) {
            self.setReport(exit, log_path);
            self.state = .reporting;
        }
        return .none;
    }

    /// The failure report to show, or null when there is none pending.
    pub fn report(self: *const TestLaunchPrompt) ?[]const u8 {
        return if (self.state == .reporting) self.report_buffer[0..self.report_len] else null;
    }

    /// The caller has shown the report (an OK on its modal).
    pub fn acknowledgeReport(self: *TestLaunchPrompt) void {
        if (self.state == .reporting) self.state = .idle;
    }

    /// Every way one Test-in-game launch can end (panels.zig's
    /// startTestGame), and what it leaves on the status line
    /// (WINDOWS.md 1: a later success used to leave an earlier failure's
    /// "test in game: ..." there for good). A failure before the game was
    /// asked to start is shown on the status line, tagged as Test in game's
    /// own. A spawn failure is reported in the modal instead, and a start
    /// is a success: both clear that tagged line, and only it, so another
    /// operation's message on the status line survives.
    pub fn noteLaunch(self: *TestLaunchPrompt, attempt: LaunchAttempt, status: *view_math.StatusSlot) void {
        switch (attempt) {
            .status_failure => |reason| status.set(.test_launch, "test in game: ", reason),
            .report_failure => |message| {
                status.clearFrom(.test_launch);
                self.reportFailure(message);
            },
            .started => status.clearFrom(.test_launch),
            .started_with_note => |note| status.set(.test_launch, "test in game: ", note),
        }
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
pub fn actOnPath(editor: *Editor, kind: DialogKind, os_path: []const u8, default_format: core.settings.Format) EditError!void {
    var buffer: [PathSlot.max_path + 4]u8 = undefined;
    const path = enginePath(&buffer, os_path, kind, default_format) orelse return error.Failed;
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

test "the palette leaves out the types a map cannot hold and the pieces that need their tool" {
    try std.testing.expect(!isPlaceable(100));
    try std.testing.expect(!isPlaceable(5));
    // D-05: a trench piece (4), a bridge span (6) and a fence (9) are drawn
    // with their own tool in M2, never placed one by one from the palette.
    try std.testing.expect(!isPlaceable(4));
    try std.testing.expect(!isPlaceable(6));
    try std.testing.expect(!isPlaceable(9));
    // A unit (1), a building (2) and the rest stay in the palette.
    for ([_]i32{ 1, 2, 3, 7, 8, 10, 15, 17 }) |game_type| try std.testing.expect(isPlaceable(game_type));
}

test "the palette's object filters: collection from the combo and the checked slots" {
    var buildings = core.bridge.ObjectFilter{};
    buildings.setName("Buildings");
    var squads = core.bridge.ObjectFilter{};
    squads.setName("Squads");
    const filters = [_]core.bridge.ObjectFilter{ buildings, squads };

    var slots: [core.settings.Settings.filter_slot_count][]const u8 = @splat("");
    var checked: [core.settings.Settings.filter_slot_count]bool = @splat(false);
    var out: [max_active_filters]usize = undefined;

    // Nothing named, nothing checked: an empty active set - everything shows.
    try std.testing.expectEqual(@as(usize, 0), collectActiveFilterIndices("", slots, checked, &filters, &out));
    // The combo alone.
    try std.testing.expectEqual(@as(usize, 1), collectActiveFilterIndices("Buildings", slots, checked, &filters, &out));
    try std.testing.expectEqual(@as(usize, 0), out[0]);
    // A checked slot adds its filter; an unchecked or unknown-named one adds
    // nothing.
    slots[0] = "Buildings";
    checked[0] = true;
    slots[2] = "Squads";
    checked[2] = true;
    slots[3] = "Gone";
    checked[3] = true;
    checked[4] = true; // slot 4 is empty
    try std.testing.expectEqual(@as(usize, 2), collectActiveFilterIndices("", slots, checked, &filters, &out));
    try std.testing.expectEqual(@as(usize, 0), out[0]);
    try std.testing.expectEqual(@as(usize, 1), out[1]);
    // An unchecked slot contributes nothing: unchecking Buildings leaves Squads.
    checked[0] = false;
    try std.testing.expectEqual(@as(usize, 1), collectActiveFilterIndices("", slots, checked, &filters, &out));
    try std.testing.expectEqual(@as(usize, 1), out[0]);
    checked[0] = true;
    // The combo naming a checked slot's own filter is not collected twice.
    try std.testing.expectEqual(@as(usize, 2), collectActiveFilterIndices("Squads", slots, checked, &filters, &out));
    try std.testing.expectEqual(@as(usize, 1), out[0]);
    try std.testing.expectEqual(@as(usize, 0), out[1]);
}

test "the palette's object filters gate the query: none active shows everything" {
    var buildings = core.bridge.ObjectFilter{};
    buildings.setName("Buildings");
    buildings.list_count = 1;
    buildings.lists[0].word_count = 1;
    @memcpy(buildings.lists[0].words[0][0.."buildings".len], "buildings");
    var squads = core.bridge.ObjectFilter{};
    squads.setName("Squads");
    squads.list_count = 1;
    squads.lists[0].word_count = 1;
    @memcpy(squads.lists[0].words[0][0.."squads".len], "squads");

    // The records' borrowed views: the scratch must outlive the filters, as
    // the palette's refreshed cache does.
    var scratch: [2]core.bridge.FilterView = .{ .{}, .{} };
    const active = [_]core.filters.Filter{ buildings.view(&scratch[0]), squads.view(&scratch[1]) };

    // With none active, everything the text filter passes shows.
    try std.testing.expect(paletteObjectVisible("10_5_cm_Flak38", "units\\technics\\", "", &.{}));
    // A buildings-folder object passes; a units-folder one does not. The
    // filters read the path; the text filter reads the name.
    try std.testing.expect(paletteObjectVisible("A_Cisterns", "buildings\\africa\\summer\\a_cisterns\\", "", &active));
    try std.testing.expect(paletteObjectVisible("gb_bren_43", "squads\\gb_bren_43\\", "", &active));
    try std.testing.expect(!paletteObjectVisible("T34", "units\\technics\\ussr\\", "", &active));
    // The text filter is ANDed on top.
    try std.testing.expect(!paletteObjectVisible("A_Cisterns", "buildings\\africa\\summer\\a_cisterns\\", "t34", &active));
    try std.testing.expect(paletteObjectVisible("A_Cisterns", "buildings\\africa\\summer\\a_cisterns\\", "cist", &active));
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

test "defaultSoundName: the rivers' loop when known, else the first name, else none" {
    const with_default = [_][]const u8{ "20mm_aviacannon", "Amb_Field", "amb_water_circle", "Zis_5" };
    try std.testing.expectEqualStrings("amb_water_circle", defaultSoundName(&with_default).?);
    const without_default = [_][]const u8{ "20mm_aviacannon", "Amb_Field" };
    try std.testing.expectEqualStrings("20mm_aviacannon", defaultSoundName(&without_default).?);
    try std.testing.expect(defaultSoundName(&.{}) == null);
}

test "soundRadiusError names a min above max, and nothing else" {
    try std.testing.expect(soundRadiusError(5, 1) != null);
    try std.testing.expect(soundRadiusError(1, 5) == null);
    try std.testing.expect(soundRadiusError(3, 3) == null);
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

test "formatTitleM3: the MFC's own fields - name, star, patches, mod (F15)" {
    var buffer: [320:0]u8 = undefined;
    // No map at all: the plain editor name.
    try std.testing.expectEqualStrings("Map Editor", formatTitleM3(&buffer, "", false, false, null, ""));
    // A saved map: name with extension, size in patches.
    try std.testing.expectEqualStrings(
        "Map Editor - coldwinter.bzm 16x16",
        formatTitleM3(&buffer, "coldwinter.bzm", false, false, .{ 16, 16 }, ""),
    );
    // Modified, with a mod key.
    try std.testing.expectEqualStrings(
        "Map Editor - mine.xml* 8x12 MOD: AP2",
        formatTitleM3(&buffer, "mine.xml", true, false, .{ 8, 12 }, "AP2"),
    );
    // A never-saved map: the New Map dialog's name stands in for a path.
    try std.testing.expectEqualStrings(
        "Map Editor - M3Auto 8x8",
        formatTitleM3(&buffer, "M3Auto", false, false, .{ 8, 8 }, ""),
    );
    // Read-only keeps its mark, after the star like the MFC's own order.
    try std.testing.expectEqualStrings(
        "Map Editor - coldwinter.bzm* (read-only) 16x16",
        formatTitleM3(&buffer, "coldwinter.bzm", true, true, .{ 16, 16 }, ""),
    );
    // A long name is clipped - never the fields after it (the MFC's own
    // 133-character budget).
    const long_name = "a" ** 200;
    const titled = formatTitleM3(&buffer, long_name, false, false, .{ 16, 16 }, "AP2");
    try std.testing.expect(std.mem.indexOf(u8, titled, " 16x16 MOD: AP2") != null);
    try std.testing.expect(titled.len < 160);
}

test "visScriptLine: values and the MFC's own dashes (V6)" {
    var buffer: [96]u8 = undefined;
    try std.testing.expectEqualStrings(
        "VIS: (12.00, 34.50, 0.00), SCRIPT: (7, -3)",
        visScriptLine(&buffer, .{ 12.0, 34.5, 0.0 }, .{ 7, -3 }),
    );
    try std.testing.expectEqualStrings(
        "VIS: (-, -, -), SCRIPT: (-, -)",
        visScriptLine(&buffer, null, null),
    );
}

test "objectLine: one object, many objects, none (V6)" {
    var buffer: [192]u8 = undefined;
    try std.testing.expectEqualStrings("Name: no selected", objectLine(&buffer, 0, "", -1, null, null));
    try std.testing.expectEqualStrings("3 objects selected", objectLine(&buffer, 3, "", -1, null, null));
    // One, without a position known: the name and Script ID alone.
    try std.testing.expectEqualStrings(
        "Name: T34, Script ID: 4244",
        objectLine(&buffer, 1, "T34", 4244, null, null),
    );
    // One with a position (map/AI units) but no box: the MFC's own
    // else-branch leaves the box out.
    try std.testing.expectEqualStrings(
        "Name: T34, Script ID: 4244, Pos: [12.00, 40.50]",
        objectLine(&buffer, 1, "T34", 4244, .{ 12.0, 40.5 }, null),
    );
    // The box when a later record carries one.
    try std.testing.expectEqualStrings(
        "Name: T34, Script ID: 4244, Pos: [12.00, 40.50], Box: [2, 3]",
        objectLine(&buffer, 1, "T34", 4244, .{ 12.0, 40.5 }, .{ 2, 3 }),
    );
}

test "selection circles: the inner ring rides the footprint, the outer follows, a small object never vanishes" {
    // A tiny object takes the minimum pair; the rings are 3 pixels apart.
    const tiny = selectionCircles(.{ 2, 2 });
    try std.testing.expectEqual(selection_circle_min, tiny[0]);
    try std.testing.expectEqual(selection_circle_min + 3.0, tiny[1]);
    // A bigger one rides the footprint's half-diagonal.
    const big = selectionCircles(.{ 40, 30 });
    try std.testing.expectApproxEqAbs(@as(f32, 0.5 * 50.0 + 2.0), big[0], 0.01);
    try std.testing.expectApproxEqAbs(big[0] + 3.0, big[1], 0.001);
}

test "property kinds: the catalogue's game types answer their MFC manipulator's set" {
    try std.testing.expectEqual(PropKind.building, propertyKind(2));
    try std.testing.expectEqual(PropKind.trench, propertyKind(4));
    try std.testing.expectEqual(PropKind.unit, propertyKind(1));
    try std.testing.expectEqual(PropKind.squad, propertyKind(15));
    // A flag, a fence, a mine - the kinds with no manipulator show none.
    try std.testing.expectEqual(PropKind.other, propertyKind(17));
    try std.testing.expectEqual(PropKind.other, propertyKind(9));
    try std.testing.expectEqual(PropKind.other, propertyKind(0));
}

test "health percent clamps to the MFC's own 1..100, non-finite stands at 100" {
    try std.testing.expectEqual(@as(f32, 1.0), clampHealthPercent(0.0));
    try std.testing.expectEqual(@as(f32, 1.0), clampHealthPercent(-5.0));
    try std.testing.expectEqual(@as(f32, 43.5), clampHealthPercent(43.5));
    try std.testing.expectEqual(@as(f32, 100.0), clampHealthPercent(140.0));
    try std.testing.expectEqual(@as(f32, 100.0), clampHealthPercent(std.math.nan(f32)));
}

test "angle: the MFC's degrees-to-direction pair turns and reads back" {
    try std.testing.expectEqual(@as(i32, 0), degreesToDirection(0));
    try std.testing.expectEqual(@as(i32, 65536 / 4), degreesToDirection(90));
    try std.testing.expectEqual(@as(i32, 65536 / 2), degreesToDirection(180));
    // The read-back rounds to whole degrees, the MFC's own +0.5.
    try std.testing.expectEqual(@as(f32, 90.5), directionToDegrees(65536 / 4));
    try std.testing.expectEqual(@as(i32, 0), degreesToDirection(std.math.nan(f32)));
}

test "the direction wheel reads the drag angle the MFC's way: y up, whole degrees" {
    // East, north, west, south - the MFC's atan2(cy, cx) with y up.
    try std.testing.expectEqual(@as(i32, 0), wheelAngleDegrees(.{ 50, 50 }, .{ 60, 50 }));
    try std.testing.expectEqual(@as(i32, 90), wheelAngleDegrees(.{ 50, 50 }, .{ 50, 40 }));
    try std.testing.expectEqual(@as(i32, 180), wheelAngleDegrees(.{ 50, 50 }, .{ 40, 50 }));
    try std.testing.expectEqual(@as(i32, 270), wheelAngleDegrees(.{ 50, 50 }, .{ 50, 60 }));
    // The diagonal and the dead centre.
    try std.testing.expectEqual(@as(i32, 45), wheelAngleDegrees(.{ 50, 50 }, .{ 60, 40 }));
    try std.testing.expectEqual(@as(i32, 0), wheelAngleDegrees(.{ 50, 50 }, .{ 50, 50 }));
    // Just below east rounds up to 360, which is 0: never out of 0..359.
    try std.testing.expectEqual(@as(i32, 0), wheelAngleDegrees(.{ 50, 50 }, .{ 150, 50.5 }));
}

test "the direction wheel turns by the delta: the short way round, across the seam, a half turn is +180" {
    try std.testing.expectEqual(@as(i32, 0), wheelDeltaDegrees(90, 90));
    try std.testing.expectEqual(@as(i32, 90), wheelDeltaDegrees(0, 90));
    try std.testing.expectEqual(@as(i32, -90), wheelDeltaDegrees(90, 0));
    // Across the 359/0 seam: a degree or two, never a near-full turn.
    try std.testing.expectEqual(@as(i32, 2), wheelDeltaDegrees(359, 1));
    try std.testing.expectEqual(@as(i32, -2), wheelDeltaDegrees(1, 359));
    try std.testing.expectEqual(@as(i32, 180), wheelDeltaDegrees(0, 180));
    try std.testing.expectEqual(@as(i32, 180), wheelDeltaDegrees(180, 0));
    try std.testing.expectEqual(@as(i32, -179), wheelDeltaDegrees(0, 181));
    // The frames of a drag add up to the turn since the grab: 350 -> 10 -> 30 -> 350 is 0.
    const frames = [_]i32{ 350, 10, 30, 350 };
    var turned: i32 = 0;
    for (frames[1..], 0..) |angle, index| turned += wheelDeltaDegrees(frames[index], angle);
    try std.testing.expectEqual(@as(i32, 0), turned);
    // A drag a full circle round adds up to 360 when taken in steps under a half turn.
    var circle: i32 = 0;
    var angle: i32 = 0;
    for (0..12) |_| {
        const next = @mod(angle + 30, 360);
        circle += wheelDeltaDegrees(angle, next);
        angle = next;
    }
    try std.testing.expectEqual(@as(i32, 360), circle);
}

test "the wheel's angle of a placement direction is the whole degree it was set from" {
    // degreesToDirection and back, on every whole degree the wheel reads.
    var degrees: i32 = 0;
    while (degrees < 360) : (degrees += 1) {
        try std.testing.expectEqual(degrees, wheelDegreesOfDirection(degreesToDirection(@floatFromInt(degrees))));
    }
    // A direction between two degrees reads as the nearest, and one past a turn wraps.
    try std.testing.expectEqual(@as(i32, 0), wheelDegreesOfDirection(0));
    try std.testing.expectEqual(@as(i32, 90), wheelDegreesOfDirection(65536 / 4));
    try std.testing.expectEqual(@as(i32, 0), wheelDegreesOfDirection(65535));
    try std.testing.expectEqual(@as(i32, 90), wheelDegreesOfDirection(65536 + 65536 / 4));
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
    var fake = kit.files.FakeFiles.init(std.testing.allocator);
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
    try std.testing.expectEqual(kit.autosave.Target.recovery_copy, kit.autosave.target(needsSaveAs(other, base, user_root, files)));
    // The user's own folders stay writable, and autosave into themselves.
    try std.testing.expect(!needsSaveAs(user_root ++ "maps/mine.bzm", base, user_root, files));
    try std.testing.expect(!needsSaveAs(user_root ++ "mods/MyMod/maps/mine.bzm", base, user_root, files));
    try std.testing.expectEqual(kit.autosave.Target.map_file, kit.autosave.target(needsSaveAs(user_root ++ "maps/mine.bzm", base, user_root, files)));
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
    try std.testing.expectEqualStrings("\\Users\\me\\a.bzm", enginePath(&buffer, "/Users/me/a.bzm", .open, .bzm).?);
    try std.testing.expectEqualStrings("C:\\maps\\a.xml", enginePath(&buffer, "C:\\maps\\a.xml", .save_as, .bzm).?);
    try std.testing.expectEqualStrings("\\tmp\\b.bzm", enginePath(&buffer, "/tmp/b", .save_as, .bzm).?);
    try std.testing.expectEqualStrings("\\tmp\\b", enginePath(&buffer, "/tmp/b", .open, .bzm).?);
    var tiny: [4]u8 = undefined;
    try std.testing.expect(enginePath(&tiny, "/tmp/b", .open, .bzm) == null);
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
    var fake_files = kit.files.FakeFiles.init(std.testing.allocator);
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
    try actOnPath(&editor, step.act_on_path.kind, step.act_on_path.path, .bzm);
    try std.testing.expectEqualStrings("\\maps\\fixture.bzm", editor.document.path.items);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(false, false));

    // Save As, then a plain Save to the path it chose.
    actions.save_as_requested = true;
    try std.testing.expectEqual(FileActions.Step{ .show_dialog = .save_as }, actions.next(false, false));
    actions.dialog.deliver("/maps/copy");
    const save_step = actions.next(false, false);
    try actOnPath(&editor, save_step.act_on_path.kind, save_step.act_on_path.path, .bzm);
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
    try actOnPath(&editor, step.act_on_path.kind, step.act_on_path.path, .bzm);
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

test "TestLaunchPrompt: Restart waits for the terminated game's exit, then starts the new one without reporting it" {
    // The exits Running.terminate really produces: TerminateProcess's code
    // on Windows, SIGTERM - or SIGKILL five seconds on - on POSIX, early or
    // late in the game's life. describe() calls each a failure.
    const terminated = [_]testlaunch.Exit{
        .{ .code = testlaunch.windows_terminate_exit_code, .signal = null, .lifetime_ms = 30_000 },
        .{ .code = testlaunch.windows_terminate_exit_code, .signal = null, .lifetime_ms = 800 },
        .{ .code = null, .signal = testlaunch.posix_terminate_signal, .lifetime_ms = 30_000 },
        .{ .code = null, .signal = testlaunch.posix_kill_signal, .lifetime_ms = 35_000 },
    };
    for (terminated) |exit| {
        try std.testing.expect(testlaunch.describe(exit) != .clean);
        var prompt: TestLaunchPrompt = .{};
        try std.testing.expectEqual(TestLaunchPrompt.Step.ask_restart, prompt.request(true));
        prompt.answer(.restart);
        try std.testing.expectEqual(TestLaunchPrompt.Step.start, prompt.gameExited(exit, "log"));
        try std.testing.expect(prompt.report() == null);
        try std.testing.expect(!prompt.isAskingRestart());
        // The new game, once it runs, is watched like any other: its own
        // crash is reported.
        try std.testing.expectEqual(TestLaunchPrompt.Step.none, prompt.gameExited(.{ .code = null, .signal = 11, .lifetime_ms = 30_000 }, "log"));
        try std.testing.expect(prompt.report() != null);
    }
}

test "TestLaunchPrompt: a kept game that dies later is still reported" {
    var prompt: TestLaunchPrompt = .{};
    _ = prompt.request(true);
    prompt.answer(.keep);
    try std.testing.expectEqual(TestLaunchPrompt.Step.none, prompt.gameExited(.{ .code = testlaunch.windows_terminate_exit_code, .signal = null, .lifetime_ms = 60_000 }, "log"));
    const message = prompt.report() orelse return error.TestUnexpectedResult;
    try std.testing.expect(std.mem.indexOf(u8, message, "code 1") != null);
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

test "Test in game status: a failure is shown, and a later successful launch clears it (WINDOWS.md 1)" {
    var prompt: TestLaunchPrompt = .{};
    var status: view_math.StatusSlot = .{};
    try std.testing.expectEqual(TestLaunchPrompt.Step.start, prompt.request(false));
    prompt.noteLaunch(.{ .status_failure = "the test copy would not save" }, &status);
    try std.testing.expectEqualStrings("test in game: the test copy would not save", status.line());
    try std.testing.expect(prompt.report() == null);

    try std.testing.expectEqual(TestLaunchPrompt.Step.start, prompt.request(false));
    prompt.noteLaunch(.started, &status);
    try std.testing.expectEqualStrings("", status.line());
    try std.testing.expect(prompt.report() == null);
}

test "Test in game status: a launch with a note shows it as a warning, and the next clean launch clears it" {
    var prompt: TestLaunchPrompt = .{};
    var status: view_math.StatusSlot = .{};
    prompt.noteLaunch(.{ .started_with_note = "the script m2_script.lua is not beside the map" }, &status);
    try std.testing.expectEqualStrings("test in game: the script m2_script.lua is not beside the map", status.line());
    try std.testing.expect(prompt.report() == null);
    prompt.noteLaunch(.started, &status);
    try std.testing.expectEqualStrings("", status.line());
}

test "copyScriptForTest: the script goes beside the test map, a missing one is a warning, no script says nothing" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var files = kit.files.FakeFiles.init(std.testing.allocator);
    defer files.deinit();
    fake.setScriptFileFixture("m2_script");
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    editor.files = files.files();
    try editor.open("maps\\mine\\a.bzm");
    // The keys the copy uses, built the way it builds them.
    var from_buffer: [128]u8 = undefined;
    var to_buffer: [128]u8 = undefined;
    const from_key = kit.script_file.scriptPathBeside(&from_buffer, "maps\\mine\\a.bzm", "m2_script").?;
    const to_key = kit.script_file.scriptPathIn(&to_buffer, "gen\\maps", "m2_script").?;
    const test_map = "gen\\maps\\mapeditor_test.bzm";
    var note: [256]u8 = undefined;
    // Nothing beside the map yet: a warning that names the script.
    const missing = copyScriptForTest(&editor, test_map, &note) orelse return error.TestUnexpectedResult;
    try std.testing.expect(std.mem.indexOf(u8, missing, "m2_script.lua") != null);
    try std.testing.expect(files.contents(to_key) == null);
    // With the file there it is copied and nothing is said.
    try files.write(from_key, "function Init() end");
    try std.testing.expect(copyScriptForTest(&editor, test_map, &note) == null);
    try std.testing.expectEqualStrings("function Init() end", files.contents(to_key).?);
    // None copies nothing.
    try editor.setScriptFile("");
    const ops = files.op_log.items.len;
    try std.testing.expect(copyScriptForTest(&editor, test_map, &note) == null);
    try std.testing.expectEqual(ops, files.op_log.items.len);
}

test "Test in game status: each pre-start failure replaces the last one's reason" {
    var prompt: TestLaunchPrompt = .{};
    var status: view_math.StatusSlot = .{};
    prompt.noteLaunch(.{ .status_failure = "no test map path" }, &status);
    prompt.noteLaunch(.{ .status_failure = "the test log's path is too long" }, &status);
    try std.testing.expectEqualStrings("test in game: the test log's path is too long", status.line());
}

test "Test in game status: a spawn failure goes to the modal and takes the stale status line with it" {
    var prompt: TestLaunchPrompt = .{};
    var status: view_math.StatusSlot = .{};
    prompt.noteLaunch(.{ .status_failure = "the test copy would not save" }, &status);
    prompt.noteLaunch(.{ .report_failure = "No game beside the Map Editor at /stage/Game" }, &status);
    try std.testing.expectEqualStrings("", status.line());
    const message = prompt.report() orelse return error.TestUnexpectedResult;
    try std.testing.expectEqualStrings("No game beside the Map Editor at /stage/Game", message);
    prompt.acknowledgeReport();

    // The next launch starts: nothing on the status line, no report.
    try std.testing.expectEqual(TestLaunchPrompt.Step.start, prompt.request(false));
    prompt.noteLaunch(.started, &status);
    try std.testing.expectEqualStrings("", status.line());
    try std.testing.expect(prompt.report() == null);
}

test "Test in game status: a success clears only its own line, never another operation's message" {
    var prompt: TestLaunchPrompt = .{};
    var status: view_math.StatusSlot = .{};
    status.set(.general, "autosave failed: ", "disk full");
    prompt.noteLaunch(.started, &status);
    try std.testing.expectEqualStrings("autosave failed: disk full", status.line());
    prompt.noteLaunch(.{ .report_failure = "the game would not start: AccessDenied" }, &status);
    try std.testing.expectEqualStrings("autosave failed: disk full", status.line());

    // And a newer message from elsewhere is not the old failure's to clear.
    prompt.acknowledgeReport();
    prompt.noteLaunch(.{ .status_failure = "no test map path" }, &status);
    status.set(.frame, "failed: ", "DeviceLost");
    prompt.noteLaunch(.started, &status);
    try std.testing.expectEqualStrings("failed: DeviceLost", status.line());
}

test "command line: a relative map resolves against the launch directory, not the installation" {
    var buffer: [PathSlot.max_path]u8 = undefined;
    if (builtin.os.tag == .windows) {
        try std.testing.expectEqualStrings("C:\\Users\\me\\maps\\x.bzm", absoluteFromLaunchDir(&buffer, "C:\\Users\\me", "maps\\x.bzm").?);
        try std.testing.expectEqualStrings("C:\\Games\\BK\\Data\\Maps\\a.bzm", absoluteFromLaunchDir(&buffer, "C:\\Users\\me", "C:\\Games\\BK\\Data\\Maps\\a.bzm").?);
        try std.testing.expectEqualStrings("C:\\Users\\x.bzm", absoluteFromLaunchDir(&buffer, "C:\\Users\\me", "..\\x.bzm").?);
    } else {
        // The build's tiers pass the engine's backslashes on every OS.
        try std.testing.expectEqualStrings("/stage/Data/Maps/Multiplayer/coldwinter.bzm", absoluteFromLaunchDir(&buffer, "/stage", "Data\\Maps\\Multiplayer\\coldwinter.bzm").?);
        try std.testing.expectEqualStrings("/Users/me/maps/x.bzm", absoluteFromLaunchDir(&buffer, "/Users/me", "maps/x.bzm").?);
        try std.testing.expectEqualStrings("/Games/BK/Data/a.bzm", absoluteFromLaunchDir(&buffer, "/Users/me", "/Games/BK/Data/a.bzm").?);
        try std.testing.expectEqualStrings("/Users/x.bzm", absoluteFromLaunchDir(&buffer, "/Users/me", "./../x.bzm").?);
    }
    var tiny: [4]u8 = undefined;
    try std.testing.expect(absoluteFromLaunchDir(&tiny, "/Users/me", "maps/x.bzm") == null);
}

test "dialog folder: a relative maps folder is under the user root, not the working directory" {
    var buffer: [PathSlot.max_path]u8 = undefined;
    const root = if (builtin.os.tag == .windows) "C:\\Users\\me\\AppData\\Blitzkrieg\\" else "/Users/me/.local/share/Nival/Blitzkrieg/";
    const absolute = if (builtin.os.tag == .windows) "D:\\Maps" else "/Volumes/Maps";
    try std.testing.expectEqualStrings(absolute, dialogFolderFor(&buffer, absolute, root, null).?);
    const expected_relative = if (builtin.os.tag == .windows) "C:\\Users\\me\\AppData\\Blitzkrieg\\mine" else "/Users/me/.local/share/Nival/Blitzkrieg/mine";
    try std.testing.expectEqualStrings(expected_relative, dialogFolderFor(&buffer, "mine", root, null).?);
    const expected_default = if (builtin.os.tag == .windows) "C:\\Users\\me\\AppData\\Blitzkrieg\\maps" else "/Users/me/.local/share/Nival/Blitzkrieg/maps";
    try std.testing.expectEqualStrings(expected_default, dialogFolderFor(&buffer, "", root, null).?);
}

// ---- M2 foundations (04-03): camera anchor slots, and the pure files whose
// tests only run when a test root imports them (Pitfall 18).

/// Editor.setCameraAnchor's numbering: -1 is the neutral anchor, 0.. a
/// player's.
pub const neutral_anchor_slot: i32 = -1;

/// `neutral`, or a player number below the anchor record's capacity (32).
/// The argument of the camera commands and predicates.
pub fn parseAnchorSlot(arg: []const u8) ?i32 {
    if (std.mem.eql(u8, arg, "neutral")) return neutral_anchor_slot;
    const player = std.fmt.parseInt(u32, arg, 10) catch return null;
    if (player >= core.records.max_camera_players) return null;
    return @intCast(player);
}

test "parseAnchorSlot takes neutral and players below the record's capacity" {
    try std.testing.expectEqual(@as(?i32, -1), parseAnchorSlot("neutral"));
    try std.testing.expectEqual(@as(?i32, 0), parseAnchorSlot("0"));
    try std.testing.expectEqual(@as(?i32, 31), parseAnchorSlot("31"));
    try std.testing.expectEqual(@as(?i32, null), parseAnchorSlot("32"));
    try std.testing.expectEqual(@as(?i32, null), parseAnchorSlot("-1"));
    try std.testing.expectEqual(@as(?i32, null), parseAnchorSlot("Neutral"));
    try std.testing.expectEqual(@as(?i32, null), parseAnchorSlot(""));
}

test {
    _ = @import("marker_logic.zig");
    _ = @import("tool_registry.zig");
    // 05-11: the single-instance endpoint, framing and socket tests.
    _ = @import("single_instance.zig");
}

test "new map fields: parse the command's format, clamp the dialog's" {
    const fields = NewMapFields.parse("8x8:summer") orelse return error.TestUnexpectedError;
    try std.testing.expectEqual(@as(i32, 8), fields.size_x);
    try std.testing.expectEqual(@as(i32, 8), fields.size_y);
    try std.testing.expectEqual(@as(i32, 0), fields.season);
    try std.testing.expectEqual(@as(usize, 0), fields.name.slice().len);
    const named = NewMapFields.parse("16x4:WINTER:my_map:EditorTestMod") orelse return error.TestUnexpectedError;
    try std.testing.expectEqual(@as(i32, 16), named.size_x);
    try std.testing.expectEqual(@as(i32, 4), named.size_y);
    try std.testing.expectEqual(@as(i32, 1), named.season);
    try std.testing.expectEqualStrings("my_map", named.name.slice());
    try std.testing.expectEqualStrings("EditorTestMod", named.mod_folder.slice());
    // Spring is the dialog's fourth; the bridge maps it onto Summer's own
    // real value (REAL_SEASONS), the parse leaves the dialog numbering.
    try std.testing.expectEqual(@as(i32, 3), (NewMapFields.parse("1x1:spring") orelse return error.TestUnexpectedError).season);
    // Africa by name, case-insensitively.
    try std.testing.expectEqual(@as(i32, 2), (NewMapFields.parse("1x1:AFRICA") orelse return error.TestUnexpectedError).season);
    // Bad sizes, a bad season and trailing parts do not parse.
    try std.testing.expect(NewMapFields.parse("0x8:summer") == null);
    try std.testing.expect(NewMapFields.parse("33x8:summer") == null);
    try std.testing.expect(NewMapFields.parse("8x8:autumn") == null);
    try std.testing.expect(NewMapFields.parse("8x8") == null);
    try std.testing.expect(NewMapFields.parse("8:8:summer") == null);
    // The dialog's own bounds: clamp, and say so.
    var clamped: NewMapFields = .{ .size_x = 40, .size_y = 0, .season = 9 };
    try std.testing.expect(!clamped.clampToValid());
    try std.testing.expectEqual(@as(i32, 32), clamped.size_x);
    try std.testing.expectEqual(@as(i32, 1), clamped.size_y);
    try std.testing.expectEqual(@as(i32, 3), clamped.season);
    var valid: NewMapFields = .{};
    try std.testing.expect(valid.clampToValid());
}

test "file actions: File > New guards through the unsaved-changes prompt, then builds" {
    var fake = try core.editor.testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };

    // A clean map: the fields run at once, as a new_map step.
    actions.requestNewMap(.{ .size_x = 2, .size_y = 3, .season = 1, .name = .{} });
    const step = actions.next(false, false);
    try std.testing.expectEqual(@as(i32, 2), step.new_map.size_x);
    try std.testing.expectEqual(@as(i32, 3), step.new_map.size_y);
    try std.testing.expectEqual(FileActions.Step.none, actions.next(false, false));

    // A dirty map: the prompt asks first, and the fields wait.
    actions.requestNewMap(.{ .size_x = 4, .size_y = 4 });
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    try std.testing.expect(actions.prompt.isAsking());
    // Don't save: the fields run.
    actions.answer_pending = .dont_save;
    const answered = actions.next(true, false);
    try std.testing.expectEqual(@as(i32, 4), answered.new_map.size_x);
    // Cancel: nothing is built.
    actions.requestNewMap(.{ .size_x = 5, .size_y = 5 });
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    actions.answer_pending = .cancel;
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));
}

test "enginePath: the default format is what an extensionless save gets" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("\\tmp\\b.bzm", enginePath(&buffer, "/tmp/b", .save_as, .bzm).?);
    try std.testing.expectEqualStrings("\\tmp\\b.xml", enginePath(&buffer, "/tmp/b", .save_as, .xml).?);
    // A path that names its format keeps it whatever the default is.
    try std.testing.expectEqualStrings("\\tmp\\b.xml", enginePath(&buffer, "/tmp/b.xml", .save_as, .bzm).?);
}

// ---------------------------------------------------------------------------
// Heights (M3, D-18): the panel's and the commands' pure helpers - parsing
// the fields, naming the level modes and the generate types.
// ---------------------------------------------------------------------------

/// A heights field's float: finite, dot decimal. Null for anything else (the
/// command grammar's own printable ASCII, no space, no comma).
pub fn parseHeightsFloat(text: []const u8) ?f32 {
    if (text.len == 0 or text.len > 32) return null;
    const value = std.fmt.parseFloat(f32, text) catch return null;
    if (!std.math.isFinite(value)) return null;
    return value;
}

/// The level modes' command names, in bridge.HeightsLevelMode's own order.
pub const heights_mode_names = [_][]const u8{ "zero", "click_tile", "instant_average", "click_average" };

/// The mode a `heights_mode:` argument names, or null.
pub fn heightsModeFromName(text: []const u8) ?core.bridge.HeightsLevelMode {
    for (heights_mode_names, 0..) |name, index| {
        if (std.mem.eql(u8, text, name)) return @enumFromInt(@as(c_int, @intCast(index)));
    }
    return null;
}

/// The generate type a `heights_generate:` argument names (the MFC dialog's
/// own three; the hidden MULTI/HETERO radios are not features), or null.
pub fn heightsGenerateTypeFromName(text: []const u8) ?core.bridge.HeightsGenerateType {
    if (std.mem.eql(u8, text, "hills")) return .hills;
    if (std.mem.eql(u8, text, "rocks")) return .rocks;
    if (std.mem.eql(u8, text, "dunes")) return .dunes;
    return null;
}

test "heights fields: the floats the MFC's own edit rules would take" {
    try std.testing.expectEqual(@as(?f32, 1.0), parseHeightsFloat("1"));
    try std.testing.expectEqual(@as(?f32, 0.03), parseHeightsFloat("0.03"));
    try std.testing.expectEqual(@as(?f32, -3.5), parseHeightsFloat("-3.5"));
    try std.testing.expectEqual(@as(?f32, null), parseHeightsFloat(""));
    try std.testing.expectEqual(@as(?f32, null), parseHeightsFloat("abc"));
    try std.testing.expectEqual(@as(?f32, null), parseHeightsFloat("1 2"));
    try std.testing.expectEqual(@as(?f32, null), parseHeightsFloat("nan"));
    try std.testing.expectEqual(@as(?f32, null), parseHeightsFloat("inf"));
}

test "heights modes and generate types: the names the commands take" {
    try std.testing.expectEqual(core.bridge.HeightsLevelMode.zero, heightsModeFromName("zero").?);
    try std.testing.expectEqual(core.bridge.HeightsLevelMode.click_tile, heightsModeFromName("click_tile").?);
    try std.testing.expectEqual(core.bridge.HeightsLevelMode.instant_average, heightsModeFromName("instant_average").?);
    try std.testing.expectEqual(core.bridge.HeightsLevelMode.click_average, heightsModeFromName("click_average").?);
    try std.testing.expectEqual(@as(?core.bridge.HeightsLevelMode, null), heightsModeFromName("Instant Average"));
    try std.testing.expectEqual(@as(?core.bridge.HeightsLevelMode, null), heightsModeFromName(""));
    try std.testing.expectEqual(core.bridge.HeightsGenerateType.hills, heightsGenerateTypeFromName("hills").?);
    try std.testing.expectEqual(core.bridge.HeightsGenerateType.rocks, heightsGenerateTypeFromName("rocks").?);
    try std.testing.expectEqual(core.bridge.HeightsGenerateType.dunes, heightsGenerateTypeFromName("dunes").?);
    try std.testing.expectEqual(@as(?core.bridge.HeightsGenerateType, null), heightsGenerateTypeFromName("multi"));
    try std.testing.expectEqual(@as(?core.bridge.HeightsGenerateType, null), heightsGenerateTypeFromName("hetero"));
}

// ---------------------------------------------------------------------------
// The Minimap (05-07, D-14..D-16): everything that needs no window.
// ---------------------------------------------------------------------------

/// The MFC minimap's 17 player colours, 0xRRGGBB: MINIMAP_PLAYER_COLORS in
/// MiniMapTypes.cpp:149-167 value for value (the MFC writes them as RGB(r, g, b);
/// MxHC is 0x80). Index 16 is the one a player outside the table gets.
pub const minimap_player_colors = [17]u32{
    0x00FF00, // 0
    0xFF0000, // 1
    0x0000FF, // 2
    0xFFFF00, // 3
    0x00FFFF, // 4
    0xFF00FF, // 5
    0xFFFFFF, // 6
    0xFF8000, // 7
    0xFF0080, // 8
    0x80FF00, // 9
    0x00FF80, // 10
    0x8000FF, // 11
    0x0080FF, // 12
    0x008080, // 13
    0x800080, // 14
    0x808000, // 15
    0x808080, // 16
};

/// A marker's colour: the table's entry, the last one for any index outside it.
pub fn minimapPlayerColor(index: i32) u32 {
    if (index < 0 or index >= minimap_player_colors.len) return minimap_player_colors[minimap_player_colors.len - 1];
    return minimap_player_colors[@intCast(index)];
}

/// The patch grid's spacing in tiles: a terrain patch is 16 x 16 tiles
/// (STerrainPatchInfo::nSizeX).
pub const minimap_grid_step_tiles: i32 = 16;

/// The AI's units per terrain tile (two AI tiles of 32).
pub const minimap_ai_units_per_tile: f32 = 64.0;

/// The size the minimap picture takes inside `avail_w` x `avail_h`, in the
/// map's own aspect (a 16 x 8 patch map is twice as wide as high). A map of no
/// size or no room gives zero.
pub fn minimapFit(avail_w: f32, avail_h: f32, tiles_w: i32, tiles_h: i32) [2]f32 {
    if (tiles_w <= 0 or tiles_h <= 0 or avail_w <= 0 or avail_h <= 0) return .{ 0, 0 };
    const aspect = @as(f32, @floatFromInt(tiles_w)) / @as(f32, @floatFromInt(tiles_h));
    var w = avail_w;
    var h = w / aspect;
    if (h > avail_h) {
        h = avail_h;
        w = h * aspect;
    }
    return .{ w, h };
}

/// A point in the picture to the world point it stands for - the MFC's own
/// formula (MiniMapDialog.cpp:316-322, OnLButtonDown): `px`, `py` are pixels
/// from the picture's top-left in a picture `rect_w` x `rect_h` pixels; the
/// picture's top is the map's far (high y) edge, and the last pixel row is
/// world y 0 - the "- 1" is the MFC's.
pub fn minimapToWorld(px: f32, py: f32, rect_w: f32, rect_h: f32, tiles_w: i32, tiles_h: i32) [2]f32 {
    if (rect_w <= 0 or rect_h <= 0) return .{ 0, 0 };
    const cell = view_math.world_cell_size;
    return .{
        px * @as(f32, @floatFromInt(tiles_w)) * cell / rect_w,
        (rect_h - py - 1) * @as(f32, @floatFromInt(tiles_h)) * cell / rect_h,
    };
}

/// Where the camera goes for a click on world point `world` - the MFC's rule
/// (MiniMapDialog.cpp:325-329): the clicked point goes to the screen's
/// centre, so the anchor moves by the clicked point plus however far the anchor
/// is from what is under the screen's centre now (`centre`), then clamped to
/// the map like every other camera move (NormalizeCamera).
pub fn minimapCameraTarget(world: [2]f32, anchor: [2]f32, centre: [2]f32, map: view_math.MapSize) [2]f32 {
    var camera: view_math.Camera = .{ .x = world[0] + anchor[0] - centre[0], .y = world[1] + anchor[1] - centre[1] };
    camera.clamp(map);
    return .{ camera.x, camera.y };
}

/// The MFC draw tool's placement of a WORLD point in a picture `rect_w` x
/// `rect_h` pixels (CSizedMiniMapDrawTool::ActualX/ActualY): y runs up.
pub fn minimapWorldToPanel(wx: f32, wy: f32, rect_w: f32, rect_h: f32, tiles_w: i32, tiles_h: i32) [2]f32 {
    const cell = view_math.world_cell_size;
    const max_x = @as(f32, @floatFromInt(tiles_w)) * cell;
    const max_y = @as(f32, @floatFromInt(tiles_h)) * cell;
    if (max_x <= 0 or max_y <= 0) return .{ 0, 0 };
    return .{ wx * rect_w / max_x, rect_h - wy * rect_h / max_y };
}

/// The same for a point in AI TILES (two per terrain tile per axis, y up): the
/// unit markers' rectangles.
pub fn minimapAiTileToPanel(ax: f32, ay: f32, rect_w: f32, rect_h: f32, tiles_w: i32, tiles_h: i32) [2]f32 {
    const max_x = @as(f32, @floatFromInt(tiles_w)) * 2;
    const max_y = @as(f32, @floatFromInt(tiles_h)) * 2;
    if (max_x <= 0 or max_y <= 0) return .{ 0, 0 };
    return .{ ax * rect_w / max_x, rect_h - ay * rect_h / max_y };
}

/// The same for a point in AI UNITS (64 per terrain tile): the fire-range
/// areas' centres.
pub fn minimapAiUnitsToPanel(ux: f32, uy: f32, rect_w: f32, rect_h: f32, tiles_w: i32, tiles_h: i32) [2]f32 {
    return minimapAiTileToPanel(ux / (minimap_ai_units_per_tile / 2), uy / (minimap_ai_units_per_tile / 2), rect_w, rect_h, tiles_w, tiles_h);
}

/// The end of a fire-range sector's edge (MiniMapTypes.cpp:118-145): the area's
/// angle word is a turn out of 65535, the edge is drawn from the centre at that
/// angle plus a quarter turn, and `radius` long - in whatever units the centre
/// and radius are in, y up.
pub fn minimapSectorEnd(cx: f32, cy: f32, radius: f32, angle: i32) [2]f32 {
    const two_pi = 2.0 * std.math.pi;
    const turned = @as(f32, @floatFromInt(angle)) / 65535.0 * two_pi + std.math.pi / 2.0;
    const wrapped = @mod(turned, two_pi);
    return .{ cx + radius * @cos(wrapped), cy + radius * @sin(wrapped) };
}

/// How many grid lines a map of `tiles` tiles has along one axis, one every
/// `step` tiles inside the map (not on its edge).
pub fn minimapGridCount(tiles: i32, step: i32) usize {
    if (tiles <= 0 or step <= 0) return 0;
    return @intCast(@divTrunc(tiles - 1, step));
}

/// Grid line `index` (0-based) as a fraction of the map's width or height, 0..1.
pub fn minimapGridFraction(index: usize, tiles: i32, step: i32) f32 {
    return @as(f32, @floatFromInt((@as(i32, @intCast(index)) + 1) * step)) / @as(f32, @floatFromInt(tiles));
}

/// The grey a tile the tileset has no colour for is drawn in.
pub const minimap_unknown_tile_color: u32 = 0x808080;

/// The terrain texture's pixels (RGBA8, one per tile, row 0 first): every tile's
/// colour from the table, MiniMapTypes.cpp's `colors[...]`. `pixels` must be
/// `tiles.len * 4` long.
pub fn rasterizeMinimapTerrain(pixels: []u8, tiles: []const u8, colors: []const u32) void {
    std.debug.assert(pixels.len == tiles.len * 4);
    for (tiles, 0..) |tile, i| {
        const rgb = if (tile < colors.len) colors[tile] else minimap_unknown_tile_color;
        pixels[i * 4 + 0] = @intCast((rgb >> 16) & 0xFF);
        pixels[i * 4 + 1] = @intCast((rgb >> 8) & 0xFF);
        pixels[i * 4 + 2] = @intCast(rgb & 0xFF);
        pixels[i * 4 + 3] = 0xFF;
    }
}

/// The height gradient the MFC shows while the Heights tool is active
/// (MiniMapTypes.cpp CMiniMapTerrain::Update, altitudes branch): one grey per
/// tile from the height of the tile's first vertex, 0 at the lowest of those
/// and 255 at the highest. `heights` is the vertex sheet (`vertex_w` per row,
/// one more than the tiles per axis). A flat sheet is all black (the MFC's
/// divides by a range it set to -1 there). A vertex whose height the engine's
/// validity rule refuses (`isValidHeight`) is red, as the MFC painted it
/// (`RGB( 0xFF, 0, 0 )`, MiniMapTypes.cpp:503).
pub fn rasterizeMinimapHeights(pixels: []u8, heights: []const f32, vertex_w: usize, tiles_w: usize, tiles_h: usize) void {
    std.debug.assert(pixels.len == tiles_w * tiles_h * 4);
    if (tiles_w == 0 or tiles_h == 0 or heights.len < vertex_w * tiles_h or vertex_w < tiles_w) {
        @memset(pixels, 0);
        return;
    }
    var low = heights[0];
    var high = heights[0];
    for (0..tiles_h) |y| {
        for (0..tiles_w) |x| {
            const h = heights[y * vertex_w + x];
            if (h < low) low = h;
            if (h > high) high = h;
        }
    }
    const span = high - low;
    const vertex_h = heights.len / vertex_w;
    for (0..tiles_h) |y| {
        for (0..tiles_w) |x| {
            const h = heights[y * vertex_w + x];
            const grey: u8 = if (span > 0) @intFromFloat(std.math.clamp(255.0 * (h - low) / span, 0, 255)) else 0;
            const at = (y * tiles_w + x) * 4;
            const valid = isValidHeight(heights, vertex_w, vertex_h, x, y);
            pixels[at + 0] = if (valid) grey else 0xFF;
            pixels[at + 1] = if (valid) grey else 0;
            pixels[at + 2] = if (valid) grey else 0;
            pixels[at + 3] = 0xFF;
        }
    }
}

/// `CVertexAltitudeInfo::IsValidHeight` (RandomMapGen/VA_StaticMethods.cpp:152):
/// the terrain is valid at a vertex when both triangle normals it makes with
/// its four neighbours (a missing neighbour stands at the vertex's own height)
/// face the camera from both sides - no slope steeper than the engine's
/// camera angle allows, no overhang. `heights` is the vertex sheet, `vertex_w`
/// per row and `vertex_h` rows. The arithmetic is the C++'s own, in f32:
/// absolute vertex positions first, then the differences.
pub fn isValidHeight(heights: []const f32, vertex_w: usize, vertex_h: usize, x: usize, y: usize) bool {
    if (x >= vertex_w or y >= vertex_h or heights.len < vertex_w * vertex_h) return false;
    const cell = view_math.world_cell_size;
    const alpha: f32 = std.math.sqrt2 / @sqrt(@as(f32, 3.0)); // CAMERA_ALPHA = FP_SQRT_2 / FP_SQRT_3
    const fx: f32 = @floatFromInt(x);
    const fy: f32 = @floatFromInt(y);
    const h0 = heights[y * vertex_w + x];
    const v0 = [3]f32{ fx * cell, fy * cell, h0 };
    const v1 = [3]f32{ (fx - 1) * cell, fy * cell, if (x > 0) heights[y * vertex_w + x - 1] else h0 };
    const v2 = [3]f32{ fx * cell, (fy - 1) * cell, if (y > 0) heights[(y - 1) * vertex_w + x] else h0 };
    const v3 = [3]f32{ fx * cell, (fy + 1) * cell, if (y + 1 < vertex_h) heights[(y + 1) * vertex_w + x] else h0 };
    const v4 = [3]f32{ (fx + 1) * cell, fy * cell, if (x + 1 < vertex_w) heights[y * vertex_w + x + 1] else h0 };
    const n0 = cross3(sub3(v2, v0), sub3(v1, v0));
    const n1 = cross3(sub3(v3, v0), sub3(v4, v0));
    const negative = [3]f32{ -1, -1, -alpha }; // V3_CAMERA_NEGATIVE
    const positive = [3]f32{ 1, 1, -alpha }; // V3_CAMERA_POSITIVE
    return dot3(negative, n0) > 0 and dot3(negative, n1) > 0 and dot3(positive, n0) > 0 and dot3(positive, n1) > 0;
}

fn sub3(a: [3]f32, b: [3]f32) [3]f32 {
    return .{ a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}

/// Misc/Geometry.h:104 `operator^( CVec3, CVec3 )`.
fn cross3(a: [3]f32, b: [3]f32) [3]f32 {
    return .{ a[1] * b[2] - b[1] * a[2], a[2] * b[0] - b[2] * a[0], a[0] * b[1] - b[0] * a[1] };
}

fn dot3(a: [3]f32, b: [3]f32) f32 {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/// What a `minimap_click:<x>x<y>` argument asks: percents of the picture's width
/// and height from its top-left, 0..100 each.
pub fn parseMinimapClick(arg: []const u8) ?[2]f32 {
    const sep = std.mem.indexOfScalar(u8, arg, 'x') orelse return null;
    const x = std.fmt.parseFloat(f32, arg[0..sep]) catch return null;
    const y = std.fmt.parseFloat(f32, arg[sep + 1 ..]) catch return null;
    if (!std.math.isFinite(x) or !std.math.isFinite(y) or x < 0 or x > 100 or y < 0 or y > 100) return null;
    return .{ x, y };
}

/// The minimap's two pictures.
pub const MinimapMode = enum {
    editor,
    game,

    pub fn fromName(name: []const u8) ?MinimapMode {
        if (std.mem.eql(u8, name, "editor")) return .editor;
        if (std.mem.eql(u8, name, "game")) return .game;
        return null;
    }

    pub fn label(self: MinimapMode) []const u8 {
        return switch (self) {
            .editor => "Editor",
            .game => "Game",
        };
    }
};

/// `<map path minus .bzm or .xml>` - what Create Minimap Images names its
/// pictures after; null for a path that ends in neither.
pub fn minimapImageBase(map_path: []const u8) ?[]const u8 {
    const dot = std.mem.lastIndexOfScalar(u8, map_path, '.') orelse return null;
    const cut = std.mem.lastIndexOfAny(u8, map_path, "/\\");
    if (cut != null and dot < cut.?) return null;
    const extension = map_path[dot..];
    if (!std.ascii.eqlIgnoreCase(extension, ".bzm") and !std.ascii.eqlIgnoreCase(extension, ".xml")) return null;
    return map_path[0..dot];
}

test "the minimap's player colours are the MFC's table, value for value" {
    // MiniMapTypes.cpp:149-167, RGB(r, g, b) with MxHC = 0x80.
    const mfc = [17][3]u8{
        .{ 0x00, 0xFF, 0x00 }, .{ 0xFF, 0x00, 0x00 }, .{ 0x00, 0x00, 0xFF }, .{ 0xFF, 0xFF, 0x00 },
        .{ 0x00, 0xFF, 0xFF }, .{ 0xFF, 0x00, 0xFF }, .{ 0xFF, 0xFF, 0xFF }, .{ 0xFF, 0x80, 0x00 },
        .{ 0xFF, 0x00, 0x80 }, .{ 0x80, 0xFF, 0x00 }, .{ 0x00, 0xFF, 0x80 }, .{ 0x80, 0x00, 0xFF },
        .{ 0x00, 0x80, 0xFF }, .{ 0x00, 0x80, 0x80 }, .{ 0x80, 0x00, 0x80 }, .{ 0x80, 0x80, 0x00 },
        .{ 0x80, 0x80, 0x80 },
    };
    try std.testing.expectEqual(@as(usize, 17), minimap_player_colors.len);
    for (mfc, 0..) |rgb, i| {
        const want = (@as(u32, rgb[0]) << 16) | (@as(u32, rgb[1]) << 8) | rgb[2];
        try std.testing.expectEqual(want, minimap_player_colors[i]);
    }
    // Outside the table is the last entry.
    try std.testing.expectEqual(@as(u32, 0x808080), minimapPlayerColor(-1));
    try std.testing.expectEqual(@as(u32, 0x808080), minimapPlayerColor(17));
    try std.testing.expectEqual(@as(u32, 0xFF0000), minimapPlayerColor(1));
}

test "minimap click: the MFC's point-to-world formula, edges included" {
    const cell = view_math.world_cell_size;
    // A 100 x 100 tile map in a 200 x 200 picture: the top-left pixel is the far
    // west edge at the map's north end (world y just under the top), the last row
    // is world y 0 less nothing: the MFC's "- 1" puts the last row at y 0.
    const top_left = minimapToWorld(0, 0, 200, 200, 100, 100);
    try std.testing.expectEqual(@as(f32, 0), top_left[0]);
    try std.testing.expectApproxEqAbs(199.0 * 100 * cell / 200.0, top_left[1], 0.01);
    const bottom_row = minimapToWorld(0, 199, 200, 200, 100, 100);
    try std.testing.expectEqual(@as(f32, 0), bottom_row[1]);
    // The right edge pixel is one pixel short of the map's east edge.
    const right = minimapToWorld(199, 100, 200, 200, 100, 100);
    try std.testing.expectApproxEqAbs(199.0 * 100 * cell / 200.0, right[0], 0.01);
    // The middle of the picture is the middle of the map (to the pixel).
    const middle = minimapToWorld(100, 99, 200, 200, 100, 100);
    try std.testing.expectApproxEqAbs(50.0 * cell, middle[0], 0.01);
    try std.testing.expectApproxEqAbs(100.0 * 100 * cell / 200.0, middle[1], 0.01);
    // A non-square map scales each axis by its own size.
    const wide = minimapToWorld(100, 50, 200, 100, 200, 100);
    try std.testing.expectApproxEqAbs(100.0 * 200 * cell / 200.0, wide[0], 0.01);
    try std.testing.expectApproxEqAbs(49.0 * 100 * cell / 100.0, wide[1], 0.01);
    // No picture, no point.
    try std.testing.expectEqual([2]f32{ 0, 0 }, minimapToWorld(5, 5, 0, 10, 8, 8));
}

test "minimap click: the camera keeps the MFC's screen-centre offset and stays on the map" {
    const map: view_math.MapSize = .{ .width_tiles = 100, .height_tiles = 100 };
    const cell = view_math.world_cell_size;
    // The anchor and the screen's centre agree: the camera goes where the click is.
    const plain = minimapCameraTarget(.{ 1000, 1500 }, .{ 800, 800 }, .{ 800, 800 }, map);
    try std.testing.expectEqual([2]f32{ 1000, 1500 }, plain);
    // The centre is 40 east and 25 south of the anchor (the iso view's offset):
    // the click's point goes to the centre, so the anchor goes to the point minus that.
    const offset = minimapCameraTarget(.{ 1000, 1500 }, .{ 800, 800 }, .{ 840, 775 }, map);
    try std.testing.expectEqual([2]f32{ 960, 1525 }, offset);
    // Past the map's edge the camera is held to it, like every camera move.
    const west = minimapCameraTarget(.{ 10, 1500 }, .{ 800, 800 }, .{ 900, 800 }, map);
    try std.testing.expectEqual(@as(f32, 0), west[0]);
    const north = minimapCameraTarget(.{ 1000, 100 * cell + 500 }, .{ 800, 800 }, .{ 800, 800 }, map);
    try std.testing.expectEqual(@as(f32, 100 * cell), north[1]);
}

test "minimap overlays: the MFC draw tool's placement, y up" {
    // World units: the origin is the bottom-left, the far corner the top-right.
    const origin = minimapWorldToPanel(0, 0, 200, 100, 50, 25);
    try std.testing.expectEqual([2]f32{ 0, 100 }, origin);
    const far = minimapWorldToPanel(50 * view_math.world_cell_size, 25 * view_math.world_cell_size, 200, 100, 50, 25);
    try std.testing.expectApproxEqAbs(@as(f32, 200), far[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f32, 0), far[1], 0.001);
    // AI tiles: two per terrain tile.
    const ai = minimapAiTileToPanel(50, 25, 200, 100, 50, 25);
    try std.testing.expectEqual([2]f32{ 100, 50 }, ai);
    // AI units: 64 per terrain tile, so 25 tiles of 64 is the middle of a 50-tile map.
    const units = minimapAiUnitsToPanel(25 * 64, 12.5 * 64, 200, 100, 50, 25);
    try std.testing.expectApproxEqAbs(@as(f32, 100), units[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f32, 50), units[1], 0.001);
}

test "minimap sector edges: the angle word plus a quarter turn, from the centre" {
    // Angle 0 is the quarter turn: straight along +y.
    const north = minimapSectorEnd(100, 100, 50, 0);
    try std.testing.expectApproxEqAbs(@as(f32, 100), north[0], 0.001);
    try std.testing.expectApproxEqAbs(@as(f32, 150), north[1], 0.001);
    // A quarter of 65535 more is a half turn from +x: along -x.
    const west = minimapSectorEnd(100, 100, 50, 16384);
    try std.testing.expectApproxEqAbs(@as(f32, 50), west[0], 0.05);
    try std.testing.expectApproxEqAbs(@as(f32, 100), west[1], 0.05);
}

test "minimap grid: one line per patch inside the map" {
    try std.testing.expectEqual(@as(usize, 0), minimapGridCount(16, 16));
    try std.testing.expectEqual(@as(usize, 1), minimapGridCount(32, 16));
    try std.testing.expectEqual(@as(usize, 31), minimapGridCount(512, 16));
    try std.testing.expectEqual(@as(usize, 0), minimapGridCount(0, 16));
    try std.testing.expectEqual(@as(f32, 0.5), minimapGridFraction(0, 32, 16));
    try std.testing.expectEqual(@as(f32, 0.25), minimapGridFraction(0, 64, 16));
    try std.testing.expectEqual(@as(f32, 0.5), minimapGridFraction(1, 64, 16));
}

test "minimap fit: the map's aspect inside the room there is" {
    const wide = minimapFit(400, 400, 200, 100);
    try std.testing.expectEqual([2]f32{ 400, 200 }, wide);
    const tall = minimapFit(400, 100, 100, 100);
    try std.testing.expectEqual([2]f32{ 100, 100 }, tall);
    try std.testing.expectEqual([2]f32{ 0, 0 }, minimapFit(0, 10, 8, 8));
    try std.testing.expectEqual([2]f32{ 0, 0 }, minimapFit(10, 10, 0, 8));
}

test "minimap terrain pixels: every tile's colour, an unknown tile grey" {
    const colors = [_]u32{ 0x102030, 0xAABBCC };
    const tiles = [_]u8{ 0, 1, 1, 7 };
    var pixels: [16]u8 = undefined;
    rasterizeMinimapTerrain(&pixels, &tiles, &colors);
    try std.testing.expectEqualSlices(u8, &.{ 0x10, 0x20, 0x30, 0xFF }, pixels[0..4]);
    try std.testing.expectEqualSlices(u8, &.{ 0xAA, 0xBB, 0xCC, 0xFF }, pixels[4..8]);
    try std.testing.expectEqualSlices(u8, &.{ 0x80, 0x80, 0x80, 0xFF }, pixels[12..16]);
}

test "minimap heights: the first vertex of each tile, lowest black and highest white" {
    // 2 x 2 tiles, 3 x 3 vertices; the last row and column of vertices belong to no tile.
    // Gentle heights, all valid for the engine's rule (a steeper one is red, below).
    const heights = [_]f32{
        0, 1, 3,
        2, 3, 3,
        3, 3, 3,
    };
    var pixels: [16]u8 = undefined;
    rasterizeMinimapHeights(&pixels, &heights, 3, 2, 2);
    try std.testing.expectEqual(@as(u8, 0), pixels[0]);
    try std.testing.expectEqual(@as(u8, 255), pixels[12]);
    try std.testing.expectEqual(@as(u8, 85), pixels[4]);
    try std.testing.expectEqual(@as(u8, 170), pixels[8]);
    try std.testing.expectEqual(@as(u8, 0xFF), pixels[3]);
    // A flat sheet is black.
    const flat = [_]f32{5} ** 9;
    rasterizeMinimapHeights(&pixels, &flat, 3, 2, 2);
    try std.testing.expectEqual(@as(u8, 0), pixels[0]);
    try std.testing.expectEqual(@as(u8, 0), pixels[12]);
}

test "isValidHeight: the engine's rule - gentle slopes are valid, a spike or a cliff either way is not" {
    // A flat sheet, and a gentle bump (the limit for a lone spike is
    // CAMERA_ALPHA * cell / 2 = about 18.5 units).
    var sheet = [_]f32{0} ** 25;
    for (0..5) |y| for (0..5) |x| try std.testing.expect(isValidHeight(&sheet, 5, 5, x, y));
    sheet[2 * 5 + 2] = 10;
    for (0..5) |y| for (0..5) |x| try std.testing.expect(isValidHeight(&sheet, 5, 5, x, y));
    // A spike up or down past the limit is invalid at its own vertex ...
    sheet[2 * 5 + 2] = 30;
    try std.testing.expect(!isValidHeight(&sheet, 5, 5, 2, 2));
    sheet[2 * 5 + 2] = -30;
    try std.testing.expect(!isValidHeight(&sheet, 5, 5, 2, 2));
    // ... and a far corner is not touched by it.
    try std.testing.expect(isValidHeight(&sheet, 5, 5, 0, 0));
    // A missing neighbour stands at the vertex's own height: the sheet's edge
    // and corner are judged on what is there.
    sheet[2 * 5 + 2] = 0;
    sheet[0] = 40;
    try std.testing.expect(!isValidHeight(&sheet, 5, 5, 0, 0));
    // Out of the sheet is never valid.
    try std.testing.expect(!isValidHeight(&sheet, 5, 5, 5, 0));
    try std.testing.expect(!isValidHeight(&sheet, 5, 5, 0, 5));
}

test "minimap heights: a vertex the engine refuses is red, the rest stay grey" {
    // 3 x 3 tiles, 4 x 4 vertices, one spike at vertex (1, 1).
    var heights = [_]f32{0} ** 16;
    heights[1 * 4 + 1] = 60;
    var pixels: [9 * 4]u8 = undefined;
    rasterizeMinimapHeights(&pixels, &heights, 4, 3, 3);
    const at = (1 * 3 + 1) * 4;
    try std.testing.expectEqualSlices(u8, &.{ 0xFF, 0, 0, 0xFF }, pixels[at .. at + 4]);
    // The spike's neighbours see the cliff too; a vertex two away does not.
    try std.testing.expectEqualSlices(u8, &.{ 0xFF, 0, 0, 0xFF }, pixels[(1 * 3 + 2) * 4 ..][0..4]);
    try std.testing.expect(!std.mem.eql(u8, pixels[(2 * 3 + 2) * 4 ..][0..3], &.{ 0xFF, 0, 0 }));
}

test "minimap click arguments and modes" {
    try std.testing.expectEqual(@as(?[2]f32, .{ 90, 10 }), parseMinimapClick("90x10"));
    try std.testing.expectEqual(@as(?[2]f32, .{ 0.5, 100 }), parseMinimapClick("0.5x100"));
    try std.testing.expectEqual(@as(?[2]f32, null), parseMinimapClick("101x10"));
    try std.testing.expectEqual(@as(?[2]f32, null), parseMinimapClick("-1x10"));
    try std.testing.expectEqual(@as(?[2]f32, null), parseMinimapClick("10"));
    try std.testing.expectEqual(@as(?[2]f32, null), parseMinimapClick("axb"));
    try std.testing.expectEqual(MinimapMode.game, MinimapMode.fromName("game").?);
    try std.testing.expectEqual(MinimapMode.editor, MinimapMode.fromName("editor").?);
    try std.testing.expectEqual(@as(?MinimapMode, null), MinimapMode.fromName("Game"));
}

test "the minimap pictures are named after the map without its extension" {
    try std.testing.expectEqualStrings("/maps/a", minimapImageBase("/maps/a.bzm").?);
    try std.testing.expectEqualStrings("C:\\maps\\a", minimapImageBase("C:\\maps\\a.XML").?);
    try std.testing.expectEqual(@as(?[]const u8, null), minimapImageBase("/maps/a.txt"));
    try std.testing.expectEqual(@as(?[]const u8, null), minimapImageBase("/maps.bzm/a"));
}

// ---------------------------------------------------------------------------
// The Layers menu (M3, D-32, 05-06): its model, with no window.
// ---------------------------------------------------------------------------

/// The Layers menu's toggle entries in the MFC menu's own order (the fire
/// ranges are the submenu after them, not a check).
pub const layer_menu_order = [_]core.layers.Layer{
    .terrain,         .grid,           .wireframe,      .depth_complexity,
    .terrain_noise,   .black_stripes,  .units,          .objects,
    .bounding_boxes,  .shadows,        .haze,           .war_fog,
    .units_passability,
};

pub const LayerMenuItem = struct {
    layer: core.layers.Layer,
    label: [:0]const u8,
    /// Shown ticks; a layer the renderer cannot draw never reads as shown.
    checked: bool,
    /// Greyed with no map open and for a layer outside the renderer's mask.
    enabled: bool,
    tooltip: [:0]const u8,
};

/// What each entry says when hovered: what it is, or - for the layer the
/// renderer cannot draw - the measurement that says why it is greyed (05-06's
/// TestM3LayerProbe).
pub fn layerTooltip(layer: core.layers.Layer, available: bool) [:0]const u8 {
    if (!available) {
        return switch (layer) {
            .depth_complexity => "The GPU renderer has no overdraw counter: switching this on paints the whole frame white (measured by the layer probe, 05-06), so it is not offered.",
            else => "This renderer cannot draw this layer.",
        };
    }
    return switch (layer) {
        .terrain => "The ground. Off leaves objects floating on black.",
        .grid => "The tile grid over the terrain.",
        .wireframe => "Everything as its triangle edges.",
        .depth_complexity => "How many times each pixel is drawn.",
        .terrain_noise => "The noise texture over the terrain.",
        .black_stripes => "The black border drawn outside the map's edge.",
        .units => "Infantry and vehicles.",
        .objects => "Buildings, trees and every other object.",
        .bounding_boxes => "Every object's bounding box.",
        .shadows => "Object shadows.",
        .haze => "The depth haze over the distance.",
        .war_fog => "The game's war fog (the editor sees every object either way).",
        .units_passability => "The tiles units cannot cross, marked on the ground.",
        .fire_ranges => "The firing ranges of the chosen units.",
    };
}

pub fn layerMenuItems(state: *const core.layers.State, mask: u32, map_open: bool) [layer_menu_order.len]LayerMenuItem {
    var items: [layer_menu_order.len]LayerMenuItem = undefined;
    for (layer_menu_order, 0..) |layer, index| {
        const available = mask & core.layers.bit(layer) != 0;
        items[index] = .{
            .layer = layer,
            .label = core.layers.label(layer),
            .checked = available and state.shown(layer),
            .enabled = map_open and available,
            .tooltip = layerTooltip(layer, available),
        };
    }
    return items;
}

/// The fire-range submenu's three radio entries' labels.
pub fn fireModeLabel(mode: core.layers.FireMode) [:0]const u8 {
    return switch (mode) {
        .off => "Off",
        .selected => "Selected units",
        .filter => "Filter",
    };
}

/// The submenu's own title with what is on: "Unit Fire Ranges" when off,
/// "Unit Fire Ranges: selected units" / "...: <filter name>" otherwise.
pub fn fireRangeTitle(buffer: []u8, state: *const core.layers.State) []const u8 {
    return switch (state.fire_mode) {
        .off => "Unit Fire Ranges",
        .selected => std.fmt.bufPrint(buffer, "Unit Fire Ranges: selected units", .{}) catch "Unit Fire Ranges",
        .filter => std.fmt.bufPrint(buffer, "Unit Fire Ranges: {s}", .{state.fireFilter()}) catch "Unit Fire Ranges",
    };
}

/// A filter name as a script writes it: spaces cannot appear in an auto
/// argument, so `Axis_Units` names "Axis Units" - the exact spelling wins
/// when a filter is named with the underscore itself. `buffer` holds the
/// result; null when no filter matches either spelling.
pub fn resolveFilterName(buffer: []u8, filters: []const core.bridge.ObjectFilter, arg: []const u8) ?[]const u8 {
    for (filters) |*one| {
        if (std.mem.eql(u8, one.nameSlice(), arg)) return one.nameSlice();
    }
    if (arg.len > buffer.len) return null;
    for (arg, 0..) |ch, i| buffer[i] = if (ch == '_') ' ' else ch;
    const spaced = buffer[0..arg.len];
    for (filters) |*one| {
        if (std.mem.eql(u8, one.nameSlice(), spaced)) return one.nameSlice();
    }
    return null;
}

test "layer menu: the MFC's thirteen toggles in its order, the fire ranges apart" {
    try std.testing.expectEqual(@as(usize, 13), layer_menu_order.len);
    for (layer_menu_order) |layer| try std.testing.expect(core.layers.isToggle(layer));
    try std.testing.expectEqual(core.layers.Layer.terrain, layer_menu_order[0]);
    try std.testing.expectEqual(core.layers.Layer.war_fog, layer_menu_order[11]);
    try std.testing.expectEqual(core.layers.Layer.units_passability, layer_menu_order[12]);
    // Every toggle layer is in the menu exactly once.
    var seen: u32 = 0;
    for (layer_menu_order) |layer| {
        try std.testing.expect(seen & core.layers.bit(layer) == 0);
        seen |= core.layers.bit(layer);
    }
    try std.testing.expectEqual(core.layers.all_bits & ~core.layers.bit(.fire_ranges), seen);
}

test "layer menu: ticks follow the state, a greyed layer never ticks, no map greys everything" {
    var state: core.layers.State = .{};
    state.set(.grid, true);
    state.set(.depth_complexity, true);
    const mask = core.layers.all_bits & ~core.layers.bit(.depth_complexity);
    const items = layerMenuItems(&state, mask, true);
    for (items) |item| {
        switch (item.layer) {
            .grid => try std.testing.expect(item.checked and item.enabled),
            .terrain => try std.testing.expect(item.checked and item.enabled),
            .wireframe => try std.testing.expect(!item.checked and item.enabled),
            // Remembered as on, but the renderer cannot draw it: unticked and greyed, with the finding.
            .depth_complexity => {
                try std.testing.expect(!item.checked and !item.enabled);
                try std.testing.expect(std.mem.indexOf(u8, item.tooltip, "white") != null);
            },
            else => {},
        }
        try std.testing.expect(item.label.len != 0 and item.tooltip.len != 0);
    }
    const closed = layerMenuItems(&state, mask, false);
    for (closed) |item| try std.testing.expect(!item.enabled);
}

test "layer menu: the fire-range title says what is on" {
    var buffer: [128]u8 = undefined;
    var state: core.layers.State = .{};
    try std.testing.expectEqualStrings("Unit Fire Ranges", fireRangeTitle(&buffer, &state));
    state.setFireRange(.selected, "");
    try std.testing.expectEqualStrings("Unit Fire Ranges: selected units", fireRangeTitle(&buffer, &state));
    state.setFireRange(.filter, "Axis Units");
    try std.testing.expectEqualStrings("Unit Fire Ranges: Axis Units", fireRangeTitle(&buffer, &state));
    try std.testing.expectEqualStrings("Off", fireModeLabel(.off));
    try std.testing.expectEqualStrings("Selected units", fireModeLabel(.selected));
    try std.testing.expectEqualStrings("Filter", fireModeLabel(.filter));
}

test "layer menu: a script names a filter with underscores for spaces, the exact name first" {
    var spaced: core.bridge.ObjectFilter = .{};
    spaced.setName("Axis Units");
    var underscored: core.bridge.ObjectFilter = .{};
    underscored.setName("Odd_Name");
    var plain: core.bridge.ObjectFilter = .{};
    plain.setName("Buildings");
    const filters = [_]core.bridge.ObjectFilter{ spaced, underscored, plain };
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("Axis Units", resolveFilterName(&buffer, &filters, "Axis_Units").?);
    try std.testing.expectEqualStrings("Axis Units", resolveFilterName(&buffer, &filters, "Axis Units").?);
    try std.testing.expectEqualStrings("Odd_Name", resolveFilterName(&buffer, &filters, "Odd_Name").?);
    try std.testing.expectEqualStrings("Buildings", resolveFilterName(&buffer, &filters, "Buildings").?);
    try std.testing.expect(resolveFilterName(&buffer, &filters, "Nothing_Here") == null);
    try std.testing.expect(resolveFilterName(&buffer, &filters, "") == null);
}

// ---------------------------------------------------------------------------
// Create Random Map (05-08, D-01..D-05) and Tools > Export lists (D-13)
// ---------------------------------------------------------------------------

/// What the seed field holds: blank draws a fresh seed, digits are the seed
/// (0..4294967295), anything else is refused with the field kept as typed.
pub const SeedText = union(enum) { blank, value: u32, invalid };

pub fn parseSeedText(text: []const u8) SeedText {
    const trimmed = std.mem.trim(u8, text, " ");
    if (trimmed.len == 0) return .blank;
    for (trimmed) |c| {
        if (c < '0' or c > '9') return .invalid;
    }
    const value = std.fmt.parseInt(u32, trimmed, 10) catch return .invalid;
    return .{ .value = value };
}

/// A map name the way the bridge takes one: one path component. The MFC
/// dialog stripped the extension a user typed (ValidatePath cuts at the last
/// '.'), and so does this - ".bzm" or ".xml", either case - and nothing else:
/// a name with a separator reaches the bridge as typed and is refused there.
pub fn cleanMapName(text: []const u8) []const u8 {
    const trimmed = std.mem.trim(u8, text, " ");
    for ([_][]const u8{ ".bzm", ".xml" }) |extension| {
        if (trimmed.len > extension.len and std.ascii.eqlIgnoreCase(trimmed[trimmed.len - extension.len ..], extension))
            return trimmed[0 .. trimmed.len - extension.len];
    }
    return trimmed;
}

/// Create Random Map's fields (D-01): the MFC dialog's own - template,
/// context, setting ("" is `<any setting>`), graph index, direction 0..3
/// (N, E, S, W), difficulty level 0..2 (the dialog shows 1..3), Save as BZM,
/// write DDS and the map name - plus the seed this port adds. A plain value;
/// `seed` is the text as typed.
pub const RmgFields = struct {
    template: NameText = .{},
    context: NameText = .{},
    setting: NameText = .{},
    map_name: NameText = .{},
    /// -1 lets the template's weights pick, as the game's briefing does; the
    /// MFC's own edit took any integer.
    graph: i32 = -1,
    angle: i32 = 0,
    level: i32 = 0,
    save_as_bzm: bool = true,
    write_dds: bool = false,
    /// Replace a map of the same name already in the maps folder (the MFC
    /// overwrote silently; the bridge asks to be told).
    overwrite: bool = false,
    seed: NameText = .{},

    pub const direction_names = [4][]const u8{ "N", "E", "S", "W" };

    /// The MFC's own rule (CCreateRandomMapDialog::UpdateControls): OK waits
    /// for a template, a context and a map name; the setting always has one.
    pub fn okEnabled(self: *const RmgFields) bool {
        return self.template.len != 0 and self.context.len != 0 and cleanMapName(self.map_name.slice()).len != 0;
    }

    /// The bridge's params for these fields, or null when the seed is typed
    /// but is not a number (the caller says so and keeps the dialog).
    pub fn toParams(self: *const RmgFields) ?core.bridge.RmgGenerateParams {
        var params: core.bridge.RmgGenerateParams = .{};
        params.setTemplate(self.template.slice());
        params.setContext(self.context.slice());
        params.setSetting(self.setting.slice());
        params.setMapName(cleanMapName(self.map_name.slice()));
        params.level = self.level;
        params.graph = self.graph;
        params.angle = self.angle;
        params.save_as_bzm = @intFromBool(self.save_as_bzm);
        params.write_dds = @intFromBool(self.write_dds);
        params.overwrite = @intFromBool(self.overwrite);
        switch (parseSeedText(self.seed.slice())) {
            .blank => params.has_seed = 0,
            .value => |value| {
                params.has_seed = 1;
                params.seed = value;
            },
            .invalid => return null,
        }
        return params;
    }

    /// `rmg_set:<field>:<value>` - one of the dialog's fields set the way the
    /// dialog sets it, so a script drives exactly what the person does (the
    /// auto grammar allows 64 characters an argument and no comma or space, which
    /// ten fields on one line would not fit): `template`, `context` (storage
    /// names), `setting` (`any` is `<any setting>`), `graph` (-1 or an index),
    /// `angle` (-1..3), `level` (1..3 as the dialog shows it), `bzm`, `dds` and
    /// `overwrite` (0 or 1), `name`, `seed` (digits, or empty for a fresh one).
    /// False - the field unchanged - when the field is unknown or the value does
    /// not fit it.
    pub fn set(self: *RmgFields, field: []const u8, value: []const u8) bool {
        if (std.mem.eql(u8, field, "template")) {
            self.template.set(value);
        } else if (std.mem.eql(u8, field, "context")) {
            self.context.set(value);
        } else if (std.mem.eql(u8, field, "setting")) {
            self.setting.set(if (std.ascii.eqlIgnoreCase(value, "any")) "" else value);
        } else if (std.mem.eql(u8, field, "graph")) {
            const graph = std.fmt.parseInt(i32, value, 10) catch return false;
            if (graph < -1) return false;
            self.graph = graph;
        } else if (std.mem.eql(u8, field, "angle")) {
            const angle = std.fmt.parseInt(i32, value, 10) catch return false;
            if (angle < -1 or angle > 3) return false;
            self.angle = angle;
        } else if (std.mem.eql(u8, field, "level")) {
            const level = std.fmt.parseInt(i32, value, 10) catch return false;
            if (level < 1 or level > 3) return false;
            self.level = level - 1;
        } else if (std.mem.eql(u8, field, "bzm")) {
            self.save_as_bzm = parseFlag(value) orelse return false;
        } else if (std.mem.eql(u8, field, "dds")) {
            self.write_dds = parseFlag(value) orelse return false;
        } else if (std.mem.eql(u8, field, "overwrite")) {
            self.overwrite = parseFlag(value) orelse return false;
        } else if (std.mem.eql(u8, field, "name")) {
            self.map_name.set(value);
        } else if (std.mem.eql(u8, field, "seed")) {
            if (parseSeedText(value) == .invalid) return false;
            self.seed.set(value);
        } else {
            return false;
        }
        return true;
    }

    fn parseFlag(text: []const u8) ?bool {
        if (std.mem.eql(u8, text, "1")) return true;
        if (std.mem.eql(u8, text, "0")) return false;
        return null;
    }
};

/// A file a Browse dialog picked, as the storage-relative name the combos
/// hold: under `<base_root>Data`, the `.xml` taken off, backslashes, lower
/// case ("scenarios\\templates\\summer\\template02"). Null for a file outside
/// the game's Data folder, a name that is not `.xml`, or one that does not fit.
pub fn storageNameFromBrowse(buffer: []u8, base_root: []const u8, os_path: []const u8) ?[]const u8 {
    var root_buffer: [4096]u8 = undefined;
    const root = std.fmt.bufPrint(&root_buffer, "{s}Data/", .{base_root}) catch return null;
    // Compare with '/' for both separators and without regard to case: the
    // OS dialog and the root need not spell them alike.
    if (os_path.len <= root.len) return null;
    for (root, os_path[0..root.len]) |expected, got| {
        const a = if (expected == '\\') '/' else std.ascii.toLower(expected);
        const b = if (got == '\\') '/' else std.ascii.toLower(got);
        if (a != b) return null;
    }
    const rest = os_path[root.len..];
    const extension = ".xml";
    if (rest.len <= extension.len or !std.ascii.eqlIgnoreCase(rest[rest.len - extension.len ..], extension)) return null;
    const stem = rest[0 .. rest.len - extension.len];
    if (stem.len > buffer.len) return null;
    for (stem, 0..) |c, i| buffer[i] = if (c == '/') '\\' else std.ascii.toLower(c);
    return buffer[0..stem.len];
}

/// The words the result modal and the status line say about a finished
/// generation: the map, the seed to ask for it again, the graph and the
/// direction used.
pub fn rmgResultLine(buffer: []u8, map_name: []const u8, result: *const core.bridge.RmgGenerateResult) []const u8 {
    const angle: usize = if (result.angle >= 0 and result.angle < 4) @intCast(result.angle) else 0;
    return std.fmt.bufPrint(buffer, "random map {s}: seed {d}, graph {d} ({s}), direction {s}", .{
        map_name,
        result.seed,
        result.graph,
        std.fs.path.basename(result.graphNameSlice()),
        RmgFields.direction_names[angle],
    }) catch "random map created";
}

/// The context combo's entries: the MFC listed only the chapters' own
/// `context.xml` files (EnumFilesInDataStorage's "context.xml" extension).
pub fn isContextName(name: []const u8) bool {
    const suffix = "\\context";
    return name.len > suffix.len and std.ascii.eqlIgnoreCase(name[name.len - suffix.len ..], suffix);
}

/// Tools > Export lists (D-13): the four lists the MFC's Tools 0..3 wrote,
/// each to `<UserRoot>mapeditor/logs/` (the MFC wrote into Data\logs, which
/// the editor never touches).
pub const ExportKind = enum {
    graphs,
    contexts,
    patches,
    maps,

    pub fn fromName(name: []const u8) ?ExportKind {
        inline for (comptime std.enums.values(ExportKind)) |kind| {
            if (std.mem.eql(u8, name, @tagName(kind))) return kind;
        }
        return null;
    }

    pub fn fileName(self: ExportKind) []const u8 {
        return switch (self) {
            .graphs => "graphs_list.txt",
            .contexts => "contexts_list.txt",
            .patches => "patches_list.txt",
            .maps => "maps_list.txt",
        };
    }

    /// The MFC's own wording of "created", without its message box.
    pub fn noun(self: ExportKind) []const u8 {
        return switch (self) {
            .graphs => "RMG graphs list",
            .contexts => "RMG contexts list",
            .patches => "RMG patches list",
            .maps => "Maps list",
        };
    }

    pub fn menuLabel(self: ExportKind) [:0]const u8 {
        return switch (self) {
            .graphs => "Graphs list",
            .contexts => "Contexts list",
            .patches => "Patches list",
            .maps => "Maps list",
        };
    }

    /// Where the names come from: the folder and the extensions the MFC
    /// enumerated, in its own order (the .bzm files before the .xml ones).
    pub fn folder(self: ExportKind) [:0]const u8 {
        return switch (self) {
            .graphs => "scenarios\\templates\\",
            .contexts => "scenarios\\chapters\\",
            .patches => "scenarios\\patches\\",
            .maps => "maps\\",
        };
    }

    pub fn extensions(self: ExportKind) []const [:0]const u8 {
        return switch (self) {
            .graphs => &.{".xml"},
            .contexts => &.{"context.xml"},
            .patches, .maps => &.{ ".bzm", ".xml" },
        };
    }
};

/// The two maps the MFC's Maps list left out (MainFrm.cpp OnTool3).
pub const maps_list_skipped = [_][]const u8{ "maps\\river3d.xml", "maps\\road3d.xml" };

/// One name per line, the MFC's `"%s\r\n"`; names in `skipped` are left out.
pub fn writeNameLines(writer: *std.Io.Writer, names: []const []const u8, skipped: []const []const u8) std.Io.Writer.Error!void {
    for (names) |name| {
        var leave_out = false;
        for (skipped) |skip| {
            if (std.mem.eql(u8, name, skip)) leave_out = true;
        }
        if (leave_out) continue;
        try writer.print("{s}\r\n", .{name});
    }
}

/// One template and its graphs for the graphs list.
pub const TemplateGraphs = struct {
    name: []const u8,
    graphs: []const core.bridge.RmgGraph,
};

/// The graphs list (MainFrm.cpp:975-981): each template's file name, then one
/// tab-indented `index weight graph` line per graph.
pub fn writeGraphsList(writer: *std.Io.Writer, templates: []const TemplateGraphs) std.Io.Writer.Error!void {
    for (templates) |template| {
        try writer.print("{s}\r\n", .{template.name});
        for (template.graphs, 0..) |*graph, index| {
            try writer.print("\t{d} {d} {s}\r\n", .{ index, graph.weight, graph.nameSlice() });
        }
    }
}

test "seed text: blank draws one, digits are the seed, anything else is refused" {
    try std.testing.expect(parseSeedText("") == .blank);
    try std.testing.expect(parseSeedText("   ") == .blank);
    try std.testing.expectEqual(@as(u32, 777), parseSeedText("777").value);
    try std.testing.expectEqual(@as(u32, 0), parseSeedText("0").value);
    try std.testing.expectEqual(@as(u32, 4294967295), parseSeedText(" 4294967295 ").value);
    try std.testing.expect(parseSeedText("4294967296") == .invalid);
    try std.testing.expect(parseSeedText("-5") == .invalid);
    try std.testing.expect(parseSeedText("12a") == .invalid);
    try std.testing.expect(parseSeedText("1 2") == .invalid);
}

test "create random map: OK waits for a template, a context and a name - the MFC's own rule" {
    var fields: RmgFields = .{};
    try std.testing.expect(!fields.okEnabled());
    fields.template.set("scenarios\\templates\\summer\\template02");
    try std.testing.expect(!fields.okEnabled());
    fields.context.set("scenarios\\chapters\\allies\\france\\context");
    try std.testing.expect(!fields.okEnabled());
    fields.map_name.set("   ");
    try std.testing.expect(!fields.okEnabled());
    fields.map_name.set("my_map.bzm");
    try std.testing.expect(fields.okEnabled());
    // The setting is never missing: empty is `<any setting>`.
    try std.testing.expectEqual(@as(usize, 0), fields.setting.len);
}

test "create random map: the map name loses a typed extension and nothing more" {
    try std.testing.expectEqualStrings("m", cleanMapName("m.bzm"));
    try std.testing.expectEqualStrings("m", cleanMapName(" m.XML "));
    try std.testing.expectEqualStrings("m.v2", cleanMapName("m.v2"));
    try std.testing.expectEqualStrings("a\\b", cleanMapName("a\\b.bzm"));
    try std.testing.expectEqualStrings(".bzm", cleanMapName(".bzm"));
}

test "create random map: the fields become the bridge's params, the seed as typed" {
    var fields: RmgFields = .{};
    fields.template.set("scenarios\\templates\\summer\\template02");
    fields.context.set("scenarios\\chapters\\allies\\france\\context");
    fields.map_name.set("test.bzm");
    fields.graph = 3;
    fields.angle = 2;
    fields.level = 1;
    fields.write_dds = true;
    fields.seed.set("12345");
    const params = fields.toParams().?;
    try std.testing.expectEqualStrings("scenarios\\templates\\summer\\template02", params.templateSlice());
    try std.testing.expectEqualStrings("", params.settingSlice());
    try std.testing.expectEqualStrings("test", params.mapNameSlice());
    try std.testing.expectEqual(@as(c_int, 3), params.graph);
    try std.testing.expectEqual(@as(c_int, 2), params.angle);
    try std.testing.expectEqual(@as(c_int, 1), params.level);
    try std.testing.expectEqual(@as(c_int, 1), params.has_seed);
    try std.testing.expectEqual(@as(c_uint, 12345), params.seed);
    try std.testing.expectEqual(@as(c_int, 1), params.write_dds);
    try std.testing.expectEqual(@as(c_int, 0), params.overwrite);
    fields.seed.set("");
    try std.testing.expectEqual(@as(c_int, 0), fields.toParams().?.has_seed);
    fields.seed.set("x1");
    try std.testing.expect(fields.toParams() == null);
}

test "create random map: rmg_set sets the dialog's fields one at a time" {
    var fields: RmgFields = .{};
    try std.testing.expect(fields.set("template", "scenarios\\templates\\summer\\template02"));
    try std.testing.expect(fields.set("context", "scenarios\\chapters\\allies\\france\\context"));
    try std.testing.expect(fields.set("graph", "0"));
    try std.testing.expect(fields.set("setting", "any"));
    try std.testing.expect(fields.set("angle", "1"));
    try std.testing.expect(fields.set("level", "3"));
    try std.testing.expect(fields.set("bzm", "1"));
    try std.testing.expect(fields.set("dds", "0"));
    try std.testing.expect(fields.set("overwrite", "1"));
    try std.testing.expect(fields.set("name", "m3_auto_rmg"));
    try std.testing.expect(fields.set("seed", "777"));
    try std.testing.expectEqualStrings("scenarios\\templates\\summer\\template02", fields.template.slice());
    try std.testing.expectEqualStrings("scenarios\\chapters\\allies\\france\\context", fields.context.slice());
    try std.testing.expectEqual(@as(i32, 0), fields.graph);
    try std.testing.expectEqual(@as(usize, 0), fields.setting.len);
    try std.testing.expectEqual(@as(i32, 1), fields.angle);
    try std.testing.expectEqual(@as(i32, 2), fields.level);
    try std.testing.expect(fields.save_as_bzm and !fields.write_dds and fields.overwrite);
    try std.testing.expectEqualStrings("m3_auto_rmg", fields.map_name.slice());
    try std.testing.expectEqualStrings("777", fields.seed.slice());
    try std.testing.expect(fields.okEnabled());
    const params = fields.toParams().?;
    try std.testing.expectEqual(@as(c_uint, 777), params.seed);
    try std.testing.expectEqual(@as(c_int, 1), params.overwrite);
    // A named setting, a blank seed, any graph and angle.
    try std.testing.expect(fields.set("setting", "scenarios\\settings\\summer_france"));
    try std.testing.expectEqualStrings("scenarios\\settings\\summer_france", fields.setting.slice());
    try std.testing.expect(fields.set("setting", "ANY"));
    try std.testing.expectEqual(@as(usize, 0), fields.setting.len);
    try std.testing.expect(fields.set("graph", "-1") and fields.set("angle", "-1") and fields.set("seed", ""));
    try std.testing.expectEqual(@as(i32, -1), fields.graph);
    try std.testing.expectEqual(@as(c_int, 0), fields.toParams().?.has_seed);
    // What does not fit leaves the field as it was.
    try std.testing.expect(!fields.set("level", "0"));
    try std.testing.expect(!fields.set("level", "4"));
    try std.testing.expect(!fields.set("angle", "4"));
    try std.testing.expect(!fields.set("graph", "-2"));
    try std.testing.expect(!fields.set("graph", "x"));
    try std.testing.expect(!fields.set("bzm", "2"));
    try std.testing.expect(!fields.set("seed", "x1"));
    try std.testing.expect(!fields.set("colour", "red"));
    try std.testing.expectEqual(@as(i32, 2), fields.level);
    try std.testing.expectEqual(@as(i32, -1), fields.angle);
}

test "create random map: the result line names the map, the seed to ask again, the graph and the direction" {
    var result: core.bridge.RmgGenerateResult = .{};
    result.seed = 777;
    result.graph = 4;
    result.angle = 3;
    const graph = "scenarios\\graphs\\summer\\graph04";
    @memcpy(result.graph_name[0..graph.len], graph);
    var buffer: [160]u8 = undefined;
    const line = rmgResultLine(&buffer, "m", &result);
    try std.testing.expect(std.mem.indexOf(u8, line, "seed 777") != null);
    try std.testing.expect(std.mem.indexOf(u8, line, "graph 4") != null);
    try std.testing.expect(std.mem.indexOf(u8, line, "direction W") != null);
}

test "create random map: the context combo lists the chapters' own context files" {
    try std.testing.expect(isContextName("scenarios\\chapters\\allies\\france\\context"));
    try std.testing.expect(isContextName("scenarios\\chapters\\x\\CONTEXT"));
    try std.testing.expect(!isContextName("scenarios\\chapters\\allies\\france\\chapter"));
    try std.testing.expect(!isContextName("context"));
}

test "export lists: names one per line with CRLF, the Maps list without the two 3D maps" {
    var out = std.Io.Writer.Allocating.init(std.testing.allocator);
    defer out.deinit();
    const names = [_][]const u8{ "maps\\arena.bzm", "maps\\river3d.xml", "maps\\duel.xml", "maps\\road3d.xml" };
    try writeNameLines(&out.writer, &names, &maps_list_skipped);
    try std.testing.expectEqualStrings("maps\\arena.bzm\r\nmaps\\duel.xml\r\n", out.written());
    out.writer.end = 0;
    try writeNameLines(&out.writer, names[0..2], &.{});
    try std.testing.expectEqualStrings("maps\\arena.bzm\r\nmaps\\river3d.xml\r\n", out.written());
}

test "export lists: the graphs list is each template then its graphs as index, weight and name" {
    var out = std.Io.Writer.Allocating.init(std.testing.allocator);
    defer out.deinit();
    var first: core.bridge.RmgGraph = .{ .weight = 2 };
    @memcpy(first.name[0.."graphs\\a".len], "graphs\\a");
    var second: core.bridge.RmgGraph = .{ .weight = 5 };
    @memcpy(second.name[0.."graphs\\b".len], "graphs\\b");
    const graphs = [_]core.bridge.RmgGraph{ first, second };
    const templates = [_]TemplateGraphs{
        .{ .name = "scenarios\\templates\\t0.xml", .graphs = &graphs },
        .{ .name = "scenarios\\templates\\t1.xml", .graphs = &.{} },
    };
    try writeGraphsList(&out.writer, &templates);
    try std.testing.expectEqualStrings(
        "scenarios\\templates\\t0.xml\r\n\t0 2 graphs\\a\r\n\t1 5 graphs\\b\r\nscenarios\\templates\\t1.xml\r\n",
        out.written(),
    );
}

test "export lists: each kind has the MFC's file, folder and extensions in its order" {
    try std.testing.expectEqualStrings("graphs_list.txt", ExportKind.graphs.fileName());
    try std.testing.expectEqualStrings("contexts_list.txt", ExportKind.contexts.fileName());
    try std.testing.expectEqualStrings("patches_list.txt", ExportKind.patches.fileName());
    try std.testing.expectEqualStrings("maps_list.txt", ExportKind.maps.fileName());
    try std.testing.expectEqualStrings("scenarios\\chapters\\", ExportKind.contexts.folder());
    try std.testing.expectEqualStrings("context.xml", ExportKind.contexts.extensions()[0]);
    try std.testing.expectEqual(@as(usize, 2), ExportKind.maps.extensions().len);
    try std.testing.expectEqualStrings(".bzm", ExportKind.patches.extensions()[0]);
    try std.testing.expectEqualStrings(".xml", ExportKind.patches.extensions()[1]);
    try std.testing.expect(ExportKind.fromName("patches").? == .patches);
    try std.testing.expect(ExportKind.fromName("fields") == null);
}

test "create random map: a browsed file inside Data is the storage name the combos hold" {
    var buffer: [192]u8 = undefined;
    try std.testing.expectEqualStrings(
        "scenarios\\templates\\summer\\template02",
        storageNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/Scenarios/Templates/Summer/Template02.xml").?,
    );
    // A Windows path against a Windows root: backslashes, any case.
    try std.testing.expectEqualStrings(
        "scenarios\\chapters\\allies\\france\\context",
        storageNameFromBrowse(&buffer, "D:\\GOG\\Blitzkrieg\\", "d:\\gog\\blitzkrieg\\DATA\\Scenarios\\Chapters\\Allies\\France\\context.xml").?,
    );
    // Outside Data, not an xml, or only the folder.
    try std.testing.expect(storageNameFromBrowse(&buffer, "/g/blitz/", "/elsewhere/Data/Scenarios/Templates/a.xml") == null);
    try std.testing.expect(storageNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/Scenarios/Templates/a.txt") == null);
    try std.testing.expect(storageNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/.xml") == null);
    try std.testing.expect(storageNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/") == null);
}

// ---------------------------------------------------------------------------
// The RMG composers (05-09): the pure rules behind the two windows
// ---------------------------------------------------------------------------

/// The graph canvas's square on screen: `origin` its top-left corner in
/// screen pixels, `side` its pixel size, `limit` the tiles across (patches
/// shown x 16). Tile y runs UP from the bottom edge, as the MFC canvas has it.
pub const CanvasGeometry = struct {
    origin_x: f32,
    origin_y: f32,
    side: f32,
    limit: i32,

    pub fn scale(self: CanvasGeometry) f32 {
        return self.side / @as(f32, @floatFromInt(@max(self.limit, 1)));
    }

    /// The tile under a screen point (RMG_CreateGraphDialog.cpp GetTilePoint):
    /// x from the left, y from the bottom, both clamped onto the canvas.
    pub fn tileAt(self: CanvasGeometry, x: f32, y: f32) core.rmg.Tile {
        const limit = @max(self.limit, 1);
        const fx = (x - self.origin_x) / self.scale();
        const fy = (y - self.origin_y) / self.scale();
        const tx: i32 = @intFromFloat(std.math.clamp(@floor(fx), 0, @as(f32, @floatFromInt(limit - 1))));
        const row: i32 = @intFromFloat(std.math.clamp(@floor(fy), 0, @as(f32, @floatFromInt(limit - 1))));
        return .{ .x = tx, .y = limit - 1 - row };
    }

    /// A node's rectangle on screen: left, top, right, bottom in pixels.
    pub fn rectOnScreen(self: CanvasGeometry, rect: core.rmg.Rect) [4]f32 {
        const s = self.scale();
        const bottom_edge = self.origin_y + self.side;
        return .{
            self.origin_x + @as(f32, @floatFromInt(rect.x1)) * s,
            bottom_edge - @as(f32, @floatFromInt(rect.y2)) * s,
            self.origin_x + @as(f32, @floatFromInt(rect.x2)) * s,
            bottom_edge - @as(f32, @floatFromInt(rect.y1)) * s,
        };
    }

    /// A tile point (node centres) on screen.
    pub fn pointOnScreen(self: CanvasGeometry, x: f32, y: f32) [2]f32 {
        const s = self.scale();
        return .{ self.origin_x + x * s, self.origin_y + self.side - y * s };
    }

    /// The link hit tolerance in tiles for a half width in pixels (the MFC's
    /// CG_GRAPH_LINK_HALF_WIDTH = 2).
    pub fn toleranceTiles(self: CanvasGeometry, half_width_pixels: f32) f32 {
        return half_width_pixels / self.scale();
    }
};

/// "3; 4; 1000" - a script ID list the MFC list columns show
/// (RMGGetUsedScriptIDsString's separator).
pub fn idsText(buffer: []u8, ids: []const i32) []const u8 {
    var len: usize = 0;
    for (ids, 0..) |id, i| {
        const piece = std.fmt.bufPrint(buffer[len..], "{s}{d}", .{ if (i == 0) "" else "; ", id }) catch return buffer[0..len];
        len += piece.len;
    }
    return buffer[0..len];
}

pub fn areasText(buffer: []u8, areas: []const []u8) []const u8 {
    var len: usize = 0;
    for (areas, 0..) |area, i| {
        const piece = std.fmt.bufPrint(buffer[len..], "{s}{s}", .{ if (i == 0) "" else "; ", area }) catch return buffer[0..len];
        len += piece.len;
    }
    return buffer[0..len];
}

pub fn namesText(buffer: []u8, names: []const []u8) []const u8 {
    return areasText(buffer, names);
}

/// The host path a copy-in will create, for the YES/NO popup (D-10): the user
/// RMG root, the destination's storage name with its backslashes as slashes,
/// and the source's own extension. Null when it does not fit.
pub fn copyInDestination(buffer: []u8, rmg_root: []const u8, dest_name: []const u8, source_path: []const u8) ?[]const u8 {
    const extension = std.fs.path.extension(source_path);
    const root = std.mem.trimEnd(u8, rmg_root, "/\\");
    const need = root.len + 1 + dest_name.len + extension.len;
    if (need > buffer.len) return null;
    @memcpy(buffer[0..root.len], root);
    buffer[root.len] = '/';
    for (dest_name, 0..) |c, i| buffer[root.len + 1 + i] = if (c == '\\') '/' else c;
    // The bridge keeps the extension lower-cased (a .BZM is written as .bzm).
    for (extension, 0..) |c, i| buffer[root.len + 1 + dest_name.len + i] = std.ascii.toLower(c);
    return buffer[0..need];
}

/// What a picked patch map is: a storage name when it lies under the game's
/// Data folder and is a .bzm or .xml (the add goes straight through), else
/// null - the caller offers the copy-in. `buffer` holds the name.
pub fn patchNameFromBrowse(buffer: []u8, base_root: []const u8, os_path: []const u8) ?[]const u8 {
    var root_buffer: [4096]u8 = undefined;
    const root = std.fmt.bufPrint(&root_buffer, "{s}Data/", .{base_root}) catch return null;
    if (os_path.len <= root.len) return null;
    for (root, os_path[0..root.len]) |expected, got| {
        const a = if (expected == '\\') '/' else std.ascii.toLower(expected);
        const b = if (got == '\\') '/' else std.ascii.toLower(got);
        if (a != b) return null;
    }
    const rest = os_path[root.len..];
    const stem_len = if (rest.len > 4 and (std.ascii.eqlIgnoreCase(rest[rest.len - 4 ..], ".bzm") or std.ascii.eqlIgnoreCase(rest[rest.len - 4 ..], ".xml"))) rest.len - 4 else return null;
    if (stem_len > buffer.len) return null;
    for (rest[0..stem_len], 0..) |c, i| buffer[i] = if (c == '/') '\\' else std.ascii.toLower(c);
    return buffer[0..stem_len];
}

/// The Containers Composer's table cell for a direction: "Yes" or blank.
pub fn directionCell(has: bool) []const u8 {
    return if (has) "Yes" else "";
}

/// The patch table's size cell, the MFC's "%4dx%-4d".
pub fn patchSizeText(buffer: []u8, size_x: i32, size_y: i32) []const u8 {
    // Unsigned: a signed number with a width prints a "+" sign.
    const x: u32 = @intCast(@max(size_x, 0));
    const y: u32 = @intCast(@max(size_y, 0));
    return std.fmt.bufPrint(buffer, "{d:>4}x{d:<4}", .{ x, y }) catch buffer[0..0];
}

/// A link dialog's cell value as the MFC shows it: radius and min length in
/// cells to two places, whole numbers kept whole (GetRoundFloat).
pub fn cellsText(buffer: []u8, world_units: f32) []const u8 {
    const cells = world_units / core.rmg.world_cell;
    const rounded = @round(cells);
    const value = if (@abs(cells - rounded) < 0.001) rounded else cells;
    return std.fmt.bufPrint(buffer, "{d:.2}", .{value}) catch buffer[0..0];
}

test "composers: the canvas maps pixels to tiles with y up, clamped, and rectangles back" {
    const geometry: CanvasGeometry = .{ .origin_x = 100, .origin_y = 50, .side = 256, .limit = 128 };
    try std.testing.expectEqual(@as(f32, 2), geometry.scale());
    // The top-left pixel is the top row: the last tile row, not 0.
    const top_left = geometry.tileAt(100, 50);
    try std.testing.expectEqual(@as(i32, 0), top_left.x);
    try std.testing.expectEqual(@as(i32, 127), top_left.y);
    const bottom_left = geometry.tileAt(100, 50 + 255);
    try std.testing.expectEqual(@as(i32, 0), bottom_left.y);
    const middle = geometry.tileAt(100 + 128, 50 + 128);
    try std.testing.expectEqual(@as(i32, 64), middle.x);
    try std.testing.expectEqual(@as(i32, 63), middle.y);
    // Outside the square is clamped onto it.
    const outside = geometry.tileAt(-500, 9000);
    try std.testing.expectEqual(@as(i32, 0), outside.x);
    try std.testing.expectEqual(@as(i32, 0), outside.y);
    const far_corner = geometry.tileAt(9000, -9000);
    try std.testing.expectEqual(@as(i32, 127), far_corner.x);
    try std.testing.expectEqual(@as(i32, 127), far_corner.y);
    // A node rectangle: its bottom edge is tile y1.
    const rect = geometry.rectOnScreen(.{ .x1 = 0, .y1 = 0, .x2 = 16, .y2 = 16 });
    try std.testing.expectEqual(@as(f32, 100), rect[0]);
    try std.testing.expectEqual(@as(f32, 50 + 256 - 32), rect[1]);
    try std.testing.expectEqual(@as(f32, 132), rect[2]);
    try std.testing.expectEqual(@as(f32, 50 + 256), rect[3]);
    try std.testing.expectEqual(@as(f32, 1), geometry.toleranceTiles(2));
    // The pixel in the middle of a tile reads back as that tile.
    const point = geometry.pointOnScreen(8.5, 8.5);
    const tile = geometry.tileAt(point[0], point[1]);
    try std.testing.expectEqual(@as(i32, 8), tile.x);
    try std.testing.expectEqual(@as(i32, 8), tile.y);
}

test "composers: the list columns say ids, areas and sizes the MFC's way" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("3; 4; 1000", idsText(&buffer, &.{ 3, 4, 1000 }));
    try std.testing.expectEqualStrings("", idsText(&buffer, &.{}));
    var a = "Ambush".*;
    var b = "Bridge".*;
    const areas = [_][]u8{ &a, &b };
    try std.testing.expectEqualStrings("Ambush; Bridge", areasText(&buffer, &areas));
    try std.testing.expectEqualStrings("   2x3   ", patchSizeText(&buffer, 2, 3));
    try std.testing.expectEqualStrings("Yes", directionCell(true));
    try std.testing.expectEqualStrings("", directionCell(false));
    // 181.019 world units is 5.66 cells; a whole number of cells stays whole.
    try std.testing.expectEqualStrings("5.66", cellsText(&buffer, 181.019));
    try std.testing.expectEqualStrings("4.00", cellsText(&buffer, 128.0));
}

test "composers: the copy-in destination is under the user RMG root, with the map's own extension" {
    var buffer: [256]u8 = undefined;
    try std.testing.expectEqualStrings(
        "/home/me/.local/share/blitz/rmg/scenarios/patches/summer/mine.bzm",
        copyInDestination(&buffer, "/home/me/.local/share/blitz/rmg", "scenarios\\patches\\summer\\mine", "/elsewhere/Mine.BZM").?,
    );
    // A trailing separator on the root does not double up.
    try std.testing.expectEqualStrings(
        "C:\\Users\\me\\rmg/scenarios/patches/winter/p.xml",
        copyInDestination(&buffer, "C:\\Users\\me\\rmg\\", "scenarios\\patches\\winter\\p", "D:\\maps\\P.xml").?,
    );
    var tiny: [8]u8 = undefined;
    try std.testing.expect(copyInDestination(&tiny, "/root", "scenarios\\patches\\summer\\mine", "/x/mine.bzm") == null);
}

test "composers: a picked patch inside Data is added by name, one outside is offered a copy" {
    var buffer: [192]u8 = undefined;
    try std.testing.expectEqualStrings(
        "scenarios\\patches\\summer\\road\\p_one",
        patchNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/Scenarios/Patches/Summer/Road/P_One.bzm").?,
    );
    try std.testing.expectEqualStrings(
        "scenarios\\patches\\winter\\p",
        patchNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/Scenarios/Patches/Winter/p.xml").?,
    );
    try std.testing.expect(patchNameFromBrowse(&buffer, "/g/blitz/", "/elsewhere/p.bzm") == null);
    try std.testing.expect(patchNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/Scenarios/Patches/p.txt") == null);
    try std.testing.expect(patchNameFromBrowse(&buffer, "/g/blitz/", "/g/blitz/Data/.bzm") == null);
}

// ---------------------------------------------------------------------------
// 05-11 (D-34): the app shell - View menu panels, drop, Options, Help, About
// ---------------------------------------------------------------------------

/// The panels that are always on screen and the status bar: what View can
/// hide (PARITY V1/V2). The floating windows each have their own flag in
/// panels.zig's State and a View entry; these are the docked ones. One bit
/// each in `Settings.hidden_panels`, in this order - append, never reorder
/// (the bits are written to mapeditor.cfg).
pub const PanelId = enum(u5) { tools, objects, properties, players, camera_anchors, sounds, status_bar };

pub fn panelLabel(id: PanelId) [:0]const u8 {
    return switch (id) {
        .tools => "Tools palette",
        .objects => "Objects",
        .properties => "Properties panel",
        .players => "Players",
        .camera_anchors => "Camera anchors",
        .sounds => "Sounds",
        .status_bar => "Status bar",
    };
}

/// The name the `view_panel` command and the `panel_visible` predicate use.
pub fn panelByName(name: []const u8) ?PanelId {
    inline for (comptime std.enums.values(PanelId)) |id| {
        if (std.mem.eql(u8, name, @tagName(id))) return id;
    }
    return null;
}

pub fn panelHidden(bits: u32, id: PanelId) bool {
    return bits & (@as(u32, 1) << @intFromEnum(id)) != 0;
}

pub fn withPanelHidden(bits: u32, id: PanelId, hidden: bool) u32 {
    const mask = @as(u32, 1) << @intFromEnum(id);
    return if (hidden) bits | mask else bits & ~mask;
}

/// Bits past the known panels are never kept: a hand-edited or newer file
/// cannot hide something this build does not draw.
pub fn knownPanelBits(bits: u32) u32 {
    const all: u32 = (@as(u32, 1) << @intCast(std.enums.values(PanelId).len)) - 1;
    return bits & all;
}

/// What dropping a file onto the window does (PARITY F12).
pub const DropVerdict = enum { open, not_a_map, bad_path };

/// A dropped path opens only when it names a map (`.bzm` or `.xml`, either
/// case), fits the path buffers and holds no control character. Anything else
/// is ignored with a status note: a drop is another process's input.
pub fn dropVerdict(os_path: []const u8) DropVerdict {
    if (os_path.len == 0 or os_path.len >= PathSlot.max_path) return .bad_path;
    for (os_path) |byte| {
        if (byte < 0x20 or byte == 0x7f) return .bad_path;
    }
    return if (hasMapExtension(os_path)) .open else .not_a_map;
}

/// The status line for a drop that did not open: the verdict and the file's
/// own name (cut to fit).
pub fn dropNote(buffer: []u8, verdict: DropVerdict, os_path: []const u8) []const u8 {
    return switch (verdict) {
        .open => "",
        .not_a_map => std.fmt.bufPrint(buffer, "not a map (.bzm or .xml): {s}", .{baseName(os_path)}) catch "not a map (.bzm or .xml)",
        .bad_path => "the dropped path cannot be opened",
    };
}

/// Applies Tools > Options' two fields to `settings`: true when something
/// changed (so the settings file is written once, only then).
pub fn applyOptions(settings: *core.settings.Settings, game_parameters: []const u8, format: core.settings.Format) bool {
    var cleaned: core.settings.Settings = .{};
    cleaned.setGameParameters(game_parameters);
    var changed = false;
    if (!std.mem.eql(u8, cleaned.gameParameters(), settings.gameParameters())) {
        settings.setGameParameters(game_parameters);
        changed = true;
    }
    if (settings.default_format != format) {
        settings.default_format = format;
        changed = true;
    }
    return changed;
}

pub fn parseFormat(name: []const u8) ?core.settings.Format {
    if (std.mem.eql(u8, name, "bzm")) return .bzm;
    if (std.mem.eql(u8, name, "xml")) return .xml;
    return null;
}

/// About (PARITY H2): the product line, where the design is written and the
/// licence the repository carries.
pub const about_title = "Blitzkrieg Map Editor";
pub const about_version = "portable editor, milestone M3 (random map templates, minimap, tools: parity with the MFC editor)";
pub const about_spec = "Design: docs/superpowers/specs/2026-09-19-portable-map-editor-design.md";
pub const about_source = "Source: github.com/jmfrank63/Blitzkrieg";
pub const about_license = "Blitzkrieg and its data belong to Nival International Ltd.; use is licensed for noncommercial purposes only (LICENSE.md).";

test "panels: the View menu's panel bits hide and show one panel each, and unknown bits are cut" {
    var bits: u32 = 0;
    inline for (comptime std.enums.values(PanelId)) |id| {
        try std.testing.expect(!panelHidden(bits, id));
        bits = withPanelHidden(bits, id, true);
        try std.testing.expect(panelHidden(bits, id));
    }
    try std.testing.expectEqual(knownPanelBits(0xffff_ffff), bits);
    bits = withPanelHidden(bits, .sounds, false);
    try std.testing.expect(!panelHidden(bits, .sounds));
    try std.testing.expect(panelHidden(bits, .status_bar));
    try std.testing.expectEqual(@as(u32, 0), knownPanelBits(@as(u32, 1) << 20));
}

test "panels: every panel has a label and a unique name the command finds" {
    inline for (comptime std.enums.values(PanelId)) |id| {
        try std.testing.expect(panelLabel(id).len != 0);
        try std.testing.expectEqual(@as(?PanelId, id), panelByName(@tagName(id)));
    }
    try std.testing.expectEqual(@as(?PanelId, null), panelByName("nothing"));
    inline for (comptime std.enums.values(PanelId), 0..) |a, i| {
        inline for (comptime std.enums.values(PanelId), 0..) |b, j| {
            if (i != j) try std.testing.expect(!std.mem.eql(u8, panelLabel(a), panelLabel(b)));
        }
    }
}

test "drop: a map opens, anything else is ignored with a note, a hostile path is refused" {
    try std.testing.expectEqual(DropVerdict.open, dropVerdict("/maps/a.bzm"));
    try std.testing.expectEqual(DropVerdict.open, dropVerdict("C:\\maps\\B.XML"));
    try std.testing.expectEqual(DropVerdict.not_a_map, dropVerdict("/maps/a.lua"));
    try std.testing.expectEqual(DropVerdict.not_a_map, dropVerdict("/maps/folder"));
    try std.testing.expectEqual(DropVerdict.bad_path, dropVerdict(""));
    try std.testing.expectEqual(DropVerdict.bad_path, dropVerdict("/maps/a\n.bzm"));
    try std.testing.expectEqual(DropVerdict.bad_path, dropVerdict("/maps/a\x00.bzm"));
    var long: [PathSlot.max_path + 8]u8 = undefined;
    @memset(&long, 'x');
    @memcpy(long[long.len - 4 ..], ".bzm");
    try std.testing.expectEqual(DropVerdict.bad_path, dropVerdict(&long));

    var buffer: [128]u8 = undefined;
    try std.testing.expectEqualStrings("not a map (.bzm or .xml): a.lua", dropNote(&buffer, .not_a_map, "/maps/a.lua"));
    try std.testing.expectEqualStrings("", dropNote(&buffer, .open, "/maps/a.bzm"));
}

test "drop: a dropped map goes through the unsaved-changes guard like Open Recent" {
    var slot: PathSlot = .{};
    var actions: FileActions = .{ .dialog = &slot };
    // A dirty document asks first; the map opens only after Don't save.
    actions.requestOpenPath("/maps/dropped.bzm");
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    actions.answer_pending = .dont_save;
    const step = actions.next(true, false);
    try std.testing.expectEqual(DialogKind.open, step.act_on_path.kind);
    try std.testing.expectEqualStrings("/maps/dropped.bzm", step.act_on_path.path);
    // A Cancel opens nothing.
    actions.requestOpenPath("/maps/dropped.bzm");
    try std.testing.expectEqual(FileActions.Step.ask_unsaved, actions.next(true, false));
    actions.answer_pending = .cancel;
    try std.testing.expectEqual(FileActions.Step.none, actions.next(true, false));
}

test "options: the two fields apply as one change, an equal value changes nothing" {
    var settings: core.settings.Settings = .{};
    try std.testing.expect(!applyOptions(&settings, "", .bzm));
    try std.testing.expect(applyOptions(&settings, " -nosound ", .bzm));
    try std.testing.expectEqualStrings("-nosound", settings.gameParameters());
    try std.testing.expect(!applyOptions(&settings, "-nosound", .bzm));
    try std.testing.expect(applyOptions(&settings, "-nosound", .xml));
    try std.testing.expectEqual(core.settings.Format.xml, settings.default_format);
    try std.testing.expect(applyOptions(&settings, "", .xml));
    try std.testing.expectEqualStrings("", settings.gameParameters());
    try std.testing.expectEqual(@as(?core.settings.Format, .xml), parseFormat("xml"));
    try std.testing.expectEqual(@as(?core.settings.Format, null), parseFormat("zip"));
}

test "options: the saved parameters reach the game as argv elements, in the file and in the argv" {
    // The settings round trip (what Options wrote is what a restart reads) and
    // the argv the test launch builds from it.
    var settings: core.settings.Settings = .{};
    _ = applyOptions(&settings, "-nosound \"-x y\"", .xml);
    var buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try core.settings.format(&settings, &writer);
    const back = core.settings.parse(writer.buffered());
    try std.testing.expectEqualStrings("-nosound \"-x y\"", back.gameParameters());
    try std.testing.expectEqual(core.settings.Format.xml, back.default_format);
    var storage: testlaunch.ArgvStorage = .{};
    const argv = testlaunch.buildArgv(&storage, .{ .game_path = "/g/Game", .game_parameters = back.gameParameters(), .log_path = "l" });
    try std.testing.expectEqualStrings("-nosound", argv[argv.len - 3]);
    try std.testing.expectEqualStrings("-x y", argv[argv.len - 2]);
    try std.testing.expectEqualStrings(testlaunch.map_file_name, argv[argv.len - 1]);
}

test "help: the key map and the registry's digit keys list every tool once" {
    // The window builds its tool lines from tool_registry.entries; every tool
    // with a key must have one that byShortcut finds again.
    var with_key: usize = 0;
    for (tool_registry.entries) |item| {
        if (item.hidden) continue;
        if (item.shortcut) |key| {
            with_key += 1;
            try std.testing.expectEqual(@as(?tool_registry.ToolId, item.id), tool_registry.byShortcut(key));
        }
    }
    try std.testing.expect(with_key >= 9);
    try std.testing.expect(view_math.key_help.len >= 15);
    try std.testing.expect(about_title.len != 0 and about_license.len != 0);
}
