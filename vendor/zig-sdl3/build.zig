const std = @import("std");
const zig = @import("builtin");

pub const image = @import("build/image.zig");
pub const mixer = @import("build/mixer.zig");
pub const net = @import("build/net.zig");
pub const shadercross = @import("build/shadercross.zig");
pub const shaders = @import("build/shaders.zig");
pub const ttf = @import("build/ttf.zig");

pub const ExtensionConfig = @import("build/ExtensionConfig.zig");

/// List of example programs to compile from the examples folder.
const examples = [_][]const u8{
    "assert",
    "callbacks",
    "callbacks-main",
    "custom-allocator",
    "dialog",
    "filesystem",
    "hello-world",
    "log",
    "message-box",
    "mixer",
    "misc",
    "net",
    "properties",
    "shadercross",
    "storage",
    "tray",
    "ttf",
};

/// Configuration
pub const SdlConfig = struct {
    /// If to use the system's SDL include path.
    sdl_system_include_path: ?std.Build.LazyPath = null,
    /// Link system SDL instead of compiling our own.
    system_sdl: bool = false,
    /// Max stack size available for log messages.
    log_message_stack_size: usize = 1024,
    /// Max stack size available for renderer debug text.
    renderer_debug_text_stack_size: usize = 1024,
    /// If to use a special SDL main instead of a normal main.
    sdl3_main: bool = false,
    /// Enable the image extension.
    ext_image: ?image.Options = null,
    /// Enable the mixer extension.
    ext_mixer: ?mixer.Options = null,
    /// Enable the networking extension.
    ext_net: ?net.Options = null,
    /// Enable the shadercross extension.
    ext_shadercross: ?shadercross.Options = null,
    /// Enable DXC support for shadercross.
    ext_shadercross_dxc: bool = false,
    /// Enable the true type font extension.
    ext_ttf: ?ttf.Options = null,
    /// How to link C SDL.
    c_sdl_preferred_linkage: std.builtin.LinkMode = .static,
    /// If to strip C SDL.
    c_sdl_strip: bool = false,
    /// If to sanitize C SDL.
    c_sdl_sanitize_c: std.zig.SanitizeC = .trap,
    /// How to link C SDL.
    c_sdl_lto: std.zig.LtoMode = .none,
    /// If to allow emscripten pthreads for C SDL.
    c_sdl_emscripten_pthreads: bool = false,
    /// Install build config header for C SDL.
    c_sdl_install_build_config_h: bool = false,
};

/// Prepare an SDL3 module.
///
/// ## Function Parameters
/// * `b`: The build system.
/// * `cfg`: SDL build configuration.
/// * `target`: Target option.
/// * `optimize` Optimization option.
/// * `public`: If to export the SDL3 module publicly.
///
/// ## Return Value
/// Returns the created SDL3 module.
pub fn prepareSdl(
    b: *std.Build,
    cfg: SdlConfig,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    public: bool,
) *std.Build.Module {
    const c_source_code = b.fmt(
        \\#include <SDL3/SDL.h>
        \\{s}
        \\#include <SDL3/SDL_main.h>
        \\#include <SDL3/SDL_vulkan.h>
        \\
        \\{s}
        \\{s}
        \\{s}
        \\{s}
        \\{s}
    , .{
        if (!cfg.sdl3_main) "#define SDL_MAIN_NOIMPL\n" else "",
        if (cfg.ext_image != null) "#include <SDL3_image/SDL_image.h>\n" else "",
        if (cfg.ext_mixer != null) "#include <SDL3_mixer/SDL_mixer.h>\n" else "",
        if (cfg.ext_net != null) "#include <SDL3_net/SDL_net.h>\n" else "",
        if (cfg.ext_shadercross != null) "#include <SDL3_shadercross/SDL_shadercross.h>\n" else "",
        if (cfg.ext_ttf != null) "#include <SDL3_ttf/SDL_ttf.h>\n#include <SDL3_ttf/SDL_textengine.h>\n" else "",
    });

    const c_source_file_step = b.addWriteFiles();
    const c_source_path = c_source_file_step.add("c.c", c_source_code);

    const translate_c = b.addTranslateC(.{
        .root_source_file = c_source_path,
        .target = target,
        .optimize = optimize,
    });

    // Zig 0.16's Windows translate-c frontend does not accept the MSVC
    // `ui64` suffix used by SIZE_MAX in the Visual C++ limits.h header.
    if (target.result.os.tag == .windows)
        translate_c.defineCMacro("SIZE_MAX", "18446744073709551615ULL");

    if (cfg.sdl_system_include_path) |val|
        translate_c.addSystemIncludePath(val);

    const c_module = translate_c.createModule();

    const sdl3 = if (public) b.addModule("sdl3", .{
        .root_source_file = b.path("src/sdl3.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "c", .module = c_module },
        },
    }) else b.createModule(.{
        .root_source_file = b.path("src/sdl3.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "c", .module = c_module },
        },
    });

    if (cfg.sdl_system_include_path) |val|
        sdl3.addSystemIncludePath(val);

    const options = b.addOptions();
    options.addOption(bool, "main", cfg.sdl3_main);
    options.addOption(
        usize,
        "log_message_stack_size",
        cfg.log_message_stack_size,
    );
    options.addOption(
        usize,
        "renderer_debug_text_stack_size",
        cfg.renderer_debug_text_stack_size,
    );
    sdl3.addOptions("options", options);

    var sdl_dep_lib: ?*std.Build.Step.Compile = null;
    if (cfg.system_sdl) {
        sdl3.linkSystemLibrary("sdl3", .{});
        translate_c.linkSystemLibrary("sdl3", .{});
    } else {
        if (b.lazyDependency("sdl", .{
            .target = target,
            .optimize = optimize,
            .preferred_linkage = cfg.c_sdl_preferred_linkage,
            .strip = cfg.c_sdl_strip,
            .sanitize_c = cfg.c_sdl_sanitize_c,
            .lto = cfg.c_sdl_lto,
            .emscripten_pthreads = cfg.c_sdl_emscripten_pthreads,
            .install_build_config_h = cfg.c_sdl_install_build_config_h,
        })) |sdl_dep| {
            const lib = sdl_dep.artifact("SDL3");
            if (cfg.sdl_system_include_path) |val|
                lib.root_module.addSystemIncludePath(val);
            b.installArtifact(lib);
            translate_c.addIncludePath(sdl_dep.path("include"));
            sdl3.linkLibrary(lib);
            sdl_dep_lib = lib;
        }
    }

    const ext_linkage = if (cfg.system_sdl) .dynamic else cfg.c_sdl_preferred_linkage;

    const extension_config = ExtensionConfig{
        .linkage = ext_linkage,
        .sdl3 = sdl3,
        .sdl_dep_lib = sdl_dep_lib,
        .system_include_path = cfg.sdl_system_include_path,
        .translate_c = translate_c,
    };

    if (cfg.ext_image) |opts|
        image.setup(b, extension_config, opts, target, optimize);
    if (cfg.ext_mixer) |opts|
        mixer.setup(b, extension_config, opts, target, optimize);
    if (cfg.ext_net) |opts|
        net.setup(b, extension_config, opts, target, optimize);
    if (cfg.ext_shadercross) |opts|
        shadercross.setup(b, extension_config, opts, target, optimize, cfg.ext_shadercross_dxc);
    if (cfg.ext_ttf) |opts|
        ttf.setup(b, extension_config, opts, target, optimize);

    return sdl3;
}

pub fn build(
    b: *std.Build,
) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // C SDL options.
    const c_sdl_preferred_linkage = b.option(std.builtin.LinkMode, "c_sdl_preferred_linkage", "Prefer building statically or dynamically linked libraries (default: static)") orelse .static;
    const c_sdl_strip = b.option(bool, "c_sdl_strip", "Strip debug symbols (default: varies)") orelse (optimize == .ReleaseSmall);
    const c_sdl_sanitize_c = b.option(std.zig.SanitizeC, "c_sdl_sanitize_c", "Detect C undefined behavior (default: trap)") orelse .trap;
    const c_sdl_lto = b.option(std.zig.LtoMode, "c_sdl_lto", "Perform link time optimization (default: false)") orelse .none;
    const c_sdl_emscripten_pthreads = b.option(bool, "c_sdl_emscripten_pthreads", "Build with pthreads support when targeting Emscripten (default: false)") orelse false;
    const c_sdl_install_build_config_h = b.option(bool, "c_sdl_install_build_config_h", "Additionally install 'SDL_build_config.h' when installing SDL (default: false)") orelse false;
    const sdl_system_include_path = b.option(std.Build.LazyPath, "sdl_system_include_path", "System include path for SDL");
    const sdl_sysroot_path = b.option(std.Build.LazyPath, "sdl_sysroot_path", "System include path for SDL");

    if (sdl_sysroot_path) |val| {
        b.sysroot = val.getPath(b);
    }
    const system_sdl = b.systemIntegrationOption("sdl", .{});

    // SDL options.
    const log_message_stack_size = b.option(usize, "log_message_stack_size", "Default log message stack size") orelse 1024;
    const renderer_debug_text_stack_size = b.option(usize, "renderer_debug_text_stack_size", "Default renderer debug text stack size") orelse 1024;
    const sdl3_main = b.option(bool, "main", "Enable SDL main") orelse false;

    // SDL extension options.
    const ext_image = b.option(bool, "ext_image", "Enable SDL_image extension") orelse false;
    const ext_mixer = b.option(bool, "ext_mixer", "Enable SDL_mixer extension") orelse false;
    const ext_net = b.option(bool, "ext_net", "Enable SDL_net extension") orelse false;
    const ext_shadercross = b.option(bool, "ext_shadercross", "Enable SDL_shadercross extension") orelse false;
    const ext_shadercross_dxc = b.option(bool, "ext_shadercross_dxc", "Enable SDL_shadercross extension DXC support") orelse false;
    const ext_ttf = b.option(bool, "ext_ttf", "Enable SDL_ttf extension") orelse false;

    // SDL image options.
    var ext_image_opts = image.Options{};
    if (ext_image) {
        ext_image_opts.enable_bmp = b.option(bool, "image_enable_bmp", "Support loading BMP images") orelse true;
        ext_image_opts.enable_gif = b.option(bool, "image_enable_gif", "Support loading GIF images") orelse true;
        ext_image_opts.enable_jpg = b.option(bool, "image_enable_jpg", "Support loading JPEG images") orelse true;
        ext_image_opts.enable_lbm = b.option(bool, "image_enable_lbm", "Support loading LBM images") orelse true;
        ext_image_opts.enable_pcx = b.option(bool, "image_enable_pcx", "Support loading PCX images") orelse true;
        ext_image_opts.enable_png = b.option(bool, "image_enable_png", "Support loading PNG images") orelse true;
        ext_image_opts.enable_pnm = b.option(bool, "image_enable_pnm", "Support loading PNM images") orelse true;
        ext_image_opts.enable_qoi = b.option(bool, "image_enable_qoi", "Support loading QOI images") orelse true;
        ext_image_opts.enable_svg = b.option(bool, "image_enable_svg", "Support loading SVG images") orelse true;
        ext_image_opts.enable_tga = b.option(bool, "image_enable_tga", "Support loading TGA images") orelse true;
        ext_image_opts.enable_xcf = b.option(bool, "image_enable_xcf", "Support loading XCF images") orelse true;
        ext_image_opts.enable_xpm = b.option(bool, "image_enable_xpm", "Support loading XPM images") orelse true;
        ext_image_opts.enable_xv = b.option(bool, "image_enable_xv", "Support loading XV images") orelse true;
    }

    // SDL mixer options.
    var ext_mixer_opts = mixer.Options{};
    if (ext_mixer) {
        ext_mixer_opts.shared = b.option(bool, "mixer_shared", "Build SDL_mixer as a shared library") orelse false;
    }

    // SDL net options.
    var ext_net_opts = net.Options{};
    if (ext_net) {
        ext_net_opts.shared = b.option(bool, "net_shared", "Build SDL_net as a shared library") orelse false;
    }

    // SDL shadercross options.
    const ext_shadercross_opts = shadercross.Options{};

    // SDL TTF options.
    var ext_ttf_opts = ttf.Options{};
    if (ext_ttf) {
        ext_ttf_opts.enable_harfbuzz = b.option(bool, "ttf_enable_harfbuzz", "Use HarfBuzz to improve text shaping") orelse true;
    }

    var sdl_config = SdlConfig{
        .c_sdl_emscripten_pthreads = c_sdl_emscripten_pthreads,
        .c_sdl_install_build_config_h = c_sdl_install_build_config_h,
        .c_sdl_lto = c_sdl_lto,
        .c_sdl_preferred_linkage = c_sdl_preferred_linkage,
        .c_sdl_sanitize_c = c_sdl_sanitize_c,
        .c_sdl_strip = c_sdl_strip,
        .ext_image = if (ext_image) ext_image_opts else null,
        .ext_mixer = if (ext_mixer) ext_mixer_opts else null,
        .ext_net = if (ext_net) ext_net_opts else null,
        .ext_shadercross = if (ext_shadercross) ext_shadercross_opts else null,
        .ext_shadercross_dxc = ext_shadercross_dxc,
        .ext_ttf = if (ext_ttf) ext_ttf_opts else null,
        .log_message_stack_size = log_message_stack_size,
        .renderer_debug_text_stack_size = renderer_debug_text_stack_size,
        .sdl3_main = sdl3_main,
        .sdl_system_include_path = sdl_system_include_path,
        .system_sdl = system_sdl,
    };
    _ = prepareSdl(b, sdl_config, target, optimize, true);
    sdl_config.ext_image = if (ext_image) ext_image_opts else null;
    sdl_config.ext_mixer = if (ext_mixer) ext_mixer_opts else null;
    sdl_config.ext_net = if (ext_net) ext_net_opts else null;
    sdl_config.ext_shadercross = if (ext_shadercross) ext_shadercross_opts else null;
    sdl_config.ext_ttf = if (ext_ttf) ext_ttf_opts else null;
    const sdl3_full = prepareSdl(b, sdl_config, target, optimize, false);

    setupDocs(b, sdl3_full);
    setupExamples(b, sdl3_full, target, optimize);
    setupTest(b, sdl3_full);
}

fn setupDocs(
    b: *std.Build,
    sdl3: *std.Build.Module,
) void {
    const sdl3_lib = b.addLibrary(.{
        .root_module = sdl3,
        .name = "sdl3",
    });
    const docs = b.addInstallDirectory(.{
        .source_dir = sdl3_lib.getEmittedDocs(),
        .install_dir = .{ .prefix = {} },
        .install_subdir = "docs",
    });
    const docs_step = b.step("docs", "Generate library documentation");
    docs_step.dependOn(&docs.step);
}

fn setupExample(
    b: *std.Build,
    sdl3: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    name: []const u8,
    examples_step: *std.Build.Step,
) struct { *std.Build.Step.Compile, *std.Build.Step.InstallArtifact } {
    const exe_module = b.createModule(.{
        .root_source_file = b.path(b.fmt("examples/{s}.zig", .{name})),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "sdl3", .module = sdl3 },
        },
    });
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = exe_module,
    });
    const example_install = b.addInstallArtifact(exe, .{});
    examples_step.dependOn(&example_install.step);
    return .{ exe, example_install };
}

fn runExample(
    b: *std.Build,
    example_compile: *std.Build.Step.Compile,
    example_install: *std.Build.Step.InstallArtifact,
    run_examples_step: *std.Build.Step,
) void {
    const run_art = b.addRunArtifact(example_compile);
    run_art.step.dependOn(&example_install.step);
    run_examples_step.dependOn(&run_art.step);
}

fn setupExamples(
    b: *std.Build,
    sdl3: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    const examples_step = b.step("examples", "Build all examples");
    const run_example: ?[]const u8 = b.option([]const u8, "example", "The example name for running an example") orelse null;
    const run_examples_step = b.step("run", "Run an example with -Dexample=<example_name> option");

    for (examples) |example| {
        const example_compile, const example_install = setupExample(
            b,
            sdl3,
            target,
            optimize,
            example,
            examples_step,
        );
        if (run_example) |run_example_name| {
            if (std.mem.eql(u8, example, run_example_name)) {
                runExample(b, example_compile, example_install, run_examples_step);
            }
        }
    }
}

fn setupTest(
    b: *std.Build,
    sdl3: *std.Build.Module,
) void {
    const tst = b.addTest(.{
        .name = "sdl3",
        .root_module = sdl3,
    });
    const tst_run = b.addRunArtifact(tst);
    const tst_step = b.step("test", "Run all tests");
    tst_step.dependOn(&tst_run.step);
}
