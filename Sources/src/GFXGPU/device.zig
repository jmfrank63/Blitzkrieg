const std = @import("std");
const sdl = @import("sdl.zig");

pub const DeviceError = error{ CreateFailed, UnsupportedDriver };
pub const DeviceApi = struct {
    create: *const fn (u32, bool, ?[*:0]const u8) ?*anyopaque,
    destroy: *const fn (*anyopaque) void,
    error_text: *const fn () []const u8,
    driver_name: *const fn (*anyopaque) []const u8,
    shader_formats: *const fn (*anyopaque) u32,
};

fn realCreate(format_flags: u32, debug_mode: bool, driver: ?[*:0]const u8) ?*anyopaque { return @ptrCast(sdl.createGpuDevice(@intCast(format_flags), debug_mode, driver)); }
fn realDestroy(device: *anyopaque) void { sdl.destroyGpuDevice(@ptrCast(@alignCast(device))); }
fn realError() []const u8 { return sdl.getError(); }
fn realDriver(device: *anyopaque) []const u8 { return std.mem.span(sdl.c.SDL_GetGPUDeviceDriver(@ptrCast(@alignCast(device)))); }
fn realFormats(device: *anyopaque) u32 { return sdl.c.SDL_GetGPUShaderFormats(@ptrCast(@alignCast(device))); }
pub const real_api = DeviceApi{ .create = realCreate, .destroy = realDestroy, .error_text = realError, .driver_name = realDriver, .shader_formats = realFormats };

pub const Device = struct {
    allocator: std.mem.Allocator,
    api: DeviceApi,
    handle: ?*anyopaque = null,
    driver: [64]u8 = [_]u8{0} ** 64,
    driver_length: u8 = 0,
    shader_formats: u32 = 0,

    pub fn init(allocator: std.mem.Allocator, api: DeviceApi, format_flags: u32, debug_mode: bool, preferred_driver: ?[*:0]const u8) DeviceError!Device {
        const handle = api.create(format_flags, debug_mode, preferred_driver) orelse return DeviceError.CreateFailed;
        var result = Device{ .allocator = allocator, .api = api, .handle = handle };
        const driver = api.driver_name(handle);
        result.driver_length = @intCast(@min(driver.len, result.driver.len - 1));
        @memcpy(result.driver[0..result.driver_length], driver[0..result.driver_length]);
        result.driver[result.driver_length] = 0;
        result.shader_formats = api.shader_formats(handle);
        if (preferred_driver) |requested| {
            const requested_slice = std.mem.span(requested);
            if (!std.mem.eql(u8, requested_slice, result.driver[0..result.driver_length])) {
                api.destroy(handle);
                return DeviceError.UnsupportedDriver;
            }
        }
        return result;
    }

    pub fn deinit(self: *Device) void {
        if (self.handle) |handle| { self.api.destroy(handle); self.handle = null; }
    }

    pub fn driverName(self: *const Device) []const u8 { return self.driver[0..self.driver_length]; }
};

test "device fake API handles failure, partial init, and exactly-once release" {
    const Context = struct { creates: u32 = 0, destroys: u32 = 0, fail: bool = false };
    var context = Context{};
    const Fake = struct {
        var ctx: *Context = undefined;
        fn create(_: u32, _: bool, _: ?[*:0]const u8) ?*anyopaque { ctx.creates += 1; if (ctx.fail) return null; return @ptrCast(ctx); }
        fn destroy(_: *anyopaque) void { ctx.destroys += 1; }
        fn errorText() []const u8 { return "fake SDL failure"; }
        fn driver(_: *anyopaque) []const u8 { return "direct3d12"; }
        fn formats(_: *anyopaque) u32 { return 1 << 3; }
    };
    Fake.ctx = &context;
    const api = DeviceApi{ .create = Fake.create, .destroy = Fake.destroy, .error_text = Fake.errorText, .driver_name = Fake.driver, .shader_formats = Fake.formats };
    context.fail = true;
    try std.testing.expectError(DeviceError.CreateFailed, Device.init(std.testing.allocator, api, 8, true, null));
    context.fail = false;
    var device = try Device.init(std.testing.allocator, api, 8, true, "direct3d12");
    defer device.deinit();
    try std.testing.expectEqualStrings("direct3d12", device.driverName());
    device.deinit();
    try std.testing.expectEqual(@as(u32, 1), context.destroys);
    try std.testing.expectError(DeviceError.UnsupportedDriver, Device.init(std.testing.allocator, api, 8, false, "vulkan"));
    try std.testing.expectEqual(@as(u32, 2), context.destroys);
}
