//! `elk.cfg` (D-16): ELK's settings file in the kit's `key=value` format.
//! This slice keeps the kit's editor-neutral keys (`kit.settings.applySharedKey`
//! and the shared writers), of which ELK uses the recent list: the ten
//! recent databases, the first being the one File > Open last opened. The
//! rest of D-16 (last PAK, XLS and game folders, filters, find options,
//! fonts, toolbars, spell checking) joins with the features that use it.
//!
//! Where the file lives: `<UserRoot>elk/elk.cfg`, the user root being the
//! game's own (Platform/Paths.cpp: `$XDG_DATA_HOME/Nival/Blitzkrieg/` or
//! `~/.local/share/Nival/Blitzkrieg/` on Linux and macOS, SDL's pref path
//! or `BK_USER_ROOT` on Windows), worked out by main.zig without the engine.
//! `BK_ELK_SETTINGS` names another file instead: the test seam, like the
//! map editor's `BK_EDITOR_SETTINGS`, so a test never touches the user's own.
const std = @import("std");
const kit = @import("editor_kit");

const ks = kit.settings;
const FixedPath = ks.FixedPath;

pub const settings_env = "BK_ELK_SETTINGS";
pub const user_folder = "elk";
pub const file_name = "elk.cfg";
/// The longest path a recent entry holds.
pub const max_path = ks.max_path;

/// The kit's shared fields, under the names its helpers expect.
pub const Settings = struct {
    scroll_speed: f32 = ks.default_scroll_speed,
    autosave: bool = true,
    autosave_minutes: u32 = ks.default_autosave_minutes,
    recent_storage: [ks.recent_capacity]FixedPath = @splat(.{}),
    recent_count: usize = 0,
    game_parameters: FixedPath = .{},
    hidden_panels: u32 = 0,

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
        _ = ks.applySharedKey(&settings, std.mem.trim(u8, trimmed[0..eq], " \t"), std.mem.trim(u8, trimmed[eq + 1 ..], " \t"));
    }
    return settings;
}

pub fn format(settings: *const Settings, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try ks.writeSharedHeadKeys(settings, writer);
    try ks.writeSharedTailKeys(settings, writer);
    try ks.writeRecentLines(settings, writer);
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
