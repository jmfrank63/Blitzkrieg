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
//! Each damage state is a model of its own (`damaged`): the intact one cut
//! where its damaged 2003 picture no longer reaches, painted from that picture.

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

pub const Vertex = struct { mesh: [3]f64, world: [3]f64, uv: [2]f64 };
/// `source`: the mesh triangle it is (mesh.triangles' order); `normal`: its
/// outward normal in the world.
pub const Triangle = struct { v: [3]u32, source: u32, group: u16, chart: u32, normal: [3]f64 };
/// A chart's box in the atlas (padding included), its group and material.
pub const Chart = struct { x: u32, y: u32, w: u32, h: u32, group: u16, material: u32 };

/// Where a texel's paint comes from: a 2003 picture (`seen`), the same
/// point of a counterpart face of the object (`copied`: the back roof slope
/// from the front one), or synthesised from a well-seen face of its
/// material (`guessed`).
pub const Paint = enum(u8) { none, seen, guessed, copied };

/// `jacobian`: per texel the 2003 picture's pixels a texel step spans (row
/// major, d(x, y) / d(u, v)) where its paint came from, zero where no
/// picture's pixel did (a flat colour): what `render` measures a turn's
/// stretch by.
pub const Texture = struct { state: []const u8, image: Image, paint: []Paint, source: []u8, jacobian: [][4]f32 };

/// How the bake paints what the 2003 pictures do not show well.
/// `coherent` (2026-10-31): a texel takes a picture's pixel only where no
/// turn stretches that pixel over more than `stretch_limit` pixels, the rest
/// is copied from the object's counterpart faces or synthesised from its
/// well-seen faces of the same material. `tile` (2026-10-29): any picture
/// seeing the face at all, the rest tiled; kept as the baseline the paint
/// review (`paint-compare`) measures against.
pub const Method = enum { tile, coherent };

/// The most render pixels one 2003 pixel may cover along any direction at any
/// turn (the largest singular value of the turn's texel-to-pixel map times
/// the inverse of the picture's): over it a turn shows the pixel as a streak.
pub const stretch_limit = 2.0;

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
    /// Per chart: a damage state's fallen roof (`damaged`), whose texels no
    /// picture sees are tiled from those its picture does.
    fallen: []const bool = &.{},
    method: Method = .coherent,

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
    const faces = try texelFaces(gpa, vertices.items, triangles, width, height_px);
    const texel_tri = faces.tri;
    const texel_bary = faces.bary;
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

/// Each texel's face: the triangle whose texels hold the texel's middle and
/// where in it (`Model.none` off every face).
fn texelFaces(gpa: std.mem.Allocator, vertices: []const Vertex, triangles: []const Triangle, width: u32, height: u32) !struct { tri: []u32, bary: [][3]f32 } {
    const count = @as(usize, width) * height;
    const texel_tri = try gpa.alloc(u32, count);
    const texel_bary = try gpa.alloc([3]f32, count);
    @memset(texel_tri, Model.none);
    for (triangles, 0..) |t, ti| {
        const a = vertices[t.v[0]].uv;
        const b = vertices[t.v[1]].uv;
        const c = vertices[t.v[2]].uv;
        const den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
        if (@abs(den) < 1e-12) continue;
        const x0: usize = @intFromFloat(@max(0, @floor(@min(a[0], b[0], c[0]))));
        const x1: usize = @intFromFloat(@min(@as(f64, @floatFromInt(width - 1)), @floor(@max(a[0], b[0], c[0]))));
        const y0: usize = @intFromFloat(@max(0, @floor(@min(a[1], b[1], c[1]))));
        const y1: usize = @intFromFloat(@min(@as(f64, @floatFromInt(height - 1)), @floor(@max(a[1], b[1], c[1]))));
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
    return .{ .tri = texel_tri, .bary = texel_bary };
}

// ---------------------------------------------------------------------------
// Damage states

/// A corner of a face being cut: where it is in the mesh and in the world,
/// and its texel.
const Corner = struct { mesh: [3]f64, world: [3]f64, uv: [2]f64 };

fn lerpCorner(a: Corner, b: Corner, t: f64) Corner {
    var out: Corner = undefined;
    for (0..3) |c| {
        out.mesh[c] = a.mesh[c] + (b.mesh[c] - a.mesh[c]) * t;
        out.world[c] = a.world[c] + (b.world[c] - a.world[c]) * t;
    }
    for (0..2) |c| out.uv[c] = a.uv[c] + (b.uv[c] - a.uv[c]) * t;
    return out;
}

/// A plane of the world: the points w where dot(normal, w) + offset >= 0
/// are on its kept side.
const Plane = struct { normal: [3]f64, offset: f64 };

fn signed(p: Plane, c: Corner) f64 {
    return mesh.dot(p.normal, c.world) + p.offset;
}

/// The part of a convex polygon on a plane's kept side (Sutherland-Hodgman).
fn clip(gpa: std.mem.Allocator, polygon: []const Corner, p: Plane) ![]Corner {
    var out: std.ArrayList(Corner) = .empty;
    for (polygon, 0..) |a, i| {
        const b = polygon[(i + 1) % polygon.len];
        const da = signed(p, a);
        const db = signed(p, b);
        if (da >= 0) try out.append(gpa, a);
        if ((da >= 0) != (db >= 0)) try out.append(gpa, lerpCorner(a, b, da / (da - db)));
    }
    return out.items;
}

/// The kept side of `p` turned over.
fn flipped(p: Plane) Plane {
    return .{ .normal = scaled(p.normal, -1), .offset = -p.offset };
}

/// The model of a damage state (`2`, `3`), as the vehicles have `2.mod` and
/// `3.mod`: the intact model cut by the visual hull of the damaged 2003
/// picture at N (`picture`, its zero cross at `anchor`; `placement` the
/// intact model's). In each pixel column of the picture the outline's top is
/// where the damaged building now ends; between the columns' edges it runs
/// straight, so neighbouring faces are cut along the same line and no crack
/// opens. A wall above the line is gone (a breach, a fallen gable); a face
/// that looks up (a roof) is lowered straight down onto the hull's surface,
/// so a roof the outline cuts lies over the cut walls as a cap and turned
/// the building stays closed. One picture cannot tell how deep a pixel
/// lies, so rubble seen inside the outline would keep a roof over it: a roof
/// group (`body.roof`, `chimney.roof`) whose pixels at N in the damaged
/// picture mostly differ from the intact picture's (`intact_picture`; one N
/// hardly sees goes as all the roofs N sees) has
/// fallen, and its faces lie flat at `rubble` of the building's height, a
/// floor of rubble inside the walls (`Model.fallen`: its texels no picture
/// sees are tiled from those it does). Of a face N does not see (a back
/// wall, a far wing) the picture tells only how much of the building stands
/// where it does see: such a face stands no higher than the share of the
/// walls N sees that the damaged picture still shows unchanged, of the
/// building's height, a roof among them lowered onto it. A
/// column the damaged picture does not reach takes nothing. The faces keep
/// their texels (the intact atlas's layout), so the damaged atlas starts
/// from the intact paint (`bake` with a base).
pub fn damaged(gpa: std.mem.Allocator, intact: Model, placement: mesh.Placement, picture: Image, intact_picture: Image, anchor: [2]f64) !Model {
    // The outline's top per column, then at each column edge the higher of
    // its two columns' (null where neither has one).
    const top = try gpa.alloc(?f64, picture.width);
    for (top, 0..) |*t, x| {
        t.* = null;
        for (0..picture.height) |y| if (picture.pixels[y * picture.width + x][3] >= 128) {
            t.* = @floatFromInt(y);
            break;
        };
    }
    const knots = try gpa.alloc(?f64, picture.width + 1);
    for (knots, 0..) |*k, c| {
        const left = if (c > 0) top[c - 1] else null;
        const right = if (c < picture.width) top[c] else null;
        k.* = if (left != null and right != null) @min(left.?, right.?) else left orelse right;
    }
    // World z per mesh z, and the picture's y per world z.
    const z_scale = placement.scale * placement.alignment.zscale;
    const y_per_z = -mesh.rows[1][2];
    // The roof groups that fell: of the pixels the intact model shows as a
    // roof at N where the intact picture is opaque, the share the damaged
    // picture shows changed (clear, or a mean channel `changed` or more off).
    const at_n = try ownBakeView(gpa, intact, intact_picture, placement, anchor);
    const counts = try gpa.alloc([2]f64, intact.groups.len);
    @memset(counts, .{ 0, 0 });
    // The walls N sees, and those of them it still shows unchanged.
    var walls = [2]f64{ 0, 0 };
    for (at_n.raster.tri, intact_picture.pixels, picture.pixels) |ti, before, after| {
        if (ti == mesh.Raster.none or before[3] < 128) continue;
        const g = intact.triangles[ti].group;
        var difference: f64 = 0;
        for (0..3) |c| difference += @abs(@as(f64, @floatFromInt(before[c])) - @as(f64, @floatFromInt(after[c]))) / 3;
        const gone = after[3] < 128 or difference >= changed;
        if (std.mem.eql(u8, lookOf(intact.groups[g]), "wall")) walls = .{ walls[0] + 1, walls[1] + @as(f64, @floatFromInt(@intFromBool(!gone))) };
        if (!std.mem.eql(u8, lookOf(intact.groups[g]), "roof")) continue;
        counts[g][0] += 1;
        counts[g][1] += @floatFromInt(@intFromBool(gone));
    }
    // A roof group N hardly sees goes as the building's roofs go.
    var all = [2]f64{ 0, 0 };
    for (counts) |n| all = .{ all[0] + n[0], all[1] + n[1] };
    const fallen_group = try gpa.alloc(bool, intact.groups.len);
    for (fallen_group, counts, intact.groups) |*f, n, g| {
        const evidence = if (n[0] >= fallen_pixels) n else all;
        f.* = std.mem.eql(u8, lookOf(g), "roof") and evidence[0] >= fallen_pixels and evidence[1] >= fallen_share * evidence[0];
    }
    var height: f64 = 0;
    for (intact.vertices) |v| height = @max(height, v.world[2]);
    const cap: Plane = .{ .normal = .{ 0, 0, -1 }, .offset = (if (walls[0] >= fallen_pixels) walls[1] / walls[0] else 1) * height };

    const corner_of = struct {
        fn f(v: Vertex) Corner {
            return .{ .mesh = v.mesh, .world = v.world, .uv = v.uv };
        }
    }.f;
    // Every face cut: its pieces, each with its face (`tri`).
    const Piece = struct { corners: []Corner, lowered: bool, tri: u32 };
    var cut: std.ArrayList(Piece) = .empty;
    for (intact.triangles, 0..) |t, ti| {
        const tri = [3]Corner{ corner_of(intact.vertices[t.v[0]]), corner_of(intact.vertices[t.v[1]]), corner_of(intact.vertices[t.v[2]]) };
        const roof = t.normal[2] >= 0.5;
        const face: u32 = @intCast(ti);
        if (fallen_group[t.group]) {
            const flat = try gpa.dupe(Corner, &tri);
            for (flat) |*a| {
                const z = @min(a.world[2], rubble * height);
                a.mesh[2] += (z - a.world[2]) / z_scale;
                a.world[2] = z;
            }
            try cut.append(gpa, .{ .corners = flat, .lowered = true, .tri = face });
            continue;
        }
        var xs_lo = std.math.inf(f64);
        var xs_hi = -std.math.inf(f64);
        for (tri) |c| {
            const s = mesh.project(c.world);
            xs_lo = @min(xs_lo, s[0] + anchor[0]);
            xs_hi = @max(xs_hi, s[0] + anchor[0]);
        }
        const c_lo: i64 = @intFromFloat(@floor(xs_lo));
        const c_hi: i64 = @intFromFloat(@ceil(xs_hi));
        // Column by column; a run of columns where the face lies wholly under
        // the line is kept as one piece, so a face is split only where the
        // outline crosses it.
        var run: ?i64 = null;
        var c = c_lo;
        while (c <= @max(c_hi, c_lo + 1)) : (c += 1) {
            const k0: ?f64 = if (c < 0 or c >= picture.width) null else knots[@intCast(c)];
            const k1: ?f64 = if (c < 0 or c >= picture.width) null else knots[@intCast(c + 1)];
            const cf: f64 = @floatFromInt(c);
            // The strip of column c: cf <= x_s < cf + 1.
            const left: Plane = .{ .normal = mesh.rows[0], .offset = anchor[0] - cf };
            const right: Plane = .{ .normal = scaled(mesh.rows[0], -1), .offset = cf + 1 - anchor[0] };
            var no_corners: [0]Corner = .{};
            const strip: []Corner = if (c < @max(c_hi, c_lo + 1)) try clip(gpa, try clip(gpa, &tri, left), right) else &no_corners;
            // Under the line y_s = k0 + (x_s - cf) (k1 - k0): kept.
            const slope = if (k0 != null and k1 != null) k1.? - k0.? else 0;
            const under: Plane = .{ .normal = sub(mesh.rows[1], scaled(mesh.rows[0], slope)), .offset = anchor[1] - (k0 orelse 0) - (anchor[0] - cf) * slope };
            const above: []Corner = if (k0 != null and k1 != null and strip.len >= 3) try clip(gpa, strip, flipped(under)) else &no_corners;
            const continues = k0 != null and k1 != null and above.len < 3 and c < @max(c_hi, c_lo + 1);
            if (continues) {
                if (run == null) run = c;
                continue;
            }
            if (run) |r| {
                const from: Plane = .{ .normal = mesh.rows[0], .offset = anchor[0] - @as(f64, @floatFromInt(r)) };
                const piece = try clip(gpa, try clip(gpa, &tri, from), flipped(left));
                if (piece.len >= 3) try cut.append(gpa, .{ .corners = piece, .lowered = false, .tri = face });
                run = null;
            }
            if (k0 == null or k1 == null or strip.len < 3) continue;
            const below = try clip(gpa, strip, under);
            if (below.len >= 3) try cut.append(gpa, .{ .corners = below, .lowered = false, .tri = face });
            if (!roof) continue;
            for (above) |*a| {
                const e = signed(under, a.*);
                const z = @max(0, a.world[2] + e / y_per_z);
                a.mesh[2] += (z - a.world[2]) / z_scale;
                a.world[2] = z;
            }
            try cut.append(gpa, .{ .corners = above, .lowered = true, .tri = face });
        }
    }

    // The pieces N sees of the cut building, at the render's samples (a sliver
    // no pixel's middle meets still shows; a back wall a fallen roof uncovers
    // is seen); a piece N does not see keeps no more than the walls' share of
    // the height, a roof among them lowered onto it.
    const seen = try gpa.alloc(bool, cut.items.len);
    @memset(seen, false);
    {
        const v = mesh.view(placement, 0, anchor);
        var points: std.ArrayList([3]f64) = .empty;
        var tris: std.ArrayList(mesh.Tri) = .empty;
        for (cut.items, 0..) |piece, pi| {
            const first: u32 = @intCast(points.items.len);
            for (piece.corners) |corner| {
                const q = v.apply(corner.mesh);
                try points.append(gpa, .{ q[0] * supersample, q[1] * supersample, q[2] });
            }
            for (1..piece.corners.len - 1) |i| try tris.append(gpa, .{ .v = .{ first, first + @as(u32, @intCast(i)), first + @as(u32, @intCast(i + 1)) }, .part = @intCast(pi) });
        }
        for ((try mesh.raster(gpa, points.items, tris.items, picture.width * supersample, picture.height * supersample)).tri) |k| {
            if (k != mesh.Raster.none) seen[tris.items[k].part] = true;
        }
    }
    var kept: std.ArrayList(Piece) = .empty;
    for (cut.items, seen) |piece, n_sees| {
        const t = intact.triangles[piece.tri];
        if (n_sees or fallen_group[t.group]) {
            try kept.append(gpa, piece);
            continue;
        }
        const below = try clip(gpa, piece.corners, cap);
        if (below.len >= 3) try kept.append(gpa, .{ .corners = below, .lowered = piece.lowered, .tri = piece.tri });
        if (t.normal[2] < 0.5) continue;
        const above = try clip(gpa, piece.corners, flipped(cap));
        if (above.len < 3) continue;
        for (above) |*a| {
            a.mesh[2] += (cap.offset - a.world[2]) / z_scale;
            a.world[2] = cap.offset;
        }
        try kept.append(gpa, .{ .corners = above, .lowered = true, .tri = piece.tri });
    }

    var vertices: std.ArrayList(Vertex) = .empty;
    var triangles: std.ArrayList(Triangle) = .empty;
    var index: std.AutoHashMap([5]u64, u32) = .init(gpa);
    for (kept.items) |piece| for (1..piece.corners.len - 1) |i| {
        const t = intact.triangles[piece.tri];
        const fan = [3]Corner{ piece.corners[0], piece.corners[i], piece.corners[i + 1] };
        const n = mesh.cross(sub(fan[1].world, fan[0].world), sub(fan[2].world, fan[0].world));
        if (mesh.dot(n, n) < 1e-12) continue;
        const normal = if (!piece.lowered) t.normal else if (n[2] < 0) mesh.unit(scaled(n, -1)) else mesh.unit(n);
        var v: [3]u32 = undefined;
        for (fan, &v) |corner, *out| {
            const key = [5]u64{ @bitCast(corner.world[0]), @bitCast(corner.world[1]), @bitCast(corner.world[2]), @bitCast(corner.uv[0]), @bitCast(corner.uv[1]) };
            const g = try index.getOrPut(key);
            if (!g.found_existing) {
                g.value_ptr.* = @intCast(vertices.items.len);
                try vertices.append(gpa, .{ .mesh = corner.mesh, .world = corner.world, .uv = corner.uv });
            }
            out.* = g.value_ptr.*;
        }
        try triangles.append(gpa, .{ .v = v, .source = @intCast(triangles.items.len), .group = t.group, .chart = t.chart, .normal = normal });
    };
    const faces = try texelFaces(gpa, vertices.items, triangles.items, intact.width, intact.height);
    const fallen = try gpa.alloc(bool, intact.charts.len);
    for (fallen, intact.charts) |*f, chart| f.* = fallen_group[chart.group];
    return .{ .vertices = vertices.items, .triangles = triangles.items, .charts = intact.charts, .groups = intact.groups, .width = intact.width, .height = intact.height, .wall = intact.wall, .texel_tri = faces.tri, .texel_bary = faces.bary, .fallen = fallen, .method = intact.method };
}

/// A pixel whose mean channel is this far from the intact picture's has changed.
const changed = 48.0;
/// A roof group seen on this many pixels at N or more, this share of them
/// changed, has fallen.
const fallen_pixels = 20.0;
const fallen_share = 0.5;
/// The height of a fallen roof's rubble, of the building's height.
const rubble = 0.1;

/// The bake view of a model's own faces in its variant's own 2003 picture
/// (`placement` and `anchor` as the model was unrolled): the depth buffer
/// of its triangles (a triangle's `source` is its index) and their normals
/// turned towards the eye, as `mesh.worldNormal` turns the mesh's.
pub fn ownBakeView(gpa: std.mem.Allocator, m: Model, picture: Image, placement: mesh.Placement, anchor: [2]f64) !BakeView {
    const v = mesh.view(placement, 0, anchor);
    const points = try gpa.alloc([3]f64, m.vertices.len);
    for (points, m.vertices) |*p, vertex| p.* = v.apply(vertex.mesh);
    const tris = try gpa.alloc(mesh.Tri, m.triangles.len);
    const normals = try gpa.alloc([3]f64, m.triangles.len);
    for (tris, normals, m.triangles) |*o, *n, t| {
        o.* = .{ .v = t.v, .part = t.group };
        n.* = if (mesh.dot(t.normal, mesh.toward_eye) < 0) scaled(t.normal, -1) else t.normal;
    }
    return .{ .picture = picture, .view = v, .raster = try mesh.raster(gpa, points, tris, picture.width, picture.height), .normals = normals };
}

/// The model's shadow on the ground under the light of turn `light`
/// (`views.lightFor` of the 2003 light), as the camera turned `turn`
/// quarter turns sees it with the zero cross at `anchor` on a canvas of
/// `size`: opaque where a face's shadow covers a pixel's middle.
pub fn shadow(gpa: std.mem.Allocator, m: Model, turn: u2, light: u2, anchor: [2]f64, size: [2]u32) !Image {
    const l = views.lightFor(views.light_2003, light);
    const points = try gpa.alloc([3]f64, m.vertices.len);
    defer gpa.free(points);
    for (points, m.vertices) |*p, v| {
        const w = v.world;
        const q = mesh.quarter(.{ w[0] - l[0] * w[2] / l[2], w[1] - l[1] * w[2] / l[2] }, turn);
        const s = mesh.project(.{ q[0], q[1], 0 });
        p.* = .{ s[0] + anchor[0], s[1] + anchor[1], 0 };
    }
    const tris = try gpa.alloc(mesh.Tri, m.triangles.len);
    defer gpa.free(tris);
    for (tris, m.triangles) |*o, t| o.* = .{ .v = t.v, .part = t.group };
    const r = try mesh.raster(gpa, points, tris, size[0], size[1]);
    const out: Image = .{ .width = size[0], .height = size[1], .pixels = try gpa.alloc([4]u8, r.tri.len) };
    for (out.pixels, r.tri) |*p, t| p.* = if (t == mesh.Raster.none) .{ 0, 0, 0, 0 } else .{ 0, 0, 0, 255 };
    return out;
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

/// A 2 x 2 matrix, row major.
pub const Mat2 = [4]f64;

fn mul2(a: Mat2, b: Mat2) Mat2 {
    return .{ a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3], a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3] };
}

fn inverse2(a: Mat2) ?Mat2 {
    const det = a[0] * a[3] - a[1] * a[2];
    if (@abs(det) < 1e-12) return null;
    return .{ a[3] / det, -a[1] / det, -a[2] / det, a[0] / det };
}

/// The singular values of a 2 x 2 matrix, the larger first: how far it
/// stretches a unit step at most and at least.
fn singular(a: Mat2) [2]f64 {
    const p = a[0] * a[0] + a[1] * a[1] + a[2] * a[2] + a[3] * a[3];
    const d = a[0] * a[3] - a[1] * a[2];
    const r = @sqrt(@max(0, p * p - 4 * d * d));
    return .{ @sqrt((p + r) / 2), @sqrt(@max(0, p - r) / 2) };
}

/// The map of a triangle's texels to the points `q` of its corners (picture
/// pixels): d(x, y) / d(u, v); null for a triangle without area in the atlas.
fn uvMap(uv: [3][2]f64, q: [3][2]f64) ?Mat2 {
    const e = Mat2{ uv[1][0] - uv[0][0], uv[2][0] - uv[0][0], uv[1][1] - uv[0][1], uv[2][1] - uv[0][1] };
    const inv = inverse2(e) orelse return null;
    return mul2(.{ q[1][0] - q[0][0], q[2][0] - q[0][0], q[1][1] - q[0][1], q[2][1] - q[0][1] }, inv);
}

/// How many pixels of a picture drawn by `drawn` one pixel of the picture a
/// texel was painted from (`source`) covers, both maps of texels to pixels:
/// the larger of the ratios of their singular values, the face's densest
/// and its sparsest direction (1 where both cameras sample the face alike,
/// more where the render sees the face more squarely than the source did).
/// Not the singular values of `drawn` times `source` inverted: a quarter turn
/// draws an upright wall's texels sheared the other way at the same density
/// (1.62 by that measure), and a shear of the same density makes no streak.
pub fn magnification(drawn: Mat2, source: Mat2) f64 {
    const r = singular(drawn);
    const s = singular(source);
    if (s[1] < 1e-9) return std.math.inf(f64);
    return @max(r[0] / s[0], r[1] / s[1]);
}

/// A triangle's texels as the camera turned `turn` quarter turns draws them
/// (`render`'s projection), in pixels.
pub fn turnMap(m: Model, t: Triangle, turn: u2) ?Mat2 {
    var uv: [3][2]f64 = undefined;
    var q: [3][2]f64 = undefined;
    for (t.v, 0..) |v, i| {
        const w = m.vertices[v].world;
        const r = mesh.quarter(.{ w[0], w[1] }, turn);
        q[i] = mesh.project(.{ r[0], r[1], w[2] });
        uv[i] = m.vertices[v].uv;
    }
    return uvMap(uv, q);
}

/// A triangle's texels as a bake view's 2003 picture shows them, in pixels.
fn viewMap(m: Model, t: Triangle, v: mesh.View) ?Mat2 {
    var uv: [3][2]f64 = undefined;
    var q: [3][2]f64 = undefined;
    for (t.v, 0..) |vi, i| {
        const p = v.apply(m.vertices[vi].mesh);
        q[i] = .{ p[0], p[1] };
        uv[i] = m.vertices[vi].uv;
    }
    return uvMap(uv, q);
}

fn toF32(a: ?Mat2) [4]f32 {
    const x = a orelse return .{ 0, 0, 0, 0 };
    return .{ @floatCast(x[0]), @floatCast(x[1]), @floatCast(x[2]), @floatCast(x[3]) };
}

fn toF64(a: [4]f32) ?Mat2 {
    if (a[0] == 0 and a[1] == 0 and a[2] == 0 and a[3] == 0) return null;
    return .{ a[0], a[1], a[2], a[3] };
}

/// How far a texel's paint is stretched where `render` draws its face: the
/// `magnification` of its picture's pixel, 0 for paint no picture's pixel
/// gave (a flat colour).
pub fn texelStretch(drawn: ?Mat2, jacobian: [4]f32) f64 {
    const r = drawn orelse return 0;
    const s = toF64(jacobian) orelse return 0;
    return magnification(r, s);
}

/// The atlas of one state painted from its 2003 pictures, the variant's own
/// first (`bake_views[0]`), the 2003 light divided out (`views.shading` with
/// the building's own `wall` light), so the atlas is the surface's colour and
/// every turn is lit by the renderer.
///
/// `coherent` (the default, 2026-10-31): a texel takes the pixel under it in
/// a picture only where that picture samples its face densely enough for
/// every turn: no turn may draw one of its pixels over more than
/// `stretch_limit` pixels (a roof slope the 2003 camera sees almost edge on,
/// shown squarely by the opposite turn, was its pixels smeared into streaks;
/// the far side of a round emplacement was radial smears). Its own picture
/// where that holds, else the picture of the colourway that stretches least.
/// The rest is painted from the object itself (`fillUnseen`): a counterpart
/// face of its material copied texel for texel, then a synthesis from its
/// well-seen faces of that material, never a stretched pixel.
///
/// `tile` (2026-10-29): the variant's own picture where it sees the point
/// at least at `grazing`, else the picture that sees it most squarely, the
/// rest tiled from seen rectangles (`paintUnseen`).
///
/// With a `base` (a damage state's atlas starts from the intact one, of the
/// same layout) a texel no picture sees keeps the base's paint and mark.
pub fn bake(gpa: std.mem.Allocator, m: *Model, state: []const u8, bake_views: []const BakeView, base: ?Texture) !void {
    const count = @as(usize, m.width) * m.height;
    const image: Image = .{ .width = m.width, .height = m.height, .pixels = try gpa.alloc([4]u8, count) };
    const paint = try gpa.alloc(Paint, count);
    const source = try gpa.alloc(u8, count);
    const jacobian = try gpa.alloc([4]f32, count);
    @memset(source, 0);
    if (base) |b| {
        @memcpy(image.pixels, b.image.pixels);
        @memcpy(paint, b.paint);
        @memcpy(jacobian, b.jacobian);
    } else {
        @memset(image.pixels, .{ 0, 0, 0, 0 });
        @memset(paint, .none);
        @memset(jacobian, .{ 0, 0, 0, 0 });
    }
    // Per triangle its four turns' maps, and per view its map and how far
    // the turns stretch that view's pixels.
    const tri_count = m.triangles.len;
    const turns = try gpa.alloc([4]?Mat2, tri_count);
    defer gpa.free(turns);
    for (turns, m.triangles) |*t, tri| for (0..4) |k| {
        t[k] = turnMap(m.*, tri, @intCast(k));
    };
    const maps = try gpa.alloc(?Mat2, tri_count * bake_views.len);
    defer gpa.free(maps);
    const stretch = try gpa.alloc(f64, tri_count * bake_views.len);
    defer gpa.free(stretch);
    for (bake_views, 0..) |bv, j| for (m.triangles, 0..) |tri, ti| {
        const s = viewMap(m.*, tri, bv.view);
        maps[j * tri_count + ti] = s;
        var worst: f64 = if (s == null) std.math.inf(f64) else 0;
        if (s) |x| for (turns[ti]) |r| if (r) |rm| {
            worst = @max(worst, magnification(rm, x));
        };
        stretch[j * tri_count + ti] = worst;
    };
    // The least stretched pixel a texel's pictures do show, for a face no
    // picture shows well and nothing of the object can paint.
    const fallback = try gpa.alloc(Fallback, count);
    defer gpa.free(fallback);
    @memset(fallback, .{});
    for (m.texel_tri, m.texel_bary, 0..) |ti, bary, i| {
        if (ti == Model.none) continue;
        const t = m.triangles[ti];
        var p = [3]f64{ 0, 0, 0 };
        for (t.v, bary) |v, b| for (0..3) |c| {
            p[c] += m.vertices[v].mesh[c] * b;
        };
        if (base == null) paint[i] = .guessed;
        switch (m.method) {
            .tile => {
                var best: f64 = -std.math.inf(f64);
                // The variant's own picture first, then the others.
                for ([_]struct { least: f64, own: bool }{ .{ .least = grazing, .own = true }, .{ .least = grazing, .own = false }, .{ .least = 0, .own = false } }) |pass| {
                    if (best > -std.math.inf(f64)) break;
                    for (bake_views, 0..) |bv, j| {
                        if (pass.own and j > 0) break;
                        const at = seenAt(bv, t, p) orelse continue;
                        const n = bv.normals[t.source];
                        const facing = mesh.dot(n, mesh.toward_eye);
                        const pixel = bv.picture.pixels[at];
                        if (facing < pass.least or facing <= best or pixel[3] < 128) continue;
                        best = facing;
                        image.pixels[i] = views.relight(pixel, .{ 1, 1, 1 }, views.shading(n, 0, m.wall));
                        image.pixels[i][3] = 255;
                        paint[i] = .seen;
                        source[i] = @intCast(j + 1);
                        jacobian[i] = toF32(maps[j * tri_count + ti]);
                    }
                }
            },
            .coherent => {
                var chosen: ?usize = null;
                var least = std.math.inf(f64);
                for (bake_views, 0..) |bv, j| {
                    const at = seenAt(bv, t, p) orelse continue;
                    const n = bv.normals[t.source];
                    const pixel = bv.picture.pixels[at];
                    if (mesh.dot(n, mesh.toward_eye) <= 0 or pixel[3] < 128) continue;
                    const s = stretch[j * tri_count + ti];
                    if (s > stretch_limit) {
                        if (s < fallback[i].stretch) fallback[i] = .{ .stretch = s, .pixel = unlit(pixel, n, m.wall), .jacobian = toF32(maps[j * tri_count + ti]) };
                        continue;
                    }
                    if (s >= least) continue;
                    least = s;
                    chosen = j;
                    image.pixels[i] = unlit(pixel, n, m.wall);
                    // The variant's own picture wins wherever it serves.
                    if (j == 0) break;
                }
                if (chosen) |j| {
                    paint[i] = .seen;
                    source[i] = @intCast(j + 1);
                    jacobian[i] = toF32(maps[j * tri_count + ti]);
                }
            },
        }
    }
    const tex: Texture = .{ .state = state, .image = image, .paint = paint, .source = source, .jacobian = jacobian };
    switch (m.method) {
        .tile => if (base == null) try paintUnseen(gpa, m.*, tex) else try tileFallen(gpa, m.*, tex),
        .coherent => {
            try fillUnseen(gpa, m.*, tex, fallback, base != null);
            if (base != null) try fillFallen(gpa, m.*, tex);
        },
    }
    var list: std.ArrayList(Texture) = .empty;
    try list.appendSlice(gpa, m.textures);
    try list.append(gpa, tex);
    m.textures = list.items;
}

/// The least stretched pixel a picture shows of a texel it does not show
/// well enough (`bake`): `stretch` inf where none shows it.
const Fallback = struct { stretch: f64 = std.math.inf(f64), pixel: [4]u8 = .{ 0, 0, 0, 0 }, jacobian: [4]f32 = .{ 0, 0, 0, 0 } };

/// A picture's pixel with the 2003 light on a face of normal `n` divided out.
fn unlit(pixel: [4]u8, n: [3]f64, wall: [3]f64) [4]u8 {
    var out = views.relight(pixel, .{ 1, 1, 1 }, views.shading(n, 0, wall));
    out[3] = 255;
    return out;
}

/// The pixel of a bake view showing mesh point `p` of triangle `t`, where no
/// face stands in front of it: its own, one in its plane, or none at that
/// pixel's middle (a picket narrower than a pixel).
fn seenAt(bv: BakeView, t: Triangle, p: [3]f64) ?usize {
    const q = bv.view.apply(p);
    const at = bv.raster.at(q[0], q[1]) orelse return null;
    const shown = bv.raster.tri[at];
    if (shown != mesh.Raster.none and shown != t.source and (bv.raster.depth[at] > q[2] + visible or mesh.dot(bv.normals[shown], bv.normals[t.source]) < 0.9)) return null;
    return at;
}

/// Whether paint whose 2003 pixels a texel step spans `jacobian` stays
/// within `stretch_limit` on a face whose turns draw it by `turns`: a
/// counterpart or exemplar texel well sampled for its own face may be too
/// sparse for a face the turns show more squarely.
fn fits(turns: [4]?Mat2, jacobian: [4]f32) bool {
    const s = toF64(jacobian) orelse return true;
    for (turns) |t| if (t) |r| if (magnification(r, s) > stretch_limit) return false;
    return true;
}

/// A chart's place for the exemplar choice: its turns' maps (a chart is one
/// plane unrolled with one pair of axes, so one triangle's serve), its
/// outward normal and its middle in the world.
const ChartPlace = struct { turns: [4]?Mat2, normal: [3]f64, middle: [3]f64 };

fn chartPlaces(gpa: std.mem.Allocator, m: Model, members: []const []const u32) ![]ChartPlace {
    const out = try gpa.alloc(ChartPlace, m.charts.len);
    for (out, members) |*o, list| {
        // A damage state's chart the cut took whole has no faces left.
        if (list.len == 0) {
            o.* = .{ .turns = .{ null, null, null, null }, .normal = .{ 0, 0, 0 }, .middle = .{ 0, 0, 0 } };
            continue;
        }
        var best: usize = list[0];
        var turns: [4]?Mat2 = .{ null, null, null, null };
        var mid = [3]f64{ 0, 0, 0 };
        var n: f64 = 0;
        for (list) |ti| {
            for (m.triangles[ti].v) |v| for (0..3) |c| {
                mid[c] += m.vertices[v].world[c];
            };
            n += 3;
            if (turns[0] == null) {
                for (0..4) |k| turns[k] = turnMap(m, m.triangles[ti], @intCast(k));
                best = ti;
            }
        }
        o.* = .{ .turns = turns, .normal = m.triangles[best].normal, .middle = .{ mid[0] / n, mid[1] / n, mid[2] / n } };
    }
    return out;
}

/// Each chart's triangles.
fn chartTriangles(gpa: std.mem.Allocator, m: Model) ![][]u32 {
    const lists = try gpa.alloc(std.ArrayList(u32), m.charts.len);
    for (lists) |*l| l.* = .empty;
    for (m.triangles, 0..) |t, ti| try lists[t.chart].append(gpa, @intCast(ti));
    const out = try gpa.alloc([]u32, m.charts.len);
    for (out, lists) |*o, l| o.* = l.items;
    return out;
}

/// The texels an exemplar may come from: seen, and so are its four
/// neighbours on the same chart, so that no exemplar holds the pixel where
/// the mesh and the 2003 picture disagree along an edge.
fn goodTexels(gpa: std.mem.Allocator, m: Model, tex: Texture, seen: []const bool) ![]bool {
    const good = try gpa.alloc(bool, seen.len);
    @memset(good, false);
    const w = m.width;
    for (0..m.height) |y| for (0..w) |x| {
        const i = y * w + x;
        if (!seen[i] or m.texel_tri[i] == Model.none) continue;
        const chart = m.triangles[m.texel_tri[i]].chart;
        good[i] = for ([_][2]i64{ .{ -1, 0 }, .{ 1, 0 }, .{ 0, -1 }, .{ 0, 1 } }) |d| {
            const nx = @as(i64, @intCast(x)) + d[0];
            const ny = @as(i64, @intCast(y)) + d[1];
            if (nx < 0 or ny < 0 or nx >= w or ny >= m.height) break false;
            const j: usize = @intCast(ny * w + nx);
            if (!seen[j] or m.texel_tri[j] == Model.none or m.triangles[m.texel_tri[j]].chart != chart) break false;
        } else true;
    };
    _ = tex;
    return good;
}

/// The largest rectangle of a chart's texels marked in `mask` (the
/// histogram method).
fn maskRect(gpa: std.mem.Allocator, m: Model, mask: []const bool, c: Chart) !?Rect {
    const heights = try gpa.alloc(u32, c.w);
    defer gpa.free(heights);
    @memset(heights, 0);
    var best: ?Rect = null;
    var best_area: u64 = 0;
    for (0..c.h) |ry| {
        for (0..c.w) |rx| {
            const i = (c.y + ry) * m.width + c.x + rx;
            heights[rx] = if (mask[i]) heights[rx] + 1 else 0;
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

fn rectArea(r: ?Rect) u64 {
    return if (r) |q| @as(u64, q[2]) * q[3] else 0;
}

/// The mean colour of the texels of `mask` in a chart (or of the rectangle
/// `rect`), and how many there are.
fn meanOf(m: Model, tex: Texture, mask: ?[]const bool, c: Chart, rect: ?Rect) struct { rgb: [3]f64, n: f64 } {
    const box: Rect = rect orelse .{ c.x, c.y, c.w, c.h };
    var sum = [3]f64{ 0, 0, 0 };
    var n: f64 = 0;
    for (box[1]..box[1] + box[3]) |y| for (box[0]..box[0] + box[2]) |x| {
        const i = y * m.width + x;
        if (mask) |k| if (!k[i]) continue;
        for (0..3) |ch| sum[ch] += @floatFromInt(tex.image.pixels[i][ch]);
        n += 1;
    };
    if (n == 0) return .{ .rgb = sum, .n = 0 };
    return .{ .rgb = .{ sum[0] / n, sum[1] / n, sum[2] / n }, .n = n };
}

/// The gain that brings paint of mean colour `from` to the brightness of
/// `to`, one factor for all three channels (a factor a channel tinted a
/// plaster wall salmon), within a sixth either way: the 2003 light is
/// divided out of every seen texel, but what the light model leaves (a sunlit
/// wall a little brighter than its shaded twin) must not carry a lit face's
/// brightness onto a shaded one.
fn gainOf(from: [3]f64, to: [3]f64) [3]f64 {
    const a = 0.3 * from[0] + 0.59 * from[1] + 0.11 * from[2];
    const b = 0.3 * to[0] + 0.59 * to[1] + 0.11 * to[2];
    const g = if (a < 1) 1 else std.math.clamp(b / a, 1.0 / 1.18, 1.18);
    return .{ g, g, g };
}

fn applyGain(p: [4]u8, g: [3]f64) [4]u8 {
    var out = p;
    for (0..3) |c| out[c] = @intFromFloat(std.math.clamp(@round(@as(f64, @floatFromInt(p[c])) * g[c]), 0, 255));
    out[3] = 255;
    return out;
}

/// A texel's row in its chart as a line of the chart's plane at height z:
/// on a plane whose u runs level, a row is one height, so a texel's row is
/// `a` z + `b` (null for a level face, where rows say nothing of height).
fn rowOfHeight(m: Model, members: []const u32) ?[2]f64 {
    if (members.len == 0) return null;
    var lo: ?Vertex = null;
    var hi: ?Vertex = null;
    for (members) |ti| for (m.triangles[ti].v) |v| {
        const x = m.vertices[v];
        if (lo == null or x.world[2] < lo.?.world[2]) lo = x;
        if (hi == null or x.world[2] > hi.?.world[2]) hi = x;
    };
    const dz = hi.?.world[2] - lo.?.world[2];
    if (dz < 0.5) return null;
    const a = (hi.?.uv[1] - lo.?.uv[1]) / dz;
    return .{ a, lo.?.uv[1] - a * lo.?.world[2] };
}

/// Every texel no picture shows well painted from the object itself, as a
/// painter would: first the same place of a counterpart face, a face of the
/// same material that looks as steeply and is about as large (the back roof
/// slope from the front one, the back wall from the front wall, the far side
/// of a sandbag ring from its near side), copied texel for texel with its
/// heights kept; what that leaves is synthesised (`quilt`) from the largest
/// well-seen rectangle of the chart itself, else of its material, else of
/// the faces that look the same way, patch by patch along the face's own
/// axes, rows kept at their height above the face's foot (courses, tiles and
/// timbers stay level and in step). A face nothing of the object can paint
/// takes its least stretched picture, else the mean seen colour.
///
/// A damage state (`damaged`: its atlas starts from the intact one) paints
/// so only the texels its damaged picture shows too stretched to take, from
/// what that picture shows well (a ruin's walls from its own rubble and
/// scorch, not the intact paint); a fallen roof's are `fillFallen`'s, and a
/// texel the damaged picture does not show keeps the intact paint.
fn fillUnseen(gpa: std.mem.Allocator, m: Model, tex: Texture, fallback: []const Fallback, damage: bool) !void {
    const members = try chartTriangles(gpa, m);
    if (!damage) try dropMisplaced(gpa, m, tex);
    const seen = try gpa.alloc(bool, tex.paint.len);
    for (seen, tex.paint, tex.source) |*s, k, src| s.* = if (damage) src > 0 else k == .seen;
    const pending = try gpa.alloc(bool, tex.paint.len);
    for (pending, tex.paint, m.texel_tri, tex.source, fallback) |*p, k, t, src, f| {
        p.* = if (damage)
            t != Model.none and src == 0 and f.stretch < std.math.inf(f64) and !(m.fallen.len > 0 and m.fallen[m.triangles[t].chart])
        else
            t != Model.none and k != .seen;
    }
    const asked = try gpa.dupe(bool, pending);
    const places = try chartPlaces(gpa, m, members);
    var lo = [3]f64{ std.math.inf(f64), std.math.inf(f64), std.math.inf(f64) };
    var hi = [3]f64{ -std.math.inf(f64), -std.math.inf(f64), -std.math.inf(f64) };
    for (m.vertices) |v| for (0..3) |c| {
        lo[c] = @min(lo[c], v.world[c]);
        hi[c] = @max(hi[c], v.world[c]);
    };
    const size = @max(1, @sqrt(mesh.dot(sub(hi, lo), sub(hi, lo))));

    // The mean seen colour of each material, what a face's paint is brought
    // to where the face itself shows too little.
    var material_mean: std.AutoHashMap(u32, [4]f64) = .init(gpa);
    for (m.charts) |c| {
        const mean = meanOf(m, tex, seen, c, null);
        const e = try material_mean.getOrPutValue(c.material, .{ 0, 0, 0, 0 });
        for (0..3) |ch| e.value_ptr[ch] += mean.rgb[ch] * mean.n;
        e.value_ptr[3] += mean.n;
    }
    var all = [4]f64{ 0, 0, 0, 0 };
    for (tex.image.pixels, seen) |p, s| if (s) {
        for (0..3) |c| all[c] += @floatFromInt(p[c]);
        all[3] += 1;
    };
    const targets = try gpa.alloc(?[3]f64, m.charts.len);
    for (m.charts, targets) |c, *t| {
        const own = meanOf(m, tex, seen, c, null);
        const mat = material_mean.get(c.material).?;
        t.* = if (own.n >= 32) own.rgb else if (mat[3] > 0) .{ mat[0] / mat[3], mat[1] / mat[3], mat[2] / mat[3] } else null;
    }

    try copyCounterparts(gpa, m, tex, members, places, seen, pending, targets);

    // A copied texel is a well-sampled pixel moved one for one, so it may
    // serve an exemplar too (an apse's hidden facets take its seen ones).
    const known = try gpa.alloc(bool, tex.paint.len);
    for (known, seen, asked, pending, tex.paint) |*k, s, a, p, paint| k.* = s or (a and !p and paint == .copied);
    const exemplary = try goodTexels(gpa, m, tex, known);
    const rects = try gpa.alloc(?Rect, m.charts.len);
    for (m.charts, rects) |c, *r| r.* = try maskRect(gpa, m, exemplary, c);
    const usable = struct {
        fn f(r: ?Rect) bool {
            return if (r) |q| q[2] >= own_tile and q[3] >= own_tile else false;
        }
    }.f;
    for (m.charts, 0..) |c, ci| {
        var any = false;
        for (c.y..c.y + c.h) |y| {
            for (c.x..c.x + c.w) |x| if (pending[y * m.width + x]) {
                any = true;
                break;
            };
            if (any) break;
        }
        if (!any) continue;
        // The exemplar: the chart's own well-seen rectangle where it is a
        // quarter of the chart or more, else the best of its material, else
        // of the faces that look the same way: best by its size, how alike
        // it faces and how near it stands (an apse's facet takes the apse,
        // not the porch's arched door), and only one whose pixels this
        // chart's turns do not stretch, nor too small for the chart. Without
        // one, the right material a little stretched beats the wrong one:
        // the least stretched exemplar of those, else the least stretched
        // pixels the pictures show of the face itself, and only then any
        // face's exemplar (a wall in roof tiles, the hold-out's confetti).
        var exemplar: ?usize = if (usable(rects[ci]) and 4 * rectArea(rects[ci]) >= @as(u64, c.w) * c.h) ci else null;
        const look = lookOf(m.groups[c.group]);
        var least_stretch: ?usize = null;
        var least: f64 = std.math.inf(f64);
        var own_pixels = false;
        for ([_]u2{ 0, 1, 2 }) |round| {
            if (exemplar != null) break;
            if (round == 2) {
                if (least_stretch != null) {
                    exemplar = least_stretch;
                    break;
                }
                for (c.y..c.y + c.h) |y| for (c.x..c.x + c.w) |x| {
                    const i = y * m.width + x;
                    own_pixels = own_pixels or (pending[i] and fallback[i].stretch < std.math.inf(f64));
                };
                if (own_pixels) break;
            }
            var best_score: f64 = 0;
            for (m.charts, rects, places, 0..) |d, r, place, di| {
                const same = switch (round) {
                    0 => d.material == c.material,
                    1 => std.mem.eql(u8, lookOf(m.groups[d.group]), look),
                    else => true,
                };
                if (!same or !usable(r)) continue;
                const q = r.?;
                // Big enough for the face: a chimney's few bricks are no
                // exemplar for a wall of the same name (e_house02_2's back
                // wing stands high on a small foot, so the unwrap calls it a
                // chimney too).
                if (round < 2 and 16 * rectArea(r) < @as(u64, c.w) * c.h and (q[2] < 32 or q[3] < 32)) continue;
                const sv = toF64(tex.jacobian[(q[1] + q[3] / 2) * m.width + q[0] + q[2] / 2]);
                var worst: f64 = 0;
                if (sv) |sm| for (places[ci].turns) |t| if (t) |tm| {
                    worst = @max(worst, magnification(tm, sm));
                };
                if (worst < least) {
                    least = worst;
                    least_stretch = di;
                }
                if (worst > stretch_limit) continue;
                const along = @max(0, mesh.dot(places[ci].normal, place.normal));
                const apart = sub(places[ci].middle, place.middle);
                const near = 1 + @sqrt(mesh.dot(apart, apart)) / (0.25 * size);
                // The largest square it holds, not its area: a strip a patch
                // wide repeats its columns down a whole wall as stripes.
                const side: f64 = @floatFromInt(@min(q[2], q[3]));
                const score = side * side * (1 + 2 * along) / near;
                if (score > best_score) {
                    best_score = score;
                    exemplar = di;
                }
            }
        }
        if (exemplar == null and !own_pixels) exemplar = least_stretch;
        if (exemplar) |e| {
            const rect = rects[e].?;
            const from = meanOf(m, tex, null, m.charts[e], rect);
            const gain = if (e == ci) [3]f64{ 1, 1, 1 } else if (targets[ci]) |t| gainOf(from.rgb, t) else [3]f64{ 1, 1, 1 };
            const rows = !std.mem.eql(u8, look, "under") and rowOfHeight(m, members[ci]) != null and rowOfHeight(m, members[e]) != null;
            try quilt(gpa, m, tex, c, pending, rect, m.charts[e], gain, targets[ci], ci, rows);
            continue;
        }
        if (damage) continue;
        for (c.y..c.y + c.h) |y| for (c.x..c.x + c.w) |x| {
            const i = y * m.width + x;
            if (!pending[i]) continue;
            pending[i] = false;
            if (fallback[i].stretch < std.math.inf(f64)) {
                tex.image.pixels[i] = fallback[i].pixel;
                tex.jacobian[i] = fallback[i].jacobian;
            } else {
                tex.image.pixels[i] = if (all[3] > 0) .{ @intFromFloat(all[0] / all[3]), @intFromFloat(all[1] / all[3]), @intFromFloat(all[2] / all[3]), 255 } else .{ 128, 128, 128, 255 };
            }
        };
    }
}

/// Where the mesh and the 2003 picture disagree, a face the mesh shows at N
/// takes pixels of another face of the picture: e_house02_2's back wing,
/// which the 2003 house does not have, stands where the picture shows the
/// main roof, and its walls took roof tiles. A wall chart whose seen paint is
/// far nearer the object's roofs than its other walls (the mean colour,
/// under `misplaced` of the distance), or a roof chart nearer its walls than
/// its other roofs, is taken as unseen, if it stands `misplaced_far` or more
/// from its own look; it needs a reference of `misplaced_least` seen texels
/// of each look.
fn dropMisplaced(gpa: std.mem.Allocator, m: Model, tex: Texture) !void {
    const Sum = struct { rgb: [3]f64 = .{ 0, 0, 0 }, n: f64 = 0 };
    const looks = [_][]const u8{ "wall", "roof" };
    var total = [2]Sum{ .{}, .{} };
    const per = try gpa.alloc(Sum, m.charts.len);
    defer gpa.free(per);
    @memset(per, .{});
    for (tex.image.pixels, tex.paint, m.texel_tri) |p, k, t| {
        if (k != .seen or t == Model.none) continue;
        const ci = m.triangles[t].chart;
        for (0..3) |ch| per[ci].rgb[ch] += @floatFromInt(p[ch]);
        per[ci].n += 1;
    }
    const lookIndex = struct {
        fn f(mm: Model, c: Chart) ?usize {
            for (looks, 0..) |l, i| if (std.mem.eql(u8, lookOf(mm.groups[c.group]), l)) return i;
            return null;
        }
    }.f;
    for (m.charts, per) |c, q| {
        const i = lookIndex(m, c) orelse continue;
        for (0..3) |ch| total[i].rgb[ch] += q.rgb[ch];
        total[i].n += q.n;
    }
    const dist = struct {
        fn f(a: [3]f64, an: f64, b: [3]f64, bn: f64) f64 {
            var d: f64 = 0;
            for (0..3) |ch| {
                const x = a[ch] / an - b[ch] / bn;
                d += x * x;
            }
            return @sqrt(d);
        }
    }.f;
    for (m.charts, per, 0..) |c, q, ci| {
        const i = lookIndex(m, c) orelse continue;
        if (q.n < own_tile * own_tile) continue;
        var same = total[i];
        for (0..3) |ch| same.rgb[ch] -= q.rgb[ch];
        same.n -= q.n;
        const other = total[1 - i];
        if (same.n < misplaced_least or other.n < misplaced_least) continue;
        const apart = dist(q.rgb, q.n, same.rgb, same.n);
        if (apart < misplaced_far or dist(q.rgb, q.n, other.rgb, other.n) >= misplaced * apart) continue;
        for (c.y..c.y + c.h) |y| for (c.x..c.x + c.w) |x| {
            const t = y * m.width + x;
            if (tex.paint[t] != .seen or m.texel_tri[t] == Model.none or m.triangles[m.texel_tri[t]].chart != ci) continue;
            tex.paint[t] = .guessed;
            tex.source[t] = 0;
        };
    }
}

const misplaced = 0.5;
const misplaced_least = 200;
/// How far, in levels of the mean colour, a chart's seen paint must stand from
/// its own look's to be misplaced at all: an African house's mud walls and
/// flat roofs are one colour, and noise must not decide.
const misplaced_far = 40;

/// A counterpart takes over a face only where it paints this share of what
/// the face lacks or more (a few texels here and there would be blotches).
const counterpart_share = 0.25;

/// Copies onto each chart with texels left to paint the same places of its
/// counterparts (`fillUnseen`). A rotation of the object about an upright
/// axis keeps a face's level rows level and its slope, so it maps a chart's
/// texels onto its counterpart's by a shift alone: rows matched by height
/// (`rowOfHeight`), columns by the charts' middles. Counterparts are tried
/// most covering first, each painting what the ones before left.
fn copyCounterparts(gpa: std.mem.Allocator, m: Model, tex: Texture, members: []const []const u32, places: []const ChartPlace, seen: []const bool, pending: []bool, targets: []const ?[3]f64) !void {
    const rows = try gpa.alloc(?[2]f64, m.charts.len);
    for (rows, members) |*r, list| r.* = rowOfHeight(m, list);
    const Candidate = struct { chart: u32, shift: [2]i64, hits: usize };
    var candidates: std.ArrayList(Candidate) = .empty;
    for (m.charts, members, 0..) |c, list, ci| {
        var lacking: usize = 0;
        for (c.y..c.y + c.h) |y| for (c.x..c.x + c.w) |x| {
            lacking += @intFromBool(pending[y * m.width + x]);
        };
        if (lacking == 0 or list.len == 0) continue;
        const n = m.triangles[list[0]].normal;
        candidates.clearRetainingCapacity();
        for (m.charts, members, 0..) |d, dlist, di| {
            if (di == ci or d.material != c.material or dlist.len == 0) continue;
            const dn = m.triangles[dlist[0]].normal;
            if (@abs(dn[2] - n[2]) > 0.03) continue;
            // About as large: within a sixth and two texels either way.
            const near = struct {
                fn f(a: u32, b: u32) bool {
                    const fa: f64 = @floatFromInt(a);
                    const fb: f64 = @floatFromInt(b);
                    return @abs(fa - fb) <= @max(fa, fb) / 6 + 2;
                }
            }.f;
            if (!near(c.w, d.w) or !near(c.h, d.h)) continue;
            const dx = @as(i64, d.x) + @divFloor(@as(i64, d.w), 2) - @as(i64, c.x) - @divFloor(@as(i64, c.w), 2);
            const dy: i64 = if (rows[ci] != null and rows[di] != null and @abs(rows[ci].?[0] - rows[di].?[0]) < 0.05 * @abs(rows[ci].?[0]) + 0.01)
                @intFromFloat(@round(rows[di].?[1] - rows[ci].?[1]))
            else
                @as(i64, d.y) + @divFloor(@as(i64, d.h), 2) - @as(i64, c.y) - @divFloor(@as(i64, c.h), 2);
            var hits: usize = 0;
            for (c.y..c.y + c.h) |y| for (c.x..c.x + c.w) |x| {
                if (!pending[y * m.width + x]) continue;
                hits += @intFromBool(counterpartTexel(m, tex, seen, places[ci].turns, d, @intCast(di), @as(i64, @intCast(x)) + dx, @as(i64, @intCast(y)) + dy) != null);
            };
            if (@as(f64, @floatFromInt(hits)) >= counterpart_share * @as(f64, @floatFromInt(lacking))) try candidates.append(gpa, .{ .chart = @intCast(di), .shift = .{ dx, dy }, .hits = hits });
        }
        std.mem.sort(Candidate, candidates.items, {}, struct {
            fn less(_: void, a: Candidate, b: Candidate) bool {
                return a.hits > b.hits or (a.hits == b.hits and a.chart < b.chart);
            }
        }.less);
        for (candidates.items) |cand| {
            const d = m.charts[cand.chart];
            const from = meanOf(m, tex, seen, d, null);
            const gain = if (targets[ci]) |t| gainOf(from.rgb, t) else [3]f64{ 1, 1, 1 };
            for (c.y..c.y + c.h) |y| for (c.x..c.x + c.w) |x| {
                const i = y * m.width + x;
                if (!pending[i]) continue;
                const j = counterpartTexel(m, tex, seen, places[ci].turns, d, cand.chart, @as(i64, @intCast(x)) + cand.shift[0], @as(i64, @intCast(y)) + cand.shift[1]) orelse continue;
                tex.image.pixels[i] = applyGain(tex.image.pixels[j], gain);
                tex.jacobian[i] = tex.jacobian[j];
                tex.paint[i] = .copied;
                pending[i] = false;
            };
        }
    }
}

/// The texel (x, y) of chart `d` where it is one of its seen texels and its
/// pixel fits the turns of the face it is copied to.
fn counterpartTexel(m: Model, tex: Texture, seen: []const bool, turns: [4]?Mat2, d: Chart, di: u32, x: i64, y: i64) ?usize {
    if (x < d.x or y < d.y or x >= @as(i64, d.x) + d.w or y >= @as(i64, d.y) + d.h) return null;
    const j: usize = @intCast(y * @as(i64, m.width) + x);
    if (!seen[j] or m.texel_tri[j] == Model.none or m.triangles[m.texel_tri[j]].chart != di) return null;
    if (!fits(turns, tex.jacobian[j])) return null;
    return j;
}

/// The side of a synthesis patch, in texels (8 world units: a window, a
/// few roof tiles, a timber panel).
const patch = 16;
/// How much a patch's mean colour away from its face's counts against it, per
/// texel it paints, against a painted texel's squared difference (`quilt`).
const prior = 0.5;
/// About how many places of the exemplar a patch is matched against.
const match_places = 600;

/// A deterministic hash of a patch's place, which picks among the
/// exemplar's equally good matches.
fn mix(seed: u64) u64 {
    var z = seed +% 0x9e3779b97f4a7c15;
    z = (z ^ (z >> 30)) *% 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) *% 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

/// Image quilting (Efros and Freeman 2001) of a chart's `pending` texels from
/// the exemplar rectangle `ex` (of chart `ex_chart`), at one texel for one,
/// along the chart's own axes: patches laid from the chart's lower left
/// corner, row by row upwards, each the exemplar's place whose texels best
/// match the texels already painted under the patch (seen ones, copied ones
/// and those of the patches before), one of those within a tenth of the
/// best picked by the patch's place; where it overlaps the patches before it
/// is cut along the path of least difference, so no seam runs straight. With
/// `rows` (a face whose rows are heights) a patch comes from the exemplar's
/// rows at its own height above its face's foot, within two texels, where
/// the exemplar reaches that height. Painted texels are marked guessed.
fn quilt(gpa: std.mem.Allocator, m: Model, tex: Texture, c: Chart, pending: []bool, ex: Rect, ex_chart: Chart, gain: [3]f64, target: ?[3]f64, seed: usize, rows: bool) !void {
    const size: u32 = @min(patch, ex[2], ex[3]);
    const pw: u32 = @min(size, c.w);
    const ph: u32 = @min(size, c.h);
    const overlap: u32 = @max(2, size / 4);
    const step_x = if (pw > overlap) pw - overlap else pw;
    const step_y = if (ph > overlap) ph - overlap else ph;
    var xs: std.ArrayList(u32) = .empty;
    defer xs.deinit(gpa);
    var x0 = c.x;
    while (true) {
        try xs.append(gpa, x0);
        if (x0 + pw >= c.x + c.w) break;
        x0 = @min(x0 + step_x, c.x + c.w - pw);
    }
    var ys: std.ArrayList(u32) = .empty;
    defer ys.deinit(gpa);
    var y0 = c.y + c.h - ph;
    while (true) {
        try ys.append(gpa, y0);
        if (y0 == c.y) break;
        y0 = if (y0 >= c.y + step_y) y0 - step_y else c.y;
    }
    // This quilting's own texels, which a later patch may cut into.
    var mine: std.AutoHashMap(usize, void) = .init(gpa);
    defer mine.deinit();
    const Choice = struct { x: u32, y: u32, err: f64 };
    var choices: std.ArrayList(Choice) = .empty;
    defer choices.deinit(gpa);
    const w = m.width;
    // The exemplar's sums (gain applied), for a patch's mean colour at once.
    const sw = ex[2] + 1;
    const sums = try gpa.alloc([3]f64, @as(usize, sw) * (ex[3] + 1));
    defer gpa.free(sums);
    @memset(sums, .{ 0, 0, 0 });
    for (0..ex[3]) |yy| for (0..ex[2]) |xx| {
        const p = tex.image.pixels[(ex[1] + yy) * w + ex[0] + xx];
        for (0..3) |ch| sums[(yy + 1) * sw + xx + 1][ch] = @as(f64, @floatFromInt(p[ch])) * gain[ch] + sums[yy * sw + xx + 1][ch] + sums[(yy + 1) * sw + xx][ch] - sums[yy * sw + xx][ch];
    };
    const exemplar = struct {
        fn f(t: Texture, width: u32, x: usize, y: usize, g: [3]f64) [4]u8 {
            return applyGain(t.image.pixels[y * width + x], g);
        }
    }.f;
    for (ys.items, 0..) |py, r| for (xs.items, 0..) |px, q| {
        var any = false;
        for (py..py + ph) |y| for (px..px + pw) |x| {
            any = any or pending[y * w + x];
        };
        if (!any) continue;
        var to_paint: f64 = 0;
        for (py..py + ph) |y| for (px..px + pw) |x| {
            to_paint += @floatFromInt(@intFromBool(pending[y * w + x]));
        };
        // The exemplar's rows to choose from.
        var y_lo = ex[1];
        var y_hi = ex[1] + ex[3] - ph;
        if (rows) {
            const above: i64 = @as(i64, c.y + c.h) - (py + ph);
            const want: i64 = @as(i64, ex_chart.y + ex_chart.h) - above - ph;
            if (want + 2 >= y_lo and want - 2 <= y_hi) {
                y_lo = @intCast(@max(@as(i64, y_lo), want - 2));
                y_hi = @intCast(@min(@as(i64, y_hi), want + 2));
            }
        }
        choices.clearRetainingCapacity();
        var best = std.math.inf(f64);
        // At most about `match_places` places, on a grid whose phase the
        // patch's place picks: a large exemplar of a level face has tens of
        // thousands, which close neighbours of one another add nothing to.
        const n_rows: u64 = y_hi - y_lo + 1;
        const n_cols: u64 = ex[2] - pw + 1;
        const grid_y: u32 = if (n_rows <= 5) 1 else @intCast(std.math.clamp(std.math.sqrt(n_rows * n_cols / match_places), 1, n_rows));
        const grid_x: u32 = @intCast(std.math.clamp((n_cols * ((n_rows + grid_y - 1) / grid_y) + match_places - 1) / match_places, 1, n_cols));
        const phase = mix(@as(u64, seed) ^ (@as(u64, px) << 20) ^ (@as(u64, py) << 40));
        var ey = y_lo + @as(u32, @intCast(phase % grid_y));
        while (ey <= y_hi) : (ey += grid_y) {
            var ex_x = ex[0] + @as(u32, @intCast((phase >> 16) % grid_x));
            while (ex_x + pw <= ex[0] + ex[2]) : (ex_x += grid_x) {
                // The texels to paint should keep the face's colour: a
                // patch's mean against the face's expected one, weighed by
                // how many it paints (with nothing painted around, a narrow
                // facet would otherwise take the exemplar's windows at
                // random, dark blocks in a plain wall).
                var err: f64 = 0;
                if (target) |tc| {
                    const x0r = ex_x - ex[0];
                    const y0r = ey - ex[1];
                    const area: f64 = @floatFromInt(pw * ph);
                    for (0..3) |ch| {
                        const sum = sums[(y0r + ph) * sw + x0r + pw][ch] - sums[y0r * sw + x0r + pw][ch] - sums[(y0r + ph) * sw + x0r][ch] + sums[y0r * sw + x0r][ch];
                        const d = sum / area - tc[ch];
                        err += prior * to_paint * d * d;
                    }
                }
                const cutoff = best * 1.1 + 1;
                outer: for (0..ph) |wy| {
                    for (0..pw) |wx| {
                        const i = (py + wy) * w + px + wx;
                        if (pending[i]) continue;
                        const a = tex.image.pixels[i];
                        const b = exemplar(tex, w, ex_x + wx, ey + wy, gain);
                        for (0..3) |ch| {
                            const d = @as(f64, @floatFromInt(a[ch])) - @as(f64, @floatFromInt(b[ch]));
                            err += d * d;
                        }
                    }
                    if (err > cutoff) break :outer;
                }
                if (err > cutoff) continue;
                best = @min(best, err);
                try choices.append(gpa, .{ .x = ex_x, .y = ey, .err = err });
            }
        }
        var ok: usize = 0;
        for (choices.items) |ch| ok += @intFromBool(ch.err <= best * 1.1 + 1);
        var pick = mix(@as(u64, seed) *% 0x100000001b3 ^ (@as(u64, px) << 32) ^ py) % @max(ok, 1);
        const chosen = for (choices.items) |ch| {
            if (ch.err > best * 1.1 + 1) continue;
            if (pick == 0) break ch;
            pick -= 1;
        } else choices.items[0];
        // The cuts through the overlaps with the patch to the left and the
        // row below: per row the column, per column the row, where the new
        // patch begins, each the path of least squared difference.
        const left: u32 = if (q > 0) xs.items[q - 1] + pw - px else 0;
        const below: u32 = if (r > 0) py + ph - ys.items[r - 1] else 0;
        var cut_col: [patch]u32 = @splat(0);
        var cut_row: [patch]u32 = @splat(ph);
        const diff = struct {
            fn f(t: Texture, own: *const std.AutoHashMap(usize, void), width: u32, i: usize, b: [4]u8) f64 {
                if (!own.contains(i)) return 0;
                var e: f64 = 0;
                for (0..3) |ch| {
                    const d = @as(f64, @floatFromInt(t.image.pixels[i][ch])) - @as(f64, @floatFromInt(b[ch]));
                    e += d * d;
                }
                _ = width;
                return e;
            }
        }.f;
        if (left > 0 and left < pw) {
            var cost: [patch][patch]f64 = undefined;
            var from: [patch][patch]u32 = undefined;
            for (0..ph) |wy| for (0..left) |wx| {
                const e = diff(tex, &mine, w, (py + wy) * w + px + wx, exemplar(tex, w, chosen.x + wx, chosen.y + wy, gain));
                var prev: f64 = 0;
                from[wy][wx] = @intCast(wx);
                if (wy > 0) {
                    prev = std.math.inf(f64);
                    const lo = if (wx > 0) wx - 1 else 0;
                    for (lo..@min(left, wx + 2)) |k| if (cost[wy - 1][k] < prev) {
                        prev = cost[wy - 1][k];
                        from[wy][wx] = @intCast(k);
                    };
                }
                cost[wy][wx] = e + prev;
            };
            var at: u32 = 0;
            for (0..left) |k| if (cost[ph - 1][k] < cost[ph - 1][at]) {
                at = @intCast(k);
            };
            var wy = ph;
            while (wy > 0) {
                wy -= 1;
                cut_col[wy] = at;
                at = from[wy][at];
            }
        }
        if (below > 0 and below < ph) {
            const top = ph - below;
            var cost: [patch][patch]f64 = undefined;
            var from: [patch][patch]u32 = undefined;
            for (0..pw) |wx| for (top..ph) |wy| {
                const e = diff(tex, &mine, w, (py + wy) * w + px + wx, exemplar(tex, w, chosen.x + wx, chosen.y + wy, gain));
                var prev: f64 = 0;
                from[wx][wy] = @intCast(wy);
                if (wx > 0) {
                    prev = std.math.inf(f64);
                    const lo = if (wy > top) wy - 1 else top;
                    for (lo..@min(ph, wy + 2)) |k| if (cost[wx - 1][k] < prev) {
                        prev = cost[wx - 1][k];
                        from[wx][wy] = @intCast(k);
                    };
                }
                cost[wx][wy] = e + prev;
            };
            var at: u32 = top;
            for (top..ph) |k| if (cost[pw - 1][k] < cost[pw - 1][at]) {
                at = @intCast(k);
            };
            var wx = pw;
            while (wx > 0) {
                wx -= 1;
                cut_row[wx] = at;
                at = from[wx][at];
            }
        }
        for (0..ph) |wy| for (0..pw) |wx| {
            const i = (py + wy) * w + px + wx;
            const own = mine.contains(i);
            if (!pending[i] and !own) continue;
            if (own and ((left > 0 and wx < cut_col[wy]) or (below > 0 and wy >= cut_row[wx]))) continue;
            const ei = (chosen.y + wy) * w + chosen.x + wx;
            tex.image.pixels[i] = exemplar(tex, w, chosen.x + wx, chosen.y + wy, gain);
            tex.jacobian[i] = tex.jacobian[ei];
            tex.paint[i] = .guessed;
            pending[i] = false;
            try mine.put(i, {});
        };
    };
}

/// A damage state's fallen roof (`Model.fallen`) where its picture does not
/// show it well: synthesised (`quilt`) from the rubble it does show well, the
/// largest such rectangle of its own chart where that is a good tile, else
/// of any fallen chart; without any it keeps the intact roof's paint.
fn fillFallen(gpa: std.mem.Allocator, m: Model, tex: Texture) !void {
    if (m.fallen.len == 0) return;
    const fresh = try gpa.alloc(bool, tex.paint.len);
    for (fresh, tex.source) |*f, s| f.* = s > 0;
    const good = try goodTexels(gpa, m, tex, fresh);
    const rects = try gpa.alloc(?Rect, m.charts.len);
    var any: ?usize = null;
    for (m.charts, m.fallen, rects, 0..) |c, f, *r, ci| {
        r.* = if (f) try maskRect(gpa, m, good, c) else null;
        if (r.*) |q| if (q[2] >= own_tile and q[3] >= own_tile and (any == null or rectArea(r.*) > rectArea(rects[any.?]))) {
            any = ci;
        };
    }
    const pending = try gpa.alloc(bool, tex.paint.len);
    for (pending, fresh, m.texel_tri) |*p, f, t| p.* = !f and t != Model.none;
    for (m.charts, m.fallen, rects, 0..) |c, f, own, ci| {
        if (!f) continue;
        const e = if (own) |q| (if (q[2] >= own_tile and q[3] >= own_tile) ci else any) else any;
        const from = e orelse continue;
        try quilt(gpa, m, tex, c, pending, rects[from].?, m.charts[from], .{ 1, 1, 1 }, null, ci, false);
    }
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
                tex.jacobian[i] = tex.jacobian[ty * m.width + tx];
            } else if (mean[3] > 0) {
                tex.image.pixels[i] = .{ @intFromFloat(mean[0] / mean[3]), @intFromFloat(mean[1] / mean[3]), @intFromFloat(mean[2] / mean[3]), 255 };
            } else {
                tex.image.pixels[i] = .{ 128, 128, 128, 255 };
            }
        };
    }
}

/// A damage state's fallen roof (`Model.fallen`) where its picture does not
/// see it: the rubble its picture does see, as a tile of the largest
/// rectangle of seen texels of its own chart where that is a good tile,
/// else of any fallen chart, repeated from the chart's lower left corner and
/// marked guessed. Without any, it keeps the intact roof's paint.
fn tileFallen(gpa: std.mem.Allocator, m: Model, tex: Texture) !void {
    if (m.fallen.len == 0) return;
    // Seen: painted by this bake (the base's own marks say nothing of the rubble).
    const fresh = try gpa.alloc(Paint, tex.paint.len);
    defer gpa.free(fresh);
    for (fresh, tex.source) |*f, s| f.* = if (s > 0) .seen else .none;
    const seen: Texture = .{ .state = tex.state, .image = tex.image, .paint = fresh, .source = tex.source, .jacobian = tex.jacobian };
    var any: ?Rect = null;
    const rects = try gpa.alloc(?Rect, m.charts.len);
    for (m.charts, m.fallen, rects) |c, f, *r| {
        r.* = if (f) try seenRect(gpa, m, seen, c) else null;
        if (r.*) |q| if (any == null or @as(u64, q[2]) * q[3] > @as(u64, any.?[2]) * any.?[3]) {
            any = q;
        };
    }
    for (m.charts, m.fallen, rects) |c, f, own| {
        if (!f) continue;
        const tile = if (own) |q| (if (q[2] >= own_tile and q[3] >= own_tile) q else any) else any;
        const t = tile orelse continue;
        for (0..c.h) |ry| for (0..c.w) |rx| {
            const i = (c.y + ry) * m.width + c.x + rx;
            if (tex.source[i] > 0 or m.texel_tri[i] == Model.none) continue;
            const from_bottom = c.h - 1 - ry;
            const tx = t[0] + @as(u32, @intCast(rx % t[2]));
            const ty = t[1] + t[3] - 1 - @as(u32, @intCast(from_bottom % t[3]));
            tex.image.pixels[i] = tex.image.pixels[ty * m.width + tx];
            tex.jacobian[i] = tex.jacobian[ty * m.width + tx];
            tex.paint[i] = .guessed;
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
/// that of the sample at its middle. `guessed`: the pixels most of whose samples show guessed
/// paint; `unpainted`: the samples on a texel no paint reached (holes);
/// `stretched`: the drawn pixels whose texel's 2003 pixel this render draws
/// over more than `stretch_limit` pixels (`texelStretch` at the pixel's
/// middle sample), a streak; `paint`: where that texel's paint came from.
pub const Rendered = struct { image: Image, guessed: []bool, stretched: []bool, paint: []Paint, unpainted: usize = 0 };

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
    // Each face's texels as this render draws them, in pixels.
    const drawn = try gpa.alloc(?Mat2, m.triangles.len);
    defer gpa.free(drawn);
    for (drawn, m.triangles) |*d, t| {
        var uv: [3][2]f64 = undefined;
        var q: [3][2]f64 = undefined;
        for (t.v, 0..) |v, i| {
            uv[i] = m.vertices[v].uv;
            q[i] = .{ points[v][0] / ss, points[v][1] / ss };
        }
        d.* = uvMap(uv, q);
    }
    const count = @as(usize, size[0]) * size[1];
    var out: Rendered = .{ .image = .{ .width = size[0], .height = size[1], .pixels = try gpa.alloc([4]u8, count) }, .guessed = try gpa.alloc(bool, count), .stretched = try gpa.alloc(bool, count), .paint = try gpa.alloc(Paint, count) };
    for (0..size[1]) |py| for (0..size[0]) |px| {
        var sum = [3]f64{ 0, 0, 0 };
        var covered: u32 = 0;
        var guessed: u32 = 0;
        var centre = [4]f64{ 0, 0, 0, 0 };
        var stretch: f64 = 0;
        var any_stretch: ?f64 = null;
        var kind: Paint = .none;
        var any_kind: ?Paint = null;
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
            const here = texelStretch(drawn[ti], tex.jacobian[i]);
            if (any_stretch == null) any_stretch = here;
            if (any_kind == null) any_kind = tex.paint[i];
            if (sx == ss / 2 and sy == ss / 2) {
                kind = tex.paint[i];
                centre = .{ @floatFromInt(lit[0]), @floatFromInt(lit[1]), @floatFromInt(lit[2]), 1 };
                stretch = here;
            }
            guessed += @intFromBool(tex.paint[i] == .guessed);
            out.unpainted += @intFromBool(tex.paint[i] == .none);
        };
        const o = py * size[0] + px;
        out.guessed[o] = covered * 2 >= ss * ss and guessed * 2 > covered;
        // The colour of the sample at the pixel's middle (the mean of the
        // covered samples where no face meets it): a 2003 pixel spans about two
        // texels, and the mean of the whole pixel blurred a turned picture
        // against the crisp 2003 sides (the quality gate's sharpness).
        if (centre[3] == 0) {
            centre = .{ sum[0], sum[1], sum[2], @floatFromInt(covered) };
            stretch = any_stretch orelse 0;
            kind = any_kind orelse .none;
        }
        out.paint[o] = if (covered * 2 >= ss * ss) kind else .none;
        out.stretched[o] = covered * 2 >= ss * ss and stretch > stretch_limit;
        out.image.pixels[o] = if (covered * 2 >= ss * ss) .{
            @intFromFloat(@round(centre[0] / centre[3])),
            @intFromFloat(@round(centre[1] / centre[3])),
            @intFromFloat(@round(centre[2] / centre[3])),
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

    // The nodes (SSkeletonFormat chunk 1: a vector of structures, one chunk 1 each).
    var nodes: std.ArrayList(u8) = .empty;
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
        try chunk(gpa, &nodes, 1, node.items);
    }
    var skeleton: std.ArrayList(u8) = .empty;
    try chunk(gpa, &skeleton, 1, nodes.items);
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
    // No fatality or death boxes: empty vectors, their count alone.
    try dataVectorChunk(gpa, &data, 5, [6]f32, &.{});
    try dataVectorChunk(gpa, &data, 6, [6]f32, &.{});
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
