const ExtensionConfig = @import("ExtensionConfig.zig");
const std = @import("std");

/// Options for SDL TTF.
pub const Options = struct {
    enable_harfbuzz: bool = true,
};

// https://github.com/allyourcodebase/SDL_ttf/blob/main/build.zig
pub fn setup(
    b: *std.Build,
    extension_config: ExtensionConfig,
    options: Options,
    target: std.Build.ResolvedTarget,
    optimizer: std.builtin.OptimizeMode,
) void {
    _ = optimizer;

    const optimize: std.builtin.OptimizeMode = .ReleaseFast; // https://github.com/libsdl-org/SDL_ttf/issues/566 (ReleaseFast prevents UBSAN from running)

    const upstream = b.lazyDependency("sdl_ttf", .{}) orelse return;

    const lib = b.addLibrary(.{
        .name = "SDL3_ttf",
        .version = .{ .major = 3, .minor = 2, .patch = 2 },
        .linkage = extension_config.linkage,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    if (extension_config.system_include_path) |val| {
        lib.root_module.addSystemIncludePath(val);
    }

    extension_config.translate_c.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(upstream.path("src"));
    lib.root_module.addCSourceFiles(.{
        .root = upstream.path("src"),
        .files = srcs,
    });

    if (options.enable_harfbuzz) {
        const harfbuzz_dep = b.dependency("harfbuzz", .{
            .target = target,
            .optimize = optimize,
        });
        lib.root_module.linkLibrary(harfbuzz_dep.artifact("harfbuzz"));
        lib.root_module.addCMacro("TTF_USE_HARFBUZZ", "1");
    }

    const freetype_dep = b.dependency("freetype", .{
        .target = target,
        .optimize = optimize,
    });
    lib.root_module.linkLibrary(freetype_dep.artifact("freetype"));

    if (extension_config.sdl_dep_lib) |sdl_lib| {
        lib.root_module.linkLibrary(sdl_lib);
    } else lib.root_module.linkSystemLibrary("sdl3", .{});
    lib.installHeadersDirectory(upstream.path("include"), "", .{});

    b.installArtifact(lib);

    extension_config.sdl3.linkLibrary(lib);
    extension_config.sdl3.addIncludePath(upstream.path("include"));
}

const srcs: []const []const u8 = &.{
    "SDL_gpu_textengine.c",
    "SDL_hashtable.c",
    "SDL_hashtable_ttf.c",
    "SDL_renderer_textengine.c",
    "SDL_surface_textengine.c",
    "SDL_ttf.c",
};
