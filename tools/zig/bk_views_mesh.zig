//! Mesh shape models (M006 round 5, docs/superpowers/specs/2026-10-25-building-views-design.md):
//! the maintainer's reviewed shapes of every object that turns, imported from
//! his family review (`bk-views import`) into tools/zig/fixtures/bk-views/meshes.
//! A mesh is triangles in the review's frame with an alignment (a yaw, a
//! height factor and a width factor) that turns it to the side its 2003
//! picture shows; the review fits its projection to the picture's opaque box,
//! so its scale and place are the picture's.
//!
//! This file holds the format, the geometry (placing a mesh on a picture,
//! turning it, a depth buffer of its triangles) and the review's own measure
//! of a mesh against its 2003 picture (`evaluate`: outline IoU and edge score
//! at 3 times a 128-pixel grid, as the review's edgefit.py computes them).

const std = @import("std");
const Image = @import("bk_views_pack.zig").Image;

/// The meshes: `<id>.json` per object (3 digits), `shared/<family>.json` for
/// the meshes a joint family's views share, `families.json` the family table.
pub const meshes_dir = "tools/zig/fixtures/bk-views/meshes";

pub const Alignment = struct { angle: f64 = 0, zscale: f64 = 1, ratio: f64 = 1 };

/// The review's own scores of the mesh against its 2003 picture.
pub const Scores = struct { silhouette: f64, edge: f64, combined: f64 };

/// Triangles of one kind (roof, body, timber, ...): vertex indices, three a triangle.
pub const Part = struct { kind: []const u8, triangles: []const u32 };

/// One object's mesh model. `vertices` are x, y, z in a row; a model of a
/// joint family names its `shared` mesh instead of holding one.
pub const Model = struct {
    id: u32,
    path: []const u8,
    family: []const u8 = "",
    measured_side: ?u8 = null,
    joint: bool = false,
    status: []const u8 = "",
    needs_review: bool = false,
    note: []const u8 = "",
    reference: []const u8 = "",
    source: []const u8 = "",
    previous_variant: []const u8 = "",
    alignment: Alignment = .{},
    scores: ?Scores = null,
    shared: ?[]const u8 = null,
    vertices: []const f64 = &.{},
    parts: []const Part = &.{},
};

pub const Shared = struct { name: []const u8, views: []const u32 = &.{}, vertices: []const f64, parts: []const Part };

pub const Family = struct {
    region: []const u8,
    family: []const u8,
    status: []const u8,
    reason: []const u8 = "",
    all_ids: []const u32,
    included: []const u32,
    excluded: []const struct { id: u32, reason: []const u8 } = &.{},
    donor_id: ?u32 = null,
    objective: ?f64 = null,
    views: []const struct { id: u32, side: u8, method: []const u8, alignment: Alignment, silhouette: f64, edge: f64, combined: f64 } = &.{},
    geometry_note: []const u8 = "",
    shared: []const u8 = "",
};

pub const Families = struct { families: []const Family };

fn readJson(comptime T: type, gpa: std.mem.Allocator, io: std.Io, path: []const u8) !?T {
    const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 * 1024 * 1024)) catch |err| switch (err) {
        error.FileNotFound => return null,
        else => return err,
    };
    return try std.json.parseFromSliceLeaky(T, gpa, bytes, .{ .ignore_unknown_fields = true });
}

/// Object `id`'s model with its shared mesh filled in, or null where it has none.
pub fn load(gpa: std.mem.Allocator, io: std.Io, dir: []const u8, id: u32) !?Model {
    var model = (try readJson(Model, gpa, io, try std.fmt.allocPrint(gpa, "{s}/{d:0>3}.json", .{ dir, id }))) orelse return null;
    if (model.shared) |name| {
        const shared = (try readJson(Shared, gpa, io, try std.fmt.allocPrint(gpa, "{s}/shared/{s}.json", .{ dir, name }))) orelse return error.MissingSharedMesh;
        model.vertices = shared.vertices;
        model.parts = shared.parts;
    }
    return model;
}

pub fn loadFamilies(gpa: std.mem.Allocator, io: std.Io, dir: []const u8) ![]const Family {
    const f = (try readJson(Families, gpa, io, try std.fmt.allocPrint(gpa, "{s}/families.json", .{dir}))) orelse return &.{};
    return f.families;
}

/// A triangle: three vertex indices and its part.
pub const Tri = struct { v: [3]u32, part: u32 };

pub fn triangles(gpa: std.mem.Allocator, model: Model) ![]Tri {
    var n: usize = 0;
    for (model.parts) |p| n += p.triangles.len / 3;
    const out = try gpa.alloc(Tri, n);
    var at: usize = 0;
    for (model.parts, 0..) |p, k| {
        var i: usize = 0;
        while (i + 3 <= p.triangles.len) : (i += 3) {
            out[at] = .{ .v = p.triangles[i..][0..3].*, .part = @intCast(k) };
            at += 1;
        }
    }
    return out;
}

pub fn vertex(model: Model, i: usize) [3]f64 {
    return model.vertices[i * 3 ..][0..3].*;
}

pub fn vertexCount(model: Model) usize {
    return model.vertices.len / 3;
}

// ---------------------------------------------------------------------------
// Geometry

/// The 2003 camera's rows (bk-views camera-2003.json): picture x and y (down)
/// of a point of the world, and the direction towards the eye.
pub const rows = [2][3]f64{ .{ 0.7071067812, 0.7071067812, 0 }, .{ 0.3535533906, -0.3535533906, -0.8660254038 } };
pub const toward_eye = [3]f64{ 0.6123724357, -0.6123724357, 0.5 };

pub fn project(p: [3]f64) [2]f64 {
    return .{ rows[0][0] * p[0] + rows[0][1] * p[1] + rows[0][2] * p[2], rows[1][0] * p[0] + rows[1][1] * p[1] + rows[1][2] * p[2] };
}

pub fn dot(a: [3]f64, b: [3]f64) f64 {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

pub fn cross(a: [3]f64, b: [3]f64) [3]f64 {
    return .{ a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}

pub fn unit(a: [3]f64) [3]f64 {
    const l = @sqrt(dot(a, a));
    if (l < 1e-12) return .{ 0, 0, 1 };
    return .{ a[0] / l, a[1] / l, a[2] / l };
}

/// k quarter turns of the world: (x, y) to (-y, x) each (bk-views turnPoint).
pub fn quarter(p: [2]f64, k: u2) [2]f64 {
    var q = p;
    for (0..k) |_| {
        const x = q[0];
        q = .{ -q[1], x };
    }
    return q;
}

/// A mesh point as the review turns it: widened by `ratio`, raised by
/// `zscale`, yawed by the alignment's angle and `turn` quarter turns.
pub fn transformed(p: [3]f64, a: Alignment, turn: u2) [3]f64 {
    const angle = std.math.degreesToRadians(a.angle + 90 * @as(f64, @floatFromInt(turn)));
    const x = p[0] * a.ratio;
    const c = @cos(angle);
    const s = @sin(angle);
    return .{ x * c - p[1] * s, x * s + p[1] * c, p[2] * a.zscale };
}

/// Where a mesh stands in a variant's world (world units from its zero
/// cross): `scale` times the transformed mesh plus `shift`.
pub const Placement = struct {
    alignment: Alignment = .{},
    scale: f64 = 1,
    shift: [3]f64 = .{ 0, 0, 0 },

    pub fn world(p: Placement, m: [3]f64) [3]f64 {
        const t = transformed(m, p.alignment, 0);
        return .{ p.scale * t[0] + p.shift[0], p.scale * t[1] + p.shift[1], p.scale * t[2] + p.shift[2] };
    }

    /// A direction of the world as a direction of the mesh.
    pub fn toMesh(p: Placement, d: [3]f64) [3]f64 {
        const angle = std.math.degreesToRadians(p.alignment.angle);
        const c = @cos(angle);
        const s = @sin(angle);
        return .{ (d[0] * c + d[1] * s) / p.alignment.ratio / p.scale, (-d[0] * s + d[1] * c) / p.scale, d[2] / p.alignment.zscale / p.scale };
    }
};

/// The opaque box of a picture: x0, y0, x1, y1 (pixel edges), or null.
pub fn opaqueBox(picture: Image) ?[4]f64 {
    var box = [4]usize{ picture.width, picture.height, 0, 0 };
    for (0..picture.height) |y| for (0..picture.width) |x| {
        if (picture.pixels[y * picture.width + x][3] == 0) continue;
        box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x + 1), @max(box[3], y + 1) };
    };
    if (box[2] == 0) return null;
    return .{ @floatFromInt(box[0]), @floatFromInt(box[1]), @floatFromInt(box[2]), @floatFromInt(box[3]) };
}

/// The mesh on a variant's 2003 picture as the review lays it: its turn-0
/// projection's box centred on the picture's opaque box, the longer sides
/// equal, and its lowest point on the ground; `anchor` is the picture's
/// zero cross.
pub fn place(model: Model, picture: Image, anchor: [2]f64) ?Placement {
    const box = opaqueBox(picture) orelse return null;
    if (vertexCount(model) == 0) return null;
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    var zmin = std.math.inf(f64);
    for (0..vertexCount(model)) |i| {
        const t = transformed(vertex(model, i), model.alignment, 0);
        const s = project(t);
        lo = .{ @min(lo[0], s[0]), @min(lo[1], s[1]) };
        hi = .{ @max(hi[0], s[0]), @max(hi[1], s[1]) };
        zmin = @min(zmin, t[2]);
    }
    const extent = @max(hi[0] - lo[0], hi[1] - lo[1]);
    if (extent <= 0) return null;
    const scale = @max(box[2] - box[0], box[3] - box[1]) / extent;
    const tz = -scale * zmin;
    // scale * centre + project(shift) + anchor = the box's centre.
    const dx = (box[0] + box[2]) / 2 - anchor[0] - scale * (lo[0] + hi[0]) / 2;
    const dy = (box[1] + box[3]) / 2 - anchor[1] - scale * (lo[1] + hi[1]) / 2;
    const sum = dx / rows[0][0];
    const diff = (dy - rows[1][2] * tz) / rows[1][0];
    return .{ .alignment = model.alignment, .scale = scale, .shift = .{ (sum + diff) / 2, (sum - diff) / 2, tz } };
}

/// An affine map of mesh points to picture x, y and depth (larger nearer).
pub const View = struct {
    m: [3][4]f64,

    pub fn apply(v: View, p: [3]f64) [3]f64 {
        var out: [3]f64 = undefined;
        for (&out, v.m) |*o, r| o.* = r[0] * p[0] + r[1] * p[1] + r[2] * p[2] + r[3];
        return out;
    }

    /// The mesh point under picture point (x, y) at `depth`.
    pub fn unapply(v: View, s: [3]f64) [3]f64 {
        const a = [3][3]f64{ v.m[0][0..3].*, v.m[1][0..3].*, v.m[2][0..3].* };
        const b = [3]f64{ s[0] - v.m[0][3], s[1] - v.m[1][3], s[2] - v.m[2][3] };
        const det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
        var out: [3]f64 = undefined;
        for (0..3) |c| {
            var m = a;
            for (0..3) |r| m[r][c] = b[r];
            out[c] = (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0])) / det;
        }
        return out;
    }
};

/// The view of a placed mesh by an eye turned `turn` quarter turns whose
/// zero cross lies at `anchor` (bk-views Eye.project).
pub fn view(p: Placement, turn: u2, anchor: [2]f64) View {
    const f = struct {
        fn f(pl: Placement, k: u2, a: [2]f64, m: [3]f64) [3]f64 {
            const w = pl.world(m);
            const q = quarter(.{ w[0], w[1] }, k);
            const t = [3]f64{ q[0], q[1], w[2] };
            const s = project(t);
            return .{ s[0] + a[0], s[1] + a[1], dot(t, toward_eye) };
        }
    }.f;
    const o = f(p, turn, anchor, .{ 0, 0, 0 });
    var out: View = undefined;
    for (0..3) |r| out.m[r][3] = o[r];
    for (0..3) |c| {
        var e = [3]f64{ 0, 0, 0 };
        e[c] = 1;
        const q = f(p, turn, anchor, e);
        for (0..3) |r| out.m[r][c] = q[r] - o[r];
    }
    return out;
}

/// The normal of triangle `t` in a placed mesh's world, turned towards an
/// eye turned `turn` (the side of a face a picture shows is its outside).
pub fn worldNormal(model: Model, p: Placement, t: Tri, turn: u2) [3]f64 {
    const n = windingNormal(model, p, t);
    const back = quarter(.{ toward_eye[0], toward_eye[1] }, 0 -% turn);
    return if (dot(n, .{ back[0], back[1], toward_eye[2] }) < 0) .{ -n[0], -n[1], -n[2] } else n;
}

/// The normal of triangle `t` by its corners' order. Placements turn and
/// stretch without mirroring, so a triangle's winding means one side in
/// every view.
pub fn windingNormal(model: Model, p: Placement, t: Tri) [3]f64 {
    const a = p.world(vertex(model, t.v[0]));
    const b = p.world(vertex(model, t.v[1]));
    const c = p.world(vertex(model, t.v[2]));
    return unit(cross(.{ b[0] - a[0], b[1] - a[1], b[2] - a[2] }, .{ c[0] - a[0], c[1] - a[1], c[2] - a[2] }));
}

/// A depth buffer of a mesh's triangles: per pixel the nearest depth and its
/// triangle (`none` where no triangle covers the pixel's centre).
pub const Raster = struct {
    width: u32,
    height: u32,
    depth: []f64,
    tri: []u32,

    pub const none = std.math.maxInt(u32);

    pub fn at(r: Raster, x: f64, y: f64) ?usize {
        if (!(x >= 0 and y >= 0)) return null;
        const xi: usize = @intFromFloat(@floor(x));
        const yi: usize = @intFromFloat(@floor(y));
        if (xi >= r.width or yi >= r.height) return null;
        return yi * r.width + xi;
    }
};

pub fn viewVertices(gpa: std.mem.Allocator, model: Model, v: View) ![][3]f64 {
    const out = try gpa.alloc([3]f64, vertexCount(model));
    for (out, 0..) |*o, i| o.* = v.apply(vertex(model, i));
    return out;
}

pub fn raster(gpa: std.mem.Allocator, points: []const [3]f64, tris: []const Tri, width: u32, height: u32) !Raster {
    const n = @as(usize, width) * height;
    const out: Raster = .{ .width = width, .height = height, .depth = try gpa.alloc(f64, n), .tri = try gpa.alloc(u32, n) };
    @memset(out.depth, -std.math.inf(f64));
    @memset(out.tri, Raster.none);
    for (tris, 0..) |t, ti| {
        const a = points[t.v[0]];
        const b = points[t.v[1]];
        const c = points[t.v[2]];
        const den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
        if (@abs(den) < 1e-12) continue;
        const x0 = @max(0, @floor(@min(a[0], b[0], c[0])));
        const x1 = @min(@as(f64, @floatFromInt(width)) - 1, @ceil(@max(a[0], b[0], c[0])));
        const y0 = @max(0, @floor(@min(a[1], b[1], c[1])));
        const y1 = @min(@as(f64, @floatFromInt(height)) - 1, @ceil(@max(a[1], b[1], c[1])));
        if (x0 > x1 or y0 > y1) continue;
        var y = y0;
        while (y <= y1) : (y += 1) {
            var x = x0;
            while (x <= x1) : (x += 1) {
                const px = x + 0.5;
                const py = y + 0.5;
                const u = ((b[1] - c[1]) * (px - c[0]) + (c[0] - b[0]) * (py - c[1])) / den;
                const v = ((c[1] - a[1]) * (px - c[0]) + (a[0] - c[0]) * (py - c[1])) / den;
                const w = 1 - u - v;
                if (u < -1e-9 or v < -1e-9 or w < -1e-9) continue;
                const d = u * a[2] + v * b[2] + w * c[2];
                const i = @as(usize, @intFromFloat(y)) * width + @as(usize, @intFromFloat(x));
                if (d <= out.depth[i]) continue;
                out.depth[i] = d;
                out.tri[i] = @intCast(ti);
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// The review's measure (edgefit.py `source` and `evaluate`)

/// The working grid: 3 times the first review's 128 pixels, the subject in
/// a box of 348.
pub const grid = 384;
const subject = 348.0;

/// A 2003 picture as the review measures it: its opaque box scaled to the
/// subject box (Lanczos, on premultiplied colour, as Pillow resizes RGBA),
/// the outline (alpha over 100), and each pixel's distance to the nearest
/// edge (Canny 40/100 of the blurred grey inside the outline, and the
/// outline's border).
pub const Target = struct { mask: []bool, distance: []f64 };

pub const Metric = struct { silhouette: f64, edge: f64, combined: f64 };

pub fn target(gpa: std.mem.Allocator, picture: Image) !?Target {
    const box = opaqueBox(picture) orelse return null;
    const bx: usize = @intFromFloat(box[0]);
    const by: usize = @intFromFloat(box[1]);
    const w: usize = @intFromFloat(box[2] - box[0]);
    const h: usize = @intFromFloat(box[3] - box[1]);
    const s = subject / @as(f64, @floatFromInt(@max(w, h)));
    const ow: usize = @max(1, @as(usize, @intFromFloat(@round(@as(f64, @floatFromInt(w)) * s))));
    const oh: usize = @max(1, @as(usize, @intFromFloat(@round(@as(f64, @floatFromInt(h)) * s))));
    // Premultiplied, as Pillow converts RGBA to RGBa before it resamples.
    const crop = try gpa.alloc([4]u8, w * h);
    defer gpa.free(crop);
    for (0..h) |y| for (0..w) |x| {
        const p = picture.pixels[(by + y) * picture.width + bx + x];
        var q = p;
        for (0..3) |c| {
            const t: u32 = @as(u32, p[c]) * p[3] + 128;
            q[c] = @intCast(((t >> 8) + t) >> 8);
        }
        crop[y * w + x] = q;
    };
    const across = try resample(gpa, crop, w, h, ow, true);
    defer gpa.free(across);
    const resized = try resample(gpa, across, ow, h, oh, false);
    defer gpa.free(resized);
    const n = grid * grid;
    const mask = try gpa.alloc(bool, n);
    @memset(mask, false);
    const grey = try gpa.alloc(u8, n);
    defer gpa.free(grey);
    @memset(grey, 0);
    const ox = (grid - @as(i64, @intCast(ow))) >> 1;
    const oy = (grid - @as(i64, @intCast(oh))) >> 1;
    for (0..oh) |y| for (0..ow) |x| {
        const gx = ox + @as(i64, @intCast(x));
        const gy = oy + @as(i64, @intCast(y));
        if (gx < 0 or gy < 0 or gx >= grid or gy >= grid) continue;
        var p = resized[y * ow + x];
        // Back to straight alpha (Pillow's RGBa to RGBA).
        if (p[3] != 0 and p[3] != 255) for (0..3) |c| {
            p[c] = @intCast(@min(255, (255 * @as(u32, p[c])) / p[3]));
        };
        const i: usize = @intCast(gy * grid + gx);
        mask[i] = p[3] > 100;
        grey[i] = @intCast((@as(u32, p[0]) * 4899 + @as(u32, p[1]) * 9617 + @as(u32, p[2]) * 1868 + (1 << 13)) >> 14);
    };
    const blurred = try gaussian(gpa, grey);
    defer gpa.free(blurred);
    const edges = try canny(gpa, blurred, 40, 100);
    defer gpa.free(edges);
    // Edges inside the outline eroded by one pixel, and the outline's border.
    const feature = try gpa.alloc(bool, n);
    defer gpa.free(feature);
    for (0..grid) |y| for (0..grid) |x| {
        var all = true;
        var any = false;
        for (0..3) |dy| for (0..3) |dx| {
            const xx = @as(i64, @intCast(x)) + @as(i64, @intCast(dx)) - 1;
            const yy = @as(i64, @intCast(y)) + @as(i64, @intCast(dy)) - 1;
            if (xx < 0 or yy < 0 or xx >= grid or yy >= grid) continue;
            const m = mask[@intCast(yy * grid + xx)];
            all = all and m;
            any = any or m;
        };
        feature[y * grid + x] = (edges[y * grid + x] and all) or (any and !all);
    };
    return .{ .mask = mask, .distance = try distance(gpa, feature) };
}

/// Pillow's Lanczos resampling of one axis (8 bits, 22-bit fixed point).
fn resample(gpa: std.mem.Allocator, in: []const [4]u8, w: usize, h: usize, size: usize, horizontal: bool) ![][4]u8 {
    const in_size = if (horizontal) w else h;
    const out = try gpa.alloc([4]u8, if (horizontal) size * h else w * size);
    const scale = @as(f64, @floatFromInt(in_size)) / @as(f64, @floatFromInt(size));
    const filterscale = @max(1, scale);
    const support = 3 * filterscale;
    const ksize: usize = @as(usize, @intFromFloat(@ceil(support))) * 2 + 1;
    const k = try gpa.alloc(i64, ksize);
    defer gpa.free(k);
    const kf = try gpa.alloc(f64, ksize);
    defer gpa.free(kf);
    for (0..size) |xx| {
        const centre = (@as(f64, @floatFromInt(xx)) + 0.5) * scale;
        const xmin: usize = @intFromFloat(@max(0, @trunc(centre - support + 0.5)));
        const xmax: usize = @min(in_size, @as(usize, @intFromFloat(@max(0, @trunc(centre + support + 0.5))))) - xmin;
        var ww: f64 = 0;
        for (0..xmax) |x| {
            kf[x] = lanczos((@as(f64, @floatFromInt(x + xmin)) - centre + 0.5) / filterscale);
            ww += kf[x];
        }
        for (0..xmax) |x| {
            const v = if (ww != 0) kf[x] / ww else kf[x];
            k[x] = @intFromFloat(if (v < 0) @trunc(-0.5 + v * (1 << 22)) else @trunc(0.5 + v * (1 << 22)));
        }
        const lines = if (horizontal) h else w;
        for (0..lines) |li| {
            var sum = [4]i64{ 1 << 21, 1 << 21, 1 << 21, 1 << 21 };
            for (0..xmax) |x| {
                const p = if (horizontal) in[li * w + xmin + x] else in[(xmin + x) * w + li];
                for (0..4) |c| sum[c] += @as(i64, p[c]) * k[x];
            }
            var q: [4]u8 = undefined;
            for (&q, sum) |*o, v| o.* = @intCast(std.math.clamp(v >> 22, 0, 255));
            if (horizontal) out[li * size + xx] = q else out[xx * w + li] = q;
        }
    }
    return out;
}

fn lanczos(x: f64) f64 {
    if (x < -3 or x >= 3) return 0;
    return sinc(x) * sinc(x / 3);
}

fn sinc(x: f64) f64 {
    if (x == 0) return 1;
    const t = x * std.math.pi;
    return @sin(t) / t;
}

/// OpenCV's 3 x 3 Gaussian blur of sigma 0.6 on 8 bits (fixed point,
/// kernel 43, 170, 43 of 256; reflected border).
fn gaussian(gpa: std.mem.Allocator, in: []const u8) ![]u8 {
    const k = [3]u32{ 43, 170, 43 };
    const across = try gpa.alloc(u32, in.len);
    defer gpa.free(across);
    const out = try gpa.alloc(u8, in.len);
    for (0..grid) |y| for (0..grid) |x| {
        var s: u32 = 0;
        for (0..3) |d| s += k[d] * in[y * grid + reflect(@as(i64, @intCast(x)) + @as(i64, @intCast(d)) - 1)];
        across[y * grid + x] = s;
    };
    for (0..grid) |y| for (0..grid) |x| {
        var s: u32 = 0;
        for (0..3) |d| s += k[d] * across[reflect(@as(i64, @intCast(y)) + @as(i64, @intCast(d)) - 1) * grid + x];
        out[y * grid + x] = @intCast(@min(255, (s + (1 << 15)) >> 16));
    };
    return out;
}

fn reflect(i: i64) usize {
    if (i < 0) return @intCast(-i);
    if (i >= grid) return @intCast(2 * (grid - 1) - i);
    return @intCast(i);
}

fn clampIndex(i: i64) usize {
    return @intCast(std.math.clamp(i, 0, grid - 1));
}

/// OpenCV's Canny with a 3 x 3 Sobel, the L1 gradient and its integer
/// non-maximum suppression, then hysteresis over the 8 neighbours.
fn canny(gpa: std.mem.Allocator, in: []const u8, low: i32, high: i32) ![]bool {
    const n = grid * grid;
    const dx = try gpa.alloc(i32, n);
    defer gpa.free(dx);
    const dy = try gpa.alloc(i32, n);
    defer gpa.free(dy);
    const mag = try gpa.alloc(i32, n);
    defer gpa.free(mag);
    for (0..grid) |y| for (0..grid) |x| {
        const at = struct {
            fn f(img: []const u8, xx: i64, yy: i64) i32 {
                return img[clampIndex(yy) * grid + clampIndex(xx)];
            }
        }.f;
        const xi: i64 = @intCast(x);
        const yi: i64 = @intCast(y);
        const gx = (at(in, xi + 1, yi - 1) + 2 * at(in, xi + 1, yi) + at(in, xi + 1, yi + 1)) - (at(in, xi - 1, yi - 1) + 2 * at(in, xi - 1, yi) + at(in, xi - 1, yi + 1));
        const gy = (at(in, xi - 1, yi + 1) + 2 * at(in, xi, yi + 1) + at(in, xi + 1, yi + 1)) - (at(in, xi - 1, yi - 1) + 2 * at(in, xi, yi - 1) + at(in, xi + 1, yi - 1));
        dx[y * grid + x] = gx;
        dy[y * grid + x] = gy;
        mag[y * grid + x] = @intCast(@abs(gx) + @abs(gy));
    };
    // 0: maybe an edge, 1: not, 2: an edge.
    const map = try gpa.alloc(u8, n);
    defer gpa.free(map);
    @memset(map, 1);
    var stack: std.ArrayList(usize) = .empty;
    defer stack.deinit(gpa);
    const magAt = struct {
        fn f(m: []const i32, xx: i64, yy: i64) i32 {
            if (xx < 0 or yy < 0 or xx >= grid or yy >= grid) return 0;
            return m[@intCast(yy * grid + xx)];
        }
    }.f;
    const tg22: i64 = 13573;
    for (0..grid) |y| for (0..grid) |x| {
        const i = y * grid + x;
        const m = mag[i];
        if (m <= low) continue;
        const xi: i64 = @intCast(x);
        const yi: i64 = @intCast(y);
        const xs: i64 = @abs(dx[i]);
        const ys: i64 = @as(i64, @abs(dy[i])) << 15;
        const tg22x = xs * tg22;
        const keep = if (ys < tg22x)
            m > magAt(mag, xi - 1, yi) and m >= magAt(mag, xi + 1, yi)
        else if (ys > tg22x + (xs << 16))
            m > magAt(mag, xi, yi - 1) and m >= magAt(mag, xi, yi + 1)
        else blk: {
            const s: i64 = if ((dx[i] ^ dy[i]) < 0) -1 else 1;
            break :blk m > magAt(mag, xi - s, yi - 1) and m > magAt(mag, xi + s, yi + 1);
        };
        if (!keep) continue;
        if (m > high) {
            map[i] = 2;
            try stack.append(gpa, i);
        } else map[i] = 0;
    };
    while (stack.pop()) |i| {
        const x: i64 = @intCast(i % grid);
        const y: i64 = @intCast(i / grid);
        for (0..3) |a| for (0..3) |b| {
            const xx = x + @as(i64, @intCast(a)) - 1;
            const yy = y + @as(i64, @intCast(b)) - 1;
            if (xx < 0 or yy < 0 or xx >= grid or yy >= grid) continue;
            const j: usize = @intCast(yy * grid + xx);
            if (map[j] != 0) continue;
            map[j] = 2;
            try stack.append(gpa, j);
        };
    }
    const out = try gpa.alloc(bool, n);
    for (out, map) |*o, v| o.* = v == 2;
    return out;
}

/// OpenCV's 3 x 3 chamfer distance (DIST_L2, mask 3: 0.955 and 1.3693) to
/// the nearest feature pixel.
fn distance(gpa: std.mem.Allocator, feature: []const bool) ![]f64 {
    const a = 0.955;
    const b = 1.3693;
    const big = 1e9;
    const d = try gpa.alloc(f64, feature.len);
    for (d, feature) |*v, f| v.* = if (f) 0 else big;
    const get = struct {
        fn f(dd: []const f64, xx: i64, yy: i64) f64 {
            if (xx < 0 or yy < 0 or xx >= grid or yy >= grid) return big;
            return dd[@intCast(yy * grid + xx)];
        }
    }.f;
    for (0..grid) |y| for (0..grid) |x| {
        const xi: i64 = @intCast(x);
        const yi: i64 = @intCast(y);
        const i = y * grid + x;
        d[i] = @min(d[i], get(d, xi - 1, yi) + a, get(d, xi, yi - 1) + a, get(d, xi - 1, yi - 1) + b, get(d, xi + 1, yi - 1) + b);
    };
    var y: usize = grid;
    while (y > 0) {
        y -= 1;
        var x: usize = grid;
        while (x > 0) {
            x -= 1;
            const xi: i64 = @intCast(x);
            const yi: i64 = @intCast(y);
            const i = y * grid + x;
            d[i] = @min(d[i], get(d, xi + 1, yi) + a, get(d, xi, yi + 1) + a, get(d, xi + 1, yi + 1) + b, get(d, xi - 1, yi + 1) + b);
        }
    }
    return d;
}

/// The edges of a mesh a picture shows as lines: an edge of a face that
/// faces the eye where it ends the surface, bends it (face normals apart by
/// more than 0.96 of a right angle's cosine) or turns it away from the eye.
/// Vertices at one place (to 5 decimals) are one.
pub const Crease = struct { a: [3]f64, b: [3]f64 };

pub fn creases(gpa: std.mem.Allocator, model: Model, tris: []const Tri, a: Alignment, turn: u2) ![]Crease {
    var ids: std.AutoHashMap([3]i64, u32) = .init(gpa);
    defer ids.deinit();
    const unique = try gpa.alloc(u32, vertexCount(model));
    defer gpa.free(unique);
    const places = try gpa.alloc([3]f64, vertexCount(model));
    defer gpa.free(places);
    for (unique, 0..) |*u, i| {
        const v = vertex(model, i);
        const key = [3]i64{ @intFromFloat(@round(v[0] * 1e5)), @intFromFloat(@round(v[1] * 1e5)), @intFromFloat(@round(v[2] * 1e5)) };
        const g = try ids.getOrPut(key);
        if (!g.found_existing) {
            g.value_ptr.* = @intCast(i);
        }
        u.* = g.value_ptr.*;
        places[i] = transformed(v, a, turn);
    }
    const normals = try gpa.alloc([3]f64, tris.len);
    defer gpa.free(normals);
    for (tris, normals) |t, *nm| {
        const p = places[t.v[0]];
        const q = places[t.v[1]];
        const r = places[t.v[2]];
        nm.* = unit(cross(.{ q[0] - p[0], q[1] - p[1], q[2] - p[2] }, .{ r[0] - p[0], r[1] - p[1], r[2] - p[2] }));
    }
    // Faces of each edge, edge by edge of the triangles in order (the
    // review's sort keeps that order among an edge's faces).
    var edges: std.array_hash_map.Auto([2]u32, std.ArrayList(u32)) = .empty;
    defer edges.deinit(gpa);
    for (0..3) |slot| for (tris, 0..) |t, ti| {
        const x = unique[t.v[slot]];
        const y = unique[t.v[(slot + 1) % 3]];
        const g = try edges.getOrPut(gpa, .{ @min(x, y), @max(x, y) });
        if (!g.found_existing) g.value_ptr.* = .empty;
        try g.value_ptr.append(gpa, @intCast(ti));
    };
    var out: std.ArrayList(Crease) = .empty;
    var it = edges.iterator();
    while (it.next()) |e| {
        const faces = e.value_ptr.items;
        var lo = std.math.inf(f64);
        var hi = -std.math.inf(f64);
        for (faces) |f| {
            const v = dot(normals[f], toward_eye);
            lo = @min(lo, v);
            hi = @max(hi, v);
        }
        if (!(hi > 0)) continue;
        const draw = faces.len == 1 or @abs(dot(normals[faces[0]], normals[faces[1]])) < 0.96 or (lo <= 0 and 0 <= hi);
        if (draw) try out.append(gpa, .{ .a = places[e.key_ptr[0]], .b = places[e.key_ptr[1]] });
        e.value_ptr.deinit(gpa);
    }
    return out.items;
}

/// The review's score of a mesh against its picture's `Target`: the outline
/// IoU of its projection laid as `place` lays it (each triangle filled with
/// its corners rounded), the mean of exp(-d / 7.5) over the pixels of its
/// creases (`creases`, d the distance to the nearest picture edge), and
/// 0.65 and 0.35 of them combined.
pub fn evaluate(gpa: std.mem.Allocator, model: Model, tris: []const Tri, a: Alignment, t: Target) !Metric {
    const n = vertexCount(model);
    const pts = try gpa.alloc([2]f64, n);
    defer gpa.free(pts);
    var lo = [2]f64{ std.math.inf(f64), std.math.inf(f64) };
    var hi = [2]f64{ -std.math.inf(f64), -std.math.inf(f64) };
    for (pts, 0..) |*p, i| {
        p.* = project(transformed(vertex(model, i), a, 0));
        lo = .{ @min(lo[0], p[0]), @min(lo[1], p[1]) };
        hi = .{ @max(hi[0], p[0]), @max(hi[1], p[1]) };
    }
    const scale = subject / @max(hi[0] - lo[0], hi[1] - lo[1]);
    const half = @as(f64, grid) / 2;
    const toGrid = struct {
        fn f(p: [2]f64, l: [2]f64, h: [2]f64, s: f64, c: f64) [2]i64 {
            return .{ @intFromFloat(@round((p[0] - (l[0] + h[0]) / 2) * s + c)), @intFromFloat(@round((p[1] - (l[1] + h[1]) / 2) * s + c)) };
        }
    }.f;
    const shape = try gpa.alloc(bool, grid * grid);
    defer gpa.free(shape);
    @memset(shape, false);
    for (tris) |tri| {
        var q: [3][2]i64 = undefined;
        for (&q, tri.v) |*c, v| c.* = toGrid(pts[v], lo, hi, scale, half);
        fillTriangle(shape, q);
    }
    var both: f64 = 0;
    var either: f64 = 0;
    for (shape, t.mask) |s, m| {
        both += @floatFromInt(@intFromBool(s and m));
        either += @floatFromInt(@intFromBool(s or m));
    }
    const iou = both / @max(1, either);
    const drawn = try gpa.alloc(bool, grid * grid);
    defer gpa.free(drawn);
    @memset(drawn, false);
    for (try creases(gpa, model, tris, a, 0)) |c| {
        line(drawn, toGrid(project(c.a), lo, hi, scale, half), toGrid(project(c.b), lo, hi, scale, half));
    }
    var sum: f64 = 0;
    var count: f64 = 0;
    for (drawn, t.distance) |d, dist| if (d) {
        sum += @exp(-dist / 7.5);
        count += 1;
    };
    const edge = if (count > 0) sum / count else 0;
    return .{ .silhouette = iou, .edge = edge, .combined = 0.65 * iou + 0.35 * edge };
}

/// A triangle with integer corners filled with its border (OpenCV's
/// fillConvexPoly draws the edges and the pixels between them).
fn fillTriangle(mask: []bool, q: [3][2]i64) void {
    for (0..3) |i| line(mask, q[i], q[(i + 1) % 3]);
    const den = (q[1][1] - q[2][1]) * (q[0][0] - q[2][0]) + (q[2][0] - q[1][0]) * (q[0][1] - q[2][1]);
    if (den == 0) return;
    const x0 = @max(0, @min(q[0][0], q[1][0], q[2][0]));
    const x1 = @min(grid - 1, @max(q[0][0], q[1][0], q[2][0]));
    const y0 = @max(0, @min(q[0][1], q[1][1], q[2][1]));
    const y1 = @min(grid - 1, @max(q[0][1], q[1][1], q[2][1]));
    var y = y0;
    while (y <= y1) : (y += 1) {
        var x = x0;
        while (x <= x1) : (x += 1) {
            const u = (q[1][1] - q[2][1]) * (x - q[2][0]) + (q[2][0] - q[1][0]) * (y - q[2][1]);
            const v = (q[2][1] - q[0][1]) * (x - q[2][0]) + (q[0][0] - q[2][0]) * (y - q[2][1]);
            const w = den - u - v;
            const inside = if (den > 0) u >= 0 and v >= 0 and w >= 0 else u <= 0 and v <= 0 and w <= 0;
            if (inside) mask[@intCast(y * grid + x)] = true;
        }
    }
}

/// An 8-connected line (Bresenham) between two pixels, clipped to the grid.
fn line(mask: []bool, a: [2]i64, b: [2]i64) void {
    var x = a[0];
    var y = a[1];
    const dx: i64 = @intCast(@abs(b[0] - a[0]));
    const dy: i64 = -@as(i64, @intCast(@abs(b[1] - a[1])));
    const sx: i64 = if (a[0] < b[0]) 1 else -1;
    const sy: i64 = if (a[1] < b[1]) 1 else -1;
    var err = dx + dy;
    while (true) {
        if (x >= 0 and y >= 0 and x < grid and y < grid) mask[@intCast(y * grid + x)] = true;
        if (x == b[0] and y == b[1]) break;
        const e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}
