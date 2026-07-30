const std = @import("std");
const device_mod = @import("device.zig");
const frame_mod = @import("frame.zig");
const sdl = @import("sdl.zig");

pub const Renderer = struct {
    allocator: std.mem.Allocator,
    device: ?device_mod.Device = null,
    frame: frame_mod.Frame = .{},
    resources: std.AutoHashMapUnmanaged(u64, void) = .empty,
    buffers: std.AutoHashMapUnmanaged(u64, BufferResource) = .empty,
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

    pub const BufferResource = struct {
        gpu: *sdl.GpuBuffer,
        size: u32,
        stride: u32,
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
        if (self.device) |device| {
            var iterator = self.buffers.valueIterator();
            while (iterator.next()) |buffer| sdl.releaseBuffer(@ptrCast(@alignCast(device.handle.?)), buffer.gpu);
        }
        self.buffers.deinit(self.allocator);
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

    pub fn createBuffer(self: *Renderer, element_count: u32, format: u32, stride: u32) !u64 {
        const device = &(self.device orelse return error.NoDevice);
        const size = std.math.mul(u32, element_count, stride) catch return error.BufferTooLarge;
        const usage: sdl.c.SDL_GPUBufferUsageFlags = if (format == 101 or format == 102)
            sdl.c.SDL_GPU_BUFFERUSAGE_INDEX
        else
            sdl.c.SDL_GPU_BUFFERUSAGE_VERTEX;
        const gpu = sdl.createBuffer(@ptrCast(@alignCast(device.handle.?)), usage, size) orelse return error.BufferCreateFailed;
        errdefer sdl.releaseBuffer(@ptrCast(@alignCast(device.handle.?)), gpu);
        const id = self.next_resource_handle;
        self.next_resource_handle += 1;
        try self.buffers.put(self.allocator, id, .{ .gpu = gpu, .size = size, .stride = stride });
        return id;
    }

    pub fn uploadBuffer(self: *Renderer, id: u64, data: *const anyopaque, byte_length: u32, byte_offset: u32) !void {
        const resource = self.buffers.get(id) orelse return error.InvalidBuffer;
        if (byte_length == 0 or byte_offset > resource.size or byte_length > resource.size - byte_offset) return error.BufferUploadOutOfBounds;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const transfer = sdl.createUploadBuffer(gpu_device, byte_length) orelse return error.TransferBufferCreateFailed;
        defer sdl.releaseTransferBuffer(gpu_device, transfer);
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(@as([*]u8, @ptrCast(mapped))[0..byte_length], @as([*]const u8, @ptrCast(data))[0..byte_length]);
        sdl.unmapTransferBuffer(gpu_device, transfer);
        const command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.uploadBuffer(gpu_device, command, transfer, resource.gpu, byte_offset, byte_length)) {
            _ = sdl.cancelCommandBuffer(command);
            return error.CopyPassFailed;
        }
        if (!sdl.submitCommandBuffer(command)) return error.SubmitFailed;
        if (!sdl.waitForIdle(gpu_device)) return error.WaitForIdleFailed;
    }

    pub fn destroyBuffer(self: *Renderer, id: u64) !void {
        const resource = self.buffers.fetchRemove(id) orelse return error.InvalidBuffer;
        const device = &(self.device orelse return error.NoDevice);
        sdl.releaseBuffer(@ptrCast(@alignCast(device.handle.?)), resource.value.gpu);
    }
};
