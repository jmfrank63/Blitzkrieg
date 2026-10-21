//! The nine languages of M002 (D060, D068), shared by the language mods build
//! (tools/zig/language_mods.zig) and the completeness gate (tools/zig/l10n_check.zig) so both read
//! one table. The shipped 1.tfd files (Times New Roman, charset 204) hold ASCII and the Cyrillic set,
//! none of the accented Latin letters, so every language mod ships fonts of its own. The Latin ones
//! are the game's fonts with the missing letters added from Liberation Serif, whose widths are Times
//! New Roman's, so an added letter matches its neighbours; the others are generated from their Noto
//! face, as in M002.
const std = @import("std");

pub const Language = struct {
    /// The overlay folder, `Localization/game/<code>`.
    code: []const u8,
    /// The mod folder below `mods/`.
    mod: []const u8,
    /// The name the language option lists (`MODName`).
    name: []const u8,
    /// The face below `vendor/fonts` the mod's fonts are generated from.
    face: []const u8,
    /// Hindi only: the script the clusters are shaped with.
    shaped: bool = false,
    /// The mod's fonts are the game's own with the missing characters added from the face
    /// (bk-font --base); otherwise they are generated from the face alone, as in M002. The Latin
    /// languages keep the game's look; the others keep their M002 fonts until the font engine
    /// milestone replaces the bitmap fonts altogether.
    keeps_game_fonts: bool = false,
};

pub const languages = [_]Language{
    .{ .code = "ru", .mod = "lang-ru", .name = "Русский", .face = "NotoSans-Regular.ttf" },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .name = "简体中文", .face = "NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .name = "日本語", .face = "NotoSansJP-Regular.otf" },
    .{ .code = "hi", .mod = "lang-hi", .name = "हिन्दी", .face = "NotoSansDevanagari-Regular.ttf", .shaped = true },
    .{ .code = "es", .mod = "lang-es", .name = "Español", .face = "LiberationSerif-Regular.ttf", .keeps_game_fonts = true },
    .{ .code = "fr", .mod = "lang-fr", .name = "Français", .face = "LiberationSerif-Regular.ttf", .keeps_game_fonts = true },
    .{ .code = "de", .mod = "lang-de", .name = "Deutsch", .face = "LiberationSerif-Regular.ttf", .keeps_game_fonts = true },
    .{ .code = "pt-BR", .mod = "lang-pt-br", .name = "Português (Brasil)", .face = "LiberationSerif-Regular.ttf", .keeps_game_fonts = true },
    .{ .code = "id", .mod = "lang-id", .name = "Bahasa Indonesia", .face = "LiberationSerif-Regular.ttf", .keeps_game_fonts = true },
};
