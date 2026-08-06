const c = @cImport({
    @cInclude("PlatformABI/platform_c.h");
});

test "platform C ABI imports as C11-compatible declarations" {
    try @import("std").testing.expect(c.BK_PLATFORM_ABI_VERSION == 1);
    try @import("std").testing.expect(@sizeOf(c.BkPlatformHandle) == 8);
    try @import("std").testing.expect(@sizeOf(c.BkPlatformResult) == 4);
}
