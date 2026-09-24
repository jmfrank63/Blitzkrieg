//! Does this machine have a GPU device SDL can use, and which driver is it?
//!
//! Non-gating by design: it answers a question rather than enforcing anything,
//! and always exits 0. CI has never created a real SDL_GPUDevice - the factory
//! test stubs device creation out with a fake, and the overlay spike is only
//! ever built - so whether the runners can run the engine and overlay tiers is
//! currently unknown rather than known-bad. This prints the fact.
//!
//! Run locally:  zig build gpu-device-probe -Dtarget=aarch64-macos
const std = @import("std");
const sdl3 = @import("sdl3");
const sdl = @import("gfxgpu").sdl;

pub fn main() !void {
    if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) {
        std.debug.print("gpu-probe: video=no ({s})\n", .{sdl.getError()});
        return;
    }
    defer sdl3.c.SDL_Quit();

    // Ask for every shader format: the probe is about whether a device exists
    // at all, not about which shaders this build ships.
    const formats = sdl.shaderformat_dxil | sdl.shaderformat_spirv | sdl.shaderformat_msl;
    const device = sdl.createGpuDevice(formats, false, null) orelse {
        std.debug.print("gpu-probe: video=yes device=no ({s})\n", .{sdl.getError()});
        return;
    };
    defer sdl.destroyGpuDevice(device);

    const driver = sdl3.c.SDL_GetGPUDeviceDriver(device);
    std.debug.print("gpu-probe: video=yes device=yes driver={s}", .{
        if (driver != null) std.mem.span(driver) else "unknown",
    });

    // A device on its own is not enough for the engine tier: it has to take a
    // window, which is where a headless runner tends to stop.
    const window = sdl3.c.SDL_CreateWindow("gpu-device-probe", 64, 64, sdl3.c.SDL_WINDOW_HIDDEN);
    if (window == null) {
        std.debug.print(" window=no ({s})\n", .{sdl.getError()});
        return;
    }
    defer sdl3.c.SDL_DestroyWindow(window);

    if (!sdl.claimWindow(device, window.?)) {
        std.debug.print(" window=yes claim=no ({s})\n", .{sdl.getError()});
        return;
    }
    defer sdl.releaseWindow(device, window.?);
    std.debug.print(" window=yes claim=yes swapchain_format={d}\n", .{sdl.swapchainFormat(device, window.?)});
}
