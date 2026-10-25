//! The engine-less window host: one SDL window, an SDL_GPU device of its
//! own and ImGui drawing into it through the kit's backend shim
//! (`editor_imgui`). `host` starts the engine (BkEditorStart owns the GPU
//! device and the frame); an editor that needs no engine (ELK, D-01; the
//! Phase 8 tools) uses this instead, with the same start / handleEvent /
//! beginFrame / endFrame / stop shape.
//!
//! A hidden window (the smoke) draws into an offscreen texture instead of
//! the swapchain: a hidden window has no surface the compositor presents,
//! and waiting on its swapchain could block, while the offscreen target runs
//! every draw call the visible window does.
const std = @import("std");
const sdl3 = @import("sdl3");
pub const app_identity = @import("app_identity.zig");
const imgui = @import("editor_imgui");
const tga = @import("capture.zig");

const c = sdl3.c;
const ig = imgui.c;

pub const Error = error{ SdlInitFailed, WindowFailed, NoDevice, ImguiFailed, FrameFailed, CaptureFailed };

pub const Options = struct {
    title: [*:0]const u8,
    /// The desktop identity (kit/app_identity.zig), as in host.zig.
    app_id: [*:0]const u8 = app_identity.elk,
    width: c_int = 1280,
    height: c_int = 800,
    hidden: bool = false,
};

/// The colour behind every panel, ImGui's own dark background.
const clear_colour: c.SDL_FColor = .{ .r = 0.06, .g = 0.06, .b = 0.06, .a = 1.0 };

pub const Window = struct {
    window: *c.SDL_Window,
    device: *c.SDL_GPUDevice,
    /// The offscreen target's size, which `capture` reads back.
    width: u32,
    height: u32,
    /// Hidden runs draw here; null when the swapchain is the target.
    offscreen: ?*c.SDL_GPUTexture,
    /// The offscreen frame still on the GPU. Acquiring a swapchain texture waits for a free
    /// one, so a visible window never runs ahead of the GPU; the offscreen target has no such
    /// wait, and without this one a scripted run queued frames as fast as it could record them.
    /// On lavapipe each frame's descriptor sets are mappings returned only when its work is
    /// done: the ELK smoke peaked at 292,779 mappings and on the CI runner, whose limit is
    /// vm.max_map_count's 65,530, an mmap failed inside vkAllocateDescriptorSets (run 39124407273).
    in_flight: ?*c.SDL_GPUFence = null,

    /// On failure, `failureReason()` says why.
    pub fn start(options: Options) Error!Window {
        failure_len = 0;
        app_identity.setMetadata(options.title, options.app_id);
        if (!c.SDL_Init(c.SDL_INIT_VIDEO)) return failWith(error.SdlInitFailed, sdlError());
        errdefer c.SDL_Quit();
        var flags: c.SDL_WindowFlags = c.SDL_WINDOW_RESIZABLE;
        // Not focusable either, so the person's own pointer and keyboard
        // never reach a scripted run (AGENTS.md's hidden test windows).
        if (options.hidden) flags |= c.SDL_WINDOW_HIDDEN | c.SDL_WINDOW_NOT_FOCUSABLE;
        const window = c.SDL_CreateWindow(options.title, options.width, options.height, flags) orelse
            return failWith(error.WindowFailed, sdlError());
        errdefer c.SDL_DestroyWindow(window);
        app_identity.setWindowIcon(window, null);

        // Every shader format the vendored ImGui SDL_GPU backend carries.
        const formats = c.SDL_GPU_SHADERFORMAT_SPIRV | c.SDL_GPU_SHADERFORMAT_DXBC | c.SDL_GPU_SHADERFORMAT_MSL | c.SDL_GPU_SHADERFORMAT_METALLIB;
        const device = c.SDL_CreateGPUDevice(formats, false, null) orelse return failWith(error.NoDevice, sdlError());
        errdefer c.SDL_DestroyGPUDevice(device);

        var offscreen: ?*c.SDL_GPUTexture = null;
        var format: c.SDL_GPUTextureFormat = undefined;
        if (options.hidden) {
            format = c.SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            var info = std.mem.zeroes(c.SDL_GPUTextureCreateInfo);
            info.type = c.SDL_GPU_TEXTURETYPE_2D;
            info.format = format;
            info.usage = c.SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            info.width = @intCast(options.width);
            info.height = @intCast(options.height);
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            offscreen = c.SDL_CreateGPUTexture(device, &info) orelse return failWith(error.NoDevice, sdlError());
        } else {
            if (!c.SDL_ClaimWindowForGPUDevice(device, window)) return failWith(error.NoDevice, sdlError());
            format = c.SDL_GetGPUSwapchainTextureFormat(device, window);
        }
        errdefer if (offscreen) |texture| c.SDL_ReleaseGPUTexture(device, texture) else c.SDL_ReleaseWindowFromGPUDevice(device, window);

        _ = ig.igCreateContext(null);
        errdefer ig.igDestroyContext(null);
        ig.igGetIO().*.IniFilename = null;
        if (!ig.bk_imgui_backend_init(@ptrCast(window), @ptrCast(device), @intCast(format)))
            return failWith(error.ImguiFailed, "the ImGui SDL3/SDL_GPU backend did not initialise");
        return .{ .window = window, .device = device, .width = @intCast(options.width), .height = @intCast(options.height), .offscreen = offscreen };
    }

    pub fn stop(self: *Window) void {
        _ = c.SDL_WaitForGPUIdle(self.device);
        if (self.in_flight) |fence| c.SDL_ReleaseGPUFence(self.device, fence);
        ig.bk_imgui_backend_shutdown();
        ig.igDestroyContext(null);
        if (self.offscreen) |texture| c.SDL_ReleaseGPUTexture(self.device, texture) else c.SDL_ReleaseWindowFromGPUDevice(self.device, self.window);
        c.SDL_DestroyGPUDevice(self.device);
        c.SDL_DestroyWindow(self.window);
        c.SDL_Quit();
        self.* = undefined;
    }

    /// True when ImGui used the event.
    pub fn handleEvent(self: *Window, event: *const c.SDL_Event) bool {
        _ = self;
        return ig.bk_imgui_backend_process_event(@ptrCast(event));
    }

    pub fn beginFrame(self: *Window) void {
        _ = self;
        ig.bk_imgui_backend_new_frame();
        ig.igNewFrame();
    }

    /// Finishes ImGui's frame and draws it over a cleared target. A window
    /// that has no swapchain texture this frame (minimised) skips drawing.
    pub fn endFrame(self: *Window) Error!void {
        ig.igRender();
        if (self.in_flight) |fence| {
            const fences = [_]?*c.SDL_GPUFence{fence};
            _ = c.SDL_WaitForGPUFences(self.device, true, &fences, 1);
            c.SDL_ReleaseGPUFence(self.device, fence);
            self.in_flight = null;
        }
        const command_buffer = c.SDL_AcquireGPUCommandBuffer(self.device) orelse return failWith(error.FrameFailed, sdlError());
        var target: ?*c.SDL_GPUTexture = self.offscreen;
        if (target == null) {
            if (!c.SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, self.window, &target, null, null)) {
                _ = c.SDL_CancelGPUCommandBuffer(command_buffer);
                return failWith(error.FrameFailed, sdlError());
            }
        }
        if (target) |texture| {
            var colour = std.mem.zeroes(c.SDL_GPUColorTargetInfo);
            colour.texture = texture;
            colour.clear_color = clear_colour;
            colour.load_op = c.SDL_GPU_LOADOP_CLEAR;
            colour.store_op = c.SDL_GPU_STOREOP_STORE;
            const pass = c.SDL_BeginGPURenderPass(command_buffer, &colour, 1, null) orelse {
                _ = c.SDL_CancelGPUCommandBuffer(command_buffer);
                return failWith(error.FrameFailed, sdlError());
            };
            c.SDL_EndGPURenderPass(pass);
            ig.bk_imgui_backend_render(@ptrCast(command_buffer), @ptrCast(texture));
        }
        if (self.offscreen != null) {
            self.in_flight = c.SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer) orelse return failWith(error.FrameFailed, sdlError());
        } else if (!c.SDL_SubmitGPUCommandBuffer(command_buffer)) return failWith(error.FrameFailed, sdlError());
    }

    /// Writes the last drawn frame of a hidden window to `path` as a 32-bit TGA and prints
    /// `capture: <path> <w>x<h>`. The frame is read back from the offscreen texture through a
    /// download transfer buffer, so a visible window (swapchain target) cannot be captured.
    /// On failure `failureReason()` names the path and SDL_GetError.
    pub fn capture(self: *Window, alloc: std.mem.Allocator, io: std.Io, path: []const u8) Error!void {
        const texture = self.offscreen orelse return failFmt("capture {s}: only a hidden window has an offscreen frame to read", .{path});
        if (self.in_flight) |fence| {
            const fences = [_]?*c.SDL_GPUFence{fence};
            _ = c.SDL_WaitForGPUFences(self.device, true, &fences, 1);
            c.SDL_ReleaseGPUFence(self.device, fence);
            self.in_flight = null;
        }
        const size: u32 = self.width * self.height * 4;
        var info = std.mem.zeroes(c.SDL_GPUTransferBufferCreateInfo);
        info.usage = c.SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        info.size = size;
        const transfer = c.SDL_CreateGPUTransferBuffer(self.device, &info) orelse
            return failFmt("capture {s}: no transfer buffer: {s}", .{ path, sdlError() });
        defer c.SDL_ReleaseGPUTransferBuffer(self.device, transfer);

        const command_buffer = c.SDL_AcquireGPUCommandBuffer(self.device) orelse
            return failFmt("capture {s}: no command buffer: {s}", .{ path, sdlError() });
        const pass = c.SDL_BeginGPUCopyPass(command_buffer) orelse {
            _ = c.SDL_CancelGPUCommandBuffer(command_buffer);
            return failFmt("capture {s}: no copy pass: {s}", .{ path, sdlError() });
        };
        var source = std.mem.zeroes(c.SDL_GPUTextureRegion);
        source.texture = texture;
        source.w = self.width;
        source.h = self.height;
        source.d = 1;
        var destination = std.mem.zeroes(c.SDL_GPUTextureTransferInfo);
        destination.transfer_buffer = transfer;
        c.SDL_DownloadFromGPUTexture(pass, &source, &destination);
        c.SDL_EndGPUCopyPass(pass);
        const fence = c.SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer) orelse
            return failFmt("capture {s}: submit failed: {s}", .{ path, sdlError() });
        const fences = [_]?*c.SDL_GPUFence{fence};
        const waited = c.SDL_WaitForGPUFences(self.device, true, &fences, 1);
        c.SDL_ReleaseGPUFence(self.device, fence);
        if (!waited) return failFmt("capture {s}: download did not finish: {s}", .{ path, sdlError() });

        const mapped = c.SDL_MapGPUTransferBuffer(self.device, transfer, false) orelse
            return failFmt("capture {s}: map failed: {s}", .{ path, sdlError() });
        defer c.SDL_UnmapGPUTransferBuffer(self.device, transfer);
        const pixels: [*]const u8 = @ptrCast(mapped);
        const bytes = tga.encodeTga(alloc, self.width, self.height, pixels[0..size]) catch |err|
            return failFmt("capture {s}: {s}", .{ path, @errorName(err) });
        defer alloc.free(bytes);
        const cwd = std.Io.Dir.cwd();
        if (std.fs.path.dirname(path)) |parent| cwd.createDirPath(io, parent) catch {};
        cwd.writeFile(io, .{ .sub_path = path, .data = bytes }) catch |err|
            return failFmt("capture {s}: {s}", .{ path, @errorName(err) });
        std.debug.print("capture: {s} {d}x{d}\n", .{ path, self.width, self.height });
    }

    pub fn setTitle(self: *Window, title: [*:0]const u8) void {
        _ = c.SDL_SetWindowTitle(self.window, title);
    }
};

var failure_buffer: [512]u8 = undefined;
var failure_len: usize = 0;

/// Why the last start or frame failed; "" when nothing did.
pub fn failureReason() []const u8 {
    return failure_buffer[0..failure_len];
}

fn failWith(err: Error, reason: []const u8) Error {
    failure_len = @min(reason.len, failure_buffer.len);
    @memcpy(failure_buffer[0..failure_len], reason[0..failure_len]);
    return err;
}

fn failFmt(comptime format: []const u8, args: anytype) Error {
    const reason = std.fmt.bufPrint(&failure_buffer, format, args) catch failure_buffer[0..];
    failure_len = reason.len;
    return error.CaptureFailed;
}

fn sdlError() []const u8 {
    return std.mem.span(c.SDL_GetError());
}
