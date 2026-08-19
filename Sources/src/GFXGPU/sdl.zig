const sdl3 = @import("sdl3");
const builtin = @import("builtin");
const present_fit = @import("present_fit.zig");

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
pub const shaderformat_spirv: ShaderFormat = c.SDL_GPU_SHADERFORMAT_SPIRV;
pub const shaderformat_msl: ShaderFormat = c.SDL_GPU_SHADERFORMAT_MSL;

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
// How the swapchain hands finished frames to the display. VSYNC is the only
// mode SDL guarantees every window can present, so it is both the default and
// the fallback. MAILBOX used to be picked unconditionally whenever the window
// supported it, which left the GPU (and the main loop driving it) running flat
// out to produce frames the display never shows - see GFX.Present.Mode.
pub const PresentMode = enum(u32) {
    vsync = 0,
    mailbox = 1,
    immediate = 2,
};

// The value carried across the C ABI. Anything unrecognized is vsync rather
// than an error: an old caller passing a zeroed field must get the safe mode.
pub fn presentModeFromValue(value: u32) PresentMode {
    return switch (value) {
        1 => .mailbox,
        2 => .immediate,
        else => .vsync,
    };
}

// SDL_SetGPUSwapchainParameters rejects a mode the window cannot present and
// takes the whole swapchain configure down with it, so an unsupported request
// degrades to vsync here instead of failing the attach.
pub fn choosePresentMode(requested: PresentMode, mailbox_supported: bool, immediate_supported: bool) PresentMode {
    return switch (requested) {
        .vsync => .vsync,
        .mailbox => if (mailbox_supported) .mailbox else .vsync,
        .immediate => if (immediate_supported) .immediate else .vsync,
    };
}

fn sdlPresentMode(mode: PresentMode) c_uint {
    return switch (mode) {
        .vsync => c.SDL_GPU_PRESENTMODE_VSYNC,
        .mailbox => c.SDL_GPU_PRESENTMODE_MAILBOX,
        .immediate => c.SDL_GPU_PRESENTMODE_IMMEDIATE,
    };
}

pub fn configureSwapchain(device: *GpuDevice, window: *Window, requested: PresentMode) bool {
    const mode = choosePresentMode(
        requested,
        c.SDL_WindowSupportsGPUPresentMode(device, window, c.SDL_GPU_PRESENTMODE_MAILBOX),
        c.SDL_WindowSupportsGPUPresentMode(device, window, c.SDL_GPU_PRESENTMODE_IMMEDIATE),
    );
    return c.SDL_SetGPUSwapchainParameters(device, window, @intCast(c.SDL_GPU_SWAPCHAINCOMPOSITION_SDR), @intCast(sdlPresentMode(mode)));
}

test "an unsupported present mode falls back to vsync, which is always supported" {
    try std.testing.expectEqual(PresentMode.mailbox, choosePresentMode(.mailbox, true, true));
    try std.testing.expectEqual(PresentMode.vsync, choosePresentMode(.mailbox, false, true));
    try std.testing.expectEqual(PresentMode.immediate, choosePresentMode(.immediate, false, true));
    try std.testing.expectEqual(PresentMode.vsync, choosePresentMode(.immediate, true, false));
    // vsync never consults support at all.
    try std.testing.expectEqual(PresentMode.vsync, choosePresentMode(.vsync, false, false));
}

test "present mode crosses the C ABI as a value, unknown meaning vsync" {
    try std.testing.expectEqual(PresentMode.vsync, presentModeFromValue(0));
    try std.testing.expectEqual(PresentMode.mailbox, presentModeFromValue(1));
    try std.testing.expectEqual(PresentMode.immediate, presentModeFromValue(2));
    try std.testing.expectEqual(PresentMode.vsync, presentModeFromValue(99));
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

// The one depth-stencil format the depth texture and every pipeline that
// declares a depth-stencil target must agree on.
pub fn depthFormat() c.SDL_GPUTextureFormat {
    return if (builtin.target.os.tag == .macos)
        c.SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT
    else
        c.SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
}

pub fn createDepthTexture(device: *GpuDevice, width: u32, height: u32) ?*GpuTexture {
    const depth_format = depthFormat();
    const info = c.SDL_GPUTextureCreateInfo{ .type = c.SDL_GPU_TEXTURETYPE_2D, .format = depth_format, .usage = c.SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, .width = width, .height = height, .layer_count_or_depth = 1, .num_levels = 1, .sample_count = c.SDL_GPU_SAMPLECOUNT_1, .props = 0 };
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

// D3D9 addresses textures with D3DTADDRESS_WRAP by default, which the terrain
// depends on: the noise texcoords tile far outside [0,1]. Clamping collapsed
// every one of those lookups onto a single edge texel, so modulating by the
// noise flattened the whole ground to one dark shade.
pub fn createSampler(device: *GpuDevice, linear: bool) ?*c.SDL_GPUSampler {
    const filter = if (linear) c.SDL_GPU_FILTER_LINEAR else c.SDL_GPU_FILTER_NEAREST;
    const info = c.SDL_GPUSamplerCreateInfo{ .min_filter = @intCast(filter), .mag_filter = @intCast(filter), .mipmap_mode = @intCast(c.SDL_GPU_SAMPLERMIPMAPMODE_NEAREST), .address_mode_u = @intCast(c.SDL_GPU_SAMPLERADDRESSMODE_REPEAT), .address_mode_v = @intCast(c.SDL_GPU_SAMPLERADDRESSMODE_REPEAT), .address_mode_w = @intCast(c.SDL_GPU_SAMPLERADDRESSMODE_REPEAT), .mip_lod_bias = 0, .max_anisotropy = 1, .compare_op = @intCast(c.SDL_GPU_COMPAREOP_ALWAYS), .min_lod = 0, .max_lod = 0, .enable_anisotropy = false, .enable_compare = false, .padding1 = 0, .padding2 = 0, .props = 0 };
    return c.SDL_CreateGPUSampler(device, &info);
}

pub fn bindFragmentSampler(render_pass: *GpuRenderPass, texture: *GpuTexture, sampler: *c.SDL_GPUSampler) void {
    const binding = c.SDL_GPUTextureSamplerBinding{ .texture = texture, .sampler = sampler };
    c.SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
}

// The multitextured terrain effects sample a second stage: the noise that
// modulates the ground, or the crosset whose alpha masks a tile transition.
pub fn bindFragmentSamplers2(render_pass: *GpuRenderPass, texture0: *GpuTexture, texture1: *GpuTexture, sampler: *c.SDL_GPUSampler) void {
    const bindings = [_]c.SDL_GPUTextureSamplerBinding{
        .{ .texture = texture0, .sampler = sampler },
        .{ .texture = texture1, .sampler = sampler },
    };
    c.SDL_BindGPUFragmentSamplers(render_pass, 0, &bindings, 2);
}

pub fn beginColorPass(command_buffer: *GpuCommandBuffer, texture: *GpuTexture, color: [4]f32, load: c.SDL_GPULoadOp) ?*GpuRenderPass {
    var target = c.SDL_GPUColorTargetInfo{ .texture = texture, .clear_color = .{ .r = color[0], .g = color[1], .b = color[2], .a = color[3] }, .load_op = load, .store_op = c.SDL_GPU_STOREOP_STORE };
    return c.SDL_BeginGPURenderPass(command_buffer, &target, 1, null);
}

pub fn beginColorDepthPass(command_buffer: *GpuCommandBuffer, color_texture: *GpuTexture, depth_texture: *GpuTexture, color: [4]f32, load: c.SDL_GPULoadOp) ?*GpuRenderPass {
    const color_target = c.SDL_GPUColorTargetInfo{ .texture = color_texture, .clear_color = .{ .r = color[0], .g = color[1], .b = color[2], .a = color[3] }, .load_op = load, .store_op = c.SDL_GPU_STOREOP_STORE };
    const depth_target = c.SDL_GPUDepthStencilTargetInfo{ .texture = depth_texture, .clear_depth = 1, .load_op = c.SDL_GPU_LOADOP_CLEAR, .store_op = c.SDL_GPU_STOREOP_STORE, .stencil_load_op = c.SDL_GPU_LOADOP_CLEAR, .stencil_store_op = c.SDL_GPU_STOREOP_STORE, .cycle = false, .clear_stencil = 0, .mip_level = 0, .layer = 0 };
    const pass = c.SDL_BeginGPURenderPass(command_buffer, &color_target, 1, &depth_target);
    // D3DRS_STENCILREF 0, which the shadow pass compares EQUAL against. SDL does
    // not promise a starting reference, so it is set rather than assumed.
    if (pass) |handle| c.SDL_SetGPUStencilReference(handle, 0);
    return pass;
}

// Presents the scene 1:1 and centered on the drawable: a mode smaller than
// the window leaves black borders (the destination is cleared), a larger one
// shows its middle. Never scaled - a player who picked 1024x768 gets exactly
// those pixels.
// Presents the whole scene scaled to the largest size that fits the drawable
// while keeping its aspect ratio (letterboxed on one axis at most). For menus
// and videos: every control stays visible regardless of the chosen mode.
pub fn blitTextureFit(command_buffer: *GpuCommandBuffer, source: *GpuTexture, source_width: u32, source_height: u32, destination: *GpuTexture, destination_width: u32, destination_height: u32) void {
    if (source_width == 0 or source_height == 0 or destination_width == 0 or destination_height == 0) return;
    const rect = present_fit.fitRect(source_width, source_height, destination_width, destination_height);
    const info = c.SDL_GPUBlitInfo{
        .source = .{ .texture = source, .w = source_width, .h = source_height },
        .destination = .{ .texture = destination, .x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h },
        .load_op = c.SDL_GPU_LOADOP_CLEAR,
        .clear_color = .{ .r = 0, .g = 0, .b = 0, .a = 1 },
        .filter = if (rect.w == source_width and rect.h == source_height) c.SDL_GPU_FILTER_NEAREST else c.SDL_GPU_FILTER_LINEAR,
    };
    c.SDL_BlitGPUTexture(command_buffer, &info);
}

pub fn blitTextureCentered(command_buffer: *GpuCommandBuffer, source: *GpuTexture, source_width: u32, source_height: u32, destination: *GpuTexture, destination_width: u32, destination_height: u32) void {
    const copy_w = @min(source_width, destination_width);
    const copy_h = @min(source_height, destination_height);
    const info = c.SDL_GPUBlitInfo{
        .source = .{ .texture = source, .x = (source_width - copy_w) / 2, .y = (source_height - copy_h) / 2, .w = copy_w, .h = copy_h },
        .destination = .{ .texture = destination, .x = (destination_width - copy_w) / 2, .y = (destination_height - copy_h) / 2, .w = copy_w, .h = copy_h },
        .load_op = c.SDL_GPU_LOADOP_CLEAR,
        .clear_color = .{ .r = 0, .g = 0, .b = 0, .a = 1 },
        .filter = c.SDL_GPU_FILTER_NEAREST,
    };
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
    return uploadBufferCycle(device, command_buffer, transfer, destination, byte_offset, byte_length, false);
}

// cycle asks SDL for a fresh internal backing when the destination is still in
// use, which is what makes a pooled buffer safe to overwrite the frame after it
// was drawn from without stalling.
pub fn uploadBufferCycle(device: *GpuDevice, command_buffer: *GpuCommandBuffer, transfer: *GpuTransferBuffer, destination: *GpuBuffer, byte_offset: u32, byte_length: u32, cycle: bool) bool {
    _ = device;
    const copy_pass = c.SDL_BeginGPUCopyPass(command_buffer) orelse return false;
    var source = c.SDL_GPUTransferBufferLocation{ .transfer_buffer = transfer, .offset = 0 };
    var target = c.SDL_GPUBufferRegion{ .buffer = destination, .offset = byte_offset, .size = byte_length };
    c.SDL_UploadToGPUBuffer(copy_pass, &source, &target, cycle);
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
    c.SDL_BindGPUIndexBuffer(render_pass, &binding, @intCast(element_size));
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

// The two stages have separate uniform bindings. Fragment entry points that read
// a cbuffer -- the dual-texture terrain pass reads g_screen to know which stage
// combine to apply -- got whatever happened to be bound until this was pushed.
pub fn pushFragmentUniformData(command_buffer: *GpuCommandBuffer, slot: u32, data: *const anyopaque, byte_length: u32) void {
    c.SDL_PushGPUFragmentUniformData(command_buffer, slot, data, byte_length);
}

const std = @import("std");
