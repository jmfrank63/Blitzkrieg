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

test "restaging updates changed data, prunes removed data, and keeps game-written files" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    try stage.stage(io, allocator, fixture.options);

    // The repository loses one file and edits another between the two runs.
    try tmp.dir.deleteFile(io, try std.fs.path.join(allocator, &.{ fixture.repo_name, "Data/Maps/dropped.map" }));
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.repo_name, "Data/Maps/edited.map" }),
        .data = "edited fixture, at a different length",
    });
    // The game writes a save into the staged tree; staging must not take it.
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ fixture.install_name, "Data/saves" }));
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.install_name, "Data/saves/profile.sav" }),
        .data = "player save",
    });

    try stage.stage(io, allocator, fixture.options);

    const destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    try expectStagedFile(destination, io, allocator, "Data/Maps/kept.map", "kept fixture");
    try expectStagedFile(destination, io, allocator, "Data/Maps/edited.map", "edited fixture, at a different length");
    try expectStagedPathAbsent(destination, io, "Data/Maps/dropped.map");
    try expectStagedFile(destination, io, allocator, "Data/saves/profile.sav", "player save");
}

test "restaging leaves a data file the repository has not touched" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    try stage.stage(io, allocator, fixture.options);

    // Same length and written after the staged copy, so staging has to read it
    // as current and skip it. Rewriting it would restore "kept fixture".
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.install_name, "Data/Maps/kept.map" }),
        .data = "KEPT FIXTURE",
    });

    try stage.stage(io, allocator, fixture.options);

    const destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    try expectStagedFile(destination, io, allocator, "Data/Maps/kept.map", "KEPT FIXTURE");
}

const RepositoryFixture = struct {
    repo_name: []const u8,
    install_name: []const u8,
    install_path: []const u8,
    options: stage.Options,
};

fn writeRepositoryFixture(io: std.Io, allocator: std.mem.Allocator, tmp: *std.testing.TmpDir) !RepositoryFixture {
    const fixture_root = try tmp.dir.realPathFileAlloc(io, ".", allocator);
    const repo_name = "repository";
    const install_name = "staged";
    const repo_path = try std.fs.path.join(allocator, &.{ fixture_root, repo_name });
    const install_path = try std.fs.path.join(allocator, &.{ fixture_root, install_name });

    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "zig-out/bin" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "zig-out/shaders" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/Configs" }));
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/Maps" }));
    const files = [_]struct { path: []const u8, data: []const u8 }{
        .{ .path = "zig-out/bin/Game", .data = "game fixture" },
        .{ .path = "Data/Configs/defconf.cfg", .data = "default fixture" },
        .{ .path = "LICENSE.md", .data = "license fixture" },
        .{ .path = "README.md", .data = "readme fixture" },
        .{ .path = "Data/Maps/kept.map", .data = "kept fixture" },
        .{ .path = "Data/Maps/edited.map", .data = "edited fixture" },
        .{ .path = "Data/Maps/dropped.map", .data = "dropped fixture" },
    };
    for (files) |file| try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, file.path }),
        .data = file.data,
    });

    return .{
        .repo_name = repo_name,
        .install_name = install_name,
        .install_path = install_path,
        .options = .{
            .repo_root = repo_path,
            .install_dir = install_path,
            .data_mode = .copy,
            .layout = .{
                .game_name = "Game",
                .runtime_files = &.{"Game"},
                .debug_files = &.{},
                .editors_supported = false,
            },
        },
    };
}

fn expectStagedFile(destination: std.Io.Dir, io: std.Io, allocator: std.mem.Allocator, path: []const u8, expected: []const u8) !void {
    const contents = try destination.readFileAlloc(io, path, allocator, .limited(1024));
    defer allocator.free(contents);
    try std.testing.expectEqualStrings(expected, contents);
}

fn expectStagedPathAbsent(destination: std.Io.Dir, io: std.Io, path: []const u8) !void {
    try std.testing.expectError(error.FileNotFound, destination.access(io, path, .{}));
}
