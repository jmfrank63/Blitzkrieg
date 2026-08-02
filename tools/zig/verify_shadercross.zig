const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const allocator = init.gpa;
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, allocator);
    defer args.deinit();
    _ = args.skip();
    const exe = args.next() orelse return error.MissingShadercrossPath;
    const output = try std.process.run(allocator, init.io, .{
        .argv = &.{ exe, "--help" },
    });
    defer allocator.free(output.stdout);
    defer allocator.free(output.stderr);
    switch (output.term) {
        .exited => |code| if (code != 0) {
            std.debug.print("shadercross --help failed ({d})\n{s}\n{s}\n", .{ code, output.stdout, output.stderr });
            return error.ShadercrossHelpFailed;
        },
        else => return error.ShadercrossHelpFailed,
    }
    const options = [_][]const u8{ "-s", "-d", "-t", "-e", "-I", "-o", "-D" };
    for (options) |option| {
        if (std.mem.indexOf(u8, output.stdout, option) == null and
            std.mem.indexOf(u8, output.stderr, option) == null)
            return error.MissingShadercrossOption;
    }
    std.debug.print("shadercross help verified: {s}\n", .{exe});
}
