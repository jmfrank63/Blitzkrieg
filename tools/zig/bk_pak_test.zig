//! Tests for `bk-pak`'s archive work; the command-line contract (exit codes,
//! streams) is in `bk_tool_conventions_test.zig`.
//!
//! What is proved here, each against bytes rather than by inspection:
//!
//! - `pack` writes exactly what the release packager writes for the same tree.
//! - `unpack` reproduces the tree, reads deflate as well as stored entries, and
//!   refuses to overwrite without `force`.
//! - A hostile or damaged archive (`..`, absolute names, drive letters, a bad
//!   CRC, a size that disagrees, a duplicate name, Zip64) is refused by
//!   `verify` and by `unpack`, and `unpack` then leaves no output at all.
//!   Hostile archives are built byte by byte here, since no well-behaved zip
//!   writer would produce them.

const std = @import("std");
const builtin = @import("builtin");
const bk_pak = @import("bk_pak.zig");
const package = @import("package.zig");

fn writeFile(io: std.Io, dir: std.Io.Dir, sub_path: []const u8, data: []const u8) !void {
    if (std.fs.path.dirname(sub_path)) |parent| try dir.createDirPath(io, parent);
    var file = try dir.createFile(io, sub_path, .{ .truncate = true });
    defer file.close(io);
    try file.writeStreamingAll(io, data);
}

fn readAll(io: std.Io, dir: std.Io.Dir, sub_path: []const u8, gpa: std.mem.Allocator) ![]u8 {
    var file = try dir.openFile(io, sub_path, .{});
    defer file.close(io);
    var buffer: [4096]u8 = undefined;
    var reader = file.reader(io, &buffer);
    return reader.interface.allocRemaining(gpa, .unlimited);
}

/// A small mod-shaped tree: nested folders, a UTF-8 name, an empty file.
const fixture_files = [_]struct { path: []const u8, data: []const u8 }{
    .{ .path = "tree/units/tank/1.xml", .data = "<RPG><Armor>12</Armor></RPG>\r\n" },
    .{ .path = "tree/units/tank/1.tga", .data = "\x00\x00\x02\x00binary\xff\xfe" },
    .{ .path = "tree/Text/m\xc3\xa4p.txt", .data = "Gr\xc3\xbc\xc3\x9fe\n" },
    .{ .path = "tree/empty.txt", .data = "" },
};

fn writeFixture(io: std.Io, dir: std.Io.Dir) !void {
    for (fixture_files) |file| try writeFile(io, dir, file.path, file.data);
}

/// Absolute paths under the temporary directory: the tool works on paths
/// relative to the process cwd, which a test cannot move.
const Paths = struct {
    gpa: std.mem.Allocator,
    root: [:0]u8,

    fn init(io: std.Io, gpa: std.mem.Allocator, dir: std.Io.Dir) !Paths {
        return .{ .gpa = gpa, .root = try dir.realPathFileAlloc(io, ".", gpa) };
    }

    fn deinit(paths: Paths) void {
        paths.gpa.free(paths.root);
    }

    fn get(paths: Paths, sub_path: []const u8) ![]u8 {
        return std.fs.path.join(paths.gpa, &.{ paths.root, sub_path });
    }
};

/// One entry of a hand-made archive. The overrides write a value that
/// disagrees with the data, which is the point of a hostile archive.
const TestEntry = struct {
    name: []const u8,
    data: []const u8,
    deflate: bool = false,
    crc_override: ?u32 = null,
    local_size_override: ?u32 = null,
};

/// Build a zip in memory. Deflate entries use deflate's own "stored block"
/// (BTYPE 00): a valid deflate stream that needs no compressor.
fn buildZip(gpa: std.mem.Allocator, entries: []const TestEntry, count_override: ?u16) ![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    defer out.deinit();
    const w = &out.writer;
    var offsets: [16]u32 = undefined;
    var stored_sizes: [16]u32 = undefined;

    for (entries, 0..) |entry, i| {
        offsets[i] = @intCast(out.written().len);
        var body: std.Io.Writer.Allocating = .init(gpa);
        defer body.deinit();
        if (entry.deflate) {
            try body.writer.writeByte(0x01);
            try body.writer.writeInt(u16, @intCast(entry.data.len), .little);
            try body.writer.writeInt(u16, ~@as(u16, @intCast(entry.data.len)), .little);
        }
        try body.writer.writeAll(entry.data);
        stored_sizes[i] = @intCast(body.written().len);

        const crc = entry.crc_override orelse std.hash.Crc32.hash(entry.data);
        try w.writeAll("PK\x03\x04");
        try w.writeInt(u16, 20, .little);
        try w.writeInt(u16, 0, .little);
        try w.writeInt(u16, if (entry.deflate) 8 else 0, .little);
        try w.writeInt(u32, 0, .little);
        try w.writeInt(u32, crc, .little);
        try w.writeInt(u32, stored_sizes[i], .little);
        try w.writeInt(u32, entry.local_size_override orelse @as(u32, @intCast(entry.data.len)), .little);
        try w.writeInt(u16, @intCast(entry.name.len), .little);
        try w.writeInt(u16, 0, .little);
        try w.writeAll(entry.name);
        try w.writeAll(body.written());
    }

    const cd_offset: u32 = @intCast(out.written().len);
    for (entries, 0..) |entry, i| {
        const crc = entry.crc_override orelse std.hash.Crc32.hash(entry.data);
        try w.writeAll("PK\x01\x02");
        try w.writeInt(u16, 20, .little);
        try w.writeInt(u16, 20, .little);
        try w.writeInt(u16, 0, .little);
        try w.writeInt(u16, if (entry.deflate) 8 else 0, .little);
        try w.writeInt(u32, 0, .little);
        try w.writeInt(u32, crc, .little);
        try w.writeInt(u32, stored_sizes[i], .little);
        try w.writeInt(u32, @intCast(entry.data.len), .little);
        try w.writeInt(u16, @intCast(entry.name.len), .little);
        try w.writeInt(u16, 0, .little);
        try w.writeInt(u16, 0, .little);
        try w.writeInt(u16, 0, .little);
        try w.writeInt(u16, 0, .little);
        try w.writeInt(u32, 0, .little);
        try w.writeInt(u32, offsets[i], .little);
        try w.writeAll(entry.name);
    }
    const cd_size: u32 = @as(u32, @intCast(out.written().len)) - cd_offset;

    const count = count_override orelse @as(u16, @intCast(entries.len));
    try w.writeAll("PK\x05\x06");
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, count, .little);
    try w.writeInt(u16, count, .little);
    try w.writeInt(u32, cd_size, .little);
    try w.writeInt(u32, cd_offset, .little);
    try w.writeInt(u16, 0, .little);
    return out.toOwnedSlice();
}

/// Write `entries` as `name` under the temporary directory.
fn writeZip(io: std.Io, gpa: std.mem.Allocator, dir: std.Io.Dir, name: []const u8, entries: []const TestEntry, count_override: ?u16) !void {
    const bytes = try buildZip(gpa, entries, count_override);
    defer gpa.free(bytes);
    try writeFile(io, dir, name, bytes);
}

test "pack writes the release packager's bytes for the same tree" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);
    const paths = try Paths.init(io, gpa, tmp.dir);
    defer paths.deinit();
    const tree = try paths.get("tree");
    defer gpa.free(tree);
    const release = try paths.get("release.zip");
    defer gpa.free(release);
    const pak = try paths.get("out/mod.pak");
    defer gpa.free(pak);

    try package.createPackage(io, gpa, tree, release);
    var problem: bk_pak.Problem = .{};
    const totals = try bk_pak.pack(io, gpa, tree, pak, false, &problem);
    try std.testing.expectEqual(@as(u64, fixture_files.len), totals.files);

    const release_bytes = try readAll(io, tmp.dir, "release.zip", gpa);
    defer gpa.free(release_bytes);
    const pak_bytes = try readAll(io, tmp.dir, "out/mod.pak", gpa);
    defer gpa.free(pak_bytes);
    try std.testing.expectEqualSlices(u8, release_bytes, pak_bytes);
}

test "unpack reproduces the packed tree and list shows it sorted" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);
    const paths = try Paths.init(io, gpa, tmp.dir);
    defer paths.deinit();
    const tree = try paths.get("tree");
    defer gpa.free(tree);
    const pak = try paths.get("mod.pak");
    defer gpa.free(pak);
    const out = try paths.get("unpacked");
    defer gpa.free(out);

    var problem: bk_pak.Problem = .{};
    _ = try bk_pak.pack(io, gpa, tree, pak, false, &problem);
    const totals = try bk_pak.unpack(io, gpa, pak, out, false, &problem);
    try std.testing.expectEqual(@as(u64, fixture_files.len), totals.files);

    for (fixture_files) |file| {
        const unpacked_path = try std.mem.concat(gpa, u8, &.{ "unpacked", file.path["tree".len..] });
        defer gpa.free(unpacked_path);
        const bytes = try readAll(io, tmp.dir, unpacked_path, gpa);
        defer gpa.free(bytes);
        try std.testing.expectEqualSlices(u8, file.data, bytes);
    }

    var entries = try bk_pak.scan(io, gpa, pak, .names_only, &problem);
    defer entries.deinit(gpa);
    const expected = [_][]const u8{ "Text/m\xc3\xa4p.txt", "empty.txt", "units/tank/1.tga", "units/tank/1.xml" };
    try std.testing.expectEqual(expected.len, entries.items.len);
    for (expected, entries.items) |name, entry| {
        try std.testing.expectEqualStrings(name, entry.name);
        try std.testing.expectEqual(bk_pak.Method.stored, entry.method);
    }
}

test "deflate entries and directory entries are read" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const paths = try Paths.init(io, gpa, tmp.dir);
    defer paths.deinit();
    try writeZip(io, gpa, tmp.dir, "deflate.pak", &.{
        .{ .name = "maps/", .data = "" },
        .{ .name = "maps/one.xml", .data = "<map/>\n", .deflate = true },
        .{ .name = "legacy\\backslash.txt", .data = "old zip.exe on Windows\n" },
    }, null);
    const pak = try paths.get("deflate.pak");
    defer gpa.free(pak);
    const out = try paths.get("out");
    defer gpa.free(out);

    var problem: bk_pak.Problem = .{};
    const totals = try bk_pak.unpack(io, gpa, pak, out, false, &problem);
    try std.testing.expectEqual(@as(u64, 2), totals.files);
    const map = try readAll(io, tmp.dir, "out/maps/one.xml", gpa);
    defer gpa.free(map);
    try std.testing.expectEqualStrings("<map/>\n", map);
    // A backslash in a legacy archive is a separator, not part of a name.
    const legacy = try readAll(io, tmp.dir, "out/legacy/backslash.txt", gpa);
    defer gpa.free(legacy);
    try std.testing.expectEqualStrings("old zip.exe on Windows\n", legacy);
}

/// `verify` and `unpack` both refuse `entries` with `expected`, and the
/// refused `unpack` leaves no output directory behind.
fn expectRefused(entries: []const TestEntry, count_override: ?u16, expected: anyerror) !void {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const paths = try Paths.init(io, gpa, tmp.dir);
    defer paths.deinit();
    // The good entry comes first, so a writer that did not check the whole
    // archive up front would already have written it.
    var with_good: [8]TestEntry = undefined;
    with_good[0] = .{ .name = "good.txt", .data = "fine\n" };
    @memcpy(with_good[1..][0..entries.len], entries);
    try writeZip(io, gpa, tmp.dir, "hostile.pak", with_good[0 .. entries.len + 1], count_override);
    const pak = try paths.get("hostile.pak");
    defer gpa.free(pak);
    const out = try paths.get("out");
    defer gpa.free(out);

    var problem: bk_pak.Problem = .{};
    try std.testing.expectError(expected, bk_pak.scan(io, gpa, pak, .check_data, &problem));
    try std.testing.expectError(expected, bk_pak.unpack(io, gpa, pak, out, false, &problem));
    try std.testing.expectError(error.FileNotFound, tmp.dir.statFile(io, "out", .{}));
}

test "names that escape the output directory are refused without writing" {
    const hostile = [_][]const u8{
        "../escape.txt",
        "maps/../../escape.txt",
        "/etc/escape.txt",
        "C:/Windows/escape.txt",
        "C:escape.txt",
        "file.txt:stream",
        "..\\..\\escape.txt",
        "\\absolute.txt",
    };
    for (hostile) |name| {
        expectRefused(&.{.{ .name = name, .data = "x" }}, null, error.UnsafeEntryName) catch |err| {
            std.debug.print("hostile name not refused: {s}\n", .{name});
            return err;
        };
    }
}

test "a name that is not UTF-8 is refused" {
    try expectRefused(&.{.{ .name = "caf\xe9.txt", .data = "x" }}, null, error.InvalidEntryName);
}

test "data that disagrees with its CRC or size is refused without writing" {
    try expectRefused(&.{.{ .name = "bad-crc.txt", .data = "payload", .crc_override = 0xdeadbeef }}, null, error.ChecksumMismatch);
    try expectRefused(&.{.{ .name = "bad-crc.xml", .data = "payload", .deflate = true, .crc_override = 1 }}, null, error.ChecksumMismatch);
    try expectRefused(&.{.{ .name = "bad-size.txt", .data = "payload", .local_size_override = 3 }}, null, error.ZipMismatchUncompLen);
}

test "a duplicate name is refused" {
    try expectRefused(&.{.{ .name = "good.txt", .data = "other\n" }}, null, error.DuplicateEntry);
}

test "a Zip64 archive fails loudly" {
    // 0xFFFF in the entry count is the Zip64 marker; the archive's real count
    // does not matter because the refusal comes first.
    try expectRefused(&.{}, 0xFFFF, error.Zip64Unsupported);
}

test "unpack refuses to overwrite unless forced" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const paths = try Paths.init(io, gpa, tmp.dir);
    defer paths.deinit();
    try writeZip(io, gpa, tmp.dir, "mod.pak", &.{
        .{ .name = "a.txt", .data = "new a\n" },
        .{ .name = "b.txt", .data = "new b\n" },
    }, null);
    try writeFile(io, tmp.dir, "out/b.txt", "old b\n");
    const pak = try paths.get("mod.pak");
    defer gpa.free(pak);
    const out = try paths.get("out");
    defer gpa.free(out);

    var problem: bk_pak.Problem = .{};
    try std.testing.expectError(error.OutputExists, bk_pak.unpack(io, gpa, pak, out, false, &problem));
    try std.testing.expect(std.mem.endsWith(u8, problem.name(), std.fs.path.sep_str ++ "out" ++ std.fs.path.sep_str ++ "b.txt"));
    // The conflict is found before anything is written: a.txt is not there.
    try std.testing.expectError(error.FileNotFound, tmp.dir.statFile(io, "out/a.txt", .{}));

    _ = try bk_pak.unpack(io, gpa, pak, out, true, &problem);
    const b = try readAll(io, tmp.dir, "out/b.txt", gpa);
    defer gpa.free(b);
    try std.testing.expectEqualStrings("new b\n", b);
}

test "pack keeps an existing output unless forced and never packs into its source" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);
    try writeFile(io, tmp.dir, "existing.pak", "keep me");
    const paths = try Paths.init(io, gpa, tmp.dir);
    defer paths.deinit();
    const tree = try paths.get("tree");
    defer gpa.free(tree);
    const existing = try paths.get("existing.pak");
    defer gpa.free(existing);
    const inside = try paths.get("tree/units/../self.pak");
    defer gpa.free(inside);

    var problem: bk_pak.Problem = .{};
    try std.testing.expectError(error.OutputExists, bk_pak.pack(io, gpa, tree, existing, false, &problem));
    const kept = try readAll(io, tmp.dir, "existing.pak", gpa);
    defer gpa.free(kept);
    try std.testing.expectEqualStrings("keep me", kept);

    try std.testing.expectError(error.OutputInsideSource, bk_pak.pack(io, gpa, tree, inside, true, &problem));
    try std.testing.expectError(error.FileNotFound, tmp.dir.statFile(io, "tree/self.pak", .{}));

    _ = try bk_pak.pack(io, gpa, tree, existing, true, &problem);
    var entries = try bk_pak.scan(io, gpa, existing, .check_data, &problem);
    defer entries.deinit(gpa);
    try std.testing.expectEqual(@as(usize, fixture_files.len), entries.items.len);
}

test "pack fails loudly past 65,535 files and leaves no archive" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.createDirPath(io, "many");
    var many = try tmp.dir.openDir(io, "many", .{});
    defer many.close(io);
    var name_buffer: [16]u8 = undefined;
    for (0..std.math.maxInt(u16) + 1) |i| {
        const name = try std.fmt.bufPrint(&name_buffer, "{x:0>5}", .{i});
        var file = try many.createFile(io, name, .{});
        file.close(io);
    }
    const paths = try Paths.init(io, gpa, tmp.dir);
    defer paths.deinit();
    const tree = try paths.get("many");
    defer gpa.free(tree);
    const pak = try paths.get("many.pak");
    defer gpa.free(pak);

    var problem: bk_pak.Problem = .{};
    try std.testing.expectError(error.TooManyEntries, bk_pak.pack(io, gpa, tree, pak, false, &problem));
    try std.testing.expectError(error.FileNotFound, tmp.dir.statFile(io, "many.pak", .{}));
}
