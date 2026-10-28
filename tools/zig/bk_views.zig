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
//! `side` makes a variant turned by quarter turns: the building as a few
//! blocks with flat, gabled or hipped roofs fitted to its family's outlines,
//! the 2003 pictures of the variant's colourway projected onto their faces
//! through the exact 2003 camera, a face no picture sees borrowing the look of
//! a matching face one does (the opposite wall, the other roof plane), the
//! last gaps patch-filled, and packed by `bk-sprite compose-pack`.
//!
//! `holdout` and `style` are the quality gate: a side that exists in 2003
//! drawn again without it and scored against it, and generated pictures
//! scored against the building's own 2003 sides.

const std = @import("std");
const bk_tool = @import("bk_tool.zig");

const usage: bk_tool.Usage = .{
    .synopsis =
    \\usage: bk-views decompose <variant-dir> <out-dir>
    \\       bk-views camera <variant-dir>... [--out <camera.json>]
    \\       bk-views inventory <buildings-dir> <out-dir>
    \\       bk-views side <variant-dir> <turns> <out-dir> [<work-dir>]
    \\       bk-views holdout <out-dir> <variant-dir>...
    \\       bk-views style <out-dir> <variant-dir> <pictures-dir>...
    \\       bk-views report <inventory.json> <gate.json> <work-root> <out-dir>
    \\       bk-views contact <buildings-dir> <generated-dir> <out-dir>
    \\       bk-views shape <out-dir> <variant-dir>...
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
        if (ctx.args.len != 4 and ctx.args.len != 5) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return sideCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3], if (ctx.args.len == 5) ctx.args[4] else ctx.args[3]);
    }
    if (std.mem.eql(u8, command, "holdout") or std.mem.eql(u8, command, "style")) {
        const style = std.mem.eql(u8, command, "style");
        if (ctx.args.len < @as(usize, if (style) 4 else 3)) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        if (style) return styleCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3..]);
        return holdoutCommand(ctx, ctx.args[1], ctx.args[2..]);
    }
    if (std.mem.eql(u8, command, "report")) {
        if (ctx.args.len != 5) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return reportCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3], ctx.args[4]);
    }
    if (std.mem.eql(u8, command, "shape")) {
        if (ctx.args.len < 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return shapeCommand(ctx, ctx.args[1], ctx.args[2..]);
    }
    if (std.mem.eql(u8, command, "contact")) {
        if (ctx.args.len != 4) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return contactCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3]);
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
// The pack formats (bk_views_pack.zig)

const formats = @import("bk_views_pack.zig");
pub const findChunk = formats.findChunk;
pub const pack_signature = formats.pack_signature;
pub const Square = formats.Square;
pub const Sprite = formats.Sprite;
pub const Pack = formats.Pack;
pub const decodePack = formats.decodePack;
pub const writePackJson = formats.writePackJson;
pub const Image = formats.Image;
pub const readDds = formats.readDds;
pub const writePng = formats.writePng;
pub const readStoredPng = formats.readStoredPng;
pub const Bounds = formats.Bounds;
pub const unpack = formats.unpack;
pub const Stats = formats.Stats;
pub const readStats = formats.readStats;
const gridRows = formats.gridRows;
const attribute = formats.attribute;
pub const lockedCenter = formats.lockedCenter;
const keepShadow = formats.keepShadow;

// ---------------------------------------------------------------------------
// decompose

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
        // townhouse03_1 holds stray 2gw/3gw packs without textures (its
        // winter ground pictures are 2wg/3wg): no picture, so none to read.
        std.Io.Dir.cwd().access(io, dds_path, .{}) catch continue;
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
        if (e != fixed) eye.shift = footprintShift(footprint, grid, eye.turn);
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

/// The other pictures' zero crosses against a hand shape: from where the hull
/// put them and from the footprint boxes laid on each other, the shift (steps
/// of 4 down to half a world unit) whose outline the blocks cover best. The
/// hull's volume leaves a thin object free along its length: leaders_02's
/// board stood 27 units off leaders_01's (by their 1.xml points).
fn alignToBlocks(fixed: usize, footprints: []const Grid, pictures: []const Image, eyes: []Eye, blocks: []const Block) void {
    for (eyes, footprints, pictures, 0..) |*eye, grid, picture, e| {
        if (e == fixed) continue;
        var best_shift = eye.shift;
        var best: f64 = -1;
        for ([_][2]f64{ eye.shift, footprintShift(footprints[fixed], grid, eye.turn) }) |start| {
            eye.shift = start;
            var iou = blocksIou(blocks, eye.*, picture);
            for ([_]f64{ 4, 2, 1, 0.5 }) |step| {
                var moved = true;
                while (moved) {
                    moved = false;
                    const at = eye.shift;
                    for ([_][2]f64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 } }) |d| {
                        var moved_eye = eye.*;
                        moved_eye.shift = .{ at[0] + d[0] * step, at[1] + d[1] * step };
                        const candidate = blocksIou(blocks, moved_eye, picture);
                        if (candidate <= iou) continue;
                        iou = candidate;
                        eye.shift = moved_eye.shift;
                        moved = true;
                    }
                }
            }
            if (iou > best) {
                best = iou;
                best_shift = eye.shift;
            }
        }
        eye.shift = best_shift;
    }
}

/// A roof over a block: flat, a gable whose ridge runs along x or y, or a hip
/// (every side sloping alike).
pub const Roof = enum { flat, gable_x, gable_y, hip };

/// A part of the building: walls over the rectangle lo..hi up to `wall`, the
/// roof up to `ridge`. Buildings are unions of a few blocks.
pub const Block = struct { lo: [2]f64, hi: [2]f64, wall: f64, ridge: f64, roof: Roof = .flat };

/// A half-space n . p <= d; a block is the intersection of up to nine.
const Plane = struct { n: [3]f64, d: f64 };

fn blockPlanes(b: Block, out: *[9]Plane) []const Plane {
    out[0..5].* = .{
        .{ .n = .{ 1, 0, 0 }, .d = b.hi[0] },
        .{ .n = .{ -1, 0, 0 }, .d = -b.lo[0] },
        .{ .n = .{ 0, 1, 0 }, .d = b.hi[1] },
        .{ .n = .{ 0, -1, 0 }, .d = -b.lo[1] },
        .{ .n = .{ 0, 0, -1 }, .d = 0 },
    };
    const c = [2]f64{ (b.lo[0] + b.hi[0]) / 2, (b.lo[1] + b.hi[1]) / 2 };
    const half = [2]f64{ (b.hi[0] - b.lo[0]) / 2, (b.hi[1] - b.lo[1]) / 2 };
    // Slopes towards +x/-x and +y/-y: z <= wall + s (half - |p - c|).
    const axes: [2]bool = switch (b.roof) {
        .flat => .{ false, false },
        .gable_x => .{ false, true },
        .gable_y => .{ true, false },
        .hip => .{ true, true },
    };
    var n: usize = 5;
    for (0..2) |a| {
        if (!axes[a]) continue;
        const s = (b.ridge - b.wall) / (if (b.roof == .hip) @min(half[0], half[1]) else half[a]);
        for ([2]f64{ 1, -1 }) |sign| {
            var normal = [3]f64{ 0, 0, 1 };
            normal[a] = sign * s;
            out[n] = .{ .n = normal, .d = b.wall + s * half[a] + sign * s * c[a] };
            n += 1;
        }
    }
    if (n == 5) {
        out[n] = .{ .n = .{ 0, 0, 1 }, .d = b.wall };
        n += 1;
    }
    return out[0..n];
}

/// Where a ray meets the building: the distance along it, the block and the
/// outward normal of the face it meets.
pub const Hit = struct { t: f64, block: usize, plane: usize, normal: [3]f64 };

fn dot(a: [3]f64, b: [3]f64) f64 {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

fn castBlock(b: Block, o: [3]f64, d: [3]f64) ?Hit {
    if (b.ridge <= 0) return null;
    var buffer: [9]Plane = undefined;
    const planes = blockPlanes(b, &buffer);
    var near = -std.math.inf(f64);
    var far = std.math.inf(f64);
    var entry: usize = 0;
    for (planes, 0..) |plane, k| {
        const along = dot(plane.n, d);
        const room = plane.d - dot(plane.n, o);
        if (along == 0) {
            if (room < 0) return null;
            continue;
        }
        const t = room / along;
        if (along < 0) {
            if (t > near) {
                near = t;
                entry = k;
            }
        } else far = @min(far, t);
    }
    if (near > far or far < 0 or near == -std.math.inf(f64)) return null;
    const n = planes[entry].n;
    const length = @sqrt(dot(n, n));
    return .{ .t = near, .block = 0, .plane = entry, .normal = .{ n[0] / length, n[1] / length, n[2] / length } };
}

/// The first face along the ray from o in direction d (a unit vector).
pub fn castRay(blocks: []const Block, o: [3]f64, d: [3]f64) ?Hit {
    var best: ?Hit = null;
    for (blocks, 0..) |b, i| {
        var hit = castBlock(b, o, d) orelse continue;
        if (best != null and hit.t >= best.?.t) continue;
        hit.block = i;
        best = hit;
    }
    return best;
}

/// The rays of picture pixels start this far before the picture plane.
const far_away = 4000.0;

/// The face an eye sees through picture point (x, y).
pub fn castPixel(blocks: []const Block, eye: Eye, x: f64, y: f64) ?Hit {
    const r = eye.ray(x, y);
    return castRay(blocks, .{ r[0][0] - far_away * r[1][0], r[0][1] - far_away * r[1][1], r[0][2] - far_away * r[1][2] }, r[1]);
}

/// Does `eye` see the surface point p, nothing standing in front of it?
pub fn sees(blocks: []const Block, eye: Eye, p: [3]f64) bool {
    const d = eye.ray(0, 0)[1];
    const hit = castRay(blocks, .{ p[0] - far_away * d[0], p[1] - far_away * d[1], p[2] - far_away * d[2] }, d) orelse return false;
    return @abs(hit.t - far_away) < 0.5;
}

/// A face of the blocks: block `block`'s half-space `plane` (blockPlanes).
pub const Face = struct { block: usize, plane: usize };

fn facePlane(blocks: []const Block, f: Face) Plane {
    var buffer: [9]Plane = undefined;
    return blockPlanes(blocks[f.block], &buffer)[f.plane];
}

fn normalOf(blocks: []const Block, f: Face) [3]f64 {
    const n = facePlane(blocks, f).n;
    const length = @sqrt(dot(n, n));
    return .{ n[0] / length, n[1] / length, n[2] / length };
}

/// A face's place: across it from its left end seen from outside (0..1) and
/// its height; a flat roof's is across x and y (0..1 each).
fn faceAt(b: Block, plane: Plane, p: [3]f64) [2]f64 {
    if (plane.n[0] == 0 and plane.n[1] == 0) return .{ (p[0] - b.lo[0]) / (b.hi[0] - b.lo[0]), (p[1] - b.lo[1]) / (b.hi[1] - b.lo[1]) };
    // Along the face runs axis a; seen from outside, right is (-n.y, n.x).
    const a: usize = if (plane.n[0] != 0) 1 else 0;
    const right = if (a == 1) plane.n[0] else -plane.n[1];
    const u = (p[a] - b.lo[a]) / (b.hi[a] - b.lo[a]);
    return .{ if (right > 0) u else 1 - u, p[2] };
}

/// The point at place `at` (faceAt) of a face.
fn facePoint(b: Block, plane: Plane, at: [2]f64) [3]f64 {
    if (plane.n[0] == 0 and plane.n[1] == 0) return .{ b.lo[0] + at[0] * (b.hi[0] - b.lo[0]), b.lo[1] + at[1] * (b.hi[1] - b.lo[1]), plane.d };
    const a: usize = if (plane.n[0] != 0) 1 else 0;
    const right = if (a == 1) plane.n[0] else -plane.n[1];
    var q: [3]f64 = undefined;
    q[2] = at[1];
    q[a] = b.lo[a] + (if (right > 0) at[0] else 1 - at[0]) * (b.hi[a] - b.lo[a]);
    q[1 - a] = (plane.d - plane.n[a] * q[a] - plane.n[2] * at[1]) / plane.n[1 - a];
    return q;
}

/// Where a point of face `from` lands on face `to`: at the same place across
/// it, its height scaled so that the eaves and the ridges meet (a flat
/// roof's place turned half round, as a building's opposite side shows it).
pub fn donorPoint(blocks: []const Block, from: Face, to: Face, p: [3]f64) [3]f64 {
    const a = blocks[from.block];
    const b = blocks[to.block];
    const pa = facePlane(blocks, from);
    var at = faceAt(a, pa, p);
    if (pa.n[0] == 0 and pa.n[1] == 0) {
        at = .{ 1 - at[0], 1 - at[1] };
    } else if (at[1] <= a.wall) {
        at[1] *= b.wall / a.wall;
    } else {
        at[1] = b.wall + (at[1] - a.wall) * (b.ridge - b.wall) / @max(0.5, a.ridge - a.wall);
    }
    return facePoint(b, facePlane(blocks, to), at);
}

/// The faces that may lend face f their look where no 2003 picture sees
/// it, best first: faces of its kind (wall, sloped roof or flat roof), its
/// own block's opposite face first, then faces of other blocks alike in
/// width and height, facing the same way, the opposite way or across.
pub fn donorsOf(gpa: std.mem.Allocator, blocks: []const Block, f: Face) ![]Face {
    const Ranked = struct { face: Face, cost: f64 };
    var ranked: std.ArrayList(Ranked) = .empty;
    const pf = facePlane(blocks, f);
    const bf = blocks[f.block];
    for (blocks, 0..) |b, j| {
        var buffer: [9]Plane = undefined;
        for (blockPlanes(b, &buffer), 0..) |pg, k| {
            if ((j == f.block and k == f.plane) or pg.n[2] < 0 or (pg.n[2] == 0) != (pf.n[2] == 0)) continue;
            const flat_f = pf.n[0] == 0 and pf.n[1] == 0;
            const flat_g = pg.n[0] == 0 and pg.n[1] == 0;
            if (flat_f != flat_g) continue;
            var cost: f64 = if (j == f.block) 0 else 1;
            if (!flat_f) {
                const hf = [2]f64{ std.math.sign(pf.n[0]), std.math.sign(pf.n[1]) };
                const hg = [2]f64{ std.math.sign(pg.n[0]), std.math.sign(pg.n[1]) };
                const along = hf[0] * hg[0] + hf[1] * hg[1];
                cost += if (along > 0) 0 else if (along < 0) 0.25 else 0.5;
                const af: usize = if (pf.n[0] != 0) 1 else 0;
                const ag: usize = if (pg.n[0] != 0) 1 else 0;
                const wf = bf.hi[af] - bf.lo[af];
                const wg = b.hi[ag] - b.lo[ag];
                cost += @abs(wf - wg) / @max(wf, wg);
            }
            cost += @abs(bf.ridge - b.ridge) / @max(bf.ridge, b.ridge);
            try ranked.append(gpa, .{ .face = .{ .block = j, .plane = k }, .cost = cost });
        }
    }
    std.mem.sort(Ranked, ranked.items, {}, struct {
        fn less(_: void, x: Ranked, y: Ranked) bool {
            return x.cost < y.cost;
        }
    }.less);
    const faces = try gpa.alloc(Face, ranked.items.len);
    for (faces, ranked.items) |*face, r| face.* = r.face;
    return faces;
}

/// The rectangles of the hull's ground slice within half a cell of the
/// footprint (with single stray voxels removed), largest first: at most six,
/// none under 25 voxels or narrower than 4, each a flat block as high as the
/// hull above it. The start of `fitBlocks`.
pub fn baseBlocks(gpa: std.mem.Allocator, hull: Hull, footprint: Grid) ![]Block {
    const w = hull.n[0];
    const h = hull.n[1];
    const slice = try gpa.alloc(bool, w * h);
    defer gpa.free(slice);
    for (slice, 0..) |*s, i| {
        const c = hull.center(i % w, i / w, 0);
        s.* = hull.cells[i] and for ([_][2]f64{ .{ 0, 0 }, .{ 1, 0 }, .{ -1, 0 }, .{ 0, 1 }, .{ 0, -1 } }) |d| {
            const x = (c[0] + d[0] * tile_size / 2 + footprint.origin[0]) / tile_size;
            const y = (c[1] + d[1] * tile_size / 2 + footprint.origin[1]) / tile_size;
            if (x >= 0 and y >= 0 and occupied(footprint.rows, @intFromFloat(x), @intFromFloat(y))) break true;
        } else false;
    }
    // An opening: a voxel stays where it lies in a full 3x3 square of them.
    const free = try gpa.alloc(bool, w * h);
    defer gpa.free(free);
    @memset(free, false);
    for (1..@max(1, h) - 1) |y| for (1..@max(1, w) - 1) |x| {
        const full = for (0..9) |k| {
            if (!slice[(y + k / 3 - 1) * w + x + k % 3 - 1]) break false;
        } else true;
        if (full) for (0..9) |k| {
            free[(y + k / 3 - 1) * w + x + k % 3 - 1] = true;
        };
    };
    var blocks: std.ArrayList(Block) = .empty;
    while (blocks.items.len < 6) {
        // The largest rectangle of free cells: for each top row and left
        // column, widen right while every row below keeps it free.
        var best = [5]usize{ 0, 0, 0, 0, 0 };
        for (0..h) |y0| for (0..w) |x0| {
            var x1 = x0;
            var y1 = y0;
            while (y1 < h and free[y1 * w + x0]) y1 += 1;
            if (y1 == y0) continue;
            var depth = y1 - y0;
            while (x1 < w and depth > 0) : (x1 += 1) {
                var d: usize = 0;
                while (d < depth and free[(y0 + d) * w + x1]) d += 1;
                depth = d;
                if (depth == 0) break;
                if ((x1 + 1 - x0) * depth > best[4] and depth >= 6 and x1 + 1 - x0 >= 6) best = .{ x0, y0, x1 + 1, y0 + depth, (x1 + 1 - x0) * depth };
            }
        };
        if (best[4] < 25) break;
        var top: usize = 0;
        for (best[1]..best[3]) |y| for (best[0]..best[2]) |x| {
            free[y * w + x] = false;
            for (0..hull.n[2]) |l| {
                if (hull.get(@intCast(x), @intCast(y), @intCast(l))) top = @max(top, l + 1);
            }
        };
        const height = @as(f64, @floatFromInt(top)) * hull.size;
        try blocks.append(gpa, .{
            .lo = .{ hull.lo[0] + @as(f64, @floatFromInt(best[0])) * hull.size, hull.lo[1] + @as(f64, @floatFromInt(best[1])) * hull.size },
            .hi = .{ hull.lo[0] + @as(f64, @floatFromInt(best[2])) * hull.size, hull.lo[1] + @as(f64, @floatFromInt(best[3])) * hull.size },
            .wall = height * 0.6,
            .ridge = height,
        });
    }
    // A small or open object (a barrel, a watchtower on legs) leaves no
    // rectangle that wide: one block over its footprint, as high as the hull.
    if (blocks.items.len == 0) {
        const box = occupiedBox(footprint.rows);
        var top: usize = 2;
        for (hull.cells, 0..) |c, i| {
            if (c) top = @max(top, i / (w * h) + 1);
        }
        const height = @as(f64, @floatFromInt(top)) * hull.size;
        try blocks.append(gpa, .{
            .lo = .{ @as(f64, @floatFromInt(box[0])) * tile_size - footprint.origin[0], @as(f64, @floatFromInt(box[1])) * tile_size - footprint.origin[1] },
            .hi = .{ @as(f64, @floatFromInt(box[2])) * tile_size - footprint.origin[0], @as(f64, @floatFromInt(box[3])) * tile_size - footprint.origin[1] },
            .wall = height * 0.6,
            .ridge = height,
        });
    }
    return blocks.items;
}

/// A picture the blocks are fitted to, sampled every `fit_step` pixels: its
/// eye, its outline (alpha 128 or more) and, per sample, the blocks its ray meets.
const Silhouette = struct { eye: Eye, w: usize, h: usize, inside: []bool, hits: []u8 };

const fit_step = 2;

fn sampleMeets(b: Block, eye: Eye, x: usize, y: usize) bool {
    const r = eye.ray(@as(f64, @floatFromInt(x * fit_step)) + 0.5, @as(f64, @floatFromInt(y * fit_step)) + 0.5);
    return castBlock(b, .{ r[0][0] - far_away * r[1][0], r[0][1] - far_away * r[1][1], r[0][2] - far_away * r[1][2] }, r[1]) != null;
}

/// The samples a block may cover in a silhouette: its box's corners projected.
fn sampleBox(b: Block, s: Silhouette) [4]usize {
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (0..8) |c| {
        const q = s.eye.project(.{ if (c & 1 != 0) b.hi[0] else b.lo[0], if (c & 2 != 0) b.hi[1] else b.lo[1], if (c & 4 != 0) @max(b.wall, b.ridge) else 0 });
        lo = .{ @min(lo[0], q[0]), @min(lo[1], q[1]) };
        hi = .{ @max(hi[0], q[0]), @max(hi[1], q[1]) };
    }
    const clamp = struct {
        fn f(v: f64, n: usize) usize {
            return @intFromFloat(std.math.clamp(v / fit_step, 0, @as(f64, @floatFromInt(n))));
        }
    }.f;
    return .{ clamp(lo[0] - 2, s.w), clamp(lo[1] - 2, s.h), clamp(hi[0] + 3, s.w), clamp(hi[1] + 3, s.h) };
}

/// A block a building may hold: at least 12 world units each way (a thinner
/// slab stands in for a gable end or a wall and shows as a fin turned), a
/// roof no steeper than 1.75 (60 degrees; a steeper one on a narrow block is a
/// spike from behind), and but for a ruin's rubble a roof on walls at least a
/// third of its height (a roof down to the ground stands in for a facade).
fn plausible(b: Block, ruin: bool) bool {
    if (b.hi[0] - b.lo[0] < min_side or b.hi[1] - b.lo[1] < min_side or b.wall < 2 or b.ridge < b.wall) return false;
    if (!ruin and b.roof != .flat and b.wall < min_wall * b.ridge) return false;
    const half = [2]f64{ (b.hi[0] - b.lo[0]) / 2, (b.hi[1] - b.lo[1]) / 2 };
    const across = switch (b.roof) {
        .flat => return true,
        .gable_x => half[1],
        .gable_y => half[0],
        .hip => @min(half[0], half[1]),
    };
    return b.ridge - b.wall <= max_slope * across;
}

const min_side = 12;
const max_slope = 1.75;
const min_wall = 0.33;

/// A block's volume, its roof counted as half the box above the walls.
fn blockVolume(b: Block) f64 {
    return (b.hi[0] - b.lo[0]) * (b.hi[1] - b.lo[1]) * (b.wall + (b.ridge - b.wall) / 2);
}

/// What a fit costs per volume of the start: an outline seen from one side
/// leaves the depth of what stands in front of the building open, and a
/// block grown tall there costs the outline nothing but stands as a tower
/// from behind. Over the 23 hold-out variants, a cost of 0.02 IoU for the
/// start's volume once more gave the best mean outline IoU of the drawn
/// sides (0.862 against 0.854 without and 0.848 at 0.05).
const volume_cost = 0.02;

/// The outline overlap of blocks with pictures, kept per sample so that
/// replacing one block is counted over the samples it may cover only, and
/// the blocks' volume.
const Fit = struct {
    silhouettes: []Silhouette,
    ruin: bool,
    /// A hand shape is refined with its roofs as drawn (`refineBlocks`).
    keep_roofs: bool = false,
    both: i64 = 0,
    either: i64 = 0,
    volume: f64 = 0,
    start: f64 = 1,

    /// The overlap's counts (both, either) with block j replaced by c.
    fn trial(fit: Fit, old: Block, j: usize, c: Block) [2]i64 {
        const bit = @as(u8, 1) << @intCast(j);
        var counts = [2]i64{ fit.both, fit.either };
        for (fit.silhouettes) |s| {
            const a = sampleBox(old, s);
            const b = sampleBox(c, s);
            for (@min(a[1], b[1])..@max(a[3], b[3])) |y| for (@min(a[0], b[0])..@max(a[2], b[2])) |x| {
                const i = y * s.w + x;
                const was = s.hits[i] != 0;
                const now = (s.hits[i] & ~bit) != 0 or sampleMeets(c, s.eye, x, y);
                if (was == now) continue;
                counts[@intFromBool(!s.inside[i])] += if (now) 1 else -1;
            };
        }
        return counts;
    }

    /// The fit's score with these counts and volume: the IoU less the cost of the volume.
    fn score(fit: Fit, counts: [2]i64, total: f64) f64 {
        return @as(f64, @floatFromInt(counts[0])) / @as(f64, @floatFromInt(@max(1, counts[1]))) - volume_cost * total / fit.start;
    }

    fn take(fit: *Fit, old: Block, j: usize, c: Block, counts: [2]i64) void {
        const bit = @as(u8, 1) << @intCast(j);
        for (fit.silhouettes) |s| {
            const a = sampleBox(old, s);
            const b = sampleBox(c, s);
            for (@min(a[1], b[1])..@max(a[3], b[3])) |y| for (@min(a[0], b[0])..@max(a[2], b[2])) |x| {
                const i = y * s.w + x;
                s.hits[i] = (s.hits[i] & ~bit) | (if (sampleMeets(c, s.eye, x, y)) bit else 0);
            };
        }
        fit.both = counts[0];
        fit.either = counts[1];
        fit.volume += blockVolume(c) - blockVolume(old);
    }
};

/// The blocks, from `start`, that best fit the pictures' outlines (the IoU
/// summed over the pictures, less the cost of their volume). Each block first
/// takes its best roof, wall and ridge height on a grid of 8 world units (the
/// ridge no higher than the start's, which is the hull's); then every edge,
/// height and roof of every block is moved while the fit improves, in steps
/// of 8 down to 1.
pub fn fitBlocks(gpa: std.mem.Allocator, start: []const Block, pictures: []const Image, eyes: []const Eye, ruin: bool) ![]Block {
    const blocks = try gpa.dupe(Block, start);
    var fit = try newFit(gpa, blocks, pictures, eyes, ruin);
    for (blocks, 0..) |b, j| bestHeights(&fit, blocks, j, 8, @max(b.wall, b.ridge) + 8, 8);
    descend(&fit, blocks, &.{ 8, 4, 2, 1 });
    while (prune(&fit, blocks)) descend(&fit, blocks, &.{ 4, 2, 1 });
    var kept: std.ArrayList(Block) = .empty;
    for (blocks) |b| if (b.ridge > 0) try kept.append(gpa, b);
    return kept.items;
}

/// A hand shape (`shapes_dir`) moved edge by edge, whole, and up or down
/// while its outlines meet the pictures' better, every block keeping its
/// roof and the roof its pitch: what `shape` offers the author to take over.
pub fn refineBlocks(gpa: std.mem.Allocator, start: []const Block, pictures: []const Image, eyes: []const Eye) ![]Block {
    const blocks = try gpa.dupe(Block, start);
    var fit = try newFit(gpa, blocks, pictures, eyes, false);
    fit.keep_roofs = true;
    descend(&fit, blocks, &.{ 4, 2, 1 });
    return blocks;
}

fn newFit(gpa: std.mem.Allocator, blocks: []const Block, pictures: []const Image, eyes: []const Eye, ruin: bool) !Fit {
    if (blocks.len > max_blocks) return error.TooManyBlocks;
    var fit: Fit = .{ .silhouettes = try gpa.alloc(Silhouette, pictures.len), .ruin = ruin };
    for (blocks) |b| fit.volume += blockVolume(b);
    fit.start = @max(1, fit.volume);
    for (fit.silhouettes, pictures, eyes) |*s, picture, eye| {
        const w = (picture.width + fit_step - 1) / fit_step;
        const h = (picture.height + fit_step - 1) / fit_step;
        s.* = .{ .eye = eye, .w = w, .h = h, .inside = try gpa.alloc(bool, w * h), .hits = try gpa.alloc(u8, w * h) };
        for (0..h) |y| for (0..w) |x| {
            const i = y * w + x;
            s.inside[i] = picture.pixels[y * fit_step * picture.width + x * fit_step][3] >= 128;
            s.hits[i] = 0;
            for (blocks, 0..) |b, j| {
                if (sampleMeets(b, eye, x, y)) s.hits[i] |= @as(u8, 1) << @intCast(j);
            }
            fit.both += @intFromBool(s.inside[i] and s.hits[i] != 0);
            fit.either += @intFromBool(s.inside[i] or s.hits[i] != 0);
        };
    }
    return fit;
}

/// What a fit costs per block: a part the outlines barely need (a slab
/// standing in for a gable end, a tower for a chimney) is dropped, and the
/// blocks left refit to the outlines alone (the gable roof the slab imitated).
const block_cost = 0.03;

/// The most blocks a fit holds (a sample's bit mask is a byte).
const max_blocks = 8;

/// A block taken out of the fit (no ray meets it).
const absent: Block = .{ .lo = .{ 0, 0 }, .hi = .{ 0, 0 }, .wall = 0, .ridge = 0 };

/// Takes out the block whose loss costs the fit least, if that is less than
/// a block's cost; one block always stays.
fn prune(fit: *Fit, blocks: []Block) bool {
    var count: usize = 0;
    for (blocks) |b| count += @intFromBool(b.ridge > 0);
    if (count < 2) return false;
    var best: ?usize = null;
    var best_counts: [2]i64 = undefined;
    var best_score = fit.score(.{ fit.both, fit.either }, fit.volume) - block_cost;
    for (blocks, 0..) |b, j| {
        if (b.ridge <= 0) continue;
        const counts = fit.trial(b, j, absent);
        const score = fit.score(counts, fit.volume - blockVolume(b));
        if (score <= best_score) continue;
        best = j;
        best_counts = counts;
        best_score = score;
    }
    const j = best orelse return false;
    fit.take(blocks[j], j, absent, best_counts);
    blocks[j] = absent;
    return true;
}

/// Block j's best roof with wall and ridge heights from `from` to `to` in `step`s.
fn bestHeights(fit: *Fit, blocks: []Block, j: usize, from: f64, to: f64, step: f64) void {
    const block = blocks[j];
    var best = block;
    var best_counts = [2]i64{ fit.both, fit.either };
    var best_score = fit.score(best_counts, fit.volume);
    for (0..4) |roof| {
        var ridge = from;
        while (ridge <= to) : (ridge += step) {
            var wall: f64 = if (roof == 0) ridge else from;
            while (wall <= ridge) : (wall += step) {
                var c = block;
                c.roof = @fromBackingInt(@intCast(roof));
                c.wall = wall;
                c.ridge = ridge;
                if (!plausible(c, fit.ruin)) continue;
                const counts = fit.trial(block, j, c);
                const score = fit.score(counts, fit.volume - blockVolume(block) + blockVolume(c));
                if (score <= best_score) continue;
                best = c;
                best_counts = counts;
                best_score = score;
            }
        }
    }
    fit.take(block, j, best, best_counts);
    blocks[j] = best;
}

/// Every edge, height and roof of every block moved by each step while the
/// fit improves.
fn descend(fit: *Fit, blocks: []Block, steps: []const f64) void {
    for (steps) |step| {
        var rounds: usize = 0;
        var improved = true;
        while (improved and rounds < 20) : (rounds += 1) {
            improved = false;
            for (blocks, 0..) |*block, j| {
                // A hand shape keeps its roofs and their pitch, and moves whole.
                for (0..22) |v| {
                    if (fit.keep_roofs and v >= 8 and v < 18 and v != 12 and v != 13) continue;
                    if (!fit.keep_roofs and v >= 18) break;
                    var c = block.*;
                    switch (v) {
                        0...3 => c.lo[v % 2] += if (v < 2) step else -step,
                        4...7 => c.hi[v % 2] += if (v < 6) step else -step,
                        8, 9 => c.wall += if (v == 8) step else -step,
                        10, 11 => c.ridge += if (v == 10) step else -step,
                        12, 13 => {
                            c.wall += if (v == 12) step else -step;
                            c.ridge += if (v == 12) step else -step;
                        },
                        14...17 => c.roof = @fromBackingInt(@intCast(v - 14)),
                        else => {
                            const d = if (v % 2 == 0) step else -step;
                            c.lo[(v - 18) / 2] += d;
                            c.hi[(v - 18) / 2] += d;
                        },
                    }
                    if (v >= 14 and v < 18 and c.roof == block.roof) continue;
                    if (if (fit.keep_roofs) c.hi[0] - c.lo[0] < 4 or c.hi[1] - c.lo[1] < 4 or c.wall < 2 else !plausible(c, fit.ruin)) continue;
                    const counts = fit.trial(block.*, j, c);
                    if (fit.score(counts, fit.volume - blockVolume(block.*) + blockVolume(c)) <= fit.score(.{ fit.both, fit.either }, fit.volume)) continue;
                    fit.take(block.*, j, c, counts);
                    block.* = c;
                    improved = true;
                }
            }
        }
    }
}

/// The blocks with the narrow parts of the first picture's outline that
/// stand up above the blocks' outline (3 to 48 pixels wide at their foot, 5
/// or more high): chimneys, towers and spires, which the blocks fitted to
/// the outlines leave out because each costs a block more than it adds to
/// the overlap (block_cost). A part is a flat-topped square block as wide as
/// its foot seen corner on, up to where it narrows below 0.7 of that, and
/// a pyramid on it up to its top where it narrows on. It stands where its
/// foot meets the top of the roof's outline below its middle (a ridge),
/// unless along the first eye's line of sight (where it shows the same) the
/// other pictures' outlines hold more of it, its foot hidden in the roof.
pub fn cutChimneys(gpa: std.mem.Allocator, blocks: []const Block, pictures: []const Image, eyes: []const Eye) ![]const Block {
    const picture = pictures[0];
    const eye = eyes[0];
    const w = picture.width;
    const h = picture.height;
    // Per column, the first row the blocks cover.
    const top = try gpa.alloc(usize, w);
    defer gpa.free(top);
    for (top, 0..) |*t, x| {
        t.* = h;
        for (0..h) |y| {
            if (castPixel(blocks, eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5) == null) continue;
            t.* = y;
            break;
        }
    }
    // The outline above the blocks' top, in runs reaching down to it.
    const above = try gpa.alloc(bool, @as(usize, w) * h);
    defer gpa.free(above);
    @memset(above, false);
    for (0..w) |x| {
        if (top[x] == h) continue;
        var y = top[x];
        while (y > 0 and picture.pixels[(y - 1) * w + x][3] >= 128) : (y -= 1) above[(y - 1) * w + x] = true;
    }
    // Each row's leftmost and rightmost pixel of the part being cut.
    const spans = try gpa.alloc([2]usize, h);
    defer gpa.free(spans);
    const rise = eye.project(.{ 0, 0, 0 })[1] - eye.project(.{ 0, 0, 1 })[1];
    var out: std.ArrayList(Block) = .empty;
    try out.appendSlice(gpa, blocks);
    var stack: std.ArrayList(usize) = .empty;
    defer stack.deinit(gpa);
    for (0..above.len) |seed| {
        if (!above[seed] or out.items.len + 2 > max_blocks) continue;
        var box = [4]usize{ w, h, 0, 0 };
        var count: usize = 0;
        @memset(spans, .{ w, 0 });
        above[seed] = false;
        try stack.append(gpa, seed);
        while (stack.pop()) |i| {
            const x = i % w;
            const y = i / w;
            box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x + 1), @max(box[3], y + 1) };
            spans[y] = .{ @min(spans[y][0], x), @max(spans[y][1], x + 1) };
            count += 1;
            for ([_][2]i64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 } }) |d| {
                const nx = @as(i64, @intCast(x)) + d[0];
                const ny = @as(i64, @intCast(y)) + d[1];
                if (nx < 0 or ny < 0 or nx >= w or ny >= h) continue;
                const n: usize = @intCast(ny * w + nx);
                if (!above[n]) continue;
                above[n] = false;
                try stack.append(gpa, n);
            }
        }
        // The foot: the widest row of the lower half (the roof cuts the
        // lowest rows aslant).
        const height = box[3] - box[1];
        var foot_width: usize = 0;
        var widest = box[3];
        for (box[1] + height / 2..box[3]) |y| {
            if (spans[y][1] -| spans[y][0] <= foot_width) continue;
            foot_width = spans[y][1] - spans[y][0];
            widest = y;
        }
        if (foot_width < 3 or foot_width > 48 or height < 5 or 3 * count < (box[2] - box[0]) * height) continue;
        // The rows up from it as wide as the foot, then the narrowing rest.
        var waist = widest;
        while (waist > box[1] and 10 * (spans[waist - 1][1] -| spans[waist - 1][0]) >= 7 * foot_width) waist -= 1;
        const xc = (box[0] + box[2]) / 2;
        if (top[xc] == h) continue;
        const r = eye.ray(@as(f64, @floatFromInt(xc)) + 0.5, @as(f64, @floatFromInt(top[xc])) + 0.5);
        const hit = castPixel(blocks, eye, @as(f64, @floatFromInt(xc)) + 0.5, @as(f64, @floatFromInt(top[xc])) + 0.5) orelse continue;
        const t = hit.t - far_away;
        const foot = [3]f64{ r[0][0] + t * r[1][0], r[0][1] + t * r[1][1], r[0][2] + t * r[1][2] };
        // A row's height over the foot: the screen moves up as it grows.
        const zOf = struct {
            fn f(z0: f64, row0: usize, row: usize, per: f64) f64 {
                return z0 + (@as(f64, @floatFromInt(row0)) - @as(f64, @floatFromInt(row))) / per;
            }
        }.f;
        const z_waist = zOf(foot[2], top[xc], waist, rise);
        const z_top = zOf(foot[2], top[xc], box[1], rise);
        const pointed = waist - box[1] >= 4;
        const spire_width: f64 = if (pointed) @floatFromInt(@max(3, spans[waist - 1][1] -| spans[waist - 1][0])) else 0;
        const along = [2]f64{ r[1][0], r[1][1] };
        const length = @sqrt(along[0] * along[0] + along[1] * along[1]);
        var best: [2]?Block = .{ null, null };
        var best_score = -std.math.inf(f64);
        var step: f64 = -48;
        while (step <= 48) : (step += 2) {
            // Along the line of sight the part shows the same.
            const c = [2]f64{ foot[0] + step * along[0] / length, foot[1] + step * along[1] / length };
            const dz = step * r[1][2] / length;
            const square = struct {
                fn f(centre: [2]f64, side: f64, wall: f64, ridge: f64, roof: Roof) Block {
                    return .{ .lo = .{ centre[0] - side / 2, centre[1] - side / 2 }, .hi = .{ centre[0] + side / 2, centre[1] + side / 2 }, .wall = wall, .ridge = ridge, .roof = roof };
                }
            }.f;
            const parts: [2]?Block = .{
                if (z_waist > foot[2] + 1) square(c, @as(f64, @floatFromInt(foot_width)) / std.math.sqrt2, z_waist + dz, z_waist + dz, .flat) else null,
                if (pointed) square(c, spire_width / std.math.sqrt2, @max(z_waist, foot[2]) + dz, z_top + dz, .hip) else null,
            };
            // Its foot hidden in the roof, its top above it.
            const under = castRay(blocks, .{ c[0], c[1], z_top + 1000 }, .{ 0, 0, -1 }) orelse continue;
            const roof = z_top + 1000 - under.t;
            if (roof < foot[2] + dz - 1 or roof >= z_top + dz - 2) continue;
            // Its whole foot on the blocks: a stalk beside a wall shows as a
            // post from the other sides.
            const half = @as(f64, @floatFromInt(foot_width)) / std.math.sqrt2 / 2;
            const on_blocks = for ([_][2]f64{ .{ -1, -1 }, .{ -1, 1 }, .{ 1, -1 }, .{ 1, 1 } }) |corner| {
                if (castRay(blocks, .{ c[0] + corner[0] * half, c[1] + corner[1] * half, z_top + 1000 }, .{ 0, 0, -1 }) == null) break false;
            } else true;
            if (!on_blocks) continue;
            var score: f64 = if (step == 0) 0.5 else 0;
            for (pictures[1..], eyes[1..]) |other, other_eye| for (parts) |part| {
                score += @floatFromInt(chimneyFit(blocks, part orelse continue, other, other_eye));
            };
            if (score <= best_score) continue;
            best = parts;
            best_score = score;
        }
        for (best) |part| try out.append(gpa, part orelse continue);
    }
    return out.items;
}

/// The pixels a chimney adds to the blocks' outline in a picture: inside
/// its outline less outside.
fn chimneyFit(blocks: []const Block, chimney: Block, picture: Image, eye: Eye) i64 {
    var score: i64 = 0;
    const box = chimneyBox(chimney, eye, picture);
    for (box[1]..box[3]) |y| for (box[0]..box[2]) |x| {
        const px = @as(f64, @floatFromInt(x)) + 0.5;
        const py = @as(f64, @floatFromInt(y)) + 0.5;
        const r = eye.ray(px, py);
        if (castBlock(chimney, .{ r[0][0] - far_away * r[1][0], r[0][1] - far_away * r[1][1], r[0][2] - far_away * r[1][2] }, r[1]) == null) continue;
        if (castPixel(blocks, eye, px, py) != null) continue;
        score += if (picture.pixels[y * picture.width + x][3] >= 128) 1 else -1;
    };
    return score;
}

/// The pixels of a picture a block's box may cover.
fn chimneyBox(b: Block, eye: Eye, picture: Image) [4]usize {
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (0..8) |c| {
        const q = eye.project(.{ if (c & 1 != 0) b.hi[0] else b.lo[0], if (c & 2 != 0) b.hi[1] else b.lo[1], if (c & 4 != 0) b.ridge else 0 });
        lo = .{ @min(lo[0], q[0]), @min(lo[1], q[1]) };
        hi = .{ @max(hi[0], q[0]), @max(hi[1], q[1]) };
    }
    const clamp = struct {
        fn f(v: f64, n: u32) usize {
            return @intFromFloat(std.math.clamp(v, 0, @as(f64, @floatFromInt(n))));
        }
    }.f;
    return .{ clamp(lo[0] - 1, picture.width), clamp(lo[1] - 1, picture.height), clamp(hi[0] + 2, picture.width), clamp(hi[1] + 2, picture.height) };
}

/// The low parts built on to the walls of the blocks that the picture shows
/// below them: ramps, steps, porches and platforms, which blocks of 12
/// world units or more and the block cost leave out, so that a turn showing
/// that wall from another side keeps them. A run of columns whose outline
/// goes on `step_rows` rows or more below the same wall (`columnFoot`) is a
/// flat block on the ground against that wall, as wide as the lowest pixels
/// of the run reach along the wall where they meet the ground (they are the
/// part's foot; not round the wall's corners), as deep as the median column
/// reaches out (a ladder leaning on the wall reaches further in a few), and
/// half as high as it is deep, 2 to 8 units (what stands up behind its front
/// is hidden by the wall it covers). It is kept where `step_fill` of what it
/// adds to the blocks' outline is the picture's.
pub fn lowParts(gpa: std.mem.Allocator, blocks: []const Block, picture: Image, eye: Eye) ![]Block {
    var out: std.ArrayList(Block) = .empty;
    const depths = try gpa.alloc(f64, picture.width);
    defer gpa.free(depths);
    var x: usize = 0;
    while (x < picture.width) {
        const first = columnFoot(blocks, eye, picture, x) orelse {
            x += 1;
            continue;
        };
        if (first.rows < step_rows) {
            x += 1;
            continue;
        }
        var buffer: [9]Plane = undefined;
        const plane = blockPlanes(blocks[first.block], &buffer)[first.plane];
        const a: usize = if (plane.n[0] != 0) 0 else 1;
        const sign = plane.n[a];
        const wall = plane.d / sign;
        var along = [2]f64{ std.math.inf(f64), -std.math.inf(f64) };
        var end = x;
        while (end < picture.width) : (end += 1) {
            const foot = columnFoot(blocks, eye, picture, end) orelse break;
            if (foot.rows < step_rows or foot.block != first.block or foot.plane != first.plane) break;
            const g = groundPoint(eye, @as(f64, @floatFromInt(end)) + 0.5, @as(f64, @floatFromInt(foot.row + foot.rows)) + 1);
            along = .{ @min(along[0], g[1 - a]), @max(along[1], g[1 - a]) };
            depths[end - x] = (g[a] - wall) * sign;
        }
        const columns = end - x;
        x = end;
        std.mem.sort(f64, depths[0..columns], {}, std.sort.asc(f64));
        const depth = depths[columns / 2];
        const b = blocks[first.block];
        along = .{ @max(along[0], b.lo[1 - a]), @min(along[1], b.hi[1 - a]) };
        if (columns < 4 or along[1] - along[0] < 6 or depth < 3 or depth > step_depth) continue;
        const height = std.math.clamp(depth / 2, 2, 8);
        var step: Block = .{ .lo = undefined, .hi = undefined, .wall = height, .ridge = height };
        step.lo[1 - a] = along[0];
        step.hi[1 - a] = along[1];
        step.lo[a] = @min(wall, wall + sign * depth);
        step.hi[a] = @max(wall, wall + sign * depth);
        // What it adds to the outline, inside the picture's or out.
        var inside: f64 = 0;
        var added: f64 = 0;
        const box = chimneyBox(step, eye, picture);
        for (box[1]..box[3]) |y| for (box[0]..box[2]) |px| {
            const fx = @as(f64, @floatFromInt(px)) + 0.5;
            const fy = @as(f64, @floatFromInt(y)) + 0.5;
            if (castPixel(&.{step}, eye, fx, fy) == null or castPixel(blocks, eye, fx, fy) != null) continue;
            added += 1;
            inside += @floatFromInt(@intFromBool(picture.pixels[y * picture.width + px][3] >= 128));
        };
        if (inside < step_fill * added) continue;
        try out.append(gpa, step);
    }
    return out.items;
}

/// A low part shows this many rows below its wall or more.
const step_rows = 3;
/// A low part reaches this far out from its wall at most (world units); a
/// deeper run is a wall the blocks missed.
const step_depth = 32;
/// The share of what a low part adds to the outline that the picture must hold.
const step_fill = 0.7;

/// Below the blocks in one column of a picture: the wall whose foot is the
/// lowest point the blocks cover there, that row, and how many rows below
/// it the picture's outline (alpha 128 or more) runs down to, where it starts
/// within `foot_gap` rows of it. Null where the blocks cover nothing there or
/// end on a roof.
const ColumnFoot = struct { block: usize, plane: usize, row: usize, rows: usize };

const foot_gap = 12;

fn columnFoot(blocks: []const Block, eye: Eye, picture: Image, x: usize) ?ColumnFoot {
    const px = @as(f64, @floatFromInt(x)) + 0.5;
    var y = picture.height;
    const hit = while (y > 0) {
        y -= 1;
        if (castPixel(blocks, eye, px, @as(f64, @floatFromInt(y)) + 0.5)) |h| break h;
    } else return null;
    if (hit.normal[2] != 0) return null;
    // The ground between a wall and a part's corner shows through.
    var below = y + 1;
    while (below < picture.height and below <= y + foot_gap and picture.pixels[below * picture.width + x][3] < 128) below += 1;
    if (below > y + foot_gap) return .{ .block = hit.block, .plane = hit.plane, .row = y, .rows = 0 };
    while (below < picture.height and picture.pixels[below * picture.width + x][3] >= 128) below += 1;
    const rows = below - 1 - y;
    return .{ .block = hit.block, .plane = hit.plane, .row = y, .rows = rows };
}

/// How a picture keeps given low parts (`lowParts` of other pictures):
/// the pixels they add to the outline of `blocks` (the building without
/// them) under its eye, and how many of those its outline (alpha 128 or
/// more) holds. A turn that drops a ramp a 2003 side shows keeps few.
pub fn lowKept(blocks: []const Block, parts: []const Block, eye: Eye, picture: Image) [2]f64 {
    var seen: f64 = 0;
    var kept: f64 = 0;
    for (0..picture.height) |y| for (0..picture.width) |x| {
        const px = @as(f64, @floatFromInt(x)) + 0.5;
        const py = @as(f64, @floatFromInt(y)) + 0.5;
        if (castPixel(parts, eye, px, py) == null or castPixel(blocks, eye, px, py) != null) continue;
        seen += 1;
        kept += @floatFromInt(@intFromBool(picture.pixels[y * picture.width + x][3] >= 128));
    };
    return .{ seen, kept };
}

/// A turn fails where the low parts the 2003 sides show cover more than
/// `low_margin` pixels of it and it holds less than `low_share` of them.
pub fn lowFails(seen: f64, kept: f64) bool {
    return seen > low_margin and kept < low_share * seen;
}

const low_margin = 40;
const low_share = 0.8;

/// How a drawn picture keeps roof parts (`roof_parts_dir`): the pixels where
/// its eye sees a part's wall, the blocks without the parts (`blocks`)
/// behind it, and how many of those it drew as that wall (its shading
/// class). A turn that paints a vent on the roof plane keeps none.
pub fn roofKept(blocks: []const Block, parts: []const Block, eye: Eye, drawn: Generated) [2]f64 {
    var seen: f64 = 0;
    var kept: f64 = 0;
    const w = drawn.image.width;
    for (0..drawn.image.height) |y| for (0..w) |x| {
        const px = @as(f64, @floatFromInt(x)) + 0.5;
        const py = @as(f64, @floatFromInt(y)) + 0.5;
        const part = castPixel(parts, eye, px, py) orelse continue;
        if (part.normal[2] != 0) continue;
        if (castPixel(blocks, eye, px, py)) |behind| if (behind.t < part.t) continue;
        seen += 1;
        kept += @floatFromInt(@intFromBool(drawn.classes[y * w + x] == shadeClass(part.normal, eye.turn)));
    };
    return .{ seen, kept };
}

/// The overlap of the blocks' outline with a picture's (alpha 128 or more).
pub fn blocksIou(blocks: []const Block, eye: Eye, picture: Image) f64 {
    var both: f64 = 0;
    var either: f64 = 0;
    for (0..picture.height) |y| for (0..picture.width) |x| {
        const in_model = castPixel(blocks, eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5) != null;
        const in_picture = picture.pixels[y * picture.width + x][3] >= 128;
        both += @floatFromInt(@intFromBool(in_model and in_picture));
        either += @floatFromInt(@intFromBool(in_model or in_picture));
    };
    return if (either == 0) 0 else both / either;
}

/// Where grid b's zero cross sits in b's frame when b shows grid a's building
/// turned `turn` times, their occupied boxes laid centre on centre.
fn footprintShift(a: Grid, b: Grid, turn: u2) [2]f64 {
    const ca = turnPoint(boxCentre(a), turn);
    const cb = boxCentre(b);
    return .{ cb[0] - ca[0], cb[1] - ca[1] };
}

fn boxCentre(grid: Grid) [2]f64 {
    const box = occupiedBox(grid.rows);
    return .{
        @as(f64, @floatFromInt(box[0] + box[2])) * tile_size / 2 - grid.origin[0],
        @as(f64, @floatFromInt(box[1] + box[3])) * tile_size / 2 - grid.origin[1],
    };
}

/// The family of the variant folder `dir` with every variant's measured side,
/// and the buildings folder the variants' paths start in.
const InFamily = struct { root: []const u8, family: Family, index: usize };

fn findFamily(gpa: std.mem.Allocator, io: std.Io, dir: []const u8) !InFamily {
    // <root>/<region>/<season>/<variant>, or one level deeper for a numbered
    // subfolder (whose name starts with a digit; one level up it would pass
    // for a family of its own). The season folder is named so in Data.
    const up = struct {
        fn f(path: ?[]const u8) ?[]const u8 {
            return std.fs.path.dirname(path orelse return null);
        }
    }.f;
    const numbered = std.ascii.isDigit(std.fs.path.basename(dir)[0]);
    const season = up(if (numbered) up(dir) else dir) orelse "";
    const named = std.ascii.eqlIgnoreCase(std.fs.path.basename(season), "summer") or std.ascii.eqlIgnoreCase(std.fs.path.basename(season), "winter");
    if (up(up(season))) |root| if (named) if (listFamilies(gpa, io, root)) |families| {
        for (families) |family| for (family.variants, 0..) |variant, i| {
            const path = try std.fs.path.join(gpa, &.{ root, variant.path });
            if (!std.ascii.eqlIgnoreCase(try std.fs.path.resolve(gpa, &.{path}), try std.fs.path.resolve(gpa, &.{dir}))) continue;
            try placeFamily(gpa, io, root, family);
            return .{ .root = root, .family = family, .index = i };
        };
    } else |_| {};
    // A variant folder elsewhere (a modder's, outside any region and season)
    // is a family of its own, side 1.
    const files = entries(gpa, io, dir, .file) catch return error.NotAVariant;
    if (!hasName(files, "1.san")) return error.NotAVariant;
    const name = std.fs.path.basename(dir);
    const variants = try gpa.alloc(Variant, 1);
    variants[0] = .{ .path = name, .suffix = null, .colourway = null, .winter = hasName(files, "1w.san"), .side = 1, .how = "single" };
    return .{ .root = std.fs.path.dirname(dir) orelse ".", .family = .{ .name = name, .region = "", .variants = variants }, .index = 0 };
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

/// The mean colour of each shading class over the 2003 pictures of a damage
/// state, so a face taken from a picture where it was lit as one class can be
/// lit as another. The intact building is relit by the light (`shading`);
/// a ruin's fitted blocks stand for rubble, and relit by the light their
/// faces took seams (e_house07_1's ruin sharpness 1.13 to 2.53, M006 S07d;
/// lit by their intact building's own light, `wallLight`, 8 variants' ruins
/// still measured 1.56 to 1.88 against the gate's 1.55, M006 S07e).
pub const Means = struct {
    sum: [8][3]f64 = std.mem.zeroes([8][3]f64),
    count: [8]f64 = std.mem.zeroes([8]f64),

    pub fn add(means: *Means, class: usize, pixel: [4]u8) void {
        for (0..3) |c| means.sum[class][c] += @floatFromInt(pixel[c]);
        means.count[class] += 1;
    }

    /// The factor that relights a colour channel from class `from` to class
    /// `to`; none where the pictures show `to` on a quarter of the pixels
    /// of `from` or fewer: a few porch roofs stand for another material
    /// than the main roof (admhouse's snowed roof turned came out dark).
    /// Where they show one of the two walls the camera sees on a tenth of
    /// the other's pixels or fewer (a board seen from one side, its end
    /// aside: leaders_02 112 against 1916), the 2003 ratio of the lit wall to
    /// the shaded one relights it (`wall_light`). e_house11_3's ruin (3193
    /// against 14664) and stubtrack/03 (87 against 381) are lit as they are.
    pub fn gain(means: Means, to: usize, from: usize, c: usize) f64 {
        if (to == from or means.count[from] < 100) return 1;
        if (to < 2 and from < 2 and means.count[to] * 10 <= means.count[from]) return if (to == 0) wall_light else 1 / wall_light;
        if (means.count[to] < @max(100, means.count[from] / 4)) return 1;
        return (means.sum[to][c] / means.count[to]) / @max(1, means.sum[from][c] / means.count[from]);
    }
};

/// The brightness of the lit wall (`wall -y`) of a 2003 picture against its
/// shaded wall (`wall +x`): the median over the intact pictures of
/// a_h01/02/03/07/08, e_ctownhouse01/03, e_house01/11, e_shed01,
/// e_townhouse01, ukrainehouse03 and e_frenchshed01 (M005 S08; they spread
/// 1.51 to 2.28; leaders' boards measure 1.93). One factor for the three
/// channels: per channel (1.82, 2.03, 2.71) turned leaders' board blue.
pub const wall_light: f64 = 2.01;

/// How bright the 2003 light makes a surface with normal `n` (in the
/// variant's frame) in a picture whose eye is turned `turn`, for one
/// material: an ambient part and a diffuse part towards `light_2003`, their
/// ratio set by `wall_light` (the shaded wall gets no direct light). It
/// predicts the 2003 roofs too: the roof facing -y against the one facing +x
/// measures 1.26 to 1.42 on e_ctownhouse02_1, e_house02_1, e_house10_2 and
/// e_house11_1 (model 1.40 at a pitch of 1), against the one facing +y 1.76
/// to 2.18 on factoryoffice, storehouse_1 and townhouse02_1 (1.65). M006
/// S07d: the ratio of the shading classes' mean colours, which it replaces,
/// took the materials along (a stone wall in the shade and a plastered one
/// in the light turned e_ctownhouse03_2's walls blue-green, roof pixels in a
/// wall's class turned them grey, and a class seen on a few pixels bleached
/// whole roof planes).
/// Per channel (`wall_light_rgb`): the sun is yellower than the sky that
/// lights the shade, so a shaded wall relit by one factor came out a
/// saturated orange beside the 2003 lit walls' cream (a_h02_2).
/// `wall`: the lit wall against the shaded one per channel (`wallLight`).
pub fn shading(n: [3]f64, turn: u2, wall: [3]f64) [3]f64 {
    const direct = @max(0, dot(n, lightFor(light_2003, turn)));
    var out: [3]f64 = undefined;
    for (&out, wall) |*o, w| o.* = 1 + (w - 1) / -light_2003[1] * direct;
    return out;
}

/// The lit wall against the shaded one per channel for one building: its own
/// 2003 walls (`means` of an intact state's pictures), where both are seen on
/// `wall_pixels` or more and their brightness ratio lies in the spread of the
/// 13 houses `wall_light` was measured on; else `wall_light_rgb`. The ratio
/// is the building's own: a_wb/01's planks measure 1.57, 1.57, 1.64, and its
/// shaded gable relit by the median (1.82, 2.03, 2.71) came out a pale
/// grey-white beside its lit planks (M006 S07e).
pub fn wallLight(means: Means) [3]f64 {
    if (means.count[0] < wall_pixels or means.count[1] < wall_pixels) return wall_light_rgb;
    var out: [3]f64 = undefined;
    var lit: f64 = 0;
    var shaded: f64 = 0;
    for (&out, 0..) |*o, c| {
        const a = means.sum[0][c] / means.count[0];
        const b = @max(1, means.sum[1][c] / means.count[1]);
        o.* = @max(1, a / b);
        lit += a;
        shaded += b;
    }
    if (lit / shaded < wall_light_spread[0] or lit / shaded > wall_light_spread[1]) return wall_light_rgb;
    // Against red, the light's colour lies between none and the median's:
    // where the lit wall is another material its colour is not taken for
    // the light's (e_frenchshed01's cream plaster against its shaded brown
    // stone, green 1.30 against red; factoryfloor01's lit red brick, 0.79).
    for (out[1..], wall_light_rgb[1..]) |*o, m| o.* = std.math.clamp(o.*, out[0], out[0] * m / wall_light_rgb[0]);
    return out;
}

/// The brightness ratios of the lit wall to the shaded one of the 13 houses
/// `wall_light` was measured on (M005 S08).
pub const wall_light_spread = [2]f64{ 1.5, 2.3 };
/// Each wall seen on this many pixels or more gives a building its own light.
pub const wall_pixels = 500;

/// The lit wall against the shaded one per channel: the medians of the 13
/// houses `wall_light` was measured on (M005 S08).
pub const wall_light_rgb = [3]f64{ 1.82, 2.03, 2.71 };

/// A pixel relit from a surface lit `from` to one lit `to` (`shading`), its
/// hue kept where a channel would clip: a clipped channel turned bright red
/// roofs yellow.
pub fn relight(pixel: [4]u8, to: [3]f64, from: [3]f64) [4]u8 {
    var v: [3]f64 = undefined;
    for (&v, 0..) |*c, ch| c.* = @as(f64, @floatFromInt(pixel[ch])) * to[ch] / from[ch];
    const scale = @min(1, 255 / @max(1, @max(v[0], v[1], v[2])));
    var out = pixel;
    for (0..3) |ch| out[ch] = @intFromFloat(@round(v[ch] * scale));
    return out;
}

/// Where a pixel of a generated picture comes from: a 2003 picture seeing
/// its point, one seeing the matching point of a face of the same block
/// (mirrored: the opposite wall, the other roof plane), of a face of another
/// block (transferred), or the patch fill.
pub const Kind = enum(u8) { empty, projected, mirrored, transferred, filled };

/// One 2003 picture as a projection source: its eye and its picture of the
/// state being drawn (null where the variant lacks it).
pub const Projector = struct { eye: Eye, picture: ?Image };

/// A generated picture with the source of each pixel and its shading class.
/// `stretched`: the pixels `projectPicture` drew from a source pixel spread
/// over `stretch_pixels` or more of them (`stretchedPixels`); empty for
/// pictures it did not draw.
pub const Generated = struct { image: Image, kinds: []Kind, classes: []u8, stretched: []const bool = &.{}, sources: []const [3]f64 = &.{} };

/// Whether point q of a face lies on or by a low or roof part: within
/// `part_margin` of its rectangle and no higher than `reach` above its top.
pub fn nearPart(parts: []const Block, q: [3]f64, reach: f64) bool {
    for (parts) |b| {
        if (q[0] >= b.lo[0] - part_margin and q[0] <= b.hi[0] + part_margin and q[1] >= b.lo[1] - part_margin and q[1] <= b.hi[1] + part_margin and q[2] <= @max(b.wall, b.ridge) + reach) return true;
    }
    return false;
}

/// How far above a part `nearPart` reaches on a face whose normal is `n`:
/// on a wall `part_reach` (a door over its step), on a roof `part_margin`,
/// as a part's pixels end at its own roof (a dormer's flashing). With the
/// wall's reach on roofs, the tiles above e_townhouse02's dormers, up to its
/// ridge, were left to the patch fill (M006 S07e).
pub fn partReach(n: [3]f64) f64 {
    return if (n[2] > 0.5) part_margin else part_reach;
}

const part_margin = 6;
const part_reach = 40;

/// A donor point on a roof by a part (`nearPart`) moved across its face, at
/// its height, by the width the part keeps clear (its own and the margins),
/// once or more, to where no part stands by; null on a wall or where it
/// leaves the face. The roof beside a front's dormers lends the back its
/// tiles: left to the patch fill, those points drew a quarter of
/// e_townhouse02's and e_jettybuilding's turned roofs as blotches (M006
/// S07e). A wall keeps the patch fill: a door step runs along most of a
/// facade, and moved past it the facade's end repeated in stripes.
fn slidePast(blocks: []const Block, body: usize, to: Face, q: [3]f64) ?[3]f64 {
    const b = blocks[to.block];
    const plane = facePlane(blocks, to);
    if (plane.n[2] <= 0) return null;
    const at = faceAt(b, plane, q);
    const a: usize = if (plane.n[0] != 0 and plane.n[1] == 0) 1 else 0;
    const width = b.hi[a] - b.lo[a];
    const clear = for (blocks[body..]) |part| {
        if (nearPart(&.{part}, q, part_margin)) break part.hi[a] - part.lo[a] + 2 * part_margin;
    } else return null;
    for (1..4) |j| for ([2]f64{ 1, -1 }) |sign| {
        const u = at[0] + sign * @as(f64, @floatFromInt(j)) * clear / width;
        if (u < 0 or u > 1) continue;
        const p = facePoint(b, plane, .{ u, at[1] });
        if (!nearPart(blocks[body..], p, part_margin)) return p;
    };
    return null;
}

/// The pixels of a picture `projectPicture` drew on a face of the body
/// (`blocks[..body]`) from a donor point near a part (`nearPart`): a back
/// wall or roof plane that repeats the chimney, dormers or door of the face
/// it copies.
pub fn copiedParts(blocks: []const Block, body: usize, eye: Eye, drawn: Generated) f64 {
    var n: f64 = 0;
    if (drawn.sources.len == 0) return 0;
    const w = drawn.image.width;
    for (drawn.kinds, drawn.sources, 0..) |kind, q, i| {
        if (kind != .mirrored and kind != .transferred) continue;
        const hit = castPixel(blocks, eye, @as(f64, @floatFromInt(i % w)) + 0.5, @as(f64, @floatFromInt(i / w)) + 0.5) orelse continue;
        if (hit.block < body and nearPart(blocks[body..], q, partReach(hit.normal))) n += 1;
    }
    return n;
}

/// The target's picture: every pixel the blocks cover takes the colour of a
/// 2003 picture that sees its point, relit to the target's shading class. A
/// point none sees borrows from the first of its face's donors (donorsOf)
/// whose matching point one sees: the opposite wall, the other roof plane.
/// Of the pictures that see it, one that lit the face as the same class
/// beats one that did not, then the one facing the surface most squarely
/// wins. `blocks[body..]` are the low and roof parts: a face of the body
/// borrows no point near one (`nearPart`), so a house's back does not repeat
/// the chimney, dormers and door steps of its front. Pixels none sees either
/// way are left for `patchFill`.
pub fn projectPicture(gpa: std.mem.Allocator, blocks: []const Block, body: usize, means: ?Means, wall: [3]f64, target: Eye, size: [2]u32, projectors: []const Projector) !Generated {
    const count = @as(usize, size[0]) * size[1];
    const uvs = try gpa.alloc([2]f64, count);
    const sources = try gpa.alloc([3]f64, count);
    const ids = try gpa.alloc(usize, count);
    @memset(ids, 0);
    var out: Generated = .{
        .image = .{ .width = size[0], .height = size[1], .pixels = try gpa.alloc([4]u8, count) },
        .kinds = try gpa.alloc(Kind, count),
        .classes = try gpa.alloc(u8, count),
        .sources = sources,
    };
    @memset(out.image.pixels, .{ 0, 0, 0, 0 });
    @memset(out.kinds, .empty);
    @memset(out.classes, 255);
    const donor_table = try gpa.alloc(?[]Face, blocks.len * 9);
    @memset(donor_table, null);
    for (0..size[1]) |y| for (0..size[0]) |x| {
        const i = y * size[0] + x;
        const r = target.ray(@as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
        const hit = castPixel(blocks, target, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5) orelse continue;
        const t = hit.t - far_away;
        const p = [3]f64{ r[0][0] + t * r[1][0], r[0][1] + t * r[1][1], r[0][2] + t * r[1][2] };
        const class = shadeClass(hit.normal, target.turn);
        out.classes[i] = @intCast(class);
        out.kinds[i] = .filled;
        var best_score: f64 = -std.math.inf(f64);
        const face: Face = .{ .block = hit.block, .plane = hit.plane };
        const donors = &donor_table[face.block * 9 + face.plane];
        if (donors.* == null) donors.* = try donorsOf(gpa, blocks, face);
        // A face seen at a grazing angle smears its pixels across the
        // target: one a picture sees more squarely, its own or a donor,
        // goes first.
        for ([_]f64{ grazing, -1 }) |least| for (0..donors.*.?.len + 1) |k| {
            if (out.kinds[i] != .filled) break;
            var q = if (k == 0) p else donorPoint(blocks, face, donors.*.?[k - 1], p);
            if (k > 0 and face.block < body and nearPart(blocks[body..], q, partReach(hit.normal))) q = slidePast(blocks, body, donors.*.?[k - 1], q) orelse continue;
            const nq = if (k == 0) hit.normal else normalOf(blocks, donors.*.?[k - 1]);
            for (projectors, 0..) |projector, e| {
                const source = projector.picture orelse continue;
                const facing = -dot(nq, projector.eye.ray(0, 0)[1]);
                if (facing < least) continue;
                if (!sees(blocks, projector.eye, q)) continue;
                const uv = projector.eye.project(q);
                const pixel = (source.at(@intFromFloat(@floor(uv[0])), @intFromFloat(@floor(uv[1]))) orelse continue).*;
                if (pixel[3] < 128) continue;
                const from = shadeClass(nq, projector.eye.turn);
                const score = @as(f64, if (from == class) 2 else 0) + facing;
                if (score <= best_score) continue;
                best_score = score;
                if (means) |m| {
                    for (0..3) |ch| out.image.pixels[i][ch] = @intFromFloat(@min(255, @round(@as(f64, @floatFromInt(pixel[ch])) * m.gain(class, from, ch))));
                } else out.image.pixels[i] = relight(pixel, shading(hit.normal, target.turn, wall), shading(nq, projector.eye.turn, wall));
                out.image.pixels[i][3] = 255;
                out.kinds[i] = if (k == 0) .projected else if (donors.*.?[k - 1].block == face.block) .mirrored else .transferred;
                uvs[i] = uv;
                sources[i] = q;
                const source_face = if (k == 0) face else donors.*.?[k - 1];
                ids[i] = 1 + (e * blocks.len + source_face.block) * 9 + source_face.plane;
            }
        };
    };
    out.stretched = try stretchedPixels(gpa, size, uvs, ids);
    // A stretched pixel is left for the patch fill, as one no picture sees
    // (S07c: a donor face smaller than the face it paints, a wall seen edge
    // on, spread railway station and shed roofs, a town house's gable and
    // thatch in streaks).
    for (out.kinds, out.stretched) |*kind, s| {
        if (s) kind.* = .filled;
    }
    return out;
}

/// A pixel is stretched where it and its right and lower neighbours take
/// one face of one 2003 picture and that picture's pixel covers
/// `stretch_pixels` or more of the target's: a face the picture sees nearly
/// edge on, a dormer's cheek or a mound's slope drawn over a whole roof
/// plane or wall, smears the few pixels it has.
pub fn stretchedPixels(gpa: std.mem.Allocator, size: [2]u32, uvs: []const [2]f64, ids: []const usize) ![]bool {
    const out = try gpa.alloc(bool, ids.len);
    @memset(out, false);
    const w: usize = size[0];
    for (0..size[1] -| 1) |y| for (0..w -| 1) |x| {
        const i = y * w + x;
        if (ids[i] == 0 or ids[i + 1] != ids[i] or ids[i + w] != ids[i]) continue;
        const a = [2]f64{ uvs[i + 1][0] - uvs[i][0], uvs[i + 1][1] - uvs[i][1] };
        const b = [2]f64{ uvs[i + w][0] - uvs[i][0], uvs[i + w][1] - uvs[i][1] };
        // The source area of one target pixel. Along one direction alone a
        // wall seen at a quarter of the target's cosine is stretched 4 times,
        // and the patch fill drew such walls worse (e_townrailwaystation_4).
        out[i] = @abs(a[0] * b[1] - a[1] * b[0]) * stretch_pixels < 1;
    };
    return out;
}

/// A source pixel spread over this many target pixels or more is stretched.
pub const stretch_pixels = 4;

/// The share of a picture's drawn pixels that still show a stretched source
/// pixel, not the patch fill.
pub fn stretchShare(g: Generated) f64 {
    if (g.stretched.len == 0) return 0;
    var drawn: f64 = 0;
    var stretched: f64 = 0;
    for (g.kinds, g.stretched) |kind, s| {
        if (kind == .empty) continue;
        drawn += 1;
        stretched += @floatFromInt(@intFromBool(s and kind != .filled));
    }
    return if (drawn == 0) 0 else stretched / drawn;
}

/// The cosine of the angle between a face and a picture's line of sight
/// under which the picture sees the face at a grazing angle.
const grazing = 0.25;

/// Fills every `filled` pixel from the picture's own projected and borrowed
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
        d.* = kind != .empty and kind != .filled;
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
        bands[hueBand(p) orelse continue] += 1;
        total += 1;
    }
    if (total > 0) for (&bands) |*band| {
        band.* /= total;
    };
    return bands;
}

/// The 30 degree hue band of a colour with a chroma of 20 or more.
fn hueBand(p: [4]u8) ?usize {
    const r: f64 = @floatFromInt(p[0]);
    const g: f64 = @floatFromInt(p[1]);
    const b: f64 = @floatFromInt(p[2]);
    const max = @max(r, g, b);
    const chroma = max - @min(r, g, b);
    if (chroma < 20) return null;
    const hue = if (max == r) @mod((g - b) / chroma, 6) else if (max == g) (b - r) / chroma + 2 else (r - g) / chroma + 4;
    return @as(usize, @intFromFloat(hue * 2)) % 12;
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

const Turned = struct { turn: u2, eye: Eye, size: [2]u32, name: []const u8 };

/// The 2003 light, towards it in the camera's frame. Fitted to the 2003
/// shadow pictures of e_ctownhouse01, a_h07, e_house01, e_house11 and
/// townhouse05 (M005 S02/S03), it came out at azimuth 232 to 258 and elevation
/// 67.5 to 75, the spread of the hull's shape: one light lit every building.
pub const light_2003 = lightDirection(245, 67.5);

/// A variant's family set up to draw that variant from other sides: the 2003
/// variants with their eyes aligned to it (`fixed` is the variant itself),
/// which of them share its colourway, and the building's blocks in each
/// damage state (and season).
const Scene = struct {
    family: []const u8,
    name: []const u8,
    own_side: u8,
    sources: []Source,
    eyes: []Eye,
    same: []bool,
    fixed: usize,
    outlines: []Image,
    states: []const []const u8,
    shapes: []const []const Block,
    /// How many of each state's blocks are the building's; the low and roof
    /// parts follow them.
    bodies: []const usize,
    /// The variant's hand shape (`shapes_dir`): its intact blocks but for
    /// the chimneys cut from the outline.
    hand: ?[]const Block,
    /// The intact blocks without the low parts (`lowParts`), which the gate
    /// finds below them.
    body: []const Block,
    /// The roof parts the 2003 variants of the colourway show (`roof_parts_dir`),
    /// in the variant's frame.
    roof: []const Block,
    /// The family is a heap (props_file): its turns are its 2003 pictures.
    prop: bool = false,
};

/// Hand shapes, from the working folder (the build root): `<family>.json`
/// maps a variant folder's name to its intact blocks in its own frame (world
/// units from its zero cross, as sources.json lists them), for buildings the
/// fit cannot draw (wings, domes, open props). `side` and `holdout` take them
/// instead of the fitted intact blocks; the damage states are fitted from them.
pub const shapes_dir = "tools/zig/fixtures/bk-views/shapes";

/// A modder's hand shape: the blocks alone, in the variant's own folder.
pub const variant_shape = "shape.json";

/// Roof parts, from the working folder: `<family>.json` maps a variant
/// folder's name to the vents, dormers and roof turrets its 2003 pictures
/// show, as blocks in its own frame (the format of `shapes_dir`). Their
/// outline hardly leaves the roof's (e_wb/01's ridge vents rise 0 to 2
/// pixels above the fitted ridge) and the fit takes them into the roof, so
/// they are drawn by hand; every variant of the colourway takes them, turned
/// into its frame (`intoFrame`), on its intact blocks.
pub const roof_parts_dir = "tools/zig/fixtures/bk-views/roof-parts";

/// Heaps, from the working folder: family name to the reason its turns are
/// its 2003 pictures (sameSprite) and never blocks. A supply depot (crates,
/// barrels and a tarpaulin or tent over them) or an earth-covered pillbox
/// looks alike from every side, and no few boxes draw it: drawn by blocks,
/// its crates and earth were smeared over plain walls (M006 S07c).
pub const props_file = "tools/zig/fixtures/bk-views/props.json";

/// A block of the frame of the picture seen by `eye` in the frame the eye
/// is aligned to: the inverse of `Eye.project`'s turn and shift.
pub fn intoFrame(b: Block, eye: Eye) Block {
    const back: u2 = 0 -% eye.turn;
    const p = turnPoint(.{ b.lo[0] - eye.shift[0], b.lo[1] - eye.shift[1] }, back);
    const q = turnPoint(.{ b.hi[0] - eye.shift[0], b.hi[1] - eye.shift[1] }, back);
    var out = b;
    out.lo = .{ @min(p[0], q[0]), @min(p[1], q[1]) };
    out.hi = .{ @max(p[0], q[0]), @max(p[1], q[1]) };
    if (eye.turn % 2 == 1) out.roof = switch (b.roof) {
        .gable_x => .gable_y,
        .gable_y => .gable_x,
        else => b.roof,
    };
    return out;
}

fn handShape(ctx: *bk_tool.Context, dir: []const u8, family: []const u8, variant: []const u8) !?[]const Block {
    if (try readJson([]const Block, ctx, try std.fs.path.join(ctx.arena, &.{ dir, variant_shape }))) |blocks| return blocks;
    const path = try std.fmt.allocPrint(ctx.arena, "{s}/{s}.json", .{ shapes_dir, family });
    const shapes = try readJson(std.json.ArrayHashMap([]const Block), ctx, path) orelse return null;
    return shapes.map.get(variant);
}

/// The scene of variant `index` of the family; variants that show side
/// `skip` are left out, so the hold-out draws that side without them.
fn loadScene(ctx: *bk_tool.Context, found: InFamily, index: usize, skip: ?u8, intact_only: bool) !Scene {
    const arena = ctx.arena;
    // Every measured variant gives the shape, those of the variant's own
    // colourway also give pixels. A variant whose side the inventory could
    // not measure is drawn from itself alone, as a single-sprite family.
    const unresolved = found.family.variants[index].side == null;
    const own_side = found.family.variants[index].side orelse 1;
    var sources: std.ArrayList(Source) = .empty;
    var fixed: usize = 0;
    for (found.family.variants, 0..) |variant, i| {
        const side = if (i == index) own_side else if (unresolved) continue else variant.side orelse continue;
        if (skip == side) continue;
        if (i == index) fixed = sources.items.len;
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

    // The intact building's blocks start from the rectangles of its hull's
    // ground slice; every other state's from the intact ones.
    const own = sources.items[fixed];
    const hand = try handShape(ctx, own.dir, found.family.name, std.fs.path.basename(own.dir));
    if (hand) |blocks| alignToBlocks(fixed, footprints, outlines, eyes, blocks);
    // The roof parts of every variant of the colourway, in this one's frame.
    var roof: std.ArrayList(Block) = .empty;
    const roof_path = try std.fmt.allocPrint(arena, "{s}/{s}.json", .{ roof_parts_dir, found.family.name });
    if (try readJson(std.json.ArrayHashMap([]const Block), ctx, roof_path)) |parts| {
        for (sources.items, eyes, same) |source, eye, s| {
            if (!s) continue;
            for (parts.map.get(std.fs.path.basename(source.dir)) orelse continue) |b| try roof.append(arena, intoFrame(b, eye));
        }
    }
    var states: std.ArrayList([]const u8) = .empty;
    for (own.names) |state| {
        const digits = std.mem.indexOfNone(u8, state, "0123456789") orelse state.len;
        if (intact_only and !std.mem.eql(u8, state, "1")) continue;
        if (digits > 0 and (digits == state.len or std.mem.eql(u8, state[digits..], "w"))) try states.append(arena, state);
    }
    const shapes = try arena.alloc([]const Block, states.items.len);
    const bodies = try arena.alloc(usize, states.items.len);
    var intact: []const Block = hand orelse try baseBlocks(arena, try carve(arena, footprints[fixed], 2, outlines, eyes), footprints[fixed]);
    var body: []const Block = intact;
    for (states.items, shapes, bodies) |state, *shape, *body_count| {
        var state_eyes: std.ArrayList(Eye) = .empty;
        var state_pictures: std.ArrayList(Image) = .empty;
        for (sources.items, eyes, 0..) |source, eye, e| {
            // The variant's own picture first: chimneys are cut from it.
            try state_pictures.insert(arena, if (e == fixed) 0 else state_pictures.items.len, pictureOf(source, state) orelse continue);
            try state_eyes.insert(arena, if (e == fixed) 0 else state_eyes.items.len, eye);
        }
        // A hand shape is the intact building, in summer and in winter, its
        // chimneys and spires drawn by its author.
        // So is a damage state whose own picture it still covers (a
        // tarpaulin stack whose damaged picture differs by a few crates).
        if (hand != null and (std.mem.eql(u8, state, "1") or std.mem.eql(u8, state, "1w") or blocksIou(hand.?, state_eyes.items[0], state_pictures.items[0]) >= outline_floor)) {
            shape.* = hand.?;
        } else {
            const fitted = try fitBlocks(arena, intact, state_pictures.items, state_eyes.items, state[0] == '3');
            shape.* = try cutChimneys(arena, fitted, state_pictures.items, state_eyes.items);
            if (std.mem.eql(u8, state, "1")) {
                intact = fitted;
                body = shape.*;
            }
        }
        // The low parts the pictures of the colourway show: a ramp one side
        // shows stays on every turn.
        var parts: std.ArrayList(Block) = .empty;
        try parts.appendSlice(arena, shape.*);
        body_count.* = shape.len;
        for (sources.items, eyes, same) |source, eye, s| {
            if (s) try parts.appendSlice(arena, try lowParts(arena, shape.*, pictureOf(source, state) orelse continue, eye));
        }
        // The intact building's roof parts.
        if (std.mem.eql(u8, state, "1") or std.mem.eql(u8, state, "1w")) try parts.appendSlice(arena, roof.items);
        shape.* = parts.items;
    }
    return .{
        .family = found.family.name,
        .name = std.fs.path.basename(own.dir),
        .own_side = own_side,
        .sources = sources.items,
        .eyes = eyes,
        .same = same,
        .fixed = fixed,
        .outlines = outlines,
        .states = states.items,
        .shapes = shapes,
        .bodies = bodies,
        .hand = hand,
        .body = body,
        .roof = roof.items,
        .prop = if (try readJson(std.json.ArrayHashMap([]const u8), ctx, props_file)) |props| props.map.contains(found.family.name) else false,
    };
}

/// Each turn's canvas: it holds every state's blocks, their shadows and the
/// 2003 ground pictures.
fn turnTarget(scene: Scene, k: u2, name: []const u8) Turned {
    var eye: Eye = .{ .anchor = .{ 0, 0 }, .turn = k };
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    const l = lightFor(light_2003, k);
    for (scene.shapes) |shape| for (shape) |b| for (0..8) |c| {
        const p = [3]f64{ if (c & 1 != 0) b.hi[0] else b.lo[0], if (c & 2 != 0) b.hi[1] else b.lo[1], if (c & 4 != 0) @max(b.wall, b.ridge) else 0 };
        for ([2][3]f64{ p, .{ p[0] - l[0] * p[2] / l[2], p[1] - l[1] * p[2] / l[2], 0 } }) |q| {
            const s = eye.project(q);
            lo = .{ @min(lo[0], s[0]), @min(lo[1], s[1]) };
            hi = .{ @max(hi[0], s[0]), @max(hi[1], s[1]) };
        }
    };
    for (scene.sources, scene.eyes, scene.same) |source, source_eye, s| {
        if (!s) continue;
        for (source.names, source.pictures) |picture_name, picture| {
            if (!std.mem.endsWith(u8, picture_name, "g")) continue;
            for (0..picture.height) |y| for (0..picture.width) |x| {
                if (picture.pixels[y * picture.width + x][3] == 0) continue;
                const q = eye.project(groundPoint(source_eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5));
                lo = .{ @min(lo[0], q[0]), @min(lo[1], q[1]) };
                hi = .{ @max(hi[0], q[0]), @max(hi[1], q[1]) };
            };
        }
    }
    eye.anchor = .{ 2 - @floor(lo[0]), 2 - @floor(lo[1]) };
    return .{
        .turn = k,
        .eye = eye,
        .size = .{ @intFromFloat(@ceil(hi[0]) - @floor(lo[0]) + 4), @intFromFloat(@ceil(hi[1]) - @floor(lo[1]) + 4) },
        .name = name,
    };
}

/// One damage state of a target: the sprite with the source of each pixel,
/// its shadow, its ground picture where the variant has one, and how well the
/// blocks cover each source's outline.
const Drawn = struct { state: []const u8, blocks: []const Block, sprite: Generated, shadow: Image, ground: ?Generated, covers: []const f64, means: Means = .{} };

fn drawTarget(ctx: *bk_tool.Context, scene: Scene, target: Turned) ![]Drawn {
    const arena = ctx.arena;
    const n = scene.sources.len;
    const own = scene.sources[scene.fixed];
    const drawn = try arena.alloc(Drawn, scene.states.len);
    for (scene.states, scene.shapes, scene.bodies, drawn) |state, blocks, body, *d| {
        var means: Means = .{};
        const projectors = try arena.alloc(Projector, n);
        const ground = try arena.alloc(Projector, n);
        var ground_count: usize = 0;
        var covers: std.ArrayList(f64) = .empty;
        for (scene.sources, scene.eyes, scene.same, projectors, 0..) |source, eye, s, *projector, e| {
            const picture = pictureOf(source, state);
            projector.* = .{ .eye = eye, .picture = if (s) picture else null };
            const p = picture orelse continue;
            var both: f64 = 0;
            var either: f64 = 0;
            for (0..p.height) |y| for (0..p.width) |x| {
                const pixel = p.pixels[y * p.width + x];
                const hit = castPixel(blocks, eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
                both += @floatFromInt(@intFromBool(hit != null and pixel[3] >= 128));
                either += @floatFromInt(@intFromBool(hit != null or pixel[3] >= 128));
                if (s and hit != null and pixel[3] >= 128) means.add(shadeClass(hit.?.normal, eye.turn), pixel);
            };
            try covers.append(arena, if (either == 0) 0 else both / either);
            if (!s) continue;
            const g = pictureOf(source, try std.fmt.allocPrint(arena, "{s}g", .{state})) orelse continue;
            // The variant's own ground picture first.
            ground[ground_count] = .{ .eye = eye, .picture = g };
            if (e == scene.fixed) std.mem.swap(Projector, &ground[0], &ground[ground_count]);
            ground_count += 1;
        }
        // The variant's own picture first: of two pictures of one side it
        // wins the tie, so e_house01_3 keeps its shingles and does not take
        // e_house01_1's thatch, a colourway apart by 0.03 (M006 S07e).
        std.mem.swap(Projector, &projectors[0], &projectors[scene.fixed]);
        const sprite = try projectPicture(arena, blocks, if (state[0] == '1') body else blocks.len, if (state[0] == '1') null else means, wallLight(means), target.eye, target.size, projectors);
        try patchFill(arena, sprite);
        const shadow: Image = .{ .width = target.size[0], .height = target.size[1], .pixels = try arena.alloc([4]u8, sprite.kinds.len) };
        const l = lightFor(light_2003, target.turn);
        for (0..shadow.height) |y| for (0..shadow.width) |x| {
            const g = groundPoint(target.eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
            shadow.pixels[y * shadow.width + x] = if (castRay(blocks, g, l) != null) .{ 0, 0, 0, 255 } else .{ 0, 0, 0, 0 };
        };
        keepShadow(sprite.image, shadow);
        const has_ground = pictureOf(own, try std.fmt.allocPrint(arena, "{s}g", .{state})) != null;
        d.* = .{
            .state = state,
            .blocks = blocks,
            .sprite = sprite,
            .shadow = shadow,
            .ground = if (has_ground) try projectGround(arena, target.eye, target.size, ground[0..ground_count]) else null,
            .covers = covers.items,
            .means = means,
        };
    }
    return drawn;
}

/// How plausible a turned intact picture's shape is against the variant's
/// 2003 one: its height over that of the 2003 picture of the side it shows
/// where the family has one (an L-shaped house turned stands lower or
/// higher), else of the variant's own, the blocks' ground over the
/// footprint's cells, the IoU of the blocks' outline with the variant's 2003
/// one (other variants of the family may be other buildings, e_house01_1 and
/// _3 differ in their roofs), and the protrusions of both pictures. `hand`:
/// the blocks are a hand shape. `low` and `low_kept`: the pixels the low
/// parts of the 2003 sides show in the turn and those it holds (`turnLowParts`);
/// `roof` and `roof_kept` likewise for the walls of the roof parts (`roofKept`).
const Shape = struct { height: f64 = 1, ground: f64 = 1, cover: f64 = 1, protrusion: f64 = 0, protrusion_2003: f64 = 0, hand: bool = false, low: f64 = 0, low_kept: f64 = 0, roof: f64 = 0, roof_kept: f64 = 0, stretch: f64 = 0, copied: f64 = 0 };

fn shapeOf(gpa: std.mem.Allocator, own: Source, fixed: usize, d: Drawn, shown: Image) !Shape {
    const original = pictureOf(own, "1").?;
    return .{
        .height = @as(f64, @floatFromInt(outlineHeight(d.sprite.image))) / @as(f64, @floatFromInt(@max(1, outlineHeight(shown)))),
        .ground = groundArea(d.blocks) / footprintArea(own.footprint),
        .cover = d.covers[fixed],
        .protrusion = try protrusion(gpa, d.sprite.image),
        .protrusion_2003 = try protrusion(gpa, original),
    };
}

/// A turned intact sprite's height against the 2003 one's, outside this
/// range: a block stands up that the 2003 side does not show (single-view
/// depth), or a tall part (a chimney stalk) is lost.
const height_range = [2]f64{ 0.7, 1.3 };
/// The blocks' ground against the footprint's cells, outside this range.
const ground_ratio = [2]f64{ 0.5, 2.0 };
/// The intact blocks cover the variant's 2003 outline less than this (IoU).
pub const outline_floor = 0.85;
/// A turned outline's protrusion may be this many times the 2003 one's plus
/// the margin. Between the 2003 sides of one building it is at most 1.5
/// times plus 0.026 (a_h07_3 against _1; props such as tents and leaders'
/// busts aside); the S05 e_house02_1_r2, whose roof sprouted peaks no 2003
/// side shows, was 0.095 against 0.028.
const protrusion_factor = 1.5;
const protrusion_margin = 0.03;
/// A turn keeps a footprint whose cells overlap their turned selves this much (IoU).
const same_footprint = 0.9;
/// An object is thin where its 2003 outline protrudes this much: mostly
/// parts narrower than the protrusion window (stalks, tanks on legs,
/// statues, wells, fountains, hydrants), which blocks of 12 world units or
/// more cannot draw. The 2003 houses stay under 0.1, kiosks and pigeon
/// lofts reach 0.2.
const thin_object = 0.15;
/// An object is small on this many footprint cells or fewer (2 x 2: wells,
/// tanks, statues, huts).
const small_object = 4;

/// Where a turned intact shape leaves what the 2003 sides make plausible, or
/// null. The height and the ground guard the fit's depth, which one picture
/// leaves open; a hand shape's depth is drawn by its author, so it answers
/// to its outline and protrusion only.
fn shapeFailure(gpa: std.mem.Allocator, s: Shape) !?[]const u8 {
    if (!s.hand and (s.height < height_range[0] or s.height > height_range[1])) return try std.fmt.allocPrint(gpa, "height {d:.2} outside {d:.1}..{d:.1}", .{ s.height, height_range[0], height_range[1] });
    if (!s.hand and (s.ground < ground_ratio[0] or s.ground > ground_ratio[1])) return try std.fmt.allocPrint(gpa, "ground {d:.2} outside {d:.1}..{d:.1}", .{ s.ground, ground_ratio[0], ground_ratio[1] });
    if (s.cover < outline_floor) return try std.fmt.allocPrint(gpa, "outline {d:.2} < {d:.2}", .{ s.cover, outline_floor });
    const most = protrusion_factor * s.protrusion_2003 + protrusion_margin;
    if (s.protrusion > most) return try std.fmt.allocPrint(gpa, "protrusion {d:.3} > {d:.3}", .{ s.protrusion, most });
    if (lowFails(s.low, s.low_kept)) return try std.fmt.allocPrint(gpa, "low parts {d:.0} of {d:.0} pixels", .{ s.low_kept, s.low });
    if (lowFails(s.roof, s.roof_kept)) return try std.fmt.allocPrint(gpa, "roof parts {d:.0} of {d:.0} pixels", .{ s.roof_kept, s.roof });
    if (s.stretch > stretch_most) return try std.fmt.allocPrint(gpa, "stretched {d:.3} > {d:.2}", .{ s.stretch, stretch_most });
    if (s.copied > low_margin) return try std.fmt.allocPrint(gpa, "parts copied {d:.0} pixels", .{s.copied});
    return null;
}

/// The share of a turned intact picture's pixels that may be stretched
/// (`stretchedPixels`). Before S07c the turns that read as smeared measured
/// 0.05 to 0.14 (e_railwaystation_2's roofs, e_shed02_2/_5, towncornhouse01's
/// gable, ukrainehouse01_1's thatch, e_house04_4's planked gable 0.063);
/// the sound ones 0.03 or less.
pub const stretch_most = 0.04;

/// The low parts the 2003 intact pictures of the colourway show below the
/// scene's body (`lowParts`), as a turn's picture keeps them (`lowKept`):
/// the pixels its eye sees of them and how many of those it holds.
fn turnLowParts(gpa: std.mem.Allocator, scene: Scene, picture: Image, eye: Eye) ![2]f64 {
    var parts: std.ArrayList(Block) = .empty;
    for (scene.sources, scene.eyes, scene.same) |source, source_eye, same| {
        if (same) try parts.appendSlice(gpa, try lowParts(gpa, scene.body, pictureOf(source, "1").?, source_eye));
    }
    return lowKept(scene.body, parts.items, eye, picture);
}

/// A turn as the variant's 2003 pictures themselves, every state, shadow and
/// ground picture, moved so that the building stands on its turned
/// footprint: for a thin object, or a small one the blocks cannot draw,
/// whose footprint the turn keeps, so that it looks alike from that side.
/// `mirror`: an odd turn of such an object on a rectangle, which a mirror
/// across the screen's vertical (world (x, y) to (-y, -x)) lays on the
/// turned footprint, as a building alike under a half turn about its axes
/// (a board, a row of bins) shows from there; the sprite and ground
/// pictures are mirrored about the zero cross, the shadow is kept, as a
/// post's falls alike from every side. The mirrored sprite is relit by the
/// gain from the shading class each pixel's face shows in the 2003 picture
/// to the class the mirrored face shows (`by`, the blocks' drawing of the
/// turn), pooled over the sprite: a lit board turned to the shade darkens.
fn sameSprite(ctx: *bk_tool.Context, scene: Scene, k: u2, name: []const u8, mirror: bool, by: []const Drawn) !struct { target: Turned, drawn: []Drawn } {
    const arena = ctx.arena;
    const own = scene.sources[scene.fixed];
    const c = boxCentre(own.footprint);
    const t = turnPoint(c, k);
    const from = mfc_camera.project(if (mirror) .{ -c[1], -c[0], 0 } else .{ c[0], c[1], 0 });
    const to = mfc_camera.project(.{ t[0], t[1], 0 });
    const first = pictureOf(own, "1").?;
    // A mirrored canvas holds the pictures mirrored about the zero cross's
    // column and the shadow as it was.
    const ax: i64 = own.anchor[0];
    const w: i64 = first.width;
    const lo: i64 = if (mirror) @min(0, 2 * ax - w + 1) else 0;
    const width: u32 = @intCast(if (mirror) @max(w - 1, 2 * ax) - lo + 1 else w);
    const target: Turned = .{
        .turn = k,
        .eye = .{ .anchor = .{ @as(f64, @floatFromInt(ax - lo)) + @round(from[0] - to[0]), @as(f64, @floatFromInt(own.anchor[1])) + @round(from[1] - to[1]) }, .turn = k },
        .size = .{ width, first.height },
        .name = name,
    };
    const place = struct {
        fn f(gpa: std.mem.Allocator, image: ?Image, size: [2]u32, shift: i64, flip: ?i64) !Image {
            const out: Image = .{ .width = size[0], .height = size[1], .pixels = try gpa.alloc([4]u8, @as(usize, size[0]) * size[1]) };
            @memset(out.pixels, .{ 0, 0, 0, 0 });
            const in = image orelse return out;
            for (0..in.height) |y| for (0..in.width) |x| {
                const xi: i64 = @intCast(x);
                const at = (if (flip) |axis| 2 * axis - xi else xi) - shift;
                out.pixels[y * size[0] + @as(usize, @intCast(at))] = in.pixels[y * in.width + x];
            };
            return out;
        }
    }.f;
    const flip: ?i64 = if (mirror) ax else null;
    const drawn = try arena.alloc(Drawn, scene.states.len);
    for (scene.states, drawn, by) |state, *d, blocks_drawn| {
        var picture = pictureOf(own, state).?;
        if (mirror and !scene.prop) {
            // One factor a channel over the sprite, so that a curved tank
            // drawn as a few block faces does not take their seams. A heap
            // is not relit: its boxes stand for crates and earth alike from
            // every side, and their walls turned the mirrored e_wt depots 5%
            // darker than the 2003 picture (M006 S07d).
            const eye = scene.eyes[scene.fixed];
            var sums = [2][3]f64{ .{ 0, 0, 0 }, .{ 0, 0, 0 } };
            for (0..picture.height) |y| for (0..picture.width) |x| {
                const pixel = picture.pixels[y * picture.width + x];
                if (pixel[3] < 128) continue;
                const hit = castPixel(blocks_drawn.blocks, eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
                for (0..3) |ch| {
                    const value: f64 = @floatFromInt(pixel[ch]);
                    sums[0][ch] += value;
                    sums[1][ch] += if (hit) |h| value * blocks_drawn.means.gain(shadeClass(.{ -h.normal[1], -h.normal[0], h.normal[2] }, eye.turn), shadeClass(h.normal, eye.turn), ch) else value;
                }
            };
            const relit: Image = .{ .width = picture.width, .height = picture.height, .pixels = try arena.dupe([4]u8, picture.pixels) };
            for (relit.pixels) |*pixel| for (0..3) |ch| {
                pixel[ch] = @intFromFloat(@min(255, @round(@as(f64, @floatFromInt(pixel[ch])) * sums[1][ch] / @max(1, sums[0][ch]))));
            };
            picture = relit;
        }
        const sprite = try asGenerated(arena, try place(arena, picture, target.size, lo, flip));
        const shadow = try place(arena, pictureOf(own, try std.fmt.allocPrint(arena, "{s}s", .{state})), target.size, lo, null);
        keepShadow(sprite.image, shadow);
        const ground = pictureOf(own, try std.fmt.allocPrint(arena, "{s}g", .{state}));
        d.* = .{ .state = state, .blocks = &.{}, .sprite = sprite, .shadow = shadow, .ground = if (ground) |g| try asGenerated(arena, try place(arena, g, target.size, lo, flip)) else null, .covers = &.{} };
    }
    return .{ .target = target, .drawn = drawn };
}

/// The family's 2003 variant that shows turn k of the scene's variant in its
/// colourway and holds every picture the variant holds (each damage state,
/// shadow, ground and winter picture), or null. M006 S02: a turn the 2003
/// art already shows is that art, not a drawing of it.
fn siblingFor(scene: Scene, k: u2) ?usize {
    const own = scene.sources[scene.fixed];
    for (scene.sources, scene.eyes, scene.same, 0..) |source, eye, same, e| {
        if (e == scene.fixed or !same or eye.turn != k) continue;
        for (own.names) |name| {
            if (pictureOf(source, name) == null) break;
        } else return e;
    }
    return null;
}

/// Turn k as the sibling's 2003 pictures themselves, unmoved on their canvas,
/// with the zero cross moved to where the variant's turned zero cross falls
/// in them: the sibling's eye sees a point q of the turned frame at
/// q + shift, so the turn's zero cross is the sibling's plus the shift's
/// projection, rounded to a whole pixel.
fn siblingSprite(ctx: *bk_tool.Context, scene: Scene, k: u2, name: []const u8, e: usize) !struct { target: Turned, drawn: []Drawn } {
    const arena = ctx.arena;
    const sibling = scene.sources[e];
    const shift = mfc_camera.project(.{ scene.eyes[e].shift[0], scene.eyes[e].shift[1], 0 });
    const first = pictureOf(sibling, "1").?;
    const target: Turned = .{
        .turn = k,
        .eye = .{ .anchor = .{ @as(f64, @floatFromInt(sibling.anchor[0])) + @round(shift[0]), @as(f64, @floatFromInt(sibling.anchor[1])) + @round(shift[1]) }, .turn = k },
        .size = .{ first.width, first.height },
        .name = name,
    };
    const drawn = try arena.alloc(Drawn, scene.states.len);
    for (scene.states, drawn) |state, *d| {
        const sprite = try asGenerated(arena, pictureOf(sibling, state).?);
        // A copy: keepShadow may mark a pixel, and the sibling's pictures are shared.
        const shadow: Image = .{ .width = first.width, .height = first.height, .pixels = try arena.alloc([4]u8, @as(usize, first.width) * first.height) };
        if (pictureOf(sibling, try std.fmt.allocPrint(arena, "{s}s", .{state}))) |s| @memcpy(shadow.pixels, s.pixels) else @memset(shadow.pixels, .{ 0, 0, 0, 0 });
        keepShadow(sprite.image, shadow);
        const ground = pictureOf(sibling, try std.fmt.allocPrint(arena, "{s}g", .{state}));
        d.* = .{ .state = state, .blocks = &.{}, .sprite = sprite, .shadow = shadow, .ground = if (ground) |g| try asGenerated(arena, g) else null, .covers = &.{} };
    }
    return .{ .target = target, .drawn = drawn };
}

/// A 2003 picture as a generated one, every pixel projected.
fn asGenerated(gpa: std.mem.Allocator, image: Image) !Generated {
    const out: Generated = .{ .image = image, .kinds = try gpa.alloc(Kind, image.pixels.len), .classes = try gpa.alloc(u8, image.pixels.len) };
    for (image.pixels, out.kinds) |p, *kind| kind.* = if (p[3] > 0) .projected else .empty;
    @memset(out.classes, 255);
    return out;
}

/// A turn drawn: by the blocks, or where the blocks cannot draw a thin or
/// small object, as its 2003 pictures (`sameSprite`), with the intact
/// shape's gate values; a turn a 2003 variant of its colourway shows, as
/// that variant's pictures (`siblingSprite`, `sibling` names it).
const DrawnTurn = struct { target: Turned, drawn: []Drawn, shape: Shape, failure: ?[]const u8, fallback: bool, mirror: bool, sibling: ?[]const u8 = null };

fn drawTurn(ctx: *bk_tool.Context, scene: Scene, blocks_target: Turned) !DrawnTurn {
    const arena = ctx.arena;
    const own = scene.sources[scene.fixed];
    const new_side = (scene.own_side - 1 + @as(u8, blocks_target.turn)) % 4 + 1;
    if (siblingFor(scene, blocks_target.turn)) |e| {
        const same = try siblingSprite(ctx, scene, blocks_target.turn, blocks_target.name, e);
        return .{ .target = same.target, .drawn = same.drawn, .shape = .{}, .failure = null, .fallback = true, .mirror = false, .sibling = std.fs.path.basename(scene.sources[e].dir) };
    }
    const drawn = try drawTarget(ctx, scene, blocks_target);
    const shown = for (scene.sources) |source| {
        if (source.side == new_side) break pictureOf(source, "1").?;
    } else pictureOf(own, "1").?;
    // The intact shape against what the 2003 sides make plausible; an
    // object the blocks cannot draw whose footprint the turn keeps is
    // turned as its 2003 pictures themselves.
    var shape: Shape = .{};
    for (drawn, scene.bodies) |d, body| {
        if (std.mem.eql(u8, d.state, "1")) {
            shape = try shapeOf(arena, own, scene.fixed, d, shown);
            shape.hand = scene.hand != null;
            const low = try turnLowParts(arena, scene, d.sprite.image, blocks_target.eye);
            shape.low = low[0];
            shape.low_kept = low[1];
            const roof = roofKept(scene.body, scene.roof, blocks_target.eye, d.sprite);
            shape.roof = roof[0];
            shape.roof_kept = roof[1];
            shape.stretch = stretchShare(d.sprite);
            shape.copied = copiedParts(d.blocks, body, blocks_target.eye, d.sprite);
        }
    }
    const failure = try shapeFailure(arena, shape);
    const box = occupiedBox(own.footprint.rows);
    const how = spriteTurn(.{
        .thin = shape.protrusion_2003 >= thin_object,
        .small = footprintArea(own.footprint) <= small_object * tile_size * tile_size,
        .failed = failure != null,
        .kept = try footprintIou(arena, own.footprint.rows, own.footprint.rows, blocks_target.turn) >= same_footprint,
        .rectangle = footprintArea(own.footprint) == @as(f64, @floatFromInt((box[2] - box[0]) * (box[3] - box[1]))) * tile_size * tile_size,
        .odd = blocks_target.turn % 2 == 1,
        .heap = scene.prop,
    });
    if (!how.sprite) return .{ .target = blocks_target, .drawn = drawn, .shape = shape, .failure = failure, .fallback = false, .mirror = false };
    const same = try sameSprite(ctx, scene, blocks_target.turn, blocks_target.name, how.mirror, drawn);
    return .{ .target = same.target, .drawn = same.drawn, .shape = shape, .failure = failure, .fallback = true, .mirror = how.mirror };
}

/// What a turn's object is, for `spriteTurn`.
pub const TurnObject = struct { thin: bool, small: bool, failed: bool, kept: bool, rectangle: bool, odd: bool, heap: bool };

/// Whether a turn is drawn as the 2003 pictures (`sameSprite`) and mirrored:
/// a thin object, or a small one whose blocks fail, where the turn keeps
/// its footprint, or mirrored on an odd turn of a rectangle; a heap
/// (`props_file`) on every turn, mirrored on an odd one that does not keep
/// its footprint, so that it spans the turned footprint's box (a crate a
/// cell off a depot's free footprint corner reads as the same heap).
pub fn spriteTurn(o: TurnObject) struct { sprite: bool, mirror: bool } {
    const prop = o.thin or (o.small and o.failed);
    const mirror = !o.kept and o.odd and (o.rectangle and prop or o.heap);
    return .{ .sprite = prop and o.kept or o.heap or mirror, .mirror = mirror };
}

/// The zero cross of a turn drawn by the blocks, where the blocks stand on the
/// variant's footprint (M006 S07). The 2003 camera cannot tell a building
/// moved along its line of sight on the ground, world (1, -1), and lifted by
/// as much from where it stands: the fit leaves the blocks anywhere on that
/// line, and turned, that error is no longer along the line of sight. Poland's
/// church (e_jettybuilding_4) stood 18 world units each way off its footprint
/// and, a quarter turned, 25 pixels to the side of it, over its neighbours. The
/// turned picture is laid as if its blocks were moved along that line by what
/// puts their ground box's middle nearest the footprint's middle; the rest of
/// that offset is the 2003 picture's own, seen from the 2003 camera, and kept.
fn footprintZero(scene: Scene, target: Turned) [2]f64 {
    const blocks = for (scene.states, scene.shapes) |state, shape| {
        if (std.mem.eql(u8, state, "1")) break shape;
    } else return target.eye.anchor;
    if (blocks.len == 0) return target.eye.anchor;
    var lo = blocks[0].lo;
    var hi = blocks[0].hi;
    for (blocks) |b| {
        lo = .{ @min(lo[0], b.lo[0]), @min(lo[1], b.lo[1]) };
        hi = .{ @max(hi[0], b.hi[0]), @max(hi[1], b.hi[1]) };
    }
    const foot = boxCentre(scene.sources[scene.fixed].footprint);
    const along = ((foot[0] - (lo[0] + hi[0]) / 2) - (foot[1] - (lo[1] + hi[1]) / 2)) / 2;
    const moved = target.eye.project(.{ along, -along, 0 });
    const still = target.eye.project(.{ 0, 0, 0 });
    return .{ target.eye.anchor[0] - @round(moved[0] - still[0]), target.eye.anchor[1] - @round(moved[1] - still[1]) };
}

fn sideCommand(ctx: *bk_tool.Context, dir_arg: []const u8, turns_arg: []const u8, out_arg: []const u8, work_arg: []const u8) !void {
    const arena = ctx.arena;
    const io = ctx.io;
    const cwd = std.Io.Dir.cwd();
    const dir = try bk_tool.inputPath(arena, dir_arg);
    const out = try bk_tool.inputPath(arena, out_arg);
    const work = try bk_tool.inputPath(arena, work_arg);
    var turns: std.ArrayList(u2) = .empty;
    var split = std.mem.splitScalar(u8, turns_arg, ',');
    while (split.next()) |t| {
        const k = std.fmt.parseInt(u2, t, 10) catch return error.Usage;
        if (k == 0) return error.Usage;
        try turns.append(arena, k);
    }

    const found = findFamily(arena, io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
    const scene = try loadScene(ctx, found, found.index, null, false);
    var targets: std.ArrayList(Turned) = .empty;
    for (turns.items) |k| try targets.append(arena, turnTarget(scene, k, try std.fmt.allocPrint(arena, "{s}_r{d}", .{ scene.name, k })));

    // The pictures, the source maps and the packs of each turn.
    // bk-sprite beside bk-views (zig-out/bin), else the one on PATH.
    const exe_dir = try std.process.executableDirPathAlloc(io, arena);
    const beside = try std.fs.path.join(arena, &.{ exe_dir, if (@import("builtin").os.tag == .windows) "bk-sprite.exe" else "bk-sprite" });
    const sprite_tool = if (cwd.access(io, beside, .{})) |_| beside else |_| "bk-sprite";
    const own = scene.sources[scene.fixed];
    for (targets.items) |blocks_target| {
        var target = blocks_target;
        const pictures_dir = try std.fs.path.join(arena, &.{ work, try std.fmt.allocPrint(arena, "{s}.work", .{scene.name}), target.name });
        const map_dir = try std.fs.path.join(arena, &.{ work, try std.fmt.allocPrint(arena, "{s}.map", .{target.name}) });
        const final = try std.fs.path.join(arena, &.{ out, target.name });
        const temp = try std.fmt.allocPrint(arena, "{s}.tmp", .{final});
        for ([_][]const u8{ pictures_dir, map_dir, temp }) |path| {
            cwd.deleteTree(io, path) catch {};
            try cwd.createDirPath(io, path);
        }
        const new_side = (scene.own_side - 1 + @as(u8, target.turn)) % 4 + 1;
        const turned = try drawTurn(ctx, scene, target);
        target = turned.target;
        const drawn = turned.drawn;
        const shape = turned.shape;
        const failure = turned.failure;
        const fallback = turned.fallback;
        const mirror = turned.mirror;
        const zero = if (fallback) target.eye.anchor else footprintZero(scene, target);
        var report: std.Io.Writer.Allocating = .init(arena);
        const r = &report.writer;
        try r.print("{{\"variant\":\"{s}\",\"side\":{d},\"turn\":{d},\"new_side\":{d},\"fallback\":{},\"mirror\":{},\"sibling\":\"{s}\",\"hand\":{},\"shape\":{{\"height\":{d:.3},\"ground\":{d:.3},\"cover\":{d:.3},\"protrusion\":{d:.4},\"protrusion_2003\":{d:.4},\"low\":{d:.0},\"low_kept\":{d:.0},\"roof\":{d:.0},\"roof_kept\":{d:.0},\"stretch\":{d:.4},\"copied\":{d:.0},\"failure\":\"{s}\"}},\n\"blocks\":{{", .{ scene.name, scene.own_side, target.turn, new_side, fallback, mirror, turned.sibling orelse "", scene.hand != null, shape.height, shape.ground, shape.cover, shape.protrusion, shape.protrusion_2003, shape.low, shape.low_kept, shape.roof, shape.roof_kept, shape.stretch, shape.copied, failure orelse "" });
        for (scene.states, scene.shapes, 0..) |state, blocks, i| {
            try r.print("{s}\n\"{s}\":[", .{ if (i > 0) "," else "", state });
            for (blocks, 0..) |b, j| try r.print("{s}{{\"lo\":[{d:.1},{d:.1}],\"hi\":[{d:.1},{d:.1}],\"wall\":{d:.1},\"ridge\":{d:.1},\"roof\":\"{s}\"}}", .{ if (j > 0) "," else "", b.lo[0], b.lo[1], b.hi[0], b.hi[1], b.wall, b.ridge, @tagName(b.roof) });
            try r.writeAll("]");
        }
        try r.writeAll("},\n\"sources\":[");
        for (scene.sources, scene.eyes, scene.same, 0..) |source, eye, s, e| {
            try r.print("{s}\n{{\"variant\":\"{s}\",\"side\":{d},\"turn\":{d},\"shift\":[{d:.1},{d:.1}],\"colourway_distance\":{d:.3},\"projects\":{}}}", .{
                if (e > 0) "," else "", std.fs.path.basename(source.dir), source.side, eye.turn, eye.shift[0], eye.shift[1], colourwayDistance(scene.outlines[scene.fixed], scene.outlines[e]), s,
            });
        }
        try r.writeAll("],\n\"pictures\":[");
        var first_picture = true;
        for (drawn) |d| {
            const shadow_name = try std.fmt.allocPrint(arena, "{s}s", .{d.state});
            const ground_name = try std.fmt.allocPrint(arena, "{s}g", .{d.state});
            try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{d.state}), try writePng(arena, d.sprite.image));
            try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{shadow_name}), try writePng(arena, d.shadow));
            try writePictureReport(ctx, r, map_dir, d.state, d.sprite, d.covers, &first_picture);
            if (!fallback) try writeOut(ctx, map_dir, try std.fmt.allocPrint(arena, "{s}_shape.png", .{d.state}), try writePng(arena, try shapePicture(arena, d.blocks, target)));
            var argv: std.ArrayList([]const u8) = .empty;
            const grid = try turnGrid(arena, own.footprint, target.turn);
            const locked = lockedCenter(.{ @floatCast(grid.origin[0]), @floatCast(grid.origin[1]) });
            try argv.appendSlice(arena, &.{
                sprite_tool,
                "compose-pack",
                try std.fs.path.join(arena, &.{ pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{d.state}) }),
                try std.fs.path.join(arena, &.{ pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{shadow_name}) }),
                try std.fs.path.join(arena, &.{ temp, d.state }),
                "--zero",
                try std.fmt.allocPrint(arena, "{d},{d}", .{ zero[0], zero[1] }),
                "--pass",
                try std.mem.join(arena, "/", grid.rows),
                "--locked",
                try std.fmt.allocPrint(arena, "{d},{d}", .{ locked[0], locked[1] }),
                "--force",
            });
            if (d.ground) |g| {
                try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{ground_name}), try writePng(arena, g.image));
                try writePictureReport(ctx, r, map_dir, ground_name, g, &.{}, &first_picture);
                try argv.appendSlice(arena, &.{ "--noise", try std.fs.path.join(arena, &.{ pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{ground_name}) }) });
            }
            if (std.mem.eql(u8, d.state, "1")) try argv.appendSlice(arena, &.{ "--icon", try std.fs.path.join(arena, &.{ temp, "icon.tga" }) });
            const result = std.process.run(arena, io, .{ .argv = argv.items, .stdout_limit = .limited(1024 * 1024), .stderr_limit = .limited(1024 * 1024) }) catch |err|
                return ctx.failKey("views.err.compose", .{ temp, @errorName(err) });
            if (result.term != .exited or result.term.exited != 0) return ctx.failKey("views.err.compose", .{ temp, result.stderr });
            // compose-pack always packs a shadow; where the 2003 variant has
            // none (pillboxes, tracks, some ruins) the turned one has none either.
            if (pictureOf(own, shadow_name) == null) for ([_][]const u8{ ".san", "_c.dds", "_h.dds", "_l.dds" }) |suffix| {
                try cwd.deleteFile(io, try std.fmt.allocPrint(arena, "{s}{s}", .{ try std.fs.path.join(arena, &.{ temp, shadow_name }), suffix }));
            };
        }
        try r.writeAll("]}\n");
        try writeOut(ctx, map_dir, "sources.json", report.written());
        try writeOut(ctx, temp, "1.xml", try turnXml(arena, own.xml, target.turn));
        if (cwd.readFileAlloc(io, try std.fs.path.join(arena, &.{ own.dir, "name.txt" }), arena, .limited(1024 * 1024))) |text| {
            try writeOut(ctx, temp, "name.txt", text);
        } else |_| {}
        cwd.deleteTree(io, final) catch {};
        try cwd.rename(temp, cwd, final, io);
        try ctx.out.print("wrote {s}: side {d} of {s}, {d} damage states\n", .{ final, new_side, scene.family, scene.states.len });
    }
}

/// `shape`: each variant's intact blocks (its hand shape, or the fit)
/// against every 2003 picture of its family: the outline IoU each, an
/// overlay per picture (both grey, the picture alone red, the blocks alone
/// blue), the blocks seen from the four sides (`r0`..`r3`), and the blocks
/// refined to the outlines with their roofs kept, for the author of a hand
/// shape. Writes `<out-dir>/<variant>/` and shape.json.
fn shapeCommand(ctx: *bk_tool.Context, out_arg: []const u8, dir_args: []const []const u8) !void {
    const arena = ctx.arena;
    const out = try bk_tool.inputPath(arena, out_arg);
    var json: std.Io.Writer.Allocating = .init(arena);
    const w = &json.writer;
    try w.writeAll("{\"shapes\":[");
    for (dir_args, 0..) |dir_arg, v| {
        const dir = try bk_tool.inputPath(arena, dir_arg);
        const found = findFamily(arena, ctx.io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
        const label = try variantLabel(arena, found.family, found.index);
        const pictures_dir = try std.fs.path.join(arena, &.{ out, label });
        try std.Io.Dir.cwd().createDirPath(ctx.io, pictures_dir);
        const scene = try loadScene(ctx, found, found.index, null, true);
        // The authored or fitted blocks and the authored roof parts,
        // without the low parts side adds.
        const blocks = try std.mem.concat(arena, Block, &.{ scene.body, scene.roof });
        // The fit refines at most max_blocks: the body alone.
        const refined = try refineBlocks(arena, scene.body, scene.outlines, scene.eyes);
        try w.print("{s}\n{{\"variant\":\"{s}\",\"hand\":{},\"pictures\":[", .{ if (v > 0) "," else "", label, scene.hand != null });
        for (scene.sources, scene.eyes, scene.outlines, 0..) |source, eye, outline, e| {
            const name = std.fs.path.basename(source.dir);
            const iou = blocksIou(blocks, eye, outline);
            const refined_iou = blocksIou(refined, eye, outline);
            try w.print("{s}\n{{\"variant\":\"{s}\",\"own\":{},\"same\":{},\"iou\":{d:.4},\"refined_iou\":{d:.4}}}", .{ if (e > 0) "," else "", name, e == scene.fixed, scene.same[e], iou, refined_iou });
            try ctx.out.print("{s}, {s}: IoU {d:.3}, refined {d:.3}\n", .{ label, name, iou, refined_iou });
            const overlay: Image = .{ .width = outline.width, .height = outline.height, .pixels = try arena.alloc([4]u8, outline.pixels.len) };
            for (0..outline.height) |y| for (0..outline.width) |x| {
                const i = y * outline.width + x;
                const in_model = castPixel(blocks, eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5) != null;
                const p = outline.pixels[i];
                overlay.pixels[i] = if (p[3] >= 128 and in_model) .{ p[0] / 2 + 64, p[1] / 2 + 64, p[2] / 2 + 64, 255 } else if (p[3] >= 128) .{ 255, 0, 0, 255 } else if (in_model) .{ 0, 0, 255, 255 } else .{ 0, 0, 0, 0 };
            };
            try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "{s}.png", .{name}), try writePng(arena, overlay));
        }
        for (0..4) |k| {
            const target = turnTarget(scene, @intCast(k), "");
            try writeOut(ctx, pictures_dir, try std.fmt.allocPrint(arena, "r{d}.png", .{k}), try writePng(arena, try shapePicture(arena, blocks, target)));
        }
        for ([_][]const Block{ blocks, refined }, [_][]const u8{ "blocks", "refined" }) |list, key| {
            try w.print("],\n\"{s}\":[", .{key});
            for (list, 0..) |b, j| try w.print("{s}\n{{\"lo\":[{d:.1},{d:.1}],\"hi\":[{d:.1},{d:.1}],\"wall\":{d:.1},\"ridge\":{d:.1},\"roof\":\"{s}\"}}", .{ if (j > 0) "," else "", b.lo[0], b.lo[1], b.hi[0], b.hi[1], b.wall, b.ridge, @tagName(b.roof) });
        }
        try w.writeAll("]}");
    }
    try w.writeAll("]}\n");
    try writeOut(ctx, out, "shape.json", json.written());
}

/// The source map of one generated picture: a PNG (projected green, mirrored
/// blue, transferred cyan, filled red) and its counts per shading class in
/// the report.
fn writePictureReport(ctx: *bk_tool.Context, r: *std.Io.Writer, map_dir: []const u8, name: []const u8, generated: Generated, covers: []const f64, first: *bool) !void {
    const arena = ctx.arena;
    const map: Image = .{ .width = generated.image.width, .height = generated.image.height, .pixels = try arena.alloc([4]u8, generated.kinds.len) };
    var counts = std.mem.zeroes([9][5]usize);
    for (map.pixels, generated.kinds, generated.classes) |*pixel, kind, class| {
        pixel.* = switch (kind) {
            .empty => .{ 0, 0, 0, 0 },
            .projected => .{ 0, 160, 0, 255 },
            .mirrored => .{ 0, 64, 255, 255 },
            .transferred => .{ 0, 200, 220, 255 },
            .filled => .{ 255, 32, 32, 255 },
        };
        counts[@min(class, 8)][@backingInt(kind)] += 1;
    }
    try writeOut(ctx, map_dir, try std.fmt.allocPrint(arena, "{s}.png", .{name}), try writePng(arena, map));
    var total = [5]usize{ 0, 0, 0, 0, 0 };
    for (counts) |c| for (0..5) |k| {
        total[k] += c[k];
    };
    try r.print("{s}\n{{\"picture\":\"{s}\",\"projected\":{d},\"mirrored\":{d},\"transferred\":{d},\"filled\":{d},\"model_iou\":[", .{ if (first.*) "" else ",", name, total[1], total[2], total[3], total[4] });
    first.* = false;
    for (covers, 0..) |c, i| try r.print("{s}{d:.4}", .{ if (i > 0) "," else "", c });
    try r.writeAll("],\"classes\":[");
    var first_class = true;
    for (counts, 0..) |c, class| {
        if (class == 8 or c[1] + c[2] + c[3] + c[4] == 0) continue;
        try r.print("{s}{{\"class\":\"{s}\",\"projected\":{d},\"mirrored\":{d},\"transferred\":{d},\"filled\":{d}}}", .{ if (first_class) "" else ",", class_names[class], c[1], c[2], c[3], c[4] });
        first_class = false;
    }
    try r.writeAll("]}");
}

/// The blocks as the target eye sees them, lit by the 2003 light and tinted
/// per block, for the source map: what shape the pixels were laid on.
fn shapePicture(gpa: std.mem.Allocator, blocks: []const Block, target: Turned) !Image {
    const image: Image = .{ .width = target.size[0], .height = target.size[1], .pixels = try gpa.alloc([4]u8, @as(usize, target.size[0]) * target.size[1]) };
    const l = lightFor(light_2003, target.turn);
    const tints = [_][3]f64{ .{ 1, 1, 1 }, .{ 1, 0.6, 0.6 }, .{ 0.6, 1, 0.6 }, .{ 0.6, 0.6, 1 }, .{ 1, 1, 0.5 }, .{ 1, 0.5, 1 }, .{ 0.5, 1, 1 }, .{ 1, 0.8, 0.5 } };
    for (0..image.height) |y| for (0..image.width) |x| {
        const hit = castPixel(blocks, target.eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
        image.pixels[y * image.width + x] = if (hit) |h| blk: {
            const shade = 60 + 180 * @max(0, dot(h.normal, l));
            const t = tints[h.block % tints.len];
            break :blk .{ @intFromFloat(shade * t[0]), @intFromFloat(shade * t[1]), @intFromFloat(shade * t[2]), 255 };
        } else .{ 0, 0, 0, 0 };
    };
    return image;
}

/// What an opening of a picture's top edge `protrusion_width` pixels wide
/// takes off its outline (alpha 128 or more), as a share of the outline's
/// area: what stands up narrower than that (chimneys, spires, stray blocks).
pub fn protrusion(gpa: std.mem.Allocator, image: Image) !f64 {
    const w = image.width;
    const bottom: f64 = @floatFromInt(outlineBottom(image));
    // The top edge's height over the outline's lowest row, a column each,
    // empty columns at the ground.
    const h = try gpa.alloc(f64, w);
    defer gpa.free(h);
    const eroded = try gpa.alloc(f64, w);
    defer gpa.free(eroded);
    var area: f64 = 0;
    for (h, 0..) |*v, x| {
        v.* = 0;
        for (0..image.height) |y| {
            if (image.pixels[y * w + x][3] < 128) continue;
            if (v.* == 0) v.* = bottom - @as(f64, @floatFromInt(y));
            area += 1;
        }
    }
    const half = protrusion_width / 2;
    // The opening: the lowest over a window, then the highest of those.
    for (eroded, 0..) |*e, x| {
        e.* = std.math.inf(f64);
        for (x -| half..@min(w, x + half + 1)) |k| e.* = @min(e.*, h[k]);
    }
    var removed: f64 = 0;
    for (h, 0..) |v, x| {
        var m: f64 = 0;
        for (x -| half..@min(w, x + half + 1)) |k| m = @max(m, eroded[k]);
        removed += v - m;
    }
    return removed / @max(1, area);
}

const protrusion_width = 48;

/// The row below a picture's outline (alpha 128 or more).
fn outlineBottom(image: Image) usize {
    var bottom: usize = 0;
    for (0..image.height) |y| for (0..image.width) |x| {
        if (image.pixels[y * image.width + x][3] >= 128) bottom = y + 1;
    };
    return bottom;
}

/// The height in pixels of a picture's outline (alpha 128 or more).
fn outlineHeight(image: Image) usize {
    var top: ?usize = null;
    var bottom: usize = 0;
    for (0..image.height) |y| for (0..image.width) |x| {
        if (image.pixels[y * image.width + x][3] < 128) continue;
        if (top == null) top = y;
        bottom = y + 1;
    };
    return bottom - (top orelse 0);
}

/// The ground the blocks stand on, in square world units, sampled every unit.
fn groundArea(blocks: []const Block) f64 {
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (blocks) |b| {
        lo = .{ @min(lo[0], b.lo[0]), @min(lo[1], b.lo[1]) };
        hi = .{ @max(hi[0], b.hi[0]), @max(hi[1], b.hi[1]) };
    }
    var area: f64 = 0;
    var y = @floor(lo[1]) + 0.5;
    while (y < hi[1]) : (y += 1) {
        var x = @floor(lo[0]) + 0.5;
        while (x < hi[0]) : (x += 1) {
            for (blocks) |b| {
                if (x >= b.lo[0] and x < b.hi[0] and y >= b.lo[1] and y < b.hi[1]) break;
            } else continue;
            area += 1;
        }
    }
    return area;
}

/// The occupied cells of a footprint in square world units.
fn footprintArea(grid: Grid) f64 {
    var cells: f64 = 0;
    for (grid.rows, 0..) |row, y| for (0..row.len / 2) |x| {
        cells += @floatFromInt(@intFromBool(occupied(grid.rows, x, y)));
    };
    return @max(1, cells) * tile_size * tile_size;
}

/// The shading classes by the way they face in the picture's own frame.
const class_names = [8][]const u8{ "wall -y", "wall +x", "wall +y", "wall -x", "roof -y", "roof +x", "roof +y", "roof -x" };

// ---------------------------------------------------------------------------
// holdout and style: the quality gate

/// What a picture looks like as a whole, over its opaque pixels (alpha 128
/// or more): the mean colour, the hue histogram and the gradient energy (the
/// mean squared brightness difference across a pixel, horizontally plus
/// vertically, where its four neighbours are opaque too).
pub const Look = struct {
    rgb: [3]f64 = .{ 0, 0, 0 },
    count: f64 = 0,
    hues: [12]f64 = @splat(0),
    energy: f64 = 0,
    inner: f64 = 0,

    pub fn add(look: *Look, image: Image) void {
        for (0..image.height) |y| for (0..image.width) |x| {
            const p = image.pixels[y * image.width + x];
            if (p[3] < 128) continue;
            for (0..3) |c| look.rgb[c] += @floatFromInt(p[c]);
            look.count += 1;
            if (hueBand(p)) |band| look.hues[band] += 1;
            const xi: i64 = @intCast(x);
            const yi: i64 = @intCast(y);
            const gx = (opaqueLuma(image, xi + 1, yi) orelse continue) - (opaqueLuma(image, xi - 1, yi) orelse continue);
            const gy = (opaqueLuma(image, xi, yi + 1) orelse continue) - (opaqueLuma(image, xi, yi - 1) orelse continue);
            look.energy += gx * gx + gy * gy;
            look.inner += 1;
        };
    }

    /// The mean brightness.
    pub fn luma(look: Look) f64 {
        const n = @max(1, look.count);
        return 0.299 * look.rgb[0] / n + 0.587 * look.rgb[1] / n + 0.114 * look.rgb[2] / n;
    }
};

fn opaqueLuma(image: Image, x: i64, y: i64) ?f64 {
    const p = (image.at(x, y) orelse return null).*;
    return if (p[3] >= 128) luma(p) else null;
}

/// How a picture's look compares with another's: the ratio of their gradient
/// energy (below 1 is blurred or flat, above 1 noisier), the distance of
/// their mean colours (0..255 a channel) and of their hue histograms (L1,
/// as shares of every opaque pixel).
pub const Style = struct { sharpness: f64, colour: f64, hue: f64 };

pub fn compareLooks(a: Look, b: Look) Style {
    var colour: f64 = 0;
    for (a.rgb, b.rgb) |x, y| {
        const d = x / @max(1, a.count) - y / @max(1, b.count);
        colour += d * d;
    }
    var hue: f64 = 0;
    // As shares of every opaque pixel: a snowed pillbox has a few coloured
    // pixels, whose hue alone swung by 1.25 against its 2003 picture at a
    // colour distance of 10.
    for (a.hues, b.hues) |x, y| hue += @abs(x / @max(1, a.count) - y / @max(1, b.count));
    const ea = a.energy / @max(1, a.inner);
    const eb = b.energy / @max(1, b.inner);
    return .{ .sharpness = ea / @max(1e-9, eb), .colour = @sqrt(colour), .hue = hue };
}

/// A generated picture against the 2003 one of the same view: the outlines'
/// overlap, the structural similarity of their brightness and their looks.
/// `offset` takes a pixel of the 2003 picture to the generated one.
pub const Score = struct { iou: f64, ssim: f64, sharpness: f64, colour: f64, hue: f64 };

pub fn scorePicture(gpa: std.mem.Allocator, generated: Image, original: Image, offset: [2]i64) !Score {
    var a: Look = .{};
    a.add(generated);
    var b: Look = .{};
    b.add(original);
    const style = compareLooks(a, b);
    return .{ .iou = shiftedIou(generated, original, offset), .ssim = try ssim(gpa, generated, original, offset), .sharpness = style.sharpness, .colour = style.colour, .hue = style.hue };
}

fn isOpaque(image: Image, x: i64, y: i64) bool {
    const p = image.at(x, y) orelse return false;
    return p[3] >= 128;
}

/// The outlines' overlap when pixel (x, y) of `b` lies on (x, y) + offset of `a`.
pub fn shiftedIou(a: Image, b: Image, offset: [2]i64) f64 {
    var both: f64 = 0;
    var either: f64 = 0;
    for (0..b.height) |y| for (0..b.width) |x| {
        const in_a = isOpaque(a, @as(i64, @intCast(x)) + offset[0], @as(i64, @intCast(y)) + offset[1]);
        const in_b = b.pixels[y * b.width + x][3] >= 128;
        both += @floatFromInt(@intFromBool(in_a and in_b));
        either += @floatFromInt(@intFromBool(in_a or in_b));
    };
    // What of a lies outside b's canvas.
    for (0..a.height) |y| for (0..a.width) |x| {
        const bx = @as(i64, @intCast(x)) - offset[0];
        const by = @as(i64, @intCast(y)) - offset[1];
        if (a.pixels[y * a.width + x][3] >= 128 and (bx < 0 or by < 0 or bx >= b.width or by >= b.height)) either += 1;
    };
    return if (either == 0) 0 else both / either;
}

/// The offset with the best outline overlap, climbing from `start` in steps
/// of 8 down to 1 pixel.
pub fn bestOffset(a: Image, b: Image, start: [2]i64) [2]i64 {
    var best = start;
    var best_iou = shiftedIou(a, b, start);
    for ([_]i64{ 8, 4, 2, 1 }) |step| {
        var moved = true;
        while (moved) {
            moved = false;
            const at = best;
            for ([_][2]i64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 }, .{ -1, -1 }, .{ 1, 1 }, .{ -1, 1 }, .{ 1, -1 } }) |d| {
                const o = [2]i64{ at[0] + d[0] * step, at[1] + d[1] * step };
                const iou = shiftedIou(a, b, o);
                if (iou <= best_iou) continue;
                best_iou = iou;
                best = o;
                moved = true;
            }
        }
    }
    return best;
}

/// The mean structural similarity (Wang et al. 2004) of the two pictures'
/// brightness at a quarter of their size (4x4 pixels averaged), over the 5x5
/// windows opaque in both: the layout of walls, roofs and windows. A side
/// drawn from other sides cannot match the 2003 one pixel for pixel, and at
/// full size both the old and the new generator score near 0.
pub fn ssim(gpa: std.mem.Allocator, a: Image, b: Image, offset: [2]i64) !f64 {
    const w = b.width / 4;
    const h = b.height / 4;
    const cells = try gpa.alloc(?[2]f64, @as(usize, w) * h);
    defer gpa.free(cells);
    for (0..h) |cy| for (0..w) |cx| {
        var sum = [2]f64{ 0, 0 };
        cells[cy * w + cx] = for (0..16) |k| {
            const bx: i64 = @intCast(cx * 4 + k % 4);
            const by: i64 = @intCast(cy * 4 + k / 4);
            sum[0] += opaqueLuma(a, bx + offset[0], by + offset[1]) orelse break null;
            sum[1] += opaqueLuma(b, bx, by) orelse break null;
        } else .{ sum[0] / 16, sum[1] / 16 };
    };
    const c1 = (0.01 * 255.0) * (0.01 * 255.0);
    const c2 = (0.03 * 255.0) * (0.03 * 255.0);
    var total: f64 = 0;
    var windows: f64 = 0;
    for (2..@max(2, h) - 2) |y| for (2..@max(2, w) - 2) |x| {
        var s = [5]f64{ 0, 0, 0, 0, 0 };
        const window = for (0..25) |k| {
            const c = cells[(y + k / 5 - 2) * w + x + k % 5 - 2] orelse break false;
            s = .{ s[0] + c[0], s[1] + c[1], s[2] + c[0] * c[0], s[3] + c[1] * c[1], s[4] + c[0] * c[1] };
        } else true;
        if (!window) continue;
        const mu = s[0] / 25;
        const mv = s[1] / 25;
        const vu = s[2] / 25 - mu * mu;
        const vv = s[3] / 25 - mv * mv;
        const cov = s[4] / 25 - mu * mv;
        total += (2 * mu * mv + c1) * (2 * cov + c2) / ((mu * mu + mv * mv + c1) * (vu + vv + c2));
        windows += 1;
    };
    return if (windows == 0) 0 else total / windows;
}

/// Picture `image` moved by -offset onto a canvas of `width` x `height`, so
/// it lies on the 2003 picture it was scored against.
fn placeOn(gpa: std.mem.Allocator, image: Image, offset: [2]i64, width: u32, height: u32) !Image {
    const out: Image = .{ .width = width, .height = height, .pixels = try gpa.alloc([4]u8, @as(usize, width) * height) };
    for (0..height) |y| for (0..width) |x| {
        const from = image.at(@as(i64, @intCast(x)) + offset[0], @as(i64, @intCast(y)) + offset[1]);
        out.pixels[y * width + x] = if (from) |p| p.* else .{ 0, 0, 0, 0 };
    };
    return out;
}

/// A variant's folder name, with its family's in front for a numbered
/// subfolder (`dot_01`).
fn variantLabel(gpa: std.mem.Allocator, family: Family, index: usize) ![]const u8 {
    const name = std.fs.path.basename(family.variants[index].path);
    return if (std.ascii.isDigit(name[0])) std.fmt.allocPrint(gpa, "{s}_{s}", .{ family.name, name }) else name;
}

/// `holdout`: each variant's side drawn again from its family without it
/// (and without any variant showing that side), from the first other side of
/// its colourway, and scored against its 2003 pictures, damage state by
/// damage state.
fn holdoutCommand(ctx: *bk_tool.Context, out_arg: []const u8, dir_args: []const []const u8) !void {
    const arena = ctx.arena;
    const io = ctx.io;
    const cwd = std.Io.Dir.cwd();
    const out = try bk_tool.inputPath(arena, out_arg);
    try cwd.createDirPath(io, out);
    var md: std.Io.Writer.Allocating = .init(arena);
    try md.writer.writeAll(
        \\| Family | Held out | From | State | IoU | SSIM | Sharpness | Colour | Hue |
        \\|---|---|---|---|---|---|---|---|---|
        \\
    );
    var json: std.Io.Writer.Allocating = .init(arena);
    try json.writer.writeAll("{\"holdout\":[");
    var rows: usize = 0;
    for (dir_args) |dir_arg| {
        const dir = try bk_tool.inputPath(arena, dir_arg);
        const found = findFamily(arena, io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
        const name = try variantLabel(arena, found.family, found.index);
        const side = found.family.variants[found.index].side orelse return ctx.failKey("views.err.no_side", .{dir});
        const held = try loadSource(ctx, dir, side);
        const outline = pictureOf(held, "1") orelse return ctx.failKey("views.err.no_san", .{dir});
        const reference = for (found.family.variants, 0..) |variant, i| {
            const s = variant.side orelse continue;
            if (s == side) continue;
            const candidate = try loadSource(ctx, try std.fs.path.join(arena, &.{ found.root, variant.path }), s);
            if (sameColourway(outline, pictureOf(candidate, "1") orelse continue)) break i;
        } else {
            try md.writer.print("| {s} | {s} | - | no other side of its colourway | | | | | |\n", .{ found.family.name, name });
            try ctx.out.print("{s}: no other side of its colourway\n", .{name});
            continue;
        };
        const scene = try loadScene(ctx, found, reference, side, false);
        const from = try variantLabel(arena, found.family, reference);
        const k: u2 = @intCast((side + 4 - scene.own_side) % 4);
        // Drawn the way `side` draws the turn, the 2003-sprite fallback included.
        const turned = try drawTurn(ctx, scene, turnTarget(scene, k, try std.fmt.allocPrint(arena, "{s}_holdout", .{name})));
        const target = turned.target;
        const pictures = try std.fs.path.join(arena, &.{ out, name });
        cwd.deleteTree(io, pictures) catch {};
        try cwd.createDirPath(io, pictures);
        // The footprints laid on each other place the 2003 picture first.
        const shift = mfc_camera.project(blk: {
            const s = footprintShift(scene.sources[scene.fixed].footprint, held.footprint, k);
            break :blk .{ s[0], s[1], 0 };
        });
        const guess = [2]i64{
            @intFromFloat(@round(target.eye.anchor[0] - @as(f64, @floatFromInt(held.anchor[0])) - shift[0])),
            @intFromFloat(@round(target.eye.anchor[1] - @as(f64, @floatFromInt(held.anchor[1])) - shift[1])),
        };
        const drawn_from = scene.sources[scene.fixed];
        for (turned.drawn) |d| {
            const original = pictureOf(held, d.state) orelse continue;
            const offset = bestOffset(d.sprite.image, original, guess);
            var score = try scorePicture(arena, d.sprite.image, original, offset);
            // Sharpness is what the drawing adds to the picture it was drawn
            // from, at the brightness of each: the 2003 sides differ by
            // themselves (dot_h_01's snowed ruin has 1.32 times the gradient
            // energy of _02's, leaders_01's lit board 2.29 times _02's shaded
            // one), and relighting a face scales its gradients with it.
            if (pictureOf(drawn_from, d.state)) |p| {
                var a: Look = .{};
                a.add(d.sprite.image);
                var b: Look = .{};
                b.add(p);
                const ratio = b.luma() / @max(1, a.luma());
                score.sharpness = compareLooks(a, b).sharpness * ratio * ratio;
            }
            try writeOut(ctx, pictures, try std.fmt.allocPrint(arena, "{s}.png", .{d.state}), try writePng(arena, try placeOn(arena, d.sprite.image, offset, original.width, original.height)));
            try writeOut(ctx, pictures, try std.fmt.allocPrint(arena, "{s}_2003.png", .{d.state}), try writePng(arena, original));
            try md.writer.print("| {s} | {s} | {s} | {s} | {d:.3} | {d:.3} | {d:.2} | {d:.1} | {d:.3} |\n", .{ found.family.name, name, from, d.state, score.iou, score.ssim, score.sharpness, score.colour, score.hue });
            try json.writer.print("{s}\n{{\"family\":\"{s}\",\"variant\":\"{s}\",\"from\":\"{s}\",\"state\":\"{s}\",\"iou\":{d:.4},\"ssim\":{d:.4},\"sharpness\":{d:.4},\"colour\":{d:.2},\"hue\":{d:.4}}}", .{ if (rows > 0) "," else "", found.family.name, name, from, d.state, score.iou, score.ssim, score.sharpness, score.colour, score.hue });
            try ctx.out.print("{s} from {s}, state {s}: IoU {d:.3}, SSIM {d:.3}, sharpness {d:.2}, colour {d:.1}, hue {d:.3}\n", .{ name, from, d.state, score.iou, score.ssim, score.sharpness, score.colour, score.hue });
            rows += 1;
        }
    }
    try json.writer.writeAll("]}\n");
    try writeOut(ctx, out, "holdout.md", md.written());
    try writeOut(ctx, out, "holdout.json", json.written());
}

/// `style`: each generated picture (`<state>.png` in the folders given, as
/// `side` leaves them in its work folder and `holdout` writes them) against
/// the 2003 pictures of the same damage state of every side of the variant's
/// family in its colourway.
fn styleCommand(ctx: *bk_tool.Context, out_arg: []const u8, dir_arg: []const u8, generated_args: []const []const u8) !void {
    const arena = ctx.arena;
    const io = ctx.io;
    const out = try bk_tool.inputPath(arena, out_arg);
    const dir = try bk_tool.inputPath(arena, dir_arg);
    const found = findFamily(arena, io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
    const own = try loadSource(ctx, dir, 0);
    const outline = pictureOf(own, "1") orelse return ctx.failKey("views.err.no_san", .{dir});
    var sources: std.ArrayList(Source) = .empty;
    const measured = found.family.variants[found.index].side != null;
    for (found.family.variants, 0..) |variant, i| {
        // A variant whose side was not measured is drawn from itself alone
        // and judged so: dzot_02's ruin has 0.66 times the gradient energy of
        // its family's others.
        if (!measured and i != found.index) continue;
        const source = try loadSource(ctx, try std.fs.path.join(arena, &.{ found.root, variant.path }), variant.side orelse 0);
        if (sameColourway(outline, pictureOf(source, "1") orelse continue)) try sources.append(arena, source);
    }
    try std.Io.Dir.cwd().createDirPath(io, out);
    var md: std.Io.Writer.Allocating = .init(arena);
    try md.writer.writeAll(
        \\| Generated | State | Sharpness | Colour | Hue |
        \\|---|---|---|---|---|
        \\
    );
    var json: std.Io.Writer.Allocating = .init(arena);
    try json.writer.writeAll("{\"style\":[");
    var rows: usize = 0;
    for (generated_args) |generated_arg| {
        const generated = try bk_tool.inputPath(arena, generated_arg);
        const name = std.fs.path.basename(generated);
        const files = entries(arena, io, generated, .file) catch return ctx.failKey("views.err.no_dir", .{generated});
        for (files) |file| {
            if (!std.mem.endsWith(u8, file, ".png")) continue;
            const state = file[0 .. file.len - 4];
            const digits = std.mem.indexOfNone(u8, state, "0123456789") orelse state.len;
            if (digits == 0 or !(digits == state.len or std.mem.eql(u8, state[digits..], "w"))) continue;
            const path = try std.fs.path.join(arena, &.{ generated, file });
            const picture = readStoredPng(arena, try readFile(ctx, path)) catch |err| return ctx.failKey("views.err.read", .{ path, @errorName(err) });
            var a: Look = .{};
            a.add(picture);
            var b: Look = .{};
            for (sources.items) |source| b.add(pictureOf(source, state) orelse continue);
            if (b.count == 0) continue;
            const s = compareLooks(a, b);
            // A damage state whose 2003 picture is the intact one (props
            // that do not break: tarpaulin stacks, tanks) is judged as intact.
            const intact: []const u8 = if (std.mem.endsWith(u8, state, "w")) "1w" else "1";
            const judged = if (pictureOf(own, state)) |damaged| if (pictureOf(own, intact)) |whole|
                if (damaged.width == whole.width and damaged.height == whole.height and std.mem.eql(u8, std.mem.sliceAsBytes(damaged.pixels), std.mem.sliceAsBytes(whole.pixels))) intact else state
            else
                state else state;
            try md.writer.print("| {s} | {s} | {d:.2} | {d:.1} | {d:.3} |\n", .{ name, state, s.sharpness, s.colour, s.hue });
            try json.writer.print("{s}\n{{\"generated\":\"{s}\",\"state\":\"{s}\",\"judged\":\"{s}\",\"sharpness\":{d:.4},\"colour\":{d:.2},\"hue\":{d:.4}}}", .{ if (rows > 0) "," else "", name, state, judged, s.sharpness, s.colour, s.hue });
            try ctx.out.print("{s}, state {s}: sharpness {d:.2}, colour {d:.1}, hue {d:.3}\n", .{ name, state, s.sharpness, s.colour, s.hue });
            rows += 1;
        }
    }
    try json.writer.writeAll("]}\n");
    try writeOut(ctx, out, "style.md", md.written());
    try writeOut(ctx, out, "style.json", json.written());
}

// ---------------------------------------------------------------------------
// report: the batch's tracked evidence

const Inventory = struct {
    families: []const struct {
        family: []const u8,
        region: []const u8,
        variants: []const struct { path: []const u8, side: ?u8 = null, winter: bool = false },
    },
};

const GateFloors = struct {
    states: std.json.ArrayHashMap(struct { iou: f64, sharpness: [2]f64, colour: f64, hue: f64 }),
    /// Variants that pass the measures but whose turns look wrong on the
    /// contact sheet, with the reason: listed for hand fixing all the same.
    by_eye: ?std.json.ArrayHashMap([]const u8) = null,
};

const SideReport = struct {
    new_side: u8,
    fallback: bool,
    mirror: bool = false,
    sibling: []const u8 = "",
    hand: bool = false,
    shape: struct { height: f64, ground: f64, cover: f64, protrusion: f64, protrusion_2003: f64, low: f64 = 0, low_kept: f64 = 0, roof: f64 = 0, roof_kept: f64 = 0, stretch: f64 = 0, copied: f64 = 0, failure: []const u8 },
    pictures: []const struct { picture: []const u8, projected: f64, mirrored: f64, transferred: f64, filled: f64, model_iou: []const f64 },
};

const StyleRow = struct { generated: []const u8 = "", variant: []const u8 = "", state: []const u8, judged: ?[]const u8 = null, iou: ?f64 = null, sharpness: f64, colour: f64, hue: f64 };

/// Where a picture fails the gate's floors of its damage state (winter
/// counting as summer), or null.
fn gateFailure(gpa: std.mem.Allocator, floors: GateFloors, row: StyleRow) !?[]const u8 {
    const state = row.judged orelse row.state;
    const digits = std.mem.indexOfNone(u8, state, "0123456789") orelse state.len;
    const f = floors.states.map.get(state[0..digits]) orelse return null;
    if (row.iou) |iou| if (iou < f.iou) return try std.fmt.allocPrint(gpa, "IoU {d:.3} < {d:.2}", .{ iou, f.iou });
    if (row.sharpness < f.sharpness[0] or row.sharpness > f.sharpness[1]) return try std.fmt.allocPrint(gpa, "sharpness {d:.2} outside {d:.2}..{d:.2}", .{ row.sharpness, f.sharpness[0], f.sharpness[1] });
    if (row.colour > f.colour) return try std.fmt.allocPrint(gpa, "colour {d:.1} > {d:.0}", .{ row.colour, f.colour });
    if (row.hue > f.hue) return try std.fmt.allocPrint(gpa, "hue {d:.3} > {d:.2}", .{ row.hue, f.hue });
    return null;
}

fn readJson(comptime T: type, ctx: *bk_tool.Context, path: []const u8) !?T {
    const bytes = std.Io.Dir.cwd().readFileAlloc(ctx.io, path, ctx.arena, .limited(64 * 1024 * 1024)) catch return null;
    return std.json.parseFromSliceLeaky(T, ctx.arena, bytes, .{ .ignore_unknown_fields = true }) catch |err|
        return ctx.failKey("views.err.read", .{ path, @errorName(err) });
}

/// Writes `text` with CRLF line ends, as every tracked text file has.
fn writeCrlf(ctx: *bk_tool.Context, dir: []const u8, name: []const u8, text: []const u8) !void {
    try writeOut(ctx, dir, name, try std.mem.replaceOwned(u8, ctx.arena, text, "\n", "\r\n"));
}

fn percent(part: f64, whole: f64) f64 {
    return if (whole == 0) 0 else 100 * part / whole;
}

/// `report`: for every variant of the inventory, what the batch left in
/// `<work-root>/<variant path>/` (side's source maps, style.json and
/// holdout.json) as `<family>.md` per family and `batch.md`.
fn reportCommand(ctx: *bk_tool.Context, inventory_arg: []const u8, gate_arg: []const u8, work_arg: []const u8, out_arg: []const u8) !void {
    const arena = ctx.arena;
    const inventory_path = try bk_tool.inputPath(arena, inventory_arg);
    const gate_path = try bk_tool.inputPath(arena, gate_arg);
    const work = try bk_tool.inputPath(arena, work_arg);
    const out = try bk_tool.inputPath(arena, out_arg);
    const inventory = (try readJson(Inventory, ctx, inventory_path)) orelse return ctx.failKey("views.err.read", .{ inventory_path, "FileNotFound" });
    const floors = (try readJson(GateFloors, ctx, gate_path)) orelse return ctx.failKey("views.err.read", .{ gate_path, "FileNotFound" });
    try std.Io.Dir.cwd().createDirPath(ctx.io, out);

    var index: std.Io.Writer.Allocating = .init(arena);
    const x = &index.writer;
    var counts = struct { variants: usize = 0, folders: usize = 0, missing: usize = 0, pass: usize = 0, held: usize = 0, held_pass: usize = 0, same: usize = 0, shape: usize = 0, eye: usize = 0, hand: usize = 0 }{};
    for (inventory.families) |family| {
        var doc: std.Io.Writer.Allocating = .init(arena);
        const d = &doc.writer;
        try d.print(
            \\# {s} ({s})
            \\
            \\Generated by `zig build building-views-batch-{s}`, written by `bk-views report`. Shares are of the
            \\pixels the blocks cover; style is against the family's 2003 pictures of the colourway; gate floors are
            \\`tools/zig/fixtures/bk-views/gate.json`.
            \\
            \\| Variant | Turn | Side | Picture | Projected % | Mirrored % | Transferred % | Filled % | Sharpness | Colour | Hue | Gate |
            \\|---|---|---|---|---|---|---|---|---|---|---|---|
            \\
        , .{ family.family, family.region, family.region });
        var shapes: std.Io.Writer.Allocating = .init(arena);
        var held_rows: std.Io.Writer.Allocating = .init(arena);
        for (family.variants) |variant| {
            counts.variants += 1;
            const name = std.fs.path.basename(variant.path);
            const dir = try std.fs.path.join(arena, &.{ work, variant.path });
            const style = try readJson(struct { style: []const StyleRow }, ctx, try std.fs.path.join(arena, &.{ dir, "style", "style.json" }));
            var failure: ?[]const u8 = null;
            var generated: usize = 0;
            var same_turns: std.ArrayList([]const u8) = .empty;
            var hand = false;
            var new_sides: std.Io.Writer.Allocating = .init(arena);
            for (1..4) |k| {
                const target = try std.fmt.allocPrint(arena, "{s}_r{d}", .{ name, k });
                const side = (try readJson(SideReport, ctx, try std.fs.path.join(arena, &.{ dir, try std.fmt.allocPrint(arena, "{s}.map", .{target}), "sources.json" }))) orelse continue;
                generated += 1;
                if (variant.side != null) try new_sides.writer.print("{s}{d}", .{ if (k > 1) ", " else "", side.new_side });
                hand = hand or side.hand;
                if (side.fallback) try same_turns.append(arena, try std.fmt.allocPrint(arena, "r{d}{s}{s}", .{ k, if (side.mirror) " mirrored" else if (side.sibling.len > 0) " = " else "", side.sibling }));
                if (side.shape.failure.len > 0 and !side.fallback and failure == null) failure = try std.fmt.allocPrint(arena, "r{d} shape: {s}", .{ k, side.shape.failure });
                for (side.pictures) |picture| {
                    if (std.mem.endsWith(u8, picture.picture, "g")) continue;
                    const all = picture.projected + picture.mirrored + picture.transferred + picture.filled;
                    var row_gate: []const u8 = "-";
                    var row: ?StyleRow = null;
                    if (style) |s| for (s.style) |r| {
                        if (std.mem.eql(u8, r.generated, target) and std.mem.eql(u8, r.state, picture.picture)) row = r;
                    };
                    if (row) |r| {
                        const why = try gateFailure(arena, floors, r);
                        row_gate = if (why) |w| w else "pass";
                        if (why != null and failure == null) failure = try std.fmt.allocPrint(arena, "r{d} {s}: {s}", .{ k, picture.picture, why.? });
                    }
                    try d.print("| {s} | r{d} | {d} | {s} | {d:.1} | {d:.1} | {d:.1} | {d:.1} | ", .{ name, k, side.new_side, picture.picture, percent(picture.projected, all), percent(picture.mirrored, all), percent(picture.transferred, all), percent(picture.filled, all) });
                    if (row) |r| try d.print("{d:.2} | {d:.1} | {d:.3} | {s} |\n", .{ r.sharpness, r.colour, r.hue, row_gate }) else try d.writeAll("- | - | - | - |\n");
                }
                const sh = side.shape;
                try shapes.writer.print("| {s} | r{d} | {d:.2} | {d:.2} | {d:.2} | {d:.3} | {d:.3} | {d:.0} | {d:.0} | {d:.0} | {d:.0} | {d:.3} | {d:.0} | {s} | {s} |\n", .{ name, k, sh.height, sh.ground, sh.cover, sh.protrusion, sh.protrusion_2003, sh.low, sh.low_kept, sh.roof, sh.roof_kept, sh.stretch, sh.copied, if (sh.failure.len > 0) sh.failure else "pass", if (side.fallback) "2003 sprite" else "blocks" });
            }
            // The hold-out where the family has another side of the colourway.
            var held: []const u8 = "-";
            if (try readJson(struct { holdout: []const StyleRow }, ctx, try std.fs.path.join(arena, &.{ dir, "holdout", "holdout.json" }))) |h| if (h.holdout.len > 0) {
                counts.held += 1;
                var why: ?[]const u8 = null;
                for (h.holdout) |r| {
                    const w = try gateFailure(arena, floors, r);
                    if (w != null and why == null) why = try std.fmt.allocPrint(arena, "{s}: {s}", .{ r.state, w.? });
                    try held_rows.writer.print("| {s} | {s} | {d:.3} | {d:.2} | {d:.1} | {d:.3} | {s} |\n", .{ name, r.state, r.iou.?, r.sharpness, r.colour, r.hue, w orelse "pass" });
                }
                held = why orelse "pass";
                if (why == null) counts.held_pass += 1;
                if (why != null and failure == null) failure = try std.fmt.allocPrint(arena, "hold-out {s}", .{why.?});
            };
            counts.folders += generated;
            if (failure == null) if (floors.by_eye) |eye| if (eye.map.get(variant.path)) |why| {
                failure = try std.fmt.allocPrint(arena, "by eye: {s}", .{why});
                counts.eye += 1;
            };
            if (generated < 3) {
                counts.missing += 1;
                failure = try std.fmt.allocPrint(arena, "not generated: {d} of 3 turns", .{generated});
            } else if (failure == null) counts.pass += 1;
            if (same_turns.items.len > 0) counts.same += 1;
            if (hand) counts.hand += 1;
            if (failure != null and std.mem.indexOf(u8, failure.?, "shape") != null) counts.shape += 1;
            try x.print("| {s} | {s} | {s} | {s} | {s} | {s} | {s} | {s} |\n", .{
                family.region,
                family.family,
                variant.path,
                if (variant.side) |s| try std.fmt.allocPrint(arena, "{d}", .{s}) else "?",
                if (variant.side == null) "? (turns 1, 2, 3)" else new_sides.written(),
                failure orelse "pass",
                held,
                if (same_turns.items.len > 0) try std.mem.join(arena, ", ", same_turns.items) else "-",
            });
        }
        try d.print(
            \\
            \\Shape of the turned intact sprite drawn by the blocks: its height over the 2003 one's, the blocks' ground
            \\over the footprint's cells, the outline IoU of the blocks against the variant's 2003 picture, the protrusion
            \\of the turned and of the 2003 outline, the pixels the turn shows of the low parts the 2003 sides show below
            \\the walls (ramps, steps, porches) and of those the turn holds, the pixels it shows of the walls of the roof
            \\parts (vents, dormers) and of those it draws as walls, the share of its pixels that still spread one 2003
            \\pixel over four or more (stretched), the pixels of the body it copied from beside a low or roof part of
            \\another side (copied); a thin object, or a small one whose shape fails, is drawn as its 2003
            \\sprite where the turn keeps the footprint, a heap (`tools/zig/fixtures/bk-views/props.json`) on every turn.
            \\
            \\| Variant | Turn | Height | Ground | Outline | Protrusion | 2003 | Low parts | Kept | Roof parts | Kept | Stretched | Copied | Shape | Drawn as |
            \\|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
            \\{s}
        , .{shapes.written()});
        if (held_rows.written().len > 0) try d.print(
            \\
            \\Hold-out: the variant's own side drawn without it, against the 2003 picture.
            \\
            \\| Variant | State | IoU | Sharpness | Colour | Hue | Gate |
            \\|---|---|---|---|---|---|---|
            \\{s}
        , .{held_rows.written()});
        try writeCrlf(ctx, out, try std.fmt.allocPrint(arena, "{s}.md", .{family.family}), doc.written());
    }
    var md: std.Io.Writer.Allocating = .init(arena);
    try md.writer.print(
        \\# Building views batch
        \\
        \\Every variant of `Data/Buildings` turned by one, two and three quarter turns with `bk-views side`, by
        \\`zig build building-views-batch-<group>` into `zig-out/remastered/Buildings/<region>/summer/<variant>_r<k>/`
        \\(a numbered subfolder `dot/01` gives `dot/01_r1`); each pack passed `bk-views-check` and decomposed back, or
        \\the step failed. Per family: `<family>.md`. Written by `bk-views report`.
        \\
        \\- Variants: {d}; generated folders: {d} of {d}; not generated: {d}.
        \\- Gate: the shape of every turn drawn by the blocks (its intact sprite's height {d:.1}..{d:.1} times the 2003
        \\  one's, the blocks' ground {d:.1}..{d:.1} times the footprint's cells, their outline IoU {d:.2} or more against
        \\  the variant's 2003 picture, its protrusion at most {d:.1} times the 2003 one's plus {d:.2}, and it holds
        \\  {d:.1} of the pixels it shows of the low parts the 2003 sides of the colourway show, ramps, steps and
        \\  porches, where they show more than {d:.0}, and draws as walls as much of the walls of the roof parts
        \\  in `tools/zig/fixtures/bk-views/roof-parts`, at most {d:.2} of its pixels still stretched, and no more than
        \\  {d:.0} pixels of the body copied from beside a part, a chimney, dormer or door step of another side), the style
        \\  of every generated picture, and the hold-out where the family has another side of the colourway, against
        \\  the floors of `tools/zig/fixtures/bk-views/gate.json`: {d} of {d} variants pass; hold-out {d} of {d}; {d}
        \\  fail on the shape; {d} more pass the measures but look wrong on the contact sheet (`bk-views contact`),
        \\  listed in gate.json's `by_eye`. A failing variant is listed with its first reason, to be fixed by hand.
        \\- 2003 sprite: {d} variants have turns drawn as their 2003 sprites, moved onto the turned footprint: thin
        \\  objects (a 2003 protrusion of {d:.2} or more) and small ones ({d} footprint cells or fewer) whose blocks fail
        \\  the shape, where the turn keeps the footprint (cells IoU {d:.1} or more), or mirrored onto it on an odd
        \\  turn of a rectangular footprint; and the heaps (supply depots, earth pillboxes) on every turn, mirrored on
        \\  an odd one.
        \\- Hand shapes: {d} variants are drawn on blocks authored by hand (`tools/zig/fixtures/bk-views/shapes`).
        \\
        \\| Region | Family | Variant | Side | New sides | Gate | Hold-out | 2003 sprite |
        \\|---|---|---|---|---|---|---|---|
        \\{s}
    , .{ counts.variants, counts.folders, 3 * counts.variants, counts.missing, height_range[0], height_range[1], ground_ratio[0], ground_ratio[1], outline_floor, protrusion_factor, protrusion_margin, low_share, @as(f64, low_margin), stretch_most, @as(f64, low_margin), counts.pass, counts.variants, counts.held_pass, counts.held, counts.shape, counts.eye, counts.same, thin_object, small_object, same_footprint, counts.hand, index.written() });
    try writeCrlf(ctx, out, "batch.md", md.written());
    try ctx.out.print("{d} variants, {d} folders, {d} not generated, {d} pass the gate, {d} fail on the shape, {d} by eye, {d} with 2003 sprites\n", .{ counts.variants, counts.folders, counts.missing, counts.pass, counts.shape, counts.eye, counts.same });
}

// ---------------------------------------------------------------------------
// contact: the generated sides at a glance

/// 3x5 pixel letters, rows top first, three bits a row (the left pixel high).
fn glyph(c: u8) u15 {
    return switch (std.ascii.toLower(c)) {
        '0' => 0b111_101_101_101_111,
        '1' => 0b010_110_010_010_111,
        '2' => 0b111_001_111_100_111,
        '3' => 0b111_001_111_001_111,
        '4' => 0b101_101_111_001_001,
        '5' => 0b111_100_111_001_111,
        '6' => 0b111_100_111_101_111,
        '7' => 0b111_001_001_010_010,
        '8' => 0b111_101_111_101_111,
        '9' => 0b111_101_111_001_111,
        'a' => 0b010_101_111_101_101,
        'b' => 0b110_101_110_101_110,
        'c' => 0b011_100_100_100_011,
        'd' => 0b110_101_101_101_110,
        'e' => 0b111_100_110_100_111,
        'f' => 0b111_100_110_100_100,
        'g' => 0b011_100_101_101_011,
        'h' => 0b101_101_111_101_101,
        'i' => 0b111_010_010_010_111,
        'j' => 0b001_001_001_101_010,
        'k' => 0b101_101_110_101_101,
        'l' => 0b100_100_100_100_111,
        'm' => 0b101_111_111_101_101,
        'n' => 0b110_101_101_101_101,
        'o' => 0b010_101_101_101_010,
        'p' => 0b110_101_110_100_100,
        'q' => 0b010_101_101_110_011,
        'r' => 0b110_101_110_101_101,
        's' => 0b011_100_010_001_110,
        't' => 0b111_010_010_010_010,
        'u' => 0b101_101_101_101_111,
        'v' => 0b101_101_101_101_010,
        'w' => 0b101_101_111_111_101,
        'x' => 0b101_101_010_101_101,
        'y' => 0b101_101_010_010_010,
        'z' => 0b111_001_010_100_111,
        '_' => 0b000_000_000_000_111,
        '/' => 0b001_001_010_100_100,
        '.' => 0b000_000_000_000_010,
        '-' => 0b000_000_111_000_000,
        else => 0,
    };
}

/// `text` in white letters twice the glyphs' size, top left at (x, y).
fn drawText(image: Image, x: usize, y: usize, text: []const u8) void {
    for (text, 0..) |c, i| {
        const g = glyph(c);
        for (0..5) |row| for (0..3) |col| {
            if ((g >> @intCast((4 - row) * 3 + 2 - col)) & 1 == 0) continue;
            for (0..4) |k| {
                const p = image.at(@intCast(x + i * 8 + col * 2 + k % 2), @intCast(y + row * 2 + k / 2)) orelse continue;
                p.* = .{ 255, 255, 255, 255 };
            }
        };
    }
}

/// A variant folder's intact sprite (`1.san` with `1_h.dds`), cut to its squares.
fn intactSprite(ctx: *bk_tool.Context, gpa: std.mem.Allocator, dir: []const u8) !?Image {
    const io = ctx.io;
    const cwd = std.Io.Dir.cwd();
    const san = cwd.readFileAlloc(io, try std.fs.path.join(gpa, &.{ dir, "1.san" }), gpa, .limited(64 * 1024 * 1024)) catch return null;
    const dds = cwd.readFileAlloc(io, try std.fs.path.join(gpa, &.{ dir, "1_h.dds" }), gpa, .limited(256 * 1024 * 1024)) catch return null;
    const pack = decodePack(gpa, san) catch |err| return ctx.failKey("views.err.bad_san", .{ dir, @errorName(err) });
    const atlas = readDds(gpa, dds) catch |err| return ctx.failKey("views.err.bad_dds", .{ dir, @errorName(err) });
    if (pack.sprites.len == 0) return null;
    var bounds: Bounds = .{};
    for (pack.sprites[0].squares) |square| bounds.add(square);
    if (bounds.min_x >= bounds.max_x) return null;
    return try unpack(gpa, pack.sprites[0], atlas, bounds);
}

/// `image` at half its size laid over `background`, at (x, y) of `page`.
fn pasteHalf(page: Image, image: Image, x: usize, y: usize, background: [3]u8) void {
    for (0..image.height / 2) |v| for (0..image.width / 2) |u| {
        var sum = [3]f64{ 0, 0, 0 };
        for (0..4) |k| {
            const p = image.pixels[(v * 2 + k / 2) * image.width + u * 2 + k % 2];
            const a = @as(f64, @floatFromInt(p[3])) / 255;
            for (0..3) |c| sum[c] += @as(f64, @floatFromInt(p[c])) * a + @as(f64, @floatFromInt(background[c])) * (1 - a);
        }
        const to = page.at(@intCast(x + u), @intCast(y + v)) orelse continue;
        to.* = .{ @intFromFloat(sum[0] / 4), @intFromFloat(sum[1] / 4), @intFromFloat(sum[2] / 4), 255 };
    };
}

/// The height of a contact page in pixels; a row that would reach past it starts the next page.
const contact_page = 1600;

/// `contact`: every variant of `<buildings-dir>` a row, its 2003 intact sprite
/// (on blue) and its generated turns `_r1.._r3` from `<generated-dir>` (on
/// grey) at half size, labelled, a family's variants together under a yellow
/// line, on pages `contact-<n>.png` in `<out-dir>`.
fn contactCommand(ctx: *bk_tool.Context, buildings_arg: []const u8, generated_arg: []const u8, out_arg: []const u8) !void {
    const arena = ctx.arena;
    const buildings = try bk_tool.inputPath(arena, buildings_arg);
    const generated = try bk_tool.inputPath(arena, generated_arg);
    const out = try bk_tool.inputPath(arena, out_arg);
    try std.Io.Dir.cwd().createDirPath(ctx.io, out);
    const Row = struct { label: []const u8, first: bool, sprites: [4]?Image };
    var rows: std.ArrayList(Row) = .empty;
    // A family's first row shown starts under a line.
    var last: ?*const Family = null;
    for (try listFamilies(arena, ctx.io, buildings)) |*family| for (family.variants) |variant| {
        var row: Row = .{ .label = variant.path, .first = last != family, .sprites = @splat(null) };
        var loading = std.heap.ArenaAllocator.init(ctx.gpa);
        defer loading.deinit();
        const gpa = loading.allocator();
        for (0..4) |k| {
            const dir = if (k == 0) try std.fs.path.join(gpa, &.{ buildings, variant.path }) else try std.fmt.allocPrint(gpa, "{s}_r{d}", .{ try std.fs.path.join(gpa, &.{ generated, variant.path }), k });
            const sprite = try intactSprite(ctx, gpa, dir) orelse continue;
            row.sprites[k] = .{ .width = sprite.width, .height = sprite.height, .pixels = try arena.dupe([4]u8, sprite.pixels) };
        }
        if (row.sprites[1] == null and row.sprites[2] == null and row.sprites[3] == null) continue;
        try rows.append(arena, row);
        last = family;
    };
    var pages: usize = 0;
    var start: usize = 0;
    while (start < rows.items.len) {
        // The rows that fit the page, and the page's width.
        var end = start;
        var height: usize = 0;
        var width: usize = 400;
        while (end < rows.items.len) : (end += 1) {
            var w: usize = 4;
            var h: usize = 0;
            for (rows.items[end].sprites) |s| if (s) |sprite| {
                w += sprite.width / 2 + 6;
                h = @max(h, sprite.height / 2);
            };
            if (end > start and height + h + 20 > contact_page) break;
            height += h + 20;
            width = @max(width, w, rows.items[end].label.len * 8 + 8);
        }
        const page: Image = .{ .width = @intCast(width), .height = @intCast(height), .pixels = try arena.alloc([4]u8, width * height) };
        @memset(page.pixels, .{ 40, 40, 40, 255 });
        var y: usize = 0;
        for (rows.items[start..end]) |row| {
            if (row.first) for (0..width) |x| {
                page.pixels[y * width + x] = .{ 220, 180, 0, 255 };
            };
            drawText(page, 4, y + 3, row.label);
            var x: usize = 4;
            var h: usize = 0;
            for (row.sprites, 0..) |s, k| if (s) |sprite| {
                pasteHalf(page, sprite, x, y + 16, if (k == 0) .{ 60, 80, 110 } else .{ 96, 96, 96 });
                x += sprite.width / 2 + 6;
                h = @max(h, sprite.height / 2);
            };
            y += h + 20;
        }
        pages += 1;
        try writeOut(ctx, out, try std.fmt.allocPrint(arena, "contact-{d:0>2}.png", .{pages}), try writePng(arena, page));
        start = end;
    }
    try ctx.out.print("{d} variants on {d} pages in {s}\n", .{ rows.items.len, pages, out });
}
