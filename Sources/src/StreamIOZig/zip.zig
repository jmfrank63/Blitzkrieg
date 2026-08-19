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

// Maps a normalized entry name to the index of the first central-directory
// entry carrying it.  Keys are the raw name slices that point into the archive
// bytes; the context normalizes on the fly so no normalized copy of any name is
// ever allocated.
const NameIndex = std.HashMapUnmanaged(u32, void, EntryNameContext, std.hash_map.default_max_load_percentage);

pub const Archive = struct {
    bytes: []const u8,
    entries: []Entry,
    index: NameIndex,
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

        // texts.pak alone holds 2939 entries, and the loader used to rescan all
        // of them - with a per-character normalizing compare - for every single
        // lookup that missed.  Index the names once at parse time instead.
        const context: EntryNameContext = .{ .entries = entries };
        var index: NameIndex = .empty;
        errdefer index.deinit(allocator);
        try index.ensureTotalCapacityContext(allocator, entry_count, context);
        for (entries, 0..) |_, position| {
            // A zip central directory may legitimately repeat a name.  find has
            // always returned the first one in directory order, so an existing
            // slot is left pointing at the entry that claimed it first.
            _ = index.getOrPutAssumeCapacityContext(@intCast(position), context);
        }
        return .{ .bytes = bytes, .entries = entries, .index = index, .allocator = allocator };
    }

    pub fn deinit(self: *Archive) void {
        self.index.deinit(self.allocator);
        self.allocator.free(self.entries);
        self.* = undefined;
    }

    pub fn find(self: *const Archive, requested_name: []const u8) ?*const Entry {
        const context: EntryNameContext = .{ .entries = self.entries };
        const adapter: EntryNameAdapter = .{ .context = context };
        const position = self.index.getKeyAdapted(requested_name, adapter) orelse return null;
        return &self.entries[position];
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

// The one normalization every path comparison and every index hash goes
// through.  If the hash and the equality ever disagreed about a byte, entries
// would silently vanish from the archive, so both are built on this function.
fn normalizePathByte(byte: u8) u8 {
    return if (byte == '\\') '/' else std.ascii.toLower(byte);
}

fn pathEql(a: []const u8, b: []const u8) bool {
    if (a.len != b.len) return false;
    for (a, b) |left, right| {
        if (normalizePathByte(left) != normalizePathByte(right)) return false;
    }
    return true;
}

fn pathHash(name: []const u8) u64 {
    var hasher = std.hash.Wyhash.init(0);
    var chunk: [64]u8 = undefined;
    var filled: usize = 0;
    for (name) |byte| {
        chunk[filled] = normalizePathByte(byte);
        filled += 1;
        if (filled == chunk.len) {
            hasher.update(&chunk);
            filled = 0;
        }
    }
    hasher.update(chunk[0..filled]);
    return hasher.final();
}

// Keys in the index are entry positions, not strings: that keeps the map one
// u32 per entry and lets the stored name stay where it already lives, inside
// the archive bytes.
const EntryNameContext = struct {
    entries: []const Entry,

    pub fn hash(self: EntryNameContext, position: u32) u64 {
        return pathHash(self.entries[position].name);
    }

    pub fn eql(self: EntryNameContext, a: u32, b: u32) bool {
        return pathEql(self.entries[a].name, self.entries[b].name);
    }
};

// Lets find probe the map with a caller-supplied path without first turning it
// into an entry, and without allocating a normalized copy of it.
const EntryNameAdapter = struct {
    context: EntryNameContext,

    pub fn hash(_: EntryNameAdapter, name: []const u8) u64 {
        return pathHash(name);
    }

    pub fn eql(self: EntryNameAdapter, name: []const u8, position: u32) bool {
        return pathEql(name, self.context.entries[position].name);
    }
};

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

// Builds a zip that is nothing but a central directory and an end record:
// enough for parse and find, which never look at the file data.
fn buildDirectoryOnlyArchive(allocator: std.mem.Allocator, names: []const []const u8) ![]u8 {
    var buffer: std.ArrayList(u8) = .empty;
    errdefer buffer.deinit(allocator);
    for (names) |name| {
        var header = [_]u8{0} ** 46;
        std.mem.writeInt(u32, header[0..4], 0x02014b50, .little);
        std.mem.writeInt(u16, header[28..30], @intCast(name.len), .little);
        try buffer.appendSlice(allocator, &header);
        try buffer.appendSlice(allocator, name);
    }
    const central_size: u32 = @intCast(buffer.items.len);
    var end = [_]u8{0} ** 22;
    std.mem.writeInt(u32, end[0..4], 0x06054b50, .little);
    std.mem.writeInt(u16, end[8..10], @intCast(names.len), .little);
    std.mem.writeInt(u16, end[10..12], @intCast(names.len), .little);
    std.mem.writeInt(u32, end[12..16], central_size, .little);
    std.mem.writeInt(u32, end[16..20], 0, .little);
    try buffer.appendSlice(allocator, &end);
    return buffer.toOwnedSlice(allocator);
}

// The behaviour the index has to reproduce byte for byte.
fn linearFind(archive: *const Archive, requested_name: []const u8) ?*const Entry {
    for (archive.entries) |*entry| {
        if (pathEql(entry.name, requested_name)) return entry;
    }
    return null;
}

fn expectSameAsLinearScan(archive: *const Archive, requested_name: []const u8) !void {
    try std.testing.expectEqual(linearFind(archive, requested_name), archive.find(requested_name));
}

test "indexed find agrees with a linear scan on case and separator variants" {
    const names = [_][]const u8{
        "Textes/Strings/Title.txt",
        "textes/strings/body.TXT",
        "Data\\Maps\\Level01.map",
        "readme",
        "",
        "UPPER/CASE/PATH.DAT",
        "mixed\\Case/Path.dat",
    };
    const bytes = try buildDirectoryOnlyArchive(std.testing.allocator, &names);
    defer std.testing.allocator.free(bytes);
    var archive = try Archive.parse(std.testing.allocator, bytes);
    defer archive.deinit();
    try std.testing.expectEqual(@as(usize, names.len), archive.entries.len);

    var variant: [64]u8 = undefined;
    for (names) |name| {
        try expectSameAsLinearScan(&archive, name);
        try std.testing.expect(archive.find(name) != null);
        for ([_]u8{ 'u', 'l', 'f', 'b' }) |mode| {
            for (name, 0..) |byte, position| {
                variant[position] = switch (mode) {
                    'u' => std.ascii.toUpper(byte),
                    'l' => std.ascii.toLower(byte),
                    'f' => if (byte == '\\') '/' else byte,
                    else => if (byte == '/') '\\' else byte,
                };
            }
            const candidate = variant[0..name.len];
            try expectSameAsLinearScan(&archive, candidate);
            try std.testing.expect(archive.find(candidate) != null);
        }
    }

    const absent = [_][]const u8{
        "Textes/Strings/Title.tx",
        "Textes/Strings/Title.txtx",
        "Textes/Strings/Titlz.txt",
        "textes\\strings",
        "readme/",
        "/readme",
        "UPPER/CASE/PATH.DA",
        "\\",
        "/",
        " ",
    };
    for (absent) |name| {
        try expectSameAsLinearScan(&archive, name);
        try std.testing.expect(archive.find(name) == null);
    }
}

test "duplicate entry names resolve to the first one in directory order" {
    const names = [_][]const u8{
        "shared/name.txt",
        "other/file.txt",
        "Shared\\Name.TXT",
        "SHARED/NAME.TXT",
    };
    const bytes = try buildDirectoryOnlyArchive(std.testing.allocator, &names);
    defer std.testing.allocator.free(bytes);
    var archive = try Archive.parse(std.testing.allocator, bytes);
    defer archive.deinit();
    try std.testing.expectEqual(&archive.entries[0], archive.find("shared/name.txt").?);
    try std.testing.expectEqual(&archive.entries[0], archive.find("SHARED\\name.txt").?);
    try std.testing.expectEqual(&archive.entries[1], archive.find("Other/File.txt").?);
}

test "empty and single entry archives behave" {
    const empty_bytes = try buildDirectoryOnlyArchive(std.testing.allocator, &.{});
    defer std.testing.allocator.free(empty_bytes);
    var empty = try Archive.parse(std.testing.allocator, empty_bytes);
    defer empty.deinit();
    try std.testing.expectEqual(@as(usize, 0), empty.entries.len);
    try std.testing.expect(empty.find("anything") == null);
    try std.testing.expect(empty.find("") == null);

    const single_bytes = try buildDirectoryOnlyArchive(std.testing.allocator, &.{"Only\\Entry.txt"});
    defer std.testing.allocator.free(single_bytes);
    var single = try Archive.parse(std.testing.allocator, single_bytes);
    defer single.deinit();
    try std.testing.expectEqual(@as(usize, 1), single.entries.len);
    try std.testing.expectEqual(&single.entries[0], single.find("only/entry.TXT").?);
    try std.testing.expect(single.find("only/entry.txt2") == null);
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

    // The real archive is the one that motivated the index: 2939 entries, so a
    // disagreement between the hash and pathEql would show up here first.
    for (archive.entries) |*entry| {
        try std.testing.expectEqual(linearFind(&archive, entry.name), archive.find(entry.name));
        try std.testing.expect(archive.find(entry.name) != null);
    }
    try std.testing.expect(archive.find("no/such/entry/at/all.txt") == null);
}
