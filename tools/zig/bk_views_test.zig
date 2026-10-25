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
