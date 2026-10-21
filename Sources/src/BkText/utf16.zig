//! UTF-16 to code points. The game's text is UTF-16 (D104), so surrogate pairs
//! must survive; a lone surrogate is U+FFFD of one unit, never an error.
const std = @import("std");

pub const replacement: u21 = 0xFFFD;

pub const Item = struct {
    cp: u21,
    /// Code units consumed: 1 or 2.
    len: u2,
};

pub const Iterator = struct {
    units: []const u16,
    index: usize = 0,

    pub fn next(self: *Iterator) ?Item {
        if (self.index >= self.units.len) return null;
        const hi = self.units[self.index];
        if (hi >= 0xD800 and hi <= 0xDBFF) {
            if (self.index + 1 < self.units.len) {
                const lo = self.units[self.index + 1];
                if (lo >= 0xDC00 and lo <= 0xDFFF) {
                    self.index += 2;
                    return .{ .cp = 0x10000 + ((@as(u21, hi) - 0xD800) << 10) + (lo - 0xDC00), .len = 2 };
                }
            }
            self.index += 1;
            return .{ .cp = replacement, .len = 1 };
        }
        self.index += 1;
        if (hi >= 0xDC00 and hi <= 0xDFFF) return .{ .cp = replacement, .len = 1 };
        return .{ .cp = hi, .len = 1 };
    }
};

pub fn iterate(units: []const u16) Iterator {
    return .{ .units = units };
}

test "BMP text yields one code point per unit" {
    var it = iterate(&.{ 'a', 0x00E9, 0x4E2D });
    try std.testing.expectEqual(@as(u21, 'a'), it.next().?.cp);
    try std.testing.expectEqual(@as(u21, 0xE9), it.next().?.cp);
    try std.testing.expectEqual(@as(u21, 0x4E2D), it.next().?.cp);
    try std.testing.expect(it.next() == null);
}

test "a surrogate pair is one code point of two units" {
    // U+20BB7 is the Han character 𠮷.
    var it = iterate(&.{ 0xD842, 0xDFB7, 'x' });
    const first = it.next().?;
    try std.testing.expectEqual(@as(u21, 0x20BB7), first.cp);
    try std.testing.expectEqual(@as(u2, 2), first.len);
    try std.testing.expectEqual(@as(u21, 'x'), it.next().?.cp);
    try std.testing.expect(it.next() == null);
}

test "a lone high surrogate is U+FFFD of one unit, also at the end" {
    var it = iterate(&.{ 0xD842, 'x', 0xD842 });
    const a = it.next().?;
    try std.testing.expectEqual(replacement, a.cp);
    try std.testing.expectEqual(@as(u2, 1), a.len);
    try std.testing.expectEqual(@as(u21, 'x'), it.next().?.cp);
    const c = it.next().?;
    try std.testing.expectEqual(replacement, c.cp);
    try std.testing.expectEqual(@as(u2, 1), c.len);
    try std.testing.expect(it.next() == null);
}

test "a lone low surrogate is U+FFFD of one unit" {
    var it = iterate(&.{ 0xDFB7, 0xD842, 0xD842, 0xDFB7 });
    try std.testing.expectEqual(replacement, it.next().?.cp);
    try std.testing.expectEqual(replacement, it.next().?.cp);
    const pair = it.next().?;
    try std.testing.expectEqual(@as(u21, 0x20BB7), pair.cp);
    try std.testing.expect(it.next() == null);
}

/// A comptime UTF-8 literal as a UTF-16 array, for tests and fixed strings.
pub fn literal(comptime s: []const u8) [std.unicode.calcUtf16LeLen(s) catch unreachable]u16 {
    var out: [std.unicode.calcUtf16LeLen(s) catch unreachable]u16 = undefined;
    _ = std.unicode.utf8ToUtf16Le(&out, s) catch unreachable;
    return out;
}
