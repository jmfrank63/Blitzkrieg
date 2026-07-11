const std = @import("std");

const Entry = struct {
    name: []u8,
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
        for (entries.items) |entry| init.gpa.free(entry.name);
        entries.deinit(init.gpa);
    }

    while (try walker.next(init.io)) |entry| {
        if (entry.kind != .file) continue;

        const rel_name = try init.gpa.dupe(u8, entry.path);
        std.mem.replaceScalar(u8, rel_name, '\\', '/');

        var file = try entry.dir.openFile(init.io, entry.basename, .{});
        defer file.close(init.io);

        var read_buffer: [64 * 1024]u8 = undefined;
        var reader = file.reader(init.io, &read_buffer);
        const data = try reader.interface.allocRemaining(init.gpa, .unlimited);
        defer init.gpa.free(data);

        const crc = std.hash.Crc32.hash(data);
        const size: u32 = @intCast(data.len);

        const local_header_offset: u32 = @intCast(writer.logicalPos());
        try writeLocalHeader(&writer.interface, rel_name, crc, size);
        try writer.interface.writeAll(rel_name);
        try writer.interface.writeAll(data);

        try entries.append(init.gpa, .{
            .name = rel_name,
            .local_header_offset = local_header_offset,
            .crc32 = crc,
            .size = size,
        });
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
