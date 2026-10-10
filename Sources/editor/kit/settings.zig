//! The editor kit's generic `key=value` settings primitive: scroll speed,
//! autosave on/off and interval, the default folder, the recent-files list,
//! the default-format enum, the game-parameters string, hidden-panels bits,
//! and the `FixedPath` buffer the file's free-text fields share. Any editor
//! on the kit (MapEditor, Resource, Mission, ...) can hold a `Settings` of
//! its own and parse/write it; the fields no other editor needs (MapEditor's
//! layer-visibility, filter slots, instant-update/fit-to-grid) sit in the
//! editor's own composed Settings beside this one.
//!
//! The format is `key=value`, one per line, `#`-prefixed full-line comments
//! allowed, CRLF tolerated. `parse` is lenient (T-03-07-01): a malformed or
//! truncated file degrades to defaults for whatever it could not make sense
//! of, never an error the caller has to handle. The app caps what it reads
//! before handing text here (64 KiB).
//!
//! Kit callers who need to share the kit's generic parser/writer with their
//! own composed Settings use `applyGenericKey` and `writeGenericKeys`, which
//! duck-type over any struct that has the generic fields by name. The
//! composed MapEditor Settings in `core/settings.zig` works this way, so the
//! on-disk format of `mapeditor.cfg` is byte-for-byte unchanged. An editor
//! with no map format and no maps folder (the Resource Editor) uses the
//! narrower `applySharedKey`, `writeSharedHeadKeys` and `writeSharedTailKeys`
//! the generic ones are built from.
const std = @import("std");
const builtin = @import("builtin");

pub const min_scroll_speed: f32 = 0.25;
pub const max_scroll_speed: f32 = 4.0;
pub const default_scroll_speed: f32 = 1.0;
pub const min_autosave_minutes: u32 = 1;
pub const max_autosave_minutes: u32 = 60;
pub const default_autosave_minutes: u32 = 2;

/// D-27: the last ten entries kept.
pub const recent_capacity = 10;

/// D-24 (M3): the format a never-saved document's Save As writes when the
/// path typed names no extension - the MFC Options field. The bridge picks
/// the format from the path's extension, so this is only ever the default.
pub const Format = enum { bzm, xml };

pub const default_format: Format = .bzm;

pub fn formatExtension(which: Format) []const u8 {
    return switch (which) {
        .bzm => ".bzm",
        .xml => ".xml",
    };
}

/// D-34 (05-11): the longest extra game command line the Options field keeps
/// - a game command line is a few switches, never a script.
pub const max_game_parameters = 256;

/// `BkEditorPathSet`'s own `char[1024]` (bridge.h) - big enough for any real
/// path, and matches the engine's own convention for a fixed path buffer.
pub const max_path = 1024;

/// A fixed-capacity path buffer used by every free-text field in the file
/// (the maps folder, every recent entry, the active filter name, each filter
/// slot name, the game parameters string). Keeping at most `max_path` bytes
/// means a `Settings` lives on the stack with no allocation.
pub const FixedPath = struct {
    buffer: [max_path]u8 = undefined,
    len: usize = 0,

    pub fn slice(self: *const FixedPath) []const u8 {
        return self.buffer[0..self.len];
    }

    /// Keeps at most `max_path` bytes and drops control characters: a newline
    /// would split the settings file's own line and plant a key of the
    /// writer's choosing (WR-B03).
    pub fn set(self: *FixedPath, text: []const u8) void {
        var len: usize = 0;
        for (text) |byte| {
            if (isControl(byte)) continue;
            if (len == self.buffer.len) break;
            self.buffer[len] = byte;
            len += 1;
        }
        self.len = len;
    }
};

pub fn isControl(byte: u8) bool {
    return byte < 0x20 or byte == 0x7f;
}

pub fn hasControl(text: []const u8) bool {
    for (text) |byte| {
        if (isControl(byte)) return true;
    }
    return false;
}

/// Case-insensitive on Windows (two spellings of a drive letter or a path
/// segment name the same file), byte-equal elsewhere.
pub fn sameOsPath(a: []const u8, b: []const u8) bool {
    if (builtin.os.tag == .windows) return std.ascii.eqlIgnoreCase(a, b);
    return std.mem.eql(u8, a, b);
}

/// The kit's own standalone settings struct, holding every generic field. An
/// editor that wants nothing else (no layers, no filter slots) can use this
/// directly; MapEditor and anything with its own sidecar builds its own
/// composed struct with the same field names and uses `applyGenericKey` /
/// `writeGenericKeys` to share the parser and writer.
pub const Settings = struct {
    scroll_speed: f32 = default_scroll_speed,
    autosave: bool = true,
    autosave_minutes: u32 = default_autosave_minutes,
    /// D-24 (M3): the default format of a never-saved document's Save As.
    default_format: Format = default_format,
    maps_folder_storage: FixedPath = .{},
    recent_storage: [recent_capacity]FixedPath = @splat(.{}),
    recent_count: usize = 0,
    /// D-34 (05-11, PARITY T2): Tools > Options' extra game command line for
    /// Test in game (the MFC's `szGameParameters`). Empty means none. It
    /// comes from this file or the Options window only and goes to the game
    /// as argv elements, never through a shell (T-05-11-03).
    game_parameters: FixedPath = .{},
    /// D-34 (05-11, PARITY V1/V2): which of the always-present panels and
    /// the status bar the View menu hid, one bit each in the app's own panel
    /// order (panels_logic.PanelId). The core only keeps the number: 0,
    /// nothing hidden, is the default and what Reset layout restores.
    hidden_panels: u32 = 0,

    pub fn gameParameters(self: *const Settings) []const u8 {
        return self.game_parameters.slice();
    }

    /// Keeps at most `max_game_parameters` bytes and drops control characters
    /// (a newline would split the settings file's own line, and no argument
    /// of a game command line holds one).
    pub fn setGameParameters(self: *Settings, text: []const u8) void {
        setGameParametersField(&self.game_parameters, text);
    }

    /// Empty means "use the default folder".
    pub fn mapsFolder(self: *const Settings) []const u8 {
        return self.maps_folder_storage.slice();
    }

    pub fn setMapsFolder(self: *Settings, text: []const u8) void {
        self.maps_folder_storage.set(text);
    }

    pub fn recentCount(self: *const Settings) usize {
        return self.recent_count;
    }

    /// `0` is the most recently opened.
    pub fn recentAt(self: *const Settings, index: usize) []const u8 {
        return self.recent_storage[index].slice();
    }

    pub fn pushRecent(self: *Settings, os_path: []const u8) void {
        pushRecentField(&self.recent_storage, &self.recent_count, os_path);
    }

    pub fn removeRecent(self: *Settings, index: usize) void {
        removeRecentField(&self.recent_storage, &self.recent_count, index);
    }
};

/// Shared implementation of `setGameParameters` used by `Settings` and by
/// any composed struct whose `game_parameters` field is a `FixedPath`.
pub fn setGameParametersField(field: *FixedPath, text: []const u8) void {
    var clean: [max_game_parameters]u8 = undefined;
    var len: usize = 0;
    for (text) |byte| {
        if (byte < 0x20 or byte == 0x7f) continue;
        if (len == clean.len) break;
        clean[len] = byte;
        len += 1;
    }
    field.set(std.mem.trim(u8, clean[0..len], " "));
}

/// Shared implementation of `pushRecent` used by `Settings` and by any
/// composed struct that keeps a `[recent_capacity]FixedPath` list beside a
/// `usize` count.
pub fn pushRecentField(storage: *[recent_capacity]FixedPath, count: *usize, os_path: []const u8) void {
    // A path with a control character (a file name may hold a newline on
    // macOS and Linux) is not kept: removing the character would name a
    // different file, and writing it would split the settings file's line
    // (WR-B03).
    if (hasControl(os_path)) return;
    var existing: ?usize = null;
    var i: usize = 0;
    while (i < count.*) : (i += 1) {
        if (sameOsPath(storage[i].slice(), os_path)) {
            existing = i;
            break;
        }
    }
    // Shifting right by one opens a slot at 0 for the new (or moved) entry;
    // walking from the far end down to 1 means every write lands in a slot
    // whose old value has already moved on, so nothing is lost to being
    // overwritten before it is read.
    const start = existing orelse @min(count.*, recent_capacity - 1);
    var shift = start;
    while (shift > 0) : (shift -= 1) storage[shift] = storage[shift - 1];
    storage[0].set(os_path);
    if (existing == null) count.* = @min(count.* + 1, recent_capacity);
}

pub fn removeRecentField(storage: *[recent_capacity]FixedPath, count: *usize, index: usize) void {
    if (index >= count.*) return;
    var i = index;
    while (i + 1 < count.*) : (i += 1) storage[i] = storage[i + 1];
    count.* -= 1;
}

/// Applies one generic `key=value` pair to any struct whose field names
/// match the generic Settings' fields (`scroll_speed`, `autosave`,
/// `autosave_minutes`, `default_format`, `maps_folder_storage`,
/// `recent_storage`+`recent_count`, `game_parameters`, `hidden_panels`).
/// Returns `true` if the key was one the kit knows (and was applied or
/// deliberately skipped as malformed), `false` otherwise so the composed
/// caller can try its own keys. Unknown keys are never an error: a newer or
/// hand-edited file may carry keys this build does not understand.
pub fn applyGenericKey(settings: anytype, key: []const u8, value: []const u8) bool {
    if (applySharedKey(settings, key, value)) return true;
    if (std.mem.eql(u8, key, "default_format")) {
        // D-24 (M3): bzm or xml; anything else is malformed and skipped,
        // keeping the default, exactly autosave's own rule.
        if (std.mem.eql(u8, value, "bzm")) {
            settings.default_format = .bzm;
        } else if (std.mem.eql(u8, value, "xml")) {
            settings.default_format = .xml;
        }
        return true;
    } else if (std.mem.eql(u8, key, "maps_folder")) {
        settings.maps_folder_storage.set(value);
        return true;
    }
    return false;
}

/// The keys every editor on the kit shares (`scroll_speed`, `autosave`,
/// `autosave_minutes`, `game_parameters`, `hidden_panels`, `recent`), without
/// the two that only mean something to a map editor (`default_format`, a map
/// format, and `maps_folder`). An editor whose composed Settings has no such
/// fields (the Resource Editor's `resourceeditor.cfg`) parses through this
/// one; `applyGenericKey` calls it first, so the map editor's parser reads
/// exactly what it did before the split.
pub fn applySharedKey(settings: anytype, key: []const u8, value: []const u8) bool {
    if (std.mem.eql(u8, key, "scroll_speed")) {
        const parsed = std.fmt.parseFloat(f32, value) catch return true;
        if (!std.math.isFinite(parsed)) return true;
        settings.scroll_speed = std.math.clamp(parsed, min_scroll_speed, max_scroll_speed);
        return true;
    } else if (std.mem.eql(u8, key, "autosave")) {
        if (std.mem.eql(u8, value, "on")) {
            settings.autosave = true;
        } else if (std.mem.eql(u8, value, "off")) {
            settings.autosave = false;
        }
        // Any other value is malformed and skipped, keeping the default.
        return true;
    } else if (std.mem.eql(u8, key, "autosave_minutes")) {
        const parsed = std.fmt.parseInt(u32, value, 10) catch return true;
        settings.autosave_minutes = std.math.clamp(parsed, min_autosave_minutes, max_autosave_minutes);
        return true;
    } else if (std.mem.eql(u8, key, "game_parameters")) {
        setGameParametersField(&settings.game_parameters, value);
        return true;
    } else if (std.mem.eql(u8, key, "hidden_panels")) {
        // A number or nothing: a malformed value keeps every panel shown.
        settings.hidden_panels = std.fmt.parseInt(u32, value, 10) catch return true;
        return true;
    } else if (std.mem.eql(u8, key, "recent")) {
        if (value.len != 0 and settings.recent_count < recent_capacity) {
            settings.recent_storage[settings.recent_count].set(value);
            settings.recent_count += 1;
        }
        // Beyond capacity: parse never reaches here for a well-formed file
        // (format never writes more than recent_capacity lines), but an
        // externally-edited file with extras just stops adding - not a reason
        // to fail the rest of the file.
        return true;
    }
    return false;
}

/// Writes every generic key of `settings` (in a stable order). Composed
/// callers (MapEditor's own Settings) write this first, then their own keys,
/// so `mapeditor.cfg`'s on-disk layout stays byte-for-byte identical to the
/// pre-split order.
pub fn writeGenericKeys(settings: anytype, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try writeSharedHeadKeys(settings, writer);
    try writer.print("default_format={s}\n", .{@tagName(settings.default_format)});
}

/// The head of the shared keys (`applySharedKey`'s): scroll_speed, autosave
/// and autosave_minutes, in `mapeditor.cfg`'s own order.
pub fn writeSharedHeadKeys(settings: anytype, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try writer.print("scroll_speed={d}\n", .{settings.scroll_speed});
    try writer.print("autosave={s}\n", .{if (settings.autosave) "on" else "off"});
    try writer.print("autosave_minutes={d}\n", .{settings.autosave_minutes});
}

/// Writes the trailing generic keys that come after a composed caller's own
/// mid-section keys. `mapeditor.cfg`'s original order was:
///   scroll_speed, autosave, autosave_minutes, default_format,
///   [MapEditor's layers + filter + instant_update + fit_to_grid section],
///   game_parameters, hidden_panels, maps_folder, filter_active, filter_slot_*,
///   recent lines.
/// Splitting the generic writer in two (`writeGenericKeys` for the head,
/// `writeGenericTailKeys` for the game_parameters/hidden_panels/maps_folder
/// group) and leaving the composed caller to fit its own keys between them
/// keeps the original byte order; the recent lines come last via
/// `writeRecentLines`.
pub fn writeGenericTailKeys(settings: anytype, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try writeSharedTailKeys(settings, writer);
    if (settings.mapsFolder().len != 0) try writer.print("maps_folder={s}\n", .{settings.mapsFolder()});
}

/// The tail of the shared keys: game_parameters and hidden_panels, each only
/// when it is not its default, in `mapeditor.cfg`'s own order.
pub fn writeSharedTailKeys(settings: anytype, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    if (settings.gameParameters().len != 0) try writer.print("game_parameters={s}\n", .{settings.gameParameters()});
    if (settings.hidden_panels != 0) try writer.print("hidden_panels={d}\n", .{settings.hidden_panels});
}

/// Writes every recent line in file order (most recent first).
pub fn writeRecentLines(settings: anytype, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    var i: usize = 0;
    while (i < settings.recent_count) : (i += 1) try writer.print("recent={s}\n", .{settings.recentAt(i)});
}

/// Standalone parse for an editor using the kit's own `Settings` directly.
/// MapEditor's composed parser rolls its own loop (so it can slot its own
/// keys in alongside the generic ones) and does not go through here.
pub fn parse(text: []const u8) Settings {
    var settings: Settings = .{};
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw_line| {
        const line = std.mem.trimEnd(u8, raw_line, "\r");
        const trimmed = std.mem.trim(u8, line, " \t");
        if (trimmed.len == 0 or trimmed[0] == '#') continue;
        const eq = std.mem.indexOfScalar(u8, trimmed, '=') orelse continue;
        const key = std.mem.trim(u8, trimmed[0..eq], " \t");
        const value = std.mem.trim(u8, trimmed[eq + 1 ..], " \t");
        _ = applyGenericKey(&settings, key, value);
    }
    return settings;
}

/// Standalone writer for an editor using the kit's own `Settings` directly.
pub fn format(self: *const Settings, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try writeGenericKeys(self, writer);
    try writeGenericTailKeys(self, writer);
    try writeRecentLines(self, writer);
}

test "defaults: an empty text is every field's default" {
    const settings = parse("");
    try std.testing.expectEqual(default_scroll_speed, settings.scroll_speed);
    try std.testing.expect(settings.autosave);
    try std.testing.expectEqual(default_autosave_minutes, settings.autosave_minutes);
    try std.testing.expectEqual(@as(usize, 0), settings.mapsFolder().len);
    try std.testing.expectEqual(@as(usize, 0), settings.recentCount());
    try std.testing.expectEqual(Format.bzm, settings.default_format);
}

test "round trip: every generic field survives format then parse" {
    var settings: Settings = .{};
    settings.scroll_speed = 2.5;
    settings.autosave = false;
    settings.autosave_minutes = 10;
    settings.default_format = .xml;
    settings.setMapsFolder("/Users/me/maps");
    settings.setGameParameters("-nosound");
    settings.hidden_panels = 0b1010;

    var buffer: [1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);

    const round_tripped = parse(writer.buffered());
    try std.testing.expectEqual(@as(f32, 2.5), round_tripped.scroll_speed);
    try std.testing.expect(!round_tripped.autosave);
    try std.testing.expectEqual(@as(u32, 10), round_tripped.autosave_minutes);
    try std.testing.expectEqual(Format.xml, round_tripped.default_format);
    try std.testing.expectEqualStrings("/Users/me/maps", round_tripped.mapsFolder());
    try std.testing.expectEqualStrings("-nosound", round_tripped.gameParameters());
    try std.testing.expectEqual(@as(u32, 0b1010), round_tripped.hidden_panels);
}

test "clamping and malformed values keep the default" {
    try std.testing.expectEqual(max_scroll_speed, parse("scroll_speed=9\n").scroll_speed);
    try std.testing.expectEqual(min_scroll_speed, parse("scroll_speed=0.01\n").scroll_speed);
    try std.testing.expectEqual(max_autosave_minutes, parse("autosave_minutes=999\n").autosave_minutes);
    try std.testing.expectEqual(min_autosave_minutes, parse("autosave_minutes=0\n").autosave_minutes);
    try std.testing.expectEqual(Format.bzm, parse("default_format=zip\n").default_format);
    try std.testing.expectEqual(@as(u32, 0), parse("hidden_panels=many\n").hidden_panels);
}

test "pushRecent: most recent first, dedup, and capacity bound" {
    var settings: Settings = .{};
    settings.pushRecent("a");
    settings.pushRecent("b");
    settings.pushRecent("a");
    try std.testing.expectEqual(@as(usize, 2), settings.recentCount());
    try std.testing.expectEqualStrings("a", settings.recentAt(0));
    try std.testing.expectEqualStrings("b", settings.recentAt(1));

    var i: u8 = 0;
    var buf: [8]u8 = undefined;
    while (i < 11) : (i += 1) {
        const p = std.fmt.bufPrint(&buf, "p{d}", .{i}) catch unreachable;
        settings.pushRecent(p);
    }
    try std.testing.expectEqual(@as(usize, recent_capacity), settings.recentCount());
    try std.testing.expectEqualStrings("p10", settings.recentAt(0));
}

test "setGameParameters drops control characters and caps length" {
    var settings: Settings = .{};
    settings.setGameParameters("  -nosound\n-windowed  ");
    try std.testing.expectEqualStrings("-nosound-windowed", settings.gameParameters());
    var long: [max_game_parameters + 40]u8 = undefined;
    @memset(&long, 'x');
    settings.setGameParameters(&long);
    try std.testing.expectEqual(@as(usize, max_game_parameters), settings.gameParameters().len);
}

test "CRLF line endings parse the same as LF, unknown and malformed keys are skipped" {
    const settings = parse("scroll_speed=2\r\nautosave=off\r\nunknown=whatever\nnoequalsline\n");
    try std.testing.expectEqual(@as(f32, 2), settings.scroll_speed);
    try std.testing.expect(!settings.autosave);
}

test "the generic writer's bytes and order are what mapeditor.cfg always had" {
    var settings: Settings = .{};
    settings.setMapsFolder("/m");
    settings.setGameParameters("-x");
    settings.hidden_panels = 3;
    settings.pushRecent("b.bzm");
    settings.pushRecent("a.bzm");
    var buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    try std.testing.expectEqualStrings("scroll_speed=1\nautosave=on\nautosave_minutes=2\ndefault_format=bzm\n" ++
        "game_parameters=-x\nhidden_panels=3\nmaps_folder=/m\nrecent=a.bzm\nrecent=b.bzm\n", writer.buffered());
}

test "applySharedKey leaves the map-only keys to applyGenericKey" {
    var settings: Settings = .{};
    try std.testing.expect(!applySharedKey(&settings, "default_format", "xml"));
    try std.testing.expect(!applySharedKey(&settings, "maps_folder", "/m"));
    try std.testing.expect(applySharedKey(&settings, "autosave", "off"));
    try std.testing.expect(applyGenericKey(&settings, "default_format", "xml"));
    try std.testing.expectEqual(Format.xml, settings.default_format);
    try std.testing.expect(!settings.autosave);
}

test "formatExtension returns the right suffix" {
    try std.testing.expectEqualStrings(".bzm", formatExtension(.bzm));
    try std.testing.expectEqualStrings(".xml", formatExtension(.xml));
}

test "FixedPath.set drops control characters and caps at max_path" {
    var p: FixedPath = .{};
    p.set("a\nb\rc");
    try std.testing.expectEqualStrings("abc", p.slice());
    var big: [max_path + 10]u8 = undefined;
    @memset(&big, 'z');
    p.set(&big);
    try std.testing.expectEqual(@as(usize, max_path), p.slice().len);
}
