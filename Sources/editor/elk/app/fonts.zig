//! The UI font (D-13): a stack of system fonts so any translation is
//! readable while editing. The first font found is the base, every further
//! one is merged into it as a fallback for the characters it lacks, and
//! ImGui 1.92 rasterises glyphs on demand, so nothing is limited to ranges
//! chosen up front. The override setting of D-13 comes with a later slice.
//! ImWchar is 16-bit in this build, so characters beyond the Basic
//! Multilingual Plane are not drawn; no game text uses them.
const std = @import("std");
const builtin = @import("builtin");
const imgui = @import("editor_imgui");

const ig = imgui.c;

pub const size_pixels: f32 = 16.0;

/// Latin, Latin Extended, Cyrillic, Chinese, Japanese and Korean: what the
/// game's existing translations are written in. The smoke asks the loaded
/// stack for every one of these.
pub const samples = [_]u16{ 'A', 0x00DF, 0x0142, 0x0159, 0x0416, 0x044F, 0x4E2D, 0x6587, 0x3042, 0x30AB, 0xD55C };

/// Base font first, then fallbacks; a missing file is skipped.
const linux_fonts = [_][]const u8{
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
};
const macos_fonts = [_][]const u8{
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/Library/Fonts/Arial Unicode.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/AppleSDGothicNeo.ttc",
};
/// Below `%WINDIR%\Fonts\`.
const windows_fonts = [_][]const u8{ "segoeui.ttf", "arial.ttf", "msyh.ttc", "meiryo.ttc", "msgothic.ttc", "malgun.ttf" };

/// Loads the stack into ImGui's atlas and returns how many files it took;
/// 0 leaves ImGui's own default font (ASCII only). Call before the first
/// frame. `windir` is `%WINDIR%` on Windows, ignored elsewhere.
pub fn load(io: std.Io, windir: ?[]const u8) usize {
    const atlas = ig.igGetIO().*.Fonts;
    var loaded: usize = 0;
    var path_buffer: [std.Io.Dir.max_path_bytes:0]u8 = undefined;
    const names: []const []const u8 = switch (builtin.os.tag) {
        .windows => &windows_fonts,
        .macos => &macos_fonts,
        else => &linux_fonts,
    };
    for (names) |name| {
        const path = if (builtin.os.tag == .windows)
            std.mem.printSentinel(&path_buffer, "{s}\\Fonts\\{s}", .{ windir orelse "C:\\Windows", name }, 0) catch continue
        else
            std.mem.printSentinel(&path_buffer, "{s}", .{name}, 0) catch continue;
        // ImGui asserts on a file it cannot read, so only existing ones go in.
        _ = std.Io.Dir.cwd().statFile(io, path, .{}) catch continue;
        var config = defaultConfig();
        config.MergeMode = loaded != 0;
        if (ig.ImFontAtlas_AddFontFromFileTTF(atlas, path.ptr, size_pixels, &config, null) != null) loaded += 1;
    }
    return loaded;
}

/// Whether the loaded stack has a glyph for `codepoint` (any merged source).
pub fn covers(codepoint: u16) bool {
    const atlas = ig.igGetIO().*.Fonts;
    if (atlas.*.Fonts.Size == 0) return false;
    return ig.ImFont_IsGlyphInFont(atlas.*.Fonts.Data[0], codepoint);
}

/// ImFontConfig's C++ constructor, which the C API does not expose.
fn defaultConfig() ig.ImFontConfig {
    var config = std.mem.zeroes(ig.ImFontConfig);
    config.FontDataOwnedByAtlas = true;
    config.ExtraSizeScale = 1.0;
    config.GlyphMaxAdvanceX = std.math.floatMax(f32);
    config.RasterizerMultiply = 1.0;
    config.RasterizerDensity = 1.0;
    return config;
}
