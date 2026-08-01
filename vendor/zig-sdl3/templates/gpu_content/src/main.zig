const options = @import("options");
const sdl3 = @import("sdl3");
const std = @import("std");

comptime {
    _ = sdl3.main_callbacks;
}

// Disable main hack.
pub const _start = void;
pub const WinMainCRTStartup = void;

const window_name = "SDL3 GPU Content Template";

const init_flags = sdl3.InitFlags{ .video = true };
const shader_formats = sdl3.gpu.ShaderFormatFlags{
    .dxil = true,
    .msl = true,
    .spirv = true,
};

// Shaders.
const vert_shader = "texturedQuad.vert";
const frag_shader = "texturedQuad.frag";
const comp_shader = "fillTexture.comp";

const shader_entry_point = "main";

const window_width = 640;
const window_height = 480;

const other_buffers = &.{};
const Vertex = extern struct {
    pos: [3]f32,
    tex_coord: [2]f32,
};

const vertices = [_]Vertex{
    .{ .pos = .{ -1, -1, 0 }, .tex_coord = .{ 0, 0 } },
    .{ .pos = .{ 1, -1, 0 }, .tex_coord = .{ 1, 0 } },
    .{ .pos = .{ 1, 1, 0 }, .tex_coord = .{ 1, 1 } },
    .{ .pos = .{ -1, -1, 0 }, .tex_coord = .{ 0, 0 } },
    .{ .pos = .{ 1, 1, 0 }, .tex_coord = .{ 1, 1 } },
    .{ .pos = .{ -1, 1, 0 }, .tex_coord = .{ 0, 1 } },
};
const vertices_bytes = std.mem.asBytes(&vertices);

const AppState = struct {
    device: sdl3.gpu.Device,
    window: sdl3.video.Window,
    draw_pipeline: sdl3.gpu.GraphicsPipeline,
    vertex_buffer: sdl3.gpu.Buffer,
    texture: sdl3.gpu.Texture,
    sampler: sdl3.gpu.Sampler,
};

pub fn init(
    init_data: sdl3.Init,
) !struct { AppState, sdl3.AppResult } {
    const allocator = init_data.gpa;
    const io = init_data.io;

    // SDL3 setup.
    _ = try sdl3.setMemoryFunctionsByAllocator(allocator);
    sdl3.log.setLogOutputFunction(void, &sdl3.extras.loggers.zigLog, null);
    try sdl3.init(init_flags);

    // Get our GPU device that supports SPIR-V.
    const device = try sdl3.gpu.Device.init(shader_formats, options.gpu_debug, null);
    errdefer device.deinit();

    // Make our demo window.
    const window = try sdl3.video.Window.init(window_name, window_width, window_height, .{});
    errdefer window.deinit();
    try device.claimWindow(window);

    // Prepare shader directory.
    var exe_dir_buf: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const exe_dir_len = try std.process.executableDirPath(io, &exe_dir_buf);
    const exe_dir_name: []const u8 = exe_dir_buf[0..exe_dir_len];
    const exe_dir = try std.Io.Dir.openDir(std.Io.Dir.cwd(), io, exe_dir_name, .{});
    defer exe_dir.close(io);
    const shader_dir = try exe_dir.openDir(io, "content/shaders", .{});
    defer shader_dir.close(io);

    // Prepare pipelines.
    var shader_read_buf: [1024]u8 = undefined;
    const vertex_shader = try sdl3.extras.gpu.loadGraphicsShaderFromDirWithMetadata(
        allocator,
        io,
        &shader_read_buf,
        device,
        shader_dir,
        shader_entry_point,
        .vertex,
        vert_shader,
    );
    defer device.releaseShader(vertex_shader);
    const fragment_shader = try sdl3.extras.gpu.loadGraphicsShaderFromDirWithMetadata(
        allocator,
        io,
        &shader_read_buf,
        device,
        shader_dir,
        shader_entry_point,
        .fragment,
        frag_shader,
    );
    defer device.releaseShader(fragment_shader);
    const pipeline_create_info = sdl3.gpu.GraphicsPipelineCreateInfo{
        .target_info = .{
            .color_target_descriptions = &.{
                .{
                    .format = try device.getSwapchainTextureFormat(window),
                },
            },
        },
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = .{
            .vertex_buffer_descriptions = &.{
                .{
                    .slot = 0,
                    .pitch = @sizeOf(Vertex),
                    .input_rate = .vertex,
                },
            },
            .vertex_attributes = &.{
                .{
                    .location = 0,
                    .buffer_slot = 0,
                    .format = .f32x3,
                    .offset = @offsetOf(Vertex, "pos"),
                },
                .{
                    .location = 1,
                    .buffer_slot = 0,
                    .format = .f32x2,
                    .offset = @offsetOf(Vertex, "tex_coord"),
                },
            },
        },
    };
    const draw_pipeline = try device.createGraphicsPipeline(pipeline_create_info);
    errdefer device.releaseGraphicsPipeline(draw_pipeline);

    // Prepare texture and sampler.
    const texture = try device.createTexture(.{
        .format = .r8g8b8a8_unorm,
        .width = window_width,
        .height = window_height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = .{ .compute_storage_write = true, .sampler = true },
    });
    errdefer device.releaseTexture(texture);
    const sampler = try device.createSampler(.{
        .address_mode_u = .repeat,
        .address_mode_v = .repeat,
    });
    errdefer device.releaseSampler(sampler);

    // Prepare vertex buffer.
    const vertex_buffer = try device.createBuffer(.{
        .usage = .{ .vertex = true },
        .size = vertices_bytes.len,
    });
    errdefer device.releaseBuffer(vertex_buffer);

    // Setup transfer buffer.
    const transfer_buffer = try device.createTransferBuffer(.{
        .usage = .upload,
        .size = vertices_bytes.len,
    });
    defer device.releaseTransferBuffer(transfer_buffer);
    {
        const transfer_buffer_mapped = try device.mapTransferBuffer(transfer_buffer, false);
        defer device.unmapTransferBuffer(transfer_buffer);
        @memcpy(transfer_buffer_mapped, vertices_bytes);
    }

    // Create compute pipeline.
    const fill_texture_pipeline, const compute_metadata = try sdl3.extras.gpu.loadComputePipelineFromDirWithMetadata(
        allocator,
        io,
        &shader_read_buf,
        device,
        shader_dir,
        shader_entry_point,
        comp_shader,
    );
    defer device.releaseComputePipeline(fill_texture_pipeline);

    // Upload transfer data.
    const cmd_buf = try device.acquireCommandBuffer();
    {
        const copy_pass = cmd_buf.beginCopyPass();
        defer copy_pass.end();
        copy_pass.uploadToBuffer(
            .{
                .transfer_buffer = transfer_buffer,
                .offset = 0,
            },
            .{
                .buffer = vertex_buffer,
                .offset = 0,
                .size = vertices_bytes.len,
            },
            false,
        );
    }

    // Create fill texture.
    {
        const compute_pass = cmd_buf.beginComputePass(&.{
            .{ .texture = texture },
        }, &.{});
        defer compute_pass.end();
        compute_pass.bindPipeline(fill_texture_pipeline);
        compute_pass.dispatch(window_width / compute_metadata.threadcount_x, window_height / compute_metadata.threadcount_y, compute_metadata.threadcount_z);
    }
    try cmd_buf.submit();

    // Finish setup.
    return .{
        .{
            .device = device,
            .window = window,
            .draw_pipeline = draw_pipeline,
            .vertex_buffer = vertex_buffer,
            .texture = texture,
            .sampler = sampler,
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
        render_pass.bindGraphicsPipeline(app_state.draw_pipeline);
        render_pass.bindVertexBuffers(
            0,
            &.{
                .{ .buffer = app_state.vertex_buffer, .offset = 0 },
            },
        );
        render_pass.bindFragmentSamplers(
            0,
            &.{
                .{ .texture = app_state.texture, .sampler = app_state.sampler },
            },
        );
        render_pass.drawPrimitives(6, 1, 0, 0);
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
        val.device.releaseSampler(val.sampler);
        val.device.releaseTexture(val.texture);
        val.device.releaseBuffer(val.vertex_buffer);
        val.device.releaseGraphicsPipeline(val.draw_pipeline);
        val.device.releaseWindow(val.window);
        val.window.deinit();
        val.device.deinit();
    }

    sdl3.quit(init_flags);
    sdl3.shutdown();
}
