const std = @import("std");
const device_mod = @import("device.zig");
const frame_mod = @import("frame.zig");
const sdl = @import("sdl.zig");

const io_c = @cImport({
    @cInclude("stdio.h");
});

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
    shader_directory: ?[]u8 = null,
    untextured_vertex_shader: ?*anyopaque = null,
    untextured_fragment_shader: ?*anyopaque = null,
    untextured_pipeline: ?*anyopaque = null,

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
            const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
            if (self.untextured_pipeline) |pipeline| sdl.c.SDL_ReleaseGPUGraphicsPipeline(gpu_device, @ptrCast(@alignCast(pipeline)));
            if (self.untextured_fragment_shader) |shader| sdl.c.SDL_ReleaseGPUShader(gpu_device, @ptrCast(@alignCast(shader)));
            if (self.untextured_vertex_shader) |shader| sdl.c.SDL_ReleaseGPUShader(gpu_device, @ptrCast(@alignCast(shader)));
        }
        if (self.shader_directory) |directory| self.allocator.free(directory);
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

    pub fn setShaderDirectory(self: *Renderer, directory: ?[*:0]const u8) !void {
        if (directory) |value| {
            const bytes = std.mem.span(value);
            self.shader_directory = try self.allocator.dupe(u8, bytes);
        }
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

    fn readShader(self: *Renderer, name: []const u8) ![]u8 {
        const directory = self.shader_directory orelse return error.ShaderDirectoryMissing;
        const path = try std.fmt.allocPrint(self.allocator, "{s}\\{s}", .{ directory, name });
        defer self.allocator.free(path);
        var path_z = try self.allocator.alloc(u8, path.len + 1);
        defer self.allocator.free(path_z);
        @memcpy(path_z[0..path.len], path);
        path_z[path.len] = 0;
        const file = io_c.fopen(@ptrCast(path_z.ptr), "rb") orelse return error.ShaderFileMissing;
        defer _ = io_c.fclose(file);
        if (io_c.fseek(file, 0, io_c.SEEK_END) != 0) return error.ShaderFileReadFailed;
        const length = io_c.ftell(file);
        if (length <= 0 or length > 64 * 1024 * 1024) return error.ShaderFileReadFailed;
        if (io_c.fseek(file, 0, io_c.SEEK_SET) != 0) return error.ShaderFileReadFailed;
        const bytes = try self.allocator.alloc(u8, @intCast(length));
        errdefer self.allocator.free(bytes);
        if (io_c.fread(bytes.ptr, 1, bytes.len, file) != bytes.len) return error.ShaderFileReadFailed;
        return bytes;
    }

    fn ensureUntexturedPipeline(self: *Renderer) !*anyopaque {
        if (self.untextured_pipeline) |pipeline| return pipeline;
        const device = &(self.device orelse return error.NoDevice);
        const vertex_code = try self.readShader("untextured.vertex.dxil");
        defer self.allocator.free(vertex_code);
        const fragment_code = try self.readShader("untextured.fragment.dxil");
        defer self.allocator.free(fragment_code);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        var vertex_entry: [13:0]u8 = undefined;
        @memcpy(vertex_entry[0..12], "vs_untextured"[0..12]);
        vertex_entry[12] = 0;
        var fragment_entry: [13:0]u8 = undefined;
        @memcpy(fragment_entry[0..12], "ps_untextured"[0..12]);
        fragment_entry[12] = 0;
        const vertex_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = vertex_code.len, .code = vertex_code.ptr, .entrypoint = &vertex_entry, .format = sdl.c.SDL_GPU_SHADERFORMAT_DXIL, .stage = sdl.c.SDL_GPU_SHADERSTAGE_VERTEX, .num_samplers = 0, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 2, .props = 0 };
        const vertex = sdl.c.SDL_CreateGPUShader(gpu_device, &vertex_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, vertex);
        const fragment_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = fragment_code.len, .code = fragment_code.ptr, .entrypoint = &fragment_entry, .format = sdl.c.SDL_GPU_SHADERFORMAT_DXIL, .stage = sdl.c.SDL_GPU_SHADERSTAGE_FRAGMENT, .num_samplers = 0, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 2, .props = 0 };
        const fragment = sdl.c.SDL_CreateGPUShader(gpu_device, &fragment_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, fragment);
        var vertex_buffers = [_]sdl.c.SDL_GPUVertexBufferDescription{.{ .slot = 0, .pitch = 16, .input_rate = sdl.c.SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 }};
        var attributes = [_]sdl.c.SDL_GPUVertexAttribute{ .{ .location = 0, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 }, .{ .location = 5, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = 12 } };
        const color_format: sdl.c.SDL_GPUTextureFormat = @intCast(self.swapchain_format);
        const target = sdl.c.SDL_GPUColorTargetDescription{ .format = color_format, .blend_state = .{ .src_color_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .dst_color_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .color_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .src_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .dst_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .alpha_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .color_write_mask = 0x0f, .enable_blend = false, .enable_color_write_mask = true } };
        const pipeline_info = sdl.c.SDL_GPUGraphicsPipelineCreateInfo{ .vertex_shader = vertex, .fragment_shader = fragment, .vertex_input_state = .{ .vertex_buffer_descriptions = &vertex_buffers, .num_vertex_buffers = 1, .vertex_attributes = &attributes, .num_vertex_attributes = 2 }, .primitive_type = sdl.c.SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, .rasterizer_state = .{ .fill_mode = sdl.c.SDL_GPU_FILLMODE_FILL, .cull_mode = sdl.c.SDL_GPU_CULLMODE_NONE, .front_face = sdl.c.SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE, .enable_depth_clip = true }, .multisample_state = .{ .sample_count = sdl.c.SDL_GPU_SAMPLECOUNT_1 }, .depth_stencil_state = .{ .enable_depth_test = false, .enable_depth_write = false }, .target_info = .{ .color_target_descriptions = &target, .num_color_targets = 1, .has_depth_stencil_target = false }, .props = 0 };
        const pipeline = sdl.c.SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info) orelse return error.PipelineCreateFailed;
        errdefer sdl.c.SDL_ReleaseGPUGraphicsPipeline(gpu_device, pipeline);
        self.untextured_vertex_shader = vertex;
        self.untextured_fragment_shader = fragment;
        self.untextured_pipeline = pipeline;
        return pipeline;
    }

    pub fn draw(self: *Renderer, vertex_buffer: u64, primitive_count: u32) !void {
        if (primitive_count == 0) return error.InvalidDraw;
        const pass = self.frame.render_pass orelse return error.InvalidState;
        const buffer = self.buffers.get(vertex_buffer) orelse return error.InvalidBuffer;
        const pipeline = try self.ensureUntexturedPipeline();
        sdl.bindPipeline(@ptrCast(@alignCast(pass)), @ptrCast(@alignCast(pipeline)));
        sdl.bindVertexBuffer(@ptrCast(@alignCast(pass)), buffer.gpu, 0);
        const vertex_count = std.math.mul(u32, primitive_count, 3) catch return error.InvalidDraw;
        sdl.drawPrimitives(@ptrCast(@alignCast(pass)), vertex_count, 0);
    }

    pub fn drawIndexed(self: *Renderer, index_buffer: u64, index_size: u32, first_index: u32, index_count: u32, vertex_offset: i32) !void {
        if (index_count == 0 or (index_size != 2 and index_size != 4)) return error.InvalidDraw;
        const pass = self.frame.render_pass orelse return error.InvalidState;
        const buffer = self.buffers.get(index_buffer) orelse return error.InvalidBuffer;
        const index_offset = std.math.mul(u32, first_index, index_size) catch return error.InvalidDraw;
        if (index_offset > buffer.size or index_count > (buffer.size - index_offset) / index_size) return error.InvalidDraw;
        const pipeline = try self.ensureUntexturedPipeline();
        sdl.bindPipeline(@ptrCast(@alignCast(pass)), @ptrCast(@alignCast(pipeline)));
        if (!sdl.bindIndexBuffer(@ptrCast(@alignCast(pass)), buffer.gpu, 0, index_size)) return error.InvalidDraw;
        sdl.drawIndexedPrimitives(@ptrCast(@alignCast(pass)), index_count, first_index, vertex_offset);
    }
};
