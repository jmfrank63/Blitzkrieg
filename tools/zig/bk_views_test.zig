//! Tests for `bk-views`; the command-line contract is in
//! `bk_tool_conventions_test.zig`, and the proof that the engine reads the
//! packs the same way is `bk-views-roundtrip` (tools/zig/bk_views_check.cpp).
//! The buildings are tracked data, read from Data/ in place; nothing is
//! written there.

const std = @import("std");
const views = @import("bk_views.zig");
const bk_tools = @import("bk_tools");

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

/// Tracked text is checked out with CRLF; the tool writes LF.
fn withoutCr(gpa: std.mem.Allocator, text: []const u8) ![]u8 {
    var out: std.ArrayList(u8) = .empty;
    for (text) |c| if (c != '\r') try out.append(gpa, c);
    return out.items;
}
