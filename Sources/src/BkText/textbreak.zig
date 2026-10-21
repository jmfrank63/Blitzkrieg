//! Line break opportunities for text without spaces: Sources/src/GFX/TextBreak.h ported name for
//! name with the same tables and ranges (D065/D100/D104), so bk_text wraps where the game wraps.
//! Private-use code points are never break opportunities, as in C++ (the Hindi cluster glyphs);
//! S05 may drop that rule once the PUA table is gone.

/// IsTextBreakHan
pub fn isHan(c: u16) bool {
    return (c >= 0x3400 and c <= 0x4DBF) or (c >= 0x4E00 and c <= 0x9FFF) or (c >= 0xF900 and c <= 0xFAFF);
}

/// IsTextBreakKana
pub fn isKana(c: u16) bool {
    return c >= 0x3040 and c <= 0x30FF;
}

/// IsTextBreakCjkPunct
pub fn isCjkPunct(c: u16) bool {
    return (c >= 0x3000 and c <= 0x303F) or (c >= 0xFF00 and c <= 0xFFEF);
}

/// IsTextBreakCjk
pub fn isCjk(c: u16) bool {
    return isHan(c) or isKana(c) or isCjkPunct(c);
}

/// IsTextBreakNoStart: characters that must not start a line: closing brackets, stops, the
/// prolonged sound mark, iteration marks, the two-dot and three-dot leaders, the katakana double
/// hyphen, the fullwidth wave dash and the small kana.
pub fn isNoStart(c: u16) bool {
    switch (c) {
        0x3001, 0x3002, 0x3005, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011, 0x3015 => return true,
        0x301C, 0x30FB, 0x30FC, 0x30FD, 0x30FE, 0x309B, 0x309C, 0x309D, 0x309E => return true,
        0x2025, 0x2026, 0x30A0, 0xFF5E => return true,
        0xFF01, 0xFF09, 0xFF0C, 0xFF0E, 0xFF1A, 0xFF1B, 0xFF1F, 0xFF3D, 0xFF5D => return true,
        else => {},
    }
    // Small hiragana and katakana: the a/i/u/e/o row, tsu, ya/yu/yo, wa, ka, ke.
    if (c >= 0x3041 and c <= 0x3049) return c & 1 != 0;
    if (c == 0x3063 or c == 0x3083 or c == 0x3085 or c == 0x3087 or c == 0x308E) return true;
    if (c >= 0x30A1 and c <= 0x30A9) return c & 1 != 0;
    return c == 0x30C3 or c == 0x30E3 or c == 0x30E5 or c == 0x30E7 or c == 0x30EE or c == 0x30F5 or c == 0x30F6;
}

/// IsTextBreakNoEnd: characters that must not end a line: opening brackets.
pub fn isNoEnd(c: u16) bool {
    return switch (c) {
        0x3008, 0x300A, 0x300C, 0x300E, 0x3010, 0x3014, 0xFF08, 0xFF3B, 0xFF5B => true,
        else => false,
    };
}

/// CanBreakTextBetween: whether the line may break between `previous` and `next`, with no space
/// there.
pub fn canBreakTextBetween(previous: u16, next: u16) bool {
    if ((previous >= 0xE000 and previous <= 0xF8FF) or (next >= 0xE000 and next <= 0xF8FF)) return false;
    if (!isCjk(previous) and !isCjk(next)) return false;
    return !isNoStart(next) and !isNoEnd(previous);
}

// ---- tests ----

const std = @import("std");

test "ranges and tables match TextBreak.h" {
    try std.testing.expect(isHan(0x4E2D) and isHan(0x3400) and isHan(0xFAFF) and !isHan(0x3399) and !isHan(0xFB00));
    try std.testing.expect(isKana(0x3042) and isKana(0x30FF) and !isKana(0x3100));
    try std.testing.expect(isCjkPunct(0x3002) and isCjkPunct(0xFF0C) and isCjkPunct(0xFFEF) and !isCjkPunct(0xFFF0));
    // Small a/i/u/e/o are odd code points of the row; the full-size ones are even.
    try std.testing.expect(isNoStart(0x3041) and !isNoStart(0x3042) and isNoStart(0x30A3) and !isNoStart(0x30A4));
    try std.testing.expect(isNoStart(0x3063) and !isNoStart(0x3064) and isNoStart(0x30FC) and isNoStart(0x2026));
    try std.testing.expect(isNoEnd(0x300C) and isNoEnd(0xFF08) and !isNoEnd(0x300D));
}

test "CanBreakTextBetween" {
    // Han against Han, Han against Latin and kana against Latin may break.
    try std.testing.expect(canBreakTextBetween(0x4E2D, 0x6587));
    try std.testing.expect(canBreakTextBetween('A', 0x4E2D));
    try std.testing.expect(canBreakTextBetween(0x3042, 'A'));
    // Latin against Latin never.
    try std.testing.expect(!canBreakTextBetween('A', 'B'));
    // Kinsoku: no line start on a stop or small kana, no line end on an opening bracket.
    try std.testing.expect(!canBreakTextBetween(0x4E2D, 0x3002));
    try std.testing.expect(!canBreakTextBetween(0x3042, 0x3083));
    try std.testing.expect(!canBreakTextBetween(0x300C, 0x4E2D));
    // Private-use is never a break.
    try std.testing.expect(!canBreakTextBetween(0xE010, 0x4E2D));
    try std.testing.expect(!canBreakTextBetween(0x4E2D, 0xF8FF));
}
