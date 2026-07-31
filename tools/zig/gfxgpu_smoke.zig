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
    var hashes: [3][32]u8 = undefined;
    for (&hashes) |*hash| {
        try std.testing.expectEqual(gpu.error_codes.ok, api.begin_frame(renderer));
        var clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0x102030ff, .depth = 1.0, .stencil = 0 };
        try std.testing.expectEqual(gpu.error_codes.ok, api.clear(renderer, &clear));
        try std.testing.expectEqual(gpu.error_codes.ok, api.draw(renderer, @intCast(buffer), 1));
        try std.testing.expectEqual(gpu.error_codes.ok, api.end_frame(renderer));
        try std.testing.expectEqual(gpu.error_codes.ok, api.present(renderer));
        var pixels: [64 * 64 * 4]u8 = undefined;
        var readback = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = 64, .height = 64, .byte_length = pixels.len, .row_pitch = 64 * 4, .data = @ptrCast(&pixels) };
        try std.testing.expectEqual(gpu.error_codes.ok, gpu.abi.gfxgpu_readback(renderer, &readback));
        std.crypto.hash.sha2.Sha256.hash(&pixels, hash, .{});
    }
    try std.testing.expectEqualSlices(u8, &hashes[0], &hashes[1]);
    try std.testing.expectEqualSlices(u8, &hashes[1], &hashes[2]);
    std.debug.print("GfxGpu reference smoke: 3 identical frame hashes\n", .{});
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
    for ([_][]const u8{ "untextured", "textured" }) |effect| {
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
    if (loader.count() != 2) return error.IncompleteShaderSmoke;
    std.debug.print("GfxGpu Zig smoke: created and released {} baseline shader pairs\n", .{loader.count()});
}
