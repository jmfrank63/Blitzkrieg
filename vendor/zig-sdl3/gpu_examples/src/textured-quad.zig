const options = @import("options");
const sdl3 = @import("sdl3");
const std = @import("std");

comptime {
    _ = sdl3.main_callbacks;
}

// Disable main hack.
pub const _start = void;
pub const WinMainCRTStartup = void;

const example_name = "Textured Quad";

const init_flags = sdl3.InitFlags{ .video = true };
const shader_formats = sdl3.gpu.ShaderFormatFlags{
    .dxil = true,
    .msl = true,
    .spirv = true,
};

// Shaders.
const vert_shader = @import("texturedQuad.vert.zig");
const frag_shader = @import("texturedQuad.frag.zig");
const vert_shader_metadata = sdl3.extras.gpu.getShaderMetadataField(vert_shader);
const frag_shader_metadata = sdl3.extras.gpu.getShaderMetadataField(frag_shader);
comptime {
    sdl3.extras.gpu.ensureCompatibleGraphicsShadersComptime(
        vert_shader_metadata,
        frag_shader_metadata,
    );
}

const ravioli_bmp = @embedFile("images/ravioli.bmp");

const window_width = 640;
const window_height = 480;

const other_buffers = &.{};
const Vertex = sdl3.extras.gpu.vertexBufferTypes(
    vert_shader_metadata,
    other_buffers,
)[0];

const vertices = [_]Vertex{
    .{ .pos = .{ -1, 1, 0 }, .tex_coord = .{ 0, 0 } },
    .{ .pos = .{ 1, 1, 0 }, .tex_coord = .{ 4, 0 } },
    .{ .pos = .{ 1, -1, 0 }, .tex_coord = .{ 4, 4 } },
    .{ .pos = .{ -1, -1, 0 }, .tex_coord = .{ 0, 4 } },
};
const vertices_bytes = std.mem.asBytes(&vertices);

const indices = [_]u16{
    0,
    1,
    2,
    0,
    2,
    3,
};
const indices_bytes = std.mem.asBytes(&indices);

const sampler_names = [_][]const u8{
    "PointClamp",
    "PointWrap",
    "LinearClamp",
    "LinearWrap",
    "AnisotropicClamp",
    "AnisotropicWrap",
};

const AppState = struct {
    device: sdl3.gpu.Device,
    window: sdl3.video.Window,
    pipeline: sdl3.gpu.GraphicsPipeline,
    vertex_buffer: sdl3.gpu.Buffer,
    index_buffer: sdl3.gpu.Buffer,
    texture: sdl3.gpu.Texture,
    samplers: [sampler_names.len]sdl3.gpu.Sampler,
    curr_sampler: usize = 0,
};

pub fn loadImage(
    bmp: []const u8,
) !sdl3.surface.Surface {
    const image_data_raw = try sdl3.surface.Surface.initFromBmpIo(try sdl3.io_stream.Stream.initFromConstMem(bmp), true);
    defer image_data_raw.deinit();
    return image_data_raw.convertFormat(sdl3.pixels.Format.packed_abgr_8_8_8_8);
}

pub fn init(
    init_data: sdl3.Init,
) !struct { AppState, sdl3.AppResult } {

    // SDL3 setup.
    _ = try sdl3.setMemoryFunctionsByAllocator(init_data.gpa);
    sdl3.log.setLogOutputFunction(void, &sdl3.extras.loggers.zigLog, null);
    try sdl3.init(init_flags);

    // Get our GPU device that supports SPIR-V.
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
            // .vertex_buffer_descriptions = vertex_buffers_info.vertex_buffer_descriptions,
            // .vertex_attributes = vertex_buffers_info.vertex_attributes,
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
    const pipeline = try device.createGraphicsPipeline(pipeline_create_info);
    errdefer device.releaseGraphicsPipeline(pipeline);

    // Create samplers.
    var samplers_initialized_num: usize = 0;
    var samplers: [sampler_names.len]sdl3.gpu.Sampler = undefined;
    errdefer for (samplers[0..samplers_initialized_num]) |sampler| {
        device.releaseSampler(sampler);
    };
    for (&samplers, 0..) |*sampler, sampler_ind| {
        const filter_mode: sdl3.gpu.Filter = if (sampler_ind < 2) .nearest else .linear;
        const mipmap_mode: sdl3.gpu.SamplerMipmapMode = if (sampler_ind < 2) .nearest else .linear;
        const address_mode: sdl3.gpu.SamplerAddressMode = if (sampler_ind % 2 == 0) .clamp_to_edge else .repeat;
        const max_anisotropy: ?f32 = if (sampler_ind > 3) 4 else null;
        sampler.* = try device.createSampler(.{
            .min_filter = filter_mode,
            .mag_filter = filter_mode,
            .mipmap_mode = mipmap_mode,
            .address_mode_u = address_mode,
            .address_mode_v = address_mode,
            .address_mode_w = address_mode,
            .max_anisotropy = max_anisotropy,
        });
        samplers_initialized_num += 1;
    }

    // Prepare vertex buffer.
    const vertex_buffer = try device.createBuffer(.{
        .usage = .{ .vertex = true },
        .size = vertices_bytes.len,
    });
    errdefer device.releaseBuffer(vertex_buffer);

    // Create the index buffer.
    const index_buffer = try device.createBuffer(.{
        .usage = .{ .index = true },
        .size = indices_bytes.len,
    });
    errdefer device.releaseBuffer(index_buffer);

    // Load the image.
    const image_data = try loadImage(ravioli_bmp);
    defer image_data.deinit();
    const image_bytes = image_data.getPixels().?[0 .. image_data.getWidth() * image_data.getHeight() * @sizeOf(u8) * 4];

    // Create texture.
    const texture = try device.createTexture(.{
        .texture_type = .two_dimensional,
        .format = .r8g8b8a8_unorm,
        .width = @intCast(image_data.getWidth()),
        .height = @intCast(image_data.getHeight()),
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = .{ .sampler = true },
        .props = .{ .name = "Ravioli Texture" },
    });
    errdefer device.releaseTexture(texture);

    // Setup transfer buffer.
    const transfer_buffer_vertex_data_off = 0;
    const transfer_buffer_index_data_off = transfer_buffer_vertex_data_off + vertices_bytes.len;
    const transfer_buffer_image_data_off = transfer_buffer_index_data_off + indices_bytes.len;
    const transfer_buffer = try device.createTransferBuffer(.{
        .usage = .upload,
        .size = @intCast(vertices_bytes.len + indices_bytes.len + image_bytes.len),
    });
    defer device.releaseTransferBuffer(transfer_buffer);
    {
        const transfer_buffer_mapped = try device.mapTransferBuffer(transfer_buffer, false);
        defer device.unmapTransferBuffer(transfer_buffer);
        @memcpy(transfer_buffer_mapped[transfer_buffer_vertex_data_off .. transfer_buffer_vertex_data_off + vertices_bytes.len], vertices_bytes);
        @memcpy(transfer_buffer_mapped[transfer_buffer_index_data_off .. transfer_buffer_index_data_off + indices_bytes.len], indices_bytes);
        @memcpy(transfer_buffer_mapped[transfer_buffer_image_data_off .. transfer_buffer_image_data_off + image_bytes.len], image_bytes);
    }

    // Upload transfer data.
    const cmd_buf = try device.acquireCommandBuffer();
    {
        const copy_pass = cmd_buf.beginCopyPass();
        defer copy_pass.end();
        copy_pass.uploadToBuffer(
            .{
                .transfer_buffer = transfer_buffer,
                .offset = transfer_buffer_vertex_data_off,
            },
            .{
                .buffer = vertex_buffer,
                .offset = 0,
                .size = vertices_bytes.len,
            },
            false,
        );
        copy_pass.uploadToBuffer(
            .{
                .transfer_buffer = transfer_buffer,
                .offset = transfer_buffer_index_data_off,
            },
            .{
                .buffer = index_buffer,
                .offset = 0,
                .size = indices_bytes.len,
            },
            false,
        );
        copy_pass.uploadToTexture(
            .{
                .transfer_buffer = transfer_buffer,
                .offset = transfer_buffer_image_data_off,
            },
            .{
                .texture = texture,
                .width = @intCast(image_data.getWidth()),
                .height = @intCast(image_data.getHeight()),
                .depth = 1,
            },
            false,
        );
    }
    try cmd_buf.submit();

    // Finish setup.
    return .{
        .{
            .device = device,
            .window = window,
            .pipeline = pipeline,
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
            .texture = texture,
            .samplers = samplers,
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
        render_pass.bindGraphicsPipeline(app_state.pipeline);
        render_pass.bindVertexBuffers(
            0,
            &.{
                .{ .buffer = app_state.vertex_buffer, .offset = 0 },
            },
        );
        render_pass.bindIndexBuffer(
            .{ .buffer = app_state.index_buffer, .offset = 0 },
            .indices_16bit,
        );
        render_pass.bindFragmentSamplers(
            0,
            &.{
                .{ .texture = app_state.texture, .sampler = app_state.samplers[app_state.curr_sampler] },
            },
        );
        render_pass.drawIndexedPrimitives(6, 1, 0, 0, 0);
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
                        if (app_state.curr_sampler == 0) {
                            app_state.curr_sampler = sampler_names.len - 1;
                        } else app_state.curr_sampler -= 1;
                        changed = true;
                    },
                    .right => {
                        if (app_state.curr_sampler >= sampler_names.len - 1) {
                            app_state.curr_sampler = 0;
                        } else app_state.curr_sampler += 1;
                        changed = true;
                    },
                    else => {},
                };
                if (changed) {
                    try sdl3.log.log("Sampler state: {s}", .{sampler_names[app_state.curr_sampler]});
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
        for (val.samplers) |sampler|
            val.device.releaseSampler(sampler);
        val.device.releaseTexture(val.texture);
        val.device.releaseBuffer(val.index_buffer);
        val.device.releaseBuffer(val.vertex_buffer);
        val.device.releaseGraphicsPipeline(val.pipeline);
        val.device.releaseWindow(val.window);
        val.window.deinit();
        val.device.deinit();
    }

    sdl3.quit(init_flags);
    sdl3.shutdown();
}
