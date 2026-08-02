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
