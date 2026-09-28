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

pub const min_scroll_speed: f32 = 0.25;
pub const max_scroll_speed: f32 = 4.0;
pub const default_scroll_speed: f32 = 1.0;
pub const min_autosave_minutes: u32 = 1;
pub const max_autosave_minutes: u32 = 60;
pub const default_autosave_minutes: u32 = 2;

/// D-27: the last ten maps.
pub const recent_capacity = 10;

/// `BkEditorPathSet`'s own `char[1024]` (bridge.h) - big enough for any real
/// path, and matches the engine's own convention for a fixed path buffer.
pub const max_path = 1024;

const FixedPath = struct {
    buffer: [max_path]u8 = undefined,
    len: usize = 0,

    fn slice(self: *const FixedPath) []const u8 {
        return self.buffer[0..self.len];
    }

    fn set(self: *FixedPath, text: []const u8) void {
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
    maps_folder_storage: FixedPath = .{},
    recent_storage: [recent_capacity]FixedPath = [_]FixedPath{.{}} ** recent_capacity,
    recent_count: usize = 0,

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
};

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
    } else if (std.mem.eql(u8, key, "maps_folder")) {
        settings.setMapsFolder(value);
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
    if (self.mapsFolder().len != 0) try writer.print("maps_folder={s}\n", .{self.mapsFolder()});
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
    settings.setMapsFolder("/Users/me/maps");

    var buffer: [1024]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try format(&settings, &writer);

    const round_tripped = parse(writer.buffered());
    try std.testing.expectEqual(@as(f32, 2.5), round_tripped.scroll_speed);
    try std.testing.expect(!round_tripped.autosave);
    try std.testing.expectEqual(@as(u32, 10), round_tripped.autosave_minutes);
    try std.testing.expectEqualStrings("/Users/me/maps", round_tripped.mapsFolder());
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

