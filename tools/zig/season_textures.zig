//! Generates the winter ("w") and Africa ("a") textures a mesh unit lacks.
//!
//! A mechanical unit picks its texture by season: "<dir>\1" in summer,
//! "<dir>\1w" in winter and "<dir>\1a" in Africa (Common/MOUnitMechanical.cpp);
//! the wreck is "2" + season and the outside passengers "1p" + season. Many
//! unit folders ship only the summer set, in the original game too, so on a
//! winter or Africa map such a unit has no paint of its season. This tool
//! derives the missing season files from the summer ones:
//!
//!   winter: a whitewash over the luminance, with a smooth value noise so the
//!           wash is patchy; near-black parts (tyres, gaps) stay dark.
//!   Africa: a sand tint curve measured from Nival's own German summer/Africa
//!           pairs (see --derive-africa-lut), looked up by a key that lifts
//!           green so summer camouflage fades under the sand paint.
//!
//! Only mesh folders (holding a .mod) are touched; sprite infantry is not.
//! Every texture exists as three quality files, <name>_c/_h/_l.dds. Each
//! generated file keeps its summer file's header, pixel format, size and mip
//! count byte for byte; only the pixel data changes (mips, if any, are rebuilt
//! from the transformed top level). A season file Data already has is never
//! generated. The output is deterministic: the same input gives the same bytes.
//!
//! The build runs it on every staging (see build.zig, addSeasonData): the
//! output, one stored archive SeasonTextures.pak, goes to a cached build
//! directory, is staged beside Data as SeasonData, and the engine mounts it
//! over Data (Sources/src/StreamIO/SeasonData.h). Data itself is never written.
//!
//! usage: season_textures <Data dir> --out <dir> [--only <path under Data>]
//!        season_textures <Data dir> --dry-run [--only <path under Data>]
//!        season_textures <Data dir> --derive-africa-lut
//! or:    zig build season-textures -- Data --dry-run [--only Units/...]
//!        (paths relative to the repository root)
const std = @import("std");

// ---------------------------------------------------------------------------
// DDS header

pub const Format = union(enum) {
    /// DXT1, DXT3 or DXT5.
    dxt: u8,
    /// Uncompressed, with per-channel bit masks in r, g, b, a order.
    rgb: struct { bytes: u8, masks: [4]u32 },
};

pub const Header = struct {
    width: u32,
    height: u32,
    mips: u32,
    format: Format,

    pub const size: usize = 128;
};

const DDPF_ALPHAPIXELS: u32 = 0x1;
const DDPF_FOURCC: u32 = 0x4;
const DDPF_RGB: u32 = 0x40;
const DDSD_MIPMAPCOUNT: u32 = 0x20000;

fn le32(bytes: []const u8, offset: usize) u32 {
    return std.mem.readInt(u32, bytes[offset..][0..4], .little);
}

pub fn parseHeader(bytes: []const u8) !Header {
    if (bytes.len < Header.size) return error.TruncatedDds;
    if (!std.mem.eql(u8, bytes[0..4], "DDS ") or le32(bytes, 4) != 124) return error.NotDds;
    const flags = le32(bytes, 8);
    const height = le32(bytes, 12);
    const width = le32(bytes, 16);
    const mip_field = le32(bytes, 28);
    if (width == 0 or height == 0) return error.NotDds;
    const pf_flags = le32(bytes, 80);
    const format: Format = if (pf_flags & DDPF_FOURCC != 0) blk: {
        const four = bytes[84..88];
        if (std.mem.eql(u8, four, "DXT1")) break :blk .{ .dxt = 1 };
        if (std.mem.eql(u8, four, "DXT3")) break :blk .{ .dxt = 3 };
        if (std.mem.eql(u8, four, "DXT5")) break :blk .{ .dxt = 5 };
        return error.UnsupportedFormat;
    } else if (pf_flags & DDPF_RGB != 0) blk: {
        const bits = le32(bytes, 88);
        if (bits != 16 and bits != 24 and bits != 32) return error.UnsupportedFormat;
        const alpha = if (pf_flags & DDPF_ALPHAPIXELS != 0) le32(bytes, 104) else 0;
        const masks = [4]u32{ le32(bytes, 92), le32(bytes, 96), le32(bytes, 100), alpha };
        for (masks[0..3]) |mask| if (mask == 0) return error.UnsupportedFormat;
        break :blk .{ .rgb = .{ .bytes = @intCast(bits / 8), .masks = masks } };
    } else return error.UnsupportedFormat;
    const mips: u32 = if (flags & DDSD_MIPMAPCOUNT != 0 and mip_field > 1) mip_field else 1;
    return .{ .width = width, .height = height, .mips = mips, .format = format };
}

pub fn levelSize(format: Format, width: u32, height: u32) usize {
    return switch (format) {
        .dxt => |kind| @as(usize, (width + 3) / 4) * ((height + 3) / 4) * @as(usize, if (kind == 1) 8 else 16),
        .rgb => |rgb| @as(usize, width) * height * rgb.bytes,
    };
}

fn mipDim(value: u32, level: u32) u32 {
    return @max(1, value >> @intCast(level));
}

// ---------------------------------------------------------------------------
// Uncompressed codec: any mask layout (A8R8G8B8, X8R8G8B8, R5G6B5, A4R4G4B4,
// A1R5G5B5, ...). A channel's bits scale to 8 bits and back with rounding, so
// a value that came from the format survives the round trip exactly.

fn expandChannel(pixel: u32, mask: u32) u8 {
    if (mask == 0) return 255;
    const shift: u5 = @intCast(@ctz(mask));
    const max = mask >> shift;
    const value = (pixel & mask) >> shift;
    return @intCast((@as(u64, value) * 255 + max / 2) / max);
}

fn packChannel(value: u8, mask: u32) u32 {
    if (mask == 0) return 0;
    const shift: u5 = @intCast(@ctz(mask));
    const max = mask >> shift;
    const packed_value: u32 = @intCast((@as(u64, value) * max + 127) / 255);
    return packed_value << shift;
}

fn decodeRgb(bytes_per_pixel: u8, masks: [4]u32, src: []const u8, out: [][4]u8) void {
    for (out, 0..) |*pixel, i| {
        var raw: u32 = 0;
        for (0..bytes_per_pixel) |b| raw |= @as(u32, src[i * bytes_per_pixel + b]) << @intCast(8 * b);
        pixel.* = .{ expandChannel(raw, masks[0]), expandChannel(raw, masks[1]), expandChannel(raw, masks[2]), expandChannel(raw, masks[3]) };
    }
}

fn encodeRgb(bytes_per_pixel: u8, masks: [4]u32, pixels: []const [4]u8, dst: []u8) void {
    for (pixels, 0..) |pixel, i| {
        var raw: u32 = 0;
        for (0..4) |c| raw |= packChannel(pixel[c], masks[c]);
        for (0..bytes_per_pixel) |b| dst[i * bytes_per_pixel + b] = @truncate(raw >> @intCast(8 * b));
    }
}

// ---------------------------------------------------------------------------
// DXT decode

fn expand565(c: u16) [3]u8 {
    const r: u8 = @intCast(c >> 11);
    const g: u8 = @intCast((c >> 5) & 63);
    const b: u8 = @intCast(c & 31);
    return .{ (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2) };
}

fn colorPalette(c0: u16, c1: u16, four_colors: bool) [4][4]u8 {
    const a = expand565(c0);
    const b = expand565(c1);
    var palette: [4][4]u8 = undefined;
    palette[0] = .{ a[0], a[1], a[2], 255 };
    palette[1] = .{ b[0], b[1], b[2], 255 };
    for (0..3) |c| {
        const x: u16 = a[c];
        const y: u16 = b[c];
        if (four_colors) {
            palette[2][c] = @intCast((2 * x + y) / 3);
            palette[3][c] = @intCast((x + 2 * y) / 3);
        } else {
            palette[2][c] = @intCast((x + y) / 2);
            palette[3][c] = 0;
        }
    }
    palette[2][3] = 255;
    palette[3][3] = if (four_colors) 255 else 0;
    return palette;
}

fn decodeColorBlock(block: *const [8]u8, dxt1: bool, out: *[16][4]u8) void {
    const c0 = std.mem.readInt(u16, block[0..2], .little);
    const c1 = std.mem.readInt(u16, block[2..4], .little);
    const indices = std.mem.readInt(u32, block[4..8], .little);
    const palette = colorPalette(c0, c1, !dxt1 or c0 > c1);
    for (out, 0..) |*pixel, i| pixel.* = palette[(indices >> @intCast(2 * i)) & 3];
}

fn alphaPalette(a0: u8, a1: u8) [8]u8 {
    var palette: [8]u8 = undefined;
    palette[0] = a0;
    palette[1] = a1;
    const x: u16 = a0;
    const y: u16 = a1;
    if (a0 > a1) {
        for (1..7) |i| palette[i + 1] = @intCast(((7 - i) * x + i * y) / 7);
    } else {
        for (1..5) |i| palette[i + 1] = @intCast(((5 - i) * x + i * y) / 5);
        palette[6] = 0;
        palette[7] = 255;
    }
    return palette;
}

fn decodeBlock(kind: u8, block: []const u8, out: *[16][4]u8) void {
    switch (kind) {
        1 => decodeColorBlock(block[0..8], true, out),
        3 => {
            decodeColorBlock(block[8..16], false, out);
            const bits = std.mem.readInt(u64, block[0..8], .little);
            for (out, 0..) |*pixel, i| pixel[3] = @as(u8, @intCast((bits >> @intCast(4 * i)) & 15)) * 17;
        },
        else => {
            decodeColorBlock(block[8..16], false, out);
            const palette = alphaPalette(block[0], block[1]);
            var bits: u64 = 0;
            for (0..6) |b| bits |= @as(u64, block[2 + b]) << @intCast(8 * b);
            for (out, 0..) |*pixel, i| pixel[3] = palette[(bits >> @intCast(3 * i)) & 7];
        },
    }
}

fn decodeDxt(kind: u8, src: []const u8, width: u32, height: u32, out: [][4]u8) void {
    const block_bytes: usize = if (kind == 1) 8 else 16;
    const blocks_x = (width + 3) / 4;
    const blocks_y = (height + 3) / 4;
    var block: [16][4]u8 = undefined;
    for (0..blocks_y) |by| for (0..blocks_x) |bx| {
        const offset = (by * blocks_x + bx) * block_bytes;
        decodeBlock(kind, src[offset .. offset + block_bytes], &block);
        for (0..4) |py| for (0..4) |px| {
            const x = bx * 4 + px;
            const y = by * 4 + py;
            if (x < width and y < height) out[y * width + x] = block[py * 4 + px];
        };
    };
}

// ---------------------------------------------------------------------------
// DXT encode: principal-axis range fit, then least-squares refinement of the
// endpoints for the chosen indices, keeping the best quantized result.

fn quantize565(color: [3]f32) u16 {
    const r: u16 = @intFromFloat(@round(std.math.clamp(color[0], 0, 255) * 31.0 / 255.0));
    const g: u16 = @intFromFloat(@round(std.math.clamp(color[1], 0, 255) * 63.0 / 255.0));
    const b: u16 = @intFromFloat(@round(std.math.clamp(color[2], 0, 255) * 31.0 / 255.0));
    return (r << 11) | (g << 5) | b;
}

const Fit = struct { c0: u16, c1: u16, indices: [16]u2, err: u32 };

/// Picks the nearest palette entry for every used pixel. `four` selects the
/// 4-colour palette; otherwise the 3-colour one (index 3 is reserved for
/// transparent texels).
fn assignIndices(pixels: *const [16][4]u8, used: u16, c0: u16, c1: u16, four: bool) Fit {
    const palette = colorPalette(c0, c1, four);
    const count: usize = if (four) 4 else 3;
    var fit = Fit{ .c0 = c0, .c1 = c1, .indices = @as([16]u2, @splat(3)), .err = 0 };
    for (pixels, 0..) |pixel, i| {
        if (used & (@as(u16, 1) << @intCast(i)) == 0) continue;
        var best: u32 = std.math.maxInt(u32);
        for (0..count) |p| {
            var e: u32 = 0;
            for (0..3) |c| {
                const d = @as(i32, pixel[c]) - @as(i32, palette[p][c]);
                e += @intCast(d * d);
            }
            if (e < best) {
                best = e;
                fit.indices[i] = @intCast(p);
            }
        }
        fit.err += best;
    }
    return fit;
}

fn principalEndpoints(pixels: *const [16][4]u8, used: u16) [2][3]f32 {
    var mean = [3]f32{ 0, 0, 0 };
    var n: f32 = 0;
    for (pixels, 0..) |pixel, i| {
        if (used & (@as(u16, 1) << @intCast(i)) == 0) continue;
        for (0..3) |c| mean[c] += @floatFromInt(pixel[c]);
        n += 1;
    }
    for (&mean) |*m| m.* /= n;
    var cov = [6]f32{ 0, 0, 0, 0, 0, 0 }; // rr rg rb gg gb bb
    for (pixels, 0..) |pixel, i| {
        if (used & (@as(u16, 1) << @intCast(i)) == 0) continue;
        const r = @as(f32, @floatFromInt(pixel[0])) - mean[0];
        const g = @as(f32, @floatFromInt(pixel[1])) - mean[1];
        const b = @as(f32, @floatFromInt(pixel[2])) - mean[2];
        cov[0] += r * r;
        cov[1] += r * g;
        cov[2] += r * b;
        cov[3] += g * g;
        cov[4] += g * b;
        cov[5] += b * b;
    }
    var axis = [3]f32{ 0.577, 0.577, 0.577 };
    for (0..8) |_| {
        const next = [3]f32{
            cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2],
            cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2],
            cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2],
        };
        const len = @sqrt(next[0] * next[0] + next[1] * next[1] + next[2] * next[2]);
        if (len < 1e-6) break;
        axis = .{ next[0] / len, next[1] / len, next[2] / len };
    }
    var lo: f32 = std.math.floatMax(f32);
    var hi: f32 = -std.math.floatMax(f32);
    for (pixels, 0..) |pixel, i| {
        if (used & (@as(u16, 1) << @intCast(i)) == 0) continue;
        var t: f32 = 0;
        for (0..3) |c| t += (@as(f32, @floatFromInt(pixel[c])) - mean[c]) * axis[c];
        lo = @min(lo, t);
        hi = @max(hi, t);
    }
    var ends: [2][3]f32 = undefined;
    for (0..3) |c| {
        ends[0][c] = mean[c] + axis[c] * hi;
        ends[1][c] = mean[c] + axis[c] * lo;
    }
    return ends;
}

/// Least-squares endpoints for fixed indices; null when the system is singular.
fn refineEndpoints(pixels: *const [16][4]u8, used: u16, indices: [16]u2, four: bool) ?[2][3]f32 {
    const weights4 = [4]f32{ 1, 0, 2.0 / 3.0, 1.0 / 3.0 };
    const weights3 = [4]f32{ 1, 0, 0.5, 0 };
    var aa: f32 = 0;
    var ab: f32 = 0;
    var bb: f32 = 0;
    var ax = [3]f32{ 0, 0, 0 };
    var bx = [3]f32{ 0, 0, 0 };
    for (pixels, 0..) |pixel, i| {
        if (used & (@as(u16, 1) << @intCast(i)) == 0) continue;
        const w0 = if (four) weights4[indices[i]] else weights3[indices[i]];
        const w1 = 1 - w0;
        aa += w0 * w0;
        ab += w0 * w1;
        bb += w1 * w1;
        for (0..3) |c| {
            ax[c] += w0 * @as(f32, @floatFromInt(pixel[c]));
            bx[c] += w1 * @as(f32, @floatFromInt(pixel[c]));
        }
    }
    const det = aa * bb - ab * ab;
    if (@abs(det) < 1e-6) return null;
    var ends: [2][3]f32 = undefined;
    for (0..3) |c| {
        ends[0][c] = (ax[c] * bb - bx[c] * ab) / det;
        ends[1][c] = (bx[c] * aa - ax[c] * ab) / det;
    }
    return ends;
}

fn fitColors(pixels: *const [16][4]u8, used: u16, four: bool) Fit {
    var ends = principalEndpoints(pixels, used);
    var best = assignIndices(pixels, used, quantize565(ends[0]), quantize565(ends[1]), four);
    for (0..3) |_| {
        if (best.err == 0) break;
        ends = refineEndpoints(pixels, used, best.indices, four) orelse break;
        const candidate = assignIndices(pixels, used, quantize565(ends[0]), quantize565(ends[1]), four);
        if (candidate.err >= best.err) break;
        best = candidate;
    }
    return best;
}

fn writeColorBlock(c0: u16, c1: u16, indices: [16]u2, out: *[8]u8) void {
    std.mem.writeInt(u16, out[0..2], c0, .little);
    std.mem.writeInt(u16, out[2..4], c1, .little);
    var bits: u32 = 0;
    for (indices, 0..) |index, i| bits |= @as(u32, index) << @intCast(2 * i);
    std.mem.writeInt(u32, out[4..8], bits, .little);
}

/// `punch_through`: DXT1 only; texels with alpha < 128 become transparent
/// (3-colour mode, index 3).
fn encodeColorBlock(pixels: *const [16][4]u8, punch_through: bool, out: *[8]u8) void {
    var used: u16 = 0xffff;
    if (punch_through) {
        for (pixels, 0..) |pixel, i| {
            if (pixel[3] < 128) used &= ~(@as(u16, 1) << @intCast(i));
        }
    }
    if (used == 0) return writeColorBlock(0, 0, @as([16]u2, @splat(3)), out);
    const four = used == 0xffff;
    var fit = fitColors(pixels, used, four);
    if (four) {
        if (fit.c0 == fit.c1) {
            fit.indices = @as([16]u2, @splat(0)); // one colour: index 0 decodes the same in every mode
        } else if (fit.c0 < fit.c1) {
            std.mem.swap(u16, &fit.c0, &fit.c1);
            for (&fit.indices) |*index| index.* ^= 1; // 0<->1, 2<->3
        }
    } else if (fit.c0 > fit.c1) {
        std.mem.swap(u16, &fit.c0, &fit.c1);
        for (&fit.indices) |*index| {
            if (index.* < 2) index.* ^= 1;
        }
    }
    writeColorBlock(fit.c0, fit.c1, fit.indices, out);
}

fn encodeAlphaBlock5(pixels: *const [16][4]u8, out: *[8]u8) void {
    var lo: u8 = 255;
    var hi: u8 = 0;
    for (pixels) |pixel| {
        lo = @min(lo, pixel[3]);
        hi = @max(hi, pixel[3]);
    }
    out[0] = hi;
    out[1] = lo;
    var bits: u64 = 0;
    if (hi != lo) {
        const palette = alphaPalette(hi, lo);
        for (pixels, 0..) |pixel, i| {
            var best: u32 = std.math.maxInt(u32);
            var best_index: u64 = 0;
            for (palette, 0..) |value, p| {
                const d = @abs(@as(i32, pixel[3]) - @as(i32, value));
                if (d < best) {
                    best = d;
                    best_index = p;
                }
            }
            bits |= best_index << @intCast(3 * i);
        }
    }
    for (0..6) |b| out[2 + b] = @truncate(bits >> @intCast(8 * b));
}

fn encodeAlphaBlock3(pixels: *const [16][4]u8, out: *[8]u8) void {
    var bits: u64 = 0;
    for (pixels, 0..) |pixel, i| bits |= @as(u64, (@as(u32, pixel[3]) * 15 + 127) / 255) << @intCast(4 * i);
    std.mem.writeInt(u64, out, bits, .little);
}

fn encodeDxt(kind: u8, pixels: []const [4]u8, width: u32, height: u32, dst: []u8) void {
    const block_bytes: usize = if (kind == 1) 8 else 16;
    const blocks_x = (width + 3) / 4;
    const blocks_y = (height + 3) / 4;
    var block: [16][4]u8 = undefined;
    for (0..blocks_y) |by| for (0..blocks_x) |bx| {
        // A partial block at the edge repeats the last row/column.
        for (0..4) |py| for (0..4) |px| {
            const x = @min(bx * 4 + px, width - 1);
            const y = @min(by * 4 + py, height - 1);
            block[py * 4 + px] = pixels[y * width + x];
        };
        const out = dst[(by * blocks_x + bx) * block_bytes ..][0..block_bytes];
        switch (kind) {
            1 => encodeColorBlock(&block, true, out[0..8]),
            3 => {
                encodeAlphaBlock3(&block, out[0..8]);
                encodeColorBlock(&block, false, out[8..16]);
            },
            else => {
                encodeAlphaBlock5(&block, out[0..8]);
                encodeColorBlock(&block, false, out[8..16]);
            },
        }
    };
}

pub fn decodeLevel(format: Format, src: []const u8, width: u32, height: u32, out: [][4]u8) void {
    switch (format) {
        .dxt => |kind| decodeDxt(kind, src, width, height, out),
        .rgb => |rgb| decodeRgb(rgb.bytes, rgb.masks, src, out),
    }
}

pub fn encodeLevel(format: Format, pixels: []const [4]u8, width: u32, height: u32, dst: []u8) void {
    switch (format) {
        .dxt => |kind| encodeDxt(kind, pixels, width, height, dst),
        .rgb => |rgb| encodeRgb(rgb.bytes, rgb.masks, pixels, dst),
    }
}

/// 2x2 box filter; an odd edge averages the texel with itself.
fn downsample(src: []const [4]u8, width: u32, height: u32, dst: [][4]u8) void {
    const w2 = @max(1, width / 2);
    const h2 = @max(1, height / 2);
    for (0..h2) |y| for (0..w2) |x| {
        const x0 = @min(2 * x, width - 1);
        const x1 = @min(2 * x + 1, width - 1);
        const y0 = @min(2 * y, height - 1);
        const y1 = @min(2 * y + 1, height - 1);
        for (0..4) |c| {
            const sum = @as(u32, src[y0 * width + x0][c]) + src[y0 * width + x1][c] + src[y1 * width + x0][c] + src[y1 * width + x1][c];
            dst[y * w2 + x][c] = @intCast((sum + 2) / 4);
        }
    };
}

// ---------------------------------------------------------------------------
// Season transforms

pub const Season = enum {
    winter,
    africa,

    pub fn suffix(season: Season) []const u8 {
        return switch (season) {
            .winter => "w",
            .africa => "a",
        };
    }
};

fn hash(x: i32, y: i32) f32 {
    var n: u32 = @as(u32, @bitCast(x)) *% 374761393 +% @as(u32, @bitCast(y)) *% 668265263;
    n = (n ^ (n >> 13)) *% 1274126177;
    return @as(f32, @floatFromInt(n & 0xffff)) / 65535.0;
}

pub fn valueNoise(x: f32, y: f32) f32 {
    const fx0 = @floor(x);
    const fy0 = @floor(y);
    const xi: i32 = @intFromFloat(fx0);
    const yi: i32 = @intFromFloat(fy0);
    const fx = x - fx0;
    const fy = y - fy0;
    const sx = fx * fx * (3 - 2 * fx);
    const sy = fy * fy * (3 - 2 * fy);
    const a = hash(xi, yi);
    const b = hash(xi + 1, yi);
    const c = hash(xi, yi + 1);
    const d = hash(xi + 1, yi + 1);
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy;
}

/// The two-octave noise both transforms share, in texels of a 256-wide
/// texture, so the _c/_h/_l files (and any size) of one texture match.
pub fn seasonNoise(u: f32, v: f32) f32 {
    return 0.6 * valueNoise(u / 10, v / 10) + 0.4 * valueNoise(u / 4, v / 4);
}

/// Mean Africa colour per summer africaKey, measured by --derive-africa-lut
/// over the 35 German units that ship both "1" and "1a": the summer _h key is
/// binned in 16 steps, each entry is (mean key, mean Africa r, g, b). Allied
/// "1a" files are not used: the British summer textures are already desert
/// paint, so their pairs say little about a repaint.
pub const africa_lut = [_][4]f32{
    .{ 0.009, 0.080, 0.055, 0.030 },
    .{ 0.096, 0.249, 0.168, 0.090 },
    .{ 0.155, 0.353, 0.234, 0.112 },
    .{ 0.219, 0.419, 0.283, 0.139 },
    .{ 0.281, 0.482, 0.331, 0.168 },
    .{ 0.343, 0.545, 0.385, 0.199 },
    .{ 0.406, 0.590, 0.432, 0.229 },
    .{ 0.469, 0.634, 0.486, 0.263 },
    .{ 0.531, 0.674, 0.531, 0.293 },
    .{ 0.593, 0.711, 0.575, 0.324 },
    .{ 0.655, 0.753, 0.618, 0.356 },
    .{ 0.717, 0.790, 0.661, 0.392 },
    .{ 0.780, 0.831, 0.711, 0.436 },
    .{ 0.842, 0.876, 0.759, 0.477 },
    .{ 0.904, 0.906, 0.804, 0.546 },
    .{ 1.051, 0.957, 0.878, 0.539 },
};

fn africaTint(key: f32) [3]f32 {
    const first = africa_lut[0];
    if (key <= first[0]) return .{ first[1] * key / first[0], first[2] * key / first[0], first[3] * key / first[0] };
    for (africa_lut[1..], 1..) |entry, i| {
        if (key <= entry[0]) {
            const prev = africa_lut[i - 1];
            const t = (key - prev[0]) / (entry[0] - prev[0]);
            return .{ prev[1] + (entry[1] - prev[1]) * t, prev[2] + (entry[2] - prev[2]) * t, prev[3] + (entry[3] - prev[3]) * t };
        }
    }
    const last = africa_lut[africa_lut.len - 1];
    return .{ last[1], last[2], last[3] };
}

/// What the Africa tint is looked up by: the luminance, plus how much greener
/// a texel is than grey. Summer camouflage is dark saturated green over a
/// lighter grey or olive base, and Nival's Africa repaint covers it with one
/// sand colour; weighting green lifts the stripes to the base so they fade.
/// 1.3 is where the stripes of 10_5_cm_LeFH18 vanish; the fit to all German
/// pairs is barely worse than luminance alone (0.110 vs 0.107 rms).
pub fn africaKey(r: f32, g: f32, b: f32) f32 {
    return 0.299 * r + 0.587 * g + 0.114 * b + 1.3 * @max(0, g - (r + b) / 2);
}

/// The hue a near-black texel keeps in Africa: the second LUT entry's colour
/// per unit of luminance, so dark gaps stay dark but warm.
const africa_dark = blk: {
    const e = africa_lut[1];
    const lum = 0.299 * e[1] + 0.587 * e[2] + 0.114 * e[3];
    break :blk [3]f32{ e[1] / lum, e[2] / lum, e[3] / lum };
};

fn toUnit(value: u8) f32 {
    return @as(f32, @floatFromInt(value)) / 255.0;
}

fn toByte(value: f32) u8 {
    return @intFromFloat(@round(std.math.clamp(value, 0, 1) * 255.0));
}

/// `u`, `v`: the texel position scaled to a 256-wide texture.
pub fn transformPixel(season: Season, pixel: [4]u8, u: f32, v: f32) [4]u8 {
    const r = toUnit(pixel[0]);
    const g = toUnit(pixel[1]);
    const b = toUnit(pixel[2]);
    const lum = 0.299 * r + 0.587 * g + 0.114 * b;
    const n = seasonNoise(u, v);
    // Near-black parts (background, tyres, gaps) stay dark.
    const keep = std.math.clamp((lum - 0.06) / 0.14, 0, 1);
    switch (season) {
        .winter => {
            // The approved prototype ran on colour-managed values, which lift
            // the texture's darks and mid tones; a luminance gamma of 0.86
            // reproduces it (mean error 1.7/255 on 10_5_cm_Flak38).
            const l = std.math.pow(f32, lum, 0.86);
            const winter_keep = std.math.clamp((l - 0.06) / 0.14, 0, 1);
            const wash = 0.50 + 0.55 * l + (n - 0.5) * 0.16;
            const o = std.math.clamp(l + (@min(1, wash) - l) * winter_keep, 0, 1);
            return .{ toByte(o * 0.99), toByte(o * 0.99), toByte(o * 0.96), pixel[3] };
        },
        .africa => {
            const key = @max(0, africaKey(r, g, b) + (n - 0.5) * 0.08);
            const tan = africaTint(key);
            var out: [4]u8 = undefined;
            for (0..3) |c| out[c] = toByte(lum * africa_dark[c] + (tan[c] - lum * africa_dark[c]) * keep);
            out[3] = pixel[3];
            return out;
        },
    }
}

pub fn transformImage(season: Season, pixels: [][4]u8, width: u32, height: u32) void {
    const scale = 256.0 / @as(f32, @floatFromInt(width));
    for (0..height) |y| for (0..width) |x| {
        const u = @as(f32, @floatFromInt(x)) * scale;
        const v = @as(f32, @floatFromInt(y)) * scale;
        const i = y * width + x;
        pixels[i] = transformPixel(season, pixels[i], u, v);
    };
}

/// The season file for one summer DDS file: the same header and layout, with
/// the transformed top level and mips rebuilt from it.
pub fn makeSeasonDds(allocator: std.mem.Allocator, src: []const u8, season: Season) ![]u8 {
    const header = try parseHeader(src);
    var total: usize = Header.size;
    for (0..header.mips) |level| total += levelSize(header.format, mipDim(header.width, @intCast(level)), mipDim(header.height, @intCast(level)));
    if (src.len < total) return error.TruncatedDds;

    const out = try allocator.dupe(u8, src);
    errdefer allocator.free(out);
    var pixels = try allocator.alloc([4]u8, @as(usize, header.width) * header.height);
    defer allocator.free(pixels);
    var scratch = try allocator.alloc([4]u8, @as(usize, header.width) * header.height);
    defer allocator.free(scratch);

    const top = levelSize(header.format, header.width, header.height);
    decodeLevel(header.format, src[Header.size..][0..top], header.width, header.height, pixels);
    transformImage(season, pixels, header.width, header.height);
    var offset: usize = Header.size;
    var width = header.width;
    var height = header.height;
    for (0..header.mips) |level| {
        if (level > 0) {
            downsample(pixels, width, height, scratch);
            width = @max(1, width / 2);
            height = @max(1, height / 2);
            std.mem.swap([][4]u8, &pixels, &scratch);
        }
        const size = levelSize(header.format, width, height);
        encodeLevel(header.format, pixels[0 .. @as(usize, width) * height], width, height, out[offset..][0..size]);
        offset += size;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Scanning Data

/// Texture bases the engine asks for with a season suffix: the unit ("1"),
/// its wreck ("2") and the outside passengers ("1p").
pub const bases = [_][]const u8{ "1", "2", "1p" };
pub const qualities = [_][]const u8{ "c", "h", "l" };
pub const seasons = [_]Season{ .winter, .africa };

const Folder = struct {
    path: []const u8,
    /// Lower-case file names, sorted.
    files: std.ArrayList([]const u8) = .empty,
    /// Actual names, same order as `files`.
    names: std.ArrayList([]const u8) = .empty,

    fn find(folder: *const Folder, lower: []const u8) ?[]const u8 {
        for (folder.files.items, folder.names.items) |file, name| {
            if (std.mem.eql(u8, file, lower)) return name;
        }
        return null;
    }

    fn isMesh(folder: *const Folder) bool {
        for (folder.files.items) |file| {
            if (std.mem.endsWith(u8, file, ".mod")) return true;
        }
        return false;
    }
};

pub const Job = struct { folder: []const u8, source: []const u8, target: []const u8, season: Season };

fn lessString(_: void, a: []const u8, b: []const u8) bool {
    return std.mem.order(u8, a, b) == .lt;
}

/// The files to generate in one folder: `<base><s>_<q>.dds` for every
/// `<base>_<q>.dds` that exists while the season file does not.
fn planFolder(allocator: std.mem.Allocator, folder: *const Folder, jobs: *std.ArrayList(Job)) !void {
    for (bases) |base| for (seasons) |season| for (qualities) |quality| {
        const source_lower = try std.fmt.allocPrint(allocator, "{s}_{s}.dds", .{ base, quality });
        const target = try std.fmt.allocPrint(allocator, "{s}{s}_{s}.dds", .{ base, season.suffix(), quality });
        const source = folder.find(source_lower) orelse continue;
        if (folder.find(target) != null) continue;
        try jobs.append(allocator, .{ .folder = folder.path, .source = source, .target = target, .season = season });
    };
}

/// Mesh folders (holding a .mod) under `root_path`/`subtree`, sorted by path.
fn scanFolders(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, subtree: []const u8) ![]Folder {
    var dir = try root.openDir(io, subtree, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(allocator);
    defer walker.deinit();
    var map: std.StringArrayHashMapUnmanaged(Folder) = .empty;
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const parent = std.fs.path.dirname(entry.path) orelse "";
        const folder_path = if (parent.len == 0) try allocator.dupe(u8, subtree) else try std.fs.path.join(allocator, &.{ subtree, parent });
        const slot = try map.getOrPut(allocator, folder_path);
        if (!slot.found_existing) slot.value_ptr.* = .{ .path = folder_path };
        const name = try allocator.dupe(u8, entry.basename);
        try slot.value_ptr.names.append(allocator, name);
        try slot.value_ptr.files.append(allocator, try std.ascii.allocLowerString(allocator, name));
    }
    var folders: std.ArrayList(Folder) = .empty;
    for (map.values()) |folder| {
        if (folder.isMesh()) try folders.append(allocator, folder);
    }
    std.mem.sort(Folder, folders.items, {}, struct {
        fn less(_: void, a: Folder, b: Folder) bool {
            return lessString({}, a.path, b.path);
        }
    }.less);
    return folders.items;
}

fn deriveAfricaLut(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, out: *std.Io.Writer) !void {
    const folders = try scanFolders(io, allocator, root, "Units" ++ std.fs.path.sep_str ++ "Technics" ++ std.fs.path.sep_str ++ "German");
    var bins: [16][5]f64 = @splat(@splat(0));
    var pairs: usize = 0;
    for (folders) |*folder| {
        const summer_name = folder.find("1_h.dds") orelse continue;
        const africa_name = folder.find("1a_h.dds") orelse continue;
        const summer_bytes = try root.readFileAlloc(io, try std.fs.path.join(allocator, &.{ folder.path, summer_name }), allocator, .limited(64 << 20));
        const africa_bytes = try root.readFileAlloc(io, try std.fs.path.join(allocator, &.{ folder.path, africa_name }), allocator, .limited(64 << 20));
        const hs = try parseHeader(summer_bytes);
        const ha = try parseHeader(africa_bytes);
        if (hs.width != ha.width or hs.height != ha.height) continue;
        const count = @as(usize, hs.width) * hs.height;
        const summer = try allocator.alloc([4]u8, count);
        const africa = try allocator.alloc([4]u8, count);
        decodeLevel(hs.format, summer_bytes[Header.size..], hs.width, hs.height, summer);
        decodeLevel(ha.format, africa_bytes[Header.size..], ha.width, ha.height, africa);
        pairs += 1;
        for (summer, africa) |s, a| {
            const key: f64 = africaKey(toUnit(s[0]), toUnit(s[1]), toUnit(s[2]));
            const k: usize = @min(15, @as(usize, @intFromFloat(key * 16)));
            bins[k][0] += 1;
            bins[k][1] += key;
            for (0..3) |c| bins[k][2 + c] += @as(f64, @floatFromInt(a[c])) / 255.0;
        }
    }
    try out.print("// {d} German 1/1a pairs: (summer africaKey, Africa r, g, b)\n", .{pairs});
    for (bins) |bin| {
        if (bin[0] == 0) continue;
        try out.print("    .{{ {d:.3}, {d:.3}, {d:.3}, {d:.3} }},\n", .{ bin[1] / bin[0], bin[2] / bin[0], bin[3] / bin[0], bin[4] / bin[0] });
    }
}

fn usage() noreturn {
    std.debug.print(
        \\usage: season_textures <Data dir> --out <dir> [--pak <name>] [--only <path under Data>]
        \\       season_textures <Data dir> --dry-run [--only <path under Data>]
        \\       season_textures <Data dir> --derive-africa-lut
        \\Writes the missing winter (w) and Africa (a) textures of every mesh folder
        \\under <Data dir>/Units (or --only) into <dir>, under the same relative
        \\paths, or with --pak into one stored archive <dir>/<name> the engine
        \\reads as a .pak. Nothing is written into <Data dir>.
        \\
    , .{});
    std.process.exit(2);
}

/// Whether a file under a mesh folder decides what `generate` writes: the
/// folder's .mod (which makes it a mesh folder) and every texture the plan
/// looks at, summer or season. `name` is lower case. build.zig keys the
/// generated tree's cache on these names, so it is rebuilt when a folder
/// gains or loses one.
pub fn isPlanName(name: []const u8) bool {
    if (std.mem.endsWith(u8, name, ".mod")) return true;
    return isSummerSource(name) or isSeasonTarget(name);
}

/// A summer texture a season file is derived from: `<base>_<q>.dds`. Its
/// bytes decide the generated file's, so build.zig makes it a file input.
pub fn isSummerSource(name: []const u8) bool {
    for (bases) |base| for (qualities) |quality| {
        if (textureNameIs(name, base, "", quality)) return true;
    };
    return false;
}

fn isSeasonTarget(name: []const u8) bool {
    for (bases) |base| for (seasons) |season| for (qualities) |quality| {
        if (textureNameIs(name, base, season.suffix(), quality)) return true;
    };
    return false;
}

fn textureNameIs(name: []const u8, base: []const u8, season: []const u8, quality: []const u8) bool {
    var buffer: [32]u8 = undefined;
    const expected = std.fmt.bufPrint(&buffer, "{s}{s}_{s}.dds", .{ base, season, quality }) catch return false;
    return std.mem.eql(u8, name, expected);
}

pub const Stats = struct {
    folders: usize = 0,
    touched: usize = 0,
    textures: [2]usize = .{ 0, 0 },
    files: [2]usize = .{ 0, 0 },
    bytes: [2]usize = .{ 0, 0 },
    failed: usize = 0,
};

/// Where `generate` puts what it makes.
pub const Sink = union(enum) {
    dry_run,
    /// Loose files under the same relative paths as in Data.
    dir: std.Io.Dir,
    /// One archive, entries named by the same relative paths.
    pak: *Pak,
};

/// A stored (uncompressed) zip, which is what the engine reads as a .pak
/// (Sources/src/StreamIOZig/zip.zig). The build ships the generated textures
/// as one: 1428 loose files would take most of the headroom the package's
/// 65,535-entry zip limit has left (tools/zig/package.zig). Entries go in the
/// order they are added, with a fixed date, so the same input gives the same
/// bytes.
pub const Pak = struct {
    bytes: std.ArrayList(u8) = .empty,
    central: std.ArrayList(u8) = .empty,
    count: u16 = 0,

    // 1980-01-01 00:00, the first date MS-DOS time can hold.
    const dos_date: u16 = (1 << 5) | 1;

    fn putInt(list: *std.ArrayList(u8), allocator: std.mem.Allocator, comptime T: type, value: T) !void {
        var buffer: [@sizeOf(T)]u8 = undefined;
        std.mem.writeInt(T, &buffer, value, .little);
        try list.appendSlice(allocator, &buffer);
    }

    /// `name` uses forward slashes, relative to Data.
    pub fn add(pak: *Pak, allocator: std.mem.Allocator, name: []const u8, data: []const u8) !void {
        if (pak.count == std.math.maxInt(u16)) return error.TooManyEntries;
        const offset = std.math.cast(u32, pak.bytes.items.len) orelse return error.ArchiveTooLarge;
        const size = std.math.cast(u32, data.len) orelse return error.ArchiveTooLarge;
        const name_len = std.math.cast(u16, name.len) orelse return error.NameTooLong;
        const crc = std.hash.Crc32.hash(data);

        const local = &pak.bytes;
        try local.appendSlice(allocator, "PK\x03\x04");
        for ([_]u16{ 20, 0, 0, 0, dos_date }) |value| try putInt(local, allocator, u16, value);
        for ([_]u32{ crc, size, size }) |value| try putInt(local, allocator, u32, value);
        for ([_]u16{ name_len, 0 }) |value| try putInt(local, allocator, u16, value);
        try local.appendSlice(allocator, name);
        try local.appendSlice(allocator, data);
        _ = std.math.cast(u32, local.items.len) orelse return error.ArchiveTooLarge;

        const central = &pak.central;
        try central.appendSlice(allocator, "PK\x01\x02");
        for ([_]u16{ 20, 20, 0, 0, 0, dos_date }) |value| try putInt(central, allocator, u16, value);
        for ([_]u32{ crc, size, size }) |value| try putInt(central, allocator, u32, value);
        for ([_]u16{ name_len, 0, 0, 0, 0 }) |value| try putInt(central, allocator, u16, value);
        for ([_]u32{ 0, offset }) |value| try putInt(central, allocator, u32, value);
        try central.appendSlice(allocator, name);
        pak.count += 1;
    }

    /// The whole archive: the entries, the central directory and its end.
    pub fn finish(pak: *Pak, allocator: std.mem.Allocator) ![]const u8 {
        const central_offset = std.math.cast(u32, pak.bytes.items.len) orelse return error.ArchiveTooLarge;
        const central_size = std.math.cast(u32, pak.central.items.len) orelse return error.ArchiveTooLarge;
        try pak.bytes.appendSlice(allocator, pak.central.items);
        try pak.bytes.appendSlice(allocator, "PK\x05\x06");
        for ([_]u16{ 0, 0, pak.count, pak.count }) |value| try putInt(&pak.bytes, allocator, u16, value);
        for ([_]u32{ central_size, central_offset }) |value| try putInt(&pak.bytes, allocator, u32, value);
        try putInt(&pak.bytes, allocator, u16, 0);
        _ = std.math.cast(u32, pak.bytes.items.len) orelse return error.ArchiveTooLarge;
        return pak.bytes.items;
    }
};

/// Derives the season files the mesh folders under `data`/`subtree` lack and
/// puts them in `sink`. Never writes into `data`: what it makes is a build
/// output, staged beside the game's Data as SeasonData and mounted over it.
pub fn generate(io: std.Io, allocator: std.mem.Allocator, data: std.Io.Dir, sink: Sink, subtree: []const u8) !Stats {
    const folders = try scanFolders(io, allocator, data, subtree);
    var jobs: std.ArrayList(Job) = .empty;
    defer jobs.deinit(allocator);
    for (folders) |*folder| try planFolder(allocator, folder, &jobs);

    var stats = Stats{ .folders = folders.len };
    var last_folder: []const u8 = "";
    for (jobs.items) |job| {
        var scratch_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
        defer scratch_state.deinit();
        const scratch = scratch_state.allocator();
        const source_path = try std.fs.path.join(scratch, &.{ job.folder, job.source });
        const target_path = try std.fs.path.join(scratch, &.{ job.folder, job.target });
        const source = try data.readFileAlloc(io, source_path, scratch, .limited(64 << 20));
        const generated = makeSeasonDds(scratch, source, job.season) catch |err| {
            std.debug.print("skip {s}: {s}\n", .{ source_path, @errorName(err) });
            stats.failed += 1;
            continue;
        };
        switch (sink) {
            .dry_run => {},
            .dir => |out_dir| {
                out_dir.createDirPath(io, job.folder) catch |err| {
                    std.debug.print("cannot create {s}: {s}\n", .{ job.folder, @errorName(err) });
                    stats.failed += 1;
                    continue;
                };
                out_dir.writeFile(io, .{ .sub_path = target_path, .data = generated }) catch |err| {
                    std.debug.print("cannot write {s}: {s}\n", .{ target_path, @errorName(err) });
                    stats.failed += 1;
                    continue;
                };
            },
            .pak => |pak| {
                const entry_name = try allocator.dupe(u8, target_path);
                std.mem.replaceScalar(u8, entry_name, std.fs.path.sep, '/');
                try pak.add(allocator, entry_name, generated);
            },
        }
        const s = @intFromEnum(job.season);
        stats.files[s] += 1;
        stats.bytes[s] += generated.len;
        if (std.mem.endsWith(u8, job.target, "_h.dds")) stats.textures[s] += 1;
        if (!std.mem.eql(u8, last_folder, job.folder)) {
            stats.touched += 1;
            last_folder = job.folder;
        }
    }
    return stats;
}

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = init.io;

    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer iterator.deinit();
    _ = iterator.skip();
    var data_path: ?[]const u8 = null;
    var out_path: ?[]const u8 = null;
    var pak_name: ?[]const u8 = null;
    var only: []const u8 = "Units";
    var dry_run = false;
    var derive = false;
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--dry-run")) {
            dry_run = true;
        } else if (std.mem.eql(u8, arg, "--derive-africa-lut")) {
            derive = true;
        } else if (std.mem.eql(u8, arg, "--only")) {
            only = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--out")) {
            out_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--pak")) {
            pak_name = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.startsWith(u8, arg, "-") or data_path != null) {
            usage();
        } else {
            data_path = try arena.dupe(u8, arg);
        }
    }
    const data = data_path orelse usage();
    if (!derive and (out_path == null) == !dry_run) usage();
    if (pak_name != null and out_path == null) usage();
    // Paths under Data use the native separator.
    const only_native = try arena.dupe(u8, std.mem.trimEnd(u8, only, "/\\"));
    for (only_native) |*c| {
        if (c.* == '/' or c.* == '\\') c.* = std.fs.path.sep;
    }

    var root = try std.Io.Dir.cwd().openDir(io, data, .{});
    defer root.close(io);
    var stdout_buffer: [4096]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(io, &stdout_buffer);
    const out = &stdout.interface;

    if (derive) {
        try deriveAfricaLut(io, arena, root, out);
        return out.flush();
    }

    var out_dir: ?std.Io.Dir = null;
    if (out_path) |path| {
        try std.Io.Dir.cwd().createDirPath(io, path);
        out_dir = try std.Io.Dir.cwd().openDir(io, path, .{});
    }
    defer if (out_dir) |*dir| dir.close(io);

    var pak: Pak = .{};
    const sink: Sink = if (out_dir) |dir| (if (pak_name != null) .{ .pak = &pak } else .{ .dir = dir }) else .dry_run;
    const stats = try generate(io, arena, root, sink, only_native);
    if (pak_name) |name| try out_dir.?.writeFile(io, .{ .sub_path = name, .data = try pak.finish(arena) });
    try out.print("{s}mesh folders: {d} under {s}{c}{s}, {d} gain season files\n", .{ if (dry_run) "dry run: " else "", stats.folders, data, std.fs.path.sep, only_native, stats.touched });
    for (seasons) |season| {
        const s = @intFromEnum(season);
        try out.print("  {s}: {d} textures, {d} files, {d} bytes\n", .{ @tagName(season), stats.textures[s], stats.files[s], stats.bytes[s] });
    }
    try out.print("  total: {d} files, {d} bytes{s}\n", .{ stats.files[0] + stats.files[1], stats.bytes[0] + stats.bytes[1], if (stats.failed > 0) " (some failed, see above)" else "" });
    try out.flush();
    if (stats.failed > 0) return error.SomeTexturesFailed;
}

// ---------------------------------------------------------------------------
// Tests

const testing = std.testing;

fn testHeader(width: u32, height: u32, mips: u32, format: Format) [128]u8 {
    var h: [128]u8 = @splat(0);
    @memcpy(h[0..4], "DDS ");
    std.mem.writeInt(u32, h[4..8], 124, .little);
    std.mem.writeInt(u32, h[8..12], 0x1007 | (if (mips > 1) DDSD_MIPMAPCOUNT else 0), .little);
    std.mem.writeInt(u32, h[12..16], height, .little);
    std.mem.writeInt(u32, h[16..20], width, .little);
    std.mem.writeInt(u32, h[28..32], mips, .little);
    std.mem.writeInt(u32, h[76..80], 32, .little);
    switch (format) {
        .dxt => |kind| {
            std.mem.writeInt(u32, h[80..84], DDPF_FOURCC, .little);
            @memcpy(h[84..88], switch (kind) {
                1 => "DXT1",
                3 => "DXT3",
                else => "DXT5",
            });
        },
        .rgb => |rgb| {
            std.mem.writeInt(u32, h[80..84], DDPF_RGB | (if (rgb.masks[3] != 0) DDPF_ALPHAPIXELS else 0), .little);
            std.mem.writeInt(u32, h[88..92], @as(u32, rgb.bytes) * 8, .little);
            for (0..4) |c| std.mem.writeInt(u32, h[92 + 4 * c ..][0..4], rgb.masks[c], .little);
        },
    }
    // A marker in the reserved area proves the header is copied, not rebuilt.
    @memcpy(h[32..40], "NIVALBK1");
    return h;
}

fn testImage(pixels: [][4]u8, width: u32, alpha: bool) void {
    for (pixels, 0..) |*p, i| {
        const x: u32 = @intCast(i % width);
        const y: u32 = @intCast(i / width);
        p.* = .{ @intCast(x * 4 + y * 2), @intCast(40 + x * 2 + y), @intCast(200 - x * 3 + y * 2), if (alpha) @intCast(x * 6 + y * 3) else 255 };
    }
}

fn maxError(a: []const [4]u8, b: []const [4]u8, channels: usize) u32 {
    var worst: u32 = 0;
    for (a, b) |x, y| for (0..channels) |c| {
        worst = @max(worst, @abs(@as(i32, x[c]) - @as(i32, y[c])));
    };
    return worst;
}

test "uncompressed formats round trip exactly" {
    const formats = [_]Format{
        .{ .rgb = .{ .bytes = 4, .masks = .{ 0xff0000, 0xff00, 0xff, 0xff000000 } } },
        .{ .rgb = .{ .bytes = 4, .masks = .{ 0xff0000, 0xff00, 0xff, 0 } } },
        .{ .rgb = .{ .bytes = 2, .masks = .{ 0xf800, 0x7e0, 0x1f, 0 } } },
        .{ .rgb = .{ .bytes = 2, .masks = .{ 0xf00, 0xf0, 0xf, 0xf000 } } },
        .{ .rgb = .{ .bytes = 2, .masks = .{ 0x7c00, 0x3e0, 0x1f, 0x8000 } } },
    };
    for (formats) |format| {
        var raw: [64 * 4]u8 = undefined;
        for (&raw, 0..) |*b, i| b.* = @truncate(i * 37 + 11);
        const size = levelSize(format, 8, 8);
        var pixels: [64][4]u8 = undefined;
        decodeLevel(format, raw[0..size], 8, 8, &pixels);
        var again: [64 * 4]u8 = undefined;
        encodeLevel(format, &pixels, 8, 8, again[0..size]);
        var decoded_again: [64][4]u8 = undefined;
        decodeLevel(format, again[0..size], 8, 8, &decoded_again);
        try testing.expectEqualSlices([4]u8, &pixels, &decoded_again);
        // Bits outside every mask (X8) may change; the masked bits must not.
        if (format.rgb.masks[3] != 0 or format.rgb.bytes == 2 and format.rgb.masks[0] == 0xf800) try testing.expectEqualSlices(u8, raw[0..size], again[0..size]);
    }
}

test "565 expansion matches the byte-replication rule" {
    try testing.expectEqual([3]u8{ 255, 255, 255 }, expand565(0xffff));
    try testing.expectEqual([3]u8{ 0, 0, 0 }, expand565(0));
    try testing.expectEqual(@as(u8, 132), expand565(16 << 11)[0]);
    // The mask codec agrees with the DXT endpoint expansion.
    try testing.expectEqual(expand565(0x8410)[0], expandChannel(0x8410, 0xf800));
}

test "DXT round trips stay within tolerance and solid blocks are exact" {
    for ([_]u8{ 1, 3, 5 }) |kind| {
        const format = Format{ .dxt = kind };
        var pixels: [32 * 16][4]u8 = undefined;
        testImage(&pixels, 32, kind != 1);
        var encoded: [32 * 16]u8 = undefined;
        const size = levelSize(format, 32, 16);
        encodeLevel(format, &pixels, 32, 16, encoded[0..size]);
        var decoded: [32 * 16][4]u8 = undefined;
        decodeLevel(format, encoded[0..size], 32, 16, &decoded);
        // Smooth ramps: an endpoint pair per block holds them to a few steps.
        try testing.expect(maxError(&pixels, &decoded, 3) <= 12);
        var alpha_error: u32 = 0;
        for (pixels, decoded) |p, q| alpha_error = @max(alpha_error, @abs(@as(i32, p[3]) - @as(i32, q[3])));
        try testing.expect(alpha_error <= @as(u32, if (kind == 1) 0 else if (kind == 3) 8 else 6));

        var solid: [16][4]u8 = @splat(.{ 0x84, 0x82, 0x10, 255 }); // 565-representable
        var block: [16]u8 = undefined;
        encodeLevel(format, &solid, 4, 4, block[0..levelSize(format, 4, 4)]);
        var back: [16][4]u8 = undefined;
        decodeLevel(format, block[0..levelSize(format, 4, 4)], 4, 4, &back);
        try testing.expectEqualSlices([4]u8, &solid, &back);
    }
}

test "DXT1 keeps transparent texels transparent" {
    var pixels: [16][4]u8 = undefined;
    for (&pixels, 0..) |*p, i| p.* = if (i % 3 == 0) .{ 0, 0, 0, 0 } else .{ @intCast(40 + i * 2), 90, @intCast(200 - i), 255 };
    var block: [8]u8 = undefined;
    encodeLevel(.{ .dxt = 1 }, &pixels, 4, 4, &block);
    var back: [16][4]u8 = undefined;
    decodeLevel(.{ .dxt = 1 }, &block, 4, 4, &back);
    for (pixels, back) |p, q| {
        try testing.expectEqual(p[3], q[3]);
        if (p[3] == 255) for (0..3) |c| try testing.expect(@abs(@as(i32, p[c]) - @as(i32, q[c])) <= 12);
    }
}

test "odd-sized DXT levels encode edge blocks" {
    var pixels: [6 * 3][4]u8 = undefined;
    testImage(&pixels, 6, false);
    const format = Format{ .dxt = 5 };
    var encoded: [64]u8 = undefined;
    const size = levelSize(format, 6, 3);
    try testing.expectEqual(@as(usize, 32), size);
    encodeLevel(format, &pixels, 6, 3, encoded[0..size]);
    var back: [6 * 3][4]u8 = undefined;
    decodeLevel(format, encoded[0..size], 6, 3, &back);
    try testing.expect(maxError(&pixels, &back, 3) <= 12);
}

test "season file keeps header, size and mip layout" {
    const allocator = testing.allocator;
    const formats = [_]Format{
        .{ .dxt = 1 },
        .{ .dxt = 5 },
        .{ .rgb = .{ .bytes = 4, .masks = .{ 0xff0000, 0xff00, 0xff, 0xff000000 } } },
        .{ .rgb = .{ .bytes = 2, .masks = .{ 0xf000, 0xf0, 0xf, 0xf000 } } },
    };
    for (formats) |format| for ([_]u32{ 1, 4 }) |mips| {
        const header = testHeader(16, 8, mips, format);
        var size: usize = Header.size;
        for (0..mips) |level| size += levelSize(format, mipDim(16, @intCast(level)), mipDim(8, @intCast(level)));
        const src = try allocator.alloc(u8, size + 3); // + trailing bytes the tool must keep
        defer allocator.free(src);
        @memcpy(src[0..128], &header);
        for (src[128..], 0..) |*b, i| b.* = @truncate(i * 13 + 5);
        const out = try makeSeasonDds(allocator, src, .winter);
        defer allocator.free(out);
        try testing.expectEqual(src.len, out.len);
        try testing.expectEqualSlices(u8, src[0..128], out[0..128]);
        try testing.expectEqualSlices(u8, src[size..], out[size..]);
        try testing.expect(!std.mem.eql(u8, src[128..size], out[128..size]));
        // Deterministic: the same input gives the same bytes.
        const again = try makeSeasonDds(allocator, src, .winter);
        defer allocator.free(again);
        try testing.expectEqualSlices(u8, out, again);
    };
}

test "winter whitens mid tones, keeps near-black and alpha" {
    const mid = transformPixel(.winter, .{ 90, 110, 60, 77 }, 13, 29);
    try testing.expect(mid[0] > 170 and mid[1] > 170 and mid[2] > 160);
    try testing.expectEqual(@as(u8, 77), mid[3]);
    try testing.expect(mid[2] <= mid[0]); // slightly warm white, not blue
    const black = transformPixel(.winter, .{ 8, 8, 8, 255 }, 13, 29);
    try testing.expect(black[0] <= 14 and black[0] == black[1] and black[2] <= black[1]);
    try testing.expectEqual(@as(u8, 255), black[3]);
}

test "Africa turns mid tones sand, keeps near-black dark and alpha" {
    const mid = transformPixel(.africa, .{ 70, 100, 50, 200 }, 50, 5);
    try testing.expect(mid[0] > mid[1] and mid[1] > mid[2]); // warm tan
    try testing.expect(mid[0] > 120);
    try testing.expectEqual(@as(u8, 200), mid[3]);
    const black = transformPixel(.africa, .{ 5, 5, 5, 255 }, 50, 5);
    try testing.expect(black[0] <= 8 and black[1] <= 6 and black[2] <= 4);
    // The tint curve follows the measured table and rises monotonically.
    var prev = africaTint(0);
    var key: f32 = 0.05;
    while (key <= 1.0) : (key += 0.05) {
        const t = africaTint(key);
        for (0..3) |c| try testing.expect(t[c] + 0.01 >= prev[c]);
        prev = t;
    }
    try testing.expectApproxEqAbs(africa_lut[8][1], africaTint(africa_lut[8][0])[0], 1e-6);
}

test "noise follows normalised texture coordinates" {
    // Texel 10 of a 128-wide level sits where texel 20 of a 256-wide one does.
    var small: [128 * 2][4]u8 = @splat(.{ 120, 120, 120, 255 });
    var large: [256 * 2][4]u8 = @splat(.{ 120, 120, 120, 255 });
    transformImage(.winter, &small, 128, 2);
    transformImage(.winter, &large, 256, 2);
    try testing.expectEqual(large[20], small[10]);
    try testing.expectEqual(large[256 + 100], small[128 + 50]);
    // Deterministic and in range.
    try testing.expectEqual(seasonNoise(3.5, 7.25), seasonNoise(3.5, 7.25));
    var y: f32 = 0;
    while (y < 50) : (y += 1.7) {
        const n = seasonNoise(y * 1.3, y);
        try testing.expect(n >= 0 and n <= 1);
    }
}

test "plans only missing season files and never an existing one" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var folder = Folder{ .path = "Units/X" };
    for ([_][]const u8{ "1.mod", "1_c.dds", "1_H.dds", "1_l.dds", "1w_c.dds", "1w_h.dds", "1w_l.dds", "2_h.dds", "icon_h.dds" }) |name| {
        try folder.names.append(arena, name);
        try folder.files.append(arena, try std.ascii.allocLowerString(arena, name));
    }
    try testing.expect(folder.isMesh());
    var jobs: std.ArrayList(Job) = .empty;
    try planFolder(arena, &folder, &jobs);
    var targets: std.ArrayList([]const u8) = .empty;
    for (jobs.items) |job| try targets.append(arena, job.target);
    const expected = [_][]const u8{ "1a_c.dds", "1a_h.dds", "1a_l.dds", "2w_h.dds", "2a_h.dds" };
    try testing.expectEqual(expected.len, targets.items.len);
    for (expected, targets.items) |e, t| try testing.expectEqualStrings(e, t);
    // The source keeps the file's real spelling.
    try testing.expectEqualStrings("1_H.dds", jobs.items[1].source);
}

test "generate writes into the output tree and never into Data" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var tmp = testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();

    // A 4x4 A8R8G8B8 summer texture, in a mesh folder that already has its
    // winter "_h" and in a sprite folder (no .mod) that is left alone.
    const format = Format{ .rgb = .{ .bytes = 4, .masks = .{ 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 } } };
    const header = testHeader(4, 4, 1, format);
    var dds: [128 + 64]u8 = undefined;
    @memcpy(dds[0..128], &header);
    for (dds[128..], 0..) |*byte, i| byte.* = @truncate(i * 37 + 11);
    try tmp.dir.createDirPath(io, "Data/Units/Tank");
    try tmp.dir.createDirPath(io, "Data/Units/Soldier");
    try tmp.dir.writeFile(io, .{ .sub_path = "Data/Units/Tank/1.mod", .data = "mesh" });
    try tmp.dir.writeFile(io, .{ .sub_path = "Data/Units/Tank/1_h.dds", .data = &dds });
    try tmp.dir.writeFile(io, .{ .sub_path = "Data/Units/Tank/1w_h.dds", .data = "painted by hand" });
    try tmp.dir.writeFile(io, .{ .sub_path = "Data/Units/Soldier/1_h.dds", .data = &dds });

    var data = try tmp.dir.openDir(io, "Data", .{ .iterate = true });
    defer data.close(io);
    try tmp.dir.createDirPath(io, "out");
    var out = try tmp.dir.openDir(io, "out", .{ .iterate = true });
    defer out.close(io);
    const sep = std.fs.path.sep_str;
    const stats = try generate(io, arena, data, .{ .dir = out }, "Units");
    try testing.expectEqual(@as(usize, 1), stats.folders);
    try testing.expectEqual(@as(usize, 0), stats.failed);
    try testing.expectEqual(@as(usize, 0), stats.files[@intFromEnum(Season.winter)]);
    try testing.expectEqual(@as(usize, 1), stats.files[@intFromEnum(Season.africa)]);

    // The Africa file is in the output, under Data's relative path, and is
    // what makeSeasonDds makes of the summer file.
    const generated = try out.readFileAlloc(io, "Units" ++ sep ++ "Tank" ++ sep ++ "1a_h.dds", arena, .limited(1 << 20));
    try testing.expectEqualSlices(u8, try makeSeasonDds(arena, &dds, .africa), generated);
    // Nothing else is in the output: not the winter file Data has, not the
    // sprite folder's.
    var files: usize = 0;
    var walker = try out.walk(arena);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind == .file) files += 1;
    }
    try testing.expectEqual(@as(usize, 1), files);
    // Data is as it was.
    try testing.expectError(error.FileNotFound, data.access(io, "Units" ++ sep ++ "Tank" ++ sep ++ "1a_h.dds", .{}));
    try testing.expectEqualStrings("painted by hand", try data.readFileAlloc(io, "Units" ++ sep ++ "Tank" ++ sep ++ "1w_h.dds", arena, .limited(64)));

    // The same input gives the same bytes on a second run.
    try tmp.dir.createDirPath(io, "again");
    var again = try tmp.dir.openDir(io, "again", .{});
    defer again.close(io);
    _ = try generate(io, arena, data, .{ .dir = again }, "Units");
    try testing.expectEqualSlices(u8, generated, try again.readFileAlloc(io, "Units" ++ sep ++ "Tank" ++ sep ++ "1a_h.dds", arena, .limited(1 << 20)));

    // As a pak: the same file under the same name, forward slashes, readable
    // by a standard zip reader, and the same bytes on every run.
    var pak: Pak = .{};
    _ = try generate(io, arena, data, .{ .pak = &pak }, "Units");
    const pak_bytes = try pak.finish(arena);
    var pak_again: Pak = .{};
    _ = try generate(io, arena, data, .{ .pak = &pak_again }, "Units");
    try testing.expectEqualSlices(u8, pak_bytes, try pak_again.finish(arena));
    try tmp.dir.writeFile(io, .{ .sub_path = "Season.pak", .data = pak_bytes });
    try tmp.dir.createDirPath(io, "unpacked");
    var unpacked = try tmp.dir.openDir(io, "unpacked", .{});
    defer unpacked.close(io);
    var pak_file = try tmp.dir.openFile(io, "Season.pak", .{});
    defer pak_file.close(io);
    var read_buffer: [4096]u8 = undefined;
    var pak_reader = pak_file.reader(io, &read_buffer);
    try std.zip.extract(unpacked, &pak_reader, .{});
    try testing.expectEqualSlices(u8, generated, try unpacked.readFileAlloc(io, "Units" ++ sep ++ "Tank" ++ sep ++ "1a_h.dds", arena, .limited(1 << 20)));
}

test "plan names cover the textures and meshes the plan reads" {
    for ([_][]const u8{ "1.mod", "1_c.dds", "2_h.dds", "1p_l.dds", "1w_h.dds", "2a_c.dds", "1pw_l.dds" }) |name| try testing.expect(isPlanName(name));
    for ([_][]const u8{ "icon_h.dds", "1b_h.dds", "1_x.dds", "11_h.dds", "1.xml" }) |name| try testing.expect(!isPlanName(name));
    try testing.expect(isSummerSource("1p_h.dds"));
    try testing.expect(!isSummerSource("1w_h.dds"));
}
