//! MapEditor's own composed settings on top of the kit's generic
//! `editor_kit.settings` primitive (`mapeditor.cfg`, D-24): every generic
//! field (scroll speed, autosave, default format, maps folder, recent list,
//! game parameters, hidden panels) comes from the kit; the MapEditor
//! sidecar brings layer visibility and the fire-range mode (D-32), the
//! filter slots + active filter (D-31) and the two terrain toggles
//! (instant_update, fit_to_grid - D-20). The flat shape and field names of
//! the pre-split `Settings` are preserved so every caller in `app/*.zig`
//! and `core/*.zig` keeps compiling without a surface change.
//!
//! The on-disk format of `mapeditor.cfg` is byte-for-byte unchanged: the
//! writer emits the same keys in the same order, and the parser accepts
//! every file the previous build wrote. T-03-07-01's lenient parse rule
//! holds: a malformed or truncated file degrades to defaults rather than
//! failing.
const std = @import("std");
const kit_settings = @import("editor_kit").settings;
const layers_mod = @import("layers.zig");

// Re-export the kit's generic constants and types so every existing
// `core.settings.X` caller keeps the same path.
pub const min_scroll_speed = kit_settings.min_scroll_speed;
pub const max_scroll_speed = kit_settings.max_scroll_speed;
pub const default_scroll_speed = kit_settings.default_scroll_speed;
pub const min_autosave_minutes = kit_settings.min_autosave_minutes;
pub const max_autosave_minutes = kit_settings.max_autosave_minutes;
pub const default_autosave_minutes = kit_settings.default_autosave_minutes;
pub const recent_capacity = kit_settings.recent_capacity;
pub const Format = kit_settings.Format;
pub const default_format = kit_settings.default_format;
pub const formatExtension = kit_settings.formatExtension;
pub const max_game_parameters = kit_settings.max_game_parameters;
pub const max_path = kit_settings.max_path;
pub const FixedPath = kit_settings.FixedPath;

/// `mapeditor.cfg`'s fields: the kit's generic fields (same names) plus the
/// MapEditor sidecar (layer visibility, filter slots, terrain toggles). The
/// field names and layout match the pre-split struct, so every caller
/// (`state.settings.scroll_speed`, `state.settings.layers`, `.instant_update`,
/// `.filter_active`, `.hidden_panels`, ...) is unchanged.
pub const Settings = struct {
    // --- Generic kit fields (same names as kit_settings.Settings). ---
    scroll_speed: f32 = kit_settings.default_scroll_speed,
    autosave: bool = true,
    autosave_minutes: u32 = kit_settings.default_autosave_minutes,
    /// D-24 (M3): the default format of a never-saved map's Save As.
    default_format: Format = kit_settings.default_format,
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

    // --- MapEditor sidecar fields. ---
    /// D-20 (M3): the Map menu's two terrain toggles, persisted like the MFC
    /// registry-backed checks. Defaults are the MFC's own: Instant Update
    /// off, Fit Objects To Grid on. The bridge session holds the live copy
    /// (its own defaults match); these are what a restart re-applies.
    instant_update: bool = false,
    fit_to_grid: bool = true,
    /// D-32 (M3): the Layers menu - which layers are shown and the fire-range
    /// mode with its filter - remembered across restarts (`layers_bits`,
    /// `fire_range_mode`, `fire_range_filter`). The editor holds the live
    /// copy and re-applies it to the renderer after every open and new map;
    /// these are what a restart starts it from.
    layers: layers_mod.State = .{},
    /// D-31 (M3): the palette's filter combo selection and the nine quick
    /// toggles' assigned filter names (empty = an unset slot), persisted
    /// like the MFC editor's dialog parameters. Names are the filters' own
    /// (Data/Editor/filter.xml keys); a name no filter has any more simply
    /// shows as an empty slot.
    filter_active: FixedPath = .{},
    filter_slots: [filter_slot_count]FixedPath = @splat(.{}),

    pub const filter_slot_count = 9;

    // --- Generic field accessors (delegated to kit helpers so every editor
    // on the kit uses the same byte-dropping and dedup rules). ---

    pub fn gameParameters(self: *const Settings) []const u8 {
        return self.game_parameters.slice();
    }

    pub fn setGameParameters(self: *Settings, text: []const u8) void {
        kit_settings.setGameParametersField(&self.game_parameters, text);
    }

    pub fn mapsFolder(self: *const Settings) []const u8 {
        return self.maps_folder_storage.slice();
    }

    pub fn setMapsFolder(self: *Settings, text: []const u8) void {
        self.maps_folder_storage.set(text);
    }

    pub fn recentCount(self: *const Settings) usize {
        return self.recent_count;
    }

    pub fn recentAt(self: *const Settings, index: usize) []const u8 {
        return self.recent_storage[index].slice();
    }

    pub fn pushRecent(self: *Settings, os_path: []const u8) void {
        kit_settings.pushRecentField(&self.recent_storage, &self.recent_count, os_path);
    }

    pub fn removeRecent(self: *Settings, index: usize) void {
        kit_settings.removeRecentField(&self.recent_storage, &self.recent_count, index);
    }

    // --- MapEditor sidecar accessors. ---

    pub fn filterSlot(self: *const Settings, index: usize) []const u8 {
        if (index >= filter_slot_count) return "";
        return self.filter_slots[index].slice();
    }

    pub fn setFilterSlot(self: *Settings, index: usize, name: []const u8) void {
        if (index >= filter_slot_count) return;
        self.filter_slots[index].set(name);
    }
};

fn applyMapKey(settings: *Settings, key: []const u8, value: []const u8) bool {
    if (std.mem.eql(u8, key, "instant_update")) {
        if (std.mem.eql(u8, value, "on")) {
            settings.instant_update = true;
        } else if (std.mem.eql(u8, value, "off")) {
            settings.instant_update = false;
        }
        return true;
    } else if (std.mem.eql(u8, key, "fit_to_grid")) {
        if (std.mem.eql(u8, value, "on")) {
            settings.fit_to_grid = true;
        } else if (std.mem.eql(u8, value, "off")) {
            settings.fit_to_grid = false;
        }
        return true;
    } else if (std.mem.eql(u8, key, "layers_bits")) {
        // D-32 (M3): the toggle layers' bits as one decimal number. A value
        // that is not a number is skipped (the default stays); a number
        // with bits past the fourteen layers, or the derived fire-range
        // bit, loses them rather than failing the key.
        const parsed = std.fmt.parseInt(u32, value, 10) catch return true;
        settings.layers.bits = parsed & layers_mod.all_bits & ~layers_mod.bit(.fire_ranges);
        return true;
    } else if (std.mem.eql(u8, key, "fire_range_mode")) {
        if (std.mem.eql(u8, value, "off")) {
            settings.layers.fire_mode = .off;
        } else if (std.mem.eql(u8, value, "selected")) {
            settings.layers.fire_mode = .selected;
        } else if (std.mem.eql(u8, value, "filter")) {
            settings.layers.fire_mode = .filter;
        }
        return true;
    } else if (std.mem.eql(u8, key, "fire_range_filter")) {
        const len = @min(value.len, layers_mod.max_filter_len);
        @memcpy(settings.layers.fire_filter_buffer[0..len], value[0..len]);
        settings.layers.fire_filter_len = len;
        return true;
    } else if (std.mem.eql(u8, key, "filter_active")) {
        // A name no filter can have (longer than the filter-name limit) is
        // a hand-edited file: skipped, so the app's fixed name buffers
        // never see it (CR-A03).
        if (value.len > layers_mod.max_filter_len) return true;
        settings.filter_active.set(value);
        return true;
    } else if (std.mem.startsWith(u8, key, "filter_slot_")) {
        const index = std.fmt.parseInt(usize, key["filter_slot_".len..], 10) catch return true;
        if (value.len > layers_mod.max_filter_len) return true;
        settings.setFilterSlot(index, value);
        return true;
    }
    return false;
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
        if (kit_settings.applyGenericKey(&settings, key, value)) continue;
        _ = applyMapKey(&settings, key, value);
        // Every other key is unknown and ignored: a newer or hand-edited
        // file may carry keys this build does not understand.
    }
    // A filter mode with no filter named (a hand-edited file, or the filter
    // key lost) shows nothing and cannot be sent; it reads as off.
    if (settings.layers.fire_mode == .filter and settings.layers.fire_filter_len == 0) settings.layers.fire_mode = .off;
    if (settings.layers.fire_mode != .filter) settings.layers.fire_filter_len = 0;
    return settings;
}

/// Writes `self` back as `mapeditor.cfg`'s own format: `parse(format(...))`
/// round-trips every field (a float within its own precision). The key
/// order matches the pre-split build byte-for-byte so existing files on
/// disk reproduce exactly after a round trip.
pub fn format(self: *const Settings, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try kit_settings.writeGenericKeys(self, writer);
    try writer.print("instant_update={s}\n", .{if (self.instant_update) "on" else "off"});
    try writer.print("fit_to_grid={s}\n", .{if (self.fit_to_grid) "on" else "off"});
    try writer.print("layers_bits={d}\n", .{self.layers.bits & ~layers_mod.bit(.fire_ranges)});
    try writer.print("fire_range_mode={s}\n", .{@tagName(self.layers.fire_mode)});
    // A value with a control character would split its line (WR-B03): the
    // writer leaves such a key out, whatever set it.
    if (self.layers.fire_mode == .filter and !kit_settings.hasControl(self.layers.fireFilter())) {
        try writer.print("fire_range_filter={s}\n", .{self.layers.fireFilter()});
    }
    try kit_settings.writeGenericTailKeys(self, writer);
    if (self.filter_active.slice().len != 0) try writer.print("filter_active={s}\n", .{self.filter_active.slice()});
    for (self.filter_slots, 0..) |slot, index| {
        if (slot.slice().len != 0) try writer.print("filter_slot_{d}={s}\n", .{ index, slot.slice() });
    }
    try kit_settings.writeRecentLines(self, writer);
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

test "layers: the MFC's own menu by default, the bits and the fire-range mode round trip" {
    try std.testing.expectEqual(layers_mod.default_bits, parse("").layers.bits);
    try std.testing.expectEqual(layers_mod.FireMode.off, parse("").layers.fire_mode);

    var settings: Settings = .{};
    settings.layers.set(.grid, true);
    settings.layers.set(.terrain_noise, false);
    settings.layers.set(.war_fog, true);
    settings.layers.setFireRange(.filter, "Axis Units");
    var buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const back = parse(writer.buffered());
    try std.testing.expectEqual(settings.layers.bits, back.layers.bits);
    try std.testing.expectEqual(layers_mod.FireMode.filter, back.layers.fire_mode);
    try std.testing.expectEqualStrings("Axis Units", back.layers.fireFilter());
    try std.testing.expect(back.layers.shown(.grid) and !back.layers.shown(.terrain_noise) and back.layers.shown(.war_fog));

    // The selected mode needs no filter; a filter key beside another mode is dropped.
    settings.layers.setFireRange(.selected, "");
    var writer2: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer2);
    const selected = parse(writer2.buffered());
    try std.testing.expectEqual(layers_mod.FireMode.selected, selected.layers.fire_mode);
    try std.testing.expectEqualStrings("", selected.layers.fireFilter());
    try std.testing.expect(std.mem.indexOf(u8, writer2.buffered(), "fire_range_filter") == null);
}

test "layers: old files without the keys keep the defaults, malformed keys are skipped, stray bits are cut" {
    const old = parse("scroll_speed=1.5\nautosave=on\n");
    try std.testing.expectEqual(layers_mod.default_bits, old.layers.bits);
    try std.testing.expectEqual(layers_mod.FireMode.off, old.layers.fire_mode);
    const malformed = parse("layers_bits=lots\nfire_range_mode=everything\n");
    try std.testing.expectEqual(layers_mod.default_bits, malformed.layers.bits);
    try std.testing.expectEqual(layers_mod.FireMode.off, malformed.layers.fire_mode);
    // Bits past the fourteen layers and the derived fire-range bit are cut.
    const stray = parse("layers_bits=4294967295\n");
    try std.testing.expectEqual(layers_mod.all_bits & ~layers_mod.bit(.fire_ranges), stray.layers.bits);
    // A filter mode with no filter reads as off - it could not be sent.
    const nameless = parse("fire_range_mode=filter\n");
    try std.testing.expectEqual(layers_mod.FireMode.off, nameless.layers.fire_mode);
    // A filter name beside the selected mode is not kept.
    const stale = parse("fire_range_mode=selected\nfire_range_filter=Buildings\n");
    try std.testing.expectEqual(layers_mod.FireMode.selected, stale.layers.fire_mode);
    try std.testing.expectEqualStrings("", stale.layers.fireFilter());
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


test "game parameters and hidden panels: default empty, round trip, control characters and length cut" {
    try std.testing.expectEqualStrings("", parse("").gameParameters());
    try std.testing.expectEqual(@as(u32, 0), parse("").hidden_panels);

    var settings: Settings = .{};
    settings.setGameParameters("  -nosound -windowed \"a b\"  ");
    settings.hidden_panels = 0b1010;
    var buffer: [1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const back = parse(writer.buffered());
    try std.testing.expectEqualStrings("-nosound -windowed \"a b\"", back.gameParameters());
    try std.testing.expectEqual(@as(u32, 0b1010), back.hidden_panels);

    // A control character is dropped, so one line stays one line.
    settings.setGameParameters("-a\n-b\r\x01");
    try std.testing.expectEqualStrings("-a-b", settings.gameParameters());
    // Nothing past the cap is kept.
    var long: [max_game_parameters + 40]u8 = undefined;
    @memset(&long, 'x');
    settings.setGameParameters(&long);
    try std.testing.expectEqual(@as(usize, max_game_parameters), settings.gameParameters().len);

    // A malformed number keeps every panel shown; empty parameters write no key.
    try std.testing.expectEqual(@as(u32, 0), parse("hidden_panels=many\n").hidden_panels);
    settings.setGameParameters("");
    var writer2: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer2);
    try std.testing.expect(std.mem.indexOf(u8, writer2.buffered(), "game_parameters") == null);
}

test "a newline in a recent path, folder or filter name plants no key in the file (WR-B03)" {
    var settings: Settings = .{};
    // A recent path with a control character is not kept at all.
    settings.pushRecent("/maps/a\ngame_parameters=--opt x.bzm");
    try std.testing.expectEqual(@as(usize, 0), settings.recentCount());
    settings.pushRecent("/maps/fine.bzm");
    try std.testing.expectEqual(@as(usize, 1), settings.recentCount());
    // The other free-text fields lose the character instead.
    settings.setMapsFolder("/maps\nhidden_panels=7");
    try std.testing.expectEqualStrings("/mapshidden_panels=7", settings.mapsFolder());
    settings.filter_active.set("one\ntwo");
    settings.setFilterSlot(0, "slot\r\n0");
    settings.layers.setFireRange(.filter, "a\nb");
    var buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const back = parse(writer.buffered());
    try std.testing.expectEqual(@as(u32, 0), back.hidden_panels);
    try std.testing.expectEqual(@as(usize, 0), back.gameParameters().len);
    try std.testing.expectEqual(@as(usize, 1), back.recentCount());
    try std.testing.expectEqualStrings("onetwo", back.filter_active.slice());
    // An empty recent= value adds no entry.
    try std.testing.expectEqual(@as(usize, 0), parse("recent=\nrecent=   \n").recentCount());
}

test "a filter name longer than the filter-name limit in the file is skipped (CR-A03)" {
    const long = &@as([layers_mod.max_filter_len + 1:0]u8, @splat('x'));
    const exact = &@as([layers_mod.max_filter_len:0]u8, @splat('y'));
    const read = parse("filter_active=" ++ long ++ "\nfilter_slot_2=" ++ long ++ "\nfilter_slot_3=" ++ exact ++ "\n");
    try std.testing.expectEqual(@as(usize, 0), read.filter_active.slice().len);
    try std.testing.expectEqual(@as(usize, 0), read.filterSlot(2).len);
    try std.testing.expectEqualStrings(exact, read.filterSlot(3));
}
