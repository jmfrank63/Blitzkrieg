const std = @import("std");
const support = @import("build_support.zig");

const MatrixEntry = struct {
    triple: []const u8,
    platform: support.PlatformTarget,
    native_run: bool,
};

pub const entries = [_]MatrixEntry{
    .{ .triple = "x86_64-windows-msvc", .platform = .windows_x64, .native_run = true },
    .{ .triple = "x86_64-linux-gnu", .platform = .linux_x64, .native_run = false },
    .{ .triple = "aarch64-macos", .platform = .macos_arm64, .native_run = false },
};

test "foundation matrix records supported targets without runtime claims" {
    try std.testing.expectEqual(@as(usize, 3), entries.len);
    try std.testing.expectEqual(support.PlatformTarget.windows_x64, entries[0].platform);
    try std.testing.expectEqual(support.PlatformTarget.linux_x64, entries[1].platform);
    try std.testing.expectEqual(support.PlatformTarget.macos_arm64, entries[2].platform);
    try std.testing.expect(entries[0].native_run);
    try std.testing.expect(!entries[1].native_run);
    try std.testing.expect(!entries[2].native_run);
}

test "foundation matrix uses compile mode for cross targets" {
    for (entries) |entry| {
        const mode = support.defaultTestMode(entry.native_run);
        if (entry.native_run) {
            try std.testing.expectEqual(support.TestMode.run, mode);
        } else {
            try std.testing.expectEqual(support.TestMode.compile, mode);
        }
    }
}

test "foundation matrix policies keep platform artifacts isolated" {
    const windows = support.policy(.windows_x64, true);
    try std.testing.expectEqualStrings("PlatformRuntime.dll", windows.runtime_filename);
    try std.testing.expectEqualStrings(".lib", windows.import_library_suffix);
    try std.testing.expect(windows.windows_only);
    try std.testing.expect(windows.native_run_eligible);

    const linux = support.policy(.linux_x64, false);
    try std.testing.expectEqualStrings("libPlatformRuntime.so", linux.runtime_filename);
    try std.testing.expectEqualStrings("$ORIGIN", linux.elf_rpath);
    try std.testing.expectEqualStrings("", linux.import_library_suffix);
    try std.testing.expect(!linux.windows_only);
    try std.testing.expect(linux.runtime_def_file == null);

    const macos = support.policy(.macos_arm64, false);
    try std.testing.expectEqualStrings("libPlatformRuntime.dylib", macos.runtime_filename);
    try std.testing.expectEqualStrings("@rpath/libPlatformRuntime.dylib", macos.macho_install_name);
    try std.testing.expectEqualStrings("libc++", macos.crt);
    try std.testing.expect(macos.runtime_def_file == null);
    try std.testing.expect(!macos.native_run_eligible);
}
