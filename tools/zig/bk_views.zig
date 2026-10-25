//! `bk-views`: the building views pipeline of the Remastered rotatable camera
//! (docs/superpowers/specs/2026-10-25-building-views-design.md), std-only Zig.
//!
//! `decompose` turns the sprite packs of one building variant back into
//! pictures: every `.san` (an engine structure file holding `SSpritesPack`,
//! Formats/fmtSprite.h) with its uncompressed `_h.dds` atlas becomes a PNG per
//! sprite and a JSON of every pack field. The pictures are what
//! `CSpritesPackBuilder::Unpack` makes, placed on one canvas per variant with
//! the zero cross at the same pixel, so the sprite, its shadow and its noise
//! picture can go back through the Resource Editor's building composer
//! (`bk-sprite compose-pack`) unchanged.
//!
//! `camera` solves the 2003 camera from the picture/world point pairs every
//! building's `1.xml` stores, and measures it by drawing a box over each
//! building's footprint against the sprite's outline.
//!
//! `inventory` groups the variant folders of `Data/Buildings` into families
//! and measures which side of its building each variant shows; `turnXml`
//! turns a variant's `1.xml` to another side.
//!
//! `side` makes a variant turned by quarter turns: a voxel block-out of the
//! building carved from its family's outlines, the 2003 pictures of the
//! variant's colourway projected onto it (Blender casts the rays,
//! tools/blender/bk_views_render.py), mirrored or patch-filled where none
//! sees a face, and packed by `bk-sprite compose-pack`.

const std = @import("std");
const bk_tool = @import("bk_tool.zig");

const usage: bk_tool.Usage = .{
    .synopsis =
    \\usage: bk-views decompose <variant-dir> <out-dir>
    \\       bk-views camera <variant-dir>... [--out <camera.json>]
    \\       bk-views inventory <buildings-dir> <out-dir>
    \\       bk-views side <variant-dir> <turns> <out-dir>
    \\
    ,
    .body = &.{"views.usage.body"},
};

pub fn main(init: std.process.Init) u8 {
    return bk_tool.main(init, "bk-views", usage, run);
}

fn run(ctx: *bk_tool.Context) !void {
    if (ctx.args.len == 0) return error.Usage;
    const command = ctx.args[0];
    if (std.mem.eql(u8, command, "decompose")) {
        if (ctx.args.len != 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return decomposeCommand(ctx, ctx.args[1], ctx.args[2]);
    }
    if (std.mem.eql(u8, command, "inventory")) {
        if (ctx.args.len != 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return inventoryCommand(ctx, ctx.args[1], ctx.args[2]);
    }
    if (std.mem.eql(u8, command, "side")) {
        if (ctx.args.len != 4) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return sideCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3]);
    }
    if (std.mem.eql(u8, command, "camera")) {
        var dirs: std.ArrayList([]const u8) = .empty;
        var out: ?[]const u8 = null;
        var i: usize = 1;
        while (i < ctx.args.len) : (i += 1) {
            const arg = ctx.args[i];
            if (std.mem.eql(u8, arg, "--out")) {
                if (out != null or i + 1 == ctx.args.len) return error.Usage;
                i += 1;
                out = ctx.args[i];
            } else if (std.mem.startsWith(u8, arg, "-")) {
                return error.Usage;
            } else {
                try dirs.append(ctx.arena, arg);
            }
        }
        if (dirs.items.len == 0) return error.Usage;
        return cameraCommand(ctx, dirs.items, out);
    }
    return error.Usage;
}

// ---------------------------------------------------------------------------
// The engine's structure file (StreamIO/StructureSaver2.cpp)

/// One chunk: an id byte, the length shifted left by one in one byte, or in
/// four bytes with the low bit set when the shifted length is 256 or more.
const Chunk = struct { id: u8, body: []const u8 };

fn nextChunk(bytes: []const u8, pos: *usize) ?Chunk {
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

fn countChunks(bytes: []const u8, id: u8) usize {
    var pos: usize = 0;
    var count: usize = 0;
    while (nextChunk(bytes, &pos)) |chunk| count += @intFromBool(chunk.id == id);
    return count;
}

/// A data chunk as DataChunk reads it: missing is zero, a wrong size is an error.
fn rawChunk(comptime T: type, bytes: []const u8, id: u8) !T {
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

fn rawArray(comptime T: type, gpa: std.mem.Allocator, bytes: []const u8, id: u8, count: i32) ![]T {
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

fn writeFloats(w: *std.Io.Writer, values: []const f32) !void {
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

    fn at(image: Image, x: i64, y: i64) ?*[4]u8 {
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

fn pngChunk(w: *std.Io.Writer, kind: *const [4]u8, data: []const u8) !void {
    try w.writeInt(u32, @intCast(data.len), .big);
    try w.writeAll(kind);
    try w.writeAll(data);
    var crc = std.hash.Crc32.init();
    crc.update(kind);
    crc.update(data);
    try w.writeInt(u32, crc.final(), .big);
}

fn adler32(bytes: []const u8) u32 {
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

// ---------------------------------------------------------------------------
// decompose

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
fn gridRows(gpa: std.mem.Allocator, xml: []const u8, comptime name: []const u8) ![]const []const u8 {
    const grid = sliceBetween(xml, "<" ++ name ++ ">", "</" ++ name ++ ">") orelse return &.{};
    var rows: std.ArrayList([]const u8) = .empty;
    var rest = grid;
    while (sliceBetween(rest, "<item>", "</item>")) |row| {
        try rows.append(gpa, row);
        rest = rest[@intFromPtr(row.ptr) - @intFromPtr(rest.ptr) + row.len ..];
    }
    return rows.items;
}

fn sliceBetween(text: []const u8, open: []const u8, close: []const u8) ?[]const u8 {
    const start = (std.mem.indexOf(u8, text, open) orelse return null) + open.len;
    const end = std.mem.indexOfPos(u8, text, start, close) orelse return null;
    return text[start..end];
}

/// The float attribute `name` of the element `text` starts with.
fn attribute(text: []const u8, name: []const u8) ?f32 {
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

const PackFile = struct { name: []const u8, pack: Pack, atlas: Image };

/// Every pack of a variant folder in name order, and the canvas they share:
/// it starts where the 2003 picture did, so its anchor is the zero cross.
const Packs = struct { files: []PackFile, bounds: Bounds };

fn readPacks(ctx: *bk_tool.Context, dir_path: []const u8) !Packs {
    const io = ctx.io;
    const arena = ctx.arena;
    var dir = std.Io.Dir.cwd().openDir(io, dir_path, .{ .iterate = true }) catch return ctx.failKey("views.err.no_dir", .{dir_path});
    defer dir.close(io);

    var names: std.ArrayList([]const u8) = .empty;
    var iterator = dir.iterate();
    while (try iterator.next(io)) |entry| {
        if (entry.kind == .file and std.ascii.endsWithIgnoreCase(entry.name, ".san"))
            try names.append(arena, try arena.dupe(u8, entry.name[0 .. entry.name.len - 4]));
    }
    if (names.items.len == 0) return ctx.failKey("views.err.no_san", .{dir_path});
    std.mem.sort([]const u8, names.items, {}, lessThan);

    var files: std.ArrayList(PackFile) = .empty;
    var bounds: Bounds = .{};
    var center: ?[2]i32 = null;
    for (names.items) |name| {
        const san_path = try std.fs.path.join(arena, &.{ dir_path, try std.fmt.allocPrint(arena, "{s}.san", .{name}) });
        const dds_path = try std.fs.path.join(arena, &.{ dir_path, try std.fmt.allocPrint(arena, "{s}_h.dds", .{name}) });
        const san = try readFile(ctx, san_path);
        const pack = decodePack(arena, san) catch |err| return ctx.failKey("views.err.bad_san", .{ san_path, @errorName(err) });
        const atlas = readDds(arena, try readFile(ctx, dds_path)) catch |err| return ctx.failKey("views.err.bad_dds", .{ dds_path, @errorName(err) });
        for (pack.sprites) |sprite| {
            if (center) |c| {
                if (c[0] != sprite.center[0] or c[1] != sprite.center[1]) return ctx.failKey("views.err.centers", .{dir_path});
            }
            center = sprite.center;
            for (sprite.squares) |square| bounds.add(square);
        }
        try files.append(arena, .{ .name = name, .pack = pack, .atlas = atlas });
    }
    if (bounds.min_x > bounds.max_x) return ctx.failKey("views.err.no_san", .{dir_path});
    // The canvas starts where the 2003 picture did: the pack's center is the
    // zero cross in that picture, and composing the canvas again keeps it.
    // Squares reaching left of or above it hold only the padding the packer
    // added outside that picture, which is empty.
    bounds.min_x = -center.?[0];
    bounds.min_y = -center.?[1];
    bounds.max_x = @max(bounds.max_x, bounds.min_x + 1);
    bounds.max_y = @max(bounds.max_y, bounds.min_y + 1);
    return .{ .files = files.items, .bounds = bounds };
}

fn decomposeCommand(ctx: *bk_tool.Context, dir_arg: []const u8, out_arg: []const u8) !void {
    const io = ctx.io;
    const arena = ctx.arena;
    const dir_path = try bk_tool.inputPath(arena, dir_arg);
    const out_path = try bk_tool.inputPath(arena, out_arg);
    const cwd = std.Io.Dir.cwd();
    const packs = try readPacks(ctx, dir_path);
    const files = packs.files;
    const bounds = packs.bounds;

    try cwd.createDirPath(io, out_path);
    var pictures: std.ArrayList([]const u8) = .empty;
    for (files) |file| {
        var json: std.Io.Writer.Allocating = .init(arena);
        try writePackJson(&json.writer, file.pack);
        try writeOut(ctx, out_path, try std.fmt.allocPrint(arena, "{s}.json", .{file.name}), json.written());
        for (file.pack.sprites, 0..) |sprite, i| {
            const picture = try std.fmt.allocPrint(arena, "{s}_{d}.png", .{ file.name, i });
            try writeOut(ctx, out_path, picture, try writePng(arena, try unpack(arena, sprite, file.atlas, bounds)));
            try pictures.append(arena, picture);
        }
    }

    const stats_path = try std.fs.path.join(arena, &.{ dir_path, "1.xml" });
    const stats = if (cwd.readFileAlloc(io, stats_path, arena, .limited(64 * 1024 * 1024))) |xml| try readStats(arena, xml) else |_| Stats{};
    var building: std.Io.Writer.Allocating = .init(arena);
    const w = &building.writer;
    const locked = lockedCenter(stats.origin);
    try w.print("{{\"canvas\":[{d},{d}],\"anchor\":[{d},{d}],\"origin\":[{d},{d}],\"locked_center\":[{d},{d}],\n\"passability\":[", .{
        bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y, -bounds.min_x, -bounds.min_y,
        stats.origin[0],             stats.origin[1],             locked[0],     locked[1],
    });
    for (stats.passability, 0..) |row, i| try w.print("{s}\"{s}\"", .{ if (i > 0) "," else "", row });
    try w.writeAll("],\n\"pictures\":[");
    for (pictures.items, 0..) |picture, i| try w.print("{s}\"{s}\"", .{ if (i > 0) "," else "", picture });
    try w.writeAll("]}\n");
    try writeOut(ctx, out_path, "building.json", building.written());
    try ctx.out.print("wrote {s}: {d} packs, {d} pictures of {d}x{d}\n", .{ out_path, files.len, pictures.items.len, bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y });
}

fn lessThan(_: void, a: []const u8, b: []const u8) bool {
    return std.mem.lessThan(u8, a, b);
}

fn readFile(ctx: *bk_tool.Context, path: []const u8) ![]u8 {
    return std.Io.Dir.cwd().readFileAlloc(ctx.io, path, ctx.arena, .limited(256 * 1024 * 1024)) catch |err|
        ctx.failKey("views.err.read", .{ path, @errorName(err) });
}

fn writeOut(ctx: *bk_tool.Context, dir: []const u8, name: []const u8, data: []const u8) !void {
    const path = try std.fs.path.join(ctx.arena, &.{ dir, name });
    std.Io.Dir.cwd().writeFile(ctx.io, .{ .sub_path = path, .data = data }) catch |err|
        return ctx.failKey("views.err.read", .{ path, @errorName(err) });
}

// ---------------------------------------------------------------------------
// camera

/// A linear orthographic camera: picture = rows * world, relative to the zero cross.
pub const Camera = struct {
    rows: [2][3]f64,

    pub fn project(camera: Camera, p: [3]f64) [2]f64 {
        var out: [2]f64 = undefined;
        for (0..2) |r| out[r] = camera.rows[r][0] * p[0] + camera.rows[r][1] * p[1] + camera.rows[r][2] * p[2];
        return out;
    }

    /// Pixels per world unit, yaw and pitch in degrees, as CCamera's angles:
    /// row 0 = s (cos yaw, sin yaw, 0), row 1 = s (-sin yaw cos pitch, cos yaw cos pitch, sin pitch).
    pub fn angles(camera: Camera) [3]f64 {
        const r = camera.rows;
        const scale = @sqrt(r[0][0] * r[0][0] + r[0][1] * r[0][1]);
        const yaw = std.math.atan2(r[0][1], r[0][0]);
        const cos_pitch = (r[1][1] * @cos(yaw) - r[1][0] * @sin(yaw)) / scale;
        const pitch = std.math.atan2(r[1][2] / scale, cos_pitch);
        return .{ scale, std.math.radiansToDegrees(yaw), std.math.radiansToDegrees(pitch) };
    }
};

/// MfcEditorCamera's linear part: one world unit per pixel, yaw 45, pitch -120.
pub const mfc_camera: Camera = blk: {
    const c = @sqrt(0.5);
    const cp = -0.5;
    const sp = -@sqrt(0.75);
    break :blk .{ .rows = .{ .{ c, c, 0 }, .{ -c * cp, c * cp, sp } } };
};

/// Least squares over the point pairs, each picture row on its own.
pub fn fitCamera(pairs: []const [5]f32) !Camera {
    if (pairs.len < 3) return error.TooFewPairs;
    var m: [3][3]f64 = std.mem.zeroes([3][3]f64);
    var rhs: [2][3]f64 = std.mem.zeroes([2][3]f64);
    for (pairs) |pair| {
        const p = [3]f64{ pair[2], pair[3], pair[4] };
        for (0..3) |i| {
            for (0..3) |j| m[i][j] += p[i] * p[j];
            rhs[0][i] += p[i] * pair[0];
            rhs[1][i] += p[i] * pair[1];
        }
    }
    var camera: Camera = undefined;
    for (0..2) |r| camera.rows[r] = try solve3(m, rhs[r]);
    return camera;
}

fn solve3(m: [3][3]f64, b: [3]f64) ![3]f64 {
    const det = determinant(m);
    if (@abs(det) < 1e-9) return error.Singular;
    var x: [3]f64 = undefined;
    for (0..3) |col| {
        var mc = m;
        for (0..3) |row| mc[row][col] = b[row];
        x[col] = determinant(mc) / det;
    }
    return x;
}

fn determinant(m: [3][3]f64) f64 {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

/// The world size of an AI tile: fWorldCellSize / 2 (items/stats_item.h), 32 x 16 px under the MFC camera.
pub const tile_size: f64 = 16.0 * 1.41421356;

/// The best overlap of a box over the locked footprint with the sprite's
/// outline (alpha 128 or more), over the box heights 0..600.
pub const BoxFit = struct { height: f64, iou: f64 };

pub fn fitBox(camera: Camera, stats: Stats, sprite: Image, anchor: [2]i32) BoxFit {
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (stats.passability, 0..) |row, y| {
        for (0..row.len / 2) |x| {
            if (std.mem.eql(u8, row[2 * x ..][0..2], "00")) continue;
            const fx: f64 = @floatFromInt(x);
            const fy: f64 = @floatFromInt(y);
            lo = .{ @min(lo[0], fx), @min(lo[1], fy) };
            hi = .{ @max(hi[0], fx + 1), @max(hi[1], fy + 1) };
        }
    }
    var best: BoxFit = .{ .height = 0, .iou = 0 };
    if (lo[0] > hi[0]) return best;
    const box = [4]f64{ lo[0] * tile_size - stats.origin[0], hi[0] * tile_size - stats.origin[0], lo[1] * tile_size - stats.origin[1], hi[1] * tile_size - stats.origin[1] };
    // Every eighth height, then every height around the best of those.
    var height: f64 = 0;
    while (height <= 600) : (height += 8) {
        const iou = boxIou(camera, box, height, sprite, anchor);
        if (iou > best.iou) best = .{ .height = height, .iou = iou };
    }
    const coarse = best.height;
    height = @max(0, coarse - 7);
    while (height <= coarse + 7) : (height += 1) {
        const iou = boxIou(camera, box, height, sprite, anchor);
        if (iou > best.iou) best = .{ .height = height, .iou = iou };
    }
    return best;
}

fn boxIou(camera: Camera, box: [4]f64, height: f64, sprite: Image, anchor: [2]i32) f64 {
    const x0, const x1, const y0, const y1 = box;
    {
        var corners: [8][2]f64 = undefined;
        for (0..8) |i| {
            const p = [3]f64{ if (i & 1 != 0) x1 else x0, if (i & 2 != 0) y1 else y0, if (i & 4 != 0) height else 0 };
            const q = camera.project(p);
            corners[i] = .{ q[0] + @as(f64, @floatFromInt(anchor[0])), q[1] + @as(f64, @floatFromInt(anchor[1])) };
        }
        var hull_buffer: [9][2]f64 = undefined;
        const hull = convexHull(&corners, &hull_buffer);
        var both: u64 = 0;
        var either: u64 = 0;
        // The box can leave the canvas; the sprite cannot, so pixels outside count as box only.
        var min = [2]f64{ 0, 0 };
        var max = [2]f64{ @floatFromInt(sprite.width), @floatFromInt(sprite.height) };
        for (hull) |p| {
            min = .{ @min(min[0], p[0]), @min(min[1], p[1]) };
            max = .{ @max(max[0], p[0]), @max(max[1], p[1]) };
        }
        var py: i64 = @intFromFloat(@floor(min[1]));
        while (py < @as(i64, @intFromFloat(@ceil(max[1])))) : (py += 1) {
            var px: i64 = @intFromFloat(@floor(min[0]));
            while (px < @as(i64, @intFromFloat(@ceil(max[0])))) : (px += 1) {
                const in_box = insideHull(hull, .{ @as(f64, @floatFromInt(px)) + 0.5, @as(f64, @floatFromInt(py)) + 0.5 });
                const in_sprite = if (sprite.at(px, py)) |pixel| pixel[3] >= 128 else false;
                both += @intFromBool(in_box and in_sprite);
                either += @intFromBool(in_box or in_sprite);
            }
        }
        return if (either == 0) 0 else @as(f64, @floatFromInt(both)) / @as(f64, @floatFromInt(either));
    }
}

/// Andrew's monotone chain, counter-clockwise in screen coordinates.
fn convexHull(points: *[8][2]f64, buffer: *[9][2]f64) []const [2]f64 {
    std.mem.sort([2]f64, points, {}, struct {
        fn less(_: void, a: [2]f64, b: [2]f64) bool {
            return a[0] < b[0] or (a[0] == b[0] and a[1] < b[1]);
        }
    }.less);
    var n: usize = 0;
    for (0..2) |pass| {
        const start = n;
        var k: usize = 0;
        while (k < 8) : (k += 1) {
            const p = if (pass == 0) points[k] else points[7 - k];
            while (n >= start + 2 and cross(buffer[n - 2], buffer[n - 1], p) <= 0) n -= 1;
            buffer[n] = p;
            n += 1;
        }
        n -= 1;
    }
    return buffer[0..n];
}

fn cross(o: [2]f64, a: [2]f64, b: [2]f64) f64 {
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
}

fn insideHull(hull: []const [2]f64, p: [2]f64) bool {
    for (hull, 0..) |a, i| {
        if (cross(a, hull[(i + 1) % hull.len], p) < 0) return false;
    }
    return hull.len >= 3;
}

fn cameraCommand(ctx: *bk_tool.Context, dir_args: []const []const u8, out_arg: ?[]const u8) !void {
    const arena = ctx.arena;
    const io = ctx.io;
    const cwd = std.Io.Dir.cwd();
    const Sample = struct { dir: []const u8, stats: Stats, sprite: Image, anchor: [2]i32 };
    var variants: std.ArrayList(Sample) = .empty;
    var pairs: std.ArrayList([5]f32) = .empty;
    for (dir_args) |dir_arg| {
        const dir = try bk_tool.inputPath(arena, dir_arg);
        const stats = readStats(arena, try readFile(ctx, try std.fs.path.join(arena, &.{ dir, "1.xml" }))) catch |err|
            return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
        if (stats.pairs.len == 0) return ctx.failKey("views.err.no_pairs", .{dir});
        try pairs.appendSlice(arena, stats.pairs);
        const san_path = try std.fs.path.join(arena, &.{ dir, "1.san" });
        const dds_path = try std.fs.path.join(arena, &.{ dir, "1_h.dds" });
        const pack = decodePack(arena, try readFile(ctx, san_path)) catch |err| return ctx.failKey("views.err.bad_san", .{ san_path, @errorName(err) });
        const atlas = readDds(arena, try readFile(ctx, dds_path)) catch |err| return ctx.failKey("views.err.bad_dds", .{ dds_path, @errorName(err) });
        if (pack.sprites.len == 0) return ctx.failKey("views.err.no_san", .{dir});
        var bounds: Bounds = .{};
        for (pack.sprites[0].squares) |square| bounds.add(square);
        try variants.append(arena, .{
            .dir = dir,
            .stats = stats,
            .sprite = try unpack(arena, pack.sprites[0], atlas, bounds),
            .anchor = .{ -bounds.min_x, -bounds.min_y },
        });
    }
    const camera = fitCamera(pairs.items) catch return ctx.failKey("views.err.few_pairs", .{});
    var residual: f64 = 0;
    for (pairs.items) |pair| {
        const q = camera.project(.{ pair[2], pair[3], pair[4] });
        residual += (q[0] - pair[0]) * (q[0] - pair[0]) + (q[1] - pair[1]) * (q[1] - pair[1]);
    }
    const rms = @sqrt(residual / @as(f64, @floatFromInt(pairs.items.len)));
    const fitted = camera.angles();
    const mfc = mfc_camera.angles();

    var json: std.Io.Writer.Allocating = .init(arena);
    const w = &json.writer;
    try w.print("{{\"pairs\":{d},\"rms_px\":{d:.4},\n\"fitted\":{{\"px_per_world_unit\":{d:.6},\"yaw\":{d:.4},\"pitch\":{d:.4},\"rows\":[[{d:.6},{d:.6},{d:.6}],[{d:.6},{d:.6},{d:.6}]]}},\n", .{
        pairs.items.len,   rms,               fitted[0],         fitted[1],         fitted[2],
        camera.rows[0][0], camera.rows[0][1], camera.rows[0][2], camera.rows[1][0], camera.rows[1][1],
        camera.rows[1][2],
    });
    try w.print("\"mfc_editor_camera\":{{\"px_per_world_unit\":{d:.6},\"yaw\":{d:.4},\"pitch\":{d:.4}}},\n\"px_per_ai_tile\":[{d:.4},{d:.4}],\n\"box_fit\":[", .{
        mfc[0], mfc[1], mfc[2], fitted[0] * tile_size * @sqrt(2.0), fitted[0] * tile_size * @sqrt(0.5),
    });
    try ctx.out.print("{d} point pairs, rms {d:.3} px: {d:.4} px per world unit, yaw {d:.2}, pitch {d:.2} (MFC editor camera: {d:.4}, {d:.2}, {d:.2})\n", .{ pairs.items.len, rms, fitted[0], fitted[1], fitted[2], mfc[0], mfc[1], mfc[2] });
    var min_iou: f64 = 1;
    for (variants.items, 0..) |variant, i| {
        const fit = fitBox(camera, variant.stats, variant.sprite, variant.anchor);
        min_iou = @min(min_iou, fit.iou);
        const name = std.fs.path.basename(variant.dir);
        try w.print("{s}\n{{\"variant\":\"{s}\",\"height\":{d},\"iou\":{d:.4}}}", .{ if (i > 0) "," else "", name, fit.height, fit.iou });
        try ctx.out.print("{s}: box height {d}, IoU {d:.3}\n", .{ name, fit.height, fit.iou });
    }
    try w.print("],\n\"min_iou\":{d:.4}}}\n", .{min_iou});
    if (out_arg) |out| {
        const path = try bk_tool.inputPath(arena, out);
        if (std.fs.path.dirname(path)) |parent| try cwd.createDirPath(io, parent);
        cwd.writeFile(io, .{ .sub_path = path, .data = json.written() }) catch |err| return ctx.failKey("views.err.read", .{ path, @errorName(err) });
    }
    try ctx.out.print("camera of {d} variants, lowest box IoU {d:.3}\n", .{ variants.items.len, min_iou });
}

// ---------------------------------------------------------------------------
// quarter turns

/// One quarter turn of a building turns world (x, y) into (-y, x) about the
/// zero cross: the footprint's front corner goes where its right corner was,
/// and a wall facing Direction d then faces d + 90 (in 1.xml Direction 0
/// faces +y, 90 -x, 180 -y and 270 +x).
pub fn turnPoint(p: [2]f64, k: u2) [2]f64 {
    var q = p;
    for (0..k) |_| {
        const x = q[0];
        q = .{ -q[1], x };
    }
    return q;
}

pub const Grid = struct { rows: []const []const u8, origin: [2]f64 };

/// A 1.xml grid after k quarter turns, with the origin that keeps every cell
/// under the same part of the building: cell (x, y) covers world
/// [x, x + 1] * tile_size - origin.
pub fn turnGrid(gpa: std.mem.Allocator, grid: Grid, k: u2) !Grid {
    var out = grid;
    for (0..k) |_| {
        const h = out.rows.len;
        const w = if (h == 0) 0 else out.rows[0].len / 2;
        const rows = try gpa.alloc([]u8, w);
        for (rows, 0..) |*row, y| {
            row.* = try gpa.alloc(u8, 2 * h);
            for (0..h) |x| row.*[2 * x ..][0..2].* = out.rows[h - 1 - x][2 * y ..][0..2].*;
        }
        // Built apart: assigned in place, origin[1] would read the new origin[0].
        const origin = [2]f64{ @as(f64, @floatFromInt(h)) * tile_size - out.origin[1], out.origin[0] };
        out = .{ .rows = rows, .origin = origin };
    }
    return out;
}

/// `1.xml` after k quarter turns: the passability and visibility grids with
/// their origins, every Position and WorldPosition, the PicturePosition the
/// 2003 camera gives the turned WorldPosition, and every Direction.
pub fn turnXml(gpa: std.mem.Allocator, xml: []const u8, k: u2) ![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    const w = &out.writer;
    var pos: usize = 0;
    while (std.mem.indexOfScalarPos(u8, xml, pos, '<')) |open| {
        const close = std.mem.indexOfScalarPos(u8, xml, open, '>') orelse break;
        try w.writeAll(xml[pos..open]);
        var tag: []const u8 = xml[open .. close + 1];
        pos = close + 1;
        inline for (.{ .{ "passability", "origin" }, .{ "visibility", "VisOrigin" } }) |names| {
            if (std.mem.eql(u8, tag, "<" ++ names[0] ++ ">")) {
                const turned = try turnGrid(gpa, try xmlGrid(gpa, xml, names[0], names[1]), k);
                try w.print("<" ++ names[0] ++ "><item size_x=\"{d}\" size_y=\"{d}\"/>", .{ if (turned.rows.len == 0) 0 else turned.rows[0].len / 2, turned.rows.len });
                for (turned.rows) |row| try w.print("<item>{s}</item>", .{row});
                pos = (std.mem.indexOfPos(u8, xml, pos, "</" ++ names[0] ++ ">") orelse return error.BadGrid) + names[0].len + 3;
                tag = "</" ++ names[0] ++ ">";
            } else if (std.mem.startsWith(u8, tag, "<" ++ names[1] ++ " ")) {
                const turned = try turnGrid(gpa, try xmlGrid(gpa, xml, names[0], names[1]), k);
                tag = try setAttribute(gpa, try setAttribute(gpa, tag, "x", turned.origin[0]), "y", turned.origin[1]);
            }
        }
        if (std.mem.startsWith(u8, tag, "<Position ") or std.mem.startsWith(u8, tag, "<WorldPosition ")) {
            const p = turnPoint(.{ attribute(tag, "x") orelse 0, attribute(tag, "y") orelse 0 }, k);
            tag = try setAttribute(gpa, try setAttribute(gpa, tag, "x", p[0]), "y", p[1]);
        } else if (std.mem.startsWith(u8, tag, "<PicturePosition ")) {
            const at = std.mem.indexOfPos(u8, xml, pos, "<WorldPosition ") orelse return error.NoWorldPosition;
            const p = turnPoint(.{ attribute(xml[at..], "x") orelse 0, attribute(xml[at..], "y") orelse 0 }, k);
            const q = mfc_camera.project(.{ p[0], p[1], attribute(xml[at..], "z") orelse 0 });
            tag = try setAttribute(gpa, try setAttribute(gpa, tag, "x", q[0]), "y", q[1]);
        }
        if (attribute(tag, "Direction")) |d| tag = try setAttribute(gpa, tag, "Direction", @mod(d + 90 * @as(f64, @floatFromInt(k)), 360));
        try w.writeAll(tag);
    }
    try w.writeAll(xml[pos..]);
    return out.written();
}

fn xmlGrid(gpa: std.mem.Allocator, xml: []const u8, comptime grid: []const u8, comptime origin: []const u8) !Grid {
    const at = std.mem.indexOf(u8, xml, "<" ++ origin ++ " ") orelse return error.NoOrigin;
    return .{ .rows = try gridRows(gpa, xml, grid), .origin = .{ attribute(xml[at..], "x") orelse 0, attribute(xml[at..], "y") orelse 0 } };
}

/// The tag with attribute `name` set to `value`, written as the f32 1.xml holds.
fn setAttribute(gpa: std.mem.Allocator, tag: []const u8, name: []const u8, value: f64) ![]const u8 {
    const key = try std.fmt.allocPrint(gpa, " {s}=\"", .{name});
    const start = (std.mem.indexOf(u8, tag, key) orelse return tag) + key.len;
    const end = std.mem.indexOfScalarPos(u8, tag, start, '"') orelse return tag;
    const v: f32 = @floatCast(value);
    return std.fmt.allocPrint(gpa, "{s}{d}{s}", .{ tag[0..start], v, tag[end..] });
}

/// How well grid a turned k times covers grid b: the overlap of their
/// occupied cells over their union, the occupied boxes laid corner on corner.
pub fn footprintIou(gpa: std.mem.Allocator, a: []const []const u8, b: []const []const u8, k: u2) !f64 {
    const turned = (try turnGrid(gpa, .{ .rows = a, .origin = .{ 0, 0 } }, k)).rows;
    const ba = occupiedBox(turned);
    const bb = occupiedBox(b);
    var both: usize = 0;
    var either: usize = 0;
    for (0..@max(ba[3] - ba[1], bb[3] - bb[1])) |y| for (0..@max(ba[2] - ba[0], bb[2] - bb[0])) |x| {
        const in_a = occupied(turned, ba[0] + x, ba[1] + y);
        const in_b = occupied(b, bb[0] + x, bb[1] + y);
        both += @intFromBool(in_a and in_b);
        either += @intFromBool(in_a or in_b);
    };
    return if (either == 0) 0 else @as(f64, @floatFromInt(both)) / @as(f64, @floatFromInt(either));
}

fn occupied(rows: []const []const u8, x: usize, y: usize) bool {
    if (y >= rows.len or 2 * x + 2 > rows[y].len) return false;
    return !std.mem.eql(u8, rows[y][2 * x ..][0..2], "00");
}

/// Min x, min y, max x, max y (exclusive) of the occupied cells.
fn occupiedBox(rows: []const []const u8) [4]usize {
    var box = [4]usize{ std.math.maxInt(usize), std.math.maxInt(usize), 0, 0 };
    for (rows, 0..) |row, y| for (0..row.len / 2) |x| {
        if (!occupied(rows, x, y)) continue;
        box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x + 1), @max(box[3], y + 1) };
    };
    return if (box[0] > box[2]) .{ 0, 0, 0, 0 } else box;
}

// ---------------------------------------------------------------------------
// inventory: which side of its building each variant shows

/// The variant's intact sprite at a quarter of its size (variants of one
/// building differ in fine detail, not in the layout of their walls), the
/// footprint's front corner on it and the two visible walls' lengths: under
/// the 2003 camera the wall facing -y runs up-left from the front corner and
/// the wall facing +x up-right, 16 px a cell.
pub const View = struct { sprite: Image, front: [2]i64, walls: [2]i64, passability: []const []const u8 };

pub const Wall = enum(u1) { left, right };

const view_scale = 4;

pub fn loadView(gpa: std.mem.Allocator, io: std.Io, dir: []const u8) !View {
    const cwd = std.Io.Dir.cwd();
    const limit: std.Io.Limit = .limited(256 * 1024 * 1024);
    const pack = try decodePack(gpa, try cwd.readFileAlloc(io, try std.fs.path.join(gpa, &.{ dir, "1.san" }), gpa, limit));
    const atlas = try readDds(gpa, try cwd.readFileAlloc(io, try std.fs.path.join(gpa, &.{ dir, "1_h.dds" }), gpa, limit));
    const stats = try readStats(gpa, try cwd.readFileAlloc(io, try std.fs.path.join(gpa, &.{ dir, "1.xml" }), gpa, limit));
    if (pack.sprites.len == 0) return error.NoSprite;
    var bounds: Bounds = .{};
    for (pack.sprites[0].squares) |square| bounds.add(square);
    const full = try unpack(gpa, pack.sprites[0], atlas, bounds);
    const sprite: Image = .{ .width = full.width / view_scale, .height = full.height / view_scale, .pixels = try gpa.alloc([4]u8, (full.width / view_scale) * (full.height / view_scale)) };
    for (0..sprite.height) |y| for (0..sprite.width) |x| {
        var sum = [4]u32{ 0, 0, 0, 0 };
        for (0..view_scale) |dy| for (0..view_scale) |dx| for (0..4) |c| {
            sum[c] += full.pixels[(y * view_scale + dy) * full.width + x * view_scale + dx][c];
        };
        for (0..4) |c| sprite.pixels[y * sprite.width + x][c] = @intCast(sum[c] / (view_scale * view_scale));
    };
    const box = occupiedBox(stats.passability);
    const corner = mfc_camera.project(.{
        @as(f64, @floatFromInt(box[2])) * tile_size - stats.origin[0],
        @as(f64, @floatFromInt(box[1])) * tile_size - stats.origin[1],
        0,
    });
    return .{
        .sprite = sprite,
        .front = .{ @intFromFloat(@round((corner[0] - @as(f64, @floatFromInt(bounds.min_x))) / view_scale)), @intFromFloat(@round((corner[1] - @as(f64, @floatFromInt(bounds.min_y))) / view_scale)) },
        .walls = .{ @intCast((box[2] - box[0]) * 16 / view_scale), @intCast((box[3] - box[1]) * 16 / view_scale) },
        .passability = stats.passability,
    };
}

/// How alike two walls look: the best normalised cross-correlation of their
/// brightness, each wall sheared so its ground line is level, over shifts of
/// up to two pixels. The light on the left and the right wall differs, which
/// the correlation ignores.
pub fn wallScore(a: View, a_wall: Wall, b: View, b_wall: Wall) f64 {
    var best: f64 = 0;
    var dx: i64 = -2;
    while (dx <= 2) : (dx += 1) {
        var dy: i64 = -2;
        while (dy <= 2) : (dy += 1) best = @max(best, correlate(a, a_wall, b, b_wall, dx, dy));
    }
    return best;
}

/// The pixel of column i (0 at the wall's left end seen from outside) and
/// row r from the ground line (r <= 0 is the wall and what stands on it).
fn wallPixel(view: View, wall: Wall, i: i64, r: i64) ?[4]u8 {
    const w = view.walls[@backingInt(wall)];
    const x = if (wall == .left) view.front[0] - w + i else view.front[0] + i;
    const ground = if (wall == .left) view.front[1] - @divFloor(w - i, 2) else view.front[1] - @divFloor(i, 2);
    const pixel = (view.sprite.at(x, ground + r) orelse return null).*;
    return if (pixel[3] >= 128) pixel else null;
}

fn correlate(a: View, a_wall: Wall, b: View, b_wall: Wall, dx: i64, dy: i64) f64 {
    var n: f64 = 0;
    var sum = [5]f64{ 0, 0, 0, 0, 0 };
    const width = @min(a.walls[@backingInt(a_wall)], b.walls[@backingInt(b_wall)]);
    var i: i64 = 0;
    while (i < width) : (i += 1) {
        var r: i64 = -@as(i64, a.sprite.height);
        while (r <= 1) : (r += 1) {
            const x = luma(wallPixel(a, a_wall, i, r) orelse continue);
            const y = luma(wallPixel(b, b_wall, i + dx, r + dy) orelse continue);
            n += 1;
            sum = .{ sum[0] + x, sum[1] + y, sum[2] + x * x, sum[3] + y * y, sum[4] + x * y };
        }
    }
    // Too little overlap says nothing.
    if (n < @as(f64, @floatFromInt(width)) * 4) return 0;
    const va = sum[2] - sum[0] * sum[0] / n;
    const vb = sum[3] - sum[1] * sum[1] / n;
    if (va <= 0 or vb <= 0) return 0;
    return (sum[4] - sum[0] * sum[1] / n) / @sqrt(va * vb);
}

fn luma(p: [4]u8) f64 {
    return 0.299 * @as(f64, @floatFromInt(p[0])) + 0.587 * @as(f64, @floatFromInt(p[1])) + 0.114 * @as(f64, @floatFromInt(p[2]));
}

/// The outlines' overlap with the front corners on one pixel.
pub fn outlineIou(a: View, b: View) f64 {
    var both: u64 = 0;
    var either: u64 = 0;
    for ([2]View{ a, b }, [2]View{ b, a }, 0..) |one, other, pass| {
        for (0..one.sprite.height) |y| for (0..one.sprite.width) |x| {
            if (one.sprite.pixels[y * one.sprite.width + x][3] < 128) continue;
            const at = other.sprite.at(@as(i64, @intCast(x)) - one.front[0] + other.front[0], @as(i64, @intCast(y)) - one.front[1] + other.front[1]);
            const in_other = if (at) |p| p[3] >= 128 else false;
            // A pixel in both is counted once, from a.
            if (pass == 0 and in_other) both += 1;
            if (pass == 0 or !in_other) either += 1;
        };
    }
    return if (either == 0) 0 else @as(f64, @floatFromInt(both)) / @as(f64, @floatFromInt(either));
}

/// Two variants show the same side (of one building, or of two that differ
/// in textures or details) when both visible walls look alike and their
/// outlines agree.
pub fn sameView(a: View, b: View) bool {
    return @min(wallScore(a, .left, b, .left), wallScore(a, .right, b, .right)) >= 0.5 and outlineIou(a, b) >= 0.75;
}

pub const Turn = struct { turn: ?u2, how: []const u8 };

/// The quarter turns from variant a to variant b. The footprints decide
/// whether the turn is even or odd, unless they fit both ways about as well;
/// then the outlines do (a half turn keeps a symmetric building's outline, a
/// quarter turn does not). An even turn that is not the same view is a half
/// turn. An odd one is the footprints' if they tell it, else the shared
/// wall's: one turn shows a's left wall as b's right wall.
pub fn measureTurn(gpa: std.mem.Allocator, a: View, b: View) !Turn {
    if (sameView(a, b)) return .{ .turn = 0, .how = "same view" };
    var fit: [4]f64 = undefined;
    for (&fit, 0..) |*f, k| f.* = try footprintIou(gpa, a.passability, b.passability, @intCast(k));
    const even = @max(fit[0], fit[2]);
    const odd = @max(fit[1], fit[3]);
    const by_footprint = @abs(even - odd) >= 0.05;
    const is_odd = if (by_footprint) odd > even else outlineIou(a, b) < 0.75;
    if (!is_odd) return .{ .turn = 2, .how = if (by_footprint) "footprint" else "outline" };
    if (@abs(fit[1] - fit[3]) >= 0.05) return .{ .turn = if (fit[1] > fit[3]) 1 else 3, .how = "footprint" };
    const lr = wallScore(a, .left, b, .right);
    const rl = wallScore(a, .right, b, .left);
    if (@abs(lr - rl) >= 0.1) return .{ .turn = if (lr > rl) 1 else 3, .how = "walls" };
    return .{ .turn = null, .how = "unresolved" };
}

/// A variant folder's name: `<family>_<side>` or `<family>_<side>_<colourway>`
/// with one digit each, or a bare family.
pub const Name = struct { family: []const u8, suffix: ?u8 = null, colourway: ?u8 = null };

pub fn parseName(name: []const u8) Name {
    var out: Name = .{ .family = name };
    const last = digitSuffix(name) orelse return out;
    out.family = name[0 .. name.len - 2];
    out.suffix = last;
    if (digitSuffix(out.family)) |side| {
        out.family = out.family[0 .. out.family.len - 2];
        out.colourway = last;
        out.suffix = side;
    }
    return out;
}

fn digitSuffix(name: []const u8) ?u8 {
    if (name.len < 3 or name[name.len - 2] != '_' or name[name.len - 1] < '1' or name[name.len - 1] > '9') return null;
    return name[name.len - 1] - '0';
}

pub const Variant = struct {
    /// Under the buildings folder, with forward slashes.
    path: []const u8,
    suffix: ?u8,
    colourway: ?u8,
    winter: bool,
    side: ?u8 = null,
    how: []const u8 = "",
    view_of: ?[]const u8 = null,
};

pub const Family = struct { name: []const u8, region: []const u8, variants: []Variant };

/// Every variant folder (one holding `1.san`) under `root`/<region>/<season>,
/// or one level deeper (`dot/01`), grouped into families in name order.
pub fn listFamilies(gpa: std.mem.Allocator, io: std.Io, root: []const u8) ![]Family {
    const Found = struct { rel: []const u8, name: Name, winter: bool };
    const Group = struct { name: []const u8, variants: std.ArrayList(Variant) };
    var families: std.ArrayList(Family) = .empty;
    for (try entries(gpa, io, root, .directory)) |region| {
        const region_path = try std.fs.path.join(gpa, &.{ root, region });
        for (try entries(gpa, io, region_path, .directory)) |season| {
            const season_path = try std.fs.path.join(gpa, &.{ region_path, season });
            var groups: std.ArrayList(Group) = .empty;
            for (try entries(gpa, io, season_path, .directory)) |folder| {
                const folder_path = try std.fs.path.join(gpa, &.{ season_path, folder });
                const files = try entries(gpa, io, folder_path, .file);
                var found: std.ArrayList(Found) = .empty;
                if (hasName(files, "1.san")) {
                    try found.append(gpa, .{ .rel = folder, .name = parseName(folder), .winter = hasName(files, "1w.san") });
                } else for (try entries(gpa, io, folder_path, .directory)) |sub| {
                    const sub_files = try entries(gpa, io, try std.fs.path.join(gpa, &.{ folder_path, sub }), .file);
                    if (!hasName(sub_files, "1.san")) continue;
                    try found.append(gpa, .{ .rel = try std.fmt.allocPrint(gpa, "{s}/{s}", .{ folder, sub }), .name = .{ .family = folder }, .winter = hasName(sub_files, "1w.san") });
                }
                for (found.items) |item| {
                    const group = for (groups.items) |*g| {
                        if (std.mem.eql(u8, g.name, item.name.family)) break g;
                    } else blk: {
                        try groups.append(gpa, .{ .name = item.name.family, .variants = .empty });
                        break :blk &groups.items[groups.items.len - 1];
                    };
                    try group.variants.append(gpa, .{
                        .path = try std.fmt.allocPrint(gpa, "{s}/{s}/{s}", .{ region, season, item.rel }),
                        .suffix = item.name.suffix,
                        .colourway = item.name.colourway,
                        .winter = item.winter,
                    });
                }
            }
            for (groups.items) |g| try families.append(gpa, .{ .name = g.name, .region = region, .variants = g.variants.items });
        }
    }
    std.mem.sort(Family, families.items, {}, struct {
        fn less(_: void, a: Family, b: Family) bool {
            const order = std.mem.order(u8, a.region, b.region);
            return order == .lt or (order == .eq and std.mem.lessThan(u8, a.name, b.name));
        }
    }.less);
    return families.items;
}

fn entries(gpa: std.mem.Allocator, io: std.Io, path: []const u8, kind: std.Io.File.Kind) ![][]const u8 {
    var dir = try std.Io.Dir.cwd().openDir(io, path, .{ .iterate = true });
    defer dir.close(io);
    var names: std.ArrayList([]const u8) = .empty;
    var iterator = dir.iterate();
    while (try iterator.next(io)) |entry| {
        if (entry.kind == kind) try names.append(gpa, try gpa.dupe(u8, entry.name));
    }
    std.mem.sort([]const u8, names.items, {}, lessThan);
    return names.items;
}

fn hasName(names: []const []const u8, name: []const u8) bool {
    for (names) |n| if (std.ascii.eqlIgnoreCase(n, name)) return true;
    return false;
}

/// Gives every variant of the family its side. The first variant is the
/// reference: its suffix, or side 1 without one. A later variant that shows
/// the same view as an earlier one takes that one's side; any other is the
/// reference turned as measured.
pub fn placeFamily(gpa: std.mem.Allocator, io: std.Io, root: []const u8, family: Family) !void {
    const first = &family.variants[0];
    first.side = first.suffix orelse 1;
    first.how = if (family.variants.len > 1) "reference" else if (first.suffix != null) "suffix" else "single";
    if (family.variants.len == 1) return;
    const views = try gpa.alloc(View, family.variants.len);
    for (family.variants, views) |variant, *view| view.* = try loadView(gpa, io, try std.fs.path.join(gpa, &.{ root, variant.path }));
    for (family.variants[1..], 1..) |*variant, i| {
        for (0..i) |j| {
            if (!sameView(views[j], views[i])) continue;
            variant.side = family.variants[j].side;
            variant.how = "same view";
            variant.view_of = family.variants[j].path;
            break;
        } else {
            const turn = try measureTurn(gpa, views[0], views[i]);
            variant.how = turn.how;
            if (turn.turn) |t| variant.side = (first.side.? - 1 + @as(u8, t)) % 4 + 1;
        }
    }
}

fn missingSides(family: Family) [4]bool {
    var missing = [4]bool{ true, true, true, true };
    for (family.variants) |variant| {
        if (variant.side) |side| missing[side - 1] = false;
    }
    return missing;
}

fn inventoryCommand(ctx: *bk_tool.Context, root_arg: []const u8, out_arg: []const u8) !void {
    const arena = ctx.arena;
    const io = ctx.io;
    const root = try bk_tool.inputPath(arena, root_arg);
    const out_path = try bk_tool.inputPath(arena, out_arg);
    const families = listFamilies(arena, io, root) catch |err| return ctx.failKey("views.err.read", .{ root, @errorName(err) });
    if (families.len == 0) return ctx.failKey("views.err.no_san", .{root});
    var variants: usize = 0;
    for (families) |family| {
        placeFamily(arena, io, root, family) catch |err| return ctx.failKey("views.err.read", .{ family.name, @errorName(err) });
        variants += family.variants.len;
    }

    var json: std.Io.Writer.Allocating = .init(arena);
    const j = &json.writer;
    try j.writeAll("{\"families\":[");
    for (families, 0..) |family, f| {
        try j.print("{s}\n{{\"family\":\"{s}\",\"region\":\"{s}\",\"missing\":[", .{ if (f > 0) "," else "", family.name, family.region });
        try writeSides(j, missingSides(family), ",");
        try j.writeAll("],\"variants\":[");
        for (family.variants, 0..) |v, i| {
            try j.print("{s}\n {{\"path\":\"{s}\",\"suffix\":{?d},\"colourway\":{?d},\"side\":{?d},\"how\":\"{s}\",\"view_of\":", .{ if (i > 0) "," else "", v.path, v.suffix, v.colourway, v.side, v.how });
            if (v.view_of) |of| try j.print("\"{s}\"", .{of}) else try j.writeAll("null");
            try j.print(",\"winter\":{}}}", .{v.winter});
        }
        try j.writeAll("]}");
    }
    try j.writeAll("]}\n");

    var md: std.Io.Writer.Allocating = .init(arena);
    const m = &md.writer;
    try m.print(
        \\# Building inventory and side convention
        \\
        \\Written by `bk-views inventory Data/Buildings <out>` (`tools/zig/bk_views.zig`), the same data as
        \\`tools/zig/fixtures/bk-views/inventory.json`; `test-bk-views` fails when either differs from what the
        \\tool writes. {d} families, {d} variant folders.
        \\
        \\**Sides.** Side k + 1 is side k turned by the quarter turn that takes world (x, y) to (-y, x) about the
        \\zero cross: the footprint's front corner goes to the right corner, the left wall becomes the right
        \\wall and every 1.xml Direction grows by 90 (`turnPoint`, `turnGrid`, `turnXml`). A suffix `_1` .. `_4`
        \\names that side where the variants of a family are turns of one building; `_1_2` is colourway 2 of
        \\side 1; a folder without a suffix, or a numbered subfolder (`dot/01`), starts at side 1. Each
        \\variant's side is measured against the family's first variant (`how`): `same view` when both visible
        \\walls correlate at 0.5 or more and the outlines overlap 0.75 or more (it repeats that view with other
        \\textures or details); otherwise the footprints say whether the turn is even or odd (`outline` when
        \\they fit both ways and the outlines decide), an even turn is a half turn, and an odd one goes to the
        \\footprint that fits better (`footprint`), else to the wall the two views share (`walls`);
        \\`unresolved` when nothing tells.
        \\
        \\| Family | Region | Variants (folder under Data/Buildings: side, how) | Missing sides | Colourways | Winter |
        \\|---|---|---|---|---|---|
        \\
    , .{ families.len, variants });
    for (families) |family| {
        try m.print("| {s} | {s} | ", .{ family.name, family.region });
        for (family.variants, 0..) |v, i| {
            try m.print("{s}`{s}`: ", .{ if (i > 0) "; " else "", v.path });
            if (v.side) |side| try m.print("{d}", .{side}) else try m.writeAll("?");
            if (i > 0) try m.print(" ({s})", .{v.how});
        }
        try m.writeAll(" | ");
        try writeSides(m, missingSides(family), ", ");
        try m.writeAll(" | ");
        var colourways: usize = 0;
        for (family.variants) |v| if (v.colourway != null) {
            try m.print("{s}`{s}`", .{ if (colourways > 0) ", " else "", std.fs.path.basename(v.path) });
            colourways += 1;
        };
        if (colourways == 0) try m.writeAll("-");
        var winter: usize = 0;
        for (family.variants) |v| winter += @intFromBool(v.winter);
        try m.print(" | {s} |\n", .{if (winter == 0) "no" else if (winter == family.variants.len) "yes" else "partly"});
    }
    try m.writeAll("\n## Where the measured side is not the suffix\n\n");
    for (families) |family| for (family.variants) |v| {
        if (v.suffix == null or (v.side != null and v.side.? == v.suffix.?)) continue;
        try m.print("- `{s}`: suffix {d}, measured ", .{ v.path, v.suffix.? });
        if (v.side) |side| try m.print("side {d} ({s}", .{ side, v.how }) else try m.print("nothing ({s}", .{v.how});
        if (v.view_of) |of| try m.print(" as `{s}`", .{of});
        try m.writeAll(")\n");
    };

    try std.Io.Dir.cwd().createDirPath(io, out_path);
    try writeOut(ctx, out_path, "inventory.json", json.written());
    try writeOut(ctx, out_path, "inventory.md", md.written());
    try ctx.out.print("wrote {s}: {d} families, {d} variants\n", .{ out_path, families.len, variants });
}

fn writeSides(w: *std.Io.Writer, sides: [4]bool, separator: []const u8) !void {
    var first = true;
    for (sides, 1..) |on, side| if (on) {
        try w.print("{s}{d}", .{ if (first) "" else separator, side });
        first = false;
    };
}

// ---------------------------------------------------------------------------
// side: a view of a building that no 2003 picture shows

/// A 2003 picture of the building in the frame of the variant being turned:
/// the picture's own side is `turn` quarter turns from that variant's, and
/// its zero cross sits `shift` (world units, in the picture's own frame) away.
pub const Eye = struct {
    anchor: [2]f64,
    turn: u2 = 0,
    shift: [2]f64 = .{ 0, 0 },

    pub fn project(eye: Eye, p: [3]f64) [2]f64 {
        const q = turnPoint(.{ p[0], p[1] }, eye.turn);
        const s = mfc_camera.project(.{ q[0] + eye.shift[0], q[1] + eye.shift[1], p[2] });
        return .{ s[0] + eye.anchor[0], s[1] + eye.anchor[1] };
    }

    /// The point of the picture plane through the zero cross under picture
    /// coordinate (x, y), and the direction the eye looks in.
    pub fn ray(eye: Eye, x: f64, y: f64) [2][3]f64 {
        const r = mfc_camera.rows;
        const s = [2]f64{ x - eye.anchor[0], y - eye.anchor[1] };
        const q = [3]f64{ s[0] * r[0][0] + s[1] * r[1][0] - eye.shift[0], s[0] * r[0][1] + s[1] * r[1][1] - eye.shift[1], s[0] * r[0][2] + s[1] * r[1][2] };
        const d = [3]f64{ r[0][1] * r[1][2] - r[0][2] * r[1][1], r[0][2] * r[1][0] - r[0][0] * r[1][2], r[0][0] * r[1][1] - r[0][1] * r[1][0] };
        const back: u2 = 0 -% eye.turn;
        const p = turnPoint(.{ q[0], q[1] }, back);
        const e = turnPoint(.{ d[0], d[1] }, back);
        return .{ .{ p[0], p[1], q[2] }, .{ e[0], e[1], d[2] } };
    }
};

/// The building as voxels of `size` world units over the target variant's
/// footprint grown by one cell, up to `max_height`.
pub const Hull = struct {
    lo: [3]f64,
    size: f64,
    n: [3]usize,
    cells: []bool,

    pub fn center(hull: Hull, i: usize, j: usize, l: usize) [3]f64 {
        return .{
            hull.lo[0] + (@as(f64, @floatFromInt(i)) + 0.5) * hull.size,
            hull.lo[1] + (@as(f64, @floatFromInt(j)) + 0.5) * hull.size,
            hull.lo[2] + (@as(f64, @floatFromInt(l)) + 0.5) * hull.size,
        };
    }

    pub fn get(hull: Hull, i: isize, j: isize, l: isize) bool {
        if (i < 0 or j < 0 or l < 0 or i >= hull.n[0] or j >= hull.n[1] or l >= hull.n[2]) return false;
        return hull.cells[(@as(usize, @intCast(l)) * hull.n[1] + @as(usize, @intCast(j))) * hull.n[0] + @as(usize, @intCast(i))];
    }

    pub fn count(hull: Hull) usize {
        var n: usize = 0;
        for (hull.cells) |c| n += @intFromBool(c);
        return n;
    }
};

pub const max_height: f64 = 400;

/// The visual hull: every voxel over the footprint whose centre lies inside
/// the outline (alpha 128 or more) of every picture, on a kept ground voxel.
pub fn carve(gpa: std.mem.Allocator, footprint: Grid, size: f64, pictures: []const Image, eyes: []const Eye) !Hull {
    const box = occupiedBox(footprint.rows);
    const lo = [3]f64{
        (@as(f64, @floatFromInt(box[0])) - 1) * tile_size - footprint.origin[0],
        (@as(f64, @floatFromInt(box[1])) - 1) * tile_size - footprint.origin[1],
        0,
    };
    const n = [3]usize{
        @intFromFloat(@ceil(@as(f64, @floatFromInt(box[2] - box[0] + 2)) * tile_size / size)),
        @intFromFloat(@ceil(@as(f64, @floatFromInt(box[3] - box[1] + 2)) * tile_size / size)),
        @intFromFloat(@ceil(max_height / size)),
    };
    const hull: Hull = .{ .lo = lo, .size = size, .n = n, .cells = try gpa.alloc(bool, n[0] * n[1] * n[2]) };
    for (0..n[2]) |l| for (0..n[1]) |j| for (0..n[0]) |i| {
        const p = hull.center(i, j, l);
        const inside = nearFootprint(footprint, p) and for (pictures, eyes) |picture, eye| {
            const q = eye.project(p);
            const pixel = picture.at(@intFromFloat(@floor(q[0])), @intFromFloat(@floor(q[1]))) orelse break false;
            if (pixel[3] < 128) break false;
        } else true;
        hull.cells[(l * n[1] + j) * n[0] + i] = inside;
    };
    // Seen from 30 degrees up, a wall facing the eye hides what stands in
    // front of it except on the ground, where the outline's lower edge ends;
    // so a column stands only where its lowest voxel is kept.
    for (0..n[1]) |j| for (0..n[0]) |i| {
        if (hull.cells[j * n[0] + i]) continue;
        for (0..n[2]) |l| hull.cells[(l * n[1] + j) * n[0] + i] = false;
    };
    return hull;
}

/// Within one cell of an occupied cell: the grids are painted by hand and
/// eaves and steps reach past them.
fn nearFootprint(footprint: Grid, p: [3]f64) bool {
    const cx: i64 = @intFromFloat(@floor((p[0] + footprint.origin[0]) / tile_size));
    const cy: i64 = @intFromFloat(@floor((p[1] + footprint.origin[1]) / tile_size));
    var dy: i64 = -1;
    while (dy <= 1) : (dy += 1) {
        var dx: i64 = -1;
        while (dx <= 1) : (dx += 1) {
            const x = cx + dx;
            const y = cy + dy;
            if (x >= 0 and y >= 0 and occupied(footprint.rows, @intCast(x), @intCast(y))) return true;
        }
    }
    return false;
}

/// Where each other picture's zero cross sits: first the footprint boxes laid
/// on each other, then the shift (in steps of 4 down to half a world unit)
/// that keeps the most voxels in the hull, one picture at a time.
pub fn alignEyes(gpa: std.mem.Allocator, fixed: usize, footprints: []const Grid, pictures: []const Image, eyes: []Eye) !void {
    const footprint = footprints[fixed];
    for (eyes, footprints, 0..) |*eye, grid, e| {
        if (e == fixed) continue;
        const a = occupiedBox(footprint.rows);
        const b = occupiedBox(grid.rows);
        const ca = turnPoint(.{
            @as(f64, @floatFromInt(a[0] + a[2])) * tile_size / 2 - footprint.origin[0],
            @as(f64, @floatFromInt(a[1] + a[3])) * tile_size / 2 - footprint.origin[1],
        }, eye.turn);
        const cb = [2]f64{
            @as(f64, @floatFromInt(b[0] + b[2])) * tile_size / 2 - grid.origin[0],
            @as(f64, @floatFromInt(b[1] + b[3])) * tile_size / 2 - grid.origin[1],
        };
        eye.shift = .{ cb[0] - ca[0], cb[1] - ca[1] };
    }
    var arena_state = std.heap.ArenaAllocator.init(gpa);
    defer arena_state.deinit();
    for (0..2) |_| for (eyes, 0..) |*eye, e| {
        if (e == fixed) continue;
        for ([_]f64{ 4, 2, 1, 0.5 }) |step| {
            while (true) {
                const start = eye.shift;
                var best = (try carve(arena_state.allocator(), footprint, 3, pictures, eyes)).count();
                var best_shift = start;
                _ = arena_state.reset(.retain_capacity);
                for ([_][2]f64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 } }) |d| {
                    eye.shift = .{ start[0] + d[0] * step, start[1] + d[1] * step };
                    const volume = (try carve(arena_state.allocator(), footprint, 3, pictures, eyes)).count();
                    _ = arena_state.reset(.retain_capacity);
                    if (volume > best) {
                        best = volume;
                        best_shift = eye.shift;
                    }
                }
                eye.shift = best_shift;
                if (best_shift[0] == start[0] and best_shift[1] == start[1]) break;
            }
        }
    };
}

/// The family of the variant folder `dir` with every variant's measured side,
/// and the buildings folder the variants' paths start in.
const InFamily = struct { root: []const u8, family: Family, index: usize };

fn findFamily(gpa: std.mem.Allocator, io: std.Io, dir: []const u8) !InFamily {
    // <root>/<region>/<season>/<variant>, or one level deeper for a numbered subfolder.
    var root: []const u8 = dir;
    for (0..4) |depth| {
        root = std.fs.path.dirname(root) orelse break;
        if (depth < 2) continue;
        const families = listFamilies(gpa, io, root) catch continue;
        for (families) |family| for (family.variants, 0..) |variant, i| {
            const path = try std.fs.path.join(gpa, &.{ root, variant.path });
            if (!std.ascii.eqlIgnoreCase(try std.fs.path.resolve(gpa, &.{path}), try std.fs.path.resolve(gpa, &.{dir}))) continue;
            try placeFamily(gpa, io, root, family);
            return .{ .root = root, .family = family, .index = i };
        };
    }
    return error.NotAVariant;
}

/// A variant's 2003 pictures, its 1.xml and its side.
const Source = struct { dir: []const u8, side: u8, xml: []const u8, footprint: Grid, names: []const []const u8, pictures: []Image, anchor: [2]i32 };

fn loadSource(ctx: *bk_tool.Context, dir: []const u8, side: u8) !Source {
    const arena = ctx.arena;
    const packs = try readPacks(ctx, dir);
    var names: std.ArrayList([]const u8) = .empty;
    var pictures: std.ArrayList(Image) = .empty;
    for (packs.files) |file| {
        if (file.pack.sprites.len == 0) continue;
        try names.append(arena, file.name);
        try pictures.append(arena, try unpack(arena, file.pack.sprites[0], file.atlas, packs.bounds));
    }
    const xml = try readFile(ctx, try std.fs.path.join(arena, &.{ dir, "1.xml" }));
    return .{
        .dir = dir,
        .side = side,
        .xml = xml,
        .footprint = xmlGrid(arena, xml, "passability", "origin") catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) }),
        .names = names.items,
        .pictures = pictures.items,
        .anchor = .{ -packs.bounds.min_x, -packs.bounds.min_y },
    };
}

fn pictureOf(source: Source, name: []const u8) ?Image {
    for (source.names, source.pictures) |n, p| if (std.mem.eql(u8, n, name)) return p;
    return null;
}

/// The hull's outer faces as quads over shared corners, for Blender.
pub const Mesh = struct { verts: []f32, quads: []u32 };

pub fn hullMesh(gpa: std.mem.Allocator, hull: Hull) !Mesh {
    var corners: std.AutoHashMapUnmanaged(u64, u32) = .empty;
    defer corners.deinit(gpa);
    var verts: std.ArrayList(f32) = .empty;
    var quads: std.ArrayList(u32) = .empty;
    for (0..hull.n[2]) |l| for (0..hull.n[1]) |j| for (0..hull.n[0]) |i| {
        const cell = [3]isize{ @intCast(i), @intCast(j), @intCast(l) };
        if (!hull.get(cell[0], cell[1], cell[2])) continue;
        for (0..3) |a| for ([2]isize{ -1, 1 }) |s| {
            var next = cell;
            next[a] += s;
            if (hull.get(next[0], next[1], next[2])) continue;
            // The face's corners counter-clockwise seen from outside: axes b and c follow a.
            const b = (a + 1) % 3;
            const c = (a + 2) % 3;
            const order: [4][2]u1 = if (s > 0) .{ .{ 0, 0 }, .{ 1, 0 }, .{ 1, 1 }, .{ 0, 1 } } else .{ .{ 0, 0 }, .{ 0, 1 }, .{ 1, 1 }, .{ 1, 0 } };
            for (order) |o| {
                var corner = [3]usize{ i, j, l };
                if (s > 0) corner[a] += 1;
                corner[b] += o[0];
                corner[c] += o[1];
                const key = (@as(u64, corner[2]) * (hull.n[1] + 1) + corner[1]) * (hull.n[0] + 1) + corner[0];
                const entry = try corners.getOrPut(gpa, key);
                if (!entry.found_existing) {
                    entry.value_ptr.* = @intCast(verts.items.len / 3);
                    for (0..3) |k| try verts.append(gpa, @floatCast(hull.lo[k] + @as(f64, @floatFromInt(corner[k])) * hull.size));
                }
                try quads.append(gpa, entry.value_ptr.*);
            }
        };
    };
    return .{ .verts = verts.items, .quads = quads.items };
}

/// A mirror of the building or the identity: p goes to matrix * p + offset.
pub const Mirror = struct { matrix: [3][3]f64, offset: [3]f64 };

pub const identity: Mirror = .{ .matrix = .{ .{ 1, 0, 0 }, .{ 0, 1, 0 }, .{ 0, 0, 1 } }, .offset = .{ 0, 0, 0 } };

pub fn mirrorPoint(m: Mirror, p: [3]f64) [3]f64 {
    var q: [3]f64 = undefined;
    for (0..3) |r| q[r] = m.matrix[r][0] * p[0] + m.matrix[r][1] * p[1] + m.matrix[r][2] * p[2] + m.offset[r];
    return q;
}

/// A check of a view: does eye `eye` see the hit point moved by `mirror`?
pub const Check = struct { eye: usize, mirror: ?usize };

/// What Blender wrote for one view: the hit point of every pixel (NaN for
/// none) and its flags (bit c: check c sees it; bit 7: the ground there is in
/// shadow).
pub const Buffers = struct { width: u32, height: u32, pos: [][3]f32, flags: []u8 };

pub const shadow_bit: u8 = 128;

/// One Blender view: `eye` (in the target variant's frame) looking at mesh
/// `mesh`, a canvas of `size`.
pub const JobView = struct { mesh: usize, eye: Eye, size: [2]u32, checks: []const Check = &.{}, light: ?[3]f64 = null, out: []const u8 };

pub fn writeJob(w: *std.Io.Writer, meshes: []const [2][]const u8, views: []const JobView, eyes: []const Eye, mirrors: []const Mirror) !void {
    try w.writeAll("{\"tolerance\":6,\"meshes\":[");
    for (meshes, 0..) |mesh, i| try w.print("{s}{{\"verts\":{f},\"quads\":{f}}}", .{ if (i > 0) "," else "", std.json.fmt(mesh[0], .{}), std.json.fmt(mesh[1], .{}) });
    try w.writeAll("],\n\"views\":[");
    for (views, 0..) |view, i| {
        const o = view.eye.ray(0, 0);
        const ux = view.eye.ray(1, 0)[0];
        const vy = view.eye.ray(0, 1)[0];
        const far = 2000.0;
        const d = o[1];
        try w.print("{s}\n{{\"mesh\":{d},\"width\":{d},\"height\":{d},\"out\":{f},", .{ if (i > 0) "," else "", view.mesh, view.size[0], view.size[1], std.json.fmt(view.out, .{}) });
        try w.print("\"origin\":[{d},{d},{d}],\"u\":[{d},{d},{d}],\"v\":[{d},{d},{d}],\"dir\":[{d},{d},{d}]", .{
            o[0][0] - far * d[0], o[0][1] - far * d[1], o[0][2] - far * d[2],
            ux[0] - o[0][0],      ux[1] - o[0][1],      ux[2] - o[0][2],
            vy[0] - o[0][0],      vy[1] - o[0][1],      vy[2] - o[0][2],
            d[0],                 d[1],                 d[2],
        });
        if (view.light) |l| try w.print(",\"light\":[{d},{d},{d}]", .{ l[0], l[1], l[2] });
        try w.writeAll(",\"checks\":[");
        for (view.checks, 0..) |check, c| {
            const m = if (check.mirror) |k| mirrors[k] else identity;
            const e = eyes[check.eye].ray(0, 0)[1];
            try w.print("{s}{{\"matrix\":[{d},{d},{d},{d},{d},{d},{d},{d},{d}],\"offset\":[{d},{d},{d}],\"dir\":[{d},{d},{d}]}}", .{
                if (c > 0) "," else "",
                m.matrix[0][0],
                m.matrix[0][1],
                m.matrix[0][2],
                m.matrix[1][0],
                m.matrix[1][1],
                m.matrix[1][2],
                m.matrix[2][0],
                m.matrix[2][1],
                m.matrix[2][2],
                m.offset[0],
                m.offset[1],
                m.offset[2],
                e[0],
                e[1],
                e[2],
            });
        }
        try w.writeAll("]}");
    }
    try w.writeAll("]}\n");
}

pub fn readBuffers(gpa: std.mem.Allocator, io: std.Io, out: []const u8, size: [2]u32) !Buffers {
    const cwd = std.Io.Dir.cwd();
    const count = @as(usize, size[0]) * size[1];
    const pos_bytes = try cwd.readFileAlloc(io, try std.fmt.allocPrint(gpa, "{s}.pos", .{out}), gpa, .limited(64 * 1024 * 1024));
    const flags = try cwd.readFileAlloc(io, try std.fmt.allocPrint(gpa, "{s}.flags", .{out}), gpa, .limited(64 * 1024 * 1024));
    if (pos_bytes.len != count * 12 or flags.len != count) return error.BadBuffers;
    const pos = try gpa.alloc([3]f32, count);
    @memcpy(std.mem.sliceAsBytes(pos), pos_bytes);
    return .{ .width = size[0], .height = size[1], .pos = pos, .flags = flags };
}

/// Runs Blender headless on the job; its own output is kept beside the job.
fn runBlender(ctx: *bk_tool.Context, job: []const u8) !void {
    const arena = ctx.arena;
    // win-home's Blender 5.1; elsewhere the one on PATH.
    const blender = if (@import("builtin").os.tag == .windows) "C:\\Program Files\\Blender Foundation\\Blender 5.1\\blender.exe" else "blender";
    const script = try bk_tool.inputPath(arena, "tools/blender/bk_views_render.py");
    const result = std.process.run(arena, ctx.io, .{
        .argv = &.{ blender, "-b", "--factory-startup", "--python-exit-code", "1", "--python", script, "--", job },
        .stdout_limit = .limited(16 * 1024 * 1024),
        .stderr_limit = .limited(16 * 1024 * 1024),
    }) catch |err| return ctx.failKey("views.err.blender", .{ job, @errorName(err) });
    if (result.term != .exited or result.term.exited != 0) return ctx.failKey("views.err.blender", .{ job, result.stderr });
}

/// The shading class of a surface with normal `n` seen by an eye turned
/// `turn`: which of -y, +x, +y, -x its horizontal part faces in that eye's
/// frame (0..3), plus 4 for a roof, which faces more up than sideways. The
/// 2003 light is fixed to the camera, so a class is lit alike in every
/// picture.
pub fn shadeClass(n: [3]f64, turn: u2) usize {
    const h = turnPoint(.{ n[0], n[1] }, turn);
    const faces = [4]f64{ -h[1], h[0], h[1], -h[0] };
    var best: usize = 0;
    for (faces, 0..) |f, q| {
        if (f > faces[best]) best = q;
    }
    return best + @as(usize, if (n[2] > 0.5) 4 else 0);
}

/// The hull's outward normal at `p`: away from the filled voxels around it.
pub fn hullNormal(hull: Hull, p: [3]f64) [3]f64 {
    var c: [3]i64 = undefined;
    for (0..3) |k| c[k] = @intFromFloat(@floor((p[k] - hull.lo[k]) / hull.size));
    var sum = [3]f64{ 0, 0, 0 };
    var dz: i64 = -3;
    while (dz <= 3) : (dz += 1) {
        var dy: i64 = -3;
        while (dy <= 3) : (dy += 1) {
            var dx: i64 = -3;
            while (dx <= 3) : (dx += 1) {
                if (!hull.get(@intCast(c[0] + dx), @intCast(c[1] + dy), @intCast(c[2] + dz))) continue;
                sum = .{ sum[0] - @as(f64, @floatFromInt(dx)), sum[1] - @as(f64, @floatFromInt(dy)), sum[2] - @as(f64, @floatFromInt(dz)) };
            }
        }
    }
    const length = @sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
    return if (length == 0) .{ 0, 0, 1 } else .{ sum[0] / length, sum[1] / length, sum[2] / length };
}

/// The mean colour of each shading class over the 2003 pictures, so a face
/// taken from a picture where it was lit as one class can be lit as another.
pub const Means = struct {
    sum: [8][3]f64 = std.mem.zeroes([8][3]f64),
    count: [8]f64 = std.mem.zeroes([8]f64),

    pub fn add(means: *Means, class: usize, pixel: [4]u8) void {
        for (0..3) |c| means.sum[class][c] += @floatFromInt(pixel[c]);
        means.count[class] += 1;
    }

    /// The factor that relights a colour channel from class `from` to class `to`.
    pub fn gain(means: Means, to: usize, from: usize, c: usize) f64 {
        if (to == from or means.count[to] < 100 or means.count[from] < 100) return 1;
        return (means.sum[to][c] / means.count[to]) / @max(1, means.sum[from][c] / means.count[from]);
    }
};

/// Where a pixel of a generated picture comes from.
pub const Kind = enum(u8) { empty, projected, mirrored, filled };

/// One 2003 picture as a projection source: its eye and its picture of the
/// state being drawn (null where the variant lacks it).
pub const Projector = struct { eye: Eye, picture: ?Image };

/// A generated picture with the source of each pixel and its shading class.
pub const Generated = struct { image: Image, kinds: []Kind, classes: []u8 };

/// The target's picture from Blender's buffers: every pixel the hull covers
/// takes the colour of the 2003 picture that sees it (or, through a mirror of
/// the building, sees its mirror image), relit to the target's shading class.
/// A seen pixel beats a mirrored one, a picture that lit the face as the same
/// class beats one that did not, and then the picture facing the surface
/// most squarely wins. Pixels none sees are left for `patchFill`.
pub fn projectPicture(gpa: std.mem.Allocator, hull: Hull, buffers: Buffers, target: Eye, checks: []const Check, mirrors: []const Mirror, projectors: []const Projector, means: Means) !Generated {
    const count = buffers.pos.len;
    const out: Generated = .{
        .image = .{ .width = buffers.width, .height = buffers.height, .pixels = try gpa.alloc([4]u8, count) },
        .kinds = try gpa.alloc(Kind, count),
        .classes = try gpa.alloc(u8, count),
    };
    @memset(out.image.pixels, .{ 0, 0, 0, 0 });
    @memset(out.kinds, .empty);
    @memset(out.classes, 255);
    for (buffers.pos, buffers.flags, 0..) |hit, flags, i| {
        if (std.math.isNan(hit[0])) continue;
        const p = [3]f64{ hit[0], hit[1], hit[2] };
        const n = hullNormal(hull, p);
        const class = shadeClass(n, target.turn);
        out.classes[i] = @intCast(class);
        out.kinds[i] = .filled;
        var best_score: f64 = -1;
        for (checks, 0..) |check, c| {
            if (flags & (@as(u8, 1) << @intCast(c)) == 0) continue;
            const projector = projectors[check.eye];
            const source = projector.picture orelse continue;
            const m = if (check.mirror) |k| mirrors[k] else identity;
            const q = mirrorPoint(m, p);
            var nm = mirrorPoint(.{ .matrix = m.matrix, .offset = .{ 0, 0, 0 } }, n);
            const uv = projector.eye.project(q);
            const pixel = (source.at(@intFromFloat(@floor(uv[0])), @intFromFloat(@floor(uv[1]))) orelse continue).*;
            if (pixel[3] < 128) continue;
            const from = shadeClass(nm, projector.eye.turn);
            const d = projector.eye.ray(0, 0)[1];
            nm = .{ -nm[0], -nm[1], -nm[2] };
            const facing = nm[0] * d[0] + nm[1] * d[1] + nm[2] * d[2];
            const score = @as(f64, if (check.mirror == null) 4 else 0) + @as(f64, if (from == class) 2 else 0) + facing;
            if (score <= best_score) continue;
            best_score = score;
            for (0..3) |ch| out.image.pixels[i][ch] = @intFromFloat(@min(255, @round(@as(f64, @floatFromInt(pixel[ch])) * means.gain(class, from, ch))));
            out.image.pixels[i][3] = 255;
            out.kinds[i] = if (check.mirror == null) .projected else .mirrored;
        }
    }
    return out;
}

/// Fills every `filled` pixel from the picture's own projected and mirrored
/// pixels of the same shading class, PatchMatch-style: pixels are filled from
/// the edge of the hole inwards, each takes the centre of the 7x7 patch that
/// best matches what is already known around it, searched among the
/// neighbours' matches moved along (propagation), random picks of the class,
/// and ever smaller windows around the best so far. Deterministic: a fixed seed.
/// How much better than a neighbour's match moved along another patch must be.
const coherence = 1.25;

pub fn patchFill(gpa: std.mem.Allocator, generated: Generated) !void {
    const image = generated.image;
    const w: i64 = image.width;
    const h: i64 = image.height;
    const count = image.pixels.len;
    // Known: the pixels a 2003 picture gave; done: known or filled already.
    const done = try gpa.alloc(bool, count);
    defer gpa.free(done);
    const match = try gpa.alloc(i64, count);
    defer gpa.free(match);
    @memset(match, -1);
    var pools: [9]std.ArrayList(i64) = @splat(.empty);
    defer for (&pools) |*pool| pool.deinit(gpa);
    const known = try gpa.alloc(bool, count);
    defer gpa.free(known);
    for (generated.kinds, generated.classes, done, known, 0..) |kind, class, *d, *k, i| {
        d.* = kind == .projected or kind == .mirrored;
        k.* = d.*;
        if (!d.*) continue;
        try pools[class].append(gpa, @intCast(i));
        try pools[8].append(gpa, @intCast(i));
    }
    if (pools[8].items.len == 0) return;

    // The holes in order of their distance from what is known (breadth first).
    var order: std.ArrayList(i64) = .empty;
    defer order.deinit(gpa);
    const queued = try gpa.alloc(bool, count);
    defer gpa.free(queued);
    @memcpy(queued, done);
    for (0..count) |i| {
        if (!done[i]) continue;
        try queueNeighbours(gpa, &order, queued, generated.kinds, @intCast(i), w, h);
    }
    var next: usize = 0;
    while (next < order.items.len) : (next += 1) try queueNeighbours(gpa, &order, queued, generated.kinds, order.items[next], w, h);

    var prng = std.Random.DefaultPrng.init(0x5eed);
    const random = prng.random();
    for (order.items) |p| {
        const class = generated.classes[@intCast(p)];
        const pool = if (pools[class].items.len >= 50) pools[class].items else pools[8].items;
        var best: i64 = -1;
        var best_cost = std.math.inf(f64);
        const Search = struct {
            image: Image,
            known: []const bool,
            done: []const bool,
            p: i64,
            w: i64,
            h: i64,
            best: *i64,
            best_cost: *f64,

            /// A candidate other than a neighbour's match must be clearly
            /// better, so neighbouring pixels copy neighbouring pixels.
            fn consider(search: @This(), q: i64, penalty: f64) void {
                if (q < 0 or q >= search.w * search.h or !search.known[@intCast(q)]) return;
                const cost = patchCost(search.image, search.done, search.p, q, search.w, search.h) * penalty;
                if (cost < search.best_cost.*) {
                    search.best_cost.* = cost;
                    search.best.* = q;
                }
            }
        };
        const search: Search = .{ .image = image, .known = known, .done = done, .p = p, .w = w, .h = h, .best = &best, .best_cost = &best_cost };
        // Propagation: a neighbour's match, moved along.
        for ([_][2]i64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 } }) |d| {
            const nx = @mod(p, w) + d[0];
            const ny = @divFloor(p, w) + d[1];
            if (nx < 0 or ny < 0 or nx >= w or ny >= h) continue;
            const m = match[@intCast(ny * w + nx)];
            if (m < 0) continue;
            const mx = @mod(m, w) - d[0];
            const my = @divFloor(m, w) - d[1];
            if (mx < 0 or my < 0 or mx >= w or my >= h) continue;
            search.consider(my * w + mx, 1);
        }
        for (0..24) |_| search.consider(pool[random.uintLessThan(usize, pool.len)], coherence);
        var radius: i64 = 32;
        while (radius >= 1 and best >= 0) : (radius = @divFloor(radius, 2)) {
            const bx = @mod(best, w) + random.intRangeAtMost(i64, -radius, radius);
            const by = @divFloor(best, w) + random.intRangeAtMost(i64, -radius, radius);
            if (bx < 0 or by < 0 or bx >= w or by >= h) continue;
            search.consider(by * w + bx, coherence);
        }
        if (best < 0) best = pool[random.uintLessThan(usize, pool.len)];
        image.pixels[@intCast(p)] = image.pixels[@intCast(best)];
        match[@intCast(p)] = best;
        done[@intCast(p)] = true;
    }
}

fn queueNeighbours(gpa: std.mem.Allocator, order: *std.ArrayList(i64), queued: []bool, kinds: []const Kind, p: i64, w: i64, h: i64) !void {
    for ([_][2]i64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 } }) |d| {
        const x = @mod(p, w) + d[0];
        const y = @divFloor(p, w) + d[1];
        if (x < 0 or y < 0 or x >= w or y >= h) continue;
        const q: usize = @intCast(y * w + x);
        if (queued[q] or kinds[q] != .filled) continue;
        queued[q] = true;
        try order.append(gpa, @intCast(q));
    }
}

/// The mean squared colour difference of the 7x7 patches around `p` and `q`
/// over the pixels known in both; a patch sharing fewer than 6 is no match.
fn patchCost(image: Image, done: []const bool, p: i64, q: i64, w: i64, h: i64) f64 {
    var sum: f64 = 0;
    var n: f64 = 0;
    var dy: i64 = -3;
    while (dy <= 3) : (dy += 1) {
        var dx: i64 = -3;
        while (dx <= 3) : (dx += 1) {
            const px = @mod(p, w) + dx;
            const py = @divFloor(p, w) + dy;
            const qx = @mod(q, w) + dx;
            const qy = @divFloor(q, w) + dy;
            if (px < 0 or py < 0 or px >= w or py >= h or qx < 0 or qy < 0 or qx >= w or qy >= h) continue;
            const a: usize = @intCast(py * w + px);
            const b: usize = @intCast(qy * w + qx);
            if (!done[a] or !done[b]) continue;
            for (0..3) |c| {
                const diff = @as(f64, @floatFromInt(image.pixels[a][c])) - @as(f64, @floatFromInt(image.pixels[b][c]));
                sum += diff * diff;
            }
            n += 1;
        }
    }
    return if (n < 6) std.math.inf(f64) else sum / n;
}

/// The share of the picture's coloured pixels (alpha 128 or more, chroma 20
/// or more) in each 30 degree hue band.
pub fn hueHistogram(image: Image) [12]f64 {
    var bands: [12]f64 = @splat(0);
    var total: f64 = 0;
    for (image.pixels) |p| {
        if (p[3] < 128) continue;
        const r: f64 = @floatFromInt(p[0]);
        const g: f64 = @floatFromInt(p[1]);
        const b: f64 = @floatFromInt(p[2]);
        const max = @max(r, g, b);
        const chroma = max - @min(r, g, b);
        if (chroma < 20) continue;
        const hue = if (max == r) @mod((g - b) / chroma, 6) else if (max == g) (b - r) / chroma + 2 else (r - g) / chroma + 4;
        bands[@as(usize, @intFromFloat(hue * 2)) % 12] += 1;
        total += 1;
    }
    if (total > 0) for (&bands) |*band| {
        band.* /= total;
    };
    return bands;
}

/// Two variants share a colourway when their intact pictures' hue
/// histograms differ by less than 0.1 (L1). Measured on the europe families:
/// 0.014 to 0.03 for variants of one colourway (e_ctownhouse01_2/_4,
/// e_house11_1/_3, e_house01_1/_3), 0.16 and more for another
/// (e_ctownhouse01_1 against _2 and _4, the `_1_2` folders).
pub fn sameColourway(a: Image, b: Image) bool {
    return colourwayDistance(a, b) < 0.1;
}

pub fn colourwayDistance(a: Image, b: Image) f64 {
    const ha = hueHistogram(a);
    const hb = hueHistogram(b);
    var d: f64 = 0;
    for (ha, hb) |x, y| d += @abs(x - y);
    return d;
}

/// The direction towards the light in an eye's own frame for horizontal
/// angle `azimuth` and elevation, in degrees.
pub fn lightDirection(azimuth: f64, elevation: f64) [3]f64 {
    const a = std.math.degreesToRadians(azimuth);
    const e = std.math.degreesToRadians(elevation);
    return .{ @cos(e) * @cos(a), @cos(e) * @sin(a), @sin(e) };
}

/// The light in the target variant's frame for an eye turned `turn`: the 2003
/// light is fixed to the camera, so it turns back with the eye.
pub fn lightFor(light: [3]f64, turn: u2) [3]f64 {
    const h = turnPoint(.{ light[0], light[1] }, 0 -% turn);
    return .{ h[0], h[1], light[2] };
}

/// How well the hull's shadow on the ground, cast towards `light` and drawn
/// outside the sprite, overlaps the 2003 shadow picture of each eye (mean IoU).
pub fn shadowScore(gpa: std.mem.Allocator, hull: Hull, light: [3]f64, eyes: []const Eye, sprites: []const Image, shadows: []const Image) !f64 {
    var total: f64 = 0;
    for (eyes, sprites, shadows) |eye, sprite, shadow| {
        const mask = try shadowMask(gpa, hull, lightFor(light, eye.turn), eye, shadow.width, shadow.height);
        defer gpa.free(mask);
        var both: f64 = 0;
        var either: f64 = 0;
        for (mask, sprite.pixels, shadow.pixels) |m, s, d| {
            const predicted = m and s[3] < 128;
            const real = d[3] >= 128;
            both += @floatFromInt(@intFromBool(predicted and real));
            either += @floatFromInt(@intFromBool(predicted or real));
        }
        total += if (either == 0) 0 else both / either;
    }
    return total / @as(f64, @floatFromInt(eyes.len));
}

/// The ground pixels the hull shades from `light` (in the hull's frame),
/// drawn by moving each surface voxel's corners along the light to the ground.
fn shadowMask(gpa: std.mem.Allocator, hull: Hull, light: [3]f64, eye: Eye, width: u32, height: u32) ![]bool {
    const mask = try gpa.alloc(bool, @as(usize, width) * height);
    @memset(mask, false);
    for (0..hull.n[2]) |l| for (0..hull.n[1]) |j| for (0..hull.n[0]) |i| {
        const x: isize = @intCast(i);
        const y: isize = @intCast(j);
        const z: isize = @intCast(l);
        if (!hull.get(x, y, z) or (hull.get(x + 1, y, z) and hull.get(x - 1, y, z) and hull.get(x, y + 1, z) and hull.get(x, y - 1, z) and hull.get(x, y, z + 1))) continue;
        const c = hull.center(i, j, l);
        for (0..16) |k| {
            const f = [2]f64{ @floatFromInt(k & 3), @floatFromInt(k >> 2) };
            const p = [3]f64{ c[0] + (f[0] - 1.5) * hull.size / 4, c[1] + (f[1] - 1.5) * hull.size / 4, c[2] };
            const g = [3]f64{ p[0] - light[0] * p[2] / light[2], p[1] - light[1] * p[2] / light[2], 0 };
            const q = eye.project(g);
            const px: i64 = @intFromFloat(@floor(q[0]));
            const py: i64 = @intFromFloat(@floor(q[1]));
            if (px >= 0 and py >= 0 and px < width and py < height) mask[@intCast(py * width + px)] = true;
        }
    };
    return mask;
}

/// The light (azimuth, elevation in degrees in the camera's frame) whose
/// shadows of the hull best match the 2003 shadow pictures: every 15 by 10
/// degrees, then every 2.5 around the best.
pub const Light = struct { azimuth: f64, elevation: f64, iou: f64 };

pub fn fitLight(gpa: std.mem.Allocator, hull: Hull, eyes: []const Eye, sprites: []const Image, shadows: []const Image) !Light {
    var best: Light = .{ .azimuth = 0, .elevation = 45, .iou = -1 };
    var a: f64 = 0;
    while (a < 360) : (a += 15) {
        var e: f64 = 10;
        while (e <= 80) : (e += 10) {
            const iou = try shadowScore(gpa, hull, lightDirection(a, e), eyes, sprites, shadows);
            if (iou > best.iou) best = .{ .azimuth = a, .elevation = e, .iou = iou };
        }
    }
    const coarse = best;
    var da: f64 = -12.5;
    while (da <= 12.5) : (da += 2.5) {
        var de: f64 = -7.5;
        while (de <= 7.5) : (de += 2.5) {
            const e = coarse.elevation + de;
            if (e < 5 or e > 85) continue;
            const iou = try shadowScore(gpa, hull, lightDirection(coarse.azimuth + da, e), eyes, sprites, shadows);
            if (iou > best.iou) best = .{ .azimuth = coarse.azimuth + da, .elevation = e, .iou = iou };
        }
    }
    return best;
}

/// The vertical mirror planes under which the hull is most like itself:
/// across x, across y and along both diagonals, each at the position (within
/// 12 world units of the hull's centre) that fits best, with the IoU of the
/// hull and its mirror image.
pub const MirrorFit = struct { mirror: Mirror, iou: f64 };

pub fn fitMirrors(hull: Hull) [4]MirrorFit {
    var centre = [2]f64{ 0, 0 };
    var n: f64 = 0;
    for (0..hull.n[2]) |l| for (0..hull.n[1]) |j| for (0..hull.n[0]) |i| {
        if (!hull.get(@intCast(i), @intCast(j), @intCast(l))) continue;
        const c = hull.center(i, j, l);
        centre = .{ centre[0] + c[0], centre[1] + c[1] };
        n += 1;
    };
    centre = .{ centre[0] / @max(1, n), centre[1] / @max(1, n) };
    const matrices = [4][3][3]f64{
        .{ .{ -1, 0, 0 }, .{ 0, 1, 0 }, .{ 0, 0, 1 } },
        .{ .{ 1, 0, 0 }, .{ 0, -1, 0 }, .{ 0, 0, 1 } },
        .{ .{ 0, 1, 0 }, .{ 1, 0, 0 }, .{ 0, 0, 1 } },
        .{ .{ 0, -1, 0 }, .{ -1, 0, 0 }, .{ 0, 0, 1 } },
    };
    const normals = [4][2]f64{ .{ 1, 0 }, .{ 0, 1 }, .{ @sqrt(0.5), -@sqrt(0.5) }, .{ @sqrt(0.5), @sqrt(0.5) } };
    var fits: [4]MirrorFit = undefined;
    for (matrices, normals, &fits) |m, normal, *fit| {
        fit.iou = -1;
        var shift: f64 = -12;
        while (shift <= 12) : (shift += hull.size / 2) {
            // The plane through c = centre + shift * normal: p' = m (p - c) + c.
            const c = [3]f64{ centre[0] + shift * normal[0], centre[1] + shift * normal[1], 0 };
            var offset: [3]f64 = undefined;
            for (0..3) |r| offset[r] = c[r] - (m[r][0] * c[0] + m[r][1] * c[1] + m[r][2] * c[2]);
            const mirror: Mirror = .{ .matrix = m, .offset = offset };
            const iou = mirrorIou(hull, mirror);
            if (iou > fit.iou) fit.* = .{ .mirror = mirror, .iou = iou };
        }
    }
    return fits;
}

fn mirrorIou(hull: Hull, mirror: Mirror) f64 {
    var both: f64 = 0;
    var filled: f64 = 0;
    for (0..hull.n[2]) |l| for (0..hull.n[1]) |j| for (0..hull.n[0]) |i| {
        if (!hull.get(@intCast(i), @intCast(j), @intCast(l))) continue;
        filled += 1;
        const q = mirrorPoint(mirror, hull.center(i, j, l));
        var cell: [3]isize = undefined;
        for (0..3) |k| cell[k] = @intFromFloat(@floor((q[k] - hull.lo[k]) / hull.size));
        both += @floatFromInt(@intFromBool(hull.get(cell[0], cell[1], cell[2])));
    };
    return if (filled == 0) 0 else both / (2 * filled - both);
}

/// The ground picture of the target: every pixel takes the 2003 ground
/// picture (`g`, drawn flat on the ground) that covers its ground point,
/// the first in the projectors' order.
pub fn projectGround(gpa: std.mem.Allocator, target: Eye, size: [2]u32, projectors: []const Projector) !Generated {
    const count = @as(usize, size[0]) * size[1];
    const out: Generated = .{
        .image = .{ .width = size[0], .height = size[1], .pixels = try gpa.alloc([4]u8, count) },
        .kinds = try gpa.alloc(Kind, count),
        .classes = try gpa.alloc(u8, count),
    };
    @memset(out.image.pixels, .{ 0, 0, 0, 0 });
    @memset(out.kinds, .empty);
    @memset(out.classes, 255);
    for (0..size[1]) |y| for (0..size[0]) |x| {
        const g = groundPoint(target, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
        for (projectors) |projector| {
            const picture = projector.picture orelse continue;
            const uv = projector.eye.project(g);
            const pixel = (picture.at(@intFromFloat(@floor(uv[0])), @intFromFloat(@floor(uv[1]))) orelse continue).*;
            if (pixel[3] == 0) continue;
            out.image.pixels[y * size[0] + x] = pixel;
            out.kinds[y * size[0] + x] = .projected;
            break;
        }
    };
    return out;
}

/// Where the ray of picture point (x, y) meets the ground.
pub fn groundPoint(eye: Eye, x: f64, y: f64) [3]f64 {
    const r = eye.ray(x, y);
    const t = -r[0][2] / r[1][2];
    return .{ r[0][0] + t * r[1][0], r[0][1] + t * r[1][1], 0 };
}

/// A mirror plane is used when the hull and its mirror image overlap this much.
pub const mirror_iou = 0.8;

const Turned = struct { turn: u2, eye: Eye, size: [2]u32, name: []const u8 };

fn sideCommand(ctx: *bk_tool.Context, dir_arg: []const u8, turns_arg: []const u8, out_arg: []const u8) !void {
    const arena = ctx.arena;
    const io = ctx.io;
    const cwd = std.Io.Dir.cwd();
    const dir = try bk_tool.inputPath(arena, dir_arg);
    const out = try bk_tool.inputPath(arena, out_arg);
    var turns: std.ArrayList(u2) = .empty;
    var split = std.mem.splitScalar(u8, turns_arg, ',');
    while (split.next()) |t| {
        const k = std.fmt.parseInt(u2, t, 10) catch return error.Usage;
        if (k == 0) return error.Usage;
        try turns.append(arena, k);
    }

    // The family's measured variants: all of them give the shape, those of
    // the variant's own colourway also give pixels.
    const found = findFamily(arena, io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
    const own_side = found.family.variants[found.index].side orelse return ctx.failKey("views.err.no_side", .{dir});
    var sources: std.ArrayList(Source) = .empty;
    var fixed: usize = 0;
    for (found.family.variants, 0..) |variant, i| {
        const side = variant.side orelse continue;
        if (i == found.index) fixed = sources.items.len;
        try sources.append(arena, try loadSource(ctx, try std.fs.path.join(arena, &.{ found.root, variant.path }), side));
    }
    const n = sources.items.len;
    const eyes = try arena.alloc(Eye, n);
    const footprints = try arena.alloc(Grid, n);
    const outlines = try arena.alloc(Image, n);
    const same = try arena.alloc(bool, n);
    for (sources.items, eyes, footprints, outlines) |source, *eye, *grid, *outline| {
        eye.* = .{ .anchor = .{ @floatFromInt(source.anchor[0]), @floatFromInt(source.anchor[1]) }, .turn = @intCast((source.side + 4 - own_side) % 4) };
        grid.* = source.footprint;
        outline.* = pictureOf(source, "1") orelse return ctx.failKey("views.err.no_san", .{source.dir});
    }
    for (outlines, same) |outline, *s| s.* = sameColourway(outlines[fixed], outline);
    try alignEyes(arena, fixed, footprints, outlines, eyes);

    // The light and the mirrors, from a coarse hull of the intact building.
    const coarse = try carve(arena, footprints[fixed], 4, outlines, eyes);
    var light_eyes: std.ArrayList(Eye) = .empty;
    var light_sprites: std.ArrayList(Image) = .empty;
    var light_shadows: std.ArrayList(Image) = .empty;
    for (sources.items, eyes, outlines) |source, eye, outline| {
        const shadow = pictureOf(source, "1s") orelse continue;
        try light_eyes.append(arena, eye);
        try light_sprites.append(arena, outline);
        try light_shadows.append(arena, shadow);
    }
    const light = try fitLight(arena, coarse, light_eyes.items, light_sprites.items, light_shadows.items);
    const light_dir = lightDirection(light.azimuth, light.elevation);
    const fits = fitMirrors(coarse);
    var projecting: usize = 0;
    for (same) |s| projecting += @intFromBool(s);
    var mirrors: std.ArrayList(Mirror) = .empty;
    for (fits) |fit| {
        // Seven checks fit in a byte beside the shadow bit.
        if (fit.iou >= mirror_iou and projecting * (mirrors.items.len + 2) <= 7) try mirrors.append(arena, fit.mirror);
    }
    var checks: std.ArrayList(Check) = .empty;
    for (same, 0..) |s, e| if (s) try checks.append(arena, .{ .eye = e, .mirror = null });
    for (mirrors.items, 0..) |_, m| for (same, 0..) |s, e| if (s) try checks.append(arena, .{ .eye = e, .mirror = m });

    // A hull per damage state (and season), each from that state's pictures.
    const own = sources.items[fixed];
    const own_name = std.fs.path.basename(own.dir);
    const work = try std.fs.path.join(arena, &.{ out, try std.fmt.allocPrint(arena, "{s}.work", .{own_name}) });
    try cwd.createDirPath(io, work);
    var states: std.ArrayList([]const u8) = .empty;
    for (own.names) |name| {
        const digits = std.mem.indexOfNone(u8, name, "0123456789") orelse name.len;
        if (digits > 0 and (digits == name.len or std.mem.eql(u8, name[digits..], "w"))) try states.append(arena, name);
    }
    const hulls = try arena.alloc(Hull, states.items.len);
    var meshes: std.ArrayList([2][]const u8) = .empty;
    for (states.items, hulls) |state, *hull| {
        var state_eyes: std.ArrayList(Eye) = .empty;
        var state_pictures: std.ArrayList(Image) = .empty;
        for (sources.items, eyes) |source, eye| {
            try state_pictures.append(arena, pictureOf(source, state) orelse continue);
            try state_eyes.append(arena, eye);
        }
        hull.* = try carve(arena, footprints[fixed], 2, state_pictures.items, state_eyes.items);
        const mesh = try hullMesh(arena, hull.*);
        const verts = try std.fs.path.join(arena, &.{ work, try std.fmt.allocPrint(arena, "{s}.verts", .{state}) });
        const quads = try std.fs.path.join(arena, &.{ work, try std.fmt.allocPrint(arena, "{s}.quads", .{state}) });
        try cwd.writeFile(io, .{ .sub_path = verts, .data = std.mem.sliceAsBytes(mesh.verts) });
        try cwd.writeFile(io, .{ .sub_path = quads, .data = std.mem.sliceAsBytes(mesh.quads) });
        try meshes.append(arena, .{ verts, quads });
    }

    // Each turn's canvas holds the hull's box, its shadow and the 2003 ground pictures.
    var targets: std.ArrayList(Turned) = .empty;
    for (turns.items) |k| {
        var eye: Eye = .{ .anchor = .{ 0, 0 }, .turn = k };
        var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
        var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
        const l = lightFor(light_dir, k);
        const hull = hulls[0];
        for (0..8) |c| {
            const p = [3]f64{
                hull.lo[0] + @as(f64, @floatFromInt(if (c & 1 != 0) hull.n[0] else 0)) * hull.size,
                hull.lo[1] + @as(f64, @floatFromInt(if (c & 2 != 0) hull.n[1] else 0)) * hull.size,
                if (c & 4 != 0) hullTop(hull) else 0,
            };
            for ([2][3]f64{ p, .{ p[0] - l[0] * p[2] / l[2], p[1] - l[1] * p[2] / l[2], 0 } }) |q| {
                const s = eye.project(q);
                lo = .{ @min(lo[0], s[0]), @min(lo[1], s[1]) };
                hi = .{ @max(hi[0], s[0]), @max(hi[1], s[1]) };
            }
        }
        for (sources.items, eyes, same) |source, source_eye, s| {
            if (!s) continue;
            for (source.names, source.pictures) |name, picture| {
                if (!std.mem.endsWith(u8, name, "g")) continue;
                for (0..picture.height) |y| for (0..picture.width) |x| {
                    if (picture.pixels[y * picture.width + x][3] == 0) continue;
                    const q = eye.project(groundPoint(source_eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5));
                    lo = .{ @min(lo[0], q[0]), @min(lo[1], q[1]) };
                    hi = .{ @max(hi[0], q[0]), @max(hi[1], q[1]) };
                };
            }
        }
        eye.anchor = .{ 2 - @floor(lo[0]), 2 - @floor(lo[1]) };
        try targets.append(arena, .{
            .turn = k,
            .eye = eye,
            .size = .{ @intFromFloat(@ceil(hi[0]) - @floor(lo[0]) + 4), @intFromFloat(@ceil(hi[1]) - @floor(lo[1]) + 4) },
            .name = try std.fmt.allocPrint(arena, "{s}_r{d}", .{ own_name, k }),
        });
    }

    // One Blender run: every source eye (for the shading classes and the
    // outline check) and every target, per state.
    var views: std.ArrayList(JobView) = .empty;
    for (states.items, 0..) |state, m| {
        for (sources.items, eyes, 0..) |source, eye, e| {
            const picture = pictureOf(source, state) orelse continue;
            try views.append(arena, .{ .mesh = m, .eye = eye, .size = .{ picture.width, picture.height }, .out = try std.fmt.allocPrint(arena, "{s}/{s}_eye{d}", .{ work, state, e }) });
        }
        for (targets.items) |target| try views.append(arena, .{
            .mesh = m,
            .eye = target.eye,
            .size = target.size,
            .checks = checks.items,
            .light = lightFor(light_dir, target.turn),
            .out = try std.fmt.allocPrint(arena, "{s}/{s}_{s}", .{ work, state, target.name }),
        });
    }
    var job: std.Io.Writer.Allocating = .init(arena);
    try writeJob(&job.writer, meshes.items, views.items, eyes, mirrors.items);
    const job_path = try std.fs.path.join(arena, &.{ work, "job.json" });
    try cwd.writeFile(io, .{ .sub_path = job_path, .data = job.written() });
    try runBlender(ctx, job_path);

    // The pictures, the source maps and the packs of each turn.
    // bk-sprite beside bk-views (zig-out/bin), else the one on PATH.
    const exe_dir = try std.process.executableDirPathAlloc(io, arena);
    const beside = try std.fs.path.join(arena, &.{ exe_dir, if (@import("builtin").os.tag == .windows) "bk-sprite.exe" else "bk-sprite" });
    const sprite_tool = if (cwd.access(io, beside, .{})) |_| beside else |_| "bk-sprite";
    for (targets.items) |target| {
        const pictures_dir = try std.fs.path.join(arena, &.{ work, target.name });
        const map_dir = try std.fs.path.join(arena, &.{ out, try std.fmt.allocPrint(arena, "{s}.map", .{target.name}) });
        const final = try std.fs.path.join(arena, &.{ out, target.name });
        const temp = try std.fmt.allocPrint(arena, "{s}.tmp", .{final});
        for ([_][]const u8{ pictures_dir, map_dir, temp }) |path| {
            cwd.deleteTree(io, path) catch {};
            try cwd.createDirPath(io, path);
        }
        var report: std.Io.Writer.Allocating = .init(arena);
        const r = &report.writer;
        try r.print("{{\"variant\":\"{s}\",\"side\":{d},\"turn\":{d},\"new_side\":{d},\n\"light\":{{\"azimuth\":{d:.1},\"elevation\":{d:.1},\"shadow_iou\":{d:.4}}},\n\"mirrors\":[", .{
            own_name, own_side, target.turn, (own_side - 1 + @as(u8, target.turn)) % 4 + 1, light.azimuth, light.elevation, light.iou,
        });
        for (fits, 0..) |fit, i| try r.print("{s}{{\"plane\":\"{s}\",\"iou\":{d:.4},\"used\":{}}}", .{ if (i > 0) "," else "", @as([]const u8, switch (i) {
            0 => "x",
            1 => "y",
            2 => "x=y",
            else => "x=-y",
        }), fit.iou, fit.iou >= mirror_iou });
        try r.writeAll("],\n\"sources\":[");
        for (sources.items, eyes, same, 0..) |source, eye, s, e| {
            try r.print("{s}\n{{\"variant\":\"{s}\",\"side\":{d},\"turn\":{d},\"shift\":[{d:.1},{d:.1}],\"colourway_distance\":{d:.3},\"projects\":{}}}", .{
                if (e > 0) "," else "", std.fs.path.basename(source.dir), source.side, eye.turn, eye.shift[0], eye.shift[1], colourwayDistance(outlines[fixed], outlines[e]), s,
            });
        }
        try r.writeAll("],\n\"pictures\":[");
        var first_picture = true;
        for (states.items, hulls) |state, hull| {
            const buffers = try readBuffers(arena, io, try std.fmt.allocPrint(arena, "{s}/{s}_{s}", .{ work, state, target.name }), target.size);
            var means: Means = .{};
            const projectors = try arena.alloc(Projector, n);
            const ground = try arena.alloc(Projector, n);
            var ground_count: usize = 0;
            var covers: std.ArrayList(f64) = .empty;
            for (sources.items, eyes, same, projectors, 0..) |source, eye, s, *projector, e| {
                const picture = pictureOf(source, state);
                projector.* = .{ .eye = eye, .picture = if (s) picture else null };
                if (picture == null) continue;
                const source_buffers = try readBuffers(arena, io, try std.fmt.allocPrint(arena, "{s}/{s}_eye{d}", .{ work, state, e }), .{ picture.?.width, picture.?.height });
                var both: f64 = 0;
                var either: f64 = 0;
                for (source_buffers.pos, picture.?.pixels) |hit, pixel| {
                    const in_hull = !std.math.isNan(hit[0]);
                    both += @floatFromInt(@intFromBool(in_hull and pixel[3] >= 128));
                    either += @floatFromInt(@intFromBool(in_hull or pixel[3] >= 128));
                    if (s and in_hull and pixel[3] >= 128) means.add(shadeClass(hullNormal(hull, .{ hit[0], hit[1], hit[2] }), eye.turn), pixel);
                }
                try covers.append(arena, if (either == 0) 0 else both / either);
                if (!s) continue;
                const g = pictureOf(source, try std.fmt.allocPrint(arena, "{s}g", .{state})) orelse continue;
                // The variant's own ground picture first.
                ground[ground_count] = .{ .eye = eye, .picture = g };
                if (e == fixed) std.mem.swap(Projector, &ground[0], &ground[ground_count]);
                ground_count += 1;
            }
            const sprite = try projectPicture(arena, hull, buffers, target.eye, checks.items, mirrors.items, projectors, means);
            try patchFill(arena, sprite);
            const shadow: Image = .{ .width = target.size[0], .height = target.size[1], .pixels = try arena.alloc([4]u8, buffers.flags.len) };
            for (shadow.pixels, buffers.flags) |*pixel, flags| pixel.* = if (flags & shadow_bit != 0) .{ 0, 0, 0, 255 } else .{ 0, 0, 0, 0 };
            const shadow_name = try std.fmt.allocPrint(arena, "{s}s", .{state});
            const ground_name = try std.fmt.allocPrint(arena, "{s}g", .{state});
            try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{state}), try writePng(arena, sprite.image));
            try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{shadow_name}), try writePng(arena, shadow));
            try writePictureReport(ctx, r, map_dir, state, sprite, covers.items, &first_picture);
            var argv: std.ArrayList([]const u8) = .empty;
            const grid = try turnGrid(arena, own.footprint, target.turn);
            const locked = lockedCenter(.{ @floatCast(grid.origin[0]), @floatCast(grid.origin[1]) });
            try argv.appendSlice(arena, &.{
                sprite_tool,
                "compose-pack",
                try std.fs.path.join(arena, &.{ pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{state}) }),
                try std.fs.path.join(arena, &.{ pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{shadow_name}) }),
                try std.fs.path.join(arena, &.{ temp, state }),
                "--zero",
                try std.fmt.allocPrint(arena, "{d},{d}", .{ target.eye.anchor[0], target.eye.anchor[1] }),
                "--pass",
                try std.mem.join(arena, "/", grid.rows),
                "--locked",
                try std.fmt.allocPrint(arena, "{d},{d}", .{ locked[0], locked[1] }),
                "--force",
            });
            if (pictureOf(own, ground_name) != null) {
                const g = try projectGround(arena, target.eye, target.size, ground[0..ground_count]);
                try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{ground_name}), try writePng(arena, g.image));
                try writePictureReport(ctx, r, map_dir, ground_name, g, &.{}, &first_picture);
                try argv.appendSlice(arena, &.{ "--noise", try std.fs.path.join(arena, &.{ pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{ground_name}) }) });
            }
            if (std.mem.eql(u8, state, "1")) try argv.appendSlice(arena, &.{ "--icon", try std.fs.path.join(arena, &.{ temp, "icon.tga" }) });
            const result = std.process.run(arena, io, .{ .argv = argv.items, .stdout_limit = .limited(1024 * 1024), .stderr_limit = .limited(1024 * 1024) }) catch |err|
                return ctx.failKey("views.err.compose", .{ temp, @errorName(err) });
            if (result.term != .exited or result.term.exited != 0) return ctx.failKey("views.err.compose", .{ temp, result.stderr });
        }
        try r.writeAll("]}\n");
        try writeOut(ctx, map_dir, "sources.json", report.written());
        try writeOut(ctx, temp, "1.xml", try turnXml(arena, own.xml, target.turn));
        if (cwd.readFileAlloc(io, try std.fs.path.join(arena, &.{ own.dir, "name.txt" }), arena, .limited(1024 * 1024))) |name| {
            try writeOut(ctx, temp, "name.txt", name);
        } else |_| {}
        cwd.deleteTree(io, final) catch {};
        try cwd.rename(temp, cwd, final, io);
        try ctx.out.print("wrote {s}: side {d} of {s}, {d} damage states\n", .{ final, (own_side - 1 + @as(u8, target.turn)) % 4 + 1, found.family.name, states.items.len });
    }
}

/// The height of the hull's highest voxel top.
fn hullTop(hull: Hull) f64 {
    var l = hull.n[2];
    while (l > 0) : (l -= 1) {
        for (0..hull.n[1]) |j| for (0..hull.n[0]) |i| {
            if (hull.get(@intCast(i), @intCast(j), @intCast(l - 1))) return hull.lo[2] + @as(f64, @floatFromInt(l)) * hull.size;
        };
    }
    return 0;
}

/// The source map of one generated picture: a PNG (projected green, mirrored
/// blue, filled red) and its counts per shading class in the report.
fn writePictureReport(ctx: *bk_tool.Context, r: *std.Io.Writer, map_dir: []const u8, name: []const u8, generated: Generated, covers: []const f64, first: *bool) !void {
    const arena = ctx.arena;
    const map: Image = .{ .width = generated.image.width, .height = generated.image.height, .pixels = try arena.alloc([4]u8, generated.kinds.len) };
    var counts = std.mem.zeroes([9][4]usize);
    for (map.pixels, generated.kinds, generated.classes) |*pixel, kind, class| {
        pixel.* = switch (kind) {
            .empty => .{ 0, 0, 0, 0 },
            .projected => .{ 0, 160, 0, 255 },
            .mirrored => .{ 0, 64, 255, 255 },
            .filled => .{ 255, 32, 32, 255 },
        };
        counts[@min(class, 8)][@backingInt(kind)] += 1;
    }
    try writeOut(ctx, map_dir, try std.fmt.allocPrint(arena, "{s}.png", .{name}), try writePng(arena, map));
    var total = [4]usize{ 0, 0, 0, 0 };
    for (counts) |c| for (0..4) |k| {
        total[k] += c[k];
    };
    try r.print("{s}\n{{\"picture\":\"{s}\",\"projected\":{d},\"mirrored\":{d},\"filled\":{d},\"hull_iou\":[", .{ if (first.*) "" else ",", name, total[1], total[2], total[3] });
    first.* = false;
    for (covers, 0..) |c, i| try r.print("{s}{d:.4}", .{ if (i > 0) "," else "", c });
    try r.writeAll("],\"classes\":[");
    var first_class = true;
    for (counts, 0..) |c, class| {
        if (class == 8 or c[1] + c[2] + c[3] == 0) continue;
        try r.print("{s}{{\"class\":\"{s}\",\"projected\":{d},\"mirrored\":{d},\"filled\":{d}}}", .{ if (first_class) "" else ",", class_names[class], c[1], c[2], c[3] });
        first_class = false;
    }
    try r.writeAll("]}");
}

/// The shading classes by the way they face in the picture's own frame.
const class_names = [8][]const u8{ "wall -y", "wall +x", "wall +y", "wall -x", "roof -y", "roof +x", "roof +y", "roof -x" };
