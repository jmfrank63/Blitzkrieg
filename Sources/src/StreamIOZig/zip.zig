const std = @import("std");

pub const Error = error{
    InvalidArchive,
    UnsupportedCompression,
    SizeMismatch,
    ChecksumMismatch,
};

pub const Entry = struct {
    name: []const u8,
    method: u16,
    crc32: u32,
    compressed_size: u32,
    uncompressed_size: u32,
    dos_modified: u32,
    local_header_offset: u32,
};

pub const Archive = struct {
    bytes: []const u8,
    entries: []Entry,
    allocator: std.mem.Allocator,

    pub fn parse(allocator: std.mem.Allocator, bytes: []const u8) !Archive {
        const eocd = findEndRecord(bytes) orelse return Error.InvalidArchive;
        const entry_count = readU16(bytes, eocd + 10) orelse return Error.InvalidArchive;
        const central_size = readU32(bytes, eocd + 12) orelse return Error.InvalidArchive;
        const central_offset = readU32(bytes, eocd + 16) orelse return Error.InvalidArchive;
        if (@as(u64, central_offset) + central_size > bytes.len) return Error.InvalidArchive;

        const entries = try allocator.alloc(Entry, entry_count);
        errdefer allocator.free(entries);
        var cursor: usize = central_offset;
        for (entries) |*entry| {
            if (readU32(bytes, cursor) != 0x02014b50) return Error.InvalidArchive;
            const name_len = readU16(bytes, cursor + 28) orelse return Error.InvalidArchive;
            const extra_len = readU16(bytes, cursor + 30) orelse return Error.InvalidArchive;
            const comment_len = readU16(bytes, cursor + 32) orelse return Error.InvalidArchive;
            const record_len = 46 + @as(usize, name_len) + extra_len + comment_len;
            if (cursor + record_len > bytes.len) return Error.InvalidArchive;
            const time = readU16(bytes, cursor + 12) orelse return Error.InvalidArchive;
            const date = readU16(bytes, cursor + 14) orelse return Error.InvalidArchive;
            entry.* = .{
                .name = bytes[cursor + 46 .. cursor + 46 + name_len],
                .method = readU16(bytes, cursor + 10) orelse return Error.InvalidArchive,
                .crc32 = readU32(bytes, cursor + 16) orelse return Error.InvalidArchive,
                .compressed_size = readU32(bytes, cursor + 20) orelse return Error.InvalidArchive,
                .uncompressed_size = readU32(bytes, cursor + 24) orelse return Error.InvalidArchive,
                .dos_modified = @as(u32, time) | (@as(u32, date) << 16),
                .local_header_offset = readU32(bytes, cursor + 42) orelse return Error.InvalidArchive,
            };
            cursor += record_len;
        }
        return .{ .bytes = bytes, .entries = entries, .allocator = allocator };
    }

    pub fn deinit(self: *Archive) void {
        self.allocator.free(self.entries);
        self.* = undefined;
    }

    pub fn find(self: *const Archive, requested_name: []const u8) ?*const Entry {
        for (self.entries) |*entry| {
            if (pathEql(entry.name, requested_name)) return entry;
        }
        return null;
    }

    pub fn extract(self: *const Archive, allocator: std.mem.Allocator, entry: *const Entry) ![]u8 {
        const offset: usize = entry.local_header_offset;
        if (readU32(self.bytes, offset) != 0x04034b50) return Error.InvalidArchive;
        const name_len = readU16(self.bytes, offset + 26) orelse return Error.InvalidArchive;
        const extra_len = readU16(self.bytes, offset + 28) orelse return Error.InvalidArchive;
        const data_offset = offset + 30 + @as(usize, name_len) + extra_len;
        const data_end = data_offset + @as(usize, entry.compressed_size);
        if (data_end > self.bytes.len) return Error.InvalidArchive;
        const compressed = self.bytes[data_offset..data_end];
        const output = switch (entry.method) {
            0 => try allocator.dupe(u8, compressed),
            8 => try inflateRaw(allocator, compressed, entry.uncompressed_size),
            else => return Error.UnsupportedCompression,
        };
        errdefer allocator.free(output);
        if (output.len != entry.uncompressed_size) return Error.SizeMismatch;
        if (std.hash.crc.Crc32.hash(output) != entry.crc32) return Error.ChecksumMismatch;
        return output;
    }
};

fn inflateRaw(allocator: std.mem.Allocator, compressed: []const u8, expected_size: usize) ![]u8 {
    var input: std.Io.Reader = .fixed(compressed);
    var allocating: std.Io.Writer.Allocating = .init(allocator);
    defer allocating.deinit();
    var window: [std.compress.flate.max_window_len]u8 = undefined;
    var decompress: std.compress.flate.Decompress = .init(&input, .raw, &window);
    const written = decompress.reader.streamRemaining(&allocating.writer) catch return decompress.err orelse Error.InvalidArchive;
    if (written != expected_size) return Error.SizeMismatch;
    return allocating.toOwnedSlice();
}

fn findEndRecord(bytes: []const u8) ?usize {
    if (bytes.len < 22) return null;
    const search_start = bytes.len - @min(bytes.len, 22 + 0xffff);
    var cursor = bytes.len - 22;
    while (true) {
        if (readU32(bytes, cursor) == 0x06054b50) {
            const comment_len = readU16(bytes, cursor + 20) orelse return null;
            if (cursor + 22 + comment_len == bytes.len) return cursor;
        }
        if (cursor == search_start) break;
        cursor -= 1;
    }
    return null;
}

fn pathEql(a: []const u8, b: []const u8) bool {
    if (a.len != b.len) return false;
    for (a, b) |left, right| {
        const normalized_left = if (left == '\\') '/' else std.ascii.toLower(left);
        const normalized_right = if (right == '\\') '/' else std.ascii.toLower(right);
        if (normalized_left != normalized_right) return false;
    }
    return true;
}

fn readU16(bytes: []const u8, offset: usize) ?u16 {
    if (offset + 2 > bytes.len) return null;
    return @as(u16, bytes[offset]) | (@as(u16, bytes[offset + 1]) << 8);
}

fn readU32(bytes: []const u8, offset: usize) ?u32 {
    if (offset + 4 > bytes.len) return null;
    return @as(u32, bytes[offset]) |
        (@as(u32, bytes[offset + 1]) << 8) |
        (@as(u32, bytes[offset + 2]) << 16) |
        (@as(u32, bytes[offset + 3]) << 24);
}

test "raw DEFLATE extraction validates size" {
    const compressed = [_]u8{ 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00 };
    const output = try inflateRaw(std.testing.allocator, &compressed, 5);
    defer std.testing.allocator.free(output);
    try std.testing.expectEqualStrings("hello", output);
}

test "ZIP paths compare case-insensitively with either separator" {
    try std.testing.expect(pathEql("Textes/Strings/Title.txt", "textes\\strings\\title.TXT"));
    try std.testing.expect(!pathEql("a/b", "a/c"));
}

test "repository PAK central directory and first entry are readable" {
    const bytes = try std.Io.Dir.cwd().readFileAlloc(std.testing.io, "Data/ELK/texts.pak", std.testing.allocator, .limited(2 * 1024 * 1024));
    defer std.testing.allocator.free(bytes);
    var archive = try Archive.parse(std.testing.allocator, bytes);
    defer archive.deinit();
    try std.testing.expect(archive.entries.len > 100);
    const output = try archive.extract(std.testing.allocator, &archive.entries[0]);
    defer std.testing.allocator.free(output);
    try std.testing.expectEqual(@as(usize, archive.entries[0].uncompressed_size), output.len);
}
