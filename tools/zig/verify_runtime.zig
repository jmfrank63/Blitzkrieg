const std = @import("std");

pub const RuntimeTarget = enum { windows, linux, macos };

pub fn runtimeName(target: RuntimeTarget) []const u8 {
    return switch (target) {
        .windows => "PlatformRuntime.dll",
        .linux => "libPlatformRuntime.so",
        .macos => "libPlatformRuntime.dylib",
    };
}

pub fn gameName(target: RuntimeTarget) []const u8 {
    return if (target == .windows) "Game.exe" else "Game";
}

pub fn hasExactlyOneRuntime(names: []const []const u8, target: RuntimeTarget) bool {
    var count: usize = 0;
    for (names) |name| {
        if (std.mem.eql(u8, name, runtimeName(target))) count += 1;
    }
    return count == 1;
}

test "runtime verifier uses target-correct names" {
    try std.testing.expectEqualStrings("PlatformRuntime.dll", runtimeName(.windows));
    try std.testing.expectEqualStrings("libPlatformRuntime.so", runtimeName(.linux));
    try std.testing.expectEqualStrings("libPlatformRuntime.dylib", runtimeName(.macos));
    try std.testing.expectEqualStrings("Game.exe", gameName(.windows));
    try std.testing.expectEqualStrings("Game", gameName(.linux));
}

test "runtime verifier rejects duplicate runtime copies" {
    try std.testing.expect(hasExactlyOneRuntime(&.{ "Game", "libPlatformRuntime.so", "libSDL3.so.0" }, .linux));
    try std.testing.expect(!hasExactlyOneRuntime(&.{ "Game", "libPlatformRuntime.so", "libPlatformRuntime.so" }, .linux));
}
