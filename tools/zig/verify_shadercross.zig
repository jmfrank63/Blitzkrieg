const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const allocator = init.gpa;
    const exe = "zig-out/tools/shadercross/install/bin/shadercross.exe";
    const output = try std.process.run(allocator, init.io, .{
        .argv = &.{ exe, "--help" },
    });
    defer allocator.free(output.stdout);
    defer allocator.free(output.stderr);
    switch (output.term) {
        .exited => |code| if (code != 0) return error.ShadercrossHelpFailed,
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
