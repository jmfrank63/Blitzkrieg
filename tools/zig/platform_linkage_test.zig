const std = @import("std");
const support = @import("build_support.zig");

fn contains(text: []const u8, needle: []const u8) bool {
    return std.mem.indexOf(u8, text, needle) != null;
}

test "PlatformRuntime has one target-correct staged name" {
    try std.testing.expectEqualStrings("PlatformRuntime.dll", support.policy(.windows_x64, false).runtime_filename);
    try std.testing.expectEqualStrings("libPlatformRuntime.so", support.policy(.linux_x64, false).runtime_filename);
    try std.testing.expectEqualStrings("libPlatformRuntime.dylib", support.policy(.macos_arm64, false).runtime_filename);
}

test "playable link graph carries PlatformRuntime through shared dependencies" {
    const build_text = try std.Io.Dir.cwd().readFileAlloc(std.testing.io, "build.zig", std.testing.allocator, .limited(20 * 1024 * 1024));
    defer std.testing.allocator.free(build_text);
    try std.testing.expect(contains(build_text, "misc_module.linkLibrary(platform_runtime)"));
    try std.testing.expect(contains(build_text, "streamio_module.linkLibrary(platform_runtime)"));
    try std.testing.expect(contains(build_text, "module.linkLibrary(platform_runtime)"));
    try std.testing.expect(contains(build_text, "PlatformRuntime.dll"));
    try std.testing.expect(contains(build_text, "libPlatformRuntime.so"));
    try std.testing.expect(contains(build_text, "libPlatformRuntime.dylib"));
    try std.testing.expect(!contains(build_text, "-Wl,-rpath,.zig-cache"));
    try std.testing.expect(!contains(build_text, "-install_name,.zig-cache"));
}

test "target policy rejects absolute cache paths as runtime layout inputs" {
    const forbidden = [_][]const u8{ ".zig-cache/", "AppData/Local/zig", "Users/runner/" };
    const build_text = try std.Io.Dir.cwd().readFileAlloc(std.testing.io, "build.zig", std.testing.allocator, .limited(20 * 1024 * 1024));
    defer std.testing.allocator.free(build_text);
    for (forbidden) |path| try std.testing.expect(!contains(build_text, path));
}
