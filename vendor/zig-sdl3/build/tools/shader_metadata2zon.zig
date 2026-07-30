const ComputePipelineMetadata = @import("ComputePipelineMetadata");
const GraphicsShaderMetadata = @import("GraphicsShaderMetadata");
const std = @import("std");

const file_buffer_size = 4096;

const usage =
    \\Usage: ./shader_metadata2zon <format> <input> <output>
    \\
    \\Options:
    \\  format: Either -g for graphics or -c for compute.
    \\  input: Input JSON metadata.
    \\  output: Output ZON metadata.
    \\
;

fn die() noreturn {
    std.debug.print("{s}\n", .{usage});
    std.process.exit(1);
}

pub fn main(
    init: std.process.Init,
) !void {
    const allocator = init.arena.allocator();
    const args = try init.minimal.args.toSlice(allocator);

    if (args.len != 4)
        die();

    const graphics = if (std.mem.eql(u8, args[1], "-g")) true else if (std.mem.eql(u8, args[1], "-c")) false else die();

    var input_file = try std.Io.Dir.cwd().openFile(init.io, args[2], .{});
    defer input_file.close(init.io);

    var input_file_buffer: [file_buffer_size]u8 = undefined;
    var output_file_buffer: [file_buffer_size]u8 = undefined;

    var input_file_reader = input_file.reader(init.io, &input_file_buffer);
    var input_file_json_reader = std.json.Reader.init(allocator, &input_file_reader.interface);
    defer input_file_json_reader.deinit();

    var metadata_graphics: GraphicsShaderMetadata = undefined;
    var metadata_compute: ComputePipelineMetadata = undefined;
    if (graphics) {
        metadata_graphics = try std.json.parseFromTokenSourceLeaky(GraphicsShaderMetadata, allocator, &input_file_json_reader, .{});
    } else {
        metadata_compute = try std.json.parseFromTokenSourceLeaky(ComputePipelineMetadata, allocator, &input_file_json_reader, .{});
    }

    var output_file = try std.Io.Dir.cwd().createFile(init.io, args[3], .{});
    defer output_file.close(init.io);

    var output_file_writer = output_file.writer(init.io, &output_file_buffer);

    if (graphics) {
        try std.zon.stringify.serialize(metadata_graphics, .{}, &output_file_writer.interface);
    } else {
        try std.zon.stringify.serialize(metadata_compute, .{}, &output_file_writer.interface);
    }
    try output_file_writer.interface.flush();
}
