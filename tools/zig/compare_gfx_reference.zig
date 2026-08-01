const std = @import("std");

const Options = struct {
    legacy: []const u8,
    candidate: []const u8,
    diff: []const u8,
    report: ?[]const u8,
    width: usize,
    height: usize,
    p99_9_rgb: u8 = 64,
    mean_rgb: f64 = 2.0,
};

fn usage() noreturn {
    std.debug.print("usage: compare_gfx_reference --legacy FILE --candidate FILE --width N --height N --diff FILE [--report FILE]\n", .{});
    std.process.exit(2);
}

fn nextArg(args: []const []const u8, index: *usize) []const u8 {
    if (index.* >= args.len) usage();
    const value = args[index.*];
    index.* += 1;
    return value;
}

fn parseOptions(args: []const []const u8) !Options {
    var result = Options{ .legacy = "", .candidate = "", .diff = "", .report = null, .width = 0, .height = 0 };
    var i: usize = 0;
    while (i < args.len) {
        const key = args[i];
        i += 1;
        if (std.mem.eql(u8, key, "--legacy")) result.legacy = nextArg(args, &i)
        else if (std.mem.eql(u8, key, "--candidate")) result.candidate = nextArg(args, &i)
        else if (std.mem.eql(u8, key, "--diff")) result.diff = nextArg(args, &i)
        else if (std.mem.eql(u8, key, "--report")) result.report = nextArg(args, &i)
        else if (std.mem.eql(u8, key, "--width")) result.width = try std.fmt.parseInt(usize, nextArg(args, &i), 10)
        else if (std.mem.eql(u8, key, "--height")) result.height = try std.fmt.parseInt(usize, nextArg(args, &i), 10)
        else if (std.mem.eql(u8, key, "--p99-9-rgb")) result.p99_9_rgb = try std.fmt.parseInt(u8, nextArg(args, &i), 10)
        else if (std.mem.eql(u8, key, "--mean-rgb")) result.mean_rgb = try std.fmt.parseFloat(f64, nextArg(args, &i))
        else usage();
    }
    if (result.legacy.len == 0 or result.candidate.len == 0 or result.diff.len == 0 or result.width == 0 or result.height == 0) usage();
    return result;
}

fn writeLe16(writer: anytype, value: u16) !void {
    try writer.writeAll(&.{ @as(u8, @truncate(value)), @as(u8, @truncate(value >> 8)) });
}

fn writeLe32(writer: anytype, value: u32) !void {
    try writer.writeAll(&.{ @as(u8, @truncate(value)), @as(u8, @truncate(value >> 8)), @as(u8, @truncate(value >> 16)), @as(u8, @truncate(value >> 24)) });
}

fn writeDiffBmp(io: std.Io, path: []const u8, pixels: []const u8, width: usize, height: usize) !void {
    const file = try std.Io.Dir.cwd().createFile(io, path, .{ .truncate = true });
    defer file.close(io);
    var buffer: [4096]u8 = undefined;
    var writer = file.writer(io, &buffer);
    const pixel_bytes: u32 = @intCast(width * height * 4);
    const file_size: u32 = 14 + 40 + pixel_bytes;
    try writer.interface.writeAll("BM");
    try writeLe32(&writer.interface, file_size);
    try writeLe16(&writer.interface, 0);
    try writeLe16(&writer.interface, 0);
    try writeLe32(&writer.interface, 54);
    try writeLe32(&writer.interface, 40);
    try writeLe32(&writer.interface, @intCast(width));
    try writeLe32(&writer.interface, @intCast(height));
    try writeLe16(&writer.interface, 1);
    try writeLe16(&writer.interface, 32);
    try writeLe32(&writer.interface, 0);
    try writeLe32(&writer.interface, pixel_bytes);
    try writeLe32(&writer.interface, 2835);
    try writeLe32(&writer.interface, 2835);
    try writeLe32(&writer.interface, 0);
    try writeLe32(&writer.interface, 0);
    var row: usize = height;
    while (row > 0) {
        row -= 1;
        var x: usize = 0;
        while (x < width) : (x += 1) {
            const p = (row * width + x) * 4;
            try writer.interface.writeAll(&.{ pixels[p + 2], pixels[p + 1], pixels[p], 255 });
        }
    }
    try writer.interface.flush();
}

pub fn main(init: std.process.Init) !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, allocator);
    defer iterator.deinit();
    var args = std.ArrayList([]const u8).empty;
    while (iterator.next()) |arg| try args.append(allocator, arg);
    if (args.items.len < 2) usage();
    const options = try parseOptions(args.items[1..]);
    const expected_bytes = options.width * options.height * 4;
    const legacy = try std.Io.Dir.cwd().readFileAlloc(init.io, options.legacy, allocator, .limited(expected_bytes + 1));
    const candidate = try std.Io.Dir.cwd().readFileAlloc(init.io, options.candidate, allocator, .limited(expected_bytes + 1));
    if (legacy.len != expected_bytes or candidate.len != expected_bytes) {
        std.debug.print("capture size mismatch: expected {d} bytes, legacy={d}, candidate={d}\n", .{ expected_bytes, legacy.len, candidate.len });
        return error.InvalidCaptureSize;
    }

    var diff = try allocator.alloc(u8, expected_bytes);
    var max_error: u8 = 0;
    var max_error_x: usize = 0;
    var max_error_y: usize = 0;
    var error_histogram: [256]u64 = [_]u64{0} ** 256;
    var total_error: u64 = 0;
    var rgb_samples: u64 = 0;
    var alpha_mismatches: u64 = 0;
    var changed_pixels: u64 = 0;
    var i: usize = 0;
    while (i < expected_bytes) : (i += 4) {
        var pixel_changed = false;
        var channel: usize = 0;
        while (channel < 3) : (channel += 1) {
            const a = legacy[i + channel];
            const b = candidate[i + channel];
            const error_value: u8 = if (a >= b) a - b else b - a;
            diff[i + channel] = error_value;
            if (error_value > max_error) {
                max_error = error_value;
                const pixel = i / 4;
                max_error_x = pixel % options.width;
                max_error_y = pixel / options.width;
            }
            total_error += error_value;
            rgb_samples += 1;
            error_histogram[error_value] += 1;
            pixel_changed = pixel_changed or error_value != 0;
        }
        diff[i + 3] = if (legacy[i + 3] == candidate[i + 3]) 0 else 255;
        if (legacy[i + 3] != candidate[i + 3]) alpha_mismatches += 1;
        if (pixel_changed) changed_pixels += 1;
    }
    try writeDiffBmp(init.io, options.diff, diff, options.width, options.height);
    const mean_error = @as(f64, @floatFromInt(total_error)) / @as(f64, @floatFromInt(rgb_samples));
    const percentile_rank = (rgb_samples * 999 + 999) / 1000;
    var percentile_count: u64 = 0;
    var p99_9_error: u8 = 255;
    for (error_histogram, 0..) |count, error_value| {
        percentile_count += count;
        if (percentile_count >= percentile_rank) {
            p99_9_error = @intCast(error_value);
            break;
        }
    }
    const passed = alpha_mismatches == 0 and p99_9_error <= options.p99_9_rgb and mean_error <= options.mean_rgb;
    var output_buffer: [512]u8 = undefined;
    var output = std.Io.File.stdout().writer(init.io, &output_buffer);
    try output.interface.print("p99_9_rgb={d}/255 max_rgb={d}/255 at=({d},{d}) mean_rgb={d:.6}/255 alpha_mismatches={d} changed_pixels={d} pass={s}\n", .{ p99_9_error, max_error, max_error_x, max_error_y, mean_error, alpha_mismatches, changed_pixels, if (passed) "true" else "false" });
    try output.interface.flush();
    if (options.report) |report_path| {
        const report = try std.Io.Dir.cwd().createFile(init.io, report_path, .{ .truncate = true });
        defer report.close(init.io);
        var report_buffer: [512]u8 = undefined;
        var report_writer = report.writer(init.io, &report_buffer);
        try report_writer.interface.print("p99_9_rgb={d}/255\nmax_rgb={d}/255\nat=({d},{d})\nmean_rgb={d:.6}/255\nalpha_mismatches={d}\nchanged_pixels={d}\npass={s}\n", .{ p99_9_error, max_error, max_error_x, max_error_y, mean_error, alpha_mismatches, changed_pixels, if (passed) "true" else "false" });
        try report_writer.interface.flush();
    }
    if (!passed) return error.ReferenceSceneMismatch;
}
