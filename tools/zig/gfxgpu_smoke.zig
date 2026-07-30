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

pub fn main(init: std.process.Init) !void {
    try checkFixtures();
    if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return error.SdlInitFailed;
    defer sdl3.c.SDL_Quit();

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
