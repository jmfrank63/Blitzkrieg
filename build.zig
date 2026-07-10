const std = @import("std");

const cflags_debug = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-Wno-deprecated-non-prototype",
};

const cflags_release = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-Wno-deprecated-non-prototype",
};

const cppflags_debug = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_ASSERT_SLOW",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const cppflags_release = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const cppflags_beta_debug = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-D_DEBUG",
    "-D_DO_ASSERT_SLOW",
    "-D_DO_CHECKED_CAST",
    "-D_STL_RANGE_CHECK",
    "-D_MT",
    "-D_DLL",
    "-D_DO_BETA_CHECK",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const cppflags_beta_release = &.{
    "-D_WINDOWS",
    "-DWIN32",
    "-DNDEBUG",
    "-D_FINALRELEASE",
    "-D_MT",
    "-D_DLL",
    "-D_DO_BETA_CHECK",
    "-fms-extensions",
    "-fdelayed-template-parsing",
    "-Wno-deprecated-declarations",
    "-Wno-microsoft-template",
    "-Wno-nonportable-include-path",
    "-Wno-reserved-user-defined-literal",
    "-Wno-switch",
    "-Wno-unused-command-line-argument",
};

const zlib_sources = &.{
    "Sources/src/zlib/adler32.c",
    "Sources/src/zlib/compress.c",
    "Sources/src/zlib/crc32.c",
    "Sources/src/zlib/deflate.c",
    "Sources/src/zlib/gzio.c",
    "Sources/src/zlib/infblock.c",
    "Sources/src/zlib/infcodes.c",
    "Sources/src/zlib/inffast.c",
    "Sources/src/zlib/inflate.c",
    "Sources/src/zlib/inftrees.c",
    "Sources/src/zlib/infutil.c",
    "Sources/src/zlib/trees.c",
    "Sources/src/zlib/uncompr.c",
    "Sources/src/zlib/zutil.c",
};

const libpng_sources = &.{
    "Sources/src/libpng/png.c",
    "Sources/src/libpng/pngerror.c",
    "Sources/src/libpng/pnggccrd.c",
    "Sources/src/libpng/pngget.c",
    "Sources/src/libpng/pngmem.c",
    "Sources/src/libpng/pngpread.c",
    "Sources/src/libpng/pngread.c",
    "Sources/src/libpng/pngrio.c",
    "Sources/src/libpng/pngrtran.c",
    "Sources/src/libpng/pngrutil.c",
    "Sources/src/libpng/pngset.c",
    "Sources/src/libpng/pngtest.c",
    "Sources/src/libpng/pngtrans.c",
    "Sources/src/libpng/pngvcrd.c",
    "Sources/src/libpng/pngwio.c",
    "Sources/src/libpng/pngwrite.c",
    "Sources/src/libpng/pngwtran.c",
    "Sources/src/libpng/pngwutil.c",
};

const misc_sources = &.{
    "Sources/src/Misc/FileUtils.cpp",
    "Sources/src/Misc/FreeIDs.cpp",
    "Sources/src/Misc/GRect.cpp",
    "Sources/src/Misc/HPTimer.cpp",
    "Sources/src/Misc/Spline.cpp",
    "Sources/src/Misc/StrProc.cpp",
    "Sources/src/Misc/Win32Random.cpp",
    "Sources/src/Misc/StdAfx.cpp",
    "Sources/src/Misc/BasicObjectFactory.cpp",
    "Sources/src/Misc/Manipulator.cpp",
    "Sources/src/Misc/MemorySystem.cpp",
    "Sources/src/Misc/Thread.cpp",
};

const image_sources = &.{
    "Sources/src/Image/ImageBMP.cpp",
    "Sources/src/Image/ImageMMP.cpp",
    "Sources/src/Image/ImageObjectFactory.cpp",
    "Sources/src/Image/ImagePNG.cpp",
    "Sources/src/Image/ImageProcessor.cpp",
    "Sources/src/Image/ImageReal.cpp",
    "Sources/src/Image/DxtCodec.cpp",
    "Sources/src/Image/ImageScale.cpp",
    "Sources/src/Image/ImageTGA.cpp",
    "Sources/src/Image/RectsComposition.cpp",
    "Sources/src/Image/GlobalsLoader.cpp",
    "Sources/src/Image/StdAfx.cpp",
};

const lualib_c_sources = &.{
    "Sources/src/LuaLib/LuaSrc/lapi.c",
    "Sources/src/LuaLib/LuaSrc/lcode.c",
    "Sources/src/LuaLib/LuaSrc/ldebug.c",
    "Sources/src/LuaLib/LuaSrc/ldo.c",
    "Sources/src/LuaLib/LuaSrc/lfunc.c",
    "Sources/src/LuaLib/LuaSrc/lgc.c",
    "Sources/src/LuaLib/LuaSrc/llex.c",
    "Sources/src/LuaLib/LuaSrc/lmem.c",
    "Sources/src/LuaLib/LuaSrc/lobject.c",
    "Sources/src/LuaLib/LuaSrc/lparser.c",
    "Sources/src/LuaLib/LuaSrc/lstate.c",
    "Sources/src/LuaLib/LuaSrc/lstring.c",
    "Sources/src/LuaLib/LuaSrc/ltable.c",
    "Sources/src/LuaLib/LuaSrc/ltm.c",
    "Sources/src/LuaLib/LuaSrc/lundump.c",
    "Sources/src/LuaLib/LuaSrc/lvm.c",
    "Sources/src/LuaLib/LuaSrc/lzio.c",
};

const lualib_cpp_sources = &.{
    "Sources/src/LuaLib/Script.cpp",
};

const net_sources = &.{
    "Sources/src/Net/GlobalsLoader.cpp",
    "Sources/src/Net/StdAfx.cpp",
    "Sources/src/Net/NetA4.cpp",
    "Sources/src/Net/NetAcks.cpp",
    "Sources/src/Net/NetConnection.cpp",
    "Sources/src/Net/NetDriverConsts.cpp",
    "Sources/src/Net/NetLogin.cpp",
    "Sources/src/Net/NetLowest.cpp",
    "Sources/src/Net/NetPeer2Peer.cpp",
    "Sources/src/Net/NetServerInfo.cpp",
    "Sources/src/Net/NetStream.cpp",
    "Sources/src/Net/NetObjectFactory.cpp",
    "Sources/src/Net/Streams.cpp",
};

const buildversion_sources = &.{
    "Sources/src/buildversion/StdAfx.cpp",
    "Sources/src/buildversion/BuildVersion.cpp",
    "Sources/src/buildversion/main.cpp",
    "Sources/src/buildversion/StringTokenizer.cpp",
};

const betakeygen_sources = &.{
    "Sources/src/betakeygen/StdAfx.cpp",
    "Sources/src/betakeygen/BetaKey.cpp",
    "Sources/src/betakeygen/main.cpp",
};

const input_sources = &.{
    "Sources/src/Input/GlobalsLoader.cpp",
    "Sources/src/Input/StdAfx.cpp",
    "Sources/src/Input/InputAPI.cpp",
    "Sources/src/Input/InputBinder.cpp",
    "Sources/src/Input/InputObjectFactory.cpp",
    "Sources/src/Input/InputSlider.cpp",
    "Sources/src/Input/Visitors.cpp",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .x86,
            .os_tag = .windows,
            .abi = .msvc,
        },
    });
    const optimize = b.standardOptimizeOption(.{});
    const toolchain = ToolchainIncludes{
        .msvc_include = b.option([]const u8, "msvc-include", "MSVC C/C++ include directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\include",
        .windows_sdk_include = b.option([]const u8, "windows-sdk-include", "Windows SDK include version directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.26100.0",
        .msvc_lib = b.option([]const u8, "msvc-lib", "MSVC x86 library directory") orelse "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\lib\\x86",
        .windows_sdk_lib = b.option([]const u8, "windows-sdk-lib", "Windows SDK library version directory") orelse "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0",
    };

    const zlib = addZlib(b, target, optimize, toolchain);
    const libpng = addLibpng(b, target, optimize, toolchain, zlib);
    const misc = addMisc(b, target, optimize, toolchain);
    const image = addImage(b, target, optimize, toolchain, zlib, libpng, misc);
    const lualib = addLuaLib(b, target, optimize, toolchain);
    const net = addNet(b, target, optimize, toolchain, misc);
    const buildversion = addBuildVersion(b, target, optimize, toolchain, misc);
    const betakeygen = addBetaKeyGen(b, target, optimize, toolchain, zlib, misc);
    const input = addInput(b, target, optimize, toolchain, misc);

    b.installArtifact(zlib);
    b.installArtifact(libpng);
    b.installArtifact(misc);
    b.installArtifact(image);
    b.installArtifact(lualib);
    b.installArtifact(net);
    b.installArtifact(buildversion);
    b.installArtifact(betakeygen);
    b.installArtifact(input);

    const zlib_step = b.step("zlib", "Build the zlib static library");
    zlib_step.dependOn(&b.addInstallArtifact(zlib, .{}).step);

    const libpng_step = b.step("libpng", "Build the libpng static library");
    libpng_step.dependOn(&b.addInstallArtifact(libpng, .{}).step);

    const misc_step = b.step("misc", "Build the Misc static library");
    misc_step.dependOn(&b.addInstallArtifact(misc, .{}).step);

    const image_step = b.step("image", "Build the Image dynamic library");
    image_step.dependOn(&b.addInstallArtifact(image, .{}).step);

    const lualib_step = b.step("lualib", "Build the LuaLib static library");
    lualib_step.dependOn(&b.addInstallArtifact(lualib, .{}).step);

    const net_step = b.step("net", "Build the Net dynamic library");
    net_step.dependOn(&b.addInstallArtifact(net, .{}).step);

    const buildversion_step = b.step("buildversion", "Build the BuildVersion console utility");
    buildversion_step.dependOn(&b.addInstallArtifact(buildversion, .{}).step);

    const betakeygen_step = b.step("betakeygen", "Build the BetaKeyGen console utility");
    betakeygen_step.dependOn(&b.addInstallArtifact(betakeygen, .{}).step);

    const input_step = b.step("input", "Build the Input dynamic library");
    input_step.dependOn(&b.addInstallArtifact(input, .{}).step);
}

fn addZlib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const zlib_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addMsvcIncludePaths(b, zlib_module, toolchain);
    zlib_module.addIncludePath(b.path("Sources/src/zlib"));
    zlib_module.addCSourceFiles(.{
        .files = zlib_sources,
        .flags = cflagsForOptimize(optimize),
    });

    const zlib = b.addLibrary(.{
        .name = "zlib",
        .linkage = .static,
        .root_module = zlib_module,
    });

    return zlib;
}

fn addLibpng(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    zlib: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const libpng_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addMsvcIncludePaths(b, libpng_module, toolchain);
    libpng_module.addIncludePath(b.path("Sources/src/libpng"));
    libpng_module.addIncludePath(b.path("Sources/src/zlib"));
    libpng_module.addCSourceFiles(.{
        .files = libpng_sources,
        .flags = cflagsForOptimize(optimize),
    });
    libpng_module.linkLibrary(zlib);

    return b.addLibrary(.{
        .name = "libpng",
        .linkage = .static,
        .root_module = libpng_module,
    });
}

fn addMisc(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const misc_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, misc_module);
    addMsvcIncludePaths(b, misc_module, toolchain);
    misc_module.addIncludePath(b.path("Sources/src/Misc"));
    misc_module.addIncludePath(b.path("Sources/src/zlib"));
    misc_module.addCSourceFiles(.{
        .files = misc_sources,
        .flags = cppflagsForOptimize(optimize),
    });

    return b.addLibrary(.{
        .name = "Misc",
        .linkage = .static,
        .root_module = misc_module,
    });
}

fn addImage(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    zlib: *std.Build.Step.Compile,
    libpng: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const image_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, image_module);
    addMsvcIncludePaths(b, image_module, toolchain);
    addMsvcLibraryPaths(b, image_module, toolchain);
    image_module.addIncludePath(b.path("Sources/src/Image"));
    image_module.addIncludePath(b.path("Sources/src/zlib"));
    image_module.addIncludePath(b.path("Sources/src/libpng"));
    image_module.addCSourceFiles(.{
        .files = image_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    image_module.linkLibrary(misc);
    image_module.linkLibrary(libpng);
    image_module.linkLibrary(zlib);
    linkMsvcRuntime(image_module, optimize);
    image_module.linkSystemLibrary("user32", .{});

    return b.addLibrary(.{
        .name = "Image",
        .linkage = .dynamic,
        .root_module = image_module,
        .win32_module_definition = b.path("Sources/src/Image/Image.def"),
    });
}

fn addLuaLib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
) *std.Build.Step.Compile {
    const lualib_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, lualib_module);
    addMsvcIncludePaths(b, lualib_module, toolchain);
    lualib_module.addIncludePath(b.path("Sources/src/LuaLib"));
    lualib_module.addIncludePath(b.path("Sources/src/LuaLib/LuaSrc"));
    lualib_module.addCSourceFiles(.{
        .files = lualib_c_sources,
        .flags = cflagsForOptimize(optimize),
    });
    lualib_module.addCSourceFiles(.{
        .files = lualib_cpp_sources,
        .flags = cppflagsForOptimize(optimize),
    });

    return b.addLibrary(.{
        .name = "LuaLib",
        .linkage = .static,
        .root_module = lualib_module,
    });
}

fn addNet(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const net_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, net_module);
    addMsvcIncludePaths(b, net_module, toolchain);
    addMsvcLibraryPaths(b, net_module, toolchain);
    net_module.addIncludePath(b.path("Sources/src/Net"));
    net_module.addCSourceFiles(.{
        .files = net_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    net_module.linkLibrary(misc);
    linkMsvcRuntime(net_module, optimize);
    net_module.linkSystemLibrary("ws2_32", .{});
    net_module.linkSystemLibrary("odbc32", .{});
    net_module.linkSystemLibrary("odbccp32", .{});

    return b.addLibrary(.{
        .name = "Net",
        .linkage = .dynamic,
        .root_module = net_module,
        .win32_module_definition = b.path("Sources/src/Net/net.def"),
    });
}

fn addBuildVersion(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const buildversion_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, buildversion_module);
    addMsvcIncludePaths(b, buildversion_module, toolchain);
    addMsvcLibraryPaths(b, buildversion_module, toolchain);
    buildversion_module.addIncludePath(b.path("Sources/src/buildversion"));
    buildversion_module.addCSourceFiles(.{
        .files = buildversion_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    buildversion_module.linkLibrary(misc);
    linkMsvcRuntime(buildversion_module, optimize);
    buildversion_module.linkSystemLibrary("odbc32", .{});
    buildversion_module.linkSystemLibrary("odbccp32", .{});

    const buildversion = b.addExecutable(.{
        .name = "BuildVersion",
        .root_module = buildversion_module,
    });
    buildversion.subsystem = .console;
    buildversion.entry = .{ .symbol_name = "mainCRTStartup" };
    return buildversion;
}

fn addBetaKeyGen(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    zlib: *std.Build.Step.Compile,
    misc: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const betakeygen_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, betakeygen_module);
    addMsvcIncludePaths(b, betakeygen_module, toolchain);
    addMsvcLibraryPaths(b, betakeygen_module, toolchain);
    betakeygen_module.addIncludePath(b.path("Sources/src/betakeygen"));
    betakeygen_module.addIncludePath(b.path("Sources/src/zlib"));
    betakeygen_module.addCSourceFiles(.{
        .files = betakeygen_sources,
        .flags = cppflagsBetaForOptimize(optimize),
    });
    betakeygen_module.linkLibrary(misc);
    betakeygen_module.linkLibrary(zlib);
    linkMsvcRuntime(betakeygen_module, optimize);
    betakeygen_module.linkSystemLibrary("odbc32", .{});
    betakeygen_module.linkSystemLibrary("odbccp32", .{});

    const betakeygen = b.addExecutable(.{
        .name = "BetaKeyGen",
        .root_module = betakeygen_module,
    });
    betakeygen.subsystem = .console;
    betakeygen.entry = .{ .symbol_name = "mainCRTStartup" };
    return betakeygen;
}

fn addInput(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    toolchain: ToolchainIncludes,
    misc: *std.Build.Step.Compile,
) *std.Build.Step.Compile {
    const input_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    addProjectIncludePaths(b, input_module);
    addMsvcIncludePaths(b, input_module, toolchain);
    addMsvcLibraryPaths(b, input_module, toolchain);
    input_module.addIncludePath(b.path("Sources/src/Input"));
    input_module.addCSourceFiles(.{
        .files = input_sources,
        .flags = cppflagsForOptimize(optimize),
    });
    input_module.linkLibrary(misc);
    linkMsvcRuntime(input_module, optimize);
    input_module.linkSystemLibrary("winmm", .{});
    input_module.linkSystemLibrary("dinput8", .{});
    input_module.linkSystemLibrary("dxguid", .{});
    input_module.linkSystemLibrary("user32", .{});
    input_module.linkSystemLibrary("odbc32", .{});
    input_module.linkSystemLibrary("odbccp32", .{});
    linkComSupport(input_module, optimize);

    return b.addLibrary(.{
        .name = "Input",
        .linkage = .dynamic,
        .root_module = input_module,
        .win32_module_definition = b.path("Sources/src/Input/Input.def"),
    });
}

fn cflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cflags_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cflags_release,
    };
}

fn cppflagsForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cppflags_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_release,
    };
}

fn cppflagsBetaForOptimize(optimize: std.builtin.OptimizeMode) []const []const u8 {
    return switch (optimize) {
        .Debug => cppflags_beta_debug,
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => cppflags_beta_release,
    };
}

fn addProjectIncludePaths(b: *std.Build, module: *std.Build.Module) void {
    module.addIncludePath(b.path("Sources/src"));
    module.addIncludePath(b.path("Sources/src/Misc"));
    module.addIncludePath(b.path("Sources/src/Formats"));
}

const ToolchainIncludes = struct {
    msvc_include: []const u8,
    windows_sdk_include: []const u8,
    msvc_lib: []const u8,
    windows_sdk_lib: []const u8,
};

fn addMsvcIncludePaths(b: *std.Build, module: *std.Build.Module, toolchain: ToolchainIncludes) void {
    module.addSystemIncludePath(.{ .cwd_relative = toolchain.msvc_include });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\ucrt", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\shared", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\um", .{toolchain.windows_sdk_include}) });
    module.addSystemIncludePath(.{ .cwd_relative = b.fmt("{s}\\winrt", .{toolchain.windows_sdk_include}) });
}

fn addMsvcLibraryPaths(b: *std.Build, module: *std.Build.Module, toolchain: ToolchainIncludes) void {
    module.addLibraryPath(.{ .cwd_relative = toolchain.msvc_lib });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\ucrt\\x86", .{toolchain.windows_sdk_lib}) });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\um\\x86", .{toolchain.windows_sdk_lib}) });
}

fn linkMsvcRuntime(module: *std.Build.Module, optimize: std.builtin.OptimizeMode) void {
    switch (optimize) {
        .Debug => {
            module.linkSystemLibrary("ucrtd", .{});
            module.linkSystemLibrary("msvcrtd", .{});
            module.linkSystemLibrary("msvcprtd", .{});
            module.linkSystemLibrary("vcruntimed", .{});
        },
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => {
            module.linkSystemLibrary("ucrt", .{});
            module.linkSystemLibrary("msvcrt", .{});
            module.linkSystemLibrary("msvcprt", .{});
            module.linkSystemLibrary("vcruntime", .{});
        },
    }
    module.linkSystemLibrary("oldnames", .{});
    module.linkSystemLibrary("kernel32", .{});
    module.linkSystemLibrary("ntdll", .{});
}

fn linkComSupport(module: *std.Build.Module, optimize: std.builtin.OptimizeMode) void {
    switch (optimize) {
        .Debug => module.linkSystemLibrary("comsuppwd", .{}),
        .ReleaseSafe, .ReleaseFast, .ReleaseSmall => module.linkSystemLibrary("comsuppw", .{}),
    }
}
