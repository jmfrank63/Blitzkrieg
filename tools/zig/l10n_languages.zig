//! The nine languages of M002 (D060, D068) and the cp1252 rule of the Latin ones, shared by the
//! language mods build (tools/zig/language_mods.zig) and the completeness gate
//! (tools/zig/l10n_check.zig) so both read one table.
const std = @import("std");

pub const Language = struct {
    /// The overlay folder, `Localization/game/<code>`.
    code: []const u8,
    /// The mod folder below `mods/`.
    mod: []const u8,
    /// The name the language option lists (`MODName`).
    name: []const u8,
    /// The face below `vendor/fonts`; none for a language that draws with the
    /// game's bitmap fonts and so ships texts only.
    face: ?[]const u8 = null,
    /// Hindi only: the script the clusters are shaped with.
    shaped: bool = false,
};

pub const languages = [_]Language{
    .{ .code = "ru", .mod = "lang-ru", .name = "Русский", .face = "NotoSans-Regular.ttf" },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .name = "简体中文", .face = "NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .name = "日本語", .face = "NotoSansJP-Regular.otf" },
    .{ .code = "hi", .mod = "lang-hi", .name = "हिन्दी", .face = "NotoSansDevanagari-Regular.ttf", .shaped = true },
    .{ .code = "es", .mod = "lang-es", .name = "Español" },
    .{ .code = "fr", .mod = "lang-fr", .name = "Français" },
    .{ .code = "de", .mod = "lang-de", .name = "Deutsch" },
    .{ .code = "pt-BR", .mod = "lang-pt-br", .name = "Português (Brasil)" },
    .{ .code = "id", .mod = "lang-id", .name = "Bahasa Indonesia" },
};

/// The Windows-1252 code point of a UTF-16 unit is defined when it is below
/// 0x80, in 0xA0..0xFF, or one of the 27 characters cp1252 puts in 0x80..0x9F.
pub fn inCp1252(unit: u16) bool {
    if (unit < 0x80 or (unit >= 0xA0 and unit <= 0xFF)) return true;
    return switch (unit) {
        0x20AC, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x017D => true,
        0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x017E, 0x0178 => true,
        else => false,
    };
}

/// The first code unit of an exported text (UTF-16LE after its byte order
/// mark) that cp1252 cannot draw, or null.
pub fn cp1252Violation(bytes: []const u8) ?u16 {
    var at: usize = 2;
    while (at + 1 < bytes.len) : (at += 2) {
        const unit = std.mem.readInt(u16, bytes[at..][0..2], .little);
        if (!inCp1252(unit)) return unit;
    }
    return null;
}
