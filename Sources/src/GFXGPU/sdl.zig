const sdl3 = @import("sdl3");

pub const c = sdl3.c;
pub const GpuDevice = c.SDL_GPUDevice;
pub const GpuCommandBuffer = c.SDL_GPUCommandBuffer;
pub const GpuTexture = c.SDL_GPUTexture;
pub const GpuRenderPass = c.SDL_GPURenderPass;
pub const GpuBuffer = c.SDL_GPUBuffer;
pub const GpuTransferBuffer = c.SDL_GPUTransferBuffer;
pub const GpuCopyPass = c.SDL_GPUCopyPass;
pub const GpuPipeline = c.SDL_GPUGraphicsPipeline;
pub const Window = c.SDL_Window;
pub const ShaderFormat = c.SDL_GPUShaderFormat;
pub const shaderformat_dxil: ShaderFormat = c.SDL_GPU_SHADERFORMAT_DXIL;

pub fn createGpuDevice(format_flags: ShaderFormat, debug_mode: bool, preferred_driver: ?[*:0]const u8) ?*GpuDevice {
    return c.SDL_CreateGPUDevice(format_flags, debug_mode, preferred_driver);
}

pub fn destroyGpuDevice(device: *GpuDevice) void {
    c.SDL_DestroyGPUDevice(device);
}
pub fn getError() []const u8 {
    return std.mem.span(c.SDL_GetError());
}

pub fn claimWindow(device: *GpuDevice, window: *Window) bool {
    return c.SDL_ClaimWindowForGPUDevice(device, window);
}
pub fn releaseWindow(device: *GpuDevice, window: *Window) void {
    c.SDL_ReleaseWindowFromGPUDevice(device, window);
}
pub fn swapchainFormat(device: *GpuDevice, window: *Window) c.SDL_GPUTextureFormat {
    return c.SDL_GetGPUSwapchainTextureFormat(device, window);
}
pub fn configureSwapchain(device: *GpuDevice, window: *Window) bool {
    const mode = if (c.SDL_WindowSupportsGPUPresentMode(device, window, c.SDL_GPU_PRESENTMODE_MAILBOX)) c.SDL_GPU_PRESENTMODE_MAILBOX else c.SDL_GPU_PRESENTMODE_VSYNC;
    return c.SDL_SetGPUSwapchainParameters(device, window, c.SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
}
pub fn acquireCommandBuffer(device: *GpuDevice) ?*GpuCommandBuffer {
    return c.SDL_AcquireGPUCommandBuffer(device);
}
pub fn waitAcquireSwapchain(command_buffer: *GpuCommandBuffer, window: *Window, texture: *?*GpuTexture, width: *u32, height: *u32) bool {
    return c.SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, texture, width, height);
}
pub fn submitCommandBuffer(command_buffer: *GpuCommandBuffer) bool {
    return c.SDL_SubmitGPUCommandBuffer(command_buffer);
}
pub fn cancelCommandBuffer(command_buffer: *GpuCommandBuffer) bool {
    return c.SDL_CancelGPUCommandBuffer(command_buffer);
}

pub fn beginClearPass(command_buffer: *GpuCommandBuffer, texture: *GpuTexture, color: [4]f32) ?*GpuRenderPass {
    var target = c.SDL_GPUColorTargetInfo{
        .texture = texture,
        .clear_color = .{ .r = color[0], .g = color[1], .b = color[2], .a = color[3] },
        .load_op = c.SDL_GPU_LOADOP_CLEAR,
        .store_op = c.SDL_GPU_STOREOP_STORE,
    };
    return c.SDL_BeginGPURenderPass(command_buffer, &target, 1, null);
}

pub fn endRenderPass(render_pass: *GpuRenderPass) void {
    c.SDL_EndGPURenderPass(render_pass);
}

pub fn createColorTexture(device: *GpuDevice, format: c.SDL_GPUTextureFormat, width: u32, height: u32) ?*GpuTexture {
    const info = c.SDL_GPUTextureCreateInfo{ .type = c.SDL_GPU_TEXTURETYPE_2D, .format = format, .usage = c.SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | c.SDL_GPU_TEXTUREUSAGE_SAMPLER, .width = width, .height = height, .layer_count_or_depth = 1, .num_levels = 1, .sample_count = c.SDL_GPU_SAMPLECOUNT_1, .props = 0 };
    return c.SDL_CreateGPUTexture(device, &info);
}

pub fn createDepthTexture(device: *GpuDevice, width: u32, height: u32) ?*GpuTexture {
    const info = c.SDL_GPUTextureCreateInfo{ .type = c.SDL_GPU_TEXTURETYPE_2D, .format = c.SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, .usage = c.SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, .width = width, .height = height, .layer_count_or_depth = 1, .num_levels = 1, .sample_count = c.SDL_GPU_SAMPLECOUNT_1, .props = 0 };
    return c.SDL_CreateGPUTexture(device, &info);
}

pub fn releaseTexture(device: *GpuDevice, texture: *GpuTexture) void {
    c.SDL_ReleaseGPUTexture(device, texture);
}

pub fn uploadTexture(command_buffer: *GpuCommandBuffer, transfer: *GpuTransferBuffer, texture: *GpuTexture, width: u32, height: u32, row_pitch: u32) bool {
    const copy_pass = c.SDL_BeginGPUCopyPass(command_buffer) orelse return false;
    const source = c.SDL_GPUTextureTransferInfo{ .transfer_buffer = transfer, .offset = 0, .pixels_per_row = row_pitch / 4, .rows_per_layer = height };
    const destination = c.SDL_GPUTextureRegion{ .texture = texture, .w = width, .h = height, .d = 1 };
    c.SDL_UploadToGPUTexture(copy_pass, &source, &destination, false);
    c.SDL_EndGPUCopyPass(copy_pass);
    return true;
}

pub fn createSampler(device: *GpuDevice) ?*c.SDL_GPUSampler {
    const info = c.SDL_GPUSamplerCreateInfo{ .min_filter = c.SDL_GPU_FILTER_NEAREST, .mag_filter = c.SDL_GPU_FILTER_NEAREST, .mipmap_mode = c.SDL_GPU_SAMPLERMIPMAPMODE_NEAREST, .address_mode_u = c.SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, .address_mode_v = c.SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, .address_mode_w = c.SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, .mip_lod_bias = 0, .max_anisotropy = 1, .compare_op = c.SDL_GPU_COMPAREOP_ALWAYS, .min_lod = 0, .max_lod = 0, .enable_anisotropy = false, .enable_compare = false, .padding1 = 0, .padding2 = 0, .props = 0 };
    return c.SDL_CreateGPUSampler(device, &info);
}

pub fn bindFragmentSampler(render_pass: *GpuRenderPass, texture: *GpuTexture, sampler: *c.SDL_GPUSampler) void {
    const binding = c.SDL_GPUTextureSamplerBinding{ .texture = texture, .sampler = sampler };
    c.SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
}

pub fn beginColorPass(command_buffer: *GpuCommandBuffer, texture: *GpuTexture, color: [4]f32, load: c.SDL_GPULoadOp) ?*GpuRenderPass {
    var target = c.SDL_GPUColorTargetInfo{ .texture = texture, .clear_color = .{ .r = color[0], .g = color[1], .b = color[2], .a = color[3] }, .load_op = load, .store_op = c.SDL_GPU_STOREOP_STORE };
    return c.SDL_BeginGPURenderPass(command_buffer, &target, 1, null);
}

pub fn beginColorDepthPass(command_buffer: *GpuCommandBuffer, color_texture: *GpuTexture, depth_texture: *GpuTexture, color: [4]f32, load: c.SDL_GPULoadOp) ?*GpuRenderPass {
    const color_target = c.SDL_GPUColorTargetInfo{ .texture = color_texture, .clear_color = .{ .r = color[0], .g = color[1], .b = color[2], .a = color[3] }, .load_op = load, .store_op = c.SDL_GPU_STOREOP_STORE };
    const depth_target = c.SDL_GPUDepthStencilTargetInfo{ .texture = depth_texture, .clear_depth = 1, .load_op = c.SDL_GPU_LOADOP_CLEAR, .store_op = c.SDL_GPU_STOREOP_STORE, .stencil_load_op = c.SDL_GPU_LOADOP_CLEAR, .stencil_store_op = c.SDL_GPU_STOREOP_STORE, .cycle = false, .clear_stencil = 0, .mip_level = 0, .layer = 0 };
    return c.SDL_BeginGPURenderPass(command_buffer, &color_target, 1, &depth_target);
}

pub fn blitTexture(command_buffer: *GpuCommandBuffer, source: *GpuTexture, destination: *GpuTexture, width: u32, height: u32) void {
    const info = c.SDL_GPUBlitInfo{ .source = .{ .texture = source, .w = width, .h = height }, .destination = .{ .texture = destination, .w = width, .h = height }, .load_op = c.SDL_GPU_LOADOP_LOAD, .filter = c.SDL_GPU_FILTER_NEAREST };
    c.SDL_BlitGPUTexture(command_buffer, &info);
}

pub fn createBuffer(device: *GpuDevice, usage: c.SDL_GPUBufferUsageFlags, size: u32) ?*GpuBuffer {
    const info = c.SDL_GPUBufferCreateInfo{ .usage = usage, .size = size, .props = 0 };
    return c.SDL_CreateGPUBuffer(device, &info);
}

pub fn releaseBuffer(device: *GpuDevice, buffer: *GpuBuffer) void {
    c.SDL_ReleaseGPUBuffer(device, buffer);
}

pub fn createUploadBuffer(device: *GpuDevice, size: u32) ?*GpuTransferBuffer {
    const info = c.SDL_GPUTransferBufferCreateInfo{ .usage = c.SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size, .props = 0 };
    return c.SDL_CreateGPUTransferBuffer(device, &info);
}

pub fn releaseTransferBuffer(device: *GpuDevice, buffer: *GpuTransferBuffer) void {
    c.SDL_ReleaseGPUTransferBuffer(device, buffer);
}

pub fn mapTransferBuffer(device: *GpuDevice, buffer: *GpuTransferBuffer) ?*anyopaque {
    return c.SDL_MapGPUTransferBuffer(device, buffer, true);
}

pub fn unmapTransferBuffer(device: *GpuDevice, buffer: *GpuTransferBuffer) void {
    c.SDL_UnmapGPUTransferBuffer(device, buffer);
}

pub fn uploadBuffer(device: *GpuDevice, command_buffer: *GpuCommandBuffer, transfer: *GpuTransferBuffer, destination: *GpuBuffer, byte_offset: u32, byte_length: u32) bool {
    _ = device;
    const copy_pass = c.SDL_BeginGPUCopyPass(command_buffer) orelse return false;
    var source = c.SDL_GPUTransferBufferLocation{ .transfer_buffer = transfer, .offset = 0 };
    var target = c.SDL_GPUBufferRegion{ .buffer = destination, .offset = byte_offset, .size = byte_length };
    c.SDL_UploadToGPUBuffer(copy_pass, &source, &target, false);
    c.SDL_EndGPUCopyPass(copy_pass);
    return true;
}

pub fn waitForIdle(device: *GpuDevice) bool {
    return c.SDL_WaitForGPUIdle(device);
}

pub fn createDownloadBuffer(device: *GpuDevice, size: u32) ?*GpuTransferBuffer {
    const info = c.SDL_GPUTransferBufferCreateInfo{ .usage = c.SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = size, .props = 0 };
    return c.SDL_CreateGPUTransferBuffer(device, &info);
}

pub fn downloadTexture(command_buffer: *GpuCommandBuffer, source: *GpuTexture, transfer: *GpuTransferBuffer, width: u32, height: u32) bool {
    const copy_pass = c.SDL_BeginGPUCopyPass(command_buffer) orelse return false;
    const source_region = c.SDL_GPUTextureRegion{ .texture = source, .w = width, .h = height, .d = 1 };
    const destination = c.SDL_GPUTextureTransferInfo{ .transfer_buffer = transfer, .offset = 0, .pixels_per_row = width, .rows_per_layer = height };
    c.SDL_DownloadFromGPUTexture(copy_pass, &source_region, &destination);
    c.SDL_EndGPUCopyPass(copy_pass);
    return true;
}

pub fn bindPipeline(render_pass: *GpuRenderPass, pipeline: *GpuPipeline) void {
    c.SDL_BindGPUGraphicsPipeline(render_pass, pipeline);
}

pub fn setViewport(render_pass: *GpuRenderPass, x: f32, y: f32, width: f32, height: f32, min_depth: f32, max_depth: f32) void {
    const viewport = c.SDL_GPUViewport{ .x = x, .y = y, .w = width, .h = height, .min_depth = min_depth, .max_depth = max_depth };
    c.SDL_SetGPUViewport(render_pass, &viewport);
}

pub fn bindVertexBuffer(render_pass: *GpuRenderPass, buffer: *GpuBuffer, offset: u32) void {
    const binding = c.SDL_GPUBufferBinding{ .buffer = buffer, .offset = offset };
    c.SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);
}

pub fn bindIndexBuffer(render_pass: *GpuRenderPass, buffer: *GpuBuffer, offset: u32, index_size: u32) bool {
    const binding = c.SDL_GPUBufferBinding{ .buffer = buffer, .offset = offset };
    const element_size = switch (index_size) {
        2 => c.SDL_GPU_INDEXELEMENTSIZE_16BIT,
        4 => c.SDL_GPU_INDEXELEMENTSIZE_32BIT,
        else => return false,
    };
    c.SDL_BindGPUIndexBuffer(render_pass, &binding, element_size);
    return true;
}

pub fn drawPrimitives(render_pass: *GpuRenderPass, vertex_count: u32, first_vertex: u32) void {
    c.SDL_DrawGPUPrimitives(render_pass, vertex_count, 1, first_vertex, 0);
}

pub fn drawIndexedPrimitives(render_pass: *GpuRenderPass, index_count: u32, first_index: u32, vertex_offset: i32) void {
    c.SDL_DrawGPUIndexedPrimitives(render_pass, index_count, 1, first_index, vertex_offset, 0);
}

pub fn pushVertexUniformData(command_buffer: *GpuCommandBuffer, slot: u32, data: *const anyopaque, byte_length: u32) void {
    c.SDL_PushGPUVertexUniformData(command_buffer, slot, data, byte_length);
}

const std = @import("std");
