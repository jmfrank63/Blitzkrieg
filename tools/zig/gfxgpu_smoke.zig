const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const gpu = @import("gfxgpu");
const bk_memory = @import("bk_memory_allocator");

const EffectProbe = struct {
    name: []const u8,
    vertex_count: u32,
    attribute_count: u32,
    lighting_layout: bool,
};

const effect_probes = [_]EffectProbe{
    .{ .name = "untextured", .vertex_count = 3, .attribute_count = 2, .lighting_layout = false },
    .{ .name = "textured", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "ui", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "unlit", .vertex_count = 3, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "unlit_textured", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "alpha_test", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "transparent", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "particle_additive", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "particle_modulate", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "transparent_multiply", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "transparent_alpha", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "transparent_additive", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "lightmap_modulate", .vertex_count = 6, .attribute_count = 4, .lighting_layout = false },
    .{ .name = "lightmap_complement", .vertex_count = 6, .attribute_count = 4, .lighting_layout = false },
    .{ .name = "lighting", .vertex_count = 3, .attribute_count = 4, .lighting_layout = true },
    .{ .name = "stencil_write", .vertex_count = 3, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "stencil_test", .vertex_count = 3, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "shadow_sprite", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "shadow_mesh", .vertex_count = 3, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "water", .vertex_count = 6, .attribute_count = 4, .lighting_layout = false },
    .{ .name = "water_single", .vertex_count = 6, .attribute_count = 4, .lighting_layout = false },
    .{ .name = "water_alpha", .vertex_count = 6, .attribute_count = 4, .lighting_layout = false },
    .{ .name = "special_video", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "special_transform", .vertex_count = 6, .attribute_count = 3, .lighting_layout = false },
    .{ .name = "special_depth", .vertex_count = 3, .attribute_count = 3, .lighting_layout = false },
};

fn readFile(init: std.process.Init, path: []const u8) ![]u8 {
    return std.Io.Dir.cwd().readFileAlloc(init.io, path, init.gpa, .limited(64 * 1024 * 1024));
}

fn recordFor(shader_manifest: *const gpu.shader_manifest.Manifest, effect: []const u8, stage: gpu.shader_manifest.Stage, format: gpu.shader_manifest.Format) !*const gpu.shader_manifest.Record {
    for (shader_manifest.records) |*record| {
        if (record.stage == stage and record.format == format and std.mem.eql(u8, record.effect, effect)) return record;
    }
    return error.MissingShaderRecord;
}

fn probePipeline(device: *sdl3.c.SDL_GPUDevice, vertex: *anyopaque, fragment: *anyopaque, attribute_count: u32, lighting_layout: bool) ?*sdl3.c.SDL_GPUGraphicsPipeline {
    var buffers = [_]sdl3.c.SDL_GPUVertexBufferDescription{.{ .slot = 0, .pitch = if (lighting_layout) 36 else if (attribute_count == 4) 32 else if (attribute_count == 3) 24 else 16, .input_rate = sdl3.c.SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 }};
    var attributes = [_]sdl3.c.SDL_GPUVertexAttribute{
        .{ .location = 0, .buffer_slot = 0, .format = sdl3.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 },
        .{ .location = 1, .buffer_slot = 0, .format = sdl3.c.SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = 12 },
        .{ .location = 2, .buffer_slot = 0, .format = if (lighting_layout) sdl3.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3 else sdl3.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 16 },
        .{ .location = 3, .buffer_slot = 0, .format = sdl3.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = if (lighting_layout) 28 else 24 },
    };
    const target = sdl3.c.SDL_GPUColorTargetDescription{ .format = sdl3.c.SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, .blend_state = .{ .src_color_blendfactor = sdl3.c.SDL_GPU_BLENDFACTOR_ONE, .dst_color_blendfactor = sdl3.c.SDL_GPU_BLENDFACTOR_ZERO, .color_blend_op = sdl3.c.SDL_GPU_BLENDOP_ADD, .src_alpha_blendfactor = sdl3.c.SDL_GPU_BLENDFACTOR_ONE, .dst_alpha_blendfactor = sdl3.c.SDL_GPU_BLENDFACTOR_ZERO, .alpha_blend_op = sdl3.c.SDL_GPU_BLENDOP_ADD, .color_write_mask = 0x0f, .enable_blend = false, .enable_color_write_mask = true } };
    // The engine's own choice for this device: RADV and Metal have no D24S8.
    const depth_format = gpu.sdl.chooseDepthFormat(@ptrCast(device));
    const info = sdl3.c.SDL_GPUGraphicsPipelineCreateInfo{ .vertex_shader = @ptrCast(vertex), .fragment_shader = @ptrCast(fragment), .vertex_input_state = .{ .vertex_buffer_descriptions = &buffers, .num_vertex_buffers = 1, .vertex_attributes = &attributes, .num_vertex_attributes = attribute_count }, .primitive_type = sdl3.c.SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, .rasterizer_state = .{ .fill_mode = sdl3.c.SDL_GPU_FILLMODE_FILL, .cull_mode = sdl3.c.SDL_GPU_CULLMODE_NONE, .front_face = sdl3.c.SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE, .enable_depth_clip = true }, .multisample_state = .{ .sample_count = sdl3.c.SDL_GPU_SAMPLECOUNT_1 }, .depth_stencil_state = .{ .compare_op = sdl3.c.SDL_GPU_COMPAREOP_LESS, .enable_depth_test = true, .enable_depth_write = true }, .target_info = .{ .color_target_descriptions = &target, .num_color_targets = 1, .depth_stencil_format = depth_format, .has_depth_stencil_target = true }, .props = 0 };
    return sdl3.c.SDL_CreateGPUGraphicsPipeline(device, &info);
}

fn checkFixtures() !void {
    const diffuse = [_]f32{ 0.2, 0.4, 0.6, 0.8 };
    const draw = [_]f32{ 0.5, 1.0, 0.25, 1.0 };
    const texture = [_]f32{ 0.5, 0.25, 1.0, 0.5 };
    const expected = [_]f32{ 0.05, 0.1, 0.15, 0.4 };
    for (0..4) |index| {
        if (@abs(diffuse[index] * draw[index] * texture[index] - expected[index]) > 0.000001) return error.CpuFixtureFailed;
    }
}

fn runReferenceSmoke(preferred_driver: [:0]const u8) !void {
    const window = sdl3.c.SDL_CreateWindow("GfxGpu reference", 64, 64, 0) orelse return error.SdlWindowFailed;
    defer sdl3.c.SDL_DestroyWindow(window);
    if (!sdl3.c.SDL_ShowWindow(window)) return error.SdlWindowShowFailed;
    sdl3.c.SDL_PumpEvents();
    var api: gpu.abi.Api = undefined;
    api.struct_size = @sizeOf(gpu.abi.Api);
    try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_get_api(gpu.abi.abi_version, &api));
    var renderer: ?*gpu.abi.RendererHandle = null;
    const shader_directory = "zig-out/shaders";
    var create_info = gpu.abi.CreateInfo{ .struct_size = @sizeOf(gpu.abi.CreateInfo), .flags = 1, .sdl_window = @ptrCast(window), .width = 64, .height = 64, .shader_directory_utf8 = shader_directory, .preferred_driver_utf8 = preferred_driver.ptr };
    const create_result = api.create(&create_info, &renderer);
    if (create_result != gpu.error_codes.ok) {
        std.debug.print("GfxGpu reference create failed: result={} SDL={s}\n", .{ create_result, std.mem.span(sdl3.c.SDL_GetError()) });
        return error.ReferenceCreateFailed;
    }
    defer api.destroy(renderer);
    const Vertex = extern struct {
        position: [3]f32,
        color: [4]u8,
        specular: [4]u8 = .{ 0, 0, 0, 0 },
        padding: [3]u32 = .{ 0, 0, 0 },
    };
    const vertices = [_]Vertex{
        .{ .position = .{ 0.0, 0.75, 0.0 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ -0.75, -0.75, 0.0 }, .color = .{ 0, 255, 0, 128 } },
        .{ .position = .{ 0.75, -0.75, 0.0 }, .color = .{ 0, 0, 255, 255 } },
    };
    var buffer: u64 = 0;
    var buffer_info = gpu.abi.BufferCreateInfo{ .struct_size = @sizeOf(gpu.abi.BufferCreateInfo), .element_count = 3, .format = 0, .stride = 32, .usage = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.create_buffer(renderer, &buffer_info, &buffer));
    defer _ = api.destroy_buffer(renderer, buffer);
    var upload = gpu.abi.BufferUploadInfo{ .struct_size = @sizeOf(gpu.abi.BufferUploadInfo), .data = @ptrCast(&vertices), .byte_length = @sizeOf(@TypeOf(vertices)), .byte_offset = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_buffer(renderer, buffer, &upload));
    const indices = [_]u32{ 0, 1, 2 };
    var index_buffer: u64 = 0;
    var index_info = gpu.abi.BufferCreateInfo{ .struct_size = @sizeOf(gpu.abi.BufferCreateInfo), .element_count = indices.len, .format = 102, .stride = @sizeOf(u32), .usage = 2 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.create_buffer(renderer, &index_info, &index_buffer));
    defer _ = api.destroy_buffer(renderer, index_buffer);
    var index_upload = gpu.abi.BufferUploadInfo{ .struct_size = @sizeOf(gpu.abi.BufferUploadInfo), .data = @ptrCast(&indices), .byte_length = @sizeOf(@TypeOf(indices)), .byte_offset = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_buffer(renderer, index_buffer, &index_upload));
    var hashes: [3][32]u8 = undefined;
    for (&hashes, 0..) |*hash, frame_index| {
        sdl3.c.SDL_PumpEvents();
        const begin_result = api.begin_frame(renderer);
        if (begin_result != gpu.error_codes.ok) {
            var diagnostic: [256]u8 = undefined;
            var diagnostic_length: u32 = 0;
            _ = api.get_last_error(renderer, &diagnostic, diagnostic.len, &diagnostic_length);
            std.debug.print("GfxGpu reference begin_frame failed: result={} diagnostic={s} SDL={s}\n", .{ begin_result, diagnostic[0..diagnostic_length], std.mem.span(sdl3.c.SDL_GetError()) });
        }
        try std.testing.expectEqual(gpu.error_codes.ok, begin_result);
        var viewport = gpu.abi.ViewportInfo{ .struct_size = @sizeOf(gpu.abi.ViewportInfo), .x = 0, .y = 0, .width = 32, .height = 32, .min_depth = 0, .max_depth = 1 };
        try std.testing.expectEqual(gpu.error_codes.ok, api.set_viewport(renderer, &viewport));
        var clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
        try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &clear));
        if (frame_index == 0) {
            try std.testing.expectEqual(gpu.error_codes.ok, api.draw(renderer, @intCast(buffer), 1));
        } else if (frame_index == 1) {
            try std.testing.expectEqual(gpu.error_codes.ok, api.bind_vertex_buffer(renderer, buffer));
            try std.testing.expectEqual(gpu.error_codes.ok, api.draw_indexed(renderer, index_buffer, 4, 0, 3, 0));
        } else {
            var temporary = gpu.abi.TemporaryGeometryInfo{ .struct_size = @sizeOf(gpu.abi.TemporaryGeometryInfo), .data = @ptrCast(&vertices), .byte_length = @sizeOf(@TypeOf(vertices)), .stride = 32, .format = 0 };
            try std.testing.expectEqual(gpu.error_codes.ok, api.draw_temporary(renderer, &temporary, 1));
        }
        try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
        try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
        var pixels: [64 * 64 * 4]u8 = undefined;
        var readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&pixels) };
        try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &readback));
        var changed = false;
        var pixel_index: usize = 0;
        while (pixel_index < pixels.len) : (pixel_index += 4) {
            if (std.mem.readInt(u32, pixels[pixel_index..][0..4], .little) != 0x10ff3020) {
                changed = true;
                break;
            }
        }
        try std.testing.expect(changed);
        std.crypto.hash.sha2.Sha256.hash(&pixels, hash, .{});
    }
    try std.testing.expectEqualSlices(u8, &hashes[0], &hashes[1]);
    try std.testing.expectEqualSlices(u8, &hashes[1], &hashes[2]);

    const TexturedVertex = extern struct { position: [3]f32, color: [4]u8, specular: [4]u8 = .{ 0, 0, 0, 0 }, uv: [2]f32, padding: u32 = 0 };
    const textured_vertices = [_]TexturedVertex{
        .{ .position = .{ -0.75, 0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 0.75, 0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 0 } },
        .{ .position = .{ 0.75, -0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 1 } },
        .{ .position = .{ -0.75, 0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 0.75, -0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 1 } },
        .{ .position = .{ -0.75, -0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 1 } },
    };
    var textured_buffer: u64 = 0;
    var textured_info = gpu.abi.BufferCreateInfo{ .struct_size = @sizeOf(gpu.abi.BufferCreateInfo), .element_count = textured_vertices.len, .format = 0, .stride = 32, .usage = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.create_buffer(renderer, &textured_info, &textured_buffer));
    defer _ = api.destroy_buffer(renderer, textured_buffer);
    var textured_upload = gpu.abi.BufferUploadInfo{ .struct_size = @sizeOf(gpu.abi.BufferUploadInfo), .data = @ptrCast(&textured_vertices), .byte_length = @sizeOf(@TypeOf(textured_vertices)), .byte_offset = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_buffer(renderer, textured_buffer, &textured_upload));
    const texture_pixels = [_]u8{ 255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255, 255, 32, 16, 255 };
    var texture: u64 = 0;
    var texture_info = gpu.abi.TextureCreateInfo{ .struct_size = @sizeOf(gpu.abi.TextureCreateInfo), .width = 2, .height = 2, .mip_count = 1, .format = 6, .usage = 1 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.create_texture(renderer, &texture_info, &texture));
    defer _ = api.destroy_texture(renderer, texture);
    var texture_upload = gpu.abi.TextureUploadInfo{ .struct_size = @sizeOf(gpu.abi.TextureUploadInfo), .data = @ptrCast(&texture_pixels), .byte_length = texture_pixels.len, .row_pitch = 8, .mip_level = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_texture(renderer, texture, &texture_upload));
    try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_texture(renderer, texture));
    var textured_clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &textured_clear));
    try std.testing.expectEqual(gpu.error_codes.ok, api.draw(renderer, @intCast(textured_buffer), 2));
    try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
    var textured_pixels: [64 * 64 * 4]u8 = undefined;
    var textured_readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = textured_pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&textured_pixels) };
    try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &textured_readback));
    const textured_probe = std.mem.readInt(u32, textured_pixels[(16 * 64 + 16) * 4 ..][0..4], .little);
    try std.testing.expect(textured_probe != 0x10ff3020);

    // R8 glyph-atlas texture: two disjoint regions uploaded separately, then read back.
    var atlas: u64 = 0;
    var atlas_info = gpu.abi.TextureCreateInfo{ .struct_size = @sizeOf(gpu.abi.TextureCreateInfo), .width = 8, .height = 8, .mip_count = 1, .format = gpu.formats.gfx_pixel_format_r8, .usage = 1 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.create_texture(renderer, &atlas_info, &atlas));
    defer _ = api.destroy_texture(renderer, atlas);
    const zeros: [64]u8 = @splat(0);
    var atlas_clear = gpu.abi.TextureUploadInfo{ .struct_size = @sizeOf(gpu.abi.TextureUploadInfo), .data = @ptrCast(&zeros), .byte_length = zeros.len, .row_pitch = 8, .mip_level = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_texture(renderer, atlas, &atlas_clear));
    const block_a = [_]u8{ 1, 2, 3, 4, 5, 6 }; // 3 x 2 at (1, 1), tightly packed
    const block_b = [_]u8{ 0xA0, 0xA1, 0xFF, 0xB0, 0xB1, 0xFF, 0xC0, 0xC1, 0xFF }; // 2 x 3 at (5, 4), pitch 3, last row short of a pitch
    var region_a = gpu.abi.TextureRegionUploadInfo{ .struct_size = @sizeOf(gpu.abi.TextureRegionUploadInfo), .data = @ptrCast(&block_a), .byte_length = block_a.len, .row_pitch = 3, .mip_level = 0, .x = 1, .y = 1, .w = 3, .h = 2 };
    var region_b = gpu.abi.TextureRegionUploadInfo{ .struct_size = @sizeOf(gpu.abi.TextureRegionUploadInfo), .data = @ptrCast(&block_b), .byte_length = block_b.len - 1, .row_pitch = 3, .mip_level = 0, .x = 5, .y = 4, .w = 2, .h = 3 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_texture_region(renderer, atlas, &region_a));
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_texture_region(renderer, atlas, &region_b));
    var atlas_texels: [64]u8 = undefined;
    const atlas_renderer: *gpu.Renderer = @ptrCast(@alignCast(renderer.?));
    try atlas_renderer.readbackTexture(atlas, &atlas_texels);
    var expected_texels: [64]u8 = @splat(0);
    for (0..2) |row| for (0..3) |column| {
        expected_texels[(1 + row) * 8 + 1 + column] = block_a[row * 3 + column];
    };
    for (0..3) |row| for (0..2) |column| {
        expected_texels[(4 + row) * 8 + 5 + column] = block_b[row * 3 + column];
    };
    try std.testing.expectEqualSlices(u8, &expected_texels, &atlas_texels);
    // A rectangle past the edge, an empty one, a short buffer and a mip level are errors that name themselves.
    var bad = region_a;
    bad.x = 6;
    try std.testing.expectEqual(gpu.error_codes.invalid_argument, api.upload_texture_region(renderer, atlas, &bad));
    var message: [160]u8 = undefined;
    var message_size: u32 = 0;
    try std.testing.expectEqual(gpu.error_codes.ok, api.get_last_error(renderer, &message, message.len, &message_size));
    try std.testing.expect(std.mem.indexOf(u8, message[0..message_size], "x=6 y=1 w=3 h=2") != null);
    bad = region_a;
    bad.w = 0;
    try std.testing.expectEqual(gpu.error_codes.invalid_argument, api.upload_texture_region(renderer, atlas, &bad));
    bad = region_a;
    bad.byte_length = 5;
    try std.testing.expectEqual(gpu.error_codes.invalid_argument, api.upload_texture_region(renderer, atlas, &bad));
    bad = region_a;
    bad.mip_level = 1;
    try std.testing.expectEqual(gpu.error_codes.invalid_argument, api.upload_texture_region(renderer, atlas, &bad));
    bad = region_a;
    bad.x = std.math.maxInt(u32);
    try std.testing.expectEqual(gpu.error_codes.invalid_argument, api.upload_texture_region(renderer, atlas, &bad));
    try std.testing.expectEqual(gpu.error_codes.invalid_argument, api.upload_texture_region(renderer, atlas + 1000, &region_a));
    try atlas_renderer.readbackTexture(atlas, &atlas_texels);
    try std.testing.expectEqualSlices(u8, &expected_texels, &atlas_texels);

    const screen_vertices = [_]TexturedVertex{
        .{ .position = .{ 8, 8, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 56, 8, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 0 } },
        .{ .position = .{ 56, 56, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 1 } },
        .{ .position = .{ 8, 8, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 56, 56, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 1 } },
        .{ .position = .{ 8, 56, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 1 } },
    };
    var screen_upload = gpu.abi.BufferUploadInfo{ .struct_size = @sizeOf(gpu.abi.BufferUploadInfo), .data = @ptrCast(&screen_vertices), .byte_length = @sizeOf(@TypeOf(screen_vertices)), .byte_offset = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_buffer(renderer, textured_buffer, &screen_upload));
    const pixel_world = gpu.abi.MatrixInfo{ .struct_size = @sizeOf(gpu.abi.MatrixInfo), .values = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    const pixel_view_proj = gpu.abi.MatrixInfo{ .struct_size = @sizeOf(gpu.abi.MatrixInfo), .values = .{ 2.0 / 64.0, 0, 0, -1, 0, -2.0 / 64.0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1 } };
    try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
    var screen_viewport = gpu.abi.ViewportInfo{ .struct_size = @sizeOf(gpu.abi.ViewportInfo), .x = 0, .y = 0, .width = 64, .height = 64, .min_depth = 0, .max_depth = 1 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_viewport(renderer, &screen_viewport));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_transform(renderer, &pixel_world, &pixel_view_proj));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_texture(renderer, texture));
    var screen_clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &screen_clear));
    try std.testing.expectEqual(gpu.error_codes.ok, api.draw(renderer, @intCast(textured_buffer), 2));
    try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
    var screen_pixels: [64 * 64 * 4]u8 = undefined;
    var screen_readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = screen_pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&screen_pixels) };
    try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &screen_readback));
    var screen_changed: usize = 0;
    for (0..64) |y| for (0..64) |x| {
        if (std.mem.readInt(u32, screen_pixels[(y * 64 + x) * 4 ..][0..4], .little) != 0x10ff3020) {
            screen_changed += 1;
        }
    };
    try std.testing.expect(screen_changed > 0);
    try std.testing.expect(std.mem.readInt(u32, screen_pixels[(16 * 64 + 16) * 4 ..][0..4], .little) != 0x10ff3020);
    try std.testing.expect(std.mem.readInt(u32, screen_pixels[(48 * 64 + 48) * 4 ..][0..4], .little) != 0x10ff3020);

    // Effect 400 samples an R8 atlas as coverage: rgb stays the vertex colour and alpha becomes
    // vertex alpha * coverage. A fully covered quad tinted green at half alpha over the clear
    // colour must read back as the 50% blend; the texels outside the quad stay the clear colour.
    // The default vertex format is XYZRHW | DIFFUSE | SPECULAR | TEX1, so the colour sits after a
    // four-float position, unlike the three-float TexturedVertex the older cases read.
    const TextVertex = extern struct { position: [4]f32, color: [4]u8, specular: [4]u8 = .{ 0, 0, 0, 0 }, uv: [2]f32 };
    const text_vertices = [_]TextVertex{
        .{ .position = .{ 8, 8, 0, 1 }, .color = .{ 0, 255, 0, 128 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 40, 8, 0, 1 }, .color = .{ 0, 255, 0, 128 }, .uv = .{ 1, 0 } },
        .{ .position = .{ 40, 40, 0, 1 }, .color = .{ 0, 255, 0, 128 }, .uv = .{ 1, 1 } },
        .{ .position = .{ 8, 8, 0, 1 }, .color = .{ 0, 255, 0, 128 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 40, 40, 0, 1 }, .color = .{ 0, 255, 0, 128 }, .uv = .{ 1, 1 } },
        .{ .position = .{ 8, 40, 0, 1 }, .color = .{ 0, 255, 0, 128 }, .uv = .{ 0, 1 } },
    };
    var text_buffer: u64 = 0;
    var text_buffer_info = gpu.abi.BufferCreateInfo{ .struct_size = @sizeOf(gpu.abi.BufferCreateInfo), .element_count = text_vertices.len, .format = 0, .stride = 32, .usage = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.create_buffer(renderer, &text_buffer_info, &text_buffer));
    defer _ = api.destroy_buffer(renderer, text_buffer);
    var text_buffer_upload = gpu.abi.BufferUploadInfo{ .struct_size = @sizeOf(gpu.abi.BufferUploadInfo), .data = @ptrCast(&text_vertices), .byte_length = @sizeOf(@TypeOf(text_vertices)), .byte_offset = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_buffer(renderer, text_buffer, &text_buffer_upload));
    var coverage: u64 = 0;
    var coverage_info = gpu.abi.TextureCreateInfo{ .struct_size = @sizeOf(gpu.abi.TextureCreateInfo), .width = 2, .height = 2, .mip_count = 1, .format = gpu.formats.gfx_pixel_format_r8, .usage = 1 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.create_texture(renderer, &coverage_info, &coverage));
    defer _ = api.destroy_texture(renderer, coverage);
    const full_coverage = [_]u8{ 255, 255, 255, 255 };
    var coverage_upload = gpu.abi.TextureUploadInfo{ .struct_size = @sizeOf(gpu.abi.TextureUploadInfo), .data = @ptrCast(&full_coverage), .byte_length = full_coverage.len, .row_pitch = 2, .mip_level = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.upload_texture(renderer, coverage, &coverage_upload));
    var text_effect = gpu.abi.StateInfo{ .struct_size = @sizeOf(gpu.abi.StateInfo), .kind = 8, .index = 0, .value = 400, .values = @splat(0) };
    var plain_effect = text_effect;
    plain_effect.value = 3;
    try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_viewport(renderer, &screen_viewport));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_transform(renderer, &pixel_world, &pixel_view_proj));
    var text_clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &text_clear));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_state(renderer, &text_effect));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_texture(renderer, coverage));
    try std.testing.expectEqual(gpu.error_codes.ok, api.draw(renderer, @intCast(text_buffer), 2));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_state(renderer, &plain_effect));
    try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
    var text_pixels: [64 * 64 * 4]u8 = undefined;
    var text_readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = text_pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&text_pixels) };
    try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &text_readback));
    // Readback is BGRA. Half-alpha green over the clear colour leaves each colour channel about halfway to the tint.
    const background_bgra = text_pixels[(60 * 64 + 60) * 4 ..][0..4];
    const inside_bgra = text_pixels[(24 * 64 + 24) * 4 ..][0..4];
    const tint_bgra = [4]u8{ 0, 255, 0, 255 };
    for (0..3) |channel| {
        const expected: i32 = @divTrunc(@as(i32, background_bgra[channel]) + @as(i32, tint_bgra[channel]), 2);
        try std.testing.expect(@abs(@as(i32, inside_bgra[channel]) - expected) <= 3);
    }
    // A texel outside the quad is still the clear colour.
    try std.testing.expectEqualSlices(u8, background_bgra, text_pixels[(4 * 64 + 4) * 4 ..][0..4]);
    const depth_vertices = [_]Vertex{
        .{ .position = .{ -0.75, 0.75, 0.75 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ 0.75, 0.75, 0.75 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ 0, -0.75, 0.75 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ -0.75, 0.75, 0.25 }, .color = .{ 0, 0, 255, 255 } },
        .{ .position = .{ 0.75, 0.75, 0.25 }, .color = .{ 0, 0, 255, 255 } },
        .{ .position = .{ 0, -0.75, 0.25 }, .color = .{ 0, 0, 255, 255 } },
    };
    try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_transform(renderer, &pixel_world, &gpu.abi.MatrixInfo{ .struct_size = @sizeOf(gpu.abi.MatrixInfo), .values = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } }));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_texture(renderer, 0));
    var depth_clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &depth_clear));
    var depth_geometry = gpu.abi.TemporaryGeometryInfo{ .struct_size = @sizeOf(gpu.abi.TemporaryGeometryInfo), .data = @ptrCast(&depth_vertices), .byte_length = @sizeOf(@TypeOf(depth_vertices)), .stride = 16, .format = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.draw_temporary(renderer, &depth_geometry, 2));
    try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
    var depth_pixels: [64 * 64 * 4]u8 = undefined;
    var depth_readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = depth_pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&depth_pixels) };
    try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &depth_readback));
    try std.testing.expect(std.mem.readInt(u32, depth_pixels[(16 * 64 + 16) * 4 ..][0..4], .little) != 0x10ff3020);

    // Draw once with D3DRS_SPECULARENABLE on. The specular variants are selected
    // by vertex format and this state, and their shader slots are indexed by the
    // variant enum -- when those caches were a hardcoded [5] the new variants
    // indexed past the end, and a release build handed Metal a garbage vertex
    // function and died inside setVertexFunction:. Nothing here inspects pixels;
    // the point is that the pipeline builds and the draw survives.
    var specular_on = gpu.abi.StateInfo{ .struct_size = @sizeOf(gpu.abi.StateInfo), .kind = 5, .index = 0, .value = 1, .values = @splat(0) };
    var specular_off = gpu.abi.StateInfo{ .struct_size = @sizeOf(gpu.abi.StateInfo), .kind = 5, .index = 0, .value = 0, .values = @splat(0) };
    try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_viewport(renderer, &screen_viewport));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_transform(renderer, &pixel_world, &pixel_view_proj));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_state(renderer, &specular_on));
    var specular_clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &specular_clear));
    // Textured and untextured take different specular variants, so both run.
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_texture(renderer, texture));
    try std.testing.expectEqual(gpu.error_codes.ok, api.draw(renderer, @intCast(textured_buffer), 2));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_texture(renderer, 0));
    var specular_geometry = gpu.abi.TemporaryGeometryInfo{ .struct_size = @sizeOf(gpu.abi.TemporaryGeometryInfo), .data = @ptrCast(&depth_vertices), .byte_length = @sizeOf(@TypeOf(depth_vertices)), .stride = 16, .format = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.draw_temporary(renderer, &specular_geometry, 2));
    // Render state only takes while a frame is recording, as it does in the game.
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_state(renderer, &specular_off));
    try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
}

pub fn main(init: std.process.Init) !void {
    try checkFixtures();
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.next();
    const driver_option = args.next() orelse return error.MissingDriverArgument;
    if (!std.mem.eql(u8, driver_option, "--driver")) return error.InvalidDriverArgument;
    const driver_arg = args.next() orelse return error.MissingDriverArgument;
    if (args.next() != null) return error.UnexpectedArgument;
    _ = bk_memory.installSdlMemoryFunctions(sdl3.c);
    if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return error.SdlInitFailed;
    defer sdl3.c.SDL_Quit();

    try runReferenceSmoke(driver_arg);

    const device = sdl3.c.SDL_CreateGPUDevice(sdl3.c.SDL_GPU_SHADERFORMAT_DXIL | sdl3.c.SDL_GPU_SHADERFORMAT_SPIRV | sdl3.c.SDL_GPU_SHADERFORMAT_MSL, false, driver_arg.ptr) orelse {
        std.debug.print("SDL_CreateGPUDevice failed: {s}\n", .{std.mem.span(sdl3.c.SDL_GetError())});
        return error.GpuDeviceFailed;
    };
    defer sdl3.c.SDL_DestroyGPUDevice(device);
    const selected_driver = std.mem.span(sdl3.c.SDL_GetGPUDeviceDriver(device));
    if (!std.mem.eql(u8, selected_driver, driver_arg)) return error.DriverSelectionMismatch;
    const selected_format = try gpu.device.formatForDriver(selected_driver);
    const selected_format_flag = gpu.device.formatFlag(selected_format);

    const manifest_bytes = try readFile(init, "zig-out/shaders/gfxgpu-shaders.manifest");
    defer init.gpa.free(manifest_bytes);
    var shader_manifest = try gpu.shader_manifest.parse(init.gpa, manifest_bytes);
    defer shader_manifest.deinit(init.gpa);

    var loader = gpu.shaders.Loader.init(init.gpa, @ptrCast(device), gpu.shaders.real_api);
    defer loader.deinit();
    for (effect_probes) |probe| {
        const vertex = try recordFor(&shader_manifest, probe.name, .vertex, selected_format);
        const fragment = try recordFor(&shader_manifest, probe.name, .fragment, selected_format);
        const vertex_path = try std.fmt.allocPrint(init.gpa, "zig-out/shaders/{s}", .{vertex.blob_path});
        defer init.gpa.free(vertex_path);
        const fragment_path = try std.fmt.allocPrint(init.gpa, "zig-out/shaders/{s}", .{fragment.blob_path});
        defer init.gpa.free(fragment_path);
        const vertex_bytes = try readFile(init, vertex_path);
        defer init.gpa.free(vertex_bytes);
        const fragment_bytes = try readFile(init, fragment_path);
        defer init.gpa.free(fragment_bytes);
        try loader.loadPair(&shader_manifest, probe.name, vertex_bytes, fragment_bytes, selected_format_flag);
    }
    if (loader.count() != effect_probes.len) return error.IncompleteShaderSmoke;
    var pipeline_count: usize = 0;
    for (effect_probes) |probe| {
        if (probe.vertex_count == 0) return error.InvalidEffectProbe;
        const pair = loader.pair(probe.name) orelse return error.MissingShaderPair;
        const pipeline = probePipeline(device, pair.vertex, pair.fragment, probe.attribute_count, probe.lighting_layout) orelse {
            std.debug.print("GfxGpu pipeline probe failed for {s}: {s}\n", .{ probe.name, std.mem.span(sdl3.c.SDL_GetError()) });
            return error.PipelineCreationFailed;
        };
        sdl3.c.SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        pipeline_count += 1;
    }
}
