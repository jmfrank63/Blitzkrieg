const std = @import("std");
const device_mod = @import("device.zig");
const frame_mod = @import("frame.zig");

pub const Renderer = struct {
    allocator: std.mem.Allocator,
    device: ?device_mod.Device = null,
    frame: frame_mod.Frame = .{},
    resources: std.AutoHashMapUnmanaged(u64, void) = .empty,
    next_resource_handle: u64 = 1,
    window: ?*anyopaque = null,
    window_claimed: bool = false,
    swapchain_format: u32 = 0,
    drawable_width: u32 = 0,
    drawable_height: u32 = 0,

    pub const LiveCounts = struct {
        textures: u32 = 0,
        buffers: u32 = 0,
        samplers: u32 = 0,
        render_targets: u32 = 0,
        shaders: u32 = 0,
        pipelines: u32 = 0,
        passes: u32 = 0,
    };

    pub fn init(allocator: std.mem.Allocator) Renderer {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *Renderer) void {
        self.cancelFrame();
        if (self.window_claimed) {
            if (self.device) |device| device.api.release_window(device.handle.?, self.window.?);
            self.window_claimed = false;
        }
        self.resources.deinit(self.allocator);
        if (self.device) |*device| device.deinit();
        self.* = undefined;
    }

    pub fn attachWindow(self: *Renderer, window: ?*anyopaque, width: u32, height: u32) !void {
        const device = &(self.device orelse return error.NoDevice);
        const window_ptr = window orelse return error.NullWindow;
        if (!device.api.claim_window(device.handle.?, window_ptr)) return error.ClaimFailed;
        errdefer device.api.release_window(device.handle.?, window_ptr);
        if (!device.api.configure_swapchain(device.handle.?, window_ptr)) return error.SwapchainConfigurationFailed;
        self.window = window_ptr;
        self.window_claimed = true;
        self.swapchain_format = device.api.swapchain_format(device.handle.?, window_ptr);
        self.drawable_width = width;
        self.drawable_height = height;
    }

    pub fn beginFrame(self: *Renderer) !bool {
        const device = &(self.device orelse return error.NoDevice);
        const window = self.window orelse return error.NoWindow;
        if (self.frame.state != .idle) return error.InvalidState;
        const command = device.api.acquire_command_buffer(device.handle.?) orelse return error.CommandBufferFailed;
        var texture: ?*anyopaque = null;
        var width = self.drawable_width;
        var height = self.drawable_height;
        if (!device.api.wait_acquire_swapchain(command, window, &texture, &width, &height)) {
            _ = device.api.cancel_command_buffer(command);
            return error.SwapchainAcquireFailed;
        }
        if (texture == null) {
            _ = device.api.cancel_command_buffer(command);
            self.frame.skipped = true;
            return false;
        }
        self.drawable_width = width;
        self.drawable_height = height;
        try self.frame.begin(true);
        self.frame.command_buffer = command;
        self.frame.swapchain_texture = texture;
        return true;
    }

    pub fn endFrame(self: *Renderer) !void {
        if (self.frame.skipped) return;
        if (self.frame.render_pass) |pass| {
            const device = &(self.device orelse return error.NoDevice);
            device.api.end_render_pass(pass);
            self.frame.render_pass = null;
            self.frame.endPass() catch return error.InvalidState;
        }
        try self.frame.end();
    }

    pub fn clear(self: *Renderer, color: [4]f32) !void {
        if (self.frame.state != .recording) return error.InvalidState;
        const device = &(self.device orelse return error.NoDevice);
        const texture = self.frame.swapchain_texture orelse return error.InvalidState;
        const command = self.frame.command_buffer orelse return error.InvalidState;
        const pass = device.api.begin_clear_pass(command, texture, color) orelse return error.RenderPassFailed;
        self.frame.beginPass() catch {
            device.api.end_render_pass(pass);
            return error.InvalidState;
        };
        self.frame.render_pass = pass;
    }

    pub fn present(self: *Renderer) !void {
        if (self.frame.skipped) {
            self.frame.cancel();
            return;
        }
        const device = &(self.device orelse return error.NoDevice);
        const command = self.frame.command_buffer orelse return error.InvalidState;
        if (!device.api.submit_command_buffer(command)) {
            self.frame.cancel();
            return error.SubmitFailed;
        }
        try self.frame.present();
    }

    pub fn cancelFrame(self: *Renderer) void {
        if (self.frame.render_pass) |pass| {
            if (self.device) |device| device.api.end_render_pass(pass);
        }
        if (self.frame.command_buffer) |command| {
            if (self.device) |device| {
                _ = device.api.cancel_command_buffer(command);
            }
        }
        self.frame.cancel();
    }
};
