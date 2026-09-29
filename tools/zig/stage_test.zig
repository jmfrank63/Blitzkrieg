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
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ repo_name, "Data/Objects/SimpleObjects/common/summer/logs/01" }));
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
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/THIRD-PARTY-NOTICES.txt" }),
        .data = "notices fixture",
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
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ repo_name, "Data/Objects/SimpleObjects/common/summer/logs/01/1.xml" }),
        .data = "log pile fixture",
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
    try expectStagedFile(destination, io, allocator, "THIRD-PARTY-NOTICES.txt", "notices fixture");
    try expectStagedFile(destination, io, allocator, "README.md", "readme fixture");
    try expectStagedFile(destination, io, allocator, "Data/Objects/SimpleObjects/common/summer/logs/01/1.xml", "log pile fixture");
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
        .{ .path = "Data/THIRD-PARTY-NOTICES.txt", .data = "notices fixture" },
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

test "--map-editor stages the file beside the game" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    // Repo-relative, the same form a hand-run `zig build package-game`
    // could pass; build.zig itself passes an absolute cache path
    // (copyMapEditor's other branch), exercised implicitly by every
    // package-game/package-game-editors build.
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.repo_name, "zig-out/bin/MapEditor" }),
        .data = "map editor fixture",
    });
    var options = fixture.options;
    options.map_editor = "zig-out/bin/MapEditor";
    try stage.stage(io, allocator, options);

    const destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    try expectStagedFile(destination, io, allocator, "MapEditor", "map editor fixture");
}

test "a missing map editor binary fails the stage naming the path" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    var options = fixture.options;
    options.map_editor = "zig-out/bin/NoSuchMapEditor";
    try std.testing.expectError(error.MissingMapEditor, stage.stage(io, allocator, options));
}

test "a mods directory in the repository is never staged" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    // Mirrors the real layout (Sources/src/Main/MainLoopCommands.cpp): mods
    // live at <install>/mods/<Name>/data, a sibling of Data, never a member
    // of it - nothing in stage.zig walks or copies this tree today, and this
    // test keeps it that way, including for the unlicensed AchtungPanzer2.
    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ fixture.repo_name, "mods/SomeMod/data" }));
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.repo_name, "mods/SomeMod/data/mod.xml" }),
        .data = "<mod/>",
    });

    try stage.stage(io, allocator, fixture.options);

    const destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    try expectStagedPathAbsent(destination, io, "mods");
    try expectStagedPathAbsent(destination, io, "mods/SomeMod/data/mod.xml");
}

fn expectStagedFile(destination: std.Io.Dir, io: std.Io, allocator: std.mem.Allocator, path: []const u8, expected: []const u8) !void {
    const contents = try destination.readFileAlloc(io, path, allocator, .limited(1024));
    defer allocator.free(contents);
    try std.testing.expectEqualStrings(expected, contents);
}

fn expectStagedPathAbsent(destination: std.Io.Dir, io: std.Io, path: []const u8) !void {
    try std.testing.expectError(error.FileNotFound, destination.access(io, path, .{}));
}

test "--season-data stages the generated textures beside Data, in both data modes" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    const generated = try std.fs.path.join(allocator, &.{ try tmp.dir.realPathFileAlloc(io, ".", allocator), "generated" });
    try tmp.dir.createDirPath(io, "generated/Units/Tank");
    try tmp.dir.writeFile(io, .{ .sub_path = "generated/Units/Tank/1w_h.dds", .data = "winter A" });
    try tmp.dir.writeFile(io, .{ .sub_path = "generated/Units/Tank/1a_h.dds", .data = "africa" });
    var options = fixture.options;
    options.season_data = generated;
    try stage.stage(io, allocator, options);

    var destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    try expectStagedFile(destination, io, allocator, "SeasonData/Units/Tank/1w_h.dds", "winter A");
    try expectStagedFile(destination, io, allocator, "SeasonData/Units/Tank/1a_h.dds", "africa");
    // Never into Data: under --link-data that is the repository's own tree.
    try expectStagedPathAbsent(destination, io, "Data/Units/Tank/1w_h.dds");
    destination.close(io);

    // Regenerated: one file changes at the same size (as a season file keeps
    // its summer file's), one is no longer made. Staged under --link-data this
    // time, where SeasonData is still a copy and Data a link.
    try tmp.dir.writeFile(io, .{ .sub_path = "generated/Units/Tank/1w_h.dds", .data = "winter B" });
    try tmp.dir.deleteFile(io, "generated/Units/Tank/1a_h.dds");
    options.data_mode = .link;
    stage.stage(io, allocator, options) catch |err| switch (err) {
        // A Windows runner without symlink rights cannot take --link-data.
        error.DataLinkPermissionDenied => return,
        else => return err,
    };
    destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    try expectStagedFile(destination, io, allocator, "SeasonData/Units/Tank/1w_h.dds", "winter B");
    try expectStagedPathAbsent(destination, io, "SeasonData/Units/Tank/1a_h.dds");
    try expectStagedFile(destination, io, allocator, "Data/Maps/kept.map", "kept fixture");
    try std.testing.expectError(error.FileNotFound, tmp.dir.access(io, try std.fs.path.join(allocator, &.{ fixture.repo_name, "Data/Units" }), .{}));
}

test "a missing season data directory fails the stage" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    var options = fixture.options;
    options.season_data = "no-such-season-data";
    try std.testing.expectError(error.FileNotFound, stage.stage(io, allocator, options));
}

test "a runtime file that is not built yet fails the stage naming it" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    // No zig-out/lib at all, then an empty one: both are how a tree looks while
    // the installs a stage-game run was not ordered after are still running.
    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    var options = fixture.options;
    options.layout.runtime_files = &.{ "Game", "libSDL3.dylib" };
    try std.testing.expectError(error.MissingRuntimeFile, stage.stage(io, allocator, options));

    try tmp.dir.createDirPath(io, try std.fs.path.join(allocator, &.{ fixture.repo_name, "zig-out/lib" }));
    try std.testing.expectError(error.MissingRuntimeFile, stage.stage(io, allocator, options));

    // Once the library is installed, the same run stages it out of zig-out/lib.
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.repo_name, "zig-out/lib/libSDL3.dylib" }),
        .data = "sdl fixture",
    });
    try stage.stage(io, allocator, options);
    const destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    try expectStagedFile(destination, io, allocator, "libSDL3.dylib", "sdl fixture");
}

test "the Windows release layout stages without the Debug-only program databases" {
    const io = std.testing.io;
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    // What package-game(-editors) --release=fast passes on Windows: every
    // runtime file is required, a .pdb the optimised build did not write is not.
    const fixture = try writeRepositoryFixture(io, allocator, &tmp);
    const runtime_files = [_][]const u8{ "Game.exe", "PlatformRuntime.dll", "SDL3.dll", "rclone.exe" };
    for (runtime_files) |name| try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.repo_name, "zig-out/bin", name }),
        .data = name,
    });
    try tmp.dir.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ fixture.repo_name, "zig-out/bin/MapEditor.exe" }),
        .data = "map editor fixture",
    });
    var options = fixture.options;
    options.map_editor = "zig-out/bin/MapEditor.exe";
    options.layout = .{
        .game_name = "Game.exe",
        .runtime_files = &runtime_files,
        .debug_files = &.{ "Game.pdb", "SDL3.pdb" },
        .editors_supported = true,
    };
    try stage.stage(io, allocator, options);

    const destination = try std.Io.Dir.cwd().openDir(io, fixture.install_path, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);
    for (runtime_files) |name| try expectStagedFile(destination, io, allocator, name, name);
    try expectStagedFile(destination, io, allocator, "MapEditor.exe", "map editor fixture");
    try expectStagedPathAbsent(destination, io, "Game.pdb");
}

// stage.zig reads zig-out/bin, zig-out/lib and zig-out/shaders by plain path,
// which the build graph cannot see, so each stage-game run has to be ordered
// after game-all and the shader compile by hand. The package runs were not,
// and a --release=fast package on a fresh Windows tree raced the installs
// into FileNotFound. addStageGameRun is the one place that orders them; this
// holds build.zig to it.
test "every stage-game run in build.zig is ordered after game-all and the shaders" {
    const text = try std.Io.Dir.cwd().readFileAlloc(std.testing.io, "build.zig", std.testing.allocator, .limited(20 * 1024 * 1024));
    defer std.testing.allocator.free(text);

    try std.testing.expect(std.mem.indexOf(u8, text, "addRunArtifact(stage_tool)") == null);
    const helper_start = std.mem.indexOf(u8, text, "\nfn addStageGameRun(") orelse return error.MissingStageGameHelper;
    // The function ends at its column-0 brace; "\n}" rather than "\n}\n" because
    // CI checks build.zig out with CRLF line endings.
    const helper_end = std.mem.indexOfPos(u8, text, helper_start + 1, "\n}") orelse return error.MissingStageGameHelper;
    const helper = text[helper_start..helper_end];
    try std.testing.expect(std.mem.indexOf(u8, helper, "run.step.dependOn(inputs.game_all_step);") != null);
    try std.testing.expect(std.mem.indexOf(u8, helper, "run.step.dependOn(shaders_step);") != null);
    // install-game, package-game and package-game-editors.
    try std.testing.expectEqual(@as(usize, 4), std.mem.count(u8, text, "addStageGameRun("));
    try std.testing.expectEqual(@as(usize, 1), std.mem.count(u8, text, "b.addRunArtifact(inputs.tool)"));
}
