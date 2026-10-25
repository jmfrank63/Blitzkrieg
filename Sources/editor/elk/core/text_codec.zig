//! The ELK text files' codec (D-06): `.elk` originals, `.txt` translations
//! and `.dsc` descriptions are UTF-16LE with an `FF FE` byte-order mark and
//! CRLF line ends on disk, while the editor works in UTF-8 with LF. `decode`
//! and `encode` are ports of `CELK::ToWideText` and `CELK::FromWideText`
//! (Sources/src/ELK/ELK_StaticMethods.cpp), so a text the MFC ELK wrote reads
//! and writes back to the same bytes. The editor side uses WTF-8, not UTF-8,
//! because a translator's text can hold a lone surrogate and it still has to
//! survive an untouched round trip unchanged.
const std = @import("std");

pub const bom_le = [2]u8{ 0xFF, 0xFE };
const bom_be = [2]u8{ 0xFE, 0xFF };

/// UTF-16 code units of a text file, trailing CR/LF/NUL removed like
/// `ToWideText`; with `remove_cr` every CR goes, not only those before an LF,
/// as the MFC edit buffer did. The MFC ELK asserted on a missing BOM and read
/// nothing; this reads such a file as UTF-16LE, and a big-endian BOM as
/// UTF-16BE, so a file a foreign tool saved still opens. A trailing odd byte
/// is dropped, as the MFC integer division did.
pub fn decode(allocator: std.mem.Allocator, bytes: []const u8, remove_cr: bool) ![]u16 {
    var start: usize = 0;
    var endian: std.builtin.Endian = .little;
    if (bytes.len >= 2 and std.mem.eql(u8, bytes[0..2], &bom_le)) {
        start = 2;
    } else if (bytes.len >= 2 and std.mem.eql(u8, bytes[0..2], &bom_be)) {
        start = 2;
        endian = .big;
    }
    const count = (bytes.len - start) / 2;
    var units: std.ArrayListUnmanaged(u16) = .empty;
    errdefer units.deinit(allocator);
    try units.ensureTotalCapacity(allocator, count);
    for (0..count) |index| {
        const unit = std.mem.readInt(u16, bytes[start + index * 2 ..][0..2], endian);
        if (remove_cr and unit == '\r') continue;
        units.appendAssumeCapacity(unit);
    }
    units.shrinkRetainingCapacity(trailingRunStart(units.items));
    return units.toOwnedSlice(allocator);
}

/// The file bytes for `units`, like `FromWideText`: with `add_cr` a CR goes
/// before every LF that has none, the trailing CR/LF/NUL run is dropped, and
/// the result always starts with `FF FE`, so an empty text is the bare BOM.
pub fn encode(allocator: std.mem.Allocator, units: []const u16, add_cr: bool) ![]u8 {
    const end = trailingRunStart(units);
    var out: std.ArrayListUnmanaged(u8) = .empty;
    errdefer out.deinit(allocator);
    try out.ensureTotalCapacity(allocator, 2 + end * 2);
    try out.appendSlice(allocator, &bom_le);
    for (units[0..end], 0..) |unit, index| {
        if (add_cr and unit == '\n' and (index == 0 or units[index - 1] != '\r')) try appendUnit(allocator, &out, '\r');
        try appendUnit(allocator, &out, unit);
    }
    return out.toOwnedSlice(allocator);
}

/// A text file's bytes as the editor shows them: WTF-8 with LF line ends.
pub fn toEditor(allocator: std.mem.Allocator, bytes: []const u8) ![]u8 {
    const units = try decode(allocator, bytes, true);
    defer allocator.free(units);
    return std.unicode.wtf16LeToWtf8Alloc(allocator, units);
}

/// The file bytes for an editor text (WTF-8 with LF line ends).
pub fn fromEditor(allocator: std.mem.Allocator, text: []const u8) ![]u8 {
    const units = try std.unicode.wtf8ToWtf16LeAlloc(allocator, text);
    defer allocator.free(units);
    return encode(allocator, units, true);
}

/// Where the trailing run of CR, LF and NUL units begins: the length the
/// MFC ELK kept.
fn trailingRunStart(units: []const u16) usize {
    var end = units.len;
    while (end > 0) : (end -= 1) {
        const unit = units[end - 1];
        if (unit != '\r' and unit != '\n' and unit != 0) break;
    }
    return end;
}

fn appendUnit(allocator: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), unit: u16) !void {
    var pair: [2]u8 = undefined;
    std.mem.writeInt(u16, &pair, unit, .little);
    try out.appendSlice(allocator, &pair);
}

const testing = std.testing;

fn le(comptime text: []const u8) []const u8 {
    comptime var out: []const u8 = &bom_le;
    inline for (text) |byte| out = out ++ [_]u8{ byte, 0 };
    return out;
}

test "decode strips the trailing CR, LF and NUL run like ToWideText" {
    const units = try decode(testing.allocator, le("ab\r\ncd\r\n\x00\r"), false);
    defer testing.allocator.free(units);
    try testing.expectEqualSlices(u16, &[_]u16{ 'a', 'b', '\r', '\n', 'c', 'd' }, units);

    const only_run = try decode(testing.allocator, le("\r\n\x00"), false);
    defer testing.allocator.free(only_run);
    try testing.expectEqual(@as(usize, 0), only_run.len);
}

test "the editor sees LF only: every CR is removed, as with bRemove_0D" {
    const text = try toEditor(testing.allocator, le("one\r\ntwo\rthree\r\n"));
    defer testing.allocator.free(text);
    try testing.expectEqualStrings("one\ntwothree", text);
}

test "encode writes FF FE, CRLF and no trailing run like FromWideText" {
    const bytes = try fromEditor(testing.allocator, "one\ntwo\r\nthree\n\n");
    defer testing.allocator.free(bytes);
    try testing.expectEqualSlices(u8, le("one\r\ntwo\r\nthree"), bytes);

    const empty = try fromEditor(testing.allocator, "\n\n");
    defer testing.allocator.free(empty);
    try testing.expectEqualSlices(u8, &bom_le, empty);
}

test "a missing BOM reads as UTF-16LE and a big-endian BOM as UTF-16BE" {
    const bare = le("Tank")[2..];
    const from_bare = try toEditor(testing.allocator, bare);
    defer testing.allocator.free(from_bare);
    try testing.expectEqualStrings("Tank", from_bare);

    const big = [_]u8{ 0xFE, 0xFF, 0x04, 0x22, 0x00, 'a', 0x00, '\r', 0x00, '\n' };
    const from_big = try toEditor(testing.allocator, &big);
    defer testing.allocator.free(from_big);
    try testing.expectEqualStrings("\u{0422}a", from_big);

    // Writes are always little-endian with FF FE, whatever was read.
    const written = try fromEditor(testing.allocator, from_big);
    defer testing.allocator.free(written);
    try testing.expectEqualSlices(u8, &[_]u8{ 0xFF, 0xFE, 0x22, 0x04, 'a', 0x00 }, written);
}

test "a trailing odd byte is dropped like the MFC integer division" {
    const text = try toEditor(testing.allocator, comptime le("ab") ++ "\x41");
    defer testing.allocator.free(text);
    try testing.expectEqualStrings("ab", text);
}

test "a lone surrogate survives the editor round trip byte for byte" {
    const bytes = [_]u8{ 0xFF, 0xFE, 'x', 0x00, 0x00, 0xD8, 'y', 0x00 };
    const text = try toEditor(testing.allocator, &bytes);
    defer testing.allocator.free(text);
    const back = try fromEditor(testing.allocator, text);
    defer testing.allocator.free(back);
    try testing.expectEqualSlices(u8, &bytes, back);
}

test "every tracked text without a trailing run encodes back to its own bytes" {
    // D-06 against the real databases: decoding a tracked .elk/.txt into the
    // editor form and encoding it again gives the same bytes, except where
    // the file ends in a CR/LF/NUL run, which FromWideText never writes (the
    // originals are copied verbatim from updates, so 1,361 of them have one).
    const io = testing.io;
    const roots = [_][]const u8{ "Data/ELK/game_data_base", "Data/AmericanELK/game_data_base" };
    var checked: usize = 0;
    var with_run: usize = 0;
    for (roots) |root| {
        var dir = try std.Io.Dir.cwd().openDir(io, root, .{ .iterate = true });
        defer dir.close(io);
        var walker = try dir.walk(testing.allocator);
        defer walker.deinit();
        while (try walker.next(io)) |entry| {
            if (entry.kind != .file) continue;
            if (!std.mem.endsWith(u8, entry.basename, ".elk") and !std.mem.endsWith(u8, entry.basename, ".txt")) continue;
            const bytes = try dir.readFileAlloc(io, entry.path, testing.allocator, .limited(1 << 20));
            defer testing.allocator.free(bytes);
            const text = try toEditor(testing.allocator, bytes);
            defer testing.allocator.free(text);
            const back = try fromEditor(testing.allocator, text);
            defer testing.allocator.free(back);
            checked += 1;
            if (std.mem.eql(u8, bytes, back)) continue;
            // Only a trailing run (or the one odd byte count) may differ.
            const units = try decode(testing.allocator, bytes, false);
            defer testing.allocator.free(units);
            const raw = (bytes.len - 2) / 2;
            try testing.expect(units.len < raw or bytes.len % 2 == 1);
            with_run += 1;
        }
    }
    try testing.expect(checked > 11_000);
    try testing.expectEqual(@as(usize, 1_366), with_run);
}
