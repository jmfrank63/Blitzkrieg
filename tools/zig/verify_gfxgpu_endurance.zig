const std = @import("std");

fn runSmoke(init: std.process.Init, allocator: std.mem.Allocator, install_dir: []const u8) !void {
    const game = try std.fs.path.join(allocator, &.{ install_dir, "Game.exe" });
    defer allocator.free(game);

    const argv = [_][]const u8{ "Game.exe", "-startup-smoke", "-windowed" };
    var child = try std.process.spawn(init.io, .{ .argv = &argv, .cwd = .{ .path = install_dir } });
    const term = try child.wait(init.io);
    switch (term) {
        .exited => |code| if (code != 0) return error.GameSmokeFailed,
        else => return error.GameSmokeFailed,
    }
}

pub fn main(init: std.process.Init) !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.next();
    const install_dir = args.next() orelse return error.MissingInstallDirectory;

    const game = try std.fs.path.join(allocator, &.{ install_dir, "Game.exe" });
    defer allocator.free(game);
    try std.Io.Dir.cwd().access(init.io, game, .{});

    var restart: usize = 0;
    while (restart < 2) : (restart += 1) try runSmoke(init, allocator, install_dir);
    std.debug.print("P08-M04 native Zig endurance smoke passed: restarts=2\n", .{});
}
