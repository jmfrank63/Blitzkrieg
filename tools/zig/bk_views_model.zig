//! Full 3D objects (docs/superpowers/specs/2026-10-29-full-3d-objects-design.md):
//! a building or prop as a real textured model, as the 2003 vehicles are.
//! The geometry is the maintainer's tracked mesh (bk_views_mesh.zig) placed
//! on its 2003 picture as the review placed it; every face lies in a chart of
//! one texture atlas (a plane's connected faces, unrolled at `density`
//! texels a world unit); the atlas is painted once from the 2003 pictures of
//! the object's colourway (each texel from the picture that sees its point
//! most squarely, the 2003 light divided out), and what no picture sees gets
//! a material paint taken from the seen faces of the same material and is
//! marked as guessed. The turns the Game draws are this model rendered
//! offline at the 2003 camera, so painting the atlas changes every turn.

const std = @import("std");
const views = @import("bk_views.zig");
const mesh = @import("bk_views_mesh.zig");
const Image = @import("bk_views_pack.zig").Image;

/// Texels a world unit. The 2003 camera draws at most 0.87 pixels a world
/// unit (a vertical edge), so two texels keep every 2003 pixel and leave the
/// painter room for detail.
pub const density = 2.0;
/// Texels around each chart, painted as its faces' nearest points, so that
/// sampling near a chart's border never reads its neighbour.
const padding = 2;
/// Subsamples a pixel edge of the offline render (4 x 4 a pixel).
pub const supersample = 4;
/// Two faces whose unit normals' cosine is this or more lie in one plane
/// for the unwrap.
const coplanar = 0.999;
/// The least side, in texels, of a seen rectangle a material paint tiles from
/// (paintUnseen).
const own_tile = 8;

/// The tracked meshes' objects that are drawn as full 3D models (the pilot
/// of the design; the list grows to all objects and then goes away).
pub const pilot_file = "tools/zig/fixtures/bk-views/full3d.json";
pub const Vertex = struct { mesh: [3]f64, world: [3]f64, uv: [2]f64 };
/// `source`: the mesh triangle it is (mesh.triangles' order); `normal`: its
/// outward normal in the world.
pub const Triangle = struct { v: [3]u32, source: u32, group: u16, chart: u32, normal: [3]f64 };
/// A chart's box in the atlas (padding included), its group and material.
pub const Chart = struct { x: u32, y: u32, w: u32, h: u32, group: u16, material: u32 };

/// Where a texel's paint comes from.
pub const Paint = enum(u8) { none, seen, guessed };

pub const Texture = struct { state: []const u8, image: Image, paint: []Paint, source: []u8 };

pub const Model = struct {
    vertices: []Vertex,
    triangles: []Triangle,
    charts: []Chart,
    /// Group names (`body.wall`, `chimney.roof`, `timber.wall`, ...).
    groups: []const []const u8,
    width: u32,
    height: u32,
    /// The building's lit wall against its shaded one per channel (`views.wallLight`).
    wall: [3]f64 = views.wall_light_rgb,
    textures: []Texture = &.{},
    /// Per atlas texel: the triangle whose face it shows (`none` off every face).
    texel_tri: []u32,
    texel_bary: [][3]f32,

    pub const none = std.math.maxInt(u32);

    pub fn texture(m: Model, state: []const u8) ?Texture {
        for (m.textures) |t| if (std.mem.eql(u8, t.state, state)) return t;
        return null;
    }
};

fn sub(a: [3]f64, b: [3]f64) [3]f64 {
    return .{ a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}

fn scaled(a: [3]f64, s: f64) [3]f64 {
    return .{ a[0] * s, a[1] * s, a[2] * s };
}

/// Whether a ray from `o` along `d` meets triangle a, b, c at a distance
/// over `near` (Moeller-Trumbore).
fn rayHits(o: [3]f64, d: [3]f64, a: [3]f64, b: [3]f64, c: [3]f64, near: f64) bool {
    const e1 = sub(b, a);
    const e2 = sub(c, a);
    const p = mesh.cross(d, e2);
    const det = mesh.dot(e1, p);
    if (@abs(det) < 1e-12) return false;
    const inv = 1 / det;
    const s = sub(o, a);
    const u = mesh.dot(s, p) * inv;
    if (u < 0 or u > 1) return false;
    const q = mesh.cross(s, e1);
    const v = mesh.dot(d, q) * inv;
    if (v < 0 or u + v > 1) return false;
    return mesh.dot(e2, q) * inv > near;
}

/// The model's faces unrolled into one atlas: the mesh placed in its
/// variant's world (`placement`, turn 0), its triangles grouped into charts
/// of connected faces in one plane, each chart laid flat (u along the
/// horizontal, v up the face, so wall courses stay level and every wall
/// stands on its chart's lower edge) and packed in rows.
pub fn unwrap(gpa: std.mem.Allocator, model: mesh.Model, tris: []const mesh.Tri, placement: mesh.Placement) !Model {
    const world = try gpa.alloc([3]f64, mesh.vertexCount(model));
    for (world, 0..) |*w, i| w.* = placement.world(mesh.vertex(model, i));
    var lo = [3]f64{ std.math.inf(f64), std.math.inf(f64), std.math.inf(f64) };
    var hi = [3]f64{ -std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
    for (world) |w| for (0..3) |c| {
        lo[c] = @min(lo[c], w[c]);
        hi[c] = @max(hi[c], w[c]);
    };

    // Outward normals: a face's winding normal unless a ray from its middle
    // that way leaves the mesh through an odd number of faces (the review's
    // meshes do not keep one winding). An open face (a fence's board) keeps
    // its winding; the render lights either side towards the eye.
    const normals = try gpa.alloc([3]f64, tris.len);
    for (tris, normals) |t, *n| {
        const a = world[t.v[0]];
        const b = world[t.v[1]];
        const c = world[t.v[2]];
        n.* = mesh.unit(mesh.cross(sub(b, a), sub(c, a)));
        const mid = [3]f64{ (a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3 };
        // A direction a little off the normal, so that a ray does not run
        // along the edges of a regular mesh.
        const d = mesh.unit(.{ n[0] + 0.0123, n[1] + 0.0071, n[2] + 0.0037 });
        var hits: usize = 0;
        for (tris) |u| {
            if (rayHits(mid, d, world[u.v[0]], world[u.v[1]], world[u.v[2]], 1e-6)) hits += 1;
        }
        if (hits % 2 == 1) n.* = scaled(n.*, -1);
    }

    // Groups: the mesh's own part kinds where it names them, else what a
    // connected piece is (the body, a chimney or tower standing high on a
    // small foot, a low part such as steps or a porch), with the way each
    // face looks (a wall, a roof, an underside).
    const component = try pieces(gpa, model, tris);
    var piece_box: std.AutoHashMap(u32, [6]f64) = .init(gpa);
    var piece_area: std.AutoHashMap(u32, f64) = .init(gpa);
    for (tris, 0..) |t, ti| {
        const c = component[ti];
        const g = try piece_box.getOrPut(c);
        if (!g.found_existing) g.value_ptr.* = .{ std.math.inf(f64), std.math.inf(f64), std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
        for (t.v) |v| for (0..3) |k| {
            g.value_ptr[k] = @min(g.value_ptr[k], world[v][k]);
            g.value_ptr[k + 3] = @max(g.value_ptr[k + 3], world[v][k]);
        };
        const a = world[t.v[0]];
        const area = @sqrt(mesh.dot(mesh.cross(sub(world[t.v[1]], a), sub(world[t.v[2]], a)), mesh.cross(sub(world[t.v[1]], a), sub(world[t.v[2]], a)))) / 2;
        (try piece_area.getOrPutValue(c, 0)).value_ptr.* += area;
    }
    var biggest: u32 = 0;
    var biggest_area: f64 = -1;
    var it = piece_area.iterator();
    while (it.next()) |e| if (e.value_ptr.* > biggest_area or (e.value_ptr.* == biggest_area and e.key_ptr.* < biggest)) {
        biggest = e.key_ptr.*;
        biggest_area = e.value_ptr.*;
    };
    const height = hi[2] - lo[2];
    const ground = (hi[0] - lo[0]) * (hi[1] - lo[1]);
    var group_names: std.ArrayList([]const u8) = .empty;
    const triangles = try gpa.alloc(Triangle, tris.len);
    for (tris, triangles, normals, 0..) |t, *out, n, ti| {
        const kind = model.parts[t.part].kind;
        const box = piece_box.get(component[ti]).?;
        const piece: []const u8 = if (!std.mem.eql(u8, kind, "mesh")) kind else if (component[ti] == biggest) "body" else if (box[5] - lo[2] > 0.6 * height and (box[3] - box[0]) * (box[4] - box[1]) < 0.08 * ground) "chimney" else if (box[5] - lo[2] < 0.3 * height) "low" else "part";
        const look: []const u8 = if (n[2] >= 0.5) "roof" else if (n[2] <= -0.5) "under" else "wall";
        const name = try std.fmt.allocPrint(gpa, "{s}.{s}", .{ piece, look });
        const group = for (group_names.items, 0..) |g, gi| {
            if (std.mem.eql(u8, g, name)) break gi;
        } else blk: {
            try group_names.append(gpa, name);
            break :blk group_names.items.len - 1;
        };
        out.* = .{ .v = t.v, .source = @intCast(ti), .group = @intCast(group), .chart = Model.none, .normal = n };
    }

    // Charts: faces of one group joined across an edge in one plane.
    var edges: std.AutoHashMap([2]u32, std.ArrayList(u32)) = .init(gpa);
    for (tris, 0..) |t, ti| for (0..3) |k| {
        const a = t.v[k];
        const b = t.v[(k + 1) % 3];
        const g = try edges.getOrPut(.{ @min(a, b), @max(a, b) });
        if (!g.found_existing) g.value_ptr.* = .empty;
        try g.value_ptr.append(gpa, @intCast(ti));
    };
    var charts: std.ArrayList(Chart) = .empty;
    var chart_tris: std.ArrayList([]const u32) = .empty;
    var stack: std.ArrayList(u32) = .empty;
    for (triangles, 0..) |*seed, si| {
        if (seed.chart != Model.none) continue;
        const id: u32 = @intCast(charts.items.len);
        var members: std.ArrayList(u32) = .empty;
        seed.chart = id;
        try stack.append(gpa, @intCast(si));
        while (stack.pop()) |ti| {
            try members.append(gpa, ti);
            const t = triangles[ti];
            for (0..3) |k| {
                const a = t.v[k];
                const b = t.v[(k + 1) % 3];
                for (edges.get(.{ @min(a, b), @max(a, b) }).?.items) |ni| {
                    const nb = &triangles[ni];
                    if (nb.chart != Model.none or nb.group != seed.group or mesh.dot(nb.normal, seed.normal) < coplanar) continue;
                    nb.chart = id;
                    try stack.append(gpa, ni);
                }
            }
        }
        std.mem.sort(u32, members.items, {}, std.sort.asc(u32));
        try charts.append(gpa, .{ .x = 0, .y = 0, .w = 0, .h = 0, .group = seed.group, .material = 0 });
        try chart_tris.append(gpa, members.items);
    }

    // Each chart laid flat: its own copy of every vertex it uses.
    var vertices: std.ArrayList(Vertex) = .empty;
    const flat = try gpa.alloc([4]f64, charts.items.len);
    for (charts.items, chart_tris.items, flat) |*chart, members, *box| {
        const n = triangles[members[0]].normal;
        const u_axis = if (@abs(n[2]) < 0.999) mesh.unit(mesh.cross(.{ 0, 0, 1 }, n)) else [3]f64{ 1, 0, 0 };
        const v_axis = mesh.cross(n, u_axis);
        box.* = .{ std.math.inf(f64), std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
        var copies: std.AutoHashMap(u32, u32) = .init(gpa);
        for (members) |ti| {
            const t = &triangles[ti];
            for (&t.v) |*v| {
                const g = try copies.getOrPut(v.*);
                if (!g.found_existing) {
                    const w = world[v.*];
                    // Texel rows run down, so a face's top is its chart's top.
                    const uv = [2]f64{ mesh.dot(w, u_axis) * density, -mesh.dot(w, v_axis) * density };
                    box.* = .{ @min(box[0], uv[0]), @min(box[1], uv[1]), @max(box[2], uv[0]), @max(box[3], uv[1]) };
                    g.value_ptr.* = @intCast(vertices.items.len);
                    try vertices.append(gpa, .{ .mesh = mesh.vertex(model, v.*), .world = w, .uv = uv });
                }
                v.* = g.value_ptr.*;
            }
        }
        box.* = .{ @floor(box[0]), @floor(box[1]), box[2], box[3] };
        chart.w = @as(u32, @intFromFloat(@ceil(box[2] - box[0]))) + 2 * padding;
        chart.h = @as(u32, @intFromFloat(@ceil(box[3] - box[1]))) + 2 * padding;
    }

    // Rows of charts, the tallest first, in the smallest atlas whose sides
    // are powers of two (the squarer of two alike).
    var widest: u32 = 0;
    for (charts.items) |c| widest = @max(widest, c.w);
    const order = try gpa.alloc(u32, charts.items.len);
    for (order, 0..) |*o, i| o.* = @intCast(i);
    std.mem.sort(u32, order, charts.items, struct {
        fn less(cs: []const Chart, a: u32, b: u32) bool {
            return cs[a].h > cs[b].h or (cs[a].h == cs[b].h and a < b);
        }
    }.less);
    var width: u32 = 0;
    var height_px: u32 = 0;
    var try_width: u32 = 64;
    while (try_width <= 8192) : (try_width *= 2) {
        if (try_width < widest) continue;
        const used = shelves(charts.items, order, try_width);
        var h: u32 = 64;
        while (h < used) h *= 2;
        const better = width == 0 or @as(u64, try_width) * h < @as(u64, width) * height_px or
            (@as(u64, try_width) * h == @as(u64, width) * height_px and @max(try_width, h) < @max(width, height_px));
        if (better) {
            width = try_width;
            height_px = h;
        }
    }
    _ = shelves(charts.items, order, width);
    for (charts.items, chart_tris.items, flat) |c, members, box| {
        var moved: std.AutoHashMap(u32, void) = .init(gpa);
        for (members) |ti| for (triangles[ti].v) |v| {
            if ((try moved.getOrPut(v)).found_existing) continue;
            vertices.items[v].uv = .{ vertices.items[v].uv[0] - box[0] + @as(f64, @floatFromInt(c.x + padding)), vertices.items[v].uv[1] - box[1] + @as(f64, @floatFromInt(c.y + padding)) };
        };
    }

    // Each texel's face, for the bake and the paint.
    const count = @as(usize, width) * height_px;
    const texel_tri = try gpa.alloc(u32, count);
    const texel_bary = try gpa.alloc([3]f32, count);
    @memset(texel_tri, Model.none);
    for (triangles, 0..) |t, ti| {
        const a = vertices.items[t.v[0]].uv;
        const b = vertices.items[t.v[1]].uv;
        const c = vertices.items[t.v[2]].uv;
        const den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
        if (@abs(den) < 1e-12) continue;
        const x0: usize = @intFromFloat(@max(0, @floor(@min(a[0], b[0], c[0]))));
        const x1: usize = @intFromFloat(@min(@as(f64, @floatFromInt(width - 1)), @floor(@max(a[0], b[0], c[0]))));
        const y0: usize = @intFromFloat(@max(0, @floor(@min(a[1], b[1], c[1]))));
        const y1: usize = @intFromFloat(@min(@as(f64, @floatFromInt(height_px - 1)), @floor(@max(a[1], b[1], c[1]))));
        for (y0..y1 + 1) |ty| for (x0..x1 + 1) |tx| {
            const px = @as(f64, @floatFromInt(tx)) + 0.5;
            const py = @as(f64, @floatFromInt(ty)) + 0.5;
            const l0 = ((b[1] - c[1]) * (px - c[0]) + (c[0] - b[0]) * (py - c[1])) / den;
            const l1 = ((c[1] - a[1]) * (px - c[0]) + (a[0] - c[0]) * (py - c[1])) / den;
            const l2 = 1 - l0 - l1;
            if (l0 < -1e-9 or l1 < -1e-9 or l2 < -1e-9) continue;
            const i = ty * width + tx;
            texel_tri[i] = @intCast(ti);
            texel_bary[i] = .{ @floatCast(l0), @floatCast(l1), @floatCast(l2) };
        };
    }
    // A texel of a chart's box no face's middle lies in (along a face's
    // edge, the padding, a sliver chart narrower than a texel) takes the
    // nearest point of the chart's faces: a sample at a face's very edge then
    // reads the face's own paint, and no chart is left unpainted.
    for (charts.items, chart_tris.items) |c, members| for (c.y..c.y + c.h) |ty| for (c.x..c.x + c.w) |tx| {
        const i = ty * width + tx;
        if (texel_tri[i] != Model.none) continue;
        const p = [2]f64{ @as(f64, @floatFromInt(tx)) + 0.5, @as(f64, @floatFromInt(ty)) + 0.5 };
        var best = std.math.inf(f64);
        for (members) |ti| {
            const t = triangles[ti];
            const near = nearest(p, vertices.items[t.v[0]].uv, vertices.items[t.v[1]].uv, vertices.items[t.v[2]].uv);
            if (near.distance >= best) continue;
            best = near.distance;
            texel_tri[i] = ti;
            texel_bary[i] = near.bary;
        }
    };

    // Materials: a group in one part of the mesh (its part kind) is one
    // material, which an unseen face takes its paint from.
    for (charts.items, chart_tris.items) |*c, members| c.material = @as(u32, c.group) * 256 + tris[triangles[members[0]].source].part;
    return .{ .vertices = vertices.items, .triangles = triangles, .charts = charts.items, .groups = group_names.items, .width = width, .height = height_px, .texel_tri = texel_tri, .texel_bary = texel_bary };
}

/// Charts laid in rows `width` texels wide, in `order`; the rows' height.
fn shelves(charts: []Chart, order: []const u32, width: u32) u32 {
    var x: u32 = 0;
    var y: u32 = 0;
    var row: u32 = 0;
    for (order) |ci| {
        const c = &charts[ci];
        if (x + c.w > width) {
            x = 0;
            y += row;
            row = 0;
        }
        c.x = x;
        c.y = y;
        x += c.w;
        row = @max(row, c.h);
    }
    return y + row;
}

/// Connected pieces of a mesh: triangles joined by a shared vertex.
fn pieces(gpa: std.mem.Allocator, model: mesh.Model, tris: []const mesh.Tri) ![]u32 {
    const parent = try gpa.alloc(u32, mesh.vertexCount(model));
    for (parent, 0..) |*p, i| p.* = @intCast(i);
    const find = struct {
        fn f(ps: []u32, i: u32) u32 {
            var r = i;
            while (ps[r] != r) r = ps[r];
            var j = i;
            while (ps[j] != r) {
                const next = ps[j];
                ps[j] = r;
                j = next;
            }
            return r;
        }
    }.f;
    for (tris) |t| {
        const a = find(parent, t.v[0]);
        for (t.v[1..]) |v| {
            const b = find(parent, v);
            if (a != b) parent[@max(a, b)] = @min(a, b);
        }
    }
    const out = try gpa.alloc(u32, tris.len);
    for (tris, out) |t, *o| o.* = find(parent, t.v[0]);
    return out;
}

/// One 2003 picture the bake paints from: its picture of the state, and the
/// mesh as that picture shows it (`mesh.view` of the view's placement at
/// turn 0, its depth buffer and its faces' normals towards its eye).
pub const BakeView = struct { picture: Image, view: mesh.View, raster: mesh.Raster, normals: []const [3]f64 };

/// How far, in world units, a point may lie behind the nearest face a view
/// sees and still be seen (bk-views `mesh_visible`).
const visible = 2.0;

/// The atlas of one state painted from its 2003 pictures, the variant's own
/// first (`bake_views[0]`): a texel its own picture sees takes that, so the
/// model at N is the variant's 2003 side and its turns the same building;
/// else the picture of the colourway that sees its point most squarely (a
/// face seen under `grazing` waits for a better picture). A texel takes the
/// pixel under it with the 2003 light divided out (`views.shading` with the
/// building's own `wall` light); the rest a material paint (`paintUnseen`).
pub fn bake(gpa: std.mem.Allocator, m: *Model, state: []const u8, bake_views: []const BakeView) !void {
    const count = @as(usize, m.width) * m.height;
    const image: Image = .{ .width = m.width, .height = m.height, .pixels = try gpa.alloc([4]u8, count) };
    const paint = try gpa.alloc(Paint, count);
    const source = try gpa.alloc(u8, count);
    @memset(image.pixels, .{ 0, 0, 0, 0 });
    @memset(paint, .none);
    @memset(source, 0);
    for (m.texel_tri, m.texel_bary, 0..) |ti, bary, i| {
        if (ti == Model.none) continue;
        const t = m.triangles[ti];
        var p = [3]f64{ 0, 0, 0 };
        for (t.v, bary) |v, b| for (0..3) |c| {
            p[c] += m.vertices[v].mesh[c] * b;
        };
        paint[i] = .guessed;
        var best: f64 = -std.math.inf(f64);
        // The variant's own picture first, then the others.
        for ([_]struct { least: f64, own: bool }{ .{ .least = grazing, .own = true }, .{ .least = grazing, .own = false }, .{ .least = 0, .own = false } }) |pass| {
            if (paint[i] == .seen) break;
            for (bake_views, 0..) |bv, j| {
                if (pass.own and j > 0) break;
                const least = pass.least;
                const q = bv.view.apply(p);
                const at = bv.raster.at(q[0], q[1]) orelse continue;
                // Seen where no face stands in front of the point: its own,
                // one in its plane, or none at that pixel's middle (a picket
                // narrower than a pixel).
                const shown = bv.raster.tri[at];
                if (shown != mesh.Raster.none and shown != t.source and (bv.raster.depth[at] > q[2] + visible or mesh.dot(bv.normals[shown], bv.normals[t.source]) < 0.9)) continue;
                const n = bv.normals[t.source];
                const facing = mesh.dot(n, mesh.toward_eye);
                const pixel = bv.picture.pixels[at];
                if (facing < least or facing <= best or pixel[3] < 128) continue;
                best = facing;
                image.pixels[i] = views.relight(pixel, .{ 1, 1, 1 }, views.shading(n, 0, m.wall));
                image.pixels[i][3] = 255;
                paint[i] = .seen;
                source[i] = @intCast(j + 1);
            }
        }
    }
    const tex: Texture = .{ .state = state, .image = image, .paint = paint, .source = source };
    try paintUnseen(gpa, m.*, tex);
    var list: std.ArrayList(Texture) = .empty;
    try list.appendSlice(gpa, m.textures);
    try list.append(gpa, tex);
    m.textures = list.items;
}

/// A face seen this obliquely or less waits for a picture that sees it better.
const grazing = 0.25;

/// A rectangle of an atlas: x, y, w, h.
const Rect = [4]u32;

/// The largest rectangle of a chart's seen texels (the histogram method).
fn seenRect(gpa: std.mem.Allocator, m: Model, tex: Texture, c: Chart) !?Rect {
    const heights = try gpa.alloc(u32, c.w);
    defer gpa.free(heights);
    @memset(heights, 0);
    var best: ?Rect = null;
    var best_area: u64 = 0;
    for (0..c.h) |ry| {
        for (0..c.w) |rx| {
            const i = (c.y + ry) * m.width + c.x + rx;
            heights[rx] = if (tex.paint[i] == .seen) heights[rx] + 1 else 0;
        }
        for (0..c.w) |rx| {
            const h = heights[rx];
            if (h == 0) continue;
            var l = rx;
            while (l > 0 and heights[l - 1] >= h) l -= 1;
            var r = rx + 1;
            while (r < c.w and heights[r] >= h) r += 1;
            const a = @as(u64, r - l) * h;
            if (a > best_area) {
                best_area = a;
                best = .{ @intCast(c.x + l), @intCast(c.y + ry + 1 - h), @intCast(r - l), h };
            }
        }
    }
    return best;
}

/// Every unseen texel on a face painted from a tile of seen paint: its own
/// chart's largest seen rectangle where that is a good tile (a wall partly
/// hidden by a chimney continues itself), else the largest of its material
/// (a back wall takes the front wall's plaster, windows and all), else of
/// any face that looks the same way (a wall, a roof), else the mean seen
/// colour. Tiles repeat from the chart's lower
/// left corner, so a wall's tile stands on the ground as the wall does.
fn paintUnseen(gpa: std.mem.Allocator, m: Model, tex: Texture) !void {
    const rects = try gpa.alloc(?Rect, m.charts.len);
    for (m.charts, rects) |c, *r| r.* = try seenRect(gpa, m, tex, c);
    const area = struct {
        fn f(r: ?Rect) u64 {
            return if (r) |q| @as(u64, q[2]) * q[3] else 0;
        }
    }.f;
    var mean = [4]f64{ 0, 0, 0, 0 };
    for (tex.image.pixels, tex.paint) |p, k| if (k == .seen) {
        for (0..3) |c| mean[c] += @floatFromInt(p[c]);
        mean[3] += 1;
    };
    // A good tile: `own_tile` texels a side or more and a quarter of its
    // chart or more; a smaller seen rectangle is mostly a strip where the mesh
    // and the 2003 picture disagree by a pixel (a gable under the roof's
    // edge seen as roof).
    const good = try gpa.alloc(bool, m.charts.len);
    for (m.charts, rects, good) |c, r, *g| g.* = if (r) |q| q[2] >= own_tile and q[3] >= own_tile and 4 * area(r) >= @as(u64, c.w) * c.h else false;
    for (m.charts, rects, good) |c, own, own_good| {
        var tile: ?Rect = if (own_good) own else null;
        const look = lookOf(m.groups[c.group]);
        // The largest good tile of the material, else of the faces that look
        // the same way, else the largest seen rectangle of those.
        for ([_]u2{ 0, 1, 2 }) |round| {
            if (tile != null) break;
            for (m.charts, rects, good) |d, r, g| {
                const same = if (round == 0) d.material == c.material else std.mem.eql(u8, lookOf(m.groups[d.group]), look);
                if (same and (g or round == 2) and area(r) > area(tile)) tile = r;
            }
        }
        for (0..c.h) |ry| for (0..c.w) |rx| {
            const i = (c.y + ry) * m.width + c.x + rx;
            if (tex.paint[i] != .guessed) continue;
            if (tile) |t| {
                const from_bottom = c.h - 1 - ry;
                const tx = t[0] + @as(u32, @intCast(rx % t[2]));
                const ty = t[1] + t[3] - 1 - @as(u32, @intCast(from_bottom % t[3]));
                tex.image.pixels[i] = tex.image.pixels[ty * m.width + tx];
            } else if (mean[3] > 0) {
                tex.image.pixels[i] = .{ @intFromFloat(mean[0] / mean[3]), @intFromFloat(mean[1] / mean[3]), @intFromFloat(mean[2] / mean[3]), 255 };
            } else {
                tex.image.pixels[i] = .{ 128, 128, 128, 255 };
            }
        };
    }
}

/// The point of triangle a, b, c nearest to p: its squared distance and
/// its barycentric weights.
fn nearest(p: [2]f64, a: [2]f64, b: [2]f64, c: [2]f64) struct { distance: f64, bary: [3]f32 } {
    const den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
    if (@abs(den) > 1e-12) {
        const l0 = ((b[1] - c[1]) * (p[0] - c[0]) + (c[0] - b[0]) * (p[1] - c[1])) / den;
        const l1 = ((c[1] - a[1]) * (p[0] - c[0]) + (a[0] - c[0]) * (p[1] - c[1])) / den;
        if (l0 >= 0 and l1 >= 0 and l0 + l1 <= 1) return .{ .distance = 0, .bary = .{ @floatCast(l0), @floatCast(l1), @floatCast(1 - l0 - l1) } };
    }
    var best: f64 = std.math.inf(f64);
    var bary = [3]f32{ 1, 0, 0 };
    const corners = [3][2]f64{ a, b, c };
    for (0..3) |k| {
        const u = corners[k];
        const v = corners[(k + 1) % 3];
        const d = [2]f64{ v[0] - u[0], v[1] - u[1] };
        const len2 = d[0] * d[0] + d[1] * d[1];
        const t = if (len2 < 1e-24) 0 else std.math.clamp(((p[0] - u[0]) * d[0] + (p[1] - u[1]) * d[1]) / len2, 0, 1);
        const q = [2]f64{ u[0] + d[0] * t, u[1] + d[1] * t };
        const dist = (p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]);
        if (dist >= best) continue;
        best = dist;
        bary = .{ 0, 0, 0 };
        bary[k] = @floatCast(1 - t);
        bary[(k + 1) % 3] = @floatCast(t);
    }
    return .{ .distance = best, .bary = bary };
}

pub fn lookOf(group: []const u8) []const u8 {
    return group[(std.mem.lastIndexOfScalar(u8, group, '.') orelse return group) + 1 ..];
}

/// The model rendered offline as the 2003 camera turned `turn` quarter
/// turns sees it (the variant's zero cross at `anchor` on a canvas of
/// `size`), lit by the 2003 light of turn `light` (`views.shading`): `supersample`
/// squared samples a pixel, each the nearest texel of the face in front; a
/// pixel is opaque where half its samples or more meet a face, its colour
/// their mean. `guessed`: the pixels most of whose samples show guessed
/// paint; `unpainted`: the samples on a texel no paint reached (holes).
pub const Rendered = struct { image: Image, guessed: []bool, unpainted: usize = 0 };

pub fn render(gpa: std.mem.Allocator, m: Model, state: []const u8, turn: u2, light: u2, anchor: [2]f64, size: [2]u32) !Rendered {
    const tex = m.texture(state) orelse return error.NoTexture;
    const ss = supersample;
    const points = try gpa.alloc([3]f64, m.vertices.len);
    defer gpa.free(points);
    for (points, m.vertices) |*p, v| {
        const q = mesh.quarter(.{ v.world[0], v.world[1] }, turn);
        const t = [3]f64{ q[0], q[1], v.world[2] };
        const s = mesh.project(t);
        p.* = .{ (s[0] + anchor[0]) * ss, (s[1] + anchor[1]) * ss, mesh.dot(t, mesh.toward_eye) };
    }
    const tris = try gpa.alloc(mesh.Tri, m.triangles.len);
    defer gpa.free(tris);
    for (tris, m.triangles) |*o, t| o.* = .{ .v = t.v, .part = t.group };
    const r = try mesh.raster(gpa, points, tris, size[0] * ss, size[1] * ss);
    // Each face lit by its normal turned towards the eye (an open board is
    // lit on the side the eye sees).
    const back = mesh.quarter(.{ mesh.toward_eye[0], mesh.toward_eye[1] }, 0 -% turn);
    const gains = try gpa.alloc([3]f64, m.triangles.len);
    defer gpa.free(gains);
    for (gains, m.triangles) |*g, t| {
        const n = if (mesh.dot(t.normal, .{ back[0], back[1], mesh.toward_eye[2] }) < 0) scaled(t.normal, -1) else t.normal;
        g.* = views.shading(n, light, m.wall);
    }
    const count = @as(usize, size[0]) * size[1];
    var out: Rendered = .{ .image = .{ .width = size[0], .height = size[1], .pixels = try gpa.alloc([4]u8, count) }, .guessed = try gpa.alloc(bool, count) };
    for (0..size[1]) |py| for (0..size[0]) |px| {
        var sum = [3]f64{ 0, 0, 0 };
        var covered: u32 = 0;
        var guessed: u32 = 0;
        for (0..ss) |sy| for (0..ss) |sx| {
            const hx = px * ss + sx;
            const hy = py * ss + sy;
            const ti = r.tri[hy * size[0] * ss + hx];
            if (ti == mesh.Raster.none) continue;
            const t = m.triangles[ti];
            const a = points[t.v[0]];
            const b = points[t.v[1]];
            const c = points[t.v[2]];
            const fx = @as(f64, @floatFromInt(hx)) + 0.5;
            const fy = @as(f64, @floatFromInt(hy)) + 0.5;
            const den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
            const l0 = ((b[1] - c[1]) * (fx - c[0]) + (c[0] - b[0]) * (fy - c[1])) / den;
            const l1 = ((c[1] - a[1]) * (fx - c[0]) + (a[0] - c[0]) * (fy - c[1])) / den;
            const l2 = 1 - l0 - l1;
            const ua = m.vertices[t.v[0]].uv;
            const ub = m.vertices[t.v[1]].uv;
            const uc = m.vertices[t.v[2]].uv;
            const u = l0 * ua[0] + l1 * ub[0] + l2 * uc[0];
            const v = l0 * ua[1] + l1 * ub[1] + l2 * uc[1];
            const tx: usize = @intFromFloat(std.math.clamp(@floor(u), 0, @as(f64, @floatFromInt(m.width - 1))));
            const ty: usize = @intFromFloat(std.math.clamp(@floor(v), 0, @as(f64, @floatFromInt(m.height - 1))));
            const i = ty * m.width + tx;
            const lit = views.relight(tex.image.pixels[i], gains[ti], .{ 1, 1, 1 });
            for (0..3) |ch| sum[ch] += @floatFromInt(lit[ch]);
            covered += 1;
            guessed += @intFromBool(tex.paint[i] == .guessed);
            out.unpainted += @intFromBool(tex.paint[i] == .none);
        };
        const o = py * size[0] + px;
        out.guessed[o] = covered * 2 >= ss * ss and guessed * 2 > covered;
        out.image.pixels[o] = if (covered * 2 >= ss * ss) .{
            @intFromFloat(@round(sum[0] / @as(f64, @floatFromInt(covered)))),
            @intFromFloat(@round(sum[1] / @as(f64, @floatFromInt(covered)))),
            @intFromFloat(@round(sum[2] / @as(f64, @floatFromInt(covered)))),
            255,
        } else .{ 0, 0, 0, 0 };
    };
    return out;
}

// ---------------------------------------------------------------------------
// Files: the vehicles' format family

/// A chunk of the engine's structure files (StructureSaver2.cpp): its id,
/// its length (below 128 one byte, `len << 1`, else four, `len << 1 | 1`)
/// and its bytes.
fn chunk(gpa: std.mem.Allocator, out: *std.ArrayList(u8), id: u8, payload: []const u8) !void {
    try out.append(gpa, id);
    if (payload.len < 128) {
        try out.append(gpa, @intCast(payload.len << 1));
    } else {
        var len: [4]u8 = undefined;
        std.mem.writeInt(u32, &len, @as(u32, @intCast(payload.len)) << 1 | 1, .little);
        try out.appendSlice(gpa, &len);
    }
    try out.appendSlice(gpa, payload);
}

fn podChunk(gpa: std.mem.Allocator, out: *std.ArrayList(u8), id: u8, value: anytype) !void {
    try chunk(gpa, out, id, std.mem.asBytes(&value));
}

/// A `std::string` (SSHelper: its length, then its characters).
fn stringChunk(gpa: std.mem.Allocator, out: *std.ArrayList(u8), id: u8, s: []const u8) !void {
    var body: std.ArrayList(u8) = .empty;
    try podChunk(gpa, &body, 1, @as(i32, @intCast(s.len)));
    try chunk(gpa, &body, 2, s);
    try chunk(gpa, out, id, body.items);
}

/// A vector of plain values (DoDataVector: its count, then the raw array).
fn dataVectorChunk(gpa: std.mem.Allocator, out: *std.ArrayList(u8), id: u8, comptime T: type, items: []const T) !void {
    var body: std.ArrayList(u8) = .empty;
    try podChunk(gpa, &body, 1, @as(i32, @intCast(items.len)));
    if (items.len > 0) try chunk(gpa, &body, 2, std.mem.sliceAsBytes(items));
    try chunk(gpa, out, id, body.items);
}

/// The model as a `.mod` (Formats/fmtMesh.h, laid out as bk-map obj2mod
/// writes one): a skeleton of one node a group, the first the top and the
/// others its children, each at its group's middle and every node a
/// locator, so the Resource Editor lists the parts by name; a mesh a group
/// on its node, every face twice (both windings: the engine culls one side,
/// and an open board is seen from both); no animation. World units, z up,
/// the variant's zero cross at the origin, as the Game places a unit.
pub fn writeMod(gpa: std.mem.Allocator, m: Model) ![]u8 {
    const G = m.groups.len;
    const centres = try gpa.alloc([3]f64, G);
    const counts = try gpa.alloc(f64, G);
    @memset(centres, .{ 0, 0, 0 });
    @memset(counts, 0);
    for (m.triangles) |t| for (t.v) |v| {
        for (0..3) |c| centres[t.group][c] += m.vertices[v].world[c];
        counts[t.group] += 1;
    };
    for (centres, counts) |*c, n| c.* = scaled(c.*, 1 / @max(n, 1));
    var lo = [3]f64{ std.math.inf(f64), std.math.inf(f64), std.math.inf(f64) };
    var hi = [3]f64{ -std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
    for (m.vertices) |v| for (0..3) |c| {
        lo[c] = @min(lo[c], v.world[c]);
        hi[c] = @max(hi[c], v.world[c]);
    };

    var skeleton: std.ArrayList(u8) = .empty;
    for (0..G) |g| {
        var node: std.ArrayList(u8) = .empty;
        try stringChunk(gpa, &node, 1, m.groups[g]);
        try podChunk(gpa, &node, 2, @as(i32, @intCast(g)));
        const bone = if (g == 0) centres[0] else sub(centres[g], centres[0]);
        try podChunk(gpa, &node, 10, [3]f32{ @floatCast(bone[0]), @floatCast(bone[1]), @floatCast(bone[2]) });
        try podChunk(gpa, &node, 11, [4]f32{ 0, 0, 0, 1 });
        const children = try gpa.alloc(i32, if (g == 0) G - 1 else 0);
        for (children, 1..) |*c, i| c.* = @intCast(i);
        try dataVectorChunk(gpa, &node, 20, i32, children);
        try chunk(gpa, &skeleton, 1, node.items);
    }
    try podChunk(gpa, &skeleton, 2, @as(i32, 0));
    const locators = try gpa.alloc(i32, G);
    for (locators, 0..) |*l, i| l.* = @intCast(i);
    try dataVectorChunk(gpa, &skeleton, 3, i32, locators);

    var meshes: std.ArrayList(u8) = .empty;
    for (0..G) |g| {
        var geoms: std.ArrayList([3]f32) = .empty;
        var norms: std.ArrayList([3]f32) = .empty;
        var texes: std.ArrayList([2]f32) = .empty;
        var components: std.ArrayList(u8) = .empty;
        var indices: std.ArrayList(u16) = .empty;
        var at: std.AutoHashMap(u32, u32) = .init(gpa);
        var glo = [3]f32{ std.math.inf(f32), std.math.inf(f32), std.math.inf(f32) };
        var ghi = [3]f32{ -std.math.inf(f32), -std.math.inf(f32), -std.math.inf(f32) };
        var count: u32 = 0;
        for (m.triangles) |t| {
            if (t.group != g) continue;
            // The face's normal, then its back's: a vertex takes its side's.
            const ni: i32 = @intCast(norms.items.len);
            try norms.append(gpa, .{ @floatCast(t.normal[0]), @floatCast(t.normal[1]), @floatCast(t.normal[2]) });
            try norms.append(gpa, .{ @floatCast(-t.normal[0]), @floatCast(-t.normal[1]), @floatCast(-t.normal[2]) });
            // Outward first, counter-clockwise seen from outside, as obj2mod
            // passes an OBJ's faces; then the back.
            const a = m.vertices[t.v[0]].world;
            const forward = mesh.dot(mesh.cross(sub(m.vertices[t.v[1]].world, a), sub(m.vertices[t.v[2]].world, a)), t.normal) >= 0;
            const front = if (forward) t.v else [3]u32{ t.v[0], t.v[2], t.v[1] };
            for ([2][3]u32{ front, .{ front[0], front[2], front[1] } }, 0..) |corners, side| for (corners) |v| {
                // A vertex is shared by the faces of its chart on one side.
                const g_at = try at.getOrPut(v * 2 + @as(u32, @intCast(side)));
                if (!g_at.found_existing) {
                    g_at.value_ptr.* = count;
                    count += 1;
                    const w = sub(m.vertices[v].world, centres[g]);
                    const p = [3]f32{ @floatCast(w[0]), @floatCast(w[1]), @floatCast(w[2]) };
                    for (0..3) |c| {
                        glo[c] = @min(glo[c], p[c]);
                        ghi[c] = @max(ghi[c], p[c]);
                    }
                    const gi: i32 = @intCast(geoms.items.len);
                    try geoms.append(gpa, p);
                    try texes.append(gpa, .{ @floatCast(m.vertices[v].uv[0] / @as(f64, @floatFromInt(m.width))), @floatCast(m.vertices[v].uv[1] / @as(f64, @floatFromInt(m.height))) });
                    var component: std.ArrayList(u8) = .empty;
                    try podChunk(gpa, &component, 1, gi);
                    try podChunk(gpa, &component, 2, ni + @as(i32, @intCast(side)));
                    try podChunk(gpa, &component, 3, gi);
                    try chunk(gpa, &components, 1, component.items);
                }
                if (g_at.value_ptr.* > std.math.maxInt(u16)) return error.TooManyVertices;
                try indices.append(gpa, @intCast(g_at.value_ptr.*));
            };
        }
        var one: std.ArrayList(u8) = .empty;
        try stringChunk(gpa, &one, 1, m.groups[g]);
        try podChunk(gpa, &one, 2, @as(i32, -1));
        try dataVectorChunk(gpa, &one, 10, [3]f32, geoms.items);
        try dataVectorChunk(gpa, &one, 11, [3]f32, norms.items);
        try dataVectorChunk(gpa, &one, 12, [2]f32, texes.items);
        try chunk(gpa, &one, 20, components.items);
        try dataVectorChunk(gpa, &one, 21, u16, indices.items);
        const centre = [3]f32{ (glo[0] + ghi[0]) / 2, (glo[1] + ghi[1]) / 2, (glo[2] + ghi[2]) / 2 };
        const half = [3]f32{ (ghi[0] - glo[0]) / 2, (ghi[1] - glo[1]) / 2, (ghi[2] - glo[2]) / 2 };
        try podChunk(gpa, &one, 30, [6]f32{ centre[0], centre[1], centre[2], half[0], half[1], half[2] });
        var radius: f32 = 0;
        for (geoms.items) |p| radius = @max(radius, @sqrt((p[0] - centre[0]) * (p[0] - centre[0]) + (p[1] - centre[1]) * (p[1] - centre[1]) + (p[2] - centre[2]) * (p[2] - centre[2])));
        try podChunk(gpa, &one, 31, [4]f32{ centre[0], centre[1], centre[2], radius });
        try chunk(gpa, &meshes, 1, one.items);
    }

    var data: std.ArrayList(u8) = .empty;
    try chunk(gpa, &data, 1, skeleton.items);
    try chunk(gpa, &data, 2, meshes.items);
    try chunk(gpa, &data, 3, &.{});
    const box = [6]f32{ @floatCast((lo[0] + hi[0]) / 2), @floatCast((lo[1] + hi[1]) / 2), @floatCast((lo[2] + hi[2]) / 2), @floatCast((hi[0] - lo[0]) / 2), @floatCast((hi[1] - lo[1]) / 2), @floatCast((hi[2] - lo[2]) / 2) };
    try podChunk(gpa, &data, 4, box);
    try chunk(gpa, &data, 5, &.{});
    try chunk(gpa, &data, 6, &.{});
    var file: std.ArrayList(u8) = .empty;
    try chunk(gpa, &file, 1, data.items);
    try chunk(gpa, &file, 0, &.{});
    try chunk(gpa, &file, 2, &.{});
    return file.items;
}

/// An uncompressed 32-bit DDS (A8R8G8B8, one level): the `_h` texture the
/// Game's loader reads as it is (GFXGPU TextureGpu.cpp).
pub fn writeDds(gpa: std.mem.Allocator, image: Image) ![]u8 {
    const out = try gpa.alloc(u8, 128 + image.pixels.len * 4);
    @memset(out[0..128], 0);
    @memcpy(out[0..4], "DDS ");
    std.mem.writeInt(u32, out[4..8], 124, .little);
    std.mem.writeInt(u32, out[8..12], 0x100f, .little);
    std.mem.writeInt(u32, out[12..16], image.height, .little);
    std.mem.writeInt(u32, out[16..20], image.width, .little);
    std.mem.writeInt(u32, out[20..24], image.width * 4, .little);
    std.mem.writeInt(u32, out[28..32], 1, .little);
    std.mem.writeInt(u32, out[76..80], 32, .little);
    std.mem.writeInt(u32, out[80..84], 0x41, .little);
    std.mem.writeInt(u32, out[88..92], 32, .little);
    for ([_]u32{ 0xff0000, 0xff00, 0xff, 0xff000000 }, 0..) |mask, i| std.mem.writeInt(u32, out[92 + 4 * i ..][0..4], mask, .little);
    std.mem.writeInt(u32, out[108..112], 0x1000, .little);
    for (image.pixels, 0..) |p, i| out[128 + i * 4 ..][0..4].* = .{ p[2], p[1], p[0], p[3] };
    return out;
}

/// A 32-bit TGA as `NImage::SaveImageAsTGA` writes one (bk_image.zig): the
/// source picture the Resource Editor's unit export converts.
pub fn writeTga(gpa: std.mem.Allocator, image: Image) ![]u8 {
    const footer = "\x00\x00\x00\x00\x00\x00\x00\x00TRUEVISION-XFILE.\x00";
    const out = try gpa.alloc(u8, 18 + image.pixels.len * 4 + footer.len);
    @memset(out[0..18], 0);
    out[2] = 2;
    std.mem.writeInt(u16, out[12..14], @intCast(image.width), .little);
    std.mem.writeInt(u16, out[14..16], @intCast(image.height), .little);
    out[16] = 32;
    out[17] = 0x28;
    for (image.pixels, 0..) |p, i| out[18 + i * 4 ..][0..4].* = .{ p[2], p[1], p[0], p[3] };
    @memcpy(out[out.len - footer.len ..], footer);
    return out;
}
