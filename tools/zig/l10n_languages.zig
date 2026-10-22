//! The nine languages of M002 (D060, D068), shared by the language mods build
//! (tools/zig/language_mods.zig) and the completeness gate (tools/zig/l10n_check.zig) so both read
//! one table. The mods ship texts only since M004 S05; bk_text draws them with the shipped faces.
const std = @import("std");

pub const Language = struct {
    /// The overlay folder, `Localization/game/<code>`.
    code: []const u8,
    /// The mod folder below `mods/`.
    mod: []const u8,
    /// The name the language option lists (`MODName`).
    name: []const u8,
    /// The face below `vendor/fonts` the glyph rule checks the language's texts against.
    face: []const u8,
};

pub const languages = [_]Language{
    .{ .code = "ru", .mod = "lang-ru", .name = "Русский", .face = "NotoSans-Regular.ttf" },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .name = "简体中文", .face = "NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .name = "日本語", .face = "NotoSansJP-Regular.otf" },
    .{ .code = "hi", .mod = "lang-hi", .name = "हिन्दी", .face = "NotoSansDevanagari-Regular.ttf" },
    .{ .code = "es", .mod = "lang-es", .name = "Español", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "fr", .mod = "lang-fr", .name = "Français", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "de", .mod = "lang-de", .name = "Deutsch", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "pt-BR", .mod = "lang-pt-br", .name = "Português (Brasil)", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "id", .mod = "lang-id", .name = "Bahasa Indonesia", .face = "LiberationSerif-Regular.ttf" },
};

/// The language code bk_text shapes and chains faces by: the primary subtag of the overlay code
/// (`zh-Hans` is `zh`, `pt-BR` is `pt`). The codes of the table are lowercase there already.
pub fn bkTextCode(code: []const u8) []const u8 {
    return code[0 .. std.mem.indexOfScalar(u8, code, '-') orelse code.len];
}
