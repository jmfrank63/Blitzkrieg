const sdl3 = @import("sdl3");

pub const c = sdl3.c;
pub const GpuDevice = c.SDL_GPUDevice;
pub const GpuCommandBuffer = c.SDL_GPUCommandBuffer;
pub const GpuTexture = c.SDL_GPUTexture;
pub const GpuRenderPass = c.SDL_GPURenderPass;
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

const std = @import("std");
