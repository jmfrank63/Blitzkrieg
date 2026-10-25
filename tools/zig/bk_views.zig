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

const std = @import("std");
const bk_tool = @import("bk_tool.zig");

const usage: bk_tool.Usage = .{
    .synopsis =
    \\usage: bk-views decompose <variant-dir> <out-dir>
    \\       bk-views camera <variant-dir>... [--out <camera.json>]
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
const Bounds = struct {
    min_x: i32 = std.math.maxInt(i32),
    min_y: i32 = std.math.maxInt(i32),
    max_x: i32 = std.math.minInt(i32),
    max_y: i32 = std.math.minInt(i32),

    fn add(bounds: *Bounds, square: Square) void {
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
    if (sliceBetween(xml, "<passability>", "</passability>")) |grid| {
        var rows: std.ArrayList([]const u8) = .empty;
        var rest = grid;
        while (sliceBetween(rest, "<item>", "</item>")) |row| {
            try rows.append(gpa, row);
            rest = rest[@intFromPtr(row.ptr) - @intFromPtr(rest.ptr) + row.len ..];
        }
        stats.passability = rows.items;
    }
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

fn decomposeCommand(ctx: *bk_tool.Context, dir_arg: []const u8, out_arg: []const u8) !void {
    const io = ctx.io;
    const arena = ctx.arena;
    const dir_path = try bk_tool.inputPath(arena, dir_arg);
    const out_path = try bk_tool.inputPath(arena, out_arg);
    const cwd = std.Io.Dir.cwd();
    var dir = cwd.openDir(io, dir_path, .{ .iterate = true }) catch return ctx.failKey("views.err.no_dir", .{dir_path});
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

    try cwd.createDirPath(io, out_path);
    var pictures: std.ArrayList([]const u8) = .empty;
    for (files.items) |file| {
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
    try ctx.out.print("wrote {s}: {d} packs, {d} pictures of {d}x{d}\n", .{ out_path, files.items.len, pictures.items.len, bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y });
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
    const Variant = struct { dir: []const u8, stats: Stats, sprite: Image, anchor: [2]i32 };
    var variants: std.ArrayList(Variant) = .empty;
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
        pairs.items.len,  rms,              fitted[0],        fitted[1],        fitted[2],
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
