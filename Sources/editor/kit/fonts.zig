//! The UI font (D059, D078): the bundled Noto faces, never the system's, so a
//! translation looks the same on every machine. Noto Sans is the base, Noto
//! Sans Devanagari and the two CJK faces are merged into it for the
//! characters it lacks, and ImGui 1.92 rasterises glyphs on demand, so nothing
//! is limited to ranges chosen up front. ImWchar is 16-bit in this build, so
//! characters beyond the Basic Multilingual Plane are not drawn; no game text
//! uses them.
//!
//! The faces are read from `<exe dir>/Fonts/` (install-elk puts them there),
//! or from `BK_FONT_DIR` when that is set. A face that is not there is an
//! error that names the full path.
//!
//! Hindi cannot be drawn glyph by glyph, so the Devanagari face is merged
//! through a loader of its own (kit/imgui/imgui_backend.cpp): text goes through
//! `encodeHindi` first, which replaces each shaped cluster by one Private Use
//! Area character, and the loader draws that character as the whole shaped
//! cluster. Half forms take the advance of the cluster shaped alone.
const std = @import("std");
const imgui = @import("editor_imgui");
const sdl3 = @import("sdl3");

const ig = imgui.c;

pub const size_pixels: f32 = 16.0;

/// The environment variable that replaces `<exe dir>/Fonts/`.
pub const dir_env = "BK_FONT_DIR";

/// Latin, Latin Extended, Cyrillic, Chinese, Japanese and Korean: what the
/// game's existing translations are written in. The smoke asks the loaded
/// stack for every one of these.
/// Korean is not among the ten languages, so the bundled faces do not cover it. Devanagari is the
/// last, drawn by the loader's own source.
pub const samples = [_]u16{ 'A', 0x00DF, 0x0142, 0x0159, 0x0416, 0x044F, 0x4E2D, 0x6587, 0x3042, 0x30AB, 0x0939 };

pub const Face = enum {
    latin,
    devanagari,
    sc,
    jp,

    pub fn fileName(face: Face) []const u8 {
        return switch (face) {
            .latin => "NotoSans-Regular.ttf",
            .devanagari => "NotoSansDevanagari-Regular.ttf",
            .sc => "NotoSansSC-Regular.otf",
            .jp => "NotoSansJP-Regular.otf",
        };
    }
};

pub const Error = error{ FontMissing, PathTooLong, FontRead, AtlasFailed, OutOfMemory };

/// The faces in merge order for a language. The first that has a glyph wins,
/// and Han characters differ between the Chinese and the Japanese face, so
/// Japanese puts its own face before the Chinese one and every other language
/// the other way round.
pub fn mergeOrder(code: []const u8) [4]Face {
    if (std.mem.eql(u8, code, "ja")) return .{ .latin, .devanagari, .jp, .sc };
    return .{ .latin, .devanagari, .sc, .jp };
}

/// The font directory: `override` (BK_FONT_DIR) when it is set and not empty,
/// else `base` (SDL_GetBasePath, which ends with a separator) plus `Fonts/`.
/// Null when neither is known.
pub fn resolveDir(buffer: []u8, override: ?[]const u8, base: ?[]const u8) ?[]const u8 {
    if (override) |dir| {
        if (dir.len != 0) return std.fmt.bufPrint(buffer, "{s}", .{dir}) catch null;
    }
    const root = base orelse return null;
    const sep = if (root.len != 0 and (root[root.len - 1] == '/' or root[root.len - 1] == '\\')) "" else &[_]u8{std.fs.path.sep};
    return std.fmt.bufPrint(buffer, "{s}{s}Fonts{c}", .{ root, sep, std.fs.path.sep }) catch null;
}

/// `<dir>/<face file>` as a sentinel string in `buffer`.
pub fn facePath(buffer: []u8, dir: []const u8, face: Face) Error![:0]const u8 {
    const sep = if (dir.len != 0 and (dir[dir.len - 1] == '/' or dir[dir.len - 1] == '\\')) "" else &[_]u8{std.fs.path.sep};
    return std.fmt.bufPrintSentinel(buffer, "{s}{s}{s}", .{ dir, sep, face.fileName() }, 0) catch error.PathTooLong;
}

/// Checks that every face is in `dir` (relative to `root`, or absolute); the
/// error line names the first path that is not.
pub fn verifyDir(io: std.Io, root: std.Io.Dir, dir: []const u8) Error!void {
    var buffer: [std.Io.Dir.max_path_bytes:0]u8 = undefined;
    for (std.enums.values(Face)) |face| {
        const path = try facePath(&buffer, dir, face);
        _ = root.statFile(io, path, .{}) catch {
            std.debug.print("fonts: missing font file {s}\n", .{path});
            return error.FontMissing;
        };
    }
}

var state_dir: [std.Io.Dir.max_path_bytes]u8 = undefined;
var state_dir_len: usize = 0;
var state_io: ?std.Io = null;
var state_gpa: ?std.mem.Allocator = null;

/// The font directory from the environment and SDL; null when neither gives one.
pub fn defaultDir(buffer: []u8, override: ?[]const u8) ?[]const u8 {
    const base: ?[]const u8 = if (sdl3.c.SDL_GetBasePath()) |ptr| std.mem.span(ptr) else null;
    return resolveDir(buffer, override, base);
}

/// Loads the stack for `code` into ImGui's atlas and returns how many faces it
/// took. Call before the first frame, after the window (SDL) exists.
/// `override` is `BK_FONT_DIR`. A missing face is `error.FontMissing`, named on
/// stderr by its full path, and leaves ImGui's own default font.
pub fn load(gpa: std.mem.Allocator, io: std.Io, override: ?[]const u8, code: []const u8) Error!usize {
    const dir = defaultDir(&state_dir, override) orelse return error.FontMissing;
    state_dir_len = dir.len;
    state_io = io;
    state_gpa = gpa;
    return try build(code);
}

/// Rebuilds the atlas for another language between frames (a language switch).
/// `load` must have succeeded before.
pub fn reload(code: []const u8) Error!usize {
    const atlas = ig.igGetIO().*.Fonts;
    ig.ImFontAtlas_ClearFonts(atlas);
    return try build(code);
}

fn build(code: []const u8) Error!usize {
    const io = state_io orelse return error.FontMissing;
    const gpa = state_gpa orelse return error.FontMissing;
    const dir = state_dir[0..state_dir_len];
    try verifyDir(io, std.Io.Dir.cwd(), dir);
    const atlas = ig.igGetIO().*.Fonts;
    var buffer: [std.Io.Dir.max_path_bytes:0]u8 = undefined;
    var loaded: usize = 0;
    for (mergeOrder(code)) |face| {
        const path = try facePath(&buffer, dir, face);
        if (face == .devanagari) {
            const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 * 1024 * 1024)) catch |err| {
                std.debug.print("fonts: {s} cannot be read: {s}\n", .{ path, @errorName(err) });
                return error.FontRead;
            };
            defer gpa.free(bytes);
            if (!ig.bkFontsAddDevanagari(bytes.ptr, bytes.len, size_pixels)) return error.AtlasFailed;
            loaded += 1;
            continue;
        }
        var config = defaultConfig();
        config.MergeMode = loaded != 0;
        if (ig.ImFontAtlas_AddFontFromFileTTF(atlas, path.ptr, size_pixels, &config, null) == null) return error.AtlasFailed;
        loaded += 1;
    }
    return loaded;
}

/// Whether the loaded stack has a glyph for `codepoint` (any merged source).
pub fn covers(codepoint: u16) bool {
    const atlas = ig.igGetIO().*.Fonts;
    if (atlas.*.Fonts.Size == 0) return false;
    return ig.ImFont_IsGlyphInFont(atlas.*.Fonts.Data[0], codepoint);
}

/// `text` (UTF-8, Devanagari) with each shaped cluster replaced by its PUA
/// character, into `buffer`. Null when the result does not fit or the Devanagari
/// face is not loaded. A full cluster table (logged once) returns the text
/// unshaped.
pub fn encodeHindi(buffer: []u8, text: [:0]const u8) ?[:0]const u8 {
    if (buffer.len == 0) return null;
    const len = ig.bkFontsEncodeHindi(text.ptr, buffer.ptr, buffer.len);
    if (len < 0) return null;
    const n: usize = @intCast(len);
    return buffer[0..n :0];
}

/// How many Private Use Area slots the encoded Hindi text has taken.
pub fn clusterCount() usize {
    return ig.bkFontsClusterCount();
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

test "Japanese merges its own CJK face before the Chinese one, every other language after" {
    const ja = mergeOrder("ja");
    try std.testing.expectEqual(Face.latin, ja[0]);
    try std.testing.expectEqual(Face.devanagari, ja[1]);
    try std.testing.expectEqual(Face.jp, ja[2]);
    try std.testing.expectEqual(Face.sc, ja[3]);
    for ([_][]const u8{ "en", "de", "fr", "ru", "pl", "es", "it", "zh", "hi", "ko", "" }) |code| {
        const order = mergeOrder(code);
        try std.testing.expectEqual(Face.latin, order[0]);
        try std.testing.expectEqual(Face.sc, order[2]);
        try std.testing.expectEqual(Face.jp, order[3]);
    }
}

test "the font directory is BK_FONT_DIR when set, else Fonts beside the executable" {
    var buffer: [128]u8 = undefined;
    try std.testing.expectEqualStrings("/tmp/faces", resolveDir(&buffer, "/tmp/faces", "/opt/game/").?);
    const sep = std.fs.path.sep;
    const beside = try std.fmt.allocPrint(std.testing.allocator, "/opt/game/Fonts{c}", .{sep});
    defer std.testing.allocator.free(beside);
    try std.testing.expectEqualStrings(beside, resolveDir(&buffer, null, "/opt/game/").?);
    try std.testing.expectEqualStrings(beside, resolveDir(&buffer, "", "/opt/game/").?);
    try std.testing.expect(resolveDir(&buffer, null, null) == null);
}

test "a face path joins the directory and the file with one separator" {
    var buffer: [128:0]u8 = undefined;
    const sep = std.fs.path.sep;
    const with = try facePath(&buffer, "/tmp/faces/", .sc);
    try std.testing.expectEqualStrings("/tmp/faces/NotoSansSC-Regular.otf", with);
    var expected: [64]u8 = undefined;
    const without = try facePath(&buffer, "/tmp/faces", .latin);
    try std.testing.expectEqualStrings(try std.fmt.bufPrint(&expected, "/tmp/faces{c}NotoSans-Regular.ttf", .{sep}), without);
}

test "a missing face is an error naming its path, a complete directory passes" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try std.testing.expectError(error.FontMissing, verifyDir(std.testing.io, tmp.dir, "."));
    for (std.enums.values(Face)) |face| try tmp.dir.writeFile(std.testing.io, .{ .sub_path = face.fileName(), .data = "x" });
    try verifyDir(std.testing.io, tmp.dir, ".");
    try tmp.dir.deleteFile(std.testing.io, "NotoSansJP-Regular.otf");
    try std.testing.expectError(error.FontMissing, verifyDir(std.testing.io, tmp.dir, "."));
}
