const std = @import("std");
const streamio = @import("streamio");

test "host filesystem fixture covers names, directories, and deterministic ordering" {
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    const io = std.testing.io;
    try tmp.dir.createDirPath(io, "nested");
    for ([_][]const u8{ "zeta.txt", "Alpha.TXT", "тест.txt", ".hidden" }) |name| {
        const file = try tmp.dir.createFile(io, name, .{});
        file.close(io);
    }
    const nested = try tmp.dir.createFile(io, "nested/child.bin", .{});
    nested.close(io);

    var names: std.ArrayListUnmanaged([]const u8) = .empty;
    defer names.deinit(std.testing.allocator);
    var iterator = tmp.dir.iterate();
    while (try iterator.next(io)) |entry| {
        try names.append(std.testing.allocator, try std.testing.allocator.dupe(u8, entry.name));
        if (entry.kind == .directory) try std.testing.expectEqualStrings("nested", entry.name);
    }
    defer for (names.items) |name| std.testing.allocator.free(name);
    std.mem.sort([]const u8, names.items, {}, struct {
        fn lessThan(_: void, lhs: []const u8, rhs: []const u8) bool {
            return std.mem.order(u8, lhs, rhs) == .lt;
        }
    }.lessThan);
    try std.testing.expectEqualStrings(".hidden", names.items[0]);
    try std.testing.expectEqualStrings("Alpha.TXT", names.items[1]);
    try std.testing.expectEqualStrings("nested", names.items[2]);
    try std.testing.expectEqualStrings("тест.txt", names.items[4]);
}

test "wildcards and host case policy are explicit" {
    try std.testing.expect(streamio.streamio_test_wildcard_match("*.pak", "DATA.PAK", true));
    try std.testing.expect(!streamio.streamio_test_wildcard_match("*.pak", "DATA.PAK", false));
    try std.testing.expect(streamio.streamio_test_wildcard_match("a?c*", "Abcdef", true));
    try std.testing.expect(!streamio.streamio_test_wildcard_match("a?c*", "abdef", true));
}

test "DOS timestamps clamp and preserve date/time fields" {
    const minimum = streamio.streamio_test_dos_timestamp(-2208988800);
    const maximum = streamio.streamio_test_dos_timestamp(5000000000);
    const known = streamio.streamio_test_dos_timestamp(946684799);
    try std.testing.expectEqual(@as(u16, 33), @as(u16, @truncate(minimum >> 16)));
    const max_date: u16 = @intCast(((2107 - 1980) << 9) | (12 << 5) | 31);
    try std.testing.expectEqual(max_date, @as(u16, @truncate(maximum >> 16)));
    try std.testing.expect((known >> 16) != 0 and (known & 0xffff) != 0);
}

test "a written stream lands in the mixed-case directory its lowercase name means" {
    if (@import("builtin").os.tag == .windows) return;
    // The minimap generator writes "maps\ussr\stalingrad\stalingrad_u.dds",
    // lowercased, into Data/Maps/USSR/Stalingrad. A new file there has no
    // spelling to resolve against, and an existing one must still be replaced
    // whole rather than overwritten from the start.
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.createDirPath(std.testing.io, "Maps/USSR/Stalingrad");
    try tmp.dir.writeFile(std.testing.io, .{ .sub_path = "Maps/USSR/Stalingrad/stalingrad_h.dds", .data = "the old, longer picture" });

    const base = try std.fmt.allocPrintSentinel(std.testing.allocator, ".zig-cache/tmp/{s}/maps\\ussr\\stalingrad\\", .{tmp.sub_path}, 0);
    defer std.testing.allocator.free(base);
    const handle = streamio.bk_storage_create(base.ptr, 2, 0) orelse return error.TestUnexpectedResult;
    defer streamio.bk_storage_destroy(handle);
    for ([_][*:0]const u8{ "stalingrad_u.dds", "stalingrad_h.dds" }) |name| {
        const stream = streamio.bk_storage_create_stream(handle, name, 2) orelse return error.TestUnexpectedResult;
        try std.testing.expectEqual(@as(c_int, 3), streamio.bk_stream_write(stream, "new", 3));
        streamio.bk_stream_destroy(stream);
    }

    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("new", try tmp.dir.readFile(std.testing.io, "Maps/USSR/Stalingrad/stalingrad_u.dds", &buffer));
    try std.testing.expectEqualStrings("new", try tmp.dir.readFile(std.testing.io, "Maps/USSR/Stalingrad/stalingrad_h.dds", &buffer));
}
