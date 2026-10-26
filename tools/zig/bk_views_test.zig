//! Tests for `bk-views`; the command-line contract is in
//! `bk_tool_conventions_test.zig`, and the proof that the engine reads the
//! packs the same way is `bk-views-roundtrip` (tools/zig/bk_views_check.cpp).
//! The buildings are tracked data, read from Data/ in place; nothing is
//! written there.

const std = @import("std");
const views = @import("bk_views.zig");
const bk_tools = @import("bk_tools");
const bk_views_gate = @import("bk_views_gate");

const house = "Data/Buildings/europe/summer/e_ctownhouse01_1/";

fn read(gpa: std.mem.Allocator, path: []const u8) ![]u8 {
    return std.Io.Dir.cwd().readFileAlloc(std.testing.io, path, gpa, .limited(64 * 1024 * 1024));
}

test "a chunk length is one byte below 128 bytes and four above" {
    var bytes: [2 + 3 + 5 + 200]u8 = undefined;
    bytes[0..5].* = .{ 7, 3 << 1, 'a', 'b', 'c' };
    bytes[5] = 9;
    std.mem.writeInt(u32, bytes[6..10], (200 << 1) | 1, .little);
    @memset(bytes[10..], 'x');
    try std.testing.expectEqualStrings("abc", views.findChunk(bytes[0..5], 7, 0).?);
    try std.testing.expectEqual(@as(usize, 200), views.findChunk(bytes[0 .. 5 + 5 + 200], 9, 0).?.len);
    try std.testing.expect(views.findChunk(&bytes, 7, 1) == null);
    // Cut short: the chunk is not there rather than read past the end.
    try std.testing.expect(views.findChunk(bytes[0..4], 7, 0) == null);
}

test "a building pack decodes to its squares, edge and zero cross" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const pack = try views.decodePack(arena, try read(arena, house ++ "1.san"));
    try std.testing.expectEqual(views.pack_signature, pack.signature);
    try std.testing.expectEqual(@as(usize, 1), pack.sprites.len);
    const sprite = pack.sprites[0];
    try std.testing.expectEqual(@as(usize, 65), sprite.squares.len);
    // Squares lie on a grid of their own size around the zero cross; their
    // atlas places start half a pixel in, (0.5 + x) / size.
    for (sprite.squares) |square| {
        try std.testing.expectEqual(@as(f32, 0), @mod(square.x, square.size));
        try std.testing.expectEqual(@as(f32, 0), @mod(square.y, square.size));
        try std.testing.expectEqual(@as(f32, 0.5), @mod(square.u1 * 256, 1));
        try std.testing.expectEqual(@as(f32, 0.5), @mod(square.v1 * 256, 1));
    }
    var lines: usize = 0;
    for (sprite.line_sizes) |size| lines += size;
    try std.testing.expectEqual(@as(usize, @intCast(sprite.edge_size)), lines);
    try std.testing.expect(sprite.bound_box[0] < 0 and sprite.bound_box[2] > 0);
}

test "a wrong signature and a truncated file are refused" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const san = try read(arena, house ++ "1.san");
    try std.testing.expectError(error.NoDataChunk, views.decodePack(arena, san[0..100]));
    const signature = std.mem.indexOf(u8, san, &.{ 127, 4 << 1 }).?;
    san[signature + 2] ^= 0xff;
    try std.testing.expectError(error.NotASpritePack, views.decodePack(arena, san));
}

test "the uncompressed atlas reads as RGBA and other DDS are refused" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const atlas = try views.readDds(arena, try read(arena, house ++ "1_h.dds"));
    try std.testing.expectEqual(@as(u32, 256), atlas.width);
    try std.testing.expectEqual(@as(u32, 256), atlas.height);
    try std.testing.expectError(error.NotArgb8888, views.readDds(arena, try read(arena, house ++ "1_c.dds")));
}

test "a PNG reads back to the same pixels" {
    const gpa = std.testing.allocator;
    var pixels: [3 * 70000][4]u8 = undefined;
    for (&pixels, 0..) |*pixel, i| pixel.* = .{ @truncate(i), @truncate(i >> 8), @truncate(i >> 16), @truncate(i * 7) };
    // More than one stored block.
    const image: views.Image = .{ .width = 3, .height = 70000, .pixels = &pixels };
    const png = try views.writePng(gpa, image);
    defer gpa.free(png);
    const back = try views.readStoredPng(gpa, png);
    defer gpa.free(back.pixels);
    try std.testing.expectEqual(image.width, back.width);
    try std.testing.expectEqualSlices([4]u8, image.pixels, back.pixels);
}

test "1.xml gives the passability, the origin and the point pairs" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const stats = try views.readStats(arena, try read(arena, house ++ "1.xml"));
    try std.testing.expectEqual(@as(usize, 8), stats.passability.len);
    for (stats.passability) |row| try std.testing.expectEqual(@as(usize, 14), row.len);
    try std.testing.expectEqualStrings("00000001010101", stats.passability[7]);
    try std.testing.expectApproxEqAbs(@as(f32, 157.826), stats.origin[0], 1e-3);
    try std.testing.expect(stats.pairs.len > 50);
    // The first fire slot: picture (-97.3996, -69.4), world (-135.34, -2.40363, 25.8651).
    try std.testing.expectApproxEqAbs(@as(f32, -97.3996), stats.pairs[0][0], 1e-4);
    try std.testing.expectApproxEqAbs(@as(f32, 25.8651), stats.pairs[0][4], 1e-4);
}

test "the grid origin lands on screen as the MFC export put it" {
    // e_ctownhouse01_1: origin (157.826, 1.13095).
    const locked = views.lockedCenter(.{ 157.826, 1.13095 });
    try std.testing.expectEqual([2]i32{ -112, -55 }, locked);
}

test "the camera fit gives back the camera that made the pairs" {
    var pairs: [12][5]f32 = undefined;
    for (&pairs, 0..) |*pair, i| {
        const fi: f64 = @floatFromInt(i);
        const p = [3]f64{ 13 * fi - 40, 7 * fi * fi - 30, 5 * @mod(fi, 4) };
        const q = views.mfc_camera.project(p);
        pair.* = .{ @floatCast(q[0]), @floatCast(q[1]), @floatCast(p[0]), @floatCast(p[1]), @floatCast(p[2]) };
    }
    const angles = (try views.fitCamera(&pairs)).angles();
    try std.testing.expectApproxEqAbs(@as(f64, 1), angles[0], 1e-5);
    try std.testing.expectApproxEqAbs(@as(f64, 45), angles[1], 1e-3);
    try std.testing.expectApproxEqAbs(@as(f64, -120), angles[2], 1e-3);
    try std.testing.expectError(error.TooFewPairs, views.fitCamera(pairs[0..2]));
}

/// The eight families with three sides, the spec's camera set.
const camera_variants = [_][]const u8{
    "africa/summer/a_h07_1",          "africa/summer/a_h07_2",          "africa/summer/a_h07_3",
    "europe/summer/e_ctownhouse01_1", "europe/summer/e_ctownhouse01_2", "europe/summer/e_ctownhouse01_4",
    "europe/summer/e_house01_1",      "europe/summer/e_house01_2",      "europe/summer/e_house01_3",
    "europe/summer/e_house02_1",      "europe/summer/e_house02_2",      "europe/summer/e_house02_4",
    "europe/summer/e_house03_2",      "europe/summer/e_house03_3",      "europe/summer/e_house03_4",
    "europe/summer/e_house11_1",      "europe/summer/e_house11_3",      "europe/summer/e_house11_4",
    "europe/summer/e_shed02_2",       "europe/summer/e_shed02_3",       "europe/summer/e_shed02_4",
    "europe/summer/e_shed02_5",       "ussr/summer/townhouse05_1",      "ussr/summer/townhouse05_2",
    "ussr/summer/townhouse05_3",
};

const CameraFile = struct {
    pairs: u32,
    rms_px: f64,
    fitted: struct { px_per_world_unit: f64, yaw: f64, pitch: f64, rows: [2][3]f64 },
    mfc_editor_camera: struct { px_per_world_unit: f64, yaw: f64, pitch: f64 },
    px_per_ai_tile: [2]f64,
    box_fit: []const struct { variant: []const u8, height: f64, iou: f64 },
    min_iou: f64,
};

test "the camera of the three-side families stays the recorded one" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = std.testing.io;
    const out = "zig-out/local-test/bk-views/camera.json";
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(arena, &.{ bk_tools.bk_views, "camera" });
    for (camera_variants) |variant| try argv.append(arena, try std.fmt.allocPrint(arena, "Data/Buildings/{s}", .{variant}));
    try argv.appendSlice(arena, &.{ "--out", out });
    const result = try std.process.run(arena, io, .{ .argv = argv.items });
    if (result.term != .exited or result.term.exited != 0) {
        std.debug.print("bk-views camera failed: {s}\n", .{result.stderr});
        return error.CameraFailed;
    }
    const options: std.json.ParseOptions = .{ .ignore_unknown_fields = true };
    const now = try std.json.parseFromSliceLeaky(CameraFile, arena, try read(arena, out), options);
    const recorded = try std.json.parseFromSliceLeaky(CameraFile, arena, try read(arena, "tools/zig/fixtures/bk-views/camera-2003.json"), options);
    try std.testing.expectEqual(recorded.pairs, now.pairs);
    try std.testing.expectApproxEqAbs(recorded.fitted.px_per_world_unit, now.fitted.px_per_world_unit, 1e-4);
    try std.testing.expectApproxEqAbs(recorded.fitted.yaw, now.fitted.yaw, 1e-2);
    try std.testing.expectApproxEqAbs(recorded.fitted.pitch, now.fitted.pitch, 1e-2);
    try std.testing.expect(now.rms_px <= recorded.rms_px + 1e-3);
    // The fitted camera is MFC's: the claim this slice makes.
    try std.testing.expectApproxEqAbs(now.mfc_editor_camera.px_per_world_unit, now.fitted.px_per_world_unit, 5e-3);
    try std.testing.expectApproxEqAbs(now.mfc_editor_camera.yaw, now.fitted.yaw, 0.1);
    try std.testing.expectApproxEqAbs(now.mfc_editor_camera.pitch, now.fitted.pitch, 0.1);
    try std.testing.expectEqual(recorded.box_fit.len, now.box_fit.len);
    for (recorded.box_fit, now.box_fit) |was, is| {
        try std.testing.expectEqualStrings(was.variant, is.variant);
        if (is.iou < was.iou - 0.01) {
            std.debug.print("{s}: box IoU {d:.4}, recorded {d:.4}\n", .{ is.variant, is.iou, was.iou });
            return error.IouDropped;
        }
    }
}

test "a variant folder's name gives its family, side and colourway" {
    const cases = [_]struct { []const u8, views.Name }{
        .{ "e_ctownhouse01_4", .{ .family = "e_ctownhouse01", .suffix = 4 } },
        .{ "e_ctownhouse02_1_2", .{ .family = "e_ctownhouse02", .suffix = 1, .colourway = 2 } },
        .{ "a_mosque_01_4", .{ .family = "a_mosque_01", .suffix = 4 } },
        .{ "e_shed02_5", .{ .family = "e_shed02", .suffix = 5 } },
        .{ "factoryfloor01", .{ .family = "factoryfloor01" } },
        .{ "barracks", .{ .family = "barracks" } },
    };
    for (cases) |case| {
        const name = views.parseName(case[0]);
        try std.testing.expectEqualStrings(case[1].family, name.family);
        try std.testing.expectEqual(case[1].suffix, name.suffix);
        try std.testing.expectEqual(case[1].colourway, name.colourway);
    }
}

test "a turned grid keeps every cell under the same part of the building" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const stats = try views.readStats(arena, try read(arena, house ++ "1.xml"));
    const grid: views.Grid = .{ .rows = stats.passability, .origin = .{ stats.origin[0], stats.origin[1] } };
    for (1..4) |k| {
        const turned = try views.turnGrid(arena, grid, @intCast(k));
        for (grid.rows, 0..) |row, y| for (0..row.len / 2) |x| {
            const centre = [2]f64{
                (@as(f64, @floatFromInt(x)) + 0.5) * views.tile_size - grid.origin[0],
                (@as(f64, @floatFromInt(y)) + 0.5) * views.tile_size - grid.origin[1],
            };
            const p = views.turnPoint(centre, @intCast(k));
            const tx: usize = @intFromFloat(@floor((p[0] + turned.origin[0]) / views.tile_size));
            const ty: usize = @intFromFloat(@floor((p[1] + turned.origin[1]) / views.tile_size));
            try std.testing.expectEqualStrings(row[2 * x ..][0..2], turned.rows[ty][2 * tx ..][0..2]);
        };
    }
    const back = try views.turnGrid(arena, try views.turnGrid(arena, grid, 3), 1);
    try std.testing.expectEqual(grid.rows.len, back.rows.len);
    for (grid.rows, back.rows) |a, b| try std.testing.expectEqualStrings(a, b);
    try std.testing.expectApproxEqAbs(grid.origin[0], back.origin[0], 1e-9);
    try std.testing.expectApproxEqAbs(grid.origin[1], back.origin[1], 1e-9);
}

test "a turned 1.xml keeps its points on the 2003 camera and four turns give it back" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const xml = try read(arena, house ++ "1.xml");
    const was = try views.readStats(arena, xml);
    const once = try views.turnXml(arena, xml, 1);
    const turned = try views.readStats(arena, once);
    const expected = try views.turnGrid(arena, .{ .rows = was.passability, .origin = .{ was.origin[0], was.origin[1] } }, 1);
    for (expected.rows, turned.passability) |a, b| try std.testing.expectEqualStrings(a, b);
    try std.testing.expectApproxEqAbs(expected.origin[0], turned.origin[0], 1e-3);
    try std.testing.expectEqual(was.pairs.len, turned.pairs.len);
    for (turned.pairs) |pair| {
        const q = views.mfc_camera.project(.{ pair[2], pair[3], pair[4] });
        try std.testing.expectApproxEqAbs(q[0], pair[0], 1e-3);
        try std.testing.expectApproxEqAbs(q[1], pair[1], 1e-3);
    }
    // Every Direction grows by 90.
    var pos: usize = 0;
    var pos_once: usize = 0;
    while (std.mem.indexOfPos(u8, xml, pos, "Direction=\"")) |at| {
        const at_once = std.mem.indexOfPos(u8, once, pos_once, "Direction=\"").?;
        pos = at + 11;
        pos_once = at_once + 11;
        const d = try std.fmt.parseFloat(f64, xml[pos..std.mem.indexOfScalarPos(u8, xml, pos, '"').?]);
        const e = try std.fmt.parseFloat(f64, once[pos_once..std.mem.indexOfScalarPos(u8, once, pos_once, '"').?]);
        try std.testing.expectApproxEqAbs(@mod(d + 90, 360), e, 1e-3);
    }
    var again = once;
    for (0..3) |_| again = try views.turnXml(arena, again, 1);
    const back = try views.readStats(arena, again);
    for (was.passability, back.passability) |a, b| try std.testing.expectEqualStrings(a, b);
    try std.testing.expectApproxEqAbs(was.origin[0], back.origin[0], 1e-3);
    try std.testing.expectApproxEqAbs(was.origin[1], back.origin[1], 1e-3);
    // The 2003 pictures are the camera's to within its fit (rms 0.0002 px).
    for (was.pairs, back.pairs) |a, b| for (a, b) |x, y| try std.testing.expectApproxEqAbs(x, y, 2e-3);
}

test "the three-side families measure as suffix k being k - 1 quarter turns, with the recorded exceptions" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = std.testing.io;
    const expected = [_]struct { []const u8, []const ?u8 }{
        // Every side where the suffix says.
        .{ "a_h07", &.{ 1, 2, 3 } },
        .{ "e_ctownhouse01", &.{ 1, 2, 4 } },
        .{ "e_house11", &.{ 1, 3, 4 } },
        // _3 repeats _1's view with another roof; _2 is a half turn, not one.
        .{ "e_house01", &.{ 1, 3, 1 } },
        // _2 repeats _1's view; _4 holds.
        .{ "e_house02", &.{ 1, 1, 4 } },
        // _3 and _4 are one view, a half turn from _2.
        .{ "e_house03", &.{ 2, 4, 4 } },
        // _3 cannot be told from its mirror turn (an open shed); _5 repeats _2.
        .{ "e_shed02", &.{ 2, null, 4, 2 } },
        // _3 repeats _2's view with a green roof.
        .{ "townhouse05", &.{ 1, 2, 2 } },
    };
    const families = try views.listFamilies(arena, io, "Data/Buildings");
    for (expected) |want| {
        const family = for (families) |f| {
            if (std.mem.eql(u8, f.name, want[0])) break f;
        } else return error.NoSuchFamily;
        try views.placeFamily(arena, io, "Data/Buildings", family);
        try std.testing.expectEqual(want[1].len, family.variants.len);
        for (family.variants, want[1]) |variant, side| {
            if (variant.side != side) {
                std.debug.print("{s}: side {?d}, expected {?d} ({s})\n", .{ variant.path, variant.side, side, variant.how });
                return error.SideChanged;
            }
        }
    }
}

test "the tracked inventory is what bk-views inventory writes" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = std.testing.io;
    const out = "zig-out/local-test/bk-views/inventory";
    const result = try std.process.run(arena, io, .{ .argv = &.{ bk_tools.bk_views, "inventory", "Data/Buildings", out } });
    if (result.term != .exited or result.term.exited != 0) {
        std.debug.print("bk-views inventory failed: {s}\n", .{result.stderr});
        return error.InventoryFailed;
    }
    for ([_][2][]const u8{
        .{ out ++ "/inventory.md", "docs/superpowers/evidence/building-views/inventory.md" },
        .{ out ++ "/inventory.json", "tools/zig/fixtures/bk-views/inventory.json" },
    }) |pair| {
        const written = try withoutCr(arena, try read(arena, pair[0]));
        const tracked = try withoutCr(arena, try read(arena, pair[1]));
        if (!std.mem.eql(u8, written, tracked)) {
            std.debug.print("{s} differs from {s}; regenerate it with bk-views inventory\n", .{ pair[1], pair[0] });
            return error.InventoryChanged;
        }
    }
}

test "an eye's ray runs through the picture point it starts at, and meets the ground under it" {
    const eye: views.Eye = .{ .anchor = .{ 120, 80 }, .turn = 3, .shift = .{ 5, -7 } };
    for ([_][2]f64{ .{ 0, 0 }, .{ 33.5, 12.25 }, .{ 200, 140 } }) |pixel| {
        const r = eye.ray(pixel[0], pixel[1]);
        for ([_]f64{ -50, 0, 80 }) |t| {
            const q = eye.project(.{ r[0][0] + t * r[1][0], r[0][1] + t * r[1][1], r[0][2] + t * r[1][2] });
            try std.testing.expectApproxEqAbs(pixel[0], q[0], 1e-9);
            try std.testing.expectApproxEqAbs(pixel[1], q[1], 1e-9);
        }
        const g = views.groundPoint(eye, pixel[0], pixel[1]);
        try std.testing.expectEqual(@as(f64, 0), g[2]);
        const q = eye.project(g);
        try std.testing.expectApproxEqAbs(pixel[0], q[0], 1e-9);
    }
    // The eye looks down, away from the walls it sees (-y and +x in its own frame).
    const d = (views.Eye{ .anchor = .{ 0, 0 } }).ray(0, 0)[1];
    try std.testing.expect(d[2] < 0 and d[0] < 0 and d[1] > 0);
}

/// The picture a box makes for `eye`: opaque where a pixel's ray meets it.
fn boxPicture(gpa: std.mem.Allocator, eye: views.Eye, lo: [3]f64, hi: [3]f64) !views.Image {
    const image: views.Image = .{ .width = 240, .height = 200, .pixels = try gpa.alloc([4]u8, 240 * 200) };
    for (0..image.height) |y| for (0..image.width) |x| {
        const r = eye.ray(@as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
        var near = -std.math.inf(f64);
        var far = std.math.inf(f64);
        for (0..3) |k| {
            const a = (lo[k] - r[0][k]) / r[1][k];
            const b = (hi[k] - r[0][k]) / r[1][k];
            near = @max(near, @min(a, b));
            far = @min(far, @max(a, b));
        }
        image.pixels[y * image.width + x] = if (near <= far) .{ 200, 150, 100, 255 } else .{ 0, 0, 0, 0 };
    };
    return image;
}

test "the hull of a box seen from three sides is the box, and alignment finds a picture's shift" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A footprint of 3 x 2 cells at the zero cross and a box of 30 world units over it.
    const footprint: views.Grid = .{ .rows = &.{ "010101", "010101" }, .origin = .{ 0, 0 } };
    const hi = [3]f64{ 3 * views.tile_size, 2 * views.tile_size, 30 };
    const true_shift = [2]f64{ 6, -4 };
    var eyes: [3]views.Eye = undefined;
    var pictures: [3]views.Image = undefined;
    var footprints: [3]views.Grid = undefined;
    for (&eyes, &pictures, &footprints, [_]u2{ 0, 1, 2 }) |*eye, *picture, *grid, turn| {
        eye.* = .{ .anchor = .{ 120, 120 }, .turn = turn, .shift = if (turn == 1) true_shift else .{ 0, 0 } };
        picture.* = try boxPicture(arena, eye.*, .{ 0, 0, 0 }, hi);
        // Each picture's own grid, as its 1.xml would hold it: the turned footprint, the shift unknown.
        grid.* = try views.turnGrid(arena, footprint, turn);
        eye.shift = .{ 0, 0 };
    }
    try views.alignEyes(arena, 0, &footprints, &pictures, &eyes);
    try std.testing.expectApproxEqAbs(true_shift[0], eyes[1].shift[0], 1);
    try std.testing.expectApproxEqAbs(true_shift[1], eyes[1].shift[1], 1);
    try std.testing.expectApproxEqAbs(@as(f64, 0), eyes[2].shift[0], 1);

    // The hull holds the box (but for the unit the alignment may miss), and
    // its ground slice is the box's base: every wall faces some eye, whose
    // outline ends at the wall's foot. Above the flat top it holds a low hip
    // cap: seen from 30 degrees up, the two look alike.
    const hull = try views.carve(arena, footprint, 2, &pictures, &eyes);
    var base: f64 = 0;
    for (0..hull.n[1]) |j| for (0..hull.n[0]) |i| {
        base += @floatFromInt(@intFromBool(hull.get(@intCast(i), @intCast(j), 0)));
    };
    // To within about a voxel (the grid does not start at the box).
    try std.testing.expect(base * 4 >= hi[0] * hi[1] and base * 4 <= (hi[0] + 6) * (hi[1] + 6));
    for (0..hull.n[2]) |l| for (0..hull.n[1]) |j| for (0..hull.n[0]) |i| {
        const c = hull.center(i, j, l);
        const inside = c[0] > 3 and c[0] < hi[0] - 3 and c[1] > 3 and c[1] < hi[1] - 3 and c[2] < hi[2] - 3;
        if (inside) try std.testing.expect(hull.get(@intCast(i), @intCast(j), @intCast(l)));
    };
    // Its ground slice within the footprint gives one block, which the fit
    // to the outlines turns into the box, to within what the alignment
    // misses (the half turn's eye sits 4.5 units off along y): a top flat to
    // within 5 units at the box's height.
    const rectangles = try views.baseBlocks(arena, hull, footprint);
    try std.testing.expectEqual(@as(usize, 1), rectangles.len);
    const box = (try views.fitBlocks(arena, rectangles, &pictures, &eyes, false))[0];
    for (0..2) |k| {
        try std.testing.expectApproxEqAbs(@as(f64, 0), box.lo[k], 2);
        try std.testing.expectApproxEqAbs(hi[k], box.hi[k], 5);
    }
    try std.testing.expectApproxEqAbs(hi[2], box.ridge, 2);
    try std.testing.expect(box.ridge - box.wall <= 5);
}

test "a post too thin for a rectangle of the hull stands as one block over its footprint" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // One cell (a barrel, a watchtower on legs) and a post 3 units wide and 40 high in it.
    const footprint: views.Grid = .{ .rows = &.{"01"}, .origin = .{ 5, 7 } };
    const eye: views.Eye = .{ .anchor = .{ 120, 120 } };
    const picture = try boxPicture(arena, eye, .{ 0, 0, 0 }, .{ 3, 3, 40 });
    const hull = try views.carve(arena, footprint, 2, &.{picture}, &.{eye});
    try std.testing.expect(hull.count() > 0);
    const blocks = try views.baseBlocks(arena, hull, footprint);
    try std.testing.expectEqual(@as(usize, 1), blocks.len);
    try std.testing.expectEqual([2]f64{ -5, -7 }, blocks[0].lo);
    try std.testing.expectEqual([2]f64{ views.tile_size - 5, views.tile_size - 7 }, blocks[0].hi);
    try std.testing.expect(blocks[0].ridge >= 40);
}

/// The picture of blocks for `eye`: opaque where a pixel's ray meets them.
fn blocksPicture(gpa: std.mem.Allocator, eye: views.Eye, blocks: []const views.Block) !views.Image {
    const image: views.Image = .{ .width = 260, .height = 240, .pixels = try gpa.alloc([4]u8, 260 * 240) };
    for (0..image.height) |y| for (0..image.width) |x| {
        const hit = views.castPixel(blocks, eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
        image.pixels[y * image.width + x] = if (hit != null) .{ 200, 150, 100, 255 } else .{ 0, 0, 0, 0 };
    };
    return image;
}

test "a gabled house seen from three sides is fitted back from a rough box" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const gabled = [_]views.Block{.{ .lo = .{ 0, 0 }, .hi = .{ 3 * views.tile_size, 2 * views.tile_size }, .wall = 30, .ridge = 55, .roof = .gable_x }};
    var eyes: [3]views.Eye = undefined;
    var pictures: [3]views.Image = undefined;
    for (&eyes, &pictures, [_]u2{ 0, 1, 2 }) |*eye, *picture, turn| {
        eye.* = .{ .anchor = .{ 130, 150 }, .turn = turn };
        picture.* = try blocksPicture(arena, eye.*, &gabled);
    }
    const start = [_]views.Block{.{ .lo = .{ 5, -6 }, .hi = .{ gabled[0].hi[0] - 4, gabled[0].hi[1] + 5 }, .wall = 22, .ridge = 60 }};
    const fitted = (try views.fitBlocks(arena, &start, &pictures, &eyes, false))[0];
    try std.testing.expectEqual(views.Roof.gable_x, fitted.roof);
    for (0..2) |k| {
        try std.testing.expectApproxEqAbs(gabled[0].lo[k], fitted.lo[k], 2);
        try std.testing.expectApproxEqAbs(gabled[0].hi[k], fitted.hi[k], 2);
    }
    try std.testing.expectApproxEqAbs(gabled[0].wall, fitted.wall, 3);
    try std.testing.expectApproxEqAbs(gabled[0].ridge, fitted.ridge, 3);
    for (eyes, pictures) |eye, picture| try std.testing.expect(views.blocksIou(&.{fitted}, eye, picture) > 0.97);
}

test "a block the outlines barely need is dropped, and a chimney is cut back from the outline" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const gabled: views.Block = .{ .lo = .{ 0, 0 }, .hi = .{ 3 * views.tile_size, 2 * views.tile_size }, .wall = 30, .ridge = 55, .roof = .gable_x };
    // A chimney 6 units square on the ridge, 15 above it.
    const chimney: views.Block = .{ .lo = .{ 30, 20 }, .hi = .{ 36, 26 }, .wall = 70, .ridge = 70 };
    var eyes: [3]views.Eye = undefined;
    var plain: [3]views.Image = undefined;
    var pictures: [3]views.Image = undefined;
    for (&eyes, &plain, &pictures, [_]u2{ 0, 1, 3 }) |*eye, *bare, *picture, turn| {
        eye.* = .{ .anchor = .{ 130, 150 }, .turn = turn };
        bare.* = try blocksPicture(arena, eye.*, &.{gabled});
        picture.* = try blocksPicture(arena, eye.*, &.{ gabled, chimney });
    }
    // A tower standing a little above the roof (from one side it is what a
    // chimney looks like) costs more than it adds: the fit drops it.
    const start = [_]views.Block{
        .{ .lo = .{ 5, -6 }, .hi = .{ gabled.hi[0] - 4, gabled.hi[1] + 5 }, .wall = 22, .ridge = 60 },
        .{ .lo = .{ 40, 10 }, .hi = .{ 56, 26 }, .wall = 58, .ridge = 58 },
    };
    try std.testing.expectEqual(@as(usize, 1), (try views.fitBlocks(arena, &start, &plain, &eyes, false)).len);
    // The chimney costs a block too; cut from the first picture's outline it
    // comes back where it stands, as high and about as wide.
    const fitted = try views.fitBlocks(arena, &start, &pictures, &eyes, false);
    try std.testing.expectEqual(@as(usize, 1), fitted.len);
    const cut = try views.cutChimneys(arena, fitted, &pictures, &eyes);
    try std.testing.expectEqual(@as(usize, 2), cut.len);
    const found = cut[1];
    for (0..2) |k| try std.testing.expectApproxEqAbs((chimney.lo[k] + chimney.hi[k]) / 2, (found.lo[k] + found.hi[k]) / 2, 4);
    try std.testing.expectApproxEqAbs(@as(f64, 6), found.hi[0] - found.lo[0], 3);
    try std.testing.expectApproxEqAbs(chimney.ridge, found.ridge, 3);
    // The outline of what is left over in the other pictures shrinks.
    for (eyes[1..], pictures[1..]) |eye, picture| try std.testing.expect(views.blocksIou(cut, eye, picture) > views.blocksIou(fitted, eye, picture));
}

test "an outline's protrusion is the share of it narrower than the window" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A wall 100 x 40 pixels, and the same with a post 8 wide and 30 high on it.
    const flat: views.Image = .{ .width = 120, .height = 80, .pixels = try arena.alloc([4]u8, 120 * 80) };
    const post: views.Image = .{ .width = 120, .height = 80, .pixels = try arena.alloc([4]u8, 120 * 80) };
    for (0..80) |y| for (0..120) |x| {
        const wall = x >= 10 and x < 110 and y >= 40;
        flat.pixels[y * 120 + x] = if (wall) .{ 1, 1, 1, 255 } else .{ 0, 0, 0, 0 };
        post.pixels[y * 120 + x] = if (wall or (x >= 50 and x < 58 and y >= 10)) .{ 1, 1, 1, 255 } else .{ 0, 0, 0, 0 };
    };
    try std.testing.expectApproxEqAbs(@as(f64, 0), try views.protrusion(arena, flat), 1e-9);
    try std.testing.expectApproxEqAbs(@as(f64, 8 * 30) / (100 * 40 + 8 * 30), try views.protrusion(arena, post), 1e-9);
}

test "a ray meets the first face, an eye sees only what nothing hides, and a hidden wall borrows the opposite one" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A low block with a tall one standing in front of it towards -y.
    const blocks = [_]views.Block{
        .{ .lo = .{ 0, 0 }, .hi = .{ 40, 40 }, .wall = 20, .ridge = 20 },
        .{ .lo = .{ 0, -20 }, .hi = .{ 40, 0 }, .wall = 60, .ridge = 60 },
    };
    const down = views.castRay(&blocks, .{ 20, 20, 100 }, .{ 0, 0, -1 }).?;
    try std.testing.expectApproxEqAbs(@as(f64, 80), down.t, 1e-9);
    try std.testing.expectEqual(@as(usize, 0), down.block);
    try std.testing.expectEqual([3]f64{ 0, 0, 1 }, down.normal);
    // Nothing behind the ray's start counts.
    try std.testing.expect(views.castRay(&blocks, .{ 20, 20, 100 }, .{ 0, 0, 1 }) == null);
    // The eye looks along -x and +y (it sees the walls facing -y and +x):
    // the tall block hides the low block's top near it, not far from it.
    const eye: views.Eye = .{ .anchor = .{ 0, 0 } };
    try std.testing.expect(!views.sees(&blocks, eye, .{ 20, 2, 20 }));
    try std.testing.expect(views.sees(&blocks, eye, .{ 38, 38, 20 }));
    // The low block's wall facing +y (plane 2) borrows from its wall facing
    // -y (plane 3) at the same height, across the same way seen from outside.
    const q = views.donorPoint(&blocks, .{ .block = 0, .plane = 2 }, .{ .block = 0, .plane = 3 }, .{ 10, 40, 12 });
    try std.testing.expectEqual([3]f64{ 30, 0, 12 }, q);
    const donors = try views.donorsOf(arena, &blocks, .{ .block = 0, .plane = 2 });
    try std.testing.expectEqual(views.Face{ .block = 0, .plane = 3 }, donors[0]);
}

test "a shading class is the way a surface faces in the eye's own frame" {
    try std.testing.expectEqual(@as(usize, 0), views.shadeClass(.{ 0, -1, 0 }, 0));
    try std.testing.expectEqual(@as(usize, 1), views.shadeClass(.{ 1, 0, 0 }, 0));
    // A quarter turn shows the wall facing -y as the one facing +x.
    try std.testing.expectEqual(@as(usize, 1), views.shadeClass(.{ 0, -1, 0 }, 1));
    try std.testing.expectEqual(@as(usize, 4 + 2), views.shadeClass(.{ 0, 0.5, 0.86 }, 0));
}

test "the patch fill continues the picture's own texture into a hole, the same way every time" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const size = 48;
    const stripe = struct {
        fn colour(x: usize) [4]u8 {
            return if (x % 6 < 3) .{ 180, 140, 90, 255 } else .{ 60, 40, 30, 255 };
        }
    }.colour;
    var results: [2][]const [4]u8 = undefined;
    for (&results) |*result| {
        const generated: views.Generated = .{
            .image = .{ .width = size, .height = size, .pixels = try arena.alloc([4]u8, size * size) },
            .kinds = try arena.alloc(views.Kind, size * size),
            .classes = try arena.alloc(u8, size * size),
        };
        for (0..size) |y| for (0..size) |x| {
            const hole = x >= 16 and x < 32 and y >= 16 and y < 32;
            generated.image.pixels[y * size + x] = if (hole) .{ 0, 0, 0, 0 } else stripe(x);
            generated.kinds[y * size + x] = if (hole) .filled else .projected;
            generated.classes[y * size + x] = 0;
        };
        try views.patchFill(arena, generated);
        result.* = generated.image.pixels;
    }
    try std.testing.expectEqualSlices([4]u8, results[0], results[1]);
    var right: usize = 0;
    for (16..32) |y| for (16..32) |x| {
        right += @intFromBool(std.mem.eql(u8, &results[0][y * size + x], &stripe(x)));
    };
    try std.testing.expect(right >= 16 * 16 * 9 / 10);
}

test "e_ctownhouse01_1 has another colourway than _2 and _4" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var pictures: [3]views.Image = undefined;
    for (&pictures, [_][]const u8{ "1", "2", "4" }) |*picture, suffix| {
        const dir = try std.fmt.allocPrint(arena, "Data/Buildings/europe/summer/e_ctownhouse01_{s}/", .{suffix});
        const pack = try views.decodePack(arena, try read(arena, try std.fmt.allocPrint(arena, "{s}1.san", .{dir})));
        const atlas = try views.readDds(arena, try read(arena, try std.fmt.allocPrint(arena, "{s}1_h.dds", .{dir})));
        var bounds: views.Bounds = .{};
        for (pack.sprites[0].squares) |square| bounds.add(square);
        picture.* = try views.unpack(arena, pack.sprites[0], atlas, bounds);
    }
    try std.testing.expect(views.sameColourway(pictures[1], pictures[2]));
    try std.testing.expect(!views.sameColourway(pictures[1], pictures[0]));
    try std.testing.expect(!views.sameColourway(pictures[2], pictures[0]));
}

/// A picture of a timber-framed wall: plaster with dark beams every 8 pixels
/// and a little texture, opaque inside a margin of 10.
fn wallPicture(gpa: std.mem.Allocator, width: u32, height: u32) !views.Image {
    const image: views.Image = .{ .width = width, .height = height, .pixels = try gpa.alloc([4]u8, width * height) };
    for (0..height) |y| for (0..width) |x| {
        const inside = x >= 10 and y >= 10 and x < width - 10 and y < height - 10;
        const beam = x % 8 < 2 or y % 8 < 2;
        const grain: u8 = @intCast((x * 7 + y * 13) % 23);
        image.pixels[y * width + x] = if (!inside) .{ 0, 0, 0, 0 } else if (beam) .{ 60 + grain, 40, 30, 255 } else .{ 200 + grain, 180, 140, 255 };
    };
    return image;
}

test "the gate scores a picture against itself as perfect, finds a shift, and sees blur and colour" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const original = try wallPicture(arena, 80, 60);
    const same = try views.scorePicture(arena, original, original, .{ 0, 0 });
    try std.testing.expectEqual(@as(f64, 1), same.iou);
    try std.testing.expectApproxEqAbs(@as(f64, 1), same.ssim, 1e-9);
    try std.testing.expectEqual(@as(f64, 1), same.sharpness);
    try std.testing.expectEqual(@as(f64, 0), same.colour);
    try std.testing.expectEqual(@as(f64, 0), same.hue);

    // The same picture moved by (5, -3) on a larger canvas: the best offset
    // brings it back from a guess 9 pixels off.
    const moved: views.Image = .{ .width = 100, .height = 80, .pixels = try arena.alloc([4]u8, 100 * 80) };
    @memset(moved.pixels, .{ 0, 0, 0, 0 });
    for (0..60) |y| for (0..80) |x| {
        moved.pixels[(y + 7) * 100 + x + 15] = original.pixels[y * 80 + x];
    };
    const offset = views.bestOffset(moved, original, .{ 9, 13 });
    try std.testing.expectEqual([2]i64{ 15, 7 }, offset);
    try std.testing.expectApproxEqAbs(@as(f64, 1), (try views.scorePicture(arena, moved, original, offset)).ssim, 1e-9);

    // A box blur keeps the outline and the colour but loses the edges.
    const blurred: views.Image = .{ .width = 80, .height = 60, .pixels = try arena.alloc([4]u8, 80 * 60) };
    for (0..60) |y| for (0..80) |x| {
        var sum = [3]u32{ 0, 0, 0 };
        for (0..9) |k| for (0..3) |c| {
            // Within the opaque part.
            const sx: usize = @min(70, @max(11, x + k % 3)) - 1;
            const sy: usize = @min(50, @max(11, y + k / 3)) - 1;
            sum[c] += original.pixels[sy * 80 + sx][c];
        };
        const p = original.pixels[y * 80 + x];
        blurred.pixels[y * 80 + x] = .{ @intCast(sum[0] / 9), @intCast(sum[1] / 9), @intCast(sum[2] / 9), p[3] };
    };
    const blur = try views.scorePicture(arena, blurred, original, .{ 0, 0 });
    try std.testing.expectEqual(@as(f64, 1), blur.iou);
    try std.testing.expect(blur.sharpness < 0.5);
    // Half a beam out of place, the layout no longer matches.
    try std.testing.expect((try views.scorePicture(arena, original, original, .{ 4, 4 })).ssim < 0.5);
    try std.testing.expect(blur.colour < 3);

    // Green paint instead of plaster: the colour and the hue move.
    const green: views.Image = .{ .width = 80, .height = 60, .pixels = try arena.dupe([4]u8, original.pixels) };
    for (green.pixels) |*p| {
        if (p[0] > 150) p.* = .{ 90, 170, 90, p[3] };
    }
    const paint = try views.scorePicture(arena, green, original, .{ 0, 0 });
    try std.testing.expect(paint.colour > 50 and paint.hue > 0.5);
}

/// The floors of one damage state in gate.json.
const Floors = struct { iou: f64, mean_iou: f64, mean_ssim: f64, sharpness: [2]f64, colour: f64, hue: f64 };

const Gate = struct {
    variants: []const []const u8,
    states: struct { @"1": Floors, @"2": Floors, @"3": Floors },

    /// A state's floors; a winter state (`2w`) counts as its summer one.
    fn floors(gate: Gate, state: []const u8) Floors {
        return switch (state[0]) {
            '1' => gate.states.@"1",
            '2' => gate.states.@"2",
            else => gate.states.@"3",
        };
    }
};

fn within(failures: *usize, what: []const u8, state: []const u8, name: []const u8, value: f64, low: f64, high: f64) void {
    if (value >= low and value <= high) return;
    std.debug.print("{s}, state {s}: {s} {d:.4} outside {d:.4}..{d:.4}\n", .{ what, state, name, value, low, high });
    failures.* += 1;
}

test "the drawn sides stay above the quality gate (tools/zig/fixtures/bk-views/gate.json)" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = std.testing.io;
    const options: std.json.ParseOptions = .{ .ignore_unknown_fields = true };
    const gate = try std.json.parseFromSliceLeaky(Gate, arena, try read(arena, "tools/zig/fixtures/bk-views/gate.json"), options);
    const out = "zig-out/local-test/bk-views/gate";
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(arena, &.{ bk_views_gate.bk_views_fast, "holdout", out ++ "/holdout" });
    for (gate.variants) |variant| try argv.append(arena, try std.fmt.allocPrint(arena, "Data/Buildings/{s}", .{variant}));
    const result = try std.process.run(arena, io, .{ .argv = argv.items, .stdout_limit = .limited(1024 * 1024) });
    if (result.term != .exited or result.term.exited != 0) {
        std.debug.print("bk-views holdout failed: {s}\n", .{result.stderr});
        return error.HoldoutFailed;
    }
    const Row = struct { variant: []const u8, state: []const u8, iou: f64, ssim: f64, sharpness: f64, colour: f64, hue: f64 };
    const rows = (try std.json.parseFromSliceLeaky(struct { holdout: []const Row }, arena, try read(arena, out ++ "/holdout/holdout.json"), options)).holdout;
    var failures: usize = 0;
    var sums: [3][3]f64 = std.mem.zeroes([3][3]f64);
    for (rows) |row| {
        const f = gate.floors(row.state);
        within(&failures, row.variant, row.state, "IoU", row.iou, f.iou, 1);
        within(&failures, row.variant, row.state, "sharpness", row.sharpness, f.sharpness[0], f.sharpness[1]);
        within(&failures, row.variant, row.state, "colour", row.colour, 0, f.colour);
        within(&failures, row.variant, row.state, "hue", row.hue, 0, f.hue);
        const level = row.state[0] - '1';
        sums[level] = .{ sums[level][0] + row.iou, sums[level][1] + row.ssim, sums[level][2] + 1 };
    }
    for (sums, [_][]const u8{ "1", "2", "3" }) |sum, state| {
        const f = gate.floors(state);
        within(&failures, "mean", state, "IoU", sum[0] / sum[2], f.mean_iou, 1);
        within(&failures, "mean", state, "SSIM", sum[1] / sum[2], f.mean_ssim, 1);
    }
    // Every variant was drawn, and its pictures keep the style of the 2003 sides.
    for (gate.variants) |variant| {
        const name = std.fs.path.basename(variant);
        for (rows) |row| {
            if (std.mem.eql(u8, row.variant, name)) break;
        } else {
            std.debug.print("{s}: not drawn\n", .{name});
            failures += 1;
        }
        const style_out = try std.fmt.allocPrint(arena, out ++ "/style/{s}", .{name});
        const style = try std.process.run(arena, io, .{ .argv = &.{
            bk_views_gate.bk_views_fast,                                     "style",                                                       style_out,
            try std.fmt.allocPrint(arena, "Data/Buildings/{s}", .{variant}), try std.fmt.allocPrint(arena, out ++ "/holdout/{s}", .{name}),
        } });
        if (style.term != .exited or style.term.exited != 0) {
            std.debug.print("bk-views style failed: {s}\n", .{style.stderr});
            return error.StyleFailed;
        }
        const StyleRow = struct { generated: []const u8, state: []const u8, sharpness: f64, colour: f64, hue: f64 };
        const styles = (try std.json.parseFromSliceLeaky(struct { style: []const StyleRow }, arena, try read(arena, try std.fmt.allocPrint(arena, "{s}/style.json", .{style_out})), options)).style;
        for (styles) |row| {
            const f = gate.floors(row.state);
            within(&failures, row.generated, row.state, "style sharpness", row.sharpness, f.sharpness[0], f.sharpness[1]);
            within(&failures, row.generated, row.state, "style colour", row.colour, 0, f.colour);
            within(&failures, row.generated, row.state, "style hue", row.hue, 0, f.hue);
        }
    }
    if (failures > 0) return error.BelowTheGate;
}

/// Tracked text is checked out with CRLF; the tool writes LF.
fn withoutCr(gpa: std.mem.Allocator, text: []const u8) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    for (text) |c| if (c != '\r') try out.append(gpa, c);
    return out.items;
}
