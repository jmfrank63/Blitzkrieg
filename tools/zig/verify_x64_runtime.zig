const std = @import("std");

const required = [_][]const u8{
    "PlatformRuntime.dll",
    "StreamIO.dll", "StreamIOOptionsAbi.dll", "Anim.dll", "GFXGPU.dll", "SDL3.dll",
    "Image.dll", "Input.dll", "Net.dll", "SFX.dll", "UI.dll", "Scene.dll",
    "AILogic.dll", "GameTT.dll", "Game.exe",
};

fn runSmoke(init: std.process.Init, allocator: std.mem.Allocator, install_dir: []const u8) !void {
    const game = try std.fs.path.join(allocator, &.{ install_dir, "Game.exe" });
    defer allocator.free(game);
    const argv = [_][]const u8{ game, "-startup-smoke", "-windowed" };
    var child = try std.process.spawn(init.io, .{ .argv = &argv, .cwd = .{ .path = install_dir } });
    const term = try child.wait(init.io);
    switch (term) {
        .exited => |code| {
            if (code != 0) {
                std.debug.print("Game.exe startup smoke exited with code {d}\\n", .{code});
                return error.GameSmokeFailed;
            }
        },
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
    const install_dir_arg = args.next() orelse return error.MissingInstallDirectory;
    const install_dir = try std.Io.Dir.cwd().realPathFileAlloc(init.io, install_dir_arg, allocator);

    for (required) |name| {
        const path = try std.fs.path.join(allocator, &.{ install_dir, name });
        try std.Io.Dir.cwd().access(init.io, path, .{});
    }
    try runSmoke(init, allocator, install_dir);
    std.debug.print("native Zig x64 runtime verification passed\n", .{});
}
