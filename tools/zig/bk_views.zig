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
const bk_pdf = @import("bk_pdf.zig");

const usage: bk_tool.Usage = .{
    .synopsis =
    \\usage: bk-views decompose <variant-dir> <out-dir>
    \\       bk-views camera <variant-dir>... [--out <camera.json>]
    \\       bk-views inventory <buildings-dir> <out-dir>
    \\       bk-views side <variant-dir> <turns> <out-dir> [<work-dir> | --no-work] [--deps=<file>]
    \\       bk-views holdout <out-dir> <variant-dir>...
    \\       bk-views style <out-dir> <variant-dir> <pictures-dir>...
    \\       bk-views report <inventory.json> <gate.json> <work-root> <out-dir>
    \\       bk-views contact <buildings-dir> <generated-dir> <out-dir>
    \\       bk-views shape <out-dir> <variant-dir>...
    \\       bk-views relight <objects-dir> <out-dir> [--deps=<file>]
    \\       bk-views ids <ids.json> <buildings-dir> <objects-dir>
    \\       bk-views models <models-dir> <ids.json> <variant-dir>...
    \\       bk-views review <out.pdf> <ids.json> <models-dir> <buildings-dir> <objects-dir>
    \\       bk-views import <family-review-manifest.json> <meshes-dir>
    \\       bk-views model3d <out-dir> <variant-dir>... [--deps=<file>]
    \\       bk-views full3d-review <out.pdf> <models-dir>
    \\
    ,
    .body = &.{"views.usage.body"},
};

pub fn main(init: std.process.Init) u8 {
    return bk_tool.main(init, "bk-views", usage, run);
}

/// The options side, relight and model3d take after their arguments, for a build
/// system that caches their runs (build.zig's install-all): `--deps=<file>` writes a
/// make-style list of every input file the run read, so zig build runs it again only
/// when one of them changed, and `--no-work` (side) keeps the pictures and source maps
/// in a scratch folder in <out-dir> that is removed when done, as nothing reads them.
var deps_path: ?[]const u8 = null;
var no_work = false;

fn run(ctx: *bk_tool.Context) !void {
    if (ctx.args.len == 0) return error.Usage;
    const command = ctx.args[0];
    if (std.mem.eql(u8, command, "side") or std.mem.eql(u8, command, "relight") or std.mem.eql(u8, command, "model3d")) {
        var kept: std.ArrayList([]const u8) = .empty;
        for (ctx.args) |arg| {
            if (std.mem.startsWith(u8, arg, "--deps=") and arg.len > "--deps=".len) {
                deps_path = arg["--deps=".len..];
            } else if (std.mem.eql(u8, command, "side") and std.mem.eql(u8, arg, "--no-work")) {
                no_work = true;
            } else try kept.append(ctx.arena, arg);
        }
        ctx.args = kept.items;
    }
    if (std.mem.eql(u8, command, "decompose")) {
        if (ctx.args.len != 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return decomposeCommand(ctx, ctx.args[1], ctx.args[2]);
    }
    if (std.mem.eql(u8, command, "relight")) {
        if (ctx.args.len != 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return relightCommand(ctx, ctx.args[1], ctx.args[2]);
    }
    if (std.mem.eql(u8, command, "inventory")) {
        if (ctx.args.len != 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return inventoryCommand(ctx, ctx.args[1], ctx.args[2]);
    }
    if (std.mem.eql(u8, command, "side")) {
        if (ctx.args.len != 4 and ctx.args.len != 5) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        if (no_work and ctx.args.len == 5) return error.Usage;
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
    if (std.mem.eql(u8, command, "ids")) {
        if (ctx.args.len != 4) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return idsCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3]);
    }
    if (std.mem.eql(u8, command, "models")) {
        if (ctx.args.len < 4) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return modelsCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3..]);
    }
    if (std.mem.eql(u8, command, "review")) {
        if (ctx.args.len != 6) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return reviewCommand(ctx, ctx.args[1], ctx.args[2], ctx.args[3], ctx.args[4], ctx.args[5]);
    }
    if (std.mem.eql(u8, command, "import")) {
        if (ctx.args.len != 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return importCommand(ctx, ctx.args[1], ctx.args[2]);
    }
    if (std.mem.eql(u8, command, "model3d")) {
        if (ctx.args.len < 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return model3dCommand(ctx, ctx.args[1], ctx.args[2..]);
    }
    if (std.mem.eql(u8, command, "full3d-review")) {
        if (ctx.args.len != 3) return error.Usage;
        for (ctx.args[1..]) |arg| if (std.mem.startsWith(u8, arg, "-")) return error.Usage;
        return full3dReviewCommand(ctx, ctx.args[1], ctx.args[2]);
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
pub const writePngDeflated = formats.writePngDeflated;
pub const readPng = formats.readPng;
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

/// The input files of the run, for `--deps=`.
var inputs: std.ArrayList([]const u8) = .empty;

/// Every file of `dir` (not below it) as an input of the run.
fn depDir(ctx: *bk_tool.Context, dir: []const u8) !void {
    if (deps_path == null) return;
    const files = entries(ctx.arena, ctx.io, dir, .file) catch return;
    for (files) |name| try inputs.append(ctx.arena, try std.fs.path.join(ctx.arena, &.{ dir, name }));
}

/// `path` as an input of the run where it exists. A fixture the run looked for and
/// did not find is left out: zig build fails on an input that is not there.
fn depFile(ctx: *bk_tool.Context, path: []const u8) !void {
    if (deps_path == null) return;
    std.Io.Dir.cwd().access(ctx.io, path, .{}) catch return;
    try inputs.append(ctx.arena, path);
}

/// What a scene is drawn from: every variant folder of the family (the inventory
/// measures each one's side) and the fixtures loadScene and loadMeshScene read.
fn depScene(ctx: *bk_tool.Context, found: InFamily, scene: Scene) !void {
    if (deps_path == null) return;
    const arena = ctx.arena;
    for (found.family.variants) |variant| try depDir(ctx, try std.fs.path.join(arena, &.{ found.root, variant.path }));
    for ([_][]const u8{ shapes_dir, roof_parts_dir }) |dir| try depFile(ctx, try std.fmt.allocPrint(arena, "{s}/{s}.json", .{ dir, found.family.name }));
    try depFile(ctx, props_file);
    try depFile(ctx, shape_ids_file);
    try depFile(ctx, try std.fmt.allocPrint(arena, "{s}/families.json", .{mesh.meshes_dir}));
    if (try readJson(ShapeIds, ctx, shape_ids_file)) |ids| for (found.family.variants) |variant| {
        const id = (try shapeIdOf(ids, arena, try std.fs.path.join(arena, &.{ found.root, variant.path }))) orelse continue;
        try depFile(ctx, try std.fmt.allocPrint(arena, "{s}/{d:0>3}.json", .{ mesh.meshes_dir, id }));
    };
    if (scene.mesh) |ms| if (ms.model.shared) |name| try depFile(ctx, try std.fmt.allocPrint(arena, "{s}/shared/{s}.json", .{ mesh.meshes_dir, name }));
}

/// Writes the make-style dep file `--deps=` names, each input by its absolute
/// path and quoted: zig build resolves a relative one against its own working
/// folder, which need not be the run's.
fn writeDeps(ctx: *bk_tool.Context) !void {
    const path = deps_path orelse return;
    const cwd = try std.process.currentPathAlloc(ctx.io, ctx.arena);
    var text: std.Io.Writer.Allocating = .init(ctx.arena);
    const w = &text.writer;
    try w.writeAll("inputs:");
    for (inputs.items) |input| {
        try w.print(" \\\n \"{s}\"", .{if (std.fs.path.isAbsolute(input)) input else try std.fs.path.join(ctx.arena, &.{ cwd, input })});
    }
    try w.writeAll("\n");
    std.Io.Dir.cwd().writeFile(ctx.io, .{ .sub_path = path, .data = text.written() }) catch |err|
        return ctx.failKey("views.err.read", .{ path, @errorName(err) });
}

// ---------------------------------------------------------------------------
// relight

/// The surface normal of each opaque pixel (alpha 128 or more) of a 2003
/// object picture in the variant's frame, guessed from its outline as a
/// body of revolution standing upright: across a row the surface turns from
/// the screen's left (world (-1, -1)) through the viewer (world (1, -1)) to
/// its right, and the upper half of each column's run leans up, to 0.7 at its
/// top (a crown, a lamp's cap). A tree, a post or a box drawn from the 2003
/// camera has no faces to fit blocks to; this is the shape the eye reads its
/// light on. Null for the rest.
pub fn spriteNormals(gpa: std.mem.Allocator, image: Image) ![]?[3]f64 {
    const w = image.width;
    const h = image.height;
    const out = try gpa.alloc(?[3]f64, @as(usize, w) * h);
    @memset(out, null);
    const rows = try gpa.alloc([2]f64, h);
    const columns = try gpa.alloc([2]f64, w);
    @memset(rows, .{ std.math.inf(f64), -std.math.inf(f64) });
    @memset(columns, .{ std.math.inf(f64), -std.math.inf(f64) });
    for (0..h) |y| for (0..w) |x| {
        if (image.pixels[y * w + x][3] < 128) continue;
        const fx: f64 = @floatFromInt(x);
        const fy: f64 = @floatFromInt(y);
        rows[y] = .{ @min(rows[y][0], fx), @max(rows[y][1], fx + 1) };
        columns[x] = .{ @min(columns[x][0], fy), @max(columns[x][1], fy + 1) };
    };
    const right = [2]f64{ std.math.sqrt1_2, std.math.sqrt1_2 };
    const viewer = [2]f64{ std.math.sqrt1_2, -std.math.sqrt1_2 };
    for (0..h) |y| for (0..w) |x| {
        if (image.pixels[y * w + x][3] < 128) continue;
        const half = @max(0.5, (rows[y][1] - rows[y][0]) / 2);
        const u = std.math.clamp((@as(f64, @floatFromInt(x)) + 0.5 - (rows[y][0] + rows[y][1]) / 2) / half, -1, 1);
        const tall = @max(1, columns[x][1] - columns[x][0]);
        const up = 0.7 * @max(0, 1 - 2 * (@as(f64, @floatFromInt(y)) + 0.5 - columns[x][0]) / tall);
        const side = @sqrt(1 - up * up);
        const front = @sqrt(1 - u * u);
        out[y * w + x] = .{ side * (u * right[0] + front * viewer[0]), side * (u * right[1] + front * viewer[1]), up };
    };
    return out;
}

/// An object's own lit side against its shaded side per channel: the mean
/// colours of the pixels the 2003 light (`shading`) lights most (direct light
/// 0.7 of the most or more) and of those it does not light, each channel held
/// between none and `wall_light_rgb`'s; with fewer than `object_pixels` of
/// either, the median. A crown that shows little of the light is relit
/// little.
pub fn objectLight(image: Image, normals: []const ?[3]f64) [3]f64 {
    const l = lightFor(light_2003, 0);
    var most: f64 = 0;
    for (normals) |n| if (n) |v| {
        most = @max(most, dot(v, l));
    };
    var means: Means = .{};
    for (image.pixels, normals) |p, n| if (n) |v| {
        const direct = dot(v, l);
        if (direct >= 0.7 * most) means.add(0, p) else if (direct <= 0) means.add(1, p);
    };
    if (means.count[0] < object_pixels or means.count[1] < object_pixels) return wall_light_rgb;
    var out: [3]f64 = undefined;
    for (&out, 0..) |*o, c| o.* = std.math.clamp((means.sum[0][c] / means.count[0]) / @max(1, means.sum[1][c] / means.count[1]), 1, wall_light_rgb[c]);
    return out;
}

/// Each side of an object seen on this many pixels or more gives it its own light.
pub const object_pixels = 50;

/// A 2003 object picture as camera step k shows it under the sun fixed in
/// the world (M006 round 2): each pixel relit from the 2003 light to the
/// light of turn -k (`shading`; the eye's frame turns by k, the sun does
/// not). `mirror`: the Game mirrors the picture at this step (a prop on a
/// longer rectangle at an odd step), so a pixel shows the face mirrored
/// across the screen's vertical (world (x, y) to (-y, -x)).
pub fn relightSprite(gpa: std.mem.Allocator, image: Image, normals: []const ?[3]f64, wall: [3]f64, k: u2, mirror: bool) !Image {
    const out: Image = .{ .width = image.width, .height = image.height, .pixels = try gpa.dupe([4]u8, image.pixels) };
    for (out.pixels, normals) |*p, n| if (n) |v| {
        const shown = if (mirror) [3]f64{ -v[1], -v[0], v[2] } else v;
        p.* = relight(p.*, shading(shown, 0 -% k, wall), shading(v, 0, wall));
    };
    return out;
}

/// `relight`: every object folder under objects-dir (one holding a `1.san`)
/// relit (`relightFolder`) into the same folder under out-dir, where the Game
/// finds it by the object's Data path once the folders lie in a mod.
fn relightCommand(ctx: *bk_tool.Context, dir_arg: []const u8, out_arg: []const u8) !void {
    const dir = try bk_tool.inputPath(ctx.arena, dir_arg);
    const out = try bk_tool.inputPath(ctx.arena, out_arg);
    const made = try relightTree(ctx, dir, out);
    try writeDeps(ctx);
    try ctx.out.print("wrote {s}: {d} relit packs\n", .{ out, made });
}

fn relightTree(ctx: *bk_tool.Context, dir: []const u8, out: []const u8) !usize {
    var made: usize = 0;
    // A folder whose pack is not a sprite pack (a mine's animation pack) keeps
    // its 2003 picture: noted on stdout, as a build step fails on a tool's stderr.
    if (hasName(try entries(ctx.arena, ctx.io, dir, .file), "1.san")) {
        try depDir(ctx, dir);
        if (decodePack(ctx.arena, try readFile(ctx, try std.fs.path.join(ctx.arena, &.{ dir, "1.san" })))) |_| {
            made += try relightFolder(ctx, dir, out);
        } else |err| try ctx.out.print("{s}: kept as drawn ({s})\n", .{ dir, @errorName(err) });
    }
    for (try entries(ctx.arena, ctx.io, dir, .directory)) |sub| {
        made += try relightTree(ctx, try std.fs.path.join(ctx.arena, &.{ dir, sub }), try std.fs.path.join(ctx.arena, &.{ out, sub }));
    }
    return made;
}

/// An object's pictures (flora, props: every one-sprite pack but shadows and
/// ground pictures) as each camera step shows them under the sun fixed in
/// the world, `<pack>_l<k>` for k 1 to 3 in out (CMOObject::ShowPicture).
/// The packs carry no shadow: the Game lays the object's own shadow turned
/// with the ground.
fn relightFolder(ctx: *bk_tool.Context, dir: []const u8, out: []const u8) !usize {
    const arena = ctx.arena;
    const io = ctx.io;
    const cwd = std.Io.Dir.cwd();
    const packs = try readPacks(ctx, dir);
    const stats = if (cwd.readFileAlloc(io, try std.fs.path.join(arena, &.{ dir, "1.xml" }), arena, .limited(64 * 1024 * 1024))) |xml| try readStats(arena, xml) else |_| Stats{};
    // The Game mirrors a prop on a longer rectangle at an odd step (CMOObject::ApplyCameraTurn).
    const rows = stats.passability.len;
    const columns = if (rows > 0) stats.passability[0].len / 2 else 0;
    const rectangle = rows != columns and rows * columns != 0;
    const exe_dir = try std.process.executableDirPathAlloc(io, arena);
    const beside = try std.fs.path.join(arena, &.{ exe_dir, if (@import("builtin").os.tag == .windows) "bk-sprite.exe" else "bk-sprite" });
    const sprite_tool = if (cwd.access(io, beside, .{})) |_| beside else |_| "bk-sprite";
    const work = try std.fs.path.join(arena, &.{ out, ".relight" });
    try cwd.createDirPath(io, work);
    const anchor = [2]i32{ -packs.bounds.min_x, -packs.bounds.min_y };
    const locked = lockedCenter(stats.origin);
    var made: usize = 0;
    for (packs.files) |file| {
        const last = file.name[file.name.len - 1];
        if (last == 's' or last == 'g' or file.pack.sprites.len != 1) continue;
        const image = try unpack(arena, file.pack.sprites[0], file.atlas, packs.bounds);
        const normals = try spriteNormals(arena, image);
        const wall = objectLight(image, normals);
        // compose-pack packs a shadow, dropped after: the object's own, else one
        // pixel the picture leaves free (an empty one does not compose).
        const shadow: Image = .{ .width = image.width, .height = image.height, .pixels = try arena.alloc([4]u8, image.pixels.len) };
        @memset(shadow.pixels, .{ 0, 0, 0, 0 });
        for (packs.files) |s| {
            if (s.name.len == file.name.len + 1 and std.mem.startsWith(u8, s.name, file.name) and s.name[file.name.len] == 's' and s.pack.sprites.len == 1)
                @memcpy(shadow.pixels, (try unpack(arena, s.pack.sprites[0], s.atlas, packs.bounds)).pixels);
        }
        var seen = false;
        for (shadow.pixels, image.pixels) |s, p| seen = seen or (s[3] > 0 and p[3] < 128);
        if (!seen) for (shadow.pixels, image.pixels) |*s, p| {
            if (p[3] < 128) {
                s.* = .{ 0, 0, 0, 255 };
                break;
            }
        };
        const shadow_png = try std.fs.path.join(arena, &.{ work, "shadow.png" });
        try writeOut(ctx, work, "shadow.png", try writePng(arena, shadow));
        for ([_]u2{ 1, 2, 3 }) |k| {
            const name = try std.fmt.allocPrint(arena, "{s}_l{d}", .{ file.name, k });
            try writeOut(ctx, work, try std.fmt.allocPrint(arena, "{s}.png", .{name}), try writePng(arena, try relightSprite(arena, image, normals, wall, k, rectangle and k % 2 == 1)));
            var argv: std.ArrayList([]const u8) = .empty;
            try argv.appendSlice(arena, &.{
                sprite_tool,                                                                    "compose-pack",
                try std.fs.path.join(arena, &.{ work, try std.fmt.allocPrint(arena, "{s}.png", .{name}) }), shadow_png,
                try std.fs.path.join(arena, &.{ out, name }),                                     "--zero",
                try std.fmt.allocPrint(arena, "{d},{d}", .{ anchor[0], anchor[1] }),               "--force",
            });
            if (rows > 0) try argv.appendSlice(arena, &.{ "--pass", try std.mem.join(arena, "/", stats.passability), "--locked", try std.fmt.allocPrint(arena, "{d},{d}", .{ locked[0], locked[1] }) });
            const target = try std.fs.path.join(arena, &.{ out, name });
            const result = std.process.run(arena, io, .{ .argv = argv.items, .stdout_limit = .limited(1024 * 1024), .stderr_limit = .limited(1024 * 1024) }) catch |err|
                return ctx.failKey("views.err.compose", .{ target, @errorName(err) });
            if (result.term != .exited or result.term.exited != 0) return ctx.failKey("views.err.compose", .{ target, result.stderr });
            for ([_][]const u8{ ".san", "_c.dds", "_h.dds", "_l.dds" }) |suffix| {
                cwd.deleteFile(io, try std.fmt.allocPrint(arena, "{s}s{s}", .{ try std.fs.path.join(arena, &.{ out, name }), suffix })) catch {};
            }
            made += 1;
        }
    }
    cwd.deleteTree(io, work) catch {};
    return made;
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
/// `hand`: the blocks are a hand shape, whose author drew its towers and
/// spires; only the chimneys of the picture alone (`chimneyMask`) that it
/// lacks are cut, as many as there are. `drawn`: the roof parts drawn by
/// hand, which stand on the blocks as they do and are not returned: no
/// chimney is cut where one stands.
pub fn cutChimneys(gpa: std.mem.Allocator, body: []const Block, drawn: []const Block, pictures: []const Image, eyes: []const Eye, hand: bool) ![]const Block {
    const blocks = try std.mem.concat(gpa, Block, &.{ body, drawn });
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
    // The chimneys of the picture alone go first, each on its own: a roof
    // fitted up into one leaves only its top above the blocks, too narrow a
    // foot, and above a gable the fit cut low it joins the gable's top.
    const chimneys = try chimneyMask(gpa, picture);
    defer gpa.free(chimneys);
    for (above, chimneys) |*a, chimney| a.* = a.* and !chimney and !hand;
    // Each row's leftmost and rightmost pixel of the part being cut.
    const spans = try gpa.alloc([2]usize, h);
    defer gpa.free(spans);
    const rise = eye.project(.{ 0, 0, 0 })[1] - eye.project(.{ 0, 0, 1 })[1];
    var out: std.ArrayList(Block) = .empty;
    try out.appendSlice(gpa, blocks);
    var stack: std.ArrayList(usize) = .empty;
    defer stack.deinit(gpa);
    for ([2][]bool{ chimneys, above }) |region| for (0..region.len) |seed| {
        if (!region[seed] or (!hand and out.items.len - drawn.len + 2 > max_blocks)) continue;
        var box = [4]usize{ w, h, 0, 0 };
        var count: usize = 0;
        @memset(spans, .{ w, 0 });
        region[seed] = false;
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
                if (!region[n]) continue;
                region[n] = false;
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
        // A chimney standing on the roof plane before the ridge shows its
        // foot below the outline's top, where its brick meets the tiles: its
        // place along the line of sight (e_house02_1's stands on the front
        // plane; put on the ridge, its 2003 pixels lay on the roof plane in
        // every turn, M006 round 3).
        const own_row = if (region.ptr == chimneys.ptr) chimneyFoot(picture, spans, box, top) else top[xc];
        // Its own foot first, else where the outline's top is (a foot the
        // colours misread, on a wall or off the blocks).
        for ([2]usize{ own_row, top[xc] }, 0..) |foot_row, attempt| {
            if (attempt == 1 and own_row == top[xc]) break;
            const own_foot = foot_row > top[xc];
            const r = eye.ray(@as(f64, @floatFromInt(xc)) + 0.5, @as(f64, @floatFromInt(foot_row)) + 0.5);
            const hit = castPixel(blocks, eye, @as(f64, @floatFromInt(xc)) + 0.5, @as(f64, @floatFromInt(foot_row)) + 0.5) orelse continue;
            const t = hit.t - far_away;
            const foot = [3]f64{ r[0][0] + t * r[1][0], r[0][1] + t * r[1][1], r[0][2] + t * r[1][2] };
            // A row's height over the foot: the screen moves up as it grows.
            const zOf = struct {
                fn f(z0: f64, row0: usize, row: usize, per: f64) f64 {
                    return z0 + (@as(f64, @floatFromInt(row0)) - @as(f64, @floatFromInt(row))) / per;
                }
            }.f;
            const z_waist = zOf(foot[2], foot_row, waist, rise);
            const z_top = zOf(foot[2], foot_row, box[1], rise);
            const pointed = waist - box[1] >= 4;
            const spire_width: f64 = if (pointed) @floatFromInt(@max(3, spans[waist - 1][1] -| spans[waist - 1][0])) else 0;
            const along = [2]f64{ r[1][0], r[1][1] };
            const length = @sqrt(along[0] * along[0] + along[1] * along[1]);
            var best: [2]?Block = .{ null, null };
            var best_score = -std.math.inf(f64);
            var step: f64 = -48;
            while (step <= 48) : (step += 2) {
                if (own_foot and step != 0) continue;
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
                // post from the other sides. A chimney's middle: one on a gable
                // end stands on the end wall (e_house06_4).
                const half = @as(f64, @floatFromInt(foot_width)) / std.math.sqrt2 / @as(f64, if (region.ptr == chimneys.ptr) 4 else 2);
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
            if (best[0] != null or best[1] != null) {
                for (best) |part| try out.append(gpa, part orelse continue);
                break;
            }
        }
    };
    return std.mem.concat(gpa, Block, &.{ body, out.items[blocks.len..] });
}

/// The row of a chimney's foot in its picture: per column of the chimney
/// above the outline's top (`spans` of the rows of `box`), down from that top
/// as long as a pixel is nearer the chimney's mean colour than the roof's
/// beside it, up to 40 rows, the median of those rows. Where the roof hides
/// the foot the first roof pixel is the top itself.
fn chimneyFoot(picture: Image, spans: []const [2]usize, box: [4]usize, top: []const usize) usize {
    const w = picture.width;
    const colourOf = struct {
        fn f(p: [4]u8) [3]f64 {
            return .{ @floatFromInt(p[0]), @floatFromInt(p[1]), @floatFromInt(p[2]) };
        }
    }.f;
    const far = struct {
        fn f(a: [3]f64, b: [3]f64) f64 {
            return (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]);
        }
    }.f;
    var mean = [3]f64{ 0, 0, 0 };
    var n: f64 = 0;
    for (box[1]..box[3]) |y| for (spans[y][0]..spans[y][1]) |x| {
        const p = picture.pixels[y * w + x];
        if (p[3] < 128 or y >= top[x]) continue;
        const c = colourOf(p);
        for (0..3) |i| mean[i] += c[i];
        n += 1;
    };
    // The middle column's top is the reference: its foot is sought.
    const xc = (box[0] + box[2]) / 2;
    if (n == 0) return top[xc];
    for (&mean) |*m| m.* /= n;
    // The roof: the opaque pixels a foot's width either side, below the top.
    const width = box[2] - box[0];
    var roof = [3]f64{ 0, 0, 0 };
    var m: f64 = 0;
    for ([2]i64{ -1, 1 }) |side| {
        for (0..width) |d| {
            const x = @as(i64, @intCast(xc)) + side * @as(i64, @intCast(width + d));
            if (x < 0 or x >= w) continue;
            const ux: usize = @intCast(x);
            for (top[ux]..@min(picture.height, top[ux] + 2 * width)) |y| {
                const p = picture.pixels[y * w + ux];
                if (p[3] < 128) continue;
                const c = colourOf(p);
                for (0..3) |i| roof[i] += c[i];
                m += 1;
            }
        }
    }
    if (m == 0) return top[xc];
    for (&roof) |*r| r.* /= m;
    var rows: [96]usize = undefined;
    var count: usize = 0;
    for (box[0]..box[2]) |x| {
        if (count == rows.len or top[x] >= picture.height) continue;
        var y = top[x];
        while (y < @min(picture.height, top[x] + 40) and picture.pixels[y * w + x][3] >= 128 and far(colourOf(picture.pixels[y * w + x]), mean) < far(colourOf(picture.pixels[y * w + x]), roof)) y += 1;
        rows[count] = y - top[x];
        count += 1;
    }
    if (count == 0) return top[xc];
    std.mem.sort(usize, rows[0..count], {}, std.sort.asc(usize));
    return top[xc] + rows[count / 2];
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
/// class under the turn's `light`, as `projectPicture` classes it: compared
/// with the eye's own turn, every turn lit by the world's sun at another step
/// kept none). A turn that paints a vent on the roof plane keeps none.
pub fn roofKept(blocks: []const Block, parts: []const Block, eye: Eye, light: u2, drawn: Generated) [2]f64 {
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
        kept += @floatFromInt(@intFromBool(drawn.classes[y * w + x] == shadeClass(part.normal, light)));
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

/// How a turn's intact picture stands to the variant's shape turned (M006
/// round 3: a turn is a rotation, never a mirror). A mirror flips handedness:
/// an L-shaped house's wing, a lean-to, a ramp or a chimney ends up on the
/// other side, and it equals the rotation only where the shape and its parts
/// are symmetric for that turn. `turned`: the outline IoU of the picture with
/// the turned shape's projection, at the best shift within `mirror_reach`
/// pixels (a 2003 sprite is laid by its footprint, and a shape and its
/// footprint's middle differ by a few units); `mirrored`: with that
/// projection mirrored across the screen's vertical; `symmetric`: the turned
/// projection with its own mirror; `parts`: the pixels the parts (chimneys,
/// low and roof parts) add to the outline beyond the body and its mirror,
/// `parts_kept` those the picture holds, `parts_mirrored` and
/// `parts_mirrored_kept` likewise for the parts mirrored; `own`: the IoU of
/// the variant's 2003 picture with the shape unturned, what the shape can
/// reach at all.
pub const Handed = struct { turned: f64 = 1, mirrored: f64 = 0, symmetric: f64 = 1, own: f64 = 1, parts: f64 = 0, parts_kept: f64 = 0, parts_mirrored: f64 = 0, parts_mirrored_kept: f64 = 0 };

/// The turned shape `blocks` (its first `body` the body, the rest parts) seen
/// by `eye`, against `picture` on that eye's canvas; the mirror's axis is the
/// screen column `axis` (the turned footprint's middle).
pub fn handedness(gpa: std.mem.Allocator, blocks: []const Block, body: usize, eye: Eye, axis: f64, picture: Image) !Handed {
    const w: i64 = picture.width;
    const h: i64 = picture.height;
    const reach: i64 = mirror_reach;
    // Twice the axis, whole: pixel x mirrors to fold - 1 - x.
    const fold: i64 = @intFromFloat(@round(2 * axis));
    // The turned shape on a canvas wide enough for its mirror and every shift.
    const pad = 2 * reach + 4;
    const lo = [2]i64{ @min(-pad, fold - w - pad), -pad };
    const hi = [2]i64{ @max(w, fold) + pad + 1, h + pad };
    const span = [2]usize{ @intCast(hi[0] - lo[0]), @intCast(hi[1] - lo[1]) };
    // Per pixel: 0 nothing, 1 body, 2 a part where no body stands (what the
    // part adds to the outline).
    const turned = try gpa.alloc(u8, span[0] * span[1]);
    defer gpa.free(turned);
    for (0..span[1]) |j| for (0..span[0]) |i| {
        const px = @as(f64, @floatFromInt(@as(i64, @intCast(i)) + lo[0])) + 0.5;
        const py = @as(f64, @floatFromInt(@as(i64, @intCast(j)) + lo[1])) + 0.5;
        turned[j * span[0] + i] = if (castPixel(blocks[0..body], eye, px, py) != null) 1 else if (castPixel(blocks, eye, px, py) != null) 2 else 0;
    };
    const mirrored = try gpa.alloc(u8, span[0] * span[1]);
    defer gpa.free(mirrored);
    const Canvas = struct {
        m: []const u8,
        lo: [2]i64,
        span: [2]usize,
        fn get(c: @This(), x: i64, y: i64) u8 {
            const i = x - c.lo[0];
            const j = y - c.lo[1];
            if (i < 0 or j < 0 or i >= c.span[0] or j >= c.span[1]) return 0;
            return c.m[@as(usize, @intCast(j)) * c.span[0] + @as(usize, @intCast(i))];
        }
    };
    const t: Canvas = .{ .m = turned, .lo = lo, .span = span };
    for (0..span[1]) |j| for (0..span[0]) |i| {
        const x = @as(i64, @intCast(i)) + lo[0];
        mirrored[j * span[0] + i] = t.get(fold - 1 - x, @as(i64, @intCast(j)) + lo[1]);
    };
    const opaque_at = struct {
        fn f(p: Image, x: i64, y: i64) bool {
            return if (p.at(x, y)) |px| px[3] >= 128 else false;
        }
    }.f;
    const m: Canvas = .{ .m = mirrored, .lo = lo, .span = span };
    // The best overlap of a over b moved by (dx, dy) within the reach; b
    // null is the picture.
    const Best = struct { iou: f64 = 0, shift: [2]i64 = .{ 0, 0 } };
    const best = struct {
        fn f(a: Canvas, b: ?Canvas, p: Image, r: i64, area: [2][2]i64) Best {
            // Every fourth shift, then every shift about the best.
            const coarse = search(a, b, p, .{ 0, 0 }, @divTrunc(r, 4), 4, area);
            return search(a, b, p, coarse.shift, 3, 1, area);
        }
        fn search(a: Canvas, b: ?Canvas, p: Image, at: [2]i64, r: i64, step: i64, area: [2][2]i64) Best {
            var out: Best = .{};
            var j: i64 = -r;
            while (j <= r) : (j += 1) {
                var i: i64 = -r;
                while (i <= r) : (i += 1) {
                    const dx = at[0] + i * step;
                    const dy = at[1] + j * step;
                    var both: f64 = 0;
                    var either: f64 = 0;
                    var y = area[0][1];
                    while (y < area[1][1]) : (y += 1) {
                        var x = area[0][0];
                        while (x < area[1][0]) : (x += 1) {
                            const in_a = a.get(x - dx, y - dy) != 0;
                            const in_b = if (b) |c| c.get(x, y) != 0 else opaque_at(p, x, y);
                            both += @floatFromInt(@intFromBool(in_a and in_b));
                            either += @floatFromInt(@intFromBool(in_a or in_b));
                        }
                    }
                    const iou = if (either == 0) 0 else both / either;
                    if (iou > out.iou) out = .{ .iou = iou, .shift = .{ dx, dy } };
                }
            }
            return out;
        }
    }.f;
    const whole = [2][2]i64{ lo, hi };
    const on_picture = [2][2]i64{ .{ -reach, -reach }, .{ w + reach, h + reach } };
    const fit = best(t, null, picture, reach, on_picture);
    const sym = best(m, t, picture, 2 * reach, whole);
    const flipped = best(m, null, picture, 2 * reach, on_picture);
    var out: Handed = .{ .turned = fit.iou, .mirrored = flipped.iou, .symmetric = sym.iou };
    // The parts as the turned shape lies on the picture, and their mirror
    // laid on that shape as well as it lies.
    var y: i64 = -reach;
    while (y < h + reach) : (y += 1) {
        var x: i64 = -reach;
        while (x < w + reach) : (x += 1) {
            const a = t.get(x - fit.shift[0], y - fit.shift[1]);
            const b = m.get(x - fit.shift[0] - sym.shift[0], y - fit.shift[1] - sym.shift[1]);
            const p = opaque_at(picture, x, y);
            // A part's pixel counts where neither the body nor its mirror
            // stands: there the two hands differ by the part alone.
            if (a == 2 and b != 1) {
                out.parts += 1;
                out.parts_kept += @floatFromInt(@intFromBool(p));
            }
            if (b == 2 and a != 1) {
                out.parts_mirrored += 1;
                out.parts_mirrored_kept += @floatFromInt(@intFromBool(p));
            }
        }
    }
    return out;
}

/// How far (pixels) the turned shape is moved to lie on a turn's picture
/// best, which is laid by its footprint (a shape and its footprint's middle
/// differ by a few units); its mirror, laid on the turned shape, twice as
/// far.
const mirror_reach = 16;

/// Where a turn's picture is not the variant's shape turned, or null: its
/// outline leaves the turned shape by more than the 2003 picture leaves the
/// shape unturned (`outline_slack`), its outline is the mirror's rather than
/// the turned shape's where the two differ (`symmetric_floor`), or it drops
/// the parts the turned shape shows (more than `low_margin` pixels, less
/// than `low_share` held) while it holds their mirror.
pub fn handedFailure(gpa: std.mem.Allocator, h: Handed) !?[]const u8 {
    if (h.turned < h.own - outline_slack) return try std.fmt.allocPrint(gpa, "turned outline {d:.3} < {d:.3}", .{ h.turned, h.own - outline_slack });
    if (h.symmetric < symmetric_floor and h.mirrored > h.turned + mirror_margin) return try std.fmt.allocPrint(gpa, "mirrored: outline {d:.3} turned, {d:.3} mirrored", .{ h.turned, h.mirrored });
    if (lowFails(h.parts, h.parts_kept) and h.parts_mirrored_kept >= low_share * h.parts_mirrored) return try std.fmt.allocPrint(gpa, "parts on the mirrored side: {d:.0} of {d:.0} pixels", .{ h.parts_kept, h.parts });
    return null;
}

/// A turn's outline may stand this far (IoU) under the 2003 picture's with
/// the shape unturned.
const outline_slack = 0.05;
/// A turned shape whose mirror covers it this well (IoU) is symmetric for
/// that turn: a mirror of its picture shows it as well as the turn.
const symmetric_floor = 0.95;
/// A mirror's outline must beat the turn's by this much (IoU) to count: a
/// round chimney stalk measures 0.715 turned and 0.716 mirrored on blocks
/// that differ from round by a few units.
const mirror_margin = 0.02;

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
    // Only buildings form families: Data/Objects' numbered folders are other
    // things (debris/05 a sand pile, /09 a stack of logs).
    const buildings = if (up(up(season))) |root| std.ascii.eqlIgnoreCase(std.fs.path.basename(root), "buildings") else false;
    if (up(up(season))) |root| if (named and buildings) if (listFamilies(gpa, io, root)) |families| {
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
    // A numbered folder (an object's `debris/05`) is named by its parent, as
    // a building's numbered subfolder is: its hand shape is `debris.json`'s
    // "05".
    const family = if (numbered) std.fs.path.basename(std.fs.path.dirname(dir) orelse dir) else name;
    return .{ .root = std.fs.path.dirname(dir) orelse ".", .family = .{ .name = family, .region = "", .variants = variants }, .index = 0 };
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

/// The chimneys a picture shows above its roofs: what an opening of the top
/// edge `chimney_window` pixels wide takes off it, in runs of columns that
/// rise `chimney_rise` to `chimney_tallest` rows and have a vertical side, a step of
/// `chimney_step` rows or more between two columns. A roof's apex the
/// opening also cuts rises only along its slopes (2 to 6 rows a column on
/// the 2003 houses, 6 at e_house02_4's steep gable); a chimney's sides rise
/// 7 to 28 at once. Found from the
/// picture alone, so the blocks cannot hide them: e_house02's fit raised its
/// roof into the chimney, and the rest of the chimney was painted on the
/// roof in every turn (M006 round 2).
pub fn chimneyMask(gpa: std.mem.Allocator, picture: Image) ![]bool {
    const w = picture.width;
    const h = picture.height;
    const mask = try gpa.alloc(bool, @as(usize, w) * h);
    @memset(mask, false);
    if (w < chimney_window) return mask;
    // Per column the first opaque row, and that of the opened outline: the
    // lowest top of the best window of columns round it.
    const top = try gpa.alloc(usize, w);
    defer gpa.free(top);
    for (top, 0..) |*t, x| {
        t.* = h;
        for (0..h) |y| if (picture.pixels[y * w + x][3] >= 128) {
            t.* = y;
            break;
        };
    }
    const opened = try gpa.alloc(usize, w);
    defer gpa.free(opened);
    @memset(opened, h);
    for (0..w - chimney_window + 1) |s| {
        const lowest = std.mem.max(usize, top[s .. s + chimney_window]);
        for (opened[s .. s + chimney_window]) |*o| o.* = @min(o.*, lowest);
    }
    // Every window round a column at the outline's end holds an empty one:
    // nothing to open there.
    for (opened, top) |*o, t| if (o.* == h) {
        o.* = t;
    };
    var x: usize = 0;
    while (x < w) {
        if (opened[x] <= top[x]) {
            x += 1;
            continue;
        }
        const a = x;
        var rise: usize = 0;
        while (x < w and opened[x] > top[x]) : (x += 1) rise = @max(rise, opened[x] - top[x]);
        var step: usize = 0;
        for (a -| 1..@min(x + 1, w - 1)) |c| step = @max(step, @max(top[c], top[c + 1]) - @min(top[c], top[c + 1]));
        if (rise < chimney_rise or rise > chimney_tallest or step < chimney_step) continue;
        for (a..x) |c| for (top[c]..opened[c]) |y| {
            mask[y * w + c] = true;
        };
    }
    return mask;
}

const chimney_window = 20;
const chimney_rise = 5;
const chimney_step = 7;
/// A part rising further above the roofs is a spire or a tower (e_chapel_1,
/// church, e_cityhall_1: 32 to 50 rows); the 2003 chimneys rise 5 to 23.
const chimney_tallest = 30;
const chimney_share = 0.5;

/// The pictures with their chimneys (`chimneyMask`) cleared, for the fit.
fn withoutChimneys(gpa: std.mem.Allocator, pictures: []const Image) ![]Image {
    const out = try gpa.alloc(Image, pictures.len);
    for (out, pictures) |*o, p| {
        o.* = .{ .width = p.width, .height = p.height, .pixels = try gpa.dupe([4]u8, p.pixels) };
        for (o.pixels, try chimneyMask(gpa, p)) |*pixel, chimney| {
            if (chimney) pixel.* = .{ 0, 0, 0, 0 };
        }
    }
    return out;
}

/// The chimneys the variant's own 2003 picture shows (`chimneyMask`) and
/// how many of their pixels the blocks cover there: a chimney the fit leaves
/// out above a gable (e_house06_4) is lost in every turn. Its own picture
/// alone, as for the outline: another variant of the family may be another
/// building (ukrainehouse01_1 and _2).
/// A turn fails where they show more than `low_margin` pixels and the blocks
/// cover less than `chimney_share` of them: a lost chimney leaves none to a
/// sixth covered, one drawn a little narrower than its cap 0.7 or more
/// (e_ctownhouse04_1, e_house11_1).
pub fn keptChimneys(gpa: std.mem.Allocator, blocks: []const Block, picture: Image, eye: Eye) ![2]f64 {
    var seen: f64 = 0;
    var kept: f64 = 0;
    const mask = try chimneyMask(gpa, picture);
    defer gpa.free(mask);
    for (mask, 0..) |chimney, i| {
        if (!chimney) continue;
        seen += 1;
        kept += @floatFromInt(@intFromBool(castPixel(blocks, eye, @as(f64, @floatFromInt(i % picture.width)) + 0.5, @as(f64, @floatFromInt(i / picture.width)) + 0.5) != null));
    }
    return .{ seen, kept };
}

/// The pixels of a picture `projectPicture` drew on a roof of the body
/// (`blocks[..body]`) from a point a 2003 picture of the colourway sees as
/// a chimney (`chimneyMask`): a chimney missing from the blocks, or standing
/// elsewhere than one of the 2003 sides shows it, is painted on the roof
/// plane under where that side saw it.
pub fn paintedChimneys(gpa: std.mem.Allocator, blocks: []const Block, body: usize, eye: Eye, drawn: Generated, pictures: []const Image, eyes: []const Eye) !f64 {
    if (drawn.sources.len == 0) return 0;
    const masks = try gpa.alloc([]bool, pictures.len);
    for (masks, pictures) |*m, p| m.* = try chimneyMask(gpa, p);
    var n: f64 = 0;
    const w = drawn.image.width;
    for (drawn.kinds, drawn.sources, 0..) |kind, q, i| {
        if (kind == .empty or kind == .filled) continue;
        const hit = castPixel(blocks, eye, @as(f64, @floatFromInt(i % w)) + 0.5, @as(f64, @floatFromInt(i / w)) + 0.5) orelse continue;
        if (hit.block >= body or hit.normal[2] <= 0.5) continue;
        for (masks, pictures, eyes) |m, p, e| {
            if (!sees(blocks, e, q)) continue;
            const uv = e.project(q);
            if (uv[0] < 0 or uv[1] < 0 or uv[0] >= @as(f64, @floatFromInt(p.width)) or uv[1] >= @as(f64, @floatFromInt(p.height))) continue;
            if (!m[@as(usize, @intFromFloat(uv[1])) * p.width + @as(usize, @intFromFloat(uv[0]))]) continue;
            n += 1;
            break;
        }
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
/// way are left for `patchFill`. `light`: the turn the light is lit by
/// (`lightFor`): the target's own turn for the 2003 light, fixed to the
/// camera; the building's facing for the sun fixed in the world, whose
/// picture of turn k at camera step s is lit by turn k - s (M006 round 2).
pub fn projectPicture(gpa: std.mem.Allocator, blocks: []const Block, body: usize, means: ?Means, wall: [3]f64, target: Eye, light: u2, size: [2]u32, projectors: []const Projector) !Generated {
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
        const class = shadeClass(hit.normal, light);
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
                } else out.image.pixels[i] = relight(pixel, shading(hit.normal, light, wall), shading(nq, projector.eye.turn, wall));
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

/// A turn to draw. `light`: the turn the light is lit by (`projectPicture`),
/// `turn` itself for the 2003 light fixed to the camera.
const Turned = struct { turn: u2, light: u2, eye: Eye, size: [2]u32, name: []const u8 };

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
    /// How many of each state's blocks are the building's; the chimneys cut
    /// from its outline, the low parts and the roof parts follow them.
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
    /// The object's tracked mesh (a joint family's shared one with the views
    /// of the colourway), where it has one.
    mesh: ?MeshScene = null,
    /// The full 3D models on that mesh every state is rendered from
    /// (`buildFull3d`): the intact model, then one a damage state.
    full3d: []const model3d.Model = &.{},

    /// The full 3D model that holds a state's atlas.
    fn modelFor(scene: Scene, state: []const u8) ?model3d.Model {
        for (scene.full3d) |m| if (m.texture(state) != null) return m;
        return null;
    }
};

/// Hand shapes, from the working folder (the build root): `<family>.json`
/// maps a variant folder's name to its intact blocks in its own frame (world
/// units from its zero cross, as sources.json lists them), for buildings the
/// fit cannot draw (wings, domes, open props). `side` and `holdout` take them
/// instead of the fitted intact blocks; the damage states are fitted from them.
pub const shapes_dir = "tools/zig/fixtures/bk-views/shapes";

/// The props of Data/Objects with a hand shape (M006 round 3, before every
/// prop became a full 3D model): the training camp's sand pile.
pub const shaped_objects = [_][]const u8{"SimpleObjects/common/summer/debris/05"};

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
/// `intact_only`: the intact state's blocks alone and no full 3D models,
/// for the block shapes' fit and review (`shape`, `models`).
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
        const intact_state = std.mem.eql(u8, state, "1") or std.mem.eql(u8, state, "1w");
        if (hand != null and (intact_state or blocksIou(hand.?, state_eyes.items[0], state_pictures.items[0]) >= outline_floor)) {
            shape.* = hand.?;
            body_count.* = shape.len;
            // The chimneys its author left out, but for those the roof parts
            // draw.
            if (intact_state) shape.* = try cutChimneys(arena, hand.?, roof.items, state_pictures.items, state_eyes.items, true);
        } else {
            // Fitted to the outlines without their chimneys, the roof stays
            // under them, and the chimneys cut from the whole outlines stand
            // on it as parts, but for those the roof parts draw.
            const fitted = try fitBlocks(arena, intact, try withoutChimneys(arena, state_pictures.items), state_eyes.items, state[0] == '3');
            shape.* = try cutChimneys(arena, fitted, if (intact_state) roof.items else &.{}, state_pictures.items, state_eyes.items, false);
            body_count.* = fitted.len;
            if (std.mem.eql(u8, state, "1")) {
                intact = fitted;
                body = shape.*;
            }
        }
        // The low parts the pictures of the colourway show: a ramp one side
        // shows stays on every turn.
        var parts: std.ArrayList(Block) = .empty;
        try parts.appendSlice(arena, shape.*);
        for (sources.items, eyes, same) |source, eye, s| {
            if (s) try parts.appendSlice(arena, try lowParts(arena, shape.*, pictureOf(source, state) orelse continue, eye));
        }
        // The intact building's roof parts.
        if (intact_state) try parts.appendSlice(arena, roof.items);
        shape.* = parts.items;
    }
    const mesh_scene = try loadMeshScene(ctx, sources.items, same, fixed);
    const full3d = if (mesh_scene != null and !intact_only) try buildFull3d(arena, sources.items, mesh_scene.?, states.items) else &.{};
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
        .mesh = mesh_scene,
        .full3d = full3d,
    };
}

/// Each turn's canvas: it holds every state's blocks, their shadows under
/// the light of turn `light` and the 2003 ground pictures.
fn turnTarget(scene: Scene, k: u2, light: u2, name: []const u8) Turned {
    var eye: Eye = .{ .anchor = .{ 0, 0 }, .turn = k };
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    const l = lightFor(light_2003, light);
    for (scene.shapes) |shape| for (shape) |b| for (0..8) |c| {
        const p = [3]f64{ if (c & 1 != 0) b.hi[0] else b.lo[0], if (c & 2 != 0) b.hi[1] else b.lo[1], if (c & 4 != 0) @max(b.wall, b.ridge) else 0 };
        for ([2][3]f64{ p, .{ p[0] - l[0] * p[2] / l[2], p[1] - l[1] * p[2] / l[2], 0 } }) |q| {
            const s = eye.project(q);
            lo = .{ @min(lo[0], s[0]), @min(lo[1], s[1]) };
            hi = .{ @max(hi[0], s[0]), @max(hi[1], s[1]) };
        }
    };
    if (scene.mesh) |ms| for (0..mesh.vertexCount(ms.model)) |i| {
        const p = ms.views[0].placement.world(mesh.vertex(ms.model, i));
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
    // A meshed object's turn is laid on the zero cross `meshZero` finds,
    // which the canvas must hold (a small prop's mesh may stand off its
    // footprint's middle by more than the margin: compose-pack hung on
    // mine_r1's zero at 31,13 of a 27 x 18 canvas, and read a negative one
    // as an option).
    if (scene.mesh) |ms| {
        const z = meshZero(scene, ms, .{ .turn = k, .light = light, .eye = eye, .size = .{ 0, 0 }, .name = name });
        lo = .{ @min(lo[0], z[0]), @min(lo[1], z[1]) };
        hi = .{ @max(hi[0], z[0]), @max(hi[1], z[1]) };
    }
    const margin: f64 = 2;
    eye.anchor = .{ margin - @floor(lo[0]), margin - @floor(lo[1]) };
    return .{
        .turn = k,
        .light = light,
        .eye = eye,
        .size = .{ @intFromFloat(@ceil(hi[0]) - @floor(lo[0]) + 2 * margin), @intFromFloat(@ceil(hi[1]) - @floor(lo[1]) + 2 * margin) },
        .name = name,
    };
}

/// The shadow the blocks cast on the ground under the light of turn `light`
/// (`projectPicture`), as `eye` sees it on a canvas of `size`.
fn castShadow(gpa: std.mem.Allocator, blocks: []const Block, eye: Eye, size: [2]u32, light: u2) !Image {
    const shadow: Image = .{ .width = size[0], .height = size[1], .pixels = try gpa.alloc([4]u8, @as(usize, size[0]) * size[1]) };
    const l = lightFor(light_2003, light);
    for (0..shadow.height) |y| for (0..shadow.width) |x| {
        const g = groundPoint(eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
        shadow.pixels[y * shadow.width + x] = if (castRay(blocks, g, l) != null) .{ 0, 0, 0, 255 } else .{ 0, 0, 0, 0 };
    };
    return shadow;
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
        if (scene.modelFor(state)) |m| {
            d.* = try full3dDrawn(arena, m, state, blocks, target);
            d.ground = try groundOf(arena, scene, state, target);
            continue;
        }
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
        const sprite = try projectPicture(arena, blocks, if (state[0] == '1') body else blocks.len, if (state[0] == '1') null else means, wallLight(means), target.eye, target.light, target.size, projectors);
        try patchFill(arena, sprite);
        const shadow = try castShadow(arena, blocks, target.eye, target.size, target.light);
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
const Shape = struct { height: f64 = 1, ground: f64 = 1, cover: f64 = 1, protrusion: f64 = 0, protrusion_2003: f64 = 0, hand: bool = false, low: f64 = 0, low_kept: f64 = 0, roof: f64 = 0, roof_kept: f64 = 0, stretch: f64 = 0, copied: f64 = 0, chimney: f64 = 0, chimney_kept: f64 = 0, painted: f64 = 0 };

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
    if (s.chimney > low_margin and s.chimney_kept < chimney_share * s.chimney) return try std.fmt.allocPrint(gpa, "chimneys {d:.0} of {d:.0} pixels", .{ s.chimney_kept, s.chimney });
    if (s.painted > low_margin) return try std.fmt.allocPrint(gpa, "chimney painted {d:.0} pixels", .{s.painted});
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
/// Under the sun fixed in the world (`light` other than k) each pixel is
/// relit instead (`sunLit`), but for a heap's.
fn sameSprite(ctx: *bk_tool.Context, scene: Scene, k: u2, light: u2, name: []const u8, mirror: bool, by: []const Drawn) !struct { target: Turned, drawn: []Drawn } {
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
        .light = light,
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
        if (light != k and !scene.prop) {
            picture = try sunLit(arena, picture, scene.eyes[scene.fixed], blocks_drawn, mirror, k, light);
        } else if (mirror and !scene.prop) {
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
        // Under the sun fixed in the world the blocks cast the shadow: the
        // 2003 one falls from the camera's light.
        const own_shadow = pictureOf(own, try std.fmt.allocPrint(arena, "{s}s", .{state}));
        const shadow = if (light != k and own_shadow != null) try castShadow(arena, blocks_drawn.blocks, target.eye, target.size, light) else try place(arena, own_shadow, target.size, lo, null);
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
/// projection, rounded to a whole pixel. Under the sun fixed in the world
/// (`light` other than k) each pixel is relit (`sunLit`, `by`: the blocks'
/// drawing of the turn), so the sibling's own detail is kept.
fn siblingSprite(ctx: *bk_tool.Context, scene: Scene, k: u2, light: u2, name: []const u8, e: usize, by: []const Drawn) !struct { target: Turned, drawn: []Drawn } {
    const arena = ctx.arena;
    const sibling = scene.sources[e];
    const shift = mfc_camera.project(.{ scene.eyes[e].shift[0], scene.eyes[e].shift[1], 0 });
    const first = pictureOf(sibling, "1").?;
    const target: Turned = .{
        .turn = k,
        .light = light,
        .eye = .{ .anchor = .{ @as(f64, @floatFromInt(sibling.anchor[0])) + @round(shift[0]), @as(f64, @floatFromInt(sibling.anchor[1])) + @round(shift[1]) }, .turn = k },
        .size = .{ first.width, first.height },
        .name = name,
    };
    const drawn = try arena.alloc(Drawn, scene.states.len);
    for (scene.states, drawn, 0..) |state, *d, i| {
        const picture = pictureOf(sibling, state).?;
        const sprite = try asGenerated(arena, if (light != k) try sunLit(arena, picture, scene.eyes[e], by[i], false, k, light) else picture);
        // A copy: keepShadow may mark a pixel, and the sibling's pictures are shared.
        const shadow: Image = .{ .width = first.width, .height = first.height, .pixels = try arena.alloc([4]u8, @as(usize, first.width) * first.height) };
        if (pictureOf(sibling, try std.fmt.allocPrint(arena, "{s}s", .{state}))) |s| {
            if (light != k) @memcpy(shadow.pixels, (try castShadow(arena, by[i].blocks, target.eye, target.size, light)).pixels) else @memcpy(shadow.pixels, s.pixels);
        } else @memset(shadow.pixels, .{ 0, 0, 0, 0 });
        keepShadow(sprite.image, shadow);
        const ground = pictureOf(sibling, try std.fmt.allocPrint(arena, "{s}g", .{state}));
        d.* = .{ .state = state, .blocks = &.{}, .sprite = sprite, .shadow = shadow, .ground = if (ground) |g| try asGenerated(arena, g) else null, .covers = &.{} };
    }
    return .{ .target = target, .drawn = drawn };
}

/// A 2003 picture seen by `eye` and shown as turn k (mirrored across the
/// screen's vertical where `mirror`), relit pixel by pixel from the 2003
/// light it was drawn under to the light of turn `light` (`projectPicture`):
/// the face a pixel shows (the blocks of `by`, its state's drawing, seen by
/// `eye`) keeps its place on the screen, so its normal in the eye's frame is
/// kept (mirrored with the picture) and taken back to the variant's frame by
/// the shown turn. An intact state is relit by the light (`shading`), a ruin
/// by its class gains, as `projectPicture` does. A pixel on no block keeps
/// its colour.
fn sunLit(gpa: std.mem.Allocator, picture: Image, eye: Eye, by: Drawn, mirror: bool, k: u2, light: u2) !Image {
    const out: Image = .{ .width = picture.width, .height = picture.height, .pixels = try gpa.dupe([4]u8, picture.pixels) };
    const wall = wallLight(by.means);
    const intact = by.state[0] == '1';
    for (out.pixels, 0..) |*pixel, i| {
        if (pixel[3] == 0) continue;
        const hit = castPixel(by.blocks, eye, @as(f64, @floatFromInt(i % picture.width)) + 0.5, @as(f64, @floatFromInt(i / picture.width)) + 0.5) orelse continue;
        const seen = turnPoint(.{ hit.normal[0], hit.normal[1] }, eye.turn);
        const back = turnPoint(if (mirror) .{ -seen[1], -seen[0] } else seen, 0 -% k);
        const n = [3]f64{ back[0], back[1], hit.normal[2] };
        if (intact) {
            pixel.* = relight(pixel.*, shading(n, light, wall), shading(hit.normal, eye.turn, wall));
        } else for (0..3) |ch| {
            pixel[ch] = @intFromFloat(@min(255, @round(@as(f64, @floatFromInt(pixel[ch])) * by.means.gain(shadeClass(n, light), shadeClass(hit.normal, eye.turn), ch))));
        }
    }
    return out;
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
const DrawnTurn = struct { target: Turned, drawn: []Drawn, shape: Shape, failure: ?[]const u8, fallback: bool, mirror: bool, sibling: ?[]const u8 = null, handed: Handed = .{}, handed_failure: ?[]const u8 = null, refused: ?[]const u8 = null };

/// The handedness of a turn's intact picture (`handedness`) against the
/// scene's intact shape turned as `target` sees it.
fn turnHanded(gpa: std.mem.Allocator, scene: Scene, target: Turned, drawn: []const Drawn) !Handed {
    const i = for (scene.states, 0..) |state, i| {
        if (std.mem.eql(u8, state, "1")) break i;
    } else return .{};
    const own = scene.sources[scene.fixed];
    const c = boxCentre(own.footprint);
    var h = try handedness(gpa, scene.shapes[i], scene.bodies[i], target.eye, target.eye.project(.{ c[0], c[1], 0 })[0], drawn[i].sprite.image);
    h.own = blocksIou(scene.shapes[i], scene.eyes[scene.fixed], pictureOf(own, "1").?);
    return h;
}

/// Judges a drawn turn by `handedness` (`turnHanded`, `handedFailure`):
/// whether its picture is the scene's shape turned.
fn judgeTurn(gpa: std.mem.Allocator, scene: Scene, turned: *DrawnTurn) !bool {
    turned.handed = try turnHanded(gpa, scene, turned.target, turned.drawn);
    turned.handed_failure = try handedFailure(gpa, turned.handed);
    return turned.handed_failure == null;
}

/// A turn as the 2003 art where it shows the turn (a sibling of the
/// colourway, or a thin, small or heaped object's own sprite, as it was or
/// mirrored), but only where that art is the shape turned (M006 round 3: a
/// turn is a rotation, never a mirror); else the shape turned, drawn by the
/// blocks. `refused` names the 2003 art passed over and why.
fn drawTurn(ctx: *bk_tool.Context, scene: Scene, blocks_target: Turned) !DrawnTurn {
    const arena = ctx.arena;
    const own = scene.sources[scene.fixed];
    const new_side = (scene.own_side - 1 + @as(u8, blocks_target.turn)) % 4 + 1;
    // A full 3D model draws every turn, a sibling's included: the sibling's
    // 2003 art is painted into its atlas (`buildFull3d`). The model is the
    // object's tracked mesh, so the blocks' gates do not judge it.
    if (scene.full3d.len > 0) return .{ .target = blocks_target, .drawn = try drawTarget(ctx, scene, blocks_target), .shape = .{}, .failure = null, .fallback = false, .mirror = false };
    var refused: ?[]const u8 = null;
    var by: ?[]Drawn = null;
    if (siblingFor(scene, blocks_target.turn)) |e| {
        if (blocks_target.light != blocks_target.turn) by = try drawTarget(ctx, scene, blocks_target);
        const same = try siblingSprite(ctx, scene, blocks_target.turn, blocks_target.light, blocks_target.name, e, by orelse &.{});
        var turned: DrawnTurn = .{ .target = same.target, .drawn = same.drawn, .shape = .{}, .failure = null, .fallback = true, .mirror = false, .sibling = std.fs.path.basename(scene.sources[e].dir) };
        if (try judgeTurn(arena, scene, &turned)) return turned;
        refused = try std.fmt.allocPrint(arena, "sibling {s}: {s}", .{ turned.sibling.?, turned.handed_failure.? });
    }
    const drawn = by orelse try drawTarget(ctx, scene, blocks_target);
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
            const roof = roofKept(scene.body, scene.roof, blocks_target.eye, blocks_target.light, d.sprite);
            shape.roof = roof[0];
            shape.roof_kept = roof[1];
            shape.stretch = stretchShare(d.sprite);
            shape.copied = copiedParts(d.blocks, body, blocks_target.eye, d.sprite);
            var pictures: std.ArrayList(Image) = .empty;
            var eyes: std.ArrayList(Eye) = .empty;
            for (scene.sources, scene.eyes, scene.same) |source, eye, same| if (same) {
                try pictures.append(arena, pictureOf(source, "1").?);
                try eyes.append(arena, eye);
            };
            const chimneys = try keptChimneys(arena, d.blocks, pictureOf(own, "1").?, scene.eyes[scene.fixed]);
            shape.chimney = chimneys[0];
            shape.chimney_kept = chimneys[1];
            shape.painted = try paintedChimneys(arena, d.blocks, body, blocks_target.eye, d.sprite, pictures.items, eyes.items);
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
    if (how.sprite) {
        const same = try sameSprite(ctx, scene, blocks_target.turn, blocks_target.light, blocks_target.name, how.mirror, drawn);
        var turned: DrawnTurn = .{ .target = same.target, .drawn = same.drawn, .shape = shape, .failure = failure, .fallback = true, .mirror = how.mirror };
        if (try judgeTurn(arena, scene, &turned)) return turned;
        refused = try std.fmt.allocPrint(arena, "{s}{s}: {s}", .{ if (refused) |r| try std.fmt.allocPrint(arena, "{s}; ", .{r}) else "", if (how.mirror) "2003 sprite mirrored" else "2003 sprite", turned.handed_failure.? });
    }
    var turned: DrawnTurn = .{ .target = blocks_target, .drawn = drawn, .shape = shape, .failure = failure, .fallback = false, .mirror = false, .refused = refused };
    _ = try judgeTurn(arena, scene, &turned);
    return turned;
}

/// What a turn's object is, for `spriteTurn`.
pub const TurnObject = struct { thin: bool, small: bool, failed: bool, kept: bool, rectangle: bool, odd: bool, heap: bool };

/// Whether a turn is offered as the 2003 pictures (`sameSprite`), which
/// `drawTurn` takes only where the gate finds them the shape turned, and
/// mirrored:
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
    return zeroOnFootprint(scene, target, lo, hi);
}

/// The zero cross that lays a shape whose ground box is `lo`..`hi` as if
/// moved along the line of sight to the footprint's middle (`footprintZero`).
fn zeroOnFootprint(scene: Scene, target: Turned, lo: [2]f64, hi: [2]f64) [2]f64 {
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
    const work = if (no_work) try std.fs.path.join(arena, &.{ out, ".work" }) else try bk_tool.inputPath(arena, work_arg);
    // A turn k (`<variant>_r<k>`, lit by the 2003 light fixed to the camera),
    // or `<k>l<s>`: turn k lit by the sun fixed in the world as camera step s
    // shows it (`<variant>_r<k>_l<s>`, the light of turn k - s, M006 round 2).
    var turns: std.ArrayList([2]u2) = .empty;
    var split = std.mem.splitScalar(u8, turns_arg, ',');
    while (split.next()) |t| {
        const at = std.mem.indexOfScalar(u8, t, 'l') orelse t.len;
        const k = std.fmt.parseInt(u2, t[0..at], 10) catch return error.Usage;
        const s = if (at < t.len) std.fmt.parseInt(u2, t[at + 1 ..], 10) catch return error.Usage else 0;
        if (k == 0 or (at < t.len and s == 0)) return error.Usage;
        try turns.append(arena, .{ k, s });
    }

    const found = findFamily(arena, io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
    const scene = try loadScene(ctx, found, found.index, null, false);
    var targets: std.ArrayList(Turned) = .empty;
    for (turns.items) |ks| try targets.append(arena, turnTarget(scene, ks[0], ks[0] -% ks[1], if (ks[1] == 0) try std.fmt.allocPrint(arena, "{s}_r{d}", .{ scene.name, ks[0] }) else try std.fmt.allocPrint(arena, "{s}_r{d}_l{d}", .{ scene.name, ks[0], ks[1] })));

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
        // One zero cross for every picture of a turn (the engine's packs share
        // it): on a mesh, the mesh's.
        const zero = if (fallback) target.eye.anchor else if (scene.mesh) |ms| meshZero(scene, ms, target) else footprintZero(scene, target);
        var report: std.Io.Writer.Allocating = .init(arena);
        const r = &report.writer;
        try r.print("{{\"variant\":\"{s}\",\"side\":{d},\"turn\":{d},\"new_side\":{d},\"full3d\":{},\"fallback\":{},\"mirror\":{},\"sibling\":\"{s}\",\"hand\":{},\"shape\":{{\"height\":{d:.3},\"ground\":{d:.3},\"cover\":{d:.3},\"protrusion\":{d:.4},\"protrusion_2003\":{d:.4},\"low\":{d:.0},\"low_kept\":{d:.0},\"roof\":{d:.0},\"roof_kept\":{d:.0},\"stretch\":{d:.4},\"copied\":{d:.0},\"chimney\":{d:.0},\"chimney_kept\":{d:.0},\"painted\":{d:.0},\"failure\":\"{s}\"}},\n", .{ scene.name, scene.own_side, target.turn, new_side, scene.full3d.len > 0, fallback, mirror, turned.sibling orelse "", scene.hand != null, shape.height, shape.ground, shape.cover, shape.protrusion, shape.protrusion_2003, shape.low, shape.low_kept, shape.roof, shape.roof_kept, shape.stretch, shape.copied, shape.chimney, shape.chimney_kept, shape.painted, failure orelse "" });
        try r.print("\"handed\":{{\"turned\":{d:.4},\"mirrored\":{d:.4},\"symmetric\":{d:.4},\"own\":{d:.4},\"parts\":{d:.0},\"parts_kept\":{d:.0},\"parts_mirrored\":{d:.0},\"parts_mirrored_kept\":{d:.0},\"failure\":\"{s}\",\"refused\":\"{s}\"}},\n\"blocks\":{{", .{ turned.handed.turned, turned.handed.mirrored, turned.handed.symmetric, turned.handed.own, turned.handed.parts, turned.handed.parts_kept, turned.handed.parts_mirrored, turned.handed.parts_mirrored_kept, turned.handed_failure orelse "", turned.refused orelse "" });
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
        try r.writeAll("]");
        if (scene.mesh) |ms| for (drawn) |d| if (std.mem.eql(u8, d.state, "1")) {
            const c = try meshCheck(arena, scene, ms, target, d);
            try r.print(",\n\"mesh\":{{\"outline\":{d:.4},\"holes\":{d},\"patched\":{d},\"colour\":{d:.1},\"kinds\":{d}}}", .{ c.outline, c.holes, c.patched, c.colour, c.kinds });
        };
        try r.writeAll(",\n\"pictures\":[");
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
                "--force",
            });
            // A prop with no passable grid (a box, a barrel) has none to lock.
            if (grid.rows.len > 0) try argv.appendSlice(arena, &.{ "--pass", try std.mem.join(arena, "/", grid.rows), "--locked", try std.fmt.allocPrint(arena, "{d},{d}", .{ locked[0], locked[1] }) });
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
    if (no_work) cwd.deleteTree(io, work) catch {};
    try depScene(ctx, found, scene);
    try writeDeps(ctx);
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
            const target = turnTarget(scene, @intCast(k), @intCast(k), "");
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
        const turned = try drawTurn(ctx, scene, turnTarget(scene, k, k, try std.fmt.allocPrint(arena, "{s}_holdout", .{name})));
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
    shape: struct { height: f64, ground: f64, cover: f64, protrusion: f64, protrusion_2003: f64, low: f64 = 0, low_kept: f64 = 0, roof: f64 = 0, roof_kept: f64 = 0, stretch: f64 = 0, copied: f64 = 0, chimney: f64 = 0, chimney_kept: f64 = 0, painted: f64 = 0, failure: []const u8 },
    handed: struct { turned: f64 = 1, mirrored: f64 = 0, symmetric: f64 = 1, own: f64 = 1, failure: []const u8 = "" } = .{},
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
            // The three turns lit by the 2003 light fixed to the camera, then the
            // three lit by the sun fixed in the world as the camera step that
            // shows each at facing 0 (`<variant>_r<k>_l<k>`, M006 round 2).
            for (1..7) |i| {
                const k = (i - 1) % 3 + 1;
                const label = if (i <= 3) try std.fmt.allocPrint(arena, "r{d}", .{k}) else try std.fmt.allocPrint(arena, "r{d}l{d}", .{ k, k });
                const target = if (i <= 3) try std.fmt.allocPrint(arena, "{s}_r{d}", .{ name, k }) else try std.fmt.allocPrint(arena, "{s}_r{d}_l{d}", .{ name, k, k });
                const side = (try readJson(SideReport, ctx, try std.fs.path.join(arena, &.{ dir, try std.fmt.allocPrint(arena, "{s}.map", .{target}), "sources.json" }))) orelse continue;
                generated += 1;
                if (variant.side != null and i <= 3) try new_sides.writer.print("{s}{d}", .{ if (k > 1) ", " else "", side.new_side });
                hand = hand or side.hand;
                if (side.fallback) try same_turns.append(arena, try std.fmt.allocPrint(arena, "{s}{s}{s}", .{ label, if (side.mirror) " mirrored" else if (side.sibling.len > 0) " = " else "", side.sibling }));
                if (side.shape.failure.len > 0 and !side.fallback and failure == null) failure = try std.fmt.allocPrint(arena, "{s} shape: {s}", .{ label, side.shape.failure });
                // Every turn, however drawn, is the shape turned (M006 round 3).
                if (side.handed.failure.len > 0 and failure == null) failure = try std.fmt.allocPrint(arena, "{s} turn: {s}", .{ label, side.handed.failure });
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
                        if (why != null and failure == null) failure = try std.fmt.allocPrint(arena, "{s} {s}: {s}", .{ label, picture.picture, why.? });
                    }
                    try d.print("| {s} | {s} | {d} | {s} | {d:.1} | {d:.1} | {d:.1} | {d:.1} | ", .{ name, label, side.new_side, picture.picture, percent(picture.projected, all), percent(picture.mirrored, all), percent(picture.transferred, all), percent(picture.filled, all) });
                    if (row) |r| try d.print("{d:.2} | {d:.1} | {d:.3} | {s} |\n", .{ r.sharpness, r.colour, r.hue, row_gate }) else try d.writeAll("- | - | - | - |\n");
                }
                const sh = side.shape;
                const ha = side.handed;
                try shapes.writer.print("| {s} | {s} | {d:.2} | {d:.2} | {d:.2} | {d:.3} | {d:.3} | {d:.0} | {d:.0} | {d:.0} | {d:.0} | {d:.3} | {d:.0} | {d:.0} | {d:.0} | {d:.0} | {s} | {d:.3} | {d:.3} | {d:.3} | {d:.3} | {s} | {s} |\n", .{ name, label, sh.height, sh.ground, sh.cover, sh.protrusion, sh.protrusion_2003, sh.low, sh.low_kept, sh.roof, sh.roof_kept, sh.stretch, sh.copied, sh.chimney, sh.chimney_kept, sh.painted, if (sh.failure.len > 0) sh.failure else "pass", ha.turned, ha.mirrored, ha.symmetric, ha.own, if (ha.failure.len > 0) ha.failure else "pass", if (side.sibling.len > 0) "2003 sibling" else if (side.mirror) "2003 sprite mirrored" else if (side.fallback) "2003 sprite" else "blocks" });
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
            \\another side (copied), the pixels of the chimneys the 2003 sides of the colourway show above the roofs and
            \\of those the blocks cover there (kept), the pixels it drew on a roof of the body from where a 2003 side
            \\sees such a chimney (painted); a thin object, or a small one whose shape fails, is drawn as its 2003
            \\sprite where the turn keeps the footprint, a heap (`tools/zig/fixtures/bk-views/props.json`) on every turn.
            \\The turn, however drawn (a turn is a rotation, never a mirror): the outline IoU of the turned intact picture
            \\with the variant's shape turned, with that turn mirrored, of the turned shape with its own mirror
            \\(symmetric), and of the 2003 picture with the shape unturned (own).
            \\
            \\| Variant | Turn | Height | Ground | Outline | Protrusion | 2003 | Low parts | Kept | Roof parts | Kept | Stretched | Copied | Chimneys | Kept | Painted | Shape | Turned | Mirrored | Symmetric | Own | Turn | Drawn as |
            \\|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
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
        \\  {d:.0} pixels of the body copied from beside a part, a chimney, dormer or door step of another side, nor
        \\  drawn on a roof from where a 2003 side of the colourway sees a chimney, and its blocks cover half or
        \\  more of the chimneys the variant's 2003 picture shows above its roofs), the style
        \\  of every generated picture, and the hold-out where the family has another side of the colourway, against
        \\  the floors of `tools/zig/fixtures/bk-views/gate.json`: {d} of {d} variants pass; hold-out {d} of {d}; {d}
        \\  fail on the shape; {d} more pass the measures but look wrong on the contact sheet (`bk-views contact`),
        \\  listed in gate.json's `by_eye`. A failing variant is listed with its first reason, to be fixed by hand.
        \\- 2003 sprite: {d} variants have turns drawn as their 2003 sprites, moved onto the turned footprint: thin
        \\  objects (a 2003 protrusion of {d:.2} or more) and small ones ({d} footprint cells or fewer) whose blocks fail
        \\  the shape, where the turn keeps the footprint (cells IoU {d:.1} or more), or mirrored onto it on an odd
        \\  turn of a rectangular footprint; and the heaps (supply depots, earth pillboxes) on every turn, mirrored on
        \\  an odd one; each, and each 2003 sibling side, only where it is the variant's shape turned (a turn is a
        \\  rotation, never a mirror: the Turn column of the family reports), else the turn is drawn on the blocks.
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

// ---------------------------------------------------------------------------
// Shape models for review (M006 round 4): every object that turns as a vector
// model of named parts, turned to the four sides and drawn for the maintainer.

/// The kind of a model part, which the review colours.
pub const PartKind = enum { body, chimney, low, roof_part, tower, pole, window, door };

/// A part of a shape model: a block (the format of `shapes_dir`), a pole (a
/// thin square prism from `from` to `to`) or a rectangle on face `face` of
/// block part `on` (a window or a door: across the face from its left end
/// seen from outside, and up, in world units).
pub const Part = struct {
    kind: PartKind,
    block: ?Block = null,
    pole: ?struct { from: [3]f64, to: [3]f64, radius: f64 } = null,
    rect: ?struct { on: usize, face: []const u8, lo: [2]f64, hi: [2]f64 } = null,
};

/// An object's shape model: its review number (`shape_ids_file`), its path
/// under Data, how it was made (hand shape, fit, poles) and the outline IoU
/// of its turn 0 with its 2003 intact picture.
pub const Model = struct { id: u32, path: []const u8, how: []const u8, fit: f64, footprint: []const []const u8, parts: []const Part };

/// The review number of every modelled object, stable across runs: an
/// object keeps its number, a new one takes the next.
pub const shape_ids_file = "tools/zig/fixtures/bk-views/shape-ids.json";
/// The models, `buildings/<family>.json` and `objects/<family>.json`, each a
/// map of variant folder name to model.
pub const models_dir = "tools/zig/fixtures/bk-views/models";

const ShapeIds = struct { ids: []const struct { id: u32, path: []const u8 } };

/// The objects that turn, in contents order: every building variant, then
/// every simple object of Data/Objects/SimpleObjects (flora is not modelled:
/// a tree turns as itself), as `Buildings/...` and `Objects/SimpleObjects/...`.
fn turningObjects(ctx: *bk_tool.Context, buildings: []const u8, objects: []const u8) ![]const []const u8 {
    var out: std.ArrayList([]const u8) = .empty;
    for ([2][]const u8{ buildings, objects }, [2][]const u8{ "Buildings", "Objects/SimpleObjects" }) |root, prefix| {
        for (try listFamilies(ctx.arena, ctx.io, root)) |family| for (family.variants) |v| {
            try out.append(ctx.arena, try std.fmt.allocPrint(ctx.arena, "{s}/{s}", .{ prefix, v.path }));
        };
    }
    return out.items;
}

/// `ids`: numbers every object that turns in `ids-file`, keeping the numbers
/// it holds.
fn idsCommand(ctx: *bk_tool.Context, ids_arg: []const u8, buildings_arg: []const u8, objects_arg: []const u8) !void {
    const arena = ctx.arena;
    const path = try bk_tool.inputPath(arena, ids_arg);
    const known = try readJson(ShapeIds, ctx, path);
    var next: u32 = 1;
    var text: std.Io.Writer.Allocating = .init(arena);
    try text.writer.writeAll("{\"ids\":[");
    var n: usize = 0;
    if (known) |k| for (k.ids) |e| {
        try text.writer.print("{s}\n{{\"id\":{d},\"path\":\"{s}\"}}", .{ if (n > 0) "," else "", e.id, e.path });
        next = @max(next, e.id + 1);
        n += 1;
    };
    for (try turningObjects(ctx, try bk_tool.inputPath(arena, buildings_arg), try bk_tool.inputPath(arena, objects_arg))) |object| {
        if (known) |k| if (for (k.ids) |e| {
            if (std.mem.eql(u8, e.path, object)) break true;
        } else false) continue;
        try text.writer.print("{s}\n{{\"id\":{d},\"path\":\"{s}\"}}", .{ if (n > 0) "," else "", next, object });
        next += 1;
        n += 1;
    }
    try text.writer.writeAll("\n]}\n");
    try writeCrlf(ctx, std.fs.path.dirname(path) orelse ".", std.fs.path.basename(path), text.written());
    try ctx.out.print("{d} objects numbered in {s}\n", .{ n, path });
}

/// The face of a model: its part, a name and its corners (a convex polygon).
const ModelFace = struct { part: usize, kind: PartKind, roof: bool, name: []const u8, points: []const [3]f64, wall: []const u8 = "" };

/// The faces of a block: four walls (a gable end reaching up to the ridge),
/// its roof planes (two for a gable, four for a hip) or a flat top.
fn blockFaces(gpa: std.mem.Allocator, out: *std.ArrayList(ModelFace), part: usize, kind: PartKind, b: Block, base: f64) !void {
    const x0 = b.lo[0];
    const x1 = b.hi[0];
    const y0 = b.lo[1];
    const y1 = b.hi[1];
    const w = b.wall;
    const r = @max(b.ridge, b.wall);
    const xm = (x0 + x1) / 2;
    const ym = (y0 + y1) / 2;
    const add = struct {
        fn f(g: std.mem.Allocator, o: *std.ArrayList(ModelFace), p: usize, k: PartKind, roof: bool, name: []const u8, points: []const [3]f64) !void {
            try o.append(g, .{ .part = p, .kind = k, .roof = roof, .name = name, .points = try g.dupe([3]f64, points) });
        }
    }.f;
    switch (b.roof) {
        .flat => {
            try add(gpa, out, part, kind, false, "wall -y", &.{ .{ x0, y0, base }, .{ x1, y0, base }, .{ x1, y0, w }, .{ x0, y0, w } });
            try add(gpa, out, part, kind, false, "wall +x", &.{ .{ x1, y0, base }, .{ x1, y1, base }, .{ x1, y1, w }, .{ x1, y0, w } });
            try add(gpa, out, part, kind, false, "wall +y", &.{ .{ x1, y1, base }, .{ x0, y1, base }, .{ x0, y1, w }, .{ x1, y1, w } });
            try add(gpa, out, part, kind, false, "wall -x", &.{ .{ x0, y1, base }, .{ x0, y0, base }, .{ x0, y0, w }, .{ x0, y1, w } });
            try add(gpa, out, part, kind, true, "roof flat", &.{ .{ x0, y0, w }, .{ x1, y0, w }, .{ x1, y1, w }, .{ x0, y1, w } });
        },
        .gable_x => {
            // The ridge along x at y = ym: walls along x, gable ends at x0 and x1.
            try add(gpa, out, part, kind, false, "wall -y", &.{ .{ x0, y0, base }, .{ x1, y0, base }, .{ x1, y0, w }, .{ x0, y0, w } });
            try add(gpa, out, part, kind, false, "gable +x", &.{ .{ x1, y0, base }, .{ x1, y1, base }, .{ x1, y1, w }, .{ x1, ym, r }, .{ x1, y0, w } });
            try add(gpa, out, part, kind, false, "wall +y", &.{ .{ x1, y1, base }, .{ x0, y1, base }, .{ x0, y1, w }, .{ x1, y1, w } });
            try add(gpa, out, part, kind, false, "gable -x", &.{ .{ x0, y1, base }, .{ x0, y0, base }, .{ x0, y0, w }, .{ x0, ym, r }, .{ x0, y1, w } });
            try add(gpa, out, part, kind, true, "roof -y", &.{ .{ x0, y0, w }, .{ x1, y0, w }, .{ x1, ym, r }, .{ x0, ym, r } });
            try add(gpa, out, part, kind, true, "roof +y", &.{ .{ x1, y1, w }, .{ x0, y1, w }, .{ x0, ym, r }, .{ x1, ym, r } });
        },
        .gable_y => {
            try add(gpa, out, part, kind, false, "gable -y", &.{ .{ x0, y0, base }, .{ x1, y0, base }, .{ x1, y0, w }, .{ xm, y0, r }, .{ x0, y0, w } });
            try add(gpa, out, part, kind, false, "wall +x", &.{ .{ x1, y0, base }, .{ x1, y1, base }, .{ x1, y1, w }, .{ x1, y0, w } });
            try add(gpa, out, part, kind, false, "gable +y", &.{ .{ x1, y1, base }, .{ x0, y1, base }, .{ x0, y1, w }, .{ xm, y1, r }, .{ x1, y1, w } });
            try add(gpa, out, part, kind, false, "wall -x", &.{ .{ x0, y1, base }, .{ x0, y0, base }, .{ x0, y0, w }, .{ x0, y1, w } });
            try add(gpa, out, part, kind, true, "roof +x", &.{ .{ x1, y0, w }, .{ x1, y1, w }, .{ xm, y1, r }, .{ xm, y0, r } });
            try add(gpa, out, part, kind, true, "roof -x", &.{ .{ x0, y1, w }, .{ x0, y0, w }, .{ xm, y0, r }, .{ xm, y1, r } });
        },
        .hip => {
            try add(gpa, out, part, kind, false, "wall -y", &.{ .{ x0, y0, base }, .{ x1, y0, base }, .{ x1, y0, w }, .{ x0, y0, w } });
            try add(gpa, out, part, kind, false, "wall +x", &.{ .{ x1, y0, base }, .{ x1, y1, base }, .{ x1, y1, w }, .{ x1, y0, w } });
            try add(gpa, out, part, kind, false, "wall +y", &.{ .{ x1, y1, base }, .{ x0, y1, base }, .{ x0, y1, w }, .{ x1, y1, w } });
            try add(gpa, out, part, kind, false, "wall -x", &.{ .{ x0, y1, base }, .{ x0, y0, base }, .{ x0, y0, w }, .{ x0, y1, w } });
            // The ridge along the longer side, as blockPlanes slopes it.
            const half = @min(x1 - x0, y1 - y0) / 2;
            const long_x = x1 - x0 > y1 - y0;
            const a = [2]f64{ if (long_x) x0 + half else xm, if (long_x) ym else y0 + half };
            const c = [2]f64{ if (long_x) x1 - half else xm, if (long_x) ym else y1 - half };
            if (long_x) {
                try add(gpa, out, part, kind, true, "roof -y", &.{ .{ x0, y0, w }, .{ x1, y0, w }, .{ c[0], c[1], r }, .{ a[0], a[1], r } });
                try add(gpa, out, part, kind, true, "roof +x", &.{ .{ x1, y0, w }, .{ x1, y1, w }, .{ c[0], c[1], r } });
                try add(gpa, out, part, kind, true, "roof +y", &.{ .{ x1, y1, w }, .{ x0, y1, w }, .{ a[0], a[1], r }, .{ c[0], c[1], r } });
                try add(gpa, out, part, kind, true, "roof -x", &.{ .{ x0, y1, w }, .{ x0, y0, w }, .{ a[0], a[1], r } });
            } else {
                try add(gpa, out, part, kind, true, "roof -y", &.{ .{ x0, y0, w }, .{ x1, y0, w }, .{ a[0], a[1], r } });
                try add(gpa, out, part, kind, true, "roof +x", &.{ .{ x1, y0, w }, .{ x1, y1, w }, .{ c[0], c[1], r }, .{ a[0], a[1], r } });
                try add(gpa, out, part, kind, true, "roof +y", &.{ .{ x1, y1, w }, .{ x0, y1, w }, .{ c[0], c[1], r } });
                try add(gpa, out, part, kind, true, "roof -x", &.{ .{ x0, y1, w }, .{ x0, y0, w }, .{ a[0], a[1], r }, .{ c[0], c[1], r } });
            }
        },
    }
}

fn cross3(a: [3]f64, b: [3]f64) [3]f64 {
    return .{ a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}

fn unit3(a: [3]f64) [3]f64 {
    const l = @sqrt(dot(a, a));
    return if (l == 0) a else .{ a[0] / l, a[1] / l, a[2] / l };
}

/// Where a part standing on the building (a chimney, a dormer, a tower)
/// shows from: the lowest point of the body's roofs under it, so that it is
/// drawn from the roof up and not through the house (the model's blocks
/// reach down to the ground, as `side` takes them).
fn partBase(gpa: std.mem.Allocator, parts: []const Part, kind: PartKind, b: Block) !f64 {
    if (kind != .chimney and kind != .roof_part and kind != .tower) return 0;
    var body: std.ArrayList(Block) = .empty;
    defer body.deinit(gpa);
    for (parts) |p| if (p.kind == .body) if (p.block) |q| try body.append(gpa, q);
    var lowest = std.math.inf(f64);
    const inset = [2]f64{ @min(0.5, (b.hi[0] - b.lo[0]) / 4), @min(0.5, (b.hi[1] - b.lo[1]) / 4) };
    for ([_][2]f64{ .{ b.lo[0] + inset[0], b.lo[1] + inset[1] }, .{ b.hi[0] - inset[0], b.lo[1] + inset[1] }, .{ b.lo[0] + inset[0], b.hi[1] - inset[1] }, .{ b.hi[0] - inset[0], b.hi[1] - inset[1] }, .{ (b.lo[0] + b.hi[0]) / 2, (b.lo[1] + b.hi[1]) / 2 } }) |c| {
        const hit = castRay(body.items, .{ c[0], c[1], 1000 }, .{ 0, 0, -1 }) orelse continue;
        lowest = @min(lowest, 1000 - hit.t);
    }
    return if (lowest == std.math.inf(f64)) 0 else @min(lowest, @max(b.wall, b.ridge) - 1);
}

/// Every face of a model's parts.
fn modelFaces(gpa: std.mem.Allocator, parts: []const Part) ![]ModelFace {
    var out: std.ArrayList(ModelFace) = .empty;
    for (parts, 0..) |p, i| {
        if (p.block) |b| try blockFaces(gpa, &out, i, p.kind, b, try partBase(gpa, parts, p.kind, b));
        if (p.pole) |pole| {
            const axis = [3]f64{ pole.to[0] - pole.from[0], pole.to[1] - pole.from[1], pole.to[2] - pole.from[2] };
            var u = cross3(axis, .{ 0, 0, 1 });
            if (dot(u, u) < 1e-9) u = .{ 1, 0, 0 };
            u = unit3(u);
            const v = unit3(cross3(axis, u));
            var corners: [8][3]f64 = undefined;
            for (0..2) |e| for (0..4) |c| {
                const end = if (e == 0) pole.from else pole.to;
                const su: f64 = if (c == 0 or c == 3) -1 else 1;
                const sv: f64 = if (c < 2) -1 else 1;
                corners[e * 4 + c] = .{ end[0] + pole.radius * (su * u[0] + sv * v[0]), end[1] + pole.radius * (su * u[1] + sv * v[1]), end[2] + pole.radius * (su * u[2] + sv * v[2]) };
            };
            for (0..4) |c| try out.append(gpa, .{ .part = i, .kind = .pole, .roof = false, .name = "pole", .points = try gpa.dupe([3]f64, &.{ corners[c], corners[(c + 1) % 4], corners[4 + (c + 1) % 4], corners[4 + c] }) });
            try out.append(gpa, .{ .part = i, .kind = .pole, .roof = false, .name = "pole end", .points = try gpa.dupe([3]f64, corners[4..8]) });
            try out.append(gpa, .{ .part = i, .kind = .pole, .roof = false, .name = "pole foot", .points = try gpa.dupe([3]f64, corners[0..4]) });
        }
        if (p.rect) |rect| {
            const on = parts[rect.on].block orelse continue;
            const n = faceDirection(rect.face);
            const start = faceStart(on, n);
            const right = [2]f64{ -n[1], n[0] };
            var corners: [4][3]f64 = undefined;
            for (&corners, [4][2]f64{ .{ rect.lo[0], rect.lo[1] }, .{ rect.hi[0], rect.lo[1] }, .{ rect.hi[0], rect.hi[1] }, .{ rect.lo[0], rect.hi[1] } }) |*c, uv| {
                c.* = .{ start[0] + right[0] * uv[0] + n[0] * 0.4, start[1] + right[1] * uv[0] + n[1] * 0.4, uv[1] };
            }
            // Drawn on its wall: part `on`'s face `face`.
            try out.append(gpa, .{ .part = rect.on, .kind = p.kind, .roof = false, .name = @tagName(p.kind), .points = try gpa.dupe([3]f64, &corners), .wall = rect.face });
        }
    }
    return out.items;
}

/// The outward direction of a block's wall named `wall +x` and so on.
fn faceDirection(name: []const u8) [2]f64 {
    return if (std.mem.endsWith(u8, name, "+x")) .{ 1, 0 } else if (std.mem.endsWith(u8, name, "-x")) .{ -1, 0 } else if (std.mem.endsWith(u8, name, "+y")) .{ 0, 1 } else .{ 0, -1 };
}

/// The left end, seen from outside, of a block's wall facing `n`: along the
/// wall runs (-n.y, n.x).
fn faceStart(b: Block, n: [2]f64) [2]f64 {
    const right = [2]f64{ -n[1], n[0] };
    return .{
        if (n[0] > 0) b.hi[0] else if (n[0] < 0) b.lo[0] else if (right[0] > 0) b.lo[0] else b.hi[0],
        if (n[1] > 0) b.hi[1] else if (n[1] < 0) b.lo[1] else if (right[1] > 0) b.lo[1] else b.hi[1],
    };
}

/// A face's outward normal: from Newell's sum, turned away from its part's
/// middle.
fn faceNormal(face: ModelFace, middle: [3]f64) [3]f64 {
    var n = [3]f64{ 0, 0, 0 };
    var c = [3]f64{ 0, 0, 0 };
    for (face.points, 0..) |p, i| {
        const q = face.points[(i + 1) % face.points.len];
        n[0] += (p[1] - q[1]) * (p[2] + q[2]);
        n[1] += (p[2] - q[2]) * (p[0] + q[0]);
        n[2] += (p[0] - q[0]) * (p[1] + q[1]);
        for (0..3) |k| c[k] += p[k] / @as(f64, @floatFromInt(face.points.len));
    }
    n = unit3(n);
    const away = [3]f64{ c[0] - middle[0], c[1] - middle[1], c[2] - middle[2] };
    return if (dot(n, away) < 0) .{ -n[0], -n[1], -n[2] } else n;
}

/// A face as an eye sees it: its screen polygon, its plane (a point and the
/// outward normal) and its mean depth along the line of sight.
const SeenFace = struct { face: ModelFace, points: []const [2]f64, depth: f64, at: [3]f64 = .{ 0, 0, 0 }, normal: [3]f64 = .{ 0, 0, 1 } };

/// The faces `eye` sees of a model (back faces left out) in a painter's
/// order: where two faces overlap on the screen, the one farther along the
/// line of sight at a point of the overlap comes first (a mean depth puts a
/// large roof plane behind a small porch roof that stands behind it); a cycle
/// is broken by the mean depth. A window or a door lies just before its wall.
fn seenFaces(gpa: std.mem.Allocator, faces: []const ModelFace, eye: Eye) ![]SeenFace {
    const d = eye.ray(0, 0)[1];
    var middles: std.AutoHashMap(usize, [4]f64) = .init(gpa);
    for (faces) |f| {
        if (f.kind == .window or f.kind == .door) continue;
        for (f.points) |p| {
            const e = try middles.getOrPutValue(f.part, .{ 0, 0, 0, 0 });
            for (0..3) |k| e.value_ptr[k] += p[k];
            e.value_ptr[3] += 1;
        }
    }
    var seen: std.ArrayList(SeenFace) = .empty;
    for (faces) |f| {
        const opening = f.kind == .window or f.kind == .door;
        const normal = if (opening) blk: {
            const n = faceDirection(f.wall);
            break :blk [3]f64{ n[0], n[1], 0 };
        } else blk: {
            const m = middles.get(f.part).?;
            break :blk faceNormal(f, .{ m[0] / m[3], m[1] / m[3], m[2] / m[3] });
        };
        if (dot(normal, d) >= -1e-6) continue;
        var depth: f64 = 0;
        for (f.points) |p| depth += dot(p, d) / @as(f64, @floatFromInt(f.points.len));
        const points = try gpa.alloc([2]f64, f.points.len);
        for (f.points, points) |p, *q| q.* = eye.project(p);
        try seen.append(gpa, .{ .face = f, .points = points, .depth = depth, .at = f.points[0], .normal = normal });
    }
    const n = seen.items.len;
    // behind[i * n + j]: face i is drawn before face j.
    const behind = try gpa.alloc(bool, n * n);
    defer gpa.free(behind);
    @memset(behind, false);
    const incoming = try gpa.alloc(usize, n);
    defer gpa.free(incoming);
    @memset(incoming, 0);
    for (0..n) |i| for (i + 1..n) |j| {
        const p = overlapPoint(gpa, seen.items[i].points, seen.items[j].points) orelse continue;
        const r = eye.ray(p[0], p[1]);
        const ti = planeDepth(seen.items[i], r);
        const tj = planeDepth(seen.items[j], r);
        if (@abs(ti - tj) < 1e-6) continue;
        const first, const second = if (ti > tj) .{ i, j } else .{ j, i };
        behind[first * n + second] = true;
        incoming[second] += 1;
    };
    // Kahn's order, the farthest ready face first; a cycle is broken at the
    // farthest face left.
    const done = try gpa.alloc(bool, n);
    defer gpa.free(done);
    @memset(done, false);
    var out: std.ArrayList(SeenFace) = .empty;
    while (out.items.len < n) {
        var pick: ?usize = null;
        for (0..n) |i| {
            if (done[i] or incoming[i] != 0) continue;
            if (pick == null or seen.items[i].depth > seen.items[pick.?].depth) pick = i;
        }
        if (pick == null) for (0..n) |i| {
            if (done[i]) continue;
            if (pick == null or seen.items[i].depth > seen.items[pick.?].depth) pick = i;
        };
        const i = pick.?;
        done[i] = true;
        try out.append(gpa, seen.items[i]);
        for (0..n) |j| if (behind[i * n + j] and !done[j]) {
            incoming[j] -|= 1;
        };
    }
    return out.items;
}

/// The depth along ray `r` (a point and a unit direction) where it meets a
/// face's plane.
fn planeDepth(s: SeenFace, r: [2][3]f64) f64 {
    const along = dot(s.normal, r[1]);
    if (@abs(along) < 1e-9) return dot(.{ s.at[0] - r[0][0], s.at[1] - r[0][1], s.at[2] - r[0][2] }, r[1]);
    return dot(s.normal, .{ s.at[0] - r[0][0], s.at[1] - r[0][1], s.at[2] - r[0][2] }) / along;
}

/// A point inside both convex polygons (the middle of their intersection,
/// clipped one by the other), or null where they overlap by less than a
/// hundredth of a square pixel.
fn overlapPoint(gpa: std.mem.Allocator, a: []const [2]f64, b: []const [2]f64) ?[2]f64 {
    var lo_a = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi_a = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (a) |p| {
        lo_a = .{ @min(lo_a[0], p[0]), @min(lo_a[1], p[1]) };
        hi_a = .{ @max(hi_a[0], p[0]), @max(hi_a[1], p[1]) };
    }
    var lo_b = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi_b = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (b) |p| {
        lo_b = .{ @min(lo_b[0], p[0]), @min(lo_b[1], p[1]) };
        hi_b = .{ @max(hi_b[0], p[0]), @max(hi_b[1], p[1]) };
    }
    if (hi_a[0] <= lo_b[0] or hi_b[0] <= lo_a[0] or hi_a[1] <= lo_b[1] or hi_b[1] <= lo_a[1]) return null;
    // Sutherland-Hodgman: a clipped by each edge of b, b turned
    // counter-clockwise first.
    var area: f64 = 0;
    for (b, 0..) |p, i| {
        const q = b[(i + 1) % b.len];
        area += p[0] * q[1] - q[0] * p[1];
    }
    if (@abs(area) < 1e-9) return null;
    const sign: f64 = if (area > 0) 1 else -1;
    var poly: std.ArrayList([2]f64) = .empty;
    defer poly.deinit(gpa);
    poly.appendSlice(gpa, a) catch return null;
    var next: std.ArrayList([2]f64) = .empty;
    defer next.deinit(gpa);
    for (b, 0..) |e0, i| {
        const e1 = b[(i + 1) % b.len];
        next.clearRetainingCapacity();
        const inside = struct {
            fn f(s: f64, u: [2]f64, v: [2]f64, p: [2]f64) f64 {
                return s * ((v[0] - u[0]) * (p[1] - u[1]) - (v[1] - u[1]) * (p[0] - u[0]));
            }
        }.f;
        for (poly.items, 0..) |p, k| {
            const q = poly.items[(k + 1) % poly.items.len];
            const ip = inside(sign, e0, e1, p);
            const iq = inside(sign, e0, e1, q);
            if (ip >= 0) next.append(gpa, p) catch return null;
            if ((ip >= 0) != (iq >= 0)) {
                const t = ip / (ip - iq);
                next.append(gpa, .{ p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1]) }) catch return null;
            }
        }
        std.mem.swap(std.ArrayList([2]f64), &poly, &next);
        if (poly.items.len < 3) return null;
    }
    var sum: f64 = 0;
    var c = [2]f64{ 0, 0 };
    for (poly.items, 0..) |p, k| {
        const q = poly.items[(k + 1) % poly.items.len];
        const w = p[0] * q[1] - q[0] * p[1];
        sum += w;
        c = .{ c[0] + (p[0] + q[0]) * w, c[1] + (p[1] + q[1]) * w };
    }
    if (@abs(sum) < 0.02) return null;
    return .{ c[0] / (3 * sum), c[1] / (3 * sum) };
}

/// Fills a convex polygon into a mask of `width` x `height` (pixel centres).
fn fillPolygon(mask: []bool, width: usize, height: usize, points: []const [2]f64) void {
    var lo: f64 = std.math.inf(f64);
    var hi: f64 = -std.math.inf(f64);
    for (points) |p| {
        lo = @min(lo, p[1]);
        hi = @max(hi, p[1]);
    }
    const y0: usize = @intFromFloat(std.math.clamp(@floor(lo), 0, @as(f64, @floatFromInt(height))));
    const y1: usize = @intFromFloat(std.math.clamp(@ceil(hi), 0, @as(f64, @floatFromInt(height))));
    for (y0..y1) |y| {
        const py = @as(f64, @floatFromInt(y)) + 0.5;
        var left: f64 = std.math.inf(f64);
        var right: f64 = -std.math.inf(f64);
        for (points, 0..) |a, i| {
            const b = points[(i + 1) % points.len];
            if ((a[1] <= py) == (b[1] <= py)) continue;
            const x = a[0] + (py - a[1]) / (b[1] - a[1]) * (b[0] - a[0]);
            left = @min(left, x);
            right = @max(right, x);
        }
        if (left > right) continue;
        const x0: usize = @intFromFloat(std.math.clamp(@ceil(left - 0.5), 0, @as(f64, @floatFromInt(width))));
        const x1: usize = @intFromFloat(std.math.clamp(@floor(right - 0.5) + 1, 0, @as(f64, @floatFromInt(width))));
        for (x0..@max(x0, x1)) |x| mask[y * width + x] = true;
    }
}

/// The outline IoU of a model's faces as `eye` sees them with `picture`'s
/// (alpha 128 or more).
pub fn modelIou(gpa: std.mem.Allocator, parts: []const Part, eye: Eye, picture: Image) !f64 {
    const mask = try gpa.alloc(bool, picture.pixels.len);
    defer gpa.free(mask);
    @memset(mask, false);
    for (try seenFaces(gpa, try modelFaces(gpa, parts), eye)) |s| fillPolygon(mask, picture.width, picture.height, s.points);
    var both: f64 = 0;
    var either: f64 = 0;
    for (mask, picture.pixels) |m, p| {
        both += @floatFromInt(@intFromBool(m and p[3] >= 128));
        either += @floatFromInt(@intFromBool(m or p[3] >= 128));
    }
    return if (either == 0) 0 else both / either;
}

/// Thin straight parts of a picture (stakes, posts, beams, a hedgehog's
/// girders) as 3D poles: lines found one after another among the opaque
/// pixels (of 600 lines through two of them, the one holding the longest run
/// of pixels within 1.6 of it, gaps of 7 at most, which a crossing beam the
/// line before took leaves), each lifted to the world by the 2003
/// projection: its lower end stands on the ground, its upper end
/// lies as deep along the line of sight (one picture cannot tell how far a
/// pole leans towards the eye; the review lists poles as unsure).
pub fn fitPoles(gpa: std.mem.Allocator, picture: Image, eye: Eye) ![]Part {
    var points: std.ArrayList([2]f64) = .empty;
    for (0..picture.height) |y| for (0..picture.width) |x| {
        if (picture.pixels[y * picture.width + x][3] >= 128) try points.append(gpa, .{ @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5 });
    };
    var prng = std.Random.DefaultPrng.init(0x5eed);
    const random = prng.random();
    var out: std.ArrayList(Part) = .empty;
    const Run = struct { count: usize = 0, lo: f64 = 0, hi: f64 = 0, origin: [2]f64 = .{ 0, 0 }, dir: [2]f64 = .{ 0, 0 } };
    const along = try gpa.alloc(f64, points.items.len);
    defer gpa.free(along);
    while (out.items.len < 24 and points.items.len >= pole_pixels) {
        var best: Run = .{};
        for (0..600) |_| {
            const a = points.items[random.uintLessThan(usize, points.items.len)];
            const b = points.items[random.uintLessThan(usize, points.items.len)];
            const dx = b[0] - a[0];
            const dy = b[1] - a[1];
            const length = @sqrt(dx * dx + dy * dy);
            if (length < 6) continue;
            const dir = [2]f64{ dx / length, dy / length };
            var n: usize = 0;
            for (points.items) |p| {
                const rx = p[0] - a[0];
                const ry = p[1] - a[1];
                if (@abs(rx * dir[1] - ry * dir[0]) > 1.6) continue;
                along[n] = rx * dir[0] + ry * dir[1];
                n += 1;
            }
            std.mem.sort(f64, along[0..n], {}, std.sort.asc(f64));
            var start: usize = 0;
            for (0..n) |i| {
                if (i > 0 and along[i] - along[i - 1] > 7) start = i;
                if (i + 1 - start > best.count) best = .{ .count = i + 1 - start, .lo = along[start], .hi = along[i], .origin = a, .dir = dir };
            }
        }
        if (best.count < pole_pixels or best.hi - best.lo < 8) break;
        // A pole is thin: about three pixels a row of its length at most (a
        // wall's face holds a whole band along the line).
        if (@as(f64, @floatFromInt(best.count)) > 4 * (best.hi - best.lo)) break;
        // And free on both sides along most of its length: a line drawn
        // through a heap's or a wall's face has picture beside it.
        var free: usize = 0;
        var samples: usize = 0;
        var step = best.lo;
        while (step <= best.hi) : (step += 2) {
            const c = [2]f64{ best.origin[0] + step * best.dir[0], best.origin[1] + step * best.dir[1] };
            samples += 1;
            const side_free = for ([2]f64{ -4.5, 4.5 }) |off| {
                const px = picture.at(@intFromFloat(@floor(c[0] - off * best.dir[1])), @intFromFloat(@floor(c[1] + off * best.dir[0])));
                if (px != null and px.?[3] >= 128) break false;
            } else true;
            free += @intFromBool(side_free);
        }
        if (2 * free < samples) break;
        var kept: std.ArrayList([2]f64) = .empty;
        var spread: f64 = 0;
        for (points.items) |p| {
            const rx = p[0] - best.origin[0];
            const ry = p[1] - best.origin[1];
            const off = @abs(rx * best.dir[1] - ry * best.dir[0]);
            const t = rx * best.dir[0] + ry * best.dir[1];
            if (off <= 2.5 and t >= best.lo - 1 and t <= best.hi + 1) {
                spread = @max(spread, off);
            } else try kept.append(gpa, p);
        }
        points = kept;
        var ends = [2][2]f64{
            .{ best.origin[0] + best.lo * best.dir[0], best.origin[1] + best.lo * best.dir[1] },
            .{ best.origin[0] + best.hi * best.dir[0], best.origin[1] + best.hi * best.dir[1] },
        };
        if (ends[0][1] < ends[1][1]) std.mem.swap([2]f64, &ends[0], &ends[1]);
        const foot = groundPoint(eye, ends[0][0], ends[0][1]);
        const rf = eye.ray(ends[0][0], ends[0][1]);
        const s = (foot[0] - rf[0][0]) * rf[1][0] + (foot[1] - rf[0][1]) * rf[1][1] + (foot[2] - rf[0][2]) * rf[1][2];
        const rt = eye.ray(ends[1][0], ends[1][1]);
        const top = [3]f64{ rt[0][0] + s * rt[1][0], rt[0][1] + s * rt[1][1], rt[0][2] + s * rt[1][2] };
        try out.append(gpa, .{ .kind = .pole, .pole = .{ .from = foot, .to = top, .radius = @max(0.6, spread * 0.8) } });
    }
    return out.items;
}

/// A line holds at least this many pixels to be a pole.
const pole_pixels = 12;

/// The parts of a scene's intact shape, by kind: the building's blocks, then
/// the chimneys cut from its outline, the low parts and the roof parts (a
/// hipped roof part rising over 100 units and twice its width is a tower or a
/// spire).
fn sceneParts(gpa: std.mem.Allocator, scene: Scene) ![]Part {
    const i = for (scene.states, 0..) |state, i| {
        if (std.mem.eql(u8, state, "1")) break i;
    } else return &.{};
    const shape = scene.shapes[i];
    const body = scene.bodies[i];
    const chimneys_end = @min(shape.len, scene.body.len);
    const roof_start = shape.len - @min(shape.len, scene.roof.len);
    const out = try gpa.alloc(Part, shape.len);
    for (shape, out, 0..) |b, *p, j| {
        const top = @max(b.wall, b.ridge);
        const kind: PartKind = if (j < body) .body else if (j < chimneys_end) .chimney else if (j < roof_start) .low else if (b.roof == .hip and top > 100 and top > 2 * @min(b.hi[0] - b.lo[0], b.hi[1] - b.lo[1])) .tower else .roof_part;
        p.* = .{ .kind = kind, .block = b };
    }
    return out;
}

/// `models`: the shape model of every variant folder given, each family to
/// `<models-dir>/<buildings|objects>/<family>.json`.
fn modelsCommand(ctx: *bk_tool.Context, out_arg: []const u8, ids_arg: []const u8, dir_args: []const []const u8) !void {
    const arena = ctx.arena;
    const out = try bk_tool.inputPath(arena, out_arg);
    const ids = (try readJson(ShapeIds, ctx, try bk_tool.inputPath(arena, ids_arg))) orelse return ctx.failKey("views.err.read", .{ ids_arg, "FileNotFound" });
    const Entry = struct { file: []const u8, text: std.Io.Writer.Allocating };
    var files: std.ArrayList(Entry) = .empty;
    for (dir_args) |dir_arg| {
        const dir = try bk_tool.inputPath(arena, dir_arg);
        // Its path under Data, with forward slashes.
        const slashed = try std.mem.replaceOwned(u8, arena, dir, "\\", "/");
        const at = std.mem.indexOf(u8, slashed, "Buildings/") orelse std.mem.indexOf(u8, slashed, "Objects/") orelse return ctx.failKey("views.err.read", .{ dir, "NotUnderData" });
        const path = std.mem.trimEnd(u8, slashed[at..], "/");
        const building = std.mem.startsWith(u8, path, "Buildings/");
        const id = for (ids.ids) |e| {
            if (std.mem.eql(u8, e.path, path)) break e.id;
        } else return ctx.failKey("views.err.read", .{ path, "NoShapeId" });
        const found = findFamily(arena, ctx.io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
        const model = try buildModel(ctx, found, id, path, building);
        const file = try std.fmt.allocPrint(arena, "{s}/{s}.json", .{ if (building) "buildings" else "objects", found.family.name });
        const entry = for (files.items) |*e| {
            if (std.mem.eql(u8, e.file, file)) break e;
        } else blk: {
            try files.append(arena, .{ .file = file, .text = .init(arena) });
            try files.items[files.items.len - 1].text.writer.writeAll("{");
            break :blk &files.items[files.items.len - 1];
        };
        try entry.text.writer.print("{s}\n\"{s}\": ", .{ if (entry.text.written().len > 1) "," else "", std.fs.path.basename(path) });
        try writeModel(&entry.text.writer, model);
        try ctx.out.print("{d} {s}: {s}, {d} parts, IoU {d:.3}\n", .{ id, path, model.how, model.parts.len, model.fit });
    }
    for (files.items) |*e| {
        try e.text.writer.writeAll("\n}\n");
        const path = try std.fs.path.join(arena, &.{ out, e.file });
        try std.Io.Dir.cwd().createDirPath(ctx.io, std.fs.path.dirname(path).?);
        try writeCrlf(ctx, std.fs.path.dirname(path).?, std.fs.path.basename(path), e.text.written());
    }
}

fn writeModel(w: *std.Io.Writer, m: Model) !void {
    try w.print("{{\"id\": {d}, \"path\": \"{s}\", \"how\": \"{s}\", \"fit\": {d:.3}, \"footprint\": [", .{ m.id, m.path, m.how, m.fit });
    for (m.footprint, 0..) |row, i| try w.print("{s}\"{s}\"", .{ if (i > 0) ", " else "", row });
    try w.writeAll("], \"parts\": [");
    for (m.parts, 0..) |p, i| {
        try w.print("{s}\n  {{\"kind\": \"{s}\"", .{ if (i > 0) "," else "", @tagName(p.kind) });
        if (p.block) |b| try w.print(", \"block\": {{\"lo\": [{d:.1}, {d:.1}], \"hi\": [{d:.1}, {d:.1}], \"wall\": {d:.1}, \"ridge\": {d:.1}, \"roof\": \"{s}\"}}", .{ b.lo[0], b.lo[1], b.hi[0], b.hi[1], b.wall, b.ridge, @tagName(b.roof) });
        if (p.pole) |q| try w.print(", \"pole\": {{\"from\": [{d:.1}, {d:.1}, {d:.1}], \"to\": [{d:.1}, {d:.1}, {d:.1}], \"radius\": {d:.1}}}", .{ q.from[0], q.from[1], q.from[2], q.to[0], q.to[1], q.to[2], q.radius });
        if (p.rect) |r| try w.print(", \"rect\": {{\"on\": {d}, \"face\": \"{s}\", \"lo\": [{d:.1}, {d:.1}], \"hi\": [{d:.1}, {d:.1}]}}", .{ r.on, r.face, r.lo[0], r.lo[1], r.hi[0], r.hi[1] });
        try w.writeAll("}");
    }
    try w.writeAll("]}");
}

/// The model of one variant: its intact blocks with their parts (its hand
/// shape or the fit, as `side` draws it), the windows and doors its 2003
/// picture shows on them, and for an object the poles where they hold its
/// outline better.
fn buildModel(ctx: *bk_tool.Context, found: InFamily, id: u32, path: []const u8, building: bool) !Model {
    const arena = ctx.arena;
    const own_dir = try std.fs.path.join(arena, &.{ found.root, found.family.variants[found.index].path });
    const own = try loadSource(ctx, own_dir, 1);
    const picture = pictureOf(own, "1") orelse return ctx.failKey("views.err.no_san", .{own_dir});
    const eye: Eye = .{ .anchor = .{ @floatFromInt(own.anchor[0]), @floatFromInt(own.anchor[1]) } };
    var parts: []const Part = &.{};
    var how: []const u8 = "none";
    var scene_hand = false;
    if (footprintArea(own.footprint) > 0) {
        if (loadScene(ctx, found, found.index, null, true)) |scene| {
            parts = try sceneParts(arena, scene);
            scene_hand = scene.hand != null;
            how = if (scene_hand) "hand shape" else "fit";
            if (building) parts = try std.mem.concat(arena, Part, &.{ parts, try findOpenings(arena, parts, picture, eye) });
        } else |_| {}
    }
    var fit = try modelIou(arena, parts, eye, picture);
    if (!building and !scene_hand) {
        // A prop is smaller than the 12 units a building's block keeps: its
        // fit, and one box over its footprint (or, with none, under the
        // middle of its outline's foot), refined to its outline with no
        // least size (a barrel, a gravestone, a box).
        const box = occupiedBox(own.footprint.rows);
        const footprint_box: Block = if (box[2] > box[0] and box[3] > box[1]) blk: {
            break :blk .{
                .lo = .{ @as(f64, @floatFromInt(box[0])) * tile_size - own.footprint.origin[0], @as(f64, @floatFromInt(box[1])) * tile_size - own.footprint.origin[1] },
                .hi = .{ @as(f64, @floatFromInt(box[2])) * tile_size - own.footprint.origin[0], @as(f64, @floatFromInt(box[3])) * tile_size - own.footprint.origin[1] },
                .wall = 12,
                .ridge = 12,
            };
        } else blk: {
            var foot = [2]f64{ 0, 0 };
            var lowest: usize = 0;
            var columns: f64 = 0;
            for (0..picture.height) |y| for (0..picture.width) |x| {
                if (picture.pixels[y * picture.width + x][3] < 128) continue;
                if (y > lowest) {
                    lowest = y;
                    foot = .{ 0, 0 };
                    columns = 0;
                }
                if (y == lowest) {
                    foot[0] += @floatFromInt(x);
                    columns += 1;
                }
            };
            const g = groundPoint(eye, foot[0] / @max(1, columns) + 0.5, @as(f64, @floatFromInt(lowest)) - 4);
            break :blk .{ .lo = .{ g[0] - 6, g[1] - 6 }, .hi = .{ g[0] + 6, g[1] + 6 }, .wall = 12, .ridge = 12 };
        };
        var fitted: std.ArrayList(Block) = .empty;
        for (parts) |p| if (p.block) |b| try fitted.append(arena, b);
        for ([2][]const Block{ fitted.items, &.{footprint_box} }) |start| {
            if (start.len == 0 or start.len > max_blocks) continue;
            const refined = try refineToOutline(arena, start, picture, eye);
            const candidate = try arena.alloc(Part, refined.len);
            for (refined, candidate, 0..) |b, *c, j| c.* = .{ .kind = if (start.len == fitted.items.len and j < parts.len) parts[j].kind else .body, .block = b };
            const candidate_fit = try modelIou(arena, candidate, eye, picture);
            if (candidate_fit > fit) {
                parts = candidate;
                fit = candidate_fit;
                how = "fit, refined";
            }
        }
    }
    if (!building and !scene_hand) {
        const poles = try fitPoles(arena, picture, eye);
        const pole_fit = try modelIou(arena, poles, eye, picture);
        if (poles.len > 0 and pole_fit > fit + 0.02) {
            parts = poles;
            fit = pole_fit;
            how = "poles";
        }
    }
    return .{ .id = id, .path = path, .how = how, .fit = fit, .footprint = own.footprint.rows, .parts = parts };
}

/// Blocks moved to a picture's outline by the exact outline IoU of their
/// drawing (`modelIou`): every edge, height, roof and whole move of each
/// block by 8, 4, 2 and 1 units while the IoU grows; a block keeps a unit
/// each way. For props, whose parts are smaller than the fit's sampling.
fn refineToOutline(gpa: std.mem.Allocator, start: []const Block, picture: Image, eye: Eye) ![]Block {
    const blocks = try gpa.dupe(Block, start);
    const parts = try gpa.alloc(Part, blocks.len);
    const sync = struct {
        fn f(ps: []Part, bs: []const Block) void {
            for (ps, bs) |*p, b| p.* = .{ .kind = .body, .block = b };
        }
    }.f;
    sync(parts, blocks);
    var best = try modelIou(gpa, parts, eye, picture);
    for ([_]f64{ 8, 4, 2, 1 }) |step| {
        var rounds: usize = 0;
        var improved = true;
        while (improved and rounds < 30) : (rounds += 1) {
            improved = false;
            for (blocks) |*block| for (0..20) |v| {
                var c = block.*;
                switch (v) {
                    0...3 => c.lo[v % 2] += if (v < 2) step else -step,
                    4...7 => c.hi[v % 2] += if (v < 6) step else -step,
                    8, 9 => c.wall += if (v == 8) step else -step,
                    10, 11 => c.ridge += if (v == 10) step else -step,
                    12...15 => c.roof = @enumFromInt(v - 12),
                    else => {
                        const d = if (v % 2 == 0) step else -step;
                        c.lo[(v - 16) / 2] += d;
                        c.hi[(v - 16) / 2] += d;
                    },
                }
                if (v >= 12 and v < 16 and c.roof == block.roof) continue;
                if (c.hi[0] - c.lo[0] < 1 or c.hi[1] - c.lo[1] < 1 or c.wall < 1) continue;
                if (c.roof == .flat) c.ridge = c.wall;
                if (c.ridge < c.wall) continue;
                const before = block.*;
                block.* = c;
                sync(parts, blocks);
                const iou = try modelIou(gpa, parts, eye, picture);
                if (iou > best + 1e-9) {
                    best = iou;
                    improved = true;
                } else block.* = before;
            };
        }
    }
    return blocks;
}

/// Windows and doors on the walls of a building's body that its 2003 picture
/// shows: on each wall seen squarely (cosine 0.3 or more), the wall's points
/// on a grid of one world unit sampled from the picture where nothing stands
/// in front; points darker than 0.7 of the wall's median brightness grouped
/// 4-connected, each group whose box is 4 to 20 units wide and 5 to 26 high,
/// between 2:5 and 5:2, and three fifths dark or more (half-timbering's long
/// thin beams are not); one from within 3 units of the ground and 10 high is
/// a door.
fn findOpenings(gpa: std.mem.Allocator, parts: []const Part, picture: Image, eye: Eye) ![]Part {
    var blocks: std.ArrayList(Block) = .empty;
    for (parts) |p| if (p.block) |b| try blocks.append(gpa, b);
    const d = eye.ray(0, 0)[1];
    var out: std.ArrayList(Part) = .empty;
    for (parts, 0..) |p, index| {
        if (p.kind != .body) continue;
        const b = p.block.?;
        for ([_][]const u8{ "wall -y", "wall +x", "wall +y", "wall -x" }) |name| {
            const n = faceDirection(name);
            if (-(n[0] * d[0] + n[1] * d[1]) < 0.3) continue;
            const right = [2]f64{ -n[1], n[0] };
            const start = faceStart(b, n);
            const across: usize = @intFromFloat(@floor(if (n[0] != 0) b.hi[1] - b.lo[1] else b.hi[0] - b.lo[0]));
            const up: usize = @intFromFloat(@floor(b.wall));
            if (across < 4 or up < 4) continue;
            // Each point's brightness, -1 where the picture does not show it.
            const level = try gpa.alloc(f64, across * up);
            var seen: std.ArrayList(f64) = .empty;
            for (0..up) |v| for (0..across) |u| {
                const fu = @as(f64, @floatFromInt(u)) + 0.5;
                const p3 = [3]f64{ start[0] + right[0] * fu + n[0] * 0.01, start[1] + right[1] * fu + n[1] * 0.01, @as(f64, @floatFromInt(v)) + 0.5 };
                level[v * across + u] = -1;
                if (!sees(blocks.items, eye, p3)) continue;
                const s = eye.project(p3);
                const px = picture.at(@intFromFloat(@floor(s[0])), @intFromFloat(@floor(s[1]))) orelse continue;
                if (px[3] < 128) continue;
                const l = 0.3 * @as(f64, @floatFromInt(px[0])) + 0.59 * @as(f64, @floatFromInt(px[1])) + 0.11 * @as(f64, @floatFromInt(px[2]));
                level[v * across + u] = l;
                try seen.append(gpa, l);
            };
            if (seen.items.len < 40) continue;
            std.mem.sort(f64, seen.items, {}, std.sort.asc(f64));
            const dark = 0.7 * seen.items[seen.items.len / 2];
            const visited = try gpa.alloc(bool, level.len);
            @memset(visited, false);
            var stack: std.ArrayList(usize) = .empty;
            for (0..level.len) |seed| {
                if (visited[seed] or level[seed] < 0 or level[seed] >= dark) continue;
                var box = [4]usize{ across, up, 0, 0 };
                var count: usize = 0;
                visited[seed] = true;
                try stack.append(gpa, seed);
                while (stack.pop()) |i| {
                    const u = i % across;
                    const v = i / across;
                    box = .{ @min(box[0], u), @min(box[1], v), @max(box[2], u + 1), @max(box[3], v + 1) };
                    count += 1;
                    for ([_][2]i64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 } }) |step| {
                        const nu = @as(i64, @intCast(u)) + step[0];
                        const nv = @as(i64, @intCast(v)) + step[1];
                        if (nu < 0 or nv < 0 or nu >= across or nv >= up) continue;
                        const k: usize = @intCast(nv * @as(i64, @intCast(across)) + nu);
                        if (visited[k] or level[k] < 0 or level[k] >= dark) continue;
                        visited[k] = true;
                        try stack.append(gpa, k);
                    }
                }
                const w = box[2] - box[0];
                const h = box[3] - box[1];
                // A window or door is a compact dark box (timber framing is long and thin).
                if (w < 4 or w > 20 or h < 5 or h > 26 or 5 * w < 2 * h or 2 * w > 5 * h or 5 * count < 3 * w * h) continue;
                const door = box[1] <= 3 and h >= 10;
                try out.append(gpa, .{ .kind = if (door) .door else .window, .rect = .{ .on = index, .face = name, .lo = .{ @floatFromInt(box[0]), @floatFromInt(box[1]) }, .hi = .{ @floatFromInt(box[2]), @floatFromInt(box[3]) } } });
            }
        }
    }
    return out.items;
}

/// The four sides a model is drawn from, by turn: N is turn 0 (the 2003
/// picture's side), E, S and W the turns one, two and three presses of the
/// Game's turn key (`.`, camera step 1, 2, 3) show.
pub const side_letters = [4]u8{ 'N', 'E', 'S', 'W' };

const kind_fill = std.enums.EnumArray(PartKind, [3]f64).init(.{
    .body = .{ 0.90, 0.86, 0.76 },
    .chimney = .{ 0.80, 0.42, 0.32 },
    .low = .{ 0.62, 0.82, 0.58 },
    .roof_part = .{ 0.58, 0.72, 0.92 },
    .tower = .{ 0.74, 0.62, 0.90 },
    .pole = .{ 0.96, 0.70, 0.30 },
    .window = .{ 0.30, 0.70, 0.90 },
    .door = .{ 0.55, 0.35, 0.62 },
});

/// A face's fill: its kind's colour, roof planes of the body in tile red.
fn faceFill(f: ModelFace) [3]f64 {
    if (f.kind == .body and f.roof) return .{ 0.82, 0.55, 0.45 };
    return kind_fill.get(f.kind);
}

const ModelFile = std.json.ArrayHashMap(Model);

/// `review`: the review document of every numbered object (`shape_ids_file`)
/// whose model `models-dir` holds: a contents page, then a page each with its
/// 2003 picture, the model's turn 0 drawn over it, and the model turned to
/// the four sides as vector drawings labelled `<number>.N` to `<number>.W`.
fn reviewCommand(ctx: *bk_tool.Context, out_arg: []const u8, ids_arg: []const u8, models_arg: []const u8, buildings_arg: []const u8, objects_arg: []const u8) !void {
    const arena = ctx.arena;
    const out = try bk_tool.inputPath(arena, out_arg);
    const ids = (try readJson(ShapeIds, ctx, try bk_tool.inputPath(arena, ids_arg))) orelse return ctx.failKey("views.err.read", .{ ids_arg, "FileNotFound" });
    const models = try bk_tool.inputPath(arena, models_arg);
    const roots = [2][]const u8{ try bk_tool.inputPath(arena, buildings_arg), try bk_tool.inputPath(arena, objects_arg) };
    // Each object's family, as `models` files it.
    var families: std.StringHashMap([]const u8) = .init(arena);
    for (roots, [2][]const u8{ "Buildings", "Objects/SimpleObjects" }) |root, prefix| {
        for (try listFamilies(arena, ctx.io, root)) |family| for (family.variants) |v| {
            try families.put(try std.fmt.allocPrint(arena, "{s}/{s}", .{ prefix, v.path }), family.name);
        };
    }
    // The meshes (`mesh.meshes_dir`) win over the block models where present.
    const Entry = struct { id: u32, path: []const u8, model: ?Model, mesh: ?mesh.Model };
    var entries_list: std.ArrayList(Entry) = .empty;
    var files: std.StringHashMap(?ModelFile) = .init(arena);
    for (ids.ids) |e| {
        const building = std.mem.startsWith(u8, e.path, "Buildings/");
        const found = try mesh.load(arena, ctx.io, mesh.meshes_dir, e.id);
        const family = families.get(e.path) orelse {
            if (found != null) try entries_list.append(arena, .{ .id = e.id, .path = e.path, .model = null, .mesh = found });
            continue;
        };
        const file = try std.fmt.allocPrint(arena, "{s}/{s}/{s}.json", .{ models, if (building) "buildings" else "objects", family });
        const loaded = files.get(file) orelse blk: {
            const m = try readJson(ModelFile, ctx, file);
            try files.put(file, m);
            break :blk m;
        };
        const model = if (loaded) |m| m.map.get(std.fs.path.basename(e.path)) else null;
        try entries_list.append(arena, .{ .id = e.id, .path = e.path, .model = model, .mesh = found });
    }
    std.mem.sort(Entry, entries_list.items, {}, struct {
        fn less(_: void, a: Entry, b: Entry) bool {
            return a.id < b.id;
        }
    }.less);
    const items = entries_list.items;

    const page_w = 842.0;
    const page_h = 595.0;
    var pdf = try bk_pdf.begin(arena);
    // Contents: the convention, the colours, then two columns of entries.
    const per_column = 46;
    const first_rows = 26;
    const contents_pages: usize = if (items.len <= 2 * first_rows) 1 else 1 + (items.len - 2 * first_rows + 2 * per_column - 1) / (2 * per_column);
    var at: usize = 0;
    for (0..contents_pages) |cp| {
        var page: bk_pdf.Page = .init(arena);
        var top: f64 = page_h - 40;
        if (cp == 0) {
            try page.text(30, top, 20, true, .{ 0, 0, 0 }, "Blitzkrieg shapes review: every object that turns, as a mesh model");
            top -= 22;
            const lines = [_][]const u8{
                "Each object has a number; its four drawings are <number>.N, .E, .S, .W. N is turn 0, the side its 2003 picture shows.",
                "E, S and W are the turns one, two and three presses of the Game's turn key (period: camera step 1, 2, 3) show.",
                "So '33.S' is object 33 seen after two presses. The numbers are kept in tools/zig/fixtures/bk-views/shape-ids.json, the meshes",
                "(the maintainer's family review, bk-views import) in tools/zig/fixtures/bk-views/meshes; * marks a mesh that needs review.",
                "Each page: the 2003 picture, the mesh's edges at N over it (outline and edge scores: the review's and measured here), the",
                "provenance, and the four turns drawn with the 2003 camera (30 degrees down, 45 degrees across). The family table and the",
                "joint families' comparison pages follow the objects. Objects without a mesh show their block model, parts coloured by kind:",
            };
            for (lines) |l| {
                try page.text(30, top, 9.5, false, .{ 0, 0, 0 }, l);
                top -= 12;
            }
            var x: f64 = 30;
            for (std.enums.values(PartKind)) |k| {
                try page.polygon(&.{ .{ x, top - 2 }, .{ x + 14, top - 2 }, .{ x + 14, top + 8 }, .{ x, top + 8 } }, kind_fill.get(k), .{ 0.3, 0.3, 0.3 }, 0.5);
                try page.text(x + 18, top, 9.5, false, .{ 0, 0, 0 }, if (k == .body) "walls" else if (k == .roof_part) "roof part (dormer, vent, crate)" else if (k == .low) "low part (ramp, step, porch)" else @tagName(k));
                x += if (k == .roof_part) 170 else if (k == .low) 150 else 70;
            }
            try page.polygon(&.{ .{ x, top - 2 }, .{ x + 14, top - 2 }, .{ x + 14, top + 8 }, .{ x, top + 8 } }, .{ 0.82, 0.55, 0.45 }, .{ 0.3, 0.3, 0.3 }, 0.5);
            try page.text(x + 18, top, 9.5, false, .{ 0, 0, 0 }, "roof");
            top -= 22;
        }
        const rows: usize = if (cp == 0) first_rows else per_column;
        for (0..2) |column| for (0..rows) |row| {
            if (at == items.len) break;
            const e = items[at];
            const y = top - @as(f64, @floatFromInt(row)) * 11.5;
            const x = 30 + @as(f64, @floatFromInt(column)) * 400;
            try page.text(x, y, 9, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}", .{e.id}));
            const how = if (e.mesh) |m| try std.fmt.allocPrint(arena, "{s}{s}", .{ m.status, if (m.needs_review) " *" else "" }) else if (e.model) |m| m.how else "no model";
            try page.text(x + 32, y, if (e.mesh != null) 8 else 9, false, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{s}  {s}", .{ e.path, how }));
            try page.text(x + 360, y, 9, false, .{ 0.3, 0.3, 0.3 }, try std.fmt.allocPrint(arena, "p. {d}", .{contents_pages + at + 1}));
            at += 1;
        };
        _ = try pdf.page(page_w, page_h, page.out.written(), &.{});
    }
    try pdf.bookmark("Contents", 0);
    var matches: std.ArrayList(Match) = .empty;
    for (items) |e| {
        var page: bk_pdf.Page = .init(arena);
        var images: std.ArrayList(u32) = .empty;
        if (e.mesh) |m| {
            try matches.append(arena, try meshPage(ctx, &pdf, &page, &images, roots, e.id, e.path, m));
            const index = try pdf.page(page_w, page_h, page.out.written(), images.items);
            try pdf.bookmark(try std.fmt.allocPrint(arena, "{d} {s}", .{ e.id, e.path }), index);
            continue;
        }
        const relative =if (std.mem.startsWith(u8, e.path, "Objects/SimpleObjects/")) e.path["Objects/SimpleObjects/".len..] else e.path["Buildings/".len..];
        const dir = try std.fs.path.join(arena, &.{ if (std.mem.startsWith(u8, e.path, "Buildings/")) roots[0] else roots[1], relative });
        try page.text(30, page_h - 42, 26, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}", .{e.id}));
        try page.text(100, page_h - 34, 13, true, .{ 0, 0, 0 }, e.path);
        const model = e.model orelse {
            try page.text(100, page_h - 50, 10, false, .{ 0.6, 0, 0 }, "no model");
            _ = try pdf.page(page_w, page_h, page.out.written(), &.{});
            continue;
        };
        try page.text(100, page_h - 50, 10, false, .{ 0.2, 0.2, 0.2 }, try std.fmt.allocPrint(arena, "{s}  |  fit (outline IoU at N) {d:.3}  |  footprint {d} x {d} cells  |  {d} parts", .{ model.how, model.fit, if (model.footprint.len > 0) model.footprint[0].len / 2 else 0, model.footprint.len, model.parts.len }));
        const faces = try modelFaces(arena, model.parts);
        // The 2003 picture, alone and with the model's turn 0 over it.
        const source = loadSource(ctx, dir, 1) catch null;
        const box = [2]f64{ 250, 215 };
        const box_y = 300.0;
        if (source) |s| if (pictureOf(s, "1")) |picture| {
            const im = try pdf.image(picture.width, picture.height, picture.pixels);
            try images.append(arena, im);
            const scale = @min(@min(box[0] / @as(f64, @floatFromInt(picture.width)), box[1] / @as(f64, @floatFromInt(picture.height))), 3.0);
            const w = scale * @as(f64, @floatFromInt(picture.width));
            const h = scale * @as(f64, @floatFromInt(picture.height));
            const eye: Eye = .{ .anchor = .{ @floatFromInt(s.anchor[0]), @floatFromInt(s.anchor[1]) } };
            for ([2]f64{ 30, 300 }, 0..) |x0, k| {
                const ox = x0 + (box[0] - w) / 2;
                const oy = box_y + (box[1] - h) / 2;
                try page.polygon(&.{ .{ ox, oy }, .{ ox + w, oy }, .{ ox + w, oy + h }, .{ ox, oy + h } }, .{ 0.80, 0.83, 0.88 }, null, 0);
                try page.image(im, ox, oy, w, h);
                if (k == 0) {
                    try page.text(x0, box_y + box[1] + 6, 9, false, .{ 0.2, 0.2, 0.2 }, "2003 picture");
                    continue;
                }
                try page.text(x0, box_y + box[1] + 6, 9, false, .{ 0.2, 0.2, 0.2 }, "model at N over it");
                for (try seenFaces(arena, faces, eye)) |f| {
                    const pts = try arena.alloc([2]f64, f.points.len);
                    for (f.points, pts) |q, *r| r.* = .{ ox + q[0] * scale, oy + h - q[1] * scale };
                    const c = faceFill(f.face);
                    try page.polygon(pts, null, .{ c[0] * 0.55, c[1] * 0.55, c[2] * 0.55 }, 0.8);
                }
            }
        };
        // The parts list.
        var y: f64 = box_y + box[1] - 4;
        try page.text(580, y + 10, 9, true, .{ 0, 0, 0 }, "parts (lo, hi, wall, ridge in world units)");
        for (model.parts, 0..) |p, i| {
            if (y < box_y - 10) {
                try page.text(580, y, 8, false, .{ 0.3, 0.3, 0.3 }, try std.fmt.allocPrint(arena, "... {d} more", .{model.parts.len - i}));
                break;
            }
            const line = if (p.block) |b| try std.fmt.allocPrint(arena, "p{d} {s} {s} ({d:.0},{d:.0})-({d:.0},{d:.0}) {d:.0}/{d:.0}", .{ i + 1, @tagName(p.kind), @tagName(b.roof), b.lo[0], b.lo[1], b.hi[0], b.hi[1], b.wall, b.ridge }) else if (p.pole) |q| try std.fmt.allocPrint(arena, "p{d} pole ({d:.0},{d:.0},{d:.0})-({d:.0},{d:.0},{d:.0}) r {d:.1}", .{ i + 1, q.from[0], q.from[1], q.from[2], q.to[0], q.to[1], q.to[2], q.radius }) else if (p.rect) |r| try std.fmt.allocPrint(arena, "p{d} {s} on p{d} {s} {d:.0}..{d:.0} x {d:.0}..{d:.0}", .{ i + 1, @tagName(p.kind), r.on + 1, r.face, r.lo[0], r.hi[0], r.lo[1], r.hi[1] }) else "";
            const c = kind_fill.get(p.kind);
            try page.polygon(&.{ .{ 580, y - 1 }, .{ 587, y - 1 }, .{ 587, y + 6 }, .{ 580, y + 6 } }, c, null, 0);
            try page.text(591, y, 7.5, false, .{ 0, 0, 0 }, line);
            y -= 9;
        }
        // The four turns at one scale.
        var turned: [4][]SeenFace = undefined;
        var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
        var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
        var bounds: [4][4]f64 = undefined;
        for (0..4) |k| {
            turned[k] = try seenFaces(arena, faces, .{ .anchor = .{ 0, 0 }, .turn = @intCast(k) });
            bounds[k] = .{ std.math.inf(f64), std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
            for (turned[k]) |f| for (f.points) |q| {
                bounds[k] = .{ @min(bounds[k][0], q[0]), @min(bounds[k][1], q[1]), @max(bounds[k][2], q[0]), @max(bounds[k][3], q[1]) };
            };
            lo = .{ @min(lo[0], bounds[k][2] - bounds[k][0]), 0 };
            hi = .{ @max(hi[0], bounds[k][2] - bounds[k][0]), @max(hi[1], bounds[k][3] - bounds[k][1]) };
        }
        const cell = [2]f64{ 190, 220 };
        const scale = if (hi[0] > 0 and hi[1] > 0) @min(@min(cell[0] / hi[0], (cell[1] - 34) / hi[1]), 3.0) else 1;
        for (0..4) |k| {
            const x0 = 30 + @as(f64, @floatFromInt(k)) * 200;
            const y0 = 30.0;
            try page.polygon(&.{ .{ x0, y0 }, .{ x0 + cell[0], y0 }, .{ x0 + cell[0], y0 + cell[1] }, .{ x0, y0 + cell[1] } }, .{ 0.97, 0.97, 0.97 }, .{ 0.75, 0.75, 0.75 }, 0.5);
            const label = try std.fmt.allocPrint(arena, "{d}.{c}", .{ e.id, side_letters[k] });
            defer {
                // The label over the drawing, on a plate of its own.
                page.polygon(&.{ .{ x0 + 2, y0 + cell[1] - 30 }, .{ x0 + 18 + 13 * @as(f64, @floatFromInt(label.len)), y0 + cell[1] - 30 }, .{ x0 + 18 + 13 * @as(f64, @floatFromInt(label.len)), y0 + cell[1] - 2 }, .{ x0 + 2, y0 + cell[1] - 2 } }, .{ 1, 1, 1 }, null, 0) catch {};
                page.text(x0 + 6, y0 + cell[1] - 26, 22, true, .{ 0, 0, 0 }, label) catch {};
                page.text(x0 + 6, y0 + 6, 7.5, false, .{ 0.35, 0.35, 0.35 }, if (k == 0) "turn 0: the 2003 side" else std.fmt.allocPrint(arena, "turn {d}: {d} press{s} of '.'", .{ k, k, if (k == 1) "" else "es" }) catch "") catch {};
            }
            if (bounds[k][0] > bounds[k][2]) continue;
            const cx = (bounds[k][0] + bounds[k][2]) / 2;
            const cy = (bounds[k][1] + bounds[k][3]) / 2;
            const mx = x0 + cell[0] / 2;
            const my = y0 + (cell[1] - 30) / 2 + 6;
            var label_at = std.AutoHashMap(usize, [2]f64).init(arena);
            for (turned[k]) |f| {
                const pts = try arena.alloc([2]f64, f.points.len);
                for (f.points, pts) |q, *r| {
                    r.* = .{ mx + (q[0] - cx) * scale, my - (q[1] - cy) * scale };
                    if (f.face.kind != .window and f.face.kind != .door) {
                        const g = try label_at.getOrPutValue(f.face.part, r.*);
                        if (r[1] > g.value_ptr[1]) g.value_ptr.* = r.*;
                    }
                }
                try page.polygon(pts, faceFill(f.face), .{ 0.25, 0.25, 0.25 }, 0.45);
            }
            // Each part's number at its top.
            if (model.parts.len <= 40) {
                var it = label_at.iterator();
                while (it.next()) |l| try page.text(l.value_ptr[0] - 3, l.value_ptr[1] + 2, 6, false, .{ 0.1, 0.1, 0.5 }, try std.fmt.allocPrint(arena, "p{d}", .{l.key_ptr.* + 1}));
            }
        }
        const index = try pdf.page(page_w, page_h, page.out.written(), images.items);
        try pdf.bookmark(try std.fmt.allocPrint(arena, "{d} {s}", .{ e.id, e.path }), index);
    }
    // The family table and each joint family's comparison page.
    const mesh_families = try mesh.loadFamilies(arena, ctx.io, mesh.meshes_dir);
    if (mesh_families.len > 0) {
        try pdf.bookmark("Families", @intCast(pdf.pages.items.len));
        try familyTable(arena, &pdf, mesh_families);
    }
    for (mesh_families) |f| {
        if (!std.mem.eql(u8, f.status, "joint candidate") or f.views.len == 0) continue;
        var page: bk_pdf.Page = .init(arena);
        var images: std.ArrayList(u32) = .empty;
        try familyPage(ctx, &pdf, &page, &images, roots, ids, f);
        const index = try pdf.page(page_w, page_h, page.out.written(), images.items);
        try pdf.bookmark(try std.fmt.allocPrint(arena, "Joint family {s}/{s}", .{ f.region, f.family }), index);
    }
    var text: std.Io.Writer.Allocating = .init(arena);
    try pdf.write(&text.writer);
    const out_dir = std.fs.path.dirname(out) orelse ".";
    try std.Io.Dir.cwd().createDirPath(ctx.io, out_dir);
    try writeOut(ctx, out_dir, std.fs.path.basename(out), text.written());
    try ctx.out.print("wrote {s}: {d} objects on {d} pages\n", .{ out, items.len, pdf.pages.items.len });
    if (matches.items.len > 0) try writeMatches(ctx, out_dir, matches.items);
}

/// How far the outline IoU and the edge score measured here may lie from the
/// maintainer's review's own (edgefit.py on OpenCV and Pillow) and still match.
pub const match_tolerance = [2]f64{ 0.01, 0.03 };

/// shapes-match.csv beside the review: per object the review's and the
/// measured scores and whether they match within `match_tolerance`, and the
/// summary on stdout.
fn writeMatches(ctx: *bk_tool.Context, dir: []const u8, matches: []const Match) !void {
    var csv: std.Io.Writer.Allocating = .init(ctx.arena);
    const w = &csv.writer;
    try w.writeAll("id,path,review_outline,measured_outline,review_edge,measured_edge,match\n");
    var compared: usize = 0;
    var mismatched: usize = 0;
    var worst = [2]f64{ 0, 0 };
    var sum = [2]f64{ 0, 0 };
    for (matches) |m| {
        const r = m.review orelse {
            try w.print("{d},{s},,,,,{s}\n", .{ m.id, m.path, if (m.measured == null) "no picture" else "no review score" });
            continue;
        };
        const s = m.measured orelse {
            try w.print("{d},{s},{d:.4},,{d:.4},,no picture\n", .{ m.id, m.path, r.silhouette, r.edge });
            mismatched += 1;
            continue;
        };
        const d = [2]f64{ @abs(s.silhouette - r.silhouette), @abs(s.edge - r.edge) };
        const ok = d[0] <= match_tolerance[0] and d[1] <= match_tolerance[1];
        compared += 1;
        mismatched += @intFromBool(!ok);
        for (0..2) |i| {
            worst[i] = @max(worst[i], d[i]);
            sum[i] += d[i];
        }
        try w.print("{d},{s},{d:.4},{d:.4},{d:.4},{d:.4},{s}\n", .{ m.id, m.path, r.silhouette, s.silhouette, r.edge, s.edge, if (ok) "yes" else "NO" });
    }
    try writeOut(ctx, dir, "shapes-match.csv", csv.written());
    const n: f64 = @floatFromInt(@max(1, compared));
    try ctx.out.print("match against the review: {d} objects compared, {d} outside the tolerance ({d} outline, {d} edge); outline difference mean {d:.4} max {d:.4}, edge mean {d:.4} max {d:.4}\n", .{ compared, mismatched, match_tolerance[0], match_tolerance[1], sum[0] / n, worst[0], sum[1] / n, worst[1] });
}

// ---------------------------------------------------------------------------
// Meshes (M006 round 5): the maintainer's reviewed shapes (bk_views_mesh.zig)

pub const mesh = @import("bk_views_mesh.zig");

fn jsonNumber(v: ?std.json.Value) ?f64 {
    return switch (v orelse return null) {
        .integer => |i| @floatFromInt(i),
        .float => |f| f,
        .number_string => |s| std.fmt.parseFloat(f64, s) catch null,
        else => null,
    };
}

fn jsonString(v: ?std.json.Value) []const u8 {
    return switch (v orelse return "") {
        .string => |s| s,
        else => "",
    };
}

/// A coordinate kept to `decimals` places, as an integer of that unit.
fn quantize(v: f64, decimals: u3) i64 {
    return @intFromFloat(@round(v * std.math.pow(f64, 10, @floatFromInt(decimals))));
}

/// An integer of 10^-decimals units as the shortest decimal.
fn writeFixed(w: *std.Io.Writer, q: i64, decimals: u3) !void {
    const unit = std.math.pow(u64, 10, decimals);
    const a = @abs(q);
    if (q < 0) try w.writeByte('-');
    try w.print("{d}", .{a / unit});
    var frac = a % unit;
    if (frac == 0) return;
    var digits: usize = decimals;
    while (frac % 10 == 0) : (digits -= 1) frac /= 10;
    try w.print(".{d:0>[1]}", .{ frac, digits });
}

/// A mesh as tracked: vertices kept to a precision of a 5000th of its
/// longest extent (1 to 5 decimals), those at one place merged, and the
/// triangles grouped by kind in the order the kinds first appear.
fn writeMeshData(gpa: std.mem.Allocator, w: *std.Io.Writer, vertices: []const std.json.Value, triangles: []const std.json.Value, kinds: []const std.json.Value) !void {
    var lo = [3]f64{ std.math.inf(f64), std.math.inf(f64), std.math.inf(f64) };
    var hi = [3]f64{ -std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
    const points = try gpa.alloc([3]f64, vertices.len);
    for (vertices, points) |v, *p| {
        for (0..3) |c| p[c] = jsonNumber(v.array.items[c]) orelse 0;
        for (0..3) |c| {
            lo[c] = @min(lo[c], p[c]);
            hi[c] = @max(hi[c], p[c]);
        }
    }
    const extent = @max(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1e-6);
    const decimals: u3 = @intFromFloat(std.math.clamp(@ceil(@log10(5000 / extent)), 1, 5));
    var index: std.array_hash_map.Auto([3]i64, u32) = .empty;
    const remap = try gpa.alloc(u32, points.len);
    for (points, remap) |p, *r| {
        const g = try index.getOrPut(gpa, .{ quantize(p[0], decimals), quantize(p[1], decimals), quantize(p[2], decimals) });
        if (!g.found_existing) g.value_ptr.* = @intCast(index.count() - 1);
        r.* = g.value_ptr.*;
    }
    try w.writeAll("\"vertices\":[");
    for (index.keys(), 0..) |k, i| {
        try w.writeAll(if (i == 0) "\n" else if (i % 6 == 0) ",\n" else ",");
        for (k, 0..) |q, c| {
            if (c > 0) try w.writeByte(',');
            try writeFixed(w, q, decimals);
        }
    }
    try w.writeAll("],\n\"parts\":[");
    var names: std.ArrayList([]const u8) = .empty;
    for (kinds) |k| {
        const name = jsonString(k);
        for (names.items) |n| {
            if (std.mem.eql(u8, n, name)) break;
        } else try names.append(gpa, name);
    }
    for (names.items, 0..) |name, n| {
        try w.print("{s}\n{{\"kind\":\"{s}\",\"triangles\":[", .{ if (n > 0) "," else "", name });
        var count: usize = 0;
        for (triangles, kinds) |t, k| {
            if (!std.mem.eql(u8, jsonString(k), name)) continue;
            try w.writeAll(if (count == 0) "\n" else if (count % 10 == 0) ",\n" else ",");
            for (t.array.items[0..3], 0..) |v, c| try w.print("{s}{d}", .{ if (c > 0) "," else "", remap[@intCast(v.integer)] });
            count += 1;
        }
        try w.writeAll("]}");
    }
    try w.writeAll("]");
}

fn writeAlignment(w: *std.Io.Writer, a: ?std.json.Value) !void {
    const o = if (a) |v| v.object else return w.writeAll("{\"angle\":0,\"zscale\":1,\"ratio\":1}");
    try w.print("{{\"angle\":{d:.4},\"zscale\":{d:.4},\"ratio\":{d:.4}}}", .{ jsonNumber(o.get("angle")) orelse 0, jsonNumber(o.get("zscale")) orelse 1, jsonNumber(o.get("ratio")) orelse 1 });
}

fn writeIds(w: *std.Io.Writer, v: ?std.json.Value) !void {
    try w.writeByte('[');
    if (v) |a| for (a.array.items, 0..) |x, i| try w.print("{s}{d}", .{ if (i > 0) "," else "", x.integer });
    try w.writeByte(']');
}

/// A path of the review's folder as the tracked meshes name their source:
/// relative to the manifest's folder, forward slashes.
fn reviewRelative(gpa: std.mem.Allocator, root: []const u8, path: []const u8) ![]const u8 {
    const slashed = try std.mem.replaceOwned(u8, gpa, path, "\\", "/");
    const base = try std.mem.replaceOwned(u8, gpa, root, "\\", "/");
    if (slashed.len > base.len and std.ascii.startsWithIgnoreCase(slashed, base) and slashed[base.len] == '/') return slashed[base.len + 1 ..];
    return slashed;
}

fn parseJsonFile(ctx: *bk_tool.Context, path: []const u8) !std.json.Value {
    const bytes = try readFile(ctx, path);
    return std.json.parseFromSliceLeaky(std.json.Value, ctx.arena, bytes, .{}) catch |err| ctx.failKey("views.err.read", .{ path, @errorName(err) });
}

/// `import`: the maintainer's family review (shapes-family-review-manifest.json:
/// every object's final model, which may lie in any layer of the review, and
/// the family table) into `<meshes-dir>`: `<id>.json` per object, the joint
/// families' shared meshes in `shared/`, and `families.json`.
fn importCommand(ctx: *bk_tool.Context, manifest_arg: []const u8, out_arg: []const u8) !void {
    const arena = ctx.arena;
    const io = ctx.io;
    const manifest_path = try bk_tool.inputPath(arena, manifest_arg);
    const out = try bk_tool.inputPath(arena, out_arg);
    const root = std.fs.path.dirname(manifest_path) orelse ".";
    const manifest = try parseJsonFile(ctx, manifest_path);
    const objects = manifest.object.get("objects").?.array.items;
    const families = manifest.object.get("families").?.array.items;
    try std.Io.Dir.cwd().createDirPath(io, try std.fs.path.join(arena, &.{ out, "shared" }));

    // The family table, and each object's family.
    var family_of: std.AutoHashMap(i64, []const u8) = .init(arena);
    var text: std.Io.Writer.Allocating = .init(arena);
    var w = &text.writer;
    try w.writeAll("{\"families\":[");
    var shared_count: usize = 0;
    for (families, 0..) |fv, i| {
        const f = fv.object;
        const name = try std.fmt.allocPrint(arena, "{s}/{s}", .{ jsonString(f.get("region")), jsonString(f.get("family")) });
        for (f.get("all_ids").?.array.items) |id| try family_of.put(id.integer, name);
        const shared_path = jsonString(f.get("shared_model"));
        const shared = if (shared_path.len > 0) std.fs.path.stem(try std.mem.replaceOwned(u8, arena, shared_path, "\\", "/")) else "";
        try w.print("{s}\n{{\"region\":\"{s}\",\"family\":\"{s}\",\"status\":\"{s}\",\"reason\":", .{ if (i > 0) "," else "", jsonString(f.get("region")), jsonString(f.get("family")), jsonString(f.get("status")) });
        try std.json.Stringify.encodeJsonString(jsonString(f.get("reason")), .{}, w);
        try w.writeAll(",\"all_ids\":");
        try writeIds(w, f.get("all_ids"));
        try w.writeAll(",\"included\":");
        try writeIds(w, f.get("included"));
        try w.writeAll(",\"excluded\":[");
        for (f.get("excluded").?.array.items, 0..) |e, j| {
            try w.print("{s}{{\"id\":{d},\"reason\":", .{ if (j > 0) "," else "", e.object.get("id").?.integer });
            try std.json.Stringify.encodeJsonString(jsonString(e.object.get("reason")), .{}, w);
            try w.writeAll("}");
        }
        try w.writeAll("]");
        if (f.get("donor_id")) |d| if (d == .integer) try w.print(",\"donor_id\":{d}", .{d.integer});
        if (jsonNumber(f.get("objective"))) |o| try w.print(",\"objective\":{d:.4}", .{o});
        try w.print(",\"shared\":\"{s}\",\"geometry_note\":", .{shared});
        try std.json.Stringify.encodeJsonString(jsonString(f.get("geometry_note")), .{}, w);
        try w.writeAll(",\"views\":[");
        if (f.get("views")) |views_value| for (views_value.array.items, 0..) |vv, j| {
            const v = vv.object;
            try w.print("{s}\n {{\"id\":{d},\"side\":{d},\"method\":\"{s}\",\"alignment\":", .{ if (j > 0) "," else "", v.get("id").?.integer, v.get("side").?.integer, jsonString(v.get("method")) });
            try writeAlignment(w, v.get("alignment"));
            try w.print(",\"silhouette\":{d:.4},\"edge\":{d:.4},\"combined\":{d:.4}}}", .{ jsonNumber(v.get("silhouette")) orelse 0, jsonNumber(v.get("edge")) orelse 0, jsonNumber(v.get("joint_score")) orelse 0 });
        };
        try w.writeAll("]}");
        if (shared.len == 0) continue;
        // The mesh the family's views share.
        const sm = (try parseJsonFile(ctx, shared_path)).object;
        var mtext: std.Io.Writer.Allocating = .init(arena);
        try mtext.writer.print("{{\"name\":\"{s}\",\"views\":", .{shared});
        try writeIds(&mtext.writer, sm.get("source_views"));
        try mtext.writer.writeAll(",\n");
        try writeMeshData(arena, &mtext.writer, sm.get("vertices").?.array.items, sm.get("triangles").?.array.items, sm.get("face_kinds").?.array.items);
        try mtext.writer.writeAll("}\n");
        try writeCrlf(ctx, try std.fs.path.join(arena, &.{ out, "shared" }), try std.fmt.allocPrint(arena, "{s}.json", .{shared}), mtext.written());
        shared_count += 1;
    }
    try w.writeAll("\n]}\n");
    try writeCrlf(ctx, out, "families.json", text.written());

    // Every object's final model.
    var vertices_total: usize = 0;
    for (objects) |ov| {
        const o = ov.object;
        const id = o.get("id").?.integer;
        const model_path = jsonString(o.get("model"));
        const m = (try parseJsonFile(ctx, model_path)).object;
        const joint = if (o.get("joint_candidate")) |j| j == .bool and j.bool else false;
        text = .init(arena);
        w = &text.writer;
        try w.print("{{\"id\":{d},\"path\":\"{s}\",\"family\":\"{s}\",", .{ id, jsonString(o.get("path")), family_of.get(id) orelse "" });
        if (o.get("measured_side")) |s| if (s == .integer) try w.print("\"measured_side\":{d},", .{s.integer});
        try w.print("\"joint\":{},\"needs_review\":{},\n\"status\":", .{ joint, if (o.get("needs_review")) |n| n == .bool and n.bool else false });
        try std.json.Stringify.encodeJsonString(jsonString(m.get("status")), .{}, w);
        try w.writeAll(",\n\"note\":");
        try std.json.Stringify.encodeJsonString(jsonString(m.get("note")), .{}, w);
        try w.writeAll(",\n\"reference\":");
        try std.json.Stringify.encodeJsonString(jsonString(m.get("bk2_reference")), .{}, w);
        try w.print(",\n\"source\":\"{s}\",\"previous_variant\":\"{s}\",\n\"alignment\":", .{ try reviewRelative(arena, root, model_path), jsonString(o.get("previous_family_variant")) });
        try writeAlignment(w, m.get("alignment"));
        if (jsonNumber(o.get("silhouette_after"))) |s| try w.print(",\n\"scores\":{{\"silhouette\":{d:.4},\"edge\":{d:.4},\"combined\":{d:.4}}}", .{ s, jsonNumber(o.get("edge_score_after")) orelse 0, jsonNumber(o.get("combined_after")) orelse 0 });
        const vertices = m.get("vertices").?.array.items;
        vertices_total += vertices.len;
        if (joint) {
            // The family's shared mesh: the same triangles the model holds.
            const name = jsonString(m.get("shared_geometry"));
            const shared = (try parseJsonFile(ctx, try std.fmt.allocPrint(arena, "{s}/family-revision/shared/{s}.json", .{ root, name }))).object;
            if (shared.get("vertices").?.array.items.len != vertices.len) return ctx.failKey("views.err.read", .{ model_path, "SharedMeshDiffers" });
            try w.print(",\n\"shared\":\"{s}\"}}\n", .{name});
        } else {
            try w.writeAll(",\n");
            try writeMeshData(arena, w, vertices, m.get("triangles").?.array.items, m.get("face_kinds").?.array.items);
            try w.writeAll("}\n");
        }
        try writeCrlf(ctx, out, try std.fmt.allocPrint(arena, "{d:0>3}.json", .{@as(u64, @intCast(id))}), text.written());
    }
    try ctx.out.print("imported {d} objects ({d} review vertices), {d} families, {d} shared meshes into {s}\n", .{ objects.len, vertices_total, families.len, shared_count, out });
}

/// The colour of a mesh face of `kind` in the review (the family review's
/// colours; a face of no named material is roof where it faces up, else
/// wall), lit from the upper left by its normal `n` as the eye sees it.
fn meshFill(kind: []const u8, n: [3]f64) [3]f64 {
    const Colour = struct { []const u8, [3]f64 };
    const table = [_]Colour{
        .{ "body", .{ 0.86, 0.82, 0.69 } },   .{ "roof", .{ 0.76, 0.46, 0.36 } },    .{ "tank", .{ 0.59, 0.69, 0.75 } },
        .{ "low", .{ 0.67, 0.72, 0.56 } },    .{ "pole", .{ 0.71, 0.58, 0.36 } },    .{ "timber", .{ 0.69, 0.55, 0.35 } },
        .{ "cloth", .{ 0.72, 0.67, 0.44 } },  .{ "earth", .{ 0.58, 0.48, 0.30 } },   .{ "sandbag", .{ 0.69, 0.65, 0.45 } },
        .{ "stone", .{ 0.70, 0.67, 0.59 } },  .{ "crate", .{ 0.64, 0.53, 0.36 } },   .{ "foliage", .{ 0.38, 0.55, 0.32 } },
        .{ "roof_part", .{ 0.57, 0.67, 0.75 } },
    };
    const base = for (table) |c| {
        if (std.mem.eql(u8, c[0], kind)) break c[1];
    } else if (@abs(n[2]) > 0.55) table[1][1] else table[0][1];
    const light = 0.73 + 0.27 * @abs(mesh.dot(n, .{ -0.4, -0.3, 0.8660254 }));
    return .{ base[0] * light, base[1] * light, base[2] * light };
}

/// A mesh turned `turn` quarter turns from its alignment, drawn with its
/// faces' colours at `scale` pixels a unit, its projection's box at the
/// picture's corner.
fn drawMesh(gpa: std.mem.Allocator, model: mesh.Model, tris: []const mesh.Tri, turn: u2, scale: f64) !Image {
    const placement: mesh.Placement = .{ .alignment = model.alignment, .scale = scale };
    var v = mesh.view(placement, turn, .{ 0, 0 });
    const points = try mesh.viewVertices(gpa, model, v);
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (points) |p| {
        lo = .{ @min(lo[0], p[0]), @min(lo[1], p[1]) };
        hi = .{ @max(hi[0], p[0]), @max(hi[1], p[1]) };
    }
    v = mesh.view(placement, turn, .{ 1 - lo[0], 1 - lo[1] });
    const w: u32 = @intFromFloat(@ceil(hi[0] - lo[0]) + 2);
    const h: u32 = @intFromFloat(@ceil(hi[1] - lo[1]) + 2);
    const r = try mesh.raster(gpa, try mesh.viewVertices(gpa, model, v), tris, w, h);
    const colours = try gpa.alloc([4]u8, tris.len);
    for (tris, colours) |t, *colour| {
        const n = mesh.worldNormal(model, placement, t, turn);
        const q = mesh.quarter(.{ n[0], n[1] }, turn);
        const c = meshFill(model.parts[t.part].kind, .{ q[0], q[1], n[2] });
        colour.* = .{ @intFromFloat(@round(c[0] * 255)), @intFromFloat(@round(c[1] * 255)), @intFromFloat(@round(c[2] * 255)), 255 };
    }
    const image: Image = .{ .width = w, .height = h, .pixels = try gpa.alloc([4]u8, @as(usize, w) * h) };
    for (image.pixels, r.tri) |*p, t| p.* = if (t == mesh.Raster.none) .{ 0, 0, 0, 0 } else colours[t];
    return image;
}

/// The mesh's creases at its alignment (`mesh.creases`) as lines on the page
/// over `rect` (x0, y0, x1, y1 in points), its projection's box fitted into
/// the rectangle; where a face of the mesh stands in front the line is not drawn.
fn drawCreases(gpa: std.mem.Allocator, page: *bk_pdf.Page, model: mesh.Model, tris: []const mesh.Tri, alignment: mesh.Alignment, rect: [4]f64) !void {
    const factor = 2.0;
    const v = mesh.view(.{ .alignment = alignment }, 0, .{ 0, 0 });
    const points = try mesh.viewVertices(gpa, model, v);
    var lo = [3]f64{ std.math.inf(f64), std.math.inf(f64), std.math.inf(f64) };
    var hi = [3]f64{ -std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
    for (points) |p| for (0..3) |c| {
        lo[c] = @min(lo[c], p[c]);
        hi[c] = @max(hi[c], p[c]);
    };
    const rw = rect[2] - rect[0];
    const rh = rect[3] - rect[1];
    const s = @min(rw / @max(hi[0] - lo[0], 1e-6), rh / @max(hi[1] - lo[1], 1e-6));
    const w: u32 = @intFromFloat(@max(1, @ceil(rw * factor)));
    const h: u32 = @intFromFloat(@max(1, @ceil(rh * factor)));
    const toRaster = struct {
        fn f(p: [3]f64, l: [3]f64, hh: [3]f64, sc: f64, ww: f64, hgt: f64) [3]f64 {
            return .{ ((p[0] - (l[0] + hh[0]) / 2) * sc + ww / 2) * factor, ((p[1] - (l[1] + hh[1]) / 2) * sc + hgt / 2) * factor, p[2] };
        }
    }.f;
    for (points) |*p| p.* = toRaster(p.*, lo, hi, s, rw, rh);
    const r = try mesh.raster(gpa, points, tris, w, h);
    const tol = @max(1e-5, (hi[2] - lo[2]) * 0.012);
    for (try mesh.creases(gpa, model, tris, alignment, 0)) |c| {
        const a = toRaster(.{ mesh.project(c.a)[0], mesh.project(c.a)[1], mesh.dot(c.a, mesh.toward_eye) }, lo, hi, s, rw, rh);
        const b = toRaster(.{ mesh.project(c.b)[0], mesh.project(c.b)[1], mesh.dot(c.b, mesh.toward_eye) }, lo, hi, s, rw, rh);
        const steps: usize = @intFromFloat(@max(2, @floor(@sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]))) + 1));
        var start: ?[2]f64 = null;
        var last: [2]f64 = undefined;
        for (0..steps) |i| {
            const t = @as(f64, @floatFromInt(i)) / @as(f64, @floatFromInt(steps - 1));
            const q = [3]f64{ a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t };
            const seen = if (r.at(q[0], q[1])) |at| q[2] >= r.depth[at] - tol else true;
            const pdf_point = [2]f64{ rect[0] + q[0] / factor, rect[3] - q[1] / factor };
            if (seen) {
                if (start == null) start = pdf_point;
                last = pdf_point;
            }
            if ((!seen or i + 1 == steps) and start != null) {
                if (start.?[0] != last[0] or start.?[1] != last[1]) try page.line(start.?, last, .{ 0.18, 0.29, 0.36 }, 0.45);
                start = null;
            }
        }
    }
}

/// Words of `text` in lines of at most `width` characters.
fn wrapText(gpa: std.mem.Allocator, text: []const u8, width: usize) ![]const []const u8 {
    var lines: std.ArrayList([]const u8) = .empty;
    var it = std.mem.tokenizeScalar(u8, text, ' ');
    var start: ?usize = null;
    var end: usize = 0;
    while (it.next()) |word| {
        const at = @intFromPtr(word.ptr) - @intFromPtr(text.ptr);
        if (start) |s| if (at + word.len - s > width) {
            try lines.append(gpa, text[s..end]);
            start = null;
        };
        if (start == null) start = at;
        end = at + word.len;
    }
    if (start) |s| try lines.append(gpa, text[s..end]);
    return lines.items;
}

/// Paragraph lines from the top `y` down; returns the next line's y.
fn paragraph(page: *bk_pdf.Page, gpa: std.mem.Allocator, x: f64, y: f64, text: []const u8, width: usize, size: f64, rgb: [3]f64) !f64 {
    var at = y;
    for (try wrapText(gpa, text, width)) |l| {
        try page.text(x, at, size, false, rgb, l);
        at -= size + 3;
    }
    return at;
}

/// The four turns of a mesh at one scale in cells of 190 x 220 points from
/// (30, 30), labelled `<label>.N` to `.W`.
fn meshTurns(gpa: std.mem.Allocator, pdf: *bk_pdf.Pdf, page: *bk_pdf.Page, images: *std.ArrayList(u32), model: mesh.Model, tris: []const mesh.Tri, label: u32) !void {
    var size = [2]f64{ 0, 0 };
    for (0..4) |k| {
        const v = mesh.view(.{ .alignment = model.alignment }, @intCast(k), .{ 0, 0 });
        var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
        var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
        for (0..mesh.vertexCount(model)) |i| {
            const p = v.apply(mesh.vertex(model, i));
            lo = .{ @min(lo[0], p[0]), @min(lo[1], p[1]) };
            hi = .{ @max(hi[0], p[0]), @max(hi[1], p[1]) };
        }
        size = .{ @max(size[0], hi[0] - lo[0]), @max(size[1], hi[1] - lo[1]) };
    }
    const scale = @min(176 / @max(size[0], 1e-6), 170 / @max(size[1], 1e-6));
    const dpi = 2.5;
    for (0..4) |k| {
        const x0 = 30 + @as(f64, @floatFromInt(k)) * 200;
        const y0 = 30.0;
        try page.polygon(&.{ .{ x0, y0 }, .{ x0 + 190, y0 }, .{ x0 + 190, y0 + 220 }, .{ x0, y0 + 220 } }, .{ 0.97, 0.97, 0.97 }, .{ 0.75, 0.75, 0.75 }, 0.5);
        if (tris.len > 0) {
            const image = try drawMesh(gpa, model, tris, @intCast(k), scale * dpi);
            const id = try pdf.image(image.width, image.height, image.pixels);
            try images.append(gpa, id);
            const w = @as(f64, @floatFromInt(image.width)) / dpi;
            const h = @as(f64, @floatFromInt(image.height)) / dpi;
            try page.image(id, x0 + (190 - w) / 2, y0 + 16 + (176 - h) / 2, w, h);
        }
        const text = try std.fmt.allocPrint(gpa, "{d}.{c}", .{ label, side_letters[k] });
        try page.polygon(&.{ .{ x0 + 2, y0 + 190 }, .{ x0 + 18 + 13 * @as(f64, @floatFromInt(text.len)), y0 + 190 }, .{ x0 + 18 + 13 * @as(f64, @floatFromInt(text.len)), y0 + 218 }, .{ x0 + 2, y0 + 218 } }, .{ 1, 1, 1 }, null, 0);
        try page.text(x0 + 6, y0 + 194, 20, true, .{ 0, 0, 0 }, text);
        try page.text(x0 + 6, y0 + 6, 7, false, .{ 0.35, 0.35, 0.35 }, if (k == 0) "turn 0: the 2003 side" else try std.fmt.allocPrint(gpa, "turn {d}: {d} press{s} of '.' (+{d} deg)", .{ k, k, if (k == 1) "" else "es", 90 * k }));
    }
}

/// A 2003 picture (`id`, the PDF's image of it) in a box of `box` points at
/// (x, y), at most three times its size, with the rectangle of its opaque
/// pixels on the page (x0, y0, x1, y1) or null where it has none.
fn placePicture(page: *bk_pdf.Page, picture: Image, id: u32, x: f64, y: f64, box: [2]f64) !?[4]f64 {
    return placeScaled(page, picture, id, x, y, box, @min(box[0] / @as(f64, @floatFromInt(picture.width)), box[1] / @as(f64, @floatFromInt(picture.height)), 3.0));
}

/// placePicture at scale points a pixel.
fn placeScaled(page: *bk_pdf.Page, picture: Image, id: u32, x: f64, y: f64, box: [2]f64, scale: f64) !?[4]f64 {
    const w = scale * @as(f64, @floatFromInt(picture.width));
    const h = scale * @as(f64, @floatFromInt(picture.height));
    const ox = x + (box[0] - w) / 2;
    const oy = y + (box[1] - h) / 2;
    try page.polygon(&.{ .{ ox, oy }, .{ ox + w, oy }, .{ ox + w, oy + h }, .{ ox, oy + h } }, .{ 0.80, 0.83, 0.88 }, null, 0);
    try page.image(id, ox, oy, w, h);
    const b = mesh.opaqueBox(picture) orelse return null;
    return .{ ox + b[0] * scale, oy + h - b[3] * scale, ox + b[2] * scale, oy + h - b[1] * scale };
}

/// The 2003 intact picture of an object (`Buildings/...` or
/// `Objects/SimpleObjects/...`) and its zero cross, or null.
fn objectPicture(ctx: *bk_tool.Context, roots: [2][]const u8, path: []const u8) ?struct { picture: Image, anchor: [2]f64 } {
    const building = std.mem.startsWith(u8, path, "Buildings/");
    const relative = path[if (building) "Buildings/".len else "Objects/SimpleObjects/".len..];
    const dir = std.fs.path.join(ctx.arena, &.{ roots[if (building) 0 else 1], relative }) catch return null;
    const s = loadSource(ctx, dir, 1) catch return null;
    const p = pictureOf(s, "1") orelse return null;
    return .{ .picture = p, .anchor = .{ @floatFromInt(s.anchor[0]), @floatFromInt(s.anchor[1]) } };
}

/// What the review measures of a mesh against its picture, and what the
/// maintainer's review recorded.
const Match = struct { id: u32, path: []const u8, review: ?mesh.Scores, measured: ?mesh.Metric };

/// An object's page of the review drawn from its mesh: the 2003 picture alone
/// and with the mesh's creases over it, the correction and provenance, and
/// the four turns.
fn meshPage(ctx: *bk_tool.Context, pdf: *bk_pdf.Pdf, page: *bk_pdf.Page, images: *std.ArrayList(u32), roots: [2][]const u8, id: u32, path: []const u8, model: mesh.Model) !Match {
    const arena = ctx.arena;
    const page_h = 595.0;
    const tris = try mesh.triangles(arena, model);
    var match: Match = .{ .id = id, .path = path, .review = model.scores, .measured = null };
    const found = objectPicture(ctx, roots, path);
    if (found) |f| if (try mesh.target(arena, f.picture)) |t| {
        if (tris.len > 0) match.measured = try mesh.evaluate(arena, model, tris, model.alignment, t);
    };
    try page.text(30, page_h - 42, 26, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}", .{id}));
    try page.text(100, page_h - 34, 13, true, .{ 0, 0, 0 }, path);
    const review = if (model.scores) |s| try std.fmt.allocPrint(arena, "review: outline {d:.3}, edge {d:.3}", .{ s.silhouette, s.edge }) else "review: no 2003 picture";
    const measured = if (match.measured) |m| try std.fmt.allocPrint(arena, "measured here: outline {d:.3}, edge {d:.3}", .{ m.silhouette, m.edge }) else "measured here: no 2003 picture";
    try page.text(100, page_h - 50, 9.5, false, .{ 0.12, 0.35, 0.45 }, try std.fmt.allocPrint(arena, "{s}{s}  |  {s}  |  {s}", .{ model.status, if (model.needs_review) " (needs review)" else "", review, measured }));
    const box = [2]f64{ 250, 200 };
    const box_y = 300.0;
    try page.text(30, box_y + box[1] + 6, 9, false, .{ 0.2, 0.2, 0.2 }, "2003 picture (unchanged)");
    try page.text(300, box_y + box[1] + 6, 9, false, .{ 0.2, 0.2, 0.2 }, "model edges over the picture");
    if (found) |f| {
        const picture = try pdf.image(f.picture.width, f.picture.height, f.picture.pixels);
        try images.append(arena, picture);
        _ = try placePicture(page, f.picture, picture, 30, box_y, box);
        if (try placePicture(page, f.picture, picture, 300, box_y, box)) |rect| if (tris.len > 0) try drawCreases(arena, page, model, tris, model.alignment, rect);
    } else {
        _ = try paragraph(page, arena, 40, box_y + 110, "No 2003 picture of this object: the mesh is drawn alone.", 40, 9, .{ 0.3, 0.3, 0.3 });
        if (tris.len > 0) {
            const image = try drawMesh(arena, model, tris, 0, 1);
            const id_image = try pdf.image(image.width, image.height, image.pixels);
            try images.append(arena, id_image);
            const s = @min(box[0] / @as(f64, @floatFromInt(image.width)), box[1] / @as(f64, @floatFromInt(image.height)));
            try page.image(id_image, 300, box_y, s * @as(f64, @floatFromInt(image.width)), s * @as(f64, @floatFromInt(image.height)));
        }
    }
    try page.text(580, box_y + box[1] + 6, 10, true, .{ 0, 0, 0 }, "Correction and provenance");
    var y = try paragraph(page, arena, 580, box_y + box[1] - 10, model.note, 46, 8, .{ 0.2, 0.2, 0.2 });
    y = try paragraph(page, arena, 580, y - 6, try std.fmt.allocPrint(arena, "Reference: {s}", .{model.reference}), 48, 7.3, .{ 0.2, 0.2, 0.2 });
    y = try paragraph(page, arena, 580, y - 6, try std.fmt.allocPrint(arena, "Alignment yaw {d:.1} deg; height {d:.2}; width {d:.2}. {d} vertices, {d} triangles.", .{ model.alignment.angle, model.alignment.zscale, model.alignment.ratio, mesh.vertexCount(model), tris.len }), 48, 7.3, .{ 0.2, 0.2, 0.2 });
    if (model.family.len > 0) y = try paragraph(page, arena, 580, y - 6, try std.fmt.allocPrint(arena, "Family {s}{s}{s}.", .{ model.family, if (model.measured_side) |s| try std.fmt.allocPrint(arena, ", measured side {d}", .{s}) else "", if (model.joint) try std.fmt.allocPrint(arena, ", joint candidate (shared mesh {s})", .{model.shared orelse ""}) else "" }), 48, 7.3, .{ 0.2, 0.2, 0.2 });
    y = try paragraph(page, arena, 580, y - 6, try std.fmt.allocPrint(arena, "Source: {s}{s}{s}", .{ model.source, if (model.previous_variant.len > 0) "; previous version kept as " else "", model.previous_variant }), 48, 7.3, .{ 0.2, 0.2, 0.2 });
    _ = try paragraph(page, arena, 580, y - 6, "Edge score: Gaussian-smoothed sprite edges against the mesh's visible creases; a review aid, not proof of a match.", 48, 7.3, .{ 0.45, 0.25, 0.1 });
    try meshTurns(arena, pdf, page, images, model, tris, id);
    try page.text(30, 12, 7, false, .{ 0.4, 0.4, 0.4 }, "Meshes from the maintainer's family review, tracked in tools/zig/fixtures/bk-views/meshes; N is the 2003 side, E/S/W the same mesh turned.");
    return match;
}

/// A joint family's comparison page: each included view's 2003 picture with
/// the shared mesh's creases at its own alignment, and the shared mesh
/// turned to the four sides from the first view's alignment.
fn familyPage(ctx: *bk_tool.Context, pdf: *bk_pdf.Pdf, page: *bk_pdf.Page, images: *std.ArrayList(u32), roots: [2][]const u8, ids: ShapeIds, family: mesh.Family) !void {
    const arena = ctx.arena;
    const page_h = 595.0;
    try page.text(25, page_h - 30, 16, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "Joint family candidate: {s}/{s}", .{ family.region, family.family }));
    try page.text(25, page_h - 46, 8.5, false, .{ 0.2, 0.2, 0.2 }, "Each 2003 picture is paired with the same mesh, turned to its measured side. Details require review.");
    if (family.geometry_note.len > 0) try page.text(25, page_h - 58, 8.5, false, .{ 0.45, 0.25, 0.1 }, family.geometry_note);
    const shared = (try mesh.load(arena, ctx.io, mesh.meshes_dir, family.views[0].id)) orelse return;
    const tris = try mesh.triangles(arena, shared);
    const width = 850 / @as(f64, @floatFromInt(family.views.len));
    for (family.views, 0..) |v, i| {
        const x = 25 + @as(f64, @floatFromInt(i)) * width;
        try page.text(x, page_h - 76, 10, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d} | measured side {d} ({s})", .{ v.id, v.side, v.method }));
        const path = for (ids.ids) |e| {
            if (e.id == v.id) break e.path;
        } else continue;
        const found = objectPicture(ctx, roots, path) orelse continue;
        const sub = (width - 14) / 2;
        const picture = try pdf.image(found.picture.width, found.picture.height, found.picture.pixels);
        try images.append(arena, picture);
        _ = try placePicture(page, found.picture, picture, x, 305, .{ sub, 190 });
        if (try placePicture(page, found.picture, picture, x + sub + 8, 305, .{ sub, 190 })) |rect| try drawCreases(arena, page, shared, tris, v.alignment, rect);
        try page.text(x, 295, 7, false, .{ 0.2, 0.2, 0.2 }, try std.fmt.allocPrint(arena, "2003 picture | model edges (outline {d:.3}, edge {d:.3})", .{ v.silhouette, v.edge }));
    }
    var model = shared;
    model.alignment = family.views[0].alignment;
    try meshTurns(arena, pdf, page, images, model, tris, family.views[0].id);
    try page.text(25, 12, 7, false, .{ 0.4, 0.3, 0.2 }, "Preliminary geometry, not a verified match. Prior shapes are kept in the maintainer's review as named variants.");
}

/// The family table: every family of numbered views, its status, which
/// views share a mesh and why the others do not.
fn familyTable(gpa: std.mem.Allocator, pdf: *bk_pdf.Pdf, families: []const mesh.Family) !void {
    const page_h = 595.0;
    var i: usize = 0;
    var first = true;
    while (i < families.len) {
        var page: bk_pdf.Page = .init(gpa);
        var y: f64 = page_h - 34;
        try page.text(30, y, 14, true, .{ 0, 0, 0 }, if (first) "Families: views of one building measured from different sides" else "Families (continued)");
        y -= 20;
        first = false;
        while (i < families.len and y > 40) : (i += 1) {
            const f = families[i];
            var ids: std.Io.Writer.Allocating = .init(gpa);
            for (f.all_ids, 0..) |id, j| try ids.writer.print("{s}{d}", .{ if (j > 0) "," else "", id });
            var why: std.Io.Writer.Allocating = .init(gpa);
            if (f.reason.len > 0) try why.writer.print("{s}. ", .{f.reason});
            for (f.excluded) |e| try why.writer.print("{d} out: {s}. ", .{ e.id, e.reason });
            for (f.views) |v| try why.writer.print("{d} side {d}. ", .{ v.id, v.side });
            const colour: [3]f64 = if (std.mem.eql(u8, f.status, "joint candidate")) .{ 0.1, 0.4, 0.1 } else if (std.mem.eql(u8, f.status, "unresolved")) .{ 0.5, 0.3, 0.1 } else .{ 0.6, 0.1, 0.1 };
            try page.text(30, y, 8, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(gpa, "{s}/{s}", .{ f.region, f.family }));
            try page.text(150, y, 8, false, colour, f.status);
            try page.text(250, y, 8, false, .{ 0, 0, 0 }, ids.written());
            const lines = try wrapText(gpa, why.written(), 110);
            for (lines) |l| {
                try page.text(340, y, 7.5, false, .{ 0.25, 0.25, 0.25 }, l);
                y -= 10;
            }
            if (lines.len == 0) y -= 10;
            y -= 2;
        }
        _ = try pdf.page(842, page_h, page.out.written(), &.{});
    }
}

// ---------------------------------------------------------------------------
// The fill (M006 round 5): a joint family's turns drawn on its shared mesh

/// A joint family's shared mesh as a variant's scene holds it: the mesh (at
/// the variant's own alignment) and its triangles, and each view of it the
/// scene's 2003 sources of the variant's colourway give (the variant's own
/// first): the source and where the mesh lies on its intact picture
/// (`mesh.place`).
pub const MeshScene = struct { model: mesh.Model, tris: []const mesh.Tri, views: []const MeshView };
pub const MeshView = struct { source: usize, placement: mesh.Placement };

/// The review number of a variant folder: its path from `Buildings/` or `Objects/`.
fn shapeIdOf(ids: ShapeIds, gpa: std.mem.Allocator, dir: []const u8) !?u32 {
    const slashed = try std.mem.replaceOwned(u8, gpa, dir, "\\", "/");
    const at = std.mem.lastIndexOf(u8, slashed, "Buildings/") orelse std.mem.lastIndexOf(u8, slashed, "Objects/SimpleObjects/") orelse return null;
    const path = std.mem.trimEnd(u8, slashed[at..], "/");
    for (ids.ids) |e| if (std.ascii.eqlIgnoreCase(e.path, path)) return e.id;
    return null;
}

/// The scene's mesh where its variant has one (`mesh.meshes_dir`, by its
/// number in shape-ids.json): a view of a joint candidate family
/// (families.json) holds the shared mesh and each view of the colourway,
/// every other object its own mesh with its own picture its only view. Null
/// for a folder without a mesh (a modder's), which keeps the blocks.
fn loadMeshScene(ctx: *bk_tool.Context, sources: []const Source, same: []const bool, fixed: usize) !?MeshScene {
    const arena = ctx.arena;
    const ids = (try readJson(ShapeIds, ctx, shape_ids_file)) orelse return null;
    const own_id = (try shapeIdOf(ids, arena, sources[fixed].dir)) orelse return null;
    const model = (try mesh.load(arena, ctx.io, mesh.meshes_dir, own_id)) orelse return null;
    const family = if (model.joint) for (try mesh.loadFamilies(arena, ctx.io, mesh.meshes_dir)) |f| {
        if (std.mem.eql(u8, f.status, "joint candidate") and std.mem.indexOfScalar(u32, f.included, own_id) != null) break f;
    } else null else null;
    if (family == null) {
        const picture = pictureOf(sources[fixed], "1") orelse return null;
        const placement = mesh.place(model, picture, .{ @floatFromInt(sources[fixed].anchor[0]), @floatFromInt(sources[fixed].anchor[1]) }) orelse return null;
        const views = try arena.alloc(MeshView, 1);
        views[0] = .{ .source = fixed, .placement = placement };
        return .{ .model = model, .tris = try mesh.triangles(arena, model), .views = views };
    }
    var views: std.ArrayList(MeshView) = .empty;
    for (0..sources.len) |i| {
        const e = if (i == 0) fixed else if (i <= fixed) i - 1 else i;
        if (!same[e]) continue;
        const id = (try shapeIdOf(ids, arena, sources[e].dir)) orelse continue;
        if (std.mem.indexOfScalar(u32, family.?.included, id) == null) continue;
        const view_model = (try mesh.load(arena, ctx.io, mesh.meshes_dir, id)) orelse continue;
        const picture = pictureOf(sources[e], "1") orelse continue;
        const placement = mesh.place(view_model, picture, .{ @floatFromInt(sources[e].anchor[0]), @floatFromInt(sources[e].anchor[1]) }) orelse continue;
        try views.append(arena, .{ .source = e, .placement = placement });
    }
    if (views.items.len == 0 or views.items[0].source != fixed) return null;
    return .{ .model = model, .tris = try mesh.triangles(arena, model), .views = views.items };
}

/// A state's ground picture turned (`projectGround`) from the colourway's
/// 2003 ground pictures, the variant's own first; null where it has none.
fn groundOf(gpa: std.mem.Allocator, scene: Scene, state: []const u8, target: Turned) !?Generated {
    const name = try std.fmt.allocPrint(gpa, "{s}g", .{state});
    if (pictureOf(scene.sources[scene.fixed], name) == null) return null;
    var ground: std.ArrayList(Projector) = .empty;
    for (scene.sources, scene.eyes, scene.same, 0..) |source, eye, s, e| {
        if (!s) continue;
        const g = pictureOf(source, name) orelse continue;
        try ground.insert(gpa, if (e == scene.fixed) 0 else ground.items.len, .{ .eye = eye, .picture = g });
    }
    return try projectGround(gpa, target.eye, target.size, ground.items);
}

/// The depth buffer of the mesh in one view, with each triangle's normal in
/// that view's world turned towards its eye.
const MeshSeen = struct { view: mesh.View, raster: mesh.Raster, normals: [][3]f64 };

fn meshSeen(gpa: std.mem.Allocator, ms: MeshScene, placement: mesh.Placement, turn: u2, anchor: [2]f64, size: [2]u32) !MeshSeen {
    const v = mesh.view(placement, turn, anchor);
    const normals = try gpa.alloc([3]f64, ms.tris.len);
    for (ms.tris, normals) |t, *n| n.* = mesh.worldNormal(ms.model, placement, t, turn);
    return .{ .view = v, .raster = try mesh.raster(gpa, try mesh.viewVertices(gpa, ms.model, v), ms.tris, size[0], size[1]), .normals = normals };
}

/// The zero cross of a turn drawn on the mesh: as `footprintZero` lays the
/// blocks, the mesh's ground box moved along the line of sight to the
/// footprint's middle.
fn meshZero(scene: Scene, ms: MeshScene, target: Turned) [2]f64 {
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (0..mesh.vertexCount(ms.model)) |i| {
        const w = ms.views[0].placement.world(mesh.vertex(ms.model, i));
        lo = .{ @min(lo[0], w[0]), @min(lo[1], w[1]) };
        hi = .{ @max(hi[0], w[0]), @max(hi[1], w[1]) };
    }
    return zeroOnFootprint(scene, target, lo, hi);
}

/// What the fill's checks measure of a turn's intact picture against the
/// mesh turned as the turn's eye sees it: the outline IoU, the mesh's pixels
/// the picture leaves empty (holes) and those the patch fill drew, and per
/// kind of face (roof, wall, timber, ...) the largest difference of a mean
/// colour channel from that kind's pixels in the variant's 2003 picture (out
/// of 255; kinds seen on `colour_pixels` or more pixels in both).
pub const MeshCheck = struct { outline: f64 = 0, holes: usize = 0, patched: usize = 0, colour: f64 = 0, kinds: usize = 0 };
pub const colour_pixels = 200;

fn meshCheck(gpa: std.mem.Allocator, scene: Scene, ms: MeshScene, target: Turned, drawn: Drawn) !MeshCheck {
    const image = drawn.sprite.image;
    const goal = try meshSeen(gpa, ms, ms.views[0].placement, target.eye.turn, target.eye.anchor, .{ image.width, image.height });
    const own = scene.sources[scene.fixed];
    const picture = pictureOf(own, "1").?;
    const at_n = try meshSeen(gpa, ms, ms.views[0].placement, 0, .{ @floatFromInt(own.anchor[0]), @floatFromInt(own.anchor[1]) }, .{ picture.width, picture.height });
    var check: MeshCheck = .{};
    var both: f64 = 0;
    var either: f64 = 0;
    const kinds = ms.model.parts.len;
    const sums = try gpa.alloc([2][4]f64, kinds);
    @memset(sums, std.mem.zeroes([2][4]f64));
    for (image.pixels, goal.raster.tri, drawn.sprite.kinds) |p, t, kind| {
        const on = t != mesh.Raster.none;
        both += @floatFromInt(@intFromBool(on and p[3] >= 128));
        either += @floatFromInt(@intFromBool(on or p[3] >= 128));
        if (!on) continue;
        check.holes += @intFromBool(p[3] < 128);
        check.patched += @intFromBool(kind == .filled);
        if (p[3] < 128) continue;
        const k = ms.tris[t].part;
        for (0..3) |c| sums[k][0][c] += @floatFromInt(p[c]);
        sums[k][0][3] += 1;
    }
    for (picture.pixels, at_n.raster.tri) |p, t| {
        if (t == mesh.Raster.none or p[3] < 128) continue;
        const k = ms.tris[t].part;
        for (0..3) |c| sums[k][1][c] += @floatFromInt(p[c]);
        sums[k][1][3] += 1;
    }
    check.outline = if (either == 0) 0 else both / either;
    for (sums) |s| {
        if (s[0][3] < colour_pixels or s[1][3] < colour_pixels) continue;
        check.kinds += 1;
        for (0..3) |c| check.colour = @max(check.colour, @abs(s[0][c] / s[0][3] - s[1][c] / s[1][3]));
    }
    return check;
}

// ---------------------------------------------------------------------------
// Full 3D objects (docs/superpowers/specs/2026-10-29-full-3d-objects-design.md)

pub const model3d = @import("bk_views_model.zig");

/// The scene's full 3D models. The intact one: its mesh unrolled where the
/// variant's own picture places it, each intact state's atlas (summer `1`,
/// winter `1w`) painted from the pictures of the colourway the mesh lies on
/// (`model3d.bake`), lit by the building's own wall light measured on its
/// intact pictures. Then a model a damage state (`model3d.damaged`, from the
/// variant's own damaged picture of summer), its atlas of each season the
/// intact one of that season painted over from its own damaged picture.
fn buildFull3d(gpa: std.mem.Allocator, sources: []const Source, ms: MeshScene, states: []const []const u8) ![]const model3d.Model {
    var models: std.ArrayList(model3d.Model) = .empty;
    var m = try model3d.unwrap(gpa, ms.model, ms.tris, ms.views[0].placement);
    const has = struct {
        fn f(list: []const []const u8, state: []const u8) bool {
            for (list) |s| if (std.mem.eql(u8, s, state)) return true;
            return false;
        }
    }.f;
    for ([_][]const u8{ "1", "1w" }) |state| {
        if (!has(states, state)) continue;
        var means: Means = .{};
        var bake_views: std.ArrayList(model3d.BakeView) = .empty;
        for (ms.views) |v| {
            const source = sources[v.source];
            const picture = pictureOf(source, state) orelse continue;
            const s = try meshSeen(gpa, ms, v.placement, 0, .{ @floatFromInt(source.anchor[0]), @floatFromInt(source.anchor[1]) }, .{ picture.width, picture.height });
            for (picture.pixels, s.raster.tri) |pixel, t| {
                if (t != mesh.Raster.none and pixel[3] >= 128) means.add(shadeClass(s.normals[t], 0), pixel);
            }
            try bake_views.append(gpa, .{ .picture = picture, .view = s.view, .raster = s.raster, .normals = s.normals });
        }
        if (std.mem.eql(u8, state, "1")) m.wall = wallLight(means);
        try model3d.bake(gpa, &m, state, bake_views.items, null);
    }
    try models.append(gpa, m);
    const own = sources[ms.views[0].source];
    const anchor = [2]f64{ @floatFromInt(own.anchor[0]), @floatFromInt(own.anchor[1]) };
    for ([_][]const u8{ "2", "3" }) |state| {
        if (!has(states, state)) continue;
        var d = try model3d.damaged(gpa, m, ms.views[0].placement, pictureOf(own, state).?, pictureOf(own, "1").?, anchor);
        for ([_][2][]const u8{ .{ state, "1" }, .{ try std.fmt.allocPrint(gpa, "{s}w", .{state}), "1w" } }) |pair| {
            if (!has(states, pair[0])) continue;
            const picture = pictureOf(own, pair[0]).?;
            const view = try model3d.ownBakeView(gpa, d, picture, ms.views[0].placement, anchor);
            try model3d.bake(gpa, &d, pair[0], &.{view}, m.texture(pair[1]) orelse m.texture("1").?);
        }
        try models.append(gpa, d);
    }
    return models.items;
}

/// A state of a turn drawn by its full 3D model (`model3d.render`), its
/// shadow the model's.
fn full3dDrawn(gpa: std.mem.Allocator, m: model3d.Model, state: []const u8, blocks: []const Block, target: Turned) !Drawn {
    const r = try model3d.render(gpa, m, state, target.eye.turn, target.light, target.eye.anchor, target.size);
    const out: Generated = .{ .image = r.image, .kinds = try gpa.alloc(Kind, r.guessed.len), .classes = try gpa.alloc(u8, r.guessed.len) };
    for (out.kinds, r.image.pixels, r.guessed) |*kind, p, g| kind.* = if (p[3] == 0) .empty else if (g) .filled else .projected;
    @memset(out.classes, 255);
    const shadow = try model3d.shadow(gpa, m, target.eye.turn, target.light, target.eye.anchor, target.size);
    keepShadow(out.image, shadow);
    return .{ .state = state, .blocks = blocks, .sprite = out, .shadow = shadow, .ground = null, .covers = &.{} };
}

/// What a picture rendered from the full 3D model shows of the 2003 art it
/// stands for: their outline IoU where the art's pixel (x, y) lies on the
/// render's (x, y) + `shift` (`search`: the best shift climbing from
/// `start`, for a 2003 sibling laid by its footprint), and the mean absolute
/// colour difference (of 255) where both are opaque there.
pub const ArtMatch = struct { iou: f64, colour: f64, shift: [2]i64 };

pub fn artMatch(rendered: Image, art: Image, start: [2]i64, search: bool) ArtMatch {
    const offset = if (search) bestOffset(rendered, art, start) else start;
    var sum: f64 = 0;
    var n: f64 = 0;
    for (0..art.height) |y| for (0..art.width) |x| {
        const a = art.pixels[y * art.width + x];
        const p = rendered.at(@as(i64, @intCast(x)) + offset[0], @as(i64, @intCast(y)) + offset[1]) orelse continue;
        if (a[3] < 128 or p[3] < 128) continue;
        for (0..3) |c| sum += @abs(@as(f64, @floatFromInt(a[c])) - @as(f64, @floatFromInt(p[c])));
        n += 3;
    };
    return .{ .iou = shiftedIou(rendered, art, offset), .colour = if (n == 0) 255 else sum / n, .shift = offset };
}

/// A picture of where two pictures of one canvas differ: grey by their colour
/// difference where both are opaque, red where only `a` is, blue where only `b` is.
fn diffImage(gpa: std.mem.Allocator, a: Image, b: Image) !Image {
    const out: Image = .{ .width = a.width, .height = a.height, .pixels = try gpa.alloc([4]u8, a.pixels.len) };
    for (out.pixels, a.pixels, b.pixels) |*d, p, q| {
        const in_a = p[3] >= 128;
        const in_b = q[3] >= 128;
        d.* = if (in_a and in_b) blk: {
            var sum: u32 = 0;
            for (0..3) |c| sum += @abs(@as(i32, p[c]) - @as(i32, q[c]));
            const g: u8 = @intCast(255 - @min(255, sum));
            break :blk .{ g, g, g, 255 };
        } else if (in_a) .{ 220, 30, 30, 255 } else if (in_b) .{ 30, 60, 220, 255 } else .{ 0, 0, 0, 0 };
    }
    return out;
}

/// A picture made at most `side` pixels on its longer side (nearest pixel),
/// for the review.
fn shrunk(gpa: std.mem.Allocator, image: Image, side: u32) !Image {
    const longer = @max(image.width, image.height);
    if (longer <= side) return image;
    const w = @max(1, image.width * side / longer);
    const h = @max(1, image.height * side / longer);
    const out: Image = .{ .width = w, .height = h, .pixels = try gpa.alloc([4]u8, @as(usize, w) * h) };
    for (0..h) |y| for (0..w) |x| {
        out.pixels[y * w + x] = image.pixels[(y * image.height / h) * image.width + x * image.width / w];
    };
    return out;
}

/// Where an atlas's paint came from: green from a 2003 picture, magenta
/// guessed (material paint), black not reached; clear off every face.
fn paintMap(gpa: std.mem.Allocator, m: model3d.Model, t: model3d.Texture) !Image {
    const map: Image = .{ .width = t.image.width, .height = t.image.height, .pixels = try gpa.alloc([4]u8, t.paint.len) };
    for (map.pixels, t.paint, m.texel_tri) |*p, k, tri| p.* = if (tri == model3d.Model.none) .{ 0, 0, 0, 0 } else switch (k) {
        .seen => .{ 40, 170, 60, 255 },
        .guessed => .{ 230, 40, 200, 255 },
        .none => .{ 0, 0, 0, 255 },
    };
    return map;
}

/// What `model3d` measures of an object and `full3d-review` lays out (the
/// object's model.json).
const Full3dReport = struct {
    id: u32,
    path: []const u8,
    mesh: []const u8,
    status: []const u8,
    needs_review: bool,
    atlas: [2]u32,
    vertices: usize,
    triangles: usize,
    charts: usize,
    groups: []const []const u8,
    textures: []const struct { state: []const u8, seen: f64, guessed: f64 },
    /// N against the 2003 picture: the render's outline IoU, the mesh's, the colour difference.
    n_iou: f64,
    n_mesh_iou: f64,
    n_colour: f64,
    /// Render samples on texels no paint reached: N, the turns, the turns under the sun.
    holes: usize,
    /// Pixels of guessed paint at N, E, S, W.
    guessed_pixels: [4]usize,
    sibling: ?struct { name: []const u8, turn: u2, laid: f64, iou: f64, colour: f64 } = null,
    damage: []const struct { state: []const u8, triangles: usize, iou: f64, colour: f64, intact_iou: f64, holes: usize },
    /// A second build and render gave the same bytes.
    same: bool,
};

/// `model3d`: each variant folder's full 3D models (`buildFull3d`) as files
/// in the vehicles' format family, in `<out-dir>/<number>/`: a `.mod` a model
/// (`1` the intact building, `2` and `3` its damage states), per state its
/// atlas as `<state>_h.dds` (what the Game's loader reads), `<state>.tga` (the
/// Resource Editor's unit source) and `<state>_paint.png` (green where a 2003
/// picture painted a texel, magenta where the material paint guessed it);
/// `model.json` (`Full3dReport`: what the review measures, a second build and
/// render compared byte for byte); and `review/`, the pictures `full3d-review`
/// lays out: the 2003 picture, the render at N and their difference, the
/// atlas and its paint map, the turns under the 2003 light (`t<k>`) and the
/// world's sun (`s<k>`), and per damage state the same at N and its turns.
/// One run an object, so the batch spreads the objects over the cores.
fn model3dCommand(ctx: *bk_tool.Context, out_arg: []const u8, dir_args: []const []const u8) !void {
    const arena = ctx.arena;
    const out = try bk_tool.inputPath(arena, out_arg);
    for (dir_args) |dir_arg| {
        const dir = try bk_tool.inputPath(arena, dir_arg);
        const found = findFamily(arena, ctx.io, dir) catch |err| return ctx.failKey("views.err.read", .{ dir, @errorName(err) });
        const scene = try loadScene(ctx, found, found.index, null, false);
        const ms = scene.mesh orelse return ctx.failKey("views.err.read", .{ dir, "NoMesh" });
        try depScene(ctx, found, scene);
        const intact = scene.full3d[0];
        const own = scene.sources[scene.fixed];
        const anchor = [2]f64{ @floatFromInt(own.anchor[0]), @floatFromInt(own.anchor[1]) };
        const folder = try std.fs.path.join(arena, &.{ out, try std.fmt.allocPrint(arena, "{d:0>3}", .{ms.model.id}) });
        const review = try std.fs.path.join(arena, &.{ folder, "review" });
        try std.Io.Dir.cwd().createDirPath(ctx.io, review);
        const picturePng = struct {
            fn f(c: *bk_tool.Context, d: []const u8, name: []const u8, image: Image) !void {
                try writeOut(c, d, name, try writePngDeflated(c.arena, image));
            }
        }.f;

        // The files, a model at a time.
        var json: std.Io.Writer.Allocating = .init(arena);
        const w = &json.writer;
        try w.print("{{\"id\":{d},\"path\":\"{s}\",\"mesh\":\"{s}\",\"status\":\"{s}\",\"needs_review\":{},\"atlas\":[{d},{d}],\"density\":{d},\"wall\":[{d:.4},{d:.4},{d:.4}],\"vertices\":{d},\"triangles\":{d},\"charts\":{d},\"groups\":[", .{ ms.model.id, ms.model.path, ms.model.shared orelse "own", ms.model.status, ms.model.needs_review, intact.width, intact.height, model3d.density, intact.wall[0], intact.wall[1], intact.wall[2], intact.vertices.len, intact.triangles.len, intact.charts.len });
        for (intact.groups, 0..) |g, i| try w.print("{s}\"{s}\"", .{ if (i > 0) "," else "", g });
        try w.writeAll("],\r\n\"textures\":[");
        var mods: std.ArrayList([]const u8) = .empty;
        var first_texture = true;
        for (scene.full3d) |m| {
            const mod = try model3d.writeMod(arena, m);
            try mods.append(arena, mod);
            try writeOut(ctx, folder, try std.fmt.allocPrint(arena, "{s}.mod", .{m.textures[0].state[0..1]}), mod);
            for (m.textures) |t| {
                var counts = [3]f64{ 0, 0, 0 };
                // Faces that look down (floors, eaves' undersides) no turn shows.
                for (t.paint, m.texel_tri) |k, tri| {
                    if (tri != model3d.Model.none and !std.mem.eql(u8, model3d.lookOf(m.groups[m.triangles[tri].group]), "under")) counts[@intFromEnum(k)] += 1;
                }
                const on = @max(counts[0] + counts[1] + counts[2], 1);
                try writeOut(ctx, folder, try std.fmt.allocPrint(arena, "{s}_h.dds", .{t.state}), try model3d.writeDds(arena, t.image));
                try writeOut(ctx, folder, try std.fmt.allocPrint(arena, "{s}.tga", .{t.state}), try model3d.writeTga(arena, t.image));
                try picturePng(ctx, folder, try std.fmt.allocPrint(arena, "{s}_paint.png", .{t.state}), try paintMap(arena, m, t));
                try w.print("{s}{{\"state\":\"{s}\",\"seen\":{d:.4},\"guessed\":{d:.4}}}", .{ if (first_texture) "" else ",", t.state, counts[1] / on, counts[2] / on });
                first_texture = false;
            }
        }
        const tex = intact.texture("1").?;
        try picturePng(ctx, review, "atlas.png", try shrunk(arena, tex.image, 512));
        try picturePng(ctx, review, "paint.png", try shrunk(arena, try paintMap(arena, intact, tex), 512));

        // N on the 2003 picture's own canvas, at its zero cross: the
        // projection must be the 2003 camera's to the pixel, so no shift.
        const picture = pictureOf(own, "1").?;
        const n = try model3d.render(arena, intact, "1", 0, 0, anchor, .{ picture.width, picture.height });
        const n_match = artMatch(n.image, picture, .{ 0, 0 }, false);
        const at_n = try meshSeen(arena, ms, ms.views[0].placement, 0, anchor, .{ picture.width, picture.height });
        var both: f64 = 0;
        var either: f64 = 0;
        for (picture.pixels, at_n.raster.tri) |p, t| {
            both += @floatFromInt(@intFromBool(p[3] >= 128 and t != mesh.Raster.none));
            either += @floatFromInt(@intFromBool(p[3] >= 128 or t != mesh.Raster.none));
        }
        try picturePng(ctx, review, "pic.png", picture);
        try picturePng(ctx, review, "n.png", n.image);
        try picturePng(ctx, review, "diff.png", try diffImage(arena, picture, n.image));

        // The turns as `side` draws them, on its canvases.
        var turns: [4]model3d.Rendered = undefined;
        var holes: usize = n.unpainted;
        var guessed_pixels: [4]usize = .{ 0, 0, 0, 0 };
        for (n.guessed) |g| guessed_pixels[0] += @intFromBool(g);
        for (0..4) |k| {
            const t = turnTarget(scene, @intCast(k), @intCast(k), "");
            turns[k] = try model3d.render(arena, intact, "1", @intCast(k), @intCast(k), t.eye.anchor, t.size);
            holes += turns[k].unpainted;
            if (k == 0) continue;
            for (turns[k].guessed) |g| guessed_pixels[k] += @intFromBool(g);
            // N lit by the sun at camera step 0 is N under the 2003 light.
            const lit = try model3d.render(arena, intact, "1", @intCast(k), 0, t.eye.anchor, t.size);
            holes += lit.unpainted;
            try picturePng(ctx, review, try std.fmt.allocPrint(arena, "t{d}.png", .{k}), turns[k].image);
            try picturePng(ctx, review, try std.fmt.allocPrint(arena, "s{d}.png", .{k}), lit.image);
        }
        try w.print("],\r\n\"n_iou\":{d:.4},\"n_mesh_iou\":{d:.4},\"n_colour\":{d:.2},\"holes\":{d},\"guessed_pixels\":[{d},{d},{d},{d}]", .{ n_match.iou, if (either == 0) 0 else both / either, n_match.colour, holes, guessed_pixels[0], guessed_pixels[1], guessed_pixels[2], guessed_pixels[3] });

        // A 2003 sibling of the colourway against the turn that shows it.
        for (ms.views[1..]) |v| {
            const source = scene.sources[v.source];
            const eye = scene.eyes[v.source];
            const art = pictureOf(source, "1") orelse continue;
            const t = turnTarget(scene, eye.turn, eye.turn, "");
            const shift = mfc_camera.project(.{ eye.shift[0], eye.shift[1], 0 });
            const start = [2]i64{ @intFromFloat(@round(t.eye.anchor[0] - anchor[0] - shift[0])), @intFromFloat(@round(t.eye.anchor[1] - anchor[1] - shift[1])) };
            const match = artMatch(turns[eye.turn].image, art, start, true);
            try w.print(",\r\n\"sibling\":{{\"name\":\"{s}\",\"turn\":{d},\"laid\":{d:.4},\"iou\":{d:.4},\"colour\":{d:.2}}}", .{ std.fs.path.basename(source.dir), eye.turn, shiftedIou(turns[eye.turn].image, art, start), match.iou, match.colour });
            break;
        }

        // The damage states: each model at N against its 2003 picture (and
        // the intact model against it, what the cut changes), and turned.
        try w.writeAll(",\r\n\"damage\":[");
        for (scene.full3d[1..], 0..) |m, i| {
            const state = m.textures[0].state;
            const art = pictureOf(own, state).?;
            const dn = try model3d.render(arena, m, state, 0, 0, anchor, .{ art.width, art.height });
            const match = artMatch(dn.image, art, .{ 0, 0 }, false);
            const before = artMatch(n.image, art, .{ 0, 0 }, false);
            var damage_holes = dn.unpainted;
            try picturePng(ctx, review, try std.fmt.allocPrint(arena, "d{s}_pic.png", .{state}), art);
            try picturePng(ctx, review, try std.fmt.allocPrint(arena, "d{s}_n.png", .{state}), dn.image);
            try picturePng(ctx, review, try std.fmt.allocPrint(arena, "d{s}_diff.png", .{state}), try diffImage(arena, art, dn.image));
            for (1..4) |k| {
                const t = turnTarget(scene, @intCast(k), @intCast(k), "");
                const r = try model3d.render(arena, m, state, @intCast(k), @intCast(k), t.eye.anchor, t.size);
                damage_holes += r.unpainted;
                try picturePng(ctx, review, try std.fmt.allocPrint(arena, "d{s}_t{d}.png", .{ state, k }), r.image);
            }
            try w.print("{s}{{\"state\":\"{s}\",\"triangles\":{d},\"iou\":{d:.4},\"colour\":{d:.2},\"intact_iou\":{d:.4},\"holes\":{d}}}", .{ if (i > 0) "," else "", state, m.triangles.len, match.iou, match.colour, before.iou, damage_holes });
        }

        // The same bytes again: the models built anew, their files and the
        // intact turns compared.
        const again = try buildFull3d(arena, scene.sources, ms, scene.states);
        var same = again.len == scene.full3d.len;
        if (same) for (scene.full3d, again, mods.items) |a, b, mod| {
            same = same and std.mem.eql(u8, mod, try model3d.writeMod(arena, b));
            for (a.textures, b.textures) |ta, tb| same = same and std.mem.eql(u8, std.mem.sliceAsBytes(ta.image.pixels), std.mem.sliceAsBytes(tb.image.pixels));
        };
        for (0..4) |k| {
            const t = turnTarget(scene, @intCast(k), @intCast(k), "");
            same = same and std.mem.eql(u8, std.mem.sliceAsBytes(turns[k].image.pixels), std.mem.sliceAsBytes((try model3d.render(arena, again[0], "1", @intCast(k), @intCast(k), t.eye.anchor, t.size)).image.pixels));
        }
        try w.print("],\r\n\"same\":{}}}\r\n", .{same});
        try writeOut(ctx, folder, "model.json", json.written());
        try ctx.out.print("wrote {s}: {d} models, atlas {d}x{d}; N against 2003: outline {d:.4}, colour {d:.2}; holes {d}; same bytes {}\n", .{ folder, scene.full3d.len, intact.width, intact.height, n_match.iou, n_match.colour, holes, same });
    }
    try writeDeps(ctx);
}


/// `full3d-review`: every object `model3d` wrote into `<models-dir>` laid out
/// for the maintainer, by number, with a contents index as the shapes
/// review's: per object a page with the 2003 picture, the render at N and
/// their difference, the mesh's creases, the atlas and its paint map and the
/// numbers; a page with the four turns under the 2003 light and under the
/// world's sun; and where it has damage states a page with each at N against
/// its 2003 picture and turned. Beside the PDF `full3d.csv` (the numbers of
/// every object) and `full3d-ranked.csv` (the objects that most need paint
/// and those whose mesh most needs fixing, worst first).
fn full3dReviewCommand(ctx: *bk_tool.Context, out_arg: []const u8, models_arg: []const u8) !void {
    const arena = ctx.arena;
    const out = try bk_tool.inputPath(arena, out_arg);
    const models = try bk_tool.inputPath(arena, models_arg);
    var reports: std.ArrayList(Full3dReport) = .empty;
    for (try entries(arena, ctx.io, models, .directory)) |name| {
        const report = (try readJson(Full3dReport, ctx, try std.fs.path.join(arena, &.{ models, name, "model.json" }))) orelse continue;
        try reports.append(arena, report);
    }
    std.mem.sort(Full3dReport, reports.items, {}, struct {
        fn less(_: void, a: Full3dReport, b: Full3dReport) bool {
            return a.id < b.id;
        }
    }.less);
    const items = reports.items;
    const page_w = 842.0;
    const page_h = 595.0;
    var pdf = try bk_pdf.begin(arena);

    // The distributions over all objects.
    const Stat = struct {
        fn median(gpa: std.mem.Allocator, values: []const f64) !f64 {
            if (values.len == 0) return 0;
            const sorted = try gpa.dupe(f64, values);
            std.mem.sort(f64, sorted, {}, std.sort.asc(f64));
            return sorted[sorted.len / 2];
        }
        fn lowest(values: []const f64) f64 {
            return if (values.len == 0) 0 else std.mem.min(f64, values);
        }
        fn highest(values: []const f64) f64 {
            return if (values.len == 0) 0 else std.mem.max(f64, values);
        }
    };
    var n_ious: std.ArrayList(f64) = .empty;
    var colours: std.ArrayList(f64) = .empty;
    var guessed: std.ArrayList(f64) = .empty;
    var damage_ious: std.ArrayList(f64) = .empty;
    var holes: usize = 0;
    var different: usize = 0;
    var pages: usize = 0;
    for (items) |r| {
        try n_ious.append(arena, r.n_iou);
        try colours.append(arena, r.n_colour);
        try guessed.append(arena, r.textures[0].guessed);
        for (r.damage) |d| try damage_ious.append(arena, d.iou);
        holes += r.holes;
        for (r.damage) |d| holes += d.holes;
        different += @intFromBool(!r.same);
        pages += 2 + @as(usize, @intFromBool(r.damage.len > 0));
    }
    const summary = try std.fmt.allocPrint(arena, "{d} objects, {d} damage states; N outline IoU median {d:.3} (lowest {d:.3}), N colour median {d:.2} (highest {d:.2}), guessed texels median {d:.0}%, damage states at N outline IoU median {d:.3} (lowest {d:.3}); holes {d}; second build different: {d}", .{
        items.len,                                 damage_ious.items.len,                  try Stat.median(arena, n_ious.items), Stat.lowest(n_ious.items),
        try Stat.median(arena, colours.items),     Stat.highest(colours.items),            100 * try Stat.median(arena, guessed.items),
        try Stat.median(arena, damage_ious.items), Stat.lowest(damage_ious.items),         holes,                                different,
    });

    // Contents: the convention, then two columns of entries.
    const per_column = 46;
    const first_rows = 30;
    const contents_pages: usize = if (items.len <= 2 * first_rows) 1 else 1 + (items.len - 2 * first_rows + 2 * per_column - 1) / (2 * per_column);
    var at: usize = 0;
    var next_page: usize = contents_pages + 1;
    for (0..contents_pages) |cp| {
        var page: bk_pdf.Page = .init(arena);
        var top: f64 = page_h - 40;
        if (cp == 0) {
            try page.text(30, top, 20, true, .{ 0, 0, 0 }, "Blitzkrieg full 3D objects: every building and object as a textured model");
            top -= 22;
            const lines = [_][]const u8{
                "Each object (its number as in shapes.pdf) is a model of its tracked mesh, its atlas painted from the 2003 pictures, rendered to the four",
                "turns: <number>.N is turn 0, the side its 2003 picture shows, E, S and W the turns one, two and three presses of the Game's turn key",
                "show, under the 2003 light fixed to the camera and lit by the world's sun. A damage state is a model of its own: the intact one cut",
                "where the damaged 2003 picture at N no longer reaches (walls above the outline gone, roofs lowered onto it), painted from that",
                "picture. Magenta paint is guessed (material paint: no 2003 picture sees it), the first place to paint. * marks a mesh to review.",
            };
            for (lines) |l| {
                try page.text(30, top, 9.5, false, .{ 0, 0, 0 }, l);
                top -= 12;
            }
            const wrapped = try wrapText(arena, summary, 170);
            for (wrapped) |l| {
                try page.text(30, top, 9.5, true, .{ 0.1, 0.3, 0.4 }, l);
                top -= 12;
            }
            top -= 8;
        }
        const rows: usize = if (cp == 0) first_rows else per_column;
        for (0..2) |column| for (0..rows) |row| {
            if (at == items.len) break;
            const r = items[at];
            const y = top - @as(f64, @floatFromInt(row)) * 11.5;
            const x = 30 + @as(f64, @floatFromInt(column)) * 400;
            try page.text(x, y, 9, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}", .{r.id}));
            try page.text(x + 32, y, 8, false, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{s}{s}  N {d:.2}{s}", .{ r.path, if (r.needs_review) " *" else "", r.n_iou, if (r.damage.len > 0) try std.fmt.allocPrint(arena, ", {d} damage states", .{r.damage.len}) else "" }));
            try page.text(x + 360, y, 9, false, .{ 0.3, 0.3, 0.3 }, try std.fmt.allocPrint(arena, "p. {d}", .{next_page}));
            next_page += 2 + @as(usize, @intFromBool(r.damage.len > 0));
            at += 1;
        };
        _ = try pdf.page(page_w, page_h, page.out.written(), &.{});
    }
    try pdf.bookmark("Contents", 0);

    var csv: std.Io.Writer.Allocating = .init(arena);
    try csv.writer.writeAll("id,path,status,needs_review,triangles,groups,charts,atlas,seen,guessed,n_iou,n_mesh_iou,n_colour,holes,guessed_pixels_e_s_w,sibling,sibling_turn,sibling_laid_iou,sibling_iou,sibling_colour,damage_states,damage_iou,damage_intact_iou,damage_colour,damage_holes,winter_guessed,same_bytes\r\n");
    for (items) |r| {
        const folder = try std.fs.path.join(arena, &.{ models, try std.fmt.allocPrint(arena, "{d:0>3}", .{r.id}) });
        const review = try std.fs.path.join(arena, &.{ folder, "review" });
        // The object's pictures, gone once its pages hold them (deflated).
        var scratch: std.heap.ArenaAllocator = .init(std.heap.page_allocator);
        defer scratch.deinit();
        const Loader = struct {
            c: *bk_tool.Context,
            gpa: std.mem.Allocator,
            fn f(l: @This(), d: []const u8, name: []const u8) !Image {
                const bytes = try std.Io.Dir.cwd().readFileAlloc(l.c.io, try std.fs.path.join(l.gpa, &.{ d, name }), l.gpa, .limited(256 * 1024 * 1024));
                return readPng(l.gpa, bytes);
            }
        };
        const loader: Loader = .{ .c = ctx, .gpa = scratch.allocator() };
        var damage_text: [4]std.Io.Writer.Allocating = .{ .init(arena), .init(arena), .init(arena), .init(arena) };
        for (r.damage, 0..) |d, i| {
            const sep = if (i > 0) "/" else "";
            try damage_text[0].writer.print("{s}{d:.4}", .{ sep, d.iou });
            try damage_text[1].writer.print("{s}{d:.4}", .{ sep, d.intact_iou });
            try damage_text[2].writer.print("{s}{d:.2}", .{ sep, d.colour });
            try damage_text[3].writer.print("{s}{d}", .{ sep, d.holes });
        }
        var winter: ?f64 = null;
        for (r.textures) |t| if (std.mem.eql(u8, t.state, "1w")) {
            winter = t.guessed;
        };
        try csv.writer.print("{d},{s},{s},{},{d},{d},{d},{d}x{d},{d:.4},{d:.4},{d:.4},{d:.4},{d:.2},{d},{d}/{d}/{d},{s},{d},{d:.4},{d:.4},{d:.2},{d},{s},{s},{s},{s},{s},{}\r\n", .{
            r.id,                                  r.path,                                    r.status,                                   r.needs_review,                         r.triangles,                                             r.groups.len,         r.charts,                 r.atlas[0],               r.atlas[1], r.textures[0].seen, r.textures[0].guessed, r.n_iou, r.n_mesh_iou, r.n_colour, r.holes,
            r.guessed_pixels[1],                   r.guessed_pixels[2],                       r.guessed_pixels[3],                        if (r.sibling) |s| s.name else "",      if (r.sibling) |s| @as(u8, s.turn) else 0,               if (r.sibling) |s| s.laid else 0, if (r.sibling) |s| s.iou else 0, if (r.sibling) |s| s.colour else 0,
            r.damage.len,                          damage_text[0].written(),                  damage_text[1].written(),                   damage_text[2].written(),               damage_text[3].written(),                                if (winter) |g| try std.fmt.allocPrint(arena, "{d:.4}", .{g}) else "", r.same,
        });

        // Page 1: the model and its paint against the 2003 picture.
        var page: bk_pdf.Page = .init(arena);
        var images: std.ArrayList(u32) = .empty;
        try page.text(30, page_h - 42, 26, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}", .{r.id}));
        try page.text(100, page_h - 34, 13, true, .{ 0, 0, 0 }, r.path);
        try page.text(100, page_h - 50, 9, false, .{ 0.12, 0.35, 0.45 }, try std.fmt.allocPrint(arena, "full 3D model: {d} vertices, {d} triangles, {d} charts, atlas {d} x {d} at {d} texels a world unit; mesh {s} ({s}{s})", .{ r.vertices, r.triangles, r.charts, r.atlas[0], r.atlas[1], model3d.density, r.mesh, r.status, if (r.needs_review) ", needs review" else "" }));
        _ = try paragraph(&page, arena, 100, page_h - 62, try std.fmt.allocPrint(arena, "Parts (groups and Resource Editor locators): {s}", .{try std.mem.join(arena, ", ", r.groups)}), 150, 7.5, .{ 0.2, 0.2, 0.2 });
        const box = [2]f64{ 190, 170 };
        const row_y = 330.0;
        const labels = [_][]const u8{ "2003 picture (N)", "render N, 2003 light", "difference (red: 2003 only, blue: model only)", "mesh creases at N" };
        for (labels, 0..) |label, i| try page.text(30 + @as(f64, @floatFromInt(i)) * 200, row_y + box[1] + 6, 8, false, .{ 0.2, 0.2, 0.2 }, label);
        for ([_][]const u8{ "pic.png", "n.png", "diff.png" }, 0..) |name, i| {
            const image = try loader.f(review, name);
            const pid = try pdf.image(image.width, image.height, image.pixels);
            try images.append(arena, pid);
            _ = try placePicture(&page, image, pid, 30 + @as(f64, @floatFromInt(i)) * 200, row_y, box);
        }
        try page.polygon(&.{ .{ 630, row_y }, .{ 630 + box[0], row_y }, .{ 630 + box[0], row_y + box[1] }, .{ 630, row_y + box[1] } }, .{ 0.97, 0.97, 0.97 }, null, 0);
        if (try mesh.load(arena, ctx.io, mesh.meshes_dir, r.id)) |m| try drawCreases(arena, &page, m, try mesh.triangles(arena, m), m.alignment, .{ 640, row_y + 10, 620 + box[0], row_y + box[1] - 10 });
        var y = try paragraph(&page, arena, 30, row_y - 14, try std.fmt.allocPrint(arena, "Render N against the 2003 picture, on its canvas at its zero cross (no shift): outline IoU {d:.4} (the mesh alone {d:.4}), mean colour difference {d:.2} of 255 where both are opaque. Holes (samples on unpainted texels) in N, the turns and those under the world's sun{s}: {d}. A second build and render: {s}.", .{ r.n_iou, r.n_mesh_iou, r.n_colour, if (r.damage.len > 0) " and the damage states" else "", r.holes, if (r.same) "the same bytes" else "DIFFERENT bytes" }), 150, 8, .{ 0.1, 0.1, 0.1 });
        if (r.sibling) |s| y = try paragraph(&page, arena, 30, y - 2, try std.fmt.allocPrint(arena, "The 2003 sibling {s} against turn {d}.{c} that shows its side: outline IoU {d:.4} where the footprints lay it, {d:.4} at the best shift, colour difference {d:.2} there.", .{ s.name, r.id, side_letters[s.turn], s.laid, s.iou, s.colour }), 150, 8, .{ 0.1, 0.1, 0.1 });
        var shares: std.Io.Writer.Allocating = .init(arena);
        for (r.textures, 0..) |t, i| try shares.writer.print("{s}{s} {d:.0}%", .{ if (i > 0) ", " else "", t.state, 100 * t.guessed });
        try page.text(30, 256, 8, false, .{ 0.2, 0.2, 0.2 }, try std.fmt.allocPrint(arena, "texture atlas, summer (1_h.dds): seen {d:.1}%, guessed {d:.1}% of the texels of faces that look up or sideways", .{ r.textures[0].seen * 100, r.textures[0].guessed * 100 }));
        try page.text(430, 256, 8, false, .{ 0.2, 0.2, 0.2 }, try std.fmt.allocPrint(arena, "paint source: green from a 2003 picture, magenta guessed. Guessed: {s}", .{shares.written()}));
        for ([_][]const u8{ "atlas.png", "paint.png" }, [_]f64{ 30, 430 }) |name, x| {
            const image = try loader.f(review, name);
            const pid = try pdf.image(image.width, image.height, image.pixels);
            try images.append(arena, pid);
            const atlas_box = [2]f64{ 380, 220 };
            _ = try placeScaled(&page, image, pid, x, 30, atlas_box, @min(atlas_box[0] / @as(f64, @floatFromInt(image.width)), atlas_box[1] / @as(f64, @floatFromInt(image.height))));
        }
        const first = try pdf.page(page_w, page_h, page.out.written(), images.items);
        try pdf.bookmark(try std.fmt.allocPrint(arena, "{d} {s}", .{ r.id, r.path }), first);

        // Page 2: the four turns.
        page = .init(arena);
        images = .empty;
        try page.text(30, page_h - 34, 16, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}  {s}: the model turned", .{ r.id, r.path }));
        var turns: [2][4]Image = undefined;
        turns[0][0] = try loader.f(review, "n.png");
        turns[1][0] = turns[0][0];
        var scale: f64 = 3;
        for (1..4) |k| {
            turns[0][k] = try loader.f(review, try std.fmt.allocPrint(arena, "t{d}.png", .{k}));
            turns[1][k] = try loader.f(review, try std.fmt.allocPrint(arena, "s{d}.png", .{k}));
        }
        for (turns[0]) |t| scale = @min(scale, 182 / @as(f64, @floatFromInt(t.width)), 200 / @as(f64, @floatFromInt(t.height)));
        for (0..2) |row| {
            const y0 = 290 - @as(f64, @floatFromInt(row)) * 265;
            try page.text(30, y0 + 238, 9, true, .{ 0, 0, 0 }, if (row == 1) "lit by the world's sun at the camera step that shows each turn (_r<k>_l<k>)" else "under the 2003 light, fixed to the camera (in the Game N is the 2003 sprite, E, S, W these, _r<k>)");
            for (0..4) |k| {
                const x = 30 + @as(f64, @floatFromInt(k)) * 197;
                try page.polygon(&.{ .{ x, y0 }, .{ x + 190, y0 }, .{ x + 190, y0 + 232 }, .{ x, y0 + 232 } }, .{ 0.97, 0.97, 0.97 }, .{ 0.75, 0.75, 0.75 }, 0.5);
                try page.text(x + 4, y0 + 216, 12, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}.{c}", .{ r.id, side_letters[k] }));
                const image = turns[row][k];
                const pid = try pdf.image(image.width, image.height, image.pixels);
                try images.append(arena, pid);
                _ = try placeScaled(&page, image, pid, x + 4, y0 + 12, .{ 182, 200 }, if (k == 0) @min(3, 182 / @as(f64, @floatFromInt(image.width)), 200 / @as(f64, @floatFromInt(image.height))) else scale);
                if (row == 0) try page.text(x + 4, y0 + 4, 7, false, .{ 0.3, 0.3, 0.3 }, try std.fmt.allocPrint(arena, "{d} pixels of guessed paint", .{r.guessed_pixels[k]}));
            }
        }
        _ = try pdf.page(page_w, page_h, page.out.written(), images.items);

        // Page 3: the damage states, compact.
        if (r.damage.len == 0) continue;
        page = .init(arena);
        images = .empty;
        try page.text(30, page_h - 34, 16, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "{d}  {s}: the damage states", .{ r.id, r.path }));
        try page.text(30, page_h - 50, 8.5, false, .{ 0.2, 0.2, 0.2 }, "Each a model of its own: the intact model cut where the damaged 2003 picture at N no longer reaches, painted from it; its N, its difference, its turns.");
        for (r.damage, 0..) |d, row| {
            const y0 = 300 - @as(f64, @floatFromInt(row)) * 250;
            try page.text(30, y0 + 228, 9, true, .{ 0, 0, 0 }, try std.fmt.allocPrint(arena, "state {s} ({d} triangles): at N outline IoU {d:.4} (the intact model {d:.4}), colour {d:.2}; holes {d}", .{ d.state, d.triangles, d.iou, d.intact_iou, d.colour, d.holes }));
            const names = [_][]const u8{ "_pic", "_n", "_diff", "_t1", "_t2", "_t3" };
            const titles = [_][]const u8{ "2003 picture", "render N", "difference", "E", "S", "W" };
            for (names, titles, 0..) |suffix, title, i| {
                const x = 30 + @as(f64, @floatFromInt(i)) * 132;
                const image = try loader.f(review, try std.fmt.allocPrint(arena, "d{s}{s}.png", .{ d.state, suffix }));
                const pid = try pdf.image(image.width, image.height, image.pixels);
                try images.append(arena, pid);
                try page.text(x, y0 + 212, 8, i >= 3, .{ 0.2, 0.2, 0.2 }, if (i >= 3) try std.fmt.allocPrint(arena, "{d}.{c} state {s}", .{ r.id, side_letters[i - 2], d.state }) else title);
                _ = try placePicture(&page, image, pid, x, y0, .{ 126, 204 });
            }
        }
        _ = try pdf.page(page_w, page_h, page.out.written(), images.items);
    }

    // The ranked lists: paint first where most of the atlas is guessed, a
    // mesh first where its outline at N matches its picture least (intact,
    // or a damage state's cut).
    var ranked: std.Io.Writer.Allocating = .init(arena);
    try ranked.writer.writeAll("list,rank,id,path,value,why\r\n");
    const order = try arena.alloc(usize, items.len);
    for (order, 0..) |*o, i| o.* = i;
    std.mem.sort(usize, order, items, struct {
        fn less(rs: []const Full3dReport, a: usize, b: usize) bool {
            return rs[a].textures[0].guessed > rs[b].textures[0].guessed or (rs[a].textures[0].guessed == rs[b].textures[0].guessed and rs[a].id < rs[b].id);
        }
    }.less);
    for (order, 1..) |i, rank| {
        const r = items[i];
        try ranked.writer.print("needs paint,{d},{d},{s},{d:.4},guessed share of the summer atlas; N colour {d:.2}\r\n", .{ rank, r.id, r.path, r.textures[0].guessed, r.n_colour });
    }
    const worst = struct {
        fn f(r: Full3dReport) f64 {
            var v = r.n_mesh_iou;
            for (r.damage) |d| v = @min(v, d.iou);
            return v;
        }
    }.f;
    std.mem.sort(usize, order, items, struct {
        fn less(rs: []const Full3dReport, a: usize, b: usize) bool {
            return worst(rs[a]) < worst(rs[b]) or (worst(rs[a]) == worst(rs[b]) and rs[a].id < rs[b].id);
        }
    }.less);
    for (order, 1..) |i, rank| {
        const r = items[i];
        try ranked.writer.print("needs mesh fix,{d},{d},{s},{d:.4},lowest outline IoU at N of the mesh ({d:.4}) and the damage states' cuts{s}\r\n", .{ rank, r.id, r.path, worst(r), r.n_mesh_iou, if (r.needs_review) "; marked needs_review" else "" });
    }

    var text: std.Io.Writer.Allocating = .init(arena);
    try pdf.write(&text.writer);
    const out_dir = std.fs.path.dirname(out) orelse ".";
    try std.Io.Dir.cwd().createDirPath(ctx.io, out_dir);
    try writeOut(ctx, out_dir, std.fs.path.basename(out), text.written());
    try writeOut(ctx, out_dir, "full3d.csv", csv.written());
    try writeOut(ctx, out_dir, "full3d-ranked.csv", ranked.written());
    try ctx.out.print("wrote {s}: {d} pages; {s}\n", .{ out, contents_pages + pages, summary });
}
