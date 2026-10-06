//! `resourceeditor.cfg`: the Resource Editor's own settings file, in the
//! kit's `key=value` format. It shares the kit's editor-neutral keys
//! (`kit.settings.applySharedKey` and the shared writers: scroll speed,
//! autosave, the interval, game parameters, hidden panels, recent files) and
//! adds its own: the last active sub-editor (MFC's "Active Frame"), the
//! folder the Open and Save As dialogs start in, and Set Directories' source
//! and game folders. The map-only keys
//! (`default_format`, `maps_folder`) are not written: a project's format is
//! its kind's extension, and its folder is a project folder, not a map
//! folder. `mapeditor.cfg` is untouched by any of this.
//!
//! Where the file lives (D002): `<UserRoot>resourceeditor/resourceeditor.cfg`,
//! with ImGui's `layout.ini` beside it. The `BK_RESOURCE_EDITOR_SETTINGS`
//! environment variable names another file instead - the test seam, like the
//! map editor's `BK_EDITOR_SETTINGS`, so a test never touches the user's own.
const std = @import("std");
const kit = @import("editor_kit");

const ks = kit.settings;
const FixedPath = ks.FixedPath;

/// The environment variable that overrides where the settings file is.
pub const settings_env = "BK_RESOURCE_EDITOR_SETTINGS";
/// The user-root folder everything the Resource Editor writes for the user
/// lives in (D002).
pub const user_folder = "resourceeditor";
pub const file_name = "resourceeditor.cfg";
pub const layout_name = "layout.ini";
pub const templates_name = "templates";

pub const Settings = struct {
    scroll_speed: f32 = ks.default_scroll_speed,
    autosave: bool = true,
    autosave_minutes: u32 = ks.default_autosave_minutes,
    recent_storage: [ks.recent_capacity]FixedPath = [_]FixedPath{.{}} ** ks.recent_capacity,
    recent_count: usize = 0,
    game_parameters: FixedPath = .{},
    hidden_panels: u32 = 0,
    /// The Editors menu's last choice as the kind's integer
    /// (`panels_logic.restoreEditor` turns it back into a sub-editor); null
    /// until one was chosen.
    last_editor: ?i32 = null,
    /// Where Open and Save As start; empty means the dialog's own default.
    projects_folder: FixedPath = .{},
    /// Tools > Set Directories (MFC's "Composer Source Directory"): the
    /// project sources' root, where Batch Mode starts and where Picture
    /// Options writes a sub-editor's gamma.cfg; empty until set.
    source_folder: FixedPath = .{},
    /// Tools > Set Directories (MFC's "Composer Executive Directory"): the
    /// folder holding the Game that Run Blitzkrieg starts; empty means the
    /// Game installed beside the editor (D-08).
    game_folder: FixedPath = .{},

    pub fn sourceFolder(self: *const Settings) []const u8 {
        return self.source_folder.slice();
    }

    pub fn gameFolder(self: *const Settings) []const u8 {
        return self.game_folder.slice();
    }

    pub fn gameParameters(self: *const Settings) []const u8 {
        return self.game_parameters.slice();
    }

    pub fn setGameParameters(self: *Settings, text: []const u8) void {
        ks.setGameParametersField(&self.game_parameters, text);
    }

    pub fn projectsFolder(self: *const Settings) []const u8 {
        return self.projects_folder.slice();
    }

    pub fn recentCount(self: *const Settings) usize {
        return self.recent_count;
    }

    /// `0` is the most recently used.
    pub fn recentAt(self: *const Settings, index: usize) []const u8 {
        return self.recent_storage[index].slice();
    }

    pub fn pushRecent(self: *Settings, os_path: []const u8) void {
        ks.pushRecentField(&self.recent_storage, &self.recent_count, os_path);
    }

    pub fn removeRecent(self: *Settings, index: usize) void {
        ks.removeRecentField(&self.recent_storage, &self.recent_count, index);
    }

    /// Removes `os_path` from the list if it is there (a recent entry that
    /// no longer opens).
    pub fn forgetRecent(self: *Settings, os_path: []const u8) void {
        var i: usize = 0;
        while (i < self.recent_count) : (i += 1) {
            if (ks.sameOsPath(self.recentAt(i), os_path)) return self.removeRecent(i);
        }
    }
};

/// Lenient like the kit's parser: unknown or malformed lines keep defaults.
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
        if (ks.applySharedKey(&settings, key, value)) continue;
        if (std.mem.eql(u8, key, "last_editor")) {
            settings.last_editor = std.fmt.parseInt(i32, value, 10) catch null;
        } else if (std.mem.eql(u8, key, "projects_folder")) {
            settings.projects_folder.set(value);
        } else if (std.mem.eql(u8, key, "source_folder")) {
            settings.source_folder.set(value);
        } else if (std.mem.eql(u8, key, "game_folder")) {
            settings.game_folder.set(value);
        }
    }
    return settings;
}

/// The shared head, this editor's own keys, the shared tail, then the recent
/// lines: the same shape as `mapeditor.cfg`, whose own keys sit in the same
/// place.
pub fn format(settings: *const Settings, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    try ks.writeSharedHeadKeys(settings, writer);
    if (settings.last_editor) |value| try writer.print("last_editor={d}\n", .{value});
    if (settings.projectsFolder().len != 0) try writer.print("projects_folder={s}\n", .{settings.projectsFolder()});
    if (settings.sourceFolder().len != 0) try writer.print("source_folder={s}\n", .{settings.sourceFolder()});
    if (settings.gameFolder().len != 0) try writer.print("game_folder={s}\n", .{settings.gameFolder()});
    try ks.writeSharedTailKeys(settings, writer);
    try ks.writeRecentLines(settings, writer);
}

/// `override` (the `BK_RESOURCE_EDITOR_SETTINGS` value) when set, otherwise
/// `<user_root>resourceeditor/resourceeditor.cfg`. Null when there is neither
/// (no user root) or the path does not fit `buffer`.
pub fn settingsPath(buffer: []u8, override: ?[]const u8, user_root: []const u8) ?[]const u8 {
    if (override) |path| {
        if (path.len == 0 or path.len > buffer.len) return null;
        @memcpy(buffer[0..path.len], path);
        return buffer[0..path.len];
    }
    if (user_root.len == 0) return null;
    return std.fmt.bufPrint(buffer, "{s}{s}{c}{s}", .{ user_root, user_folder, std.fs.path.sep, file_name }) catch null;
}

/// The GUI sub-editor's own templates ("Create new template" writes here,
/// the palette lists it after Data/Editor/UI), in the settings file's folder.
/// Never under Data.
pub fn templatesPath(buffer: []u8, settings_path: []const u8) ?[]const u8 {
    const folder = std.fs.path.dirname(settings_path) orelse return null;
    return std.fmt.bufPrint(buffer, "{s}{c}{s}", .{ folder, std.fs.path.sep, templates_name }) catch null;
}

/// ImGui's `layout.ini`, in the settings file's own folder.
pub fn layoutPath(buffer: []u8, settings_path: []const u8) ?[]const u8 {
    const folder = std.fs.path.dirname(settings_path) orelse return null;
    return std.fmt.bufPrint(buffer, "{s}{c}{s}", .{ folder, std.fs.path.sep, layout_name }) catch null;
}

const testing = std.testing;

test "defaults, and a round trip of every key" {
    const empty = parse("");
    try testing.expect(empty.autosave);
    try testing.expectEqual(ks.default_autosave_minutes, empty.autosave_minutes);
    try testing.expect(empty.last_editor == null);
    try testing.expectEqual(@as(usize, 0), empty.recentCount());

    var settings: Settings = .{};
    settings.scroll_speed = 2;
    settings.autosave = false;
    settings.autosave_minutes = 7;
    settings.last_editor = 11;
    settings.projects_folder.set("/home/me/projects");
    settings.setGameParameters("-windowed");
    settings.hidden_panels = 4;
    settings.pushRecent("/p/b.wpn");
    settings.pushRecent("/p/a.scp");
    var buffer: [1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    try testing.expectEqualStrings("scroll_speed=2\nautosave=off\nautosave_minutes=7\nlast_editor=11\n" ++
        "projects_folder=/home/me/projects\ngame_parameters=-windowed\nhidden_panels=4\nrecent=/p/a.scp\nrecent=/p/b.wpn\n", writer.buffered());

    const back = parse(writer.buffered());
    try testing.expectEqual(@as(f32, 2), back.scroll_speed);
    try testing.expect(!back.autosave);
    try testing.expectEqual(@as(u32, 7), back.autosave_minutes);
    try testing.expectEqual(@as(i32, 11), back.last_editor.?);
    try testing.expectEqualStrings("/home/me/projects", back.projectsFolder());
    try testing.expectEqualStrings("-windowed", back.gameParameters());
    try testing.expectEqual(@as(u32, 4), back.hidden_panels);
    try testing.expectEqual(@as(usize, 2), back.recentCount());
    try testing.expectEqualStrings("/p/a.scp", back.recentAt(0));
}

test "the map-only keys are neither read nor written, malformed values keep defaults" {
    const settings = parse("default_format=xml\r\nmaps_folder=/m\r\nlast_editor=abc\r\nautosave_minutes=999\r\n# a comment\r\n");
    try testing.expect(settings.last_editor == null);
    try testing.expectEqual(ks.max_autosave_minutes, settings.autosave_minutes);
    var buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    try testing.expect(std.mem.indexOf(u8, writer.buffered(), "default_format") == null);
    try testing.expect(std.mem.indexOf(u8, writer.buffered(), "maps_folder") == null);
}

test "Set Directories' source and game folders round-trip after projects_folder, and stay out when empty" {
    var settings: Settings = .{};
    settings.source_folder.set("/src/complete");
    settings.game_folder.set("/opt/blitzkrieg");
    var buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);
    try testing.expect(std.mem.indexOf(u8, writer.buffered(), "source_folder=/src/complete\ngame_folder=/opt/blitzkrieg\n") != null);
    const back = parse(writer.buffered());
    try testing.expectEqualStrings("/src/complete", back.sourceFolder());
    try testing.expectEqualStrings("/opt/blitzkrieg", back.gameFolder());

    var empty_writer: std.Io.Writer = .fixed(&buffer);
    try format(&Settings{}, &empty_writer);
    try testing.expect(std.mem.indexOf(u8, empty_writer.buffered(), "_folder") == null);
}

test "forgetRecent drops one entry by path" {
    var settings: Settings = .{};
    settings.pushRecent("a.wpn");
    settings.pushRecent("b.wpn");
    settings.forgetRecent("a.wpn");
    try testing.expectEqual(@as(usize, 1), settings.recentCount());
    try testing.expectEqualStrings("b.wpn", settings.recentAt(0));
    settings.forgetRecent("absent.wpn");
    try testing.expectEqual(@as(usize, 1), settings.recentCount());
}

test "settings and layout paths: the seam wins, otherwise the user root's resourceeditor folder" {
    var buffer: [256]u8 = undefined;
    try testing.expectEqualStrings("/tmp/x.cfg", settingsPath(&buffer, "/tmp/x.cfg", "/home/me/.local/share/").?);
    const sep = [_]u8{std.fs.path.sep};
    try testing.expectEqualStrings("/u/resourceeditor" ++ sep ++ "resourceeditor.cfg", settingsPath(&buffer, null, "/u/").?);
    try testing.expect(settingsPath(&buffer, null, "") == null);
    try testing.expect(settingsPath(&buffer, "", "/u/") == null);
    var layout_buffer: [256]u8 = undefined;
    try testing.expectEqualStrings("/tmp/cfg" ++ sep ++ "layout.ini", layoutPath(&layout_buffer, "/tmp/cfg/x.cfg").?);
    try testing.expect(layoutPath(&layout_buffer, "x.cfg") == null);
}

test "the user's template folder sits beside the settings file" {
    var buffer: [256]u8 = undefined;
    const sep = [_]u8{std.fs.path.sep};
    try testing.expectEqualStrings("/tmp/cfg" ++ sep ++ "templates", templatesPath(&buffer, "/tmp/cfg/x.cfg").?);
    try testing.expect(templatesPath(&buffer, "x.cfg") == null);
}
