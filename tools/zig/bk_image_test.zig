//! Tests for `bk-image`; the command-line contract (exit codes, streams) is in
//! `bk_tool_conventions_test.zig`.
//!
//! `offset-romb` is judged by explicit goldens (applying it twice is not the
//! identity, so a round trip proves nothing). The goldens in
//! `fixtures/small-tools/offset-romb` were made by compiling OffsetRomb's own
//! `FlipRomb1`/`FlipRomb2` source (`Sources/src/Tools/OffsetRomb/main.cpp`,
//! included verbatim) over the fixture pixels, with one line added to its
//! `FlipLines`: skip a swap that reaches outside the image, the rule
//! `bk_image.zig` documents. The fixtures are a 16x16 tile (D-29) and a 64x32
//! one, every pixel distinct, stored bottom-up without a footer; the goldens
//! are in `SaveImageAsTGA`'s layout.

const std = @import("std");
const bk_image = @import("bk_image.zig");
const bk_tools = @import("bk_tools");

const tile16 = @embedFile("fixtures/small-tools/offset-romb/tile16.tga");
const tile16_golden = @embedFile("fixtures/small-tools/offset-romb/tile16.golden.tga");
const tile64x32 = @embedFile("fixtures/small-tools/offset-romb/tile64x32.tga");
const tile64x32_golden = @embedFile("fixtures/small-tools/offset-romb/tile64x32.golden.tga");

fn offsetRomb(gpa: std.mem.Allocator, bytes: []const u8) ![]u8 {
    var image = try bk_image.readTga(gpa, bytes);
    defer gpa.free(image.pixels);
    try bk_image.offsetRomb(&image);
    return bk_image.writeTga(gpa, image);
}

test "offset-romb matches the legacy code's golden on both fixture tiles" {
    const gpa = std.testing.allocator;
    for ([_][2][]const u8{ .{ tile16, tile16_golden }, .{ tile64x32, tile64x32_golden } }) |pair| {
        const out = try offsetRomb(gpa, pair[0]);
        defer gpa.free(out);
        try std.testing.expectEqualSlices(u8, pair[1], out);
    }
}

test "offset-romb moves pixels and twice is not the identity" {
    const gpa = std.testing.allocator;
    const once = try offsetRomb(gpa, tile16);
    defer gpa.free(once);
    const twice = try offsetRomb(gpa, once);
    defer gpa.free(twice);
    const original = try bk_image.readTga(gpa, tile16);
    defer gpa.free(original.pixels);
    const back = try bk_image.writeTga(gpa, original);
    defer gpa.free(back);
    try std.testing.expect(!std.mem.eql(u8, back, once));
    try std.testing.expect(!std.mem.eql(u8, back, twice));
}

test "a size whose lines never close is refused, as the legacy exe hung on it" {
    const gpa = std.testing.allocator;
    const pixels = try gpa.alloc([4]u8, 16);
    defer gpa.free(pixels);
    @memset(pixels, .{ 1, 2, 3, 4 });
    var image: bk_image.Image = .{ .width = 4, .height = 4, .pixels = pixels };
    try std.testing.expectError(error.NeverCloses, bk_image.offsetRomb(&image));
}

test "the TGA reader takes what the engine's reads and writes what it writes" {
    const gpa = std.testing.allocator;
    // A golden is in SaveImageAsTGA's layout: reading and writing it back is
    // the identity.
    {
        const image = try bk_image.readTga(gpa, tile16_golden);
        defer gpa.free(image.pixels);
        const out = try bk_image.writeTga(gpa, image);
        defer gpa.free(out);
        try std.testing.expectEqualSlices(u8, tile16_golden, out);
    }
    // 2x2, 24-bit, bottom-up, with a 3-byte image ID: B G R per pixel.
    {
        const bytes = [_]u8{ 3, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 24, 0, 'i', 'd', '!' } ++
            [_]u8{ 1, 2, 3, 4, 5, 6 } ++ [_]u8{ 7, 8, 9, 10, 11, 12 };
        const image = try bk_image.readTga(gpa, &bytes);
        defer gpa.free(image.pixels);
        try std.testing.expectEqualSlices([4]u8, &.{ .{ 7, 8, 9, 255 }, .{ 10, 11, 12, 255 }, .{ 1, 2, 3, 255 }, .{ 4, 5, 6, 255 } }, image.pixels);
    }
    // 3x1, RLE 32-bit, top-down: a run of two, then one raw pixel.
    {
        const bytes = [_]u8{ 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1, 0, 32, 0x28 } ++
            [_]u8{ 0x81, 1, 2, 3, 4 } ++ [_]u8{ 0x00, 5, 6, 7, 8 };
        const image = try bk_image.readTga(gpa, &bytes);
        defer gpa.free(image.pixels);
        try std.testing.expectEqualSlices([4]u8, &.{ .{ 1, 2, 3, 4 }, .{ 1, 2, 3, 4 }, .{ 5, 6, 7, 8 } }, image.pixels);
    }
    // 2x1, 8-bit grey.
    {
        const bytes = [_]u8{ 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 8, 0x20, 9, 200 };
        const image = try bk_image.readTga(gpa, &bytes);
        defer gpa.free(image.pixels);
        try std.testing.expectEqualSlices([4]u8, &.{ .{ 9, 9, 9, 255 }, .{ 200, 200, 200, 255 } }, image.pixels);
    }
    // Colour-mapped, 16-bit and cut-short files are refused.
    try std.testing.expectError(error.Unsupported, bk_image.readTga(gpa, &[_]u8{ 0, 1, 1, 0, 0, 0, 0, 24, 0, 0, 0, 0, 1, 0, 1, 0, 8, 0 }));
    try std.testing.expectError(error.Unsupported, bk_image.readTga(gpa, &[_]u8{ 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 16, 0, 0, 0 }));
    try std.testing.expectError(error.Truncated, bk_image.readTga(gpa, tile16[0 .. tile16.len - 1]));
    try std.testing.expectError(error.Truncated, bk_image.readTga(gpa, "TGA"));
}

// The built executable on the fixture, in place and with --out, in a
// temporary directory: the files it writes are the goldens.
test "the bk-image executable writes the golden" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.createDirPath(io, "tiles");
    try tmp.dir.writeFile(io, .{ .sub_path = "tiles/tile.tga", .data = tile64x32 });
    const exe = try std.Io.Dir.cwd().realPathFileAlloc(io, bk_tools.bk_image, gpa);
    defer gpa.free(exe);
    for ([_][]const []const u8{
        &.{ exe, "offset-romb", "tiles\\tile.tga", "--out", "out/new.tga" },
        &.{ exe, "offset-romb", "tiles/tile.tga" },
    }, [_][]const u8{ "out/new.tga", "tiles/tile.tga" }) |argv, written| {
        const result = try std.process.run(gpa, io, .{ .argv = argv, .cwd = .{ .dir = tmp.dir } });
        defer gpa.free(result.stdout);
        defer gpa.free(result.stderr);
        try std.testing.expectEqualStrings("", result.stderr);
        try std.testing.expect(result.term == .exited and result.term.exited == 0);
        const data = try tmp.dir.readFileAlloc(io, written, gpa, .unlimited);
        defer gpa.free(data);
        try std.testing.expectEqualSlices(u8, tile64x32_golden, data);
    }
}
