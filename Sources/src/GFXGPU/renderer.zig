const std = @import("std");
const device_mod = @import("device.zig");
const frame_mod = @import("frame.zig");
const sdl = @import("sdl.zig");
const manifest = @import("shader_manifest.zig");

const io_c = @cImport({
    @cInclude("stdio.h");
});

pub const Renderer = struct {
    allocator: std.mem.Allocator,
    device: ?device_mod.Device = null,
    frame: frame_mod.Frame = .{},
    resources: std.AutoHashMapUnmanaged(u64, void) = .empty,
    textures: std.AutoHashMapUnmanaged(u64, TextureResource) = .empty,
    buffers: std.AutoHashMapUnmanaged(u64, BufferResource) = .empty,
    temporary_buffers: std.ArrayListUnmanaged(u64) = .empty,
    next_resource_handle: u64 = 1,
    window: ?*anyopaque = null,
    window_claimed: bool = false,
    swapchain_format: u32 = 0,
    drawable_width: u32 = 0,
    drawable_height: u32 = 0,
    scene_texture: ?*sdl.GpuTexture = null,
    scene_depth: ?*sdl.GpuTexture = null,
    shader_directory: ?[]u8 = null,
    untextured_vertex_shader: ?*anyopaque = null,
    untextured_fragment_shader: ?*anyopaque = null,
    untextured_pipeline: ?*anyopaque = null,
    textured_vertex_shader: ?*anyopaque = null,
    textured_fragment_shader: ?*anyopaque = null,
    textured_pipeline: ?*anyopaque = null,
    sampler: ?*sdl.c.SDL_GPUSampler = null,
    linear_sampler: ?*sdl.c.SDL_GPUSampler = null,
    use_linear_sampler: bool = false,
    bound_texture: ?u64 = null,
    bound_vertex_buffer: ?u64 = null,
    viewport: ?ViewportState = null,
    world_matrix: [16]f32 = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },
    view_proj_matrix: [16]f32 = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },
    draw_color: [4]f32 = .{ 1, 1, 1, 1 },
    last_error: []const u8 = "",

    pub const ViewportState = struct { x: f32, y: f32, width: f32, height: f32, min_depth: f32, max_depth: f32 };

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
    pub const TextureResource = struct { gpu: *sdl.GpuTexture, width: u32, height: u32 };

    const MatrixUniforms = extern struct { matrix: [16]f32, padding: [4]f32 };
    const DrawUniforms = extern struct { matrix: [16]f32, color: [4]f32 };

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
            var texture_iterator = self.textures.valueIterator();
            while (texture_iterator.next()) |texture| sdl.releaseTexture(gpu_device, texture.gpu);
            if (self.scene_texture) |texture| sdl.releaseTexture(gpu_device, texture);
            if (self.scene_depth) |texture| sdl.releaseTexture(gpu_device, texture);
            if (self.textured_pipeline) |pipeline| sdl.c.SDL_ReleaseGPUGraphicsPipeline(gpu_device, @ptrCast(@alignCast(pipeline)));
            if (self.textured_fragment_shader) |shader| sdl.c.SDL_ReleaseGPUShader(gpu_device, @ptrCast(@alignCast(shader)));
            if (self.textured_vertex_shader) |shader| sdl.c.SDL_ReleaseGPUShader(gpu_device, @ptrCast(@alignCast(shader)));
            if (self.sampler) |sampler| sdl.c.SDL_ReleaseGPUSampler(gpu_device, sampler);
            if (self.linear_sampler) |sampler| sdl.c.SDL_ReleaseGPUSampler(gpu_device, sampler);
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
        self.textures.deinit(self.allocator);
        self.temporary_buffers.deinit(self.allocator);
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
        self.scene_texture = sdl.createColorTexture(@ptrCast(@alignCast(device.handle.?)), @intCast(self.swapchain_format), width, height) orelse return error.SceneTextureCreateFailed;
        self.scene_depth = sdl.createDepthTexture(@ptrCast(@alignCast(device.handle.?)), width, height) orelse return error.DepthTextureCreateFailed;
    }

    pub fn setShaderDirectory(self: *Renderer, directory: ?[*:0]const u8) !void {
        if (directory) |value| {
            const bytes = std.mem.span(value);
            self.shader_directory = try self.allocator.dupe(u8, bytes);
        }
    }

    pub fn resize(self: *Renderer, width: u32, height: u32) !void {
        if (width == 0 or height == 0) return error.InvalidViewport;
        const device = &(self.device orelse return error.NoDevice);
        if (self.frame.state != .idle) return error.InvalidState;
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        if (self.scene_texture) |texture| sdl.releaseTexture(gpu_device, texture);
        if (self.scene_depth) |texture| sdl.releaseTexture(gpu_device, texture);
        self.scene_texture = null;
        self.scene_depth = null;
        self.scene_texture = sdl.createColorTexture(gpu_device, @intCast(self.swapchain_format), width, height) orelse return error.SceneTextureCreateFailed;
        self.scene_depth = sdl.createDepthTexture(gpu_device, width, height) orelse return error.DepthTextureCreateFailed;
        self.drawable_width = width;
        self.drawable_height = height;
    }

    pub fn beginFrame(self: *Renderer) !bool {
        const device = &(self.device orelse return error.NoDevice);
        const window = self.window orelse return error.NoWindow;
        if (self.frame.state != .idle) return error.InvalidState;
        // Validate/create the pipeline before acquiring a swapchain texture.
        // SDL_GPU cannot cancel a command buffer after acquisition, so a
        // pipeline failure must happen before the frame enters that state.
        _ = try self.ensureUntexturedPipeline();
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
        if (self.scene_texture) |scene| {
            const command = self.frame.command_buffer orelse return error.InvalidState;
            const swapchain = self.frame.swapchain_texture orelse return error.InvalidState;
            sdl.blitTexture(@ptrCast(@alignCast(command)), scene, @ptrCast(@alignCast(swapchain)), self.drawable_width, self.drawable_height);
        }
        try self.frame.end();
    }

    pub fn clear(self: *Renderer, color: [4]f32) !void {
        if (self.frame.state != .recording) return error.InvalidState;
        const device = &(self.device orelse return error.NoDevice);
        const texture: *sdl.GpuTexture = if (self.scene_texture) |scene| scene else @ptrCast(@alignCast(self.frame.swapchain_texture orelse return error.InvalidState));
        const command = self.frame.command_buffer orelse return error.InvalidState;
        // Keep the reference path's render pass color-only until depth state is
        // wired through the legacy adapter.  Some D3D12 devices reject the
        // temporary D24S8 target during pass creation even though the texture
        // itself was created successfully.
        const pass: ?*anyopaque = if (self.scene_texture != null)
            @ptrCast(sdl.beginColorPass(@ptrCast(@alignCast(command)), @ptrCast(@alignCast(texture)), color, sdl.c.SDL_GPU_LOADOP_CLEAR))
        else
            device.api.begin_clear_pass(command, @ptrCast(texture), color);
        const render_pass = pass orelse return error.RenderPassFailed;
        self.frame.beginPass() catch {
            if (self.scene_texture != null) {
                sdl.endRenderPass(@ptrCast(@alignCast(render_pass)));
            } else {
                device.api.end_render_pass(render_pass);
            }
            return error.InvalidState;
        };
        self.frame.render_pass = render_pass;
    }

    pub fn setViewport(self: *Renderer, viewport: ViewportState) !void {
        if (self.frame.state != .recording and self.frame.state != .pass_active) return error.InvalidState;
        if (viewport.width <= 0 or viewport.height <= 0 or viewport.min_depth < 0 or viewport.max_depth > 1 or viewport.min_depth > viewport.max_depth) return error.InvalidViewport;
        self.viewport = viewport;
        if (self.frame.render_pass) |pass| {
            sdl.setViewport(@ptrCast(@alignCast(pass)), viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth);
        }
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
        self.releaseTemporaryBuffers();
    }

    pub fn cancelFrame(self: *Renderer) void {
        if (self.frame.render_pass) |pass| {
            if (self.device) |device| device.api.end_render_pass(pass);
        }
        if (self.frame.command_buffer) |command| {
            if (self.device) |device| {
                // SDL forbids cancelling a command buffer after a swapchain
                // texture has been acquired. Close the pass and submit the
                // command buffer when that acquisition already happened.
                if (self.frame.swapchain_texture != null) {
                    _ = device.api.submit_command_buffer(command);
                } else {
                    _ = device.api.cancel_command_buffer(command);
                }
            }
        }
        self.frame.cancel();
    }

    pub fn createBuffer(self: *Renderer, element_count: u32, format: u32, stride: u32, usage_flags: u32) !u64 {
        const device = &(self.device orelse return error.NoDevice);
        const size = std.math.mul(u32, element_count, stride) catch return error.BufferTooLarge;
        const usage: sdl.c.SDL_GPUBufferUsageFlags = if ((usage_flags & 2) != 0 or format == 101 or format == 102)
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

    fn textureFormat(format: u32) ?sdl.c.SDL_GPUTextureFormat {
        return switch (format) {
            6 => sdl.c.SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
            32 => sdl.c.SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            else => null,
        };
    }

    pub fn createTexture(self: *Renderer, width: u32, height: u32, format: u32) !u64 {
        const device = &(self.device orelse return error.NoDevice);
        const texture_format = textureFormat(format) orelse return error.UnsupportedTextureFormat;
        const info = sdl.c.SDL_GPUTextureCreateInfo{ .type = sdl.c.SDL_GPU_TEXTURETYPE_2D, .format = texture_format, .usage = sdl.c.SDL_GPU_TEXTUREUSAGE_SAMPLER, .width = width, .height = height, .layer_count_or_depth = 1, .num_levels = 1, .sample_count = sdl.c.SDL_GPU_SAMPLECOUNT_1, .props = 0 };
        const gpu = sdl.c.SDL_CreateGPUTexture(@ptrCast(@alignCast(device.handle.?)), &info) orelse return error.TextureCreateFailed;
        errdefer sdl.releaseTexture(@ptrCast(@alignCast(device.handle.?)), gpu);
        const id = self.next_resource_handle;
        self.next_resource_handle += 1;
        try self.textures.put(self.allocator, id, .{ .gpu = gpu, .width = width, .height = height });
        return id;
    }

    pub fn uploadTexture(self: *Renderer, id: u64, data: *const anyopaque, byte_length: u32, row_pitch: u32) !void {
        const texture = self.textures.get(id) orelse return error.InvalidTexture;
        if (row_pitch < texture.width * 4 or byte_length < row_pitch * texture.height) return error.TextureUploadOutOfBounds;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const transfer = sdl.createUploadBuffer(gpu_device, row_pitch * texture.height) orelse return error.TransferBufferCreateFailed;
        defer sdl.releaseTransferBuffer(gpu_device, transfer);
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(@as([*]u8, @ptrCast(mapped))[0 .. row_pitch * texture.height], @as([*]const u8, @ptrCast(data))[0 .. row_pitch * texture.height]);
        sdl.unmapTransferBuffer(gpu_device, transfer);
        const command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.uploadTexture(command, transfer, texture.gpu, texture.width, texture.height, row_pitch)) {
            _ = sdl.cancelCommandBuffer(command);
            return error.CopyPassFailed;
        }
        if (!sdl.submitCommandBuffer(command)) return error.SubmitFailed;
        if (!sdl.waitForIdle(gpu_device)) return error.WaitForIdleFailed;
    }

    pub fn destroyTexture(self: *Renderer, id: u64) !void {
        const texture = self.textures.fetchRemove(id) orelse return error.InvalidTexture;
        const device = &(self.device orelse return error.NoDevice);
        sdl.releaseTexture(@ptrCast(@alignCast(device.handle.?)), texture.value.gpu);
        if (self.bound_texture == id) self.bound_texture = null;
    }

    pub fn bindTexture(self: *Renderer, id: u64) !void {
        if (!self.textures.contains(id)) return error.InvalidTexture;
        self.bound_texture = id;
    }

    pub fn bindVertexBuffer(self: *Renderer, id: u64) !void {
        if (!self.buffers.contains(id)) return error.InvalidBuffer;
        self.bound_vertex_buffer = id;
    }

    fn releaseTemporaryBuffers(self: *Renderer) void {
        while (self.temporary_buffers.pop()) |id| self.destroyBuffer(id) catch {};
    }

    pub fn drawTemporary(self: *Renderer, data: *const anyopaque, byte_length: u32, stride: u32, primitive_count: u32) !void {
        if (byte_length == 0 or stride == 0 or byte_length % stride != 0 or primitive_count == 0) return error.InvalidDraw;
        const vertex_count = std.math.mul(u32, primitive_count, 3) catch return error.InvalidDraw;
        if (vertex_count > byte_length / stride) return error.InvalidDraw;
        const id = try self.createBuffer(byte_length / stride, 0, stride, 0);
        errdefer self.destroyBuffer(id) catch {};
        try self.uploadBuffer(id, data, byte_length, 0);
        try self.draw(id, primitive_count);
        try self.temporary_buffers.append(self.allocator, id);
    }

    fn readShader(self: *Renderer, name: []const u8) ![]u8 {
        const directory = self.shader_directory orelse return error.ShaderDirectoryMissing;
        const path = try std.fmt.allocPrint(self.allocator, "{s}/{s}", .{ directory, name });
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

    fn shaderName(self: *Renderer, effect: []const u8, stage: []const u8, format: manifest.Format) ![]u8 {
        const suffix = switch (format) {
            .dxil => "dxil",
            .spirv => "spirv",
            .msl => "msl",
        };
        return std.fmt.allocPrint(self.allocator, "{s}.{s}.{s}", .{ effect, stage, suffix });
    }

    fn ensureUntexturedPipeline(self: *Renderer) !*anyopaque {
        if (self.untextured_pipeline) |pipeline| return pipeline;
        const device = &(self.device orelse return error.NoDevice);
        const format = try device.shaderFormat();
        const shader_format = device_mod.formatFlag(format);
        const vertex_name = try self.shaderName("untextured", "vertex", format);
        defer self.allocator.free(vertex_name);
        const fragment_name = try self.shaderName("untextured", "fragment", format);
        defer self.allocator.free(fragment_name);
        const vertex_code = try self.readShader(vertex_name);
        defer self.allocator.free(vertex_code);
        const fragment_code = try self.readShader(fragment_name);
        defer self.allocator.free(fragment_code);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        var vertex_entry: [13:0]u8 = undefined;
        @memcpy(vertex_entry[0..12], "vs_untextured"[0..12]);
        vertex_entry[12] = 0;
        var fragment_entry: [13:0]u8 = undefined;
        @memcpy(fragment_entry[0..12], "ps_untextured"[0..12]);
        fragment_entry[12] = 0;
        const vertex_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = vertex_code.len, .code = vertex_code.ptr, .entrypoint = &vertex_entry, .format = shader_format, .stage = sdl.c.SDL_GPU_SHADERSTAGE_VERTEX, .num_samplers = 0, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 2, .props = 0 };
        const vertex = sdl.c.SDL_CreateGPUShader(gpu_device, &vertex_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, vertex);
        // ps_untextured only consumes the interpolated vertex color. Its
        // cbuffer declarations are shared source declarations, not bindings
        // referenced by the fragment stage.
        const fragment_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = fragment_code.len, .code = fragment_code.ptr, .entrypoint = &fragment_entry, .format = shader_format, .stage = sdl.c.SDL_GPU_SHADERSTAGE_FRAGMENT, .num_samplers = 0, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 0, .props = 0 };
        const fragment = sdl.c.SDL_CreateGPUShader(gpu_device, &fragment_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, fragment);
        var vertex_buffers = [_]sdl.c.SDL_GPUVertexBufferDescription{.{ .slot = 0, .pitch = 32, .input_rate = sdl.c.SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 }};
        // DXIL exposes POSITION0 and COLOR0 as input locations 0 and 1.
        // The legacy engine's color semantic is tracked as location 5 in its
        // vertex-layout mask, but SDL_GPU pipeline locations follow the shader
        // signature rather than that legacy numbering.
        var attributes = [_]sdl.c.SDL_GPUVertexAttribute{ .{ .location = 0, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 }, .{ .location = 1, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = 12 } };
        const color_format: sdl.c.SDL_GPUTextureFormat = @intCast(self.swapchain_format);
        const target = sdl.c.SDL_GPUColorTargetDescription{ .format = color_format, .blend_state = .{ .src_color_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .dst_color_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .color_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .src_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .dst_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .alpha_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .color_write_mask = 0x0f, .enable_blend = false, .enable_color_write_mask = true } };
        const pipeline_info = sdl.c.SDL_GPUGraphicsPipelineCreateInfo{ .vertex_shader = vertex, .fragment_shader = fragment, .vertex_input_state = .{ .vertex_buffer_descriptions = &vertex_buffers, .num_vertex_buffers = 1, .vertex_attributes = &attributes, .num_vertex_attributes = 2 }, .primitive_type = sdl.c.SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, .rasterizer_state = .{ .fill_mode = sdl.c.SDL_GPU_FILLMODE_FILL, .cull_mode = sdl.c.SDL_GPU_CULLMODE_NONE, .front_face = sdl.c.SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE, .enable_depth_clip = true }, .multisample_state = .{ .sample_count = sdl.c.SDL_GPU_SAMPLECOUNT_1 }, .depth_stencil_state = .{}, .target_info = .{ .color_target_descriptions = &target, .num_color_targets = 1, .depth_stencil_format = 0, .has_depth_stencil_target = false }, .props = 0 };
        const pipeline = sdl.c.SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info) orelse return error.PipelineCreateFailed;
        errdefer sdl.c.SDL_ReleaseGPUGraphicsPipeline(gpu_device, pipeline);
        self.untextured_vertex_shader = vertex;
        self.untextured_fragment_shader = fragment;
        self.untextured_pipeline = pipeline;
        return pipeline;
    }

    fn pushUntexturedUniforms(self: *Renderer) !void {
        const command = self.frame.command_buffer orelse return error.InvalidState;
        const frame_uniforms = MatrixUniforms{ .matrix = self.view_proj_matrix, .padding = .{ 0, 0, 0, 0 } };
        const draw_uniforms = DrawUniforms{ .matrix = self.world_matrix, .color = self.draw_color };
        sdl.pushVertexUniformData(@ptrCast(@alignCast(command)), 0, @ptrCast(&frame_uniforms), @sizeOf(MatrixUniforms));
        sdl.pushVertexUniformData(@ptrCast(@alignCast(command)), 1, @ptrCast(&draw_uniforms), @sizeOf(DrawUniforms));
    }

    fn ensureTexturedPipeline(self: *Renderer) !*anyopaque {
        if (self.textured_pipeline) |pipeline| return pipeline;
        const device = &(self.device orelse return error.NoDevice);
        const format = try device.shaderFormat();
        const shader_format = device_mod.formatFlag(format);
        const vertex_name = try self.shaderName("textured", "vertex", format);
        defer self.allocator.free(vertex_name);
        const fragment_name = try self.shaderName("textured", "fragment", format);
        defer self.allocator.free(fragment_name);
        const vertex_code = try self.readShader(vertex_name);
        defer self.allocator.free(vertex_code);
        const fragment_code = try self.readShader(fragment_name);
        defer self.allocator.free(fragment_code);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        var vertex_entry: [13:0]u8 = undefined;
        @memcpy(vertex_entry[0..11], "vs_textured"[0..11]);
        vertex_entry[11] = 0;
        var fragment_entry: [13:0]u8 = undefined;
        @memcpy(fragment_entry[0..11], "ps_textured"[0..11]);
        fragment_entry[11] = 0;
        const vertex_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = vertex_code.len, .code = vertex_code.ptr, .entrypoint = &vertex_entry, .format = shader_format, .stage = sdl.c.SDL_GPU_SHADERSTAGE_VERTEX, .num_samplers = 0, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 2, .props = 0 };
        const vertex = sdl.c.SDL_CreateGPUShader(gpu_device, &vertex_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, vertex);
        const fragment_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = fragment_code.len, .code = fragment_code.ptr, .entrypoint = &fragment_entry, .format = shader_format, .stage = sdl.c.SDL_GPU_SHADERSTAGE_FRAGMENT, .num_samplers = 1, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 0, .props = 0 };
        const fragment = sdl.c.SDL_CreateGPUShader(gpu_device, &fragment_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, fragment);
        var vertex_buffers = [_]sdl.c.SDL_GPUVertexBufferDescription{.{ .slot = 0, .pitch = 32, .input_rate = sdl.c.SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 }};
        var attributes = [_]sdl.c.SDL_GPUVertexAttribute{
            .{ .location = 0, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 },
            .{ .location = 1, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = 12 },
            .{ .location = 2, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 20 },
        };
        const target = sdl.c.SDL_GPUColorTargetDescription{ .format = @intCast(self.swapchain_format), .blend_state = .{ .src_color_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_SRC_ALPHA, .dst_color_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, .color_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .src_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .dst_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .alpha_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .color_write_mask = 0x0f, .enable_blend = true, .enable_color_write_mask = true } };
        const info = sdl.c.SDL_GPUGraphicsPipelineCreateInfo{ .vertex_shader = vertex, .fragment_shader = fragment, .vertex_input_state = .{ .vertex_buffer_descriptions = &vertex_buffers, .num_vertex_buffers = 1, .vertex_attributes = &attributes, .num_vertex_attributes = 3 }, .primitive_type = sdl.c.SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, .rasterizer_state = .{ .fill_mode = sdl.c.SDL_GPU_FILLMODE_FILL, .cull_mode = sdl.c.SDL_GPU_CULLMODE_NONE, .front_face = sdl.c.SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE, .enable_depth_clip = true }, .multisample_state = .{ .sample_count = sdl.c.SDL_GPU_SAMPLECOUNT_1 }, .depth_stencil_state = .{ .compare_op = sdl.c.SDL_GPU_COMPAREOP_ALWAYS, .enable_depth_test = false, .enable_depth_write = false }, .target_info = .{ .color_target_descriptions = &target, .num_color_targets = 1, .depth_stencil_format = sdl.c.SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, .has_depth_stencil_target = true }, .props = 0 };
        const pipeline = sdl.c.SDL_CreateGPUGraphicsPipeline(gpu_device, &info) orelse return error.PipelineCreateFailed;
        errdefer sdl.c.SDL_ReleaseGPUGraphicsPipeline(gpu_device, pipeline);
        self.textured_vertex_shader = vertex;
        self.textured_fragment_shader = fragment;
        self.textured_pipeline = pipeline;
        self.sampler = sdl.createSampler(gpu_device, false) orelse return error.SamplerCreateFailed;
        self.linear_sampler = sdl.createSampler(gpu_device, true) orelse return error.SamplerCreateFailed;
        return pipeline;
    }

    pub fn draw(self: *Renderer, vertex_buffer: u64, primitive_count: u32) !void {
        if (primitive_count == 0) return error.InvalidDraw;
        const pass = self.frame.render_pass orelse return error.InvalidState;
        const buffer = self.buffers.get(vertex_buffer) orelse return error.InvalidBuffer;
        self.bound_vertex_buffer = vertex_buffer;
        const pipeline = if (self.bound_texture != null) try self.ensureTexturedPipeline() else try self.ensureUntexturedPipeline();
        try self.pushUntexturedUniforms();
        sdl.bindPipeline(@ptrCast(@alignCast(pass)), @ptrCast(@alignCast(pipeline)));
        if (self.bound_texture) |texture_id| {
            const texture = self.textures.get(texture_id) orelse return error.InvalidTexture;
            const sampler = if (self.use_linear_sampler) self.linear_sampler else self.sampler;
            const sampler_handle = sampler orelse return error.SamplerMissing;
            sdl.bindFragmentSampler(@ptrCast(@alignCast(pass)), texture.gpu, sampler_handle);
        }
        if (self.viewport) |viewport| sdl.setViewport(@ptrCast(@alignCast(pass)), viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth);
        sdl.bindVertexBuffer(@ptrCast(@alignCast(pass)), buffer.gpu, 0);
        const vertex_count = std.math.mul(u32, primitive_count, 3) catch return error.InvalidDraw;
        sdl.drawPrimitives(@ptrCast(@alignCast(pass)), vertex_count, 0);
    }

    pub fn drawIndexed(self: *Renderer, index_buffer: u64, index_size: u32, first_index: u32, index_count: u32, vertex_offset: i32) !void {
        if (index_count == 0 or (index_size != 2 and index_size != 4)) return error.InvalidDraw;
        const pass = self.frame.render_pass orelse return error.InvalidState;
        const buffer = self.buffers.get(index_buffer) orelse return error.InvalidBuffer;
        const vertex_id = self.bound_vertex_buffer orelse return error.VertexBufferMissing;
        const vertex_buffer = self.buffers.get(vertex_id) orelse return error.InvalidBuffer;
        const index_offset = std.math.mul(u32, first_index, index_size) catch return error.InvalidDraw;
        if (index_offset > buffer.size or index_count > (buffer.size - index_offset) / index_size) return error.InvalidDraw;
        const pipeline = if (self.bound_texture != null) try self.ensureTexturedPipeline() else try self.ensureUntexturedPipeline();
        try self.pushUntexturedUniforms();
        sdl.bindPipeline(@ptrCast(@alignCast(pass)), @ptrCast(@alignCast(pipeline)));
        if (self.bound_texture) |texture_id| {
            const texture = self.textures.get(texture_id) orelse return error.InvalidTexture;
            const sampler = if (self.use_linear_sampler) self.linear_sampler else self.sampler;
            const sampler_handle = sampler orelse return error.SamplerMissing;
            sdl.bindFragmentSampler(@ptrCast(@alignCast(pass)), texture.gpu, sampler_handle);
        }
        if (self.viewport) |viewport| sdl.setViewport(@ptrCast(@alignCast(pass)), viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth);
        sdl.bindVertexBuffer(@ptrCast(@alignCast(pass)), vertex_buffer.gpu, 0);
        if (!sdl.bindIndexBuffer(@ptrCast(@alignCast(pass)), buffer.gpu, 0, index_size)) return error.InvalidDraw;
        sdl.drawIndexedPrimitives(@ptrCast(@alignCast(pass)), index_count, first_index, vertex_offset);
    }

    pub fn readback(self: *Renderer, destination: []u8, width: u32, height: u32, row_pitch: u32) !void {
        const texture = self.scene_texture orelse return error.ReadbackUnavailable;
        if (width == 0 or height == 0 or row_pitch < width * 4 or destination.len < @as(usize, row_pitch) * height) return error.ReadbackInvalid;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const transfer = sdl.createDownloadBuffer(gpu_device, row_pitch * height) orelse return error.TransferBufferCreateFailed;
        defer sdl.releaseTransferBuffer(gpu_device, transfer);
        const command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.downloadTexture(command, texture, transfer, width, height)) {
            _ = sdl.cancelCommandBuffer(command);
            return error.CopyPassFailed;
        }
        if (!sdl.submitCommandBuffer(command)) return error.SubmitFailed;
        if (!sdl.waitForIdle(gpu_device)) return error.WaitForIdleFailed;
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(destination[0 .. @as(usize, row_pitch) * height], @as([*]const u8, @ptrCast(mapped))[0 .. @as(usize, row_pitch) * height]);
        sdl.unmapTransferBuffer(gpu_device, transfer);
    }
};
