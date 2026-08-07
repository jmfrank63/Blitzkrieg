const std = @import("std");
const stage = @import("stage.zig");

test "copy-data remains the default" {
    const options = stage.Options{ .repo_root = ".", .install_dir = "zig-out/game/test" };
    try std.testing.expectEqual(stage.DataMode.copy, options.data_mode);
}

test "explicit link permission failures are actionable" {
    try std.testing.expectEqual(error.DataLinkPermissionDenied, stage.classifyDataLinkError(error.PermissionDenied));
    try std.testing.expectEqual(error.DataLinkPermissionDenied, stage.classifyDataLinkError(error.AccessDenied));
}

test "stale images are never accepted as runtime inputs" {
    try std.testing.expect(!stage.shouldReplaceRuntime("Game.exe.stale"));
    try std.testing.expect(stage.shouldReplaceRuntime("Game.exe"));
}

test "Linux SDL staging resolves the versioned shared object" {
    try std.testing.expectEqualStrings("libSDL3.so.0.4.0", stage.runtimeSourceName("libSDL3.so.0"));
    try std.testing.expectEqualStrings("libPlatformRuntime.so", stage.runtimeSourceName("libPlatformRuntime.so"));
}

test "target layout carries target-specific runtime names" {
    const windows = stage.RuntimeLayout{
        .game_name = "Game.exe",
        .runtime_files = &.{ "Game.exe", "PlatformRuntime.dll", "GFXGPU.dll" },
        .debug_files = &.{"Game.pdb"},
        .editors_supported = true,
    };
    const unix = stage.RuntimeLayout{
        .game_name = "Game",
        .runtime_files = &.{ "Game", "libPlatformRuntime.so", "libGFXGPU.so" },
        .debug_files = &.{},
        .editors_supported = false,
    };
    try std.testing.expectEqualStrings("Game.exe", windows.game_name);
    try std.testing.expectEqualStrings("libPlatformRuntime.so", unix.runtime_files[1]);
    try std.testing.expect(!unix.editors_supported);
}

test "stages through a destination path with spaces and non-ASCII characters" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture_root = try tmp.dir.realPathFileAlloc(io, ".", allocator);
    const repo_name = "repository with spaces - тест";
    const install_name = "staged output with spaces - 测试";
    const repo_path = try std.fs.path.join(allocator, &.{ fixture_root, repo_name });
    const install_path = try std.fs.path.join(allocator, &.{ fixture_root, install_name });

    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "zig-out/bin" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "zig-out/shaders" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/Configs" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/Maps" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/cache" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/temp" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/saves" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/logs" }));
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "zig-out/bin/Game" }),
        .data = "game fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "zig-out/shaders/textured.vertex.spirv" }),
        .data = "shader fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/Configs/defconf.cfg" }),
        .data = "default fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "LICENSE.md" }),
        .data = "license fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "README.md" }),
        .data = "readme fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/Maps/fixture.map" }),
        .data = "map fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/cache/compiled.bin" }),
        .data = "cache fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/temp/session.bin" }),
        .data = "temp fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/saves/profile.sav" }),
        .data = "save fixture",
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/logs/stage.log" }),
        .data = "log fixture",
    });

    try stage.stage(io, allocator, .{
        .repo_root = repo_path,
        .install_dir = install_path,
        .data_mode = .copy,
        .layout = .{
            .game_name = "Game",
            .runtime_files = &.{"Game"},
            .debug_files = &.{},
            .editors_supported = false,
        },
    });

    try std.testing.expect(std.mem.indexOf(u8, install_path, install_name) != null);
    const destination = try std.Io.Dir.cwd().openDir(io, install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    try expectStagedFile(destination, io, allocator, "Game", "game fixture");
    try expectStagedFile(destination, io, allocator, "Shaders/GfxGpu/textured.vertex.spirv", "shader fixture");
    try expectStagedFile(destination, io, allocator, "Data/Maps/fixture.map", "map fixture");
    try expectStagedFile(destination, io, allocator, "config.cfg", "default fixture");
    try expectStagedFile(destination, io, allocator, "defconf.cfg", "default fixture");
    try expectStagedFile(destination, io, allocator, "LICENSE.md", "license fixture");
    try expectStagedFile(destination, io, allocator, "README.md", "readme fixture");
    for ([_][]const u8{
        "Data/cache/compiled.bin",
        "Data/temp/session.bin",
        "Data/saves/profile.sav",
        "Data/logs/stage.log",
    }) |forbidden_path| {
        try expectStagedPathAbsent(destination, io, forbidden_path);
    }
    try destination.access(io, "saves", .{});
}

fn expectStagedFile(destination: std.Io.Dir, io: std.Io, allocator: std.mem.Allocator, path: []const u8, expected: []const u8) !void {
    const contents = try destination.readFileAlloc(io, path, allocator, .limited(1024));
    defer allocator.free(contents);
    try std.testing.expectEqualStrings(expected, contents);
}

fn expectStagedPathAbsent(destination: std.Io.Dir, io: std.Io, path: []const u8) !void {
    try std.testing.expectError(error.FileNotFound, destination.access(io, path, .{}));
}
