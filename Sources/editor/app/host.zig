//! The editor's process: one SDL window, the engine started on it through
//! the bridge, and ImGui drawing into the engine's own frame. Nothing here
//! edits; the view and the panels (Tasks 4-5) sit on top.
const std = @import("std");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
pub const c = @cImport(@cInclude("bridge.h"));

pub const HostError = error{ SdlInitFailed, WindowFailed, EngineFailed, NoDevice, ImguiFailed, FrameFailed };

pub const Options = struct {
    title: [*:0]const u8,
    width: c_int = 1280,
    height: c_int = 800,
    hidden: bool = false,
    data_root: [*:0]const u8 = ".",
};

pub const Host = struct {
    window: *sdl3.c.SDL_Window,
    session: *c.BkEditorSession,

    pub fn start(options: Options) HostError!Host {
        if (!sdl3.c.SDL_Init(sdl3.c.SDL_INIT_VIDEO)) return error.SdlInitFailed;
        errdefer sdl3.c.SDL_Quit();
        // No SDL_WINDOW_HIGH_PIXEL_DENSITY: a point is a pixel, so a mouse
        // position is a screen position (see the plan's Decisions).
        var flags: sdl3.c.SDL_WindowFlags = sdl3.c.SDL_WINDOW_RESIZABLE;
        if (options.hidden) flags |= sdl3.c.SDL_WINDOW_HIDDEN;
        const window = sdl3.c.SDL_CreateWindow(options.title, options.width, options.height, flags) orelse return error.WindowFailed;
        errdefer sdl3.c.SDL_DestroyWindow(window);

        var session: ?*c.BkEditorSession = null;
        const started = c.BkEditorStart(window, options.data_root, &session);
        if (started != c.BK_EDITOR_OK) {
            std.debug.print("map-editor: the engine did not start: {s}\n", .{std.mem.span(c.BkEditorLastMessage(session))});
            if (session) |s| _ = c.BkEditorStop(s);
            return if (started == c.BK_EDITOR_NO_DEVICE) error.NoDevice else error.EngineFailed;
        }
        errdefer _ = c.BkEditorStop(session.?);

        var device: ?*anyopaque = null;
        var format: c_uint = 0;
        if (c.BkEditorGpuDevice(session, &device, &format) != c.BK_EDITOR_OK) return error.NoDevice;
        _ = imgui.c.igCreateContext(null);
        errdefer imgui.c.igDestroyContext(null);
        imgui.c.igGetIO().*.IniFilename = null;
        if (!imgui.c.bk_imgui_backend_init(@ptrCast(window), device, format)) return error.ImguiFailed;
        errdefer imgui.c.bk_imgui_backend_shutdown();
        if (c.BkEditorSetOverlay(session, overlay, null) != c.BK_EDITOR_OK) return error.ImguiFailed;
        return .{ .window = window, .session = session.? };
    }

    pub fn stop(self: *Host) void {
        _ = c.BkEditorSetOverlay(self.session, null, null);
        imgui.c.bk_imgui_backend_shutdown();
        imgui.c.igDestroyContext(null);
        _ = c.BkEditorStop(self.session);
        sdl3.c.SDL_DestroyWindow(self.window);
        sdl3.c.SDL_Quit();
        self.* = undefined;
    }

    /// True when ImGui used the event. A window resize is passed to the
    /// engine here, so the screen stays the window.
    pub fn handleEvent(self: *Host, event: *const sdl3.c.SDL_Event) bool {
        if (event.type == sdl3.c.SDL_EVENT_WINDOW_RESIZED) {
            _ = c.BkEditorResize(self.session, event.window.data1, event.window.data2);
        }
        return imgui.c.bk_imgui_backend_process_event(@ptrCast(event));
    }

    pub fn beginFrame(self: *Host) void {
        _ = self;
        imgui.c.bk_imgui_backend_new_frame();
        imgui.c.igNewFrame();
    }

    /// Finishes ImGui's frame and draws the engine's; the overlay puts ImGui's
    /// draw data into it before present. A lost device is a skipped frame,
    /// not an error (BK_EDITOR_REFUSED from BkEditorFrame).
    pub fn endFrame(self: *Host) HostError!void {
        imgui.c.igRender();
        const status = c.BkEditorFrame(self.session);
        if (status != c.BK_EDITOR_OK and status != c.BK_EDITOR_REFUSED) return error.FrameFailed;
    }
};

fn overlay(user: ?*anyopaque, command_buffer: ?*anyopaque, target: ?*anyopaque, width: c_uint, height: c_uint) callconv(.c) void {
    _ = user;
    _ = width;
    _ = height;
    imgui.c.bk_imgui_backend_render(command_buffer, target);
}
