const std = @import("std");

const buf_size = 1024;

const Error = error{
    BadArgCount,
    UnrecognizedArg,
};

const DepthReplacing = void;
const LocalSize = struct {
    x: u32,
    y: u32,
    z: u32,
};

fn usage(
    io: std.Io,
    err: Error,
) !void {
    var buf1: [buf_size]u8 = undefined;
    var buf2: [buf_size]u8 = undefined;
    var err_writer = std.Io.File.stderr().writer(io, &buf1).interface;
    var out_writer = std.Io.File.stdout().writer(io, &buf2).interface;
    switch (err) {
        error.BadArgCount => try err_writer.writeAll("Invalid arguments specified"),
        error.UnrecognizedArg => try err_writer.writeAll("Unrecognized argument"),
    }
    try out_writer.writeAll("Usage: spirv-execution-mode <input.spvasm> <output.spvasm> [DepthReplacing] [LocalSize x y z]");
    try out_writer.flush();
    return err;
}

pub fn main(
    init: std.process.Init,
) !void {
    const allocator = init.arena.allocator();
    const io = init.io;

    // Gather parameters.
    var depth_replacing: ?DepthReplacing = null;
    var local_size: ?LocalSize = null;
    var args = try init.minimal.args.iterateAllocator(allocator);
    defer args.deinit();
    _ = args.next();
    const input_file = args.next() orelse return usage(io, error.BadArgCount);
    const output_file = args.next() orelse return usage(io, error.BadArgCount);
    while (args.next()) |execution_mode| {
        if (std.mem.eql(u8, execution_mode, "DepthReplacing")) {
            depth_replacing = {};
        } else if (std.mem.eql(u8, execution_mode, "LocalSize")) {
            const x = args.next() orelse return usage(io, error.BadArgCount);
            const y = args.next() orelse return usage(io, error.BadArgCount);
            const z = args.next() orelse return usage(io, error.BadArgCount);
            local_size = .{
                .x = try std.fmt.parseInt(u32, x, 10),
                .y = try std.fmt.parseInt(u32, y, 10),
                .z = try std.fmt.parseInt(u32, z, 10),
            };
        } else return usage(io, error.UnrecognizedArg);
    }

    // Output modified source.
    var read_buf: [buf_size]u8 = undefined;
    var write_buf: [buf_size]u8 = undefined;
    var in_file = try std.Io.Dir.cwd().openFile(io, input_file, .{});
    defer in_file.close(io);
    var out_file = try std.Io.Dir.cwd().createFile(io, output_file, .{});
    defer out_file.close(io);
    var f_reader = in_file.reader(io, &read_buf);
    const reader = &f_reader.interface;
    var f_writer = out_file.writer(io, &write_buf);
    const writer = &f_writer.interface;
    while (true) {
        const line = reader.takeDelimiterInclusive('\n') catch |err| {
            switch (err) {
                error.EndOfStream => break,
                else => return err,
            }
        };
        try writer.writeAll(line);
        var toks = std.mem.tokenizeScalar(u8, line, ' ');
        const op = toks.next() orelse continue;
        if (!std.mem.eql(u8, op, "OpEntryPoint"))
            continue;
        _ = toks.next();
        const entry_point_var = toks.next() orelse continue;
        if (depth_replacing != null) {
            try writer.writeAll("               OpExecutionMode ");
            try writer.writeAll(entry_point_var);
            try writer.writeAll(" DepthReplacing\n");
        }
        if (local_size) |sz| {
            try writer.writeAll("               OpExecutionMode ");
            try writer.writeAll(entry_point_var);
            try writer.print(" LocalSize {d} {d} {d}\n", .{ sz.x, sz.y, sz.z });
        }
    }
    try writer.flush();
}
