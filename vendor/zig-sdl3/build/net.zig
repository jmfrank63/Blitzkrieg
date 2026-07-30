const ExtensionConfig = @import("ExtensionConfig.zig");
const std = @import("std");

/// Options for SDL net.
pub const Options = struct {
    shared: bool = false,
};

pub fn setup(
    b: *std.Build,
    extension_config: ExtensionConfig,
    options: Options,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    const upstream = b.lazyDependency("sdl_net", .{}) orelse return;
    const native_os = target.result.os.tag;

    const lib_name = "SDL3_net";
    const version = std.SemanticVersion.parse("3.0.0") catch unreachable;

    // Library.
    const lib = b.addLibrary(.{
        .name = lib_name,
        .version = version,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
        .linkage = extension_config.linkage,
    });

    if (extension_config.system_include_path) |val| {
        lib.root_module.addSystemIncludePath(val);
    }

    var lib_c_flags: std.ArrayListUnmanaged([]const u8) = .empty;
    defer lib_c_flags.deinit(b.allocator);
    lib_c_flags.appendSlice(b.allocator, &.{"-std=c99"}) catch @panic("OOM");

    lib.root_module.addCSourceFile(.{
        .file = upstream.path("src/SDL_net.c"),
        .flags = lib_c_flags.items,
    });
    // Headers.
    extension_config.translate_c.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(upstream.path("include"));
    // Defines.
    lib.root_module.addCMacro("BUILD_SDL", "1");
    lib.root_module.addCMacro("SDL_BUILD_MAJOR_VERSION", b.fmt("{d}", .{version.major}));
    lib.root_module.addCMacro("SDL_BUILD_MINOR_VERSION", b.fmt("{d}", .{version.minor}));
    lib.root_module.addCMacro("SDL_BUILD_MICRO_VERSION", b.fmt("{d}", .{version.patch}));
    if (options.shared and native_os == .windows) {
        lib.root_module.addCMacro("DLL_EXPORT", "");
    }
    if (native_os != .windows and native_os != .haiku) {
        lib.root_module.addCMacro("_DEFAULT_SOURCE", "");
    }
    if (extension_config.sdl_dep_lib) |sdl_lib| {
        lib.root_module.linkLibrary(sdl_lib);
    } else lib.root_module.linkSystemLibrary("sdl3", .{});
    // Linking.
    if (native_os == .windows) {
        lib.root_module.linkSystemLibrary("iphlpapi", .{});
        lib.root_module.linkSystemLibrary("ws2_32", .{});
        if (options.shared) {
            lib.root_module.addWin32ResourceFile(.{ .file = upstream.path("src/version.rc") });
        }
    } else if (native_os == .haiku) {
        lib.root_module.linkSystemLibrary("network", .{});
    }
    // Linker version.
    if (target.result.ofmt == .elf or target.result.ofmt == .macho) {
        lib.setVersionScript(upstream.path("src/SDL_net.sym"));
    }
    if (options.shared) {
        lib.linker_allow_shlib_undefined = false;
    }

    b.installArtifact(lib);
    // Installation.
    const install_header = b.addInstallHeaderFile(upstream.path("include/SDL3_net/SDL_net.h"), "SDL3_net/SDL_net.h");
    b.getInstallStep().dependOn(&install_header.step);
    const install_license = b.addInstallFile(upstream.path("LICENSE.txt"), "share/licenses/SDL3_net/LICENSE.txt");
    b.getInstallStep().dependOn(&install_license.step);

    extension_config.sdl3.linkLibrary(lib);
    extension_config.sdl3.addIncludePath(upstream.path("include"));
}
