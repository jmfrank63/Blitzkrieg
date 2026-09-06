const std = @import("std");

pub const PlatformTarget = enum { windows_x64, windows_x64_gnu, linux_x64, linux_arm64, macos_x64, macos_arm64 };

// Windows comes in two ABI flavours and most build decisions care about only
// one of the two questions. "Is this Windows?" governs the Win32 sources, the
// resource script and the subsystem; "does this use MSVC?" governs the toolchain
// paths, the CRT and the SDK requirement. Conflating them is what makes a MinGW
// target either miss its Win32 sources or demand a Visual Studio install.
pub fn isWindows(platform: PlatformTarget) bool {
    return platform == .windows_x64 or platform == .windows_x64_gnu;
}

pub fn usesMsvc(platform: PlatformTarget) bool {
    return platform == .windows_x64;
}

// Zig's bundled libc++ is the only C++ standard library available for the GNU
// Windows ABI: macOS takes its headers from the SDK sysroot, Linux from the
// host GCC install, and MSVC from the Visual Studio tree, so MinGW is the one
// flavour that has to ask for a standard library explicitly. Without it even
// <cstdint> is missing.
pub fn needsBundledLibcpp(platform: PlatformTarget) bool {
    return platform == .windows_x64_gnu;
}

pub fn isLinux(platform: PlatformTarget) bool {
    return platform == .linux_x64 or platform == .linux_arm64;
}

pub fn isMacos(platform: PlatformTarget) bool {
    return platform == .macos_x64 or platform == .macos_arm64;
}
pub const TestMode = enum { compile, run };
pub const TargetArch = enum { x86, x86_64, aarch64 };
pub const TargetOs = enum { windows, linux, macos };
pub const TargetAbi = enum { msvc, gnu, none };

pub const TargetDescriptor = struct {
    arch: TargetArch,
    os: TargetOs,
    abi: TargetAbi,
};

pub const ShaderFormat = enum { dxil, spirv, metallib };
pub const GpuDriver = enum { d3d12, vulkan, metal };
pub const Subsystem = enum { console, windows };
pub const EntryPoint = enum { main, main_crt_startup, win_main_crt_startup };

pub const SourceSets = struct {
    shared: []const []const u8,
    windows: []const []const u8,
    posix: []const []const u8,
    linux: []const []const u8,
    macos: []const []const u8,
    windows_oracle: []const []const u8,
    excluded_utilities: []const []const u8,
};

pub fn sourceSets(platform: PlatformTarget) SourceSets {
    const shared = &.{ "Sources/src/PlatformABI", "Sources/src/Platform/Clock.cpp", "Sources/src/Platform/Debug.cpp" };
    const windows = &.{ "Sources/src/Game/WindowsMain.cpp", "Sources/src/Platform/SocketWin32.cpp" };
    const posix = &.{ "Sources/src/Platform/SocketPosix.cpp" };
    const linux = &.{ "Sources/src/Platform/Linux/Paths.cpp", "Sources/src/Platform/Linux/System.cpp" };
    const macos = &.{ "Sources/src/Platform/MacOS/Paths.mm", "Sources/src/Platform/MacOS/System.mm" };
    const windows_oracle = &.{ "Sources/src/Platform/WindowsOracle" };
    const excluded_utilities = &.{ "BuildVersion", "BetaKeyGen", "FontGen", "GFX legacy renderer" };
    return .{
        .shared = shared,
        .windows = if (isWindows(platform)) windows else &.{},
        .posix = if (!isWindows(platform)) posix else &.{},
        .linux = if (isLinux(platform)) linux else &.{},
        // Both macOS architectures need the Cocoa adapters. Keying this on
        // macos_arm64 alone left an Intel build with no Paths/System sources at
        // all, which the arm64-only CI job could never have caught.
        .macos = if (isMacos(platform)) macos else &.{},
        // The oracle records real MSVC behaviour to compare the portable layer
        // against, so it is meaningful only for the MSVC ABI.
        .windows_oracle = if (usesMsvc(platform)) windows_oracle else &.{},
        .excluded_utilities = excluded_utilities,
    };
}

pub const Policy = struct {
    platform: PlatformTarget,
    executable_name: []const u8,
    shared_library_suffix: []const u8,
    // Staging and package trees are laid out as <os>/<arch>/<variant> so a
    // second architecture for an OS sits beside the first instead of colliding
    // with it: macos/arm64/release next to macos/x86_64/release.
    os_dir: []const u8,
    arch_dir: []const u8,
    shader_format: ShaderFormat,
    gpu_driver: GpuDriver,
    runtime_filename: []const u8,
    import_library_suffix: []const u8,
    elf_rpath: []const u8,
    macho_install_name: []const u8,
    crt: []const u8,
    runtime_def_file: ?[]const u8,
    subsystem: Subsystem,
    native_run_eligible: bool,
    windows_only: bool,
};

pub const PolicyError = error{UnsupportedTarget};

pub fn describe(target: std.Target) TargetDescriptor {
    return .{
        .arch = switch (target.cpu.arch) {
            .x86 => .x86,
            .x86_64 => .x86_64,
            .aarch64 => .aarch64,
            else => @panic("target architecture is not represented by the platform policy"),
        },
        .os = switch (target.os.tag) {
            .windows => .windows,
            .linux => .linux,
            .macos => .macos,
            else => @panic("target operating system is not represented by the platform policy"),
        },
        .abi = switch (target.abi) {
            .msvc => .msvc,
            .gnu, .gnueabi, .gnueabihf, .musl, .musleabi, .musleabihf => .gnu,
            .none => .none,
            else => @panic("target ABI is not represented by the platform policy"),
        },
    };
}

pub fn classify(target: std.Target) PolicyError!PlatformTarget {
    return classifyDescriptor(describe(target));
}

pub fn classifyDescriptor(target: TargetDescriptor) PolicyError!PlatformTarget {
    if (target.arch == .x86_64 and target.os == .windows and target.abi == .msvc) return .windows_x64;
    if (target.arch == .x86_64 and target.os == .windows and target.abi == .gnu) return .windows_x64_gnu;
    if (target.arch == .x86_64 and target.os == .linux and target.abi == .gnu) return .linux_x64;
    if (target.arch == .aarch64 and target.os == .linux and target.abi == .gnu) return .linux_arm64;
    if (target.arch == .x86_64 and target.os == .macos and target.abi == .none) return .macos_x64;
    if (target.arch == .aarch64 and target.os == .macos and target.abi == .none) return .macos_arm64;
    return error.UnsupportedTarget;
}

pub fn policy(platform: PlatformTarget, native: bool) Policy {
    return switch (platform) {
        .windows_x64 => .{
            .platform = platform,
            .executable_name = "Game.exe",
            .shared_library_suffix = ".dll",
            .os_dir = "windows",
            .arch_dir = "x86_64",
            .shader_format = .dxil,
            .gpu_driver = .d3d12,
            .runtime_filename = "PlatformRuntime.dll",
            .import_library_suffix = ".lib",
            .elf_rpath = "",
            .macho_install_name = "",
            .crt = "msvc",
            .runtime_def_file = "Sources/src/PlatformABI/PlatformRuntime.def",
            .subsystem = .console,
            .native_run_eligible = native,
            .windows_only = true,
        },
        // MinGW produces the same Windows artefacts through a different
        // toolchain, so it needs its own staging root: sharing windows/x86_64
        // with the MSVC build would have the two overwrite each other.
        .windows_x64_gnu => .{
            .platform = platform,
            .executable_name = "Game.exe",
            .shared_library_suffix = ".dll",
            .os_dir = "windows-mingw",
            .arch_dir = "x86_64",
            .shader_format = .dxil,
            .gpu_driver = .d3d12,
            .runtime_filename = "PlatformRuntime.dll",
            .import_library_suffix = ".lib",
            .elf_rpath = "",
            .macho_install_name = "",
            .crt = "mingw",
            .runtime_def_file = "Sources/src/PlatformABI/PlatformRuntime.def",
            .subsystem = .console,
            .native_run_eligible = native,
            .windows_only = true,
        },
        .linux_x64 => .{
            .platform = platform,
            .executable_name = "Game",
            .shared_library_suffix = ".so",
            .os_dir = "linux",
            .arch_dir = "x86_64",
            .shader_format = .spirv,
            .gpu_driver = .vulkan,
            .runtime_filename = "libPlatformRuntime.so",
            .import_library_suffix = "",
            .elf_rpath = "$ORIGIN",
            .macho_install_name = "",
            .crt = "glibc",
            .runtime_def_file = null,
            .subsystem = .console,
            .native_run_eligible = native,
            .windows_only = false,
        },
        .linux_arm64 => .{
            .platform = platform,
            .executable_name = "Game",
            .shared_library_suffix = ".so",
            .os_dir = "linux",
            .arch_dir = "aarch64",
            .shader_format = .spirv,
            .gpu_driver = .vulkan,
            .runtime_filename = "libPlatformRuntime.so",
            .import_library_suffix = "",
            .elf_rpath = "$ORIGIN",
            .macho_install_name = "",
            .crt = "glibc",
            .runtime_def_file = null,
            .subsystem = .console,
            .native_run_eligible = native,
            .windows_only = false,
        },
        .macos_x64 => .{
            .platform = platform,
            .executable_name = "Game",
            .shared_library_suffix = ".dylib",
            .os_dir = "macos",
            .arch_dir = "x86_64",
            .shader_format = .metallib,
            .gpu_driver = .metal,
            .runtime_filename = "libPlatformRuntime.dylib",
            .import_library_suffix = "",
            .elf_rpath = "",
            .macho_install_name = "@rpath/libPlatformRuntime.dylib",
            .crt = "libc++",
            .runtime_def_file = null,
            .subsystem = .console,
            .native_run_eligible = native,
            .windows_only = false,
        },
        .macos_arm64 => .{
            .platform = platform,
            .executable_name = "Game",
            .shared_library_suffix = ".dylib",
            .os_dir = "macos",
            .arch_dir = "arm64",
            .shader_format = .metallib,
            .gpu_driver = .metal,
            .runtime_filename = "libPlatformRuntime.dylib",
            .import_library_suffix = "",
            .elf_rpath = "",
            .macho_install_name = "@rpath/libPlatformRuntime.dylib",
            .crt = "libc++",
            .runtime_def_file = null,
            .subsystem = .console,
            .native_run_eligible = native,
            .windows_only = false,
        },
    };
}

pub fn libraryArch(platform: PlatformTarget) []const u8 {
    return switch (platform) {
        .windows_x64, .windows_x64_gnu => "x64",
        .linux_x64 => "x86_64",
        .linux_arm64 => "aarch64",
        .macos_x64 => "x86_64",
        .macos_arm64 => "arm64",
    };
}

// Only the MSVC ABI needs a Visual Studio install: Zig ships its own mingw-w64
// headers and import libraries for the GNU Windows ABI.
pub fn windowsSdkRequired(platform: PlatformTarget) bool {
    return usesMsvc(platform);
}

pub fn subsystem(platform: PlatformTarget, graphical: bool) Subsystem {
    if (isWindows(platform) and graphical) return .windows;
    return .console;
}

pub fn entryPoint(platform: PlatformTarget, graphical: bool) EntryPoint {
    if (isWindows(platform) and graphical) return .win_main_crt_startup;
    return .main;
}

pub fn resourceFile(platform: PlatformTarget) ?[]const u8 {
    return if (isWindows(platform)) "Sources/src/Main/Game.rc" else null;
}

pub fn defFile(platform: PlatformTarget, arch: TargetArch) ?[]const u8 {
    if (!isWindows(platform)) return null;
    return if (arch == .x86_64) "Sources/src/StreamIOZig/StreamIO.x64.def" else null;
}

// The rclone binary the game ships with. Cloud sync must work on a machine
// with nothing on PATH, and daemon discovery already looks in the executable's
// own directory first, so the whole job is getting the right archive's
// executable into the staged layout.
pub const BundledRclone = struct {
    /// The build.zig.zon dependency holding this platform's official archive.
    dependency: []const u8,
    /// The member to take out of it. Zig strips the archive's single root
    /// directory, so the executable sits at the package root.
    archive_member: []const u8,
    /// What it is installed and staged as, which is also the name discovery
    /// looks for beside the game.
    installed_name: []const u8,
};

pub fn bundledRclone(platform: PlatformTarget) BundledRclone {
    // Both Windows ABI flavours run the same x64 executable; the MinGW build is
    // still a Windows program looking for rclone.exe.
    return switch (platform) {
        .windows_x64, .windows_x64_gnu => .{
            .dependency = "rclone_windows_x64",
            .archive_member = "rclone.exe",
            .installed_name = "rclone.exe",
        },
        .linux_x64 => .{ .dependency = "rclone_linux_x64", .archive_member = "rclone", .installed_name = "rclone" },
        .linux_arm64 => .{ .dependency = "rclone_linux_arm64", .archive_member = "rclone", .installed_name = "rclone" },
        .macos_x64 => .{ .dependency = "rclone_macos_x64", .archive_member = "rclone", .installed_name = "rclone" },
        .macos_arm64 => .{ .dependency = "rclone_macos_arm64", .archive_member = "rclone", .installed_name = "rclone" },
    };
}

pub fn parseTestMode(value: []const u8) !TestMode {
    if (std.mem.eql(u8, value, "compile")) return .compile;
    if (std.mem.eql(u8, value, "run")) return .run;
    return error.InvalidTestMode;
}

pub fn defaultTestMode(native: bool) TestMode {
    return if (native) .run else .compile;
}

pub fn validateTestMode(mode: TestMode, native: bool) !void {
    if (mode == .run and !native) return error.NonNativeRun;
}

test "supported target table" {
    try std.testing.expectEqual(PlatformTarget.windows_x64, try classifyDescriptor(.{ .arch = .x86_64, .os = .windows, .abi = .msvc }));
    try std.testing.expectEqual(PlatformTarget.windows_x64_gnu, try classifyDescriptor(.{ .arch = .x86_64, .os = .windows, .abi = .gnu }));
    try std.testing.expectEqual(PlatformTarget.linux_x64, try classifyDescriptor(.{ .arch = .x86_64, .os = .linux, .abi = .gnu }));
    try std.testing.expectEqual(PlatformTarget.linux_arm64, try classifyDescriptor(.{ .arch = .aarch64, .os = .linux, .abi = .gnu }));
    try std.testing.expectEqual(PlatformTarget.macos_x64, try classifyDescriptor(.{ .arch = .x86_64, .os = .macos, .abi = .none }));
    try std.testing.expectEqual(PlatformTarget.macos_arm64, try classifyDescriptor(.{ .arch = .aarch64, .os = .macos, .abi = .none }));
}

test "every supported target names its own rclone archive" {
    // One archive per platform and no sharing except the two Windows ABIs,
    // which really do run the same executable. A platform pointing at another
    // platform's dependency would stage a binary that cannot run.
    const platforms = [_]PlatformTarget{ .windows_x64, .windows_x64_gnu, .linux_x64, .linux_arm64, .macos_x64, .macos_arm64 };
    var seen: [platforms.len][]const u8 = undefined;
    for (platforms, 0..) |platform, index| {
        const bundle = bundledRclone(platform);
        const expected_name = if (isWindows(platform)) "rclone.exe" else "rclone";
        try std.testing.expectEqualStrings(expected_name, bundle.installed_name);
        try std.testing.expectEqualStrings(expected_name, bundle.archive_member);
        for (seen[0..index], platforms[0..index]) |other, other_platform| {
            if (isWindows(platform) and isWindows(other_platform)) continue;
            try std.testing.expect(!std.mem.eql(u8, other, bundle.dependency));
        }
        seen[index] = bundle.dependency;
    }
    try std.testing.expectEqualStrings(bundledRclone(.windows_x64).dependency, bundledRclone(.windows_x64_gnu).dependency);
}

test "unsupported target diagnostics" {
    try std.testing.expectError(error.UnsupportedTarget, classifyDescriptor(.{ .arch = .x86, .os = .windows, .abi = .msvc }));
    try std.testing.expectError(error.UnsupportedTarget, classifyDescriptor(.{ .arch = .x86, .os = .linux, .abi = .gnu }));
    try std.testing.expectError(error.UnsupportedTarget, classifyDescriptor(.{ .arch = .aarch64, .os = .windows, .abi = .gnu }));
}

test "windows ABI flavours share Win32 policy but not the MSVC toolchain" {
    // The whole point of splitting isWindows from usesMsvc: MinGW is Windows for
    // sources, subsystem and resources, and is not Visual Studio for anything.
    inline for (.{ PlatformTarget.windows_x64, PlatformTarget.windows_x64_gnu }) |flavour| {
        const sources = sourceSets(flavour);
        try std.testing.expect(sources.windows.len > 0);
        try std.testing.expectEqual(@as(usize, 0), sources.posix.len);
        try std.testing.expectEqual(Subsystem.windows, subsystem(flavour, true));
        try std.testing.expectEqual(EntryPoint.win_main_crt_startup, entryPoint(flavour, true));
        try std.testing.expect(resourceFile(flavour) != null);
        try std.testing.expect(defFile(flavour, .x86_64) != null);
        try std.testing.expectEqualStrings("Game.exe", policy(flavour, false).executable_name);
    }
    try std.testing.expect(windowsSdkRequired(.windows_x64));
    try std.testing.expect(!windowsSdkRequired(.windows_x64_gnu));
    try std.testing.expect(sourceSets(.windows_x64).windows_oracle.len > 0);
    try std.testing.expectEqual(@as(usize, 0), sourceSets(.windows_x64_gnu).windows_oracle.len);
    // Distinct staging roots, or one flavour's install would clobber the other.
    try std.testing.expect(!std.mem.eql(
        u8,
        policy(.windows_x64, false).os_dir,
        policy(.windows_x64_gnu, false).os_dir,
    ));
}

test "second architecture sits beside the first for every OS" {
    const linux_x64 = policy(.linux_x64, false);
    const linux_arm = policy(.linux_arm64, false);
    try std.testing.expectEqualStrings(linux_x64.os_dir, linux_arm.os_dir);
    try std.testing.expectEqualStrings("aarch64", linux_arm.arch_dir);
    try std.testing.expectEqualStrings("$ORIGIN", linux_arm.elf_rpath);
    try std.testing.expectEqual(ShaderFormat.spirv, linux_arm.shader_format);
    try std.testing.expectEqual(GpuDriver.vulkan, linux_arm.gpu_driver);
    try std.testing.expect(!windowsSdkRequired(.linux_arm64));
    try std.testing.expectEqualStrings("aarch64", libraryArch(.linux_arm64));

    // Both macOS architectures carry the Cocoa adapters; Intel used to get none.
    try std.testing.expect(sourceSets(.macos_x64).macos.len > 0);
    try std.testing.expect(sourceSets(.macos_arm64).macos.len > 0);
    try std.testing.expect(sourceSets(.linux_arm64).linux.len > 0);
    try std.testing.expect(sourceSets(.linux_arm64).posix.len > 0);
}

test "platform output table" {
    const windows = policy(.windows_x64, true);
    try std.testing.expectEqualStrings("Game.exe", windows.executable_name);
    try std.testing.expectEqualStrings(".dll", windows.shared_library_suffix);
    try std.testing.expectEqual(ShaderFormat.dxil, windows.shader_format);
    try std.testing.expectEqual(GpuDriver.d3d12, windows.gpu_driver);
    try std.testing.expectEqualStrings("PlatformRuntime.dll", windows.runtime_filename);
    try std.testing.expectEqualStrings(".lib", windows.import_library_suffix);
    try std.testing.expectEqualStrings("msvc", windows.crt);
    try std.testing.expect(windows.runtime_def_file != null);

    const linux = policy(.linux_x64, false);
    try std.testing.expectEqualStrings("Game", linux.executable_name);
    try std.testing.expectEqualStrings(".so", linux.shared_library_suffix);
    try std.testing.expectEqual(ShaderFormat.spirv, linux.shader_format);
    try std.testing.expectEqual(GpuDriver.vulkan, linux.gpu_driver);
    try std.testing.expectEqualStrings("libPlatformRuntime.so", linux.runtime_filename);
    try std.testing.expectEqualStrings("$ORIGIN", linux.elf_rpath);
    try std.testing.expectEqualStrings("glibc", linux.crt);
    try std.testing.expect(linux.runtime_def_file == null);

    const macos = policy(.macos_arm64, true);
    try std.testing.expectEqualStrings(".dylib", macos.shared_library_suffix);
    try std.testing.expectEqual(ShaderFormat.metallib, macos.shader_format);
    try std.testing.expectEqual(GpuDriver.metal, macos.gpu_driver);
    try std.testing.expectEqualStrings("libPlatformRuntime.dylib", macos.runtime_filename);
    try std.testing.expectEqualStrings("@rpath/libPlatformRuntime.dylib", macos.macho_install_name);
    try std.testing.expectEqualStrings("libc++", macos.crt);
    try std.testing.expect(macos.runtime_def_file == null);
    try std.testing.expectEqualStrings("x64", libraryArch(.windows_x64));
    try std.testing.expectEqualStrings("arm64", libraryArch(.macos_arm64));
    try std.testing.expect(windowsSdkRequired(.windows_x64));
    try std.testing.expect(!windowsSdkRequired(.linux_x64));
    try std.testing.expectEqual(Subsystem.windows, subsystem(.windows_x64, true));
    try std.testing.expectEqual(Subsystem.console, subsystem(.linux_x64, true));
    try std.testing.expectEqual(EntryPoint.win_main_crt_startup, entryPoint(.windows_x64, true));
    try std.testing.expectEqual(EntryPoint.main, entryPoint(.macos_arm64, true));
    try std.testing.expect(resourceFile(.windows_x64) != null);
    try std.testing.expect(resourceFile(.linux_x64) == null);
    try std.testing.expect(defFile(.windows_x64, .x86_64) != null);
    try std.testing.expect(defFile(.linux_x64, .x86_64) == null);
}

test "test mode defaults and native-run guard" {
    try std.testing.expectEqual(TestMode.run, defaultTestMode(true));
    try std.testing.expectEqual(TestMode.compile, defaultTestMode(false));
    try std.testing.expectError(error.NonNativeRun, validateTestMode(.run, false));
    try validateTestMode(.compile, false);
    try std.testing.expectEqual(TestMode.compile, try parseTestMode("compile"));
    try std.testing.expectError(error.InvalidTestMode, parseTestMode("invalid"));
}

test "target source sets keep platform boundaries explicit" {
    const windows = sourceSets(.windows_x64);
    try std.testing.expect(windows.shared.len > 0);
    try std.testing.expect(windows.windows.len > 0);
    try std.testing.expectEqual(@as(usize, 0), windows.posix.len);
    try std.testing.expect(windows.windows_oracle.len > 0);
    try std.testing.expect(windows.excluded_utilities.len > 0);

    const linux = sourceSets(.linux_x64);
    try std.testing.expect(linux.posix.len > 0);
    try std.testing.expect(linux.linux.len > 0);
    try std.testing.expectEqual(@as(usize, 0), linux.windows.len);
    try std.testing.expectEqual(@as(usize, 0), linux.windows_oracle.len);
    try std.testing.expectEqual(@as(usize, 0), linux.macos.len);

    const macos = sourceSets(.macos_arm64);
    try std.testing.expect(macos.posix.len > 0);
    try std.testing.expect(macos.macos.len > 0);
    try std.testing.expectEqual(@as(usize, 0), macos.windows.len);
    try std.testing.expectEqual(@as(usize, 0), macos.linux.len);
}
