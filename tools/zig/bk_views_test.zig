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
    // Deflated (the full 3D models' atlases), and read back by the general reader.
    const deflated = try views.writePngDeflated(gpa, image);
    defer gpa.free(deflated);
    try std.testing.expect(deflated.len < png.len);
    for ([_][]const u8{ png, deflated }) |bytes| {
        const again = try views.readPng(gpa, bytes);
        defer gpa.free(again.pixels);
        try std.testing.expectEqualSlices([4]u8, image.pixels, again.pixels);
    }
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
    const cut = try views.cutChimneys(arena, fitted, &.{}, &pictures, &eyes, false);
    try std.testing.expectEqual(@as(usize, 2), cut.len);
    const found = cut[1];
    for (0..2) |k| try std.testing.expectApproxEqAbs((chimney.lo[k] + chimney.hi[k]) / 2, (found.lo[k] + found.hi[k]) / 2, 4);
    try std.testing.expectApproxEqAbs(@as(f64, 6), found.hi[0] - found.lo[0], 3);
    try std.testing.expectApproxEqAbs(chimney.ridge, found.ridge, 3);
    // The outline of what is left over in the other pictures shrinks.
    for (eyes[1..], pictures[1..]) |eye, picture| try std.testing.expect(views.blocksIou(cut, eye, picture) > views.blocksIou(fitted, eye, picture));
}

test "a ramp below a wall is cut as a low part, and a turn that drops it fails the gate" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const shed: views.Block = .{ .lo = .{ 0, 0 }, .hi = .{ 3 * views.tile_size, 2 * views.tile_size }, .wall = 30, .ridge = 55, .roof = .gable_x };
    // A ramp 30 units wide and 12 deep against the wall the first eye sees on its left.
    const ramp: views.Block = .{ .lo = .{ 20, -12 }, .hi = .{ 50, 0 }, .wall = 5, .ridge = 5 };
    const eyes = [3]views.Eye{ .{ .anchor = .{ 130, 150 } }, .{ .anchor = .{ 130, 150 }, .turn = 1 }, .{ .anchor = .{ 130, 150 }, .turn = 2 } };
    const picture = try blocksPicture(arena, eyes[0], &.{ shed, ramp });
    // Its foot meets the ground where the ramp's does; its height follows its depth.
    const parts = try views.lowParts(arena, &.{shed}, picture, eyes[0]);
    try std.testing.expectEqual(@as(usize, 1), parts.len);
    for (0..2) |k| {
        try std.testing.expectApproxEqAbs(ramp.lo[k], parts[0].lo[k], 3);
        try std.testing.expectApproxEqAbs(ramp.hi[k], parts[0].hi[k], 3);
    }
    try std.testing.expectApproxEqAbs(ramp.ridge, parts[0].ridge, 2);
    // The shed alone shows nothing below its walls.
    try std.testing.expectEqual(@as(usize, 0), (try views.lowParts(arena, &.{shed}, try blocksPicture(arena, eyes[0], &.{shed}), eyes[0])).len);
    // The next side shows the ramp's wall on its right: drawn without the
    // ramp it keeps none of it and fails, drawn with it it keeps all.
    const dropped = views.lowKept(&.{shed}, parts, eyes[1], try blocksPicture(arena, eyes[1], &.{shed}));
    try std.testing.expect(dropped[0] > 100);
    try std.testing.expectEqual(@as(f64, 0), dropped[1]);
    try std.testing.expect(views.lowFails(dropped[0], dropped[1]));
    const kept = views.lowKept(&.{shed}, parts, eyes[1], try blocksPicture(arena, eyes[1], &.{ shed, parts[0] }));
    try std.testing.expectEqual(kept[0], kept[1]);
    try std.testing.expect(!views.lowFails(kept[0], kept[1]));
    // From behind the shed hides it: nothing to keep.
    try std.testing.expectEqual(@as(f64, 0), views.lowKept(&.{shed}, parts, eyes[2], try blocksPicture(arena, eyes[2], &.{shed}))[0]);
}

/// A picture of blocks as drawn, with each pixel's shading class.
fn blocksDrawn(gpa: std.mem.Allocator, eye: views.Eye, blocks: []const views.Block) !views.Generated {
    const image = try blocksPicture(gpa, eye, blocks);
    const drawn: views.Generated = .{ .image = image, .kinds = try gpa.alloc(views.Kind, image.pixels.len), .classes = try gpa.alloc(u8, image.pixels.len) };
    for (0..image.height) |y| for (0..image.width) |x| {
        const hit = views.castPixel(blocks, eye, @as(f64, @floatFromInt(x)) + 0.5, @as(f64, @floatFromInt(y)) + 0.5);
        drawn.kinds[y * image.width + x] = if (hit != null) .projected else .empty;
        drawn.classes[y * image.width + x] = if (hit) |h| @intCast(views.shadeClass(h.normal, eye.turn)) else 255;
    };
    return drawn;
}

test "a roof part turns into the frame of another side, and a turn that paints it on the roof fails the gate" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const shed: views.Block = .{ .lo = .{ 0, 0 }, .hi = .{ 3 * views.tile_size, 2 * views.tile_size }, .wall = 30, .ridge = 55, .roof = .gable_x };
    // A ridge vent 20 units long standing 4 above the ridge.
    const vent: views.Block = .{ .lo = .{ 20, 12 }, .hi = .{ 40, 33 }, .wall = 59, .ridge = 59 };
    // Seen by a picture one turn on whose zero cross sits elsewhere, the
    // vent is given in that picture's frame and comes back where it stands.
    const eye: views.Eye = .{ .anchor = .{ 130, 150 }, .turn = 1, .shift = .{ 7, -3 } };
    const a = views.turnPoint(vent.lo, 1);
    const b = views.turnPoint(vent.hi, 1);
    const given: views.Block = .{ .lo = .{ @min(a[0], b[0]) + 7, @min(a[1], b[1]) - 3 }, .hi = .{ @max(a[0], b[0]) + 7, @max(a[1], b[1]) - 3 }, .wall = 59, .ridge = 59, .roof = .gable_y };
    const back = views.intoFrame(given, eye);
    for (0..2) |k| {
        try std.testing.expectApproxEqAbs(vent.lo[k], back.lo[k], 1e-9);
        try std.testing.expectApproxEqAbs(vent.hi[k], back.hi[k], 1e-9);
    }
    try std.testing.expectEqual(views.Roof.gable_x, back.roof);
    // Every side drawn with the vent keeps its walls; drawn without it,
    // with the vent's pixels on the roof planes, keeps none and fails.
    for ([_]u2{ 0, 1, 2, 3 }) |turn| {
        const turned: views.Eye = .{ .anchor = .{ 130, 150 }, .turn = turn };
        const kept = views.roofKept(&.{shed}, &.{vent}, turned, turned.turn, try blocksDrawn(arena, turned, &.{ shed, vent }));
        try std.testing.expect(kept[0] > 40);
        try std.testing.expectEqual(kept[0], kept[1]);
        const flat = views.roofKept(&.{shed}, &.{vent}, turned, turned.turn, try blocksDrawn(arena, turned, &.{shed}));
        try std.testing.expectEqual(@as(f64, 0), flat[1]);
        try std.testing.expect(views.lowFails(flat[0], flat[1]));
    }
}

test "a face turned to another side is relit by the 2003 light, not by the colours of the walls" {
    // The lit wall against the shaded one is `wall_light_rgb`, on every turn.
    const lit = [3]f64{ 0, -1, 0 };
    const shaded = [3]f64{ 1, 0, 0 };
    for (0..3) |c| {
        try std.testing.expectApproxEqAbs(views.wall_light_rgb[c], views.shading(lit, 0, views.wall_light_rgb)[c] / views.shading(shaded, 0, views.wall_light_rgb)[c], 1e-9);
        try std.testing.expectApproxEqAbs(views.wall_light_rgb[c], views.shading(.{ -1, 0, 0 }, 1, views.wall_light_rgb)[c] / views.shading(lit, 1, views.wall_light_rgb)[c], 1e-9);
    }
    // A roof is lit from above more than either wall; a wall turned away
    // from the light keeps the ambient light, as the shaded one.
    const roof = [3]f64{ 0, -std.math.sqrt1_2, std.math.sqrt1_2 };
    try std.testing.expect(views.shading(roof, 0, views.wall_light_rgb)[1] > views.shading(lit, 0, views.wall_light_rgb)[1]);
    try std.testing.expectEqual(views.shading(shaded, 0, views.wall_light_rgb), views.shading(.{ 0, 1, 0 }, 0, views.wall_light_rgb));
    // A shaded cream wall turned to the light comes out cream, not orange;
    // a relit pixel keeps its hue where a channel would clip.
    const cream = views.relight(.{ 110, 100, 70, 255 }, views.shading(lit, 0, views.wall_light_rgb), views.shading(shaded, 0, views.wall_light_rgb));
    try std.testing.expectEqual([4]u8{ 200, 203, 190, 255 }, cream);
    const tile = views.relight(.{ 200, 80, 40, 255 }, .{ 2, 2, 2 }, .{ 1, 1, 1 });
    try std.testing.expectEqual([4]u8{ 255, 102, 51, 255 }, tile);
}

test "a building's own walls set its light where they are seen and lie in the 2003 spread" {
    // a_wb/01's planks: the shaded gable turned to the light comes out as
    // its lit planks, not the pale grey the median light made of it.
    var planks: views.Means = .{};
    for (0..600) |_| {
        planks.add(0, .{ 160, 149, 101, 255 });
        planks.add(1, .{ 102, 95, 62, 255 });
    }
    const own = views.wallLight(planks);
    const lit = views.relight(.{ 102, 95, 62, 255 }, views.shading(.{ 0, -1, 0 }, 0, own), views.shading(.{ 1, 0, 0 }, 0, own));
    try std.testing.expectEqual([4]u8{ 160, 149, 101, 255 }, lit);
    const median = views.relight(.{ 102, 95, 62, 255 }, views.shading(.{ 0, -1, 0 }, 0, views.wall_light_rgb), views.shading(.{ 1, 0, 0 }, 0, views.wall_light_rgb));
    try std.testing.expect(median[2] > 160);
    // A wall seen on a few pixels, or walls of about one brightness (a box
    // that put roof pixels on both), keep the median.
    var few: views.Means = .{};
    for (0..100) |_| {
        few.add(0, .{ 160, 149, 101, 255 });
        few.add(1, .{ 102, 95, 62, 255 });
    }
    try std.testing.expectEqual(views.wall_light_rgb, views.wallLight(few));
    var alike: views.Means = .{};
    for (0..600) |_| {
        alike.add(0, .{ 97, 70, 42, 255 });
        alike.add(1, .{ 89, 72, 47, 255 });
    }
    try std.testing.expectEqual(views.wall_light_rgb, views.wallLight(alike));
    // A lit wall of another material lends its brightness, not its colour:
    // factoryfloor01's lit red brick against its shaded grey (redder than
    // no colour at all) and e_frenchshed01's cream plaster against brown
    // stone (greener than the median) are held to that range.
    var brick: views.Means = .{};
    var plaster: views.Means = .{};
    for (0..600) |_| {
        brick.add(0, .{ 162, 108, 67, 255 });
        brick.add(1, .{ 90, 76, 53, 255 });
        plaster.add(0, .{ 142, 118, 63, 255 });
        plaster.add(1, .{ 95, 61, 25, 255 });
    }
    const red = views.wallLight(brick);
    try std.testing.expectApproxEqAbs(162.0 / 90.0, red[0], 1e-9);
    try std.testing.expectEqual(red[0], red[1]);
    try std.testing.expectEqual(red[0], red[2]);
    const cream = views.wallLight(plaster);
    for (1..3) |c| try std.testing.expectApproxEqAbs(cream[0] * views.wall_light_rgb[c] / views.wall_light_rgb[0], cream[c], 1e-9);
}
test "a turn lit by the sun fixed in the world keeps each face lit as the 2003 light lights it" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A flat box painted as the 2003 light lights it: one grey, times its shading.
    const blocks = [_]views.Block{.{ .lo = .{ 0, 0 }, .hi = .{ 3 * views.tile_size, 2 * views.tile_size }, .wall = 30, .ridge = 30 }};
    const eye: views.Eye = .{ .anchor = .{ 130, 150 } };
    const picture = try blocksPicture(arena, eye, &blocks);
    for (picture.pixels, 0..) |*p, i| {
        const hit = views.castPixel(&blocks, eye, @as(f64, @floatFromInt(i % picture.width)) + 0.5, @as(f64, @floatFromInt(i / picture.width)) + 0.5) orelse continue;
        const s = views.shading(hit.normal, 0, views.wall_light_rgb);
        p.* = .{ @intFromFloat(@round(40 * s[0])), @intFromFloat(@round(40 * s[1])), @intFromFloat(@round(40 * s[2])), 255 };
    }
    const projectors = [_]views.Projector{.{ .eye = eye, .picture = picture }};
    const size = [2]u32{ picture.width, picture.height };
    // The half turn shows the +y and -x walls. Lit by the 2003 light fixed
    // to the camera (light 2) the +y wall is lit as the 2003 picture's lit
    // -y wall; lit by the world's sun as camera step 2 shows a building of
    // facing 0 (light 0) it is the back, in the shade, and -x takes the
    // sun's side light.
    const back: views.Eye = .{ .anchor = .{ 130, 150 }, .turn = 2 };
    // Green of the +y and the -x wall, per light.
    var sums = [2][2]f64{ .{ 0, 0 }, .{ 0, 0 } };
    for ([_]u2{ 2, 0 }, &sums) |light, *sum| {
        const drawn = try views.projectPicture(arena, &blocks, blocks.len, null, views.wall_light_rgb, back, light, size, &projectors);
        for (drawn.image.pixels, drawn.kinds, 0..) |p, kind, i| {
            if (kind == .empty or kind == .filled) continue;
            const hit = views.castPixel(&blocks, back, @as(f64, @floatFromInt(i % size[0])) + 0.5, @as(f64, @floatFromInt(i / size[0])) + 0.5).?;
            // Each pixel is the grey lit as its face is under that light.
            const s = views.shading(hit.normal, light, views.wall_light_rgb);
            for (0..3) |c| try std.testing.expectApproxEqAbs(40 * s[c], @as(f64, @floatFromInt(p[c])), 1.5);
            if (hit.normal[1] > 0.5) sum[0] += @floatFromInt(p[1]);
            if (hit.normal[0] < -0.5) sum[1] += @floatFromInt(p[1]);
        }
    }
    try std.testing.expect(sums[0][0] > 1.5 * sums[1][0]);
    try std.testing.expect(sums[1][1] > sums[0][1]);
}

test "an object picture relit for a camera step turns its lit side with the world's sun" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A round crown 60 pixels wide, painted as the 2003 light lights the shape
    // spriteNormals reads into it.
    const crown: views.Image = .{ .width = 80, .height = 80, .pixels = try arena.alloc([4]u8, 80 * 80) };
    for (0..80) |y| for (0..80) |x| {
        const dx = @as(f64, @floatFromInt(x)) + 0.5 - 40;
        const dy = @as(f64, @floatFromInt(y)) + 0.5 - 40;
        crown.pixels[y * 80 + x] = if (dx * dx + dy * dy < 900) .{ 60, 60, 60, 255 } else .{ 0, 0, 0, 0 };
    };
    const normals = try views.spriteNormals(arena, crown);
    // Its left edge faces the screen's left (world (-1, -1)), its right edge
    // the screen's right, its middle the viewer (world (1, -1)).
    const left = normals[40 * 80 + 11].?;
    const right = normals[40 * 80 + 68].?;
    const middle = normals[60 * 80 + 40].?;
    try std.testing.expect((-left[0] - left[1]) * std.math.sqrt1_2 > 0.9);
    try std.testing.expect((right[0] + right[1]) * std.math.sqrt1_2 > 0.9);
    try std.testing.expect((middle[0] - middle[1]) * std.math.sqrt1_2 > 0.95);
    for (crown.pixels, normals) |*p, n| if (n) |v| {
        const s = views.shading(v, 0, views.wall_light_rgb);
        p.* = .{ @intFromFloat(@round(40 * s[0])), @intFromFloat(@round(40 * s[1])), @intFromFloat(@round(40 * s[2])), 255 };
    };
    // Lit from the screen's left at step 0; the half turn shows it from the
    // other side, where the sun stands on the screen's right.
    const half = try views.relightSprite(arena, crown, normals, views.wall_light_rgb, 2, false);
    var sides = [2][2]f64{ .{ 0, 0 }, .{ 0, 0 } };
    for (crown.pixels, half.pixels, 0..) |a, b, i| {
        if (a[3] == 0) continue;
        // The 2003 grey was rounded, and the relight scales that rounding.
        const s = views.shading(normals[i].?, 2, views.wall_light_rgb);
        try std.testing.expectApproxEqAbs(40 * s[1], @as(f64, @floatFromInt(b[1])), 3);
        const on_right: usize = @intFromBool(i % 80 >= 40);
        sides[0][on_right] += @floatFromInt(a[1]);
        sides[1][on_right] += @floatFromInt(b[1]);
    }
    try std.testing.expect(sides[0][0] > sides[0][1]);
    try std.testing.expect(sides[1][1] > sides[1][0]);
    // A crown shows its own light: lit against shaded within the median's.
    const own = views.objectLight(crown, normals);
    for (0..3) |c| try std.testing.expect(own[c] >= 1 and own[c] <= views.wall_light_rgb[c]);
}

test "a back roof copied from the front leaves the front's chimney out, and a copy that keeps it fails the gate" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A gabled house and a chimney on its front roof plane (towards -y),
    // which the 2003 eye sees and its half turn does not.
    const gabled: views.Block = .{ .lo = .{ 0, 0 }, .hi = .{ 4 * views.tile_size, 3 * views.tile_size }, .wall = 30, .ridge = 60, .roof = .gable_x };
    const chimney: views.Block = .{ .lo = .{ 25, 8 }, .hi = .{ 37, 20 }, .wall = 75, .ridge = 75 };
    const blocks = [_]views.Block{ gabled, chimney };
    const eye: views.Eye = .{ .anchor = .{ 130, 170 } };
    const picture = try blocksPicture(arena, eye, &blocks);
    const back: views.Eye = .{ .anchor = .{ 130, 170 }, .turn = 2 };
    const size = [2]u32{ picture.width, picture.height };
    const projectors = [_]views.Projector{.{ .eye = eye, .picture = picture }};
    // Told which block is a part, the back roof plane borrows nothing
    // near it; told none, it copies the chimney's foot and fails.
    const drawn = try views.projectPicture(arena, &blocks, 1, null, views.wall_light_rgb, back, back.turn, size, &projectors);
    try std.testing.expectEqual(@as(f64, 0), views.copiedParts(&blocks, 1, back, drawn));
    const copied = try views.projectPicture(arena, &blocks, blocks.len, null, views.wall_light_rgb, back, back.turn, size, &projectors);
    try std.testing.expect(views.copiedParts(&blocks, 1, back, copied) > 40);
    // The back roof is still drawn, from the rest of the front one.
    var mirrored: usize = 0;
    for (drawn.kinds) |kind| mirrored += @intFromBool(kind == .mirrored);
    try std.testing.expect(mirrored > 1000);
    // The tiles beside and above the chimney's foot lend the back roof
    // theirs (`slidePast`, `partReach`): no pixel the copy draws from the
    // front is left to the patch fill.
    var lost: usize = 0;
    for (copied.kinds, drawn.kinds) |a, b| lost += @intFromBool(a == .mirrored and b == .filled);
    try std.testing.expectEqual(@as(usize, 0), lost);
}

test "a chimney is found from the picture alone, cut whole from a roof fitted up into it, and a turn that paints it on the roof fails the gate" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A gabled house and a chimney 12 units square on its ridge, 20 above it
    // (e_house02_1).
    const gabled: views.Block = .{ .lo = .{ 0, 0 }, .hi = .{ 3 * views.tile_size, 2 * views.tile_size }, .wall = 30, .ridge = 55, .roof = .gable_x };
    const chimney: views.Block = .{ .lo = .{ 28, 17 }, .hi = .{ 40, 29 }, .wall = 75, .ridge = 75 };
    var eyes: [3]views.Eye = undefined;
    var pictures: [3]views.Image = undefined;
    for (&eyes, &pictures, [_]u2{ 0, 1, 3 }) |*eye, *picture, turn| {
        eye.* = .{ .anchor = .{ 130, 150 }, .turn = turn };
        picture.* = try blocksPicture(arena, eye.*, &.{ gabled, chimney });
    }
    // The gable's apex is no chimney; the chimney's top is.
    var found: usize = 0;
    for (try views.chimneyMask(arena, pictures[0])) |m| found += @intFromBool(m);
    try std.testing.expect(found > 40);
    for (try views.chimneyMask(arena, try blocksPicture(arena, eyes[0], &.{gabled}))) |m| try std.testing.expect(!m);
    // The roof the fit raised into the chimney (S05..S07e on e_house02)
    // leaves only its top above the blocks; the chimney is cut whole all the
    // same, as wide as it is and where it stands.
    var swollen = gabled;
    swollen.ridge = 66;
    const cut = try views.cutChimneys(arena, &.{swollen}, &.{}, &pictures, &eyes, false);
    try std.testing.expect(cut.len >= 2);
    try std.testing.expectApproxEqAbs(@as(f64, 12), cut[1].hi[0] - cut[1].lo[0], 4);
    for (0..2) |k| try std.testing.expectApproxEqAbs((chimney.lo[k] + chimney.hi[k]) / 2, (cut[1].lo[k] + cut[1].hi[k]) / 2, 6);
    // Kept: the blocks cover the chimney the picture shows, or lose it.
    const lost = try views.keptChimneys(arena, &.{gabled}, pictures[0], eyes[0]);
    try std.testing.expectEqual(@as(f64, 0), lost[1]);
    const kept = try views.keptChimneys(arena, &.{ gabled, chimney }, pictures[0], eyes[0]);
    try std.testing.expect(kept[1] >= 0.9 * kept[0]);
    // Painted: a quarter turn of the swollen roof draws the chimney's
    // pixels on the roof plane; with the chimney a part it draws none.
    const turned: views.Eye = .{ .anchor = .{ 130, 150 }, .turn = 1 };
    const size = [2]u32{ pictures[0].width, pictures[0].height };
    const projectors = [_]views.Projector{.{ .eye = eyes[0], .picture = pictures[0] }};
    const painted = try views.projectPicture(arena, &.{swollen}, 1, null, views.wall_light_rgb, turned, turned.turn, size, &projectors);
    try std.testing.expect(try views.paintedChimneys(arena, &.{swollen}, 1, turned, painted, pictures[0..1], eyes[0..1]) > 40);
    const blocks = [_]views.Block{ gabled, chimney };
    const drawn = try views.projectPicture(arena, &blocks, 1, null, views.wall_light_rgb, turned, turned.turn, size, &projectors);
    try std.testing.expectEqual(@as(f64, 0), try views.paintedChimneys(arena, &blocks, 1, turned, drawn, pictures[0..1], eyes[0..1]));
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

test "a variant folder of its own is a family of one, drawn on the shape.json beside its packs" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = std.testing.io;
    const cwd = std.Io.Dir.cwd();
    // A modder's copy of e_frenchshed01, in no region or season folder.
    const copy = "zig-out/local-test/bk-views/lone/e_frenchshed01";
    cwd.deleteTree(io, "zig-out/local-test/bk-views/lone") catch {};
    try cwd.createDirPath(io, copy);
    const from = "Data/Buildings/europe/summer/e_frenchshed01";
    var dir = try cwd.openDir(io, from, .{ .iterate = true });
    defer dir.close(io);
    var iterator = dir.iterate();
    while (try iterator.next(io)) |entry| {
        if (entry.kind != .file) continue;
        try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(arena, "{s}/{s}", .{ copy, entry.name }), .data = try read(arena, try std.fmt.allocPrint(arena, "{s}/{s}", .{ from, entry.name })) });
    }
    try cwd.writeFile(io, .{ .sub_path = copy ++ "/" ++ views.variant_shape, .data = "[{\"lo\": [-40, -20], \"hi\": [8, 12], \"wall\": 20, \"ridge\": 30, \"roof\": \"gable_x\"}]\r\n" });
    const result = try std.process.run(arena, io, .{ .argv = &.{ bk_views_gate.bk_views_fast, "shape", "zig-out/local-test/bk-views/lone/shape", copy }, .stdout_limit = .limited(1024 * 1024) });
    if (result.term != .exited or result.term.exited != 0) {
        std.debug.print("bk-views shape failed: {s}\n", .{result.stderr});
        return error.ShapeFailed;
    }
    const Shapes = struct { shapes: []const struct { variant: []const u8, hand: bool, blocks: []const views.Block } };
    const shapes = try std.json.parseFromSliceLeaky(Shapes, arena, try read(arena, "zig-out/local-test/bk-views/lone/shape/shape.json"), .{ .ignore_unknown_fields = true });
    try std.testing.expectEqual(@as(usize, 1), shapes.shapes.len);
    try std.testing.expect(shapes.shapes[0].hand);
    try std.testing.expectEqualSlices(views.Block, &.{.{ .lo = .{ -40, -20 }, .hi = .{ 8, 12 }, .wall = 20, .ridge = 30, .roof = .gable_x }}, shapes.shapes[0].blocks);
}

test "every hand shape reaches the outline floor against the 2003 pictures of its colourway" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = std.testing.io;
    const options: std.json.ParseOptions = .{ .ignore_unknown_fields = true };
    const Inventory = struct { families: []const struct { family: []const u8, variants: []const struct { path: []const u8 } } };
    const inventory = try std.json.parseFromSliceLeaky(Inventory, arena, try read(arena, "tools/zig/fixtures/bk-views/inventory.json"), options);
    // Every variant a shape file names, by its family's variant folders.
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(arena, &.{ bk_views_gate.bk_views_fast, "shape", "zig-out/local-test/bk-views/shapes" });
    var dir = try std.Io.Dir.cwd().openDir(io, views.shapes_dir, .{ .iterate = true });
    defer dir.close(io);
    var iterator = dir.iterate();
    var named: usize = 0;
    while (try iterator.next(io)) |entry| {
        if (!std.mem.endsWith(u8, entry.name, ".json")) continue;
        const family = entry.name[0 .. entry.name.len - ".json".len];
        const shapes = try std.json.parseFromSliceLeaky(std.json.ArrayHashMap(std.json.Value), arena, try read(arena, try std.fmt.allocPrint(arena, "{s}/{s}", .{ views.shapes_dir, entry.name })), options);
        const variants = for (inventory.families) |f| {
            if (std.mem.eql(u8, f.family, family)) break f.variants;
        } else {
            // A prop of Data/Objects drawn on its shape (the sand pile).
            for (shapes.map.keys()) |key| {
                const path = for (views.shaped_objects) |o| {
                    if (std.mem.endsWith(u8, o, try std.fmt.allocPrint(arena, "/{s}/{s}", .{ family, key }))) break o;
                } else return error.NoSuchFamily;
                try argv.append(arena, try std.fmt.allocPrint(arena, "Data/Objects/{s}", .{path}));
                named += 1;
            }
            continue;
        };
        for (shapes.map.keys()) |key| {
            const variant = for (variants) |v| {
                if (std.mem.eql(u8, std.fs.path.basename(v.path), key)) break v;
            } else return error.NoSuchVariant;
            try argv.append(arena, try std.fmt.allocPrint(arena, "Data/Buildings/{s}", .{variant.path}));
            named += 1;
        }
    }
    try std.testing.expect(named > 0);
    const result = try std.process.run(arena, io, .{ .argv = argv.items, .stdout_limit = .limited(1024 * 1024) });
    if (result.term != .exited or result.term.exited != 0) {
        std.debug.print("bk-views shape failed: {s}\n", .{result.stderr});
        return error.ShapeFailed;
    }
    // Its own picture reaches the shape gate's outline floor. The family's
    // other pictures of its colourway (those that lend it pixels) reach the
    // blocks' hold-out mean floor of the intact state (gate.json's before the
    // full 3D objects): their zero crosses are aligned to about a world unit
    // and their sides measured on hand-painted grids (dot/03 and /04 both
    // read as side 4). Other colourways may be other buildings (e_shed02_4 is
    // an open shed, _5 a closed one).
    const others: f64 = 0.84;
    const Picture = struct { variant: []const u8, own: bool, same: bool, iou: f64 };
    const report = try std.json.parseFromSliceLeaky(struct { shapes: []const struct { variant: []const u8, hand: bool, pictures: []const Picture } }, arena, try read(arena, "zig-out/local-test/bk-views/shapes/shape.json"), options);
    try std.testing.expectEqual(named, report.shapes.len);
    var failures: usize = 0;
    for (report.shapes) |shape| {
        if (!shape.hand) {
            std.debug.print("{s}: its hand shape was not read\n", .{shape.variant});
            failures += 1;
        }
        for (shape.pictures) |picture| {
            const floor = if (picture.own) views.outline_floor else others;
            if (!(picture.own or picture.same) or picture.iou >= floor) continue;
            std.debug.print("{s} against {s}: outline IoU {d:.4} < {d:.2}\n", .{ shape.variant, picture.variant, picture.iou, floor });
            failures += 1;
        }
    }
    if (failures > 0) return error.BelowTheFloor;
}

/// Tracked text is checked out with CRLF; the tool writes LF.
fn withoutCr(gpa: std.mem.Allocator, text: []const u8) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    for (text) |c| if (c != '\r') try out.append(gpa, c);
    return out.items;
}

test "a source pixel spread over four target pixels is stretched, left to the patch fill, and fails the gate kept" {
    const gpa = std.testing.allocator;
    // A 6 x 6 picture: its left half takes one source pixel per target
    // pixel, its right half a ninth of one (a third of a pixel each way).
    var uvs: [36][2]f64 = undefined;
    var ids: [36]usize = undefined;
    for (0..6) |y| for (0..6) |x| {
        const fx: f64 = @floatFromInt(x);
        const fy: f64 = @floatFromInt(y);
        uvs[y * 6 + x] = if (x < 3) .{ fx, fy } else .{ 10 + fx / 3, fy / 3 };
        ids[y * 6 + x] = if (x < 3) 1 else 2;
    };
    const stretched = try views.stretchedPixels(gpa, .{ 6, 6 }, &uvs, &ids);
    defer gpa.free(stretched);
    // The right half but its last column and row, whose neighbours are off
    // the picture; the left half none, nor the seam (other ids).
    for (0..6) |y| for (0..6) |x| try std.testing.expectEqual(x >= 3 and x < 5 and y < 5, stretched[y * 6 + x]);
    var kinds: [36]views.Kind = @splat(.projected);
    var pixels: [36][4]u8 = @splat(.{ 0, 0, 0, 255 });
    var classes: [36]u8 = @splat(0);
    const drawn: views.Generated = .{ .image = .{ .width = 6, .height = 6, .pixels = &pixels }, .kinds = &kinds, .classes = &classes, .stretched = stretched };
    // 10 of 36 drawn pixels are stretched: over the gate's share.
    try std.testing.expectApproxEqAbs(10.0 / 36.0, views.stretchShare(drawn), 1e-9);
    try std.testing.expect(views.stretchShare(drawn) > views.stretch_most);
    // Left to the patch fill, they no longer count.
    for (&kinds, stretched) |*kind, s| {
        if (s) kind.* = .filled;
    }
    try std.testing.expectEqual(@as(f64, 0), views.stretchShare(drawn));
}

test "a heap turns as its 2003 pictures on every turn, mirrored on an odd one, and every heap is a family" {
    const building: views.TurnObject = .{ .thin = false, .small = false, .failed = false, .kept = false, .rectangle = true, .odd = true, .heap = false };
    // A house stays on its blocks, failing or not.
    try std.testing.expect(!views.spriteTurn(building).sprite);
    try std.testing.expect(!views.spriteTurn(.{ .thin = false, .small = false, .failed = true, .kept = true, .rectangle = true, .odd = false, .heap = false }).sprite);
    // A depot (e_wt/02: 5 x 6 cells, two corners free) on each turn.
    for ([_]bool{ true, false }) |odd| {
        const how = views.spriteTurn(.{ .thin = false, .small = false, .failed = false, .kept = false, .rectangle = false, .odd = odd, .heap = true });
        try std.testing.expect(how.sprite);
        try std.testing.expectEqual(odd, how.mirror);
    }
    // A heap whose footprint the turn keeps is not mirrored.
    try std.testing.expect(!views.spriteTurn(.{ .thin = false, .small = false, .failed = false, .kept = true, .rectangle = true, .odd = true, .heap = true }).mirror);
    // Every heap names a family of the inventory.
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const options: std.json.ParseOptions = .{ .ignore_unknown_fields = true };
    const Inventory = struct { families: []const struct { family: []const u8 } };
    const inventory = try std.json.parseFromSliceLeaky(Inventory, arena, try read(arena, "tools/zig/fixtures/bk-views/inventory.json"), options);
    const props = try std.json.parseFromSliceLeaky(std.json.ArrayHashMap([]const u8), arena, try read(arena, views.props_file), options);
    try std.testing.expect(props.map.count() > 0);
    for (props.map.keys()) |family| {
        for (inventory.families) |f| {
            if (std.mem.eql(u8, f.family, family)) break;
        } else return error.NoSuchFamily;
    }
}

/// A picture mirrored across the screen's vertical through point `from` and
/// moved so that `from` lands on `to`, as a mirrored sprite is laid on its
/// turned footprint.
fn mirrorPicture(gpa: std.mem.Allocator, image: views.Image, from: [2]f64, to: [2]f64) !views.Image {
    const out: views.Image = .{ .width = image.width, .height = image.height, .pixels = try gpa.alloc([4]u8, image.pixels.len) };
    const fold: i64 = @intFromFloat(@round(2 * from[0]));
    const dx: i64 = @intFromFloat(@round(to[0] - from[0]));
    const dy: i64 = @intFromFloat(@round(to[1] - from[1]));
    for (0..image.height) |y| for (0..image.width) |x| {
        const at = fold - 1 - (@as(i64, @intCast(x)) - dx);
        out.pixels[y * image.width + x] = if (image.at(at, @as(i64, @intCast(y)) - dy)) |p| p.* else .{ 0, 0, 0, 0 };
    };
    return out;
}

test "a turn is the shape turned: an L-shaped house mirrored or left as it was fails, a box mirrored passes" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // A house with a wing at one end of its front and a chimney at the
    // other end of its ridge: no mirror shows it turned.
    const winged = [_]views.Block{
        .{ .lo = .{ 0, 0 }, .hi = .{ 90, 40 }, .wall = 30, .ridge = 55, .roof = .gable_x },
        .{ .lo = .{ 0, -40 }, .hi = .{ 30, 0 }, .wall = 30, .ridge = 50, .roof = .gable_y },
        .{ .lo = .{ 70, 16 }, .hi = .{ 78, 24 }, .wall = 70, .ridge = 70 },
    };
    const eyes = [2]views.Eye{ .{ .anchor = .{ 130, 150 } }, .{ .anchor = .{ 130, 150 }, .turn = 1 } };
    const axis = eyes[1].project(.{ 45, 0, 0 })[0];
    // Drawn turned it is its own turned shape.
    const turned = try views.handedness(arena, &winged, 2, eyes[1], axis, try blocksPicture(arena, eyes[1], &winged));
    try std.testing.expect(turned.turned > 0.99);
    try std.testing.expect(turned.symmetric < 0.95);
    try std.testing.expectEqual(@as(?[]const u8, null), try views.handedFailure(arena, turned));
    // The 2003 picture mirrored lays it on the turned footprint, the wing and
    // the chimney on the other hand.
    const mirrored = try views.handedness(arena, &winged, 2, eyes[1], axis, try mirrorPicture(arena, try blocksPicture(arena, eyes[0], &winged), eyes[0].project(.{ 45, 0, 0 }), eyes[1].project(.{ 45, 0, 0 })));
    try std.testing.expect(mirrored.turned < 0.9);
    try std.testing.expect(try views.handedFailure(arena, mirrored) != null);
    // The 2003 picture as it was, at a half turn.
    const half: views.Eye = .{ .anchor = .{ 130, 150 }, .turn = 2 };
    const same = try views.handedness(arena, &winged, 2, half, half.project(.{ 45, 0, 0 })[0], try blocksPicture(arena, eyes[0], &winged));
    try std.testing.expect(try views.handedFailure(arena, same) != null);
    // A box's 2003 picture mirrored is the box turned, and passes.
    const box = [_]views.Block{.{ .lo = .{ 0, 0 }, .hi = .{ 90, 40 }, .wall = 30, .ridge = 30 }};
    const plain = try views.handedness(arena, &box, 1, eyes[1], eyes[1].project(.{ 45, 20, 0 })[0], try mirrorPicture(arena, try blocksPicture(arena, eyes[0], &box), eyes[0].project(.{ 45, 20, 0 }), eyes[1].project(.{ 45, 20, 0 })));
    try std.testing.expect(plain.turned > 0.97);
    try std.testing.expectEqual(@as(?[]const u8, null), try views.handedFailure(arena, plain));
}

test {
    // The review document's PDF writer.
    _ = @import("bk_pdf.zig");
}

test "a shape model drawn at turn 0 is its own outline, and a stake in a picture is lifted to a pole standing on the ground" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const eye: views.Eye = .{ .anchor = .{ 130, 150 } };
    // A gabled house with a chimney: the model's faces cover what the
    // blocks' rays meet.
    const gabled = [_]views.Block{
        .{ .lo = .{ 0, 0 }, .hi = .{ 90, 40 }, .wall = 30, .ridge = 55, .roof = .gable_x },
        .{ .lo = .{ 70, 16 }, .hi = .{ 78, 24 }, .wall = 70, .ridge = 70 },
    };
    const parts = [_]views.Part{ .{ .kind = .body, .block = gabled[0] }, .{ .kind = .chimney, .block = gabled[1] } };
    try std.testing.expect(try views.modelIou(arena, &parts, eye, try blocksPicture(arena, eye, &gabled)) > 0.97);
    // A stake 40 units long leaning in the picture's plane: one pole, its
    // foot on the ground under the stake's lower end.
    const picture: views.Image = .{ .width = 260, .height = 240, .pixels = try arena.alloc([4]u8, 260 * 240) };
    @memset(picture.pixels, .{ 0, 0, 0, 0 });
    for (0..40) |i| for (0..3) |w| {
        picture.pixels[(150 - i) * 260 + 120 + i / 2 + w] = .{ 90, 70, 50, 255 };
    };
    const poles = try views.fitPoles(arena, picture, eye);
    try std.testing.expectEqual(@as(usize, 1), poles.len);
    const pole = poles[0].pole.?;
    try std.testing.expectApproxEqAbs(@as(f64, 0), pole.from[2], 0.01);
    try std.testing.expect(pole.to[2] > 30);
    const foot = eye.project(pole.from);
    try std.testing.expect(@abs(foot[1] - 150) < 3 and @abs(foot[0] - 121.5) < 3);
}

/// The intact picture of a tracked building variant and its zero cross.
fn variantPicture(arena: std.mem.Allocator, dir: []const u8) !struct { picture: views.Image, anchor: [2]f64 } {
    const pack = try views.decodePack(arena, try read(arena, try std.fmt.allocPrint(arena, "{s}1.san", .{dir})));
    const atlas = try views.readDds(arena, try read(arena, try std.fmt.allocPrint(arena, "{s}1_h.dds", .{dir})));
    var bounds: views.Bounds = .{};
    for (pack.sprites[0].squares) |square| bounds.add(square);
    return .{ .picture = try views.unpack(arena, pack.sprites[0], atlas, bounds), .anchor = .{ @floatFromInt(-bounds.min_x), @floatFromInt(-bounds.min_y) } };
}

test "a joint family's tracked mesh scores on each 2003 side as the review did, lies on its pictures and turns from one side to the other" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const mesh = views.mesh;
    // a_h01_1 and a_h01_2 (3 and 4) share one mesh, measured sides 1 and 2.
    var models: [2]mesh.Model = undefined;
    for (&models, [_]u32{ 3, 4 }, [_][]const u8{ "a_h01_1", "a_h01_2" }) |*m, id, name| {
        m.* = (try mesh.load(arena, std.testing.io, mesh.meshes_dir, id)).?;
        try std.testing.expect(m.joint);
        try std.testing.expectEqualStrings("africa_a_h01", m.shared.?);
        const tris = try mesh.triangles(arena, m.*);
        const found = try variantPicture(arena, try std.fmt.allocPrint(arena, "Data/Buildings/africa/summer/{s}/", .{name}));
        // The review's measure, as edgefit.py takes it, within the tolerance.
        const score = try mesh.evaluate(arena, m.*, tris, m.alignment, (try mesh.target(arena, found.picture)).?);
        try std.testing.expectApproxEqAbs(m.scores.?.silhouette, score.silhouette, views.match_tolerance[0]);
        try std.testing.expectApproxEqAbs(m.scores.?.edge, score.edge, views.match_tolerance[1]);
        // Placed on the picture, the mesh at N covers its outline.
        const placement = mesh.place(m.*, found.picture, found.anchor).?;
        const r = try mesh.raster(arena, try mesh.viewVertices(arena, m.*, mesh.view(placement, 0, found.anchor)), tris, found.picture.width, found.picture.height);
        var both: f64 = 0;
        var either: f64 = 0;
        for (found.picture.pixels, r.tri) |p, t| {
            both += @floatFromInt(@intFromBool(p[3] >= 128 and t != mesh.Raster.none));
            either += @floatFromInt(@intFromBool(p[3] >= 128 or t != mesh.Raster.none));
        }
        try std.testing.expect(both / either > 0.85);
    }
    // Side 2 is side 1 turned a quarter: the same mesh, its yaw 90 degrees on.
    const v = mesh.vertex(models[0], 7);
    const a = mesh.transformed(v, models[0].alignment, 1);
    const b = mesh.transformed(v, models[1].alignment, 0);
    for (0..3) |c| try std.testing.expectApproxEqAbs(a[c], b[c], 1e-6);
}
test "a box's full 3D model unrolls each face flat, its normals outward, and writes the vehicles' .mod chunks" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const mesh = views.mesh;
    const model3d = views.model3d;
    // A box 20 x 10 x 8 with its faces wound either way, as the review's meshes are.
    const vertices = [_]f64{ 0, 0, 0, 20, 0, 0, 20, 10, 0, 0, 10, 0, 0, 0, 8, 20, 0, 8, 20, 10, 8, 0, 10, 8 };
    const triangles = [_]u32{ 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7 };
    const parts = [_]mesh.Part{.{ .kind = "mesh", .triangles = &triangles }};
    const box: mesh.Model = .{ .id = 0, .path = "box", .vertices = &vertices, .parts = &parts };
    const m = try model3d.unwrap(arena, box, try mesh.triangles(arena, box), .{});
    try std.testing.expectEqual(@as(usize, 6), m.charts.len);
    for (m.triangles) |t| {
        var mid = [3]f64{ 0, 0, 0 };
        for (t.v) |v| for (0..3) |c| {
            mid[c] += m.vertices[v].world[c] / 3;
        };
        try std.testing.expect(mesh.dot(t.normal, .{ mid[0] - 10, mid[1] - 5, mid[2] - 4 }) > 0);
    }
    var names: [3][]const u8 = undefined;
    for (m.groups, 0..) |g, i| names[i] = g;
    try std.testing.expectEqual(@as(usize, 3), m.groups.len);
    for ([_][]const u8{ "body.under", "body.roof", "body.wall" }) |want| {
        for (names) |n| {
            if (std.mem.eql(u8, n, want)) break;
        } else return error.GroupMissing;
    }
    // A wall chart is as wide and high as the wall at `density` texels a unit.
    for (m.charts) |c| try std.testing.expect(c.w - 4 == @as(u32, @intFromFloat(20 * model3d.density)) or c.w - 4 == @as(u32, @intFromFloat(10 * model3d.density)) or c.w - 4 == @as(u32, @intFromFloat(8 * model3d.density)));
    // The .mod: a data chunk 1 holding the skeleton (1, its nodes in chunk
    // 1, a node a group), the meshes (2, one a group) and the box (4), then
    // the empty chunks 0 and 2.
    const mod = try model3d.writeMod(arena, m);
    const data = views.findChunk(mod, 1, 0).?;
    try std.testing.expectEqual(@as(usize, 3), countChunks(views.findChunk(views.findChunk(data, 1, 0).?, 1, 0).?, 1));
    try std.testing.expectEqual(@as(usize, 3), countChunks(views.findChunk(data, 2, 0).?, 1));
    try std.testing.expectEqual(@as(usize, 24), views.findChunk(data, 4, 0).?.len);
    try std.testing.expect(views.findChunk(mod, 0, 0) != null and views.findChunk(mod, 2, 0) != null);
}

fn countChunks(bytes: []const u8, id: u8) usize {
    var n: usize = 0;
    while (views.findChunk(bytes, id, n) != null) n += 1;
    return n;
}

test "a_h01_1 as a full 3D model renders at N as its 2003 picture, has no holes, and renders the same every time" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const mesh = views.mesh;
    const model3d = views.model3d;
    const model = (try mesh.load(arena, std.testing.io, mesh.meshes_dir, 3)).?;
    const tris = try mesh.triangles(arena, model);
    const found = try variantPicture(arena, "Data/Buildings/africa/summer/a_h01_1/");
    const placement = mesh.place(model, found.picture, found.anchor).?;
    var m = try model3d.unwrap(arena, model, tris, placement);
    // Its own picture the only one the bake paints from.
    const v = mesh.view(placement, 0, found.anchor);
    const normals = try arena.alloc([3]f64, tris.len);
    for (tris, normals) |t, *n| n.* = mesh.worldNormal(model, placement, t, 0);
    const raster = try mesh.raster(arena, try mesh.viewVertices(arena, model, v), tris, found.picture.width, found.picture.height);
    try model3d.bake(arena, &m, "1", &.{.{ .picture = found.picture, .view = v, .raster = raster, .normals = normals }}, null);
    // N on the picture's own canvas at its zero cross, with no shift: the
    // outline as the mesh's, the colour within a few levels where N shows the
    // picture's own pixels. (A face the picture samples too sparsely for some
    // turn is painted from the object, at N too; with the colourway's other
    // picture, as model3d bakes it, N's colour difference is under 1.)
    const n = try model3d.render(arena, m, "1", 0, 0, found.anchor, .{ found.picture.width, found.picture.height });
    const match = views.artMatch(n.image, found.picture, .{ 0, 0 }, false);
    try std.testing.expect(match.iou > 0.9);
    var difference: f64 = 0;
    var projected: f64 = 0;
    for (n.image.pixels, found.picture.pixels, n.paint) |r, a, k| {
        if (k != .seen or a[3] < 128) continue;
        for (0..3) |c| difference += @abs(@as(f64, @floatFromInt(r[c])) - @as(f64, @floatFromInt(a[c]))) / 3;
        projected += 1;
    }
    try std.testing.expect(projected > 1000);
    try std.testing.expect(difference / projected < 4);
    try std.testing.expectEqual(@as(usize, 0), n.unpainted);
    // A quarter turn: no holes, and the same bytes a second time.
    const size = [2]u32{ 2 * found.picture.width, 2 * found.picture.height };
    const anchor = [2]f64{ found.anchor[0] + @as(f64, @floatFromInt(found.picture.width)) / 2, found.anchor[1] + @as(f64, @floatFromInt(found.picture.height)) / 2 };
    const e = try model3d.render(arena, m, "1", 1, 1, anchor, size);
    try std.testing.expectEqual(@as(usize, 0), e.unpainted);
    var opaque_pixels: usize = 0;
    for (e.image.pixels) |p| opaque_pixels += @intFromBool(p[3] == 255);
    try std.testing.expect(opaque_pixels > 1000);
    const again = try model3d.render(arena, m, "1", 1, 1, anchor, size);
    try std.testing.expectEqualSlices(u8, std.mem.sliceAsBytes(e.image.pixels), std.mem.sliceAsBytes(again.image.pixels));
}

test "a full 3D model's paint stretches no 2003 pixel: e_house02_2's back roof slope comes from the object, not smeared from N, at every turn" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const mesh = views.mesh;
    const model3d = views.model3d;
    // e_house02_2 (62), a house of the maintainer's village screenshot: the
    // 2003 camera sees its back roof slope almost edge on.
    const model = (try mesh.load(arena, std.testing.io, mesh.meshes_dir, 62)).?;
    const tris = try mesh.triangles(arena, model);
    const found = try variantPicture(arena, "Data/Buildings/europe/summer/e_house02_2/");
    const placement = mesh.place(model, found.picture, found.anchor).?;
    const v = mesh.view(placement, 0, found.anchor);
    const normals = try arena.alloc([3]f64, tris.len);
    for (tris, normals) |t, *n| n.* = mesh.worldNormal(model, placement, t, 0);
    const raster = try mesh.raster(arena, try mesh.viewVertices(arena, model, v), tris, found.picture.width, found.picture.height);
    const bake_view: model3d.BakeView = .{ .picture = found.picture, .view = v, .raster = raster, .normals = normals };
    const size = [2]u32{ 2 * found.picture.width, 2 * found.picture.height };
    const anchor = [2]f64{ found.anchor[0] + @as(f64, @floatFromInt(found.picture.width)) / 2, found.anchor[1] + @as(f64, @floatFromInt(found.picture.height)) / 2 };

    // An upright wall a quarter turn shows sheared the other way at the same
    // density: not stretched.
    var m = try model3d.unwrap(arena, model, tris, placement);
    var walls: usize = 0;
    for (m.triangles) |t| {
        if (@abs(t.normal[2]) > 0.01) continue;
        const at_n = model3d.turnMap(m, t, 0) orelse continue;
        for (1..4) |k| try std.testing.expect(model3d.magnification(model3d.turnMap(m, t, @intCast(k)).?, at_n) < 1.001);
        walls += 1;
    }
    try std.testing.expect(walls > 0);

    // The 2026-10-29 paint stretched the 2003 pixels of the faces N sees
    // obliquely over the turns that show them squarely (over a hundredth of
    // the turns' pixels); the coherent paint
    // stretches none, copies the back faces from their counterparts, and
    // still draws N as the 2003 picture.
    var stretched = [2]usize{ 0, 0 };
    var drawn: usize = 0;
    for ([_]model3d.Method{ .tile, .coherent }, 0..) |method, i| {
        m.method = method;
        m.textures = &.{};
        try model3d.bake(arena, &m, "1", &.{bake_view}, null);
        for (1..4) |k| {
            const r = try model3d.render(arena, m, "1", @intCast(k), @intCast(k), anchor, size);
            for (r.stretched, r.image.pixels) |s, p| {
                stretched[i] += @intFromBool(s);
                if (i == 1) drawn += @intFromBool(p[3] == 255);
            }
            try std.testing.expectEqual(@as(usize, 0), r.unpainted);
        }
    }
    try std.testing.expect(stretched[0] * 100 > drawn);
    try std.testing.expectEqual(@as(usize, 0), stretched[1]);
    var copied: usize = 0;
    for (m.textures[0].paint) |p| copied += @intFromBool(p == .copied);
    try std.testing.expect(copied > 0);
    const n = try model3d.render(arena, m, "1", 0, 0, found.anchor, .{ found.picture.width, found.picture.height });
    try std.testing.expect(views.artMatch(n.image, found.picture, .{ 0, 0 }, false).colour < 4);
    // The same bytes a second time.
    var again = try model3d.unwrap(arena, model, tris, placement);
    try model3d.bake(arena, &again, "1", &.{bake_view}, null);
    try std.testing.expectEqualSlices(u8, std.mem.sliceAsBytes(m.textures[0].image.pixels), std.mem.sliceAsBytes(again.textures[0].image.pixels));
}

/// The pictures of a variant's states on the canvas they share (the bounds of
/// all their packs), and its zero cross there.
fn statePictures(arena: std.mem.Allocator, dir: []const u8, states: []const []const u8) !struct { pictures: []views.Image, anchor: [2]f64 } {
    const packs = try arena.alloc(views.Pack, states.len);
    const atlases = try arena.alloc(views.Image, states.len);
    var bounds: views.Bounds = .{};
    for (states, packs, atlases) |state, *pack, *atlas| {
        pack.* = try views.decodePack(arena, try read(arena, try std.fmt.allocPrint(arena, "{s}{s}.san", .{ dir, state })));
        atlas.* = try views.readDds(arena, try read(arena, try std.fmt.allocPrint(arena, "{s}{s}_h.dds", .{ dir, state })));
        for (pack.sprites[0].squares) |square| bounds.add(square);
    }
    const pictures = try arena.alloc(views.Image, states.len);
    for (pictures, packs, atlases) |*picture, pack, atlas| picture.* = try views.unpack(arena, pack.sprites[0], atlas, bounds);
    return .{ .pictures = pictures, .anchor = .{ @floatFromInt(-bounds.min_x), @floatFromInt(-bounds.min_y) } };
}

test "a damage state's model is the intact one cut to its damaged picture at N, its fallen roof rubble, every turn painted and the same every time" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const mesh = views.mesh;
    const model3d = views.model3d;
    // e_wb/01 (111): its state 3 has lost its roof and most of its walls.
    const model = (try mesh.load(arena, std.testing.io, mesh.meshes_dir, 111)).?;
    const found = try statePictures(arena, "Data/Buildings/europe/summer/e_wb/01/", &.{ "1", "3" });
    const intact_picture = found.pictures[0];
    const damaged_picture = found.pictures[1];
    const placement = mesh.place(model, intact_picture, found.anchor).?;
    var m = try model3d.unwrap(arena, model, try mesh.triangles(arena, model), placement);
    try model3d.bake(arena, &m, "1", &.{try model3d.ownBakeView(arena, m, intact_picture, placement, found.anchor)}, null);
    var d = try model3d.damaged(arena, m, placement, damaged_picture, intact_picture, found.anchor);
    try model3d.bake(arena, &d, "3", &.{try model3d.ownBakeView(arena, d, damaged_picture, placement, found.anchor)}, m.texture("1").?);
    // At N the cut model lies on the damaged picture far better than the
    // intact one, its colour within a few levels.
    const size = [2]u32{ damaged_picture.width, damaged_picture.height };
    const n = try model3d.render(arena, d, "3", 0, 0, found.anchor, size);
    const cut = views.artMatch(n.image, damaged_picture, .{ 0, 0 }, false);
    const whole = views.artMatch((try model3d.render(arena, m, "1", 0, 0, found.anchor, size)).image, damaged_picture, .{ 0, 0 }, false);
    try std.testing.expect(cut.iou > 0.84);
    try std.testing.expect(cut.iou > whole.iou + 0.1);
    try std.testing.expect(cut.colour < 6);
    try std.testing.expectEqual(@as(usize, 0), n.unpainted);
    // Its roof fell: the roof's charts are rubble, no face of it stands
    // higher than the rubble.
    var fallen: usize = 0;
    for (d.fallen) |f| fallen += @intFromBool(f);
    try std.testing.expect(fallen > 0);
    var height: f64 = 0;
    for (m.vertices) |v| height = @max(height, v.world[2]);
    for (d.triangles) |t| if (d.fallen[t.chart]) for (t.v) |v| try std.testing.expect(d.vertices[v].world[2] <= 0.1 * height + 1e-9);
    // A quarter turn: no holes, and the same bytes a second time; the
    // shadow is the cut model's, all but a twentieth within the intact one's.
    const turned = [2]u32{ 2 * size[0], 2 * size[1] };
    const anchor = [2]f64{ found.anchor[0] + @as(f64, @floatFromInt(size[0])) / 2, found.anchor[1] + @as(f64, @floatFromInt(size[1])) / 2 };
    const e = try model3d.render(arena, d, "3", 1, 1, anchor, turned);
    try std.testing.expectEqual(@as(usize, 0), e.unpainted);
    const again = try model3d.render(arena, d, "3", 1, 1, anchor, turned);
    try std.testing.expectEqualSlices(u8, std.mem.sliceAsBytes(e.image.pixels), std.mem.sliceAsBytes(again.image.pixels));
    const shadows = [2]views.Image{ try model3d.shadow(arena, m, 1, 1, anchor, turned), try model3d.shadow(arena, d, 1, 1, anchor, turned) };
    var cast: usize = 0;
    var outside: usize = 0;
    for (shadows[0].pixels, shadows[1].pixels) |intact_shadow, cut_shadow| {
        cast += @intFromBool(cut_shadow[3] == 255);
        outside += @intFromBool(cut_shadow[3] == 255 and intact_shadow[3] == 0);
    }
    // A fallen roof's eaves lie flat just beyond the walls.
    if (cast == 0 or outside * 20 > cast) {
        std.debug.print("the cut model's shadow: {d} pixels, {d} outside the intact one's\n", .{ cast, outside });
        return error.ShadowOutside;
    }
    _ = try model3d.writeMod(arena, d);
}
