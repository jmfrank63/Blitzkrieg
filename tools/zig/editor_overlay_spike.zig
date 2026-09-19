//! Overlay spike for the portable Map Editor: proves that Dear ImGui, called
//! from Zig, draws on top of a GFXGPU frame in the same frame, and measures
//! both from one readback. Usage:
//!   editor-overlay-spike [--hidden] [--out <file.rgba>]
//! Exit code 0 and "overlay-spike: PASS" on success.
const std = @import("std");
const sdl3 = @import("sdl3");
const gpu = @import("gfxgpu");
const imgui = @import("editor_imgui");

const window_width = 320;
const window_height = 240;
const panel = struct {
    const x = 20;
    const y = 20;
    const w = 100;
    const h = 80;
};

const Pixel = struct { r: u8, g: u8, b: u8 };

fn pixelAt(pixels: []const u8, row_pitch: u32, x: u32, y: u32) Pixel {
    const i = @as(usize, y) * row_pitch + @as(usize, x) * 4;
    // Byte 1 is green in both RGBA and BGRA; bytes 0 and 2 swap, which the
    // checks below do not care about (magenta and green are symmetric).
    return .{ .r = pixels[i], .g = pixels[i + 1], .b = pixels[i + 2] };
}

fn isMagenta(p: Pixel) bool {
    return p.r > 200 and p.g < 60 and p.b > 200;
}

fn isGreen(p: Pixel) bool {
    return p.g > 200 and p.r < 60 and p.b < 60;
}

pub fn main(init: std.process.Init) !void {
    var hidden = false;
    var out_path: ?[]const u8 = null;
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.next();
    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--hidden")) {
            hidden = true;
        } else if (std.mem.eql(u8, arg, "--out")) {
            out_path = args.next() orelse return error.MissingOutPath;
        } else return error.UnexpectedArgument;
    }

    if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return error.SdlInitFailed;
    defer sdl3.c.SDL_Quit();
    const flags: sdl3.c.SDL_WindowFlags = if (hidden) sdl3.c.SDL_WINDOW_HIDDEN else 0;
    const window = sdl3.c.SDL_CreateWindow("Map Editor overlay spike", window_width, window_height, flags) orelse return error.SdlWindowFailed;
    defer sdl3.c.SDL_DestroyWindow(window);
    if (!hidden) _ = sdl3.c.SDL_ShowWindow(window);
    sdl3.c.SDL_PumpEvents();

    var api: gpu.abi.Api = undefined;
    api.struct_size = @sizeOf(gpu.abi.Api);
    if (gpu.abi.gfxgpu_get_api(gpu.abi.abi_version, &api) != gpu.error_codes.ok) return error.GfxGpuApiFailed;
    var renderer: ?*gpu.abi.RendererHandle = null;
    var create_info = gpu.abi.CreateInfo{ .struct_size = @sizeOf(gpu.abi.CreateInfo), .flags = 0, .sdl_window = @ptrCast(window), .width = window_width, .height = window_height, .shader_directory_utf8 = "zig-out/shaders", .preferred_driver_utf8 = null };
    if (api.create(&create_info, &renderer) != gpu.error_codes.ok) {
        std.debug.print("overlay-spike: renderer create failed: SDL={s}\n", .{std.mem.span(sdl3.c.SDL_GetError())});
        return error.RendererCreateFailed;
    }
    defer api.destroy(renderer);

    var device: ?*anyopaque = null;
    var format: u32 = 0;
    if (api.get_gpu_device(renderer, &device, &format) != gpu.error_codes.ok) return error.GpuDeviceUnavailable;

    _ = imgui.c.igCreateContext(null);
    defer imgui.c.igDestroyContext(null);
    imgui.c.igGetIO().*.IniFilename = null;
    if (!imgui.c.bk_imgui_backend_init(@ptrCast(window), device, format)) return error.ImguiBackendInitFailed;
    defer imgui.c.bk_imgui_backend_shutdown();
    if (api.set_overlay(renderer, imgui.overlayCallback, null) != gpu.error_codes.ok) return error.SetOverlayFailed;

    // The capture has the drawable's size, which the renderer refreshes on
    // every swapchain acquire and which equals the window's size in pixels.
    // If gfxgpu_readback_frame answers invalid_state, the two differ: query
    // the size again after the first presented frame.
    var pixel_width: c_int = 0;
    var pixel_height: c_int = 0;
    _ = sdl3.c.SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height);
    const width: u32 = @intCast(pixel_width);
    const height: u32 = @intCast(pixel_height);
    const scale: f32 = @as(f32, @floatFromInt(width)) / window_width;

    const row_pitch = width * 4;
    const pixels = try init.gpa.alloc(u8, @as(usize, row_pitch) * height);
    defer init.gpa.free(pixels);

    var captured = false;
    var presented: u32 = 0;
    var attempt: u32 = 0;
    while (attempt < 120 and !captured) : (attempt += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = imgui.c.bk_imgui_backend_process_event(@ptrCast(&event));

        imgui.c.bk_imgui_backend_new_frame();
        imgui.c.igNewFrame();
        imgui.c.igSetNextWindowPos(.{ .x = panel.x, .y = panel.y }, imgui.c.ImGuiCond_Always);
        imgui.c.igSetNextWindowSize(.{ .x = panel.w, .y = panel.h }, imgui.c.ImGuiCond_Always);
        imgui.c.igPushStyleColorImVec4(imgui.c.ImGuiCol_WindowBg, .{ .x = 1, .y = 0, .z = 1, .w = 1 });
        _ = imgui.c.igBegin("probe", null, imgui.c.ImGuiWindowFlags_NoDecoration | imgui.c.ImGuiWindowFlags_NoMove | imgui.c.ImGuiWindowFlags_NoSavedSettings);
        imgui.c.igEnd();
        imgui.c.igPopStyleColor();
        imgui.c.igRender();

        // Let a few frames through first so the font atlas is uploaded and
        // the window has settled on screen.
        const capture_this = presented >= 3;
        if (capture_this) _ = api.set_frame_capture(renderer, 1);
        if (api.begin_frame(renderer) != gpu.error_codes.ok) {
            // Skipped (no swapchain texture this time, e.g. occluded): try again.
            if (capture_this) _ = api.set_frame_capture(renderer, 0);
            sdl3.c.SDL_Delay(16);
            continue;
        }
        var viewport = gpu.abi.ViewportInfo{ .struct_size = @sizeOf(gpu.abi.ViewportInfo), .x = 0, .y = 0, .width = window_width, .height = window_height, .min_depth = 0, .max_depth = 1 };
        _ = api.set_viewport(renderer, &viewport);
        // D3DCOLOR 0xAARRGGBB: opaque green.
        var clear = gpu.abi.ClearInfo{ .struct_size = @sizeOf(gpu.abi.ClearInfo), .mask = 1, .color_rgba8 = 0xff00ff00, .depth = 1.0, .stencil = 0 };
        if (api.clear(renderer, &clear) != gpu.error_codes.ok) return error.ClearFailed;
        if (api.end_frame(renderer) != gpu.error_codes.ok) return error.EndFrameFailed;
        if (api.present(renderer) != gpu.error_codes.ok) return error.PresentFailed;
        presented += 1;
        if (capture_this) {
            var info = gpu.abi.ReadbackInfo{ .struct_size = @sizeOf(gpu.abi.ReadbackInfo), .width = width, .height = height, .byte_length = @intCast(pixels.len), .row_pitch = row_pitch, .data = pixels.ptr };
            if (gpu.abi.gfxgpu_readback_frame(renderer, &info) != gpu.error_codes.ok) return error.ReadbackFailed;
            captured = true;
        }
    }
    if (!captured) {
        std.debug.print("overlay-spike: FAIL no frame could be presented in {} attempts (hidden={})\n", .{ attempt, hidden });
        std.process.exit(2);
    }

    if (out_path) |path| {
        try std.Io.Dir.cwd().writeFile(init.io, .{ .sub_path = path, .data = pixels });
    }

    const inside = pixelAt(pixels, row_pitch, @intFromFloat((panel.x + panel.w / 2) * scale), @intFromFloat((panel.y + panel.h / 2) * scale));
    const outside = pixelAt(pixels, row_pitch, width - @as(u32, @intFromFloat(10 * scale)), height - @as(u32, @intFromFloat(10 * scale)));
    const pass = isMagenta(inside) and isGreen(outside);
    const driver_name = sdl3.c.SDL_GetGPUDeviceDriver(@ptrCast(device));
    const driver: []const u8 = if (driver_name != null) std.mem.span(driver_name) else "unknown";
    std.debug.print("overlay-spike: {s} driver={s} format={} size={}x{} hidden={} inside=({},{},{}) outside=({},{},{})\n", .{
        if (pass) "PASS" else "FAIL", driver, format, width, height, hidden,
        inside.r, inside.g, inside.b, outside.r, outside.g, outside.b,
    });
    if (!pass) std.process.exit(1);
}
