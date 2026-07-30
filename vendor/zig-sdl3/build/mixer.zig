const ExtensionConfig = @import("ExtensionConfig.zig");
const std = @import("std");

/// Options for SDL mixer.
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
    const upstream = b.lazyDependency("sdl_mixer", .{}) orelse return;
    const native_os = target.result.os.tag;

    const lib_name = "SDL3_mixer";
    const version = std.SemanticVersion.parse("3.2.2") catch unreachable;

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

    lib.root_module.addCSourceFiles(.{
        .root = upstream.path("src"),
        .files = srcs,
    });

    // Headers.
    extension_config.translate_c.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(upstream.path("include"));
    lib.root_module.addIncludePath(upstream.path("src"));

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

    // The following are the decoders that can be built without any additional
    // dependencies, using the codec libraries vendored by SDL_mixer itself
    // (dr_libs, stb_vorbis, timidity). RAW and SINEWAVE are always enabled.
    // The remaining decoders (libvorbis, opus, libFLAC, mpg123, libxmp,
    // wavpack, game-music-emu, FluidSynth) are not yet supported by this build
    // script, as they require additional dependencies; the source files for
    // them are compiled but disable themselves when their macro is undefined.
    lib.root_module.addCMacro("DECODER_WAV", "");
    lib.root_module.addCMacro("DECODER_AIFF", "");
    lib.root_module.addCMacro("DECODER_AU", "");
    lib.root_module.addCMacro("DECODER_VOC", "");
    lib.root_module.addCMacro("DECODER_OGGVORBIS_STB", "");
    lib.root_module.addCMacro("DECODER_FLAC_DRFLAC", "");
    lib.root_module.addCMacro("DECODER_MP3_DRMP3", "");
    lib.root_module.addCMacro("DECODER_MIDI_TIMIDITY", "");
    if (extension_config.sdl_dep_lib) |sdl_lib| {
        lib.root_module.linkLibrary(sdl_lib);
    } else lib.root_module.linkSystemLibrary("sdl3", .{});

    // Linking.
    if (options.shared and native_os == .windows) {
        lib.root_module.addWin32ResourceFile(.{ .file = upstream.path("src/version.rc") });
    }

    // Linker version.
    if (target.result.ofmt == .elf or target.result.ofmt == .macho) {
        lib.setVersionScript(upstream.path("src/SDL_mixer.sym"));
    }
    if (options.shared) {
        lib.linker_allow_shlib_undefined = false;
    }

    b.installArtifact(lib);

    // Installation.
    const install_header = b.addInstallHeaderFile(upstream.path("include/SDL3_mixer/SDL_mixer.h"), "SDL3_mixer/SDL_mixer.h");
    b.getInstallStep().dependOn(&install_header.step);
    const install_license = b.addInstallFile(upstream.path("LICENSE.txt"), "share/licenses/SDL3_mixer/LICENSE.txt");
    b.getInstallStep().dependOn(&install_license.step);

    extension_config.sdl3.linkLibrary(lib);
    extension_config.sdl3.addIncludePath(upstream.path("include"));
}

const srcs: []const []const u8 = &.{
    "SDL_mixer.c",
    "SDL_mixer_metadata_tags.c",
    "SDL_mixer_spatialization.c",
    "decoder_aiff.c",
    "decoder_au.c",
    "decoder_drflac.c",
    "decoder_drmp3.c",
    "decoder_flac.c",
    "decoder_fluidsynth.c",
    "decoder_gme.c",
    "decoder_mpg123.c",
    "decoder_opus.c",
    "decoder_raw.c",
    "decoder_sinewave.c",
    "decoder_stb_vorbis.c",
    "decoder_timidity.c",
    "decoder_voc.c",
    "decoder_vorbis.c",
    "decoder_wav.c",
    "decoder_wavpack.c",
    "decoder_xmp.c",
    "timidity/common.c",
    "timidity/instrum.c",
    "timidity/mix.c",
    "timidity/output.c",
    "timidity/playmidi.c",
    "timidity/readmidi.c",
    "timidity/resample.c",
    "timidity/tables.c",
    "timidity/timidity.c",
};
