const std = @import("std");
const sdl3 = @import("sdl3");
const gpu = @import("gfxgpu");

fn readFile(init: std.process.Init, path: []const u8) ![]u8 {
    return std.Io.Dir.cwd().readFileAlloc(init.io, path, init.gpa, .limited(64 * 1024 * 1024));
}

fn recordFor(shader_manifest: *const gpu.shader_manifest.Manifest, effect: []const u8, stage: gpu.shader_manifest.Stage) !*const gpu.shader_manifest.Record {
    for (shader_manifest.records) |*record| {
        if (record.stage == stage and std.mem.eql(u8, record.effect, effect)) return record;
    }
    return error.MissingShaderRecord;
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

fn runReferenceSmoke() !void {
    std.debug.print("GfxGpu reference: creating SDL window\n", .{});
    const window = sdl3.c.SDL_CreateWindow("GfxGpu reference", 64, 64, 0) orelse return error.SdlWindowFailed;
    std.debug.print("GfxGpu reference: SDL window created\n", .{});
    defer sdl3.c.SDL_DestroyWindow(window);
    var api: gpu.abi.Api = undefined;
    api.struct_size = @sizeOf(gpu.abi.Api);
    try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_get_api(gpu.abi.abi_version, &api));
    var renderer: ?*gpu.abi.RendererHandle = null;
    const shader_directory = "../shaders";
    var create_info = gpu.abi.CreateInfo{ .struct_size = @sizeOf(gpu.abi.CreateInfo), .flags = 1, .sdl_window = @ptrCast(window), .width = 64, .height = 64, .shader_directory_utf8 = shader_directory, .preferred_driver_utf8 = null };
    std.debug.print("GfxGpu reference: creating window-backed renderer\n", .{});
    const create_result = api.create(&create_info, &renderer);
    std.debug.print("GfxGpu reference: renderer create returned {}\n", .{create_result});
    if (create_result != gpu.error_codes.ok) {
        std.debug.print("GfxGpu reference create failed: result={} SDL={s}\n", .{ create_result, std.mem.span(sdl3.c.SDL_GetError()) });
        return error.ReferenceCreateFailed;
    }
    defer api.destroy(renderer);
    const Vertex = extern struct {
        position: [3]f32,
        color: [4]u8,
    };
    const vertices = [_]Vertex{
        .{ .position = .{ 0.0, 0.75, 0.0 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ -0.75, -0.75, 0.0 }, .color = .{ 0, 255, 0, 255 } },
        .{ .position = .{ 0.75, -0.75, 0.0 }, .color = .{ 0, 0, 255, 255 } },
    };
    var buffer: u64 = 0;
    var buffer_info = gpu.abi.BufferCreateInfo{ .struct_size = @sizeOf(gpu.abi.BufferCreateInfo), .element_count = 3, .format = 0, .stride = 16, .usage = 0 };
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
        try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
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
            var temporary = gpu.abi.TemporaryGeometryInfo{ .struct_size = @sizeOf(gpu.abi.TemporaryGeometryInfo), .data = @ptrCast(&vertices), .byte_length = @sizeOf(@TypeOf(vertices)), .stride = 16 };
            try std.testing.expectEqual(gpu.error_codes.ok, api.draw_temporary(renderer, &temporary, 1));
        }
        try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
        try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
        var pixels: [64 * 64 * 4]u8 = undefined;
        var readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&pixels) };
        try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &readback));
        try std.testing.expect(std.mem.readInt(u32, pixels[(16 * 64 + 16) * 4 ..][0..4], .little) != 0x10ff3020);
        std.crypto.hash.sha2.Sha256.hash(&pixels, hash, .{});
    }
    try std.testing.expectEqualSlices(u8, &hashes[0], &hashes[1]);
    try std.testing.expectEqualSlices(u8, &hashes[1], &hashes[2]);
    std.debug.print("GfxGpu reference smoke: 3 identical frame hashes\n", .{});

    const TexturedVertex = extern struct { position: [3]f32, color: [4]u8, uv: [2]f32 };
    const textured_vertices = [_]TexturedVertex{
        .{ .position = .{ -0.75, 0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 0.75, 0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 0 } },
        .{ .position = .{ 0.75, -0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 1 } },
        .{ .position = .{ -0.75, 0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 0 } },
        .{ .position = .{ 0.75, -0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 1, 1 } },
        .{ .position = .{ -0.75, -0.75, 0 }, .color = .{ 255, 255, 255, 255 }, .uv = .{ 0, 1 } },
    };
    var textured_buffer: u64 = 0;
    var textured_info = gpu.abi.BufferCreateInfo{ .struct_size = @sizeOf(gpu.abi.BufferCreateInfo), .element_count = textured_vertices.len, .format = 0, .stride = 24, .usage = 0 };
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
    try std.testing.expect(std.mem.readInt(u32, textured_pixels[(16 * 64 + 16) * 4 ..][0..4], .little) != 0x10ff3020);
    std.debug.print("GfxGpu textured smoke: visible textured quad\n", .{});

    const depth_vertices = [_]Vertex{
        .{ .position = .{ -0.75, 0.75, 0.75 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ 0.75, 0.75, 0.75 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ 0, -0.75, 0.75 }, .color = .{ 255, 0, 0, 255 } },
        .{ .position = .{ -0.75, 0.75, 0.25 }, .color = .{ 0, 0, 255, 255 } },
        .{ .position = .{ 0.75, 0.75, 0.25 }, .color = .{ 0, 0, 255, 255 } },
        .{ .position = .{ 0, -0.75, 0.25 }, .color = .{ 0, 0, 255, 255 } },
    };
    try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.set_texture(renderer, 0));
    var depth_clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &depth_clear));
    var depth_geometry = gpu.abi.TemporaryGeometryInfo{ .struct_size = @sizeOf(gpu.abi.TemporaryGeometryInfo), .data = @ptrCast(&depth_vertices), .byte_length = @sizeOf(@TypeOf(depth_vertices)), .stride = 16 };
    try std.testing.expectEqual(gpu.error_codes.ok, api.draw_temporary(renderer, &depth_geometry, 2));
    try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
    try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
    var depth_pixels: [64 * 64 * 4]u8 = undefined;
    var depth_readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = depth_pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&depth_pixels) };
    try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &depth_readback));
    try std.testing.expect(std.mem.readInt(u32, depth_pixels[(16 * 64 + 16) * 4 ..][0..4], .little) != 0x10ff3020);
    std.debug.print("GfxGpu depth smoke: overlapping triangles resolved\n", .{});
}

pub fn main(init: std.process.Init) !void {
    try checkFixtures();
    if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return error.SdlInitFailed;
    defer sdl3.c.SDL_Quit();

    try runReferenceSmoke();

    const device = sdl3.c.SDL_CreateGPUDevice(sdl3.c.SDL_GPU_SHADERFORMAT_DXIL, false, null) orelse {
        std.debug.print("SDL_CreateGPUDevice failed: {s}\n", .{std.mem.span(sdl3.c.SDL_GetError())});
        return error.GpuDeviceFailed;
    };
    defer sdl3.c.SDL_DestroyGPUDevice(device);

    const manifest_bytes = try readFile(init, "../shaders/gfxgpu-shaders.manifest");
    defer init.gpa.free(manifest_bytes);
    var shader_manifest = try gpu.shader_manifest.parse(init.gpa, manifest_bytes);
    defer shader_manifest.deinit(init.gpa);

    var loader = gpu.shaders.Loader.init(init.gpa, @ptrCast(device), gpu.shaders.real_api);
    defer loader.deinit();
    for ([_][]const u8{ "untextured", "textured", "ui", "unlit", "unlit_textured", "alpha_test", "transparent", "particle_additive", "particle_modulate", "transparent_multiply", "transparent_alpha", "transparent_additive" }) |effect| {
        const vertex = try recordFor(&shader_manifest, effect, .vertex);
        const fragment = try recordFor(&shader_manifest, effect, .fragment);
        const vertex_path = try std.fmt.allocPrint(init.gpa, "../shaders/{s}", .{vertex.blob_path});
        defer init.gpa.free(vertex_path);
        const fragment_path = try std.fmt.allocPrint(init.gpa, "../shaders/{s}", .{fragment.blob_path});
        defer init.gpa.free(fragment_path);
        const vertex_bytes = try readFile(init, vertex_path);
        defer init.gpa.free(vertex_bytes);
        const fragment_bytes = try readFile(init, fragment_path);
        defer init.gpa.free(fragment_bytes);
        try loader.loadPair(&shader_manifest, effect, vertex_bytes, fragment_bytes, sdl3.c.SDL_GPU_SHADERFORMAT_DXIL);
    }
    if (loader.count() != 12) return error.IncompleteShaderSmoke;
    std.debug.print("GfxGpu Zig smoke: created and released {} UI/unlit/alpha shader pairs\n", .{loader.count()});
}
