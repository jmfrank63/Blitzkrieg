//! Which code points an sfnt font (TrueType or CFF-flavoured OpenType) has a glyph for, read from
//! its `cmap` table, so the l10n-check glyph rule can tell a translation that would draw .notdef
//! boxes from one that will not (M002 D063, D068). Every offset is bounds-checked: a truncated or
//! malformed font is an error, never a read past the buffer.
const std = @import("std");

pub const Error = error{ Truncated, NoCmap, NoUnicodeSubtable, BadFormat };

pub const Cmap = struct {
    bytes: []const u8,
    /// Offset of the chosen Unicode subtable.
    sub: usize,
    format: u16,

    /// Reads the table directory and picks a Unicode subtable: platform 3 encoding 10 (format 12)
    /// first, else platform 3 encoding 1 or platform 0 (format 4 or 12).
    pub fn init(bytes: []const u8) Error!Cmap {
        const num_tables = try u16At(bytes, 4);
        var table_offset: ?usize = null;
        var i: usize = 0;
        while (i < num_tables) : (i += 1) {
            const rec = 12 + i * 16;
            if (rec + 16 > bytes.len) return error.Truncated;
            if (std.mem.eql(u8, bytes[rec..][0..4], "cmap")) {
                table_offset = try u32At(bytes, rec + 8);
                break;
            }
        }
        const cmap = table_offset orelse return error.NoCmap;
        const count = try u16At(bytes, cmap + 2);
        var best: ?usize = null;
        var best_rank: u8 = 0;
        i = 0;
        while (i < count) : (i += 1) {
            const rec = cmap + 4 + i * 8;
            const platform = try u16At(bytes, rec);
            const encoding = try u16At(bytes, rec + 2);
            const sub = cmap + try u32At(bytes, rec + 4);
            const format = try u16At(bytes, sub);
            if (format != 4 and format != 12) continue;
            const rank: u8 = if (platform == 3 and encoding == 10 and format == 12) 3 else if (platform == 3 and encoding == 1 and format == 4) 2 else if (platform == 0) 1 else 0;
            if (rank > best_rank) {
                best_rank = rank;
                best = sub;
            }
        }
        const sub = best orelse return error.NoUnicodeSubtable;
        return .{ .bytes = bytes, .sub = sub, .format = try u16At(bytes, sub) };
    }

    /// Whether the font maps `code_point` to a glyph other than .notdef.
    pub fn covers(self: Cmap, code_point: u21) Error!bool {
        return switch (self.format) {
            4 => self.coversFormat4(code_point),
            12 => self.coversFormat12(code_point),
            else => error.BadFormat,
        };
    }

    fn coversFormat12(self: Cmap, code_point: u21) Error!bool {
        const groups = try u32At(self.bytes, self.sub + 12);
        var low: usize = 0;
        var high: usize = groups;
        while (low < high) {
            const mid = low + (high - low) / 2;
            const at = self.sub + 16 + mid * 12;
            const start = try u32At(self.bytes, at);
            const end = try u32At(self.bytes, at + 4);
            if (code_point < start) {
                high = mid;
            } else if (code_point > end) {
                low = mid + 1;
            } else {
                const glyph = (try u32At(self.bytes, at + 8)) + (code_point - start);
                return glyph != 0;
            }
        }
        return false;
    }

    fn coversFormat4(self: Cmap, code_point: u21) Error!bool {
        if (code_point > 0xFFFF) return false;
        const seg_count = (try u16At(self.bytes, self.sub + 6)) / 2;
        const ends = self.sub + 14;
        const starts = ends + @as(usize, seg_count) * 2 + 2;
        const deltas = starts + @as(usize, seg_count) * 2;
        const offsets = deltas + @as(usize, seg_count) * 2;
        var seg: usize = 0;
        while (seg < seg_count) : (seg += 1) {
            const end = try u16At(self.bytes, ends + seg * 2);
            if (code_point > end) continue;
            const start = try u16At(self.bytes, starts + seg * 2);
            if (code_point < start) return false;
            const delta = try u16At(self.bytes, deltas + seg * 2);
            const range_offset = try u16At(self.bytes, offsets + seg * 2);
            if (range_offset == 0) return (@as(u16, @intCast(code_point)) +% delta) != 0;
            const glyph_at = offsets + seg * 2 + range_offset + (code_point - start) * 2;
            const glyph = try u16At(self.bytes, glyph_at);
            return glyph != 0;
        }
        return false;
    }
};

fn u16At(bytes: []const u8, at: usize) Error!u16 {
    if (at + 2 > bytes.len) return error.Truncated;
    return std.mem.readInt(u16, bytes[at..][0..2], .big);
}

fn u32At(bytes: []const u8, at: usize) Error!u32 {
    if (at + 4 > bytes.len) return error.Truncated;
    return std.mem.readInt(u32, bytes[at..][0..4], .big);
}

fn loadFont(name: []const u8) ![]u8 {
    const path = try std.fmt.allocPrint(std.testing.allocator, "vendor/fonts/{s}", .{name});
    defer std.testing.allocator.free(path);
    return std.Io.Dir.cwd().readFileAlloc(std.testing.io, path, std.testing.allocator, .limited(32 << 20));
}

test "the vendored fonts cover the scripts they are named for" {
    const testing = std.testing;
    const sc = try loadFont("NotoSansSC-Regular.otf");
    defer testing.allocator.free(sc);
    const jp = try loadFont("NotoSansJP-Regular.otf");
    defer testing.allocator.free(jp);
    const deva = try loadFont("NotoSansDevanagari-Regular.ttf");
    defer testing.allocator.free(deva);
    const latin = try loadFont("NotoSans-Regular.ttf");
    defer testing.allocator.free(latin);

    try testing.expect(try (try Cmap.init(sc)).covers(0x76F4));
    try testing.expect(try (try Cmap.init(jp)).covers(0x76F4));
    const hindi = try Cmap.init(deva);
    try testing.expect(try hindi.covers(0x0915));
    try testing.expect(try hindi.covers(0x094D));
    try testing.expect(try hindi.covers(0x093F));
    const noto = try Cmap.init(latin);
    try testing.expect(!try noto.covers(0x76F4));
    try testing.expect(try noto.covers(0x0416));
    try testing.expect(try noto.covers('A'));
}

test "a truncated or garbage font is an error, not a crash" {
    const testing = std.testing;
    const latin = try loadFont("NotoSans-Regular.ttf");
    defer testing.allocator.free(latin);
    // Cut inside the table directory, and in the middle of the file (the cmap table lies beyond).
    try testing.expectError(error.Truncated, Cmap.init(latin[0..20]));
    try testing.expectError(error.Truncated, Cmap.init(latin[0..200]));
    try testing.expectError(error.Truncated, Cmap.init(&.{ 0, 1, 0, 0 }));
    var junk: [64]u8 = @splat(0xff);
    try testing.expectError(error.Truncated, Cmap.init(&junk));
}
