const options = @import("options");
const sdl3 = @import("sdl3");
const std = @import("std");

comptime {
    _ = sdl3.main_callbacks;
}

// Disable main hack.
pub const _start = void;
pub const WinMainCRTStartup = void;

const example_name = "Clear Screen";
const screen_width = 1024;
const screen_height = 576;
const init_flags = sdl3.InitFlags{ .video = true };
const shader_formats = sdl3.gpu.ShaderFormatFlags{
    .dxil = true,
    .msl = true,
    .spirv = true,
};

const AppState = struct {
    device: sdl3.gpu.Device,
    window: sdl3.video.Window,
};

pub fn init(
    init_data: sdl3.Init,
) !struct { AppState, sdl3.AppResult } {

    // SDL3 setup.
    _ = try sdl3.setMemoryFunctionsByAllocator(init_data.gpa);
    sdl3.log.setLogOutputFunction(void, &sdl3.extras.loggers.zigLog, null);
    try sdl3.init(init_flags);

    // Get our GPU device that supports available shader formats.
    const device = try sdl3.gpu.Device.init(shader_formats, options.gpu_debug, null);
    errdefer device.deinit();

    // Make our demo window.
    const window = try sdl3.video.Window.init(example_name, screen_width, screen_height, .{});
    errdefer window.deinit();
    try device.claimWindow(window);

    // Finish setup.
    return .{
        .{
            .device = device,
            .window = window,
        },
        .run,
    };
}

pub fn iterate(
    app_state: *AppState,
) !sdl3.AppResult {

    // Get command buffer and swapchain texture.
    const cmd_buf = try app_state.device.acquireCommandBuffer();
    const swapchain_texture, _, _ = try cmd_buf.waitAndAcquireSwapchainTexture(app_state.window);
    if (swapchain_texture) |texture| {

        // Start a render pass if the swapchain texture is available. Make sure to clear it.
        const render_pass = cmd_buf.beginRenderPass(&.{
            sdl3.gpu.ColorTargetInfo{
                .texture = texture,
                .clear_color = .{ .r = 0.3, .g = 0.3, .b = 0.5, .a = 1 },
                .load = .clear,
            },
        }, null);
        defer render_pass.end();
    }

    // Finally submit the command buffer.
    try cmd_buf.submit();

    return .run;
}

pub fn event(
    app_state: *AppState,
    curr_event: sdl3.events.Event,
) !sdl3.AppResult {
    _ = app_state;
    switch (curr_event) {
        .terminating => return .success,
        .quit => return .success,
        else => {},
    }
    return .run;
}

pub fn quit(
    app_state: ?*AppState,
    result: sdl3.AppResult,
) void {
    _ = result;
    if (app_state) |val| {
        val.device.releaseWindow(val.window);
        val.window.deinit();
        val.device.deinit();
    }

    sdl3.quit(init_flags);
    sdl3.shutdown();
}
