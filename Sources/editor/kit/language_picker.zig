//! The language picker every editor on the kit offers (D061): System, then the ten languages under
//! their own names. `draw` shows it; choosing is the caller's to apply, because the font stack and
//! the string tables may only change between frames: write the `ui_language` setting, call
//! `l10n.setLanguage` (or `applySetting`) and `fonts.reload`, then let the next frame draw.
//!
//! The selection logic is pure and tested here: the list order and the mapping from a row to the
//! setting value it stands for.
const std = @import("std");
const imgui = @import("editor_imgui");
const l10n = @import("l10n.zig");
const kit_settings = @import("settings.zig");
const fonts = @import("fonts.zig");
const sdl3 = @import("sdl3");

const ig = imgui.c;

/// Row 0 is System, then the languages in `l10n.codes` order.
pub const row_count = l10n.codes.len + 1;

/// The `ui_language` value row `index` stands for: `system` or a language code. Null beyond the list.
pub fn valueAt(index: usize) ?[]const u8 {
    if (index >= row_count) return null;
    return if (index == 0) l10n.system_setting else l10n.codes[index - 1];
}

/// The row a `ui_language` value selects. A value no row stands for (an unknown code) selects System,
/// which is what l10n falls back to for it.
pub fn rowOf(value: []const u8) usize {
    for (l10n.codes, 1..) |code, row| {
        if (std.ascii.eqlIgnoreCase(value, code)) return row;
    }
    return 0;
}

/// The table code a `ui_language` setting stands for. `system` follows the operating system's
/// preferred languages; a scripted run never does, so its frames are English on any machine.
pub fn chooseLanguage(settings: anytype, scripted: bool) []const u8 {
    const setting = kit_settings.uiLanguage(settings);
    if (scripted and std.mem.eql(u8, setting, l10n.system_setting)) return "en";
    var storage: [8][48]u8 = undefined;
    var locales: [8][]const u8 = undefined;
    var count: usize = 0;
    var listed: c_int = 0;
    if (sdl3.c.SDL_GetPreferredLocales(&listed)) |list| {
        defer sdl3.c.SDL_free(@ptrCast(list));
        while (count < locales.len and count < @as(usize, @intCast(listed))) : (count += 1) {
            const locale = list[count] orelse break;
            const language = std.mem.span(locale.*.language);
            locales[count] = (if (locale.*.country) |country|
                std.fmt.bufPrint(&storage[count], "{s}_{s}", .{ language, std.mem.span(country) })
            else
                std.fmt.bufPrint(&storage[count], "{s}", .{language})) catch language;
        }
    }
    const resolved = l10n.resolveLanguage(setting, locales[0..count]);
    if (resolved.unknown_setting) std.debug.print("l10n: unknown ui_language {s}, using {s}\n", .{ setting, resolved.code });
    return resolved.code;
}

/// What every editor does once its window and ImGui exist, before the first frame: resolves the
/// language from `settings`, loads the bundled fonts for it (`BK_FONT_DIR` or `<exe dir>/Fonts`),
/// points the tables at `BK_L10N_DIR` and selects the language. Returns the table code, or the
/// font error (already named on stderr by the loader) so the caller can stop with its own prefix.
pub fn setup(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, settings: anytype, scripted: bool) fonts.Error![]const u8 {
    const code = chooseLanguage(settings, scripted);
    const font_dir: ?[]const u8 = environ.getAlloc(gpa, fonts.dir_env) catch null;
    defer if (font_dir) |value| gpa.free(value);
    _ = try fonts.load(gpa, io, font_dir, code);
    const table_dir: ?[]const u8 = environ.getAlloc(gpa, l10n.override_env) catch null;
    defer if (table_dir) |value| gpa.free(value);
    l10n.setOverrideDir(io, table_dir);
    l10n.setLanguage(code);
    return code;
}

/// What the picker returns when the user picks a row other than the current one.
pub const Choice = struct { value: []const u8 };

/// A row's label as drawn, in `buffer`: "System" is translated, a language keeps its own name (the
/// Hindi one cluster-encoded for the merged font).
fn rowLabel(buffer: []u8, index: usize) [:0]const u8 {
    if (index == 0) return std.mem.span(l10n.t("kit.language.system"));
    const name = l10n.nativeName(l10n.codes[index - 1]);
    if (std.mem.eql(u8, l10n.codes[index - 1], "hi")) {
        var plain: [64:0]u8 = undefined;
        const z = std.mem.printSentinel(&plain, "{s}", .{name}, 0) catch return "hi";
        if (fonts.encodeHindi(buffer, z)) |encoded| return encoded;
    }
    return std.mem.printSentinel(buffer, "{s}", .{name}, 0) catch "?";
}

/// Draws the combo labelled `kit.language.label` with `current` (a `ui_language` value) selected.
/// Returns the choice on the frame the user picks another row.
pub fn draw(current: []const u8) ?Choice {
    var preview_buffer: [96:0]u8 = undefined;
    const selected = rowOf(current);
    const preview = rowLabel(&preview_buffer, selected);
    if (!ig.igBeginCombo(l10n.id("kit.language.label"), preview.ptr, 0)) return null;
    defer ig.igEndCombo();
    var choice: ?Choice = null;
    for (0..row_count) |row| {
        var label_buffer: [96:0]u8 = undefined;
        const label = rowLabel(&label_buffer, row);
        if (ig.igSelectableEx(label.ptr, row == selected, 0, .{ .x = 0, .y = 0 }) and row != selected) {
            choice = .{ .value = valueAt(row).? };
        }
    }
    return choice;
}

test "the list is System, then English and the nine languages in l10n order" {
    try std.testing.expectEqual(@as(usize, 11), row_count);
    try std.testing.expectEqualStrings("system", valueAt(0).?);
    try std.testing.expectEqualStrings("en", valueAt(1).?);
    for (l10n.codes, 1..) |code, row| try std.testing.expectEqualStrings(code, valueAt(row).?);
    try std.testing.expect(valueAt(row_count) == null);
}

test "a value selects its row, system and anything unknown select System" {
    try std.testing.expectEqual(@as(usize, 0), rowOf("system"));
    try std.testing.expectEqual(@as(usize, 0), rowOf(""));
    try std.testing.expectEqual(@as(usize, 0), rowOf("xx"));
    try std.testing.expectEqual(@as(usize, 1), rowOf("en"));
    for (l10n.codes, 1..) |code, row| {
        try std.testing.expectEqual(row, rowOf(code));
        try std.testing.expectEqualStrings(code, valueAt(rowOf(code)).?);
    }
}
