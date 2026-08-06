const std = @import("std");

pub const PlatformTarget = enum { windows_x64, linux_x64, macos_arm64 };
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

pub const Policy = struct {
    platform: PlatformTarget,
    executable_name: []const u8,
    shared_library_suffix: []const u8,
    package_root: []const u8,
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
    if (target.arch == .x86_64 and target.os == .linux and target.abi == .gnu) return .linux_x64;
    if (target.arch == .aarch64 and target.os == .macos and target.abi == .none) return .macos_arm64;
    return error.UnsupportedTarget;
}

pub fn policy(platform: PlatformTarget, native: bool) Policy {
    return switch (platform) {
        .windows_x64 => .{
            .platform = platform,
            .executable_name = "Game.exe",
            .shared_library_suffix = ".dll",
            .package_root = "windows-x64",
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
        .linux_x64 => .{
            .platform = platform,
            .executable_name = "Game",
            .shared_library_suffix = ".so",
            .package_root = "linux-x64",
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
        .macos_arm64 => .{
            .platform = platform,
            .executable_name = "Game",
            .shared_library_suffix = ".dylib",
            .package_root = "macos-arm64",
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
        .windows_x64 => "x64",
        .linux_x64 => "x86_64",
        .macos_arm64 => "arm64",
    };
}

pub fn windowsSdkRequired(platform: PlatformTarget) bool {
    return platform == .windows_x64;
}

pub fn subsystem(platform: PlatformTarget, graphical: bool) Subsystem {
    if (platform == .windows_x64 and graphical) return .windows;
    return .console;
}

pub fn entryPoint(platform: PlatformTarget, graphical: bool) EntryPoint {
    if (platform == .windows_x64 and graphical) return .win_main_crt_startup;
    return .main;
}

pub fn resourceFile(platform: PlatformTarget) ?[]const u8 {
    return if (platform == .windows_x64) "Sources/src/Main/Game.rc" else null;
}

pub fn defFile(platform: PlatformTarget, arch: TargetArch) ?[]const u8 {
    if (platform != .windows_x64) return null;
    return if (arch == .x86_64) "Sources/src/StreamIOZig/StreamIO.x64.def" else null;
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
    try std.testing.expectEqual(PlatformTarget.linux_x64, try classifyDescriptor(.{ .arch = .x86_64, .os = .linux, .abi = .gnu }));
    try std.testing.expectEqual(PlatformTarget.macos_arm64, try classifyDescriptor(.{ .arch = .aarch64, .os = .macos, .abi = .none }));
}

test "unsupported target diagnostics" {
    try std.testing.expectError(error.UnsupportedTarget, classifyDescriptor(.{ .arch = .x86, .os = .windows, .abi = .msvc }));
    try std.testing.expectError(error.UnsupportedTarget, classifyDescriptor(.{ .arch = .aarch64, .os = .linux, .abi = .gnu }));
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
