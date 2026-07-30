const sdl3 = @import("sdl3");
const std = @import("std");

pub fn main(init: std.process.Init) !void {
    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer = std.Io.File.stdout().writer(init.io, &stdout_buffer);
    const out = &stdout_file_writer.interface;

    // Paths available.
    try out.print("Application Directory: \"{s}\"\n", .{try sdl3.filesystem.getBasePath()});
    try out.print("User Home Directory: \"{s}\"\n", .{try sdl3.filesystem.getUserFolder(.home)});
    const cwd = try sdl3.filesystem.getCurrentDirectory();
    defer sdl3.free(cwd);
    try out.print("Current Working Directory: \"{s}\"\n", .{cwd});

    // Check if a path exists.
    _ = sdl3.filesystem.getPathExists("/home");

    // Setup an allocator.
    const allocator = std.heap.smp_allocator;

    // Iterate a path above the application directory.
    var above_app_path = try sdl3.filesystem.Path.init(allocator, try sdl3.filesystem.getBasePath());
    defer above_app_path.deinit();
    _ = above_app_path.parent();
    try out.print("Enumerating: \"{s}\"\n", .{above_app_path.get()});

    // Show all the entries.
    const items = try sdl3.filesystem.getAllDirectoryItems(allocator, above_app_path.get());
    defer sdl3.filesystem.freeAllDirectoryItems(allocator, items);
    for (items.items) |item| {
        try out.print("Found: \"{s}\"\n", .{item});
    }

    try stdout_file_writer.interface.flush();
}
