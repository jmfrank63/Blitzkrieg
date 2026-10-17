//! The zip writer of the ELK's exports (D-08), in place of the MFC ELK's
//! `zip.exe -9 -R -D`: every file is an entry under its relative name with
//! forward slashes, no folder gets an entry of its own, and the data is
//! deflated at level 9 - or stored, as zip.exe did, when deflating would not
//! make it smaller. The engine's `zip.zig` reads what this writes, CRC check
//! included, and so does `unzip -t`.
//!
//! Changes for the portable stack: the entries are sorted byte-wise by name,
//! where zip.exe took the order the file system listed, and every entry has
//! the same DOS time, where zip.exe took each file's, so the same files
//! always give the same bytes. There are no extra fields (zip.exe wrote NT
//! security descriptors and Unix times) and no Zip64: an archive is limited
//! to 65,535 entries and 4 GiB, far beyond any text pack.
const std = @import("std");
const flate = std.compress.flate;

pub const File = struct {
    /// Relative, forward slashes, as the archive stores it.
    name: []const u8,
    bytes: []const u8,
};

/// The DOS time of every entry: 1980-01-01 00:00:00, the earliest the format
/// holds, in `zip.zig`'s `Entry.dos_modified` layout (time in the low half,
/// date in the high one). The engine only passes the stamp on as a stream's
/// modification time; a fixed one is what makes repeated exports equal.
pub const dos_time: u16 = 0;
pub const dos_date: u16 = (0 << 9) | (1 << 5) | 1;
pub const dos_modified: u32 = @as(u32, dos_time) | (@as(u32, dos_date) << 16);

pub const Error = error{
    /// Empty, absolute, with a backslash, an empty, `.` or `..` component, or
    /// longer than a zip name can be.
    InvalidName,
    /// Two names equal without regard to case, as the game compares them.
    DuplicateName,
    /// More entries or bytes than a zip without Zip64 holds.
    TooLarge,
};

/// The archive of `files`, sorted by name; the caller owns it.
pub fn write(gpa: std.mem.Allocator, files: []const File) ![]u8 {
    if (files.len > std.math.maxInt(u16)) return Error.TooLarge;
    const sorted = try gpa.dupe(File, files);
    defer gpa.free(sorted);
    std.mem.sort(File, sorted, {}, lessThan);
    // The game finds names without regard to case, so two that differ only
    // in case would leave one of them unreachable.
    var seen: std.StringHashMapUnmanaged(void) = .empty;
    defer {
        var keys = seen.keyIterator();
        while (keys.next()) |key| gpa.free(key.*);
        seen.deinit(gpa);
    }
    for (sorted) |file| {
        try checkName(file.name);
        const lower = try std.ascii.allocLowerString(gpa, file.name);
        const slot = try seen.getOrPut(gpa, lower);
        if (slot.found_existing) {
            gpa.free(lower);
            return Error.DuplicateName;
        }
    }

    var out: std.ArrayListUnmanaged(u8) = .empty;
    errdefer out.deinit(gpa);
    var central: std.ArrayListUnmanaged(u8) = .empty;
    defer central.deinit(gpa);
    for (sorted) |file| {
        const deflated = try deflate(gpa, file.bytes);
        defer gpa.free(deflated);
        const stored = deflated.len >= file.bytes.len;
        const record: Record = .{
            .method = if (stored) 0 else 8,
            .crc32 = std.hash.Crc32.hash(file.bytes),
            .compressed_size = try size32(if (stored) file.bytes.len else deflated.len),
            .uncompressed_size = try size32(file.bytes.len),
            .name = file.name,
            .offset = try size32(out.items.len),
        };
        try record.appendLocal(gpa, &out);
        try out.appendSlice(gpa, if (stored) file.bytes else deflated);
        try record.appendCentral(gpa, &central);
    }
    const central_offset = try size32(out.items.len);
    try out.appendSlice(gpa, central.items);
    var end: [22]u8 = @splat(0);
    std.mem.writeInt(u32, end[0..4], 0x06054b50, .little);
    std.mem.writeInt(u16, end[8..10], @intCast(sorted.len), .little);
    std.mem.writeInt(u16, end[10..12], @intCast(sorted.len), .little);
    std.mem.writeInt(u32, end[12..16], try size32(central.items.len), .little);
    std.mem.writeInt(u32, end[16..20], central_offset, .little);
    try out.appendSlice(gpa, &end);
    _ = try size32(out.items.len);
    return out.toOwnedSlice(gpa);
}

fn lessThan(_: void, a: File, b: File) bool {
    return std.mem.lessThan(u8, a.name, b.name);
}

fn checkName(name: []const u8) Error!void {
    if (name.len == 0 or name.len > std.math.maxInt(u16) or name[0] == '/') return Error.InvalidName;
    if (std.mem.indexOfScalar(u8, name, '\\') != null) return Error.InvalidName;
    var components = std.mem.splitScalar(u8, name, '/');
    while (components.next()) |component| {
        if (component.len == 0 or std.mem.eql(u8, component, ".") or std.mem.eql(u8, component, "..")) return Error.InvalidName;
    }
}

fn size32(value: usize) Error!u32 {
    return std.math.cast(u32, value) orelse Error.TooLarge;
}

/// Raw deflate at level 9; the caller owns the bytes.
fn deflate(gpa: std.mem.Allocator, bytes: []const u8) ![]u8 {
    var output: std.Io.Writer.Allocating = try .initCapacity(gpa, @max(64, bytes.len / 2));
    defer output.deinit();
    // The compressor's hash chains are far too big for the stack.
    const window = try gpa.alloc(u8, flate.max_window_len);
    defer gpa.free(window);
    const compress = try gpa.create(flate.Compress);
    defer gpa.destroy(compress);
    compress.* = try .init(&output.writer, window, .raw, .level_9);
    try compress.writer.writeAll(bytes);
    try compress.finish();
    return output.toOwnedSlice();
}

const Record = struct {
    method: u16,
    crc32: u32,
    compressed_size: u32,
    uncompressed_size: u32,
    name: []const u8,
    offset: u32,

    /// 1.0 suffices to extract a stored entry, 2.0 a deflated one.
    fn versionNeeded(self: Record) u16 {
        return if (self.method == 8) 20 else 10;
    }

    fn appendLocal(self: Record, gpa: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8)) !void {
        var header: [30]u8 = @splat(0);
        std.mem.writeInt(u32, header[0..4], 0x04034b50, .little);
        std.mem.writeInt(u16, header[4..6], self.versionNeeded(), .little);
        std.mem.writeInt(u16, header[8..10], self.method, .little);
        std.mem.writeInt(u16, header[10..12], dos_time, .little);
        std.mem.writeInt(u16, header[12..14], dos_date, .little);
        std.mem.writeInt(u32, header[14..18], self.crc32, .little);
        std.mem.writeInt(u32, header[18..22], self.compressed_size, .little);
        std.mem.writeInt(u32, header[22..26], self.uncompressed_size, .little);
        std.mem.writeInt(u16, header[26..28], @intCast(self.name.len), .little);
        try out.appendSlice(gpa, &header);
        try out.appendSlice(gpa, self.name);
    }

    fn appendCentral(self: Record, gpa: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8)) !void {
        var header: [46]u8 = @splat(0);
        std.mem.writeInt(u32, header[0..4], 0x02014b50, .little);
        // Made by MS-DOS (host 0) with 2.0, as no Unix permissions are kept.
        std.mem.writeInt(u16, header[4..6], 20, .little);
        std.mem.writeInt(u16, header[6..8], self.versionNeeded(), .little);
        std.mem.writeInt(u16, header[10..12], self.method, .little);
        std.mem.writeInt(u16, header[12..14], dos_time, .little);
        std.mem.writeInt(u16, header[14..16], dos_date, .little);
        std.mem.writeInt(u32, header[16..20], self.crc32, .little);
        std.mem.writeInt(u32, header[20..24], self.compressed_size, .little);
        std.mem.writeInt(u32, header[24..28], self.uncompressed_size, .little);
        std.mem.writeInt(u16, header[28..30], @intCast(self.name.len), .little);
        std.mem.writeInt(u32, header[42..46], self.offset, .little);
        try out.appendSlice(gpa, &header);
        try out.appendSlice(gpa, self.name);
    }
};

const testing = std.testing;
const zip = @import("zip");

test "the engine's zip reader reads every entry back, sorted, CRC checked, at the fixed time" {
    var repeated: [4096]u8 = undefined;
    for (&repeated, 0..) |*byte, at| byte.* = "ELK text "[at % 9];
    const files = [_]File{
        .{ .name = "units/tank/name.txt", .bytes = &repeated },
        .{ .name = "fonts/tiny/1.tfd", .bytes = "tfd" },
        .{ .name = "a.txt", .bytes = "" },
        .{ .name = "fonts/tiny/1_c.dds", .bytes = "\x00\x01\x02\x03" },
    };
    const bytes = try write(testing.allocator, &files);
    defer testing.allocator.free(bytes);
    var archive = try zip.Archive.parse(testing.allocator, bytes);
    defer archive.deinit();
    const order = [_]usize{ 2, 1, 3, 0 };
    try testing.expectEqual(order.len, archive.entries.len);
    for (order, archive.entries) |index, *entry| {
        try testing.expectEqualStrings(files[index].name, entry.name);
        try testing.expectEqual(std.hash.Crc32.hash(files[index].bytes), entry.crc32);
        try testing.expectEqual(dos_modified, entry.dos_modified);
        const extracted = try archive.extract(testing.allocator, entry);
        defer testing.allocator.free(extracted);
        try testing.expectEqualSlices(u8, files[index].bytes, extracted);
    }
    // Deflate where it helps, stored where it does not.
    try testing.expectEqual(@as(u16, 8), archive.find("units/tank/name.txt").?.method);
    try testing.expect(archive.find("units/tank/name.txt").?.compressed_size < repeated.len);
    try testing.expectEqual(@as(u16, 0), archive.find("fonts/tiny/1.tfd").?.method);
    // The same files in another order give the same bytes.
    const reversed = [_]File{ files[3], files[2], files[1], files[0] };
    const again = try write(testing.allocator, &reversed);
    defer testing.allocator.free(again);
    try testing.expectEqualSlices(u8, bytes, again);
}

test "names the game could not find are refused" {
    const bad = [_][]const u8{ "", "/a.txt", "a\\b.txt", "a//b.txt", "a/", "./a.txt", "a/../b.txt" };
    for (bad) |name| try testing.expectError(Error.InvalidName, write(testing.allocator, &.{.{ .name = name, .bytes = "x" }}));
    try testing.expectError(Error.DuplicateName, write(testing.allocator, &.{ .{ .name = "a.txt", .bytes = "1" }, .{ .name = "A.TXT", .bytes = "2" } }));
}
