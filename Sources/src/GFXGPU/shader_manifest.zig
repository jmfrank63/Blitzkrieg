const std = @import("std");

pub const magic = "GFXS";
pub const schema_version: u16 = 3;
pub const max_string_length: usize = std.math.maxInt(u16);

pub const Format = enum(u8) {
    dxil = 1,
    spirv = 2,
    msl = 3,
};

pub const Stage = enum(u8) {
    vertex = 0,
    fragment = 1,
    compute = 2,
};

pub const Record = struct {
    effect: []u8,
    name: []u8,
    entry_point: []u8,
    stage: Stage,
    format: Format = .dxil,
    blob_path: []u8,
    byte_length: u32,
    required_vertex_mask: u32,
    sampler_count: u32,
    storage_texture_count: u32,
    storage_buffer_count: u32,
    uniform_buffer_count: u32,
    hash: [32]u8,

    pub fn deinit(self: *Record, allocator: std.mem.Allocator) void {
        allocator.free(self.effect);
        allocator.free(self.name);
        allocator.free(self.entry_point);
        allocator.free(self.blob_path);
        self.* = undefined;
    }
};

pub const Manifest = struct {
    format: Format,
    records: []Record,

    pub fn deinit(self: *Manifest, allocator: std.mem.Allocator) void {
        for (self.records) |*record| record.deinit(allocator);
        allocator.free(self.records);
        self.* = undefined;
    }
};

pub const Error = error{
    BadMagic,
    UnsupportedVersion,
    UnsupportedFormat,
    MissingStagePair,
    Truncated,
    InvalidStage,
    InvalidPath,
    DuplicateRecord,
    AllocationFailed,
};

const Reader = struct {
    bytes: []const u8,
    offset: usize = 0,

    fn take(self: *Reader, length: usize) Error![]const u8 {
        const end = std.math.add(usize, self.offset, length) catch return Error.Truncated;
        if (end > self.bytes.len) return Error.Truncated;
        const result = self.bytes[self.offset..end];
        self.offset = end;
        return result;
    }

    fn readU8(self: *Reader) Error!u8 {
        return (try self.take(1))[0];
    }

    fn readU16(self: *Reader) Error!u16 {
        return std.mem.readInt(u16, (try self.take(2))[0..2], .little);
    }

    fn readU32(self: *Reader) Error!u32 {
        return std.mem.readInt(u32, (try self.take(4))[0..4], .little);
    }
};

fn readString(reader: *Reader, allocator: std.mem.Allocator) (Error || std.mem.Allocator.Error)![]u8 {
    const length = try reader.readU16();
    const bytes = try reader.take(length);
    return allocator.dupe(u8, bytes);
}

fn validRelativePath(path: []const u8) bool {
    if (path.len == 0 or path[0] == '/' or path[0] == '\\' or std.mem.indexOfScalar(u8, path, ':') != null) return false;
    var part_start: usize = 0;
    while (part_start <= path.len) {
        const remaining = path[part_start..];
        const separator = std.mem.indexOfAny(u8, remaining, "/\\") orelse remaining.len;
        const part = remaining[0..separator];
        if (std.mem.eql(u8, part, "..") or std.mem.eql(u8, part, "")) return false;
        if (separator == remaining.len) break;
        part_start += separator + 1;
    }
    return true;
}

fn duplicate(records: []const Record, record: *const Record) bool {
    for (records) |existing| {
        if (std.mem.eql(u8, existing.effect, record.effect) and existing.stage == record.stage and existing.format == record.format) return true;
    }
    return false;
}

fn parseFormat(value: u8) Error!Format {
    return switch (value) {
        @intFromEnum(Format.dxil) => .dxil,
        @intFromEnum(Format.spirv) => .spirv,
        @intFromEnum(Format.msl) => .msl,
        else => Error.UnsupportedFormat,
    };
}

fn parseStage(value: u8) Error!Stage {
    return switch (value) {
        @intFromEnum(Stage.vertex) => .vertex,
        @intFromEnum(Stage.fragment) => .fragment,
        @intFromEnum(Stage.compute) => .compute,
        else => Error.InvalidStage,
    };
}

pub fn parse(allocator: std.mem.Allocator, bytes: []const u8) (Error || std.mem.Allocator.Error)!Manifest {
    var reader = Reader{ .bytes = bytes };
    if (!std.mem.eql(u8, try reader.take(4), magic)) return Error.BadMagic;
    if (try reader.readU16() != schema_version) return Error.UnsupportedVersion;
    const format = try parseFormat(try reader.readU8());
    _ = try reader.readU8();
    const count = try reader.readU32();
    const records = allocator.alloc(Record, count) catch return Error.AllocationFailed;
    var initialized: usize = 0;
    errdefer {
        for (records[0..initialized]) |*record| record.deinit(allocator);
        allocator.free(records);
    }

    for (records) |*record| {
        record.* = .{
            .effect = try readString(&reader, allocator),
            .name = undefined,
            .entry_point = undefined,
            .stage = undefined,
            .format = undefined,
            .blob_path = undefined,
            .byte_length = undefined,
            .required_vertex_mask = undefined,
            .sampler_count = undefined,
            .storage_texture_count = undefined,
            .storage_buffer_count = undefined,
            .uniform_buffer_count = undefined,
            .hash = undefined,
        };
        errdefer record.deinit(allocator);
        record.name = try readString(&reader, allocator);
        record.entry_point = try readString(&reader, allocator);
        record.format = try parseFormat(try reader.readU8());
        record.stage = try parseStage(try reader.readU8());
        _ = try reader.readU8();
        record.blob_path = try readString(&reader, allocator);
        if (!validRelativePath(record.blob_path)) return Error.InvalidPath;
        record.byte_length = try reader.readU32();
        record.required_vertex_mask = try reader.readU32();
        record.sampler_count = try reader.readU32();
        record.storage_texture_count = try reader.readU32();
        record.storage_buffer_count = try reader.readU32();
        record.uniform_buffer_count = try reader.readU32();
        @memcpy(&record.hash, try reader.take(record.hash.len));
        if (duplicate(records[0..initialized], record)) return Error.DuplicateRecord;
        initialized += 1;
    }
    for (records) |record| {
        if (record.stage == .compute) continue;
        const counterpart: Stage = if (record.stage == .vertex) .fragment else .vertex;
        var found = false;
        for (records) |other| {
            if (other.stage == counterpart and other.format == record.format and std.mem.eql(u8, other.effect, record.effect)) {
                found = true;
                break;
            }
        }
        if (!found) return Error.MissingStagePair;
    }
    if (reader.offset != bytes.len) return Error.Truncated;
    return .{ .format = format, .records = records };
}

test "manifest parser rejects malformed corpus" {
    const valid = [_]u8{
        'G', 'F', 'X', 'S', 3, 0,   1, 0, 1,   0,   0,   0,
        1,   0,   'e', 1,   0, 'n', 4, 0, 'm', 'a', 'i', 'n',
        1,   0,   0,   1,   0, 'x', 0, 0, 0,   0,   0,   0,
        0,   0,   0,   0,   0, 0,   0, 0, 0,   0,   0,   0,
        0,   0,   0,   0,   0, 0,   0, 0, 0,   0,   0,   0,
        0,   0,   0,   0,   0, 0,   0, 0, 0,   0,   0,   0,
        0,   0,   0,   0,   0,
    };
    try std.testing.expectError(Error.BadMagic, parse(std.testing.allocator, "BAD!"));
    var bad_version = valid;
    bad_version[4] = 1;
    try std.testing.expectError(Error.UnsupportedVersion, parse(std.testing.allocator, &bad_version));
    var bad_format = valid;
    bad_format[6] = 99;
    try std.testing.expectError(Error.UnsupportedFormat, parse(std.testing.allocator, &bad_format));
    try std.testing.expectError(Error.Truncated, parse(std.testing.allocator, valid[0 .. valid.len - 1]));
}

test "manifest parser rejects traversal and duplicate records" {
    try std.testing.expect(!validRelativePath("../probe.dxil"));
    try std.testing.expect(!validRelativePath("C:/probe.dxil"));
    try std.testing.expect(validRelativePath("probe.vertex.dxil"));
}

test "manifest format values are stable and distinct" {
    try std.testing.expectEqual(Format.dxil, try parseFormat(1));
    try std.testing.expectEqual(Format.spirv, try parseFormat(2));
    try std.testing.expectEqual(Format.msl, try parseFormat(3));
    try std.testing.expectError(Error.UnsupportedFormat, parseFormat(4));
}
