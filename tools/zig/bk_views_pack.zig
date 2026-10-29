//! The building packs' file formats, std-only and without the tools'
//! language tables, so the Resource Editor reads packs as bk-views does
//! (M005 S08): the engine's structure file, SSpritesPack, the `_h.dds` atlas,
//! the stored PNG bk-views writes, a pack unpacked onto its canvas, and the
//! variant's 1.xml as decompose and compose-pack need it.
const std = @import("std");

// ---------------------------------------------------------------------------
// The engine's structure file (StreamIO/StructureSaver2.cpp)

/// One chunk: an id byte, the length shifted left by one in one byte, or in
/// four bytes with the low bit set when the shifted length is 256 or more.
pub const Chunk = struct { id: u8, body: []const u8 };

pub fn nextChunk(bytes: []const u8, pos: *usize) ?Chunk {
    if (pos.* + 2 > bytes.len) return null;
    const id = bytes[pos.*];
    var length: u32 = bytes[pos.* + 1];
    var at = pos.* + 2;
    if (length & 1 != 0) {
        if (pos.* + 5 > bytes.len) return null;
        length = std.mem.readInt(u32, bytes[pos.* + 1 ..][0..4], .little);
        at = pos.* + 5;
    }
    length >>= 1;
    if (at + length > bytes.len) return null;
    pos.* = at + length;
    return .{ .id = id, .body = bytes[at..][0..length] };
}

/// The `nth` (from 0) chunk `id` inside `bytes`, as GetShortChunk finds it for
/// the element counter nth + 1.
pub fn findChunk(bytes: []const u8, id: u8, nth: usize) ?[]const u8 {
    var pos: usize = 0;
    var seen: usize = 0;
    while (nextChunk(bytes, &pos)) |chunk| {
        if (chunk.id != id) continue;
        if (seen == nth) return chunk.body;
        seen += 1;
    }
    return null;
}

pub fn countChunks(bytes: []const u8, id: u8) usize {
    var pos: usize = 0;
    var count: usize = 0;
    while (nextChunk(bytes, &pos)) |chunk| count += @intFromBool(chunk.id == id);
    return count;
}

/// A data chunk as DataChunk reads it: missing is zero, a wrong size is an error.
pub fn rawChunk(comptime T: type, bytes: []const u8, id: u8) !T {
    const body = findChunk(bytes, id, 0) orelse return std.mem.zeroes(T);
    if (body.len != @sizeOf(T)) return error.BadChunkSize;
    return std.mem.bytesToValue(T, body);
}

// ---------------------------------------------------------------------------
// SSpritesPack

pub const pack_signature: u32 = 0x314e4153;

/// SSpritesPack::SSprite::SSquare: 9 floats, stored raw.
pub const Square = extern struct {
    x: f32,
    y: f32,
    size: f32,
    u1: f32,
    v1: f32,
    u2: f32,
    v2: f32,
    depth_left: f32,
    depth_right: f32,
};

pub const Sprite = struct {
    squares: []Square,
    horizontal: bool,
    /// CVarArray2D<short, BYTE>: the edge crossings line by line.
    edge_size: i32,
    line_count: i32,
    edges: []i16,
    line_sizes: []u8,
    bound_box: [4]f32,
    center: [2]i32,
};

pub const Pack = struct { signature: u32, sprites: []Sprite };

/// SSpritesPack::Load: chunk 127 the signature, chunk 1 the pack, all inside
/// the file's data chunk 1.
pub fn decodePack(gpa: std.mem.Allocator, file: []const u8) !Pack {
    const data = findChunk(file, 1, 0) orelse return error.NoDataChunk;
    const signature = try rawChunk(u32, data, 127);
    if (signature != pack_signature) return error.NotASpritePack;
    const pack = findChunk(data, 1, 0) orelse return error.NoPack;
    const list = findChunk(pack, 1, 0) orelse &.{};
    const sprites = try gpa.alloc(Sprite, countChunks(list, 1));
    for (sprites, 0..) |*sprite, i| {
        const body = findChunk(list, 1, i).?;
        const squares = findChunk(body, 1, 0) orelse &.{};
        const count: usize = @intCast(@max(0, try rawChunk(i32, squares, 1)));
        sprite.squares = try gpa.alloc(Square, count);
        if (count > 0) {
            const raw = findChunk(squares, 2, 0) orelse return error.NoSquares;
            if (raw.len != count * @sizeOf(Square)) return error.BadChunkSize;
            @memcpy(std.mem.sliceAsBytes(sprite.squares), raw);
        }
        const edge = findChunk(body, 2, 0) orelse &.{};
        sprite.horizontal = try rawChunk(u8, edge, 1) != 0;
        const lines = findChunk(edge, 2, 0) orelse &.{};
        sprite.edge_size = try rawChunk(i32, lines, 1);
        sprite.line_count = try rawChunk(i32, lines, 2);
        sprite.edges = try rawArray(i16, gpa, lines, 3, sprite.edge_size);
        sprite.line_sizes = try rawArray(u8, gpa, lines, 4, sprite.line_count);
        sprite.bound_box = try rawChunk([4]f32, edge, 3);
        sprite.center = try rawChunk([2]i32, body, 3);
    }
    return .{ .signature = signature, .sprites = sprites };
}

pub fn rawArray(comptime T: type, gpa: std.mem.Allocator, bytes: []const u8, id: u8, count: i32) ![]T {
    const items = try gpa.alloc(T, @intCast(@max(0, count)));
    const body = findChunk(bytes, id, 0) orelse {
        @memset(items, 0);
        return items;
    };
    if (body.len != items.len * @sizeOf(T)) return error.BadChunkSize;
    @memcpy(std.mem.sliceAsBytes(items), body);
    return items;
}

/// The pack's fields as text, the format `bk-views-check` writes from the
/// engine's own reading: floats in the shortest form that reads back.
pub fn writePackJson(w: *std.Io.Writer, pack: Pack) !void {
    try w.print("{{\"signature\":{d},\"sprites\":[\n", .{pack.signature});
    for (pack.sprites, 0..) |sprite, i| {
        if (i > 0) try w.writeAll(",\n");
        try w.print("{{\"center\":[{d},{d}],\"horizontal\":{},\"bound_box\":", .{ sprite.center[0], sprite.center[1], sprite.horizontal });
        try writeFloats(w, &sprite.bound_box);
        try w.print(",\"edge_size\":{d},\"line_count\":{d},\n\"line_sizes\":[", .{ sprite.edge_size, sprite.line_count });
        for (sprite.line_sizes, 0..) |size, k| try w.print("{s}{d}", .{ if (k > 0) "," else "", size });
        try w.writeAll("],\n\"edges\":[");
        for (sprite.edges, 0..) |edge, k| try w.print("{s}{d}", .{ if (k > 0) "," else "", edge });
        try w.writeAll("],\n\"squares\":[");
        for (sprite.squares, 0..) |square, k| {
            try w.writeAll(if (k > 0) ",\n" else "\n");
            try writeFloats(w, &std.mem.bytesToValue([9]f32, std.mem.asBytes(&square)));
        }
        try w.writeAll("]}");
    }
    try w.writeAll("]}\n");
}

pub fn writeFloats(w: *std.Io.Writer, values: []const f32) !void {
    try w.writeByte('[');
    for (values, 0..) |value, i| try w.print("{s}{d}", .{ if (i > 0) "," else "", value });
    try w.writeByte(']');
}

// ---------------------------------------------------------------------------
// Pictures

/// RGBA, top row first.
pub const Image = struct {
    width: u32,
    height: u32,
    pixels: [][4]u8,

    pub fn at(image: Image, x: i64, y: i64) ?*[4]u8 {
        if (x < 0 or y < 0 or x >= image.width or y >= image.height) return null;
        return &image.pixels[@intCast(y * image.width + x)];
    }
};

/// An uncompressed 32-bit DDS with alpha (the `_h.dds` every export writes as
/// ARGB8888), into RGBA.
pub fn readDds(gpa: std.mem.Allocator, bytes: []const u8) !Image {
    if (bytes.len < 128 or !std.mem.eql(u8, bytes[0..4], "DDS ")) return error.NotADds;
    const height = std.mem.readInt(u32, bytes[12..16], .little);
    const width = std.mem.readInt(u32, bytes[16..20], .little);
    const masks = [_]u32{ 0xff0000, 0xff00, 0xff, 0xff000000 };
    if (std.mem.readInt(u32, bytes[88..92], .little) != 32) return error.NotArgb8888;
    for (masks, 0..) |mask, i| {
        if (std.mem.readInt(u32, bytes[92 + 4 * i ..][0..4], .little) != mask) return error.NotArgb8888;
    }
    const count = @as(usize, width) * height;
    if (bytes.len < 128 + count * 4) return error.Truncated;
    const pixels = try gpa.alloc([4]u8, count);
    for (pixels, 0..) |*pixel, i| {
        const bgra = bytes[128 + i * 4 ..][0..4];
        pixel.* = .{ bgra[2], bgra[1], bgra[0], bgra[3] };
    }
    return .{ .width = width, .height = height, .pixels = pixels };
}

/// An RGBA PNG with stored (uncompressed) deflate blocks: short to write,
/// readable everywhere, and the same bytes for the same picture.
pub fn writePng(gpa: std.mem.Allocator, image: Image) ![]u8 {
    const row = image.width * 4 + 1;
    const raw = try gpa.alloc(u8, row * image.height);
    defer gpa.free(raw);
    for (0..image.height) |y| {
        raw[y * row] = 0;
        @memcpy(raw[y * row + 1 ..][0 .. row - 1], std.mem.sliceAsBytes(image.pixels[y * image.width ..][0..image.width]));
    }
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    const w = &out.writer;
    try w.writeAll("\x89PNG\r\n\x1a\n");
    var header: [13]u8 = undefined;
    std.mem.writeInt(u32, header[0..4], image.width, .big);
    std.mem.writeInt(u32, header[4..8], image.height, .big);
    header[8..13].* = .{ 8, 6, 0, 0, 0 };
    try pngChunk(w, "IHDR", &header);

    var zlib: std.Io.Writer.Allocating = .init(gpa);
    defer zlib.deinit();
    try zlib.writer.writeAll(&.{ 0x78, 0x01 });
    var at: usize = 0;
    while (true) {
        const n = @min(raw.len - at, 65535);
        const last = at + n == raw.len;
        try zlib.writer.writeByte(@intFromBool(last));
        try zlib.writer.writeInt(u16, @intCast(n), .little);
        try zlib.writer.writeInt(u16, ~@as(u16, @intCast(n)), .little);
        try zlib.writer.writeAll(raw[at..][0..n]);
        at += n;
        if (last) break;
    }
    try zlib.writer.writeInt(u32, adler32(raw), .big);
    try pngChunk(w, "IDAT", zlib.written());
    try pngChunk(w, "IEND", "");
    return out.toOwnedSlice();
}

/// writePng with its pixels deflated (the fastest level): the full 3D
/// models' atlases and their review pictures, several megabytes each stored.
pub fn writePngDeflated(gpa: std.mem.Allocator, image: Image) ![]u8 {
    const row = image.width * 4 + 1;
    const raw = try gpa.alloc(u8, row * image.height);
    defer gpa.free(raw);
    for (0..image.height) |y| {
        raw[y * row] = 0;
        @memcpy(raw[y * row + 1 ..][0 .. row - 1], std.mem.sliceAsBytes(image.pixels[y * image.width ..][0..image.width]));
    }
    var zlib: std.Io.Writer.Allocating = try .initCapacity(gpa, 64 * 1024);
    defer zlib.deinit();
    const window = try gpa.alloc(u8, std.compress.flate.max_window_len);
    defer gpa.free(window);
    var c = try std.compress.flate.Compress.init(&zlib.writer, window, .zlib, .fastest);
    try c.writer.writeAll(raw);
    try c.finish();
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    const w = &out.writer;
    try w.writeAll("\x89PNG\r\n\x1a\n");
    var header: [13]u8 = undefined;
    std.mem.writeInt(u32, header[0..4], image.width, .big);
    std.mem.writeInt(u32, header[4..8], image.height, .big);
    header[8..13].* = .{ 8, 6, 0, 0, 0 };
    try pngChunk(w, "IHDR", &header);
    try pngChunk(w, "IDAT", zlib.written());
    try pngChunk(w, "IEND", "");
    return out.toOwnedSlice();
}

/// Reads back what writePng and writePngDeflated write (8-bit RGBA, one
/// IDAT, every row unfiltered).
pub fn readPng(gpa: std.mem.Allocator, bytes: []const u8) !Image {
    if (bytes.len < 33 or !std.mem.eql(u8, bytes[0..8], "\x89PNG\r\n\x1a\n")) return error.NotAPng;
    const width = std.mem.readInt(u32, bytes[16..20], .big);
    const height = std.mem.readInt(u32, bytes[20..24], .big);
    const idat = std.mem.indexOf(u8, bytes, "IDAT") orelse return error.NotAPng;
    const length = std.mem.readInt(u32, bytes[idat - 4 ..][0..4], .big);
    var input: std.Io.Reader = .fixed(bytes[idat + 4 ..][0..length]);
    const window = try gpa.alloc(u8, std.compress.flate.max_window_len);
    defer gpa.free(window);
    var d: std.compress.flate.Decompress = .init(&input, .zlib, window);
    const row = @as(usize, width) * 4 + 1;
    const raw = try d.reader.readAlloc(gpa, row * height);
    defer gpa.free(raw);
    const pixels = try gpa.alloc([4]u8, @as(usize, width) * height);
    for (0..height) |y| {
        if (raw[y * row] != 0) return error.FilteredPng;
        @memcpy(std.mem.sliceAsBytes(pixels[y * width ..][0..width]), raw[y * row + 1 ..][0 .. row - 1]);
    }
    return .{ .width = width, .height = height, .pixels = pixels };
}

pub fn pngChunk(w: *std.Io.Writer, kind: *const [4]u8, data: []const u8) !void {
    try w.writeInt(u32, @intCast(data.len), .big);
    try w.writeAll(kind);
    try w.writeAll(data);
    var crc = std.hash.Crc32.init();
    crc.update(kind);
    crc.update(data);
    try w.writeInt(u32, crc.final(), .big);
}

pub fn adler32(bytes: []const u8) u32 {
    var a: u32 = 1;
    var b: u32 = 0;
    for (bytes) |byte| {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

/// Reads back what writePng writes (stored blocks only), for the tests.
pub fn readStoredPng(gpa: std.mem.Allocator, bytes: []const u8) !Image {
    if (bytes.len < 33 or !std.mem.eql(u8, bytes[0..8], "\x89PNG\r\n\x1a\n")) return error.NotAPng;
    const width = std.mem.readInt(u32, bytes[16..20], .big);
    const height = std.mem.readInt(u32, bytes[20..24], .big);
    const idat = std.mem.indexOf(u8, bytes, "IDAT") orelse return error.NotAPng;
    const length = std.mem.readInt(u32, bytes[idat - 4 ..][0..4], .big);
    const zlib = bytes[idat + 4 ..][0..length];
    const pixels = try gpa.alloc([4]u8, @as(usize, width) * height);
    const raw = std.mem.sliceAsBytes(pixels);
    var at: usize = 2;
    var done: usize = 0;
    var row_byte: usize = 0;
    while (true) {
        const last = zlib[at] & 1 != 0;
        const n = std.mem.readInt(u16, zlib[at + 1 ..][0..2], .little);
        for (zlib[at + 5 ..][0..n]) |byte| {
            if (row_byte % (width * 4 + 1) != 0) {
                raw[done] = byte;
                done += 1;
            }
            row_byte += 1;
        }
        at += 5 + @as(usize, n);
        if (last) break;
    }
    return .{ .width = width, .height = height, .pixels = pixels };
}

/// The squares' bounding box relative to the zero cross, as GetActualRect cuts it.
pub const Bounds = struct {
    min_x: i32 = std.math.maxInt(i32),
    min_y: i32 = std.math.maxInt(i32),
    max_x: i32 = std.math.minInt(i32),
    max_y: i32 = std.math.minInt(i32),

    pub fn add(bounds: *Bounds, square: Square) void {
        bounds.min_x = @min(bounds.min_x, @as(i32, @intFromFloat(square.x)));
        bounds.min_y = @min(bounds.min_y, @as(i32, @intFromFloat(square.y)));
        bounds.max_x = @max(bounds.max_x, @as(i32, @intFromFloat(square.x + square.size)));
        bounds.max_y = @max(bounds.max_y, @as(i32, @intFromFloat(square.y + square.size)));
    }
};

/// CSpritesPackBuilder::Unpack onto a canvas whose top left is `bounds`' corner.
pub fn unpack(gpa: std.mem.Allocator, sprite: Sprite, atlas: Image, bounds: Bounds) !Image {
    const width: u32 = @intCast(bounds.max_x - bounds.min_x);
    const height: u32 = @intCast(bounds.max_y - bounds.min_y);
    const pixels = try gpa.alloc([4]u8, @as(usize, width) * height);
    @memset(pixels, .{ 0, 0, 0, 0 });
    const canvas: Image = .{ .width = width, .height = height, .pixels = pixels };
    const aw: f32 = @floatFromInt(atlas.width);
    const ah: f32 = @floatFromInt(atlas.height);
    for (sprite.squares) |square| {
        const sx: i64 = @intFromFloat(square.u1 * aw);
        const sy: i64 = @intFromFloat(square.v1 * ah);
        const w: i64 = @as(i64, @intFromFloat(square.u2 * aw)) - sx;
        const h: i64 = @as(i64, @intFromFloat(square.v2 * ah)) - sy;
        const dx: i64 = @as(i64, @intFromFloat(square.x)) - bounds.min_x;
        const dy: i64 = @as(i64, @intFromFloat(square.y)) - bounds.min_y;
        var y: i64 = 0;
        while (y < h) : (y += 1) {
            var x: i64 = 0;
            while (x < w) : (x += 1) {
                const from = atlas.at(sx + x, sy + y) orelse continue;
                const to = canvas.at(dx + x, dy + y) orelse continue;
                to.* = from.*;
            }
        }
    }
    return canvas;
}

/// The variant's `1.xml` as decompose and camera need it.
pub const Stats = struct {
    passability: []const []const u8 = &.{},
    origin: [2]f32 = .{ 0, 0 },
    /// PicturePosition (relative to the zero cross) beside WorldPosition.
    pairs: []const [5]f32 = &.{},
};

pub fn readStats(gpa: std.mem.Allocator, xml: []const u8) !Stats {
    var stats: Stats = .{};
    stats.passability = try gridRows(gpa, xml, "passability");
    if (std.mem.indexOf(u8, xml, "<origin ")) |at| {
        stats.origin = .{ attribute(xml[at..], "x") orelse 0, attribute(xml[at..], "y") orelse 0 };
    }
    var pairs: std.ArrayList([5]f32) = .empty;
    var pos: usize = 0;
    while (std.mem.indexOfPos(u8, xml, pos, "<PicturePosition ")) |picture| {
        const world = std.mem.indexOfPos(u8, xml, picture, "<WorldPosition ") orelse break;
        pos = world;
        const next = std.mem.indexOfPos(u8, xml, picture + 1, "<PicturePosition ") orelse xml.len;
        if (next < world) continue;
        const p = xml[picture..];
        const q = xml[world..];
        try pairs.append(gpa, .{
            attribute(p, "x") orelse continue, attribute(p, "y") orelse continue,
            attribute(q, "x") orelse continue, attribute(q, "y") orelse continue,
            attribute(q, "z") orelse continue,
        });
    }
    stats.pairs = pairs.items;
    return stats;
}

/// The rows of the grid `name` (passability or visibility), two hex digits a cell.
pub fn gridRows(gpa: std.mem.Allocator, xml: []const u8, comptime name: []const u8) ![]const []const u8 {
    const grid = sliceBetween(xml, "<" ++ name ++ ">", "</" ++ name ++ ">") orelse return &.{};
    var rows: std.ArrayList([]const u8) = .empty;
    var rest = grid;
    while (sliceBetween(rest, "<item>", "</item>")) |row| {
        try rows.append(gpa, row);
        rest = rest[@intFromPtr(row.ptr) - @intFromPtr(rest.ptr) + row.len ..];
    }
    return rows.items;
}

pub fn sliceBetween(text: []const u8, open: []const u8, close: []const u8) ?[]const u8 {
    const start = (std.mem.indexOf(u8, text, open) orelse return null) + open.len;
    const end = std.mem.indexOfPos(u8, text, start, close) orelse return null;
    return text[start..end];
}

/// The float attribute `name` of the element `text` starts with.
pub fn attribute(text: []const u8, name: []const u8) ?f32 {
    const end = std.mem.indexOfScalar(u8, text, '>') orelse return null;
    const tag = text[0..end];
    var pos: usize = 0;
    while (std.mem.indexOfPos(u8, tag, pos, name)) |at| : (pos = at + 1) {
        if (at == 0 or tag[at - 1] != ' ' or !std.mem.startsWith(u8, tag[at + name.len ..], "=\"")) continue;
        const value = tag[at + name.len + 2 ..];
        const quote = std.mem.indexOfScalar(u8, value, '"') orelse return null;
        return std.fmt.parseFloat(f32, value[0..quote]) catch null;
    }
    return null;
}

/// The grid origin on screen under the MFC editors' camera
/// (NCompose::Origin2DPosition), cut to int as SPackParameter takes it.
pub fn lockedCenter(origin: [2]f32) [2]i32 {
    const c: f32 = @cos(@as(f32, 45.0 * std.math.pi / 180.0));
    const s: f32 = @sin(@as(f32, 45.0 * std.math.pi / 180.0));
    const cp: f32 = @cos(@as(f32, -120.0 * std.math.pi / 180.0));
    const x = c * -origin[0] + s * -origin[1];
    const y = -s * cp * -origin[0] + c * cp * -origin[1];
    return .{ @intFromFloat(x), @intFromFloat(y) };
}

/// The export packs the shadow outside the sprite only, and an empty pack
/// fails: a low ruin whose shadow falls all under it keeps one shadow pixel
/// below its lowest pixel (the canvas has a margin), as does a picture
/// imported over its shadow.
pub fn keepShadow(sprite: Image, shadow: Image) void {
    var lowest: ?usize = null;
    for (sprite.pixels, shadow.pixels, 0..) |p, s, i| {
        if (s[3] > 0 and p[3] < 128) return;
        if (p[3] >= 128) lowest = i;
    }
    if (lowest) |i| if (i + shadow.width < shadow.pixels.len) {
        shadow.pixels[i + shadow.width] = .{ 0, 0, 0, 255 };
    };
}
