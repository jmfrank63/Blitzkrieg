//! `elk.cfg` (D-16): ELK's settings file in the kit's `key=value` format.
//! This slice keeps the kit's editor-neutral keys (`kit.settings.applySharedKey`
//! and the shared writers), of which ELK uses the recent list: the ten
//! recent databases, the first being the one File > Open last opened. Of
//! ELK's own keys it keeps the named filters (`filter=`, one line each in
//! the core's `Filter` form; none in the file means the five built-in ones,
//! as the MFC ELK made them when its registry held none) and the current
//! one (`current_filter=`), which the Create Filter dialog edits through
//! `addFilter`, `renameFilter`, `deleteFilter` and `setFilter`, and the
//! collapse-item mode (`collapse_item=`, on unless the file says `0`, the
//! MFC registry's default), and the find options of Edit > Find...
//! (`find_text=`, `find_down=`, `find_match_case=`, `find_whole_word=`, the
//! registry's FindText, FindDown, FindMatchCase and FindWholeWord, with
//! their defaults: down, neither box ticked). The registry's FindWindow and
//! FindPosition are not kept: closing the dialog reset them to the start.
//! The game folder (`game_folder=`) is where File > Export to language mod,
//! Install to game and Run Game write and start the game; empty means the
//! folder ELK runs from, which is the game's when ELK is installed beside it.
//! Spell check keeps its dictionary folder (`spell_folder=`, empty meaning
//! `dictionaries` beside ELK) and language (`spell_language=`, a dictionary's
//! name such as `en_US`; empty or missing takes the folder's first). The rest
//! of D-16 (last PAK and XLS folders, fonts, toolbars) joins with the
//! features that use it.
//!
//! Where the file lives: `<UserRoot>elk/elk.cfg`, the user root being the
//! game's own (Platform/Paths.cpp: `$XDG_DATA_HOME/Nival/Blitzkrieg/` or
//! `~/.local/share/Nival/Blitzkrieg/` on Linux and macOS, SDL's pref path
//! or `BK_USER_ROOT` on Windows), worked out by main.zig without the engine.
//! `BK_ELK_SETTINGS` names another file instead: the test seam, like the
//! map editor's `BK_EDITOR_SETTINGS`, so a test never touches the user's own.
const std = @import("std");
const kit = @import("editor_kit");
const elk = @import("elk_core");

const ks = kit.settings;
const FixedPath = ks.FixedPath;
pub const Filter = elk.filter.Filter;

pub const settings_env = "BK_ELK_SETTINGS";
pub const user_folder = "elk";
pub const file_name = "elk.cfg";
/// The longest path a recent entry holds.
pub const max_path = ks.max_path;
/// The most named filters the file keeps; later lines are dropped.
pub const filter_capacity = 32;
/// The longest filter name the dialog takes.
pub const max_filter_name = 128;

/// The kit's shared fields, under the names its helpers expect.
pub const Settings = struct {
    scroll_speed: f32 = ks.default_scroll_speed,
    autosave: bool = true,
    autosave_minutes: u32 = ks.default_autosave_minutes,
    recent_storage: [ks.recent_capacity]FixedPath = @splat(.{}),
    recent_count: usize = 0,
    game_parameters: FixedPath = .{},
    hidden_panels: u32 = 0,
    /// Each named filter's line; no line means the built-in five.
    filter_storage: [filter_capacity]FixedPath = @splat(.{}),
    filter_count: usize = 0,
    current_filter_storage: FixedPath = .{},
    /// Browse collapses the branch it leaves (`bCollapseItem`, B5).
    collapse_item: bool = true,
    /// Edit > Find...'s text and options (`SSearchParam`).
    find_text_storage: FixedPath = .{},
    find_down: bool = true,
    find_match_case: bool = false,
    find_whole_word: bool = false,
    /// The game installation exports and Run Game use; empty is ELK's own folder.
    game_folder_storage: FixedPath = .{},
    /// Spell check's dictionary folder; empty is `dictionaries` beside ELK.
    spell_folder_storage: FixedPath = .{},
    spell_language_storage: FixedPath = .{},

    pub fn spellFolder(self: *const Settings) []const u8 {
        return self.spell_folder_storage.slice();
    }

    pub fn spellLanguage(self: *const Settings) []const u8 {
        return self.spell_language_storage.slice();
    }

    pub fn gameFolder(self: *const Settings) []const u8 {
        return self.game_folder_storage.slice();
    }

    pub fn gameParameters(self: *const Settings) []const u8 {
        return self.game_parameters.slice();
    }

    pub fn recentCount(self: *const Settings) usize {
        return self.recent_count;
    }

    /// `0` is the most recently opened.
    pub fn recentAt(self: *const Settings, index: usize) []const u8 {
        return self.recent_storage[index].slice();
    }

    pub fn pushRecent(self: *Settings, os_path: []const u8) void {
        ks.pushRecentField(&self.recent_storage, &self.recent_count, os_path);
    }

    pub fn filterCount(self: *const Settings) usize {
        return if (self.filter_count == 0) elk.filter.built_in.len else self.filter_count;
    }

    /// The named filters in the file's order, borrowing its storage.
    pub fn filterAt(self: *const Settings, index: usize) Filter {
        if (self.filter_count == 0) return elk.filter.built_in[index];
        return Filter.parse(self.filter_storage[index].slice()).?;
    }

    /// The current filter's name; "All" when the file names none.
    pub fn currentFilter(self: *const Settings) []const u8 {
        const name = self.current_filter_storage.slice();
        return if (name.len == 0) elk.filter.default_name else name;
    }

    pub fn setCurrentFilter(self: *Settings, name: []const u8) void {
        self.current_filter_storage.set(name);
    }

    pub fn findText(self: *const Settings) []const u8 {
        return self.find_text_storage.slice();
    }

    /// The typed text, kept on one line of the file: a line end or other
    /// control character ends it.
    pub fn setFindText(self: *Settings, text: []const u8) void {
        var end: usize = 0;
        while (end < text.len and !ks.isControl(text[end])) end += 1;
        self.find_text_storage.set(text[0..end]);
    }

    /// Appends a filter keeping every text (a new `SSimpleFilter`), named
    /// `wanted` made unique; its index, or null when the name is empty or
    /// the list is full.
    pub fn addFilter(self: *Settings, wanted: []const u8) ?usize {
        self.ownFilters();
        if (self.filter_count == filter_capacity) return null;
        var name_buffer: [max_filter_name + 16]u8 = undefined;
        const name = self.uniqueFilterName(wanted, null, &name_buffer) orelse return null;
        if (!self.writeFilter(self.filter_count, .{ .name = name })) return null;
        self.filter_count += 1;
        return self.filter_count - 1;
    }

    /// Renames the filter at `index` in place, its criteria kept; the
    /// current filter follows its new name. False when the name is empty.
    pub fn renameFilter(self: *Settings, index: usize, wanted: []const u8) bool {
        self.ownFilters();
        var name_buffer: [max_filter_name + 16]u8 = undefined;
        const name = self.uniqueFilterName(wanted, index, &name_buffer) orelse return false;
        var renamed = self.filterAt(index);
        const was_current = std.mem.eql(u8, renamed.name, self.currentFilter());
        renamed.name = name;
        if (!self.writeFilter(index, renamed)) return false;
        if (was_current) self.setCurrentFilter(name);
        return true;
    }

    /// Removes the filter at `index`; the current filter falls back to
    /// "All" when it was this one. The last filter stays: a file with none
    /// would bring the five built-in ones back, and the MFC dialog's OK
    /// was disabled with an empty list.
    pub fn deleteFilter(self: *Settings, index: usize) bool {
        self.ownFilters();
        if (self.filter_count <= 1 or index >= self.filter_count) return false;
        if (std.mem.eql(u8, self.filterAt(index).name, self.currentFilter())) self.setCurrentFilter(elk.filter.default_name);
        for (index..self.filter_count - 1) |at| self.filter_storage[at] = self.filter_storage[at + 1];
        self.filter_count -= 1;
        return true;
    }

    /// The filter at `index` gets `filter`'s states, changed flag and
    /// conditions (its name is the dialog's to change). False when the line
    /// would not fit.
    pub fn setFilter(self: *Settings, index: usize, filter: Filter) bool {
        self.ownFilters();
        var edited = filter;
        edited.name = self.filterAt(index).name;
        return self.writeFilter(index, edited);
    }

    /// The editor writes lines, so the five built-in filters become the
    /// file's own before the first edit.
    fn ownFilters(self: *Settings) void {
        if (self.filter_count != 0) return;
        for (elk.filter.built_in, 0..) |filter, index| _ = self.writeFilter(index, filter);
        self.filter_count = elk.filter.built_in.len;
    }

    /// `filter`'s line into the slot, formatted aside first because its
    /// name may borrow the slot itself.
    fn writeFilter(self: *Settings, index: usize, filter: Filter) bool {
        var buffer: [max_path]u8 = undefined;
        var writer: std.Io.Writer = .fixed(&buffer);
        filter.format(&writer) catch return false;
        self.filter_storage[index].set(writer.buffered());
        return true;
    }

    /// `wanted` cleaned of what would break its line (the `|` separator,
    /// control characters, outer spaces), then made unique as
    /// `GetUniqueFilterName` did: `name`, else `name_0`, `name_1`, ...
    /// The filter at `skip` (the one being renamed) does not count.
    fn uniqueFilterName(self: *const Settings, wanted: []const u8, skip: ?usize, buffer: []u8) ?[]const u8 {
        var clean_buffer: [max_filter_name]u8 = undefined;
        var len: usize = 0;
        for (std.mem.trim(u8, wanted, " \t")) |byte| {
            if (byte == '|' or ks.isControl(byte)) continue;
            if (len == clean_buffer.len) break;
            clean_buffer[len] = byte;
            len += 1;
        }
        const clean = std.mem.trim(u8, clean_buffer[0..len], " ");
        if (clean.len == 0) return null;
        var name: []const u8 = std.fmt.bufPrint(buffer, "{s}", .{clean}) catch return null;
        var suffix: usize = 0;
        while (self.hasFilter(name, skip)) : (suffix += 1) {
            name = std.fmt.bufPrint(buffer, "{s}_{d}", .{ clean, suffix }) catch return null;
        }
        return name;
    }

    fn hasFilter(self: *const Settings, name: []const u8, skip: ?usize) bool {
        for (0..self.filterCount()) |index| {
            if (skip == index) continue;
            if (std.mem.eql(u8, self.filterAt(index).name, name)) return true;
        }
        return false;
    }

    /// Removes `os_path` from the list if it is there (a recent database
    /// that no longer opens).
    pub fn forgetRecent(self: *Settings, os_path: []const u8) void {
        var i: usize = 0;
        while (i < self.recent_count) : (i += 1) {
            if (ks.sameOsPath(self.recentAt(i), os_path)) return ks.removeRecentField(&self.recent_storage, &self.recent_count, i);
        }
    }
};

/// Lenient like the kit's parser: unknown or malformed lines keep defaults.
pub fn parse(text: []const u8) Settings {
    var settings: Settings = .{};
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw_line| {
        const trimmed = std.mem.trim(u8, std.mem.trimEnd(u8, raw_line, "\r"), " \t");
        if (trimmed.len == 0 or trimmed[0] == '#') continue;
        const eq = std.mem.indexOfScalar(u8, trimmed, '=') orelse continue;
        const key = std.mem.trim(u8, trimmed[0..eq], " \t");
        const value = std.mem.trim(u8, trimmed[eq + 1 ..], " \t");
        if (std.mem.eql(u8, key, "filter")) {
            // Only lines that parse are kept, so `filterAt` cannot fail.
            if (settings.filter_count < filter_capacity and value.len <= max_path and Filter.parse(value) != null) {
                settings.filter_storage[settings.filter_count].set(value);
                settings.filter_count += 1;
            }
        } else if (std.mem.eql(u8, key, "current_filter")) {
            settings.current_filter_storage.set(value);
        } else if (std.mem.eql(u8, key, "collapse_item")) {
            settings.collapse_item = !std.mem.eql(u8, value, "0");
        } else if (std.mem.eql(u8, key, "find_text")) {
            // Untrimmed: a search may start or end with a space.
            settings.setFindText(std.mem.trimEnd(u8, raw_line, "\r")[std.mem.indexOfScalar(u8, raw_line, '=').? + 1 ..]);
        } else if (std.mem.eql(u8, key, "find_down")) {
            settings.find_down = !std.mem.eql(u8, value, "0");
        } else if (std.mem.eql(u8, key, "find_match_case")) {
            settings.find_match_case = std.mem.eql(u8, value, "1");
        } else if (std.mem.eql(u8, key, "find_whole_word")) {
            settings.find_whole_word = std.mem.eql(u8, value, "1");
        } else if (std.mem.eql(u8, key, "game_folder")) {
            settings.game_folder_storage.set(value);
        } else if (std.mem.eql(u8, key, "spell_folder")) {
            settings.spell_folder_storage.set(value);
        } else if (std.mem.eql(u8, key, "spell_language")) {
            settings.spell_language_storage.set(value);
        } else _ = ks.applySharedKey(&settings, key, value);
    }
    return settings;
}

pub fn format(settings: *const Settings, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try ks.writeSharedHeadKeys(settings, writer);
    try ks.writeSharedTailKeys(settings, writer);
    try ks.writeRecentLines(settings, writer);
    for (0..settings.filterCount()) |index| {
        try writer.writeAll("filter=");
        try settings.filterAt(index).format(writer);
        try writer.writeAll("\n");
    }
    try writer.print("current_filter={s}\n", .{settings.currentFilter()});
    try writer.print("collapse_item={d}\n", .{@intFromBool(settings.collapse_item)});
    try writer.print("find_text={s}\n", .{settings.findText()});
    try writer.print("find_down={d}\n", .{@intFromBool(settings.find_down)});
    try writer.print("find_match_case={d}\n", .{@intFromBool(settings.find_match_case)});
    try writer.print("find_whole_word={d}\n", .{@intFromBool(settings.find_whole_word)});
    try writer.print("game_folder={s}\n", .{settings.gameFolder()});
    try writer.print("spell_folder={s}\n", .{settings.spellFolder()});
    try writer.print("spell_language={s}\n", .{settings.spellLanguage()});
}

/// The settings file: the override when one is given, else
/// `<user_root>elk/elk.cfg`; null when neither is known.
pub fn settingsPath(buffer: []u8, override: ?[]const u8, user_root: []const u8) ?[]const u8 {
    if (override) |given| {
        if (given.len == 0 or given.len > buffer.len) return null;
        @memcpy(buffer[0..given.len], given);
        return buffer[0..given.len];
    }
    if (user_root.len == 0) return null;
    return std.fmt.bufPrint(buffer, "{s}{s}{c}{s}", .{ user_root, user_folder, std.fs.path.sep, file_name }) catch null;
}

/// The game's user root without the engine, as Platform/Paths.cpp's
/// `preferenceRoot` forms it on Linux and macOS (with its trailing
/// separator); "" when neither variable is set. Windows asks SDL (main.zig).
pub fn posixUserRoot(buffer: []u8, xdg_data_home: ?[]const u8, home: ?[]const u8) []const u8 {
    if (xdg_data_home) |xdg| if (xdg.len != 0)
        return std.fmt.bufPrint(buffer, "{s}/Nival/Blitzkrieg/", .{std.mem.trimEnd(u8, xdg, "/")}) catch "";
    if (home) |folder| if (folder.len != 0)
        return std.fmt.bufPrint(buffer, "{s}/.local/share/Nival/Blitzkrieg/", .{std.mem.trimEnd(u8, folder, "/")}) catch "";
    return "";
}

const testing = std.testing;

test "the recent databases round-trip through the file" {
    var settings: Settings = .{};
    settings.pushRecent("/b/elk.xml");
    settings.pushRecent("/a/elk.xml");
    var buffer: [1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const again = parse(writer.buffered());
    try testing.expectEqual(@as(usize, 2), again.recentCount());
    try testing.expectEqualStrings("/a/elk.xml", again.recentAt(0));
    try testing.expectEqualStrings("/b/elk.xml", again.recentAt(1));
    var forgotten = again;
    forgotten.forgetRecent("/a/elk.xml");
    try testing.expectEqualStrings("/b/elk.xml", forgotten.recentAt(0));
}

test "the named filters and the current one round-trip through the file" {
    var settings: Settings = .{};
    try testing.expectEqual(@as(usize, 5), settings.filterCount());
    try testing.expectEqualStrings("All", settings.currentFilter());
    settings.setCurrentFilter("Translated");
    var buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    try testing.expect(std.mem.indexOf(u8, writer.buffered(), "filter=Not Translated|1000|0|\n") != null);
    const again = parse(writer.buffered());
    try testing.expectEqual(@as(usize, 5), again.filter_count);
    try testing.expectEqualStrings("Translated", again.currentFilter());
    try testing.expectEqual(elk.filter.built_in[3].states, again.filterAt(3).states);

    // A filter of the user's own, with its conditions; a broken line is dropped.
    const own = parse("filter=Tanks|0011|1|units,tanks;medals\nfilter=broken|1\ncurrent_filter=Tanks\n");
    try testing.expectEqual(@as(usize, 1), own.filterCount());
    try testing.expectEqualStrings("units,tanks;medals", own.filterAt(0).conditions);
    try testing.expect(own.filterAt(0).changed);
    try testing.expectEqualStrings("Tanks", own.currentFilter());
}

test "the dialog's edits: add, rename in place, delete with the fall back to All" {
    var settings: Settings = .{};
    settings.setCurrentFilter("Outdated");
    // The first edit makes the five built-in filters the file's own.
    const tanks = settings.addFilter("  Tanks|done ").?;
    try testing.expectEqual(@as(usize, 5), tanks);
    try testing.expectEqual(@as(usize, 6), settings.filter_count);
    try testing.expectEqualStrings("Tanksdone", settings.filterAt(tanks).name);
    try testing.expectEqual(@as([4]bool, @splat(true)), settings.filterAt(tanks).states);
    // Names stay unique the MFC way.
    try testing.expectEqualStrings("Tanksdone_0", settings.filterAt(settings.addFilter("Tanksdone").?).name);
    try testing.expectEqualStrings("Tanksdone_1", settings.filterAt(settings.addFilter("Tanksdone").?).name);
    try testing.expectEqual(@as(?usize, null), settings.addFilter(" | "));

    try testing.expect(settings.setFilter(tanks, .{ .name = "ignored", .states = .{ false, false, true, true }, .changed = true, .conditions = "units,tanks" }));
    try testing.expectEqualStrings("Tanksdone|0011|1|units,tanks", settings.filter_storage[tanks].slice());

    // A rename keeps the position and the criteria; the current filter follows.
    try testing.expect(settings.renameFilter(2, "Old texts"));
    try testing.expectEqualStrings("Old texts", settings.filterAt(2).name);
    try testing.expectEqualStrings("Old texts", settings.currentFilter());
    try testing.expectEqual(elk.filter.built_in[2].states, settings.filterAt(2).states);
    try testing.expect(settings.renameFilter(2, "Old texts"));
    try testing.expectEqualStrings("Old texts", settings.filterAt(2).name);
    try testing.expect(settings.renameFilter(tanks, "Approved"));
    try testing.expectEqualStrings("Approved_0", settings.filterAt(tanks).name);
    try testing.expectEqualStrings("units,tanks", settings.filterAt(tanks).conditions);

    // Deleting the current filter falls back to All; others leave it alone.
    try testing.expect(settings.deleteFilter(3));
    try testing.expectEqualStrings("Old texts", settings.currentFilter());
    try testing.expect(settings.deleteFilter(2));
    try testing.expectEqualStrings("All", settings.currentFilter());
    try testing.expectEqualStrings("Approved", settings.filterAt(2).name);

    // Through the file and back; the last filter is never deleted.
    var buffer: [4096]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    var again = parse(writer.buffered());
    try testing.expectEqual(settings.filter_count, again.filter_count);
    try testing.expectEqualStrings("Approved_0", again.filterAt(3).name);
    while (again.filter_count > 1) try testing.expect(again.deleteFilter(0));
    try testing.expect(!again.deleteFilter(0));
}

test "collapse-item is on unless the file turns it off" {
    try testing.expect(parse("").collapse_item);
    try testing.expect(!parse("collapse_item=0\n").collapse_item);
    var buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&.{ .collapse_item = false }, &writer);
    try testing.expect(!parse(writer.buffered()).collapse_item);
}

test "the find options round-trip, the text with its spaces" {
    const defaults = parse("");
    try testing.expect(defaults.find_down and !defaults.find_match_case and !defaults.find_whole_word);
    try testing.expectEqualStrings("", defaults.findText());
    var settings: Settings = .{ .find_down = false, .find_match_case = true, .find_whole_word = true };
    settings.setFindText(" Kuban Shield \nrest");
    try testing.expectEqualStrings(" Kuban Shield ", settings.findText());
    var buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const again = parse(writer.buffered());
    try testing.expectEqualStrings(" Kuban Shield ", again.findText());
    try testing.expect(!again.find_down and again.find_match_case and again.find_whole_word);
    try testing.expectEqualStrings("Kuban", parse("find_text=Kuban\r\n").findText());
}

test "the game folder round-trips; none is ELK's own" {
    try testing.expectEqualStrings("", parse("").gameFolder());
    var settings: Settings = .{};
    settings.game_folder_storage.set("/games/Blitzkrieg");
    var buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    try testing.expectEqualStrings("/games/Blitzkrieg", parse(writer.buffered()).gameFolder());
}

test "the spell folder and language round-trip; none is ELK's own folder and its first" {
    const defaults = parse("");
    try testing.expectEqualStrings("", defaults.spellFolder());
    try testing.expectEqualStrings("", defaults.spellLanguage());
    var settings: Settings = .{};
    settings.spell_folder_storage.set("/dictionaries");
    settings.spell_language_storage.set("en_US");
    var buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    const again = parse(writer.buffered());
    try testing.expectEqualStrings("/dictionaries", again.spellFolder());
    try testing.expectEqualStrings("en_US", again.spellLanguage());
}

test "the file lives in the game's user root unless the seam names one" {
    var root_buffer: [256]u8 = undefined;
    var path_buffer: [256]u8 = undefined;
    const root = posixUserRoot(&root_buffer, "/x/data/", "/home/u");
    try testing.expectEqualStrings("/x/data/Nival/Blitzkrieg/", root);
    try testing.expectEqualStrings("/home/u/.local/share/Nival/Blitzkrieg/", posixUserRoot(&root_buffer, "", "/home/u"));
    try testing.expectEqualStrings("", posixUserRoot(&root_buffer, null, null));
    if (std.fs.path.sep == '/') try testing.expectEqualStrings("/r/elk/elk.cfg", settingsPath(&path_buffer, null, "/r/").?);
    try testing.expectEqualStrings("t.cfg", settingsPath(&path_buffer, "t.cfg", "/r/").?);
    try testing.expectEqual(@as(?[]const u8, null), settingsPath(&path_buffer, null, ""));
}
