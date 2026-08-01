const options = @import("options");
const sdl3 = @import("sdl3");
const std = @import("std");

comptime {
    _ = sdl3.main_callbacks;
}

// Disable main hack.
pub const _start = void;
pub const WinMainCRTStartup = void;

const example_name = "Basic Triangle";

const init_flags = sdl3.InitFlags{ .video = true };
const shader_formats = sdl3.gpu.ShaderFormatFlags{
    .dxil = true,
    .msl = true,
    .spirv = true,
};

// Shaders.
const vert_shader = @import("rawTriangle.vert.zig");
const frag_shader = @import("solidColor.frag.zig");
const vert_shader_metadata = sdl3.extras.gpu.getShaderMetadataField(vert_shader);
const frag_shader_metadata = sdl3.extras.gpu.getShaderMetadataField(frag_shader);
comptime {
    sdl3.extras.gpu.ensureCompatibleGraphicsShadersComptime(
        vert_shader_metadata,
        frag_shader_metadata,
    );
}

const window_width = 640;
const window_height = 480;
const small_viewport = sdl3.gpu.Viewport{
    .region = .{ .x = 100, .y = 120, .w = 320, .h = 240 },
    .min_depth = 0.1,
    .max_depth = 1.0,
};
const scissor_rect = sdl3.rect.IRect{ .x = 320, .y = 240, .w = 320, .h = 240 };

const AppState = struct {
    device: sdl3.gpu.Device,
    window: sdl3.video.Window,
    fill_pipeline: sdl3.gpu.GraphicsPipeline,
    line_pipeline: sdl3.gpu.GraphicsPipeline,
    use_wireframe_mode: bool = false,
    use_small_viewport: bool = false,
    use_scissor_rect: bool = false,
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
    const window = try sdl3.video.Window.init(example_name, window_width, window_height, .{});
    errdefer window.deinit();
    try device.claimWindow(window);

    // Prepare pipelines.
    const vertex_shader = try sdl3.extras.gpu.loadGraphicsShaderEmbeddedWithMetadata(device, vert_shader);
    defer device.releaseShader(vertex_shader);
    const fragment_shader = try sdl3.extras.gpu.loadGraphicsShaderEmbeddedWithMetadata(device, frag_shader);
    defer device.releaseShader(fragment_shader);
    var pipeline_create_info = sdl3.gpu.GraphicsPipelineCreateInfo{
        .target_info = .{
            .color_target_descriptions = &.{
                .{
                    .format = try device.getSwapchainTextureFormat(window),
                },
            },
        },
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
    };
    const fill_pipeline = try device.createGraphicsPipeline(pipeline_create_info);
    errdefer device.releaseGraphicsPipeline(fill_pipeline);
    pipeline_create_info.rasterizer_state.fill_mode = .line;
    const line_pipeline = try device.createGraphicsPipeline(pipeline_create_info);
    errdefer device.releaseGraphicsPipeline(line_pipeline);

    // Finish setup.
    try sdl3.log.log("Press left to toggle wireframe", .{});
    try sdl3.log.log("Press down to toggle small viewport", .{});
    try sdl3.log.log("Press right to toggle scissor rect", .{});
    try sdl3.log.log(
        "State: {{Wireframe: {any}, SmallViewport: {any}, ScissorRect: {any}}}",
        .{ false, false, false },
    );
    return .{
        .{
            .device = device,
            .window = window,
            .line_pipeline = line_pipeline,
            .fill_pipeline = fill_pipeline,
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
                .clear_color = .{ .r = 0, .g = 0, .b = 0, .a = 1 },
                .load = .clear,
            },
        }, null);
        defer render_pass.end();
        render_pass.bindGraphicsPipeline(if (app_state.use_wireframe_mode) app_state.line_pipeline else app_state.fill_pipeline);
        if (app_state.use_small_viewport)
            render_pass.setViewport(small_viewport);
        if (app_state.use_scissor_rect)
            render_pass.setScissor(scissor_rect);
        render_pass.drawPrimitives(3, 1, 0, 0);
    }

    // Finally submit the command buffer.
    try cmd_buf.submit();

    return .run;
}

pub fn event(
    app_state: *AppState,
    curr_event: sdl3.events.Event,
) !sdl3.AppResult {
    switch (curr_event) {
        .key_down => |key| {
            if (!key.repeat) {
                var changed = false;
                if (key.key) |val| switch (val) {
                    .left => {
                        app_state.use_wireframe_mode = !app_state.use_wireframe_mode;
                        changed = true;
                    },
                    .down => {
                        app_state.use_small_viewport = !app_state.use_small_viewport;
                        changed = true;
                    },
                    .right => {
                        app_state.use_scissor_rect = !app_state.use_scissor_rect;
                        changed = true;
                    },
                    else => {},
                };
                if (changed) {
                    try sdl3.log.log(
                        "State: {{Wireframe: {any}, SmallViewport: {any}, ScissorRect: {any}}}",
                        .{ app_state.use_wireframe_mode, app_state.use_small_viewport, app_state.use_scissor_rect },
                    );
                }
            }
        },
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
        val.device.releaseGraphicsPipeline(val.fill_pipeline);
        val.device.releaseGraphicsPipeline(val.line_pipeline);
        val.device.releaseWindow(val.window);
        val.window.deinit();
        val.device.deinit();
    }

    sdl3.quit(init_flags);
    sdl3.shutdown();
}
