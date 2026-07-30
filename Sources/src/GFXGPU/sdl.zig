const sdl3 = @import("sdl3");

pub const c = sdl3.c;
pub const GpuDevice = c.SDL_GPUDevice;
pub const Window = c.SDL_Window;
pub const ShaderFormat = c.SDL_GPUShaderFormat;
pub const shaderformat_dxil: ShaderFormat = c.SDL_GPU_SHADERFORMAT_DXIL;

pub fn createGpuDevice(format_flags: ShaderFormat, debug_mode: bool, preferred_driver: ?[*:0]const u8) ?*GpuDevice {
    return c.SDL_CreateGPUDevice(format_flags, debug_mode, preferred_driver);
}

pub fn destroyGpuDevice(device: *GpuDevice) void { c.SDL_DestroyGPUDevice(device); }
pub fn getError() []const u8 { return std.mem.span(c.SDL_GetError()); }

const std = @import("std");
