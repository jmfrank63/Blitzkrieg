//! The metrics of a game font, read from its `1.tfd` for the fit rule of l10n-check (M003 D097).
//! A `.tfd` is the engine's structure saver format (StructureSaver.cpp): chunks of a one-byte id
//! and a length, doubled, in one byte or, when odd, four. The file's chunk 1 holds the object saved
//! as chunk 1, SFontFormat (Sources/src/Formats/fmtFont.cpp): chunk 2 the metrics, chunk 3 the
//! characters and chunk 4 the kerning pairs, each map as the n-th key chunk 1 paired with the n-th
//! value chunk 2. Only what text layout needs is kept; the texture rectangles are not.
const std = @import("std");

pub const Char = struct { a: f32, b: f32, c: f32, width: f32 };

pub const Font = struct {
    height: i32,
    external_leading: i32,
    space_width: f32,
    default_char: u16,
    chars: std.AutoHashMapUnmanaged(u16, Char),
    /// Kerning pairs keyed `(previous << 16) | current`, as SFontFormat::GetKern looks them up.
    kerns: std.AutoHashMapUnmanaged(u32, f32),

    /// SFontFormat::GetLineSpace: what one line of text costs in height.
    pub fn lineSpace(self: Font) i32 {
        return self.height + self.external_leading;
    }

    /// SFontFormat::GetChar: the character, else the default character, else null (a font without
    /// either cannot measure that text).
    pub fn char(self: Font, c: u16) ?Char {
        return self.chars.get(c) orelse self.chars.get(self.default_char);
    }

    pub fn kern(self: Font, previous: u16, current: u16) f32 {
        return self.kerns.get((@as(u32, previous) << 16) | current) orelse 0;
    }

    /// FontGpuWrapMetrics::Advance: A + kern + B + C of `current` after `previous`, added in that
    /// order because float addition is not associative and the engine's wrap sums them so.
    pub fn advance(self: Font, previous: u16, current: u16) f32 {
        const desc = self.char(current) orelse return self.space_width;
        return desc.a + self.kern(previous, current) + desc.b + desc.c;
    }

    /// FontGpu::TextWidth: the width of a run, which stops at a line break.
    pub fn runWidth(self: Font, text: []const u16) f32 {
        var width: f32 = 0;
        var previous: u16 = 0;
        for (text) |c| {
            if (c == 0 or c == '\n' or c == '\r') break;
            width += self.advance(previous, c);
            previous = c;
        }
        return width;
    }

    pub fn read(arena: std.mem.Allocator, tfd: []const u8) !Font {
        const format = try child(try child(tfd, 1, 0), 1, 0);
        const metrics = try child(format, 2, 0);
        var font: Font = .{
            .height = try int(i32, try child(metrics, 1, 0)),
            .external_leading = try int(i32, try child(metrics, 5, 0)),
            .space_width = @bitCast(try int(u32, try child(metrics, 8, 0))),
            .default_char = try int(u16, try child(metrics, 10, 0)),
            .chars = .empty,
            .kerns = .empty,
        };
        const chars_chunk = try child(format, 3, 0);
        var index: usize = 0;
        while (child(chars_chunk, 1, index)) |key| : (index += 1) {
            const desc = try child(chars_chunk, 2, index);
            var fields: [4]f32 = undefined;
            for (&fields, 5..) |*field, id| field.* = @bitCast(try int(u32, try child(desc, @intCast(id), 0)));
            try font.chars.put(arena, try int(u16, key), .{ .a = fields[0], .b = fields[1], .c = fields[2], .width = fields[3] });
        } else |err| if (err != error.ChunkMissing) return err;
        if (font.chars.count() == 0) return error.NoCharacters;

        // A font without kerning pairs has no chunk 4.
        if (child(format, 4, 0)) |kerns_chunk| {
            index = 0;
            while (child(kerns_chunk, 1, index)) |key| : (index += 1) {
                const value = try child(kerns_chunk, 2, index);
                try font.kerns.put(arena, try int(u32, key), @bitCast(try int(u32, value)));
            } else |err| if (err != error.ChunkMissing) return err;
        } else |err| if (err != error.ChunkMissing) return err;
        return font;
    }
};

/// The n-th chunk `id` directly inside `data`.
fn child(data: []const u8, id: u8, n: usize) ![]const u8 {
    var at: usize = 0;
    var seen: usize = 0;
    while (at + 2 <= data.len) {
        const chunk_id = data[at];
        var length: usize = data[at + 1];
        at += 2;
        if (length & 1 != 0) {
            if (at + 3 > data.len) return error.BadChunk;
            length |= @as(usize, std.mem.readInt(u24, data[at..][0..3], .little)) << 8;
            at += 3;
        }
        length >>= 1;
        if (at + length > data.len) return error.BadChunk;
        if (chunk_id == id) {
            if (seen == n) return data[at .. at + length];
            seen += 1;
        }
        at += length;
    }
    return error.ChunkMissing;
}

fn int(comptime T: type, data: []const u8) !T {
    if (data.len != @sizeOf(T)) return error.BadChunk;
    return std.mem.readInt(T, data[0..@sizeOf(T)], .little);
}

fn load(arena: std.mem.Allocator, size: []const u8) !Font {
    var path_buffer: [64]u8 = undefined;
    const path = try std.fmt.bufPrint(&path_buffer, "Data/Fonts/{s}/1.tfd", .{size});
    const bytes = try std.Io.Dir.cwd().readFileAlloc(std.testing.io, path, arena, .limited(1 << 20));
    return Font.read(arena, bytes);
}

test "the shipped fonts read: height, line space, space width and counts" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const want = [_]struct { size: []const u8, height: i32, space: f32, kerns: usize }{
        .{ .size = "small", .height = 16, .space = 4, .kerns = 319 },
        .{ .size = "medium", .height = 20, .space = 5, .kerns = 324 },
        .{ .size = "large", .height = 40, .space = 9, .kerns = 461 },
    };
    for (want) |one| {
        const font = try load(arena, one.size);
        try std.testing.expectEqual(one.height, font.height);
        try std.testing.expectEqual(one.height + 1, font.lineSpace());
        try std.testing.expectEqual(one.space, font.space_width);
        try std.testing.expectEqual(@as(usize, 225), font.chars.count());
        try std.testing.expectEqual(one.kerns, font.kerns.count());
        try std.testing.expectEqual(@as(u16, 31), font.default_char);
        // A space advances by A + B + C, which is the font's space width.
        try std.testing.expectEqual(one.space, font.advance(0, ' '));
    }
}

test "advance sums A, kern, B and C; an unknown character falls back to the default" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const font = try load(arena_state.allocator(), "small");
    // 'A' in the small font: A 0, B 10, C -1, so it advances 9 after no character.
    try std.testing.expectEqual(@as(f32, 9), font.advance(0, 'A'));
    const medium = try load(arena_state.allocator(), "medium");
    try std.testing.expectEqual(@as(f32, 12), medium.advance(0, 'A'));
    // U+4E00 (a Han character) is not among the 225 characters of a Latin font.
    try std.testing.expect(font.chars.get(0x4E00) == null);
    try std.testing.expectEqual(font.advance(0, font.default_char), font.advance(0, 0x4E00));
    // A kerning pair adds to the advance of its second character.
    var it = font.kerns.iterator();
    const pair = it.next().?;
    const previous: u16 = @intCast(pair.key_ptr.* >> 16);
    const current: u16 = @intCast(pair.key_ptr.* & 0xffff);
    try std.testing.expectEqual(pair.value_ptr.*, font.kern(previous, current));
    try std.testing.expect(font.advance(previous, current) != font.advance(0, current) or pair.value_ptr.* == 0);
}

test "runWidth stops at a line break" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const font = try load(arena_state.allocator(), "small");
    const one = [_]u16{ 'A', 'A' };
    const broken = [_]u16{ 'A', 'A', '\n', 'A' };
    try std.testing.expectEqual(font.runWidth(&one), font.runWidth(&broken));
    try std.testing.expectEqual(@as(f32, 0), font.runWidth(&.{}));
}

test "read rejects a file that is not a font" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    try std.testing.expectError(error.ChunkMissing, Font.read(arena_state.allocator(), "\x01\x00"));
}

test "the shipped fonts carry the punctuation French typography uses" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    // The shipped 1.tfd files hold the Cyrillic set (225 characters), not the cp1252 accented
    // letters: a Latin overlay's accents are drawn by the font the language mods build (the glyph
    // rule of l10n-check asks for cp1252). What French needs besides letters is here: the
    // no-break space before `: ; ! ?`, the guillemets, the typographic apostrophe, the ellipsis
    // and the en dash. A missing glyph would fall back to the default character without a word.
    const needed = [_]u16{ 0x00A0, 0x00AB, 0x00BB, 0x2019, 0x2026, 0x2013 };
    for ([_][]const u8{ "small", "medium", "large" }) |size| {
        const font = try load(arena_state.allocator(), size);
        for (needed) |c| {
            std.testing.expect(font.chars.contains(c)) catch |err| {
                std.debug.print("{s} font has no glyph for U+{X:0>4}\n", .{ size, c });
                return err;
            };
        }
    }
}

test "the shipped fonts carry the punctuation Spanish typography uses" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    // Like the accented letters, the inverted marks (U+00BF, U+00A1) and n with tilde (U+00F1, U+00D1)
    // are not in the Cyrillic 1.tfd files: all four are cp1252 and are drawn by the font the language
    // mods build, and l10n-game-reads-it-es measures that no glyph is missing. What the shipped fonts
    // do carry is what Spanish prose uses besides letters: the guillemets for quotations, the
    // typographic apostrophe, the ellipsis and the dashes.
    const needed = [_]u16{ 0x00AB, 0x00BB, 0x2019, 0x2026, 0x2013, 0x2014 };
    for ([_][]const u8{ "small", "medium", "large" }) |size| {
        const font = try load(arena_state.allocator(), size);
        for (needed) |c| {
            std.testing.expect(font.chars.contains(c)) catch |err| {
                std.debug.print("{s} font has no glyph for U+{X:0>4}\n", .{ size, c });
                return err;
            };
        }
    }
}
