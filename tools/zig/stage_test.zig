const std = @import("std");
const stage = @import("stage.zig");

test "copy-data remains the default" {
    const options = stage.Options{ .repo_root = ".", .install_dir = "zig-out/game/test" };
    try std.testing.expectEqual(stage.DataMode.copy, options.data_mode);
}

test "explicit link permission failures are actionable" {
    try std.testing.expectEqual(error.DataLinkPermissionDenied, stage.classifyDataLinkError(error.PermissionDenied));
    try std.testing.expectEqual(error.DataLinkPermissionDenied, stage.classifyDataLinkError(error.AccessDenied));
}

test "stale images are never accepted as runtime inputs" {
    try std.testing.expect(!stage.shouldReplaceRuntime("Game.exe.stale"));
    try std.testing.expect(stage.shouldReplaceRuntime("Game.exe"));
}

test "target layout carries target-specific runtime names" {
    const windows = stage.RuntimeLayout{
        .game_name = "Game.exe",
        .runtime_files = &.{ "Game.exe", "PlatformRuntime.dll", "GFXGPU.dll" },
        .debug_files = &.{ "Game.pdb" },
        .editors_supported = true,
    };
    const unix = stage.RuntimeLayout{
        .game_name = "Game",
        .runtime_files = &.{ "Game", "libPlatformRuntime.so", "libGFXGPU.so" },
        .debug_files = &.{},
        .editors_supported = false,
    };
    try std.testing.expectEqualStrings("Game.exe", windows.game_name);
    try std.testing.expectEqualStrings("libPlatformRuntime.so", unix.runtime_files[1]);
    try std.testing.expect(!unix.editors_supported);
}
