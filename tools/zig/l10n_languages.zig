//! The nine languages of M002 (D060, D068), shared by the language mods build
//! (tools/zig/language_mods.zig) and the completeness gate (tools/zig/l10n_check.zig) so both read
//! one table. Every language mod ships fonts generated from its face: the game's own 1.tfd files hold
//! ASCII and the Cyrillic set, none of the accented Latin letters, so the Latin languages need them
//! as much as the others do. Theirs come from Liberation Serif, whose advance widths are those of
//! Times New Roman, the face of the game's bitmaps, so the 2003 layouts keep the widths they were
//! designed for.
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
};

pub const languages = [_]Language{
    .{ .code = "ru", .mod = "lang-ru", .name = "Русский", .face = "NotoSans-Regular.ttf" },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .name = "简体中文", .face = "NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .name = "日本語", .face = "NotoSansJP-Regular.otf" },
    .{ .code = "hi", .mod = "lang-hi", .name = "हिन्दी", .face = "NotoSansDevanagari-Regular.ttf", .shaped = true },
    .{ .code = "es", .mod = "lang-es", .name = "Español", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "fr", .mod = "lang-fr", .name = "Français", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "de", .mod = "lang-de", .name = "Deutsch", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "pt-BR", .mod = "lang-pt-br", .name = "Português (Brasil)", .face = "LiberationSerif-Regular.ttf" },
    .{ .code = "id", .mod = "lang-id", .name = "Bahasa Indonesia", .face = "LiberationSerif-Regular.ttf" },
};
