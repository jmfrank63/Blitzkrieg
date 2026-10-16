//! `bk-image`: image conversions modders need, as subcommands of one tool
//! (Phase 8, D-21). std-only Zig: these are pure pixel transforms (D-03).
//!
//! `offset-romb` is OffsetRomb.exe ("Romboid tile offsetter"): it swaps the
//! two diamond-shaped halves of a terrain tile with Bresenham lines
//! (`FlipRomb1` then `FlipRomb2`), so tile art drawn in the old offset layout
//! becomes the game's. The lines and the order of the swaps are the legacy
//! code's, line for line. One difference: for most tile sizes the legacy lines
//! end one row below the image (FlipRomb1's second block runs to row `H`), and
//! the legacy exe swapped those pixels with whatever memory followed the
//! image. Here a swap that reaches outside the pixel buffer is skipped, so the
//! output is deterministic; it differs from the legacy one only in those
//! pixels, which never had a defined value. A size whose lines never close
//! (4x4, for one) looped forever in the legacy exe and is refused here.
//!
//! Images are read the way the engine's TGA reader (`Image/ImageTGA.cpp`)
//! reads the true-colour and grey ones (types 2, 10, 3 and 11), and written as
//! its `SaveImageAsTGA` writes them: uncompressed 32-bit BGRA, top row first,
//! with the TRUEVISION footer, which is what OffsetRomb.exe wrote.

const std = @import("std");
const bk_tool = @import("bk_tool.zig");

const usage =
    \\usage: bk-image offset-romb <in.tga> [--out <out.tga>]
    \\
    \\offset-romb  swaps the two diamond-shaped halves of a terrain tile, as
    \\             OffsetRomb did, to bring tile art drawn in the old offset layout
    \\             into the game's. Writes a 32-bit TGA to <out.tga>, or over
    \\             <in.tga> without --out.
    \\
;

pub fn main(init: std.process.Init) u8 {
    return bk_tool.main(init, "bk-image", usage, run);
}

fn run(ctx: *bk_tool.Context) !void {
    if (ctx.args.len == 0 or !std.mem.eql(u8, ctx.args[0], "offset-romb")) return error.Usage;
    var input: ?[]const u8 = null;
    var output: ?[]const u8 = null;
    var i: usize = 1;
    while (i < ctx.args.len) : (i += 1) {
        const arg = ctx.args[i];
        if (std.mem.eql(u8, arg, "--out")) {
            if (output != null or i + 1 == ctx.args.len) return error.Usage;
            i += 1;
            output = ctx.args[i];
        } else if (std.mem.startsWith(u8, arg, "-") or input != null) {
            return error.Usage;
        } else {
            input = arg;
        }
    }
    if (input == null) return error.Usage;

    var arena_state: std.heap.ArenaAllocator = .init(ctx.gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const in_path = try bk_tool.inputPath(arena, input.?);
    const out_path = try bk_tool.inputPath(arena, output orelse input.?);
    const cwd = std.Io.Dir.cwd();

    const bytes = cwd.readFileAlloc(ctx.io, in_path, arena, .limited(256 * 1024 * 1024)) catch |err| switch (err) {
        error.FileNotFound => return ctx.fail("{s}: no such file", .{in_path}),
        else => return ctx.fail("{s}: {s}", .{ in_path, @errorName(err) }),
    };
    var image = readTga(arena, bytes) catch |err| return ctx.fail("{s}: {s}", .{ in_path, tgaErrorText(err) });
    offsetRomb(&image) catch return ctx.fail("{s}: OffsetRomb's lines never close on a {d}x{d} image", .{ in_path, image.width, image.height });
    const encoded = try writeTga(arena, image);
    if (std.fs.path.dirname(out_path)) |parent| try cwd.createDirPath(ctx.io, parent);
    cwd.writeFile(ctx.io, .{ .sub_path = out_path, .data = encoded }) catch |err|
        return ctx.fail("{s}: {s}", .{ out_path, @errorName(err) });
    try ctx.out.print("wrote {s}: {d}x{d}, romb halves swapped\n", .{ out_path, image.width, image.height });
}

/// Pixels in the engine's `SColor` byte order (B, G, R, A), top row first.
pub const Image = struct {
    width: u32,
    height: u32,
    pixels: [][4]u8,
};

pub const TgaError = error{ Truncated, Unsupported, OutOfMemory };

fn tgaErrorText(err: TgaError) []const u8 {
    return switch (err) {
        error.Truncated => "not a TGA, or cut short",
        error.Unsupported => "not a true-colour (24/32-bit) or 8-bit grey TGA",
        error.OutOfMemory => "out of memory",
    };
}

/// The engine reader's types: true colour (2, RLE 10) at 24 or 32 bits and
/// grey (3, RLE 11) at 8 bits. Colour-mapped TGAs are refused.
pub fn readTga(gpa: std.mem.Allocator, bytes: []const u8) TgaError!Image {
    if (bytes.len < 18) return error.Truncated;
    const id_length = bytes[0];
    const color_map_type = bytes[1];
    const image_type = bytes[2];
    const width = std.mem.readInt(u16, bytes[12..14], .little);
    const height = std.mem.readInt(u16, bytes[14..16], .little);
    const depth = bytes[16];
    const descriptor = bytes[17];
    const grey = image_type == 3 or image_type == 11;
    const rle = image_type == 10 or image_type == 11;
    if (color_map_type != 0 or !(image_type == 2 or image_type == 10 or grey)) return error.Unsupported;
    if (if (grey) depth != 8 else depth != 24 and depth != 32) return error.Unsupported;
    if (width == 0 or height == 0) return error.Unsupported;
    const pixel_size: usize = depth / 8;
    const count = @as(usize, width) * height;
    const pixels = try gpa.alloc([4]u8, count);
    errdefer gpa.free(pixels);

    var pos: usize = 18 + @as(usize, id_length);
    var done: usize = 0;
    while (done < count) {
        var run_length: usize = count - done;
        var repeat = false;
        if (rle) {
            if (pos >= bytes.len) return error.Truncated;
            const header = bytes[pos];
            pos += 1;
            run_length = @min(@as(usize, header & 0x7f) + 1, count - done);
            repeat = header & 0x80 != 0;
        }
        const raw_count = if (repeat) 1 else run_length;
        if (bytes.len - @min(bytes.len, pos) < raw_count * pixel_size) return error.Truncated;
        for (0..run_length) |k| {
            const at = pos + (if (repeat) 0 else k) * pixel_size;
            pixels[done + k] = switch (pixel_size) {
                1 => .{ bytes[at], bytes[at], bytes[at], 0xff },
                3 => .{ bytes[at], bytes[at + 1], bytes[at + 2], 0xff },
                else => bytes[at..][0..4].*,
            };
        }
        pos += raw_count * pixel_size;
        done += run_length;
    }
    // Bottom-left origin unless the descriptor says top-to-bottom, as the
    // engine reads it; the left-to-right bit is ignored there too.
    if (descriptor & 0x20 == 0) {
        const row = @as(usize, width);
        for (0..height / 2) |y| {
            const top = pixels[y * row ..][0..row];
            const bottom = pixels[(height - 1 - y) * row ..][0..row];
            for (top, bottom) |*a, *b| std.mem.swap([4]u8, a, b);
        }
    }
    return .{ .width = width, .height = height, .pixels = pixels };
}

/// `NImage::SaveImageAsTGA`'s bytes: an 18-byte header (type 2, 32 bits,
/// 8 alpha bits, top-to-bottom), the pixels, and the 26-byte footer.
pub fn writeTga(gpa: std.mem.Allocator, image: Image) ![]u8 {
    const footer = "\x00\x00\x00\x00\x00\x00\x00\x00TRUEVISION-XFILE.\x00";
    const out = try gpa.alloc(u8, 18 + image.pixels.len * 4 + footer.len);
    @memset(out[0..18], 0);
    out[2] = 2;
    std.mem.writeInt(u16, out[12..14], @intCast(image.width), .little);
    std.mem.writeInt(u16, out[14..16], @intCast(image.height), .little);
    out[16] = 32;
    out[17] = 0x28;
    @memcpy(out[18..][0 .. image.pixels.len * 4], std.mem.sliceAsBytes(image.pixels));
    @memcpy(out[out.len - footer.len ..], footer);
    return out;
}

/// OffsetRomb's CBrazenhaimLine.
const Line = struct {
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
    xerr: i32 = 0,
    yerr: i32 = 0,
    xlen: i32,
    ylen: i32,
    len: i32,
    xinc: i32,
    yinc: i32,

    fn init(x1: i32, y1: i32, x2: i32, y2: i32) Line {
        const xlen: i32 = @intCast(@abs(x2 - x1) + 1);
        const ylen: i32 = @intCast(@abs(y2 - y1) + 1);
        return .{
            .x1 = x1,
            .y1 = y1,
            .x2 = x2,
            .y2 = y2,
            .xlen = xlen,
            .ylen = ylen,
            .len = @max(xlen, ylen),
            .xinc = std.math.sign(x2 - x1),
            .yinc = std.math.sign(y2 - y1),
        };
    }

    fn next(line: *Line) bool {
        if (line.x1 == line.x2 and line.y1 == line.y2) return false;
        line.xerr += line.xlen;
        if (line.xerr >= line.len) {
            line.x1 += line.xinc;
            line.xerr -= line.len;
        }
        line.yerr += line.ylen;
        if (line.yerr >= line.len) {
            line.y1 += line.yinc;
            line.yerr -= line.len;
        }
        return true;
    }
};

/// The legacy accessor was `image[y][x]` on one row-major buffer, so an x past
/// the row's end is the next row's pixel; only a position outside the whole
/// buffer is no pixel.
fn pixelIndex(image: *const Image, x: i32, y: i32) ?usize {
    const index = @as(i64, y) * image.width + x;
    if (index < 0 or index >= image.pixels.len) return null;
    return @intCast(index);
}

/// FlipLines: swaps the pixels under two lines, step by step, until either
/// ends (the second line is not stepped once the first has ended).
fn flipLines(image: *Image, first: Line, second: Line) void {
    var line1 = first;
    var line2 = second;
    while (true) {
        if (pixelIndex(image, line1.x1, line1.y1)) |a| {
            if (pixelIndex(image, line2.x1, line2.y1)) |b| std.mem.swap([4]u8, &image.pixels[a], &image.pixels[b]);
        }
        if (!line1.next() or !line2.next()) break;
    }
}

/// One `while` block of FlipRomb1 or FlipRomb2: two line pairs per pass, the
/// coordinates stepped in between as the legacy code steps them.
const Block = struct {
    top: [4]i32,
    bottom: [4]i32,
};

const Step = enum { romb1, romb2 };

fn runBlock(image: *Image, block: Block, step: Step) error{NeverCloses}!void {
    var t = block.top;
    var b = block.bottom;
    // The four distances move by one or two each pass, so the loop closes
    // within width + height passes if it closes at all.
    var passes: u32 = 0;
    const limit = 2 * (image.width + image.height) + 8;
    while (t[0] != b[0] and t[2] != b[2] and t[1] != b[1] and t[3] != b[3]) {
        passes += 1;
        if (passes > limit) return error.NeverCloses;
        flipLines(image, .init(t[0], t[1], t[2], t[3]), .init(b[0], b[1], b[2], b[3]));
        switch (step) {
            .romb1 => {
                t[1] += 1;
                t[3] += 1;
                b[0] += 2;
                b[2] += 2;
            },
            .romb2 => {
                t[0] += 2;
                t[2] += 2;
                b[1] -= 1;
                b[3] -= 1;
            },
        }
        flipLines(image, .init(t[0], t[1], t[2], t[3]), .init(b[0], b[1], b[2], b[3]));
        switch (step) {
            .romb1 => {
                t[0] -= 2;
                t[2] -= 2;
                b[1] -= 1;
                b[3] -= 1;
            },
            .romb2 => {
                t[1] += 1;
                t[3] += 1;
                b[0] -= 2;
                b[2] -= 2;
            },
        }
    }
}

/// OffsetRomb's main: FlipRomb1 then FlipRomb2 over the whole image. The
/// arrays are each line's x1, y1, x2, y2.
pub fn offsetRomb(image: *Image) error{NeverCloses}!void {
    const w: i32 = @intCast(image.width);
    const h: i32 = @intCast(image.height);
    const hx = @divTrunc(w, 2);
    const hy = @divTrunc(h, 2);
    const qx = @divTrunc(hx, 2);
    const qy = @divTrunc(hy, 2);
    // FlipRomb1
    try runBlock(image, .{ .top = .{ hx - 1, 0, hx * 2 - 1, hy }, .bottom = .{ qx, qy, qx * 3, qy * 3 } }, .romb1);
    try runBlock(image, .{ .top = .{ qx, qy, qx * 3, qy * 3 }, .bottom = .{ 0, hy, hx - 1, hy * 2 } }, .romb1);
    // FlipRomb2
    try runBlock(image, .{ .top = .{ 0, hy, hx, 0 }, .bottom = .{ qx, qy * 3, qx * 3 + 1, qy } }, .romb2);
    try runBlock(image, .{ .top = .{ qx, qy * 3, qx * 3 + 1, qy }, .bottom = .{ hx - 1, h - 1, w - 1, hy } }, .romb2);
}
