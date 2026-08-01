const std = @import("std");

pub const PresentMode = enum { mailbox, vsync };
pub const Composition = enum { sdr };
pub const SurfaceError = error{ NullWindow, ClaimFailed, Released, InvalidExtent };
pub const SurfaceApi = struct {
    claim: *const fn (*anyopaque, *anyopaque) bool,
    release: *const fn (*anyopaque, *anyopaque) void,
};

pub const Surface = struct {
    api: SurfaceApi,
    device: *anyopaque,
    window: *anyopaque,
    format: u32,
    width: u32,
    height: u32,
    present_mode: PresentMode,
    composition: Composition = .sdr,
    claimed: bool = true,

    pub fn init(api: SurfaceApi, device: ?*anyopaque, window: ?*anyopaque, format: u32, width: u32, height: u32, mailbox_supported: bool) SurfaceError!Surface {
        const device_ptr = device orelse return SurfaceError.ClaimFailed;
        const window_ptr = window orelse return SurfaceError.NullWindow;
        if (!api.claim(device_ptr, window_ptr)) return SurfaceError.ClaimFailed;
        if (width == 0 or height == 0) { api.release(device_ptr, window_ptr); return SurfaceError.InvalidExtent; }
        return .{ .api = api, .device = device_ptr, .window = window_ptr, .format = format, .width = width, .height = height, .present_mode = if (mailbox_supported) .mailbox else .vsync };
    }

    pub fn deinit(self: *Surface) void {
        if (self.claimed) { self.api.release(self.device, self.window); self.claimed = false; }
    }

    pub fn resize(self: *Surface, width: u32, height: u32) SurfaceError!void {
        if (!self.claimed) return SurfaceError.Released;
        if (width == 0 or height == 0) return SurfaceError.InvalidExtent;
        self.width = width; self.height = height;
    }
};

test "surface claim is borrowed, fallback is VSYNC, and release is idempotent" {
    var claims: u32 = 0; var releases: u32 = 0;
    const Fake = struct {
        var claim_count: *u32 = undefined; var release_count: *u32 = undefined;
        fn claim(_: *anyopaque, _: *anyopaque) bool { claim_count.* += 1; return true; }
        fn release(_: *anyopaque, _: *anyopaque) void { release_count.* += 1; }
    };
    Fake.claim_count = &claims; Fake.release_count = &releases;
    var device: u8 = 0; var window: u8 = 0;
    var surface = try Surface.init(.{ .claim = Fake.claim, .release = Fake.release }, &device, &window, 1, 320, 200, false);
    try std.testing.expectEqual(PresentMode.vsync, surface.present_mode);
    surface.deinit(); surface.deinit();
    try std.testing.expectEqual(@as(u32, 1), claims); try std.testing.expectEqual(@as(u32, 1), releases);
    try std.testing.expectError(SurfaceError.NullWindow, Surface.init(.{ .claim = Fake.claim, .release = Fake.release }, &device, null, 1, 1, true));
}
