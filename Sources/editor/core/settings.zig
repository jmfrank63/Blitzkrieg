//! The editor's own settings file (`mapeditor.cfg`, D-24): scroll/swipe
//! speed, autosave on/off and interval, the default maps folder, and the
//! recent-files list (D-25, D-27). A small hand-written `key=value` format
//! (CONTEXT.md leaves the format to the builder) rather than pulling in the
//! game's own `OptionSystem`, which is tied to a much larger option-
//! registration system for four scalar settings. Std-only, like the rest of
//! the core: reading and writing the file itself, and reacting to a change,
//! are the app's job (main.zig, panels.zig); this is only the format and the
//! in-memory value.
//!
//! T-03-07-01: the app caps what it reads before handing text here (64 KiB);
//! `parse` itself is lenient rather than fallible - a malformed or truncated
//! file degrades to defaults for whatever it could not make sense of, never
//! an error the caller has to handle.
const std = @import("std");
const builtin = @import("builtin");

pub const min_scroll_speed: f32 = 0.25;
pub const max_scroll_speed: f32 = 4.0;
pub const default_scroll_speed: f32 = 1.0;
pub const min_autosave_minutes: u32 = 1;
pub const max_autosave_minutes: u32 = 60;
pub const default_autosave_minutes: u32 = 2;

/// D-27: the last ten maps.
pub const recent_capacity = 10;

/// D-24 (M3): the format a never-saved map's Save As writes when the path
/// typed names no extension - the MFC Options field. The bridge picks the
/// format from the path's extension, so this is only ever the default.
pub const Format = enum { bzm, xml };

pub const default_format: Format = .bzm;

pub fn formatExtension(which: Format) []const u8 {
    return switch (which) {
        .bzm => ".bzm",
        .xml => ".xml",
    };
}

/// `BkEditorPathSet`'s own `char[1024]` (bridge.h) - big enough for any real
/// path, and matches the engine's own convention for a fixed path buffer.
pub const max_path = 1024;

const FixedPath = struct {
    buffer: [max_path]u8 = undefined,
    len: usize = 0,

    pub fn slice(self: *const FixedPath) []const u8 {
        return self.buffer[0..self.len];
    }

    pub fn set(self: *FixedPath, text: []const u8) void {
        self.len = @min(text.len, self.buffer.len);
        @memcpy(self.buffer[0..self.len], text[0..self.len]);
    }
};

/// `mapeditor.cfg`'s fields (D-24, D-25, D-27): fixed buffers throughout, so
/// a `Settings` lives on the stack with no allocation.
pub const Settings = struct {
    scroll_speed: f32 = default_scroll_speed,
    autosave: bool = true,
    autosave_minutes: u32 = default_autosave_minutes,
    /// D-24 (M3): the default format of a never-saved map's Save As.
    default_format: Format = default_format,
    /// D-20 (M3): the Map menu's two terrain toggles, persisted like the MFC
    /// registry-backed checks. Defaults are the MFC's own: Instant Update
    /// off, Fit Objects To Grid on. The bridge session holds the live copy
    /// (its own defaults match); these are what a restart re-applies.
    instant_update: bool = false,
    fit_to_grid: bool = true,
    maps_folder_storage: FixedPath = .{},
    recent_storage: [recent_capacity]FixedPath = [_]FixedPath{.{}} ** recent_capacity,
    recent_count: usize = 0,

    /// D-31 (M3): the palette's filter combo selection and the nine quick
    /// toggles' assigned filter names (empty = an unset slot), persisted like
    /// the MFC editor's dialog parameters. Names are the filters' own
    /// (Data/Editor/filter.xml keys); a name no filter has any more simply
    /// shows as an empty slot.
    filter_active: FixedPath = .{},
    filter_slots: [filter_slot_count]FixedPath = [_]FixedPath{.{}} ** filter_slot_count,

    pub const filter_slot_count = 9;

    pub fn filterSlot(self: *const Settings, index: usize) []const u8 {
        if (index >= filter_slot_count) return "";
        return self.filter_slots[index].slice();
    }

    pub fn setFilterSlot(self: *Settings, index: usize, name: []const u8) void {
        if (index >= filter_slot_count) return;
        self.filter_slots[index].set(name);
    }

    /// Empty means "use the default maps folder" (D-25's hint text).
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

    /// D-27: `os_path` to the front. An entry already in the list (byte-equal
    /// - case-insensitive on Windows, where two spellings of a drive letter
    /// or a path segment name the same file) moves up instead of repeating;
    /// past `recent_capacity` entries, the oldest (last) one drops off.
    pub fn pushRecent(self: *Settings, os_path: []const u8) void {
        var existing: ?usize = null;
        var i: usize = 0;
        while (i < self.recent_count) : (i += 1) {
            if (sameOsPath(self.recent_storage[i].slice(), os_path)) {
                existing = i;
                break;
            }
        }
        // Shifting right by one opens a slot at 0 for the new (or moved)
        // entry; walking from the far end down to 1 means every write lands
        // in a slot whose old value has already moved on, so nothing is lost
        // to being overwritten before it is read.
        const start = existing orelse @min(self.recent_count, recent_capacity - 1);
        var shift = start;
        while (shift > 0) : (shift -= 1) self.recent_storage[shift] = self.recent_storage[shift - 1];
        self.recent_storage[0].set(os_path);
        if (existing == null) self.recent_count = @min(self.recent_count + 1, recent_capacity);
    }

    /// D-27: removes a stale (missing-on-disk) entry by its index, shifting
    /// the rest up. Out of range is a no-op.
    pub fn removeRecent(self: *Settings, index: usize) void {
        if (index >= self.recent_count) return;
        var i = index;
        while (i + 1 < self.recent_count) : (i += 1) self.recent_storage[i] = self.recent_storage[i + 1];
        self.recent_count -= 1;
    }
};

fn sameOsPath(a: []const u8, b: []const u8) bool {
    if (builtin.os.tag == .windows) return std.ascii.eqlIgnoreCase(a, b);
    return std.mem.eql(u8, a, b);
}

fn applyKey(settings: *Settings, key: []const u8, value: []const u8) void {
    if (std.mem.eql(u8, key, "scroll_speed")) {
        const parsed = std.fmt.parseFloat(f32, value) catch return;
        if (!std.math.isFinite(parsed)) return;
        settings.scroll_speed = std.math.clamp(parsed, min_scroll_speed, max_scroll_speed);
    } else if (std.mem.eql(u8, key, "autosave")) {
        if (std.mem.eql(u8, value, "on")) {
            settings.autosave = true;
        } else if (std.mem.eql(u8, value, "off")) {
            settings.autosave = false;
        }
        // Any other value is malformed and skipped, keeping the default.
    } else if (std.mem.eql(u8, key, "autosave_minutes")) {
        const parsed = std.fmt.parseInt(u32, value, 10) catch return;
        settings.autosave_minutes = std.math.clamp(parsed, min_autosave_minutes, max_autosave_minutes);
    } else if (std.mem.eql(u8, key, "default_format")) {
        // D-24 (M3): bzm or xml; anything else is malformed and skipped,
        // keeping the default, exactly autosave's own rule.
        if (std.mem.eql(u8, value, "bzm")) {
            settings.default_format = .bzm;
        } else if (std.mem.eql(u8, value, "xml")) {
            settings.default_format = .xml;
        }
    } else if (std.mem.eql(u8, key, "instant_update")) {
        if (std.mem.eql(u8, value, "on")) {
            settings.instant_update = true;
        } else if (std.mem.eql(u8, value, "off")) {
            settings.instant_update = false;
        }
    } else if (std.mem.eql(u8, key, "fit_to_grid")) {
        if (std.mem.eql(u8, value, "on")) {
            settings.fit_to_grid = true;
        } else if (std.mem.eql(u8, value, "off")) {
            settings.fit_to_grid = false;
        }
    } else if (std.mem.eql(u8, key, "maps_folder")) {
        settings.setMapsFolder(value);
    } else if (std.mem.eql(u8, key, "filter_active")) {
        settings.filter_active.set(value);
    } else if (std.mem.startsWith(u8, key, "filter_slot_")) {
        const index = std.fmt.parseInt(usize, key["filter_slot_".len..], 10) catch return;
        settings.setFilterSlot(index, value);
    } else if (std.mem.eql(u8, key, "recent")) {
        if (settings.recent_count < recent_capacity) {
            settings.recent_storage[settings.recent_count].set(value);
            settings.recent_count += 1;
        }
        // Beyond capacity: parse never reaches here for a well-formed file
        // (format never writes more than recent_capacity lines), but an
        // externally-edited file with extras just stops adding - not a
        // reason to fail the rest of the file.
    }
    // Every other key is unknown and ignored: a newer or hand-edited file
    // may carry keys this build does not understand.
}

/// Parses `mapeditor.cfg`'s text: `key=value` lines, `#`-prefixed full-line
/// comments (blank lines too), CRLF tolerated. Unknown keys are ignored; a
/// line with no `=`, or a value that will not parse as its key's type, is
/// skipped rather than failing the whole file; every numeric value out of
/// range is clamped rather than rejected. A missing key keeps its default.
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
        applyKey(&settings, key, value);
    }
    return settings;
}

/// Writes `self` back as `mapeditor.cfg`'s own format: `parse(format(...))`
/// round-trips every field (a float within its own precision).
pub fn format(self: *const Settings, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try writer.print("scroll_speed={d}\n", .{self.scroll_speed});
    try writer.print("autosave={s}\n", .{if (self.autosave) "on" else "off"});
    try writer.print("autosave_minutes={d}\n", .{self.autosave_minutes});
    try writer.print("default_format={s}\n", .{@tagName(self.default_format)});
    try writer.print("instant_update={s}\n", .{if (self.instant_update) "on" else "off"});
    try writer.print("fit_to_grid={s}\n", .{if (self.fit_to_grid) "on" else "off"});
    if (self.mapsFolder().len != 0) try writer.print("maps_folder={s}\n", .{self.mapsFolder()});
    if (self.filter_active.slice().len != 0) try writer.print("filter_active={s}\n", .{self.filter_active.slice()});
    for (self.filter_slots, 0..) |slot, index| {
        if (slot.slice().len != 0) try writer.print("filter_slot_{d}={s}\n", .{ index, slot.slice() });
    }
    var i: usize = 0;
    while (i < self.recent_count) : (i += 1) try writer.print("recent={s}\n", .{self.recentAt(i)});
}

test "defaults: an empty file (no mapeditor.cfg yet) is every field's default" {
    const settings = parse("");
    try std.testing.expectEqual(default_scroll_speed, settings.scroll_speed);
    try std.testing.expect(settings.autosave);
    try std.testing.expectEqual(default_autosave_minutes, settings.autosave_minutes);
    try std.testing.expectEqual(@as(usize, 0), settings.mapsFolder().len);
    try std.testing.expectEqual(@as(usize, 0), settings.recentCount());
}

test "round trip: every field survives format then parse" {
    var settings: Settings = .{};
    settings.scroll_speed = 2.5;
    settings.autosave = false;
    settings.autosave_minutes = 10;
    settings.default_format = .xml;
    settings.instant_update = true;
    settings.fit_to_grid = false;
    settings.setMapsFolder("/Users/me/maps");

    var buffer: [1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);

    const round_tripped = parse(writer.buffered());
    try std.testing.expectEqual(@as(f32, 2.5), round_tripped.scroll_speed);
    try std.testing.expect(!round_tripped.autosave);
    try std.testing.expectEqual(@as(u32, 10), round_tripped.autosave_minutes);
    try std.testing.expectEqual(Format.xml, round_tripped.default_format);
    try std.testing.expect(round_tripped.instant_update);
    try std.testing.expect(!round_tripped.fit_to_grid);
    try std.testing.expectEqualStrings("/Users/me/maps", round_tripped.mapsFolder());
}

test "terrain toggles: the MFC's own defaults, on/off reads, malformed keeps the default" {
    try std.testing.expect(!parse("").instant_update);
    try std.testing.expect(parse("").fit_to_grid);
    const flipped = parse("instant_update=on\nfit_to_grid=off\n");
    try std.testing.expect(flipped.instant_update);
    try std.testing.expect(!flipped.fit_to_grid);
    const malformed = parse("instant_update=yes\nfit_to_grid=1\n");
    try std.testing.expect(!malformed.instant_update);
    try std.testing.expect(malformed.fit_to_grid);
}

test "default_format: bzm by default, xml reads, malformed keeps the default" {
    try std.testing.expectEqual(Format.bzm, parse("").default_format);
    try std.testing.expectEqual(Format.xml, parse("default_format=xml\n").default_format);
    try std.testing.expectEqual(Format.bzm, parse("default_format=zip\n").default_format);
    try std.testing.expectEqualStrings(".xml", formatExtension(.xml));
    try std.testing.expectEqualStrings(".bzm", formatExtension(.bzm));
}

test "filter slots and the active filter persist, malformed indices are skipped" {
    const read = parse("filter_active=Buildings\nfilter_slot_0=Buildings\nfilter_slot_8=Squads\nfilter_slot_9=Nope\nfilter_slot_x=Nope\n");
    try std.testing.expectEqualStrings("Buildings", read.filter_active.slice());
    try std.testing.expectEqualStrings("Buildings", read.filterSlot(0));
    try std.testing.expectEqualStrings("Squads", read.filterSlot(8));
    try std.testing.expectEqualStrings("", read.filterSlot(9));

    var settings: Settings = .{};
    settings.filter_active.set("Flora");
    settings.setFilterSlot(3, "Obj Terrain");
    var buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const round_tripped = parse(writer.buffered());
    try std.testing.expectEqualStrings("Flora", round_tripped.filter_active.slice());
    try std.testing.expectEqualStrings("Obj Terrain", round_tripped.filterSlot(3));
    try std.testing.expectEqualStrings("", round_tripped.filterSlot(0));
}

test "round trip: repeated 'recent' lines parse in file order, most recent first" {
    const settings = parse("recent=/maps/b.bzm\nrecent=/maps/a.bzm\n");
    try std.testing.expectEqual(@as(usize, 2), settings.recentCount());
    try std.testing.expectEqualStrings("/maps/b.bzm", settings.recentAt(0));
    try std.testing.expectEqualStrings("/maps/a.bzm", settings.recentAt(1));

    var buffer: [256]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const round_tripped = parse(writer.buffered());
    try std.testing.expectEqual(@as(usize, 2), round_tripped.recentCount());
    try std.testing.expectEqualStrings("/maps/b.bzm", round_tripped.recentAt(0));
    try std.testing.expectEqualStrings("/maps/a.bzm", round_tripped.recentAt(1));
}

test "clamping: scroll_speed 9 clamps to 4, and autosave_minutes clamps too" {
    try std.testing.expectEqual(max_scroll_speed, parse("scroll_speed=9\n").scroll_speed);
    try std.testing.expectEqual(min_scroll_speed, parse("scroll_speed=0.01\n").scroll_speed);
    try std.testing.expectEqual(max_autosave_minutes, parse("autosave_minutes=999\n").autosave_minutes);
    try std.testing.expectEqual(min_autosave_minutes, parse("autosave_minutes=0\n").autosave_minutes);
}

test "CRLF line endings parse the same as LF" {
    const settings = parse("scroll_speed=2\r\nautosave=off\r\n");
    try std.testing.expectEqual(@as(f32, 2), settings.scroll_speed);
    try std.testing.expect(!settings.autosave);
}

test "an unknown key is ignored, not a parse failure" {
    const settings = parse("unknown_key=whatever\nscroll_speed=3\n");
    try std.testing.expectEqual(@as(f32, 3), settings.scroll_speed);
}

test "comment and blank lines (leading '#', with or without indent) are skipped" {
    const settings = parse("# a comment\n\nscroll_speed=1.5\n   # indented comment\n");
    try std.testing.expectEqual(@as(f32, 1.5), settings.scroll_speed);
}

test "a malformed line with no '=' is skipped, not fatal to the rest of the file" {
    const settings = parse("this has no equals\nscroll_speed=2\n");
    try std.testing.expectEqual(@as(f32, 2), settings.scroll_speed);
}

test "pushRecent: most recent first" {
    var settings: Settings = .{};
    settings.pushRecent("a");
    settings.pushRecent("b");
    settings.pushRecent("c");
    try std.testing.expectEqual(@as(usize, 3), settings.recentCount());
    try std.testing.expectEqualStrings("c", settings.recentAt(0));
    try std.testing.expectEqualStrings("b", settings.recentAt(1));
    try std.testing.expectEqualStrings("a", settings.recentAt(2));
}

test "pushRecent: an equal path moves to the front instead of repeating" {
    var settings: Settings = .{};
    settings.pushRecent("a");
    settings.pushRecent("b");
    settings.pushRecent("a");
    try std.testing.expectEqual(@as(usize, 2), settings.recentCount());
    try std.testing.expectEqualStrings("a", settings.recentAt(0));
    try std.testing.expectEqualStrings("b", settings.recentAt(1));
}

test "pushRecent: the 11th drops the oldest" {
    var settings: Settings = .{};
    var buffer: [8]u8 = undefined;
    var i: u8 = 0;
    while (i < 11) : (i += 1) {
        const path = std.fmt.bufPrint(&buffer, "{d}", .{i}) catch unreachable;
        settings.pushRecent(path);
    }
    try std.testing.expectEqual(@as(usize, recent_capacity), settings.recentCount());
    try std.testing.expectEqualStrings("10", settings.recentAt(0));
    try std.testing.expectEqualStrings("1", settings.recentAt(recent_capacity - 1));
}

test "removeRecent: removes by index and shifts the rest up" {
    var settings: Settings = .{};
    settings.pushRecent("a");
    settings.pushRecent("b");
    settings.pushRecent("c"); // c, b, a
    settings.removeRecent(1); // removes "b"
    try std.testing.expectEqual(@as(usize, 2), settings.recentCount());
    try std.testing.expectEqualStrings("c", settings.recentAt(0));
    try std.testing.expectEqualStrings("a", settings.recentAt(1));
}

test "removeRecent: an out-of-range index is a no-op" {
    var settings: Settings = .{};
    settings.pushRecent("a");
    settings.removeRecent(5);
    try std.testing.expectEqual(@as(usize, 1), settings.recentCount());
}

