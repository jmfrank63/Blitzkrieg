const std = @import("std");
const sdl = @import("sdl.zig");
const manifest = @import("shader_manifest.zig");

pub const DeviceError = error{ CreateFailed, UnsupportedDriver, UnsupportedShaderFormat };
pub const DeviceApi = struct {
    create: *const fn (u32, bool, ?[*:0]const u8) ?*anyopaque,
    destroy: *const fn (*anyopaque) void,
    error_text: *const fn () []const u8,
    driver_name: *const fn (*anyopaque) []const u8,
    shader_formats: *const fn (*anyopaque) u32,
    claim_window: *const fn (*anyopaque, *anyopaque) bool,
    release_window: *const fn (*anyopaque, *anyopaque) void,
    swapchain_format: *const fn (*anyopaque, *anyopaque) u32,
    configure_swapchain: *const fn (*anyopaque, *anyopaque) bool,
    acquire_command_buffer: *const fn (*anyopaque) ?*anyopaque,
    wait_acquire_swapchain: *const fn (*anyopaque, *anyopaque, *?*anyopaque, *u32, *u32) bool,
    submit_command_buffer: *const fn (*anyopaque) bool,
    cancel_command_buffer: *const fn (*anyopaque) bool,
    begin_clear_pass: *const fn (*anyopaque, *anyopaque, [4]f32) ?*anyopaque,
    end_render_pass: *const fn (*anyopaque) void,
};

fn realCreate(format_flags: u32, debug_mode: bool, driver: ?[*:0]const u8) ?*anyopaque {
    return @ptrCast(sdl.createGpuDevice(@intCast(format_flags), debug_mode, driver));
}
fn realDestroy(device: *anyopaque) void {
    sdl.destroyGpuDevice(@ptrCast(@alignCast(device)));
}
fn realError() []const u8 {
    return sdl.getError();
}
fn realDriver(device: *anyopaque) []const u8 {
    return std.mem.span(sdl.c.SDL_GetGPUDeviceDriver(@ptrCast(@alignCast(device))));
}
fn realFormats(device: *anyopaque) u32 {
    return sdl.c.SDL_GetGPUShaderFormats(@ptrCast(@alignCast(device)));
}
fn realClaim(device: *anyopaque, window: *anyopaque) bool {
    return sdl.claimWindow(@ptrCast(@alignCast(device)), @ptrCast(@alignCast(window)));
}
fn realRelease(device: *anyopaque, window: *anyopaque) void {
    sdl.releaseWindow(@ptrCast(@alignCast(device)), @ptrCast(@alignCast(window)));
}
fn realFormat(device: *anyopaque, window: *anyopaque) u32 {
    return @intCast(sdl.swapchainFormat(@ptrCast(@alignCast(device)), @ptrCast(@alignCast(window))));
}
fn realConfigure(device: *anyopaque, window: *anyopaque) bool {
    return sdl.configureSwapchain(@ptrCast(@alignCast(device)), @ptrCast(@alignCast(window)));
}
fn realAcquire(device: *anyopaque) ?*anyopaque {
    return @ptrCast(sdl.acquireCommandBuffer(@ptrCast(@alignCast(device))));
}
fn realWaitAcquire(command: *anyopaque, window: *anyopaque, texture: *?*anyopaque, width: *u32, height: *u32) bool {
    return sdl.waitAcquireSwapchain(@ptrCast(@alignCast(command)), @ptrCast(@alignCast(window)), @ptrCast(@alignCast(texture)), width, height);
}
fn realSubmit(command: *anyopaque) bool {
    return sdl.submitCommandBuffer(@ptrCast(@alignCast(command)));
}
fn realCancel(command: *anyopaque) bool {
    return sdl.cancelCommandBuffer(@ptrCast(@alignCast(command)));
}
fn realBeginClear(command: *anyopaque, texture: *anyopaque, color: [4]f32) ?*anyopaque {
    return @ptrCast(sdl.beginClearPass(@ptrCast(@alignCast(command)), @ptrCast(@alignCast(texture)), color));
}
fn realEndRenderPass(pass: *anyopaque) void {
    sdl.endRenderPass(@ptrCast(@alignCast(pass)));
}
pub const real_api = DeviceApi{ .create = realCreate, .destroy = realDestroy, .error_text = realError, .driver_name = realDriver, .shader_formats = realFormats, .claim_window = realClaim, .release_window = realRelease, .swapchain_format = realFormat, .configure_swapchain = realConfigure, .acquire_command_buffer = realAcquire, .wait_acquire_swapchain = realWaitAcquire, .submit_command_buffer = realSubmit, .cancel_command_buffer = realCancel, .begin_clear_pass = realBeginClear, .end_render_pass = realEndRenderPass };

pub fn formatForDriver(driver: []const u8) DeviceError!manifest.Format {
    if (std.mem.eql(u8, driver, "direct3d12")) return .dxil;
    if (std.mem.eql(u8, driver, "vulkan")) return .spirv;
    if (std.mem.eql(u8, driver, "metal")) return .msl;
    return DeviceError.UnsupportedDriver;
}

pub fn formatFlag(format: manifest.Format) u32 {
    return switch (format) {
        .dxil => @intCast(sdl.shaderformat_dxil),
        .spirv => @intCast(sdl.shaderformat_spirv),
        .msl => @intCast(sdl.shaderformat_msl),
    };
}

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
        if (self.handle) |handle| {
            self.api.destroy(handle);
            self.handle = null;
        }
    }

    pub fn driverName(self: *const Device) []const u8 {
        return self.driver[0..self.driver_length];
    }

    pub fn shaderFormat(self: *const Device) DeviceError!manifest.Format {
        const format = try formatForDriver(self.driverName());
        if (self.shader_formats & formatFlag(format) == 0) return DeviceError.UnsupportedShaderFormat;
        return format;
    }
};

test "driver selects its portable shader format" {
    try std.testing.expectEqual(manifest.Format.dxil, try formatForDriver("direct3d12"));
    try std.testing.expectEqual(manifest.Format.spirv, try formatForDriver("vulkan"));
    try std.testing.expectEqual(manifest.Format.msl, try formatForDriver("metal"));
    try std.testing.expectError(DeviceError.UnsupportedDriver, formatForDriver("unknown"));
}

test "device fake API handles failure, partial init, and exactly-once release" {
    const Context = struct { creates: u32 = 0, destroys: u32 = 0, fail: bool = false };
    var context = Context{};
    const Fake = struct {
        var ctx: *Context = undefined;
        fn create(_: u32, _: bool, _: ?[*:0]const u8) ?*anyopaque {
            ctx.creates += 1;
            if (ctx.fail) return null;
            return @ptrCast(ctx);
        }
        fn destroy(_: *anyopaque) void {
            ctx.destroys += 1;
        }
        fn errorText() []const u8 {
            return "fake SDL failure";
        }
        fn driver(_: *anyopaque) []const u8 {
            return "direct3d12";
        }
        fn formats(_: *anyopaque) u32 {
            return 1 << 3;
        }
    };
    Fake.ctx = &context;
    const api = DeviceApi{ .create = Fake.create, .destroy = Fake.destroy, .error_text = Fake.errorText, .driver_name = Fake.driver, .shader_formats = Fake.formats, .claim_window = undefined, .release_window = undefined, .swapchain_format = undefined, .configure_swapchain = undefined, .acquire_command_buffer = undefined, .wait_acquire_swapchain = undefined, .submit_command_buffer = undefined, .cancel_command_buffer = undefined, .begin_clear_pass = undefined, .end_render_pass = undefined };
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
