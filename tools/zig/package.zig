const std = @import("std");

const Entry = struct {
    name: []u8,
    source_path: []u8,
    local_header_offset: u32,
    crc32: u32,
    size: u32,
};

pub fn main(init: std.process.Init) !void {
    var args_it = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args_it.deinit();

    _ = args_it.skip();
    const source_root = args_it.next() orelse return error.InvalidArguments;
    const output_zip = args_it.next() orelse return error.InvalidArguments;

    const cwd = std.Io.Dir.cwd();
    try ensureParentDir(init.io, output_zip);

    var output_file = try cwd.createFile(init.io, output_zip, .{ .truncate = true, .read = false });
    defer output_file.close(init.io);

    var out_buffer: [64 * 1024]u8 = undefined;
    var writer = output_file.writer(init.io, &out_buffer);

    var source_dir = try cwd.openDir(init.io, source_root, .{ .iterate = true });
    defer source_dir.close(init.io);

    var walker = try source_dir.walk(init.gpa);
    defer walker.deinit();

    var entries = std.ArrayList(Entry).empty;
    defer {
        for (entries.items) |entry| {
            init.gpa.free(entry.name);
            init.gpa.free(entry.source_path);
        }
        entries.deinit(init.gpa);
    }

    while (try walker.next(init.io)) |entry| {
        if (entry.kind != .file) continue;

        const file_info = try crcAndSize(init.io, entry.dir, entry.basename);
        const source_path = try init.gpa.dupe(u8, entry.path);
        const rel_name = init.gpa.dupe(u8, entry.path) catch |err| {
            init.gpa.free(source_path);
            return err;
        };
        std.mem.replaceScalar(u8, rel_name, '\\', '/');

        entries.append(init.gpa, .{
            .name = rel_name,
            .source_path = source_path,
            .local_header_offset = 0,
            .crc32 = file_info.crc32,
            .size = file_info.size,
        }) catch |err| {
            init.gpa.free(rel_name);
            init.gpa.free(source_path);
            return err;
        };
    }

    std.mem.sort(Entry, entries.items, {}, struct {
        fn lessThan(_: void, lhs: Entry, rhs: Entry) bool {
            return std.mem.order(u8, lhs.name, rhs.name) == .lt;
        }
    }.lessThan);

    for (entries.items) |*entry| {
        entry.local_header_offset = @intCast(writer.logicalPos());
        try writeLocalHeader(&writer.interface, entry.name, entry.crc32, entry.size);
        try writer.interface.writeAll(entry.name);
        try streamFile(init.io, source_dir, entry.source_path, &writer.interface);
    }

    const central_dir_offset: u32 = @intCast(writer.logicalPos());
    for (entries.items) |entry| {
        try writeCentralHeader(&writer.interface, entry);
        try writer.interface.writeAll(entry.name);
    }
    const central_dir_size: u32 = @intCast(writer.logicalPos() - central_dir_offset);

    try writer.interface.writeAll("PK\x05\x06");
    try writer.interface.writeInt(u16, 0, .little);
    try writer.interface.writeInt(u16, 0, .little);
    try writer.interface.writeInt(u16, @intCast(entries.items.len), .little);
    try writer.interface.writeInt(u16, @intCast(entries.items.len), .little);
    try writer.interface.writeInt(u32, central_dir_size, .little);
    try writer.interface.writeInt(u32, central_dir_offset, .little);
    try writer.interface.writeInt(u16, 0, .little);
    try writer.interface.flush();
}

const FileInfo = struct {
    crc32: u32,
    size: u32,
};

fn crcAndSize(io: std.Io, dir: std.Io.Dir, path: []const u8) !FileInfo {
    var file = try dir.openFile(io, path, .{});
    defer file.close(io);
    const stat = try file.stat(io);
    const size: u32 = @intCast(stat.size);

    var read_buffer: [64 * 1024]u8 = undefined;
    var reader = file.reader(io, &read_buffer);
    var crc = std.hash.Crc32.init();
    var chunk: [64 * 1024]u8 = undefined;
    while (true) {
        const n = try reader.interface.readSliceShort(&chunk);
        if (n == 0) break;
        crc.update(chunk[0..n]);
    }
    return .{ .crc32 = crc.final(), .size = size };
}

fn streamFile(io: std.Io, dir: std.Io.Dir, path: []const u8, writer: *std.Io.Writer) !void {
    var file = try dir.openFile(io, path, .{});
    defer file.close(io);
    var read_buffer: [64 * 1024]u8 = undefined;
    var reader = file.reader(io, &read_buffer);
    _ = try reader.interface.streamRemaining(writer);
}

fn ensureParentDir(io: std.Io, path: []const u8) !void {
    if (std.fs.path.dirname(path)) |dir_name| {
        const cwd = std.Io.Dir.cwd();
        try cwd.createDirPath(io, dir_name);
    }
}

fn writeLocalHeader(writer: *std.Io.Writer, name: []const u8, crc32: u32, size: u32) !void {
    const name_len: u16 = @intCast(name.len);
    try writer.writeAll("PK\x03\x04");
    try writer.writeInt(u16, 20, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u32, crc32, .little);
    try writer.writeInt(u32, size, .little);
    try writer.writeInt(u32, size, .little);
    try writer.writeInt(u16, name_len, .little);
    try writer.writeInt(u16, 0, .little);
}

fn writeCentralHeader(writer: *std.Io.Writer, entry: Entry) !void {
    const name_len: u16 = @intCast(entry.name.len);
    try writer.writeAll("PK\x01\x02");
    try writer.writeInt(u16, 20, .little);
    try writer.writeInt(u16, 20, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u32, entry.crc32, .little);
    try writer.writeInt(u32, entry.size, .little);
    try writer.writeInt(u32, entry.size, .little);
    try writer.writeInt(u16, name_len, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u32, 0, .little);
    try writer.writeInt(u32, entry.local_header_offset, .little);
}
