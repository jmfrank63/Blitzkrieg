const ExtensionConfig = @import("ExtensionConfig.zig");
const std = @import("std");

/// Options for SDL image.
pub const Options = struct {
    enable_bmp: bool = true,
    enable_gif: bool = true,
    enable_jpg: bool = true,
    enable_lbm: bool = true,
    enable_pcx: bool = true,
    enable_png: bool = true,
    enable_pnm: bool = true,
    enable_qoi: bool = true,
    enable_svg: bool = true,
    enable_tga: bool = true,
    enable_xcf: bool = true,
    enable_xpm: bool = true,
    enable_xv: bool = true,
};

// Most of this is copied from https://github.com/allyourcodebase/SDL_image/blob/main/build.zig.
pub fn setup(
    b: *std.Build,
    extension_config: ExtensionConfig,
    options: Options,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    const upstream = b.lazyDependency("sdl_image", .{}) orelse return;

    const lib = b.addLibrary(.{
        .name = "SDL3_image",
        .version = .{ .major = 3, .minor = 2, .patch = 4 },
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
    if (extension_config.sdl_dep_lib) |sdl_lib| {
        lib.root_module.linkLibrary(sdl_lib);
    } else lib.root_module.linkSystemLibrary("sdl3", .{});

    // Use stb_image for loading JPEG and PNG files. Native alternatives such as
    // Windows Imaging Component and Apple's Image I/O framework are not yet
    // supported by this build script.
    lib.root_module.addCMacro("USE_STBIMAGE", "");

    // The following are options for supported file formats. AVIF, JXL, TIFF,
    // and WebP are not yet supported by this build script, as they require
    // additional dependencies.
    if (options.enable_bmp)
        lib.root_module.addCMacro("LOAD_BMP", "");
    if (options.enable_gif)
        lib.root_module.addCMacro("LOAD_GIF", "");
    if (options.enable_jpg)
        lib.root_module.addCMacro("LOAD_JPG", "");
    if (options.enable_lbm)
        lib.root_module.addCMacro("LOAD_LBM", "");
    if (options.enable_pcx)
        lib.root_module.addCMacro("LOAD_PCX", "");
    if (options.enable_png)
        lib.root_module.addCMacro("LOAD_PNG", "");
    if (options.enable_pnm)
        lib.root_module.addCMacro("LOAD_PNM", "");
    if (options.enable_qoi)
        lib.root_module.addCMacro("LOAD_QOI", "");
    if (options.enable_svg)
        lib.root_module.addCMacro("LOAD_SVG", "");
    if (options.enable_tga)
        lib.root_module.addCMacro("LOAD_TGA", "");
    if (options.enable_xcf)
        lib.root_module.addCMacro("LOAD_XCF", "");
    if (options.enable_xpm)
        lib.root_module.addCMacro("LOAD_XPM", "");
    if (options.enable_xv)
        lib.root_module.addCMacro("LOAD_XV", "");

    extension_config.translate_c.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(upstream.path("src"));

    lib.root_module.addCSourceFiles(.{
        .root = upstream.path("src"),
        .files = &.{
            "IMG.c",
            "IMG_WIC.c",
            "IMG_avif.c",
            "IMG_bmp.c",
            "IMG_gif.c",
            "IMG_jpg.c",
            "IMG_jxl.c",
            "IMG_lbm.c",
            "IMG_pcx.c",
            "IMG_png.c",
            "IMG_pnm.c",
            "IMG_qoi.c",
            "IMG_stb.c",
            "IMG_svg.c",
            "IMG_tga.c",
            "IMG_tif.c",
            "IMG_webp.c",
            "IMG_xcf.c",
            "IMG_xpm.c",
            "IMG_xv.c",
        },
    });

    if (target.result.os.tag == .macos) {
        lib.root_module.addCSourceFile(.{
            .file = upstream.path("src/IMG_ImageIO.m"),
        });
        lib.root_module.linkFramework("Foundation", .{});
        lib.root_module.linkFramework("ApplicationServices", .{});
    }

    lib.installHeadersDirectory(upstream.path("include"), "", .{});

    extension_config.sdl3.linkLibrary(lib);
}
